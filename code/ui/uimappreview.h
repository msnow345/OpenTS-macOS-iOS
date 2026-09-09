/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The multiplayer map preview as a surface a document can show. The map selection screen
// and the skirmish setup screen both hold one, at the size their own template's preview
// frame gives it.
//
// docs/UI_DESIGN.md, "Assets and strings", owns the routing.

#pragma once

#include "uisurface.h"


class MapPreviewSurfaceClass : public UISurfaceBufferClass
{
	public:
		// The extents are the interior of the template's preview frame, in game logical
		// units, because a provider's pixels are game logical units.
		MapPreviewSurfaceClass(int width, int height);

		// Draws the session's current preview, scaled and centered the way
		// MapPreviewClass::Blit_Preview scales it into a dialog's group box. The letterbox
		// around a picture of a different shape is left transparent rather than black.
		void Redraw(void);

	private:
		int Width;
		int Height;
};
