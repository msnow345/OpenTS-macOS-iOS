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

/* $Header: /CounterStrike/SOUNDDLG.CPP 1     3/03/97 10:25a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : SOUNDDLG.CPP                                                 *
 *                                                                                             *
 *                   Programmer : Maria del Mar McCready-Legg, Joe L. Bostic                   *
 *                                                                                             *
 *                   Start Date : Jan 8, 1995                                                  *
 *                                                                                             *
 *                  Last Update : September 22, 1995 [JLB]                                     *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   MusicListClass::Draw_Entry -- Draw the score line in a list box.                          *
 *   SoundControlsClass::Process -- Handles all the options graphic interface.                 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include "sounddlg.h"

#include "dbgprint.h"
#include "audio/audioengine.h"
#include "globals.h"
#include "goptions.h"
#include "incdec.h"
#include "init.h"
#include "language/language.h"
#include "theme.h"
#include "ui/uisound.h"
#include "winfix.h"


/// <summary>
/// Handles the sound and music options dialog.
/// This routine brings up the sound controls and runs them until the player dismisses
/// them. A cut down version of the screen is used when there is no game in progress, since
/// the in game options do not apply there.
/// </summary>
/// <remarks>This routine will not return until the player closes the dialog.</remarks>
void SoundControlsClass::Dialog(void)
{
	DebugString("SoundControls: GameSpeed = %d, ScrollRate = %d, Detail = %d\n", Options.GameSpeed, Options.ScrollRate, Options.DetailLevel);

	UISoundPresenterClass screen;
	screen.Refresh();
	UI_Sound_Screen(screen);

	DebugString("SoundControls: GameSpeed = %d, ScrollRate = %d, Detail = %d\n", Options.GameSpeed, Options.ScrollRate, Options.DetailLevel);
}

