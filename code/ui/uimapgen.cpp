/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The random map generator screen. What is preserved from IDD_MAPGEN, IDD_MAPGEN_FS and
// IDD_MAPGEN_WDT, and where each came from: the variant is chosen by whether Firestorm is
// enabled and whether the session names a tournament territory, not by the caller; the
// environment and time of day lists are sorted by name and the two size lists are not,
// because only the first two templates carry CBS_SORT; every setting is read off the screen
// when a button is pressed rather than tracked, which is what Get_Settings did; a tournament
// territory narrows each track bar's span and may lock it, and a span with nothing left to
// choose between is shown disabled rather than hidden; the tournament variant fixes the
// teams at two on two and offers no player count; and the seed field, which every template
// declares hidden and disabled, becomes visible only where a territory says the player may
// change the seed.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uimapgen.h"

#include "uiinternal.h"
#include "uimappreview.h"
#include "uirmlview.h"
#include "uishell.h"
#include "uisurface.h"

#include "addon.h"
#include "ccrand.h"
#include "data.h"
#include "dbgprint.h"
#include "init.h"
#include "language/language.h"
#include "mapgen.h"
#include "preview.h"
#include "scenario.h"
#include "session.h"
#include "wdtnet.h"
#include "worlddom.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>


namespace {

	// The strings the three lists are built from, in the order the settings are numbered.
	int const _BiomeNames[BIOME_COUNT] = {
		TXT_BIOME_TUNDRA,
		TXT_BIOME_TAIGA,
		TXT_BIOME_TEMPERATE,
		TXT_BIOME_DESERT,
		TXT_BIOME_MUTATED,
	};

	int const _TimeNames[TIME_OF_DAY_COUNT] = {
		TXT_TIME_MORNING,
		TXT_TIME_AFTERNOON,
		TXT_TIME_DUSK,
		TXT_TIME_NIGHT,
	};

	int const _SizeNames[MAPSIZE_COUNT] = {
		TXT_MAPSIZE_SMALL,
		TXT_MAPSIZE_MEDIUM,
		TXT_MAPSIZE_LARGE,
		TXT_MAPSIZE_VERY_LARGE,
	};


	// The territory the session is being fought over, or NULL outside a tournament game.
	WDTTerritory * Tournament_Territory(void)
	{
		if (Session.Type != GAME_INTERNET || !Session.IsWDT) {
			return(NULL);
		}
		return(WDT_Get_Territory(Session.WDTTerritory));
	}


	// A span with nothing to choose between is shown disabled and left on the full scale,
	// which is what Set_Scroll_Bar did with a max no greater than its min.
	void Set_Range(UIMapGenPresenterClass::RangeType & range, int min, int max, int value, bool enable)
	{
		if (max <= min) {
			range.Min = 0;
			range.Max = 100;
			range.Enabled = false;
		} else {
			range.Min = min;
			range.Max = max;
			range.Enabled = enable;
		}
		range.Value = value;
	}

}	// namespace


static UIMapGenPresenterClass * _MapGenScreen = NULL;


UIMapGenPresenterClass * UI_MapGen_Screen(void)
{
	return(_MapGenScreen);
}


/// <summary>
/// Picks the variant and reads the generator's settings into the view-model.
/// </summary>
/// <param name="callback">The progress callback the driver ran on every pass of its loop.</param>
void UIMapGenPresenterClass::Open(bool (*callback)())
{
	Callback = callback;
	Pending = PENDING_NONE;
	Result.reset();
	IsClosing = false;

	WDTTerritory const * const wdt = Tournament_Territory();
	if (wdt != NULL) {
		Variant = VARIANT_WDT;
	} else if (Addon_Enabled(ADDON_FIRESTORM)) {
		Variant = VARIANT_FIRESTORM;
	} else {
		Variant = VARIANT_BASE;
	}

	// A screen with no seed of its own rolls one, which is what the dialog did as it opened.
	if (RandomMapGen.SeedData.Seed == -1) {
		RandomMapGen.SeedData.Seed = Sim_Random_Pick(0U, 65535U);
	}

	// The preview button is the one control the map debugger takes away, because that build
	// generates the map outright rather than previewing it.
	CanPreview = !Debug_Map;

	Build_Choices();
	Refresh();
}


void UIMapGenPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_MAPGEN_SET) {
		Set_Value(intent.Identity, intent.Value);
		return;
	}

	if (intent.Action == UI_MAPGEN_TOGGLE) {
		if (intent.Identity == "lifeforms" && Lifeforms.Enabled) Lifeforms.State = !Lifeforms.State;
		else if (intent.Identity == "ionstorms" && IonStorms.Enabled) IonStorms.State = !IonStorms.State;
		else if (intent.Identity == "transitions" && Transitions.Enabled) Transitions.State = !Transitions.State;
		return;
	}

	if (intent.Action == UI_MAPGEN_SEED) {
		SeedText = intent.Identity;
		return;
	}

	if (intent.Action == UI_MAPGEN_CANCEL) {
		Answer(ANSWER_CANCELLED);
		return;
	}

	if (intent.Action == UI_MAPGEN_OK) {
		Apply();

		if (Debug_Map) {
			RandomMapGen.Generate_Random_Map(false);
			Scen->Set_Scenario_Name(Fetch_String(TXT_RANDOM_MAP_DESCRIPTION));
			Write_Scenario_INI("RandMap.Map", true);
		} else if (RandomMapGen.MapPreview == NULL || RandomMapGen.MapPreview->Get_Preview_Surface() == NULL) {
			// A map the player never previewed has to be built before it can be accepted.
			RandomMapGen.Generate_Random_Map(true);
		}

		Answer(ANSWER_ACCEPTED);
		return;
	}

	if (intent.Action == UI_MAPGEN_PREVIEW) {
		if (!CanPreview) return;
		Apply();
		Generate_Preview();
		return;
	}

	if (intent.Action == UI_MAPGEN_SURPRISE) {
		if (!CanSurprise) return;
		Apply();
		RandomMapGen.SeedData.Randomize();
		Refresh();
		return;
	}

	if (intent.Action == UI_MAPGEN_LOAD) {
		if (!CanLoad) return;
		Apply();
		Pending = PENDING_LOAD;
		return;
	}

	if (intent.Action == UI_MAPGEN_SAVE) {
		Apply();
		Pending = PENDING_SAVE;
		return;
	}

	if (intent.Action == UI_MAPGEN_DELETE) {
		if (!CanDelete) return;
		Apply();
		Pending = PENDING_DELETE;
		return;
	}
}


/// <summary>
/// Reads the generator's settings and the territory's permissions into the view-model.
/// </summary>
void UIMapGenPresenterClass::Refresh(void)
{
	MapSeedClass & seed = RandomMapGen.SeedData;
	WDTTerritory const * const wdt = Tournament_Territory();

	seed.Fixup_Settings();

	Biome = seed.Biome;
	Time = seed.Time;
	Width = seed.Width;
	Height = seed.Height;

	char text[32];
	std::snprintf(text, sizeof(text), "%d", seed.Seed);
	SeedText = text;

	BiomeEnabled = wdt == NULL || wdt->UserModBiome;
	TimeEnabled = wdt == NULL || wdt->UserModTime;
	WidthEnabled = wdt == NULL || wdt->UserModWidth;
	HeightEnabled = wdt == NULL || wdt->UserModHeight;
	SeedEnabled = wdt == NULL || wdt->UserModSeed;

	if (wdt != NULL) {
		Set_Range(Tiberium, wdt->TiberiumAmountMin, wdt->TiberiumAmountMax, seed.Tiberium, wdt->UserModTiberiumAmount);
		Set_Range(Hills, wdt->HillsMin, wdt->HillsMax, seed.Hills, wdt->UserModHills);
		Set_Range(Water, wdt->WaterMin, wdt->WaterMax, seed.WaterAmount, wdt->UserModWater);
		Set_Range(Cliffs, wdt->CliffsMin, wdt->CliffsMax, seed.Cliffs, wdt->UserModCliffs);
		Set_Range(Vegetation, wdt->VegetationMin, wdt->VegetationMax, seed.Vegetation, wdt->UserModVegetation);
		Set_Range(Cities, wdt->CitiesMin, wdt->CitiesMax, seed.Cities, wdt->UserModCities);
		Set_Range(TiberiumFields, wdt->TiberiumFieldsMin, wdt->TiberiumFieldsMax, seed.TiberiumLayout, wdt->UserModTiberiumFields);
		Set_Range(Accessibility, wdt->AccessibilityMin, wdt->AccessibilityMax, seed.Accessibility, wdt->UserModAccessability);
		Set_Range(Veinholes, 0, 5, seed.VeinholeMonsters, wdt->UserModVeinholeMonsters);

		// The territory fixes the teams, so the boxes say what they are rather than
		// offering a choice.
		OneOnOne = false;
		TwoOnTwo = true;

		Lifeforms.State = seed.TiberiumWildlife > 0;
		Lifeforms.Enabled = wdt->UserModTiberiumCreatures;
		Transitions.State = seed.UseTransitions;
		Transitions.Enabled = wdt->UserModTimeTransitions;
		IonStorms.State = seed.UseIonStorms;
		IonStorms.Enabled = true;

		// Nothing left to roll leaves the randomize button dead.
		CanSurprise = wdt->UserModBiome || wdt->UserModTime || wdt->UserModCliffs
			|| wdt->UserModAccessability || wdt->UserModHills || wdt->UserModTiberiumAmount
			|| wdt->UserModTiberiumFields || wdt->UserModWater || wdt->UserModVegetation
			|| wdt->UserModCities || wdt->UserModWidth || wdt->UserModHeight
			|| wdt->UserModVeinholeMonsters;
	} else {
		Set_Range(Tiberium, 1, 100, seed.Tiberium, true);
		Set_Range(Players, 2, MAX_PLAYERS, seed.NumPlayers, true);
		Set_Range(Hills, 0, 100, seed.Hills, true);
		Set_Range(Water, 0, 100, seed.WaterAmount, true);
		Set_Range(Cliffs, 0, 100, seed.Cliffs, true);
		Set_Range(Vegetation, 0, 100, seed.Vegetation, true);
		Set_Range(Cities, 0, 100, seed.Cities, true);
		Set_Range(TiberiumFields, 0, 100, seed.TiberiumLayout, true);
		Set_Range(Accessibility, 0, 100, seed.Accessibility, true);
		Set_Range(Veinholes, 0, 5, seed.VeinholeMonsters, true);

		Lifeforms.State = seed.TiberiumWildlife > 0;
		Lifeforms.Enabled = true;
		Transitions.State = seed.UseTransitions;
		Transitions.Enabled = true;
		IonStorms.State = seed.UseIonStorms;
		IonStorms.Enabled = true;

		CanSurprise = true;
	}

	Refresh_File_Buttons();
	SettingsChanged = true;
}


/// <summary>
/// The maintenance the driver ran on every pass of its own loop: the caller's progress
/// callback and the title screen behind the screen.
/// </summary>
void UIMapGenPresenterClass::Service(void)
{
	if (Callback != NULL) {
		Callback();
	}
	Title_Screen_Restore(false);
}


/// <summary>
/// Runs the browser the player asked for with this screen out of the way.
/// </summary>
void UIMapGenPresenterClass::Run_Pending(void)
{
	PendingType const pending = Pending;
	Pending = PENDING_NONE;

	MapSeedClass & seed = RandomMapGen.SeedData;

	switch (pending) {
		case PENDING_LOAD:
			if (seed.LoadOptionsClass::Load()) {
				Refresh();

				// A loaded seed is previewed at once, which the dialog did by posting its
				// own preview command to itself after it had put the settings back.
				Queue(UIIntent{UI_MAPGEN_PREVIEW, "", 0});
				return;
			}
			Refresh();
			return;

		case PENDING_SAVE:
			seed.MapDescription[0] = '\0';
			seed.LoadOptionsClass::Save(seed.MapDescription);
			Refresh_File_Buttons();
			return;

		case PENDING_DELETE:
			seed.LoadOptionsClass::Delete();
			Refresh_File_Buttons();
			return;

		default:
			return;
	}
}


/// <summary>
/// Writes the view-model back into the generator's settings, so that whatever the player
/// has dialed in becomes the seed the generator works from.
/// </summary>
void UIMapGenPresenterClass::Apply(void)
{
	MapSeedClass & seed = RandomMapGen.SeedData;

	seed.Biome = Biome;
	seed.Time = Time;
	seed.Width = Width;
	seed.Height = Height;
	seed.Seed = std::atoi(SeedText.c_str());

	seed.Tiberium = Tiberium.Value;
	seed.Hills = Hills.Value;
	seed.WaterAmount = Water.Value;
	seed.Cliffs = Cliffs.Value;
	seed.Vegetation = Vegetation.Value;
	seed.Cities = Cities.Value;
	seed.TiberiumLayout = TiberiumFields.Value;
	seed.Accessibility = Accessibility.Value;

	// A tournament territory fixes the player count at four; the variant that shows it has
	// no player bar at all.
	seed.NumPlayers = Variant == VARIANT_WDT ? 4 : Players.Value;

	seed.TiberiumWildlife = 0;
	seed.VeinholeMonsters = 0;
	seed.UseIonStorms = false;
	seed.UseTransitions = false;
	seed.UseBlueTiberium = false;

	if (Addon_Enabled(ADDON_FIRESTORM)) {
		seed.TiberiumWildlife = Lifeforms.State ? 30 : 0;
		seed.VeinholeMonsters = Veinholes.Value;
		seed.UseIonStorms = IonStorms.State;
		seed.UseTransitions = Transitions.State;
		seed.UseBlueTiberium = (double)seed.Tiberium > 0.75;
	}

	seed.Fixup_Settings();
}


/// <summary>
/// Reports what the view-model holds for a control, so a view can drop a change that only
/// puts back the value it was given.
/// </summary>
int UIMapGenPresenterClass::Value_Of(std::string const & field) const
{
	if (field == "biome") return(Biome);
	if (field == "time") return(Time);
	if (field == "width") return(Width);
	if (field == "height") return(Height);

	if (field == "players") return(Players.Value);
	if (field == "accessibility") return(Accessibility.Value);
	if (field == "cliffs") return(Cliffs.Value);
	if (field == "hills") return(Hills.Value);
	if (field == "tiberium") return(Tiberium.Value);
	if (field == "tiberiumfields") return(TiberiumFields.Value);
	if (field == "water") return(Water.Value);
	if (field == "vegetation") return(Vegetation.Value);
	if (field == "cities") return(Cities.Value);
	if (field == "veinholes") return(Veinholes.Value);

	return(-1);
}


void UIMapGenPresenterClass::Set_Value(std::string const & field, int value)
{
	if (field == "biome") { Biome = value; return; }
	if (field == "time") { Time = value; return; }
	if (field == "width") { Width = value; return; }
	if (field == "height") { Height = value; return; }

	if (field == "players") { Players.Value = value; return; }
	if (field == "accessibility") { Accessibility.Value = value; return; }
	if (field == "cliffs") { Cliffs.Value = value; return; }
	if (field == "hills") { Hills.Value = value; return; }
	if (field == "tiberium") { Tiberium.Value = value; return; }
	if (field == "tiberiumfields") { TiberiumFields.Value = value; return; }
	if (field == "water") { Water.Value = value; return; }
	if (field == "vegetation") { Vegetation.Value = value; return; }
	if (field == "cities") { Cities.Value = value; return; }
	if (field == "veinholes") { Veinholes.Value = value; return; }

}


/// <summary>
/// Builds the three lists the combo boxes offer. The mutated environment belongs to
/// Firestorm and is left out without it.
/// </summary>
void UIMapGenPresenterClass::Build_Choices(void)
{
	Biomes.clear();
	for (int index = BIOME_FIRST; index < BIOME_COUNT; index++) {
		if (index != BIOME_MUTATED || Addon_Enabled(ADDON_FIRESTORM)) {
			Biomes.push_back(ChoiceType{Fetch_String(_BiomeNames[index]), index});
		}
	}

	Times.clear();
	for (int index = TIME_OF_DAY_FIRST; index < TIME_OF_DAY_COUNT; index++) {
		Times.push_back(ChoiceType{Fetch_String(_TimeNames[index]), index});
	}

	Sizes.clear();
	for (int index = 0; index < MAPSIZE_COUNT; index++) {
		Sizes.push_back(ChoiceType{Fetch_String(_SizeNames[index]), index});
	}

	// Only the environment and time of day combos carry CBS_SORT, so only those two are
	// listed by name; the two size lists keep the order their settings are numbered in.
	auto by_name = [](ChoiceType const & left, ChoiceType const & right) { return(left.Name < right.Name); };
	std::sort(Biomes.begin(), Biomes.end(), by_name);
	std::sort(Times.begin(), Times.end(), by_name);
}


void UIMapGenPresenterClass::Answer(int answer)
{
	UIResult result;
	result.Outcome = answer == ANSWER_ACCEPTED ? UIResult::OUTCOME_ACCEPTED : UIResult::OUTCOME_CANCELLED;
	result.Value = answer;
	Result = result;
}


/// <summary>
/// Builds the map the current settings describe and keeps it as the seed a later accept
/// works from.
/// </summary>
void UIMapGenPresenterClass::Generate_Preview(void)
{
	RandomMapGen.Generate_Random_Map(true);
	RandomMapGen.MapPreview->Create_Preview();

	delete RandomMapGen.MapSeeder;
	RandomMapGen.MapSeeder = new MapSeedClass;
	memcpy(RandomMapGen.MapSeeder, &RandomMapGen.SeedData, sizeof(MapSeedClass));

	PreviewChanged = true;
}


void UIMapGenPresenterClass::Refresh_File_Buttons(void)
{
	bool const present = RandomMapGen.SeedData.Files_Present();
	CanLoad = present;
	CanDelete = present;
}


/*
**	The RmlUi view. The three templates are one document family: they differ by which
**	controls exist and where they stand, so each carries its own document and its own model
**	name.
*/
namespace {

	// A combo row as the document shows it.
	struct ChoiceViewType
	{
		std::string Name;
		int Value = 0;
	};


	class MapGenViewClass : public UIRmlViewClass
	{
		public:
			MapGenViewClass(UIMapGenPresenterClass & presenter, char const * document)
				: UIRmlViewClass(presenter, document), Screen(presenter) {}

			virtual void Bind(Rml::DataModelConstructor & model) override;
			virtual void Sync(void) override;

			// Puts the ranges on the track bars before their values, because a range control
			// clamps a value into the range it is holding.
			void Arm_Ranges(void);

			void Attach_Preview(void);
			void Release_Preview(void);

		private:
			void Rebuild_Rows(void);
			void Bind_Range(Rml::DataModelConstructor & model, char const * name,
				UIMapGenPresenterClass::RangeType & range);

			UIMapGenPresenterClass & Screen;

			std::vector<ChoiceViewType> BiomeRows;
			std::vector<ChoiceViewType> TimeRows;
			std::vector<ChoiceViewType> SizeRows;

			std::unique_ptr<MapPreviewSurfaceClass> Preview;

			// Have the controls been given their spans yet? A change raised while they are
			// being armed is the model settling, not the player moving anything.
			bool Settled = false;
	};


	// The name the document gives the preview's pixels, and the interior of the template's
	// preview frame in game logical units.
	char const * const PREVIEW_SURFACE = "mapgenpreview";
	int const PREVIEW_WIDTH = 292;
	int const PREVIEW_HEIGHT = 214;


	void MapGenViewClass::Rebuild_Rows(void)
	{
		BiomeRows.clear();
		for (UIMapGenPresenterClass::ChoiceType const & row : Screen.Biomes) {
			BiomeRows.push_back(ChoiceViewType{row.Name, row.Value});
		}

		TimeRows.clear();
		for (UIMapGenPresenterClass::ChoiceType const & row : Screen.Times) {
			TimeRows.push_back(ChoiceViewType{row.Name, row.Value});
		}

		SizeRows.clear();
		for (UIMapGenPresenterClass::ChoiceType const & row : Screen.Sizes) {
			SizeRows.push_back(ChoiceViewType{row.Name, row.Value});
		}
	}


	void MapGenViewClass::Bind_Range(Rml::DataModelConstructor & model, char const * name,
		UIMapGenPresenterClass::RangeType & range)
	{
		model.Bind(name, &range.Value);

		Rml::String enabled = name;
		enabled += "on";
		model.Bind(enabled, &range.Enabled);
	}


	void MapGenViewClass::Bind(Rml::DataModelConstructor & model)
	{
		Rebuild_Rows();

		if (auto choice = model.RegisterStruct<ChoiceViewType>()) {
			choice.RegisterMember("name", &ChoiceViewType::Name);
			choice.RegisterMember("value", &ChoiceViewType::Value);
		}
		model.RegisterArray<std::vector<ChoiceViewType>>();

		model.Bind("biomes", &BiomeRows);
		model.Bind("times", &TimeRows);
		model.Bind("sizes", &SizeRows);

		model.Bind("biome", &Screen.Biome);
		model.Bind("time", &Screen.Time);
		model.Bind("width", &Screen.Width);
		model.Bind("height", &Screen.Height);

		model.Bind("biomeon", &Screen.BiomeEnabled);
		model.Bind("timeon", &Screen.TimeEnabled);
		model.Bind("widthon", &Screen.WidthEnabled);
		model.Bind("heighton", &Screen.HeightEnabled);

		model.Bind("seed", &Screen.SeedText);
		model.Bind("seedon", &Screen.SeedEnabled);

		Bind_Range(model, "players", Screen.Players);
		Bind_Range(model, "accessibility", Screen.Accessibility);
		Bind_Range(model, "cliffs", Screen.Cliffs);
		Bind_Range(model, "hills", Screen.Hills);
		Bind_Range(model, "tiberium", Screen.Tiberium);
		Bind_Range(model, "tiberiumfields", Screen.TiberiumFields);
		Bind_Range(model, "water", Screen.Water);
		Bind_Range(model, "vegetation", Screen.Vegetation);
		Bind_Range(model, "cities", Screen.Cities);
		Bind_Range(model, "veinholes", Screen.Veinholes);

		model.Bind("lifeforms", &Screen.Lifeforms.State);
		model.Bind("lifeformson", &Screen.Lifeforms.Enabled);
		model.Bind("ionstorms", &Screen.IonStorms.State);
		model.Bind("ionstormson", &Screen.IonStorms.Enabled);
		model.Bind("transitions", &Screen.Transitions.State);
		model.Bind("transitionson", &Screen.Transitions.Enabled);

		model.Bind("oneonone", &Screen.OneOnOne);
		model.Bind("twoontwo", &Screen.TwoOnTwo);

		model.Bind("cansurprise", &Screen.CanSurprise);
		model.Bind("canpreview", &Screen.CanPreview);
		model.Bind("canload", &Screen.CanLoad);
		model.Bind("candelete", &Screen.CanDelete);

		model.BindEventCallback("press",
			[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
				if (arguments.empty()) return;
				Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
			});

		// A form control is bound one way and a change matching the value the model holds is
		// dropped, so putting the model on a control cannot look like the player moving it.
		model.BindEventCallback("change",
			[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const & arguments) {
				if (!Settled || arguments.empty()) return;

				Rml::String const field = arguments[0].Get<Rml::String>();
				int const value = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);
				if (value == Screen.Value_Of(field)) return;

				Screen.Queue(UIIntent{UI_MAPGEN_SET, field, value});
			});

		model.BindEventCallback("toggle",
			[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
				if (arguments.empty()) return;
				Screen.Queue(UIIntent{UI_MAPGEN_TOGGLE, arguments[0].Get<Rml::String>(), 0});
			});

		model.BindEventCallback("typeseed",
			[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
				Rml::String const value = event.GetParameter<Rml::String>("value", Rml::String());
				if (value == Screen.SeedText) return;
				Screen.Queue(UIIntent{UI_MAPGEN_SEED, value, 0});
			});

		model.BindEventCallback("key",
			[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
				int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
				if (key == Rml::Input::KI_ESCAPE) {
					Screen.Queue(UIIntent{UI_MAPGEN_CANCEL, "", 0});
				}
			});
	}


	/// <summary>
	/// Puts each track bar's span on the control before its value, because a range control
	/// clamps a value into the range it is holding.
	/// </summary>
	void MapGenViewClass::Arm_Ranges(void)
	{
		if (Element == nullptr) return;

		Settled = false;

		struct { char const * Id; UIMapGenPresenterClass::RangeType const * Range; } const bars[] = {
			{ "players", &Screen.Players },
			{ "accessibility", &Screen.Accessibility },
			{ "cliffs", &Screen.Cliffs },
			{ "hills", &Screen.Hills },
			{ "tiberium", &Screen.Tiberium },
			{ "tiberiumfields", &Screen.TiberiumFields },
			{ "water", &Screen.Water },
			{ "vegetation", &Screen.Vegetation },
			{ "cities", &Screen.Cities },
			{ "veinholes", &Screen.Veinholes },
		};

		for (auto const & bar : bars) {
			Rml::Element * const element = Element->GetElementById(bar.Id);
			if (element == nullptr) continue;

			element->SetAttribute("min", bar.Range->Min);
			element->SetAttribute("max", bar.Range->Max);
			element->SetAttribute("step", 1);
			element->SetAttribute("value", bar.Range->Value);
		}

		Settled = true;
	}


	void MapGenViewClass::Attach_Preview(void)
	{
		Preview = std::make_unique<MapPreviewSurfaceClass>(PREVIEW_WIDTH, PREVIEW_HEIGHT, &RandomMapGen.MapPreview);
		UI_Register_Surface(PREVIEW_SURFACE, Preview.get());
	}


	void MapGenViewClass::Release_Preview(void)
	{
		UI_Unregister_Surface(PREVIEW_SURFACE);
		Preview.reset();
	}


	void MapGenViewClass::Sync(void)
	{
		if (!Model) return;

		if (Screen.PreviewChanged && Preview != nullptr) {
			Preview->Redraw();
			Screen.PreviewChanged = false;
		}

		if (Screen.SettingsChanged) {
			Arm_Ranges();
			Screen.SettingsChanged = false;
		}

		Model.DirtyAllVariables();
	}


	MapGenViewClass * _View = NULL;

}	// namespace


void UI_MapGen_Preview_Changed(void)
{
	if (_MapGenScreen == NULL || _View == NULL) {
		return;
	}

	// The generator does not pump between phases, so the picture is put on screen here, the
	// way the dialog got a synchronous repaint out of SendMessage.
	_MapGenScreen->PreviewChanged = true;
	_View->Sync();
	UI_Paint_Now(false);
}


/// <summary>
/// Shows the variant this session gets and runs it until the player accepts or cancels.
/// </summary>
/// <returns>What Do_Random_Map_Dialog reports: 1 accepted, 2 cancelled, 0 not shown.</returns>
int UI_MapGen_Run(UIMapGenPresenterClass & presenter)
{
	char const * document = "mapgen.rml";
	if (presenter.Variant == UIMapGenPresenterClass::VARIANT_WDT) {
		document = "mapgenwdt.rml";
	} else if (presenter.Variant == UIMapGenPresenterClass::VARIANT_FIRESTORM) {
		document = "mapgenfs.rml";
	}

	MapGenViewClass * const view = new MapGenViewClass(presenter, document);
	if (!view->Prepare(true)) {
		delete view;
		return(0);
	}

	_View = view;
	_MapGenScreen = &presenter;

	view->Attach_Preview();
	view->Arm_Ranges();
	view->Sync();

	while (!presenter.Result.has_value()) {
		UI_Run_Modal(presenter, *view);

		if (!presenter.Suspends()) {
			break;
		}

		// A browser draws where this screen is, so the document steps aside for it.
		view->Hide();
		presenter.Run_Pending();
		view->Show();
		view->Sync();
	}

	view->Release_Preview();
	view->Close();

	_MapGenScreen = NULL;
	_View = NULL;
	delete view;

	return(presenter.Result.has_value() ? presenter.Result->Value : UIMapGenPresenterClass::ANSWER_CANCELLED);
}
