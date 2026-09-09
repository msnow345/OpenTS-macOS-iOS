/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The game controls screen's behavior, with no toolkit in it. Three templates share it and
// they carry different controls, so the variant is part of the view-model rather than
// something a view works out for itself.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


// What a view raises. Value carries the slider step or the check state.
inline constexpr char const * UI_GAMECTRL_SPEED = "speed";
inline constexpr char const * UI_GAMECTRL_SCROLL = "scroll";
inline constexpr char const * UI_GAMECTRL_DETAIL = "detail";
inline constexpr char const * UI_GAMECTRL_DIFFICULTY = "difficulty";
inline constexpr char const * UI_GAMECTRL_CAMEO_TEXT = "cameotext";
inline constexpr char const * UI_GAMECTRL_ACTION_LINES = "actionlines";
inline constexpr char const * UI_GAMECTRL_TOOLTIPS = "tooltips";
inline constexpr char const * UI_GAMECTRL_COASTING = "coasting";
inline constexpr char const * UI_GAMECTRL_EDGE_SCROLL = "edgescroll";
inline constexpr char const * UI_GAMECTRL_SOUND = "sound";
inline constexpr char const * UI_GAMECTRL_KEYBOARD = "keyboard";
inline constexpr char const * UI_GAMECTRL_ACCEPT = "accept";
inline constexpr char const * UI_GAMECTRL_CANCEL = "cancel";


class UIGameControlsPresenterClass : public UIPresenterClass
{
	public:
		// Which of the three templates the state below belongs to. The names are the
		// game's own, and they do not mean what they look like: the screen shown with no
		// game running is IDD_OPT_CTRL_GAME_SP and the one shown during any local game is
		// IDD_OPT_CTRL_GAME_MP.
		enum VariantType {
			VARIANT_FRONTEND,
			VARIANT_SESSION,
			VARIANT_INTERNET,
		};

		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_ACCEPT,
			CHOICE_CANCEL,
			CHOICE_SOUND,
			CHOICE_KEYBOARD,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// Writes the staged settings back into the game and saves them. The driver calls
		// this where the dialog called Set, which is after its loop and before the screen
		// comes down.
		void Apply(void);

		// Does the way the player left the screen commit the settings? Leaving through the
		// sound or keyboard button does, which is not obvious and is the dialog's own
		// behavior: both wrote IDOK.
		bool Commits(void) const;

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		VariantType Variant = VARIANT_FRONTEND;

		// Slider steps. Game speed and scroll rate count backward, so the fastest sits at
		// step zero; detail and difficulty count forward. A view shows steps; only this
		// class knows what they mean.
		int SpeedStep = 0;
		int ScrollStep = 0;
		int DetailStep = 0;
		int DifficultyStep = 0;

		bool CameoText = false;
		bool ActionLines = false;
		bool ShowToolTips = false;
		bool Coasting = false;
		bool EdgeScroll = false;

		// Is there an audio device to talk to? With none the sound button is disabled, as
		// the dialog disabled it.
		bool SoundAvailable = false;

		// Can the pointer be moved to a position the game chooses? Without that the only
		// scroll method left is the coasting one, so the choice is shown and locked rather
		// than removed.
		bool CoastingAvailable = false;

		std::vector<std::string> SpeedLabels;
		std::vector<std::string> ScrollLabels;
		std::vector<std::string> DetailLabels;
		std::vector<std::string> DifficultyLabels;

		// What each variant carries. A setting whose control the template omits is not
		// applied, which is what the dialog's own null checks on GetDlgItem amounted to.
		bool Has_Speed(void) const { return(Variant != VARIANT_INTERNET); }
		bool Has_Difficulty(void) const { return(Variant == VARIANT_FRONTEND); }
		bool Has_Sub_Screens(void) const { return(Variant != VARIANT_FRONTEND); }

		ChoiceType Choice = CHOICE_NONE;
};


// Shows the screen through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_Game_Controls_Screen(UIGameControlsPresenterClass & presenter);
