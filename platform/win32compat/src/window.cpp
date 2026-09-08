/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#include <SDL3/SDL_metal.h>

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

static std::vector<Win32Window *> _Windows;
static std::unordered_map<std::string, WNDPROC> _Classes;
static HWND _MainWindow;
static SDL_MetalView _MetalView;


Win32Window * Win32_Lookup(HWND window)
{
	if (window == NULL) {
		return(NULL);
	}

	for (Win32Window * candidate : _Windows) {
		if ((HWND)candidate == window) {
			return(candidate);
		}
	}

	return(NULL);
}


HWND Win32_Main_Window(void)
{
	return(_MainWindow);
}


WNDPROC Win32_Class_Procedure(char const * name)
{
	if (name == NULL) {
		return(NULL);
	}

	auto found = _Classes.find(name);
	return(found == _Classes.end() ? NULL : found->second);
}


float Win32_Pixel_Density(void)
{
	Win32Window * main = Win32_Lookup(_MainWindow);

	if (main == NULL || main->Handle == NULL) {
		return(1.0f);
	}

	float const density = SDL_GetWindowPixelDensity(main->Handle);
	return(density > 0.0f ? density : 1.0f);
}


extern "C" ATOM RegisterClass(WNDCLASS const * cls)
{
	if (cls == NULL || cls->lpszClassName == NULL) {
		return(0);
	}

	_Classes[cls->lpszClassName] = cls->lpfnWndProc;
	return(1);
}


extern "C" HWND CreateWindowEx(DWORD exstyle, LPCSTR classname, LPCSTR windowname, DWORD style,
	int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
	(void)parent;
	(void)menu;
	(void)instance;
	(void)param;

	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		return(NULL);
	}

	Win32Window * window = new Win32Window();
	window->Procedure = Win32_Class_Procedure(classname);
	window->Style = style;
	window->ExStyle = exstyle;
	window->Enabled = true;
	SDL_strlcpy(window->ClassName, classname != NULL ? classname : "", sizeof(window->ClassName));
	SDL_strlcpy(window->Title, windowname != NULL ? windowname : "", sizeof(window->Title));

	// A zero size is what the windowed path asks for before it measures the frame it wants,
	// so the window opens at a size SDL accepts and is moved to the real one afterwards.
	int const openwidth = width > 0 ? width : 640;
	int const openheight = height > 0 ? height : 480;

	SDL_WindowFlags flags = SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if ((style & WS_POPUP) != 0) {
		flags |= SDL_WINDOW_BORDERLESS;
	}

	window->Handle = SDL_CreateWindow(window->Title, openwidth, openheight, flags);

	if (window->Handle == NULL) {
		delete window;
		return(NULL);
	}

	_Windows.push_back(window);

	if (_MainWindow == NULL) {
		_MainWindow = (HWND)window;
	}

	if (window->Procedure != NULL) {
		window->Procedure((HWND)window, WM_CREATE, 0, 0);
	}

	return((HWND)window);
}


extern "C" BOOL DestroyWindow(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL) {
		return(FALSE);
	}

	if (window->Procedure != NULL) {
		window->Procedure(handle, WM_DESTROY, 0, 0);
	}

	if (window->Handle != NULL) {
		SDL_DestroyWindow(window->Handle);
	}

	for (auto it = _Windows.begin(); it != _Windows.end(); ++it) {
		if (*it == window) {
			_Windows.erase(it);
			break;
		}
	}

	if (_MainWindow == handle) {
		_MainWindow = NULL;
	}

	delete window;
	return(TRUE);
}


extern "C" BOOL ShowWindow(HWND handle, int command)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(FALSE);
	}

	bool const previous = window->Visible;

	switch (command) {
		case SW_HIDE:
			SDL_HideWindow(window->Handle);
			window->Visible = false;
			break;

		case SW_MINIMIZE:
		case SW_SHOWMINIMIZED:
			SDL_MinimizeWindow(window->Handle);
			break;

		default:
			SDL_ShowWindow(window->Handle);
			SDL_RaiseWindow(window->Handle);
			window->Visible = true;
			break;
	}

	return(previous ? TRUE : FALSE);
}


extern "C" BOOL ShowWindowAsync(HWND handle, int command)
{
	return(ShowWindow(handle, command));
}


extern "C" BOOL UpdateWindow(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL) {
		return(FALSE);
	}

	if (window->Procedure != NULL) {
		window->Procedure(handle, WM_PAINT, 0, 0);
	}

	return(TRUE);
}


extern "C" BOOL MoveWindow(HWND handle, int x, int y, int width, int height, BOOL repaint)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(FALSE);
	}

	SDL_SetWindowPosition(window->Handle, x, y);
	SDL_SetWindowSize(window->Handle, width, height);

	if (repaint) {
		UpdateWindow(handle);
	}

	return(TRUE);
}


extern "C" BOOL SetWindowPos(HWND handle, HWND after, int x, int y, int cx, int cy, UINT flags)
{
	(void)after;

	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(FALSE);
	}

	if ((flags & SWP_NOMOVE) == 0) {
		SDL_SetWindowPosition(window->Handle, x, y);
	}

	if ((flags & SWP_NOSIZE) == 0) {
		SDL_SetWindowSize(window->Handle, cx, cy);
	}

	return(TRUE);
}


// The frame is measured in physical pixels because the engine scales it to the drawable
// area itself, so the client rectangle reports pixels and every position the layer
// reports elsewhere is converted to match.
extern "C" BOOL GetClientRect(HWND handle, LPRECT rect)
{
	if (rect == NULL) {
		return(FALSE);
	}

	rect->left = 0;
	rect->top = 0;
	rect->right = 0;
	rect->bottom = 0;

	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(FALSE);
	}

	int width = 0;
	int height = 0;
	if (!SDL_GetWindowSizeInPixels(window->Handle, &width, &height)) {
		return(FALSE);
	}

	rect->right = width;
	rect->bottom = height;
	return(TRUE);
}


extern "C" BOOL GetWindowRect(HWND handle, LPRECT rect)
{
	if (rect == NULL) {
		return(FALSE);
	}

	rect->left = 0;
	rect->top = 0;
	rect->right = 0;
	rect->bottom = 0;

	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(FALSE);
	}

	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	SDL_GetWindowPosition(window->Handle, &x, &y);
	SDL_GetWindowSizeInPixels(window->Handle, &width, &height);

	float const density = Win32_Pixel_Density();
	rect->left = (LONG)(x * density);
	rect->top = (LONG)(y * density);
	rect->right = rect->left + width;
	rect->bottom = rect->top + height;
	return(TRUE);
}


extern "C" BOOL ClientToScreen(HWND handle, LPPOINT point)
{
	if (point == NULL) {
		return(FALSE);
	}

	RECT rect;
	if (!GetWindowRect(handle, &rect)) {
		return(FALSE);
	}

	point->x += rect.left;
	point->y += rect.top;
	return(TRUE);
}


extern "C" BOOL ScreenToClient(HWND handle, LPPOINT point)
{
	if (point == NULL) {
		return(FALSE);
	}

	RECT rect;
	if (!GetWindowRect(handle, &rect)) {
		return(FALSE);
	}

	point->x -= rect.left;
	point->y -= rect.top;
	return(TRUE);
}


extern "C" int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count)
{
	for (UINT index = 0; index < count; index++) {
		if (from != NULL) ClientToScreen(from, &points[index]);
		if (to != NULL) ScreenToClient(to, &points[index]);
	}

	return(0);
}


// The window is borderless or resizable but never has a client area smaller than the
// frame, so the adjustment the engine asks for is the identity.
extern "C" BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD exstyle)
{
	(void)style;
	(void)menu;
	(void)exstyle;
	return(rect != NULL);
}


extern "C" LONG_PTR GetWindowLongPtr(HWND handle, int index)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL) {
		return(0);
	}

	switch (index) {
		case GWL_STYLE: return((LONG_PTR)window->Style);
		case GWL_EXSTYLE: return((LONG_PTR)window->ExStyle);
		case GWLP_WNDPROC: return((LONG_PTR)window->Procedure);
		default: return(0);
	}
}


extern "C" LONG_PTR SetWindowLongPtr(HWND handle, int index, LONG_PTR value)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL) {
		return(0);
	}

	LONG_PTR const previous = GetWindowLongPtr(handle, index);

	switch (index) {
		case GWL_STYLE: window->Style = (DWORD)value; break;
		case GWL_EXSTYLE: window->ExStyle = (DWORD)value; break;
		case GWLP_WNDPROC: window->Procedure = (WNDPROC)value; break;
		default: break;
	}

	return(previous);
}


extern "C" LONG_PTR GetWindowLong(HWND handle, int index)
{
	return(GetWindowLongPtr(handle, index));
}


extern "C" LONG_PTR SetWindowLong(HWND handle, int index, LONG_PTR value)
{
	return(SetWindowLongPtr(handle, index, value));
}


extern "C" BOOL SetWindowText(HWND handle, LPCSTR text)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || text == NULL) {
		return(FALSE);
	}

	SDL_strlcpy(window->Title, text, sizeof(window->Title));

	if (window->Handle != NULL) {
		SDL_SetWindowTitle(window->Handle, window->Title);
	}

	return(TRUE);
}


extern "C" int GetWindowText(HWND handle, LPSTR text, int max)
{
	if (text == NULL || max <= 0) {
		return(0);
	}

	text[0] = '\0';

	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL) {
		return(0);
	}

	SDL_strlcpy(text, window->Title, (size_t)max);
	return((int)strlen(text));
}


extern "C" int GetWindowTextLength(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);
	return(window == NULL ? 0 : (int)strlen(window->Title));
}


extern "C" int GetClassName(HWND handle, LPSTR name, int max)
{
	if (name == NULL || max <= 0) {
		return(0);
	}

	name[0] = '\0';

	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL) {
		return(0);
	}

	SDL_strlcpy(name, window->ClassName, (size_t)max);
	return((int)strlen(name));
}


extern "C" BOOL IsWindow(HWND handle)
{
	return(Win32_Lookup(handle) != NULL);
}


extern "C" BOOL IsWindowVisible(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);
	return(window != NULL && window->Visible);
}


extern "C" BOOL IsWindowEnabled(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);
	return(window != NULL && window->Enabled);
}


extern "C" BOOL IsChild(HWND parent, HWND child)
{
	(void)parent;
	(void)child;
	return(FALSE);
}


extern "C" BOOL EnableWindow(HWND handle, BOOL enable)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL) {
		return(FALSE);
	}

	BOOL const previous = window->Enabled ? FALSE : TRUE;
	window->Enabled = enable != FALSE;
	return(previous);
}


extern "C" HWND GetParent(HWND handle) { (void)handle; return(NULL); }
extern "C" HWND GetWindow(HWND handle, UINT command) { (void)handle; (void)command; return(NULL); }
extern "C" HWND GetTopWindow(HWND handle) { (void)handle; return(NULL); }
extern "C" HWND GetDesktopWindow(void) { return(NULL); }
extern "C" HWND GetActiveWindow(void) { return(_MainWindow); }
extern "C" HWND SetActiveWindow(HWND handle) { (void)handle; return(_MainWindow); }
extern "C" HWND GetFocus(void) { return(_MainWindow); }
extern "C" HWND SetFocus(HWND handle) { (void)handle; return(_MainWindow); }
extern "C" HWND WindowFromPoint(POINT point) { (void)point; return(_MainWindow); }
extern "C" HWND ChildWindowFromPoint(HWND parent, POINT point) { (void)parent; (void)point; return(NULL); }
extern "C" BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM param) { (void)parent; (void)proc; (void)param; return(TRUE); }
extern "C" HMENU GetMenu(HWND handle) { (void)handle; return(NULL); }
extern "C" HMENU GetSystemMenu(HWND handle, BOOL revert) { (void)handle; (void)revert; return(NULL); }
extern "C" BOOL EnableMenuItem(HMENU menu, UINT item, UINT enable) { (void)menu; (void)item; (void)enable; return(FALSE); }
extern "C" BOOL DeleteMenu(HMENU menu, UINT position, UINT flags) { (void)menu; (void)position; (void)flags; return(FALSE); }
extern "C" BOOL RegisterHotKey(HWND handle, int id, UINT modifiers, UINT key) { (void)handle; (void)id; (void)modifiers; (void)key; return(FALSE); }
extern "C" int GetWindowContextHelpId(HWND handle) { (void)handle; return(0); }
extern "C" BOOL WinHelp(HWND handle, LPCSTR help, UINT command, ULONG_PTR data) { (void)handle; (void)help; (void)command; (void)data; return(FALSE); }


extern "C" BOOL SetForegroundWindow(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(FALSE);
	}

	SDL_RaiseWindow(window->Handle);
	return(TRUE);
}


extern "C" BOOL BringWindowToTop(HWND handle)
{
	return(SetForegroundWindow(handle));
}


extern "C" HWND FindWindow(LPCSTR classname, LPCSTR windowname)
{
	(void)windowname;

	for (Win32Window * candidate : _Windows) {
		if (classname == NULL || strcmp(candidate->ClassName, classname) == 0) {
			return((HWND)candidate);
		}
	}

	return(NULL);
}


extern "C" BOOL CloseWindow(HWND handle)
{
	return(ShowWindow(handle, SW_MINIMIZE));
}


// The frame is presented every time it changes rather than in answer to a paint request,
// so an invalidation has nothing to record and an update rectangle is always empty.
extern "C" BOOL InvalidateRect(HWND handle, RECT const * rect, BOOL erase) { (void)handle; (void)rect; (void)erase; return(TRUE); }
extern "C" BOOL ValidateRect(HWND handle, RECT const * rect) { (void)handle; (void)rect; return(TRUE); }
extern "C" BOOL RedrawWindow(HWND handle, RECT const * rect, HRGN region, UINT flags) { (void)handle; (void)rect; (void)region; (void)flags; return(TRUE); }


extern "C" BOOL GetUpdateRect(HWND handle, LPRECT rect, BOOL erase)
{
	(void)handle;
	(void)erase;

	if (rect != NULL) {
		rect->left = rect->top = rect->right = rect->bottom = 0;
	}

	return(FALSE);
}


extern "C" BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom)
{
	if (rect == NULL) {
		return(FALSE);
	}

	rect->left = left;
	rect->top = top;
	rect->right = right;
	rect->bottom = bottom;
	return(TRUE);
}


extern "C" BOOL IntersectRect(LPRECT dest, RECT const * a, RECT const * b)
{
	if (dest == NULL || a == NULL || b == NULL) {
		return(FALSE);
	}

	dest->left = a->left > b->left ? a->left : b->left;
	dest->top = a->top > b->top ? a->top : b->top;
	dest->right = a->right < b->right ? a->right : b->right;
	dest->bottom = a->bottom < b->bottom ? a->bottom : b->bottom;

	if (dest->right <= dest->left || dest->bottom <= dest->top) {
		dest->left = dest->top = dest->right = dest->bottom = 0;
		return(FALSE);
	}

	return(TRUE);
}


extern "C" BOOL PtInRect(RECT const * rect, POINT point)
{
	if (rect == NULL) {
		return(FALSE);
	}

	return(point.x >= rect->left && point.x < rect->right && point.y >= rect->top && point.y < rect->bottom);
}


extern "C" int GetSystemMetrics(int index)
{
	SDL_DisplayID const display = SDL_GetPrimaryDisplay();
	SDL_DisplayMode const * mode = SDL_GetDesktopDisplayMode(display);
	float const density = Win32_Pixel_Density();

	switch (index) {
		case SM_CXSCREEN:
		case SM_CXFULLSCREEN:
			return(mode != NULL ? (int)(mode->w * density) : 640);

		case SM_CYSCREEN:
		case SM_CYFULLSCREEN:
			return(mode != NULL ? (int)(mode->h * density) : 480);

		case SM_CXBORDER:
		case SM_CYBORDER:
			return(1);

		case SM_CXDRAG:
		case SM_CYDRAG:
			return(4);

		case SM_SWAPBUTTON:
			return(0);

		default:
			return(0);
	}
}


extern "C" HMONITOR MonitorFromWindow(HWND handle, DWORD flags)
{
	(void)handle;
	(void)flags;
	return((HMONITOR)(ULONG_PTR)SDL_GetPrimaryDisplay());
}


extern "C" BOOL GetMonitorInfo(HMONITOR monitor, LPMONITORINFO info)
{
	(void)monitor;

	if (info == NULL) {
		return(FALSE);
	}

	SetRect(&info->rcMonitor, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
	info->rcWork = info->rcMonitor;
	info->dwFlags = 0;
	return(TRUE);
}


extern "C" BOOL EnumDisplaySettings(LPCSTR device, DWORD mode, DEVMODE * settings)
{
	(void)device;
	(void)mode;
	(void)settings;
	return(FALSE);
}


// A message box has no native equivalent that can be shown from inside the engine's own
// loop without a second event source, so the text is reported where a headless run sees
// it and the caller is told the default button was chosen.
extern "C" int MessageBox(HWND handle, LPCSTR text, LPCSTR caption, UINT type)
{
	(void)handle;

	SDL_Log("%s: %s", caption != NULL ? caption : "OpenTS", text != NULL ? text : "");

	if ((type & MB_YESNO) == MB_YESNO) {
		return(IDYES);
	}

	return(IDOK);
}


extern "C" int MessageBoxIndirect(MSGBOXPARAMS const * params)
{
	if (params == NULL) {
		return(IDOK);
	}

	return(MessageBox(params->hwndOwner, params->lpszText, params->lpszCaption, params->dwStyle));
}


extern "C" void * Win32Compat_Native_Window_Handle(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(NULL);
	}

	if (_MetalView == NULL) {
		_MetalView = SDL_Metal_CreateView(window->Handle);
	}

	if (_MetalView == NULL) {
		return(NULL);
	}

	return(SDL_Metal_GetLayer(_MetalView));
}


extern "C" int Win32Compat_Window_Refresh_Rate(HWND handle)
{
	Win32Window * window = Win32_Lookup(handle);

	if (window == NULL || window->Handle == NULL) {
		return(0);
	}

	SDL_DisplayMode const * mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window->Handle));
	return(mode != NULL ? (int)(mode->refresh_rate + 0.5f) : 0);
}
