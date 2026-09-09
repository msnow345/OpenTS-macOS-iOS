/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The skirmish setup screen. Behavior traced out of Skirmish_On_WM_INITDIALOG,
// Skirmish_On_WM_COMMAND and Skirmish_Mode_Dialog in skirmish.cpp.
//
// What the extraction fixes in place: a side row carries the country it stands for rather
// than its position, because the list holds only the countries that may be played; the map
// the player asked for is checked for enough start positions when the accept button is
// pressed, and a map with too few leaves the screen standing; Short Game turns Bases on and
// turning Bases off turns Short Game off; and backing out still records the name, side and
// color, which is what the cancel arm read before it answered.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uiskirmish.h"

#include "uiscenariopick.h"

#include "uimappreview.h"
#include "uirmlview.h"

#include "_rules.h"
#include "data.h"
#include "houstype.h"
#include "init.h"
#include "language/language.h"
#include "msgbox.h"
#include "netdlg2.h"
#include "goptions.h"
#include "mapgen.h"
#include "mplayer.h"
#include "netshare.h"
#include "preview.h"
#include "rules.h"
#include "session.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <cstdio>
#include <cstring>


// The least money a skirmish may be started with, which is where the credits track bar
// begins.
enum { MP_MIN_MONEY = 2500 };


UISkirmishPresenterClass::UISkirmishPresenterClass(void) :
	Preview(UI_SKIRMISH_PREVIEW_SURFACE)
{
}


void UISkirmishPresenterClass::Refresh(void)
{
	Handle = Session.Handle;

	Sides.clear();
	SelectedSide = 0;
	for (int index = 0; index < HouseTypes.Count(); index++) {
		HouseTypeClass const * const house = HouseTypes[index];
		if (!house->IsMultiplay) continue;

		if (index == Session.House) {
			SelectedSide = (int)Sides.size();
		}
		Sides.push_back(SideType{(char const *)house->GivenName, index});
	}

	Colors.clear();
	Colors.push_back(Fetch_String(TXT_GOLD));
	Colors.push_back(Fetch_String(TXT_RED));
	Colors.push_back(Fetch_String(TXT_BLUE));
	Colors.push_back(Fetch_String(TXT_GREEN));
	Colors.push_back(Fetch_String(TXT_ORANGE));
	Colors.push_back(Fetch_String(TXT_SKY_BLUE));
	Colors.push_back(Fetch_String(TXT_PURPLE));
	Colors.push_back(Fetch_String(TXT_PINK));
	SelectedColor = Session.PrefColor;

	UnitCount = SliderType{Session.Options.UnitCount, SessionClass::CountMin[1], SessionClass::CountMax[1], 1};
	Credits = SliderType{Session.Options.Credits, MP_MIN_MONEY, Rule->MPMaxMoney, 250};
	TechLevel = SliderType{BuildLevel, 1, MPLAYER_BUILD_LEVEL_MAX, 1};
	AILevel = SliderType{(int)Session.Options.AIDifficulty, 0, 2, 1};
	AIPlayers = SliderType{Session.Options.AIPlayers > 1 ? Session.Options.AIPlayers : 1, 1, 7, 1};

	// The track bar runs the other way round from the setting: its left end is the slowest
	// game, and the dialog turned one into the other at both ends.
	GameSpeed = SliderType{6 - Session.Options.GameSpeed, 0, 6, 1};

	Bases = Session.Options.Bases;
	Crates = Session.Options.Goodies;
	FogOfWar = Session.Options.FogOfWar;
	Bridges = Session.Options.BridgeDestruction;
	MCVRedeploy = Session.Options.MCVRedeploy;
	ShortGame = Session.Options.ShortGame;
	MultiEngineer = Session.Options.CrapEngineers;

	// The screen opens on the first scenario whatever the session was carrying.
	Set_Scenario_Info_From_Index(0);
	Session.Options.ScenarioIndex = 0;
	ScenarioName = Session.Options.ScenarioDescription;

	Clear_Vector(&Session.Players);
	Clear_Vector(&Session.Computers);

	Rebuild_Network_Map_Preview();
	PreviewGeneration++;

	CanAccept = true;
	ListChanged = true;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UISkirmishPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


/// <summary>
/// Records the name, side and color the player is showing.
/// Both leaving and backing out read these, because they are the player's own preferences
/// rather than the game's settings.
/// </summary>
void UISkirmishPresenterClass::Read_Identity(void)
{
	std::snprintf(Session.Handle, sizeof(Session.Handle), "%s", Handle.c_str());

	if (SelectedSide >= 0 && SelectedSide < (int)Sides.size()) {
		Session.House = (HousesType)Sides[SelectedSide].Country;
	} else {
		Session.House = (HousesType)HOUSE_FIRST;
	}

	Session.ColorIdx = SelectedColor;
	Session.PrefColor = SelectedColor;
}


/// <summary>
/// Writes the player's multiplayer preferences, and drops the preview.
/// </summary>
void UISkirmishPresenterClass::End(void)
{
	if (MultiplayerMapPreview != NULL) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = NULL;
	}

	Session.Write_MultiPlayer_Settings();
}


/// <summary>
/// Runs the map selection screen with this one already out of the way.
/// Backing out of it leaves the scenario this screen was showing, which is what putting the
/// old index back did.
/// </summary>
void UISkirmishPresenterClass::Run_Pending(void)
{
	if (Pending != SUB_PICK_MAP) {
		return;
	}

	Pending = SUB_NONE;

	int const previous = Session.Options.ScenarioIndex;

	bool const picked = Pick_Scenario_Screen();

	if (picked && Set_Scenario_Info_From_Index(Session.Options.ScenarioIndex) == true) {
		ScenarioName = Session.Options.ScenarioDescription;
	} else {
		Session.Options.ScenarioIndex = previous;
		Set_Scenario_Info_From_Index(previous);
	}

	// A generated map has a picture of its own beside it rather than one read out of the map.
	int const index = Session.Options.ScenarioIndex;
	if (index >= 0 && index < Session.Scenarios.Count()
		&& stricmp(Session.Scenarios[index]->Get_Filename(), RANDOM_MAP_FILE_NAME) == 0) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = new MapPreviewClass;
		MultiplayerMapPreview->Read_PCX_Preview("RandMap.img");
		if (MultiplayerMapPreview->Get_Preview_Surface() == NULL) {
			Rebuild_Network_Map_Preview();
		}
	} else {
		Rebuild_Network_Map_Preview();
	}

	PreviewGeneration++;
}


/// <summary>
/// Starts the game the player set up, if the map has room for it.
/// The start position count is checked here rather than when the slider moved, because the
/// map can change after it did.
/// </summary>
void UISkirmishPresenterClass::Accept(void)
{
	CanAccept = false;

	int const waypoints = RandomMapWaypointCount(Session.Options.ScenarioIndex);
	if (waypoints < AIPlayers.Value + 1) {
		char buffer[256];
		std::snprintf(buffer, sizeof(buffer), Fetch_String(TXT_SCENARIO_TOO_SMALL), waypoints);
		WWMessageBox().Process(buffer, TXT_OK);
		CanAccept = true;
		return;
	}

	Read_Identity();

	Session.Options.UnitCount = UnitCount.Value;
	BuildLevel = TechLevel.Value;
	Session.Options.Credits = Credits.Value;
	Session.Options.AIDifficulty = (DiffType)AILevel.Value;
	Session.Options.AIPlayers = AIPlayers.Value;
	Session.Options.GameSpeed = 6 - GameSpeed.Value;
	Options.GameSpeed = Session.Options.GameSpeed;

	NodeNameType * const who = new NodeNameType;
	if (who != NULL) {
		strcpy(who->Name, Session.Handle);
		who->Player.House = Session.House;
		who->Player.Color = Session.ColorIdx;
		who->Player.ProcessTime = -1;
		Session.Players.Add(who);
	}

	Session.Options.Bases = Bases;
	Session.Options.Goodies = Crates;
	Session.Options.FogOfWar = FogOfWar;
	Session.Options.BridgeDestruction = Bridges;
	Session.Options.MCVRedeploy = MCVRedeploy;
	Session.Options.ShortGame = ShortGame;
	Session.Options.HarvTruce = false;
	Session.Options.CrapEngineers = MultiEngineer;

	if (MultiplayerMapPreview != NULL) {
		delete MultiplayerMapPreview;
		MultiplayerMapPreview = NULL;
	}

	Outcome = true;

	UIResult result;
	result.Outcome = UIResult::OUTCOME_ACCEPTED;
	Result = result;
}


void UISkirmishPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_SKIRMISH_HANDLE) {
		Handle = intent.Identity;
		if (Handle.size() > HANDLE_LIMIT) {
			Handle.resize(HANDLE_LIMIT);
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_SIDE) {
		if (intent.Value >= 0 && intent.Value < (int)Sides.size()) {
			SelectedSide = intent.Value;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_COLOR) {
		if (intent.Value >= 0 && intent.Value < (int)Colors.size()) {
			SelectedColor = intent.Value;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_SLIDER) {
		SliderType * slider = NULL;
		if (intent.Identity == UI_SKIRMISH_UNITCOUNT) slider = &UnitCount;
		else if (intent.Identity == UI_SKIRMISH_CREDITS) slider = &Credits;
		else if (intent.Identity == UI_SKIRMISH_TECHLEVEL) slider = &TechLevel;
		else if (intent.Identity == UI_SKIRMISH_AILEVEL) slider = &AILevel;
		else if (intent.Identity == UI_SKIRMISH_AIPLAYERS) slider = &AIPlayers;
		else if (intent.Identity == UI_SKIRMISH_GAMESPEED) slider = &GameSpeed;

		if (slider != NULL) {
			int value = intent.Value;
			if (value < slider->Minimum) value = slider->Minimum;
			if (value > slider->Maximum) value = slider->Maximum;
			slider->Value = value;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_TOGGLE) {
		if (intent.Identity == UI_SKIRMISH_BASES) {
			Bases = !Bases;
			// A short game is decided by what a player still holds, so it needs bases.
			if (!Bases) ShortGame = false;
		} else if (intent.Identity == UI_SKIRMISH_SHORTGAME) {
			ShortGame = !ShortGame;
			if (ShortGame) Bases = true;
		} else if (intent.Identity == UI_SKIRMISH_CRATES) {
			Crates = !Crates;
		} else if (intent.Identity == UI_SKIRMISH_FOG) {
			FogOfWar = !FogOfWar;
		} else if (intent.Identity == UI_SKIRMISH_BRIDGES) {
			Bridges = !Bridges;
		} else if (intent.Identity == UI_SKIRMISH_MCV) {
			MCVRedeploy = !MCVRedeploy;
		} else if (intent.Identity == UI_SKIRMISH_ENGINEER) {
			MultiEngineer = !MultiEngineer;
		}
		return;
	}

	if (intent.Action == UI_SKIRMISH_PICK_MAP) {
		Pending = SUB_PICK_MAP;
		return;
	}

	if (intent.Action == UI_SKIRMISH_ACCEPT) {
		Accept();
		return;
	}

	if (intent.Action == UI_SKIRMISH_CANCEL) {
		Read_Identity();
		Outcome = false;

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
// 215 x 106 dialog units, which is 322.5 by 172.25 at the family's 1.5 across and 1.625
// down, and the picture sits inside its one pixel border.
enum { PREVIEW_WIDTH = 320, PREVIEW_HEIGHT = 170 };


/// <summary>
/// The RmlUi half of the skirmish setup screen.
/// </summary>
class SkirmishViewClass : public UIRmlViewClass
{
	public:
		// A color the player may take, with the swatch the owner-draw combo drew its row in.
		struct ColorRowType
		{
			std::string Name;
			std::string Hex;
		};

		SkirmishViewClass(UISkirmishPresenterClass & presenter) :
			UIRmlViewClass(presenter, "skirmish.rml"),
			Screen(presenter)
		{
			UI_Register_Surface(Screen.Preview.c_str(), &Picture);
		}

		virtual ~SkirmishViewClass(void) override
		{
			UI_Unregister_Surface(Screen.Preview.c_str());
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

		// Puts the track bar ranges the rules give this screen on the controls, and lets
		// the change handlers start reporting. A range is set before the data binding fills
		// a value in, so a value outside a track bar's default range is not clamped away.
		void Settle(void);

	private:
		void Move(char const * which, int value);
		void Press(char const * action);
		void Set_Range(char const * id, UISkirmishPresenterClass::SliderType const & slider);

		std::string Field_Text(void) const;
		Rml::ElementFormControlInput * Field(void) const;

		UISkirmishPresenterClass & Screen;

		// The view owns the pixels; the presenter carries only the name they answer to.
		MapPreviewSurfaceClass Picture{PREVIEW_WIDTH, PREVIEW_HEIGHT};

		std::vector<ColorRowType> ColorRows;

		unsigned int Drawn = 0;
		bool Settled = false;
};


Rml::ElementFormControlInput * SkirmishViewClass::Field(void) const
{
	if (Element == nullptr) {
		return(nullptr);
	}
	return(rmlui_dynamic_cast<Rml::ElementFormControlInput *>(Element->GetElementById("name")));
}


std::string SkirmishViewClass::Field_Text(void) const
{
	Rml::ElementFormControlInput * const field = Field();
	if (field == nullptr) {
		return(Screen.Handle);
	}
	return(field->GetValue());
}


void SkirmishViewClass::Set_Range(char const * id, UISkirmishPresenterClass::SliderType const & slider)
{
	if (Element == nullptr) {
		return;
	}

	Rml::Element * const control = Element->GetElementById(id);
	if (control == nullptr) {
		return;
	}

	// The range is set before the value, because a track bar clamps a value into the range
	// it is holding and the default range stops well short of what the rules allow.
	control->SetAttribute("min", slider.Minimum);
	control->SetAttribute("max", slider.Maximum);
	control->SetAttribute("step", slider.Step);
	control->SetAttribute("value", slider.Value);
}


void SkirmishViewClass::Settle(void)
{
	Set_Range("unitcount", Screen.UnitCount);
	Set_Range("credits", Screen.Credits);
	Set_Range("techlevel", Screen.TechLevel);
	Set_Range("ailevel", Screen.AILevel);
	Set_Range("aiplayers", Screen.AIPlayers);
	Set_Range("gamespeed", Screen.GameSpeed);

	Settled = true;
}


/// <summary>
/// Queues a track bar's new position, dropping one that matches what the model already
/// holds so that setting a control from the model is not read back as a move.
/// </summary>
void SkirmishViewClass::Move(char const * which, int value)
{
	if (!Settled) return;

	UISkirmishPresenterClass::SliderType const * held = NULL;
	if (which == UI_SKIRMISH_UNITCOUNT) held = &Screen.UnitCount;
	else if (which == UI_SKIRMISH_CREDITS) held = &Screen.Credits;
	else if (which == UI_SKIRMISH_TECHLEVEL) held = &Screen.TechLevel;
	else if (which == UI_SKIRMISH_AILEVEL) held = &Screen.AILevel;
	else if (which == UI_SKIRMISH_AIPLAYERS) held = &Screen.AIPlayers;
	else if (which == UI_SKIRMISH_GAMESPEED) held = &Screen.GameSpeed;

	if (held == NULL || held->Value == value) {
		return;
	}

	Screen.Queue(UIIntent{UI_SKIRMISH_SLIDER, which, value});
}


/// <summary>
/// Queues what a button or its key stands for.
/// The name is read out of the field here rather than tracked, because that is when the
/// dialog read its edit control, and the read is queued ahead of the action it is read for
/// so the two execute in that order. Backing out records the name too, which is what the
/// cancel arm did.
/// </summary>
void SkirmishViewClass::Press(char const * action)
{
	if (action == UI_SKIRMISH_ACCEPT || action == UI_SKIRMISH_CANCEL) {
		std::string const text = Field_Text();
		if (text != Screen.Handle) {
			Screen.Queue(UIIntent{UI_SKIRMISH_HANDLE, text, 0});
		}
	}

	Screen.Queue(UIIntent{action, "", 0});
}


void SkirmishViewClass::Bind(Rml::DataModelConstructor & model)
{
	ColorRows.clear();
	for (int index = 0; index < (int)Screen.Colors.size(); index++) {
		char hex[8];
		if (index < MAX_PLAYERS) {
			// A COLORREF holds its blue byte highest, which is the order RGB() packs.
			unsigned long const color = (unsigned long)PlayerColorTable[index];
			std::snprintf(hex, sizeof(hex), "#%02x%02x%02x",
				(unsigned)(color & 0xFF), (unsigned)((color >> 8) & 0xFF), (unsigned)((color >> 16) & 0xFF));
		} else {
			std::snprintf(hex, sizeof(hex), "#b9bcae");
		}
		ColorRows.push_back(ColorRowType{Screen.Colors[index], hex});
	}

	if (auto side = model.RegisterStruct<UISkirmishPresenterClass::SideType>()) {
		side.RegisterMember("name", &UISkirmishPresenterClass::SideType::Name);
	}
	model.RegisterArray<std::vector<UISkirmishPresenterClass::SideType>>();

	if (auto swatch = model.RegisterStruct<ColorRowType>()) {
		swatch.RegisterMember("name", &ColorRowType::Name);
		swatch.RegisterMember("hex", &ColorRowType::Hex);
	}
	model.RegisterArray<std::vector<ColorRowType>>();

	if (auto slider = model.RegisterStruct<UISkirmishPresenterClass::SliderType>()) {
		slider.RegisterMember("value", &UISkirmishPresenterClass::SliderType::Value);
		slider.RegisterMember("min", &UISkirmishPresenterClass::SliderType::Minimum);
		slider.RegisterMember("max", &UISkirmishPresenterClass::SliderType::Maximum);
		slider.RegisterMember("step", &UISkirmishPresenterClass::SliderType::Step);
	}

	model.Bind("handle", &Screen.Handle);
	model.Bind("sides", &Screen.Sides);
	model.Bind("selectedside", &Screen.SelectedSide);
	model.Bind("colors", &ColorRows);
	model.Bind("selectedcolor", &Screen.SelectedColor);

	model.Bind("unitcount", &Screen.UnitCount);
	model.Bind("credits", &Screen.Credits);
	model.Bind("techlevel", &Screen.TechLevel);
	model.Bind("ailevel", &Screen.AILevel);
	model.Bind("aiplayers", &Screen.AIPlayers);
	model.Bind("gamespeed", &Screen.GameSpeed);

	model.Bind("bases", &Screen.Bases);
	model.Bind("crates", &Screen.Crates);
	model.Bind("fog", &Screen.FogOfWar);
	model.Bind("bridges", &Screen.Bridges);
	model.Bind("mcv", &Screen.MCVRedeploy);
	model.Bind("shortgame", &Screen.ShortGame);
	model.Bind("engineer", &Screen.MultiEngineer);

	model.Bind("scenarioname", &Screen.ScenarioName);
	model.Bind("preview", &Screen.Preview);
	model.Bind("canaccept", &Screen.CanAccept);

	// The field is bound one way, so a value the model already holds is never queued back
	// as a change the player did not type.
	model.BindEventCallback("rename",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			Rml::String const value = event.GetParameter<Rml::String>("value", Rml::String());
			if (value == Screen.Handle) return;
			Screen.Queue(UIIntent{UI_SKIRMISH_HANDLE, value, 0});
		});

	model.BindEventCallback("chooseside",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			if (!Settled) return;
			int const row = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);
			if (row == Screen.SelectedSide) return;
			Screen.Queue(UIIntent{UI_SKIRMISH_SIDE, "", row});
		});

	model.BindEventCallback("choosecolor",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			if (!Settled) return;
			int const row = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);
			if (row == Screen.SelectedColor) return;
			Screen.Queue(UIIntent{UI_SKIRMISH_COLOR, "", row});
		});

	model.BindEventCallback("move",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const which = arguments[0].Get<Rml::String>();
			int const value = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);

			if (which == UI_SKIRMISH_UNITCOUNT) Move(UI_SKIRMISH_UNITCOUNT, value);
			else if (which == UI_SKIRMISH_CREDITS) Move(UI_SKIRMISH_CREDITS, value);
			else if (which == UI_SKIRMISH_TECHLEVEL) Move(UI_SKIRMISH_TECHLEVEL, value);
			else if (which == UI_SKIRMISH_AILEVEL) Move(UI_SKIRMISH_AILEVEL, value);
			else if (which == UI_SKIRMISH_AIPLAYERS) Move(UI_SKIRMISH_AIPLAYERS, value);
			else if (which == UI_SKIRMISH_GAMESPEED) Move(UI_SKIRMISH_GAMESPEED, value);
		});

	// A check box is a class plus a click that queues a toggle, not a two-way bound control.
	model.BindEventCallback("toggle",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_SKIRMISH_TOGGLE, arguments[0].Get<Rml::String>(), 0});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const action = arguments[0].Get<Rml::String>();
			if (action == UI_SKIRMISH_ACCEPT) Press(UI_SKIRMISH_ACCEPT);
			else if (action == UI_SKIRMISH_CANCEL) Press(UI_SKIRMISH_CANCEL);
			else if (action == UI_SKIRMISH_PICK_MAP) Press(UI_SKIRMISH_PICK_MAP);
		});

	// Escape backs out and Enter starts the game, which is what IsDialogMessage delivered to
	// a template that names IDCANCEL and no default push button.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE) {
				Press(UI_SKIRMISH_CANCEL);
			} else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Press(UI_SKIRMISH_ACCEPT);
			}
		});
}


void SkirmishViewClass::Sync(void)
{
	if (!Model) return;

	// The track bar, combo box and field values are not dirtied, because each already
	// carries what its own change event reported.
	Model.DirtyVariable("sides");
	Model.DirtyVariable("colors");
	Model.DirtyVariable("bases");
	Model.DirtyVariable("crates");
	Model.DirtyVariable("fog");
	Model.DirtyVariable("bridges");
	Model.DirtyVariable("mcv");
	Model.DirtyVariable("shortgame");
	Model.DirtyVariable("engineer");
	Model.DirtyVariable("scenarioname");
	Model.DirtyVariable("canaccept");

	// The picture is redrawn where it changed, not every pass, so the element uploads once
	// per map rather than once per present.
	if (Screen.PreviewGeneration != Drawn) {
		Drawn = Screen.PreviewGeneration;
		Picture.Redraw();
	}

	Screen.ListChanged = false;
}


/// <summary>
/// Shows the skirmish screen and waits for the player to leave it.
/// </summary>
UIResult UI_Skirmish_Screen(UISkirmishPresenterClass & presenter)
{
	SkirmishViewClass view(presenter);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	view.Settle();

	// The map selection screen draws where this one is, so the document steps aside for it,
	// which is what the dialog's own ShowWindow did.
	while (!presenter.Result.has_value()) {
		UI_Run_Modal(presenter, view);

		if (presenter.Pending == UISkirmishPresenterClass::SUB_NONE) {
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
