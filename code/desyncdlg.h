/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "ui/uidesync.h"
#include "win.h"

#include <cstdint>
#include <string>
#include <vector>

/*
 * The dialog shown when a network game goes out of sync. The master chooses to load a saved
 * game, to continue without the players out of sync, or to quit; everyone else waits. Both
 * variants list the players with their state and carry a chat box. Game logic is halted while
 * it is up, and the network is kept alive with heartbeats.
 */
class DesyncDialogClass
{
	public:
		enum class OutcomeType {
			Continue,
			Load,
			Quit,
		};

		// Blocks until a decision has been made; the network is serviced throughout.
		OutcomeType Run(void);

		bool Is_Active(void) const {return(IsRunning);}

		// Sends the heartbeat and drops silent players; called from the network maintenance
		// so that both outlive a nested dialog's message loop.
		void Service(void);

		// Every notification is a no-op while the dialog is not open.
		void Notify_Chat(char const * name, char const * text);
		void Notify_Player_Left(int house, char const * name);
		void Notify_Continue(void);
		void Notify_Heartbeat(int house);
		void Notify_Master_Changed(void);

	private:
		OutcomeType Run_Legacy(void);
		void Create_Dialog(void);
		void Destroy_Dialog(void);
		void Fit_To_Screen(void);
		void Become_Host_If_Promoted(void);
		void Update_Player_List(void);
		void Refill_Chat_List(void);
		void Send_Chat(void);
		void On_Chat_Edit_Focus(bool gained);
		void Update_Countdown(void);
		void Draw_Countdown_Bar(HWND window);
		static INT_PTR CALLBACK Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

		// The screen's whole behavior. The dialog reads this model onto its controls and
		// queues an intent from a control; it decides nothing itself.
		UIDesyncPresenterClass Screen;

		HWND Window = NULL;
		bool IsRunning = false;
		bool IsHostDialog = false;
		bool ChatPlaceholderActive = false;
		bool CountdownShown = false;
		std::size_t DrawnMessages = 0;
};

extern DesyncDialogClass DesyncDialog;
