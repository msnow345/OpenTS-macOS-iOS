/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "uimappreview.h"

#include "dsurface.h"
#include "netshare.h"
#include "preview.h"
#include "xsurface.h"

#include <algorithm>


MapPreviewSurfaceClass::MapPreviewSurfaceClass(int width, int height) :
	UISurfaceBufferClass(width, height),
	Width(width),
	Height(height)
{
	Set_Transparent_Color(DSurface::Build_Hicolor_Pixel(255, 0, 255));
	Clear();
}


void MapPreviewSurfaceClass::Redraw(void)
{
	Clear();

	if (MultiplayerMapPreview == NULL) {
		return;
	}

	XSurface * const picture = MultiplayerMapPreview->Get_Preview_Surface();
	if (picture == NULL) {
		return;
	}

	Rect const source = picture->Get_Rect();
	if (source.Width <= 0 || source.Height <= 0) {
		return;
	}

	int const scale = std::min(1000 * Width / source.Width, 1000 * Height / source.Height);

	Rect destination;
	destination.Width = (scale * source.Width) / 1000;
	destination.Height = (scale * source.Height) / 1000;
	destination.X = Width / 2 - destination.Width / 2;
	destination.Y = Height / 2 - destination.Height / 2;

	Get_Surface().Blit_From(destination, *picture, source, false, false);
	Mark_Dirty();
}
