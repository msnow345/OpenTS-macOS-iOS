/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The main options screen's behavior, with no toolkit in it. The screen is five buttons and
// almost no state of its own, but it owns the family: it suspends the game while any
// options screen is up and writes the settings out when the player leaves, and the sound
// and game controls screens each pick their own layout from the suspension it holds.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"


inline constexpr char const * UI_MAINOPT_SOUND = "sound";
inline constexpr char const * UI_MAINOPT_DISPLAY = "display";
inline constexpr char const * UI_MAINOPT_KEYBOARD = "keyboard";
inline constexpr char const * UI_MAINOPT_SETTINGS = "settings";
inline constexpr char const * UI_MAINOPT_EXIT = "exit";


class UIMainOptionsPresenterClass : public UIPresenterClass
{
	public:
		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_SOUND,
			CHOICE_DISPLAY,
			CHOICE_KEYBOARD,
			CHOICE_SETTINGS,
			CHOICE_EXIT,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// Suspends the game for as long as the options family is up. What this holds is
		// read by the screens the family opens, so it is behavior rather than bookkeeping:
		// the sound and game controls screens each choose their layout from it.
		void Begin(void);

		// Writes the settings out and puts the game back the way Begin found it.
		void End(void);

		// Runs the screen the last choice asked for, then clears the request. Safe to call
		// with nothing pending.
		void Run_Pending(void);

		// Does the last choice leave the options family? Anything the screen does not
		// recognize leaves it, which is what the dialog's default arm did.
		bool Exits(void) const { return(Choice == CHOICE_EXIT); }

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/

		// Is there an audio device to talk to? With none the sound button is disabled, as
		// the dialog disabled it.
		bool SoundAvailable = false;

		ChoiceType Choice = CHOICE_NONE;

	private:
		bool WasGameActive = false;
};
