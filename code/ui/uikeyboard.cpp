/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The keyboard screen. Behavior traced out of Hotkey_Dialog_Proc and its four private
// messages in options.cpp.
//
// What the extraction fixes in place, none of it obvious from the template: assigning with
// nothing captured CLEARS the selected command's key, because the dialog removed the old
// binding before it looked at the new one and only re-added when the key was not zero;
// assigning a key another command already holds TAKES it, because the key is removed from
// the index before it is added; cancel is not a no-op but a reload, since every assignment
// was already made against the live index; and reset deletes only the player's own
// KEYBOARD.INI, so the defaults a deployment ships are what is left.
//
// One inherited defect is preserved rather than repaired: changing the category left the
// command list with no selection, because the dialog handed ListBox_SetCurSel the
// description control instead of the list. The description therefore clears and the
// shortcut, the capture and the assigned-to text all keep whatever they were showing, until
// the player picks a command.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uikeyboard.h"

#include "_command.h"
#include "ccfile.h"
#include "ccini.h"
#include "cdfile.h"
#include "command.h"
#include "dbgprint.h"
#include "globals.h"
#include "init.h"
#include "language/language.h"
#include "msgbox.h"
#include "ownrdraw.h"
#include "vector.h"

#include <algorithm>
#include <cstring>


// Build_Hotkey_String lives in ownrdraw.cpp and is the only thing this screen wants from
// there. It spells a key, not a control, and moves with the rest of the keyboard support
// when OwnerDraw is retired.
std::string UIKeyboardPresenterClass::Key_Name(int key)
{
	char buffer[64];
	buffer[0] = '\0';
	Build_Hotkey_String((KeyNumType)key, buffer);
	return(std::string(buffer));
}


static CommandClass const * Find_Command(std::string const & unique)
{
	for (int index = 0; index < AllCommands.Count(); index++) {
		if (unique == AllCommands[index]->Get_Unique_Name()) {
			return(AllCommands[index]);
		}
	}
	return(NULL);
}


// The key a command answers to now, or zero when it answers to none.
static int Key_Of_Command(CommandClass const * command)
{
	for (int index = 0; index < HotkeyCommands.Count(); index++) {
		if (HotkeyCommands.Fetch_By_Position(index) == command) {
			return(HotkeyCommands.Fetch_ID_By_Position(index));
		}
	}
	return(0);
}


static bool Less_Ignoring_Case(std::string const & left, std::string const & right)
{
	return(stricmp(left.c_str(), right.c_str()) < 0);
}


/// <summary>
/// Rebuilds the whole screen from the command list and the live key assignments.
/// </summary>
void UIKeyboardPresenterClass::Refresh(void)
{
	Categories.clear();

	for (int index = 0; index < AllCommands.Count(); index++) {
		char const * const category = AllCommands[index]->Get_Category();
		if (category == NULL) continue;

		bool present = false;
		for (std::string const & known : Categories) {
			if (stricmp(known.c_str(), category) == 0) {
				present = true;
				break;
			}
		}
		if (!present) {
			Categories.push_back(category);
		}
	}

	// The combo box carried CBS_SORT, so the player sees the categories in order rather
	// than in the order the command list was built.
	std::sort(Categories.begin(), Categories.end(), Less_Ignoring_Case);

	SelectedCategory = Categories.empty() ? -1 : 0;

	CapturedKey = 0;
	CapturedText.clear();
	AssignedTo.clear();
	CurrentShortcut.clear();

	Fill_Commands();
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIKeyboardPresenterClass::Service(void)
{
	if (!GameActive) {
		Title_Screen_Restore();
	}
}


void UIKeyboardPresenterClass::Fill_Commands(void)
{
	Commands.clear();
	SelectedCommand = -1;
	Description.clear();

	if (SelectedCategory < 0 || SelectedCategory >= (int)Categories.size()) {
		return;
	}

	std::string const & category = Categories[SelectedCategory];

	for (int index = 0; index < AllCommands.Count(); index++) {
		CommandClass const * const command = AllCommands[index];
		if (command->Get_Category() == NULL) continue;
		if (stricmp(command->Get_Category(), category.c_str()) != 0) continue;

		CommandType entry;
		entry.Label = (command->Get_Display_Name() != NULL) ? command->Get_Display_Name() : "";
		entry.Description = (command->Get_Description() != NULL) ? command->Get_Description() : "";
		entry.UniqueName = command->Get_Unique_Name();
		Commands.push_back(entry);
	}

	// The list box carried LBS_SORT.
	std::sort(Commands.begin(), Commands.end(),
		[](CommandType const & left, CommandType const & right) {
			return(Less_Ignoring_Case(left.Label, right.Label));
		});
}


void UIKeyboardPresenterClass::Show_Command(void)
{
	if (SelectedCommand < 0 || SelectedCommand >= (int)Commands.size()) {
		return;
	}

	CommandType const & entry = Commands[SelectedCommand];
	CommandClass const * const command = Find_Command(entry.UniqueName);

	Description = entry.Description;
	CurrentShortcut = Key_Name(Key_Of_Command(command));

	// The capture control was emptied every time a command was shown, so a key captured for
	// one command cannot be assigned to the next by accident.
	CapturedKey = 0;
	CapturedText.clear();
	AssignedTo.clear();
}


void UIKeyboardPresenterClass::Apply_Hotkey(void)
{
	if (SelectedCommand < 0 || SelectedCommand >= (int)Commands.size()) {
		return;
	}

	CommandClass const * const command = Find_Command(Commands[SelectedCommand].UniqueName);
	if (command == NULL) {
		return;
	}

	for (int index = 0; index < HotkeyCommands.Count(); index++) {
		if (HotkeyCommands.Fetch_By_Position(index) == command) {
			HotkeyCommands.Remove_Index(HotkeyCommands.Fetch_ID_By_Position(index));
			break;
		}
	}

	if (CapturedKey != 0) {
		HotkeyCommands.Remove_Index(CapturedKey);
		HotkeyCommands.Add_Index(CapturedKey, command);
	}
}


void UIKeyboardPresenterClass::Reset_All(void)
{
	DebugString("Deleting users KEYBOARD.INI\n");

	// Only the player's own file is discarded; the defaults a deployment ships are what the
	// reset falls back on.
	CCFileClass file("KEYBOARD.INI");
	file.Delete();

	Init_Hotkeys();
	Refresh();
}


void UIKeyboardPresenterClass::Save_Assignments(void) const
{
	CCINIClass ini;
	ini.Clear();

	for (int index = 0; index < HotkeyCommands.Count(); index++) {
		CommandClass const * const command = HotkeyCommands.Fetch_By_Position(index);
		int const key = HotkeyCommands.Fetch_ID_By_Position(index);
		ini.Put_Int("Hotkey", command->Get_Unique_Name(), key);
	}

	CDFileClass file("Keyboard.ini");
	ini.Save(file, false);
}


void UIKeyboardPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_KEYBOARD_CATEGORY) {
		if (intent.Value >= 0 && intent.Value < (int)Categories.size() && intent.Value != SelectedCategory) {
			SelectedCategory = intent.Value;
			Fill_Commands();
		}
		return;
	}

	if (intent.Action == UI_KEYBOARD_COMMAND) {
		if (intent.Value >= 0 && intent.Value < (int)Commands.size()) {
			SelectedCommand = intent.Value;
			Show_Command();
		}
		return;
	}

	if (intent.Action == UI_KEYBOARD_CAPTURE) {
		CapturedKey = intent.Value;
		CapturedText = Key_Name(CapturedKey);

		AssignedTo.clear();
		if (HotkeyCommands.Is_Present(CapturedKey)) {
			CommandClass const * const holder = HotkeyCommands[CapturedKey];
			if (holder != NULL && holder->Get_Display_Name() != NULL) {
				AssignedTo = holder->Get_Display_Name();
			}
		}
		return;
	}

	if (intent.Action == UI_KEYBOARD_ASSIGN) {
		Apply_Hotkey();
		Show_Command();
		return;
	}

	if (intent.Action == UI_KEYBOARD_RESET) {
		// The second argument is the default response, a button position rather than a
		// control identifier, and the dialog passed one: Enter answers No.
		if (WWMessageBox()._Process(TXT_RESET_HOTKEYS, 1, TXT_YES, TXT_NO, TXT_NONE, false) == 0) {
			Reset_All();
		}
		return;
	}

	UIResult result;

	if (intent.Action == UI_KEYBOARD_ACCEPT) {
		Save_Assignments();
		Choice = CHOICE_ACCEPT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;

	} else if (intent.Action == UI_KEYBOARD_CANCEL) {
		// Every assignment was made against the live index, so leaving without accepting
		// has to put the file's assignments back.
		Init_Hotkeys();
		Choice = CHOICE_CANCEL;
		result.Outcome = UIResult::OUTCOME_CANCELLED;

	} else {
		return;
	}

	Result = result;
}
