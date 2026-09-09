/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The network lobby. Behavior traced out of MPlayer_Game_List_Dialog_Proc,
// MPlayer_Host_Dialog_Proc, MPlayer_Guest_Dialog_Proc, Net2DisplayGameList,
// _Net2DisplayUsers and Net2Remote_Connect in netdlg2.cpp.
//
// What the extraction fixes in place: the rosters are read into the model where the
// session changes rather than where a list is drawn, so a presentation that draws a
// different number of times cannot lose a change or repeat one; the host's accepted
// status is a fact about the player, so it is recorded with the roster rather than while
// painting a row; a game row carries whether the game is open rather than the bracketed
// caption; and the selection is clamped against a list a host can shorten at any moment,
// which is what the display function did before it drew.
//
// What the host's half fixes in place: a chat line is bounded into the packet field it is
// copied to rather than into a buffer the sender chose; the option track bars carry the
// ranges DisplayGameopts sets rather than a control's default; and the players the host has
// picked out to kick are resolved to names when the kick is executed rather than read back
// off a list box that the roster may have moved underneath.
//
// Packets are untouched. Nothing here changes what goes on the wire, only who owns the
// state the screens show.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uilobby.h"

#include "uimappreview.h"
#include "uirmlview.h"

#include "_rand.h"
#include "_rules.h"
#include "_timer.h"
#include "conquer.h"
#include "data.h"
#include "houstype.h"
#include "ipxmgr.h"
#include "language/language.h"
#include "mapgen.h"
#include "mplayer.h"
#include "netdlg.h"
#include "netdlg2.h"
#include "netdlg.h"
#include "netshare.h"
#include "preview.h"
#include "rules.h"
#include "dbgprint.h"
#include "session.h"
#include "utf8.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstdio>
#include <cstring>


// The least money a network game may be started with, which is where the credits track bar
// begins. DisplayGameopts names the same figure.
enum { MP_MIN_MONEY = 2500 };


// The lobby the driver is running. A message produced away from a screen reaches the model
// through this, the way PMessagePrintf found the topmost dialog with somewhere to show one.
static UILobbyPresenterClass * _LobbyScreen = NULL;


UILobbyPresenterClass * UI_Lobby_Screen(void)
{
	return(_LobbyScreen);
}


void UI_Set_Lobby_Screen(UILobbyPresenterClass * screen)
{
	_LobbyScreen = screen;
}


void UILobbyPresenterClass::Refresh(void)
{
	Handle = Session.Handle;
	Color = Session.ColorIdx;

	Build_Game_Rows();
	Build_User_Rows();
}


/// <summary>
/// Puts the lobby's own rosters back the way the game list dialog created them: the
/// player's chat entry first, and an entry standing for the lobby itself at the head of
/// the game list.
/// </summary>
void UILobbyPresenterClass::Open(void)
{
	CurGame = 0;
	Net2IsGameListActive = true;

	Handle = Session.Handle;

	Session.Options.ScenarioDescription[0] = '\0';
	Session.ColorIdx = Session.PrefColor;
	Color = Session.ColorIdx;

	Clear_Vector(&Session.Games);
	Clear_Vector(&Session.Players);
	Clear_Vector(&Session.Chat);

	NodeNameType * const who = new NodeNameType;
	strcpy(who->Name, Session.Handle);
	who->Chat.LastTime = 0;
	who->Chat.LastChance = 0;
	who->Chat.Color = Session.GPacket.PlayerInfo.Color;
	Session.Chat.Add(who);

	NodeNameType * const game = new NodeNameType;
	strcpy(game->Name, "");
	game->Game.IsOpen = 0;
	game->Game.LastTime = 0;
	Session.Games.Add(game);

	Send_Join_Queries(true, false, true, true);

	Build_Game_Rows();
	Build_User_Rows();
}


/// <summary>
/// Records where a guest stands as its screen opens: it has accepted nothing yet, and the
/// scenario description is cleared because the host has not sent one.
/// </summary>
void UILobbyPresenterClass::Open_Guest(void)
{
	Build_Identity_Lists();
	Read_Options();

	CanAccept = false;

	for (int index = 0; index < Session.Players.Count(); index++) {
		if (strcmp(Session.Players[index]->Name, Session.Handle) == 0) {
			Session.Players[index]->Player.Status = 0;
		}
	}

	Session.Options.ScenarioDescription[0] = '\0';
	ScenarioName.clear();

	Rebuild_Network_Map_Preview();
	PreviewGeneration++;

	Build_User_Rows();
}


/// <summary>
/// The host's settings have arrived and been written to the session.
/// The model is read back where they landed rather than where a control is written, which is
/// the same split Fill_List took: a presentation that draws a different number of times
/// cannot lose a change or repeat one.
/// </summary>
void UILobbyPresenterClass::Options_Received(void)
{
	Read_Options();
	PreviewGeneration++;
	Build_User_Rows();
}


/// <summary>
/// Builds the country and color lists both setup screens show, and picks out the ones this
/// player is wearing. A side row carries the country it stands for rather than its position,
/// because the list holds only the countries that may be played.
/// </summary>
void UILobbyPresenterClass::Build_Identity_Lists(void)
{
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

	House = Session.House;
	Color = Session.ColorIdx;
}


/// <summary>
/// Reads the session's game options into the view-model, with the ranges the rules give
/// them. The ranges are the ones DisplayGameopts puts on the track bars on its initializing
/// pass, and they belong with the values because a range control clamps a value into the
/// range it is holding.
/// </summary>
void UILobbyPresenterClass::Read_Options(void)
{
	UnitCount = SliderType{Session.Options.UnitCount, 1, 10, 1};
	TechLevel = SliderType{BuildLevel, 1, MPLAYER_BUILD_LEVEL_MAX, 1};
	Credits = SliderType{Session.Options.Credits, MP_MIN_MONEY, Rule->MPMaxMoney, 100};
	AIPlayers = SliderType{Session.Options.AIPlayers, 0, 6, 1};
	AILevel = SliderType{(int)Session.Options.AIDifficulty, 0, 2, 1};

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
	Allies = Session.Options.AlliesAllowed;
	HarvTruce = Session.Options.HarvTruce;

	ScenarioName = Session.Options.ScenarioDescription;

	OptionsChanged = true;
}


/// <summary>
/// The game the host has just created. The setup opens on the first scenario whatever the
/// session was carrying, seeds the match, and tells the guests what this player is wearing,
/// which is what the dialog did by sending itself its own two selection changes.
/// </summary>
void UILobbyPresenterClass::Open_Host(void)
{
	VerNum.Init_Clipping();

	srand(NonCriticalRandomNumber(1, 0x7FFF));
	Seed = rand();

	Set_Scenario_Info_From_Index(0);
	Session.Options.ScenarioIndex = 0;

	Build_Identity_Lists();
	Read_Options();

	Rebuild_Network_Map_Preview();
	PreviewGeneration++;

	CanStart = true;

	Host_Side(SelectedSide);
	Host_Color(Color);
}


/// <summary>
/// Records the country the host is showing and tells the guests about it.
/// </summary>
void UILobbyPresenterClass::Host_Side(int row)
{
	if (row < 0 || row >= (int)Sides.size()) {
		return;
	}

	SelectedSide = row;
	House = Sides[row].Country;
	Session.House = (HousesType)House;

	if (Session.Players.Count() > 0) {
		Session.Players[0]->Player.House = Session.House;
	}

	PumpGameopts(1, 0);
	Build_User_Rows();
}


/// <summary>
/// Records the color the host asked for, resolved against the colors already taken.
/// A color somebody else is wearing is bumped forward until a free one is found; if that
/// moved the color away from what the host asked for, the host is told so and the search
/// runs again from the color it was wearing before.
/// </summary>
void UILobbyPresenterClass::Host_Color(int color)
{
	if (color < 0 || color >= (int)Colors.size()) {
		return;
	}

	int old_color = Session.ColorIdx;

	Session.ColorIdx = color;
	Session.PrefColor = Session.ColorIdx;

	int newcolor;
	int found;
	int probe = Session.ColorIdx;
	for (;;) {
		int count = 0;
		newcolor = probe;
		found = false;

		while (count < Session.Players.Count()) {
			if (count != 0 && Session.Players[count]->Player.Color == probe) {
				probe++;
				found = true;
			}
			count++;
		}

		if (!found) break;

		probe %= MAX_MPLAYER_COLORS;
	}

	int resolved = newcolor;
	if (newcolor != Session.ColorIdx) {
		Record_Message(ColorSystem, Fetch_String(TXT_COLOR_IN_USE));

		probe = old_color;
		for (;;) {
			int count = 0;
			old_color = probe;
			found = false;

			while (count < Session.Players.Count()) {
				if (count != 0 && Session.Players[count]->Player.Color == probe) {
					probe++;
					found = true;
				}
				count++;
			}

			if (!found) break;

			probe %= MAX_MPLAYER_COLORS;
		}

		resolved = old_color;
	}

	Session.ColorIdx = resolved;
	Color = resolved;

	if (Session.Players.Count() > 0) {
		Session.Players[0]->Player.Color = resolved;
	}

	Build_User_Rows();
	PumpGameopts(1, 0);
}


/// <summary>
/// Turns a game option on or off. Short Game needs bases, so turning bases off turns the
/// short game off with it and turning the short game on turns bases on.
/// </summary>
void UILobbyPresenterClass::Toggle(std::string const & which)
{
	if (which == UI_LOBBY_BASES) {
		Bases = !Bases;
		if (!Bases) ShortGame = false;
	} else if (which == UI_LOBBY_SHORTGAME) {
		ShortGame = !ShortGame;
		if (ShortGame) Bases = true;
	} else if (which == UI_LOBBY_CRATES) {
		Crates = !Crates;
	} else if (which == UI_LOBBY_FOG) {
		FogOfWar = !FogOfWar;
	} else if (which == UI_LOBBY_BRIDGES) {
		Bridges = !Bridges;
	} else if (which == UI_LOBBY_MCV) {
		MCVRedeploy = !MCVRedeploy;
	} else if (which == UI_LOBBY_ENGINEER) {
		MultiEngineer = !MultiEngineer;
	} else if (which == UI_LOBBY_ALLIES) {
		Allies = !Allies;
	} else if (which == UI_LOBBY_HARVTRUCE) {
		HarvTruce = !HarvTruce;
	} else {
		return;
	}

	Session.Options.Bases = Bases;
	Session.Options.ShortGame = ShortGame;
	Session.Options.Goodies = Crates;
	Session.Options.FogOfWar = FogOfWar;
	Session.Options.BridgeDestruction = Bridges;
	Session.Options.MCVRedeploy = MCVRedeploy;
	Session.Options.CrapEngineers = MultiEngineer;
	Session.Options.AlliesAllowed = Allies;
	Session.Options.HarvTruce = HarvTruce;

	OptionsChanged = true;
}


/// <summary>
/// Moves a game option's track bar. The dialog re-read every bar on each notification, so
/// the whole set is written through rather than the one that moved.
/// </summary>
void UILobbyPresenterClass::Slide(std::string const & which, int value)
{
	SliderType * slider = NULL;
	if (which == UI_LOBBY_UNITCOUNT) slider = &UnitCount;
	else if (which == UI_LOBBY_CREDITS) slider = &Credits;
	else if (which == UI_LOBBY_TECHLEVEL) slider = &TechLevel;
	else if (which == UI_LOBBY_AILEVEL) slider = &AILevel;
	else if (which == UI_LOBBY_AIPLAYERS) slider = &AIPlayers;
	else if (which == UI_LOBBY_GAMESPEED) slider = &GameSpeed;

	if (slider == NULL) {
		return;
	}

	if (value < slider->Minimum) value = slider->Minimum;
	if (value > slider->Maximum) value = slider->Maximum;
	slider->Value = value;

	Session.Options.UnitCount = UnitCount.Value;
	BuildLevel = TechLevel.Value;
	Session.Options.Credits = Credits.Value;
	Session.Options.AIPlayers = AIPlayers.Value;
	Session.Options.AIDifficulty = (DiffType)AILevel.Value;
	Session.Options.GameSpeed = 6 - GameSpeed.Value;
}


/// <summary>
/// Throws the picked players out of the game.
/// The rows are resolved to names here rather than when they were picked, because the roster
/// can move underneath a selection at any moment, and the host cannot kick itself.
/// </summary>
void UILobbyPresenterClass::Kick(void)
{
	for (int const row : PickedUsers) {
		if (row < 0 || row >= (int)Users.size()) {
			continue;
		}

		std::string const & name = Users[row].Name;
		if (name == Session.Handle) {
			continue;
		}

		int index = -1;
		for (int i = 0; i < Session.Players.Count(); i++) {
			if (name == Session.Players[i]->Name) {
				index = i;
				break;
			}
		}

		if (index == -1) {
			continue;
		}

		memset(&Session.GPacket, 0, sizeof(Session.GPacket));
		Session.GPacket.Command = NET_REJECT_JOIN;
		Session.GPacket.Reject.Why = (int)REJECT_BY_OWNER;
		Ipx.Send_Global_Message(&Session.GPacket, 455, 1, &Session.Players[index]->Address);
	}

	PickedUsers.clear();
	Build_User_Rows();
}


/// <summary>
/// Runs the scenario picker with the host screen out of the way, and puts the map it chose
/// on the model. Backing out leaves the scenario the screen was showing, which is what
/// putting the old index back did.
/// </summary>
void UILobbyPresenterClass::Run_Pending(void)
{
	if (Pending != SUB_PICK_MAP) {
		return;
	}

	Pending = SUB_NONE;

	int const previous = Session.Options.ScenarioIndex;

	IsRandomMap = false;
	bool const picked = Pick_Scenario_Screen();
	IsRandomMap = true;

	if (!picked) {
		Session.Options.ScenarioIndex = previous;
		Set_Scenario_Info_From_Index(previous);

		// Backing out puts the settings back on the wire, which is what the cancel arm did
		// and the accept arm left to the driver's own pump.
		PumpGameopts(1, 0);
	} else if (Set_Scenario_Info_From_Index(Session.Options.ScenarioIndex) != true) {
		Session.Options.ScenarioIndex = previous;
		Set_Scenario_Info_From_Index(previous);
	}

	ScenarioName = Session.Options.ScenarioDescription;

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
	OptionsChanged = true;
}


/// <summary>
/// Records a line of chat or system text for whatever is showing the lobby.
/// The line is kept whole; breaking it to the width it is shown at belongs to the
/// presentation, which is what _DrawMessage did with the list box it was handed.
/// </summary>
void UILobbyPresenterClass::Record_Message(int color, char const * text)
{
	if (text == NULL) {
		return;
	}

	Messages.push_back(ChatLineType{text, color});
	if ((int)Messages.size() > MESSAGE_LIMIT) {
		Messages.erase(Messages.begin());
	}

	MessagesChanged = true;
}


/// <summary>
/// Tells the game that this player has accepted the host's settings.
/// </summary>
void UILobbyPresenterClass::Accept(void)
{
	if (Session.Players.Count() == 0) {
		return;
	}

	Session.Players[0]->Player.Status = 1;
	CanAccept = false;

	SendPublicGameopts("A1");

	Build_User_Rows();
}


/// <summary>
/// Tells the host which country and color this player is showing. The country is whatever
/// the model is already holding, because the side is recorded by its own intent ahead of
/// this one, the way the dialog read both of its boxes before sending one packet.
/// </summary>
void UILobbyPresenterClass::Change_Identity(int color)
{
	Color = color;
	Session.PrefColor = color;

	char options[64];
	std::snprintf(options, sizeof(options), "R%d,%d", House, color);
	SendPrivateGameopts(Session.GameName, options);
}


/// <summary>
/// Reads the advertised games into the view-model and clamps the selection.
/// A game can vanish while the list is open, so a selection past the end is pulled back
/// and the queries are asked again for the game the selection landed on.
/// </summary>
void UILobbyPresenterClass::Build_Game_Rows(void)
{
	int const count = Session.Games.Count();

	if (CurGame >= count) {
		CurGame = count - 1;
		Send_Join_Queries(0, 1, 0, 0);
	}
	if (CurGame < 0) {
		CurGame = 0;
	}

	Games.clear();

	GameRowType lobby;
	lobby.Label = Fetch_String(TXT_LOBBY);
	lobby.IsOpen = false;
	Games.push_back(lobby);

	for (int index = 1; index < Session.Games.Count(); index++) {
		NodeNameType const * const node = Session.Games[index];

		// The name is the node's first member, which is what the caption format has always
		// been handed. A row carries the name and whether the game is open; the bracketed
		// caption belongs to the presentation.
		GameRowType row;
		row.Label = node->Name;
		row.IsOpen = node->Game.IsOpen != 0;
		Games.push_back(row);
	}

	SelectedGame = CurGame;
	GamesChanged = true;
}


/// <summary>
/// Reads the chat roster, or the selected game's player roster, into the view-model.
/// Out in the lobby the rows are the handles of everybody chatting. Inside a game each row
/// carries the player's color, the side its emblem stands for, and whether the player is
/// the host or has accepted the settings.
/// </summary>
void UILobbyPresenterClass::Build_User_Rows(void)
{
	Users.clear();

	if (CurGame == 0) {
		UserRowType me;
		me.Name = Session.Handle;
		Users.push_back(me);

		for (int index = 1; index < Session.Chat.Count(); index++) {
			UserRowType row;
			row.Name = Session.Chat[index]->Name;
			Users.push_back(row);
		}

		UsersChanged = true;
		return;
	}

	for (int index = 0; index < Session.Players.Count(); index++) {
		NodeNameType * const node = Session.Players[index];

		// The host counts as having accepted its own settings, which the list recorded on
		// the player rather than on the row it was about to draw.
		bool const host = strcmp(node->Name, Session.GameName) == 0;
		if (host) {
			node->Player.Status = 1;
		}

		UserRowType row;
		row.Name = node->Name;
		row.Color = node->Player.Color;
		row.IsHost = host;
		row.HasAccepted = node->Player.Status != 0;

		int const country = node->Player.House;
		row.Side = (country >= HOUSE_FIRST && country < HouseTypes.Count())
			? (int)HouseTypes[country]->Side : (int)SIDE_NONE;

		if (row.Side == SIDE_GDI) {
			row.SideName = Fetch_String(TXT_GDI);
		} else if (row.Side == SIDE_NOD || row.Side == SIDE_NONE) {
			row.SideName = Fetch_String(TXT_NOD);
		} else {
			row.SideName = (char const *)HouseTypes[country]->GivenName;
		}

		Users.push_back(row);
	}

	UsersChanged = true;
}


/// <summary>
/// One pass of the maintenance the lobby's driver ran on every turn of its own loop: the
/// transport is serviced, the join protocol is answered, the host's options are broadcast if
/// they moved, and a game or a chat partner that has stopped answering is dropped.
/// </summary>
void UILobbyPresenterClass::Service(void)
{
	Net2ServiceLobby();
}


/// <summary>
/// Records the name the player is showing and tells the lobby about it.
/// The name is truncated to what the session's buffer holds, and the model carries the
/// truncated name back so the field shows what was actually kept.
/// </summary>
void UILobbyPresenterClass::Rename(std::string const & name)
{
	if (name == Session.Handle) {
		return;
	}

	UTF8::Copy(Session.Handle, sizeof(Session.Handle), name.c_str());
	Handle = Session.Handle;

	Send_Join_Queries(0, 0, 1, 0);
	Build_User_Rows();
}


/// <summary>
/// Moves the highlight to another advertised game and asks that game who is in it.
/// A player already joined to a game cannot browse away from it, which is what the list
/// refused to do once JoinState left JOIN_NOTHING.
/// </summary>
void UILobbyPresenterClass::Pick_Game(int row)
{
	if (JoinState > JOIN_NOTHING) {
		return;
	}
	if (row < 0 || row >= Session.Games.Count() || !Net2IsGameListActive) {
		return;
	}

	int const previous = CurGame;

	CurGame = row;
	strcpy(Session.GameName, Session.Games[row]->Name);
	SelectedGame = CurGame;

	Clear_Vector(&Session.Players);

	if (previous != CurGame) {
		Send_Join_Queries(1, 1, 1, 0);
	}

	Build_User_Rows();
}


/// <summary>
/// Sends a line of chat to whoever is listening, which is the game's players once joined
/// and the lobby's chat roster otherwise. A line of two characters or fewer is dropped,
/// which is what the edit control's own handler did.
/// </summary>
void UILobbyPresenterClass::Say(std::string const & text)
{
	if (text.size() <= 2) {
		return;
	}

	PMessagePrintf(ColorMe, "[%s] %s", Session.Handle, text.c_str());

	GlobalPacketType gpacket;
	memset(&gpacket, 0, sizeof(gpacket));

	gpacket.Command = NET_MESSAGE;
	strcpy(gpacket.Name, Session.Handle);
	std::snprintf(gpacket.Message.Buf, sizeof(gpacket.Message.Buf), "%s", text.c_str());
	gpacket.Message.Color = Session.ColorIdx;
	gpacket.Message.NameCRC = Compute_Name_CRC(Session.GameName);

	DynamicVectorClass<NodeNameType *> & who =
		JoinState == JOIN_CONFIRMED ? Session.Players : Session.Chat;

	for (int index = 1; index < who.Count(); index++) {
		Ipx.Send_Global_Message(&gpacket, sizeof(gpacket), 1, &who[index]->Address);
		Call_Back();
	}
}


/// <summary>
/// Records what the driver loop is being asked to do next, and ends the pass.
/// The answer is the screen's result as well, because the runner returns on a result and the
/// driver reads the answer after it does; the family's own loop clears both before it shows
/// the next screen.
/// </summary>
void UILobbyPresenterClass::Answer(ResponseType response)
{
	Response = response;

	UIResult result;
	result.Outcome = response == RESPONSE_CANCEL
		? UIResult::OUTCOME_CANCELLED : UIResult::OUTCOME_ACCEPTED;
	Result = result;
}


void UILobbyPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_LOBBY_RENAME) {
		Rename(intent.Identity);
		return;
	}

	if (intent.Action == UI_LOBBY_PICK_GAME) {
		Pick_Game(intent.Value);
		return;
	}

	if (intent.Action == UI_LOBBY_SAY) {
		Say(intent.Identity);
		return;
	}

	if (intent.Action == UI_LOBBY_COLOR) {
		Session.ColorIdx = intent.Value;
		Color = intent.Value;
		return;
	}

	if (intent.Action == UI_LOBBY_SIDE) {
		House = intent.Value;
		return;
	}

	if (intent.Action == UI_LOBBY_TOGGLE) {
		Toggle(intent.Identity);
		return;
	}

	if (intent.Action == UI_LOBBY_SLIDER) {
		Slide(intent.Identity, intent.Value);
		return;
	}

	if (intent.Action == UI_LOBBY_PICK_USER) {
		// A picked row is added rather than replacing the selection, because the player list
		// is a multiple-selection one and a second click takes a row back off it.
		auto const found = std::find(PickedUsers.begin(), PickedUsers.end(), intent.Value);
		if (found != PickedUsers.end()) {
			PickedUsers.erase(found);
		} else if (intent.Value >= 0 && intent.Value < (int)Users.size()) {
			PickedUsers.push_back(intent.Value);
		}
		UsersChanged = true;
		return;
	}

	if (intent.Action == UI_LOBBY_KICK) {
		Kick();
		return;
	}

	if (intent.Action == UI_LOBBY_PICK_MAP) {
		Pending = SUB_PICK_MAP;
		return;
	}

	if (intent.Action == UI_LOBBY_GO) {
		// The button goes away until the driver has decided the game may begin, which is what
		// disabling the window stood for; the driver puts it back when it refuses.
		CanStart = false;
		Answer(RESPONSE_GO);
		return;
	}

	if (intent.Action == UI_LOBBY_IDENTITY) {
		Change_Identity(intent.Value);
		return;
	}

	if (intent.Action == UI_LOBBY_HOST_SIDE) {
		Host_Side(intent.Value);
		return;
	}

	if (intent.Action == UI_LOBBY_HOST_COLOR) {
		Host_Color(intent.Value);
		return;
	}

	if (intent.Action == UI_LOBBY_ACCEPT) {
		Accept();
		return;
	}

	if (intent.Action == UI_LOBBY_JOIN) {
		Answer(RESPONSE_JOIN);
		return;
	}

	if (intent.Action == UI_LOBBY_NEW) {
		Answer(RESPONSE_NEW);
		return;
	}

	if (intent.Action == UI_LOBBY_CANCEL) {
		Answer(RESPONSE_CANCEL);
		return;
	}
}


//---------------------------------------------------------------------------------------
// The RmlUi views.
//---------------------------------------------------------------------------------------

// The picture the preview frame holds, in game logical units. The frame is the setup
// templates' 126 x 73 dialog units, which is 189 by 118.625 at the family's 1.5 across and
// 1.625 down, and the picture sits inside its one pixel border.
enum { PREVIEW_WIDTH = 187, PREVIEW_HEIGHT = 116 };

inline constexpr char const * UI_LOBBY_HOST_PREVIEW = "lobbyhostpreview";
inline constexpr char const * UI_LOBBY_GUEST_PREVIEW = "lobbyguestpreview";


/// <summary>
/// The RmlUi half of one of the lobby's three screens.
/// The three documents show overlapping halves of one model, so one view serves them all
/// and binds only what its own document names. Each names its data model after its own
/// file, which is what the base class derives the name from; a document that names another
/// document's model gets no bindings and no events at all.
/// </summary>
class LobbyViewClass : public UIRmlViewClass
{
	public:
		// A color a player may take, with the swatch the owner-draw combo drew its row in.
		struct ColorRowType
		{
			std::string Name;
			std::string Hex;
		};

		// A player row as its document shows it: the model's row plus the color the name is
		// drawn in, the marker the list drew as a surface, and whether the host has picked
		// the row out to kick.
		struct UserViewType
		{
			std::string Name;
			std::string SideName;
			std::string Mark;
			std::string Hex;
			bool Picked = false;
		};

		struct MessageViewType
		{
			std::string Text;
			std::string Hex;
		};

		LobbyViewClass(UILobbyPresenterClass & presenter, char const * document,
			UILobbyPresenterClass::ScreenType kind, char const * preview);
		virtual ~LobbyViewClass(void) override;

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

		// Puts the track bar ranges the rules give this screen on the controls, and lets the
		// change handlers start reporting. A range is set before the data binding fills a
		// value in, so a value outside a track bar's default range is not clamped away.
		void Settle(void);

		UILobbyPresenterClass::ScreenType Kind;

	private:
		void Move(char const * which, int value);
		void Press(char const * action);
		void Set_Range(char const * id, UILobbyPresenterClass::SliderType const & slider);
		void Submit_Chat(void);
		void Rebuild_Rows(void);

		static std::string Swatch(int color);

		UILobbyPresenterClass & Screen;

		// The view owns the pixels; the presenter carries only the name they answer to.
		std::string Preview;
		MapPreviewSurfaceClass Picture{PREVIEW_WIDTH, PREVIEW_HEIGHT};

		std::vector<ColorRowType> ColorRows;
		std::vector<UserViewType> UserRows;
		std::vector<UILobbyPresenterClass::GameRowType> GameRows;
		std::vector<MessageViewType> MessageRows;

		unsigned int Drawn = 0;
		bool Settled = false;
};


LobbyViewClass::LobbyViewClass(UILobbyPresenterClass & presenter, char const * document,
	UILobbyPresenterClass::ScreenType kind, char const * preview) :
	UIRmlViewClass(presenter, document),
	Kind(kind),
	Screen(presenter),
	Preview(preview != NULL ? preview : "")
{
	if (!Preview.empty()) {
		UI_Register_Surface(Preview.c_str(), &Picture);
	}
}


LobbyViewClass::~LobbyViewClass(void)
{
	if (!Preview.empty()) {
		UI_Unregister_Surface(Preview.c_str());
	}
}


/// <summary>
/// Turns a player color into the CSS color the owner-draw list drew its row in, which is
/// what OD_SETCOLOR was handed out of PlayerColorTable.
/// </summary>
std::string LobbyViewClass::Swatch(int color)
{
	char hex[8];
	if (color >= 0 && color < MAX_PLAYERS) {
		// A COLORREF holds its blue byte highest, which is the order RGB() packs.
		unsigned long const packed = (unsigned long)PlayerColorTable[color];
		std::snprintf(hex, sizeof(hex), "#%02x%02x%02x",
			(unsigned)(packed & 0xFF), (unsigned)((packed >> 8) & 0xFF), (unsigned)((packed >> 16) & 0xFF));
	} else {
		std::snprintf(hex, sizeof(hex), "#b9bcae");
	}
	return(hex);
}


void LobbyViewClass::Set_Range(char const * id, UILobbyPresenterClass::SliderType const & slider)
{
	if (Element == nullptr) {
		return;
	}

	Rml::Element * const control = Element->GetElementById(id);
	if (control == nullptr) {
		return;
	}

	// The range is set before the value, because a track bar clamps a value into the range
	// it is holding and the default range stops well short of what the rules allow.
	control->SetAttribute("min", slider.Minimum);
	control->SetAttribute("max", slider.Maximum);
	control->SetAttribute("step", slider.Step);
	control->SetAttribute("value", slider.Value);
}


void LobbyViewClass::Settle(void)
{
	if (Kind != UILobbyPresenterClass::SCREEN_GAME_LIST) {
		Set_Range("unitcount", Screen.UnitCount);
		Set_Range("credits", Screen.Credits);
		Set_Range("techlevel", Screen.TechLevel);
		Set_Range("ailevel", Screen.AILevel);
		Set_Range("aiplayers", Screen.AIPlayers);
		Set_Range("gamespeed", Screen.GameSpeed);
	}

	Settled = true;
}


/// <summary>
/// Reads the chat entry and queues what was typed, then empties the field, which is what the
/// edit control's own handler did once its text had been taken.
/// </summary>
void LobbyViewClass::Submit_Chat(void)
{
	if (Element == nullptr) {
		return;
	}

	Rml::ElementFormControlInput * const field =
		rmlui_dynamic_cast<Rml::ElementFormControlInput *>(Element->GetElementById("say"));
	if (field == nullptr) {
		return;
	}

	Rml::String const text = field->GetValue();
	field->SetValue("");

	if (text.empty()) {
		return;
	}

	Screen.Queue(UIIntent{UI_LOBBY_SAY, text, 0});
}


void LobbyViewClass::Move(char const * which, int value)
{
	if (!Settled) return;

	UILobbyPresenterClass::SliderType const * held = NULL;
	if (which == UI_LOBBY_UNITCOUNT) held = &Screen.UnitCount;
	else if (which == UI_LOBBY_CREDITS) held = &Screen.Credits;
	else if (which == UI_LOBBY_TECHLEVEL) held = &Screen.TechLevel;
	else if (which == UI_LOBBY_AILEVEL) held = &Screen.AILevel;
	else if (which == UI_LOBBY_AIPLAYERS) held = &Screen.AIPlayers;
	else if (which == UI_LOBBY_GAMESPEED) held = &Screen.GameSpeed;

	if (held == NULL || held->Value == value) {
		return;
	}

	Screen.Queue(UIIntent{UI_LOBBY_SLIDER, which, value});
}


/// <summary>
/// Queues what a button or its key stands for. The game list's name field is read here
/// rather than tracked, because that is when the dialog read its edit control.
/// </summary>
void LobbyViewClass::Press(char const * action)
{
	if (Kind == UILobbyPresenterClass::SCREEN_GAME_LIST && Element != nullptr) {
		Rml::ElementFormControlInput * const field =
			rmlui_dynamic_cast<Rml::ElementFormControlInput *>(Element->GetElementById("yourname"));
		if (field != nullptr) {
			Rml::String const text = field->GetValue();
			if (text != Screen.Handle) {
				Screen.Queue(UIIntent{UI_LOBBY_RENAME, text, 0});
			}
		}
	}

	Screen.Queue(UIIntent{action, "", 0});
}


void LobbyViewClass::Rebuild_Rows(void)
{
	GameRows = Screen.Games;

	UserRows.clear();
	for (int index = 0; index < (int)Screen.Users.size(); index++) {
		UILobbyPresenterClass::UserRowType const & row = Screen.Users[index];

		UserViewType view;
		view.Name = row.Name;
		view.SideName = row.SideName;
		view.Hex = Swatch(row.Color);

		// The host and accepted markers, which the list drew as the wolhost.pcx and
		// wolacpt.pcx surfaces.
		if (row.IsHost) {
			view.Mark = "*";
		} else if (row.HasAccepted) {
			view.Mark = "+";
		}

		view.Picked = std::find(Screen.PickedUsers.begin(), Screen.PickedUsers.end(), index)
			!= Screen.PickedUsers.end();

		UserRows.push_back(view);
	}

	MessageRows.clear();
	for (UILobbyPresenterClass::ChatLineType const & line : Screen.Messages) {
		MessageViewType view;
		view.Text = line.Text;

		if (line.Color < 0) {
			view.Hex = "#b9bcae";
		} else {
			unsigned long const packed = (unsigned long)line.Color;
			char hex[8];
			std::snprintf(hex, sizeof(hex), "#%02x%02x%02x",
				(unsigned)(packed & 0xFF), (unsigned)((packed >> 8) & 0xFF), (unsigned)((packed >> 16) & 0xFF));
			view.Hex = hex;
		}

		MessageRows.push_back(view);
	}
}


void LobbyViewClass::Bind(Rml::DataModelConstructor & model)
{
	ColorRows.clear();
	for (int index = 0; index < (int)Screen.Colors.size(); index++) {
		ColorRows.push_back(ColorRowType{Screen.Colors[index], Swatch(index)});
	}

	Rebuild_Rows();

	if (auto row = model.RegisterStruct<UILobbyPresenterClass::GameRowType>()) {
		row.RegisterMember("label", &UILobbyPresenterClass::GameRowType::Label);
		row.RegisterMember("isopen", &UILobbyPresenterClass::GameRowType::IsOpen);
	}
	model.RegisterArray<std::vector<UILobbyPresenterClass::GameRowType>>();

	if (auto row = model.RegisterStruct<UserViewType>()) {
		row.RegisterMember("name", &UserViewType::Name);
		row.RegisterMember("sidename", &UserViewType::SideName);
		row.RegisterMember("mark", &UserViewType::Mark);
		row.RegisterMember("hex", &UserViewType::Hex);
		row.RegisterMember("picked", &UserViewType::Picked);
	}
	model.RegisterArray<std::vector<UserViewType>>();

	if (auto row = model.RegisterStruct<MessageViewType>()) {
		row.RegisterMember("text", &MessageViewType::Text);
		row.RegisterMember("hex", &MessageViewType::Hex);
	}
	model.RegisterArray<std::vector<MessageViewType>>();

	if (auto swatch = model.RegisterStruct<ColorRowType>()) {
		swatch.RegisterMember("name", &ColorRowType::Name);
		swatch.RegisterMember("hex", &ColorRowType::Hex);
	}
	model.RegisterArray<std::vector<ColorRowType>>();

	if (auto side = model.RegisterStruct<UILobbyPresenterClass::SideType>()) {
		side.RegisterMember("name", &UILobbyPresenterClass::SideType::Name);
	}
	model.RegisterArray<std::vector<UILobbyPresenterClass::SideType>>();

	if (auto slider = model.RegisterStruct<UILobbyPresenterClass::SliderType>()) {
		slider.RegisterMember("value", &UILobbyPresenterClass::SliderType::Value);
		slider.RegisterMember("min", &UILobbyPresenterClass::SliderType::Minimum);
		slider.RegisterMember("max", &UILobbyPresenterClass::SliderType::Maximum);
		slider.RegisterMember("step", &UILobbyPresenterClass::SliderType::Step);
	}

	model.Bind("handle", &Screen.Handle);
	model.Bind("games", &GameRows);
	model.Bind("selectedgame", &Screen.SelectedGame);
	model.Bind("users", &UserRows);
	model.Bind("messages", &MessageRows);

	model.Bind("sides", &Screen.Sides);
	model.Bind("selectedside", &Screen.SelectedSide);
	model.Bind("colors", &ColorRows);
	model.Bind("selectedcolor", &Screen.Color);

	model.Bind("scenarioname", &Screen.ScenarioName);
	model.Bind("preview", &Preview);

	model.Bind("unitcount", &Screen.UnitCount);
	model.Bind("credits", &Screen.Credits);
	model.Bind("techlevel", &Screen.TechLevel);
	model.Bind("ailevel", &Screen.AILevel);
	model.Bind("aiplayers", &Screen.AIPlayers);
	model.Bind("gamespeed", &Screen.GameSpeed);

	model.Bind("bases", &Screen.Bases);
	model.Bind("crates", &Screen.Crates);
	model.Bind("fog", &Screen.FogOfWar);
	model.Bind("bridges", &Screen.Bridges);
	model.Bind("mcv", &Screen.MCVRedeploy);
	model.Bind("shortgame", &Screen.ShortGame);
	model.Bind("engineer", &Screen.MultiEngineer);
	model.Bind("allies", &Screen.Allies);
	model.Bind("harvtruce", &Screen.HarvTruce);

	model.Bind("canaccept", &Screen.CanAccept);
	model.Bind("canstart", &Screen.CanStart);

	// The field is bound one way, so a value the model already holds is never queued back as
	// a change the player did not type.
	model.BindEventCallback("rename",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			Rml::String const value = event.GetParameter<Rml::String>("value", Rml::String());
			if (value == Screen.Handle) return;
			Screen.Queue(UIIntent{UI_LOBBY_RENAME, value, 0});
		});

	model.BindEventCallback("pickgame",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_LOBBY_PICK_GAME, "", arguments[0].Get<int>()});
		});

	model.BindEventCallback("pickuser",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_LOBBY_PICK_USER, "", arguments[0].Get<int>()});
		});

	model.BindEventCallback("chooseside",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			if (!Settled) return;
			int const row = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);
			if (row == Screen.SelectedSide) return;

			if (Kind == UILobbyPresenterClass::SCREEN_HOST) {
				Screen.Queue(UIIntent{UI_LOBBY_HOST_SIDE, "", row});
				return;
			}

			// The guest records the side ahead of the color, because the dialog read both of
			// its boxes and sent one packet carrying the pair.
			Screen.SelectedSide = row;
			if (row >= 0 && row < (int)Screen.Sides.size()) {
				Screen.Queue(UIIntent{UI_LOBBY_SIDE, "", Screen.Sides[row].Country});
			}
			Screen.Queue(UIIntent{UI_LOBBY_IDENTITY, "", Screen.Color});
		});

	model.BindEventCallback("choosecolor",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			if (!Settled) return;
			int const row = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);
			if (row == Screen.Color) return;

			if (Kind == UILobbyPresenterClass::SCREEN_HOST) {
				Screen.Queue(UIIntent{UI_LOBBY_HOST_COLOR, "", row});
				return;
			}

			if (Screen.SelectedSide >= 0 && Screen.SelectedSide < (int)Screen.Sides.size()) {
				Screen.Queue(UIIntent{UI_LOBBY_SIDE, "", Screen.Sides[Screen.SelectedSide].Country});
			}
			Screen.Queue(UIIntent{UI_LOBBY_IDENTITY, "", row});
		});

	model.BindEventCallback("move",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const which = arguments[0].Get<Rml::String>();
			int const value = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);

			if (which == UI_LOBBY_UNITCOUNT) Move(UI_LOBBY_UNITCOUNT, value);
			else if (which == UI_LOBBY_CREDITS) Move(UI_LOBBY_CREDITS, value);
			else if (which == UI_LOBBY_TECHLEVEL) Move(UI_LOBBY_TECHLEVEL, value);
			else if (which == UI_LOBBY_AILEVEL) Move(UI_LOBBY_AILEVEL, value);
			else if (which == UI_LOBBY_AIPLAYERS) Move(UI_LOBBY_AIPLAYERS, value);
			else if (which == UI_LOBBY_GAMESPEED) Move(UI_LOBBY_GAMESPEED, value);
		});

	// A check box is a class plus a click that queues a toggle, not a two-way bound control.
	model.BindEventCallback("toggle",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_LOBBY_TOGGLE, arguments[0].Get<Rml::String>(), 0});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const action = arguments[0].Get<Rml::String>();
			if (action == UI_LOBBY_JOIN) Press(UI_LOBBY_JOIN);
			else if (action == UI_LOBBY_NEW) Press(UI_LOBBY_NEW);
			else if (action == UI_LOBBY_CANCEL) Press(UI_LOBBY_CANCEL);
			else if (action == UI_LOBBY_ACCEPT) Press(UI_LOBBY_ACCEPT);
			else if (action == UI_LOBBY_GO) Press(UI_LOBBY_GO);
			else if (action == UI_LOBBY_KICK) Press(UI_LOBBY_KICK);
			else if (action == UI_LOBBY_PICK_MAP) Press(UI_LOBBY_PICK_MAP);
		});

	// Enter in the chat field sends the line, which is what EN_MAXTEXT stood for on an
	// ES_WANTRETURN edit control. Escape backs out of the screen.
	model.BindEventCallback("submit",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Submit_Chat();
				event.StopPropagation();
			}
		});

	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Press(UI_LOBBY_CANCEL);
			}
		});
}


void LobbyViewClass::Sync(void)
{
	if (!Model) return;

	Rebuild_Rows();

	Model.DirtyVariable("games");
	Model.DirtyVariable("selectedgame");
	Model.DirtyVariable("users");
	Model.DirtyVariable("messages");
	Model.DirtyVariable("scenarioname");
	Model.DirtyVariable("canaccept");
	Model.DirtyVariable("canstart");

	// The track bar, combo box and field values are not dirtied, because each already
	// carries what its own change event reported. The options are, because the host's
	// coupling and the guest's packets both move them from underneath.
	Model.DirtyVariable("bases");
	Model.DirtyVariable("crates");
	Model.DirtyVariable("fog");
	Model.DirtyVariable("bridges");
	Model.DirtyVariable("mcv");
	Model.DirtyVariable("shortgame");
	Model.DirtyVariable("engineer");
	Model.DirtyVariable("allies");
	Model.DirtyVariable("harvtruce");

	// The guest never moves a track bar -- every one of them is WS_DISABLED on its template
	// -- so its values come from the host's packets and have to be dirtied. The host's own
	// bars already carry what their change events reported.
	if (Kind == UILobbyPresenterClass::SCREEN_GUEST) {
		Model.DirtyVariable("unitcount");
		Model.DirtyVariable("credits");
		Model.DirtyVariable("techlevel");
		Model.DirtyVariable("ailevel");
		Model.DirtyVariable("aiplayers");
		Model.DirtyVariable("gamespeed");
		Model.DirtyVariable("selectedside");
		Model.DirtyVariable("selectedcolor");
	}

	// The picture is redrawn where it changed, not every pass, so the element uploads once
	// per map rather than once per present.
	if (!Preview.empty() && Screen.PreviewGeneration != Drawn) {
		Drawn = Screen.PreviewGeneration;
		Picture.Redraw();
	}
}


// The three documents, kept alive across the driver's passes because the lobby moves
// between them and comes back, the way it kept its host and game list dialogs alive
// together.
static LobbyViewClass * _GameListView = NULL;
static LobbyViewClass * _HostView = NULL;
static LobbyViewClass * _GuestView = NULL;
static LobbyViewClass * _ShownView = NULL;


static LobbyViewClass ** Lobby_View_Slot(UILobbyPresenterClass::ScreenType kind)
{
	switch (kind) {
		case UILobbyPresenterClass::SCREEN_GAME_LIST: return(&_GameListView);
		case UILobbyPresenterClass::SCREEN_HOST:      return(&_HostView);
		case UILobbyPresenterClass::SCREEN_GUEST:     return(&_GuestView);
		default:                                      return(NULL);
	}
}


void UI_Lobby_Close_Views(void)
{
	delete _GameListView;
	delete _HostView;
	delete _GuestView;

	_GameListView = NULL;
	_HostView = NULL;
	_GuestView = NULL;
	_ShownView = NULL;
}


/// <summary>
/// Shows whichever of the three documents the screen says it is on, and runs it until the
/// player answers.
/// </summary>
UIResult UI_Lobby_Run(UILobbyPresenterClass & presenter)
{
	UIResult failed;
	failed.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;

	LobbyViewClass ** const slot = Lobby_View_Slot(presenter.Showing);
	if (slot == NULL) {
		return(failed);
	}

	if (*slot == NULL) {
		char const * document = "gamelist.rml";
		char const * preview = NULL;
		if (presenter.Showing == UILobbyPresenterClass::SCREEN_HOST) {
			document = "mphost.rml";
			preview = UI_LOBBY_HOST_PREVIEW;
		} else if (presenter.Showing == UILobbyPresenterClass::SCREEN_GUEST) {
			document = "mpguest.rml";
			preview = UI_LOBBY_GUEST_PREVIEW;
		}

		LobbyViewClass * const view = new LobbyViewClass(presenter, document, presenter.Showing, preview);
		if (!view->Prepare(true)) {
			delete view;
			return(failed);
		}

		view->Settle();
		*slot = view;
	}

	// The screen the lobby moved away from steps aside rather than being torn down, because
	// it is come back to and its document is the same one.
	if (_ShownView != NULL && _ShownView != *slot) {
		_ShownView->Hide();
	}
	if (!(*slot)->Is_Visible()) {
		(*slot)->Show();
	}
	_ShownView = *slot;

	// The ranges go back on the controls every time a screen is shown, because a screen that
	// is come back to opens on what the model holds now rather than on what it held when the
	// document was first loaded.
	(*slot)->Settle();

	// A family reopened in a loop resets the close mark and the held result, since a close
	// marks the presenter closing and a marked presenter drains nothing.
	presenter.Result.reset();
	presenter.IsClosing = false;
	presenter.Answered = false;
	presenter.Running = presenter.Showing;

	(*slot)->Sync();

	while (!presenter.Result.has_value()) {
		UI_Run_Modal(presenter, **slot);

		if (presenter.Pending == UILobbyPresenterClass::SUB_NONE) {
			break;
		}

		// The scenario picker draws where the host screen is, so the document steps aside
		// for it, which is what the dialog's own ShowWindow did.
		(*slot)->Hide();
		presenter.Run_Pending();
		(*slot)->Show();
		(*slot)->Sync();
	}

	presenter.Running = UILobbyPresenterClass::SCREEN_NONE;
	return(presenter.Result.value_or(UIResult{}));
}
