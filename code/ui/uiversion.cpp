/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The version information screen: the first screen to migrate, and the pattern the rest
// follow. The presenter gathers the same facts the dialog procedure gathered and holds
// them as plain strings; the view renders them and turns a click or a key into an intent.
// Neither half knows about the other's world.
//
// docs/UI_DESIGN.md, "Screens", owns the contract this keeps to.

#include "always.h"

#include "uiversion.h"

#include "uirmlview.h"

#include "addon.h"
#include "data.h"
#include "getcpu.h"
#include "globals.h"
#include "language/language.h"
#include "opents_build.h"
#include "version.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <cstdio>


// What the view asks for. The strings are the intents' whole vocabulary, so a document can
// name an action without naming a control.
static char const * const ACTION_ACCEPT = "accept";
static char const * const ACTION_CANCEL = "cancel";


/// <summary>
/// The toolkit-free half of the version screen.
/// </summary>
class VersionPresenterClass : public UIPresenterClass
{
	public:
		// The view-model. Plain values, copied out of the engine by Refresh, and living
		// longer than the data model the view binds to them.
		std::vector<std::string> Lines;

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
};


/// <summary>
/// Collects what a player is asked for when they report a problem.
/// These are the same facts, from the same sources and in the same order, that the dialog
/// procedure put into its list box.
/// </summary>
void VersionPresenterClass::Refresh(void)
{
	char buffer[256];

	Lines.clear();

	if (Addon_Installed(ADDON_FIRESTORM) == true) {
		std::string title = Fetch_String(TXT_SHORT_TITLE);
		title += ": ";
		title += Get_Addon_Title(ADDON_FIRESTORM);
		Lines.push_back(title);
	} else {
		Lines.push_back(Fetch_String(TXT_SHORT_TITLE));
	}

	std::snprintf(buffer, sizeof(buffer), "Version %s", Version_Name());
	Lines.push_back(buffer);

	std::snprintf(buffer, sizeof(buffer), "Internal Version %s", VerNum.Version_Name());
	Lines.push_back(buffer);

#ifdef _DEBUG
	std::snprintf(buffer, sizeof(buffer), "Debug Build: %s - %s", OPENTS_BUILD_DESCRIPTION, OPENTS_COMMIT_DATE);
#else
	std::snprintf(buffer, sizeof(buffer), "Release Build: %s - %s", OPENTS_BUILD_DESCRIPTION, OPENTS_COMMIT_DATE);
#endif
	Lines.push_back(buffer);

	int cpu_type = 5;
	char vendor[32];
	vendor[0] = '\0';
	Get_CPU_Type(cpu_type, vendor, sizeof(vendor) - 1);
	std::snprintf(buffer, sizeof(buffer), "CPU vendor: %s", vendor);
	Lines.push_back(buffer);

	Get_Language_Version(buffer);
	Lines.push_back(buffer);
}


/// <summary>
/// Answers an intent the view raised. The screen reads nothing and changes nothing, so
/// the only transition it has is the one that ends it.
/// </summary>
void VersionPresenterClass::Execute(UIIntent const & intent)
{
	UIResult result;

	if (intent.Action == ACTION_ACCEPT) {
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
	} else if (intent.Action == ACTION_CANCEL) {
		result.Outcome = UIResult::OUTCOME_CANCELLED;
	} else {
		return;
	}

	Result = result;
}


/// <summary>
/// The RmlUi half. It owns the document, the data model bound to the presenter's
/// view-model, and the mapping from what the player did to what the screen was asked for.
/// </summary>
class VersionViewClass : public UIRmlViewClass
{
	public:
		VersionViewClass(VersionPresenterClass & presenter);

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

	private:
		void Dismiss(char const * action);

		VersionPresenterClass & Screen;
};


VersionViewClass::VersionViewClass(VersionPresenterClass & presenter) :
	UIRmlViewClass(presenter, "version.rml"),
	Screen(presenter)
{
}


void VersionViewClass::Dismiss(char const * action)
{
	UIIntent intent;
	intent.Action = action;
	Screen.Queue(intent);
}


void VersionViewClass::Bind(Rml::DataModelConstructor & model)
{
	model.RegisterArray<std::vector<std::string>>();
	model.Bind("lines", &Screen.Lines);

	// An event handler never acts: it queues, and the runner executes the queue after
	// Context::Update has returned.
	model.BindEventCallback("dismiss",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			Dismiss(arguments.empty() ? ACTION_ACCEPT : arguments[0].Get<Rml::String>().c_str());
		});

	// Return accepts and Escape cancels, which is what the dialog's IDOK and IDCANCEL did.
	// The document listens rather than the shell, because which keys dismiss a screen is
	// the screen's business.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);

			if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Dismiss(ACTION_ACCEPT);
			} else if (key == Rml::Input::KI_ESCAPE) {
				Dismiss(ACTION_CANCEL);
			}
		});
}


void VersionViewClass::Sync(void)
{
	// Nothing an intent can execute changes the view-model, so there is nothing to dirty.
	// The screen's only transition ends it.
}


/// <summary>
/// Shows the version information and waits for the player to dismiss it.
/// </summary>
/// <returns>The screen's result. OUTCOME_FAILED_TO_OPEN means nothing was shown.</returns>
UIResult UI_Version_Screen(void)
{
	// The presenter is declared first so that it is destroyed last: the data model the view
	// binds reads the presenter's view-model, and must not outlive it.
	VersionPresenterClass presenter;
	VersionViewClass view(presenter);

	presenter.Refresh();

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
