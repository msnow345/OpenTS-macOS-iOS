/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

// The game composes its own frame in system memory and presents it through bgfx, so GDI
// is reached only by the legacy dialog layer and by the tactical map's text, both of which
// draw with the host font on Windows. Nothing here draws: a device context that does not
// exist refuses every call, and the callers fall back to the engine's own font path.

extern "C" HDC GetDC(HWND window) { (void)window; return(NULL); }
extern "C" int ReleaseDC(HWND window, HDC dc) { (void)window; (void)dc; return(0); }
extern "C" HDC CreateCompatibleDC(HDC dc) { (void)dc; return(NULL); }
extern "C" BOOL DeleteDC(HDC dc) { (void)dc; return(FALSE); }
extern "C" int SaveDC(HDC dc) { (void)dc; return(0); }
extern "C" BOOL RestoreDC(HDC dc, int state) { (void)dc; (void)state; return(FALSE); }
extern "C" HGDIOBJ SelectObject(HDC dc, HGDIOBJ object) { (void)dc; (void)object; return(NULL); }
extern "C" BOOL DeleteObject(HGDIOBJ object) { (void)object; return(FALSE); }
extern "C" int GetObject(HGDIOBJ object, int size, LPVOID buffer) { (void)object; (void)size; (void)buffer; return(0); }
extern "C" HGDIOBJ GetStockObject(int index) { (void)index; return(NULL); }
extern "C" HBRUSH CreateSolidBrush(COLORREF color) { (void)color; return(NULL); }
extern "C" HBITMAP CreateBitmap(int width, int height, UINT planes, UINT bits, void const * data) { (void)width; (void)height; (void)planes; (void)bits; (void)data; return(NULL); }
extern "C" HBITMAP CreateDIBSection(HDC dc, BITMAPINFO const * info, UINT usage, void ** bits, HANDLE section, DWORD offset) { (void)dc; (void)info; (void)usage; (void)section; (void)offset; if (bits != NULL) *bits = NULL; return(NULL); }
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
