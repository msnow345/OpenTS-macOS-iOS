/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The progress and wait box, IDD_PROGRESS_WAIT. It stands over a job the caller drives, so
// it is not modal and the caller's own loop keeps running underneath it, the way the wait
// box does. Only plain values cross this header.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once


// Opens the box. A false return means nothing was shown, which is the caller's cue to open
// the legacy dialog instead. The box carries no bar until one is named.
bool UI_Progress_Wait_Open(void);

// Names the artwork the bar is drawn from, as ProgressScreenClass::Set_Graphic_Data names
// it. An unknown name leaves the box with no bar rather than failing.
void UI_Progress_Wait_Set_Bar(char const * shape);

// Moves the bar. The fraction is the part of the job that is finished, 0 to 1.
void UI_Progress_Wait_Set_Progress(double fraction);

void UI_Progress_Wait_Close(void);
bool UI_Progress_Wait_Is_Open(void);
