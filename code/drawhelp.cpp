/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "drawhelp.h"

#include "_surface.h"
#include "_xmouse.h"
#include "arraylist.h"
#include "dbgprint.h"
#include "dict.h"
#include "dsurface.h"
#include "hsv.h"
#include "misc.h"
#include "rgb.h"
#include "srfcache.h"
#include "utf8.h"
#include "wstring.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>


extern unsigned int Wstring_Hash(Wstring & string);


COLORREF ODColorText = RGB(112, 255, 0);

unsigned short ODRComponentMask;
unsigned short ODGComponentMask;
unsigned short ODBComponentMask;


static bool ODGetFontMetrics(char const * font_name, ODFontMetrics * metrics);
static void ODDrawCharRemap(Surface & dst_surf, const char * text, int max_chars, Rect const & rect, char const * font_name, COLORREF color, char flags, int char_spacing);
static int ODColorToHiColor(COLORREF color);
static void ODBuildRemapColors(COLORREF color, unsigned char const * palette, RGBClass * out);


/// <summary>
/// Reads a picture into the surface cache if it is not there already.
/// The cache is a pure lookup and never loads anything itself. The routine that filled it
/// up front went with the owner-draw dialogs, and no other point in startup is both after
/// the mix files are mounted and before every surviving screen paints, so a picture is read
/// when it is first asked for. A name that could not be read is not tried again.
/// </summary>
/// <param name="red_channel">True to reduce the picture to the red component of its own
/// palette, which is how a coverage sheet is stored.</param>
/// <returns>bool; Is the picture in the cache?</returns>
static bool ODCacheImage(char const * name, int bpp, bool red_channel)
{
	static std::set<std::string> _attempted;

	if (SurfaceCache.GetSurface(name) != NULL) {
		return(true);
	}
	if (!_attempted.insert(std::string(name)).second) {
		return(false);
	}

	if (!SurfaceCache.CachePCX(name, bpp, red_channel)) {
		DebugString("TS: %s could not be read.\n", name);
		return(false);
	}
	return(true);
}


/// <summary>
/// Reads a remap font's two sheets into the surface cache.
/// The index sheet keeps its palette indices and its palette; the alpha sheet is reduced to
/// the red component of its own palette, which is the coverage each pixel carries.
/// </summary>
static void ODCacheFontSheets(char const * font_name)
{
	char name[64];

	snprintf(name, sizeof(name), "%si.pcx", font_name);
	ODCacheImage(name, 1, false);

	snprintf(name, sizeof(name), "%sa.pcx", font_name);
	ODCacheImage(name, 1, true);
}


/// <summary>
/// Fetches one of the dialog system's pictures, reading it on first request.
/// </summary>
/// <param name="name">File name of the .PCX, which is also its name in the cache.</param>
/// <returns>Returns with the cached surface, or NULL if the picture could not be read. The
/// surface stays owned by the cache.</returns>
Surface * OD_Fetch_Image(char const * name)
{
	ODCacheImage(name, 2, false);
	return(SurfaceCache.GetSurface(name));
}


/// <summary>
/// Maps a decoded code point onto the glyph the remap sheets index by.
/// </summary>
/// <returns>Returns with the Windows-1252 code of the glyph, or that of '?' for a code
/// point the sheets do not carry.</returns>
unsigned char OD_Font_Glyph(char32_t code)
{
	if (code < ' ') {
		return((unsigned char)code);
	}
	int index = UTF8::Windows_1252_Glyph(code);
	return((unsigned char)(index < 0 ? '?' : index));
}


static unsigned char OD_Glyph(char32_t code)
{
	return(OD_Font_Glyph(code));
}


/// <summary>
/// Sets up the color component masks used for blending.
/// The masks depend on how the display surface packs its pixels, so this routine cannot
/// run until the video mode is known.
/// </summary>
static void ODInitMasks(void)
{
	ODRComponentMask = 255;
	ODRComponentMask = ODRComponentMask >> DSurface::Get_Red_Left();
	ODRComponentMask <<= DSurface::Get_Red_Right();

	ODGComponentMask = 255;
	ODGComponentMask = ODGComponentMask >> DSurface::Get_Green_Left();
	ODGComponentMask <<= DSurface::Get_Green_Right();

	ODBComponentMask = 255;
	ODBComponentMask = ODBComponentMask >> DSurface::Get_Blue_Left();
	ODBComponentMask <<= DSurface::Get_Blue_Right();
}


/// <summary>
/// Converts a Windows color reference into a display pixel.
/// The dialog colors are all written as RGB() values, so they have to be packed into the
/// pixel layout of the display surface before anything can be drawn with them.
/// </summary>
/// <returns>Returns with the packed pixel value. An all-ones color is passed through
/// unchanged.</returns>
static int ODColorToHiColor(COLORREF color)
{
	if (color == 0xFFFFFFFF) {
		return(0xFFFFFFFF);
	}
	/// Do not replace the union with direct byte extraction. It improves several callers and
	/// breaks ProgressBarCtrlProc, which is otherwise exact -- and an exact caller outranks the
	/// partial ones.
	union {
		struct {
			unsigned int red : 8;
			unsigned int green : 8;
			unsigned int blue : 8;
			unsigned int a : 8;
		};
		int v;
	} c;

	c.v = color;

	return(DSurface::Build_Hicolor_Pixel(c.red, c.green, c.blue));
}


/// <summary>
/// Draws word wrapped text with a remapped bitmap font.
/// This routine breaks the text into lines that will fit the rectangle -- honoring the
/// newlines already in it and breaking at a space wherever one can be found -- and hands
/// each line in turn to ODDrawCharRemap.
/// </summary>
/// <param name="name">The base name of the font sheets to draw with.</param>
/// <param name="flags">The OD_DRAW_CHAR alignment flags to lay each line out with.</param>
/// <param name="char_spacing">The extra spacing to insert between characters.</param>
int OD_Draw_Text_Remap(Surface & surface, const char * text, Rect const & rect, char const * name, COLORREF color, int flags, int char_spacing)
{
	int line_len = strlen(text);
	char const * line_ptr = text;
	Rect draw_rect = rect;

	ODFontMetrics data;
	if (!ODGetFontMetrics(name, &data)) {
		return(0);
	}

	while (line_len) {
		if (line_ptr) {
			char const * nl_ptr = strchr(line_ptr, '\n');
			if (nl_ptr) {
				int nl_len = (int)(nl_ptr - line_ptr) + 1;
				if (line_len >= nl_len) {
					line_len = nl_len;
				}
			}
		}

		if ((unsigned char)*line_ptr <= ' ') {
			++line_ptr;
			if (--line_len == 0) {
				return(0);
			}
		}

		int text_width = 0;
		for (char const * cursor = text; cursor - text < line_len; ) {
			text_width += char_spacing + data.charWidths[OD_Glyph(UTF8::Decode(cursor))];
		}

		if (text_width > draw_rect.Width - draw_rect.X) {
			int fallback = (int)UTF8::Boundary_Before(line_ptr, line_len - 1);
			int cut = line_len - 1;

			flags &= ~4;

			while (cut > 0) {
				if ((unsigned char)line_ptr[cut] <= ' ') {
					break;
				}
				--cut;
			}
			if (cut > 0) {
				line_len = cut;
				if (cut != -1) {
					continue;
				}
			}

			line_len = fallback;
		} else {
			ODDrawCharRemap(surface, line_ptr, line_len, draw_rect, name, color, (char)flags, char_spacing);
			line_ptr += line_len;
			draw_rect.Y += data.glyphHeight;
			line_len = strlen(line_ptr);
		}
	}

	return(0);
}


/// <summary>
/// Determines how strongly a hue should be remapped.
/// The font remapper uses this to pull its hue shift back around the primary colors, so
/// that text tinted near one of them does not swing away from the color asked for.
/// </summary>
/// <param name="hue">The hue to compute the factor for.</param>
/// <returns>Returns with the scale factor; the nearer the hue sits to a primary, the smaller
/// it gets.</returns>
static float ODCalcTextRemapFactor(int hue)
{
	float val = 1.0f;

	int arr[3];
	arr[0] = 43;
	arr[1] = 128;
	arr[2] = 213;

	for (int i = 0; i < 3; i++) {
		int value = arr[i];

		if (hue > value - 16 && hue <= value) {
			val = float(value - hue);
			val *= (1.0f / 16);
			val *= (60.0f / 100);
			val += (40.0f / 100);
		} else if (hue > value && hue <= value + 16) {
			val = float(hue - value);
			val *= (1.0f / 16);
			val *= (60.0f / 100);
			val += (40.0f / 100);
		}
	}
	return(val);
}


/// <summary>
/// Shifts a font sheet's palette toward the color text is asked to be drawn in.
/// The sheets hold an intensity ramp of their own hue, so each entry keeps its own
/// saturation and value and is pulled around to the requested hue rather than replaced
/// by it.
/// </summary>
/// <param name="palette">The 768-byte palette of the font's index sheet.</param>
/// <param name="out">Receives one remapped color for each of the 256 palette entries.</param>
static void ODBuildRemapColors(COLORREF color, unsigned char const * palette, RGBClass * out)
{
	RGBClass remap_rgb((unsigned char)color, (unsigned char)(color >> 8), (unsigned char)(color >> 16));
	HSVClass remap_hsv = remap_rgb;

	int hue = remap_hsv.Get_Hue();

	int end = int(hue + 15.0);
	float min_factor = 1.0f;
	for (int i = int(hue - 15.0); i <= end; ++i) {
		float factor = ODCalcTextRemapFactor(i);
		if (factor < min_factor) {
			min_factor = factor;
		}
	}

	unsigned char sat = (unsigned char)remap_hsv.Get_Saturation();
	unsigned char val = (unsigned char)remap_hsv.Get_Value();

	float hue_float = (float)hue;
	unsigned char const * pal = palette;
	for (int i = 0; i < 256; ++i) {
		RGBClass pal_rgb;
		pal_rgb.Set_Red(pal[0]);
		pal_rgb.Set_Green(pal[1]);
		pal_rgb.Set_Blue(pal[2]);
		HSVClass pal_hsv = pal_rgb;

		HSVClass out_hsv = pal_hsv;
		out_hsv.Set_Hue((unsigned char)(int)(hue_float - (int)(68.0f - pal_hsv.Get_Hue()) * min_factor));
		out_hsv.Set_Saturation((unsigned char)((sat * out_hsv.Get_Saturation()) >> 8));
		out_hsv.Set_Value((unsigned char)((val * out_hsv.Get_Value()) >> 8));

		out[i] = out_hsv;
		pal += 3;
	}
}


/// <summary>
/// Draws a line of text with a remapped bitmap font.
/// This routine builds a table that shifts the font's own palette toward the color asked
/// for and then alpha blends each character onto the destination surface. It is the low
/// level draw that all of the owner-draw remapped text ends up going through.
/// </summary>
/// <param name="max_chars">The maximum number of characters of the text to draw.</param>
/// <param name="rect">The rectangle to align the text within.</param>
/// <param name="font_name">The base name of the font sheets to draw with.</param>
/// <param name="flags">The OD_DRAW_CHAR alignment flags to lay the text out with.</param>
/// <param name="char_spacing">The extra spacing to insert between characters.</param>
static void ODDrawCharRemap(Surface & dst_surf, const char *text, int max_chars, Rect const & rect, char const *font_name, COLORREF color, char flags, int char_spacing)
{
	int i;
	Rect draw_rect = rect;

	ODCacheFontSheets(font_name);

	char name_i[64];
	strcpy(name_i, font_name);
	strcat(name_i, "i.pcx");

	char palette[768];
	Surface *sheet_i = SurfaceCache.GetSurface(name_i, palette);
	if (sheet_i == NULL) {
		return;
	}

	char name_a[64];
	strcpy(name_a, font_name);
	strcat(name_a, "a.pcx");

	Surface *sheet_a = SurfaceCache.GetSurface(name_a, NULL);
	if (sheet_a == NULL) {
		return;
	}

	RGBClass remap_colors[256];
	ODBuildRemapColors(color, (unsigned char *)palette, remap_colors);

	unsigned short remap_table[256];
	for (i = 0; i < 256; ++i) {
		int packed = (((remap_colors[i].Get_Blue() << 8) | remap_colors[i].Get_Green()) << 8) | remap_colors[i].Get_Red();
		remap_table[i] = (unsigned short)ODColorToHiColor(packed);
	}

	ODFontMetrics font_data;
	if (!ODGetFontMetrics(font_name, &font_data)) {
		return;
	}

	if ((int)strlen(text) < max_chars) {
		max_chars = strlen(text);
	}

	int total_width = 0;
	for (char const * cursor = text; cursor - text < max_chars; ) {
		total_width += font_data.charWidths[OD_Glyph(UTF8::Decode(cursor))] + char_spacing;
	}

	if ((flags & OD_DRAW_CHAR_FLAG_HORIZONTAL_CENTER) != 0) {
		draw_rect.X += (draw_rect.Width - draw_rect.X - total_width) / 2;
	} else if ((flags & OD_DRAW_CHAR_ALIGN_FLAG_RIGHT) != 0) {
		draw_rect.X = draw_rect.Width - total_width - 1;
	}

	if ((flags & OD_DRAW_CHAR_FLAG_VERTICAL_CENTER) != 0) {
		draw_rect.Y = draw_rect.Y + (draw_rect.Height - font_data.glyphHeight - draw_rect.Y) / 2;
	}

	draw_rect.Y -= font_data.topMargin;
	--draw_rect.X;

	unsigned char *src_i = (unsigned char *)sheet_i->Lock();
	unsigned char *src_a = (unsigned char *)sheet_a->Lock();
	unsigned char *dst = (unsigned char *)dst_surf.Lock();

	if (src_i != NULL && src_a != NULL && dst != NULL) {
		int cell_w = font_data.glyphWidth + font_data.leftMargin;
		int cell_h = font_data.glyphHeight + font_data.topMargin;
		int chars_per_row = sheet_i->Get_Width() / (font_data.glyphWidth + font_data.leftMargin);
		int dst_stride = dst_surf.Stride() / 2;

		/*
		 * The two sheets are separate allocations, so the coverage sheet cannot be indexed
		 * by an offset from the color sheet: the difference between two unrelated pointers
		 * does not fit an int on a 64-bit host, which is what the inherited code stored it
		 * in. Each sheet is walked through its own pointer and its own stride instead.
		 */
		int index_stride = sheet_i->Stride();
		int alpha_stride = sheet_a->Stride();

		int x = draw_rect.X;
		for (char const * cursor = text; cursor - text < max_chars; ) {

			unsigned char index = OD_Glyph(UTF8::Decode(cursor));
			if (index <= ' ') {
				x += font_data.charWidths[index] + char_spacing;
			} else {
				int glyph = index + 1;
				int src_x = (glyph % chars_per_row) * cell_w;
				int src_y = (glyph / chars_per_row) * cell_h;

				int src_y_end = src_y + cell_h;
				unsigned char *alpha_col = src_a + (src_y * alpha_stride + src_x);
				unsigned char *index_col = src_i + (src_y * index_stride + src_x);
				unsigned char *dst_col = dst + 2 * (dst_stride * draw_rect.Y + x);

				for (int sx = src_x; sx < src_x + cell_w; ++sx) {
					if (src_y < src_y_end) {
						unsigned short *dst_px = (unsigned short *)dst_col;
						unsigned char *alpha_px = alpha_col;
						unsigned char *index_px = index_col;

						int sy = src_y_end - src_y;
						do {
							unsigned char alpha = *alpha_px;
							if (alpha != 0) {
								*dst_px = OD_Blend_Color(*dst_px, remap_table[*index_px], alpha);
							}

							dst_px += dst_stride;
							alpha_px += alpha_stride;
							index_px += index_stride;
							--sy;
						} while (sy != 0);
					}

					++alpha_col;
					++index_col;
					dst_col += 2;
				}

				x += font_data.charWidths[index] + char_spacing;
			}
		}
	}

	if (&dst_surf != NULL) {
		dst_surf.Unlock();
	}
	sheet_a->Unlock();
	sheet_i->Unlock();
}


/// <summary>
/// Fetches the metrics of a remappable bitmap font.
/// This routine measures the font's sheet -- the margins, the size of a character cell and
/// the inked width of every character -- so that the remap text routines know how to lay
/// characters out. Measuring is expensive, so the result is kept by font name.
/// </summary>
/// <param name="font_name">The base name of the font, without the sheet suffix.</param>
/// <param name="metrics">Buffer to fill in with the measurements.</param>
/// <returns>bool; Were the metrics available?</returns>
static bool ODGetFontMetrics(char const * font_name, ODFontMetrics * metrics)
{
	static Dictionary<Wstring, ODFontMetrics> metricsDict(Wstring_Hash);

	char buf[64];
	strcpy(buf, font_name);
	strcat(buf, "a.pcx");

	Wstring name;
	name = (char *)font_name;
	name.toLower();

	ODFontMetrics * found = NULL;
	if (metricsDict.getPointer(name, &found)) {
		if (metrics != NULL) {
			*metrics = *found;
			return(true);
		}
	}

	DebugString("TS: Computing font metrics....\n");

	ODFontMetrics temp;
	memset(&temp, 0, sizeof(temp));

	ODCacheFontSheets(font_name);

	char palette[768];
	Surface * surf = SurfaceCache.GetSurface(buf, palette);
	if (surf == NULL) {
		return(false);
	}

	char * basePtr = (char *)surf->Lock();
	int stride = surf->Stride();

	/*
	 * ----------------------------------------------------------------
	 * Vertical metrics: topMargin = blank rows above the glyph row,
	 * glyphHeight = inked rows (probed at column 4).
	 * ----------------------------------------------------------------
	 */
	temp.topMargin = 0;
	while (temp.topMargin < surf->Get_Height()) {
		if (basePtr[stride * temp.topMargin + 4] != 0) break;
		++temp.topMargin;
	}
	int y = temp.topMargin;
	while (y < surf->Get_Height()) {
		if (basePtr[stride * y + 4] == 0) break;
		++y;
		++temp.glyphHeight;
	}

	/*
	 * ----------------------------------------------------------------
	 * Horizontal metrics: leftMargin = blank columns before the glyphs,
	 * glyphWidth = inked columns (probed along row 'top').
	 * ----------------------------------------------------------------
	 */
	temp.leftMargin = 0;
	while (temp.leftMargin < surf->Get_Width()) {
		if (basePtr[stride * temp.topMargin + temp.leftMargin] != 0) break;
		++temp.leftMargin;
	}
	int left = temp.leftMargin;

	int x;
	x = left;
	while (x < surf->Get_Width()) {
		if (basePtr[stride * temp.topMargin + x] == 0) break;
		++x;
		++temp.glyphWidth;
	}

	/*
	 * ----------------------------------------------------------------
	 * Compute per-character metrics
	 * ----------------------------------------------------------------
	 */
	int width = surf->Get_Width();
	int charsPerRow = width / (left + temp.glyphWidth);
	for (int ch = 0; ch < 256; ++ch) {

		int left = temp.leftMargin;
		int fontHeight = temp.glyphHeight;
		int top = temp.topMargin;
		int fontWidth = temp.glyphWidth;

		int glyphY = top + (fontHeight + top) * ((ch + 1) / charsPerRow);
		int glyphX = left + (left + fontWidth) * ((ch + 1) % charsPerRow);

		int first = -1;
		int last = 0;

		for (int x = glyphX; x < glyphX + fontWidth; ++x) {
			int nonEmpty = 0;
			for (int y = glyphY; y < glyphY + fontHeight; ++y) {
				if (basePtr[stride * y + x] != 0) ++nonEmpty;
			}
			if (nonEmpty) {
				last = x;
				if (first == -1) first = x;
			}
		}

		if (first != -1) {
			temp.charWidths[ch] = (last - first + 1);
		} else {
			temp.charWidths[ch] = (fontWidth / 3 + 1);
		}
	}

	surf->Unlock();

	/*
	 * ----------------------------------------------------------------
	 * Store result in caller's buffer
	 * ----------------------------------------------------------------
	 */
	memcpy(metrics, &temp, sizeof(ODFontMetrics));

	metricsDict.add(name, temp);

	return(true);
}


/// <summary>
/// Measures a remap font.
/// </summary>
/// <param name="font_name">The base name of the font, without the sheet suffix.</param>
/// <returns>bool; Could the font's sheets be read?</returns>
bool OD_Font_Metrics(char const * font_name, ODFontMetrics & metrics)
{
	return(ODGetFontMetrics(font_name, &metrics));
}


/// <summary>
/// Composes a remap font's glyph sheet into premultiplied RGBA pixels.
/// The sheets are combined the way ODDrawCharRemap combines them per pixel: the coverage
/// sheet supplies alpha and the index sheet a palette entry shifted toward the color asked
/// for. The result is what that blend produces over black, so a caller can hand it to a
/// renderer that blends premultiplied source over what is already there.
/// </summary>
/// <param name="pixels">Receives width * height * 4 bytes in RGBA order.</param>
/// <returns>bool; Could the font's sheets be read?</returns>
bool OD_Font_Sheet(char const * font_name, COLORREF color, int & width, int & height, std::vector<unsigned char> & pixels)
{
	ODCacheFontSheets(font_name);

	char name_i[64];
	snprintf(name_i, sizeof(name_i), "%si.pcx", font_name);

	char palette[768];
	Surface * sheet_i = SurfaceCache.GetSurface(name_i, palette);
	if (sheet_i == NULL) {
		return(false);
	}

	char name_a[64];
	snprintf(name_a, sizeof(name_a), "%sa.pcx", font_name);

	Surface * sheet_a = SurfaceCache.GetSurface(name_a, NULL);
	if (sheet_a == NULL) {
		return(false);
	}

	width = sheet_i->Get_Width();
	height = sheet_i->Get_Height();
	if (width <= 0 || height <= 0 || sheet_a->Get_Width() < width || sheet_a->Get_Height() < height) {
		return(false);
	}

	RGBClass remap_colors[256];
	ODBuildRemapColors(color, (unsigned char *)palette, remap_colors);

	unsigned char * src_i = (unsigned char *)sheet_i->Lock();
	unsigned char * src_a = (unsigned char *)sheet_a->Lock();
	if (src_i == NULL || src_a == NULL) {
		if (src_i != NULL) {
			sheet_i->Unlock();
		}
		if (src_a != NULL) {
			sheet_a->Unlock();
		}
		return(false);
	}

	int index_stride = sheet_i->Stride();
	int alpha_stride = sheet_a->Stride();

	pixels.assign((std::size_t)width * height * 4, 0);

	for (int y = 0; y < height; ++y) {
		unsigned char const * row_i = src_i + (std::size_t)index_stride * y;
		unsigned char const * row_a = src_a + (std::size_t)alpha_stride * y;
		unsigned char * out = pixels.data() + (std::size_t)width * y * 4;

		for (int x = 0; x < width; ++x) {
			unsigned char alpha = row_a[x];
			if (alpha != 0) {
				RGBClass const & rgb = remap_colors[row_i[x]];
				out[0] = (unsigned char)((rgb.Get_Red() * alpha + 127) / 255);
				out[1] = (unsigned char)((rgb.Get_Green() * alpha + 127) / 255);
				out[2] = (unsigned char)((rgb.Get_Blue() * alpha + 127) / 255);
				out[3] = alpha;
			}
			out += 4;
		}
	}

	sheet_a->Unlock();
	sheet_i->Unlock();
	return(true);
}


/// <summary>
/// Draws a line of text onto a surface.
/// This routine borrows a device context from the surface, unlocking it as often as it
/// must beforehand, and lets Windows put the text out aligned within the rectangle given.
/// Nothing is drawn while the game does not hold the focus.
/// </summary>
/// <param name="len">The number of characters of the text to draw.</param>
/// <param name="surface">The surface to draw upon, or NULL to draw on the alternate
/// surface.</param>
/// <returns>Returns with the pixel width of the text.</returns>
int OD_Draw_Text(COLORREF color, HFONT font, Rect const & rect, const char * text, int len, int x_alignment, int y_alignment, Surface * surface)
{
	if (!GameInFocus && !WindowedMode) {
		return(0);
	}

	DSurface *destsurf = (DSurface *)surface;
	if (!surface) {
		destsurf = (DSurface *)AlternateSurface;
	}

	int lock_count = 0;
	while (destsurf->Is_Locked()) {
		lock_count++;
		destsurf->Unlock();
	}

	SIZE text_size;

	HDC hDC = destsurf->GetDC();
	if (hDC) {

		if (font) {
			SelectObject(hDC, font);
		}

		SetTextColor(hDC, color);
		SetBkMode(hDC, TRANSPARENT);

		GetTextExtentPoint32(hDC, text, len, &text_size);

		int x_offset = rect.X;
		int y_offset = rect.Y;

		if (x_alignment == OD_TEXT_ALIGN_MIN) {
			x_offset += (rect.Width - text_size.cx + 1) / 2;
		} else if (x_alignment == OD_TEXT_ALIGN_CENTER) {
			x_offset += (text_size.cx + 1) / -2;
		} else if (x_alignment == OD_TEXT_ALIGN_MAX) {
			x_offset += -1 - text_size.cx;
		}

		if (y_alignment == OD_TEXT_ALIGN_MIN) {
			y_offset += (rect.Height - text_size.cy + 1) / 2;
		} else if (y_alignment == OD_TEXT_ALIGN_CENTER) {
			y_offset += (text_size.cy + 1) / -2;
		} else if (y_alignment == OD_TEXT_ALIGN_MAX) {
			y_offset += -1 - text_size.cy;
		}

		TextOut(hDC, x_offset, y_offset, text, len);
		destsurf->ReleaseDC(hDC);
	} else {
		text_size.cx = 0;
	}

	while (lock_count) {
		destsurf->Lock();
		lock_count--;
	}

	return(text_size.cx);
}



/// <summary>
/// Fetches a window's rectangle relative to the main game window.
/// The dialog layout code works in the main window's client space rather than in screen
/// coordinates, so it uses this routine in place of GetWindowRect.
/// </summary>
/// <param name="rect">Receives the window rectangle, offset into the main window's
/// client area.</param>
/// <returns>bool; Was the window rectangle available?</returns>
BOOL Get_Display_Rect(HWND window, LPRECT rect)
{
	RECT client;
	BOOL res = GetWindowRect(window, rect);
	if (!res) {
		return(res);
	}
	GetClientRect(MainWindow, &client);
	ClientToScreen(MainWindow, (LPPOINT)&client);
	rect->left -= client.left;
	rect->right -= client.left;
	rect->top -= client.top;
	rect->bottom -= client.top;
	return(res);
}


struct EzFont {
	char FaceName[128];
	int DeciPtWidth;
	int DeciPtHeight;
	int Attributes;
	HFONT FontHandle;
};

static ArrayList<EzFont> g_EzFonts;


/// derived from MSDN "Moving Your Game to Windows, Part III" ttfont.cpp

#define EZ_ATTR_BOLD		  1
#define EZ_ATTR_ITALIC		  2
#define EZ_ATTR_UNDERLINE	  4
#define EZ_ATTR_STRIKEOUT	  8

static HFONT Ez_Create_Font(HDC hdc, const char * face_name, int decipt_width, int decipt_height, int attributes);


/// <summary>
/// Fetches a font of the typeface and point size requested.
/// This routine keeps every font it has built, so repeated requests for the same
/// description hand back the same handle rather than burning another GDI object.
/// The dialog drawing code calls this routine wherever it needs a font.
/// </summary>
/// <param name="hdc">The device context to build the font for. If this is NULL, the
/// font is only looked up and never created.</param>
/// <param name="decipt_width">The character width in tenths of a point.</param>
/// <param name="decipt_height">The character height in tenths of a point.</param>
/// <param name="attributes">Bit flags of the EZ_ATTR_ style attributes to apply.</param>
/// <returns>Returns with a handle to the font, or NULL if it was neither cached nor
/// able to be created.</returns>
/// <remarks>The returned handle stays owned by the font cache. Do not delete it.</remarks>
HFONT WS_Get_Font(HDC hdc, const char * face_name, int decipt_width, int decipt_height, int attributes)
{
	EzFont font;

	for (int index = 0; index < g_EzFonts.length(); index++) {
		g_EzFonts.get(font, index);
		if (!strcmp(font.FaceName, face_name) && font.DeciPtWidth == decipt_width && font.DeciPtHeight == decipt_height && font.Attributes == attributes) {
			return(font.FontHandle);
		}
	}

	if (hdc == NULL) {
		return(NULL);
	}

	HFONT hFont = Ez_Create_Font(hdc, face_name, decipt_width, decipt_height, attributes);

	if (hFont == NULL) {
		return(NULL);
	}

	strcpy(font.FaceName, face_name);
	font.DeciPtWidth = decipt_width;
	font.DeciPtHeight = decipt_height;
	font.Attributes = attributes;
	font.FontHandle = hFont;

	if (g_EzFonts.addTail(font)) {
		return(hFont);
	}

	return(NULL);
}



/// <summary>
/// Creates a font of the typeface and point size requested.
/// This routine maps the requested decipoint dimensions through the device context's
/// current transform, so the font it builds matches the coordinate space the caller
/// draws in. Use WS_Get_Font in preference to this routine -- that one caches its fonts.
/// </summary>
/// <param name="hdc">The device context the font is to be built for.</param>
/// <param name="decipt_width">The character width in tenths of a point. Zero lets the
/// typeface choose its own aspect.</param>
/// <param name="decipt_height">The character height in tenths of a point.</param>
/// <param name="attributes">Bit flags of the EZ_ATTR_ style attributes to apply.</param>
/// <returns>Returns with a handle to the font created, or NULL if it could not be
/// created.</returns>
/// <remarks>The caller takes ownership of the font handle.</remarks>
static HFONT Ez_Create_Font(HDC hdc, const char * face_name, int decipt_width,
					int decipt_height, int attributes)
{
	HFONT		hFont ;
	LOGFONT	lf ;
	POINT		pt ;
	TEXTMETRIC tm ;

	SaveDC (hdc) ;

	SetGraphicsMode (hdc, GM_ADVANCED) ;
	ModifyWorldTransform (hdc, NULL, MWT_IDENTITY) ;
	SetViewportOrgEx (hdc, 0, 0, NULL) ;
	SetWindowOrgEx   (hdc, 0, 0, NULL) ;

	pt.x = decipt_width ;
	pt.y = decipt_height ;

	DPtoLP (hdc, &pt, 1) ;

	lf.lfHeight			= -pt.y ;
	lf.lfWidth			= 0 ;
	lf.lfEscapement		= 0 ;
	lf.lfOrientation	= 0 ;
	lf.lfWeight		 = attributes & EZ_ATTR_BOLD	   ? 700 : 0 ;
	lf.lfItalic		 = attributes & EZ_ATTR_ITALIC    ?   1 : 0 ;
	lf.lfUnderline 	 = attributes & EZ_ATTR_UNDERLINE ?   1 : 0 ;
	lf.lfStrikeOut 	 = attributes & EZ_ATTR_STRIKEOUT ?   1 : 0 ;
	lf.lfCharSet		= ANSI_CHARSET ;
	lf.lfOutPrecision	= 0 ;
	lf.lfClipPrecision	= 0 ;
	lf.lfQuality		= 0 ;
	lf.lfPitchAndFamily	= 0 ;

	strcpy (lf.lfFaceName, face_name) ;

	hFont = CreateFontIndirect (&lf) ;

	if (decipt_width != 0) {
		hFont = (HFONT) SelectObject (hdc, hFont) ;
		GetTextMetrics (hdc, &tm) ;
		DeleteObject (SelectObject (hdc, hFont)) ;
		lf.lfWidth = (int) (tm.tmAveCharWidth *
									fabs (pt.x) / fabs (pt.y) + 0.5);
		hFont = CreateFontIndirect (&lf) ;
	}

	RestoreDC (hdc, -1);
	return(hFont);
}


static int _pointer_depth;


/// <summary>
/// Hands the mouse pointer to the host while a screen of its own is shown.
/// With the game's mouse released, WM_SETCURSOR falls through to the window class and the
/// host draws an arrow. A front end has no game pointer of its own, so without this a
/// screen shows none.
/// </summary>
/// <remarks>Each call must be matched by a call to Recapture_Pointer.</remarks>
void Release_Pointer_To_Host(void)
{
	if (MouseCursor != nullptr && MouseCursor->Is_Captured()) {
		MouseCursor->Release_Mouse();
	}

	_pointer_depth++;
}


/// <summary>
/// Takes the pointer back once the last screen holding it has gone.
/// </summary>
void Recapture_Pointer(void)
{
	if (_pointer_depth > 0) {
		_pointer_depth--;
	}

	if (_pointer_depth == 0 && MouseCursor != nullptr && !MouseCursor->Is_Captured()) {
		MouseCursor->Capture_Mouse();
	}
}


/// <summary>
/// Builds the color masks the blending helpers paint with.
/// The masks depend on how the display surface packs its pixels, so the video mode has to
/// be up before this runs.
/// </summary>
void Prepare_Draw_Resources(void)
{
	ODInitMasks();
}
