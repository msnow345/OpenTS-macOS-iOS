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

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		SDL_Log("SDL could not start: %s", SDL_GetError());
		return(1);
	}

	int const result = WinMain((HINSTANCE)(ULONG_PTR)1, NULL, NULL, 1);

	SDL_Quit();
	return(result);
}
