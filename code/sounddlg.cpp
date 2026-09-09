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

/* $Header: /CounterStrike/SOUNDDLG.CPP 1     3/03/97 10:25a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : SOUNDDLG.CPP                                                 *
 *                                                                                             *
 *                   Programmer : Maria del Mar McCready-Legg, Joe L. Bostic                   *
 *                                                                                             *
 *                   Start Date : Jan 8, 1995                                                  *
 *                                                                                             *
 *                  Last Update : September 22, 1995 [JLB]                                     *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   MusicListClass::Draw_Entry -- Draw the score line in a list box.                          *
 *   SoundControlsClass::Process -- Handles all the options graphic interface.                 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include "sounddlg.h"

#include "dbgprint.h"
#include "audio/audioengine.h"
#include "globals.h"
#include "goptions.h"
#include "incdec.h"
#include "init.h"
#include "language/language.h"
#include "ownrdraw.h"
#include "theme.h"
#include "ui/uishell.h"
#include "ui/uisound.h"
#include "winfix.h"

bool DialogInitialized = false;

// The screen the dialog procedure reads and writes. A dialog procedure is reached by
// Windows rather than by its driver, so this is how it finds the presenter its driver made.
static UISoundPresenterClass * _Screen = nullptr;


/// <summary>
/// Puts the view-model back into the controls that an executed intent can have changed.
/// Only the check boxes need it: shuffle and repeat exclude one another, so checking one
/// clears the other, and nothing else changes a control from underneath the player.
/// </summary>
static void Sound_Sync_Controls(HWND window, UISoundPresenterClass const & screen)
{
	HWND button = GetDlgItem(window, IDC_SOUND_SHUFFLE);
	if (button) {
		Button_SetCheck(button, screen.Shuffle ? BST_CHECKED : BST_UNCHECKED);
	}

	button = GetDlgItem(window, IDC_SOUND_REPEAT);
	if (button) {
		Button_SetCheck(button, screen.Repeat ? BST_CHECKED : BST_UNCHECKED);
	}
}


/// <summary>
/// Handles the sound and music options dialog.
/// This routine brings up the sound controls and then services the owner draw dialog
/// handler until the player dismisses them. A cut down version of the dialog is used
/// when there is no game in progress, since the in game options do not apply there.
/// </summary>
/// <remarks>This routine will not return until the player closes the dialog.</remarks>
void SoundControlsClass::Dialog(void)
{
	DebugString("SoundControls: GameSpeed = %d, ScrollRate = %d, Detail = %d\n", Options.GameSpeed, Options.ScrollRate, Options.DetailLevel);
	DialogInitialized = false;

	UISoundPresenterClass screen;
	screen.Refresh();

	// The selection is latched here, at screen entry, and the legacy dialog opens only when
	// the document could not be prepared.
	if (UI_Use_Rml()) {
		UIResult const result = UI_Sound_Screen(screen);
		if (result.Outcome != UIResult::OUTCOME_FAILED_TO_OPEN) {
			DebugString("SoundControls: GameSpeed = %d, ScrollRate = %d, Detail = %d\n", Options.GameSpeed, Options.ScrollRate, Options.DetailLevel);
			return;
		}
		screen.IsClosing = false;
		screen.Result.reset();
	}

	_Screen = &screen;

	HWND dialog;
	if (screen.Is_Lite()) {
		dialog = OwnerDraw::Begin_Dialog(IDD_SOUND_OPTIONS_DIALOG_LITE, Sound_Option_Dialog_Func);
	} else {
		dialog = OwnerDraw::Begin_Dialog(IDD_SOUND_OPTIONS_DIALOG, Sound_Option_Dialog_Func);
	}

	if (dialog) {

		OwnerDraw::Display_Dialog(dialog);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				UIResult ended;
				ended.Outcome = UIResult::OUTCOME_SESSION_ENDED;
				ended.GameEnded = true;
				screen.Result = ended;
			}

			// A control handler queues rather than acts, so the queue is executed here,
			// after the pump has returned and before the pass's maintenance.
			screen.Drain();
			Sound_Sync_Controls(dialog, screen);

			screen.Service();
		}

		OwnerDraw::End_Dialog(dialog);
	}

	_Screen = nullptr;

	DebugString("SoundControls: GameSpeed = %d, ScrollRate = %d, Detail = %d\n", Options.GameSpeed, Options.ScrollRate, Options.DetailLevel);
}


/***********************************************************************************************
 * SoundControlsClass::Process -- Handles all the options graphic interface.                   *
 *                                                                                             *
 *    This routine is the main control for the visual representation of the options            *
 *    screen. It handles the visual overlay and the player input.                              *
 *                                                                                             *
 * INPUT:      none                                                                            *
 *                                                                                             *
 * OUTPUT:     none                                                                            *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:    12/31/1994 MML : Created.                                                       *
 *=============================================================================================*/
INT_PTR CALLBACK SoundControlsClass::Sound_Option_Dialog_Func(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);

	if (rc != 0) {
		return(rc);
	}

	// The driver owns the screen for the whole life of the dialog, so a message that
	// arrives without one has nothing to act on.
	if (_Screen == nullptr) {
		return(FALSE);
	}

	UISoundPresenterClass & screen = *_Screen;

	switch (message) {
		case WM_INITDIALOG: {
				DialogInitialized = false;

				/*
				**	Music volume slider.
				*/
				HWND track = GetDlgItem(window, IDC_MUSIC_VOLUME);
				if (track) {
					SendMessage(track, OD_TRACKSILENT, 0, 0);
					Slider_SetRange(track, 0, UISoundPresenterClass::VOLUME_LEVELS);
					Slider_SetPos(track, screen.MusicVolume);
					EnableWindow(track, screen.Available);
				}

				/*
				**	Sound volume slider.
				*/
				track = GetDlgItem(window, IDC_SOUND_VOLUME);
				if (track) {
					SendMessage(track, OD_TRACKSILENT, 0, 0);
					Slider_SetRange(track, 0, UISoundPresenterClass::VOLUME_LEVELS);
					Slider_SetPos(track, screen.SoundVolume);
					EnableWindow(track, screen.Available);
				}

				track = GetDlgItem(window, IDC_VOICE_VOLUME);
				if (track) {
					SendMessage(track, OD_TRACKSILENT, 0, 0);
					Slider_SetRange(track, 0, UISoundPresenterClass::VOLUME_LEVELS);
					Slider_SetPos(track, screen.VoiceVolume);
					EnableWindow(track, screen.Available);
				}

				if (screen.HasMusic) {

					/*
					**	Shuffle control.
					*/
					HWND button = GetDlgItem(window, IDC_SOUND_SHUFFLE);
					if (button) {
						Button_SetCheck(button, screen.Shuffle ? BST_CHECKED : BST_UNCHECKED);
						EnableWindow(button, screen.Available);
					}

					/*
					**	Repeat control.
					*/
					button = GetDlgItem(window, IDC_SOUND_REPEAT);
					if (button) {
						Button_SetCheck(button, screen.Repeat ? BST_CHECKED : BST_UNCHECKED);
						EnableWindow(button, screen.Available);
					}

					/*
					**	Add the eligible themes to the list box, in the order the screen
					**	built them, and show the one that is playing.
					*/
					HWND list = GetDlgItem(window, IDC_SOUND_TRACKLIST);
					if (list) {
						ListBox_ResetContent(list);

						for (int index = 0; index < (int)screen.Tracks.size(); index++) {
							int const row = ListBox_AddString(list, screen.Tracks[index].Label.c_str());
							if (row != LB_ERR) {
								ListBox_SetItemData(list, row, index);
							}
						}

						ListBox_SetCurSel(list, screen.Selected);
						ListBox_SetTopIndex(list, screen.Selected);
						EnableWindow(list, screen.Available);
					}
				}

				DialogInitialized = true;
			}

			break;

		case WM_COMMAND:
			switch (LOWORD(wparam)) {

				/*
				**	Toggle the shuffle button.
				*/
				case IDC_SOUND_SHUFFLE:
					screen.Queue(UIIntent{UI_SOUND_SHUFFLE, "", Button_GetCheck((HWND)lparam) == BST_CHECKED});
					break;

				/*
				**	Toggle the repeat button.
				*/
				case IDC_SOUND_REPEAT:
					screen.Queue(UIIntent{UI_SOUND_REPEAT, "", Button_GetCheck((HWND)lparam) == BST_CHECKED});
					break;

				/*
				**	Stop all themes from playing.
				*/
				case IDC_SOUND_STOP:
					if (HIWORD(wparam) == 0) {
						screen.Queue(UIIntent{UI_SOUND_STOP, "", 0});
					}
					break;

				case IDOK:
					if (HIWORD(wparam) == 0) {
						screen.Queue(UIIntent{UI_SOUND_ACCEPT, "", 0});
					}
					break;

				/*
				**	Start the currently selected theme to play.
				*/
				case IDC_SOUND_PLAY:
					if (HIWORD(wparam) == 0) {
						HWND list = GetDlgItem(window, IDC_SOUND_TRACKLIST);
						if (list) {
							int const row = ListBox_GetCurSel(list);
							if (row != LB_ERR) {
								screen.Queue(UIIntent{UI_SOUND_SELECT, "", (int)ListBox_GetItemData(list, row)});
								screen.Queue(UIIntent{UI_SOUND_PLAY, "", 0});
							}
						}
					}
					break;
			}
			break;

		/*
		 * Control volume.
		 */
		case WM_HSCROLL:
			if (DialogInitialized) {
				HWND track = (HWND)lparam;
				if (track == GetDlgItem(window, IDC_MUSIC_VOLUME)) {
					screen.Queue(UIIntent{UI_SOUND_MUSIC, "", Slider_GetPos(track)});
				} else if (track == GetDlgItem(window, IDC_SOUND_VOLUME)) {
					screen.Queue(UIIntent{UI_SOUND_SOUND, "", Slider_GetPos(track)});
				} else if (track == GetDlgItem(window, IDC_VOICE_VOLUME)) {
					screen.Queue(UIIntent{UI_SOUND_VOICE, "", Slider_GetPos(track)});
				}
			}
			break;
	}

	return(FALSE);
}
