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

#include "uiinternal.h"
#include "uirmlview.h"

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
#include "keyboard.h"
#include "vector.h"

#include "keyboard.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


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


//---------------------------------------------------------------------------------------
// The RmlUi view.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the keyboard screen.
/// </summary>
class KeyboardViewClass : public UIRmlViewClass
{
	public:
		KeyboardViewClass(UIKeyboardPresenterClass & presenter) :
			UIRmlViewClass(presenter, "keyboard.rml"),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		UIKeyboardPresenterClass & Screen;
};


// Is this the virtual key of a modifier on its own? A hotkey control holds nothing while
// only modifiers are down and takes the binding when a real key arrives, so a modifier
// pressed by itself is not a capture.
static bool Is_Modifier_Key(int key)
{
	return(key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU
		|| (key >= 0xA0 && key <= 0xA5));
}


void KeyboardViewClass::Bind(Rml::DataModelConstructor & model)
{
	if (auto command = model.RegisterStruct<UIKeyboardPresenterClass::CommandType>()) {
		command.RegisterMember("label", &UIKeyboardPresenterClass::CommandType::Label);
	}
	model.RegisterArray<std::vector<UIKeyboardPresenterClass::CommandType>>();
	model.RegisterArray<std::vector<std::string>>();

	model.Bind("categories", &Screen.Categories);
	model.Bind("category", &Screen.SelectedCategory);
	model.Bind("commands", &Screen.Commands);
	model.Bind("selectedcommand", &Screen.SelectedCommand);
	model.Bind("description", &Screen.Description);
	model.Bind("shortcut", &Screen.CurrentShortcut);
	model.Bind("capturedtext", &Screen.CapturedText);
	model.Bind("assignedto", &Screen.AssignedTo);

	// The combo box is bound one way, as every form control in this family is, so the
	// category the model holds cannot be re-queued as a change the player did not make.
	model.BindEventCallback("choose",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			Rml::String const value = event.GetParameter<Rml::String>("value", Rml::String());
			if (value.empty()) return;

			int const row = std::atoi(value.c_str());
			if (row == Screen.SelectedCategory) return;

			Screen.Queue(UIIntent{UI_KEYBOARD_CATEGORY, "", row});
		});

	model.BindEventCallback("pick",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_KEYBOARD_COMMAND, "", (int)arguments[0].Get<float>()});
		});

	// The capture control. It stands where msctls_hotkey32 stood, so it takes the key
	// itself and leaves the keys IsDialogMessage took from that control alone: Escape and
	// Enter still leave the screen and Tab still moves the focus.
	model.BindEventCallback("capture",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const identifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);

			if (identifier == Rml::Input::KI_ESCAPE || identifier == Rml::Input::KI_RETURN
				|| identifier == Rml::Input::KI_NUMPADENTER || identifier == Rml::Input::KI_TAB) {
				return;
			}

			int const key = UI_Virtual_Key(identifier);
			if (key == 0 || Is_Modifier_Key(key)) {
				return;
			}

			// The game's encoding is the virtual key with its modifier bits above it, and
			// those bits are the HOTKEYF_ values the hotkey control reported byte for byte.
			int encoded = key;
			if (event.GetParameter<bool>("shift_key", false)) encoded |= WWKEY_SHIFT_BIT;
			if (event.GetParameter<bool>("ctrl_key", false)) encoded |= WWKEY_CTRL_BIT;
			if (event.GetParameter<bool>("alt_key", false)) encoded |= WWKEY_ALT_BIT;

			event.StopPropagation();
			Screen.Queue(UIIntent{UI_KEYBOARD_CAPTURE, "", encoded});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// Escape cancels and Enter accepts, which is what IsDialogMessage delivered to a dialog
	// whose template names no default push button.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Screen.Queue(UIIntent{UI_KEYBOARD_CANCEL, "", 0});
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_KEYBOARD_ACCEPT, "", 0});
			}
		});
}


void KeyboardViewClass::Sync(void)
{
	if (!Model) return;

	Model.DirtyVariable("categories");
	Model.DirtyVariable("category");
	Model.DirtyVariable("commands");
	Model.DirtyVariable("selectedcommand");
	Model.DirtyVariable("description");
	Model.DirtyVariable("shortcut");
	Model.DirtyVariable("capturedtext");
	Model.DirtyVariable("assignedto");
}


/// <summary>
/// Shows the keyboard screen and waits for the player to leave it.
/// </summary>
UIResult UI_Keyboard_Screen(UIKeyboardPresenterClass & presenter)
{
	KeyboardViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
