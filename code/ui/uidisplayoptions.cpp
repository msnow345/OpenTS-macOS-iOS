/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The display options screen. Behavior traced out of Display_Options_Dialog_Body in
// mainopt.cpp.
//
// What the extraction fixes in place: the resolution is staged and only a trial the player
// confirms writes it to the settings, while the movie stretching preference is written
// straight to the settings at accept and left alone at cancel; and the staged resolution
// moves only when the player leaves the screen on a row other than the one it opened on, so
// re-picking the row already in force stages nothing and skips the trial.
//
// EnumDisplayModes reports nothing on a platform without host mode enumeration, and this
// screen then offers an empty list, which is what the dialog did with the same answer.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uidisplayoptions.h"

#include "uirmlview.h"

#include "globals.h"
#include "init.h"
#include "goptions.h"
#include "options.h"
#include "video.h"

#include <cstdio>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


void UIDisplayOptionsPresenterClass::Refresh(void)
{
	Modes.clear();
	Selected = -1;

	StagedWidth = Options.ScreenWidth;
	StagedHeight = Options.ScreenHeight;
	StretchMovies = Options.StretchMovies;

	int * const modes = EnumDisplayModes(MIN_WIDTH, MIN_HEIGHT, MAX_WIDTH, MAX_HEIGHT);
	if (modes != NULL) {
		for (int * mode = modes; *mode != 0; mode += 2) {
			ModeType entry;
			entry.Width = mode[0];
			entry.Height = mode[1];

			char buffer[64];
			std::snprintf(buffer, sizeof(buffer), "%d x %d", entry.Width, entry.Height);
			entry.Label = buffer;

			if (entry.Width == StagedWidth && entry.Height == StagedHeight) {
				Selected = (int)Modes.size();
			}

			Modes.push_back(entry);
		}
		delete [] modes;
	}

	Opened = Selected;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIDisplayOptionsPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


bool UIDisplayOptionsPresenterClass::Wants_Mode_Change(void) const
{
	return(StagedWidth != Options.ScreenWidth || StagedHeight != Options.ScreenHeight);
}


void UIDisplayOptionsPresenterClass::Commit(void)
{
	Options.ScreenWidth = StagedWidth;
	Options.ScreenHeight = StagedHeight;
}


void UIDisplayOptionsPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_DISPLAY_SELECT) {
		if (intent.Value >= 0 && intent.Value < (int)Modes.size()) {
			Selected = intent.Value;
		}
		return;
	}

	if (intent.Action == UI_DISPLAY_STRETCH) {
		StretchMovies = (intent.Value != 0);
		return;
	}

	UIResult result;

	if (intent.Action == UI_DISPLAY_ACCEPT) {
		if (Selected != Opened && Selected >= 0 && Selected < (int)Modes.size()) {
			StagedWidth = Modes[Selected].Width;
			StagedHeight = Modes[Selected].Height;
		}

		// The stretching preference is not staged. The dialog wrote it at IDOK and left it
		// alone at IDCANCEL, so it survives a resolution the player then refuses.
		Options.StretchMovies = StretchMovies;

		Choice = CHOICE_ACCEPT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;

	} else if (intent.Action == UI_DISPLAY_CANCEL) {
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
/// The RmlUi half of the display options screen.
/// </summary>
class DisplayOptionsViewClass : public UIRmlViewClass
{
	public:
		DisplayOptionsViewClass(UIDisplayOptionsPresenterClass & presenter) :
			UIRmlViewClass(presenter, "display.rml"),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		UIDisplayOptionsPresenterClass & Screen;
};


void DisplayOptionsViewClass::Bind(Rml::DataModelConstructor & model)
{
	if (auto mode = model.RegisterStruct<UIDisplayOptionsPresenterClass::ModeType>()) {
		mode.RegisterMember("label", &UIDisplayOptionsPresenterClass::ModeType::Label);
	}
	model.RegisterArray<std::vector<UIDisplayOptionsPresenterClass::ModeType>>();

	model.Bind("modes", &Screen.Modes);
	model.Bind("selected", &Screen.Selected);
	model.Bind("stretch", &Screen.StretchMovies);

	model.BindEventCallback("pick",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_DISPLAY_SELECT, "", (int)arguments[0].Get<float>()});
		});

	model.BindEventCallback("toggle",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const &) {
			Screen.Queue(UIIntent{UI_DISPLAY_STRETCH, "", Screen.StretchMovies ? 0 : 1});
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
				Screen.Queue(UIIntent{UI_DISPLAY_CANCEL, "", 0});
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_DISPLAY_ACCEPT, "", 0});
			}
		});
}


void DisplayOptionsViewClass::Sync(void)
{
	if (!Model) return;

	Model.DirtyVariable("selected");
	Model.DirtyVariable("stretch");
}


/// <summary>
/// Shows the display options and waits for the player to leave them.
/// </summary>
UIResult UI_Display_Options_Screen(UIDisplayOptionsPresenterClass & presenter)
{
	DisplayOptionsViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
