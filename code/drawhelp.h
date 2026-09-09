/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "surface.h"
#include "win.h"

/*
 * Drawing and window helpers shared by the screens that draw into the game's own
 * surfaces. The OD_ and WS_ names are inherited from the owner-draw dialogs these
 * routines were first written for; nothing here has anything to do with a dialog.
 */

#define OD_TEXT_ALIGN_MIN 1
#define OD_TEXT_ALIGN_CENTER 2
#define OD_TEXT_ALIGN_MAX 3

/// Flags for OD_Draw_Text_Remap.
#define OD_DRAW_CHAR_FLAG_HORIZONTAL_CENTER 1
#define OD_DRAW_CHAR_ALIGN_FLAG_RIGHT 2
#define OD_DRAW_CHAR_FLAG_VERTICAL_CENTER 4

int OD_Draw_Text_Remap(Surface & surface, const char * string, Rect const & rect, char const * name, COLORREF color, int flags, int char_spacing);

Surface * OD_Fetch_Image(char const * name);
int OD_Draw_Text(COLORREF color, HFONT font, Rect const & rect, const char * text, int len, int x_alignment, int y_alignment, Surface * surface);

HFONT WS_Get_Font(HDC hdc, const char * face_name, int decipt_width, int decipt_height, int attributes);

BOOL Get_Display_Rect(HWND window, LPRECT rect);

void Prepare_Draw_Resources(void);

void Release_Pointer_To_Host(void);
void Recapture_Pointer(void);

extern COLORREF ODColorText;

extern unsigned short ODRComponentMask;
extern unsigned short ODGComponentMask;
extern unsigned short ODBComponentMask;


/// <summary>
/// Blends a color over a display pixel.
/// </summary>
/// <param name="alpha">How much of the color to mix in, from 0 to 255.</param>
inline unsigned short OD_Blend_Color(unsigned short pixel, unsigned short color, unsigned char alpha)
{
	unsigned blend_color_alpha = alpha;
	unsigned blend_pixel_alpha = 255 - alpha;

	unsigned short r = ((((pixel & ODRComponentMask) * blend_pixel_alpha) + ((color & ODRComponentMask) * blend_color_alpha)) >> 8) & ODRComponentMask;
	unsigned short g = ((((pixel & ODGComponentMask) * blend_pixel_alpha) + ((color & ODGComponentMask) * blend_color_alpha)) >> 8) & ODGComponentMask;
	unsigned short b = (((pixel & ODBComponentMask) * blend_pixel_alpha) + ((color & ODBComponentMask) * blend_color_alpha)) >> 8;
	return((unsigned short)(r | g | b));
}
