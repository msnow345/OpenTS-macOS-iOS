/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The in-game options screen. Behavior traced out of goptions.cpp and kept where it was,
// with the window handling left behind in the view.
//
// What the extraction fixes in place, each of which the dialog decided rather than the
// template: save and load mean different things in a solo game and in a session, and only
// the solo ones open a browser at all; delete never ends the screen, it just changes what
// is on disk and the screen is refreshed; a skirmish has no briefing to restate; resume is
// where the two sliders are applied, because dragging one only moved its label; abort
// asks for the surrender box rather than the abort box in a tournament session; and the
// screen answers with a choice rather than with a control identifier.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uigameoptions.h"

#include "data.h"
#include "dbgprint.h"
#include "event.h"
#include "gamedlg.h"
#include "globals.h"
#include "house.h"
#include "language/language.h"
#include "loaddlg.h"
#include "options.h"
#include "savemgr.h"
#include "scenario.h"
#include "session.h"
#include "stats.h"

#include "special.hh"

#include <cstring>


// The connection quality labels, best first, which is the order the slider counts in.
static int const _ConnectionNames[] = {
	TXT_WORST_CONNECTION,
	TXT_POOR_CONNECTION,
	TXT_GOOD_CONNECTION,
	TXT_BEST_CONNECTION
};

static int const CONNECTION_STEPS = 4;


static bool Is_Solo_Session(void)
{
	return(Session.Type == GAME_NORMAL || Session.Type == GAME_SKIRMISH);
}


/// <summary>
/// Copies the state the screen shows out of the game.
/// Called at open and again whenever a sub-screen has changed what is on disk, which is what
/// the dialog's second call to its own WM_INITDIALOG handler did.
/// </summary>
void UIGameOptionsPresenterClass::Refresh(void)
{
	IsMultiplayer = !Is_Solo_Session();
	HasSliders = (Session.Type == GAME_INTERNET);

	if (Is_Solo_Session()) {
		bool const present = LoadOptionsClass().Files_Present();
		CanSave = true;
		CanLoad = present;
		CanDelete = present;
	} else {
		CanSave = SaveManager.Is_Multiplayer_Saving_Allowed();
		CanLoad = SaveManager.Multiplayer_Load_Is_Allowed() && MultiplayerLoadOptionsClass().Files_Present();
		CanDelete = true;
	}

	CanBrief = (Session.Type != GAME_SKIRMISH);

	SpeedStep = (OptionsClass::MAX_SPEED_SETTING - 1) - Options.GameSpeed;
	ConnectionStep = (CONNECTION_STEPS - 1) - Session.LatencyFudge;

	SpeedLabels.clear();
	for (int step = 0; step < OptionsClass::MAX_SPEED_SETTING; step++) {
		SpeedLabels.push_back(Fetch_String(GameSpeedNames[step]));
	}

	ConnectionLabels.clear();
	for (int step = 0; step < CONNECTION_STEPS; step++) {
		ConnectionLabels.push_back(Fetch_String(_ConnectionNames[step]));
	}
}


void UIGameOptionsPresenterClass::Finish(ChoiceType choice, UIResult::OutcomeType outcome)
{
	Choice = choice;

	UIResult result;
	result.Outcome = outcome;
	Result = result;
}


void UIGameOptionsPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_GAMEOPT_SPEED) {
		// Dragging only moves the label. The setting is applied when the player resumes,
		// which is where the dialog read the slider back.
		SpeedStep = intent.Value;
		return;
	}

	if (intent.Action == UI_GAMEOPT_CONNECTION) {
		ConnectionStep = intent.Value;
		return;
	}

	// The two checks below are the dialog's own, made when the button was pressed rather
	// than when it was enabled, because a session can withdraw permission while the screen
	// is up. The view-model's CanSave and CanLoad say what to show, not what to allow.
	if (intent.Action == UI_GAMEOPT_SAVE) {
		if (Is_Solo_Session()) {
			Pending = SUB_SAVE;
		} else if (SaveManager.Is_Multiplayer_Saving_Allowed()) {
			OutList.push_back(EventClass(PlayerPtr->HeapID, EventClass::SAVEGAME));
			Finish(CHOICE_SAVE_REQUESTED, UIResult::OUTCOME_ACCEPTED);
		}
		return;
	}

	if (intent.Action == UI_GAMEOPT_LOAD) {
		if (Is_Solo_Session()) {
			Pending = SUB_LOAD;
		} else if (SaveManager.Multiplayer_Load_Is_Allowed()) {
			// A list opened from in here would sit inside the main loop and stall the match;
			// the menu loop opens it between frames instead.
			SpecialDialog = SDLG_LOAD;
			Finish(CHOICE_LOADED, UIResult::OUTCOME_ACCEPTED);
		}
		return;
	}

	if (intent.Action == UI_GAMEOPT_DELETE) {
		Pending = SUB_DELETE;
		return;
	}

	if (intent.Action == UI_GAMEOPT_BRIEFING) {
		Finish(CHOICE_BRIEFING, UIResult::OUTCOME_ACCEPTED);
		return;
	}

	if (intent.Action == UI_GAMEOPT_RESUME) {
		if (Session.Type == GAME_INTERNET) {
			int const fudge = (CONNECTION_STEPS - 1) - ConnectionStep;
			if (fudge != Session.LatencyFudge) {
				OutList.push_back(EventClass(PlayerPtr->HeapID, EventClass::LATENCYFUDGE, fudge));
				DebugString("LATENCYFUDGE event created - %d\n", fudge);
			}

			int const speed = (OptionsClass::MAX_SPEED_SETTING - 1) - SpeedStep;
			if (Options.GameSpeed != speed) {
				OutList.push_back(EventClass(PlayerPtr->HeapID, EventClass::GAMESPEED, speed));
			}
		}
		Finish(CHOICE_RESUME, UIResult::OUTCOME_ACCEPTED);
		return;
	}

	if (intent.Action == UI_GAMEOPT_ABORT) {
		if (Session.Type == GAME_INTERNET) {
			SpecialDialog = WestwoodOnline_Tournament ? SDLG_SURRENDER : SDLG_ABORT;
		} else {
			SpecialDialog = SDLG_ABORT;
		}
		Finish(CHOICE_ABORT, UIResult::OUTCOME_CANCELLED);
		return;
	}

	if (intent.Action == UI_GAMEOPT_SETTINGS) {
		SpecialDialog = SDLG_SETTINGS;
		Finish(CHOICE_SETTINGS, UIResult::OUTCOME_ACCEPTED);
		return;
	}
}


/// <summary>
/// Runs the browser an executed intent asked for.
/// The caller has already got its own presentation out of the way, which is all a view has
/// to do about a screen opening on top of this one.
/// </summary>
void UIGameOptionsPresenterClass::Run_Pending(void)
{
	SubScreenType const pending = Pending;
	Pending = SUB_NONE;

	switch (pending) {
		case SUB_SAVE: {
				char description[512];
				std::strcpy(description, Scen->Description);
				LoadOptionsClass().Save(description);
				Refresh();
			}
			break;

		case SUB_LOAD:
			if (LoadOptionsClass().Load()) {
				Finish(CHOICE_LOADED, UIResult::OUTCOME_ACCEPTED);
			}
			break;

		case SUB_DELETE:
			LoadOptionsClass().Delete();
			Refresh();
			break;

		default:
			break;
	}
}
