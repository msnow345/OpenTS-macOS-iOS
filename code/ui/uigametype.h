/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The game type screen's behavior, with no toolkit in it. Two buttons and a way back, shown
// only when an expansion is installed and there is a choice to make.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"


inline constexpr char const * UI_GAMETYPE_ORIGINAL = "original";
inline constexpr char const * UI_GAMETYPE_FIRESTORM = "firestorm";
inline constexpr char const * UI_GAMETYPE_BACK = "back";


class UIGameTypePresenterClass : public UIPresenterClass
{
	public:
		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_ORIGINAL,
			CHOICE_FIRESTORM,
			CHOICE_BACK,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// Puts the addon system into the state the choice asks for, and says whether the
		// game carries on. Anything but backing out carries on, which is the dialog's own
		// default arm.
		bool Apply(int & addon);

		ChoiceType Choice = CHOICE_NONE;
};


// Shows the screen through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_Game_Type_Screen(UIGameTypePresenterClass & presenter);
