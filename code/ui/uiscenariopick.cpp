/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The multiplayer map selection screen. Behavior traced out of Scenario_DlgProc and
// Scenario_Select_Callback in netshare.cpp.
//
// What the extraction fixes in place: the preview follows the highlighted row rather than
// the session's own choice, and the session's choice is put back after every look, so
// browsing the list changes nothing until the player accepts; the random map generator draws
// where this screen is, so the screen steps aside for it; and backing out leaves the
// scenario the screen opened on.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uiscenariopick.h"

#include "uirmlview.h"
#include "uisurface.h"

#include "data.h"
#include "mapgen.h"
#include "netdlg2.h"
#include "netshare.h"
#include "dsurface.h"
#include "preview.h"
#include "session.h"
#include "xsurface.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>


UIScenarioPickPresenterClass::UIScenarioPickPresenterClass(void)
{
}


void UIScenarioPickPresenterClass::Build_List(void)
{
	Scenarios.clear();
	for (int index = 0; index < Session.Scenarios.Count(); index++) {
		Scenarios.push_back(Session.Scenarios[index]->Description());
	}
	ListChanged = true;
}


void UIScenarioPickPresenterClass::Refresh(void)
{
	Build_List();

	Selected = Session.Options.ScenarioIndex;
	Original = Selected;
	LastPreviewed = -1;
}


/// <summary>
/// Builds the preview for the highlighted row and puts the session's own choice back.
/// The list can be walked without committing to anything, so the scenario information the
/// preview needs is loaded, used, and then replaced by the one the screen opened on.
/// </summary>
void UIScenarioPickPresenterClass::Preview_Selection(void)
{
	if (Selected == LastPreviewed || Selected < 0 || Selected >= Session.Scenarios.Count()) {
		return;
	}

	Set_Scenario_Info_From_Index(Selected);

	// A generated map has a picture of its own beside it rather than one read out of the map.
	if (stricmp(Session.Scenarios[Selected]->Get_Filename(), RANDOM_MAP_FILE_NAME) == 0) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = new MapPreviewClass;
		MultiplayerMapPreview->Read_PCX_Preview("RandMap.img");
		if (MultiplayerMapPreview->Get_Preview_Surface() == NULL) {
			Rebuild_Network_Map_Preview();
		}
	} else {
		Rebuild_Network_Map_Preview();
	}

	LastPreviewed = Selected;
	PreviewGeneration++;

	Session.Options.ScenarioIndex = Original;
	Set_Scenario_Info_From_Index(Original);
}


/// <summary>
/// The maintenance the dialog's wait callback ran on every pass of its own loop.
/// </summary>
void UIScenarioPickPresenterClass::Service(void)
{
	Preview_Selection();

	// A game already in the lobby keeps talking while the host browses. The runner services
	// a session of its own, so only the network pump the callback added belongs here.
	if (Session.Type == GAME_IPX || Session.Type == GAME_INTERNET) {
		Net2Callback();
	}
}


/// <summary>
/// Runs the map generator with the screen already out of the way.
/// </summary>
void UIScenarioPickPresenterClass::Run_Pending(void)
{
	if (Pending != SUB_RANDOM_MAP) {
		return;
	}

	Pending = SUB_NONE;

	int const scenario = CreateRandomMap();
	if (scenario == -1) {
		return;
	}

	// A generated map joins the list and is highlighted, but the session keeps the scenario
	// the screen opened on until the player accepts.
	Build_List();
	Selected = scenario;
	LastPreviewed = -1;

	Set_Scenario_Info_From_Index(scenario);
	if (MultiplayerMapPreview == NULL || MultiplayerMapPreview->Get_Preview_Surface() == NULL) {
		Rebuild_Network_Map_Preview();
	}
	PreviewGeneration++;

	Session.Options.ScenarioIndex = Original;
	Set_Scenario_Info_From_Index(Original);
}


void UIScenarioPickPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_SCENARIOPICK_SELECT) {
		if (intent.Value < 0 || intent.Value >= (int)Scenarios.size()) {
			return;
		}
		Selected = intent.Value;
		return;
	}

	if (intent.Action == UI_SCENARIOPICK_RANDOM) {
		Pending = SUB_RANDOM_MAP;
		return;
	}

	if (intent.Action == UI_SCENARIOPICK_ACCEPT) {
		// The dialog clamped a list with nothing selected to the first entry.
		Selected = Selected > 0 ? Selected : 0;
		Session.Options.ScenarioIndex = Selected;

		UIResult result;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
		result.Value = Selected;
		Result = result;
		return;
	}

	if (intent.Action == UI_SCENARIOPICK_CANCEL) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
		Result = result;
		return;
	}
}


//---------------------------------------------------------------------------------------
// The RmlUi view.
//---------------------------------------------------------------------------------------

// The picture the preview frame holds, in game logical units. The frame is the template's
// 129 x 80 dialog units, which is 193.5 by 130 at the family's 1.5 across and 1.625 down,
// and the picture sits inside its one pixel border.
enum { PREVIEW_WIDTH = 191, PREVIEW_HEIGHT = 128 };


/// <summary>
/// The map preview as a surface a document can show.
/// The picture is scaled into the frame the way MapPreviewClass::Blit_Preview scales it into
/// the dialog's group box, so what the document shows is what the dialog showed. The frame is
/// filled with a color key first, so the letterbox around a picture of a different shape is
/// transparent rather than black.
/// </summary>
class MapPreviewSurfaceClass : public UISurfaceBufferClass
{
	public:
		MapPreviewSurfaceClass(void) :
			UISurfaceBufferClass(PREVIEW_WIDTH, PREVIEW_HEIGHT)
		{
			Set_Transparent_Color(DSurface::Build_Hicolor_Pixel(255, 0, 255));
			Clear();
		}

		void Redraw(void);
};


void MapPreviewSurfaceClass::Redraw(void)
{
	Clear();

	if (MultiplayerMapPreview == NULL) {
		return;
	}

	XSurface * const picture = MultiplayerMapPreview->Get_Preview_Surface();
	if (picture == NULL) {
		return;
	}

	Rect const source = picture->Get_Rect();
	if (source.Width <= 0 || source.Height <= 0) {
		return;
	}

	int const scale = std::min(1000 * PREVIEW_WIDTH / source.Width, 1000 * PREVIEW_HEIGHT / source.Height);

	Rect destination;
	destination.Width = (scale * source.Width) / 1000;
	destination.Height = (scale * source.Height) / 1000;
	destination.X = PREVIEW_WIDTH / 2 - destination.Width / 2;
	destination.Y = PREVIEW_HEIGHT / 2 - destination.Height / 2;

	Get_Surface().Blit_From(destination, *picture, source, false, false);
	Mark_Dirty();
}


/// <summary>
/// The RmlUi half of the map selection screen.
/// </summary>
class ScenarioPickViewClass : public UIRmlViewClass
{
	public:
		ScenarioPickViewClass(UIScenarioPickPresenterClass & presenter) :
			UIRmlViewClass(presenter, "selectmap.rml"),
			Screen(presenter)
		{
			UI_Register_Surface(Screen.Preview.c_str(), &Picture);
		}

		virtual ~ScenarioPickViewClass(void) override
		{
			UI_Unregister_Surface(Screen.Preview.c_str());
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		UIScenarioPickPresenterClass & Screen;

		// The view owns the pixels; the presenter carries only the name they answer to.
		MapPreviewSurfaceClass Picture;

		unsigned int Drawn = 0;
};


void ScenarioPickViewClass::Bind(Rml::DataModelConstructor & model)
{
	model.RegisterArray<std::vector<std::string>>();

	model.Bind("scenarios", &Screen.Scenarios);
	model.Bind("selected", &Screen.Selected);
	model.Bind("preview", &Screen.Preview);

	model.BindEventCallback("pick",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_SCENARIOPICK_SELECT, "", (int)arguments[0].Get<float>()});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// Escape backs out and Enter takes the highlighted map, which is what IsDialogMessage
	// delivered to a template that names IDCANCEL and no default push button.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Screen.Queue(UIIntent{UI_SCENARIOPICK_CANCEL, "", 0});
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_SCENARIOPICK_ACCEPT, "", 0});
			}
		});
}


void ScenarioPickViewClass::Sync(void)
{
	if (!Model) return;

	Model.DirtyVariable("scenarios");
	Model.DirtyVariable("selected");

	// The picture is redrawn where it changed, not every pass, so the element uploads once
	// per map rather than once per present.
	if (Screen.PreviewGeneration != Drawn) {
		Drawn = Screen.PreviewGeneration;
		Picture.Redraw();
	}

	Screen.ListChanged = false;
}


/// <summary>
/// Shows the map selection screen and waits for the player to leave it.
/// </summary>
UIResult UI_Scenario_Pick_Screen(UIScenarioPickPresenterClass & presenter)
{
	ScenarioPickViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	// The map generator draws where this screen is, so the document steps aside for it.
	while (!presenter.Result.has_value()) {
		UI_Run_Modal(presenter, view);

		if (presenter.Pending == UIScenarioPickPresenterClass::SUB_NONE) {
			break;
		}

		view.Hide();
		presenter.Run_Pending();
		view.Show();
		view.Sync();
	}

	UIResult const result = presenter.Result.value_or(UIResult{});
	view.Close();
	return(result);
}
