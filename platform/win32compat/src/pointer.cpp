/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

// The engine reads the pointer two ways at once. Mouse messages carry a position that the
// keyboard queue records, and the scroll handler, the gadgets, the tooltip timer and the
// placement cursor poll GetCursorPos and GetAsyncKeyState every frame. Both read the state
// kept here, so the message queue and the poll cannot disagree, and a host with no mouse
// answers the poll as well as a host with one.

static float _PointerX;
static float _PointerY;
static SDL_MouseButtonFlags _PointerButtons;
static bool _PointerStarted;


void Win32_Pointer_Move(float x, float y)
{
	_PointerX = x;
	_PointerY = y;
}


void Win32_Pointer_Button(Uint8 button, bool down)
{
	SDL_MouseButtonFlags mask = 0;

	switch (button) {
		case SDL_BUTTON_LEFT: mask = SDL_BUTTON_LMASK; break;
		case SDL_BUTTON_RIGHT: mask = SDL_BUTTON_RMASK; break;
		case SDL_BUTTON_MIDDLE: mask = SDL_BUTTON_MMASK; break;
		default: return;
	}

	if (down) {
		_PointerButtons |= mask;
	} else {
		_PointerButtons &= ~mask;
	}
}


void Win32_Pointer_Position(float * x, float * y)
{
	if (x != NULL) *x = _PointerX;
	if (y != NULL) *y = _PointerY;
}


SDL_MouseButtonFlags Win32_Pointer_Buttons(void)
{
	return(_PointerButtons);
}


// Warping is what the tactical map's dragging scroll methods are built on: they pull the
// pointer back to the press point every frame so the map appears to travel under it. A host
// with no pointer of its own has nothing to pull, so it answers no and the game offers those
// methods to nobody.
bool Win32_Pointer_Can_Warp(void)
{
#ifdef OPENTS_IOS
	return(false);
#else
	return(true);
#endif
}


// A host reports motion only while its mouse is over a window it is delivering input to, and
// the engine polls a position whether it is or not, so the host is asked directly while it is
// not. The same call gives the pointer its opening position before the player has moved the
// mouse. The buttons are left to the events, which are the only place a press can arrive from.
void Win32_Pointer_Follow_Host_Mouse(void)
{
#ifndef OPENTS_IOS
	Win32Window * main = Win32_Lookup(Win32_Main_Window());

	if (main == NULL || main->Handle == NULL) {
		return;
	}

	bool const receiving = SDL_GetMouseFocus() == main->Handle && SDL_GetKeyboardFocus() == main->Handle;

	if (_PointerStarted && receiving) {
		return;
	}

	float x = 0.0f;
	float y = 0.0f;
	SDL_GetGlobalMouseState(&x, &y);

	int wx = 0;
	int wy = 0;
	SDL_GetWindowPosition(main->Handle, &wx, &wy);

	_PointerX = x - (float)wx;
	_PointerY = y - (float)wy;

	if (!_PointerStarted) {
		_PointerButtons = SDL_GetMouseState(NULL, NULL);
		_PointerStarted = true;
	}
#endif
}
