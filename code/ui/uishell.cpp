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

#include "dbgprint.h"
#include "hostclock.h"
#include "conquer.h"
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

// The window holds the mouse capture while a gesture a toolkit consumed is in progress.
// The owner of a press owns its release, so a press that crossed into the game or out of
// it still completes where it started.
static int _CaptureButton = -1;

#ifndef NDEBUG
static Rml::ElementDocument * _TestDocument = nullptr;
#endif


/// <summary>
/// Turns a Win32 virtual key into the identifier RmlUi names it by.
/// Only the keys a document can act on are mapped; an unmapped key is left to the game.
/// </summary>
static Rml::Input::KeyIdentifier Key_Identifier(WPARAM key)
{
	using namespace Rml::Input;

	if (key >= 'A' && key <= 'Z') {
		return((KeyIdentifier)(KI_A + (int)(key - 'A')));
	}
	if (key >= '0' && key <= '9') {
		return((KeyIdentifier)(KI_0 + (int)(key - '0')));
	}
	if (key >= VK_F1 && key <= VK_F12) {
		return((KeyIdentifier)(KI_F1 + (int)(key - VK_F1)));
	}

	switch (key) {
		case VK_BACK: return(KI_BACK);
		case VK_TAB: return(KI_TAB);
		case VK_RETURN: return(KI_RETURN);
		case VK_ESCAPE: return(KI_ESCAPE);
		case VK_SPACE: return(KI_SPACE);
		case VK_PRIOR: return(KI_PRIOR);
		case VK_NEXT: return(KI_NEXT);
		case VK_END: return(KI_END);
		case VK_HOME: return(KI_HOME);
		case VK_LEFT: return(KI_LEFT);
		case VK_UP: return(KI_UP);
		case VK_RIGHT: return(KI_RIGHT);
		case VK_DOWN: return(KI_DOWN);
		case VK_INSERT: return(KI_INSERT);
		case VK_DELETE: return(KI_DELETE);
		case VK_SHIFT: return(KI_LSHIFT);
		case VK_CONTROL: return(KI_LCONTROL);
		case VK_MENU: return(KI_LMENU);
		default: return(KI_UNKNOWN);
	}
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


static void Mark_Overlay_Dirty(void)
{
	_OverlayIsDirty = true;
	Video_Mark_Dirty();
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

	if (!_Initialized || _Context == nullptr || _Changing || ticking) {
		return;
	}

	ticking = true;

	unsigned int const now = Host_Milliseconds();
	double const elapsed = (double)(now - _LastTickTime) / 1000.0;
	_LastTickTime = now;

	_Context->Update();

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


bool UI_Document_Is_Visible(void)
{
	return(_Initialized && _Context != nullptr && _Context->GetNumDocuments() > 0);
}


bool UI_Use_Rml(void)
{
	// No screen has migrated yet. The transitional key docs/UI_DESIGN.md describes arrives
	// with the first one, named by the change that introduces it.
	return(false);
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
/// Offers a window message to the toolkits before the game sees it.
/// The order follows docs/UI_DESIGN.md: ImGui's capture flags first, then a modal
/// document, then whatever an element under the cursor claims. A mouse move is always
/// delivered and never consumed, so the game keeps tracking the cursor underneath.
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
			return(false);
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
			bool const consumed = !_Context->ProcessMouseButtonDown(button, modifiers);
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

			return(UI_Dev_Wants_Mouse() || consumed);
		}

		case WM_MOUSEWHEEL: {
			POINT const point = Message_Point(message, lparam);
			float const notches = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;

			_Context->ProcessMouseMove((int)point.x, (int)point.y, modifiers);
			UI_Dev_Mouse_Wheel(notches);

			if (UI_Dev_Wants_Mouse()) {
				return(true);
			}

			return(!_Context->ProcessMouseWheel(-notches, modifiers));
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
				return(false);
			}

			return(!_Context->ProcessKeyDown(identifier, modifiers));
		}

		case WM_KEYUP:
		case WM_SYSKEYUP: {
			Rml::Input::KeyIdentifier const identifier = Key_Identifier(wparam);
			if (identifier == Rml::Input::KI_UNKNOWN) {
				return(false);
			}

			bool const consumed = !_Context->ProcessKeyUp(identifier, modifiers);
			return(UI_Dev_Wants_Keyboard() || consumed);
		}

		case WM_CHAR: {
			if (UI_Dev_Wants_Keyboard()) {
				return(true);
			}

			// Consuming the physical key never suppresses the text it generated, so this
			// is decided on its own.
			if (wparam < 32) {
				return(false);
			}

			return(!_Context->ProcessTextInput((Rml::Character)wparam));
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
	Document(document != nullptr ? document : "")
{
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

	Rml::DataModelConstructor constructor = _Context->CreateDataModel(Document);
	if (!constructor) {
		DebugString("[UI] The data model for %s could not be created.\n", Document.c_str());
		return(false);
	}

	Bind(constructor);
	Model = constructor.GetModelHandle();

	Element = _Context->LoadDocument(Document);
	if (Element == nullptr) {
		_Context->RemoveDataModel(Document);
		DebugString("[UI] The document %s could not be loaded.\n", Document.c_str());
		return(false);
	}

	Element->Show(modal ? Rml::ModalFlag::Modal : Rml::ModalFlag::None);
	Mark_Overlay_Dirty();
	return(true);
}


void UIRmlViewClass::Close(void)
{
	if (_Context == nullptr || Element == nullptr) {
		return;
	}

	Presenter.IsClosing = true;
	Presenter.Discard();

	Element->Close();
	Element = nullptr;

	_Context->RemoveDataModel(Document);
	Model = Rml::DataModelHandle();

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

	static bool inmainloop = false;

	while (!presenter.Result.has_value()) {
		Windows_Message_Handler();

		if (Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH && !Session.NetOpen && !Session.Suspended) {
			if (!inmainloop) {
				inmainloop = true;
				bool const ended = Main_Loop();
				inmainloop = false;

				if (ended) {
					result.Outcome = UIResult::OUTCOME_SESSION_ENDED;
					result.GameEnded = true;
					return(result);
				}
			}
		} else {
			Call_Back();
		}

		_Context->Update();
		presenter.Drain();
		view.Sync();

		Mark_Overlay_Dirty();
		Video_Present_If_Dirty();
	}

	return(presenter.Result.value());
}
