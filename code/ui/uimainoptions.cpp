/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The main options screen. Behavior traced out of Main_Options_Dialog and
// Main_Options_Dialog_Proc in mainopt.cpp.
//
// What the extraction fixes in place: the driver cleared GameActive around the whole family
// and put it back on the way out, so every screen the family opens sees a game that is not
// running whether or not one is, and both the sound and the game controls screens choose
// their layout from exactly that; and the settings are written once, as the player leaves
// the family, not as each sub-screen closes.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uimainoptions.h"

#include "audio/audioengine.h"
#include "gamedlg.h"
#include "globals.h"
#include "init.h"
#include "mainopt.h"
#include "goptions.h"
#include "options.h"
#include "sounddlg.h"


void UIMainOptionsPresenterClass::Refresh(void)
{
	SoundAvailable = AudioEngine.Is_Available();
}


void UIMainOptionsPresenterClass::Begin(void)
{
	WasGameActive = GameActive;
	GameActive = false;
}


void UIMainOptionsPresenterClass::End(void)
{
	Options.Save_Settings();
	GameActive = WasGameActive;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIMainOptionsPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


void UIMainOptionsPresenterClass::Execute(UIIntent const & intent)
{
	UIResult result;
	result.Outcome = UIResult::OUTCOME_ACCEPTED;

	if (intent.Action == UI_MAINOPT_SOUND) {
		if (!SoundAvailable) {
			return;
		}
		Choice = CHOICE_SOUND;
	} else if (intent.Action == UI_MAINOPT_DISPLAY) {
		Choice = CHOICE_DISPLAY;
	} else if (intent.Action == UI_MAINOPT_KEYBOARD) {
		Choice = CHOICE_KEYBOARD;
	} else if (intent.Action == UI_MAINOPT_SETTINGS) {
		Choice = CHOICE_SETTINGS;
	} else {
		// Anything else leaves, which is the dialog's own default arm: it took whatever
		// identifier arrived, and every identifier that was not a sub-screen ended the
		// family.
		Choice = CHOICE_EXIT;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	}

	Result = result;
}


/// <summary>
/// Runs the screen the player asked for.
/// </summary>
void UIMainOptionsPresenterClass::Run_Pending(void)
{
	ChoiceType const pending = Choice;
	Choice = CHOICE_NONE;

	switch (pending) {
		case CHOICE_SOUND:
			SoundControlsClass().Dialog();
			break;

		case CHOICE_DISPLAY:
			Display_Options_Dialog();
			break;

		case CHOICE_KEYBOARD:
			Options.Hotkey_Dialog();
			break;

		case CHOICE_SETTINGS:
			GameControlsClass().Dialog();
			break;

		default:
			break;
	}
}
