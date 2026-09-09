/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The campaign choice screen. Behavior traced out of Choose_Campaign and
// Campaign_Choice_Dialog_Proc in init.cpp, including the availability test that decided
// which campaigns were listed at all.
//
// What the extraction fixes in place: a row carries the campaign it stands for rather than
// its position, because the list skips a campaign the player cannot reach; the difficulty
// is written to the settings only on accept, which is where the dialog read the slider
// back; and the difficulty caption starts as the template's own, because the dialog set it
// only when the slider moved.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uicampaign.h"

#include "uirmlview.h"

#include "addon.h"
#include "campaign.h"
#include "data.h"
#include "dbgprint.h"
#include "campaign.hh"
#include "gamedlg.h"
#include "globals.h"
#include "goptions.h"
#include "init.h"
#include "language/language.h"
#include "vector.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


// A base game campaign is offered only when no addon is running, and an addon's own
// campaign only when that particular addon is running. Moved here from init.cpp, where it
// existed for this screen alone.
static bool Campaign_Available(CampaignClass * campaign)
{
	if (Addon_Enabled(ADDON_ANY) == true) {
		if (campaign->RequiredAddon == ADDON_BASE_GAME) {
			return(false);
		}
		if (Addon_Enabled((AddonType)campaign->RequiredAddon)) {
			return(true);
		}
		return(false);
	}

	if (campaign->RequiredAddon == ADDON_BASE_GAME) {
		return(true);
	}

	return(false);
}


void UICampaignPresenterClass::Refresh(void)
{
	Campaigns.clear();
	Selected = -1;

	for (int index = 0; index < ::Campaigns.Count(); index++) {
		CampaignClass * const campaign = ::Campaigns[index];

		if (!Campaign_Available(campaign)) {
			DebugString("\tSkipping Campaign [%d] - %s\n", index, campaign->Description);
			continue;
		}

		DebugString("\tAdding Campaign [%d] - %s\n", index, campaign->Description);

		EntryType entry;
		entry.Label = campaign->Description;
		entry.Campaign = index;
		Campaigns.push_back(entry);
	}

	// The dialog selected the first row it had listed.
	if (!Campaigns.empty()) {
		Selected = 0;
	}

	Difficulty = Options.Difficulty;
	if (Difficulty < 0) Difficulty = 0;
	if (Difficulty >= DIFFICULTY_STEPS) Difficulty = DIFFICULTY_STEPS - 1;

	// The template's own caption, which is what the dialog left showing until the slider
	// was moved.
	DifficultyLabel = "Harder";
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UICampaignPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


int UICampaignPresenterClass::Chosen(void) const
{
	if (Choice != CHOICE_ACCEPT) {
		return(CAMPAIGN_NONE);
	}
	if (Selected < 0 || Selected >= (int)Campaigns.size()) {
		return(CAMPAIGN_NONE);
	}
	return(Campaigns[Selected].Campaign);
}


void UICampaignPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_CAMPAIGN_SELECT) {
		if (intent.Value >= 0 && intent.Value < (int)Campaigns.size()) {
			Selected = intent.Value;
		}
		return;
	}

	if (intent.Action == UI_CAMPAIGN_DIFFICULTY) {
		if (intent.Value >= 0 && intent.Value < DIFFICULTY_STEPS) {
			Difficulty = intent.Value;
			DifficultyLabel = Fetch_String(GameDifficultyNames[Difficulty]);
		}
		return;
	}

	UIResult result;

	if (intent.Action == UI_CAMPAIGN_ACCEPT) {
		Options.Difficulty = Difficulty;
		Choice = CHOICE_ACCEPT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;

	} else if (intent.Action == UI_CAMPAIGN_CANCEL) {
		Choice = CHOICE_CANCEL;
		result.Outcome = UIResult::OUTCOME_CANCELLED;

	} else {
		return;
	}

	result.Value = Chosen();
	Result = result;
}
