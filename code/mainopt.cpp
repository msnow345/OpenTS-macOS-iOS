/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "mainopt.h"

#include "_map.h"
#include "_mixfile.h"
#include "_rect.h"
#include "_surface.h"
#include "convert.h"
#include "data.h"
#include "dbgprint.h"
#include "audio/audioengine.h"
#include "dsurface.h"
#include "gamedlg.h"
#include "globals.h"
#include "init.h"
#include "language/language.h"
#include "misc.h"
#include "video.h"
#include "mixfile.h"
#include "msgbox.h"
#include "newmenu.h"
#include "ownrdraw.h"
#include "sidebar.h"
#include "sounddlg.h"
#include "stimer.h"
#include "surface.h"
#include "wwmouse.h"
#include "ui/uidisplayconfirm.h"
#include "ui/uidisplayoptions.h"
#include "ui/uimainoptions.h"

#include "color.hh"


INT_PTR CALLBACK Main_Options_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
INT_PTR CALLBACK Display_Options_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
bool Change_Display_Mode(int width, int height);
bool Test_Display_Mode_Dialog(int width, int height);
INT_PTR CALLBACK Test_Display_Mode_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);


// The screens the dialog procedures read and write. A driver owns one for the whole life of
// its dialog, which is the lifetime DWLP_USER gave the result pointer each replaces.
static UIMainOptionsPresenterClass * _MainScreen = NULL;
static UIDisplayOptionsPresenterClass * _DisplayScreen = NULL;
static UIDisplayConfirmPresenterClass * _ConfirmScreen = NULL;


static void Options_Queue(UIPresenterClass & screen, char const * action, int value = 0)
{
	UIIntent intent;
	intent.Action = action;
	intent.Value = value;
	screen.Queue(intent);
}


/// <summary>
/// Brings up the main options dialog.
/// This routine drives the options menu, dispatching to the sound, display, network,
/// keyboard and game settings dialogs until the player backs out. A resolution change is
/// offered as a trial first, and the settings are written out when the player leaves.
/// </summary>
/// <remarks>Game logic is suspended for the duration of this routine.</remarks>
void Main_Options_Dialog(void)
{
	UIMainOptionsPresenterClass screen;
	screen.Begin();
	screen.Refresh();

	_MainScreen = &screen;

	while (true) {
		screen.Result.reset();
		screen.Choice = UIMainOptionsPresenterClass::CHOICE_NONE;

		HWND main_handle;
		do {
			main_handle = OwnerDraw::Begin_Dialog(IDD_OPT_MAIN, Main_Options_Dialog_Proc);
		} while (main_handle == 0);

		OwnerDraw::Move_Dialog(main_handle, -1, (HiddenSurface->Get_Height() - 400) / 2 + 147);
		OwnerDraw::Display_Dialog(main_handle);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				// A session that ended underneath the screen leaves the family, which is
				// what the driver's own unanswered result did on the way to its default arm.
				UIResult ended;
				ended.Outcome = UIResult::OUTCOME_SESSION_ENDED;
				ended.GameEnded = true;
				screen.Choice = UIMainOptionsPresenterClass::CHOICE_EXIT;
				screen.Result = ended;
				break;
			}

			screen.Drain();
			screen.Service();
		}

		OwnerDraw::End_Dialog(main_handle);

		if (screen.Exits()) {
			break;
		}

		// The sub-screen runs with this one destroyed, which is the coexistence rule the
		// driver already kept.
		screen.Run_Pending();
	}

	_MainScreen = NULL;

	screen.End();
}


/// <summary>
/// Brings up the display options and offers a chosen resolution as a trial.
/// A mode the player refuses, or does not answer for, brings the screen straight back up
/// with the old resolution in force; anything else leaves.
/// </summary>
void Display_Options_Dialog(void)
{
	while (true) {
		UIDisplayOptionsPresenterClass screen;
		screen.Refresh();

		_DisplayScreen = &screen;

		HWND handle;
		do {
			handle = OwnerDraw::Begin_Dialog(IDD_OPT_DISPLAY, Display_Options_Dialog_Proc);
		} while (handle == 0);
		OwnerDraw::Display_Dialog(handle);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				UIResult ended;
				ended.Outcome = UIResult::OUTCOME_SESSION_ENDED;
				ended.GameEnded = true;
				screen.Result = ended;
				break;
			}

			screen.Drain();
			screen.Service();
		}

		OwnerDraw::End_Dialog(handle);
		_DisplayScreen = NULL;

		if (screen.Choice != UIDisplayOptionsPresenterClass::CHOICE_ACCEPT) {
			break;
		}
		if (!screen.Wants_Mode_Change()) {
			break;
		}

		if (WWMessageBox().Process(TXT_ABOUT_TO_TRY_MODE, TXT_OK, TXT_CANCEL) == 0) {
			if (!Test_Display_Mode_Dialog(screen.StagedWidth, screen.StagedHeight)) {
				continue;
			}
			screen.Commit();
		}

		break;
	}
}


/// <summary>
/// Handles the main options dialog.
/// The procedure queues what the player pressed for the driver to execute after the pump,
/// and disables the sound button when there is no audio hardware to talk to.
/// </summary>
INT_PTR CALLBACK Main_Options_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	HWND handle;

	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);
	if (rc == 0) {
		if (_MainScreen == NULL) {
			return(0);
		}

		UIMainOptionsPresenterClass & screen = *_MainScreen;

		switch (message) {

			case WM_COMMAND:
				switch (LOWORD(wparam)) {
					case IDC_OPTMAIN_SOUND:
						Options_Queue(screen, UI_MAINOPT_SOUND);
						break;

					case IDC_OPTMAIN_DISPLAY:
						Options_Queue(screen, UI_MAINOPT_DISPLAY);
						break;

					case IDC_OPTMAIN_KEYBOARD:
						Options_Queue(screen, UI_MAINOPT_KEYBOARD);
						break;

					case IDC_OPTMAIN_GAME_SETTINGS:
						Options_Queue(screen, UI_MAINOPT_SETTINGS);
						break;

					default:
						Options_Queue(screen, UI_MAINOPT_EXIT);
						break;
				}
				break;

			case WM_INITDIALOG:
				handle = GetDlgItem(window, IDC_OPTMAIN_SOUND);
				if (handle) {
					EnableWindow(handle, screen.SoundAvailable);
				}
				break;

		}
		return(0);
	}
	return(rc);
}


/// <summary>
/// Switches the game over to a new render resolution.
/// Every drawing surface is destroyed and recreated at the new size, so any pointer held
/// across this call is stale.
/// </summary>
/// <param name="width">The width to render at.</param>
/// <param name="height">The height to render at.</param>
/// <returns>bool; Was the mode changed? If not, nothing has been disturbed.</returns>
bool Change_Display_Mode(int width, int height)
{
	DebugString("About to set video mode\n");

	Hide_Mouse();

	if (!Video_Set_Mode(width, height)) {
		DebugString("Video_Set_Mode failed.\n");
		Show_Mouse();
		return(false);
		}

	VisibleRect = Rect(0, 0, width, height);
	DebugString("VisibleRect: %dx%d\n", width, height);

	if (VisibleSurface != NULL) {
		delete VisibleSurface;
		VisibleSurface = NULL;
	}

	if (AlternateSurface != NULL) {
		delete AlternateSurface;
		AlternateSurface = NULL;
	}

	if (HiddenSurface != NULL) {
		delete HiddenSurface;
		HiddenSurface = NULL;
	}

	if (TileSurface != NULL) {
		delete TileSurface;
		TileSurface = NULL;
	}

	if (SidebarSurface != NULL) {
		delete SidebarSurface;
		SidebarSurface = NULL;
	}

	if (CompositeSurface != NULL) {
		delete CompositeSurface;
		CompositeSurface = NULL;
	}

	VisibleSurface = DSurface::Create_Primary();
	if (VisibleSurface == NULL) {
		Show_Mouse();
		return(false);
	}

	/*
	 * A window that is tracking the frame follows it to the new size. One the player
	 * sized themselves, and a window covering the screen, both stay as they are and the
	 * frame is scaled into them instead.
	 */
	if (WindowedMode && Options.WindowWidth <= 0 && Options.WindowHeight <= 0) {
		RECT windowrect;
		SetRect(&windowrect, 0, 0, width, height);
		AdjustWindowRectEx(&windowrect, GetWindowLong(MainWindow, GWL_STYLE), FALSE, GetWindowLong(MainWindow, GWL_EXSTYLE));

		int newwidth = windowrect.right - windowrect.left;
		int newheight = windowrect.bottom - windowrect.top;

		/*
		 * The window grows about its middle rather than its corner, so the picture stays
		 * where the player was looking.
		 */
		RECT current;
		GetWindowRect(MainWindow, &current);
		int x = current.left + (((current.right - current.left) - newwidth) / 2);
		int y = current.top + (((current.bottom - current.top) - newheight) / 2);

		/*
		 * Growing about the middle can push the window past the edges of the screen, and a
		 * title bar above the top of it cannot be grabbed to bring the window back.
		 */
		MONITORINFO monitor;
		monitor.cbSize = sizeof(monitor);
		if (GetMonitorInfo(MonitorFromWindow(MainWindow, MONITOR_DEFAULTTONEAREST), &monitor)) {
			if (x + newwidth > monitor.rcWork.right) x = monitor.rcWork.right - newwidth;
			if (y + newheight > monitor.rcWork.bottom) y = monitor.rcWork.bottom - newheight;
			if (x < monitor.rcWork.left) x = monitor.rcWork.left;
			if (y < monitor.rcWork.top) y = monitor.rcWork.top;
		}

		SetWindowPos(MainWindow, NULL, x, y, newwidth, newheight, SWP_NOZORDER);
	}

	Rect temp = VisibleRect;
	temp.X = ((Options.IsSidebarOnRight || Debug_Map) ? 0 : SidebarClass::SIDE_WIDTH);
	temp.Y = 16;
	temp.Width -= SidebarClass::SIDE_WIDTH;
	temp.Height -= 16;

	Allocate_Surfaces(VisibleRect, Rect(0, 0, temp.Width, VisibleRect.Height), Rect(0, 0, temp.Width, VisibleRect.Height), Rect(0, 0, SidebarClass::SIDE_WIDTH, VisibleRect.Height));
	LogicalSurface = HiddenSurface;

	if (MouseCursor != NULL) {
		((WWMouseClass*)MouseCursor)->Calc_Confining_Rect();
	}

	Map.Set_View_Dimensions(temp);

	Map.Init_IO();
	Map.Activate(
#ifdef _DEBUG
		Debug_Map == true ? 1 : 0
#else
		1
#endif
	);
	Map.Reposition_Sidebar();
	Map.Flag_To_Redraw(GS_REDRAW_ALL);
	Show_Mouse();

	DebugString("Mode change complete.\n");

	return(true);
}


/// <summary>
/// Tries a display mode out and asks the player to confirm it.
/// This routine switches to the requested mode and puts up a confirmation dialog. If the
/// player does not accept the mode -- or says nothing at all, because a bad mode may well
/// leave the screen unreadable -- the previous resolution is restored.
/// </summary>
/// <param name="width">The width of the display mode to try.</param>
/// <param name="height">The height of the display mode to try.</param>
/// <returns>bool; Was the new display mode accepted and left in place?</returns>
bool Test_Display_Mode_Dialog(int width, int height)
{
	DebugString("Testing display mode @ %dx%d\n", width, height);
	Hide_Mouse();
	HiddenSurface->Fill(TBLACK);
	Update_Visible_Surface();

	if (!Change_Display_Mode(width, height)) {
		return(false);
	}

	HiddenSurface->Fill(TBLACK);
	Update_Visible_Surface();
	Show_Mouse();
	Draw_Menu_Background();

	UIDisplayConfirmPresenterClass screen;
	screen.Refresh();

	_ConfirmScreen = &screen;

	bool accepted = true;

	HWND dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CONFIRM_MODE, Test_Display_Mode_Dialog_Proc);
	if (dialog) {
		OwnerDraw::Display_Dialog(dialog);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				break;
			}

			screen.Drain();
			screen.Service();
		}

		OwnerDraw::End_Dialog(dialog);

		accepted = (screen.Choice == UIDisplayConfirmPresenterClass::CHOICE_ACCEPT);
	}

	_ConfirmScreen = NULL;

	if (!accepted) {
		DebugString("Resetting display mode @ %dx%d\n", Options.ScreenWidth, Options.ScreenHeight);
		Change_Display_Mode(Options.ScreenWidth, Options.ScreenHeight);
		LogicalSurface = HiddenSurface;
		return(false);
	}

	DebugString("Keeping display mode @ %dx%d\n", width, height);
	LogicalSurface = HiddenSurface;
	return(true);
}


/// <summary>
/// Handles the mode confirmation dialog.
/// The procedure queues what the player pressed. Anything that is not the accept button is
/// a refusal, which is what the driver's test against IDOK made of every other identifier
/// the dialog could produce.
/// </summary>
INT_PTR CALLBACK Test_Display_Mode_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);
	if (rc == 0) {
		if (_ConfirmScreen == NULL) {
			return(0);
		}

		switch (message) {
			case WM_COMMAND: {
				int const id = LOWORD(wparam);
				if (id > 0 && id <= IDCANCEL) {
					Options_Queue(*_ConfirmScreen, (id == IDOK) ? UI_MODECONFIRM_ACCEPT : UI_MODECONFIRM_CANCEL);
				}
				break;
			}
		}
		return(0);
	}
	return(rc);
}


/// <summary>
/// Handles the display options dialog messages.
/// The procedure fills the resolution list from the view-model, queues the row and the
/// movie stretching preference the player left it on, and hands the driver what the player
/// pressed to execute after the pump.
/// </summary>
static __forceinline BOOL Display_Options_Dialog_Body(HWND window, UINT message, WPARAM wparam)
{
	if (_DisplayScreen == NULL) {
		return(0);
	}

	UIDisplayOptionsPresenterClass & screen = *_DisplayScreen;

	switch (message) {
		case WM_COMMAND:
			switch (LOWORD(wparam)) {
				default:
					return(0);

				case IDC_DISPLAY_RESLIST: {
					HWND list = GetDlgItem(window, IDC_DISPLAY_RESLIST);
					if (list) {
						Options_Queue(screen, UI_DISPLAY_SELECT, ListBox_GetCurSel(list));
					}
				}
				return(0);

				case IDOK: {
					HWND list = GetDlgItem(window, IDC_DISPLAY_RESLIST);
					if (list) {
						Center_Window_Within_Window(window, MainWindow);
						Options_Queue(screen, UI_DISPLAY_SELECT, ListBox_GetCurSel(list));
					}
					HWND button = GetDlgItem(window, IDC_STRETCH_MOVIES);
					if (button) {
						Options_Queue(screen, UI_DISPLAY_STRETCH, Button_GetCheck(button) == BST_CHECKED ? 1 : 0);
					}
					Options_Queue(screen, UI_DISPLAY_ACCEPT);
				}
				break;

				case IDCANCEL:
					Options_Queue(screen, UI_DISPLAY_CANCEL);
					break;
			}
			break;

		case WM_INITDIALOG: {
			HWND list = GetDlgItem(window, IDC_DISPLAY_RESLIST);
			if (list) {
				for (UIDisplayOptionsPresenterClass::ModeType const & mode : screen.Modes) {
					int const index = ListBox_AddString(list, mode.Label.c_str());
					ListBox_SetItemData(list, index, index);
				}
				ListBox_SetCurSel(list, screen.Selected);
			}

			HWND button = GetDlgItem(window, IDC_STRETCH_MOVIES);
			if (button) {
				Button_SetCheck(button, screen.StretchMovies != false);
			}
		}
		break;

	}
	return(0);
}


/// <summary>
/// Handles the display options dialog.
/// This routine gives the owner draw dialog system first refusal on the message and only
/// deals with what it leaves behind.
/// </summary>
INT_PTR CALLBACK Display_Options_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);
	if (rc == 0) {
		return(Display_Options_Dialog_Body(window, message, wparam));
	}
	return(rc);
}
