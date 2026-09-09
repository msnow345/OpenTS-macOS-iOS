/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The network lobby's behavior, with no toolkit in it. The game list, the host setup and
// the guest setup are one screen family sharing one model, because they share the session's
// game, player and chat rosters and hand the driver one answer between them.
//
// This holds the game list half. The host and guest commands still write the driver's
// response themselves.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_LOBBY_RENAME = "rename";
inline constexpr char const * UI_LOBBY_PICK_GAME = "pickgame";
inline constexpr char const * UI_LOBBY_JOIN = "join";
inline constexpr char const * UI_LOBBY_NEW = "new";
inline constexpr char const * UI_LOBBY_CANCEL = "cancel";
inline constexpr char const * UI_LOBBY_SAY = "say";
inline constexpr char const * UI_LOBBY_COLOR = "color";
inline constexpr char const * UI_LOBBY_SIDE = "side";
inline constexpr char const * UI_LOBBY_IDENTITY = "identity";
inline constexpr char const * UI_LOBBY_ACCEPT = "accept";


class UILobbyPresenterClass : public UIPresenterClass
{
	public:
		// What the driver loop is being asked to do next. These stand where the dialogs'
		// own control identifiers stood, so a presenter names no control.
		enum ResponseType {
			RESPONSE_NONE,
			RESPONSE_CANCEL,
			RESPONSE_JOIN,
			RESPONSE_NEW,
		};

		// A game somebody is advertising. The lobby itself heads the list.
		struct GameRowType
		{
			std::string Label;
			bool IsOpen = false;
		};

		// Somebody in the lobby, or a player in the game the list is showing. Out in the
		// lobby only the name is known; inside a game the row also carries the color, the
		// side its emblem stands for and whether the player is the host or has accepted.
		struct UserRowType
		{
			std::string Name;
			std::string SideName;
			int Color = 0;

		// The country the player is showing, which is a country rather than a row, because
		// the side list holds only the countries that may be played.
		int House = 0;

		// Is the accept button available? A guest may accept once per change the host
		// makes, which is what disabling the button after a press stood for.
		bool CanAccept = false;
			int Side = -1;
			bool IsHost = false;
			bool HasAccepted = false;
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// The rosters the lobby opens with, which the game list dialog built as it was
		// created: the player's own chat entry and the lobby's own game entry.
		void Open(void);

		// The rosters and the guest's own standing when the guest screen opens: a guest
		// arrives having accepted nothing.
		void Open_Guest(void);

		// Reads the session's rosters into the view-model. Marking the host as accepted
		// happens here rather than while drawing, because it is a fact about the player
		// rather than about the row.
		void Build_Game_Rows(void);
		void Build_User_Rows(void);

		/*
		**	The view-model.
		*/
		std::string Handle;

		// The longest handle the name field accepts, in bytes, which is the limit the
		// dialog set on its edit control.
		enum { HANDLE_LIMIT = 16 };

		int Color = 0;

		// The country the player is showing, which is a country rather than a row, because
		// the side list holds only the countries that may be played.
		int House = 0;

		// Is the accept button available? A guest may accept once per change the host
		// makes, which is what disabling the button after a press stood for.
		bool CanAccept = false;

		std::vector<GameRowType> Games;
		int SelectedGame = 0;

		std::vector<UserRowType> Users;

		ResponseType Response = RESPONSE_NONE;

		// Did the last executed intent move a roster? A view redraws only what moved.
		bool GamesChanged = false;
		bool UsersChanged = false;

	private:
		void Rename(std::string const & name);
		void Pick_Game(int row);
		void Say(std::string const & text);
		void Accept(void);
		void Change_Identity(int color);
};
