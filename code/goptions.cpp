/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2025 Electronic Arts Inc.
 * Copyright 2026 OpenTS contributors
 *
 * Contains material derived from Electronic Arts source code.
 * Modified by OpenTS contributors, 2026.
 * EA's GPLv3 Section 7 additional terms and supplemental warranty
 * disclaimers apply; see LICENSE.md.
 ******************************************************************************/

/* $Header: /counterstrike/GOPTIONS.CPP 6     3/15/97 7:18p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : OPTIONS.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : June 8, 1994                                                 *
 *                                                                                             *
 *                  Last Update : July 27, 1995 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   OptionsClass::Process -- Handles all the options graphic interface.                       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include "goptions.h"

#include "_keyboar.h"
#include "_map.h"
#include "data.h"
#include "dbgprint.h"
#include "gamedlg.h"
#include "language/language.h"
#include "loaddlg.h"
#include "ownrdraw.h"
#include "queue.h"
#include "restate.h"
#include "savemgr.h"
#include "scenario.h"
#include "stats.h"
#include "ui/uigameoptions.h"

#include "special.hh"

void Game_Options_On_INITDIALOG(HWND window, UIGameOptionsPresenterClass const & screen);
INT_PTR CALLBACK Game_Options_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
INT_PTR CALLBACK Abort_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
void Abort_Dialog_On_COMMAND(HWND window, UINT message, WPARAM wparam, LPARAM lparam);


// The screen the dialog procedure reads and writes. The driver owns it for the whole life
// of the dialog, which is the same lifetime DWLP_USER gave the result pointer it replaces.
static UIGameOptionsPresenterClass * _Screen = NULL;


static void Game_Options_Queue(UIGameOptionsPresenterClass & screen, char const * action, int value = 0)
{
	UIIntent intent;
	intent.Action = action;
	intent.Value = value;
	screen.Queue(intent);
}


/// <summary>
/// Puts the view-model's enabled states into the controls the dialog enabled by hand.
/// A control the template left alone for a session type is left alone here too, so styling
/// adds no restriction the dialog did not have.
/// </summary>
static void Game_Options_Sync_Controls(HWND window, UIGameOptionsPresenterClass const & screen)
{
	HWND handle;

	if (!screen.IsMultiplayer) {
		handle = GetDlgItem(window, IDC_LOAD_GAME);
		if (handle) {
			EnableWindow(handle, screen.CanLoad);
		}

		handle = GetDlgItem(window, IDC_DELETE_GAME);
		if (handle) {
			EnableWindow(handle, screen.CanDelete);
		}
	} else {
		handle = GetDlgItem(window, IDC_SAVE_GAME);
		if (handle) {
			EnableWindow(handle, screen.CanSave);
		}

		handle = GetDlgItem(window, IDC_LOAD_GAME);
		if (handle) {
			EnableWindow(handle, screen.CanLoad);
		}
	}

	if (!screen.CanBrief) {
		handle = GetDlgItem(window, IDC_BRIEFING);
		if (handle) {
			EnableWindow(handle, FALSE);
		}
	}
}

/// <summary>
/// Displays the in game options dialog.
/// This routine is used by the special dialog handler when the player calls up the options
/// screen. Which dialog appears depends on the kind of game in progress. Game input stays
/// locked out for as long as the dialog is up, and if the player asked for the mission
/// briefing it is restated on the way out.
/// </summary>
void Game_Options_Dialog(void)
{
	UIGameOptionsPresenterClass screen;
	screen.Refresh();

	_Screen = &screen;

	HWND dialog;
	if (Session.Type == GAME_NORMAL || Session.Type == GAME_SKIRMISH) {
		dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_SP, Game_Options_Dialog_Proc);
	} else if (Session.Type == GAME_INTERNET) {
		dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_WOL, Game_Options_Dialog_Proc);
	} else {
		dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_MP, Game_Options_Dialog_Proc);
	}

	IgnoreInput = true;
	Keyboard->Clear();

	if (dialog) {

		OwnerDraw::Display_Dialog(dialog);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				// A session that ended underneath the screen leaves it as though the player
				// had resumed, which is the IDOK the driver used to write.
				UIResult ended;
				ended.Outcome = UIResult::OUTCOME_SESSION_ENDED;
				ended.GameEnded = true;
				screen.Choice = UIGameOptionsPresenterClass::CHOICE_RESUME;
				screen.Result = ended;
				break;
			}

			// A control handler queues rather than acts, so the queue is executed here,
			// after the pump has returned.
			screen.Drain();

			// Getting out of the way of a screen this one opens is the view's work; what
			// running it means is the presenter's.
			if (screen.Pending != UIGameOptionsPresenterClass::SUB_NONE) {
				ShowWindow(dialog, SW_HIDE);
				UpdateWindow(MainWindow);
				screen.Run_Pending();
				if (!screen.Result.has_value()) {
					ShowWindow(dialog, SW_SHOW);
					UpdateWindow(dialog);
				}
			}

			Game_Options_Sync_Controls(dialog, screen);
		}

		OwnerDraw::End_Dialog(dialog);
	}

	_Screen = nullptr;

	Keyboard->Clear();

	if (screen.Choice == UIGameOptionsPresenterClass::CHOICE_BRIEFING) {
		Restate_Mission(Scen);
	}

	IgnoreInput = Scen->IsInputLocked;

	if (screen.Choice == UIGameOptionsPresenterClass::CHOICE_LOADED) {
		if (MouseCursor->Is_Hidden() == false && Scen->IsInputLocked == 1) {
			Hide_Mouse();
		} else if (MouseCursor->Is_Hidden() == true && Scen->IsInputLocked == 0) {
			Show_Mouse();
		}
	}

	Map.Flag_To_Redraw(GS_REDRAW_ALL);
}


/// <summary>
/// Handles messages for the in game options dialog.
/// The procedure reads the view-model and queues what the player asked for; the driver
/// executes the queue after the pump returns, as docs/UI_DESIGN.md requires of every
/// screen. Dragging the game speed or connection quality slider updates the label beside it,
/// which is the view's own business.
/// </summary>
/// <returns>Returns with TRUE if the owner draw system consumed the message.</returns>
INT_PTR CALLBACK Game_Options_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);

	if (rc) {
		return(rc);
	}

	// The driver owns the screen for the whole life of the dialog, so a message that arrives
	// without one has nothing to act on.
	if (_Screen == nullptr) {
		return(FALSE);
	}

	UIGameOptionsPresenterClass & screen = *_Screen;
	HWND handle;

	switch (message) {

		case WM_INITDIALOG:
			Game_Options_On_INITDIALOG(window, screen);
			break;

		case WM_COMMAND: {
			int code = HIWORD(wparam);

			switch (LOWORD(wparam)) {

				case IDC_SAVE_GAME:
					if (!code) Game_Options_Queue(screen, UI_GAMEOPT_SAVE);
					break;

				case IDC_LOAD_GAME:
					if (!code) Game_Options_Queue(screen, UI_GAMEOPT_LOAD);
					break;

				case IDC_BRIEFING:
					if (!code) Game_Options_Queue(screen, UI_GAMEOPT_BRIEFING);
					break;

				case IDC_DELETE_GAME:
					if (!code) Game_Options_Queue(screen, UI_GAMEOPT_DELETE);
					break;

				case IDC_RESUME_MISSION:
					if (!code) {
						// The sliders are read here rather than tracked, because a keyboard
						// or page move changes a track bar without raising WM_HSCROLL's
						// thumb notification, and resume is where the dialog read them.
						handle = GetDlgItem(window, IDC_CTRLWOL_CONNECTION);
						if (handle) {
							Game_Options_Queue(screen, UI_GAMEOPT_CONNECTION, SendMessage(handle, TBM_GETPOS, 0, 0));
						}
						handle = GetDlgItem(window, IDC_GAME_SPEED_SLIDER);
						if (handle) {
							Game_Options_Queue(screen, UI_GAMEOPT_SPEED, SendMessage(handle, TBM_GETPOS, 0, 0));
						}
						Game_Options_Queue(screen, UI_GAMEOPT_RESUME);
					}
					break;

				case IDC_ABORT_MISSION:
					if (!code) Game_Options_Queue(screen, UI_GAMEOPT_ABORT);
					break;

				case IDC_GAME_CONTROLS:
					if (!code) Game_Options_Queue(screen, UI_GAMEOPT_SETTINGS);
					break;

				default:
					break;
			}
			break;
		}

		case WM_HSCROLL: {
			if (LOWORD(wparam) == SB_THUMBTRACK) {
				int pos = HIWORD(wparam);
				char const * label = NULL;

				if ((HWND)lparam == GetDlgItem(window, IDC_GAME_SPEED_SLIDER)) {
					if (pos >= 0 && pos < (int)screen.SpeedLabels.size()) {
						label = screen.SpeedLabels[pos].c_str();
					}
					handle = GetDlgItem(window, IDC_GAME_SPEED_LABEL);
					Game_Options_Queue(screen, UI_GAMEOPT_SPEED, pos);
				} else if ((HWND)lparam == GetDlgItem(window, IDC_CTRLWOL_CONNECTION)) {
					if (pos >= 0 && pos < (int)screen.ConnectionLabels.size()) {
						label = screen.ConnectionLabels[pos].c_str();
					}
					// The connection label shares the scroll speed label's identifier, which
					// is what the IDD_OPT_CTRL_WOL template names it.
					handle = GetDlgItem(window, IDC_SCROLL_SPEED_LABEL);
					Game_Options_Queue(screen, UI_GAMEOPT_CONNECTION, pos);
				} else {
					break;
				}

				if (handle && label != NULL) {
					Static_SetText(handle, label);
				}
			}
			break;
		}

		default:
			break;
	}

	return(FALSE);
}


/// <summary>
/// Prepares the controls of the game options dialog.
/// Everything here comes out of the view-model, which the presenter refreshed before the
/// dialog was created and again whenever a save or a delete changed what is on disk.
/// </summary>
void Game_Options_On_INITDIALOG(HWND window, UIGameOptionsPresenterClass const & screen)
{
	HWND handle;

	Game_Options_Sync_Controls(window, screen);

	if (screen.HasSliders) {

		handle = GetDlgItem(window, IDC_CTRLWOL_CONNECTION);
		if (handle) {
			SetSliderRangeAndPos(handle, 0, 3, screen.ConnectionStep);
		}

		handle = GetDlgItem(window, IDC_GAME_SPEED_SLIDER);
		if (handle) {
			Slider_SetRange(handle, 0, OptionsClass::MAX_SPEED_SETTING-1);
			Slider_SetPos(handle, screen.SpeedStep);
		}
	}
}


/// <summary>
/// Displays the abort mission dialog and waits for an answer.
/// This routine is used by the special dialog handler when the player asks to abandon or
/// surrender the mission. It does not return until the player has settled on one of the
/// choices offered.
/// </summary>
/// <returns>Returns with IDOK to quit the mission, IDABORT to restart or surrender it, or
/// IDCANCEL to carry on playing.</returns>
int Abort_Dialog(void)
{
	int rc = 0;

	HWND dialog = OwnerDraw::Begin_Dialog(IDD_MISSION_ABORT, Abort_Dialog_Proc);

	if (dialog) {

		SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR)&rc);

		OwnerDraw::Display_Dialog(dialog);

		while (rc == 0) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				rc = IDOK;
			}
		}
		OwnerDraw::End_Dialog(dialog);
	}
	return(rc);
}


/// <summary>
/// Handles messages for the abort mission dialog.
/// This routine offers every message to the owner draw system first. What is left it uses
/// to relabel the restart button as a surrender for a multiplayer game, and to pass button
/// presses along to Abort_Dialog_On_COMMAND.
/// </summary>
/// <returns>Returns with the result of the owner draw default dialog handler.</returns>
INT_PTR CALLBACK Abort_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	HWND handle;

	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);
	if (rc == 0) {
		switch (message) {
			case WM_INITDIALOG:
				handle = GetDlgItem(window, IDC_RESTART_MISSION);
				if (Session.Type != GAME_NORMAL) {
					SetWindowText(handle, Fetch_String(TXT_SURRENDER));
					if (PlayerPtr->IsDefeated || PlayerPtr->IsToWin || PlayerPtr->IsToLose || PlayerPtr->IsToDie) {
						EnableWindow(handle, FALSE);
					}
				}
				break;

			case WM_COMMAND:
				Abort_Dialog_On_COMMAND(window, LOWORD(wparam), 0, HIWORD(wparam));
				break;
		}
		rc = 0;
	}
	return(rc);
}


/// <summary>
/// Handles a button press in the abort mission dialog.
/// This routine records the player's choice in the result variable that Abort_Dialog
/// attached to the dialog window, which is what ends the dialog's message pump.
/// </summary>
/// <param name="message">The control identifier of the button that was pressed.</param>
/// <param name="lparam">The notification code that came with the button press.</param>
void Abort_Dialog_On_COMMAND(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	int* retval = (int *)GetWindowLongPtr(window, DWLP_USER);

	switch ((int)message) {
		case IDC_ABORT_MISSION:
			if (lparam == 0) {
				*retval = IDOK;
			}
			break;

		case IDC_RESTART_MISSION:
			if (lparam == 0) {
				*retval = IDABORT;
			}
			break;

		case IDOK:
		case IDCANCEL:
			if (lparam == 0) {
				*retval = IDCANCEL;
			}
			break;
	}
}
