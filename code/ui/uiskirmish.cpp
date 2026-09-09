/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The skirmish setup screen. Behavior traced out of Skirmish_On_WM_INITDIALOG,
// Skirmish_On_WM_COMMAND and Skirmish_Mode_Dialog in skirmish.cpp.
//
// What the extraction fixes in place: a side row carries the country it stands for rather
// than its position, because the list holds only the countries that may be played; the map
// the player asked for is checked for enough start positions when the accept button is
// pressed, and a map with too few leaves the screen standing; Short Game turns Bases on and
// turning Bases off turns Short Game off; and backing out still records the name, side and
// color, which is what the cancel arm read before it answered.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uiskirmish.h"

#include "uiscenariopick.h"

#include "_rules.h"
#include "data.h"
#include "houstype.h"
#include "init.h"
#include "language/language.h"
#include "msgbox.h"
#include "netdlg2.h"
#include "goptions.h"
#include "mapgen.h"
#include "mplayer.h"
#include "netshare.h"
#include "preview.h"
#include "rules.h"
#include "session.h"

#include <cstdio>
#include <cstring>


// The least money a skirmish may be started with, which is where the credits track bar
// begins.
enum { MP_MIN_MONEY = 2500 };


UISkirmishPresenterClass::UISkirmishPresenterClass(void) :
	Preview(UI_MAP_PREVIEW_SURFACE)
{
}


void UISkirmishPresenterClass::Refresh(void)
{
	Handle = Session.Handle;

	Sides.clear();
	SelectedSide = 0;
	for (int index = 0; index < HouseTypes.Count(); index++) {
		HouseTypeClass const * const house = HouseTypes[index];
		if (!house->IsMultiplay) continue;

		if (index == Session.House) {
			SelectedSide = (int)Sides.size();
		}
		Sides.push_back(SideType{(char const *)house->GivenName, index});
	}

	Colors.clear();
	Colors.push_back(Fetch_String(TXT_GOLD));
	Colors.push_back(Fetch_String(TXT_RED));
	Colors.push_back(Fetch_String(TXT_BLUE));
	Colors.push_back(Fetch_String(TXT_GREEN));
	Colors.push_back(Fetch_String(TXT_ORANGE));
	Colors.push_back(Fetch_String(TXT_SKY_BLUE));
	Colors.push_back(Fetch_String(TXT_PURPLE));
	Colors.push_back(Fetch_String(TXT_PINK));
	SelectedColor = Session.PrefColor;

	UnitCount = SliderType{Session.Options.UnitCount, SessionClass::CountMin[1], SessionClass::CountMax[1], 1};
	Credits = SliderType{Session.Options.Credits, MP_MIN_MONEY, Rule->MPMaxMoney, 250};
	TechLevel = SliderType{BuildLevel, 1, MPLAYER_BUILD_LEVEL_MAX, 1};
	AILevel = SliderType{(int)Session.Options.AIDifficulty, 0, 2, 1};
	AIPlayers = SliderType{Session.Options.AIPlayers > 1 ? Session.Options.AIPlayers : 1, 1, 7, 1};

	// The track bar runs the other way round from the setting: its left end is the slowest
	// game, and the dialog turned one into the other at both ends.
	GameSpeed = SliderType{6 - Session.Options.GameSpeed, 0, 6, 1};

	Bases = Session.Options.Bases;
	Crates = Session.Options.Goodies;
	FogOfWar = Session.Options.FogOfWar;
	Bridges = Session.Options.BridgeDestruction;
	MCVRedeploy = Session.Options.MCVRedeploy;
	ShortGame = Session.Options.ShortGame;
	MultiEngineer = Session.Options.CrapEngineers;

	// The screen opens on the first scenario whatever the session was carrying.
	Set_Scenario_Info_From_Index(0);
	Session.Options.ScenarioIndex = 0;
	ScenarioName = Session.Options.ScenarioDescription;

	Clear_Vector(&Session.Players);
	Clear_Vector(&Session.Computers);

	Rebuild_Network_Map_Preview();
	PreviewGeneration++;

	CanAccept = true;
	ListChanged = true;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UISkirmishPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


/// <summary>
/// Records the name, side and color the player is showing.
/// Both leaving and backing out read these, because they are the player's own preferences
/// rather than the game's settings.
/// </summary>
void UISkirmishPresenterClass::Read_Identity(void)
{
	std::snprintf(Session.Handle, sizeof(Session.Handle), "%s", Handle.c_str());

	if (SelectedSide >= 0 && SelectedSide < (int)Sides.size()) {
		Session.House = (HousesType)Sides[SelectedSide].Country;
	} else {
		Session.House = (HousesType)HOUSE_FIRST;
	}

	Session.ColorIdx = SelectedColor;
	Session.PrefColor = SelectedColor;
}


/// <summary>
/// Writes the player's multiplayer preferences, and drops the preview.
/// </summary>
void UISkirmishPresenterClass::End(void)
{
	if (MultiplayerMapPreview != NULL) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = NULL;
	}

	Session.Write_MultiPlayer_Settings();
}


/// <summary>
/// Runs the map selection screen with this one already out of the way.
/// Backing out of it leaves the scenario this screen was showing, which is what putting the
/// old index back did.
/// </summary>
void UISkirmishPresenterClass::Run_Pending(void)
{
	if (Pending != SUB_PICK_MAP) {
		return;
	}

	Pending = SUB_NONE;

	int const previous = Session.Options.ScenarioIndex;

	bool const picked = Pick_Scenario_Screen();

	if (picked && Set_Scenario_Info_From_Index(Session.Options.ScenarioIndex) == true) {
		ScenarioName = Session.Options.ScenarioDescription;
	} else {
		Session.Options.ScenarioIndex = previous;
		Set_Scenario_Info_From_Index(previous);
	}

	// A generated map has a picture of its own beside it rather than one read out of the map.
	int const index = Session.Options.ScenarioIndex;
	if (index >= 0 && index < Session.Scenarios.Count()
		&& stricmp(Session.Scenarios[index]->Get_Filename(), RANDOM_MAP_FILE_NAME) == 0) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = new MapPreviewClass;
		MultiplayerMapPreview->Read_PCX_Preview("RandMap.img");
		if (MultiplayerMapPreview->Get_Preview_Surface() == NULL) {
			Rebuild_Network_Map_Preview();
		}
	} else {
		Rebuild_Network_Map_Preview();
	}

	PreviewGeneration++;
}


/// <summary>
/// Starts the game the player set up, if the map has room for it.
/// The start position count is checked here rather than when the slider moved, because the
/// map can change after it did.
/// </summary>
void UISkirmishPresenterClass::Accept(void)
{
	CanAccept = false;

	int const waypoints = RandomMapWaypointCount(Session.Options.ScenarioIndex);
	if (waypoints < AIPlayers.Value + 1) {
		char buffer[256];
		std::snprintf(buffer, sizeof(buffer), Fetch_String(TXT_SCENARIO_TOO_SMALL), waypoints);
		WWMessageBox().Process(buffer, TXT_OK);
		CanAccept = true;
		return;
	}

	Read_Identity();

	Session.Options.UnitCount = UnitCount.Value;
	BuildLevel = TechLevel.Value;
	Session.Options.Credits = Credits.Value;
	Session.Options.AIDifficulty = (DiffType)AILevel.Value;
	Session.Options.AIPlayers = AIPlayers.Value;
	Session.Options.GameSpeed = 6 - GameSpeed.Value;
	Options.GameSpeed = Session.Options.GameSpeed;

	NodeNameType * const who = new NodeNameType;
	if (who != NULL) {
		strcpy(who->Name, Session.Handle);
		who->Player.House = Session.House;
		who->Player.Color = Session.ColorIdx;
		who->Player.ProcessTime = -1;
		Session.Players.Add(who);
	}

	Session.Options.Bases = Bases;
	Session.Options.Goodies = Crates;
	Session.Options.FogOfWar = FogOfWar;
	Session.Options.BridgeDestruction = Bridges;
	Session.Options.MCVRedeploy = MCVRedeploy;
	Session.Options.ShortGame = ShortGame;
	Session.Options.HarvTruce = false;
	Session.Options.CrapEngineers = MultiEngineer;

	if (MultiplayerMapPreview != NULL) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = NULL;
	}

	Outcome = true;

	UIResult result;
	result.Outcome = UIResult::OUTCOME_ACCEPTED;
	Result = result;
}


void UISkirmishPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_SKIRMISH_HANDLE) {
		Handle = intent.Identity;
		if (Handle.size() > HANDLE_LIMIT) {
			Handle.resize(HANDLE_LIMIT);
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_SIDE) {
		if (intent.Value >= 0 && intent.Value < (int)Sides.size()) {
			SelectedSide = intent.Value;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_COLOR) {
		if (intent.Value >= 0 && intent.Value < (int)Colors.size()) {
			SelectedColor = intent.Value;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_SLIDER) {
		SliderType * slider = NULL;
		if (intent.Identity == UI_SKIRMISH_UNITCOUNT) slider = &UnitCount;
		else if (intent.Identity == UI_SKIRMISH_CREDITS) slider = &Credits;
		else if (intent.Identity == UI_SKIRMISH_TECHLEVEL) slider = &TechLevel;
		else if (intent.Identity == UI_SKIRMISH_AILEVEL) slider = &AILevel;
		else if (intent.Identity == UI_SKIRMISH_AIPLAYERS) slider = &AIPlayers;
		else if (intent.Identity == UI_SKIRMISH_GAMESPEED) slider = &GameSpeed;

		if (slider != NULL) {
			int value = intent.Value;
			if (value < slider->Minimum) value = slider->Minimum;
			if (value > slider->Maximum) value = slider->Maximum;
			slider->Value = value;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_TOGGLE) {
		if (intent.Identity == UI_SKIRMISH_BASES) {
			Bases = !Bases;
			// A short game is decided by what a player still holds, so it needs bases.
			if (!Bases) ShortGame = false;
		} else if (intent.Identity == UI_SKIRMISH_SHORTGAME) {
			ShortGame = !ShortGame;
			if (ShortGame) Bases = true;
		} else if (intent.Identity == UI_SKIRMISH_CRATES) {
			Crates = !Crates;
		} else if (intent.Identity == UI_SKIRMISH_FOG) {
			FogOfWar = !FogOfWar;
		} else if (intent.Identity == UI_SKIRMISH_BRIDGES) {
			Bridges = !Bridges;
		} else if (intent.Identity == UI_SKIRMISH_MCV) {
			MCVRedeploy = !MCVRedeploy;
		} else if (intent.Identity == UI_SKIRMISH_ENGINEER) {
			MultiEngineer = !MultiEngineer;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_PICK_MAP) {
		Pending = SUB_PICK_MAP;
		return;
	}

	if (intent.Action == UI_SKIRMISH_ACCEPT) {
		Accept();
		return;
	}

	if (intent.Action == UI_SKIRMISH_CANCEL) {
		Read_Identity();
		Outcome = false;

		UIResult result;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
		Result = result;
		return;
	}
}
