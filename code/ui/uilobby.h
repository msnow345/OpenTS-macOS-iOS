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
// All three screens are here: the game list, the host setup and the guest setup.
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
inline constexpr char const * UI_LOBBY_GO = "go";
inline constexpr char const * UI_LOBBY_HOST_SIDE = "hostside";
inline constexpr char const * UI_LOBBY_HOST_COLOR = "hostcolor";
inline constexpr char const * UI_LOBBY_KICK = "kick";
inline constexpr char const * UI_LOBBY_PICK_USER = "pickuser";
inline constexpr char const * UI_LOBBY_PICK_MAP = "pickmap";
inline constexpr char const * UI_LOBBY_TOGGLE = "toggle";
inline constexpr char const * UI_LOBBY_SLIDER = "slider";

// The game options the host owns. A track bar and a check box name themselves, because an
// intent carries an identity rather than a control.
inline constexpr char const * UI_LOBBY_UNITCOUNT = "unitcount";
inline constexpr char const * UI_LOBBY_CREDITS = "credits";
inline constexpr char const * UI_LOBBY_TECHLEVEL = "techlevel";
inline constexpr char const * UI_LOBBY_AILEVEL = "ailevel";
inline constexpr char const * UI_LOBBY_AIPLAYERS = "aiplayers";
inline constexpr char const * UI_LOBBY_GAMESPEED = "gamespeed";

inline constexpr char const * UI_LOBBY_BASES = "bases";
inline constexpr char const * UI_LOBBY_CRATES = "crates";
inline constexpr char const * UI_LOBBY_FOG = "fog";
inline constexpr char const * UI_LOBBY_BRIDGES = "bridges";
inline constexpr char const * UI_LOBBY_MCV = "mcv";
inline constexpr char const * UI_LOBBY_SHORTGAME = "shortgame";
inline constexpr char const * UI_LOBBY_ENGINEER = "engineer";
inline constexpr char const * UI_LOBBY_ALLIES = "allies";
inline constexpr char const * UI_LOBBY_HARVTRUCE = "harvtruce";


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
			RESPONSE_GO,
		};

		// Which of the family's three screens is being shown. The driver moves between them
		// and the presentation follows; a presenter names a screen rather than a window.
		enum ScreenType {
			SCREEN_NONE,
			SCREEN_GAME_LIST,
			SCREEN_HOST,
			SCREEN_GUEST,
		};

		// A screen the lobby opens and comes back from. The scenario picker draws where the
		// host screen is, so its owner takes the host screen off the screen and puts it
		// back rather than running it underneath.
		enum PendingType {
			SUB_NONE,
			SUB_PICK_MAP,
		};

		// A country that may be played, carrying the country itself rather than its
		// position, because the list holds only the multiplayable countries.
		struct SideType
		{
			std::string Name;
			int Country = 0;
		};

		// A track bar and the range the rules give it. A range control clamps a value into
		// the range it is holding, so a view sets the range before the value.
		struct SliderType
		{
			int Value = 0;
			int Minimum = 0;
			int Maximum = 0;
			int Step = 1;
		};

		// A line of chat or system text, as PMessagePrintf composed it. Wrapping it to the
		// width it is shown at belongs to the presentation.
		struct ChatLineType
		{
			std::string Text;
			int Color = -1;
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
			int Side = -1;
			bool IsHost = false;
			bool HasAccepted = false;
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// The scenario picker draws where the host screen is, so the host screen is stepped
		// aside for it rather than run underneath. The family also steps aside when the
		// protocol moves it to another of its three screens, because the runner picks the
		// document once and a join is confirmed from inside the service.
		virtual bool Suspends(void) const override
		{
			return(Pending != SUB_NONE || (Running != SCREEN_NONE && Running != Showing));
		}

		// The screen the runner is holding a document open for, which the runner sets and
		// clears around itself.
		ScreenType Running = SCREEN_NONE;

		// The rosters the lobby opens with, which the game list dialog built as it was
		// created: the player's own chat entry and the lobby's own game entry.
		void Open(void);

		// The rosters and the guest's own standing when the guest screen opens: a guest
		// arrives having accepted nothing.
		void Open_Guest(void);

		// The game the host has just created: the scenario the setup opens on, the option
		// values the rules allow, and the country and color lists both setup screens show.
		void Open_Host(void);

		// Runs the scenario picker with the host screen out of the way, and puts the map it
		// chose on the model. Called by the owner between passes, never from an event.
		void Run_Pending(void);

		// Records a line of chat or system text for whatever is showing the lobby. Called
		// from the network code wherever PMessagePrintf composes one.
		void Record_Message(int color, char const * text);

		// The host's settings have arrived and been written to the session. Called where the
		// options are decoded, so a presentation that is not a window sees them too.
		void Options_Received(void);

		// Reads the session's rosters into the view-model. Marking the host as accepted
		// happens here rather than while drawing, because it is a fact about the player
		// rather than about the row.
		void Build_Game_Rows(void);
		void Build_User_Rows(void);

		/*
		**	The view-model.
		*/
		ScreenType Showing = SCREEN_NONE;

		std::string Handle;

		// The longest handle the name field accepts, in bytes. Session.Handle is
		// MPLAYER_NAME_MAX bytes and travels in a packet field of that size, so anything
		// longer is thrown away rather than sent.
		enum { HANDLE_LIMIT = 11 };

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

		// Which player rows the host has picked out to kick. The list is a LBS_MULTIPLESEL
		// one, so this is a set of rows rather than a single selection.
		std::vector<int> PickedUsers;

		std::vector<ChatLineType> Messages;

		// The most lines the model keeps, which is what the message list box was capped at.
		enum { MESSAGE_LIMIT = 128 };

		/*
		**	The host's half of the model. The guest screen shows the same fields and cannot
		**	change them, which is what WS_DISABLED on every one of its controls stood for.
		*/
		std::vector<SideType> Sides;
		int SelectedSide = 0;

		std::vector<std::string> Colors;

		std::string ScenarioName;

		SliderType UnitCount;
		SliderType Credits;
		SliderType TechLevel;
		SliderType AILevel;
		SliderType AIPlayers;
		SliderType GameSpeed;

		bool Bases = false;
		bool Crates = false;
		bool FogOfWar = false;
		bool Bridges = false;
		bool MCVRedeploy = false;
		bool ShortGame = false;
		bool MultiEngineer = false;
		bool Allies = false;
		bool HarvTruce = false;

		// Is the start button available? Pressing it takes it away until the driver has
		// decided the game may begin, which is what disabling the window stood for.
		bool CanStart = true;

		PendingType Pending = SUB_NONE;

		// Moves when the map picture changes, so a view uploads once per map rather than
		// once per present.
		unsigned int PreviewGeneration = 0;

		ResponseType Response = RESPONSE_NONE;

		// Did the last executed intent move a roster? A view redraws only what moved.
		bool GamesChanged = false;
		bool UsersChanged = false;
		bool MessagesChanged = false;
		bool OptionsChanged = false;

	private:
		void Answer(ResponseType response);
		void Rename(std::string const & name);
		void Pick_Game(int row);
		void Say(std::string const & text);
		void Accept(void);
		void Change_Identity(int color);

		void Build_Identity_Lists(void);
		void Read_Options(void);
		void Host_Side(int row);
		void Host_Color(int color);
		void Toggle(std::string const & which);
		void Slide(std::string const & which, int value);
		void Kick(void);
};


// The lobby screen the driver is running, or NULL when no lobby is up. The network code
// reaches the model through this wherever a change is produced away from a screen.
UILobbyPresenterClass * UI_Lobby_Screen(void);
void UI_Set_Lobby_Screen(UILobbyPresenterClass * screen);


// Shows whichever of the three documents the screen says it is on, and runs it until the
// player answers. The documents outlive one call, because the lobby moves between them and
// comes back; UI_Lobby_Close_Views drops them when the lobby ends.
UIResult UI_Lobby_Run(UILobbyPresenterClass & presenter);
void UI_Lobby_Close_Views(void);
