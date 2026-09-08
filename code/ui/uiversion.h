/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The version information screen, reached from the main menu. Only the result contract
// crosses this header, so the caller carries no toolkit of any kind.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"


// Shows the version information and does not return until the player dismisses it. A
// OUTCOME_FAILED_TO_OPEN result means the documents could not be prepared and nothing was
// shown, which is the caller's cue to open the legacy dialog instead.
UIResult UI_Version_Screen(void);
