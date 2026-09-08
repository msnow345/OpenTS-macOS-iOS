/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The RmlUi half of a screen. A view owns its document and its data model and turns the
// toolkit's events into intents on the presenter it was built against. Only files under
// code/ui include this, because it names RmlUi types.
//
// docs/UI_DESIGN.md, "Screens", owns this contract.

#pragma once

#include "uiscreen.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>


class UIRmlViewClass
{
	public:
		UIRmlViewClass(UIPresenterClass & presenter, char const * document);
		virtual ~UIRmlViewClass(void);

		// Loads the document, binds the data model and shows it. A failure leaves nothing
		// shown and reports the resource that could not be prepared.
		bool Prepare(bool modal);

		// Hides and releases the document in the order docs/UI_DESIGN.md sets out: the
		// screen is marked closing first, then focus and capture are dropped, then the
		// model is removed while its storage still lives.
		void Close(void);

		bool Is_Visible(void) const;

		// Fills in the view-model's fields and events. Called once, before the document is
		// loaded, since a document binds its model as it parses.
		virtual void Bind(Rml::DataModelConstructor & model) = 0;

		// Marks whatever the last drained intents changed, so RmlUi redraws only that.
		virtual void Sync(void) = 0;

	protected:
		UIPresenterClass & Presenter;
		Rml::String Document;
		Rml::ElementDocument * Element = nullptr;
		Rml::DataModelHandle Model;
};


// Runs a screen to a result the way the legacy dialog drivers do. uishell.cpp owns it; it
// is declared here because it names both halves of a screen.
UIResult UI_Run_Modal(UIPresenterClass & presenter, UIRmlViewClass & view);
