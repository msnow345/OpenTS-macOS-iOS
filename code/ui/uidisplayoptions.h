/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The display options screen's behavior, with no toolkit in it. The resolution it settles
// on is staged rather than applied, because a mode is tried and confirmed before the
// settings remember it; the file-scope TempOptions copy the dialog used for that staging
// lives here now.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_DISPLAY_SELECT = "select";    // Value: row
inline constexpr char const * UI_DISPLAY_STRETCH = "stretch";  // Value: check state
inline constexpr char const * UI_DISPLAY_ACCEPT = "accept";
inline constexpr char const * UI_DISPLAY_CANCEL = "cancel";


class UIDisplayOptionsPresenterClass : public UIPresenterClass
{
	public:
		// The bounds the dialog asked the display for.
		enum {
			MIN_WIDTH = 640,
			MIN_HEIGHT = 400,
			MAX_WIDTH = 4096,
			MAX_HEIGHT = 4096,
		};

		struct ModeType
		{
			std::string Label;
			int Width = 0;
			int Height = 0;
		};

		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_ACCEPT,
			CHOICE_CANCEL,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// Is a resolution staged that the game is not already running at? Only one that is
		// gets tried, and only a tried one is ever written to the settings.
		bool Wants_Mode_Change(void) const;

		// Writes the staged resolution into the settings, once its trial was accepted.
		void Commit(void);

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		std::vector<ModeType> Modes;
		int Selected = -1;
		bool StretchMovies = false;

		// The resolution the screen is staging. It starts at the one in force and moves
		// only when the player accepts a row other than the one the screen opened on, which
		// is what the dialog's own previous-against-current comparison amounted to.
		int StagedWidth = 0;
		int StagedHeight = 0;

		ChoiceType Choice = CHOICE_NONE;

	private:
		int Opened = -1;
};
