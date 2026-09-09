/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The message box and the wait box, the two screens every other screen can open. Only the
// result contract crosses this header, so a caller carries no toolkit and no window handle.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"


// Shows a message with up to three buttons and does not return until one is answered. The
// result's Value is the index of the button the player picked, counted the way
// WWMessageBox::Process counts them; GameEnded says the session ended underneath the box.
// OUTCOME_FAILED_TO_OPEN means nothing was shown, which is the caller's cue to open the
// legacy dialog instead.
UIResult UI_Message_Box_Screen(char const * message, int defresponse,
	char const * b1txt, char const * b2txt, char const * b3txt);


// Opens the box that stands over a long operation. It is not modal: the caller keeps
// running its own loop underneath and closes the box when the operation finishes.
//
// A caption for the cancel button shows it; cancelling raises the flag and feeds an escape
// key to the game keyboard, as the dialog's cancel button does. The flag is read while the
// box is open, so it must outlive it.
bool UI_Wait_Box_Open(char const * message, char const * cancelcaption, bool * cancelled);

// Replaces the text of the box that is already showing.
void UI_Wait_Box_Set_Text(char const * message);

void UI_Wait_Box_Close(void);
bool UI_Wait_Box_Is_Open(void);
