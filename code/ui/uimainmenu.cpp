/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The main menu. Behavior traced out of Main_Menu and Main_Menu_Dialog_Proc in init.cpp.
//
// What the extraction fixes in place: the load button is disabled when there is no saved
// game to offer, and the test is made as the screen opens rather than once at startup; the
// keys the driver watched for beside the buttons belong to the screen, not to the window it
// was drawn in, so a typed character reaches Cheat_Key_Process through an intent and the
// version screen and the credits are choices like any other; and the version screen is a
// screen of a different kind, so it nests and the view steps aside for it, which is what
// the dialog's own ShowWindow did.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uimainmenu.h"

#include "uirmlview.h"

#include "voc.h"
#include "globals.h"
#include "init.h"
#include "loaddlg.h"
#include "_rules.h"
#include "rules.h"
#include "scrnsel.hh"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

void Version_Dialog(void);


void UIMainMenuPresenterClass::Refresh(void)
{
	CanLoad = LoadOptionsClass().Files_Present();
}


/// <summary>
/// The maintenance the menu driver ran on every pass of its own loop.
/// </summary>
void UIMainMenuPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


/// <summary>
/// The selection the caller's own switch is written against.
/// </summary>
int UIMainMenuPresenterClass::Selection(void) const
{
	switch (Choice) {
		case CHOICE_CAMPAIGN: return(SEL_CAMPAIGN_GAME);
		case CHOICE_LOAD: return(SEL_LOAD_GAME);
		case CHOICE_MULTIPLAYER: return(SEL_MULTIPLAYER_GAME);
		case CHOICE_INTRO: return(SEL_INTRO);
		case CHOICE_OPTIONS: return(SEL_OPTIONS);
		case CHOICE_EXIT: return(SEL_EXIT);
		case CHOICE_CREDITS: return(SEL_VIEW_CREDITS);
		default: return(SEL_NONE);
	}
}


/// <summary>
/// Runs the screen the last choice asked for, then clears the request.
/// </summary>
void UIMainMenuPresenterClass::Run_Pending(void)
{
	if (!VersionPending) {
		return;
	}

	VersionPending = false;
	Version_Dialog();
}


void UIMainMenuPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_MAINMENU_VERSION) {
		VersionPending = true;
		return;
	}

	if (intent.Action == UI_MAINMENU_TYPED) {
		// A cheat word is spelled out one character at a time and only the word completing
		// it answers, which is why nothing here is a choice.
		if (Cheat_Key_Process((char)intent.Value)) {
			Sound_Effect(Rule->OptionsChanged);
			Title_Screen_Restore(true);
		}
		return;
	}

	UIResult result;
	result.Outcome = UIResult::OUTCOME_ACCEPTED;

	if (intent.Action == UI_MAINMENU_CAMPAIGN) {
		Choice = CHOICE_CAMPAIGN;
	} else if (intent.Action == UI_MAINMENU_LOAD) {
		// The permission is checked when the button is pressed rather than when it was
		// enabled, because a saved game can arrive or go while the screen is up.
		if (!LoadOptionsClass().Files_Present()) {
			return;
		}
		Choice = CHOICE_LOAD;
	} else if (intent.Action == UI_MAINMENU_MULTIPLAYER) {
		Choice = CHOICE_MULTIPLAYER;
	} else if (intent.Action == UI_MAINMENU_INTRO) {
		Choice = CHOICE_INTRO;
	} else if (intent.Action == UI_MAINMENU_OPTIONS) {
		Choice = CHOICE_OPTIONS;
	} else if (intent.Action == UI_MAINMENU_CREDITS) {
		Choice = CHOICE_CREDITS;
	} else if (intent.Action == UI_MAINMENU_EXIT) {
		Choice = CHOICE_EXIT;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	} else {
		return;
	}

	result.Value = Selection();
	Result = result;
}
