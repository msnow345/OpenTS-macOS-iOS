/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The out-of-sync screen's behavior, with no toolkit in it. The master's decision screen and
// the wait screen everyone else gets are one screen family sharing one model, because they
// differ by which controls exist rather than by what the screen does.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include "desync.h"

#include <cstdint>
#include <string>
#include <vector>


inline constexpr char const * UI_DESYNC_LOAD = "load";
inline constexpr char const * UI_DESYNC_CONTINUE = "continue";
inline constexpr char const * UI_DESYNC_QUIT = "quit";
inline constexpr char const * UI_DESYNC_SAY = "say";


class UIDesyncPresenterClass : public UIPresenterClass
{
	public:
		// What the screen answers its driver with. These stand where the dialog's own control
		// identifiers stood, so a presenter names no control.
		enum OutcomeType {
			OUTCOME_CONTINUE,
			OUTCOME_LOAD,
			OUTCOME_QUIT,
		};

		// A seat's standing, which the list drew as colored text in its own column.
		enum StatusType {
			STATUS_OK,
			STATUS_OUT_OF_SYNC,
			STATUS_LEFT,
		};

		// A seat and what is known about it. The name is carried rather than the house,
		// because a house the computer takes over is renamed and the list would lose it.
		struct PlayerRowType
		{
			std::string Name;
			StatusType Status = STATUS_OK;
			bool IsHost = false;
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// The multiplayer save browser draws where this screen is, so this screen is stepped
		// aside for it rather than run underneath. The family also steps aside when this
		// machine is promoted to master, because the runner picks its document once and the
		// promotion arrives from inside the service.
		virtual bool Suspends(void) const override
		{
			return(PromptPending || (Running >= 0 && (Running != 0) != IsMaster));
		}

		// Which variant the runner is holding a document open for, which the runner sets and
		// clears around itself: -1 for none, 0 for the wait screen, 1 for the master's.
		int Running = -1;

		// The rosters and the timers the screen opens with. Called once, before a view is
		// prepared, because the wait screen's quit delay is measured from here.
		void Open(void);

		// Runs the multiplayer save browser with this screen out of the way. Called by the
		// owner between passes, never from an event.
		void Run_Pending(void);

		// Records a line of chat for whatever is showing the screen, in the form the dialog
		// composed it.
		void Record_Chat(char const * name, char const * text);

		// A seat has gone. The name is kept because the roster entry is dropped with it.
		void Player_Left(int house, char const * name);

		void Master_Decided_To_Continue(void);
		void Heartbeat_Heard(int house);
		void Master_Changed(void);

		// Reads the session's seats into the view-model. The master marker is recorded with
		// the row rather than while painting it, because it is a fact about the seat.
		void Build_Player_Rows(void);

		/*
		**	The view-model.
		*/

		// Is this machine the master? The master decides and everyone else waits, which is
		// what the two templates stood for.
		bool IsMaster = false;

		std::vector<PlayerRowType> Players;

		// The chat backlog, kept whole here and wrapped by whatever shows it.
		std::vector<std::string> Messages;

		// The most lines the model keeps, which is what the chat list box was capped at.
		enum { MESSAGE_LIMIT = 50 };

		bool CanLoad = false;
		bool CanContinue = false;

		// A waiting player may not quit at once: the dialog left its quit button disabled
		// for the first ten seconds so a brief stall is not abandoned by reflex.
		bool CanQuit = false;

		bool CountdownActive = false;
		std::string CountdownText;

		// How much of the countdown is left, out of MultiplayerLoadClass::COUNTDOWN_MS, which
		// is what the shrinking bar was drawn from.
		int CountdownRemaining = 0;
		int CountdownTotal = 0;

		bool PlayersChanged = false;
		bool MessagesChanged = false;

		// Has the save browser been asked for? The owner runs it between passes.
		bool PromptPending = false;

		OutcomeType Outcome = OUTCOME_CONTINUE;

	private:
		void Answer(OutcomeType outcome);
		void Say(std::string const & text);
		void Append_Chat_Line(char const * line);
		void Send_Heartbeat(void);
		void Send_Continue(void);
		void Check_Timeouts(void);
		void Start_Countdown(void);
		void Update_Countdown(void);

		DesyncClass State;
		std::int64_t OpenedAt = 0;
		bool ContinueReceived = false;
		int LastCountdownSecond = -1;
};


// The out-of-sync screen the driver is running, or NULL when none is up. The network code
// reaches the model through this wherever a change is produced away from a screen.
UIDesyncPresenterClass * UI_Desync_Screen(void);
void UI_Set_Desync_Screen(UIDesyncPresenterClass * screen);


// Shows the variant the screen says it is, and runs it until the decision is made. The
// document is released when the screen closes, because the screen is not come back to.
UIResult UI_Desync_Run(UIDesyncPresenterClass & presenter);
void UI_Desync_Close_View(void);
