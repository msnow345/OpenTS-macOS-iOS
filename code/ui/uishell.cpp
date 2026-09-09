/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The UI shell. It owns the RmlUi context, the ImGui context, the overlay pass, the input
// hook and the modal runner, and it is the only place outside code/ui that any of those
// libraries is reachable from.
//
// docs/UI_DESIGN.md owns the architecture. Nothing here knows what a screen means: it
// knows which presentation owns a region and an input scope, and no more.

#include "always.h"

#include "uishell.h"

#include "uiinternal.h"
#include "uirmlview.h"

#include "_keyboar.h"
#include "dbgprint.h"
#include "hostclock.h"
#include "conquer.h"
#include "goptions.h"
#include "keyboard.h"
#include "mainloop.h"
#include "msgloop.h"
#include "session.h"
#include "vidscale.h"
#include "video.h"

#include <RmlUi/Core.h>

#include <imgui.h>

#include <windows.h>
#include <windowsx.h>

#include <algorithm>


static bool _Initialized = false;
static Rml::Context * _Context = nullptr;
static bool _OverlayIsDirty = false;
static unsigned int _LastTickTime = 0;

// Set while a document is being shown or hidden, so the message pump that the keyboard
// queue's own cleanup runs cannot re-enter the screen it is closing.
static bool _Changing = false;

// How many exclusive modal documents are shown. A modal takes every mouse and key message
// the way IgnoreInput does around a legacy dialog, and screens nest, so this counts rather
// than flags.
static int _ModalDepth = 0;

// How many modal runners are on the stack. A runner owns the context between its own
// passes, so the tick that Main_Loop and Call_Back make from inside one is dropped rather
// than updating the context a second time in the same pass.
static int _RunningModal = 0;


// The window holds the mouse capture while a gesture a toolkit consumed is in progress.
// The owner of a press owns its release, so a press that crossed into the game or out of
// it still completes where it started.
static int _CaptureButton = -1;

#ifndef NDEBUG
static Rml::ElementDocument * _TestDocument = nullptr;
#endif


// The keys RmlUi names that are not part of one of the four runs above. The pairing is
// read in both directions, so an entry's order matters where two virtual keys share an
// identifier or two identifiers share a virtual key: the first entry naming a virtual key
// is the identifier that key produces, and the first entry naming an identifier is the
// virtual key it converts back to.
//
// Where a modifier has both a general and a sided code the general one comes first, because
// that is what this engine's keyboard queue is written against: keyboard.h has no name for
// 0xA0 to 0xA5 at all, and platform/win32compat reports 0x10, 0x11 and 0x12 for both sides.
struct KeyPairType
{
	unsigned short VirtualKey;
	Rml::Input::KeyIdentifier Identifier;
};

static KeyPairType const _KeyPairs[] = {
	{ 0x08, Rml::Input::KI_BACK },
	{ 0x09, Rml::Input::KI_TAB },
	{ 0x0C, Rml::Input::KI_CLEAR },
	{ 0x0D, Rml::Input::KI_RETURN },
	{ 0x10, Rml::Input::KI_LSHIFT },
	{ 0x11, Rml::Input::KI_LCONTROL },
	{ 0x12, Rml::Input::KI_LMENU },
	{ 0x13, Rml::Input::KI_PAUSE },
	{ 0x14, Rml::Input::KI_CAPITAL },
	{ 0x15, Rml::Input::KI_KANA },
	{ 0x15, Rml::Input::KI_HANGUL },
	{ 0x17, Rml::Input::KI_JUNJA },
	{ 0x18, Rml::Input::KI_FINAL },
	{ 0x19, Rml::Input::KI_HANJA },
	{ 0x19, Rml::Input::KI_KANJI },
	{ 0x1B, Rml::Input::KI_ESCAPE },
	{ 0x1C, Rml::Input::KI_CONVERT },
	{ 0x1D, Rml::Input::KI_NONCONVERT },
	{ 0x1E, Rml::Input::KI_ACCEPT },
	{ 0x1F, Rml::Input::KI_MODECHANGE },
	{ 0x20, Rml::Input::KI_SPACE },
	{ 0x21, Rml::Input::KI_PRIOR },
	{ 0x22, Rml::Input::KI_NEXT },
	{ 0x23, Rml::Input::KI_END },
	{ 0x24, Rml::Input::KI_HOME },
	{ 0x25, Rml::Input::KI_LEFT },
	{ 0x26, Rml::Input::KI_UP },
	{ 0x27, Rml::Input::KI_RIGHT },
	{ 0x28, Rml::Input::KI_DOWN },
	{ 0x29, Rml::Input::KI_SELECT },
	{ 0x2A, Rml::Input::KI_PRINT },
	{ 0x2B, Rml::Input::KI_EXECUTE },
	{ 0x2C, Rml::Input::KI_SNAPSHOT },
	{ 0x2D, Rml::Input::KI_INSERT },
	{ 0x2E, Rml::Input::KI_DELETE },
	{ 0x2F, Rml::Input::KI_HELP },
	{ 0x5B, Rml::Input::KI_LWIN },
	{ 0x5C, Rml::Input::KI_RWIN },
	{ 0x5D, Rml::Input::KI_APPS },
	{ 0x5F, Rml::Input::KI_SLEEP },
	{ 0x6A, Rml::Input::KI_MULTIPLY },
	{ 0x6B, Rml::Input::KI_ADD },
	{ 0x6C, Rml::Input::KI_SEPARATOR },
	{ 0x6D, Rml::Input::KI_SUBTRACT },
	{ 0x6E, Rml::Input::KI_DECIMAL },
	{ 0x6F, Rml::Input::KI_DIVIDE },
	{ 0x90, Rml::Input::KI_NUMLOCK },
	{ 0x91, Rml::Input::KI_SCROLL },
	{ 0x92, Rml::Input::KI_OEM_NEC_EQUAL },
	{ 0x92, Rml::Input::KI_OEM_FJ_JISHO },
	{ 0x93, Rml::Input::KI_OEM_FJ_MASSHOU },
	{ 0x94, Rml::Input::KI_OEM_FJ_TOUROKU },
	{ 0x95, Rml::Input::KI_OEM_FJ_LOYA },
	{ 0x96, Rml::Input::KI_OEM_FJ_ROYA },
	{ 0xA0, Rml::Input::KI_LSHIFT },
	{ 0xA1, Rml::Input::KI_RSHIFT },
	{ 0xA2, Rml::Input::KI_LCONTROL },
	{ 0xA3, Rml::Input::KI_RCONTROL },
	{ 0xA4, Rml::Input::KI_LMENU },
	{ 0xA5, Rml::Input::KI_RMENU },
	{ 0xA6, Rml::Input::KI_BROWSER_BACK },
	{ 0xA7, Rml::Input::KI_BROWSER_FORWARD },
	{ 0xA8, Rml::Input::KI_BROWSER_REFRESH },
	{ 0xA9, Rml::Input::KI_BROWSER_STOP },
	{ 0xAA, Rml::Input::KI_BROWSER_SEARCH },
	{ 0xAB, Rml::Input::KI_BROWSER_FAVORITES },
	{ 0xAC, Rml::Input::KI_BROWSER_HOME },
	{ 0xAD, Rml::Input::KI_VOLUME_MUTE },
	{ 0xAE, Rml::Input::KI_VOLUME_DOWN },
	{ 0xAF, Rml::Input::KI_VOLUME_UP },
	{ 0xB0, Rml::Input::KI_MEDIA_NEXT_TRACK },
	{ 0xB1, Rml::Input::KI_MEDIA_PREV_TRACK },
	{ 0xB2, Rml::Input::KI_MEDIA_STOP },
	{ 0xB3, Rml::Input::KI_MEDIA_PLAY_PAUSE },
	{ 0xB4, Rml::Input::KI_LAUNCH_MAIL },
	{ 0xB5, Rml::Input::KI_LAUNCH_MEDIA_SELECT },
	{ 0xB6, Rml::Input::KI_LAUNCH_APP1 },
	{ 0xB7, Rml::Input::KI_LAUNCH_APP2 },
	{ 0xBA, Rml::Input::KI_OEM_1 },
	{ 0xBB, Rml::Input::KI_OEM_PLUS },
	{ 0xBC, Rml::Input::KI_OEM_COMMA },
	{ 0xBD, Rml::Input::KI_OEM_MINUS },
	{ 0xBE, Rml::Input::KI_OEM_PERIOD },
	{ 0xBF, Rml::Input::KI_OEM_2 },
	{ 0xC0, Rml::Input::KI_OEM_3 },
	{ 0xDB, Rml::Input::KI_OEM_4 },
	{ 0xDC, Rml::Input::KI_OEM_5 },
	{ 0xDD, Rml::Input::KI_OEM_6 },
	{ 0xDE, Rml::Input::KI_OEM_7 },
	{ 0xDF, Rml::Input::KI_OEM_8 },
	{ 0xE1, Rml::Input::KI_OEM_AX },
	{ 0xE2, Rml::Input::KI_OEM_102 },
	{ 0xE3, Rml::Input::KI_ICO_HELP },
	{ 0xE4, Rml::Input::KI_ICO_00 },
	{ 0xE5, Rml::Input::KI_PROCESSKEY },
	{ 0xE6, Rml::Input::KI_ICO_CLEAR },
	{ 0xF6, Rml::Input::KI_ATTN },
	{ 0xF7, Rml::Input::KI_CRSEL },
	{ 0xF8, Rml::Input::KI_EXSEL },
	{ 0xF9, Rml::Input::KI_EREOF },
	{ 0xFA, Rml::Input::KI_PLAY },
	{ 0xFB, Rml::Input::KI_ZOOM },
	{ 0xFD, Rml::Input::KI_PA1 },
	{ 0xFE, Rml::Input::KI_OEM_CLEAR },
};


/// <summary>
/// Turns a Win32 virtual key into the identifier RmlUi names it by.
/// Every key RmlUi has a name for is mapped, because a key nothing maps is never delivered
/// to a document at all and a screen that binds a shortcut has to see the whole keyboard.
/// </summary>
static Rml::Input::KeyIdentifier Key_Identifier(WPARAM key)
{
	using namespace Rml::Input;

	// The four runs where the two enumerations march in step.
	if (key >= 'A' && key <= 'Z') {
		return((KeyIdentifier)(KI_A + (int)(key - 'A')));
	}
	if (key >= '0' && key <= '9') {
		return((KeyIdentifier)(KI_0 + (int)(key - '0')));
	}
	if (key >= 0x60 && key <= 0x69) {
		return((KeyIdentifier)(KI_NUMPAD0 + (int)(key - 0x60)));
	}
	if (key >= VK_F1 && key <= VK_F24) {
		return((KeyIdentifier)(KI_F1 + (int)(key - VK_F1)));
	}

	for (KeyPairType const & pair : _KeyPairs) {
		if (pair.VirtualKey == key) {
			return(pair.Identifier);
		}
	}

	return(KI_UNKNOWN);
}


/// <summary>
/// Turns an identifier RmlUi reports back into the Win32 virtual key it came from.
/// A screen that records a keypress needs this, because an RmlUi key event carries the
/// identifier and the game's own encoding is a virtual key.
/// </summary>
/// <returns>int; The virtual key, or zero for an identifier no key produces.</returns>
int UI_Virtual_Key(int identifier)
{
	using namespace Rml::Input;

	if (identifier >= KI_A && identifier <= KI_Z) {
		return('A' + (identifier - KI_A));
	}
	if (identifier >= KI_0 && identifier <= KI_9) {
		return('0' + (identifier - KI_0));
	}
	if (identifier >= KI_NUMPAD0 && identifier <= KI_NUMPAD9) {
		return(0x60 + (identifier - KI_NUMPAD0));
	}
	if (identifier >= KI_F1 && identifier <= KI_F24) {
		return(VK_F1 + (identifier - KI_F1));
	}

	// The numeric keypad's Enter is a Return as far as Win32 is concerned; only the
	// extended-key bit in a message's own parameters tells them apart, and that bit is
	// gone by the time RmlUi has named the key.
	if (identifier == KI_NUMPADENTER) {
		return(VK_RETURN);
	}

	for (KeyPairType const & pair : _KeyPairs) {
		if (pair.Identifier == identifier) {
			return(pair.VirtualKey);
		}
	}

	return(0);
}


static int Key_Modifiers(void)
{
	int modifiers = 0;

	if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= Rml::Input::KM_CTRL;
	if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= Rml::Input::KM_SHIFT;
	if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= Rml::Input::KM_ALT;

	return(modifiers);
}


/// <summary>
/// Converts a position in the window's client area into the overlay's own space.
/// The overlay is laid out in physical pixels measured from the frame's top left corner,
/// so the letterbox bars fall outside it and never become an edge click.
/// </summary>
static void Client_Point_To_Overlay(POINT & point)
{
	VideoScaleInfo const & scale = Video_Get_Scale_Info();

	point.x -= scale.DestX;
	point.y -= scale.DestY;
}


/// <summary>
/// Reads the position a mouse message carries, in the overlay's space.
/// The wheel reports in screen coordinates and everything else in the client area, which
/// is the one difference the conversion has to make.
/// </summary>
static POINT Message_Point(UINT message, LPARAM lparam)
{
	POINT point;
	point.x = GET_X_LPARAM(lparam);
	point.y = GET_Y_LPARAM(lparam);

	if (message == WM_MOUSEWHEEL) {
		ScreenToClient(MainWindow, &point);
	}

	Client_Point_To_Overlay(point);
	return(point);
}


/// <summary>
/// Records that something the shell draws has to be put on screen again.
/// Only the overlay's flag is set. The game's frame is left alone so that a present made
/// for a document costs the overlay's draw calls and not the frame's pixels; both resize
/// paths mark the frame themselves, because a new target needs the frame uploaded again.
/// </summary>
static void Mark_Overlay_Dirty(void)
{
	_OverlayIsDirty = true;
}


/// <summary>
/// Points the context at where the frame lands and at the scale it is drawn.
/// One authored density-independent pixel is one game logical unit, so a document authored
/// at a legacy dialog's size keeps that size on screen while its text is rasterized at the
/// physical resolution.
/// </summary>
static void Apply_Scale_Info(void)
{
	if (_Context == nullptr) {
		return;
	}

	VideoScaleInfo const & scale = Video_Get_Scale_Info();
	if (scale.DestWidth <= 0 || scale.DestHeight <= 0) {
		return;
	}

	_Context->SetDimensions(Rml::Vector2i(scale.DestWidth, scale.DestHeight));
	_Context->SetDensityIndependentPixelRatio(std::min(scale.ScaleX, scale.ScaleY));
	Mark_Overlay_Dirty();
}


/// <summary>
/// Starts the shell on the running renderer.
/// </summary>
/// <returns>bool; Is the shell ready? A false return leaves the game running without one.</returns>
bool UI_Init(void)
{
	if (_Initialized) {
		return(true);
	}

	if (!UI_Render_Init()) {
		DebugString("[UI] The overlay renderer could not be started.\n");
		return(false);
	}

	Rml::SetSystemInterface(UI_System_Interface());
	Rml::SetFileInterface(UI_File_Interface());
	Rml::SetRenderInterface(UI_Render_Interface());

	if (!Rml::Initialise()) {
		DebugString("[UI] RmlUi could not be started.\n");
		UI_Render_Shutdown();
		return(false);
	}

	VideoScaleInfo const & scale = Video_Get_Scale_Info();
	_Context = Rml::CreateContext("game",
		Rml::Vector2i(std::max(scale.DestWidth, 1), std::max(scale.DestHeight, 1)));

	if (_Context == nullptr) {
		DebugString("[UI] The RmlUi context could not be created.\n");
		Rml::Shutdown();
		UI_Render_Shutdown();
		return(false);
	}

	// The shipped font loads by bare name, so it is found in ui/ or in a mix on the same
	// terms as everything else a document names.
	if (!Rml::LoadFontFace("LatoLatin-Regular.ttf")) {
		DebugString("[UI] The shipped font could not be loaded.\n");
	}

	UI_Surface_Element_Init();
	UI_Dev_Init();

	Apply_Scale_Info();
	_LastTickTime = Host_Milliseconds();
	_Initialized = true;

	DebugString("[UI] Shell started at %dx%d.\n", scale.DestWidth, scale.DestHeight);
	return(true);
}


void UI_Shutdown(void)
{
	if (!_Initialized) {
		return;
	}

	_Changing = true;

#ifndef NDEBUG
	_TestDocument = nullptr;
#endif

	UI_Dev_Shutdown();
	UI_Surface_Element_Shutdown();

	_Context = nullptr;
	Rml::Shutdown();
	UI_Render_Shutdown();

	_Initialized = false;
	_OverlayIsDirty = false;
	_CaptureButton = -1;
	_Changing = false;
}


void UI_On_Resize(void)
{
	if (!_Initialized) {
		return;
	}

	Apply_Scale_Info();
}


/// <summary>
/// Advances layout and animation for documents no modal loop is running.
/// Called from Main_Loop beside Map.Input, on wall-clock time, so nothing here reads or
/// advances a deterministic game timer.
/// </summary>
void UI_Tick(void)
{

	// The service points nest: a dialog driver's Call_Back runs inside a loop that already
	// ticked. A nested request is dropped rather than updating the context twice, which is
	// what keeps a pump reached from inside an update out of it.
	static bool ticking = false;

	if (!_Initialized || _Context == nullptr || _Changing || ticking || _RunningModal > 0) {
		return;
	}

	ticking = true;

	unsigned int const now = Host_Milliseconds();
	double const elapsed = (double)(now - _LastTickTime) / 1000.0;
	_LastTickTime = now;

	_Context->Update();
	UI_Message_Box_Service();

	// RmlUi cannot say whether it needs redrawing, so anything on screen marks the overlay
	// on every tick and the present pacing caps the rate.
	if (_Context->GetNumDocuments() > 0 || UI_Dev_Is_Open()) {
		Mark_Overlay_Dirty();
	}

	if (UI_Dev_Is_Open()) {
		VideoScaleInfo const & scale = Video_Get_Scale_Info();
		UI_Dev_New_Frame(scale.DestWidth, scale.DestHeight, elapsed);
	}

	ticking = false;
}


void UI_Render_Overlay(void)
{
	if (!_Initialized || _Context == nullptr) {
		return;
	}

	VideoScaleInfo const & scale = Video_Get_Scale_Info();
	UI_Render_Begin(scale.DestX, scale.DestY, scale.DestWidth, scale.DestHeight);

	_Context->Render();
	UI_Dev_Render();

	UI_Render_End();
	_OverlayIsDirty = false;
}


bool UI_Overlay_Is_Dirty(void)
{
	return(_OverlayIsDirty);
}


/// <summary>
/// Lays out and presents what the shell draws.
/// A screen that has no loop of its own -- the wait box, the progress box -- is on screen
/// only when something else pumps, so this is the equivalent of the synchronous WM_PAINT
/// those boxes were repainted with.
/// </summary>
/// <param name="immediate">Present whether or not the pacing is ready for another frame.
/// A box that must be seen before a long operation begins gets no second chance.</param>
void UI_Paint_Now(bool immediate)
{
	UI_Tick();

	if (immediate) {
		Video_Present();
	} else {
		Video_Present_If_Dirty();
	}
}


bool UI_Document_Is_Visible(void)
{
	return(_Initialized && _Context != nullptr && _Context->GetNumDocuments() > 0);
}


/// <summary>
/// Should a migrated screen use its RmlUi view rather than its legacy one?
/// The answer is latched at screen entry, never mid-gesture, and LegacyDialogs in SUN.INI
/// returns every migrated screen to the view it replaced for as long as one exists. The key
/// and this function both go when OwnerDraw does.
/// </summary>
bool UI_Use_Rml(void)
{
	return(_Initialized && _Context != nullptr && !Options.LegacyDialogs);
}


#ifndef NDEBUG
/// <summary>
/// Shows or hides the document that proves the shell renders, clips and takes input.
/// Debug builds only; it exists to be looked at, not to be shipped.
/// </summary>
static void Toggle_Test_Document(void)
{
	if (_Context == nullptr) {
		return;
	}

	_Changing = true;

	if (_TestDocument != nullptr) {
		_TestDocument->Close();
		_TestDocument = nullptr;
		DebugString("[UI] Test document closed.\n");
	} else {
		_TestDocument = _Context->LoadDocument("uitest.rml");
		if (_TestDocument != nullptr) {
			_TestDocument->Show();
			DebugString("[UI] Test document shown.\n");
		} else {
			DebugString("[UI] Test document uitest.rml could not be loaded.\n");
		}
	}

	_Changing = false;

	// Closing marks the overlay too, so the pixels a hidden document left behind go away.
	Mark_Overlay_Dirty();
}


/// <summary>
/// Answers the developer keys the shell owns.
/// </summary>
/// <returns>bool; Was the key one of them?</returns>
static bool Handle_Developer_Key(WPARAM key)
{
	if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 || (GetKeyState(VK_SHIFT) & 0x8000) == 0) {
		return(false);
	}

	switch (key) {
		case 'U':
			Toggle_Test_Document();
			return(true);

		case 'I':
			UI_Dev_Toggle();
			Mark_Overlay_Dirty();
			return(true);

		default:
			return(false);
	}
}
#endif


/// <summary>
/// Opens an exclusive input scope for a modal document.
/// The keyboard queue is cleared so a key pressed before the screen opened cannot be read
/// by whatever runs underneath it, and the screen is marked changing first so that the
/// message pump inside Keyboard->Clear() cannot re-enter it.
/// </summary>
static void Enter_Modal_Scope(void)
{
	bool const changing = _Changing;
	_Changing = true;

	_ModalDepth++;

	if (Keyboard != nullptr) {
		Keyboard->Clear();
	}

	_Changing = changing;
}


/// <summary>
/// Closes the input scope a modal document opened, dropping any capture it still holds.
/// </summary>
static void Leave_Modal_Scope(void)
{
	if (_ModalDepth <= 0) {
		return;
	}

	bool const changing = _Changing;
	_Changing = true;

	_ModalDepth--;

	if (_CaptureButton != -1) {
		_CaptureButton = -1;
		if (GetCapture() == MainWindow) {
			ReleaseCapture();
		}
	}

	if (Keyboard != nullptr) {
		Keyboard->Clear();
	}

	_Changing = changing;
}


/// <summary>
/// Offers a window message to the toolkits before the game sees it.
/// The order follows docs/UI_DESIGN.md: ImGui's capture flags first, then a modal
/// document, then whatever an element under the cursor claims. A modal document takes
/// every mouse and key message, which is what IgnoreInput does around a legacy dialog.
/// With none shown a mouse move is always delivered and never consumed, so the game keeps
/// tracking the cursor underneath.
/// </summary>
/// <returns>bool; Was the message consumed? The window procedure returns without handling
/// it when so, which is what keeps it out of the keyboard queue.</returns>
bool UI_Handle_Window_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (!_Initialized || _Context == nullptr || _Changing || window != MainWindow) {
		return(false);
	}

	int const modifiers = Key_Modifiers();

	switch (message) {
		case WM_MOUSEMOVE: {
			POINT const point = Message_Point(message, lparam);
			UI_Dev_Mouse_Position((float)point.x, (float)point.y);
			_Context->ProcessMouseMove((int)point.x, (int)point.y, modifiers);
			return(_ModalDepth > 0);
		}

		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK: {
			int const button = (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) ? 0
				: ((message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) ? 1 : 2);

			POINT const point = Message_Point(message, lparam);
			_Context->ProcessMouseMove((int)point.x, (int)point.y, modifiers);
			UI_Dev_Mouse_Position((float)point.x, (float)point.y);
			UI_Dev_Mouse_Button(button, true);

			if (UI_Dev_Wants_Mouse()) {
				return(true);
			}

			// A false return means the press reached an element, so the game must not see
			// it. The press then owns its release wherever the cursor ends up.
			bool const consumed = !_Context->ProcessMouseButtonDown(button, modifiers) || _ModalDepth > 0;
			if (consumed) {
				_CaptureButton = button;
				SetCapture(MainWindow);
			}
			return(consumed);
		}

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP: {
			int const button = (message == WM_LBUTTONUP) ? 0 : ((message == WM_RBUTTONUP) ? 1 : 2);

			POINT const point = Message_Point(message, lparam);
			_Context->ProcessMouseMove((int)point.x, (int)point.y, modifiers);
			UI_Dev_Mouse_Position((float)point.x, (float)point.y);
			UI_Dev_Mouse_Button(button, false);

			bool const owned = (_CaptureButton == button);
			bool const consumed = !_Context->ProcessMouseButtonUp(button, modifiers);

			if (owned) {
				_CaptureButton = -1;
				if (GetCapture() == MainWindow) {
					ReleaseCapture();
				}
				return(true);
			}

			return(_ModalDepth > 0 || UI_Dev_Wants_Mouse() || consumed);
		}

		case WM_MOUSEWHEEL: {
			POINT const point = Message_Point(message, lparam);
			float const notches = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;

			_Context->ProcessMouseMove((int)point.x, (int)point.y, modifiers);
			UI_Dev_Mouse_Wheel(notches);

			if (UI_Dev_Wants_Mouse()) {
				return(true);
			}

			return(_ModalDepth > 0 || !_Context->ProcessMouseWheel(-notches, modifiers));
		}

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN: {
#ifndef NDEBUG
			if (Handle_Developer_Key(wparam)) {
				return(true);
			}
#endif
			if (UI_Dev_Wants_Keyboard()) {
				return(true);
			}

			Rml::Input::KeyIdentifier const identifier = Key_Identifier(wparam);
			if (identifier == Rml::Input::KI_UNKNOWN) {
				return(_ModalDepth > 0);
			}

			return(_ModalDepth > 0 || !_Context->ProcessKeyDown(identifier, modifiers));
		}

		case WM_KEYUP:
		case WM_SYSKEYUP: {
			Rml::Input::KeyIdentifier const identifier = Key_Identifier(wparam);
			if (identifier == Rml::Input::KI_UNKNOWN) {
				return(_ModalDepth > 0);
			}

			bool const consumed = !_Context->ProcessKeyUp(identifier, modifiers);
			return(_ModalDepth > 0 || UI_Dev_Wants_Keyboard() || consumed);
		}

		case WM_CHAR: {
			if (UI_Dev_Wants_Keyboard()) {
				return(true);
			}

			// Consuming the physical key never suppresses the text it generated, so this
			// is decided on its own.
			if (wparam < 32) {
				return(_ModalDepth > 0);
			}

			return(_ModalDepth > 0 || !_Context->ProcessTextInput((Rml::Character)wparam));
		}

		default:
			return(false);
	}
}


//---------------------------------------------------------------------------------------
// The RmlUi view base. It lives here rather than in a file of its own because the context
// it attaches to is the shell's.
//---------------------------------------------------------------------------------------

UIRmlViewClass::UIRmlViewClass(UIPresenterClass & presenter, char const * document) :
	Presenter(presenter),
	Document(document != nullptr ? document : ""),
	ModelName(Document)
{
	Rml::String::size_type const dot = ModelName.rfind('.');
	if (dot != Rml::String::npos) {
		ModelName.erase(dot);
	}
}


UIRmlViewClass::~UIRmlViewClass(void)
{
	Close();
}


/// <summary>
/// Loads the document, binds its data model and shows it.
/// Preparation completes before the view becomes interactive, so a failure here leaves
/// nothing shown and the caller opens the legacy view instead.
/// </summary>
/// <param name="modal">Should the document take focus away from every other one?</param>
/// <returns>bool; Is the view ready to be interacted with?</returns>
bool UIRmlViewClass::Prepare(bool modal)
{
	if (_Context == nullptr || Element != nullptr) {
		return(false);
	}

	Rml::DataModelConstructor constructor = _Context->CreateDataModel(ModelName);
	if (!constructor) {
		DebugString("[UI] The data model for %s could not be created.\n", Document.c_str());
		return(false);
	}

	Bind(constructor);
	Model = constructor.GetModelHandle();

	Element = _Context->LoadDocument(Document);
	if (Element == nullptr) {
		_Context->RemoveDataModel(ModelName);
		DebugString("[UI] The document %s could not be loaded.\n", Document.c_str());
		return(false);
	}

	Element->Show(modal ? Rml::ModalFlag::Modal : Rml::ModalFlag::None);

	if (modal) {
		IsModal = true;
		Enter_Modal_Scope();
	}

	Mark_Overlay_Dirty();
	return(true);
}


/// <summary>
/// Takes the document off the screen without releasing it.
/// A screen this one opens draws where this one is, and the coexistence rule in
/// docs/UI_DESIGN.md wants only one presentation over a region; the modal scope goes with
/// it, so the screen underneath takes input while it is away.
/// </summary>
void UIRmlViewClass::Hide(void)
{
	if (Element == nullptr || !Element->IsVisible()) {
		return;
	}

	Element->Hide();

	if (IsModal) {
		Leave_Modal_Scope();
	}

	Mark_Overlay_Dirty();
}


/// <summary>
/// Puts the document back on the screen with the scope it had.
/// </summary>
void UIRmlViewClass::Show(void)
{
	if (Element == nullptr || Element->IsVisible()) {
		return;
	}

	Element->Show(IsModal ? Rml::ModalFlag::Modal : Rml::ModalFlag::None);

	if (IsModal) {
		Enter_Modal_Scope();
	}

	Mark_Overlay_Dirty();
}


void UIRmlViewClass::Close(void)
{
	if (_Context == nullptr || Element == nullptr) {
		return;
	}

	// The order docs/UI_DESIGN.md sets out: mark the screen closing and discard its
	// intents, then drop focus and capture, then release the document while the storage its
	// data model reads still lives, and only then clear the keyboard queue.
	Presenter.IsClosing = true;
	Presenter.Discard();

	Element->Close();
	Element = nullptr;

	_Context->RemoveDataModel(ModelName);
	Model = Rml::DataModelHandle();

	if (IsModal) {
		IsModal = false;
		Leave_Modal_Scope();
	}

	Mark_Overlay_Dirty();
}


bool UIRmlViewClass::Is_Visible(void) const
{
	return(Element != nullptr && Element->IsVisible());
}


/// <summary>
/// Runs a screen to a result, the way OwnerDraw::Dialog_Message_Handler runs a dialog.
/// The loop keeps the shape the legacy drivers have: it pumps messages, then steps the
/// game where a network session needs stepping and calls back where it does not, and only
/// then updates the toolkit and executes what the screen's events queued. An event handler
/// never acts directly, so a nested screen starts from the queue one level up.
/// </summary>
/// <param name="presenter">The screen to run. It carries the result it produces.</param>
/// <param name="view">The view bound to it, updated after each pass.</param>
/// <returns>The screen's result. GameEnded carries what the legacy driver returns when
/// the session ended underneath it.</returns>
UIResult UI_Run_Modal(UIPresenterClass & presenter, UIRmlViewClass & view)
{
	UIResult result;
	result.Outcome = UIResult::OUTCOME_CANCELLED;

	if (!_Initialized || _Context == nullptr) {
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	// Main_Loop can reach a screen of its own, and the guard is the one
	// OwnerDraw::Dialog_Message_Handler keeps for the same reason: the inner driver services
	// the game with a callback rather than stepping it twice.
	static bool inmainloop = false;

	_RunningModal++;

	while (!presenter.Result.has_value()) {
		Windows_Message_Handler();

		if (Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH && !Session.NetOpen && !Session.Suspended) {
			if (!inmainloop) {
				inmainloop = true;
				bool const ended = Main_Loop();
				inmainloop = false;

				if (ended) {
					_RunningModal--;
					result.Outcome = UIResult::OUTCOME_SESSION_ENDED;
					result.GameEnded = true;
					return(result);
				}
			}
		} else {
			Call_Back();
		}

		presenter.Service();

		_Context->Update();
		presenter.Drain();
		view.Sync();
		UI_Message_Box_Service();

		Mark_Overlay_Dirty();
		Video_Present_If_Dirty();
	}

	_RunningModal--;
	return(presenter.Result.value());
}
