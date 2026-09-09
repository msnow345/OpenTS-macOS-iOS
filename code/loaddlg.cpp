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

/* $Header: /CounterStrike/LOADDLG.CPP 1     3/03/97 10:25a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : LOADDLG.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Maria Legg, Joe Bostic, Bill Randolph                        *
 *                                                                                             *
 *                   Start Date : March 19, 1995                                               *
 *                                                                                             *
 *                  Last Update : June 25, 1995 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   LoadOptionsClass::LoadOptionsClass -- class constructor                                   *
 *   LoadOptionsClass::~LoadOptionsClass -- class destructor                                   *
 *   LoadOptionsClass::Process -- main processing routine                                      *
 *   LoadOptionsClass::Clear_List -- clears the list box & Files arrays                        *
 *   LoadOptionsClass::Fill_List -- fills the list box & GameNum arrays                        *
 *   LoadOptionsClass::Num_From_Ext -- clears the list box & GameNum arrays                    *
 *   LoadOptionsClass::Compare -- for qsort                                                    *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include "autosave.h"

#include "loaddlg.h"

#include "campaign.h"
#include "conquer.h"
#include "data.h"
#include "gamedirs.h"
#include "globals.h"
#include "houstype.h"
#include "init.h"
#include "language/language.h"
#include "msgbox.h"
#include "ownrdraw.h"
#include "saveload.h"
#include "savemgr.h"
#include "savever.h"
#include "scenario.h"
#include "session.h"
#include "ui/uisavebrowser.h"
#include "ui/uishell.h"
#include "win.h"

#include <algorithm>
#include <cstdio>
#include <vector>


/***********************************************************************************************
 * LoadOptionsClass::LoadOptionsClass -- class constructor                                     *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      style      style for this load/save dialog (LOAD/SAVE/DELETE)                          *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
LoadOptionsClass::LoadOptionsClass(void) :
	Files(0),
	Style(NONE),
	Description(NULL),
	Callback(NULL),
	State(STATE_PENDING)
{
	Style = NONE;
	Description = NULL;
	Callback = NULL;
	Extension = "SAV";
	MinSpaceRequired = 2048;
	Files.Clear();
}


/***********************************************************************************************
 * LoadOptionsClass::~LoadOptionsClass -- class destructor                                     *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
LoadOptionsClass::~LoadOptionsClass(void)
{
	for (int i = 0; i < Files.Count(); i++) {
		delete Files[i];
	}
	Files.Clear();
}


/// <summary>
/// Brings up the load game dialog.
/// This routine is used by the options menu to let the player pick a saved game and
/// resume it.
/// </summary>
/// <returns>bool; Was a game loaded?</returns>
bool LoadOptionsClass::Load(void)
{
	Style = LOAD;
	Description = NULL;
	return(Dialog());
}


/// <summary>
/// Brings up the save game dialog.
/// This routine is used by the options menu when the player wants to record the current
/// game. The description offered is used to prime the edit field.
/// </summary>
/// <param name="description">The description to suggest for the saved game.</param>
/// <returns>bool; Was the game saved?</returns>
bool LoadOptionsClass::Save(char *description)
{
	Style = SAVE;
	Description = description;
	return(Dialog());
}


/// <summary>
/// Brings up the delete game dialog.
/// This routine is used by the options menu to let the player clear out save games that
/// are no longer wanted.
/// </summary>
/// <returns>bool; Did the player go through with the deletion?</returns>
bool LoadOptionsClass::Delete(void)
{
	Style = WWDELETE;
	Description = NULL;
	return(Dialog());
}


/// <summary>
/// Handles a control notification from the load game dialog.
/// This routine records how the player left the dialog, so that the processing loop
/// knows whether a game was chosen or the player backed out.
/// </summary>
/// <param name="wparam">The identifier of the control that was activated.</param>
/// <param name="lparam">Window handle of the control that was activated.</param>
/// <param name="id">The notification code that accompanied the control.</param>
void LoadOptionsClass::Load_Dialog_On_WM_COMMAND(HWND window, WPARAM wparam, LPARAM lparam, int id)
{
	UISaveBrowserPresenterClass * screen = (UISaveBrowserPresenterClass *)GetWindowLongPtr(window, DWLP_USER);
	if (screen == NULL) {
		return;
	}

	switch ((int)wparam) {
		case IDC_MISSION_LOAD_LIST:
			if (id == 2 && ListBox_GetCount((HWND)lparam) > 0) {
				screen->Queue(UIIntent{UI_SAVEBROWSER_SELECT, "", ListBox_GetCurSel((HWND)lparam)});
				screen->Queue(UIIntent{UI_SAVEBROWSER_ACCEPT, "", 0});
			}
			break;

		case IDOK:
			if (id == 0) {
				screen->Queue(UIIntent{UI_SAVEBROWSER_SELECT, "", ListBox_GetCurSel(GetDlgItem(window, IDC_MISSION_LOAD_LIST))});
				screen->Queue(UIIntent{UI_SAVEBROWSER_ACCEPT, "", 0});
			}
			break;

		case IDCANCEL:
			if (id == 0) {
				screen->Queue(UIIntent{UI_SAVEBROWSER_CANCEL, "", 0});
			}
			break;
	}
}


/// <summary>
/// Handles a control notification from the save game dialog.
/// Picking a game in the list copies its description into the edit field, so that the
/// player can save over an existing game without typing the name out again. The buttons
/// record how the player left the dialog.
/// </summary>
/// <param name="wparam">The identifier of the control that was activated.</param>
/// <param name="lparam">Window handle of the control that was activated.</param>
/// <param name="id">The notification code that accompanied the control.</param>
void LoadOptionsClass::Save_Dialog_On_WM_COMMAND(HWND window, WPARAM wparam, LPARAM lparam, int id)
{
	UISaveBrowserPresenterClass * screen = (UISaveBrowserPresenterClass *)GetWindowLongPtr(window, DWLP_USER);
	if (screen == NULL) {
		return;
	}

	switch ((int)wparam) {
		case IDC_MISSION_SAVE_LIST:
			if (id == 1 && ListBox_GetCount((HWND)lparam) > 0) {
				int const row = ListBox_GetCurSel((HWND)lparam);
				if (row != LB_ERR) {
					screen->Queue(UIIntent{UI_SAVEBROWSER_SELECT, "", row});
				}
			}
			break;

		case IDOK:
			if (id == 0) {
				// The field is read here rather than tracked, because the description the
				// player typed is only ever wanted at the moment the button is pressed.
				char buffer[256];
				GetWindowText(GetDlgItem(window, IDC_MISSION_SAVE_DESC), buffer, DESCRIP_MAX+36);
				screen->Queue(UIIntent{UI_SAVEBROWSER_DESCRIBE, buffer, 0});
				screen->Queue(UIIntent{UI_SAVEBROWSER_ACCEPT, "", 0});
			}
			break;

		case IDCANCEL:
			if (id == 0) {
				screen->Queue(UIIntent{UI_SAVEBROWSER_CANCEL, "", 0});
			}
			break;
	}
}


/// <summary>
/// Handles a control notification from the delete game dialog.
/// This routine records how the player left the dialog, so that the processing loop
/// knows whether to go ahead with the deletion.
/// </summary>
/// <param name="wparam">The identifier of the control that was activated.</param>
/// <param name="id">The notification code that accompanied the control.</param>
void LoadOptionsClass::Delete_Dialog_On_WM_COMMAND(HWND window, WPARAM wparam, LPARAM lparam, int id)
{
	UISaveBrowserPresenterClass * screen = (UISaveBrowserPresenterClass *)GetWindowLongPtr(window, DWLP_USER);
	if (screen == NULL) {
		return;
	}

	switch ((int)wparam) {
		case IDOK:
			if (id == 0) {
				screen->Queue(UIIntent{UI_SAVEBROWSER_SELECT, "", ListBox_GetCurSel(GetDlgItem(window, IDC_MISSION_DELETE_LIST))});
				screen->Queue(UIIntent{UI_SAVEBROWSER_ACCEPT, "", 0});
			}
			break;

		case IDCANCEL:
			if (id == 0) {
				screen->Queue(UIIntent{UI_SAVEBROWSER_CANCEL, "", 0});
			}
			break;
	}
}


/// <summary>
/// Handles messages for the load game dialog.
/// The owner draw system is given first refusal on every message. What is left over is
/// used to set up the file list columns and to pass control activity along to the
/// command handler.
/// </summary>
/// <returns>Returns with the message result, or FALSE if nothing here dealt with it.</returns>
INT_PTR CALLBACK LoadOptionsClass::Load_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);

	if (rc == 0) {

		switch (message) {
			case WM_MOVING:
				return(On_WM_MOVING(window, wparam, lparam));

			case WM_COMMAND:
				Load_Dialog_On_WM_COMMAND(window, LOWORD(wparam), lparam, HIWORD(wparam));
				break;

			case OD_SUBCLASSED:
				SendDlgItemMessage(window, IDC_MISSION_LOAD_LIST, OD_ADDCOLUMN, 0xF9, 2);
				SendDlgItemMessage(window, IDC_MISSION_LOAD_LIST, OD_ADDCOLUMN, 0x38, 255);
				SendDlgItemMessage(window, IDC_MISSION_LOAD_LIST, OD_ADDCOLUMN, 0, 315);
				break;
		}
		return(FALSE);
	}
	return(rc);
}


/// <summary>
/// Handles messages for the save game dialog.
/// The owner draw system is given first refusal on every message. What is left over is
/// used to set up the file list columns, cap the length of the description the player
/// may type, and pass control activity along to the command handler.
/// </summary>
/// <returns>Returns with the message result, or FALSE if nothing here dealt with it.</returns>
INT_PTR CALLBACK LoadOptionsClass::Save_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);

	if (rc == 0) {

		switch (message) {
			case WM_MOVING:
				return(On_WM_MOVING(window, wparam, lparam));

			case WM_COMMAND:
				Save_Dialog_On_WM_COMMAND(window, LOWORD(wparam), lparam, HIWORD(wparam));
				break;

			case WM_INITDIALOG:
				SendMessage(GetDlgItem(window, IDC_MISSION_SAVE_DESC), EM_SETLIMITTEXT, 79, 0);
				break;

			case OD_SUBCLASSED:
				SendDlgItemMessage(window, IDC_MISSION_SAVE_LIST, OD_ADDCOLUMN, 0xF9, 2);
				SendDlgItemMessage(window, IDC_MISSION_SAVE_LIST, OD_ADDCOLUMN, 0x38, 255);
				SendDlgItemMessage(window, IDC_MISSION_SAVE_LIST, OD_ADDCOLUMN, 0, 315);
				break;
		}
		return(FALSE);
	}
	return(rc);
}


/// <summary>
/// Handles messages for the delete game dialog.
/// The owner draw system is given first refusal on every message. What is left over is
/// used to set up the file list columns and to pass control activity along to the
/// command handler.
/// </summary>
/// <returns>Returns with the message result, or FALSE if nothing here dealt with it.</returns>
INT_PTR CALLBACK LoadOptionsClass::Delete_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);

	if (rc == 0) {

		switch (message) {
			case WM_COMMAND:
				Delete_Dialog_On_WM_COMMAND(window, LOWORD(wparam), lparam, HIWORD(wparam));
				break;

			case WM_MOVING:
				return(On_WM_MOVING(window, wparam, lparam));

			case OD_SUBCLASSED:
				SendDlgItemMessage(window, IDC_MISSION_DELETE_LIST, OD_ADDCOLUMN, 0xF9, 2);
				SendDlgItemMessage(window, IDC_MISSION_DELETE_LIST, OD_ADDCOLUMN, 0x38, 255);
				SendDlgItemMessage(window, IDC_MISSION_DELETE_LIST, OD_ADDCOLUMN, 0, 315);
				break;
		}
		return(FALSE);
	}
	return(rc);
}


/// <summary>
/// Is a saved game of this name already there? Asked before one is written, since a name the
/// folder holds is written over rather than added to.
/// </summary>
static bool Saved_Game_Exists(char const * name)
{
	return(GetFileAttributes(Saved_Game_Name(name).c_str()) != INVALID_FILE_ATTRIBUTES);
}


/***********************************************************************************************
 * LoadOptionsClass::Process -- main processing routine                                        *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      false = User cancelled, true = operation completed                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
/// <summary>
/// Puts the view-model on the dialog's own controls.
/// </summary>
static void Save_Browser_Sync_Controls(HWND window, UISaveBrowserPresenterClass & screen)
{
	if (screen.Style != UISaveBrowserPresenterClass::STYLE_SAVE) {
		return;
	}

	HWND const field = GetDlgItem(window, IDC_MISSION_SAVE_DESC);
	if (field == NULL) {
		return;
	}

	char current[256];
	GetWindowText(field, current, sizeof(current));

	if (strcmp(current, screen.Description.c_str()) != 0) {
		SetWindowText(field, screen.Description.c_str());
	}

	if (screen.FocusDescription) {
		screen.FocusDescription = false;
		SetFocus(field);
		Edit_SetSel(field, 0, -1);
	}
}


/// <summary>
/// Rebuilds the list control when the view-model's list has moved.
/// </summary>
void LoadOptionsClass::Sync_List(HWND list, HWND dialog, UISaveBrowserPresenterClass & screen)
{
	if (list == 0 || !screen.ListChanged) {
		return;
	}

	screen.ListChanged = false;
	Fill_List(list, screen.Selected);
	EnableWindow(GetDlgItem(dialog, 1), screen.CanAct ? TRUE : FALSE);
}


bool LoadOptionsClass::Dialog(void)
{
	UISaveBrowserPresenterClass::StyleType style = UISaveBrowserPresenterClass::STYLE_LOAD;
	if (Style == SAVE) {
		style = UISaveBrowserPresenterClass::STYLE_SAVE;
	} else if (Style == WWDELETE) {
		style = UISaveBrowserPresenterClass::STYLE_DELETE;
	}

	UISaveBrowserPresenterClass screen(*this, style);

	if (!screen.Can_Open()) {
		return(false);
	}

	screen.Refresh();

	State = STATE_PENDING;

	if (UI_Use_Rml()) {
		UIResult const result = UI_Save_Browser_Screen(screen);

		if (result.Outcome != UIResult::OUTCOME_FAILED_TO_OPEN) {
			Clear_List();
			State = screen.Accepted() ? STATE_OK : STATE_CLOSE;
			return(screen.Accepted());
		}

		// Preparation failed, so nothing is shown and the legacy dialog answers instead. A
		// suspended screen leaves the presenter marked, and the dialog runs the same screen.
		screen.Result.reset();
		screen.IsClosing = false;
	}

	HWND dialog = 0;
	HWND list = 0;

	switch (Style) {
		case LOAD:
			dialog = OwnerDraw::Begin_Dialog(IDD_MISSION_LOAD, Load_Dialog_Proc);
			list = GetDlgItem(dialog, IDC_MISSION_LOAD_LIST);
			break;

		case SAVE:
			dialog = OwnerDraw::Begin_Dialog(IDD_MISSION_SAVE, Save_Dialog_Proc);
			list = GetDlgItem(dialog, IDC_MISSION_SAVE_LIST);
			break;

		case WWDELETE:
			dialog = OwnerDraw::Begin_Dialog(IDD_MISSION_DELETE, Delete_Dialog_Proc);
			list = GetDlgItem(dialog, IDC_MISSION_DELETE_LIST);
			break;

		default:
			break;
	}

	State = STATE_PENDING;

	if (dialog) {

		SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR)&screen);

		Sync_List(list, dialog, screen);
		Save_Browser_Sync_Controls(dialog, screen);

		OwnerDraw::Display_Dialog(dialog);

		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == true) {
				screen.Queue(UIIntent{UI_SAVEBROWSER_CANCEL, "", 0});
			}

			// A control handler queues rather than acts, so the queue is executed here,
			// after the pump has returned.
			screen.Drain();

			// A load draws where this screen is, so the dialog gets out of its way, which
			// is what its own ShowWindow did.
			if (screen.Pending != UISaveBrowserPresenterClass::SUB_NONE) {
				ShowWindow(dialog, SW_HIDE);
				UpdateWindow(MainWindow);
				screen.Run_Pending();
				if (!screen.Result.has_value()) {
					ShowWindow(dialog, SW_SHOW);
					UpdateWindow(dialog);
				}
			}

			Sync_List(list, dialog, screen);
			Save_Browser_Sync_Controls(dialog, screen);

			screen.Service();
		}

		Clear_List();

		OwnerDraw::End_Dialog(dialog);
	}

	State = screen.Accepted() ? STATE_OK : STATE_CLOSE;

	return(screen.Accepted());
}


/// <summary>
/// Fetches a save game filename that is not already in use.
/// This routine is used when the player saves into an empty slot and there is no
/// existing file to write over.
/// </summary>
/// <param name="name">Buffer to fill in with the filename chosen.</param>
/// <remarks>Be sure the buffer is big enough to hold a complete filename.</remarks>
void LoadOptionsClass::Pick_Filename(char *name)
{
	do {
		sprintf(name, "SAVE%04lX.%3s", rand(), Extension);
	} while (Saved_Game_Exists(name));
}


/***********************************************************************************************
 * LoadOptionsClass::Clear_List -- clears the list box & Files arrays                          *
 *                                                                                             *
 * This step is essential, because it frees all the strings allocated for list items.          *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
void LoadOptionsClass::Clear_List(void)
{
	/*
	**	Clear the array of game numbers
	*/
	for (int i = 0; i < Files.Count(); i++) {
		delete Files[i];
	}
	Files.Clear();
}


/***********************************************************************************************
 * LoadOptionsClass::Build_List -- reads the folder into the file list                         *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *   06/25/1995 JLB : Shows which saved games are "(old)".                                     *
 *=============================================================================================*/
void LoadOptionsClass::Build_List(void)
{
	FileEntryClass * fdata = NULL;  // for adding entries to 'Files'
	WIN32_FIND_DATAA ff;            // for FindFirstFile

	/*
	**	Make sure the list is empty
	*/
	Clear_List();

	/*
	**	Add the Empty Slot entry
	*/
	if (Style == SAVE) {
		fdata = new FileEntryClass;
		strcpy(fdata->Descr, Fetch_String(TXT_EMPTY_SLOT));
		if (PlayerPtr != NULL) {
			fdata->Scenario = Scen->Scenario;
			fdata->House = Scen->PlayerHouse;
			fdata->Num = Scen->Campaign;
			strcpy(fdata->PlayerName, PlayerPtr->Class->GivenName);
		} else {
			fdata->Scenario = 0;
			fdata->House = (HousesType)Session.House;
			fdata->Num = -1;
			strcpy(fdata->PlayerName, Session.Handle);
		}
		SYSTEMTIME time;
		GetSystemTime(&time);
		SystemTimeToFileTime(&time, &fdata->DateTime);
		fdata->Type = Session.Type;
		fdata->Valid = false;
		Files.Add(fdata);
	}

	char buffer[128];
	sprintf(buffer, "*.%3s", Extension);

	/*
	**	Find all savegame files
	*/
	std::vector<WIN32_FIND_DATAA> found;

	HANDLE hFind = FindFirstFile(Saved_Game_Name(buffer).c_str(), &ff);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if ((ff.dwFileAttributes & (FILE_ATTRIBUTE_TEMPORARY|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN)) != 0) {
				continue;
			}
			found.push_back(ff);
		} while (FindNextFile(hFind, &ff));

		FindClose(hFind);
	}

	// Newest first, so a bounded scan reads the headers of the files that matter.
	std::sort(found.begin(), found.end(), [](WIN32_FIND_DATAA const & a, WIN32_FIND_DATAA const & b) {
		return(CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) > 0);
	});
	if (found.size() > Scan_Limit()) {
		found.resize(Scan_Limit());
	}

	fdata = NULL;
	for (WIN32_FIND_DATAA & record : found) {
		if (fdata == NULL) {
			fdata = new FileEntryClass;
		}

		/*
		**	get the game's info; if success, add it to the list
		*/
		if (Read_File(fdata, &record) == true) {
			Files.Add(fdata);
			fdata = NULL;
		}
	}

	if (fdata != NULL) {
		delete fdata;
	}

	if (Files.Count() > 0) {

		/*
		**	Now sort the list in order of Date/Time (newest first, oldest last)
		*/
		qsort((void *)(&Files[0]), Files.Count(), sizeof(class FileEntryClass *), LoadOptionsClass::Compare);
	}
}


/***********************************************************************************************
 * LoadOptionsClass::Fill_List -- fills the list box & GameNum arrays                          *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *   06/25/1995 JLB : Shows which saved games are "(old)".                                     *
 *=============================================================================================*/
void LoadOptionsClass::Fill_List(HWND window, int selected)
{
	OwnerDraw::CellData thecell;
	FileEntryClass * fdata = NULL;
	char buffer[128];

	if (Files.Count() > 0) {

		ListBox_ResetContent(window);

		/*
		**	Now add every file's name to the list box
		*/
		for (int i = 0; i < Files.Count(); i++) {
			fdata = Files[i];

			int row = ListBox_AddString(window, fdata);

			if (fdata->Type != GAME_NORMAL) {
				thecell.type = OwnerDraw::CellData::TEXT;
				thecell.string.set("*");
				SendMessage(window, OD_SETCELL, MAKEWPARAM(200, row), (LPARAM)&thecell);
			}

			if (fdata->DateTime.dwHighDateTime != -1 && fdata->DateTime.dwLowDateTime != -1) {
				FILETIME ft;
				SYSTEMTIME time;
				FileTimeToLocalFileTime(&fdata->DateTime, &ft);
				FileTimeToSystemTime(&ft, &time);
				GetDateFormat(LANG_USER_DEFAULT, TIME_NOMINUTESORSECONDS, &time, NULL, buffer, sizeof(buffer));
				thecell.type = OwnerDraw::CellData::TEXT;
				thecell.string.set(buffer);
				SendMessage(window, OD_SETCELL, MAKEWPARAM(255, row), (LPARAM)&thecell);
				GetTimeFormat(LANG_USER_DEFAULT, TIME_NOSECONDS, &time, NULL, buffer, sizeof(buffer));
				thecell.type = OwnerDraw::CellData::TEXT;
				thecell.string.set(buffer);
				SendMessage(window, OD_SETCELL, MAKEWPARAM(315, row), (LPARAM)&thecell);
			}

			ListBox_SetItemData(window, row, (LPARAM)fdata);
		}

		ListBox_SetCurSel(window, selected);
		ListBox_SetTopIndex(window, selected);
	}
}


/// <summary>
/// Are there any save games available to load?
/// This routine is used to decide whether the load option should be offered to the
/// player at all. It settles the question as cheaply as it can, so it stops at the
/// first save game it can actually read.
/// </summary>
/// <returns>bool; Was at least one loadable save game found?</returns>
bool LoadOptionsClass::Files_Present(void)
{
	bool files_found = false;

	char pattern[64];
	sprintf(pattern, "*.%3s", Extension);

	WIN32_FIND_DATAA find_data;
	HANDLE hFind = FindFirstFile(Saved_Game_Name(pattern).c_str(), &find_data);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if ((find_data.dwFileAttributes & (FILE_ATTRIBUTE_TEMPORARY|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN)) != 0) {
				continue;
			}

			FileEntryClass entry;
			if (Read_File(&entry, &find_data) == true) {
				files_found = true;
				break;
			}
		} while (FindNextFile(hFind, &find_data));

		FindClose(hFind);
	}

	return(files_found);
}


/***********************************************************************************************
 * LoadOptionsClass::Compare -- for qsort                                                      *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      p1,p2      ptrs to elements to compare                                                 *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      0 = same, -1 = (*p1) goes BEFORE (*p2), 1 = (*p1) goes AFTER (*p2)                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
int __cdecl LoadOptionsClass::Compare(const void * p1, const void * p2)
{
	FileEntryClass * fe1, * fe2;

	fe1 = *((FileEntryClass **)p1);
	fe2 = *((FileEntryClass **)p2);

	int res = CompareFileTime(&fe1->DateTime, &fe2->DateTime);
	return(-res);
}


/// <summary>
/// Restores the game held in the file specified.
/// A message box is displayed while the load runs, and the scenario is taken out of
/// play first so that nothing tries to tick while the game state is being replaced.
/// </summary>
/// <returns>bool; Was the game loaded?</returns>
bool LoadOptionsClass::Load_File(const char * file_name)
{
	HWND dialog = OwnerDraw::Custom_Message_Box(Fetch_String(TXT_LOADING), NULL, NULL);
	if (dialog != 0) {
		OwnerDraw::Display_Dialog(dialog);
	}
	ScenarioActive = false;
	TacticalActive = false;
	bool loaded = Load_Game(file_name);
	if (dialog != 0) {
		OwnerDraw::End_Dialog(dialog);
	}
	return(loaded);
}


/// <summary>
/// Saves the current game to the file specified.
/// A message box is displayed while the save runs, since writing a save game takes long
/// enough that the player would otherwise think the game had locked up.
/// </summary>
/// <param name="descr">The description to record alongside the saved game.</param>
/// <returns>bool; Was the game saved?</returns>
bool LoadOptionsClass::Save_File(const char * file_name, const char * descr)
{
	HWND dialog = OwnerDraw::Custom_Message_Box(Fetch_String(TXT_SAVING_GAME), NULL, NULL);
	if (dialog != 0) {
		OwnerDraw::Display_Dialog(dialog);
	}
	bool saved = SaveManager.Request_Save_Game(file_name, descr, false,
		SaveManagerClass::NoticeType::Requested);
	if (dialog != 0) {
		OwnerDraw::End_Dialog(dialog);
	}
	return(saved);
}


/// <summary>
/// A saved game reports itself in the message list at the frame boundary, so the dialog shows
/// no box of its own.
/// </summary>
int LoadOptionsClass::Save_Confirmation(void) const
{
	return(TXT_NONE);
}


/// <summary>
/// Removes the save game file specified.
/// </summary>
/// <returns>bool; Was the file deleted?</returns>
bool LoadOptionsClass::Delete_File(const char * file_name)
{
	if (DeleteFile(Saved_Game_Name(file_name).c_str()) == TRUE) {
		return(true);
	}
	return(false);
}


/// <summary>
/// Fills in a save game list entry from a file found on disk.
/// This routine peeks at the save game's header to recover the description, scenario
/// and player it belongs to. A save written by an older game version is still accepted,
/// but its description is marked so the player can tell.
/// </summary>
/// <param name="fdata">The list entry to fill in.</param>
/// <param name="ff">The find record naming the file to examine.</param>
/// <returns>bool; Was a usable save game found in the file?</returns>
bool LoadOptionsClass::Read_File(FileEntryClass * fdata, WIN32_FIND_DATAA * ff)
{
	if (fdata == NULL && ff == NULL) {
		return(false);
	}

	SaveVersionInfo savever;

	/*
	 * get the game's info;
	 */
	bool ok = Get_Savefile_Info(ff->cFileName, &savever);
	if (!ok) {
		return(false);
	}

	if (savever.Get_Internal_Version() != ExpectedGameVersion) {
		return(false);
	}

	wsprintf(fdata->Descr, "%s", savever.Get_Scenario_Description());

	fdata->Valid = ok;
	fdata->Scenario = savever.Get_Scenario_Number();
	fdata->Num = savever.Get_Campaign_Number();
	fdata->Type = (GameType)savever.Get_Game_Type();
	strcpy(fdata->Filename, ff->cFileName);
	strcpy(fdata->PlayerName, savever.Get_Player_House());
	if (strlen(fdata->Filename) == 0) {
		strcpy(fdata->Filename, ff->cAlternateFileName);
	}
	fdata->DateTime.dwHighDateTime = ff->ftLastWriteTime.dwHighDateTime;
	fdata->DateTime.dwLowDateTime = ff->ftLastWriteTime.dwLowDateTime;
	return(true);
}


MultiplayerLoadOptionsClass::MultiplayerLoadOptionsClass(void)
{
	Extension = "NET";
	Picked[0] = '\0';
}


/// <summary>
/// Records the pick without loading it; every machine loads together once the master asks.
/// </summary>
bool MultiplayerLoadOptionsClass::Load_File(const char * file_name)
{
	std::snprintf(Picked, sizeof(Picked), "%s", file_name);
	return(true);
}


/// <summary>
/// Lists a numbered save of this kind of game and nothing else.
/// </summary>
bool MultiplayerLoadOptionsClass::Read_File(FileEntryClass * entry, WIN32_FIND_DATAA * ff)
{
	if (entry == NULL || ff == NULL || Multiplayer_Save_Slot(ff->cFileName) < 0) {
		return(false);
	}
	return(LoadOptionsClass::Read_File(entry, ff) && entry->Type == Session.Type);
}
