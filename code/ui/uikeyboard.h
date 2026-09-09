/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The keyboard screen's behavior, with no toolkit in it. The screen rebinds the game's
// commands, so what it holds is a staged view of HotkeyCommands: assignments are made
// against the live index as the player works and are written to KEYBOARD.INI only on
// accept, while cancel throws the index away and loads the file again.
//
// The captured key is a plain integer in the game's own encoding, low byte the virtual key
// and the shift, control and alt bits above it, which is the encoding HotkeyCommands is
// indexed by. A view decides how to capture one; this class never sees the keypress.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_KEYBOARD_CATEGORY = "category";  // Value: row
inline constexpr char const * UI_KEYBOARD_COMMAND = "command";    // Value: row
inline constexpr char const * UI_KEYBOARD_CAPTURE = "capture";    // Value: encoded key
inline constexpr char const * UI_KEYBOARD_ASSIGN = "assign";
inline constexpr char const * UI_KEYBOARD_RESET = "reset";
inline constexpr char const * UI_KEYBOARD_ACCEPT = "accept";
inline constexpr char const * UI_KEYBOARD_CANCEL = "cancel";


class UIKeyboardPresenterClass : public UIPresenterClass
{
	public:
		// A command as the screen shows it. The unique name is the identity an intent is
		// resolved against, because the command list is rebuilt whenever the category
		// changes and a row number outlives nothing.
		struct CommandType
		{
			std::string Label;
			std::string Description;
			std::string UniqueName;
		};

		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_ACCEPT,
			CHOICE_CANCEL,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// Spells a key out the way the player's own keyboard layout names it, modifiers
		// first. Empty for a key of zero, which is what an unbound command carries.
		static std::string Key_Name(int key);

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		std::vector<std::string> Categories;
		int SelectedCategory = -1;

		std::vector<CommandType> Commands;
		int SelectedCommand = -1;

		// The selected command's description and the key it answers to now.
		std::string Description;
		std::string CurrentShortcut;

		// What the capture control is holding, and the command that key already belongs to.
		int CapturedKey = 0;
		std::string CapturedText;
		std::string AssignedTo;

		ChoiceType Choice = CHOICE_NONE;

	private:
		void Fill_Commands(void);
		void Show_Command(void);
		void Apply_Hotkey(void);
		void Reset_All(void);
		void Save_Assignments(void) const;
};
