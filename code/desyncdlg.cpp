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

#include "desyncdlg.h"

#include "_map.h"
#include "_rect.h"
#include "_surface.h"
#include "_xmouse.h"
#include "chat.h"
#include "conquer.h"
#include "data.h"
#include "dbgprint.h"
#include "dsurface.h"
#include "globals.h"
#include "house.h"
#include "ipxmgr.h"
#include "language/language.h"
#include "loaddlg.h"
#include "misc.h"
#include "mpload.h"
#include "netdlg.h"
#include "netglobal.h"
#include "savemgr.h"
#include "session.h"
#include "ui/uishell.h"
#include "syncreport.h"
#include "win.h"
#include "winfix.h"


#include <algorithm>
#include <cstdio>
#include <cstring>



/// <summary>
/// Shows the screen and runs it until the master has decided, or this player has quit. Game
/// logic is halted for the duration; chat, sign-offs, heartbeats and the master's decision
/// still come through, since the network is serviced the whole time.
/// </summary>
DesyncDialogClass::OutcomeType DesyncDialogClass::Run(void)
{
	DebugString("Out-of-sync dialog opening on frame %d\n", Frame);

	// A raised suspension makes the runner's pump service the network instead of the game.
	TacticalActive = false;
	Session.Suspended++;

	IsRunning = true;
	Screen.Open();
	UI_Set_Desync_Screen(&Screen);

	OutcomeType outcome = OutcomeType::Continue;

	UI_Desync_Run(Screen);
	UI_Desync_Close_View();

	switch (Screen.Outcome) {
		case UIDesyncPresenterClass::OUTCOME_LOAD: outcome = OutcomeType::Load; break;
		case UIDesyncPresenterClass::OUTCOME_QUIT: outcome = OutcomeType::Quit; break;
		default:                                   outcome = OutcomeType::Continue; break;
	}

	UI_Set_Desync_Screen(NULL);
	IsRunning = false;

	Session.Suspended--;
	TacticalActive = true;
	Map.Flag_To_Redraw(GS_REDRAW_ALL);

	DebugString("Out-of-sync dialog closed with outcome %d\n", (int)outcome);
	return(outcome);
}


void DesyncDialogClass::Service(void)
{
	// The runner services the screen itself, so the network maintenance has nothing of its
	// own to do here while the screen is up.
}


void DesyncDialogClass::Notify_Chat(char const * name, char const * text)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Record_Chat(name, text);
}


void DesyncDialogClass::Notify_Player_Left(int house, char const * name)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Player_Left(house, name);
}


void DesyncDialogClass::Notify_Continue(void)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Master_Decided_To_Continue();
}


void DesyncDialogClass::Notify_Heartbeat(int house)
{
	if (Is_Active()) {
		Screen.Heartbeat_Heard(house);
	}
}


void DesyncDialogClass::Notify_Master_Changed(void)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Master_Changed();
}
