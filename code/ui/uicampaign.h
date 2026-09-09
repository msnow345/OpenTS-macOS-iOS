/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The campaign choice screen's behavior, with no toolkit in it. A row carries the campaign
// it stands for rather than its position, because the list skips a campaign the player
// cannot reach and a row number then means nothing on its own.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_CAMPAIGN_SELECT = "select";        // Value: row
inline constexpr char const * UI_CAMPAIGN_DIFFICULTY = "difficulty";  // Value: slider step
inline constexpr char const * UI_CAMPAIGN_ACCEPT = "accept";
inline constexpr char const * UI_CAMPAIGN_CANCEL = "cancel";


class UICampaignPresenterClass : public UIPresenterClass
{
	public:
		// The three positions the difficulty track bar was given.
		enum { DIFFICULTY_STEPS = 3 };

		struct EntryType
		{
			std::string Label;
			int Campaign = 0;
		};

		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_ACCEPT,
			CHOICE_CANCEL,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// The campaign the player settled on, or the none campaign when they backed out.
		int Chosen(void) const;

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		std::vector<EntryType> Campaigns;
		int Selected = -1;

		int Difficulty = 0;

		// What the difficulty caption reads. The dialog left the template's own caption
		// showing until the slider was moved, so that caption is where this starts.
		std::string DifficultyLabel;

		ChoiceType Choice = CHOICE_NONE;
};


// Shows the screen through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_Campaign_Screen(UICampaignPresenterClass & presenter);
