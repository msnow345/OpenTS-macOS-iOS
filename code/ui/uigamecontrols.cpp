/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The game controls screen. Behavior traced out of gamedlg.cpp.
//
// What the extraction fixes in place, none of it obvious from the templates: leaving
// through the sound or the keyboard button applies and saves the settings, because both
// wrote the same IDOK the accept button did; the difficulty is applied only with no game
// running, so the slider the in-game template also carries is never read; a game speed
// change during a network session is issued as an event instead of being written, so every
// player stays in step; and the internet variant carries no game speed slider at all.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uigamecontrols.h"

#include "uirmlview.h"

#include "_map.h"
#include "audio/audioengine.h"
#include "_tooltip.h"
#include "cctooltip.h"
#include "data.h"
#include "event.h"
#include "gamedlg.h"
#include "globals.h"
#include "house.h"
#include "init.h"
#include "language/language.h"
#include "options.h"
#include "session.h"
#include "techno.h"

#include "special.hh"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


static void Fill_Labels(std::vector<std::string> & labels, int const * names, int count)
{
	labels.clear();
	for (int index = 0; index < count; index++) {
		labels.push_back(Fetch_String(names[index]));
	}
}


void UIGameControlsPresenterClass::Refresh(void)
{
	if (GameActive) {
		Variant = (Session.Type == GAME_INTERNET) ? VARIANT_INTERNET : VARIANT_SESSION;
	} else {
		Variant = VARIANT_FRONTEND;
	}

	SpeedStep = (OptionsClass::MAX_SPEED_SETTING - 1) - Options.GameSpeed;
	ScrollStep = (OptionsClass::MAX_SCROLL_SETTING - 1) - Options.ScrollRate;
	DetailStep = Options.DetailLevel;
	DifficultyStep = Options.Difficulty;

	CameoText = Options.SidebarCameoText;
	ActionLines = Options.ActionLines;
	ShowToolTips = Options.ToolTips;
	Coasting = (Options.ScrollMethod == 0);
	EdgeScroll = Options.AutoScroll;

	SoundAvailable = AudioEngine.Is_Available();

	Fill_Labels(SpeedLabels, GameSpeedNames, OptionsClass::MAX_SPEED_SETTING);
	Fill_Labels(ScrollLabels, GameScrollSpeedNames, OptionsClass::MAX_SCROLL_SETTING);
	Fill_Labels(DetailLabels, GameDetailLevelNames, OptionsClass::MAX_DETAIL_SETTING);
	Fill_Labels(DifficultyLabels, GameDifficultyNames, OptionsClass::MAX_DIFFICULTY_SETTING);
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIGameControlsPresenterClass::Service(void)
{
	if (!GameActive) {
		Title_Screen_Restore();
	}
}


bool UIGameControlsPresenterClass::Commits(void) const
{
	return(Choice == CHOICE_ACCEPT || Choice == CHOICE_SOUND || Choice == CHOICE_KEYBOARD);
}


void UIGameControlsPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_GAMECTRL_SPEED) {
		SpeedStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_SCROLL) {
		ScrollStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_DETAIL) {
		DetailStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_DIFFICULTY) {
		DifficultyStep = intent.Value;
		return;
	}
	if (intent.Action == UI_GAMECTRL_CAMEO_TEXT) {
		CameoText = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_ACTION_LINES) {
		ActionLines = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_TOOLTIPS) {
		ShowToolTips = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_COASTING) {
		Coasting = (intent.Value != 0);
		return;
	}
	if (intent.Action == UI_GAMECTRL_EDGE_SCROLL) {
		EdgeScroll = (intent.Value != 0);
		return;
	}

	UIResult result;

	if (intent.Action == UI_GAMECTRL_ACCEPT) {
		Choice = CHOICE_ACCEPT;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_GAMECTRL_CANCEL) {
		Choice = CHOICE_CANCEL;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	} else if (intent.Action == UI_GAMECTRL_SOUND) {
		if (!Has_Sub_Screens()) {
			return;
		}
		SpecialDialog = SDLG_SOUND;
		Choice = CHOICE_SOUND;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == UI_GAMECTRL_KEYBOARD) {
		if (!Has_Sub_Screens()) {
			return;
		}
		SpecialDialog = SDLG_KEYBOARD;
		Choice = CHOICE_KEYBOARD;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else {
		return;
	}

	Result = result;
}


void UIGameControlsPresenterClass::Apply(void)
{
	if (Has_Speed()) {
		int const gamespeed = (OptionsClass::MAX_SPEED_SETTING - 1) - SpeedStep;
		if (Options.GameSpeed != gamespeed) {
			if (GameActive && Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH) {
				OutList.push_back(EventClass(PlayerPtr->HeapID, EventClass::GAMESPEED, gamespeed));
			} else {
				Options.GameSpeed = gamespeed;
			}
		}
	}

	Options.ScrollRate = (OptionsClass::MAX_SCROLL_SETTING - 1) - ScrollStep;

	if (Options.DetailLevel != DetailStep) {
		Options.DetailLevel = DetailStep;
		Map.Reinit_Cell_Drawers();
	}

	if (Options.SidebarCameoText != CameoText) {
		Options.SidebarCameoText = CameoText;
		Map.Toggle_Cameo_Text(CameoText);
	}

	Options.ActionLines = ActionLines;
	TechnoClass::Set_Action_Lines(Options.ActionLines);

	Options.ToolTips = ShowToolTips;
	if (ToolTips != NULL && GameActive) {
		ToolTips->Activate(Options.ToolTips);
	}

	Options.ScrollMethod = Coasting ? 0 : 1;
	Options.AutoScroll = EdgeScroll;

	if (Has_Difficulty()) {
		Options.Difficulty = DifficultyStep;
	}
}


//---------------------------------------------------------------------------------------
// The RmlUi view. One document per dialog template, because the three templates differ by
// which controls exist rather than by how one is arranged.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the game controls screen.
/// </summary>
class GameControlsViewClass : public UIRmlViewClass
{
	public:
		GameControlsViewClass(UIGameControlsPresenterClass & presenter, char const * document) :
			UIRmlViewClass(presenter, document),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

		// A slider takes its position from the model as the document loads, and that raises
		// a change event of its own. Nothing is queued until this is set.
		void Settle(void) { Settled = true; }

	private:
		void Move(char const * which, int step);
		void Update_Labels(void);

		UIGameControlsPresenterClass & Screen;
		bool Settled = false;

		Rml::String SpeedText;
		Rml::String ScrollText;
		Rml::String DetailText;
		Rml::String DifficultyText;
};


static Rml::String Label_At(std::vector<std::string> const & labels, int step)
{
	if (step < 0 || step >= (int)labels.size()) {
		return(Rml::String());
	}
	return(Rml::String(labels[step]));
}


void GameControlsViewClass::Update_Labels(void)
{
	SpeedText = Label_At(Screen.SpeedLabels, Screen.SpeedStep);
	ScrollText = Label_At(Screen.ScrollLabels, Screen.ScrollStep);
	DetailText = Label_At(Screen.DetailLabels, Screen.DetailStep);
	DifficultyText = Label_At(Screen.DifficultyLabels, Screen.DifficultyStep);
}


void GameControlsViewClass::Move(char const * which, int step)
{
	if (!Settled) return;

	if (which == UI_GAMECTRL_SPEED && step == Screen.SpeedStep) return;
	if (which == UI_GAMECTRL_SCROLL && step == Screen.ScrollStep) return;
	if (which == UI_GAMECTRL_DETAIL && step == Screen.DetailStep) return;
	if (which == UI_GAMECTRL_DIFFICULTY && step == Screen.DifficultyStep) return;

	Screen.Queue(UIIntent{which, "", step});
}


void GameControlsViewClass::Bind(Rml::DataModelConstructor & model)
{
	Update_Labels();

	model.Bind("speed", &Screen.SpeedStep);
	model.Bind("scroll", &Screen.ScrollStep);
	model.Bind("detail", &Screen.DetailStep);
	model.Bind("difficulty", &Screen.DifficultyStep);
	model.Bind("speedtext", &SpeedText);
	model.Bind("scrolltext", &ScrollText);
	model.Bind("detailtext", &DetailText);
	model.Bind("difficultytext", &DifficultyText);

	model.Bind("cameotext", &Screen.CameoText);
	model.Bind("actionlines", &Screen.ActionLines);
	model.Bind("tooltips", &Screen.ShowToolTips);
	model.Bind("coasting", &Screen.Coasting);
	model.Bind("edgescroll", &Screen.EdgeScroll);
	model.Bind("soundavailable", &Screen.SoundAvailable);

	model.BindEventCallback("move",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const which = arguments[0].Get<Rml::String>();
			int const step = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);

			if (which == UI_GAMECTRL_SPEED) Move(UI_GAMECTRL_SPEED, step);
			else if (which == UI_GAMECTRL_SCROLL) Move(UI_GAMECTRL_SCROLL, step);
			else if (which == UI_GAMECTRL_DETAIL) Move(UI_GAMECTRL_DETAIL, step);
			else if (which == UI_GAMECTRL_DIFFICULTY) Move(UI_GAMECTRL_DIFFICULTY, step);
		});

	// A check box is a class plus a click that queues a toggle, not a two-way bound control.
	model.BindEventCallback("toggle",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const which = arguments[0].Get<Rml::String>();
			if (which == UI_GAMECTRL_CAMEO_TEXT) Screen.Queue(UIIntent{UI_GAMECTRL_CAMEO_TEXT, "", Screen.CameoText ? 0 : 1});
			else if (which == UI_GAMECTRL_ACTION_LINES) Screen.Queue(UIIntent{UI_GAMECTRL_ACTION_LINES, "", Screen.ActionLines ? 0 : 1});
			else if (which == UI_GAMECTRL_TOOLTIPS) Screen.Queue(UIIntent{UI_GAMECTRL_TOOLTIPS, "", Screen.ShowToolTips ? 0 : 1});
			else if (which == UI_GAMECTRL_COASTING) Screen.Queue(UIIntent{UI_GAMECTRL_COASTING, "", Screen.Coasting ? 0 : 1});
			else if (which == UI_GAMECTRL_EDGE_SCROLL) Screen.Queue(UIIntent{UI_GAMECTRL_EDGE_SCROLL, "", Screen.EdgeScroll ? 0 : 1});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// Escape cancels, which is the IDCANCEL the dialog's own cancel arm took. Enter accepts,
	// because the template names no default push button and Windows then sent IDOK.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Screen.Queue(UIIntent{UI_GAMECTRL_CANCEL, "", 0});
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_GAMECTRL_ACCEPT, "", 0});
			}
		});
}


void GameControlsViewClass::Sync(void)
{
	if (!Model) return;

	Update_Labels();

	// The slider positions are not dirtied, because a slider already carries the position
	// its own change event reported.
	Model.DirtyVariable("speedtext");
	Model.DirtyVariable("scrolltext");
	Model.DirtyVariable("detailtext");
	Model.DirtyVariable("difficultytext");
	Model.DirtyVariable("cameotext");
	Model.DirtyVariable("actionlines");
	Model.DirtyVariable("tooltips");
	Model.DirtyVariable("coasting");
	Model.DirtyVariable("edgescroll");
}


/// <summary>
/// Shows the game controls and waits for the player to leave them.
/// </summary>
UIResult UI_Game_Controls_Screen(UIGameControlsPresenterClass & presenter)
{
	char const * document = "gamecontrols.rml";
	if (presenter.Variant == UIGameControlsPresenterClass::VARIANT_SESSION) {
		document = "gamecontrolsmp.rml";
	} else if (presenter.Variant == UIGameControlsPresenterClass::VARIANT_INTERNET) {
		document = "gamecontrolswol.rml";
	}

	GameControlsViewClass view(presenter, document);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	view.Settle();

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
