/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The save game browser's behavior, with no toolkit in it. One screen serves loading,
// saving and deleting, because the three templates differ by which controls exist and by
// what the action button does, not by how the list is built.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>

class LoadOptionsClass;


inline constexpr char const * UI_SAVEBROWSER_SELECT = "select";      // Value: row
inline constexpr char const * UI_SAVEBROWSER_DESCRIBE = "describe";  // Identity: the text
inline constexpr char const * UI_SAVEBROWSER_ACCEPT = "accept";
inline constexpr char const * UI_SAVEBROWSER_CANCEL = "cancel";


class UISaveBrowserPresenterClass : public UIPresenterClass
{
	public:
		enum StyleType {
			STYLE_LOAD,
			STYLE_SAVE,
			STYLE_DELETE,
		};

		// A screen this one opens on top of itself. Loading takes a while and draws where
		// this screen is, so the view steps aside for it, which is what the dialog's own
		// ShowWindow did.
		enum SubScreenType {
			SUB_NONE,
			SUB_LOAD,
		};

		struct EntryType
		{
			std::string Description;

			// The date and the time the list showed in its own two columns.
			std::string Date;
			std::string Time;

			// Was the game a multiplayer one? The list marked those with a star.
			bool Session = false;

			// Does the row stand for a saved game? The save list opens with one row that
			// does not, which is the empty slot.
			bool Valid = false;
		};

		UISaveBrowserPresenterClass(LoadOptionsClass & options, StyleType style);

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;
		virtual bool Suspends(void) const override { return(Pending != SUB_NONE); }

		// Is there room on disk to save at all? Reports the shortage where the dialog
		// reported it, before anything is shown.
		bool Can_Open(void);

		// Runs the sub-screen an executed intent asked for and clears the request. Safe to
		// call with nothing pending.
		void Run_Pending(void);

		// Did the player go through with the operation? This is what the callers read.
		bool Accepted(void) const { return(Outcome); }

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		StyleType Style;

		std::vector<EntryType> Entries;
		int Selected = -1;

		// What the description field holds. The save screen is the only style that has one.
		std::string Description;

		// The longest description the field accepts, in bytes, which is what the dialog
		// capped the edit control at.
		enum { DESCRIPTION_LIMIT = 79 };

		// Is there anything for the action button to act upon? The dialog disabled it with
		// an empty list.
		bool CanAct = false;

		// Should the description field take the focus with its text selected? The dialog
		// did that when a row was picked and when it refused an empty description.
		bool FocusDescription = false;

		SubScreenType Pending = SUB_NONE;

		// Has the list itself changed since a view last drew it? Only a deletion moves it,
		// so a view that rebuilds a control has one thing to test.
		bool ListChanged = false;

	private:
		void Accept(void);
		void Finish(bool accepted);

		LoadOptionsClass & Options;
		bool Outcome = false;
};


// Shows the browser through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_Save_Browser_Screen(UISaveBrowserPresenterClass & presenter);
