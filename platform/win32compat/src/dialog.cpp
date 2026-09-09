/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#include <cstring>

// Every dialog in the tree is a Win32 resource template driven by OwnerDraw, and
// docs/UI_DESIGN.md step 13 replaces that layer rather than porting it. Creating a dialog
// therefore fails here, and each driver takes the path it already has for a dialog that
// could not be created. Nothing below draws or measures anything: a control that was
// never created answers no message.

extern "C" HWND CreateDialogIndirectParam(HINSTANCE instance, LPCDLGTEMPLATE templ, HWND parent,
	DLGPROC proc, LPARAM param)
{
	(void)instance;
	(void)templ;
	(void)parent;
	(void)proc;
	(void)param;
	return(NULL);
}


extern "C" HWND CreateDialogParam(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param)
{
	(void)instance;
	(void)name;
	(void)parent;
	(void)proc;
	(void)param;
	return(NULL);
}


extern "C" INT_PTR DialogBoxParam(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param)
{
	(void)instance;
	(void)name;
	(void)parent;
	(void)proc;
	(void)param;
	return(IDCANCEL);
}


extern "C" BOOL EndDialog(HWND dialog, INT_PTR result) { (void)dialog; (void)result; return(TRUE); }
extern "C" BOOL IsDialogMessage(HWND dialog, LPMSG msg) { (void)dialog; (void)msg; return(FALSE); }
extern "C" HWND GetDlgItem(HWND dialog, int id) { (void)dialog; (void)id; return(NULL); }
extern "C" int GetDlgCtrlID(HWND control) { (void)control; return(0); }
extern "C" HWND GetNextDlgTabItem(HWND dialog, HWND control, BOOL previous) { (void)dialog; (void)control; (void)previous; return(NULL); }
extern "C" BOOL SetDlgItemText(HWND dialog, int id, LPCSTR text) { (void)dialog; (void)id; (void)text; return(FALSE); }
extern "C" BOOL CheckDlgButton(HWND dialog, int id, UINT check) { (void)dialog; (void)id; (void)check; return(FALSE); }
extern "C" UINT IsDlgButtonChecked(HWND dialog, int id) { (void)dialog; (void)id; return(BST_UNCHECKED); }


extern "C" UINT GetDlgItemText(HWND dialog, int id, LPSTR text, int max)
{
	(void)dialog;
	(void)id;

	if (text != NULL && max > 0) {
		text[0] = '\0';
	}

	return(0);
}


extern "C" LRESULT SendDlgItemMessage(HWND dialog, int id, UINT message, WPARAM wparam, LPARAM lparam)
{
	return(SendMessage(GetDlgItem(dialog, id), message, wparam, lparam));
}


extern "C" void InitCommonControls(void) {}
extern "C" BOOL ImageList_BeginDrag(HIMAGELIST list, int image, int x, int y) { (void)list; (void)image; (void)x; (void)y; return(FALSE); }
extern "C" BOOL ImageList_DragEnter(HWND lock, int x, int y) { (void)lock; (void)x; (void)y; return(FALSE); }
extern "C" BOOL ImageList_DragMove(int x, int y) { (void)x; (void)y; return(FALSE); }
extern "C" BOOL ImageList_DragShowNolock(BOOL show) { (void)show; return(FALSE); }
extern "C" void ImageList_EndDrag(void) {}
extern "C" BOOL ImageList_Destroy(HIMAGELIST list) { (void)list; return(FALSE); }

