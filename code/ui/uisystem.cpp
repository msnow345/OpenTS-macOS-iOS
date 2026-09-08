/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// What RmlUi asks the host for: the clock its animations run on, where its messages go,
// the cursor it wants shown, the clipboard, and the strings its documents name.
//
// docs/UI_DESIGN.md, "Assets and strings", owns the string half.

#include "always.h"

#include "uiinternal.h"

#include "dbgprint.h"
#include "hostclock.h"

#include <RmlUi/Core/SystemInterface.h>

#include <windows.h>

#include <string>


static unsigned int _StartTime = 0;
static std::string _CursorName;


/// <summary>
/// Looks a document's string name up in the engine's string table.
/// The generated name table arrives with the UTF-8 transition, which docs/UI_DESIGN.md
/// makes a prerequisite of the first screen that shows text. Until then every name is
/// unknown and the document's own text is what appears.
/// </summary>
/// <returns>bool; Was the name resolved?</returns>
static bool Lookup_String(std::string const & name, std::string & text)
{
	(void)name;
	(void)text;
	return(false);
}


class UISystemInterface : public Rml::SystemInterface
{
	public:
		virtual double GetElapsedTime() override
		{
			return((double)(Host_Milliseconds() - _StartTime) / 1000.0);
		}

		virtual int TranslateString(Rml::String & translated, const Rml::String & input) override;

		virtual bool LogMessage(Rml::Log::Type type, const Rml::String & message) override
		{
			char const * label = "info";
			switch (type) {
				case Rml::Log::LT_ERROR: label = "error"; break;
				case Rml::Log::LT_ASSERT: label = "assert"; break;
				case Rml::Log::LT_WARNING: label = "warning"; break;
				default: break;
			}

			DebugString("[UI] %s: %s\n", label, message.c_str());
			return(true);
		}

		virtual void SetMouseCursor(const Rml::String & name) override
		{
			// The game's pointer is built from its own shapes rather than from a system
			// cursor, so a document cannot ask for one yet. The request is recorded for
			// the screen that first needs a text caret to act on.
			_CursorName = name;
		}

		virtual void SetClipboardText(const Rml::String & text) override;
		virtual void GetClipboardText(Rml::String & text) override;
};

static UISystemInterface _SystemInterface;


/// <summary>
/// Replaces a [[NAME]] reference with the engine string it names.
/// </summary>
/// <returns>int; How many replacements were made.</returns>
int UISystemInterface::TranslateString(Rml::String & translated, const Rml::String & input)
{
	translated.clear();
	int replaced = 0;

	std::size_t position = 0;
	while (position < input.size()) {
		std::size_t open = input.find("[[", position);
		if (open == Rml::String::npos) {
			break;
		}

		std::size_t close = input.find("]]", open + 2);
		if (close == Rml::String::npos) {
			break;
		}

		std::string text;
		std::string const name = input.substr(open + 2, close - open - 2);

		translated.append(input, position, open - position);

		if (Lookup_String(name, text)) {
			translated.append(text);
			replaced++;
		} else {
			translated.append(input, open, close + 2 - open);
		}

		position = close + 2;
	}

	translated.append(input, position, Rml::String::npos);
	return(replaced);
}


void UISystemInterface::SetClipboardText(const Rml::String & text)
{
#ifdef _WIN32
	if (!OpenClipboard(NULL)) {
		return;
	}

	EmptyClipboard();

	HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
	if (block != NULL) {
		char * buffer = (char *)GlobalLock(block);
		if (buffer != NULL) {
			memcpy(buffer, text.c_str(), text.size() + 1);
			GlobalUnlock(block);
			SetClipboardData(CF_TEXT, block);
		} else {
			GlobalFree(block);
		}
	}

	CloseClipboard();
#else
	// The compatibility layer carries no clipboard yet. An editable screen ships only
	// after paste has been exercised, so this has to be supplied before one does.
	(void)text;
#endif
}


void UISystemInterface::GetClipboardText(Rml::String & text)
{
	text.clear();

#ifdef _WIN32
	if (!OpenClipboard(NULL)) {
		return;
	}

	HANDLE block = GetClipboardData(CF_TEXT);
	if (block != NULL) {
		char const * buffer = (char const *)GlobalLock(block);
		if (buffer != NULL) {
			text = buffer;
			GlobalUnlock(block);
		}
	}

	CloseClipboard();
#endif
}


Rml::SystemInterface * UI_System_Interface(void)
{
	if (_StartTime == 0) {
		_StartTime = Host_Milliseconds();
	}

	return(&_SystemInterface);
}
