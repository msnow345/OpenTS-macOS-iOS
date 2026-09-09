/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The abort and surrender screen. Behavior traced out of goptions.cpp.
//
// Two things the dialog decided rather than the template: the middle button is relabelled
// a surrender for anything but a solo mission, and it is disabled for a player whose fate
// is already settled, so a defeated player is offered no surrender. Only the surrender
// caption comes from the string table; the restart caption stays the template's, which is
// where its translation lives, so the view-model asks for an override rather than naming
// both.

#include "always.h"

#include "uiabort.h"

#include "data.h"
#include "house.h"
#include "language/language.h"
#include "session.h"


void UIAbortPresenterClass::Refresh(void)
{
	if (Session.Type == GAME_NORMAL) {
		RestartCaption.clear();
		CanRestart = true;
	} else {
		RestartCaption = Fetch_String(TXT_SURRENDER);
		CanRestart = !(PlayerPtr->IsDefeated || PlayerPtr->IsToWin || PlayerPtr->IsToLose || PlayerPtr->IsToDie);
	}
}


void UIAbortPresenterClass::Execute(UIIntent const & intent)
{
	UIResult result;

	if (intent.Action == UI_ABORT_QUIT) {
		Choice = CHOICE_QUIT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_ABORT_RESTART) {
		Choice = CHOICE_RESTART;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_ABORT_CANCEL) {
		Choice = CHOICE_CANCEL;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	} else {
		return;
	}

	Result = result;
}
