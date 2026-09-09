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

// The virtual key codes the engine's keyboard queue is written against. They are stated
// as numbers rather than taken from the engine's own header so that this layer does not
// depend on the engine it serves.
static int const VK_LBUTTON_CODE = 0x01;
static int const VK_RBUTTON_CODE = 0x02;
static int const VK_MBUTTON_CODE = 0x04;

struct KeyMapping
{
	SDL_Scancode Scancode;
	int VirtualKey;
};

static KeyMapping const _Keys[] = {
	{ SDL_SCANCODE_BACKSPACE, 0x08 }, { SDL_SCANCODE_TAB, 0x09 },
	{ SDL_SCANCODE_RETURN, 0x0D }, { SDL_SCANCODE_ESCAPE, 0x1B },
	{ SDL_SCANCODE_SPACE, 0x20 }, { SDL_SCANCODE_PAGEUP, 0x21 },
	{ SDL_SCANCODE_PAGEDOWN, 0x22 }, { SDL_SCANCODE_END, 0x23 },
	{ SDL_SCANCODE_HOME, 0x24 }, { SDL_SCANCODE_LEFT, 0x25 },
	{ SDL_SCANCODE_UP, 0x26 }, { SDL_SCANCODE_RIGHT, 0x27 },
	{ SDL_SCANCODE_DOWN, 0x28 }, { SDL_SCANCODE_INSERT, 0x2D },
	{ SDL_SCANCODE_DELETE, 0x2E },
	{ SDL_SCANCODE_LSHIFT, 0x10 }, { SDL_SCANCODE_RSHIFT, 0x10 },
	{ SDL_SCANCODE_LCTRL, 0x11 }, { SDL_SCANCODE_RCTRL, 0x11 },
	{ SDL_SCANCODE_LALT, 0x12 }, { SDL_SCANCODE_RALT, 0x12 },
	{ SDL_SCANCODE_CAPSLOCK, 0x14 }, { SDL_SCANCODE_PAUSE, 0x13 },
	{ SDL_SCANCODE_PRINTSCREEN, 0x2C }, { SDL_SCANCODE_SCROLLLOCK, 0x91 },
	{ SDL_SCANCODE_NUMLOCKCLEAR, 0x90 },
	{ SDL_SCANCODE_KP_0, 0x60 }, { SDL_SCANCODE_KP_1, 0x61 }, { SDL_SCANCODE_KP_2, 0x62 },
	{ SDL_SCANCODE_KP_3, 0x63 }, { SDL_SCANCODE_KP_4, 0x64 }, { SDL_SCANCODE_KP_5, 0x65 },
	{ SDL_SCANCODE_KP_6, 0x66 }, { SDL_SCANCODE_KP_7, 0x67 }, { SDL_SCANCODE_KP_8, 0x68 },
	{ SDL_SCANCODE_KP_9, 0x69 }, { SDL_SCANCODE_KP_MULTIPLY, 0x6A },
	{ SDL_SCANCODE_KP_PLUS, 0x6B }, { SDL_SCANCODE_KP_MINUS, 0x6D },
	{ SDL_SCANCODE_KP_PERIOD, 0x6E }, { SDL_SCANCODE_KP_DIVIDE, 0x6F },
	{ SDL_SCANCODE_KP_ENTER, 0x0D },
	{ SDL_SCANCODE_F1, 0x70 }, { SDL_SCANCODE_F2, 0x71 }, { SDL_SCANCODE_F3, 0x72 },
	{ SDL_SCANCODE_F4, 0x73 }, { SDL_SCANCODE_F5, 0x74 }, { SDL_SCANCODE_F6, 0x75 },
	{ SDL_SCANCODE_F7, 0x76 }, { SDL_SCANCODE_F8, 0x77 }, { SDL_SCANCODE_F9, 0x78 },
	{ SDL_SCANCODE_F10, 0x79 }, { SDL_SCANCODE_F11, 0x7A }, { SDL_SCANCODE_F12, 0x7B },
	{ SDL_SCANCODE_SEMICOLON, 0xBA }, { SDL_SCANCODE_EQUALS, 0xBB },
	{ SDL_SCANCODE_COMMA, 0xBC }, { SDL_SCANCODE_MINUS, 0xBD },
	{ SDL_SCANCODE_PERIOD, 0xBE }, { SDL_SCANCODE_SLASH, 0xBF },
	{ SDL_SCANCODE_GRAVE, 0xC0 }, { SDL_SCANCODE_LEFTBRACKET, 0xDB },
	{ SDL_SCANCODE_BACKSLASH, 0xDC }, { SDL_SCANCODE_RIGHTBRACKET, 0xDD },
	{ SDL_SCANCODE_APOSTROPHE, 0xDE },
};


int Win32_Virtual_Key(SDL_Scancode scancode, SDL_Keycode keycode)
{
	if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
		return('A' + (scancode - SDL_SCANCODE_A));
	}

	if (scancode == SDL_SCANCODE_0) {
		return('0');
	}

	if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
		return('1' + (scancode - SDL_SCANCODE_1));
	}

	for (KeyMapping const & mapping : _Keys) {
		if (mapping.Scancode == scancode) {
			return(mapping.VirtualKey);
		}
	}

	if (keycode > 0 && keycode < 128) {
		return(SDL_toupper((int)keycode));
	}

	return(0);
}


SDL_Scancode Scancode_For_Virtual_Key(int key)
{
	if (key >= 'A' && key <= 'Z') {
		return((SDL_Scancode)(SDL_SCANCODE_A + (key - 'A')));
	}

	if (key == '0') {
		return(SDL_SCANCODE_0);
	}

	if (key > '0' && key <= '9') {
		return((SDL_Scancode)(SDL_SCANCODE_1 + (key - '1')));
	}

	for (KeyMapping const & mapping : _Keys) {
		if (mapping.VirtualKey == key) {
			return(mapping.Scancode);
		}
	}

	return(SDL_SCANCODE_UNKNOWN);
}


// The engine polls held keys through this rather than through the message queue, so the
// keyboard is answered from the host's live state. The mouse buttons are answered from the
// pointer this layer owns instead, because a host may have no mouse to ask.
extern "C" SHORT GetAsyncKeyState(int key)
{
	SDL_MouseButtonFlags const buttons = Win32_Pointer_Buttons();

	switch (key) {
		case VK_LBUTTON_CODE: return((buttons & SDL_BUTTON_LMASK) != 0 ? (SHORT)0x8000 : 0);
		case VK_RBUTTON_CODE: return((buttons & SDL_BUTTON_RMASK) != 0 ? (SHORT)0x8000 : 0);
		case VK_MBUTTON_CODE: return((buttons & SDL_BUTTON_MMASK) != 0 ? (SHORT)0x8000 : 0);
		default: break;
	}

	SDL_Scancode const scancode = Scancode_For_Virtual_Key(key);

	if (scancode == SDL_SCANCODE_UNKNOWN) {
		return(0);
	}

	int count = 0;
	bool const * state = SDL_GetKeyboardState(&count);

	if (state == NULL || (int)scancode >= count) {
		return(0);
	}

	return(state[scancode] ? (SHORT)0x8000 : 0);
}


extern "C" SHORT GetKeyState(int key)
{
	SHORT const held = GetAsyncKeyState(key);

	// Only the toggling keys carry a low bit, and the host reports those as modifiers.
	SDL_Keymod const modifiers = SDL_GetModState();

	if (key == 0x14) {
		return(held | ((modifiers & SDL_KMOD_CAPS) != 0 ? 1 : 0));
	}

	if (key == 0x90) {
		return(held | ((modifiers & SDL_KMOD_NUM) != 0 ? 1 : 0));
	}

	return(held);
}


extern "C" UINT MapVirtualKey(UINT code, UINT type)
{
	switch (type) {
		// A virtual key to a scan code, and back.
		case 0: return((UINT)Scancode_For_Virtual_Key((int)code));
		case 1: return((UINT)Win32_Virtual_Key((SDL_Scancode)code, 0));

		// A virtual key to the character it types unshifted.
		case 2: {
			SDL_Scancode const scancode = Scancode_For_Virtual_Key((int)code);
			SDL_Keycode const keycode = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
			return(keycode < 128 ? (UINT)SDL_toupper((int)keycode) : 0);
		}

		default:
			return(0);
	}
}


extern "C" int GetKeyNameText(LONG param, LPSTR name, int size)
{
	if (name == NULL || size <= 0) {
		return(0);
	}

	name[0] = '\0';

	SDL_Scancode const scancode = (SDL_Scancode)((param >> 16) & 0xFF);
	char const * text = SDL_GetScancodeName(scancode);

	if (text == NULL) {
		return(0);
	}

	SDL_strlcpy(name, text, (size_t)size);
	return((int)strlen(name));
}


extern "C" int ToUnicode(UINT key, UINT scan, BYTE const * state, LPWSTR buffer, int size, UINT flags)
{
	(void)scan;
	(void)state;
	(void)flags;

	if (buffer == NULL || size <= 0) {
		return(0);
	}

	SDL_Scancode const scancode = Scancode_For_Virtual_Key((int)key);
	SDL_Keymod const modifiers = SDL_GetModState();
	SDL_Keycode const keycode = SDL_GetKeyFromScancode(scancode, modifiers, false);

	if (keycode == 0 || keycode > 0xFFFF) {
		return(0);
	}

	buffer[0] = (wchar_t)keycode;
	return(1);
}


extern "C" BOOL GetCursorPos(LPPOINT point)
{
	if (point == NULL) {
		return(FALSE);
	}

	float x = 0.0f;
	float y = 0.0f;
	Win32_Pointer_Position(&x, &y);

	float const density = Win32_Pixel_Density();
	point->x = (LONG)(x * density);
	point->y = (LONG)(y * density);

	// The same conversion the engine's own round trips use, so a position that goes back
	// through ScreenToClient lands where it started.
	return(ClientToScreen(Win32_Main_Window(), point));
}


// Windows moves the pointer at once, so the position this layer owns is written here rather
// than waited for. A host with a mouse of its own is asked to move it as well, because the
// host is what draws it.
extern "C" BOOL SetCursorPos(int x, int y)
{
	POINT point;
	point.x = (LONG)x;
	point.y = (LONG)y;

	if (!ScreenToClient(Win32_Main_Window(), &point)) {
		return(FALSE);
	}

	float const density = Win32_Pixel_Density();
	Win32_Pointer_Move((float)point.x / density, (float)point.y / density);

#ifndef OPENTS_IOS
	return(SDL_WarpMouseGlobal(x / density, y / density) ? TRUE : FALSE);
#else
	return(TRUE);
#endif
}


static bool _CursorShown = true;
static int _CursorCount;
static HWND _Capture;


// One record per cursor the game builds out of its own shape art. The game keeps the
// handles and reselects them as the pointer changes shape, so a record lives until the
// game destroys it.
struct Win32Cursor
{
	SDL_Cursor * Cursor;
};

static std::vector<Win32Cursor *> _Cursors;
static HCURSOR _CurrentCursor;


static Win32Cursor * Lookup_Cursor(HCURSOR cursor)
{
	for (Win32Cursor * record : _Cursors) {
		if ((HCURSOR)record == cursor) {
			return(record);
		}
	}

	return(NULL);
}


// The pointer the game selects and the counter this API keeps both decide whether anything
// is on screen, so they are applied together.
static void Apply_Cursor(void)
{
#ifdef OPENTS_IOS
	// A finger is its own pointer. The game keeps choosing shapes, because the choice is
	// what the rest of it reads, but nothing draws them.
	SDL_HideCursor();
	return;
#else
	Win32Cursor * record = Lookup_Cursor(_CurrentCursor);

	if (!_CursorShown || _CursorCount < 0) {
		SDL_HideCursor();
		return;
	}

	// While the mouse is released the game selects no shape of its own, and Windows would
	// be drawing the window class's cursor, so the host's own pointer stands in for it.
	SDL_SetCursor(record != NULL ? record->Cursor : SDL_GetDefaultCursor());
	SDL_ShowCursor();
#endif
}


extern "C" HCURSOR SetCursor(HCURSOR cursor)
{
	HCURSOR const previous = _CurrentCursor;

	_CurrentCursor = Lookup_Cursor(cursor) != NULL ? cursor : NULL;
	_CursorShown = _CurrentCursor != NULL;

	Apply_Cursor();
	return(previous);
}


extern "C" int ShowCursor(BOOL show)
{
	_CursorCount += show ? 1 : -1;

	if (show) {
		_CursorShown = true;
	}

	Apply_Cursor();
	return(_CursorCount);
}


// Confining the pointer is what the game does at the window's edge while scrolling. SDL
// confines to a window rather than to a desktop rectangle, so the request is honoured at
// window granularity and a rectangle smaller than the window is not. A host with no mouse
// has nothing to confine, and the pointer this layer owns never leaves the window anyway.
extern "C" BOOL ClipCursor(RECT const * rect)
{
	Win32Window * main = Win32_Lookup(Win32_Main_Window());

	if (main == NULL || main->Handle == NULL) {
		return(FALSE);
	}

#ifndef OPENTS_IOS
	return(SDL_SetWindowMouseGrab(main->Handle, rect != NULL) ? TRUE : FALSE);
#else
	(void)rect;
	return(TRUE);
#endif
}


// The capture is what the message router asks about to keep a drag going, so it is recorded
// whatever the host does with its own mouse.
extern "C" HWND SetCapture(HWND window)
{
	HWND const previous = _Capture;
	_Capture = window;

#ifndef OPENTS_IOS
	SDL_CaptureMouse(true);
#endif

	return(previous);
}


extern "C" BOOL ReleaseCapture(void)
{
	_Capture = NULL;

#ifndef OPENTS_IOS
	SDL_CaptureMouse(false);
#endif

	return(TRUE);
}


extern "C" HWND GetCapture(void)
{
	return(_Capture);
}


extern "C" HCURSOR LoadCursor(HINSTANCE instance, LPCSTR name) { (void)instance; (void)name; return(NULL); }
extern "C" HICON LoadIcon(HINSTANCE instance, LPCSTR name) { (void)instance; (void)name; return(NULL); }
extern "C" BOOL DestroyIcon(HICON icon) { (void)icon; return(TRUE); }


// A 32-bit device-independent bitmap holds its pixels as blue, green, red and alpha in
// memory order, which is what the host calls ARGB8888 on a little-endian machine.
static SDL_Surface * Surface_From_Bitmap(Win32Bitmap const * bitmap)
{
	SDL_Surface * surface = SDL_CreateSurface(bitmap->Width, bitmap->Height, SDL_PIXELFORMAT_ARGB8888);

	if (surface == NULL) {
		return(NULL);
	}

	for (int y = 0; y < bitmap->Height; y++) {
		int const source = bitmap->TopDown ? y : bitmap->Height - 1 - y;
		memcpy((unsigned char *)surface->pixels + (std::size_t)y * (std::size_t)surface->pitch,
			bitmap->Bits + (std::size_t)source * (std::size_t)bitmap->Pitch,
			(std::size_t)bitmap->Width * 4);
	}

	return(surface);
}


/*
 * The game draws its pointer at the scale its frame is presented at, which is measured in
 * physical pixels, while the host lays a cursor out in the points its display uses. The
 * image is offered at the point size that matches, with the pixels the game drew carried
 * alongside it so a dense display still shows all of them.
 */
extern "C" HCURSOR CreateIconIndirect(ICONINFO * info)
{
	if (info == NULL) {
		return(NULL);
	}

	Win32Bitmap const * color = Win32_Lookup_Bitmap(info->hbmColor);

	if (color == NULL || color->BitCount != 32) {
		return(NULL);
	}

	SDL_Surface * pixels = Surface_From_Bitmap(color);

	if (pixels == NULL) {
		return(NULL);
	}

	float const density = Win32_Pixel_Density();
	SDL_Surface * image = pixels;
	int hotx = (int)info->xHotspot;
	int hoty = (int)info->yHotspot;

	if (density > 1.0f) {
		int const width = (int)(pixels->w / density);
		int const height = (int)(pixels->h / density);
		SDL_Surface * scaled = width > 0 && height > 0
			? SDL_ScaleSurface(pixels, width, height, SDL_SCALEMODE_NEAREST) : NULL;

		if (scaled != NULL && SDL_AddSurfaceAlternateImage(scaled, pixels)) {
			image = scaled;
			hotx = (int)(hotx / density);
			hoty = (int)(hoty / density);
		} else if (scaled != NULL) {
			SDL_DestroySurface(scaled);
		}
	}

	if (hotx >= image->w) hotx = image->w - 1;
	if (hoty >= image->h) hoty = image->h - 1;
	if (hotx < 0) hotx = 0;
	if (hoty < 0) hoty = 0;

	SDL_Cursor * cursor = SDL_CreateColorCursor(image, hotx, hoty);

	if (image != pixels) {
		SDL_DestroySurface(image);
	}
	SDL_DestroySurface(pixels);

	if (cursor == NULL) {
		return(NULL);
	}

	Win32Cursor * record = new(std::nothrow) Win32Cursor;

	if (record == NULL) {
		SDL_DestroyCursor(cursor);
		return(NULL);
	}

	record->Cursor = cursor;
	_Cursors.push_back(record);
	return((HCURSOR)record);
}


extern "C" BOOL DestroyCursor(HCURSOR cursor)
{
	for (auto it = _Cursors.begin(); it != _Cursors.end(); ++it) {
		if ((HCURSOR)*it == cursor) {
			if (_CurrentCursor == cursor) {
				_CurrentCursor = NULL;
				SDL_SetCursor(SDL_GetDefaultCursor());
			}

			SDL_DestroyCursor((*it)->Cursor);
			delete *it;
			_Cursors.erase(it);
			return(TRUE);
		}
	}

	return(FALSE);
}
