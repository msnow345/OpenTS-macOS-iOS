/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <string>
#include <vector>

// Turns fingers into the pointer the engine already reads. Nothing is dispatched while a
// gesture is still ambiguous: a press that becomes a two finger pan must never have left a
// click behind it, because a stray left click on the tactical map issues an order or places
// a building. The number of fingers is what separates a rubber band from a pan, and that
// only works because the first finger commits to nothing.
//
// What the layer decides, and nothing more:
//
//   one finger, tapped               left click at the press point
//   one finger, dragged              left button held from the press point
//   one finger, held still           right click, which is the engine's cancel and deselect
//   two fingers, dragged             a scroll offset the tactical view consumes, with inertia
//   any tap while a movie plays      the ESC the movie skip already waits for
//
// Where the pointer rests between gestures is part of the design rather than an accident. A
// finger that leaves the glass sends no motion away from where it was, so a press near an
// edge would leave the engine believing the pointer still rested there: the hover would
// never retire and the edge scroll, which is armed by entering the band and disarmed by
// leaving it, would scroll for ever. The pointer is parked at the middle of the window when
// no gesture is running, which retires the hover, keeps the edge scroll disarmed, and puts
// a placement ghost in the middle of the view rather than under the last thing tapped.
//
// Nobody driving this can watch what the recognizer decided, so it writes down every finger
// it is given, every phase it moves through and every message it posts. See Log_Open below
// for how it is turned on and where it lands.

namespace
{

// One physical measure the rest are taken from, because a threshold in pixels means a
// different gesture on every panel. The floor keeps it sane if the display lies about its
// density.
constexpr float TOUCH_DEAD_ZONE_MM = 3.0f;
constexpr float TOUCH_POINTS_PER_MM = 160.0f / 25.4f;
constexpr float TOUCH_DEAD_ZONE_FLOOR = 8.0f;

constexpr Uint64 TOUCH_LONG_PRESS_NS = 400ULL * 1000000ULL;

// The engine keeps its own virtual key table, so the one key this layer sends by name is
// spelled out here the way the rest of the layer spells the mouse buttons.
constexpr int TOUCH_VK_ESCAPE = 0x1B;

// A synthetic press and release drained in one pass would leave the pressed state visible
// for no rendered frame at all, so no button in the game would ever flash. The release is
// held back for longer than a frame at the slowest rate the engine is expected to present.
constexpr Uint64 TOUCH_PRESS_FLASH_NS = 33ULL * 1000000ULL;

// People ease off as they lift, so the release speed is measured over a window of
// timestamped samples rather than taken from the last motion.
constexpr Uint64 TOUCH_VELOCITY_WINDOW_NS = 60ULL * 1000000ULL;
constexpr int TOUCH_VELOCITY_SAMPLES = 16;

// UIScrollView's own deceleration rate, applied per millisecond so that the same flick
// travels the same distance whatever the frame rate.
constexpr double TOUCH_DECAY_PER_MS = 0.998;
constexpr double TOUCH_COAST_STOP_POINTS_PER_MS = 0.02;

enum PhaseType
{
	PHASE_IDLE,			// nothing is being tracked
	PHASE_PENDING,		// one finger is down and the gesture is not yet known
	PHASE_DRAG,			// the left button is held and follows the finger
	PHASE_PRESSED,		// the long press has fired; the finger has nothing left to say
	PHASE_PAN,			// two fingers are moving the tactical view
	PHASE_SPENT,		// the gesture is over but fingers are still on the glass
	PHASE_MOVIE			// a movie is playing, so a tap means skip and nothing else
};

struct FingerRecord
{
	SDL_FingerID Id;
	float X;
	float Y;
};

struct DeferredRelease
{
	Uint8 Button;
	Uint64 Due;
	bool Pending;
};

struct VelocitySample
{
	Uint64 Time;
	float X;
	float Y;
};

PhaseType _Phase = PHASE_IDLE;
std::vector<FingerRecord> _Fingers;

float _PressX;
float _PressY;
Uint64 _PressTime;

float _LastX;
float _LastY;

float _PanX;
float _PanY;
bool _PanEngaged;
std::vector<VelocitySample> _Samples;

double _CoastX;
double _CoastY;
Uint64 _CoastTime;
bool _Coasting;

// The offset the tactical view has yet to travel, in the window's own pixels. The engine
// takes it whole and converts it into the frame's pixels itself.
double _ScrollX;
double _ScrollY;

DeferredRelease _Release;
bool _MovieMode;

// Which button this layer is holding down, so that nothing here ever releases a button a
// real mouse pressed.
Uint8 _HeldButton;

float Dead_Zone(void);
bool Window_Size(float & width, float & height);

// The pointer is parked once the gesture's own messages are all out. Parking ahead of a
// deferred release would deliver that release at the middle of the window.
bool _ParkPending;

FILE * _Log;
bool _LogChecked;


char const * Phase_Name(PhaseType phase)
{
	switch (phase) {
		case PHASE_IDLE: return("idle");
		case PHASE_PENDING: return("pending");
		case PHASE_DRAG: return("drag");
		case PHASE_PRESSED: return("pressed");
		case PHASE_PAN: return("pan");
		case PHASE_SPENT: return("spent");
		case PHASE_MOVIE: return("movie");
		default: return("?");
	}
}


bool Directory_Exists(char const * path)
{
	struct stat info;
	return(stat(path, &info) == 0 && S_ISDIR(info.st_mode));
}


// The log is turned on by a directory the player can make, and written into it. A device has
// no command line to pass a flag on and no console to read, so the switch has to be
// something a person can reach: on iOS the directory is "touchlog" inside the application's
// Documents folder, which is the folder the Files app shows, and the log can be copied out
// of the same place. Everywhere else the directory is "touchlog" beside the working
// directory, and OPENTS_TOUCH_LOG or -TOUCHLOG on the command line will create it.
std::string Log_Directory(void)
{
#ifdef OPENTS_IOS
	char const * home = getenv("HOME");
	return(std::string(home != NULL ? home : ".") + "/Documents/touchlog");
#else
	char const * named = getenv("OPENTS_TOUCH_LOG_DIR");
	return(named != NULL && *named != '\0' ? std::string(named) : std::string("touchlog"));
#endif
}


bool Log_Requested(void)
{
	char const * flag = getenv("OPENTS_TOUCH_LOG");

	if (flag != NULL && *flag != '\0' && strcmp(flag, "0") != 0) {
		return(true);
	}

	LPWSTR const command = GetCommandLineW();

	for (LPWSTR cursor = command; cursor != NULL && *cursor != L'\0'; cursor++) {
		if (wcsncmp(cursor, L"-TOUCHLOG", 9) == 0 || wcsncmp(cursor, L"-touchlog", 9) == 0) {
			return(true);
		}
	}

	return(false);
}


FILE * Log_Open(void)
{
	if (_LogChecked) {
		return(_Log);
	}

	_LogChecked = true;

	std::string const directory = Log_Directory();

	if (!Directory_Exists(directory.c_str())) {
		if (!Log_Requested()) {
			return(NULL);
		}
		if (mkdir(directory.c_str(), 0777) != 0 && !Directory_Exists(directory.c_str())) {
			return(NULL);
		}
	}

	std::time_t const stamp = std::time(NULL);
	std::tm parts = {};
	localtime_r(&stamp, &parts);

	char name[64];
	std::snprintf(name, sizeof(name), "/touch-%04d%02d%02d-%02d%02d%02d.log",
		parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
		parts.tm_hour, parts.tm_min, parts.tm_sec);

	_Log = std::fopen((directory + name).c_str(), "w");

	if (_Log != NULL) {
		float width = 0.0f;
		float height = 0.0f;
		bool const sized = Window_Size(width, height);
		std::fprintf(_Log, "window %gx%g points, density %g, dead zone %g points, long press %llu ms\n",
			sized ? width : 0.0f, sized ? height : 0.0f, (double)Win32_Pixel_Density(),
			(double)Dead_Zone(), (unsigned long long)(TOUCH_LONG_PRESS_NS / 1000000ULL));
		std::fflush(_Log);
	}

	return(_Log);
}


// Every line is flushed. A run that ends in a memory kill leaves no crash report, so the
// last line written is often the only evidence of what the gesture was doing.
void Log(char const * format, ...)
{
	FILE * file = Log_Open();

	if (file == NULL) {
		return;
	}

	std::fprintf(file, "%8llu ", (unsigned long long)(SDL_GetTicksNS() / 1000000ULL));

	va_list arguments;
	va_start(arguments, format);
	std::vfprintf(file, format, arguments);
	va_end(arguments);

	std::fputc('\n', file);
	std::fflush(file);
}


void Log_Phase(PhaseType from, PhaseType to, char const * why)
{
	if (from == to) {
		return;
	}

	Log("phase %s -> %s (%s)", Phase_Name(from), Phase_Name(to), why);
}


void Set_Phase(PhaseType phase, char const * why)
{
	Log_Phase(_Phase, phase, why);
	_Phase = phase;
}


float Dead_Zone(void)
{
	return(std::max(TOUCH_DEAD_ZONE_FLOOR, TOUCH_DEAD_ZONE_MM * TOUCH_POINTS_PER_MM));
}


bool Window_Size(float & width, float & height)
{
	Win32Window * main = Win32_Lookup(Win32_Main_Window());

	if (main == NULL || main->Handle == NULL) {
		return(false);
	}

	int w = 0;
	int h = 0;

	if (!SDL_GetWindowSize(main->Handle, &w, &h) || w <= 0 || h <= 0) {
		return(false);
	}

	width = (float)w;
	height = (float)h;
	return(true);
}


void Move_To(float x, float y)
{
	Win32_Pointer_Move(x, y);
	Win32_Post_Pointer_Message(WM_MOUSEMOVE);
	Log("emit WM_MOUSEMOVE at %.1f,%.1f points", x, y);
	_LastX = x;
	_LastY = y;
}


void Park(void)
{
	float width = 0.0f;
	float height = 0.0f;

	if (Window_Size(width, height)) {
		Log("park");
		Move_To(width * 0.5f, height * 0.5f);
	}
}


void Press(Uint8 button)
{
	_HeldButton = button;
	Win32_Pointer_Button(button, true);
	Win32_Post_Pointer_Message(button == SDL_BUTTON_RIGHT ? WM_RBUTTONDOWN : WM_LBUTTONDOWN);
	Log("emit %s at %.1f,%.1f points",
		button == SDL_BUTTON_RIGHT ? "WM_RBUTTONDOWN" : "WM_LBUTTONDOWN", _LastX, _LastY);
}


void Release_Now(Uint8 button)
{
	_HeldButton = 0;
	Win32_Pointer_Button(button, false);
	Win32_Post_Pointer_Message(button == SDL_BUTTON_RIGHT ? WM_RBUTTONUP : WM_LBUTTONUP);
	Log("emit %s at %.1f,%.1f points",
		button == SDL_BUTTON_RIGHT ? "WM_RBUTTONUP" : "WM_LBUTTONUP", _LastX, _LastY);
}


// A release is never dropped or reordered: anything that would dispatch ahead of it flushes
// it first, so the only thing the deferral costs is the frame the pressed state is visible
// for.
void Flush_Release(void)
{
	if (!_Release.Pending) {
		return;
	}

	_Release.Pending = false;
	Release_Now(_Release.Button);
}


void Defer_Release(Uint8 button, Uint64 now)
{
	Flush_Release();
	_Release.Button = button;
	_Release.Due = now + TOUCH_PRESS_FLASH_NS;
	_Release.Pending = true;
}


void Stop_Coast(void)
{
	_Coasting = false;
	_CoastX = 0.0;
	_CoastY = 0.0;
}


void Add_Scroll(float dx, float dy)
{
	// The view travels against the finger, so what is under the finger stays under it.
	double const density = (double)Win32_Pixel_Density();
	_ScrollX -= (double)dx * density;
	_ScrollY -= (double)dy * density;
}


void Centroid(float & x, float & y)
{
	int const count = std::min((int)_Fingers.size(), 2);

	x = 0.0f;
	y = 0.0f;

	if (count == 0) {
		return;
	}

	for (int index = 0; index < count; index++) {
		x += _Fingers[index].X;
		y += _Fingers[index].Y;
	}

	x /= (float)count;
	y /= (float)count;
}


void Sample_Velocity(Uint64 now, float x, float y)
{
	VelocitySample sample;
	sample.Time = now;
	sample.X = x;
	sample.Y = y;
	_Samples.push_back(sample);

	while ((int)_Samples.size() > TOUCH_VELOCITY_SAMPLES) {
		_Samples.erase(_Samples.begin());
	}
}


void Begin_Coast(Uint64 now)
{
	Stop_Coast();

	while (_Samples.size() > 1 && now - _Samples.front().Time > TOUCH_VELOCITY_WINDOW_NS) {
		_Samples.erase(_Samples.begin());
	}

	if (_Samples.size() < 2) {
		_Samples.clear();
		return;
	}

	VelocitySample const first = _Samples.front();
	VelocitySample const last = _Samples.back();
	double const span = (double)(last.Time - first.Time) / 1000000.0;
	_Samples.clear();

	if (span <= 0.0) {
		return;
	}

	_CoastX = (double)(last.X - first.X) / span;
	_CoastY = (double)(last.Y - first.Y) / span;

	double const speed = std::sqrt(_CoastX * _CoastX + _CoastY * _CoastY);

	if (speed < TOUCH_COAST_STOP_POINTS_PER_MS) {
		Log("no coast: released at %.3f points per ms over %.1f ms", speed, span);
		Stop_Coast();
		return;
	}

	Log("coast at %.3f points per ms measured over %.1f ms", speed, span);
	_CoastTime = now;
	_Coasting = true;
}


void Advance_Coast(Uint64 now)
{
	if (!_Coasting) {
		return;
	}

	double const step = (double)(now - _CoastTime) / 1000000.0;
	_CoastTime = now;

	if (step <= 0.0) {
		return;
	}

	Add_Scroll((float)(_CoastX * step), (float)(_CoastY * step));

	double const decay = std::pow(TOUCH_DECAY_PER_MS, step);
	_CoastX *= decay;
	_CoastY *= decay;

	if (std::sqrt(_CoastX * _CoastX + _CoastY * _CoastY) < TOUCH_COAST_STOP_POINTS_PER_MS) {
		Stop_Coast();
	}
}


void End_Gesture(void)
{
	if (_Fingers.empty()) {
		Set_Phase(PHASE_IDLE, "every finger has left");
		_ParkPending = true;
	} else {
		Set_Phase(PHASE_SPENT, "gesture decided, fingers still down");
	}
}


void Begin_Pan(Uint64 now)
{
	if (_Phase == PHASE_DRAG) {
		Log("drag abandoned for a pan");
		Flush_Release();
		Release_Now(SDL_BUTTON_LEFT);
	}

	Stop_Coast();
	_Samples.clear();
	_PanEngaged = false;
	Centroid(_PanX, _PanY);
	Sample_Velocity(now, _PanX, _PanY);
	Set_Phase(PHASE_PAN, "a second finger arrived");
	_ParkPending = true;
}


FingerRecord * Find(SDL_FingerID id)
{
	for (FingerRecord & finger : _Fingers) {
		if (finger.Id == id) {
			return(&finger);
		}
	}

	return(NULL);
}


void Finger_Down(SDL_FingerID id, float x, float y, Uint64 now)
{
	if (Find(id) == NULL) {
		FingerRecord finger;
		finger.Id = id;
		finger.X = x;
		finger.Y = y;
		_Fingers.push_back(finger);
	}

	// A glide is caught the moment the glass is touched, which is what makes a flick feel
	// like something you can stop rather than something you have to wait out.
	Stop_Coast();

	if (_Fingers.size() >= 2) {
		if (_Phase == PHASE_PENDING || _Phase == PHASE_DRAG || _Phase == PHASE_IDLE) {
			Begin_Pan(now);
		}
		return;
	}

	if (_Phase != PHASE_IDLE) {
		return;
	}

	Flush_Release();
	_PressX = x;
	_PressY = y;
	_PressTime = now;
	Set_Phase(_MovieMode ? PHASE_MOVIE : PHASE_PENDING, "first finger down");
}


void Finger_Motion(SDL_FingerID id, float x, float y, Uint64 now)
{
	FingerRecord * finger = Find(id);

	if (finger == NULL) {
		return;
	}

	finger->X = x;
	finger->Y = y;

	if (_Phase == PHASE_PAN) {
		float cx = 0.0f;
		float cy = 0.0f;
		Centroid(cx, cy);
		Sample_Velocity(now, cx, cy);

		if (!_PanEngaged) {
			if (std::hypot(cx - _PanX, cy - _PanY) < Dead_Zone()) {
				return;
			}
			// Measured from the moment it engages, the way UIKit does, so the threshold is
			// not spent as movement.
			_PanEngaged = true;
			_PanX = cx;
			_PanY = cy;
			return;
		}

		Add_Scroll(cx - _PanX, cy - _PanY);

		// The reference is re-taken every motion so that drift over a long drag cannot
		// accumulate into a jump.
		_PanX = cx;
		_PanY = cy;
		return;
	}

	if (_Phase == PHASE_PENDING) {
		if (std::hypot(x - _PressX, y - _PressY) < Dead_Zone()) {
			return;
		}

		Log("drag: moved %.1f points from the press point, past %.1f",
			std::hypot(x - _PressX, y - _PressY), Dead_Zone());
		Flush_Release();
		Move_To(_PressX, _PressY);
		Press(SDL_BUTTON_LEFT);
		Move_To(x, y);
		Set_Phase(PHASE_DRAG, "one finger past the dead zone");
		return;
	}

	if (_Phase == PHASE_DRAG) {
		Move_To(x, y);
	}
}


void Finger_Up(SDL_FingerID id, Uint64 now)
{
	for (auto it = _Fingers.begin(); it != _Fingers.end(); ++it) {
		if (it->Id == id) {
			_Fingers.erase(it);
			break;
		}
	}

	switch (_Phase) {
		case PHASE_PENDING:
			// A tap is delivered where the finger landed rather than where it left, because
			// a finger rolls as it lifts and a dense row of buttons is unforgiving about it.
			Log("tap after %llu ms", (unsigned long long)((now - _PressTime) / 1000000ULL));
			Flush_Release();
			Move_To(_PressX, _PressY);
			Press(SDL_BUTTON_LEFT);
			Defer_Release(SDL_BUTTON_LEFT, now);
			End_Gesture();
			break;

		case PHASE_MOVIE:
			Log("emit ESC to skip the movie");
			Win32_Post_Key_Message(TOUCH_VK_ESCAPE, true);
			Win32_Post_Key_Message(TOUCH_VK_ESCAPE, false);
			End_Gesture();
			break;

		case PHASE_DRAG:
			Flush_Release();
			Release_Now(SDL_BUTTON_LEFT);
			End_Gesture();
			break;

		case PHASE_PAN:
			if (_Fingers.size() < 2) {
				Begin_Coast(now);
				End_Gesture();
			}
			break;

		case PHASE_PRESSED:
		case PHASE_SPENT:
			End_Gesture();
			break;

		default:
			if (_Fingers.empty()) {
				Set_Phase(PHASE_IDLE, "every finger has left");
			}
			break;
	}
}

}	// namespace


void Win32_Touch_Cancel(void)
{
	// Nothing here may touch a pointer this layer is not driving: on a host with a mouse
	// this is reached with a real button held, and releasing it would end the player's drag.
	if (_Phase == PHASE_IDLE && _Fingers.empty() && _HeldButton == 0 && !_Release.Pending && !_Coasting) {
		return;
	}

	Log("cancel");
	Flush_Release();

	if (_HeldButton != 0) {
		Release_Now(_HeldButton);
	}

	_Fingers.clear();
	_Samples.clear();
	Stop_Coast();
	_PanEngaged = false;
	Set_Phase(PHASE_IDLE, "cancelled");
	_ParkPending = true;
}


bool Win32_Touch_Handle_Event(SDL_Event const & event)
{
	switch (event.type) {
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_MOTION:
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_CANCELED:
			break;

		default:
			return(false);
	}

	// A trackpad reports fingers as well, and a trackpad already has a pointer of its own.
	if (SDL_GetTouchDeviceType(event.tfinger.touchID) != SDL_TOUCH_DEVICE_DIRECT) {
		Log("ignored: touch device %llu is not a screen",
			(unsigned long long)event.tfinger.touchID);
		return(false);
	}

	float width = 0.0f;
	float height = 0.0f;

	if (!Window_Size(width, height)) {
		Log("ignored: no window to measure the finger against");
		return(true);
	}

	// Finger positions arrive as a fraction of the window, and the pointer is kept in the
	// window's own logical points.
	float const x = event.tfinger.x * width;
	float const y = event.tfinger.y * height;
	Uint64 const now = event.tfinger.timestamp;

	switch (event.type) {
		case SDL_EVENT_FINGER_DOWN:
			Log("down  finger=%llu at %.1f,%.1f points, %d already down, phase %s",
				(unsigned long long)event.tfinger.fingerID, x, y, (int)_Fingers.size(), Phase_Name(_Phase));
			Finger_Down(event.tfinger.fingerID, x, y, now);
			break;

		case SDL_EVENT_FINGER_MOTION:
			Log("move  finger=%llu at %.1f,%.1f points, phase %s",
				(unsigned long long)event.tfinger.fingerID, x, y, Phase_Name(_Phase));
			Finger_Motion(event.tfinger.fingerID, x, y, now);
			break;

		case SDL_EVENT_FINGER_UP:
			Log("up    finger=%llu at %.1f,%.1f points, phase %s",
				(unsigned long long)event.tfinger.fingerID, x, y, Phase_Name(_Phase));
			Finger_Up(event.tfinger.fingerID, now);
			break;

		case SDL_EVENT_FINGER_CANCELED:
			Log("cancelled finger=%llu, phase %s",
				(unsigned long long)event.tfinger.fingerID, Phase_Name(_Phase));
			Win32_Touch_Cancel();
			break;

		default:
			break;
	}

	return(true);
}


// Runs once per pump. A finger that has stopped moving produces no events at all, so the
// long press has to be looked for rather than waited for, and the same pass is where a
// glide advances and where a state that can no longer be true is put right.
void Win32_Touch_Service(void)
{
	Uint64 const now = SDL_GetTicksNS();

	if (_Release.Pending && now >= _Release.Due) {
		Flush_Release();
	}

	if (_ParkPending && !_Release.Pending && _HeldButton == 0) {
		_ParkPending = false;
		Park();
	}

	if (_Phase == PHASE_PENDING
	&&	!_Fingers.empty()
	&&	now - _PressTime >= TOUCH_LONG_PRESS_NS
	&&	std::hypot(_Fingers.front().X - _PressX, _Fingers.front().Y - _PressY) < Dead_Zone()) {
		Log("long press held %llu ms without leaving the dead zone",
			(unsigned long long)((now - _PressTime) / 1000000ULL));
		Flush_Release();
		Move_To(_PressX, _PressY);
		Press(SDL_BUTTON_RIGHT);
		Defer_Release(SDL_BUTTON_RIGHT, now);
		Set_Phase(PHASE_PRESSED, "long press fired");
	}

	Advance_Coast(now);

	// Nothing here may outlive the fingers that started it. A gesture that ends in a way no
	// phase anticipated would otherwise leave a button held, and the next touch anywhere
	// would drag a selection from wherever the last one was.
	if (_Fingers.empty() && _Phase != PHASE_IDLE) {
		Set_Phase(PHASE_IDLE, "no fingers are down");
	}

	if (_Fingers.empty() && !_Release.Pending && _HeldButton != 0) {
		Log("invariant: a button was still held with no finger on the glass");
		Win32_Touch_Cancel();
	}
}


// True while a fullscreen movie is playing. A tap then means skip and nothing else, and it
// is delivered as the key the skip already waits for, so the vote a network game holds and
// the overlay that explains it both keep working.
void Win32_Touch_Set_Movie_Mode(bool playing)
{
	if (_MovieMode == playing) {
		return;
	}

	_MovieMode = playing;
	Log("movie %s", playing ? "started" : "ended");

	if (playing) {
		Win32_Touch_Cancel();
	}
}


bool Win32_Touch_Take_Scroll(int * x, int * y)
{
	int const dx = (int)_ScrollX;
	int const dy = (int)_ScrollY;

	_ScrollX -= (double)dx;
	_ScrollY -= (double)dy;

	if (x != NULL) *x = dx;
	if (y != NULL) *y = dy;

	if (dx != 0 || dy != 0) {
		Log("scroll taken %d,%d window pixels", dx, dy);
		return(true);
	}

	return(false);
}
