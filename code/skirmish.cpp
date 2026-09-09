/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "skirmish.h"

#include "_rules.h"
#include "data.h"
#include "globals.h"
#include "goptions.h"
#include "houstype.h"
#include "netdlg2.h"
#include "init.h"
#include "language/language.h"
#include "mapgen.h"
#include "mplayer.h"
#include "msgbox.h"
#include "netshare.h"
#include "newmenu.h"
#include "ownrdraw.h"
#include "rules.h"
#include "ui/uiskirmish.h"
#include "ui/uishell.h"
#include "win.h"


INT_PTR CALLBACK Skirmish_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
BOOL Skirmish_On_WM_INITDIALOG(HWND window, WPARAM wparam, LPARAM lparam);


/// <summary>
/// Reads the controls the screen takes its settings from and queues what they hold.
/// The dialog read its sliders, name field and boxes when a button was pressed rather than
/// tracking them, because a keyboard or page move changes a track bar without raising the
/// notification a tracking handler would follow.
/// </summary>
static void Skirmish_Read_Controls(HWND window, UISkirmishPresenterClass & screen)
{
	static struct { int id; char const * name; } const sliders[] = {
		{ IDC_SKIRMISH_UNITCOUNT, UI_SKIRMISH_UNITCOUNT },
		{ IDC_SKIRMISH_CREDITS,   UI_SKIRMISH_CREDITS },
		{ IDC_SKIRMISH_TECHLEVEL, UI_SKIRMISH_TECHLEVEL },
		{ IDC_DIFFICULTY_SLIDER,  UI_SKIRMISH_AILEVEL },
		{ IDC_SKIRMISH_AIPLAYERS, UI_SKIRMISH_AIPLAYERS },
		{ IDC_GAME_SPEED_SLIDER,  UI_SKIRMISH_GAMESPEED },
	};

	for (auto const & entry : sliders) {
		HWND const handle = GetDlgItem(window, entry.id);
		if (handle) {
			screen.Queue(UIIntent{UI_SKIRMISH_SLIDER, entry.name, Slider_GetPos(handle)});
		}
	}

	char buffer[128];
	GetWindowText(GetDlgItem(window, IDC_SKIRMISH_NAME), buffer, sizeof(buffer));
	screen.Queue(UIIntent{UI_SKIRMISH_HANDLE, buffer, 0});

	HWND handle = GetDlgItem(window, IDC_SKIRMISH_SIDE);
	if (handle) {
		int const country = Country_From_Box(handle);
		for (int row = 0; row < (int)screen.Sides.size(); row++) {
			if (screen.Sides[row].Country == country) {
				screen.Queue(UIIntent{UI_SKIRMISH_SIDE, "", row});
				break;
			}
		}
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_COLOR);
	if (handle) {
		screen.Queue(UIIntent{UI_SKIRMISH_COLOR, "", (int)ComboBox_GetCurSel(handle)});
	}
}


/// <summary>
/// Handles a control notification from the skirmish dialog.
/// The controls are read into the view-model and the command is queued as an intent; the
/// driver executes the queue after the pump returns.
/// </summary>
void Skirmish_On_WM_COMMAND(HWND window, int message, WPARAM wparam, LPARAM lparam)
{
	UISkirmishPresenterClass * const screen =
		(UISkirmishPresenterClass *)GetWindowLongPtr(window, DWLP_USER);
	if (screen == NULL) {
		return;
	}

	switch (message) {
		case IDOK:
			if (lparam == 0) {
				EnableWindow(GetDlgItem(window, 1), FALSE);
				Skirmish_Read_Controls(window, *screen);
				screen->Queue(UIIntent{UI_SKIRMISH_ACCEPT, "", 0});
			}
			break;

		case IDCANCEL:
			if (!lparam) {
				Skirmish_Read_Controls(window, *screen);
				screen->Queue(UIIntent{UI_SKIRMISH_CANCEL, "", 0});
			}
			break;

		case IDC_SHORT_GAME:
			screen->Queue(UIIntent{UI_SKIRMISH_TOGGLE, UI_SKIRMISH_SHORTGAME, 0});
			break;

		case IDC_SKIRMISH_BASES:
			screen->Queue(UIIntent{UI_SKIRMISH_TOGGLE, UI_SKIRMISH_BASES, 0});
			break;

		case IDC_SKIRMISH_CRATES:
			screen->Queue(UIIntent{UI_SKIRMISH_TOGGLE, UI_SKIRMISH_CRATES, 0});
			break;

		case IDC_SKIRMISH_FOG:
			screen->Queue(UIIntent{UI_SKIRMISH_TOGGLE, UI_SKIRMISH_FOG, 0});
			break;

		case IDC_SKIRMISH_BRIDGES:
			screen->Queue(UIIntent{UI_SKIRMISH_TOGGLE, UI_SKIRMISH_BRIDGES, 0});
			break;

		case IDC_REDEPLOY_MCV:
			screen->Queue(UIIntent{UI_SKIRMISH_TOGGLE, UI_SKIRMISH_MCV, 0});
			break;

		case IDC_MULTI_ENGINEER:
			screen->Queue(UIIntent{UI_SKIRMISH_TOGGLE, UI_SKIRMISH_ENGINEER, 0});
			break;

		case IDC_MULTIMAP:
			screen->Queue(UIIntent{UI_SKIRMISH_PICK_MAP, "", 0});
			break;
	}
}


/// <summary>
/// Puts the view-model on the dialog's own controls.
/// </summary>
static void Skirmish_Sync_Controls(HWND window, UISkirmishPresenterClass & screen)
{
	static struct { int id; bool UISkirmishPresenterClass::* field; } const boxes[] = {
		{ IDC_SKIRMISH_BASES,   &UISkirmishPresenterClass::Bases },
		{ IDC_SKIRMISH_CRATES,  &UISkirmishPresenterClass::Crates },
		{ IDC_SKIRMISH_FOG,     &UISkirmishPresenterClass::FogOfWar },
		{ IDC_SKIRMISH_BRIDGES, &UISkirmishPresenterClass::Bridges },
		{ IDC_REDEPLOY_MCV,     &UISkirmishPresenterClass::MCVRedeploy },
		{ IDC_SHORT_GAME,       &UISkirmishPresenterClass::ShortGame },
		{ IDC_MULTI_ENGINEER,   &UISkirmishPresenterClass::MultiEngineer },
	};

	for (auto const & entry : boxes) {
		HWND const handle = GetDlgItem(window, entry.id);
		if (handle == NULL) continue;

		int const wanted = (screen.*(entry.field)) ? BST_CHECKED : BST_UNCHECKED;
		if (Button_GetCheck(handle) != wanted) {
			Button_SetCheck(handle, wanted);
		}
	}

	SendDlgItemMessage(window, IDC_SCENARIONAME, WM_SETTEXT, 0, (LPARAM)screen.ScenarioName.c_str());
	EnableWindow(GetDlgItem(window, 1), screen.CanAccept ? TRUE : FALSE);
}


/// <summary>
/// Handles the skirmish game setup dialog.
/// This routine is used by the main menu when the player picks a skirmish game. The house
/// and side rules are re-read first so that the dialog offers the current playable sides,
/// and the chosen settings are recorded as the player's multiplayer preferences on the way
/// out.
/// </summary>
/// <returns>bool; Did the player accept the settings and ask for the game to start?</returns>
bool Skirmish_Mode_Dialog(void)
{
	int rc = -1;

	Prepare_Side_Roster();

	Hide_Mouse();
	Draw_Menu_Background();
	Show_Mouse();

	UISkirmishPresenterClass screen;
	screen.Refresh();

	if (UI_Use_Rml()) {
		UIResult const result = UI_Skirmish_Screen(screen);
		if (result.Outcome != UIResult::OUTCOME_FAILED_TO_OPEN) {
			rc = screen.Accepted() ? IDOK : IDCANCEL;
		}
	}

	HWND dialog = rc == -1 ? OwnerDraw::Begin_Dialog(IDD_SKIRMISH, Skirmish_Dialog_Proc) : NULL;
	if (dialog) {
		SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR)&screen);
		Skirmish_Sync_Controls(dialog, screen);
		OwnerDraw::Display_Dialog(dialog);
		while (!screen.Result.has_value()) {
			if (OwnerDraw::Dialog_Message_Handler() == IDOK) {
				break;
			}

			// A control handler queues rather than acts, so the queue is executed here,
			// after the pump has returned.
			screen.Drain();

			// The map selection screen draws where this one is, so the dialog gets out of
			// its way, which is what its own ShowWindow did.
			if (screen.Pending != UISkirmishPresenterClass::SUB_NONE) {
				ShowWindow(dialog, SW_HIDE);
				screen.Run_Pending();
				ShowWindow(dialog, SW_SHOW);
				InvalidateRect(dialog, NULL, FALSE);
			}

			Skirmish_Sync_Controls(dialog, screen);
			screen.Service();
		}
		OwnerDraw::End_Dialog(dialog);
		rc = screen.Accepted() ? IDOK : IDCANCEL;
	}

	if (rc == -1) {
		rc = IDCANCEL;
	}

	screen.End();

	if (rc == IDCANCEL) {
		Hide_Mouse();
		Draw_Menu_Background();
		Show_Mouse();
	}

	if (rc == IDOK) {
		return(true);
	}

	return(false);
}


/// <summary>
/// Handles the messages sent to the skirmish setup dialog.
/// The owner draw dialog handler is given first refusal on every message. Anything it
/// leaves alone is dealt with here -- dialog setup, button and slider notifications, and
/// repainting the map preview.
/// </summary>
/// <returns>Returns with TRUE if the message was handled, otherwise FALSE so that Windows
/// performs its default processing.</returns>
INT_PTR CALLBACK Skirmish_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);

	if (rc == 0) {

		switch (message) {
			case WM_COMMAND:
				Skirmish_On_WM_COMMAND(window, LOWORD(wparam), lparam, HIWORD(wparam));
				return(TRUE);

			case WM_INITDIALOG:
				return(Skirmish_On_WM_INITDIALOG(window, wparam, lparam));

			case WM_PAINT:
				if (MultiplayerMapPreview) {
					MultiplayerMapPreview->Blit_Preview(window);
				}
				ValidateRect(window, NULL);
				break;

			case WM_HSCROLL: {
				int code = LOWORD(wparam);
				if (code != SB_THUMBPOSITION && code != SB_THUMBTRACK) {
					Slider_GetPos((HWND)lparam);
				}
				switch (GetDlgCtrlID((HWND)lparam)) {
					case IDC_SKIRMISH_UNITCOUNT:
						GetDlgItem(window, IDC_SKIRMISH_UNITCOUNT_LABEL);
						break;
					case IDC_SKIRMISH_TECHLEVEL:
						GetDlgItem(window, IDC_SKIRMISH_TECHLEVEL_LABEL);
						break;
					case IDC_DIFFICULTY_SLIDER:
						GetDlgItem(window, IDC_SKIRMISH_AILEVEL_LABEL);
						break;
					case IDC_SKIRMISH_AIPLAYERS:
						GetDlgItem(window, IDC_SKIRMISH_AIPLAYERS_LABEL);
						break;
				}
			}
				break;
		}
		return(FALSE);
	}
	return(rc);
}


/// <summary>
/// Prepares the skirmish dialog for display.
/// This routine fills the sliders, side and color combo boxes, and option check boxes
/// with the player's current multiplayer settings, selects the starting scenario, and
/// puts up its map preview.
/// </summary>
/// <returns>Always FALSE, so that Windows leaves the keyboard focus where the dialog
/// template put it.</returns>
BOOL Skirmish_On_WM_INITDIALOG(HWND window, WPARAM wparam, LPARAM lparam)
{
	#define MP_MIN_MONEY 2500

	HWND handle = GetDlgItem(window, IDC_SKIRMISH_UNITCOUNT);
	if (handle) {
		Slider_SetRange(handle, SessionClass::CountMin[1], SessionClass::CountMax[1]);
		Slider_SetPos(handle, Session.Options.UnitCount);
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_UNITCOUNT_LABEL);
	if (handle) {
		//
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_TECHLEVEL);
	if (handle) {
		Slider_SetRange(handle, 1, MPLAYER_BUILD_LEVEL_MAX);
		Slider_SetPos(handle, BuildLevel);
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_TECHLEVEL_LABEL);
	if (handle) {
		//
	}

	handle = GetDlgItem(window, IDC_DIFFICULTY_SLIDER);
	if (handle) {
		Slider_SetRange(handle, 0, 2);
		Slider_SetPos(handle, Session.Options.AIDifficulty);
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_AILEVEL_LABEL);
	if (handle) {
		//
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_CREDITS);
	if (handle) {
		Slider_SetRange(handle, MP_MIN_MONEY, Rule->MPMaxMoney);
		Slider_SetPos(handle, Session.Options.Credits);
		SendMessage(handle, OD_SETTRACKSTEP, 0, 250);
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_AIPLAYERS);
	if (handle) {
		Slider_SetRange(handle, 1, 7);
		Slider_SetPos(handle, Session.Options.AIPlayers > 1 ? Session.Options.AIPlayers : 1);
	}

	handle = GetDlgItem(window, IDC_GAME_SPEED_SLIDER);
	if (handle) {
		Slider_SetRange(handle, 0, 6);
		Slider_SetPos(handle, 6 - Session.Options.GameSpeed);
	}

	handle = GetDlgItem(window, IDC_SKIRMISH_NAME);
	if (handle) SetWindowText(handle, Session.Handle);

	handle = GetDlgItem(window, IDC_SKIRMISH_SIDE);
	if (handle) {
		Fill_Country_Box(handle);
		Select_Country_In_Box(handle, Session.House);
	}

	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_GOLD));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_RED));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_BLUE));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_GREEN));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_ORANGE));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_SKY_BLUE));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_PURPLE));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_INSERTSTRING, -1, (LPARAM)Fetch_String(TXT_PINK));
	SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, CB_SETCURSEL, Session.PrefColor, 0);

	for (int player = 0; player < MAX_PLAYERS; player++) {
		SendDlgItemMessage(window, IDC_SKIRMISH_COLOR, OD_SETCOLOR, player, (LPARAM)PlayerColorTable[player]);
	}

	Set_Scenario_Info_From_Index(0);
	Session.Options.ScenarioIndex = 0;
	SendDlgItemMessage(window, IDC_SCENARIONAME, WM_SETTEXT, 0, (LPARAM)Session.Options.ScenarioDescription);
	Clear_Vector(&Session.Players);
	Clear_Vector(&Session.Computers);

	handle = GetDlgItem(window, IDC_SKIRMISH_BASES);
	if (handle) Button_SetCheck(handle, Session.Options.Bases ? BST_CHECKED : BST_UNCHECKED);

	handle = GetDlgItem(window, IDC_SKIRMISH_CRATES);
	if (handle) Button_SetCheck(handle, Session.Options.Goodies ? BST_CHECKED : BST_UNCHECKED);

	handle = GetDlgItem(window, IDC_SKIRMISH_FOG);
	if (handle) Button_SetCheck(handle, Session.Options.FogOfWar ? BST_CHECKED : BST_UNCHECKED);

	handle = GetDlgItem(window, IDC_SKIRMISH_BRIDGES);
	if (handle) Button_SetCheck(handle, Session.Options.BridgeDestruction ? BST_CHECKED : BST_UNCHECKED);

	handle = GetDlgItem(window, IDC_REDEPLOY_MCV);
	if (handle) Button_SetCheck(handle, Session.Options.MCVRedeploy ? BST_CHECKED : BST_UNCHECKED);

	handle = GetDlgItem(window, IDC_MULTI_ENGINEER);
	if (handle) Button_SetCheck(handle, Session.Options.CrapEngineers ? BST_CHECKED : BST_UNCHECKED);

	handle = GetDlgItem(window, IDC_SHORT_GAME);
	if (handle) Button_SetCheck(handle, Session.Options.ShortGame ? BST_CHECKED : BST_UNCHECKED);

	Update_Network_Dialog_Preview(window);
	return(FALSE);
}
