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

/* $Header: /CounterStrike/GAMEDLG.CPP 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : GAMEDLG.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Maria del Mar McCready Legg, Joe L. Bostic                   *
 *                                                                                             *
 *                   Start Date : Jan 8, 1995                                                  *
 *                                                                                             *
 *                  Last Update : Jan 18, 1995   [MML]                                         *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   OptionsClass::Process -- Handles all the options graphic interface.                       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include "gamedlg.h"

#include "_map.h"
#include "_tooltip.h"
#include "cctooltip.h"
#include "data.h"
#include "dbgprint.h"
#include "audio/audioengine.h"
#include "globals.h"
#include "init.h"
#include "language/language.h"
#include "ownrdraw.h"
#include "queue.h"
#include "session.h"
#include "techno.h"
#include "ui/uigamecontrols.h"

#include "special.hh"

#include <string>
#include <vector>

int GameSpeedNames[OptionsClass::MAX_SPEED_SETTING] = {
	TXT_SLOWEST,
	TXT_SLOWER,
	TXT_SLOW,
	TXT_MEDIUM,
	TXT_FAST,
	TXT_FASTER,
	TXT_FASTEST
};

int GameScrollSpeedNames[OptionsClass::MAX_SCROLL_SETTING] = {
	TXT_SLOWEST,
	TXT_SLOWER,
	TXT_SLOW,
	TXT_MEDIUM,
	TXT_FAST,
	TXT_FASTER,
	TXT_FASTEST
};

int GameDetailLevelNames[OptionsClass::MAX_DETAIL_SETTING] = {
	TXT_LOW,
	TXT_MEDIUM,
	TXT_HIGH
};

int GameDifficultyNames[OptionsClass::MAX_DIFFICULTY_SETTING] = {
	TXT_EASY,
	TXT_NORMAL,
	TXT_HARD
};


INT_PTR CALLBACK Game_Controls_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
void Game_Controls_Dialog_On_COMMAND(HWND window, UINT message, WPARAM wparam, LPARAM lparam);


// The screen the dialog procedure reads and writes. The driver owns it for the whole life
// of the dialog, which is the lifetime DWLP_USER gave the result pointer it replaces.
static UIGameControlsPresenterClass * _Screen = NULL;


static void Game_Controls_Queue(UIGameControlsPresenterClass & screen, char const * action, int value = 0)
{
	UIIntent intent;
	intent.Action = action;
	intent.Value = value;
	screen.Queue(intent);
}


// Reads every control back into the view-model. The dialog read them at IDOK rather than
// tracking them, because a keyboard or page move changes a track bar without raising the
// thumb notification the label follows.
static void Game_Controls_Read_Back(HWND window, UIGameControlsPresenterClass & screen)
{
	static struct {
		int Control;
		char const * Action;
	} const _sliders[] = {
		{ IDC_GAME_SPEED_SLIDER, UI_GAMECTRL_SPEED },
		{ IDC_SCROLL_SPEED_SLIDER, UI_GAMECTRL_SCROLL },
		{ IDC_DETAIL_LEVEL_SLIDER, UI_GAMECTRL_DETAIL },
		{ IDC_DIFFICULTY_SLIDER, UI_GAMECTRL_DIFFICULTY },
	};

	static struct {
		int Control;
		char const * Action;
	} const _checks[] = {
		{ IDC_SIDEBAR_TEXT, UI_GAMECTRL_CAMEO_TEXT },
		{ IDC_TARGET_LINES, UI_GAMECTRL_ACTION_LINES },
		{ IDC_TOOLTIPS, UI_GAMECTRL_TOOLTIPS },
		{ IDC_SCROLL_COASTING, UI_GAMECTRL_COASTING },
		{ IDC_EDGE_SCROLL, UI_GAMECTRL_EDGE_SCROLL },
	};

	for (auto const & slider : _sliders) {
		HWND handle = GetDlgItem(window, slider.Control);
		if (handle) {
			Game_Controls_Queue(screen, slider.Action, Slider_GetPos(handle));
		}
	}

	for (auto const & check : _checks) {
		HWND handle = GetDlgItem(window, check.Control);
		if (handle) {
			Game_Controls_Queue(screen, check.Action, Button_GetCheck(handle) == TRUE ? 1 : 0);
		}
	}
}

/***********************************************************************************************
 * OptionsClass::Process -- Handles all the options graphic interface.                         *
 *                                                                                             *
 *    This routine is the main control for the visual representation of the options            *
 *    screen. It handles the visual overlay and the player input.                              *
 *                                                                                             *
 * INPUT:   none                                                                               *
 * OUTPUT:  none                                                                               *
 * WARNINGS:   none                                                                            *
 * HISTORY:                                                                                    *
 *   12/31/1994 MML : Created.                                                                 *
 *=============================================================================================*/
void GameControlsClass::Dialog(void)
{
	DebugString("GameControls: GameSpeed = %d, ScrollRate = %d, Detail = %d\n", Options.GameSpeed, Options.ScrollRate, Options.DetailLevel);

	UIGameControlsPresenterClass screen;
	screen.Refresh();

	_Screen = &screen;

	if (GameActive == true) {
		if (Session.Type == GAME_INTERNET) {
			_Dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_GAME_WOL, Game_Controls_Dialog_Proc);
		} else {
			_Dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_GAME_MP, Game_Controls_Dialog_Proc);
		}
	} else {
		_Dialog = OwnerDraw::Begin_Dialog(IDD_OPT_CTRL_GAME_SP, Game_Controls_Dialog_Proc);
	}

	if (_Dialog) {

		OwnerDraw::Display_Dialog(_Dialog);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				// A session that ended underneath the screen leaves the settings alone,
				// which is what the driver's own result of two did.
				UIResult ended;
				ended.Outcome = UIResult::OUTCOME_SESSION_ENDED;
				ended.GameEnded = true;
				screen.Result = ended;
				break;
			}

			screen.Drain();
			screen.Service();
		}

		if (screen.Commits()) {
			screen.Apply();
			Options.Save_Settings();
		}

		OwnerDraw::End_Dialog(_Dialog);
	}

	_Screen = NULL;

	DebugString("GameControls: GameSpeed = %d, ScrollRate = %d, Detail = %d\n", Options.GameSpeed, Options.ScrollRate, Options.DetailLevel);
}


/// <summary>
/// Handles the messages sent to the game controls dialog.
/// The procedure primes its controls from the view-model, tracks the label alongside a
/// slider the player is dragging, and queues what the player pressed for the driver to
/// execute after the pump.
/// </summary>
/// <returns>Returns with a non-zero value if the message was consumed by the ownerdraw
/// layer.</returns>
INT_PTR CALLBACK Game_Controls_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	HWND handle;
	int index;

	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);
	if (rc == 0) {
		if (_Screen == NULL) {
			return(0);
		}

		UIGameControlsPresenterClass & screen = *_Screen;

		switch (message) {
			case WM_INITDIALOG:
				handle = GetDlgItem(window, IDC_GAME_SPEED_SLIDER);
				if (handle) {
					SendMessage(handle, OD_TRACKNUMBERS, 0, 0);
					Slider_SetRange(handle, 0, (OptionsClass::MAX_SPEED_SETTING-1));
					Slider_SetPos(handle, screen.SpeedStep);
				}

				handle = GetDlgItem(window, IDC_SCROLL_SPEED_SLIDER);
				if (handle) {
					SendMessage(handle, OD_TRACKNUMBERS, 0, 0);
					Slider_SetRange(handle, 0, (OptionsClass::MAX_SCROLL_SETTING-1));
					Slider_SetPos(handle, screen.ScrollStep);
				}

				handle = GetDlgItem(window, IDC_DETAIL_LEVEL_SLIDER);
				if (handle) {
					SendMessage(handle, OD_TRACKNUMBERS, 0, 0);
					Slider_SetRange(handle, 0, (OptionsClass::MAX_DETAIL_SETTING-1));
					Slider_SetPos(handle, screen.DetailStep);
				}

				handle = GetDlgItem(window, IDC_SIDEBAR_TEXT);
				if (handle) {
					Button_SetCheck(handle, screen.CameoText);
				}

				handle = GetDlgItem(window, IDC_TARGET_LINES);
				if (handle) {
					Button_SetCheck(handle, screen.ActionLines);
				}

				handle = GetDlgItem(window, IDC_TOOLTIPS);
				if (handle) {
					Button_SetCheck(handle, screen.ShowToolTips);
				}

				handle = GetDlgItem(window, IDC_SCROLL_COASTING);
				if (handle) {
					Button_SetCheck(handle, screen.Coasting);
				}

				handle = GetDlgItem(window, IDC_EDGE_SCROLL);
				if (handle) {
					Button_SetCheck(handle, screen.EdgeScroll);
				}

				if (screen.Has_Sub_Screens()) {
					handle = GetDlgItem(window, IDC_OPT_SOUND_BTN);
					if (handle) {
						EnableWindow(handle, screen.SoundAvailable);
					}
				} else {
					handle = GetDlgItem(window, IDC_DIFFICULTY_SLIDER);
					if (handle) {
						SendMessage(handle, OD_TRACKNUMBERS, 0, 0);
						Slider_SetRange(handle, 0, (OptionsClass::MAX_DIFFICULTY_SETTING-1));
						Slider_SetPos(handle, screen.DifficultyStep);
					}
				}
				break;

			case WM_COMMAND:
				Game_Controls_Dialog_On_COMMAND(window, LOWORD(wparam), 0, HIWORD(wparam));
				break;

			case WM_HSCROLL:
				if (LOWORD(wparam) == SB_THUMBTRACK) {
					index = HIWORD(wparam);
					std::vector<std::string> const * labels = NULL;

					handle = 0;
					if ((HWND)lparam == GetDlgItem(window, IDC_GAME_SPEED_SLIDER)) {
						labels = &screen.SpeedLabels;
						handle = GetDlgItem(window, IDC_GAME_SPEED_LABEL);
					} else if ((HWND)lparam == GetDlgItem(window, IDC_SCROLL_SPEED_SLIDER)) {
						labels = &screen.ScrollLabels;
						handle = GetDlgItem(window, IDC_SCROLL_SPEED_LABEL);
					} else if ((HWND)lparam == GetDlgItem(window, IDC_DETAIL_LEVEL_SLIDER)) {
						labels = &screen.DetailLabels;
						handle = GetDlgItem(window, IDC_DETAIL_LEVEL_LABEL);
					} else if (!screen.Has_Sub_Screens() && (HWND)lparam == GetDlgItem(window, IDC_DIFFICULTY_SLIDER)) {
						labels = &screen.DifficultyLabels;
						handle = GetDlgItem(window, IDC_DIFFICULTY_LABEL);
					}
					if (handle && labels != NULL && index >= 0 && index < (int)labels->size()) {
						SetWindowText(handle, (*labels)[index].c_str());
					}
				}
				break;
		}
		rc = 0;
	}
	return(rc);
}


/// <summary>
/// Queues what the player pressed in the game controls dialog.
/// Leaving through the sound or the keyboard button reads the controls back as the accept
/// button does, because the dialog answered with the same IDOK for all three.
/// </summary>
/// <param name="window">The game controls dialog window.</param>
/// <param name="message">The identifier of the control that was activated.</param>
/// <param name="lparam">The notification code the control sent.</param>
void Game_Controls_Dialog_On_COMMAND(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (_Screen == NULL) {
		return;
	}

	UIGameControlsPresenterClass & screen = *_Screen;

	switch ((INT)message) {
		case IDC_OPT_KEYBOARD_BTN:
			if (lparam == 0 && screen.Has_Sub_Screens()) {
				Game_Controls_Read_Back(window, screen);
				Game_Controls_Queue(screen, UI_GAMECTRL_KEYBOARD);
			}
			break;

		case IDC_OPT_SOUND_BTN:
			if (lparam == 0 && screen.Has_Sub_Screens()) {
				Game_Controls_Read_Back(window, screen);
				Game_Controls_Queue(screen, UI_GAMECTRL_SOUND);
			}
			break;

		case IDOK:
			if (lparam == 0) {
				Game_Controls_Read_Back(window, screen);
				Game_Controls_Queue(screen, UI_GAMECTRL_ACCEPT);
			}
			break;

		case IDCANCEL:
			Game_Controls_Queue(screen, UI_GAMECTRL_CANCEL);
			break;
	}
}
