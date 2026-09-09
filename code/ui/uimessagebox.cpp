/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The message box and the wait box. The message box is the first screen that runs the game
// underneath itself: in a network session the modal runner keeps stepping Main_Loop while
// the box owns the input, which is what WWMessageBox::Process has always done through
// OwnerDraw::Dialog_Message_Handler.
//
// What is preserved from the dialog, and why each of these is here rather than obvious:
// the buttons are laid out first, third, second across the box, which is the order the
// IDD_MSGBOX_3 template places them in; a lone button moves to the middle slot; Enter
// answers with the caller's default response rather than with a button, because the
// template names no default push button and Windows then sends IDOK; Escape answers with
// button two, because Windows sends IDCANCEL whether or not that button exists; and a box
// with no button at all answers with zero without waiting.
//
// docs/UI_DESIGN.md, "Screens" and "Scheduling", own the contracts this keeps to.

#include "always.h"

#include "uimessagebox.h"

#include "uiinternal.h"
#include "uirmlview.h"

#include "_keyboar.h"
#include "globals.h"
#include "init.h"
#include "keyboard.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <memory>
#include <string>


// The vocabulary a document has. A press names the button it came from; the other two are
// the keyboard's answers, which are not buttons and do not carry an index of their own.
static char const * const ACTION_PRESS = "press";
static char const * const ACTION_DEFAULT = "default";
static char const * const ACTION_CANCEL = "cancel";

// The button index Windows produces for the Escape key, which reaches the dialog as
// IDCANCEL and so answers with the second button whether or not one is shown.
static int const ESCAPE_RESPONSE = 1;


/// <summary>
/// The toolkit-free half of the message box.
/// </summary>
class MessageBoxPresenterClass : public UIPresenterClass
{
	public:
		// The view-model. Plain values, and the only thing a document reads.
		std::string Message;
		std::string Buttons[3];
		bool Shown[3] = { false, false, false };

		// Does the first button sit in the middle slot? It does when it is the only one, as
		// the dialog moved it there.
		bool FirstIsCentred = false;

		int DefaultResponse = 0;

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override {}
		virtual void Service(void) override;
};


/// <summary>
/// Answers an intent the view raised. Every one of them ends the screen, so the mapping
/// from what the player did to what the caller is told is the whole of the behavior.
/// </summary>
void MessageBoxPresenterClass::Execute(UIIntent const & intent)
{
	UIResult result;

	if (intent.Action == ACTION_PRESS) {
		if (intent.Value < 0 || intent.Value > 2 || !Shown[intent.Value]) {
			return;
		}
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
		result.Value = intent.Value;
	} else if (intent.Action == ACTION_DEFAULT) {
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
		result.Value = DefaultResponse;
	} else if (intent.Action == ACTION_CANCEL) {
		result.Outcome = UIResult::OUTCOME_CANCELLED;
		result.Value = ESCAPE_RESPONSE;
	} else {
		return;
	}

	Result = result;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void MessageBoxPresenterClass::Service(void)
{
	if (!GameActive) {
		Title_Screen_Restore();
	}
}


/// <summary>
/// The RmlUi half of the message box.
/// </summary>
class MessageBoxViewClass : public UIRmlViewClass
{
	public:
		MessageBoxViewClass(MessageBoxPresenterClass & presenter);

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		void Answer(char const * action, int value);

		MessageBoxPresenterClass & Screen;
};


MessageBoxViewClass::MessageBoxViewClass(MessageBoxPresenterClass & presenter) :
	UIRmlViewClass(presenter, "messagebox.rml"),
	Screen(presenter)
{
}


void MessageBoxViewClass::Answer(char const * action, int value)
{
	UIIntent intent;
	intent.Action = action;
	intent.Value = value;
	Screen.Queue(intent);
}


void MessageBoxViewClass::Bind(Rml::DataModelConstructor & model)
{
	model.Bind("message", &Screen.Message);
	model.Bind("button1", &Screen.Buttons[0]);
	model.Bind("button2", &Screen.Buttons[1]);
	model.Bind("button3", &Screen.Buttons[2]);
	model.Bind("shown1", &Screen.Shown[0]);
	model.Bind("shown2", &Screen.Shown[1]);
	model.Bind("shown3", &Screen.Shown[2]);
	model.Bind("centred", &Screen.FirstIsCentred);

	// An event handler never acts: it queues, and the runner executes the queue after
	// Context::Update has returned.
	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			Answer(ACTION_PRESS, arguments.empty() ? 0 : (int)arguments[0].Get<float>());
		});

	// Enter and Escape are the box's own hotkeys, and neither answers with a button: Enter
	// yields the caller's default response and Escape the second button's index.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);

			if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Answer(ACTION_DEFAULT, 0);
			} else if (key == Rml::Input::KI_ESCAPE) {
				Answer(ACTION_CANCEL, 0);
			}
		});
}


void MessageBoxViewClass::Sync(void)
{
	// Nothing an intent can execute changes the view-model. Every one of them ends the
	// screen instead.
}


/// <summary>
/// Shows a message and waits for the player to answer it.
/// </summary>
UIResult UI_Message_Box_Screen(char const * message, int defresponse,
	char const * b1txt, char const * b2txt, char const * b3txt)
{
	// The presenter is declared first so that it is destroyed last: the data model the view
	// binds reads the presenter's view-model, and must not outlive it.
	MessageBoxPresenterClass presenter;
	MessageBoxViewClass view(presenter);

	char const * const captions[3] = { b1txt, b2txt, b3txt };

	// The dialog counted its buttons this way: each caption that is present raises the count
	// to its own slot, so a box that skips a slot still counts by the highest one filled.
	int count = 0;
	for (int index = 0; index < 3; index++) {
		if (captions[index] != nullptr && captions[index][0] != '\0') {
			presenter.Buttons[index] = captions[index];
			presenter.Shown[index] = true;
			count = index + 1;
		}
	}

	// A box with nothing to press is answered for the player, without a pass of the loop and
	// so without a frame in which it could be seen.
	if (count == 0) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
		result.Value = 0;
		return(result);
	}

	presenter.FirstIsCentred = (count == 1);
	presenter.DefaultResponse = defresponse;

	if (message != nullptr) {
		presenter.Message = message;
	}

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}


//---------------------------------------------------------------------------------------
// The wait box. It stands over a long operation rather than waiting for an answer, so it
// is not modal and the caller's own loop keeps running underneath it.
//---------------------------------------------------------------------------------------

class WaitBoxPresenterClass : public UIPresenterClass
{
	public:
		std::string Message;
		std::string CancelCaption;
		bool CanCancel = false;

		// The caller's flag, raised when the player cancels. It is the caller's storage, the
		// way the dialog kept it in DWLP_USER, and it outlives the box.
		bool * Cancelled = nullptr;

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override {}
};


void WaitBoxPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action != ACTION_CANCEL || !CanCancel) {
		return;
	}

	// The escape key is what the operation underneath is watching for; the flag only tells
	// the caller which of the two ways out was taken.
	if (Keyboard != nullptr) {
		Keyboard->Put(KN_ESC);
	}

	if (Cancelled != nullptr) {
		*Cancelled = true;
	}
}


class WaitBoxViewClass : public UIRmlViewClass
{
	public:
		WaitBoxViewClass(WaitBoxPresenterClass & presenter);

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		WaitBoxPresenterClass & Screen;
};


WaitBoxViewClass::WaitBoxViewClass(WaitBoxPresenterClass & presenter) :
	UIRmlViewClass(presenter, "waitbox.rml"),
	Screen(presenter)
{
}


void WaitBoxViewClass::Bind(Rml::DataModelConstructor & model)
{
	model.Bind("message", &Screen.Message);
	model.Bind("cancelcaption", &Screen.CancelCaption);
	model.Bind("cancancel", &Screen.CanCancel);

	model.BindEventCallback("cancel",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const &) {
			UIIntent intent;
			intent.Action = ACTION_CANCEL;
			Screen.Queue(intent);
		});
}


void WaitBoxViewClass::Sync(void)
{
	if (Model) {
		Model.DirtyVariable("message");
	}
}


// The one wait box. Every caller opens one, holds it for the length of an operation and
// closes it, and no caller opens a second while one is up.
static std::unique_ptr<WaitBoxPresenterClass> _WaitPresenter;
static std::unique_ptr<WaitBoxViewClass> _WaitView;


bool UI_Wait_Box_Open(char const * message, char const * cancelcaption, bool * cancelled)
{
	if (_WaitView != nullptr) {
		return(false);
	}

	// The presenter is created first so that it is destroyed last, for the same reason the
	// modal screens declare it first.
	_WaitPresenter = std::make_unique<WaitBoxPresenterClass>();
	_WaitView = std::make_unique<WaitBoxViewClass>(*_WaitPresenter);

	if (message != nullptr) {
		_WaitPresenter->Message = message;
	}

	if (cancelcaption != nullptr && cancelcaption[0] != '\0') {
		_WaitPresenter->CancelCaption = cancelcaption;
		_WaitPresenter->CanCancel = true;
	}

	_WaitPresenter->Cancelled = cancelled;

	if (!_WaitView->Prepare(false)) {
		_WaitView.reset();
		_WaitPresenter.reset();
		return(false);
	}

	// The box must be on screen before the operation underneath begins. A caller that saves
	// a game and never pumps again would otherwise show nothing at all.
	UI_Paint_Now(true);
	return(true);
}


void UI_Wait_Box_Set_Text(char const * message)
{
	if (_WaitView == nullptr) {
		return;
	}

	_WaitPresenter->Message = (message != nullptr) ? message : "";
	_WaitView->Sync();

	// Set_Custom_Message_Box_Text repainted the box before it returned, through UpdateWindow.
	UI_Paint_Now(true);
}


void UI_Wait_Box_Close(void)
{
	if (_WaitView == nullptr) {
		return;
	}

	_WaitView->Close();
	_WaitView.reset();
	_WaitPresenter.reset();
}


bool UI_Wait_Box_Is_Open(void)
{
	return(_WaitView != nullptr);
}


/// <summary>
/// Executes what the wait box's own events queued.
/// The box has no loop of its own, so the shell's tick is its safe point: the queue is
/// drained after Context::Update has returned and never from inside an event handler.
/// </summary>
void UI_Message_Box_Service(void)
{
	if (_WaitPresenter != nullptr) {
		_WaitPresenter->Drain();
	}
}
