/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

bool Change_Display_Mode(int width, int height);
void Main_Options_Dialog(void);

// The display options and the mode trial the family reaches through them. The loop stays
// with the driver, because only a view knows how to bring the screen back up after a mode
// the player refused.
void Display_Options_Dialog(void);
