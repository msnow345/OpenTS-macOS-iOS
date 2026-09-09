/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The reconnect and kick-vote screen, IDD_MPLAYER_DISCONNECT. It stands over a stalled
// multiplayer game while Wait_For_Players keeps servicing the network, so it has no loop of
// its own: the wait loop opens it, services it once a pass and closes it.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_RECONNECT_KICK = "kick";
inline constexpr char const * UI_RECONNECT_CANCEL = "cancel";


class UIReconnectPresenterClass : public UIPresenterClass
{
	public:
		// A seat and how far behind it is. The bar's remaining part and its color are
		// figures rather than pixels, because a presenter draws nothing.
		struct PlayerRowType
		{
			std::string Name;

			// How much of the seat's bar is left, 0 to 100, which is what Draw_Sync_Bars
			// scaled the group box's width by.
			int Remaining = 100;

			// 0 while the seat is keeping up, 1 once it is late, 2 once it is very late.
			int Lateness = 0;
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;

		// The state the screen opens with: the seats, the prose for the stall it stands
		// over, and the discarded kick proposals and vote tallies the dialog cleared on
		// creation.
		void Open(bool reconnect, int const * frames, int connections);

		// Moves every seat's bar. The timings are indexed by connection, as the wait loop
		// holds them, because that is how the dialog read them.
		void Update_Bars(unsigned elapsed, unsigned const * timings, int count);

		void Set_Time_Remaining(int seconds);

		// Records a line for whatever is showing the screen. The vote announcements arrive
		// here from the wait loop as well as from a button.
		void Record_Message(char const * line);

		// Puts a kick to the other players and casts this machine's own vote. The index is
		// into the session's player list, as the button that raised it was.
		void Propose_Kick(int index);

		/*
		**	The view-model.
		*/

		std::vector<PlayerRowType> Players;
		std::vector<std::string> Messages;

		// The most lines the model keeps, which is what ListBox_Trim capped the message
		// list box at.
		enum { MESSAGE_LIMIT = 50 };

		std::string TimeText;

		// Has the player given up on the stalled game? The wait loop reads this where it
		// read IDCANCEL out of the dialog's own result.
		bool Cancelled = false;

		bool PlayersChanged = false;
		bool MessagesChanged = false;
		bool TimeChanged = false;
};


// The screen the wait loop is running, or NULL when none is up. The vote tally reaches the
// model through this, because a vote is counted where no screen is in hand.
UIReconnectPresenterClass * UI_Reconnect_Screen(void);

// Opens the screen and shows its document. A false return means no document was shown and
// the caller opens the legacy dialog against the same presenter.
bool UI_Reconnect_Open(bool reconnect, int const * frames, int connections);

// The pass the wait loop gives the screen: the queued intents are executed and the document
// is brought up to the model and put on screen. Does nothing without a document.
void UI_Reconnect_Service(void);

void UI_Reconnect_Close(void);
bool UI_Reconnect_Has_View(void);
