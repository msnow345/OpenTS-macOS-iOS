/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The abort and surrender screen's behavior, with no toolkit in it. One screen serves both:
// the middle choice is a restart in a solo mission and a surrender in a session, which is
// what IDD_MISSION_ABORT's own procedure decided at WM_INITDIALOG.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>


inline constexpr char const * UI_ABORT_QUIT = "quit";
inline constexpr char const * UI_ABORT_RESTART = "restart";
inline constexpr char const * UI_ABORT_CANCEL = "cancel";


class UIAbortPresenterClass : public UIPresenterClass
{
	public:
		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_QUIT,
			CHOICE_RESTART,
			CHOICE_CANCEL,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/

		// What the middle button should be relabelled to, or empty to leave the caption the
		// view already carries. Only the surrender overrides it; the restart is the caption
		// the template holds, and the template is the localized resource.
		std::string RestartCaption;

		// Is the middle choice offered at all? A player whose fate is already settled cannot
		// surrender, which is what the dialog disabled the button for.
		bool CanRestart = true;

		ChoiceType Choice = CHOICE_NONE;
};


// Shows the screen through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_Abort_Screen(UIAbortPresenterClass & presenter);
