/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The display options screen. Behavior traced out of Display_Options_Dialog_Body in
// mainopt.cpp.
//
// What the extraction fixes in place: the resolution is staged and only a trial the player
// confirms writes it to the settings, while the movie stretching preference is written
// straight to the settings at accept and left alone at cancel; and the staged resolution
// moves only when the player leaves the screen on a row other than the one it opened on, so
// re-picking the row already in force stages nothing and skips the trial.
//
// EnumDisplayModes reports nothing on a platform without host mode enumeration, and this
// screen then offers an empty list, which is what the dialog did with the same answer.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uidisplayoptions.h"

#include "globals.h"
#include "init.h"
#include "goptions.h"
#include "options.h"
#include "video.h"

#include <cstdio>


void UIDisplayOptionsPresenterClass::Refresh(void)
{
	Modes.clear();
	Selected = -1;

	StagedWidth = Options.ScreenWidth;
	StagedHeight = Options.ScreenHeight;
	StretchMovies = Options.StretchMovies;

	int * const modes = EnumDisplayModes(MIN_WIDTH, MIN_HEIGHT, MAX_WIDTH, MAX_HEIGHT);
	if (modes != NULL) {
		for (int * mode = modes; *mode != 0; mode += 2) {
			ModeType entry;
			entry.Width = mode[0];
			entry.Height = mode[1];

			char buffer[64];
			std::snprintf(buffer, sizeof(buffer), "%d x %d", entry.Width, entry.Height);
			entry.Label = buffer;

			if (entry.Width == StagedWidth && entry.Height == StagedHeight) {
				Selected = (int)Modes.size();
			}

			Modes.push_back(entry);
		}
		delete [] modes;
	}

	Opened = Selected;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIDisplayOptionsPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


bool UIDisplayOptionsPresenterClass::Wants_Mode_Change(void) const
{
	return(StagedWidth != Options.ScreenWidth || StagedHeight != Options.ScreenHeight);
}


void UIDisplayOptionsPresenterClass::Commit(void)
{
	Options.ScreenWidth = StagedWidth;
	Options.ScreenHeight = StagedHeight;
}


void UIDisplayOptionsPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_DISPLAY_SELECT) {
		if (intent.Value >= 0 && intent.Value < (int)Modes.size()) {
			Selected = intent.Value;
		}
		return;
	}

	if (intent.Action == UI_DISPLAY_STRETCH) {
		StretchMovies = (intent.Value != 0);
		return;
	}

	UIResult result;

	if (intent.Action == UI_DISPLAY_ACCEPT) {
		if (Selected != Opened && Selected >= 0 && Selected < (int)Modes.size()) {
			StagedWidth = Modes[Selected].Width;
			StagedHeight = Modes[Selected].Height;
		}

		// The stretching preference is not staged. The dialog wrote it at IDOK and left it
		// alone at IDCANCEL, so it survives a resolution the player then refuses.
		Options.StretchMovies = StretchMovies;

		Choice = CHOICE_ACCEPT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;

	} else if (intent.Action == UI_DISPLAY_CANCEL) {
		Choice = CHOICE_CANCEL;
		result.Outcome = UIResult::OUTCOME_CANCELLED;

	} else {
		return;
	}

	Result = result;
}
