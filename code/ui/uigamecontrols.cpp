/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The game controls screen. Behavior traced out of gamedlg.cpp.
//
// What the extraction fixes in place, none of it obvious from the templates: leaving
// through the sound or the keyboard button applies and saves the settings, because both
// wrote the same IDOK the accept button did; the difficulty is applied only with no game
// running, so the slider the in-game template also carries is never read; a game speed
// change during a network session is issued as an event instead of being written, so every
// player stays in step; and the internet variant carries no game speed slider at all.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uigamecontrols.h"

#include "_map.h"
#include "audio/audioengine.h"
#include "_tooltip.h"
#include "cctooltip.h"
#include "data.h"
#include "event.h"
#include "gamedlg.h"
#include "globals.h"
#include "house.h"
#include "init.h"
#include "language/language.h"
#include "options.h"
#include "session.h"
#include "techno.h"

#include "special.hh"


static void Fill_Labels(std::vector<std::string> & labels, int const * names, int count)
{
	labels.clear();
	for (int index = 0; index < count; index++) {
		labels.push_back(Fetch_String(names[index]));
	}
}


void UIGameControlsPresenterClass::Refresh(void)
{
	if (GameActive) {
		Variant = (Session.Type == GAME_INTERNET) ? VARIANT_INTERNET : VARIANT_SESSION;
	} else {
		Variant = VARIANT_FRONTEND;
	}

	SpeedStep = (OptionsClass::MAX_SPEED_SETTING - 1) - Options.GameSpeed;
	ScrollStep = (OptionsClass::MAX_SCROLL_SETTING - 1) - Options.ScrollRate;
	DetailStep = Options.DetailLevel;
	DifficultyStep = Options.Difficulty;

	CameoText = Options.SidebarCameoText;
	ActionLines = Options.ActionLines;
	ShowToolTips = Options.ToolTips;
	Coasting = (Options.ScrollMethod == 0);
	EdgeScroll = Options.AutoScroll;

	SoundAvailable = AudioEngine.Is_Available();

	Fill_Labels(SpeedLabels, GameSpeedNames, OptionsClass::MAX_SPEED_SETTING);
	Fill_Labels(ScrollLabels, GameScrollSpeedNames, OptionsClass::MAX_SCROLL_SETTING);
	Fill_Labels(DetailLabels, GameDetailLevelNames, OptionsClass::MAX_DETAIL_SETTING);
	Fill_Labels(DifficultyLabels, GameDifficultyNames, OptionsClass::MAX_DIFFICULTY_SETTING);
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIGameControlsPresenterClass::Service(void)
{
	if (!GameActive) {
		Title_Screen_Restore();
	}
}


bool UIGameControlsPresenterClass::Commits(void) const
{
	return(Choice == CHOICE_ACCEPT || Choice == CHOICE_SOUND || Choice == CHOICE_KEYBOARD);
}


void UIGameControlsPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_GAMECTRL_SPEED) {
		SpeedStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_SCROLL) {
		ScrollStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_DETAIL) {
		DetailStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_DIFFICULTY) {
		DifficultyStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_CAMEO_TEXT) {
		CameoText = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_ACTION_LINES) {
		ActionLines = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_TOOLTIPS) {
		ShowToolTips = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_COASTING) {
		Coasting = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_EDGE_SCROLL) {
		EdgeScroll = (intent.Value != 0);
		return;
	}

	UIResult result;

	if (intent.Action == UI_GAMECTRL_ACCEPT) {
		Choice = CHOICE_ACCEPT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_GAMECTRL_CANCEL) {
		Choice = CHOICE_CANCEL;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	} else if (intent.Action == UI_GAMECTRL_SOUND) {
		if (!Has_Sub_Screens()) {
			return;
		}
		SpecialDialog = SDLG_SOUND;
		Choice = CHOICE_SOUND;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_GAMECTRL_KEYBOARD) {
		if (!Has_Sub_Screens()) {
			return;
		}
		SpecialDialog = SDLG_KEYBOARD;
		Choice = CHOICE_KEYBOARD;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else {
		return;
	}

	Result = result;
}


void UIGameControlsPresenterClass::Apply(void)
{
	if (Has_Speed()) {
		int const gamespeed = (OptionsClass::MAX_SPEED_SETTING - 1) - SpeedStep;
		if (Options.GameSpeed != gamespeed) {
			if (GameActive && Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH) {
				OutList.push_back(EventClass(PlayerPtr->HeapID, EventClass::GAMESPEED, gamespeed));
			} else {
				Options.GameSpeed = gamespeed;
			}
		}
	}

	Options.ScrollRate = (OptionsClass::MAX_SCROLL_SETTING - 1) - ScrollStep;

	if (Options.DetailLevel != DetailStep) {
		Options.DetailLevel = DetailStep;
		Map.Reinit_Cell_Drawers();
	}

	if (Options.SidebarCameoText != CameoText) {
		Options.SidebarCameoText = CameoText;
		Map.Toggle_Cameo_Text(CameoText);
	}

	Options.ActionLines = ActionLines;
	TechnoClass::Set_Action_Lines(Options.ActionLines);

	Options.ToolTips = ShowToolTips;
	if (ToolTips != NULL && GameActive) {
		ToolTips->Activate(Options.ToolTips);
	}

	Options.ScrollMethod = Coasting ? 0 : 1;
	Options.AutoScroll = EdgeScroll;

	if (Has_Difficulty()) {
		Options.Difficulty = DifficultyStep;
	}
}
