/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The multiplayer map selection screen's behavior, with no toolkit in it. The skirmish
// setup screen and the network lobbies both open it.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


inline constexpr char const * UI_SCENARIOPICK_SELECT = "select";
inline constexpr char const * UI_SCENARIOPICK_ACCEPT = "accept";
inline constexpr char const * UI_SCENARIOPICK_CANCEL = "cancel";
inline constexpr char const * UI_SCENARIOPICK_RANDOM = "random";


// The artwork the map preview is registered under. A presenter carries the name; the view
// owns the provider.
inline constexpr char const * UI_MAP_PREVIEW_SURFACE = "mappreview";


class UIScenarioPickPresenterClass : public UIPresenterClass
{
	public:
		// A screen this one opens on top of itself. The map generator draws where this
		// screen is, so the view steps aside for it, which is what its ShowWindow did.
		enum SubScreenType {
			SUB_NONE,
			SUB_RANDOM_MAP,
		};

		UIScenarioPickPresenterClass(void);

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;
		virtual bool Suspends(void) const override { return(Pending != SUB_NONE); }

		// Runs the sub-screen an executed intent asked for and clears the request.
		void Run_Pending(void);

		// Which scenario the player settled on. Only meaningful once the screen accepted.
		int Chosen(void) const { return(Selected); }

		/*
		**	The view-model.
		*/
		std::vector<std::string> Scenarios;
		int Selected = 0;

		// The name of the artwork the preview draws into, never a surface.
		std::string Preview = UI_MAP_PREVIEW_SURFACE;

		// Moves whenever the preview was rebuilt, so a view marks its provider dirty once
		// per change rather than once per pass.
		unsigned int PreviewGeneration = 0;

		SubScreenType Pending = SUB_NONE;

		// Has the list itself changed? Only the map generator moves it.
		bool ListChanged = false;

	private:
		void Build_List(void);
		void Preview_Selection(void);

		// The scenario the screen opened on. The preview walks the list without moving the
		// session's own choice, so the session is put back after every look.
		int Original = 0;
		int LastPreviewed = -1;
};


// Shows the picker through its RmlUi view.
UIResult UI_Scenario_Pick_Screen(UIScenarioPickPresenterClass & presenter);
