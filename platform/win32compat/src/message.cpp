/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#include <deque>
#include <vector>

// The engine speaks Win32 messages everywhere: the window procedure, the keyboard queue,
// the scroll handler and the tooltip timer all switch on WM_ values, and so do the dialog
// drivers. Translating host events into those messages leaves every one of those call
// sites as it is on Windows, and leaves both builds dispatching the same vocabulary.

static std::deque<MSG> _Queue;
static bool _Quitting;
static int _QuitCode;

struct Win32Timer
{
	HWND Window;
	UINT_PTR Id;
	UINT Interval;
	Uint64 Due;
	TIMERPROC Procedure;
};

static std::vector<Win32Timer> _Timers;


void Win32_Post_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	MSG msg = {};
	msg.hwnd = window;
	msg.message = message;
	msg.wParam = wparam;
	msg.lParam = lparam;
	msg.time = (DWORD)SDL_GetTicks();
	_Queue.push_back(msg);
}


// The host reports mouse positions in its own logical coordinates; the engine measures its
// client area in physical pixels, so a position crosses the density before it is packed
// into the message the way Windows packs it.
static LPARAM Point_To_LParam(float x, float y)
{
	float const density = Win32_Pixel_Density();
	int const px = (int)(x * density);
	int const py = (int)(y * density);
	return(MAKELPARAM((short)px, (short)py));
}


static WPARAM Mouse_Key_State(void)
{
	SDL_MouseButtonFlags const buttons = SDL_GetMouseState(NULL, NULL);
	SDL_Keymod const modifiers = SDL_GetModState();

	WPARAM state = 0;
	if ((buttons & SDL_BUTTON_LMASK) != 0) state |= MK_LBUTTON;
	if ((buttons & SDL_BUTTON_RMASK) != 0) state |= MK_RBUTTON;
	if ((buttons & SDL_BUTTON_MMASK) != 0) state |= MK_MBUTTON;
	if ((modifiers & SDL_KMOD_SHIFT) != 0) state |= MK_SHIFT;
	if ((modifiers & SDL_KMOD_CTRL) != 0) state |= MK_CONTROL;
	return(state);
}


extern int Win32_Virtual_Key(SDL_Scancode scancode, SDL_Keycode keycode);


static void Translate_Event(SDL_Event const & event)
{
	HWND const main = Win32_Main_Window();

	if (main == NULL) {
		return;
	}

	switch (event.type) {
		case SDL_EVENT_QUIT:
			Win32_Post_Message(main, WM_CLOSE, 0, 0);
			break;

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			Win32_Post_Message(main, WM_CLOSE, 0, 0);
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			Win32_Post_Message(main, WM_ACTIVATEAPP, 1, 0);
			Win32_Post_Message(main, WM_SETFOCUS, 0, 0);
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			Win32_Post_Message(main, WM_ACTIVATEAPP, 0, 0);
			Win32_Post_Message(main, WM_KILLFOCUS, 0, 0);
			break;

		case SDL_EVENT_WINDOW_SHOWN:
			Win32_Post_Message(main, WM_SHOWWINDOW, 1, 0);
			break;

		case SDL_EVENT_WINDOW_HIDDEN:
			Win32_Post_Message(main, WM_SHOWWINDOW, 0, 0);
			break;

		case SDL_EVENT_WINDOW_MINIMIZED:
			Win32_Post_Message(main, WM_SIZE, SIZE_MINIMIZED, 0);
			break;

		case SDL_EVENT_WINDOW_RESTORED:
			Win32_Post_Message(main, WM_SIZE, SIZE_RESTORED, 0);
			break;

		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			Win32_Post_Message(main, WM_SIZE, SIZE_RESTORED,
				MAKELPARAM((short)event.window.data1, (short)event.window.data2));
			break;

		case SDL_EVENT_WINDOW_MOVED:
			Win32_Post_Message(main, WM_MOVE, 0, MAKELPARAM((short)event.window.data1, (short)event.window.data2));
			break;

		case SDL_EVENT_WINDOW_EXPOSED:
			Win32_Post_Message(main, WM_PAINT, 0, 0);
			break;

		case SDL_EVENT_MOUSE_MOTION:
			Win32_Post_Message(main, WM_MOUSEMOVE, Mouse_Key_State(),
				Point_To_LParam(event.motion.x, event.motion.y));
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			bool const down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
			bool const doubled = down && event.button.clicks >= 2;
			UINT message = 0;

			switch (event.button.button) {
				case SDL_BUTTON_LEFT:
					message = down ? (doubled ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN) : WM_LBUTTONUP;
					break;
				case SDL_BUTTON_RIGHT:
					message = down ? (doubled ? WM_RBUTTONDBLCLK : WM_RBUTTONDOWN) : WM_RBUTTONUP;
					break;
				case SDL_BUTTON_MIDDLE:
					message = down ? (doubled ? WM_MBUTTONDBLCLK : WM_MBUTTONDOWN) : WM_MBUTTONUP;
					break;
				default:
					return;
			}

			Win32_Post_Message(main, message, Mouse_Key_State(), Point_To_LParam(event.button.x, event.button.y));
			break;
		}

		case SDL_EVENT_MOUSE_WHEEL: {
			// Windows reports the wheel in notch multiples in the high word, and the
			// position in screen rather than client coordinates.
			int const notches = (int)(event.wheel.y * 120.0f);
			float mx = 0.0f;
			float my = 0.0f;
			SDL_GetGlobalMouseState(&mx, &my);
			Win32_Post_Message(main, WM_MOUSEWHEEL,
				MAKEWPARAM((WORD)Mouse_Key_State(), (WORD)(short)notches), Point_To_LParam(mx, my));
			break;
		}

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP: {
			int const key = Win32_Virtual_Key(event.key.scancode, event.key.key);
			if (key == 0) {
				return;
			}

			bool const down = event.type == SDL_EVENT_KEY_DOWN;
			bool const system = (event.key.mod & SDL_KMOD_ALT) != 0;
			UINT const message = down
				? (system ? WM_SYSKEYDOWN : WM_KEYDOWN)
				: (system ? WM_SYSKEYUP : WM_KEYUP);

			// The repeat count, the scan code and the transition bit occupy the same
			// places in the parameter that Windows puts them in.
			LPARAM lparam = 1;
			lparam |= (LPARAM)(event.key.scancode & 0xFF) << 16;
			if (!down) lparam |= (LPARAM)3 << 30;

			Win32_Post_Message(main, message, (WPARAM)key, lparam);
			break;
		}

		case SDL_EVENT_TEXT_INPUT: {
			for (char const * cursor = event.text.text; cursor != NULL && *cursor != '\0'; cursor++) {
				Win32_Post_Message(main, WM_CHAR, (WPARAM)(unsigned char)*cursor, 1);
			}
			break;
		}

		default:
			break;
	}
}


static void Service_Timers(void)
{
	Uint64 const now = SDL_GetTicks();

	for (Win32Timer & timer : _Timers) {
		if (now >= timer.Due) {
			timer.Due = now + timer.Interval;
			Win32_Post_Message(timer.Window, WM_TIMER, (WPARAM)timer.Id, (LPARAM)timer.Procedure);
		}
	}
}


void Win32_Pump_Host_Events(void)
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		return;
	}

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		Translate_Event(event);
	}

	Service_Timers();
}


static bool Matches_Filter(MSG const & msg, HWND window, UINT filtermin, UINT filtermax)
{
	if (window != NULL && msg.hwnd != window) {
		return(false);
	}

	if (filtermin == 0 && filtermax == 0) {
		return(true);
	}

	return(msg.message >= filtermin && msg.message <= filtermax);
}


extern "C" BOOL PeekMessage(LPMSG msg, HWND window, UINT filtermin, UINT filtermax, UINT remove)
{
	Win32_Pump_Host_Events();

	for (auto it = _Queue.begin(); it != _Queue.end(); ++it) {
		if (!Matches_Filter(*it, window, filtermin, filtermax)) {
			continue;
		}

		if (msg != NULL) {
			*msg = *it;
		}

		if ((remove & PM_REMOVE) != 0) {
			_Queue.erase(it);
		}

		return(TRUE);
	}

	return(FALSE);
}


// The engine drives its own frame, so a wait for a message must never block: every caller
// reaches this from inside a loop that also has drawing and simulation to do.
extern "C" BOOL GetMessage(LPMSG msg, HWND window, UINT filtermin, UINT filtermax)
{
	if (!PeekMessage(msg, window, filtermin, filtermax, PM_REMOVE)) {
		return(FALSE);
	}

	return(msg != NULL && msg->message == WM_QUIT ? FALSE : TRUE);
}


extern "C" BOOL TranslateMessage(MSG const * msg)
{
	// Character messages arrive from the host's own text input, so a key message needs no
	// second pass to produce one.
	(void)msg;
	return(FALSE);
}


extern "C" LRESULT DispatchMessage(MSG const * msg)
{
	if (msg == NULL) {
		return(0);
	}

	Win32Window * window = Win32_Lookup(msg->hwnd);

	if (window == NULL || window->Procedure == NULL) {
		return(0);
	}

	return(window->Procedure(msg->hwnd, msg->message, msg->wParam, msg->lParam));
}


extern "C" BOOL PostMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	Win32_Post_Message(window, message, wparam, lparam);
	return(TRUE);
}


extern "C" LRESULT SendMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	Win32Window * record = Win32_Lookup(window);

	if (record == NULL || record->Procedure == NULL) {
		return(0);
	}

	return(record->Procedure(window, message, wparam, lparam));
}


extern "C" void PostQuitMessage(int code)
{
	_Quitting = true;
	_QuitCode = code;
	Win32_Post_Message(Win32_Main_Window(), WM_QUIT, (WPARAM)code, 0);
}


extern "C" LRESULT DefWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	(void)window;
	(void)wparam;
	(void)lparam;

	switch (message) {
		case WM_NCHITTEST:
			return(HTCLIENT);

		case WM_SETCURSOR:
			return(TRUE);

		default:
			return(0);
	}
}


extern "C" LRESULT CallWindowProc(WNDPROC proc, HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (proc == NULL) {
		return(DefWindowProc(window, message, wparam, lparam));
	}

	return(proc(window, message, wparam, lparam));
}


extern "C" int TranslateAccelerator(HWND window, HACCEL table, LPMSG msg)
{
	(void)window;
	(void)table;
	(void)msg;
	return(0);
}


extern "C" UINT_PTR SetTimer(HWND window, UINT_PTR id, UINT elapse, TIMERPROC proc)
{
	for (Win32Timer & timer : _Timers) {
		if (timer.Window == window && timer.Id == id) {
			timer.Interval = elapse;
			timer.Due = SDL_GetTicks() + elapse;
			timer.Procedure = proc;
			return(id);
		}
	}

	Win32Timer timer;
	timer.Window = window;
	timer.Id = id;
	timer.Interval = elapse;
	timer.Due = SDL_GetTicks() + elapse;
	timer.Procedure = proc;
	_Timers.push_back(timer);
	return(id);
}


extern "C" BOOL KillTimer(HWND window, UINT_PTR id)
{
	for (auto it = _Timers.begin(); it != _Timers.end(); ++it) {
		if (it->Window == window && it->Id == id) {
			_Timers.erase(it);
			return(TRUE);
		}
	}

	return(FALSE);
}
