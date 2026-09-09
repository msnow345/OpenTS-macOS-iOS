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
#include "session.h"
#include "utf8.h"

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
	House = Session.House;
	Color = Session.ColorIdx;
	CanAccept = false;

	for (int index = 0; index < Session.Players.Count(); index++) {
		if (strcmp(Session.Players[index]->Name, Session.Handle) == 0) {
			Session.Players[index]->Player.Status = 0;
		}
	}

	Session.Options.ScenarioDescription[0] = '\0';

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
/// The maintenance the driver ran on every pass of its own loop: a game or a chat partner
/// that has stopped answering is dropped, and a partner close to timing out is asked once
/// more before it goes.
/// </summary>
void UILobbyPresenterClass::Service(void)
{
	Net2ServiceGameList();
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
		Response = RESPONSE_GO;
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
		Response = RESPONSE_JOIN;
		return;
	}

	if (intent.Action == UI_LOBBY_NEW) {
		Response = RESPONSE_NEW;
		return;
	}

	if (intent.Action == UI_LOBBY_CANCEL) {
		Response = RESPONSE_CANCEL;
		return;
	}
}
