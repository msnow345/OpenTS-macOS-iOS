/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The save game browser. Behavior traced out of LoadOptionsClass::Dialog and the three
// command handlers in loaddlg.cpp.
//
// What the extraction fixes in place: a row carries its entry rather than its position in
// the control; the action button is disabled with an empty list but every check that can
// refuse the operation is made when the button is pressed; a refused load, a refused save
// and a declined deletion all leave the screen standing, which is what putting the state
// back to pending did; and deleting the last game leaves the screen accepted, which is what
// the delete arm did by falling out of its own loop.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uisavebrowser.h"

#include "uirmlview.h"

#include "campaign.h"
#include "conquer.h"
#include "data.h"
#include "gamedirs.h"
#include "init.h"
#include "language/language.h"
#include "loaddlg.h"
#include "msgbox.h"
#include "saveload.h"
#include "vector.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Traits.h>

#include <cstdio>
#include <cstring>


// Is a saved game of this name already there? Asked before one is written, since a name the
// folder holds is written over rather than added to.
static bool Saved_Game_Exists(char const * name)
{
	return(GetFileAttributes(Saved_Game_Name(name).c_str()) != INVALID_FILE_ATTRIBUTES);
}


UISaveBrowserPresenterClass::UISaveBrowserPresenterClass(LoadOptionsClass & options, StyleType style) :
	Style(style),
	Options(options)
{
}


/// <summary>
/// Is there room on disk to save at all?
/// </summary>
bool UISaveBrowserPresenterClass::Can_Open(void)
{
	if (Style != STYLE_SAVE) {
		return(true);
	}

	if (Disk_Space_Available() >= Options.MinSpaceRequired) {
		return(true);
	}

	WWMessageBox().Process(TXT_DISKFULL, TXT_OK, TXT_NONE, TXT_NONE);
	return(false);
}


void UISaveBrowserPresenterClass::Refresh(void)
{
	Options.Build_List();

	Entries.clear();
	Selected = -1;

	for (int index = 0; index < Options.Files.Count(); index++) {
		FileEntryClass const * const file = Options.Files[index];

		EntryType entry;
		entry.Description = file->Descr;
		entry.Session = (file->Type != GAME_NORMAL);
		entry.Valid = file->Valid;

		if (file->DateTime.dwHighDateTime != (DWORD)-1 && file->DateTime.dwLowDateTime != (DWORD)-1) {
			FILETIME local;
			SYSTEMTIME stamp;
			char buffer[128];

			FileTimeToLocalFileTime(&file->DateTime, &local);
			FileTimeToSystemTime(&local, &stamp);

			GetDateFormat(LANG_USER_DEFAULT, TIME_NOMINUTESORSECONDS, &stamp, NULL, buffer, sizeof(buffer));
			entry.Date = buffer;
			GetTimeFormat(LANG_USER_DEFAULT, TIME_NOSECONDS, &stamp, NULL, buffer, sizeof(buffer));
			entry.Time = buffer;
		}

		Entries.push_back(entry);
	}

	// The load list opens on the first game it could actually read; the other two open on
	// their first row, which for a save is the empty slot.
	if (!Entries.empty()) {
		Selected = 0;

		if (Style == STYLE_LOAD) {
			for (size_t index = 0; index < Entries.size(); index++) {
				if (Entries[index].Valid) {
					Selected = (int)index;
					break;
				}
			}
		}
	}

	CanAct = !Entries.empty();
	ListChanged = true;

	Description.clear();
	if (Style == STYLE_SAVE && Options.Description != NULL) {
		Description = Options.Description;
	}
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UISaveBrowserPresenterClass::Service(void)
{
	if (Options.Callback != NULL) {
		Options.Callback();
	}

	if (!GameActive) {
		Title_Screen_Restore(false);
	}
}


void UISaveBrowserPresenterClass::Finish(bool accepted)
{
	Outcome = accepted;

	UIResult result;
	result.Outcome = accepted ? UIResult::OUTCOME_ACCEPTED : UIResult::OUTCOME_CANCELLED;
	Result = result;
}


/// <summary>
/// Loads the game the player picked, with the screen already out of the way.
/// A load that fails leaves the screen standing so another game can be tried, which is what
/// putting the dialog's state back to pending did.
/// </summary>
void UISaveBrowserPresenterClass::Run_Pending(void)
{
	if (Pending != SUB_LOAD) {
		return;
	}

	Pending = SUB_NONE;

	if (Selected < 0 || Selected >= Options.Files.Count()) {
		return;
	}

	if (!Options.Load_File(Options.Files[Selected]->Filename)) {
		WWMessageBox().Process(TXT_ERROR_LOADING_GAME, TXT_OK, TXT_NONE, TXT_NONE);
		return;
	}

	Finish(true);
}


void UISaveBrowserPresenterClass::Accept(void)
{
	// No row means no operation, which is the LB_ERR the driver tested for.
	if (Selected < 0 || Selected >= Options.Files.Count()) {
		return;
	}

	FileEntryClass * const entry = Options.Files[Selected];

	switch (Style) {
		case STYLE_LOAD:
			// The campaign list is read before the screen steps aside, where the dialog
			// read it, because the load needs it and a mission save carries a campaign.
			if (entry->Num != -1) {
				Init_Campaigns();
			}
			Pending = SUB_LOAD;
			break;

		case STYLE_SAVE: {
			if (Description.empty()) {
				WWMessageBox().Process(TXT_MUSTENTER_DESCRIPTION, TXT_OK, TXT_NONE, TXT_NONE);
				FocusDescription = true;
				return;
			}

			char picked[256];
			char const * filename = NULL;

			if (entry->Valid) {
				filename = entry->Filename;
			} else {
				Options.Pick_Filename(picked);
				filename = picked;
			}

			if (filename == NULL) {
				return;
			}

			// A name the folder already holds is written over, so it is confirmed first.
			if (Saved_Game_Exists(filename)
				&& WWMessageBox()._Process(TXT_CONFIRM_SAVE, 1, TXT_YES, TXT_NO, TXT_NONE)) {
				return;
			}

			if (!Options.Save_File(filename, Description.c_str())) {
				WWMessageBox().Process(TXT_ERROR_SAVING_GAME, TXT_OK, TXT_NONE, TXT_NONE);
				return;
			}

			int const confirmation = Options.Save_Confirmation();
			if (confirmation != TXT_NONE) {
				WWMessageBox().Process(confirmation, TXT_OK, TXT_NONE, TXT_NONE);
			}

			if (Options.Description != NULL) {
				strcpy(Options.Description, Description.c_str());
			}

			Finish(true);
			break;
		}

		case STYLE_DELETE: {
			char buffer[256];
			sprintf(buffer, "%s\n%s", Fetch_String(TXT_DELETE_FILE_QUERY), entry->Descr);

			if (WWMessageBox()._Process(buffer, 1, TXT_YES, TXT_NO, TXT_NONE)) {
				return;
			}

			Options.Delete_File(entry->Filename);

			Options.Files.Delete_Index(Selected);
			delete entry;
			Entries.erase(Entries.begin() + Selected);

			// The list stays open for another deletion; emptying it leaves the screen.
			Selected = Entries.empty() ? -1 : 0;
			CanAct = !Entries.empty();
			ListChanged = true;

			if (Entries.empty()) {
				Finish(true);
			}
			break;
		}
	}
}


void UISaveBrowserPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_SAVEBROWSER_SELECT) {
		if (intent.Value < 0 || intent.Value >= (int)Entries.size()) {
			return;
		}

		Selected = intent.Value;

		// Picking a row in the save list offers that game's description, so an existing
		// game can be written over without typing its name out again; the empty slot offers
		// the description the caller suggested.
		if (Style == STYLE_SAVE) {
			if (Entries[Selected].Valid) {
				Description = Entries[Selected].Description;
			} else if (Options.Description != NULL) {
				Description = Options.Description;
			}
			FocusDescription = true;
		}
		return;
	}

	if (intent.Action == UI_SAVEBROWSER_DESCRIBE) {
		Description = intent.Identity;
		if (Description.size() > DESCRIPTION_LIMIT) {
			Description.resize(DESCRIPTION_LIMIT);
		}
		return;
	}

	if (intent.Action == UI_SAVEBROWSER_ACCEPT) {
		Accept();
		return;
	}

	if (intent.Action == UI_SAVEBROWSER_CANCEL) {
		Finish(false);
		return;
	}
}


//---------------------------------------------------------------------------------------
// The RmlUi view.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the save game browser.
/// </summary>
class SaveBrowserViewClass : public UIRmlViewClass
{
	public:
		SaveBrowserViewClass(UISaveBrowserPresenterClass & presenter, char const * document) :
			UIRmlViewClass(presenter, document),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		void Press(Rml::String const & action);

		// The description the field is holding, which the save screen reads when the action
		// button is pressed rather than tracking, as the dialog read its edit control.
		std::string Field_Text(void) const;

		Rml::ElementFormControlInput * Field(void) const;

		UISaveBrowserPresenterClass & Screen;
};


Rml::ElementFormControlInput * SaveBrowserViewClass::Field(void) const
{
	if (Element == nullptr) {
		return(nullptr);
	}
	return(rmlui_dynamic_cast<Rml::ElementFormControlInput *>(Element->GetElementById("description")));
}


std::string SaveBrowserViewClass::Field_Text(void) const
{
	Rml::ElementFormControlInput * const field = Field();
	if (field == nullptr) {
		return(Screen.Description);
	}
	return(field->GetValue());
}


/// <summary>
/// Queues what a button or its key stands for.
/// The save screen reads the description out of the field here rather than tracking it,
/// because that is when the dialog read its edit control, and the read is queued ahead of
/// the action it is read for so the two execute in that order.
/// </summary>
void SaveBrowserViewClass::Press(Rml::String const & action)
{
	if (Screen.Style == UISaveBrowserPresenterClass::STYLE_SAVE && action == UI_SAVEBROWSER_ACCEPT) {
		std::string const text = Field_Text();
		if (text != Screen.Description) {
			Screen.Queue(UIIntent{UI_SAVEBROWSER_DESCRIBE, text, 0});
		}
	}

	Screen.Queue(UIIntent{action, "", 0});
}


void SaveBrowserViewClass::Bind(Rml::DataModelConstructor & model)
{
	if (auto entry = model.RegisterStruct<UISaveBrowserPresenterClass::EntryType>()) {
		entry.RegisterMember("description", &UISaveBrowserPresenterClass::EntryType::Description);
		entry.RegisterMember("date", &UISaveBrowserPresenterClass::EntryType::Date);
		entry.RegisterMember("time", &UISaveBrowserPresenterClass::EntryType::Time);
	}
	model.RegisterArray<std::vector<UISaveBrowserPresenterClass::EntryType>>();

	model.Bind("entries", &Screen.Entries);
	model.Bind("selected", &Screen.Selected);
	model.Bind("description", &Screen.Description);
	model.Bind("canact", &Screen.CanAct);

	model.BindEventCallback("pick",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_SAVEBROWSER_SELECT, "", (int)arguments[0].Get<float>()});
		});

	// The field is bound one way, so a value the model already holds is never queued back as
	// a change the player did not type.
	model.BindEventCallback("describe",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			Rml::String const value = event.GetParameter<Rml::String>("value", Rml::String());
			if (value == Screen.Description) return;
			Screen.Queue(UIIntent{UI_SAVEBROWSER_DESCRIBE, value, 0});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Press(arguments[0].Get<Rml::String>());
		});

	// Escape cancels and Enter presses the action button, which is what IsDialogMessage
	// delivered to a template that names IDCANCEL and no default push button.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Press(UI_SAVEBROWSER_CANCEL);
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Press(UI_SAVEBROWSER_ACCEPT);
			}
		});
}


void SaveBrowserViewClass::Sync(void)
{
	if (!Model) return;

	Model.DirtyVariable("entries");
	Model.DirtyVariable("selected");
	Model.DirtyVariable("description");
	Model.DirtyVariable("canact");

	// Picking a row, and a refused empty description, put the focus on the field with its
	// text selected, which is what the dialog did with SetFocus and Edit_SetSel.
	if (Screen.FocusDescription) {
		Screen.FocusDescription = false;
		if (Rml::ElementFormControlInput * const field = Field()) {
			field->Focus();
			field->Select();
		}
	}

	Screen.ListChanged = false;
}


/// <summary>
/// Shows the browser and waits for the player to leave it.
/// </summary>
UIResult UI_Save_Browser_Screen(UISaveBrowserPresenterClass & presenter)
{
	char const * document = "missionload.rml";
	if (presenter.Style == UISaveBrowserPresenterClass::STYLE_SAVE) {
		document = "missionsave.rml";
	} else if (presenter.Style == UISaveBrowserPresenterClass::STYLE_DELETE) {
		document = "missiondelete.rml";
	}

	SaveBrowserViewClass view(presenter, document);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	// Loading draws where this screen is, so the document steps aside for it, which is what
	// the dialog's own ShowWindow did.
	while (!presenter.Result.has_value()) {
		UI_Run_Modal(presenter, view);

		if (presenter.Pending == UISaveBrowserPresenterClass::SUB_NONE) {
			break;
		}

		view.Hide();
		presenter.Run_Pending();
		if (presenter.Result.has_value()) {
			break;
		}
		view.Show();
		view.Sync();
	}

	UIResult const result = presenter.Result.value_or(UIResult{});
	view.Close();
	return(result);
}
