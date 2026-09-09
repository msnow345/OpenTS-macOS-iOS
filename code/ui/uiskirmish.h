/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The skirmish setup screen's behavior, with no toolkit in it.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_SKIRMISH_HANDLE = "handle";      // Identity: the name typed
inline constexpr char const * UI_SKIRMISH_SIDE = "side";          // Value: row in Sides
inline constexpr char const * UI_SKIRMISH_COLOR = "color";        // Value: row in Colors
inline constexpr char const * UI_SKIRMISH_SLIDER = "slider";      // Identity: which, Value: position
inline constexpr char const * UI_SKIRMISH_TOGGLE = "toggle";      // Identity: which
inline constexpr char const * UI_SKIRMISH_PICK_MAP = "pickmap";
inline constexpr char const * UI_SKIRMISH_ACCEPT = "accept";
inline constexpr char const * UI_SKIRMISH_CANCEL = "cancel";

// The sliders and check boxes, named rather than numbered, because an intent carries an
// identity and never a control.
inline constexpr char const * UI_SKIRMISH_UNITCOUNT = "unitcount";
inline constexpr char const * UI_SKIRMISH_CREDITS = "credits";
inline constexpr char const * UI_SKIRMISH_TECHLEVEL = "techlevel";
inline constexpr char const * UI_SKIRMISH_AILEVEL = "ailevel";
inline constexpr char const * UI_SKIRMISH_AIPLAYERS = "aiplayers";
inline constexpr char const * UI_SKIRMISH_GAMESPEED = "gamespeed";

inline constexpr char const * UI_SKIRMISH_BASES = "bases";
inline constexpr char const * UI_SKIRMISH_CRATES = "crates";
inline constexpr char const * UI_SKIRMISH_FOG = "fog";
inline constexpr char const * UI_SKIRMISH_BRIDGES = "bridges";
inline constexpr char const * UI_SKIRMISH_MCV = "mcv";
inline constexpr char const * UI_SKIRMISH_SHORTGAME = "shortgame";
inline constexpr char const * UI_SKIRMISH_ENGINEER = "engineer";


class UISkirmishPresenterClass : public UIPresenterClass
{
	public:
		// A screen this one opens on top of itself.
		enum SubScreenType {
			SUB_NONE,
			SUB_PICK_MAP,
		};

		// A track bar, with the range the dialog gave it.
		struct SliderType
		{
			int Value = 0;
			int Minimum = 0;
			int Maximum = 0;

			// The amount one move covers. Credits move in steps of 250, which is what
			// OD_SETTRACKSTEP set on that control.
			int Step = 1;
		};

		// A playable side, carrying the country it stands for rather than its position,
		// because the list holds only the countries that may be played.
		struct SideType
		{
			std::string Name;
			int Country = 0;
		};

		UISkirmishPresenterClass(void);

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;
		virtual bool Suspends(void) const override { return(Pending != SUB_NONE); }

		void Run_Pending(void);

		// Did the player ask for the game to start?
		bool Accepted(void) const { return(Outcome); }

		// Writes the player's multiplayer preferences, which the driver did on the way out
		// whichever way the screen was left.
		void End(void);

		/*
		**	The view-model.
		*/
		std::string Handle;

		std::vector<SideType> Sides;
		int SelectedSide = 0;

		std::vector<std::string> Colors;
		int SelectedColor = 0;

		SliderType UnitCount;
		SliderType Credits;
		SliderType TechLevel;
		SliderType AILevel;
		SliderType AIPlayers;
		SliderType GameSpeed;

		bool Bases = false;
		bool Crates = false;
		bool FogOfWar = false;
		bool Bridges = false;
		bool MCVRedeploy = false;
		bool ShortGame = false;
		bool MultiEngineer = false;

		std::string ScenarioName;

		// The name of the artwork the map preview draws into, never a surface.
		std::string Preview;

		// Moves whenever the preview was rebuilt.
		unsigned int PreviewGeneration = 0;

		// The longest handle the name field accepts, in bytes, which is the buffer the
		// dialog read the control into.
		enum { HANDLE_LIMIT = 19 };

		// Is the accept button available? The dialog disabled it while it checked whether
		// the map has room for the computer players asked for.
		bool CanAccept = true;

		SubScreenType Pending = SUB_NONE;

		bool ListChanged = false;

	private:
		void Accept(void);
		void Read_Identity(void);

		bool Outcome = false;
};


// Shows the skirmish screen through its RmlUi view.
UIResult UI_Skirmish_Screen(UISkirmishPresenterClass & presenter);
