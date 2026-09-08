/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include <windows.h>

#include <SDL3/SDL.h>

// One window record per HWND the engine asks for. Only the main window is backed by an
// SDL window; everything the legacy dialog layer would have created is refused, so a
// record without a Handle never exists.
struct Win32Window
{
	SDL_Window * Handle;
	WNDPROC Procedure;
	char ClassName[64];
	char Title[128];
	DWORD Style;
	DWORD ExStyle;
	bool Visible;
	bool Enabled;
};

Win32Window * Win32_Lookup(HWND window);
HWND Win32_Main_Window(void);
WNDPROC Win32_Class_Procedure(char const * name);

// Drains the host's event queue into the engine's message queue. Every entry point that
// waits for a message calls this, so the engine keeps its own pump and its own frame pace.
void Win32_Pump_Host_Events(void);
void Win32_Post_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

// Converts between the host's coordinates and the physical client pixels the engine
// measures its frame in. The two agree on a display whose pixel density is one.
float Win32_Pixel_Density(void);

// The layer bgfx presents into, which SDL owns and this layer only hands over.
extern "C" void * Win32Compat_Native_Window_Handle(HWND window);
extern "C" int Win32Compat_Window_Refresh_Rate(HWND window);
