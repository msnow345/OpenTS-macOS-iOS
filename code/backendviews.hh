/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The bgfx views the frame and the overlays are drawn through. bgfx renders views in
// ascending order, so these numbers are the draw order: the magnify pass has to precede
// the present pass for the present to sample this frame's output rather than the last
// one's, and both have to precede the overlays for the overlays to land on top.
//
// bgfxbackend.cpp owns the first two and code/ui the last two. They share this header so
// that neither can renumber a view the other draws through.

#pragma once


enum BackendViewType {
	BACKEND_VIEW_PRESCALE = 0,
	BACKEND_VIEW_PRESENT = 1,
	BACKEND_VIEW_UI = 2,
	BACKEND_VIEW_DEV = 3,
};
