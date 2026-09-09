/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The main menu's behavior, with no toolkit in it. The screen is six buttons and the keys
// the driver watched for beside them, which are part of the screen rather than of the
// window it was drawn in: the version screen, the credits, and the cheat words.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"


inline constexpr char const * UI_MAINMENU_CAMPAIGN = "campaign";
inline constexpr char const * UI_MAINMENU_LOAD = "load";
inline constexpr char const * UI_MAINMENU_MULTIPLAYER = "multiplayer";
inline constexpr char const * UI_MAINMENU_INTRO = "intro";
inline constexpr char const * UI_MAINMENU_OPTIONS = "options";
inline constexpr char const * UI_MAINMENU_EXIT = "exit";
inline constexpr char const * UI_MAINMENU_VERSION = "version";
inline constexpr char const * UI_MAINMENU_CREDITS = "credits";
inline constexpr char const * UI_MAINMENU_TYPED = "typed";   // Value: the character typed


class UIMainMenuPresenterClass : public UIPresenterClass
{
	public:
		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_CAMPAIGN,
			CHOICE_LOAD,
			CHOICE_MULTIPLAYER,
			CHOICE_INTRO,
			CHOICE_OPTIONS,
			CHOICE_EXIT,
			CHOICE_CREDITS,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// The selection the caller's own switch is written against.
		int Selection(void) const;

		// Is the version screen waiting to be run? It is a screen of a different kind, so
		// it nests, and the view gets out of its way the way the dialog's ShowWindow did.
		bool VersionPending = false;
		void Run_Pending(void);

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/

		// Is there a saved game to offer? With none the load button is disabled, as the
		// dialog disabled it.
		bool CanLoad = false;

		ChoiceType Choice = CHOICE_NONE;
};


// Shows the screen through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_Main_Menu_Screen(UIMainMenuPresenterClass & presenter);
