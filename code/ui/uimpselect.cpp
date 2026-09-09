/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The multiplayer game selection screen. Behavior traced out of Select_MPlayer_Game and its
// procedure in mplayer.cpp.
//
// What the extraction fixes in place: only the network and skirmish buttons answer, and
// anything else leaves with no session chosen, which is the dialog's own default arm; the
// internet and world domination buttons stay where the template put them and are disabled,
// because neither the service they led to nor the tour it hosted can be reached; and the
// modem and serial button is on the template but reaches nothing, so it leaves as well.
//
// docs/UI_DESIGN.md, "Screens", owns the contracts this keeps to.

#include "always.h"

#include "uimpselect.h"

#include "uirmlview.h"

#include "addon.h"
#include "init.h"
#include "session.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>


void UIMPSelectPresenterClass::Refresh(void)
{
	Variant = (Addon_Installed(ADDON_FIRESTORM) == ADDON_FIRESTORM) ? VARIANT_FIRESTORM : VARIANT_BASE;
	InternetAvailable = false;
	WorldDominationAvailable = false;
	Choice = CHOICE_NONE;
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UIMPSelectPresenterClass::Service(void)
{
	Title_Screen_Restore();
}


/// <summary>
/// The session type the caller's own switch is written against.
/// </summary>
int UIMPSelectPresenterClass::Session_Type(void) const
{
	switch (Choice) {
		case CHOICE_NETWORK: return(GAME_IPX);
		case CHOICE_SKIRMISH: return(GAME_SKIRMISH);
		default: return(GAME_NORMAL);
	}
}


void UIMPSelectPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_MPSELECT_INTERNET && !InternetAvailable) {
		return;
	}
	if (intent.Action == UI_MPSELECT_WORLDDOM && !WorldDominationAvailable) {
		return;
	}

	UIResult result;
	result.Outcome = UIResult::OUTCOME_ACCEPTED;

	if (intent.Action == UI_MPSELECT_NETWORK) {
		Choice = CHOICE_NETWORK;
	} else if (intent.Action == UI_MPSELECT_SKIRMISH) {
		Choice = CHOICE_SKIRMISH;
	} else {
		// Modem and serial, and anything else the screen carries, leave with no session
		// chosen, which is where the dialog's default arm sent them.
		Choice = CHOICE_BACK;
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	}

	result.Value = Session_Type();
	Result = result;
}
