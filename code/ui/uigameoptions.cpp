/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The in-game options screen. Behavior traced out of goptions.cpp and kept where it was,
// with the window handling left behind in the view.
//
// What the extraction fixes in place, each of which the dialog decided rather than the
// template: save and load mean different things in a solo game and in a session, and only
// the solo ones open a browser at all; delete never ends the screen, it just changes what
// is on disk and the screen is refreshed; a skirmish has no briefing to restate; resume is
// where the two sliders are applied, because dragging one only moved its label; abort
// asks for the surrender box rather than the abort box in a tournament session; and the
// screen answers with a choice rather than with a control identifier.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uigameoptions.h"

#include "uirmlview.h"

#include "data.h"
#include "dbgprint.h"
#include "event.h"
#include "gamedlg.h"
#include "globals.h"
#include "house.h"
#include "language/language.h"
#include "goptions.h"
#include "loaddlg.h"
#include "nettiming.h"
#include "options.h"
#include "savemgr.h"
#include "scenario.h"
#include "session.h"
#include "stats.h"

#include "special.hh"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <cstring>




static bool Is_Solo_Session(void)
{
	return(Session.Type == GAME_NORMAL || Session.Type == GAME_SKIRMISH);
}


/// <summary>
/// Copies the state the screen shows out of the game.
/// Called at open and again whenever a sub-screen has changed what is on disk, which is what
/// the dialog's second call to its own WM_INITDIALOG handler did.
/// </summary>
void UIGameOptionsPresenterClass::Refresh(void)
{
	IsMultiplayer = !Is_Solo_Session();
	HasSliders = (Session.Type == GAME_INTERNET);

	if (Is_Solo_Session()) {
		bool const present = LoadOptionsClass().Files_Present();
		CanSave = true;
		CanLoad = present;
		CanDelete = present;
	} else {
		CanSave = SaveManager.Is_Multiplayer_Saving_Allowed();
		CanLoad = SaveManager.Multiplayer_Load_Is_Allowed() && MultiplayerLoadOptionsClass().Files_Present();
		CanDelete = true;
	}

	CanBrief = (Session.Type != GAME_SKIRMISH);

	SpeedStep = (OptionsClass::MAX_SPEED_SETTING - 1) - Options.GameSpeed;

	NetTiming::TimingSettings const timing{Session.FrameSendRate, Session.MaxAhead};
	unsigned int const rung = (timing.FrameSendRate >= NetTiming::MINIMUM_TIMING_RUNG
		&& timing.FrameSendRate <= NetTiming::MAXIMUM_TIMING_RUNG)
			? timing.FrameSendRate : NetTiming::MAXIMUM_TIMING_RUNG;

	ConnectionRung = (int)rung;
	ConnectionQualityTextID = Network_Quality_Text_ID(NetTiming::Connection_Quality_For_Settings(timing));
	// The template's slider runs worst to best from left to right, so rung 1 sits at its right end.
	ConnectionStep = (int)(NetTiming::MINIMUM_TIMING_RUNG + NetTiming::MAXIMUM_TIMING_RUNG - rung);

	SpeedLabels.clear();
	for (int step = 0; step < OptionsClass::MAX_SPEED_SETTING; step++) {
		SpeedLabels.push_back(Fetch_String(GameSpeedNames[step]));
	}
}


void UIGameOptionsPresenterClass::Finish(ChoiceType choice, UIResult::OutcomeType outcome)
{
	Choice = choice;

	UIResult result;
	result.Outcome = outcome;
	Result = result;
}


void UIGameOptionsPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_GAMEOPT_SPEED) {
		// Dragging only moves the label. The setting is applied when the player resumes,
		// which is where the dialog read the slider back.
		SpeedStep = intent.Value;
		return;
	}

	// The two checks below are the dialog's own, made when the button was pressed rather
	// than when it was enabled, because a session can withdraw permission while the screen
	// is up. The view-model's CanSave and CanLoad say what to show, not what to allow.
	if (intent.Action == UI_GAMEOPT_SAVE) {
		if (Is_Solo_Session()) {
			Pending = SUB_SAVE;
		} else if (SaveManager.Is_Multiplayer_Saving_Allowed()) {
			OutList.push_back(EventClass(PlayerPtr->HeapID, EventClass::SAVEGAME));
			Finish(CHOICE_SAVE_REQUESTED, UIResult::OUTCOME_ACCEPTED);
		}
		return;
	}

	if (intent.Action == UI_GAMEOPT_LOAD) {
		if (Is_Solo_Session()) {
			Pending = SUB_LOAD;
		} else if (SaveManager.Multiplayer_Load_Is_Allowed()) {
			// A list opened from in here would sit inside the main loop and stall the match;
			// the menu loop opens it between frames instead.
			SpecialDialog = SDLG_LOAD;
			Finish(CHOICE_LOADED, UIResult::OUTCOME_ACCEPTED);
		}
		return;
	}

	if (intent.Action == UI_GAMEOPT_DELETE) {
		Pending = SUB_DELETE;
		return;
	}

	if (intent.Action == UI_GAMEOPT_BRIEFING) {
		Finish(CHOICE_BRIEFING, UIResult::OUTCOME_ACCEPTED);
		return;
	}

	if (intent.Action == UI_GAMEOPT_RESUME) {
		if (Session.Type == GAME_INTERNET) {
			int const speed = (OptionsClass::MAX_SPEED_SETTING - 1) - SpeedStep;
			if (Options.GameSpeed != speed) {
				OutList.push_back(EventClass(PlayerPtr->HeapID, EventClass::GAMESPEED, speed));
			}
		}
		Finish(CHOICE_RESUME, UIResult::OUTCOME_ACCEPTED);
		return;
	}

	if (intent.Action == UI_GAMEOPT_ABORT) {
		if (Session.Type == GAME_INTERNET) {
			SpecialDialog = WestwoodOnline_Tournament ? SDLG_SURRENDER : SDLG_ABORT;
		} else {
			SpecialDialog = SDLG_ABORT;
		}
		Finish(CHOICE_ABORT, UIResult::OUTCOME_CANCELLED);
		return;
	}

	if (intent.Action == UI_GAMEOPT_SETTINGS) {
		SpecialDialog = SDLG_SETTINGS;
		Finish(CHOICE_SETTINGS, UIResult::OUTCOME_ACCEPTED);
		return;
	}
}


/// <summary>
/// Runs the browser an executed intent asked for.
/// The caller has already got its own presentation out of the way, which is all a view has
/// to do about a screen opening on top of this one.
/// </summary>
void UIGameOptionsPresenterClass::Run_Pending(void)
{
	SubScreenType const pending = Pending;
	Pending = SUB_NONE;

	switch (pending) {
		case SUB_SAVE: {
				char description[512];
				std::strcpy(description, Scen->Description);
				LoadOptionsClass().Save(description);
				Refresh();
			}
			break;

		case SUB_LOAD:
			if (LoadOptionsClass().Load()) {
				Finish(CHOICE_LOADED, UIResult::OUTCOME_ACCEPTED);
			}
			break;

		case SUB_DELETE:
			LoadOptionsClass().Delete();
			Refresh();
			break;

		default:
			break;
	}
}


//---------------------------------------------------------------------------------------
// The RmlUi view. One document per dialog template, because the three templates differ by
// which controls exist rather than by how one is arranged.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the in-game options screen.
/// </summary>
class GameOptionsViewClass : public UIRmlViewClass
{
	public:
		GameOptionsViewClass(UIGameOptionsPresenterClass & presenter, char const * document) :
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

		UIGameOptionsPresenterClass & Screen;
		bool Settled = false;

		// The caption beside each slider. Held here because the labels the presenter
		// carries are indexed by step and a document binds a value, not a lookup.
		Rml::String SpeedLabel;
		Rml::String ConnectionLabel;
};


void GameOptionsViewClass::Update_Labels(void)
{
	SpeedLabel.clear();
	if (Screen.SpeedStep >= 0 && Screen.SpeedStep < (int)Screen.SpeedLabels.size()) {
		SpeedLabel = Screen.SpeedLabels[Screen.SpeedStep];
	}

	char connection[64];
	snprintf(connection, sizeof(connection), Fetch_String(TXT_CONNECTION_QUALITY_RUNG),
		Fetch_String(Screen.ConnectionQualityTextID), (unsigned int)Screen.ConnectionRung);
	ConnectionLabel = connection;
}


void GameOptionsViewClass::Move(char const * which, int step)
{
	if (!Settled) return;

	// A position the screen already holds raises no intent, so setting a slider from the
	// model cannot look like a move the player did not make.
	if (which == UI_GAMEOPT_SPEED && step == Screen.SpeedStep) return;

	Screen.Queue(UIIntent{which, "", step});
}


void GameOptionsViewClass::Bind(Rml::DataModelConstructor & model)
{
	Update_Labels();

	model.Bind("cansave", &Screen.CanSave);
	model.Bind("canload", &Screen.CanLoad);
	model.Bind("candelete", &Screen.CanDelete);
	model.Bind("canbrief", &Screen.CanBrief);
	model.Bind("speed", &Screen.SpeedStep);
	model.Bind("connection", &Screen.ConnectionStep);
	model.Bind("speedlabel", &SpeedLabel);
	model.Bind("connectionlabel", &ConnectionLabel);

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	model.BindEventCallback("move",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const which = arguments[0].Get<Rml::String>();
			int const step = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);

			if (which == UI_GAMEOPT_SPEED) Move(UI_GAMEOPT_SPEED, step);
		});

	// Escape resumes, which is the IDCANCEL the dialog answered with its resume arm. Enter
	// resumes too: the template names no default push button, so Windows sent IDOK, and the
	// dialog treated that as the resume button.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE || key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_GAMEOPT_RESUME, "", 0});
			}
		});
}


void GameOptionsViewClass::Sync(void)
{
	if (!Model) return;

	Update_Labels();

	// The slider positions are not dirtied, because a slider already carries the position
	// its own change event reported.
	Model.DirtyVariable("cansave");
	Model.DirtyVariable("canload");
	Model.DirtyVariable("candelete");
	Model.DirtyVariable("canbrief");
	Model.DirtyVariable("speedlabel");
	Model.DirtyVariable("connectionlabel");
}


/// <summary>
/// Shows the in-game options and waits for the player to choose.
/// </summary>
UIResult UI_Game_Options_Screen(UIGameOptionsPresenterClass & presenter)
{
	char const * document = "gameoptionsmp.rml";
	if (!presenter.IsMultiplayer) {
		document = "gameoptions.rml";
	} else if (presenter.HasSliders) {
		document = "gameoptionswol.rml";
	}

	GameOptionsViewClass view(presenter, document);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	view.Settle();

	// A browser this screen opens is a screen of a different kind, so it nests; getting out
	// of the way of it is hiding this document, which is what the dialog's ShowWindow did.
	while (!presenter.Result.has_value()) {
		UI_Run_Modal(presenter, view);

		if (presenter.Pending == UIGameOptionsPresenterClass::SUB_NONE) {
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
