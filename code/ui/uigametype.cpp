/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The game type screen. Behavior traced out of Select_Game_Type_Dialog and its procedure in
// addon.cpp.
//
// What the extraction fixes in place: the addon state is cleared before the choice is
// applied and the required addon is set afterwards whichever way the player went, and only
// backing out stops the game carrying on, which is the dialog's own default arm reading any
// identifier that was not Firestorm as the base game.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uigametype.h"

#include "uirmlview.h"

#include "addon.h"
#include "init.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


void UIGameTypePresenterClass::Refresh(void)
{
	Choice = CHOICE_NONE;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIGameTypePresenterClass::Service(void)
{
	Title_Screen_Restore(false);
}


bool UIGameTypePresenterClass::Apply(int & addon)
{
	// Every addon off before the choice is applied, which is what the dialog did by
	// assigning the active set directly.
	Disable_Addon(ADDON_ANY);

	if (Choice == CHOICE_BACK) {
		return(false);
	}

	if (Choice == CHOICE_FIRESTORM) {
		Enable_Addon(ADDON_FIRESTORM);
		addon = ADDON_FIRESTORM;
	} else {
		addon = ADDON_BASE_GAME;
	}

	Set_Required_Addon((AddonType)addon);
	return(true);
}


void UIGameTypePresenterClass::Execute(UIIntent const & intent)
{
	UIResult result;
	result.Outcome = UIResult::OUTCOME_ACCEPTED;

	if (intent.Action == UI_GAMETYPE_FIRESTORM) {
		Choice = CHOICE_FIRESTORM;
	} else if (intent.Action == UI_GAMETYPE_ORIGINAL) {
		Choice = CHOICE_ORIGINAL;
	} else if (intent.Action == UI_GAMETYPE_BACK) {
		Choice = CHOICE_BACK;
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
/// The RmlUi half of the game type screen.
/// </summary>
class GameTypeViewClass : public UIRmlViewClass
{
	public:
		GameTypeViewClass(UIGameTypePresenterClass & presenter) :
			UIRmlViewClass(presenter, "gametype.rml"),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override {}

	private:
		UIGameTypePresenterClass & Screen;
};


void GameTypeViewClass::Bind(Rml::DataModelConstructor & model)
{
	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// The Main Menu button is the template's IDCANCEL, so Escape is what it is, and Enter
	// reaches the dialog's default arm, which is the base game.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Screen.Queue(UIIntent{UI_GAMETYPE_BACK, "", 0});
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_GAMETYPE_ORIGINAL, "", 0});
			}
		});
}


/// <summary>
/// Shows the game type choice and waits for the player to make it.
/// </summary>
UIResult UI_Game_Type_Screen(UIGameTypePresenterClass & presenter)
{
	GameTypeViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
