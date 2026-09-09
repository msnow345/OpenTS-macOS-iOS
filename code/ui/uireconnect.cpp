/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The reconnect and kick-vote screen. What is preserved from IDD_MPLAYER_DISCONNECT, and
// where each came from: a seat gets a button carrying its name and a bar beside it that
// shrinks and turns yellow and then red as the wait on that seat drags on, which is what
// Draw_Sync_Bars painted straight into the surface; pressing a seat's button proposes that
// the seat be kicked, and proposing to kick yourself, or anybody at all in a tournament
// game, earns a line in the message list and nothing else; the message list carries the
// stall's own explanation, which differs between a reconnect and a first-time wait and
// between a LAN game and an internet one; and Cancel gives up on the game.
//
// The screen has no loop of its own. Wait_For_Players keeps servicing the network while the
// game is stalled, and it opens the screen, services it once a pass and closes it, the way
// it created and destroyed a modeless dialog.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uireconnect.h"

#include "uiinternal.h"
#include "uirmlview.h"
#include "uishell.h"

#include "data.h"
#include "dbgprint.h"
#include "ipxmgr.h"
#include "language/language.h"
#include "netglobal.h"
#include "queue.h"
#include "session.h"
#include "stats.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstdio>


bool Cast_Kick_Vote(int kicker, int kickee);


/// <summary>
/// Records the seats, the stall's explanation and the cleared vote tallies the screen opens
/// with.
/// </summary>
/// <param name="reconnect">True when the game is trying to reconnect to somebody, false
/// when it is still waiting for a connection that has never been made.</param>
/// <param name="frames">Each connection's reported frame number, used to name the seat the
/// game is furthest behind.</param>
/// <param name="connections">How many entries frames carries.</param>
void UIReconnectPresenterClass::Open(bool reconnect, int const * frames, int connections)
{
	Cancelled = false;
	Messages.clear();
	TimeText.clear();
	Result.reset();
	IsClosing = false;

	// A stale proposal from an earlier stall is not a vote in this one.
	while (Session.KickProposals.Count()) {
		delete Session.KickProposals[0];
		Session.KickProposals.Delete_Index(0);
	}
	memset(Session.KickVoteCount, 0, sizeof(Session.KickVoteCount));
	memset(Session.KickVoteWho, 0xFF, sizeof(Session.KickVoteWho));

	Refresh();

	char buffer[256];

	if (!reconnect) {
		Record_Message(Fetch_String(TXT_WAITING_FOR_CONNECTIONS));
		return;
	}

	// The seat the game is furthest behind is the one it is trying to reconnect to.
	int oldest = 0;
	int lowest = 0x7fffffff;
	for (int index = 0; index < connections && frames != NULL; index++) {
		if (frames[index] < lowest) {
			lowest = frames[index];
			oldest = index;
		}
	}

	char const * name = "";
	if (Session.Type == GAME_IPX || Session.Type == GAME_INTERNET) {
		name = Ipx.Connection_Name(Ipx.Connection_ID(oldest));
	} else if (Session.Players.Count() > 1) {
		name = Session.Players[1]->Name;
	}

	std::snprintf(buffer, sizeof(buffer), Fetch_String(TXT_RECONNECTING_TO), name);
	Record_Message(buffer);
	Record_Message("");

	if (Session.Type == GAME_INTERNET) {
		Record_Message(Fetch_String(TXT_RECONNECT_HELP3));
		Record_Message(Fetch_String(TXT_RECONNECT_HELP3B));
		Record_Message(Fetch_String(TXT_RECONNECT_HELP3C));
	}

	Record_Message(Fetch_String(TXT_RECONNECT_HELP2));
	if (Session.Type == GAME_INTERNET) {
		Record_Message(Fetch_String(TXT_RECONNECT_HELP2B));
	} else if (Session.Type == GAME_IPX) {
		Record_Message(Fetch_String(TXT_RECONNECT_HELP4));
	}

	Record_Message("");
	Record_Message(Fetch_String(TXT_RECONNECT_HELP5));
	Record_Message(Fetch_String(TXT_RECONNECT_HELP1));
	Record_Message("");
}


void UIReconnectPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_RECONNECT_KICK) {
		Propose_Kick(intent.Value);
		return;
	}

	if (intent.Action == UI_RECONNECT_CANCEL) {
		Cancelled = true;

		UIResult result;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
		Result = result;
		return;
	}
}


/// <summary>
/// Reads the session's seats into the view-model. Only the seats the game holds get a row,
/// which is what destroying the spare buttons stood for.
/// </summary>
void UIReconnectPresenterClass::Refresh(void)
{
	Players.clear();

	for (int index = 0; index < Session.Players.Count() && index < MAX_PLAYERS; index++) {
		PlayerRowType row;
		row.Name = Session.Players[index]->Name;
		Players.push_back(row);
	}

	PlayersChanged = true;
}


/// <summary>
/// Moves every seat's bar. The local seat is never behind, so its bar stays full; every
/// other seat's is the wait since that seat last reported in.
/// </summary>
void UIReconnectPresenterClass::Update_Bars(unsigned elapsed, unsigned const * timings, int count)
{
	for (int index = 0; index < (int)Players.size(); index++) {
		unsigned progress = 0;

		if (index != 0 && index < Session.Players.Count()) {
			// A seat whose connection has already gone away has no timing to read. The
			// dialog indexed the array with the -1 it got back for one.
			int const connection = Ipx.Connection_Index(Session.Players[index]->Player.ID);
			if (timings != NULL && connection >= 0 && connection < count) {
				progress = elapsed - timings[connection];
			}
		}

		Players[index].Lateness = progress > 480 ? 2 : (progress > 240 ? 1 : 0);
		Players[index].Remaining = std::max(100 - (int)(100 * progress / 1200), 0);
	}

	PlayersChanged = true;
}


void UIReconnectPresenterClass::Set_Time_Remaining(int seconds)
{
	char buffer[256];
	std::snprintf(buffer, sizeof(buffer), Fetch_String(TXT_TIME_ALLOWED), seconds);

	TimeText = buffer;
	TimeChanged = true;
}


void UIReconnectPresenterClass::Record_Message(char const * line)
{
	Messages.emplace_back(line != NULL ? line : "");
	if ((int)Messages.size() > MESSAGE_LIMIT) {
		Messages.erase(Messages.begin());
	}

	MessagesChanged = true;
}


/// <summary>
/// Proposes that a seat be kicked out of the game, telling every other player and casting
/// this machine's own vote.
/// </summary>
/// <param name="index">Index into the session's player list of the seat to be kicked.</param>
void UIReconnectPresenterClass::Propose_Kick(int index)
{
	if (index < 0 || index >= Session.Players.Count()) {
		return;
	}

	DebugString("Propose_Kick_Player %d - %s. Local id is %d\n", index, Session.Players[index]->Name,
		Session.Players[0]->Player.ID);

	if (index == 0) {
		Record_Message(Fetch_String(TXT_RECONNECT_KICK_SELF));
		return;
	}

	if (Session.Type == GAME_INTERNET && WestwoodOnline_Tournament) {
		Record_Message(Fetch_String(TXT_CANT_KICK));
		return;
	}

	int const kicker = Session.Players[0]->Player.ID;
	int const kickee = Session.Players[index]->Player.ID;
	if (!Kick_Vote_Is_Possible(kicker, kickee)) {
		return;
	}

	GlobalPacketType gpacket;
	NetGlobal::Initialize_Packet(gpacket, NET_PROPOSE_KICK);
	std::snprintf(gpacket.Name, sizeof(gpacket.Name), "%s", Session.Players[0]->Name);
	gpacket.Kick.KickerID = static_cast<unsigned int>(kicker);
	gpacket.Kick.KickeeID = static_cast<unsigned int>(kickee);

	for (int other = 1; other < Session.Players.Count(); other++) {
		DebugString("Sending kick proposal to %s\n", Session.Players[other]->Name);
		Ipx.Send_Global_Message(&gpacket, sizeof(gpacket), 1, &Session.Players[other]->Address);
	}

	Cast_Kick_Vote(kicker, kickee);
}


/*
**	The RmlUi view. One document: the template has no variants, and the seats it shows are
**	the ones the game holds rather than a fixed eight.
*/
namespace {

	// A seat as the document lays it out. The row's own position is carried here because the
	// template puts the eight seats in two columns of four rather than in a list.
	struct SeatViewType
	{
		std::string Name;
		std::string Left;
		std::string Top;
		std::string Width;
		std::string Hex;
	};


	class ReconnectViewClass : public UIRmlViewClass
	{
		public:
			ReconnectViewClass(UIReconnectPresenterClass & presenter)
				: UIRmlViewClass(presenter, "reconnect.rml"), Screen(presenter) {}

			virtual void Bind(Rml::DataModelConstructor & model) override;
			virtual void Sync(void) override;

		private:
			void Rebuild_Rows(void);

			UIReconnectPresenterClass & Screen;

			std::vector<SeatViewType> Seats;
	};


	void ReconnectViewClass::Rebuild_Rows(void)
	{
		Seats.clear();

		for (int index = 0; index < (int)Screen.Players.size(); index++) {
			UIReconnectPresenterClass::PlayerRowType const & row = Screen.Players[index];

			SeatViewType seat;
			seat.Name = row.Name;

			// The template's two columns of four, at 22 and 175 dialog units across and
			// every 18 units down from 12.
			char position[16];
			std::snprintf(position, sizeof(position), "%gdp", index < 4 ? 33.0 : 262.5);
			seat.Left = position;
			std::snprintf(position, sizeof(position), "%gdp", 19.5 + (index % 4) * 29.25);
			seat.Top = position;

			// The bar keeps a floor of six pixels of the group box it is drawn in, which is
			// ten percent of the box's sixty.
			std::snprintf(position, sizeof(position), "%d%%", std::max(row.Remaining, 10));
			seat.Width = position;

			seat.Hex = row.Lateness >= 2 ? "#c80000" : (row.Lateness == 1 ? "#c8c800" : "#00c800");

			Seats.push_back(seat);
		}
	}


	void ReconnectViewClass::Bind(Rml::DataModelConstructor & model)
	{
		Rebuild_Rows();

		if (auto seat = model.RegisterStruct<SeatViewType>()) {
			seat.RegisterMember("name", &SeatViewType::Name);
			seat.RegisterMember("left", &SeatViewType::Left);
			seat.RegisterMember("top", &SeatViewType::Top);
			seat.RegisterMember("width", &SeatViewType::Width);
			seat.RegisterMember("hex", &SeatViewType::Hex);
		}
		model.RegisterArray<std::vector<SeatViewType>>();

		model.Bind("seats", &Seats);
		model.Bind("messages", &Screen.Messages);
		model.Bind("timetext", &Screen.TimeText);

		model.BindEventCallback("kick",
			[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
				if (arguments.empty()) return;
				Screen.Queue(UIIntent{UI_RECONNECT_KICK, "", arguments[0].Get<int>()});
			});

		model.BindEventCallback("press",
			[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const &) {
				Screen.Queue(UIIntent{UI_RECONNECT_CANCEL, "", 0});
			});

		// Escape gives up on the stalled game, which is the IDCANCEL IsDialogMessage sent
		// the dialog whether or not the key reached its cancel button.
		model.BindEventCallback("key",
			[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
				int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
				if (key == Rml::Input::KI_ESCAPE) {
					Screen.Queue(UIIntent{UI_RECONNECT_CANCEL, "", 0});
				}
			});
	}


	void ReconnectViewClass::Sync(void)
	{
		if (!Model) return;

		Rebuild_Rows();

		Model.DirtyVariable("seats");
		Model.DirtyVariable("messages");
		Model.DirtyVariable("timetext");

		Screen.PlayersChanged = false;
		Screen.MessagesChanged = false;
		Screen.TimeChanged = false;
	}


	// The one reconnect screen. Wait_For_Players opens one when the game stalls and closes
	// it when the stall ends, and no caller opens a second while one is up.
	UIReconnectPresenterClass * _Presenter = NULL;
	ReconnectViewClass * _View = NULL;

}	// namespace


UIReconnectPresenterClass * UI_Reconnect_Screen(void)
{
	return(_Presenter);
}


bool UI_Reconnect_Open(bool reconnect, int const * frames, int connections)
{
	UI_Reconnect_Close();

	_Presenter = new UIReconnectPresenterClass;
	_Presenter->Open(reconnect, frames, connections);

	// The presentation is latched here, at screen entry. A document that will not prepare
	// drops the screen back to the legacy dialog, which runs against the same presenter.
	if (!UI_Use_Rml()) {
		return(false);
	}

	ReconnectViewClass * const view = new ReconnectViewClass(*_Presenter);

	// The wait loop stops servicing the map's input while this screen is up, so the screen
	// owns the input scope the way the dialog did.
	if (!view->Prepare(true)) {
		delete view;
		return(false);
	}

	_View = view;
	UI_Paint_Now(true);
	return(true);
}


/// <summary>
/// The pass the wait loop gives the screen: what its events queued is executed, the document
/// is brought up to the model, and the result is put on screen.
/// </summary>
void UI_Reconnect_Service(void)
{
	if (_View == NULL || _Presenter == NULL) {
		return;
	}

	_Presenter->Drain();
	_View->Sync();
	UI_Paint_Now(false);
}


void UI_Reconnect_Close(void)
{
	if (_View != NULL) {
		_View->Close();
		delete _View;
		_View = NULL;
	}

	delete _Presenter;
	_Presenter = NULL;
}


bool UI_Reconnect_Has_View(void)
{
	return(_View != NULL);
}
