/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The multiplayer map selection screen. Behavior traced out of Scenario_DlgProc and
// Scenario_Select_Callback in netshare.cpp.
//
// What the extraction fixes in place: the preview follows the highlighted row rather than
// the session's own choice, and the session's choice is put back after every look, so
// browsing the list changes nothing until the player accepts; the random map generator draws
// where this screen is, so the screen steps aside for it; and backing out leaves the
// scenario the screen opened on.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uiscenariopick.h"

#include "data.h"
#include "mapgen.h"
#include "netdlg2.h"
#include "netshare.h"
#include "preview.h"
#include "session.h"

#include <cstring>


UIScenarioPickPresenterClass::UIScenarioPickPresenterClass(void)
{
}


void UIScenarioPickPresenterClass::Build_List(void)
{
	Scenarios.clear();
	for (int index = 0; index < Session.Scenarios.Count(); index++) {
		Scenarios.push_back(Session.Scenarios[index]->Description());
	}
	ListChanged = true;
}


void UIScenarioPickPresenterClass::Refresh(void)
{
	Build_List();

	Selected = Session.Options.ScenarioIndex;
	Original = Selected;
	LastPreviewed = -1;
}


/// <summary>
/// Builds the preview for the highlighted row and puts the session's own choice back.
/// The list can be walked without committing to anything, so the scenario information the
/// preview needs is loaded, used, and then replaced by the one the screen opened on.
/// </summary>
void UIScenarioPickPresenterClass::Preview_Selection(void)
{
	if (Selected == LastPreviewed || Selected < 0 || Selected >= Session.Scenarios.Count()) {
		return;
	}

	Set_Scenario_Info_From_Index(Selected);

	// A generated map has a picture of its own beside it rather than one read out of the map.
	if (stricmp(Session.Scenarios[Selected]->Get_Filename(), RANDOM_MAP_FILE_NAME) == 0) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = new MapPreviewClass;
		MultiplayerMapPreview->Read_PCX_Preview("RandMap.img");
		if (MultiplayerMapPreview->Get_Preview_Surface() == NULL) {
			Rebuild_Network_Map_Preview();
		}
	} else {
		Rebuild_Network_Map_Preview();
	}

	LastPreviewed = Selected;
	PreviewGeneration++;

	Session.Options.ScenarioIndex = Original;
	Set_Scenario_Info_From_Index(Original);
}


/// <summary>
/// The maintenance the dialog's wait callback ran on every pass of its own loop.
/// </summary>
void UIScenarioPickPresenterClass::Service(void)
{
	Preview_Selection();

	// A game already in the lobby keeps talking while the host browses. The runner services
	// a session of its own, so only the network pump the callback added belongs here.
	if (Session.Type == GAME_IPX || Session.Type == GAME_INTERNET) {
		Net2Callback();
	}
}


/// <summary>
/// Runs the map generator with the screen already out of the way.
/// </summary>
void UIScenarioPickPresenterClass::Run_Pending(void)
{
	if (Pending != SUB_RANDOM_MAP) {
		return;
	}

	Pending = SUB_NONE;

	int const scenario = CreateRandomMap();
	if (scenario == -1) {
		return;
	}

	// A generated map joins the list and is highlighted, but the session keeps the scenario
	// the screen opened on until the player accepts.
	Build_List();
	Selected = scenario;
	LastPreviewed = -1;

	Set_Scenario_Info_From_Index(scenario);
	if (MultiplayerMapPreview == NULL || MultiplayerMapPreview->Get_Preview_Surface() == NULL) {
		Rebuild_Network_Map_Preview();
	}
	PreviewGeneration++;

	Session.Options.ScenarioIndex = Original;
	Set_Scenario_Info_From_Index(Original);
}


void UIScenarioPickPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_SCENARIOPICK_SELECT) {
		if (intent.Value < 0 || intent.Value >= (int)Scenarios.size()) {
			return;
		}
		Selected = intent.Value;
		return;
	}

	if (intent.Action == UI_SCENARIOPICK_RANDOM) {
		Pending = SUB_RANDOM_MAP;
		return;
	}

	if (intent.Action == UI_SCENARIOPICK_ACCEPT) {
		// The dialog clamped a list with nothing selected to the first entry.
		Selected = Selected > 0 ? Selected : 0;
		Session.Options.ScenarioIndex = Selected;

		UIResult result;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
		result.Value = Selected;
		Result = result;
		return;
	}

	if (intent.Action == UI_SCENARIOPICK_CANCEL) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
		Result = result;
		return;
	}
}
