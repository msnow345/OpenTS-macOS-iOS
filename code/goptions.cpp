/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2025 Electronic Arts Inc.
 * Copyright 2026 OpenTS contributors
 *
 * Contains material derived from Electronic Arts source code.
 * Modified by OpenTS contributors, 2026.
 * EA's GPLv3 Section 7 additional terms and supplemental warranty
 * disclaimers apply; see LICENSE.md.
 ******************************************************************************/

/* $Header: /counterstrike/GOPTIONS.CPP 6     3/15/97 7:18p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : OPTIONS.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : June 8, 1994                                                 *
 *                                                                                             *
 *                  Last Update : July 27, 1995 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   OptionsClass::Process -- Handles all the options graphic interface.                       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include "goptions.h"

#include "_keyboar.h"
#include "_map.h"
#include "data.h"
#include "dbgprint.h"
#include "gamedlg.h"
#include "language/language.h"
#include "loaddlg.h"
#include "queue.h"
#include "restate.h"
#include "savemgr.h"
#include "scenario.h"
#include "stats.h"
#include "ui/uiabort.h"
#include "ui/uigameoptions.h"

#include "special.hh"



// What the driver does on the way out. The briefing is restated after the screen has gone,
// which is where the dialog driver restated it.
static void Game_Options_Finish(UIGameOptionsPresenterClass const & screen)
{
	Keyboard->Clear();

	if (screen.Choice == UIGameOptionsPresenterClass::CHOICE_BRIEFING) {
		Restate_Mission(Scen);
	}

	IgnoreInput = Scen->IsInputLocked;

	if (screen.Choice == UIGameOptionsPresenterClass::CHOICE_LOADED) {
		if (MouseCursor->Is_Hidden() == false && Scen->IsInputLocked == 1) {
			Hide_Mouse();
		} else if (MouseCursor->Is_Hidden() == true && Scen->IsInputLocked == 0) {
			Show_Mouse();
		}
	}

	Map.Flag_To_Redraw(GS_REDRAW_ALL);
}


int Network_Quality_Text_ID(NetTiming::ConnectionQuality quality)
{
	switch (quality) {
		case NetTiming::ConnectionQuality::Fast: return(TXT_BEST_CONNECTION);
		case NetTiming::ConnectionQuality::Normal: return(TXT_GOOD_CONNECTION);
		case NetTiming::ConnectionQuality::Poor: return(TXT_POOR_CONNECTION);
		case NetTiming::ConnectionQuality::Bad: return(TXT_WORST_CONNECTION);
	}
	return(TXT_WORST_CONNECTION);
}


/// <summary>
/// Displays the in game options dialog.
/// This routine is used by the special dialog handler when the player calls up the options
/// screen. Which layout appears depends on the kind of game in progress. Game input stays
/// locked out for as long as the screen is up, and if the player asked for the mission
/// briefing it is restated on the way out.
/// </summary>
void Game_Options_Dialog(void)
{
	UIGameOptionsPresenterClass screen;
	screen.Refresh();

	IgnoreInput = true;
	Keyboard->Clear();

	UI_Game_Options_Screen(screen);

	Game_Options_Finish(screen);
}


// Maps the screen's choice onto the value the special dialog handler expects. A screen that
// never opened answers zero, which is what the driver's own result was left at.
static int Abort_Choice_Result(UIAbortPresenterClass const & screen)
{
	switch (screen.Choice) {
		case UIAbortPresenterClass::CHOICE_QUIT:
			return(IDOK);

		case UIAbortPresenterClass::CHOICE_RESTART:
			return(IDABORT);

		case UIAbortPresenterClass::CHOICE_CANCEL:
			return(IDCANCEL);

		default:
			return(0);
	}
}


/// <summary>
/// Displays the abort mission dialog and waits for an answer.
/// This routine is used by the special dialog handler when the player asks to abandon or
/// surrender the mission. It does not return until the player has settled on one of the
/// choices offered.
/// </summary>
/// <returns>Returns with IDOK to quit the mission, IDABORT to restart or surrender it, or
/// IDCANCEL to carry on playing.</returns>
int Abort_Dialog(void)
{
	UIAbortPresenterClass screen;
	screen.Refresh();
	UI_Abort_Screen(screen);

	return(Abort_Choice_Result(screen));
}
