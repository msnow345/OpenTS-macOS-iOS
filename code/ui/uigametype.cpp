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
