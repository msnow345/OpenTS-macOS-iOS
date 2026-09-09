/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The network lobby. Behavior traced out of MPlayer_Game_List_Dialog_Proc,
// Net2DisplayGameList, _Net2DisplayUsers and Net2Remote_Connect in netdlg2.cpp.
//
// What the extraction fixes in place: the rosters are read into the model where the
// session changes rather than where a list is drawn, so a presentation that draws a
// different number of times cannot lose a change or repeat one; the host's accepted
// status is a fact about the player, so it is recorded with the roster rather than while
// painting a row; a game row carries whether the game is open rather than the bracketed
// caption; and the selection is clamped against a list a host can shorten at any moment,
// which is what the display function did before it drew.
//
// Packets are untouched. Nothing here changes what goes on the wire, only who owns the
// state the screens show.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uilobby.h"

#include "_timer.h"
#include "conquer.h"
#include "data.h"
#include "houstype.h"
#include "ipxmgr.h"
#include "language/language.h"
#include "mplayer.h"
#include "netdlg.h"
#include "netdlg2.h"
#include "netshare.h"
#include "session.h"
#include "utf8.h"

#include <cstdio>
#include <cstring>


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
