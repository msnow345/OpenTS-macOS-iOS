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

// The one bitmap a native build creates is the canvas the game draws a mouse cursor onto,
// so a bitmap object carries only what a cursor is built from.
struct Win32Bitmap
{
	int Width;
	int Height;
	int BitCount;
	int Pitch;
	bool TopDown;
	unsigned char * Bits;
};

Win32Bitmap * Win32_Lookup_Bitmap(HBITMAP bitmap);

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

// The one pointer this layer owns. A position is in the main window's client area, in the
// host's own coordinates; the buttons are an SDL button mask. A host mouse writes it, and a
// host without one leaves the writing to whatever stands in for a mouse there. Everything
// the engine polls or receives about the pointer is answered from it, so the message queue
// and the polled state cannot disagree.
void Win32_Pointer_Move(float x, float y);
void Win32_Pointer_Button(Uint8 button, bool down);
void Win32_Pointer_Position(float * x, float * y);
SDL_MouseButtonFlags Win32_Pointer_Buttons(void);
void Win32_Pointer_Follow_Host_Mouse(void);

// Whether the host has a pointer this layer can move. A warp is what the tactical map's
// dragging scroll methods are built on, so a host that answers no cannot offer them.
bool Win32_Pointer_Can_Warp(void);

// Posts a message carrying the pointer's current position and buttons, and a key message
// the same shape the host's own keys arrive in.
void Win32_Post_Pointer_Message(UINT message);
void Win32_Post_Key_Message(int virtualkey, bool down);

// The touch recognizer. It writes the pointer above, so everything the engine already reads
// about the pointer answers for a finger as well. Handle_Event takes the finger events out
// of the host's queue; Service runs once per pump, because a finger that has stopped moving
// sends nothing at all; Cancel abandons whatever is in flight and is what a suspension or a
// lost window calls.
bool Win32_Touch_Handle_Event(SDL_Event const & event);
void Win32_Touch_Service(void);
void Win32_Touch_Cancel(void);
void Win32_Touch_Set_Movie_Mode(bool playing);

// The offset the tactical view has still to travel, in the window's own pixels, taken whole
// so the remainder is never lost between polls.
bool Win32_Touch_Take_Scroll(int * x, int * y);

// The layer bgfx presents into, which SDL owns and this layer only hands over.
extern "C" void * Win32Compat_Native_Window_Handle(HWND window);
extern "C" int Win32Compat_Window_Refresh_Rate(HWND window);
