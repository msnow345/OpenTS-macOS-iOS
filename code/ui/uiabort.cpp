/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The abort and surrender screen. Behavior traced out of goptions.cpp.
//
// Two things the dialog decided rather than the template: the middle button is relabelled
// a surrender for anything but a solo mission, and it is disabled for a player whose fate
// is already settled, so a defeated player is offered no surrender. Only the surrender
// caption comes from the string table; the restart caption stays the template's, which is
// where its translation lives, so the view-model asks for an override rather than naming
// both.

#include "always.h"

#include "uiabort.h"

#include "uirmlview.h"

#include "data.h"
#include "house.h"
#include "language/language.h"
#include "session.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


void UIAbortPresenterClass::Refresh(void)
{
	if (Session.Type == GAME_NORMAL) {
		RestartCaption.clear();
		CanRestart = true;
	} else {
		RestartCaption = Fetch_String(TXT_SURRENDER);
		CanRestart = !(PlayerPtr->IsDefeated || PlayerPtr->IsToWin || PlayerPtr->IsToLose || PlayerPtr->IsToDie);
	}
}


void UIAbortPresenterClass::Execute(UIIntent const & intent)
{
	UIResult result;

	if (intent.Action == UI_ABORT_QUIT) {
		Choice = CHOICE_QUIT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_ABORT_RESTART) {
		Choice = CHOICE_RESTART;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_ABORT_CANCEL) {
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
/// The RmlUi half of the abort and surrender screen.
/// </summary>
class AbortViewClass : public UIRmlViewClass
{
	public:
		AbortViewClass(UIAbortPresenterClass & presenter) :
			UIRmlViewClass(presenter, "abort.rml"),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override {}

	private:
		UIAbortPresenterClass & Screen;

		// The middle button's caption. The document carries the restart wording the
		// template carried, and this replaces it only where the screen asks.
		Rml::String RestartCaption;
};


void AbortViewClass::Bind(Rml::DataModelConstructor & model)
{
	RestartCaption = Screen.RestartCaption.empty() ? "Restart" : Rml::String(Screen.RestartCaption);

	model.Bind("restartcaption", &RestartCaption);
	model.Bind("canrestart", &Screen.CanRestart);

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// Escape carries on playing, which is the IDCANCEL the dialog answered with its cancel
	// arm. Enter does the same: the template names no default push button, so Windows sent
	// IDOK, and the dialog had no arm for it.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Screen.Queue(UIIntent{UI_ABORT_CANCEL, "", 0});
			}
		});
}


/// <summary>
/// Shows the abort box and waits for the player to choose.
/// </summary>
UIResult UI_Abort_Screen(UIAbortPresenterClass & presenter)
{
	AbortViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
