/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "hostclock.h"
#include "always.h"

#include "desyncdlg.h"

#include "_map.h"
#include "_rect.h"
#include "_surface.h"
#include "_xmouse.h"
#include "chat.h"
#include "conquer.h"
#include "data.h"
#include "dbgprint.h"
#include "dsurface.h"
#include "globals.h"
#include "house.h"
#include "ipxmgr.h"
#include "language/language.h"
#include "loaddlg.h"
#include "misc.h"
#include "mpload.h"
#include "netdlg.h"
#include "netglobal.h"
#include "ownrdraw.h"
#include "savemgr.h"
#include "session.h"
#include "srfcache.h"
#include "ui/uishell.h"
#include "syncreport.h"
#include "win.h"
#include "windlg.h"
#include "winfix.h"

#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <cstring>


namespace {

	// Column positions within the player list, in the units the lobby's lists use.
	constexpr int HOST_COLUMN_X = 2;
	constexpr int NAME_COLUMN_X = 20;
	constexpr int STATUS_COLUMN_WIDTH = 56;
	constexpr int CHAT_BACKLOG_MAX = 50;


	// The dialog's pixel size follows the presentation layout, so the column is measured.
	int Status_Column_X(HWND list)
	{
		RECT rect = {};
		GetClientRect(list, &rect);
		return(rect.right - STATUS_COLUMN_WIDTH);
	}

}	// namespace


/// <summary>
/// Shows the screen and runs it until the master has decided, or this player has quit. Game
/// logic is halted for the duration; chat, sign-offs, heartbeats and the master's decision
/// still come through, since the network is serviced the whole time.
/// </summary>
DesyncDialogClass::OutcomeType DesyncDialogClass::Run(void)
{
	DebugString("Out-of-sync dialog opening on frame %d\n", Frame);

	// A raised suspension makes a nested dialog's pump service the network instead of the game.
	TacticalActive = false;
	Session.Suspended++;

	IsRunning = true;
	Screen.Open();
	UI_Set_Desync_Screen(&Screen);

	OutcomeType outcome = OutcomeType::Continue;

	// The presentation is latched here, at screen entry, and a document that will not
	// prepare drops the screen back to the legacy dialog.
	bool answered = false;
	if (UI_Use_Rml()) {
		UIResult const answer = UI_Desync_Run(Screen);
		answered = answer.Outcome != UIResult::OUTCOME_FAILED_TO_OPEN;
	}
	UI_Desync_Close_View();

	if (!answered) {
		Run_Legacy();
	}

	switch (Screen.Outcome) {
		case UIDesyncPresenterClass::OUTCOME_LOAD: outcome = OutcomeType::Load; break;
		case UIDesyncPresenterClass::OUTCOME_QUIT: outcome = OutcomeType::Quit; break;
		default:                                   outcome = OutcomeType::Continue; break;
	}

	UI_Set_Desync_Screen(NULL);
	IsRunning = false;

	Session.Suspended--;
	TacticalActive = true;
	Map.Flag_To_Redraw(GS_REDRAW_ALL);

	DebugString("Out-of-sync dialog closed with outcome %d\n", (int)outcome);
	return(outcome);
}


/// <summary>
/// Runs the OwnerDraw dialog against the same screen, for a build whose documents will not
/// prepare. It puts the model on its controls and queues an intent from a control.
/// </summary>
DesyncDialogClass::OutcomeType DesyncDialogClass::Run_Legacy(void)
{
	CountdownShown = false;
	DrawnMessages = 0;

	Create_Dialog();
	if (Window == NULL) {
		DebugString("The out-of-sync dialog could not be created; continuing\n");
		return(OutcomeType::Continue);
	}

	while (!Screen.Result.has_value()) {
		Call_Back();

		Screen.Service();
		Screen.Drain();

		Become_Host_If_Promoted();

		if (Screen.PromptPending) {
			EnableWindow(Window, FALSE);
			Screen.Run_Pending();
			EnableWindow(Window, TRUE);
			SetFocus(GetDlgItem(Window, IDC_DESYNC_PLAYER_LIST));
		}

		if (Screen.PlayersChanged) {
			Update_Player_List();
			Screen.PlayersChanged = false;
		}
		if (Screen.MessagesChanged) {
			Refill_Chat_List();
			Screen.MessagesChanged = false;
		}

		EnableWindow(GetDlgItem(Window, IDC_DESYNC_QUIT), Screen.CanQuit ? TRUE : FALSE);
		if (IsHostDialog) {
			EnableWindow(GetDlgItem(Window, IDC_DESYNC_LOAD), Screen.CanLoad ? TRUE : FALSE);
			EnableWindow(GetDlgItem(Window, IDC_DESYNC_CONTINUE), Screen.CanContinue ? TRUE : FALSE);
		}

		Update_Countdown();

		Host_Sleep(10);
	}

	Destroy_Dialog();
	return(OutcomeType::Continue);
}


void DesyncDialogClass::Service(void)
{
	if (!Is_Active()) {
		return;
	}

	// The RmlUi runner services the screen itself; this is the path the network maintenance
	// takes while a nested dialog owns the pump.
	if (Window != NULL) {
		Screen.Service();
	}
}


void DesyncDialogClass::Notify_Chat(char const * name, char const * text)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Record_Chat(name, text);
}


void DesyncDialogClass::Notify_Player_Left(int house, char const * name)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Player_Left(house, name);
}


void DesyncDialogClass::Notify_Continue(void)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Master_Decided_To_Continue();
}


void DesyncDialogClass::Notify_Heartbeat(int house)
{
	if (Is_Active()) {
		Screen.Heartbeat_Heard(house);
	}
}


void DesyncDialogClass::Notify_Master_Changed(void)
{
	if (!Is_Active()) {
		return;
	}

	Screen.Master_Changed();
}


/// <summary>
/// Creates the variant the local player gets: the decision dialog for the master, the wait
/// dialog for everyone else.
/// </summary>
void DesyncDialogClass::Create_Dialog(void)
{
	IsHostDialog = Screen.IsMaster;
	int const id = IsHostDialog ? IDD_DESYNC_HOST : IDD_DESYNC_WAIT;

	Window = WS_Create_Dialog(ProgramInstance, id, MainWindow, Dialog_Proc, FALSE);
	if (Window == NULL) {
		return;
	}

	Fit_To_Screen();
	Center_Window_Within_Window(Window);

	RECT placed;
	GetWindowRect(Window, &placed);
	MapWindowPoints(HWND_DESKTOP, MainWindow, (POINT *)&placed, 2);
	DebugString("Out-of-sync dialog placed at %d,%d size %dx%d in a %dx%d view\n",
		placed.left, placed.top, placed.right - placed.left, placed.bottom - placed.top, VideoModeWidth, VideoModeHeight);

	// The name column goes first: the list draws each row's own string in the first column added.
	HWND list = GetDlgItem(Window, IDC_DESYNC_PLAYER_LIST);
	if (list != NULL) {
		int const status_x = Status_Column_X(list);
		SendMessage(list, OD_ADDCOLUMN, status_x - NAME_COLUMN_X - 6, NAME_COLUMN_X);
		SendMessage(list, OD_ADDCOLUMN, 0, HOST_COLUMN_X);
		SendMessage(list, OD_ADDCOLUMN, 0, status_x);
	}
	Update_Player_List();

	if (IsHostDialog) {
		EnableWindow(GetDlgItem(Window, IDC_DESYNC_LOAD), Screen.CanLoad);
		EnableWindow(GetDlgItem(Window, IDC_DESYNC_CONTINUE), Screen.CanContinue);
	}
	EnableWindow(GetDlgItem(Window, IDC_DESYNC_QUIT), Screen.CanQuit);

	Refill_Chat_List();

	HWND edit = GetDlgItem(Window, IDC_DESYNC_CHAT_EDIT);
	if (edit != NULL) {
		SetWindowText(edit, Fetch_String(TXT_CHAT_HINT));
		ChatPlaceholderActive = true;
	}

	CountdownShown = false;
	Update_Countdown();

	MouseCursor->Hide_Mouse();
	ShowWindow(Window, SW_SHOWNORMAL);
	UpdateWindow(Window);
	MouseCursor->Show_Mouse();

	// The player list takes the focus, or the dialog would hand it to the chat box and clear the hint.
	SetForegroundWindow(Window);
	SetFocus(GetDlgItem(Window, IDC_DESYNC_PLAYER_LIST));
}


void DesyncDialogClass::Destroy_Dialog(void)
{
	if (Window != NULL) {
		WS_Destroy_Dialog(Window, 0);
		Window = NULL;
	}
}


/// <summary>
/// Takes the excess height out of the chat list when the presented dialog is taller than the
/// screen, and moves everything below the list up by the same amount.
/// </summary>
void DesyncDialogClass::Fit_To_Screen(void)
{
	RECT dialog_rect;
	GetWindowRect(Window, &dialog_rect);
	int const dialog_height = dialog_rect.bottom - dialog_rect.top;
	if (dialog_height <= VideoModeHeight) {
		return;
	}

	HWND chat = GetDlgItem(Window, IDC_DESYNC_CHAT_LIST);
	if (chat == NULL) {
		return;
	}
	RECT chat_rect;
	GetWindowRect(chat, &chat_rect);
	int const chat_height = chat_rect.bottom - chat_rect.top;

	int const delta = std::min<int>(dialog_height - VideoModeHeight, chat_height * 2 / 3);
	SetWindowPos(chat, NULL, 0, 0, chat_rect.right - chat_rect.left, chat_height - delta,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	for (int id : {IDC_DESYNC_CHAT_EDIT, IDC_DESYNC_COUNTDOWN_TEXT, IDC_DESYNC_COUNTDOWN_BAR,
			IDC_DESYNC_LOAD, IDC_DESYNC_CONTINUE, IDC_DESYNC_QUIT}) {
		HWND control = GetDlgItem(Window, id);
		if (control != NULL) {
			RECT rect;
			GetWindowRect(control, &rect);
			MapWindowPoints(HWND_DESKTOP, Window, (POINT *)&rect, 1);
			SetWindowPos(control, NULL, rect.left, rect.top - delta, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}

	SetWindowPos(Window, NULL, 0, 0, dialog_rect.right - dialog_rect.left, dialog_height - delta,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}


/// <summary>
/// Replaces the wait dialog with the decision dialog once this machine has become master,
/// unless a load is already counting down, when there is nothing left to decide.
/// </summary>
void DesyncDialogClass::Become_Host_If_Promoted(void)
{
	if (Window == NULL || IsHostDialog == Screen.IsMaster) {
		return;
	}

	DebugString("This machine is the new master; switching to the decision dialog\n");
	Destroy_Dialog();
	Create_Dialog();
}


void DesyncDialogClass::Update_Player_List(void)
{
	if (Window == NULL) {
		return;
	}

	HWND list = GetDlgItem(Window, IDC_DESYNC_PLAYER_LIST);
	if (list == NULL) {
		return;
	}

	ListBox_ResetContent(list);

	int const status_x = Status_Column_X(list);

	for (UIDesyncPresenterClass::PlayerRowType const & player : Screen.Players) {
		int const row = ListBox_AddString(list, player.Name.c_str());
		if (row < 0) {
			continue;
		}

		if (player.IsHost) {
			OwnerDraw::CellData host;
			host.type = OwnerDraw::CellData::SURFACE;
			host.surf = SurfaceCache.GetSurface("wolhost.pcx");
			host.hint.set("");
			SendMessage(list, OD_SETCELL, MAKEWPARAM(HOST_COLUMN_X, row), (LPARAM)&host);
		}

		int text = TXT_OK;
		COLORREF color = RGB(0, 200, 0);
		if (player.Status == UIDesyncPresenterClass::STATUS_LEFT) {
			text = TXT_SYNC_STATUS_LEFT;
			color = RGB(200, 0, 0);
		} else if (player.Status == UIDesyncPresenterClass::STATUS_OUT_OF_SYNC) {
			text = TXT_SYNC_STATUS_OUT;
			color = RGB(200, 200, 0);
		}

		OwnerDraw::CellData status;
		status.type = OwnerDraw::CellData::TEXT;
		status.string.set(Fetch_String(text));
		status.hint.set("");
		status.color = color;
		SendMessage(list, OD_SETCELL, MAKEWPARAM(status_x, row), (LPARAM)&status);
	}

	InvalidateRect(list, NULL, FALSE);
}


void DesyncDialogClass::Refill_Chat_List(void)
{
	if (Window == NULL) {
		return;
	}

	HWND list = GetDlgItem(Window, IDC_DESYNC_CHAT_LIST);
	if (list == NULL) {
		return;
	}

	ListBox_ResetContent(list);
	for (std::string const & line : Screen.Messages) {
		ListBox_AddString(list, line.c_str());
	}
	ListBox_SetTopIndex(list, ListBox_GetCount(list) - 1);
}


void DesyncDialogClass::Send_Chat(void)
{
	if (Window == NULL || ChatPlaceholderActive) {
		return;
	}

	HWND edit = GetDlgItem(Window, IDC_DESYNC_CHAT_EDIT);
	if (edit == NULL) {
		return;
	}

	char buffer[MAX_MESSAGE_LENGTH];
	GetWindowText(edit, buffer, sizeof(buffer));
	if (buffer[0] == '\0') {
		return;
	}

	SetWindowText(edit, "");
	SetFocus(edit);

	Screen.Queue(UIIntent{UI_DESYNC_SAY, buffer, 0});
}


void DesyncDialogClass::On_Chat_Edit_Focus(bool gained)
{
	if (Window == NULL) {
		return;
	}

	HWND edit = GetDlgItem(Window, IDC_DESYNC_CHAT_EDIT);
	if (edit == NULL) {
		return;
	}

	if (gained && ChatPlaceholderActive) {
		SetWindowText(edit, "");
		ChatPlaceholderActive = false;
	} else if (!gained && GetWindowTextLength(edit) == 0) {
		SetWindowText(edit, Fetch_String(TXT_CHAT_HINT));
		ChatPlaceholderActive = true;
	}
}


/// <summary>
/// Shows the countdown once a load is scheduled and keeps its text and bar current.
/// </summary>
void DesyncDialogClass::Update_Countdown(void)
{
	if (Window == NULL || !Screen.CountdownActive) {
		return;
	}

	if (!CountdownShown) {
		CountdownShown = true;
		ShowWindow(GetDlgItem(Window, IDC_DESYNC_COUNTDOWN_TEXT), SW_SHOW);
		ShowWindow(GetDlgItem(Window, IDC_DESYNC_COUNTDOWN_BAR), SW_SHOW);
	}

	SetDlgItemText(Window, IDC_DESYNC_COUNTDOWN_TEXT, Screen.CountdownText.c_str());
	InvalidateRect(Window, NULL, FALSE);
}


/// <summary>
/// Draws the countdown bar over its placeholder the way the reconnect dialog draws its sync
/// bars: shrinking, and green to yellow to red as the load nears.
/// </summary>
void DesyncDialogClass::Draw_Countdown_Bar(HWND window)
{
	if (!CountdownShown || DesyncDialog.Screen.CountdownTotal <= 0) {
		return;
	}

	HWND bar = GetDlgItem(window, IDC_DESYNC_COUNTDOWN_BAR);
	if (bar == NULL) {
		return;
	}

	RECT winrect;
	Get_Display_Rect(bar, &winrect);

	Rect bar_rect;
	bar_rect.X = winrect.left;
	bar_rect.Y = winrect.top;
	bar_rect.Width = winrect.right - winrect.left;
	bar_rect.Height = winrect.bottom - winrect.top;

	int const total = DesyncDialog.Screen.CountdownTotal;
	int const remaining = std::clamp(DesyncDialog.Screen.CountdownRemaining, 0, total);
	int const elapsed = total - remaining;

	unsigned short color = DSurface::Build_Hicolor_Pixel(0, 200, 0);
	if (elapsed > total * 2 / 5) {
		color = DSurface::Build_Hicolor_Pixel(200, 200, 0);
		if (elapsed > total * 4 / 5) {
			color = DSurface::Build_Hicolor_Pixel(200, 0, 0);
		}
	}

	bar_rect.Width = std::max(6, bar_rect.Width * remaining / total);

	AlternateSurface->Fill_Rect(AlternateSurface->Get_Rect(), bar_rect, color);
}


INT_PTR CALLBACK DesyncDialogClass::Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
		case WM_INITDIALOG:
			OwnerDraw::Subclass_Dialog(window, 0);
			break;

		case WM_DRAWITEM:
			OwnerDraw::Draw_Item((DRAWITEMSTRUCT *)lparam);
			return(TRUE);

		case WM_PAINT:
			OwnerDraw::Draw_Dialog_Back(window);
			DesyncDialog.Draw_Countdown_Bar(window);
			ValidateRect(window, NULL);
			break;

		case WM_MOVING:
			return(On_WM_MOVING(window, wparam, lparam));

		case WM_CTLCOLORMSGBOX:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
		case WM_CTLCOLORBTN:
		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSCROLLBAR:
		case WM_CTLCOLORSTATIC:
			return((INT_PTR)GetStockObject(BLACK_BRUSH));

		case WM_ERASEBKGND:
			return(TRUE);

		case WM_COMMAND:
			switch (LOWORD(wparam)) {
				case IDC_DESYNC_LOAD:
					DesyncDialog.Screen.Queue(UIIntent{UI_DESYNC_LOAD, "", 0});
					break;

				case IDC_DESYNC_CONTINUE:
					DesyncDialog.Screen.Queue(UIIntent{UI_DESYNC_CONTINUE, "", 0});
					break;

				case IDC_DESYNC_QUIT:
					DesyncDialog.Screen.Queue(UIIntent{UI_DESYNC_QUIT, "", 0});
					break;

				// Enter in the chat box arrives as IDOK, since the dialog has no default button.
				case IDOK:
					DesyncDialog.Send_Chat();
					break;

				case IDC_DESYNC_CHAT_EDIT:
					if (HIWORD(wparam) == EN_SETFOCUS) {
						DesyncDialog.On_Chat_Edit_Focus(true);
					} else if (HIWORD(wparam) == EN_KILLFOCUS) {
						DesyncDialog.On_Chat_Edit_Focus(false);
					}
					break;
			}
			break;
	}

	return(FALSE);
}
