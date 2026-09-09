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
#include "ui/uiabort.h"
#include "ui/uigameoptions.h"
#include "ui/uishell.h"

#include "special.hh"

void Game_Options_On_INITDIALOG(HWND window, UIGameOptionsPresenterClass const & screen);
INT_PTR CALLBACK Game_Options_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
INT_PTR CALLBACK Abort_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
void Abort_Dialog_On_COMMAND(HWND window, UINT message, WPARAM wparam, LPARAM lparam);


// The screen the dialog procedure reads and writes. The driver owns it for the whole life
// of the dialog, which is the same lifetime DWLP_USER gave the result pointer it replaces.
static UIGameOptionsPresenterClass * _Screen = NULL;

// The abort screen, owned by Abort_Dialog for the life of its dialog.
static UIAbortPresenterClass * _Abort = NULL;


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

// What the driver does on the way out, whichever view was shown. The briefing is restated
// after the screen has gone, which is where the dialog driver restated it.
static void Game_Options_Finish(UIGameOptionsPresenterClass const & screen)
{
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

	IgnoreInput = true;
	Keyboard->Clear();

	// The selection is latched here, at screen entry, and the legacy dialog opens only when
	// the document could not be prepared.
	if (UI_Use_Rml()) {
		UIResult const result = UI_Game_Options_Screen(screen);
		if (result.Outcome != UIResult::OUTCOME_FAILED_TO_OPEN) {
			Game_Options_Finish(screen);
			return;
		}
		screen.IsClosing = false;
		screen.Result.reset();
	}

	_Screen = &screen;

	HWND dialog;
	if (Session.Type == GAME_NORMAL || Session.Type == GAME_SKIRMISH) {
		dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_SP, Game_Options_Dialog_Proc);
	} else if (Session.Type == GAME_INTERNET) {
		dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_WOL, Game_Options_Dialog_Proc);
	} else {
		dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_MP, Game_Options_Dialog_Proc);
	}

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

	Game_Options_Finish(screen);
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


// Maps the screen's choice onto the value the special dialog handler expects. A screen that
// never opened answers zero, which is what the driver's own result was left at.
static int Abort_Choice_Result(UIAbortPresenterClass const & screen)
{
	switch (screen.Choice) {
		case UIAbortPresenterClass::CHOICE_QUIT:
			return(IDOK);

		case UIAbortPresenterClass::CHOICE_RESTART:
			return(IDABORT);

		case UIAbortPresenterClass::CHOICE_CANCEL:
			return(IDCANCEL);

		default:
			return(0);
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
	UIAbortPresenterClass screen;
	screen.Refresh();

	// The selection is latched here, at screen entry, and the legacy dialog opens only when
	// the document could not be prepared.
	if (UI_Use_Rml()) {
		UIResult const result = UI_Abort_Screen(screen);
		if (result.Outcome != UIResult::OUTCOME_FAILED_TO_OPEN) {
			return(Abort_Choice_Result(screen));
		}
		screen.IsClosing = false;
		screen.Result.reset();
	}

	_Abort = &screen;

	HWND dialog = OwnerDraw::Begin_Dialog(IDD_MISSION_ABORT, Abort_Dialog_Proc);

	if (dialog) {

		OwnerDraw::Display_Dialog(dialog);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				// A session that ended underneath the box answers as though the player chose
				// to quit, which is the IDOK the driver used to write.
				UIResult ended;
				ended.Outcome = UIResult::OUTCOME_SESSION_ENDED;
				ended.GameEnded = true;
				screen.Choice = UIAbortPresenterClass::CHOICE_QUIT;
				screen.Result = ended;
				break;
			}

			screen.Drain();
		}

		OwnerDraw::End_Dialog(dialog);
	}

	_Abort = NULL;

	return(Abort_Choice_Result(screen));
}


/// <summary>
/// Handles messages for the abort mission dialog.
/// The procedure relabels and disables the middle button from the view-model, and queues
/// what the player pressed for the driver to execute after the pump.
/// </summary>
/// <returns>Returns with the result of the owner draw default dialog handler.</returns>
INT_PTR CALLBACK Abort_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	HWND handle;

	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);
	if (rc == 0) {
		if (_Abort == NULL) {
			return(0);
		}

		UIAbortPresenterClass & screen = *_Abort;

		switch (message) {
			case WM_INITDIALOG:
				handle = GetDlgItem(window, IDC_RESTART_MISSION);
				if (handle) {
					if (!screen.RestartCaption.empty()) {
						SetWindowText(handle, screen.RestartCaption.c_str());
					}
					if (!screen.CanRestart) {
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
/// Queues what the player pressed in the abort mission dialog.
/// </summary>
/// <param name="message">The control identifier of the button that was pressed.</param>
/// <param name="lparam">The notification code that came with the button press.</param>
void Abort_Dialog_On_COMMAND(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (_Abort == NULL || lparam != 0) {
		return;
	}

	UIIntent intent;

	switch ((int)message) {
		case IDC_ABORT_MISSION:
			intent.Action = UI_ABORT_QUIT;
			break;

		case IDC_RESTART_MISSION:
			intent.Action = UI_ABORT_RESTART;
			break;

		case IDOK:
		case IDCANCEL:
			intent.Action = UI_ABORT_CANCEL;
			break;

		default:
			return;
	}

	_Abort->Queue(intent);
}
