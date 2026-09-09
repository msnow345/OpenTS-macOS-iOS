/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The display mode confirmation's behavior, with no toolkit in it. The screen exists to
// take back a resolution the player cannot see, so its timeout is the whole point of it and
// belongs here rather than in a view: a mode that leaves the screen unreadable is answered
// by saying nothing, and saying nothing has to mean no.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include "stimer.h"
#include "timer.h"


inline constexpr char const * UI_MODECONFIRM_ACCEPT = "accept";
inline constexpr char const * UI_MODECONFIRM_CANCEL = "cancel";


class UIDisplayConfirmPresenterClass : public UIPresenterClass
{
	public:
		enum {
			// What the dialog driver's own CDTimerClass was set to, and what it re-armed to
			// after firing.
			TIMEOUT_SECONDS = 10,
			REARM_SECONDS = 5,
		};

		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_ACCEPT,
			CHOICE_CANCEL,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// Whole seconds left before the mode is taken back, counted down for a view that
		// wants to show them. Nothing depends on the number; the rollback is driven by the
		// timer itself.
		int Seconds_Remaining(void) const;

		ChoiceType Choice = CHOICE_NONE;

	private:
		CDTimerClass<SystemTimerClass> Timer;
};
