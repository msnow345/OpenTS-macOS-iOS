/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The multiplayer game selection screen's behavior, with no toolkit in it. Two templates
// share it and they differ by which buttons exist, so the variant is part of the view-model
// rather than something a view works out for itself.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"


inline constexpr char const * UI_MPSELECT_INTERNET = "internet";
inline constexpr char const * UI_MPSELECT_WORLDDOM = "worlddom";
inline constexpr char const * UI_MPSELECT_MODEM = "modem";
inline constexpr char const * UI_MPSELECT_NETWORK = "network";
inline constexpr char const * UI_MPSELECT_SKIRMISH = "skirmish";
inline constexpr char const * UI_MPSELECT_BACK = "back";


class UIMPSelectPresenterClass : public UIPresenterClass
{
	public:
		enum VariantType {
			VARIANT_BASE,
			VARIANT_FIRESTORM,
		};

		enum ChoiceType {
			CHOICE_NONE,
			CHOICE_NETWORK,
			CHOICE_SKIRMISH,
			CHOICE_BACK,
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		// The session type the caller's own switch is written against.
		int Session_Type(void) const;

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		VariantType Variant = VARIANT_BASE;

		// Neither the online service these led to nor the tour it hosted can be reached, so
		// the buttons stay on the screen and never answer, which is what the dialog did by
		// disabling them.
		bool InternetAvailable = false;
		bool WorldDominationAvailable = false;

		ChoiceType Choice = CHOICE_NONE;
};


// Shows the screen through its RmlUi view. FAILED_TO_OPEN leaves nothing shown and the
// caller falls through to the legacy dialog.
UIResult UI_MPlayer_Select_Screen(UIMPSelectPresenterClass & presenter);
