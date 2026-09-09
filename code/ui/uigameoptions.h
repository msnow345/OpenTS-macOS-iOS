/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The in-game options screen's behavior, with no toolkit in it. It is the door to save,
// load, the mission briefing, the game settings and abort, so what it decides is worth
// having in one toolkit-free place before either view is written.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


// What a view raises.
inline constexpr char const * UI_GAMEOPT_SAVE = "save";
inline constexpr char const * UI_GAMEOPT_LOAD = "load";
inline constexpr char const * UI_GAMEOPT_DELETE = "delete";
inline constexpr char const * UI_GAMEOPT_BRIEFING = "briefing";
inline constexpr char const * UI_GAMEOPT_RESUME = "resume";
inline constexpr char const * UI_GAMEOPT_ABORT = "abort";
inline constexpr char const * UI_GAMEOPT_SETTINGS = "settings";
inline constexpr char const * UI_GAMEOPT_SPEED = "speed";            // Value: slider step
inline constexpr char const * UI_GAMEOPT_CONNECTION = "connection";  // Value: slider step


class UIGameOptionsPresenterClass : public UIPresenterClass
{
	public:
		// What the player settled on. The driver maps this onto the value its own caller
		// expects; no control identifier reaches this class.
		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_RESUME,
			CHOICE_BRIEFING,
			CHOICE_ABORT,
			CHOICE_SETTINGS,
			CHOICE_SAVE_REQUESTED,
			CHOICE_LOADED,
		};

		// A screen this one opens on top of itself. The view hides whatever it has to hide,
		// then asks for the pending one to run; only the view knows how to get out of the
		// way, and only this class knows what running it means.
		enum SubScreenType {
			SUB_NONE,
			SUB_SAVE,
			SUB_LOAD,
			SUB_DELETE,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;

		// Runs the sub-screen an executed intent asked for and clears the request. Safe to
		// call with nothing pending.
		void Run_Pending(void);

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		bool CanSave = false;
		bool CanLoad = false;
		bool CanDelete = false;
		bool CanBrief = false;

		// Does the screen carry the game speed and connection quality sliders? Only the
		// template the game shows for an internet session does.
		bool HasSliders = false;

		// Is this a session the player saves and loads through the multiplayer path? The two
		// paths differ in what the buttons mean, not only in whether they are enabled.
		bool IsMultiplayer = false;

		// Slider steps, counted the way the templates count them: the fastest game speed and
		// the best connection sit at step zero, so a step is the setting counted backward. A
		// view shows steps; only this class knows what they mean.
		int SpeedStep = 0;
		int ConnectionStep = 0;

		// The label beside each slider, indexed by step.
		std::vector<std::string> SpeedLabels;
		std::vector<std::string> ConnectionLabels;

		ChoiceType Choice = CHOICE_NONE;
		SubScreenType Pending = SUB_NONE;

	private:
		void Finish(ChoiceType choice, UIResult::OutcomeType outcome);
};


// Shows the screen through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_Game_Options_Screen(UIGameOptionsPresenterClass & presenter);
