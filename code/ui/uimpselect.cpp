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


//---------------------------------------------------------------------------------------
// The RmlUi view. One document per template, because the two differ by which buttons exist
// rather than by how one is arranged.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the multiplayer game selection screen.
/// </summary>
class MPSelectViewClass : public UIRmlViewClass
{
	public:
		MPSelectViewClass(UIMPSelectPresenterClass & presenter, char const * document) :
			UIRmlViewClass(presenter, document),
			Screen(presenter)
		{
		}

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override {}

	private:
		UIMPSelectPresenterClass & Screen;
};


void MPSelectViewClass::Bind(Rml::DataModelConstructor & model)
{
	model.Bind("internet", &Screen.InternetAvailable);
	model.Bind("worlddom", &Screen.WorldDominationAvailable);

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{arguments[0].Get<Rml::String>(), "", 0});
		});

	// The Main Menu button is the template's IDCANCEL, and Enter reaches the same default
	// arm the dialog sent every unhandled identifier to.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_ESCAPE || key == Rml::Input::KI_RETURN
				|| key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_MPSELECT_BACK, "", 0});
			}
		});
}


/// <summary>
/// Shows the multiplayer game choices and waits for the player to make one.
/// </summary>
UIResult UI_MPlayer_Select_Screen(UIMPSelectPresenterClass & presenter)
{
	char const * const document =
		(presenter.Variant == UIMPSelectPresenterClass::VARIANT_FIRESTORM) ? "mpselectfs.rml" : "mpselect.rml";

	MPSelectViewClass view(presenter, document);

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
