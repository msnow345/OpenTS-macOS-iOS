/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Pixels the engine draws at run time, shown inside a document by the <surface> element.
// A provider is registered under a name and a document names it: <surface src="name"/>.
// No RmlUi type appears here, so a screen's behavior half and the engine can own a provider
// without carrying the toolkit.
//
// docs/UI_DESIGN.md, "Assets and strings", owns the routing.

#pragma once

class Surface;


class UISurfaceProviderClass
{
	public:
		virtual ~UISurfaceProviderClass(void);

		virtual int Get_Width(void) const = 0;
		virtual int Get_Height(void) const = 0;

		// Writes Get_Width() by Get_Height() premultiplied RGBA8 pixels, top row first.
		virtual bool Read_Pixels(unsigned char * pixels) const = 0;

		// Says the pixels changed. The element uploads them again before it next draws them
		// and does nothing at all while this stands still, so a redraw costs no upload.
		void Mark_Dirty(void) { Generation++; }
		unsigned int Get_Generation(void) const { return(Generation); }

	private:
		unsigned int Generation = 1;
};


// A provider backed by an engine surface. A screen draws into it with the engine's ordinary
// drawing calls and marks it dirty; the conversion to what a texture wants happens here.
// Pixels matching the transparent color are written fully transparent, which is how the
// game's own artwork carries its mask.
class UISurfaceBufferClass : public UISurfaceProviderClass
{
	public:
		UISurfaceBufferClass(int width, int height);
		virtual ~UISurfaceBufferClass(void) override;

		Surface & Get_Surface(void) const { return(*Buffer); }

		void Set_Transparent_Color(int color) { Transparent = color; }

		// Fills the whole buffer with the transparent color and marks it dirty.
		void Clear(void);

		virtual int Get_Width(void) const override { return(Width); }
		virtual int Get_Height(void) const override { return(Height); }
		virtual bool Read_Pixels(unsigned char * pixels) const override;

	private:
		Surface * Buffer = nullptr;
		int Width = 0;
		int Height = 0;
		int Transparent = 0;
};


// Names a provider so a document can reach it. A name is unique among live providers, and a
// registration is dropped before its provider is destroyed.
void UI_Register_Surface(char const * name, UISurfaceProviderClass * provider);
void UI_Unregister_Surface(char const * name);
