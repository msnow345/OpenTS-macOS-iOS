/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The toolkit-free half of a screen. A presenter holds the view-model, answers queries and
// executes intents; it never sees a window handle, a surface, an RmlUi element or an ImGui
// call, so the same presenter serves an RmlUi view, a legacy dialog and a test.
//
// docs/UI_DESIGN.md, "Screens", owns this contract.

#pragma once

#include <optional>
#include <string>
#include <vector>


// What a view asks the presenter to do. An intent carries identities and copied data only:
// never a document node, a borrowed buffer, a window handle or an engine pointer, because
// it is executed at the owner's next safe point rather than where it was raised.
struct UIIntent
{
	std::string Action;
	std::string Identity;
	int Value = 0;
};


// What a screen answers with. The outcome maps onto the return values the dialog drivers
// already use, and GameEnded carries what OwnerDraw::Dialog_Message_Handler returns.
struct UIResult
{
	enum OutcomeType {
		OUTCOME_ACCEPTED,
		OUTCOME_CANCELLED,
		OUTCOME_SESSION_ENDED,
		OUTCOME_FAILED_TO_OPEN,
	};

	OutcomeType Outcome = OUTCOME_CANCELLED;
	int Value = 0;
	bool GameEnded = false;
};


class UIPresenterClass
{
	public:
		virtual ~UIPresenterClass(void) {}

		// Records an intent for the owner to execute. Safe to call from a toolkit event
		// handler, which must never act directly.
		void Queue(UIIntent const & intent) { Intents.push_back(intent); }

		// Executes every queued intent in order at the owner's safe point. Intents raised
		// while a screen is closing are discarded rather than replayed.
		void Drain(void)
		{
			std::vector<UIIntent> pending;
			pending.swap(Intents);

			for (UIIntent const & intent : pending) {
				if (IsClosing) break;
				Execute(intent);
			}
		}

		void Discard(void) { Intents.clear(); }

		virtual void Execute(UIIntent const & intent) = 0;
		virtual void Refresh(void) = 0;

		std::optional<UIResult> Result;
		bool IsClosing = false;

	protected:
		std::vector<UIIntent> Intents;
};
