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


MapPreviewSurfaceClass::MapPreviewSurfaceClass(int width, int height, MapPreviewClass * const * source) :
	UISurfaceBufferClass(width, height),
	Width(width),
	Height(height),
	Source(source != NULL ? source : &MultiplayerMapPreview)
{
	Set_Transparent_Color(DSurface::Build_Hicolor_Pixel(255, 0, 255));
	Clear();
}


void MapPreviewSurfaceClass::Redraw(void)
{
	Clear();

	if (*Source == NULL) {
		return;
	}

	XSurface * const picture = (*Source)->Get_Preview_Surface();
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

	// The picture is resampled here rather than blitted. Bit_Blit copies the smaller of the
	// two rectangles row for row, so an engine surface blit between rectangles of different
	// sizes crops the picture instead of scaling it; only DSurface's own blitter stretches,
	// and this buffer is not one.
	Surface & buffer = Get_Surface();
	for (int y = 0; y < destination.Height; y++) {
		int const sy = source.Y + (y * source.Height) / destination.Height;
		for (int x = 0; x < destination.Width; x++) {
			int const sx = source.X + (x * source.Width) / destination.Width;
			buffer.Put_Pixel(Point2D(destination.X + x, destination.Y + y),
				picture->Get_Pixel(Point2D(sx, sy)));
		}
	}

	Mark_Dirty();
}
