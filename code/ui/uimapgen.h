/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The random map generator screen. One screen with three variants -- the base game's, the
// Firestorm one and the tournament one -- chosen by what the session is rather than by the
// caller, which is the sound screen's shape.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_MAPGEN_OK = "ok";
inline constexpr char const * UI_MAPGEN_CANCEL = "cancel";
inline constexpr char const * UI_MAPGEN_LOAD = "load";
inline constexpr char const * UI_MAPGEN_SAVE = "save";
inline constexpr char const * UI_MAPGEN_DELETE = "delete";
inline constexpr char const * UI_MAPGEN_PREVIEW = "preview";
inline constexpr char const * UI_MAPGEN_SURPRISE = "surprise";

// Carries a control's new value: the identity names the setting and the value carries it.
inline constexpr char const * UI_MAPGEN_SET = "set";

// Flips a check box. The identity names it.
inline constexpr char const * UI_MAPGEN_TOGGLE = "toggle";

// Carries the seed the player typed, in the identity, because it arrives as text.
inline constexpr char const * UI_MAPGEN_SEED = "seed";


class UIMapGenPresenterClass : public UIPresenterClass
{
	public:
		// Which of the three templates the screen is. The addon and the session decide it,
		// not the menu that opened the screen.
		enum VariantType {
			VARIANT_BASE,
			VARIANT_FIRESTORM,
			VARIANT_WDT,
		};

		// What the screen answers its driver with. Do_Random_Map_Dialog reports 1 for an
		// accepted map and 2 for a cancelled screen.
		enum {
			ANSWER_ACCEPTED = 1,
			ANSWER_CANCELLED = 2,
		};

		// A choice a combo box offers. The value is the setting, not the row, because two
		// of the lists are sorted by name and a row is not a setting.
		struct ChoiceType
		{
			std::string Name;
			int Value = 0;
		};

		// A track bar and the span it is allowed. A tournament territory narrows the span
		// and may lock the bar, and a span with nothing to choose between is shown disabled
		// rather than hidden, which is what Set_Scroll_Bar did.
		struct RangeType
		{
			int Min = 0;
			int Max = 100;
			int Value = 0;
			bool Enabled = true;
		};

		// A check box and whether the player may change it.
		struct SwitchType
		{
			bool State = false;
			bool Enabled = true;
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// The load, save and delete browsers draw where this screen is, so this screen is
		// stepped aside for them rather than run underneath.
		virtual bool Suspends(void) const override { return(Pending != PENDING_NONE); }

		// Picks the variant and reads the generator's settings into the view-model. Called
		// once, before a view is prepared.
		void Open(bool (*callback)());

		// What the view-model holds for a control, so a view can drop a change that only puts
		// back the value it was given.
		int Value_Of(std::string const & field) const;

		// Runs the browser the player asked for with this screen out of the way. Called by
		// the owner between passes, never from an event.
		void Run_Pending(void);

		/*
		**	The view-model.
		*/

		VariantType Variant = VARIANT_BASE;

		std::vector<ChoiceType> Biomes;
		std::vector<ChoiceType> Times;
		std::vector<ChoiceType> Sizes;

		int Biome = 0;
		int Time = 0;
		int Width = 0;
		int Height = 0;

		bool BiomeEnabled = true;
		bool TimeEnabled = true;
		bool WidthEnabled = true;
		bool HeightEnabled = true;

		std::string SeedText;
		bool SeedEnabled = true;

		RangeType Players;
		RangeType Accessibility;
		RangeType Cliffs;
		RangeType Hills;
		RangeType Tiberium;
		RangeType TiberiumFields;
		RangeType Water;
		RangeType Vegetation;
		RangeType Cities;
		RangeType Veinholes;

		SwitchType Lifeforms;
		SwitchType IonStorms;
		SwitchType Transitions;

		// The tournament variant's two team boxes. The dialog set them and left them
		// disabled, so they say what the territory is rather than offering a choice.
		bool OneOnOne = false;
		bool TwoOnTwo = true;

		bool CanSurprise = true;
		bool CanPreview = true;
		bool CanLoad = false;
		bool CanDelete = false;

		// Has the preview picture moved? The view reads it and clears it, because the
		// picture is the view's to own.
		bool PreviewChanged = false;

		bool SettingsChanged = false;

	private:
		enum PendingType {
			PENDING_NONE,
			PENDING_LOAD,
			PENDING_SAVE,
			PENDING_DELETE,
		};

		void Apply(void);
		void Set_Value(std::string const & field, int value);
		void Build_Choices(void);
		void Answer(int answer);
		void Generate_Preview(void);
		void Refresh_File_Buttons(void);

		PendingType Pending = PENDING_NONE;

		// The maintenance the driver ran on every pass of its own loop.
		bool (*Callback)(void) = NULL;
};


// The generator screen the driver is running, or NULL when none is up. The generator reaches
// the picture through this while it is building a map.
UIMapGenPresenterClass * UI_MapGen_Screen(void);

// The map preview has been redrawn. Called where the generator told the dialog to repaint.
void UI_MapGen_Preview_Changed(void);

// Shows the variant the screen says it is and runs it until the player accepts or cancels.
// The answer is what Do_Random_Map_Dialog returns; zero means no document was shown.
int UI_MapGen_Run(UIMapGenPresenterClass & presenter);
