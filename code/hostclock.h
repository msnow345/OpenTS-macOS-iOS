/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

// The engine's coarse clock, in milliseconds from a clock that only ever moves forward.
// Every caller measures an interval with it and none depends on where it starts. The
// reading wraps roughly every forty nine days, so compare differences and not the
// readings themselves.
inline uint32_t Host_Milliseconds(void)
{
	return((uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
}


// Yields the processor for at least the requested number of milliseconds. A zero wait gives
// up the rest of the current time slice without pausing, which is what the frame loops use.
inline void Host_Sleep(unsigned int milliseconds)
{
	if (milliseconds == 0) {
		std::this_thread::yield();
		return;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
