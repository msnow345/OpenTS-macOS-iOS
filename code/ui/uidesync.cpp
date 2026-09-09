/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "hostclock.h"
#include "always.h"

#include "uidesync.h"

#include "uirmlview.h"
#include "uishell.h"

#include "chat.h"
#include "dbgprint.h"
#include "house.h"
#include "ipxmgr.h"
#include "conquer.h"
#include "data.h"
#include "language/language.h"
#include "loaddlg.h"
#include "mpload.h"
#include "msglist.h"
#include "netdlg.h"
#include "netglobal.h"
#include "savemgr.h"
#include "session.h"
#include "syncreport.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstdio>


static UIDesyncPresenterClass * _DesyncScreen = NULL;


UIDesyncPresenterClass * UI_Desync_Screen(void)
{
	return(_DesyncScreen);
}


void UI_Set_Desync_Screen(UIDesyncPresenterClass * screen)
{
	_DesyncScreen = screen;
}


/// <summary>
/// Records the seats, the timers and the standing the screen opens with.
/// </summary>
void UIDesyncPresenterClass::Open(void)
{
	IsMaster = Session.Am_I_Master();
	OpenedAt = Monotonic_Milliseconds();
	State.Begin(OpenedAt);

	ContinueReceived = false;
	CountdownActive = false;
	LastCountdownSecond = -1;
	PromptPending = false;
	Outcome = OUTCOME_CONTINUE;
	Messages.clear();

	// The master decides and everyone else waits, so only the master's screen carries the
	// two decisions; the wait screen's quit comes back after the delay.
	CanContinue = IsMaster;
	CanLoad = IsMaster && SaveManager.Multiplayer_Load_Is_Allowed() && MultiplayerLoadOptionsClass().Files_Present();
	CanQuit = IsMaster;

	Build_Player_Rows();
}


void UIDesyncPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_DESYNC_LOAD) {
		if (!CanLoad || CountdownActive) return;
		PromptPending = true;
		return;
	}

	if (intent.Action == UI_DESYNC_CONTINUE) {
		if (!CanContinue || CountdownActive) return;
		Send_Continue();
		Answer(OUTCOME_CONTINUE);
		return;
	}

	if (intent.Action == UI_DESYNC_QUIT) {
		if (!CanQuit) return;
		Answer(OUTCOME_QUIT);
		return;
	}

	if (intent.Action == UI_DESYNC_SAY) {
		Say(intent.Identity);
		return;
	}
}


void UIDesyncPresenterClass::Refresh(void)
{
	Build_Player_Rows();
}


/// <summary>
/// The maintenance the dialog's own loop ran on every pass: heartbeats out, silent seats
/// dropped, the master's decision taken, and the countdown moved.
/// </summary>
void UIDesyncPresenterClass::Service(void)
{
	std::int64_t const now = Monotonic_Milliseconds();

	if (State.Heartbeat_Is_Due(now)) {
		Send_Heartbeat();
		State.Heartbeat_Sent(now);
	}
	Check_Timeouts();

	// A waiting player's quit comes back once the stall has lasted long enough to be worth
	// abandoning, which is what the disabled button stood for.
	if (!IsMaster && !CanQuit && now - OpenedAt >= DesyncClass::QUIT_DELAY_MS) {
		CanQuit = true;
	}

	if (!CountdownActive && SaveManager.MultiplayerLoad.Is_Pending()) {
		Start_Countdown();
	}

	if (CountdownActive) {
		Update_Countdown();
		if (SaveManager.MultiplayerLoad.Is_Due(now)) {
			Answer(OUTCOME_LOAD);
		}
		return;
	}

	if (ContinueReceived) {
		Answer(OUTCOME_CONTINUE);
	}
}


/// <summary>
/// Runs the multiplayer save browser with this screen out of the way, the way the dialog
/// disabled itself around the same prompt.
/// </summary>
void UIDesyncPresenterClass::Run_Pending(void)
{
	if (!PromptPending) {
		return;
	}

	PromptPending = false;
	SaveManager.Multiplayer_Load_Prompt();
}


void UIDesyncPresenterClass::Record_Chat(char const * name, char const * text)
{
	char buffer[MAX_MESSAGE_LENGTH + MAX_MESSAGE_PREFIX];
	std::snprintf(buffer, sizeof(buffer), "%s: %s", name, text);
	Append_Chat_Line(buffer);
}


void UIDesyncPresenterClass::Player_Left(int house, char const * name)
{
	State.Mark_Left(house, name);

	if (name != NULL && name[0] != '\0') {
		char buffer[128];
		std::snprintf(buffer, sizeof(buffer), Fetch_String(TXT_LEFT_GAME), name);
		Append_Chat_Line(buffer);
	}

	Build_Player_Rows();
	Master_Changed();
}


void UIDesyncPresenterClass::Master_Decided_To_Continue(void)
{
	DebugString("The master chose to continue without the players out of sync\n");
	ContinueReceived = true;
}


void UIDesyncPresenterClass::Heartbeat_Heard(int house)
{
	State.Heard(house, Monotonic_Milliseconds());
}


/// <summary>
/// Takes up the master's decisions once this machine has become master, unless a load is
/// already counting down, when there is nothing left to decide.
/// </summary>
void UIDesyncPresenterClass::Master_Changed(void)
{
	Build_Player_Rows();

	if (IsMaster || CountdownActive || !Session.Am_I_Master()) {
		return;
	}

	DebugString("This machine is the new master; it makes the decision now\n");
	IsMaster = true;
	CanContinue = true;
	CanQuit = true;
	CanLoad = SaveManager.Multiplayer_Load_Is_Allowed() && MultiplayerLoadOptionsClass().Files_Present();
}


void UIDesyncPresenterClass::Build_Player_Rows(void)
{
	Players.clear();

	int const master = Session.Master_Player_ID();

	for (int house = 0; house < MAX_PLAYERS && house < Houses.Count(); house++) {
		HouseClass const * housep = Houses[house];
		bool const left = State.Has_Left(house);

		// A player who left stays listed, though their seat is no longer human.
		if (housep == NULL || (!housep->IsHuman && !left)) {
			continue;
		}

		PlayerRowType row;

		// The roster entry is gone by now, so the kept name is the only copy while the list
		// rebuilds.
		row.Name = left && State.Left_Name(house)[0] != '\0' ? State.Left_Name(house) : housep->IniName.c_str();
		row.IsHost = house == master;

		if (left) {
			row.Status = STATUS_LEFT;
		} else if (Sync_Is_Out_Of_Sync(house)) {
			row.Status = STATUS_OUT_OF_SYNC;
		}

		Players.push_back(row);
	}

	PlayersChanged = true;
}


void UIDesyncPresenterClass::Answer(OutcomeType outcome)
{
	Outcome = outcome;

	// A screen answers its driver with a result as well as an outcome, because the runner
	// returns on a result.
	UIResult result;
	result.Outcome = outcome == OUTCOME_QUIT ? UIResult::OUTCOME_CANCELLED : UIResult::OUTCOME_ACCEPTED;
	result.Value = (int)outcome;
	Result = result;
}


void UIDesyncPresenterClass::Say(std::string const & text)
{
	if (text.empty()) {
		return;
	}

	char buffer[MAX_MESSAGE_LENGTH];
	std::snprintf(buffer, sizeof(buffer), "%s", text.c_str());

	Session.MessageScope = ChatScopeType::Everyone;
	Session.MessageAddress = IPXAddressClass();
	Chat_Send(buffer);
}


void UIDesyncPresenterClass::Append_Chat_Line(char const * line)
{
	Messages.emplace_back(line);
	if ((int)Messages.size() > MESSAGE_LIMIT) {
		Messages.erase(Messages.begin());
	}
	MessagesChanged = true;
}


void UIDesyncPresenterClass::Send_Heartbeat(void)
{
	if (PlayerPtr == NULL || Session.Players.Count() == 0) {
		return;
	}

	GlobalPacketType packet;
	NetGlobal::Initialize_Packet(packet, NET_DESYNC_HEARTBEAT);
	std::snprintf(packet.Name, sizeof(packet.Name), "%s", Session.Players[0]->Name);

	for (int index = 1; index < Session.Players.Count(); index++) {
		Ipx.Send_Global_Message(&packet, sizeof(packet), 0, &Session.Players[index]->Address);
	}
	Ipx.Service();
}


void UIDesyncPresenterClass::Send_Continue(void)
{
	DebugString("Telling every seat to continue without the players out of sync\n");

	GlobalPacketType packet;
	NetGlobal::Initialize_Packet(packet, NET_DESYNC_CONTINUE);
	std::snprintf(packet.Name, sizeof(packet.Name), "%s", Session.Players[0]->Name);

	for (int index = 1; index < Session.Players.Count(); index++) {
		Ipx.Send_Global_Message(&packet, sizeof(packet), 1, &Session.Players[index]->Address);
		Ipx.Service();
	}
}


/// <summary>
/// Drops the seats that have fallen silent, so a machine that died without a sign-off
/// neither holds up the decision nor lingers in the seats a later load reconciles.
/// </summary>
void UIDesyncPresenterClass::Check_Timeouts(void)
{
	std::int64_t const now = Monotonic_Milliseconds();

	for (int index = Session.Players.Count() - 1; index >= 1; index--) {
		int const house = Session.Players[index]->Player.ID;
		if (!State.Is_Silent(house, now)) {
			continue;
		}

		DebugString("No heartbeat from %s (house %d) for %d seconds; dropping the seat\n",
			Session.Players[index]->Name, house, (int)(DesyncClass::HEARTBEAT_TIMEOUT_MS / 1000));

		std::string const name = Session.Players[index]->Name;
		Destroy_Connection(house, 1);
		Player_Left(house, name.c_str());
	}
}


void UIDesyncPresenterClass::Start_Countdown(void)
{
	DebugString("Counting down to the multiplayer load\n");

	CountdownActive = true;
	LastCountdownSecond = -1;
	CountdownTotal = (int)MultiplayerLoadClass::COUNTDOWN_MS;

	Append_Chat_Line(Fetch_String(TXT_LOADING_SAVED_GAME));

	// Nothing is left to decide once the load is scheduled, which is what disabling both
	// buttons stood for.
	CanLoad = false;
	CanContinue = false;

	Update_Countdown();
}


void UIDesyncPresenterClass::Update_Countdown(void)
{
	if (!CountdownActive || !SaveManager.MultiplayerLoad.Is_Pending()) {
		return;
	}

	std::int64_t const now = Monotonic_Milliseconds();
	CountdownRemaining = std::clamp((int)SaveManager.MultiplayerLoad.Milliseconds_Left(now), 0, CountdownTotal);

	int const seconds = SaveManager.MultiplayerLoad.Seconds_Left(now);
	if (seconds == LastCountdownSecond) {
		return;
	}
	LastCountdownSecond = seconds;

	char buffer[128];
	std::snprintf(buffer, sizeof(buffer),
		Fetch_String(seconds == 1 ? TXT_LOADING_IN_SECOND : TXT_LOADING_IN_SECONDS), seconds);
	CountdownText = buffer;
}


/*
**	The RmlUi view. The master's screen and the wait screen are the same document family:
**	they differ by which of the three buttons exist and by the block of prose beside the
**	list, so each carries its own document and its own model name.
*/
namespace {

	// A seat as the document shows it, with the status text and its color resolved here
	// rather than in the presenter.
	struct PlayerViewType
	{
		std::string Name;
		std::string Status;
		std::string Hex;
		std::string Mark;
	};


	class DesyncViewClass : public UIRmlViewClass
	{
		public:
			DesyncViewClass(UIDesyncPresenterClass & presenter, char const * document)
				: UIRmlViewClass(presenter, document), Screen(presenter) {}

			virtual void Bind(Rml::DataModelConstructor & model) override;
			virtual void Sync(void) override;

		private:
			void Rebuild_Rows(void);

			UIDesyncPresenterClass & Screen;

			std::vector<PlayerViewType> PlayerRows;

			// How much of the countdown bar is left, as a percentage, because a document
			// states a width rather than drawing a rectangle, and the color the elapsed
			// time gives it.
			std::string BarWidth = "0%";
			std::string BarHex = "#00c800";
	};


	void DesyncViewClass::Rebuild_Rows(void)
	{
		PlayerRows.clear();

		for (UIDesyncPresenterClass::PlayerRowType const & row : Screen.Players) {
			PlayerViewType view;
			view.Name = row.Name;

			switch (row.Status) {
				case UIDesyncPresenterClass::STATUS_LEFT:
					view.Status = Fetch_String(TXT_SYNC_STATUS_LEFT);
					view.Hex = "#c80000";
					break;

				case UIDesyncPresenterClass::STATUS_OUT_OF_SYNC:
					view.Status = Fetch_String(TXT_SYNC_STATUS_OUT);
					view.Hex = "#c8c800";
					break;

				default:
					view.Status = Fetch_String(TXT_OK);
					view.Hex = "#00c800";
					break;
			}

			// The master marker, which the list drew as the wolhost.pcx surface. PCX
			// decoding is not here yet, so the marker is a character in the same column.
			view.Mark = row.IsHost ? "*" : "";

			PlayerRows.push_back(view);
		}

		int const total = Screen.CountdownTotal > 0 ? Screen.CountdownTotal : 1;
		int const remaining = std::clamp(Screen.CountdownRemaining, 0, total);
		char percent[16];
		std::snprintf(percent, sizeof(percent), "%d%%", remaining * 100 / total);
		BarWidth = percent;

		// Green to yellow to red as the load nears, which is what Draw_Countdown_Bar chose
		// from the elapsed fraction.
		int const elapsed = total - remaining;
		BarHex = "#00c800";
		if (elapsed > total * 2 / 5) {
			BarHex = elapsed > total * 4 / 5 ? "#c80000" : "#c8c800";
		}
	}


	void DesyncViewClass::Bind(Rml::DataModelConstructor & model)
	{
		Rebuild_Rows();

		if (auto row = model.RegisterStruct<PlayerViewType>()) {
			row.RegisterMember("name", &PlayerViewType::Name);
			row.RegisterMember("status", &PlayerViewType::Status);
			row.RegisterMember("hex", &PlayerViewType::Hex);
			row.RegisterMember("mark", &PlayerViewType::Mark);
		}
		model.RegisterArray<std::vector<PlayerViewType>>();
		model.RegisterArray<std::vector<std::string>>();

		model.Bind("players", &PlayerRows);
		model.Bind("messages", &Screen.Messages);

		model.Bind("canload", &Screen.CanLoad);
		model.Bind("cancontinue", &Screen.CanContinue);
		model.Bind("canquit", &Screen.CanQuit);

		model.Bind("countdown", &Screen.CountdownActive);
		model.Bind("countdowntext", &Screen.CountdownText);
		model.Bind("barwidth", &BarWidth);
		model.Bind("barhex", &BarHex);

		model.BindEventCallback("press",
			[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
				if (arguments.empty()) return;

				Rml::String const action = arguments[0].Get<Rml::String>();
				if (action == UI_DESYNC_LOAD) Screen.Queue(UIIntent{UI_DESYNC_LOAD, "", 0});
				else if (action == UI_DESYNC_CONTINUE) Screen.Queue(UIIntent{UI_DESYNC_CONTINUE, "", 0});
				else if (action == UI_DESYNC_QUIT) Screen.Queue(UIIntent{UI_DESYNC_QUIT, "", 0});
			});

		// Enter in the chat field sends the line, which is what the dialog's IDOK arm did,
		// since it had no default button.
		model.BindEventCallback("submit",
			[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
				int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
				if (key != Rml::Input::KI_RETURN && key != Rml::Input::KI_NUMPADENTER) {
					return;
				}

				Rml::Element * const field = Element != nullptr ? Element->GetElementById("say") : nullptr;
				if (field == nullptr) return;

				Rml::String const text = field->GetAttribute<Rml::String>("value", Rml::String());
				field->SetAttribute("value", Rml::String());
				Screen.Queue(UIIntent{UI_DESYNC_SAY, text, 0});
				event.StopPropagation();
			});
	}


	void DesyncViewClass::Sync(void)
	{
		if (!Model) return;

		Rebuild_Rows();

		Model.DirtyVariable("players");
		Model.DirtyVariable("messages");
		Model.DirtyVariable("canload");
		Model.DirtyVariable("cancontinue");
		Model.DirtyVariable("canquit");
		Model.DirtyVariable("countdown");
		Model.DirtyVariable("countdowntext");
		Model.DirtyVariable("barwidth");
		Model.DirtyVariable("barhex");

		Screen.PlayersChanged = false;
		Screen.MessagesChanged = false;
	}


	DesyncViewClass * _View = NULL;

}	// namespace


void UI_Desync_Close_View(void)
{
	delete _View;
	_View = NULL;
}


/// <summary>
/// Shows the variant this machine gets and runs it until the decision is made.
/// </summary>
UIResult UI_Desync_Run(UIDesyncPresenterClass & presenter)
{
	UIResult failed;
	failed.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;

	// The master's screen replaces the wait screen when this machine is promoted, so the
	// document is released rather than kept when the variant moves.
	static bool shown_as_master = false;
	if (_View != NULL && shown_as_master != presenter.IsMaster) {
		UI_Desync_Close_View();
	}

	if (_View == NULL) {
		shown_as_master = presenter.IsMaster;

		DesyncViewClass * const view = new DesyncViewClass(presenter,
			presenter.IsMaster ? "desynchost.rml" : "desyncwait.rml");
		if (!view->Prepare(true)) {
			delete view;
			return(failed);
		}

		_View = view;
	}

	// A family reopened in a loop resets the close mark and the held result, since a close
	// marks the presenter closing and a marked presenter drains nothing.
	presenter.Result.reset();
	presenter.IsClosing = false;
	presenter.Running = presenter.IsMaster ? 1 : 0;
	_View->Sync();

	while (!presenter.Result.has_value()) {
		UI_Run_Modal(presenter, *_View);

		if (!presenter.PromptPending) {
			break;
		}

		// The save browser draws where this screen is, so the document steps aside for it.
		_View->Hide();
		presenter.Run_Pending();
		_View->Show();
		_View->Sync();
	}

	presenter.Running = -1;
	return(presenter.Result.value_or(UIResult{}));
}
