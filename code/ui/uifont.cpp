/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Draws document text with the game's own dlgsys bitmap sheets.
//
// The dialogs never drew their buttons, captions, tabs, check boxes, combo boxes or track
// bar numbers with a scalable face. They drew from a pair of 640x160 sheets, dlgsysi.pcx
// and dlgsysa.pcx, laid out as 16 by 16 cells of 40 by 10 pixels: the first holds palette
// indices, the second the coverage of each pixel. drawhelp.cpp owns both the metrics, which
// are probed out of the ink rather than tabulated anywhere, and the palette shift that
// turns the sheet's own ramp into the colour text is asked for. This file only turns what
// drawhelp hands over into a texture and a quad per glyph.
//
// RmlUi installs one font engine, and the list boxes and tooltips legitimately want the
// shipped TrueType face, so this derives from RmlUi's own engine and answers only for the
// dlgsys family, delegating everything else to it untouched. A face handle of ours is
// recognised by the registry below; anything else is the base engine's and is passed
// straight through.
//
// A document selects the sheets with `font-family: dlgsys`. Its `font-size` is read as the
// height of a glyph cell, so `font-size: 10dp` draws one sheet pixel per authored pixel and
// the text scales with the frame exactly as the artwork around it does.

#include "always.h"

#include "uiinternal.h"

#include "dbgprint.h"
#include "drawhelp.h"
#include "utf8.h"

#include <RmlUi/Core/CallbackTexture.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FontMetrics.h>
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/Types.h>

#include <Core/FontEngineDefault/FontEngineInterfaceDefault.h>

#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>


// The family a document names to get the sheets, and the base name the sheets are stored
// under. drawhelp appends the "i" and "a" suffixes itself.
static char const _FontFamily[] = "dlgsys";


// One remapped copy of the glyph sheet. The sheet is drawn at its own resolution and scaled
// by the quads, so a colour needs one texture whatever size a document asks for.
struct UIFontSheet
{
	Rml::CallbackTextureSource Source;
	int Width = 0;
	int Height = 0;
};


// One size of the bitmap face. Everything here is derived from the sheet's own ink.
class UIFontFaceClass
{
	public:
		explicit UIFontFaceClass(int size, ODFontMetrics const & metrics);

		int Advance(unsigned char glyph) const { return(_Advance[glyph]); }
		int Width(char const * text, std::size_t length) const;

		Rml::FontMetrics const & Metrics(void) const { return(_Metrics); }
		int Size(void) const { return(_Size); }

		float Scale(void) const { return(_Scale); }
		int CellWidth(void) const { return(_Sheet.leftMargin + _Sheet.glyphWidth); }
		int CellHeight(void) const { return(_Sheet.topMargin + _Sheet.glyphHeight); }
		int TopMargin(void) const { return(_Sheet.topMargin); }

	private:
		ODFontMetrics _Sheet;
		Rml::FontMetrics _Metrics = {};
		int _Size = 0;
		float _Scale = 1.0f;
		int _Advance[256] = {};
};


UIFontFaceClass::UIFontFaceClass(int size, ODFontMetrics const & metrics) :
	_Sheet(metrics),
	_Size(size)
{
	int cell_height = _Sheet.topMargin + _Sheet.glyphHeight;
	if (cell_height <= 0) {
		cell_height = 1;
	}

	_Scale = (float)size / (float)cell_height;
	if (_Scale <= 0.0f) {
		_Scale = 1.0f;
	}

	for (int i = 0; i < 256; ++i) {
		_Advance[i] = (int)std::lround((double)_Sheet.charWidths[i] * _Scale);
	}

	// The sheets give inked extents, not typographic ones. The whole glyph sits above the
	// baseline with nothing below it, so a line box centres the ink on itself: RmlUi puts
	// the baseline half the leading below the top, which places the ink block exactly where
	// OD_DRAW_CHAR_FLAG_VERTICAL_CENTER placed it.
	_Metrics.size = size;
	_Metrics.ascent = (float)_Sheet.glyphHeight * _Scale;
	_Metrics.descent = 0.0f;
	_Metrics.line_spacing = (float)cell_height * _Scale;
	_Metrics.x_height = _Metrics.ascent * 0.5f;
	_Metrics.underline_position = 0.0f;
	_Metrics.underline_thickness = std::max(1.0f, _Scale);
	_Metrics.has_ellipsis = false;
}


int UIFontFaceClass::Width(char const * text, std::size_t length) const
{
	int width = 0;
	char const * cursor = text;
	char const * end = text + length;

	while (cursor < end) {
		char const * before = cursor;
		char32_t code = UTF8::Decode(cursor);
		if (cursor <= before) {
			break;
		}
		width += _Advance[OD_Font_Glyph(code)];
	}

	return(width);
}


static std::map<int, std::unique_ptr<UIFontFaceClass>> _Faces;
static std::map<unsigned int, UIFontSheet> _Sheets;
static bool _MetricsRead = false;
static bool _MetricsUsable = false;
static ODFontMetrics _SheetMetrics = {};
static int _Version = 1;


/// <summary>
/// Measures the sheets once and remembers whether they could be read at all.
/// The sheets live in a mix file, so the first document shown before the mixes are mounted
/// would find nothing; a later attempt is not made, and the family falls back to whatever
/// the document lists after it.
/// </summary>
static bool Sheet_Metrics(ODFontMetrics & metrics)
{
	if (!_MetricsRead) {
		_MetricsRead = true;
		_MetricsUsable = OD_Font_Metrics(_FontFamily, _SheetMetrics);

		if (_MetricsUsable) {
			DebugString("[UI] %s cells %dx%d, ink %dx%d, margins %d,%d.\n", _FontFamily,
				_SheetMetrics.leftMargin + _SheetMetrics.glyphWidth,
				_SheetMetrics.topMargin + _SheetMetrics.glyphHeight,
				_SheetMetrics.glyphWidth, _SheetMetrics.glyphHeight,
				_SheetMetrics.leftMargin, _SheetMetrics.topMargin);
		} else {
			DebugString("[UI] The %s font sheets could not be read.\n", _FontFamily);
		}
	}

	if (!_MetricsUsable) {
		return(false);
	}

	metrics = _SheetMetrics;
	return(true);
}


/// <summary>
/// Returns the glyph sheet remapped to one colour, building and uploading it on first use.
/// </summary>
static Rml::Texture Sheet_Texture(Rml::RenderManager & manager, unsigned int color, int & width, int & height)
{
	auto found = _Sheets.find(color);
	if (found == _Sheets.end()) {
		int sheet_width = 0;
		int sheet_height = 0;
		std::vector<unsigned char> pixels;

		if (!OD_Font_Sheet(_FontFamily, (COLORREF)color, sheet_width, sheet_height, pixels)) {
			return(Rml::Texture());
		}

		UIFontSheet sheet;
		sheet.Width = sheet_width;
		sheet.Height = sheet_height;
		sheet.Source = Rml::CallbackTextureSource(
			[pixels = std::move(pixels), sheet_width, sheet_height](Rml::CallbackTextureInterface const & texture) -> bool {
				return(texture.GenerateTexture(
					Rml::Span<Rml::byte const>(pixels.data(), pixels.size()),
					Rml::Vector2i(sheet_width, sheet_height)));
			});

		found = _Sheets.emplace(color, std::move(sheet)).first;
	}

	width = found->second.Width;
	height = found->second.Height;
	return(found->second.Source.GetTexture(manager));
}


/// <summary>
/// Recovers the colour a caller asked for from the premultiplied one RmlUi hands over.
/// </summary>
static unsigned int Unpremultiplied_Color(Rml::ColourbPremultiplied colour)
{
	int alpha = colour.alpha;
	int red = colour.red;
	int green = colour.green;
	int blue = colour.blue;

	if (alpha > 0 && alpha < 255) {
		red = std::min(255, red * 255 / alpha);
		green = std::min(255, green * 255 / alpha);
		blue = std::min(255, blue * 255 / alpha);
	}

	return((unsigned int)(red | (green << 8) | (blue << 16)));
}


// The bitmap engine answers for one family and hands everything else to RmlUi's own engine
// untouched, so a document that asks for the shipped TrueType face is unaffected.
class UIFontEngineClass : public Rml::FontEngineInterfaceDefault
{
	public:
		Rml::FontFaceHandle GetFontFaceHandle(Rml::String const & family, Rml::Style::FontStyle style,
			Rml::Style::FontWeight weight, int size) override;
		Rml::FontEffectsHandle PrepareFontEffects(Rml::FontFaceHandle handle, Rml::FontEffectList const & effects) override;
		Rml::FontMetrics const & GetFontMetrics(Rml::FontFaceHandle handle) override;
		int GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
			Rml::TextShapingContext const & shaping, Rml::Character prior) override;
		int GenerateString(Rml::RenderManager & manager, Rml::FontFaceHandle handle, Rml::FontEffectsHandle effects,
			Rml::StringView string, Rml::Vector2f position, Rml::ColourbPremultiplied colour, float opacity,
			Rml::TextShapingContext const & shaping, Rml::TexturedMeshList & mesh_list) override;
		int GetVersion(Rml::FontFaceHandle handle) override;
		void ReleaseFontResources(void) override;
};


static UIFontFaceClass * Bitmap_Face(Rml::FontFaceHandle handle)
{
	for (auto const & entry : _Faces) {
		if ((Rml::FontFaceHandle)entry.second.get() == handle) {
			return(entry.second.get());
		}
	}
	return(nullptr);
}


Rml::FontFaceHandle UIFontEngineClass::GetFontFaceHandle(Rml::String const & family, Rml::Style::FontStyle style,
	Rml::Style::FontWeight weight, int size)
{
	if (Rml::StringUtilities::ToLower(family) != _FontFamily) {
		return(Rml::FontEngineInterfaceDefault::GetFontFaceHandle(family, style, weight, size));
	}

	ODFontMetrics metrics;
	if (size <= 0 || !Sheet_Metrics(metrics)) {
		return(0);
	}

	auto found = _Faces.find(size);
	if (found == _Faces.end()) {
		found = _Faces.emplace(size, std::make_unique<UIFontFaceClass>(size, metrics)).first;
	}

	return((Rml::FontFaceHandle)found->second.get());
}


Rml::FontEffectsHandle UIFontEngineClass::PrepareFontEffects(Rml::FontFaceHandle handle, Rml::FontEffectList const & effects)
{
	if (Bitmap_Face(handle) != nullptr) {
		return(0);
	}
	return(Rml::FontEngineInterfaceDefault::PrepareFontEffects(handle, effects));
}


Rml::FontMetrics const & UIFontEngineClass::GetFontMetrics(Rml::FontFaceHandle handle)
{
	UIFontFaceClass const * face = Bitmap_Face(handle);
	if (face != nullptr) {
		return(face->Metrics());
	}
	return(Rml::FontEngineInterfaceDefault::GetFontMetrics(handle));
}


int UIFontEngineClass::GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
	Rml::TextShapingContext const & shaping, Rml::Character prior)
{
	UIFontFaceClass const * face = Bitmap_Face(handle);
	if (face == nullptr) {
		return(Rml::FontEngineInterfaceDefault::GetStringWidth(handle, string, shaping, prior));
	}

	int width = face->Width(string.begin(), string.size());
	width += (int)std::lround((double)shaping.letter_spacing) * (int)string.size();
	return(width);
}


int UIFontEngineClass::GenerateString(Rml::RenderManager & manager, Rml::FontFaceHandle handle,
	Rml::FontEffectsHandle effects, Rml::StringView string, Rml::Vector2f position,
	Rml::ColourbPremultiplied colour, float opacity, Rml::TextShapingContext const & shaping,
	Rml::TexturedMeshList & mesh_list)
{
	UIFontFaceClass const * face = Bitmap_Face(handle);
	if (face == nullptr) {
		return(Rml::FontEngineInterfaceDefault::GenerateString(manager, handle, effects, string, position,
			colour, opacity, shaping, mesh_list));
	}

	int sheet_width = 0;
	int sheet_height = 0;
	Rml::Texture texture = Sheet_Texture(manager, Unpremultiplied_Color(colour), sheet_width, sheet_height);
	if (!texture || sheet_width <= 0 || sheet_height <= 0) {
		return(0);
	}

	mesh_list.resize(1);
	mesh_list[0].texture = texture;
	Rml::Mesh & mesh = mesh_list[0].mesh;
	mesh.vertices.reserve(string.size() * 4);
	mesh.indices.reserve(string.size() * 6);

	float const scale = face->Scale();
	int const cell_width = face->CellWidth();
	int const cell_height = face->CellHeight();
	int const columns = (cell_width > 0) ? (sheet_width / cell_width) : 1;

	// The remapped sheet already carries the colour, so the quads only carry the opacity,
	// which is what RmlUi's own engine does for a colour glyph.
	Rml::ColourbPremultiplied const vertex_colour(colour.alpha, colour.alpha);

	Rml::Vector2f const dimensions((float)cell_width * scale, (float)cell_height * scale);
	float const top = position.y - face->Metrics().ascent - (float)face->TopMargin() * scale;
	int const spacing = (int)std::lround((double)shaping.letter_spacing);

	int line_width = 0;
	char const * cursor = string.begin();
	char const * end = string.end();

	while (cursor < end) {
		char const * before = cursor;
		char32_t code = UTF8::Decode(cursor);
		if (cursor <= before) {
			break;
		}

		unsigned char glyph = OD_Font_Glyph(code);
		if (glyph > ' ' && columns > 0) {
			// Cell zero is blank; the sheet stores the glyph for code n in cell n + 1.
			int cell = glyph + 1;
			float left = (float)((cell % columns) * cell_width);
			float upper = (float)((cell / columns) * cell_height);

			Rml::Vector2f const top_left(left / (float)sheet_width, upper / (float)sheet_height);
			Rml::Vector2f const bottom_right((left + cell_width) / (float)sheet_width,
				(upper + cell_height) / (float)sheet_height);

			// The blit started one pixel left of the pen, which is what puts the ink at the
			// pen once the cell's own left margin is crossed.
			Rml::Vector2f const origin(position.x + (float)line_width - scale, top);

			Rml::MeshUtilities::GenerateQuad(mesh, origin.Round(), dimensions, vertex_colour,
				top_left, bottom_right);
		}

		line_width += face->Advance(glyph) + spacing;
	}

	return(std::max(line_width, 0));
}


int UIFontEngineClass::GetVersion(Rml::FontFaceHandle handle)
{
	if (Bitmap_Face(handle) != nullptr) {
		return(_Version);
	}
	return(Rml::FontEngineInterfaceDefault::GetVersion(handle));
}


void UIFontEngineClass::ReleaseFontResources(void)
{
	_Sheets.clear();
	++_Version;
	Rml::FontEngineInterfaceDefault::ReleaseFontResources();
}


static UIFontEngineClass _FontEngine;


Rml::FontEngineInterface * UI_Font_Interface(void)
{
	return(&_FontEngine);
}


void UI_Font_Shutdown(void)
{
	_Sheets.clear();
	_Faces.clear();
	_MetricsRead = false;
	_MetricsUsable = false;
}
