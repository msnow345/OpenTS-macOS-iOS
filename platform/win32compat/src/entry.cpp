/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#ifdef OPENTS_IOS
// UIKit, not the C runtime, starts an iOS application. This header renames main to SDL_main
// and supplies the real entry point, which creates the UIApplication and its delegate and
// then calls back here. Without it the process has no application object, and so no view
// controller and no events.
#include <SDL3/SDL_main.h>
#endif

// Windows enters the game at WinMain. Nothing else does, so the host's entry point records
// the arguments the shell handed over and hands control to the same function the supported
// build starts in.

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE previous, char * commandline, int show);

void Win32_Record_Arguments(int argc, char ** argv);


int main(int argc, char ** argv)
{
	Win32_Record_Arguments(argc, argv);

#ifdef OPENTS_IOS
	// The recognizer needs the fingers themselves. A host that also turned them into mouse
	// events would put a click on the screen before anything knew what the gesture was, and
	// a stray click on the tactical map is an order.
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

	// The host reserves a band along the bottom of the screen for its own swipe, and the
	// sidebar reaches into it. This asks for the swipe to be handed to the game first and
	// only taken by the host on a second one. The bar itself stays on screen: the host
	// offers hiding it and deferring the swipe as alternatives, not together, and losing a
	// drag to the home screen costs more than a dim line does.
	SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
#endif

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		SDL_Log("SDL could not start: %s", SDL_GetError());
		return(1);
	}

	int const result = WinMain((HINSTANCE)(ULONG_PTR)1, NULL, NULL, 1);

	SDL_Quit();
	return(result);
}
