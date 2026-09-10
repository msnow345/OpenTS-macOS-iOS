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

#pragma once

#include "win.h"

class Surface;
class PaletteClass;
struct NativeWindow;

void Create_Main_Window ( HINSTANCE instance , int command_show , int width , int height);
NativeWindow Win_Native_Window(HWND window);
bool Win_Window_Drawable_Size(HWND window, int & width, int & height);
bool Win_Preferred_Frame_Size(int & width, int & height);
bool Win_Log_Directory(char * path, int size);
bool Win_Shipped_Data_Directory(char * path, int size);
int Win_Window_Refresh_Rate(HWND window);
bool Win_Set_Window_Fullscreen(HWND window, bool fullscreen);
bool Win_Pointer_Can_Warp(void);
bool Win_Pointer_Is_Drawn(void);
bool Win_Window_Safe_Area(HWND window, RECT & area);
bool Win_Pointer_Take_Scroll(int & x, int & y);
void Win_Set_Movie_Playing(bool playing);
void Set_Window_Fullscreen(bool fullscreen);

void Load_Title_Screen(char const * name, Surface * surface, PaletteClass * palette);

unsigned int Build_Number(void);
