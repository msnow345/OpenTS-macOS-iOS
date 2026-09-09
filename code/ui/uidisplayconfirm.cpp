/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The display mode confirmation. Behavior traced out of Test_Display_Mode_Dialog and
// Test_Display_Mode_Dialog_Proc in mainopt.cpp.
//
// The timeout was expressed there as a posted WM_COMMAND carrying WM_DESTROY, which is 2,
// which the procedure recorded because it accepted any identifier from one to IDCANCEL, and
// IDCANCEL is also 2. So the timeout was a cancel spelled awkwardly, and it is a cancel
// here. The driver re-armed its timer to five seconds after firing because the posted
// message took another pass to arrive; this produces the result directly, and re-arms for
// the same reason: a caller that keeps servicing must not be handed the answer twice.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uidisplayconfirm.h"

#include "uirmlview.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


void UIDisplayConfirmPresenterClass::Refresh(void)
{
	Choice = CHOICE_NONE;
	Timer = TIMEOUT_SECONDS * TIMER_SECOND;
}


int UIDisplayConfirmPresenterClass::Seconds_Remaining(void) const
{
	int const ticks = (int)Timer.Value();
	if (ticks <= 0) {
		return(0);
	}
	return((ticks + TIMER_SECOND - 1) / TIMER_SECOND);
}


/// <summary>
/// Takes the mode back when the player says nothing.
/// </summary>
void UIDisplayConfirmPresenterClass::Service(void)
{
	if (Result.has_value()) {
		return;
	}

	if (Timer <= 0) {
		Timer = REARM_SECONDS * TIMER_SECOND;

		Choice = CHOICE_CANCEL;

		UIResult result;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
		Result = result;
	}
}


void UIDisplayConfirmPresenterClass::Execute(UIIntent const & intent)
{
	UIResult result;

	if (intent.Action == UI_MODECONFIRM_ACCEPT) {
		Choice = CHOICE_ACCEPT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_MODECONFIRM_CANCEL) {
		Choice = CHOICE_CANCEL;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	} else {
		return;
	}

	Result = result;
}


//---------------------------------------------------------------------------------------
// The RmlUi view.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the display mode confirmation.
/// </summary>
class DisplayConfirmViewClass : public UIRmlViewClass
{
	public:
		DisplayConfirmViewClass(UIDisplayConfirmPresenterClass & presenter) :
			UIRmlViewClass(presenter, "modeconfirm.rml"),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		UIDisplayConfirmPresenterClass & Screen;
		int Remaining = 0;
};


void DisplayConfirmViewClass::Bind(Rml::DataModelConstructor & model)
{
	Remaining = Screen.Seconds_Remaining();
	model.Bind("seconds", &Remaining);

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// Escape refuses the mode, as does saying nothing. Enter keeps it, because the template
	// names no default push button and Windows then sent the dialog IDOK, which its
	// procedure recorded and its driver compared against IDOK.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Screen.Queue(UIIntent{UI_MODECONFIRM_CANCEL, "", 0});
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_MODECONFIRM_ACCEPT, "", 0});
			}
		});
}


void DisplayConfirmViewClass::Sync(void)
{
	if (!Model) return;

	int const remaining = Screen.Seconds_Remaining();
	if (remaining != Remaining) {
		Remaining = remaining;
		Model.DirtyVariable("seconds");
	}
}


/// <summary>
/// Shows the mode confirmation and waits for the player, or for the timeout.
/// </summary>
UIResult UI_Display_Confirm_Screen(UIDisplayConfirmPresenterClass & presenter)
{
	DisplayConfirmViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
