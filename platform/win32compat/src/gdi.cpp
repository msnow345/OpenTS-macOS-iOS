/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <vector>

// The game composes its own frame in system memory and presents it through bgfx, so GDI
// is reached only by the legacy dialog layer and by the tactical map's text, both of which
// draw with the host font on Windows. Nothing here draws: a device context that does not
// exist refuses every call, and the callers fall back to the engine's own font path.
//
// Bitmaps are the exception. The game builds its mouse cursor by drawing one of its own
// shape frames into a bitmap and handing it to CreateIconIndirect, so a bitmap has to hold
// real pixels for the cursor to exist at all.

extern "C" HDC GetDC(HWND window) { (void)window; return(NULL); }
extern "C" int ReleaseDC(HWND window, HDC dc) { (void)window; (void)dc; return(0); }
extern "C" HDC CreateCompatibleDC(HDC dc) { (void)dc; return(NULL); }
extern "C" BOOL DeleteDC(HDC dc) { (void)dc; return(FALSE); }
extern "C" int SaveDC(HDC dc) { (void)dc; return(0); }
extern "C" BOOL RestoreDC(HDC dc, int state) { (void)dc; (void)state; return(FALSE); }
extern "C" HGDIOBJ SelectObject(HDC dc, HGDIOBJ object) { (void)dc; (void)object; return(NULL); }
extern "C" int GetObject(HGDIOBJ object, int size, LPVOID buffer) { (void)object; (void)size; (void)buffer; return(0); }
extern "C" HGDIOBJ GetStockObject(int index) { (void)index; return(NULL); }
extern "C" HBRUSH CreateSolidBrush(COLORREF color) { (void)color; return(NULL); }
extern "C" HFONT CreateFont(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline, DWORD strikeout, DWORD charset, DWORD outprecision, DWORD clipprecision, DWORD quality, DWORD pitch, LPCSTR face) { (void)height; (void)width; (void)escapement; (void)orientation; (void)weight; (void)italic; (void)underline; (void)strikeout; (void)charset; (void)outprecision; (void)clipprecision; (void)quality; (void)pitch; (void)face; return(NULL); }
extern "C" HFONT CreateFontIndirect(LOGFONT const * font) { (void)font; return(NULL); }
extern "C" BOOL BitBlt(HDC dest, int x, int y, int width, int height, HDC source, int sx, int sy, DWORD rop) { (void)dest; (void)x; (void)y; (void)width; (void)height; (void)source; (void)sx; (void)sy; (void)rop; return(FALSE); }
extern "C" BOOL StretchBlt(HDC dest, int x, int y, int width, int height, HDC source, int sx, int sy, int swidth, int sheight, DWORD rop) { (void)dest; (void)x; (void)y; (void)width; (void)height; (void)source; (void)sx; (void)sy; (void)swidth; (void)sheight; (void)rop; return(FALSE); }
extern "C" int SetStretchBltMode(HDC dc, int mode) { (void)dc; (void)mode; return(0); }
extern "C" int SetDIBitsToDevice(HDC dc, int x, int y, DWORD width, DWORD height, int sx, int sy, UINT start, UINT lines, void const * bits, BITMAPINFO const * info, UINT usage) { (void)dc; (void)x; (void)y; (void)width; (void)height; (void)sx; (void)sy; (void)start; (void)lines; (void)bits; (void)info; (void)usage; return(0); }
extern "C" BOOL TextOut(HDC dc, int x, int y, LPCSTR text, int length) { (void)dc; (void)x; (void)y; (void)text; (void)length; return(FALSE); }
extern "C" int DrawText(HDC dc, LPCSTR text, int length, LPRECT rect, UINT format) { (void)dc; (void)text; (void)length; (void)rect; (void)format; return(0); }
extern "C" BOOL GetTextMetrics(HDC dc, LPTEXTMETRIC metrics) { (void)dc; (void)metrics; return(FALSE); }
extern "C" UINT SetTextAlign(HDC dc, UINT align) { (void)dc; (void)align; return(0); }
extern "C" COLORREF SetTextColor(HDC dc, COLORREF color) { (void)dc; (void)color; return(0); }
extern "C" COLORREF GetTextColor(HDC dc) { (void)dc; return(0); }
extern "C" COLORREF SetBkColor(HDC dc, COLORREF color) { (void)dc; (void)color; return(0); }
extern "C" COLORREF GetBkColor(HDC dc) { (void)dc; return(0); }
extern "C" int SetBkMode(HDC dc, int mode) { (void)dc; (void)mode; return(0); }
extern "C" int GetBkMode(HDC dc) { (void)dc; return(0); }
extern "C" int SetGraphicsMode(HDC dc, int mode) { (void)dc; (void)mode; return(0); }
extern "C" BOOL SetViewportOrgEx(HDC dc, int x, int y, LPPOINT previous) { (void)dc; (void)x; (void)y; (void)previous; return(FALSE); }
extern "C" BOOL SetWindowOrgEx(HDC dc, int x, int y, LPPOINT previous) { (void)dc; (void)x; (void)y; (void)previous; return(FALSE); }
extern "C" BOOL DPtoLP(HDC dc, LPPOINT points, int count) { (void)dc; (void)points; (void)count; return(FALSE); }
extern "C" BOOL FillRect(HDC dc, RECT const * rect, HBRUSH brush) { (void)dc; (void)rect; (void)brush; return(FALSE); }
extern "C" BOOL PatBlt(HDC dc, int x, int y, int width, int height, DWORD rop) { (void)dc; (void)x; (void)y; (void)width; (void)height; (void)rop; return(FALSE); }
extern "C" BOOL ModifyWorldTransform(HDC dc, XFORM const * transform, DWORD mode) { (void)dc; (void)transform; (void)mode; return(FALSE); }
extern "C" void GdiFlush(void) {}


extern "C" BOOL GetTextExtentPoint32(HDC dc, LPCSTR text, int length, LPSIZE size)
{
	(void)dc;
	(void)text;
	(void)length;

	if (size != NULL) {
		size->cx = 0;
		size->cy = 0;
	}

	return(FALSE);
}


extern "C" int GetDeviceCaps(HDC dc, int index)
{
	(void)dc;
	(void)index;
	return(0);
}


static std::vector<Win32Bitmap *> _Bitmaps;


Win32Bitmap * Win32_Lookup_Bitmap(HBITMAP bitmap)
{
	for (Win32Bitmap * record : _Bitmaps) {
		if ((HBITMAP)record == bitmap) {
			return(record);
		}
	}

	return(NULL);
}


static Win32Bitmap * Make_Bitmap(int width, int height, int bitcount, int alignment, bool topdown)
{
	if (width <= 0 || height <= 0) {
		return(NULL);
	}

	// A device-dependent bitmap aligns its scan lines to a word and a device-independent
	// one to a double word, and the caller sizes the pixels it hands over to match.
	int const bits = alignment * 8;
	int const pitch = (((width * bitcount) + bits - 1) / bits) * alignment;

	Win32Bitmap * record = new(std::nothrow) Win32Bitmap;

	if (record == NULL) {
		return(NULL);
	}

	record->Width = width;
	record->Height = height;
	record->BitCount = bitcount;
	record->Pitch = pitch;
	record->TopDown = topdown;
	record->Bits = new(std::nothrow) unsigned char[(std::size_t)pitch * (std::size_t)height]();

	if (record->Bits == NULL) {
		delete record;
		return(NULL);
	}

	_Bitmaps.push_back(record);
	return(record);
}


extern "C" HBITMAP CreateBitmap(int width, int height, UINT planes, UINT bits, void const * data)
{
	if (planes != 1 || (bits != 1 && bits != 32)) {
		return(NULL);
	}

	Win32Bitmap * record = Make_Bitmap(width, height, (int)bits, 2, true);

	if (record != NULL && data != NULL) {
		memcpy(record->Bits, data, (std::size_t)record->Pitch * (std::size_t)record->Height);
	}

	return((HBITMAP)record);
}


// Only the layout the cursor is drawn in is offered: an uncompressed 32-bit image whose
// rows the caller then writes itself. Anything else is refused the way the rest of GDI is.
extern "C" HBITMAP CreateDIBSection(HDC dc, BITMAPINFO const * info, UINT usage, void ** bits, HANDLE section, DWORD offset)
{
	(void)dc;
	(void)usage;

	if (bits != NULL) {
		*bits = NULL;
	}

	if (info == NULL || section != NULL || offset != 0) {
		return(NULL);
	}

	if (info->bmiHeader.biPlanes != 1 || info->bmiHeader.biBitCount != 32 || info->bmiHeader.biCompression != BI_RGB) {
		return(NULL);
	}

	LONG const rawheight = info->bmiHeader.biHeight;
	Win32Bitmap * record = Make_Bitmap((int)info->bmiHeader.biWidth,
		(int)(rawheight < 0 ? -rawheight : rawheight), 32, 4, rawheight < 0);

	if (record != NULL && bits != NULL) {
		*bits = record->Bits;
	}

	return((HBITMAP)record);
}


extern "C" BOOL DeleteObject(HGDIOBJ object)
{
	for (auto it = _Bitmaps.begin(); it != _Bitmaps.end(); ++it) {
		if ((HGDIOBJ)*it == object) {
			delete [] (*it)->Bits;
			delete *it;
			_Bitmaps.erase(it);
			return(TRUE);
		}
	}

	return(FALSE);
}
