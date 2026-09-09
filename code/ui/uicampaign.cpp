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


//---------------------------------------------------------------------------------------
// The RmlUi view.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the campaign choice screen.
/// </summary>
class CampaignViewClass : public UIRmlViewClass
{
	public:
		CampaignViewClass(UICampaignPresenterClass & presenter) :
			UIRmlViewClass(presenter, "campaign.rml"),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

		// The track bar takes its position from the model as the document loads, and that
		// raises a change event of its own. Nothing is queued until this is set.
		void Settle(void) { Settled = true; }

	private:
		UICampaignPresenterClass & Screen;
		bool Settled = false;
};


void CampaignViewClass::Bind(Rml::DataModelConstructor & model)
{
	if (auto entry = model.RegisterStruct<UICampaignPresenterClass::EntryType>()) {
		entry.RegisterMember("label", &UICampaignPresenterClass::EntryType::Label);
	}
	model.RegisterArray<std::vector<UICampaignPresenterClass::EntryType>>();

	model.Bind("campaigns", &Screen.Campaigns);
	model.Bind("selected", &Screen.Selected);
	model.Bind("difficulty", &Screen.Difficulty);
	model.Bind("difficultyname", &Screen.DifficultyLabel);

	model.BindEventCallback("pick",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_CAMPAIGN_SELECT, "", (int)arguments[0].Get<float>()});
		});

	// The track bar is bound one way and a position the screen already holds raises no
	// intent, so setting it from the model cannot move a difficulty the player did not.
	// The dialog read its slider back at accept because a keyboard or page move raised no
	// thumb notification; RmlUi raises a change for every move, so there is nothing left to
	// read back.
	model.BindEventCallback("slide",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			if (!Settled) return;

			int const step = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);
			if (step == Screen.Difficulty) return;

			Screen.Queue(UIIntent{UI_CAMPAIGN_DIFFICULTY, "", step});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// Escape cancels and Enter accepts, which is what IsDialogMessage delivered to a dialog
	// whose template names no default push button.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Screen.Queue(UIIntent{UI_CAMPAIGN_CANCEL, "", 0});
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_CAMPAIGN_ACCEPT, "", 0});
			}
		});
}


void CampaignViewClass::Sync(void)
{
	if (!Model) return;

	Model.DirtyVariable("campaigns");
	Model.DirtyVariable("selected");
	Model.DirtyVariable("difficultyname");
}


/// <summary>
/// Shows the campaign list and waits for the player to choose.
/// </summary>
UIResult UI_Campaign_Screen(UICampaignPresenterClass & presenter)
{
	CampaignViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	view.Settle();

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
