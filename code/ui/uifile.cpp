/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// RmlUi's files, read through the game's own file system so that a document, a style, an
// image or a font loads from a loose ui/ directory or from a mix on equal terms.
//
// Every reference is reduced to its bare name before it is opened. RmlUi joins a relative
// reference with the path of the document that named it, and CDFileClass treats a name
// carrying a directory as a literal path that skips the search order, so a joined name
// would only ever be found on disk. The name alone is the lookup key: the user path, then
// the current directory, then the search paths, then the mix files. That is what lets a
// mod override a document by shipping it earlier in that order.
//
// docs/UI_DESIGN.md, "Assets and strings", owns this.

#include "always.h"

#include "uiinternal.h"

#include "ccfile.h"
#include "dbgprint.h"

#include <RmlUi/Core/FileInterface.h>

#include <cstring>
#include <string>


/// <summary>
/// Strips every directory a reference carries, leaving the name the archives hold.
/// </summary>
static std::string Base_Name(std::string const & path)
{
	std::size_t const mark = path.find_last_of("\\/:");
	return(mark == std::string::npos ? path : path.substr(mark + 1));
}


class UIFileInterface : public Rml::FileInterface
{
	public:
		virtual Rml::FileHandle Open(const Rml::String & path) override
		{
			std::string const name = Base_Name(path);
			if (name.empty()) {
				return(0);
			}

			CCFileClass * file = new CCFileClass(name.c_str());
			if (!file->Is_Available() || !file->Open(FileClass::READ)) {
				delete file;
				DebugString("[UI] File %s was not found.\n", name.c_str());
				return(0);
			}

			return((Rml::FileHandle)file);
		}

		virtual void Close(Rml::FileHandle handle) override
		{
			CCFileClass * file = (CCFileClass *)handle;
			if (file != nullptr) {
				file->Close();
				delete file;
			}
		}

		virtual size_t Read(void * buffer, size_t size, Rml::FileHandle handle) override
		{
			CCFileClass * file = (CCFileClass *)handle;
			if (file == nullptr || buffer == nullptr || size == 0) {
				return(0);
			}

			// The engine counts bytes in an int, so a request larger than that is served in
			// pieces rather than truncated silently.
			size_t total = 0;
			while (total < size) {
				size_t const remaining = size - total;
				int const chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;

				int const read = file->Read((char *)buffer + total, chunk);
				if (read <= 0) {
					break;
				}

				total += (size_t)read;
				if (read < chunk) {
					break;
				}
			}

			return(total);
		}

		virtual bool Seek(Rml::FileHandle handle, long offset, int origin) override
		{
			CCFileClass * file = (CCFileClass *)handle;
			if (file == nullptr) {
				return(false);
			}

			int const size = file->Size();
			long target = offset;

			switch (origin) {
				case SEEK_CUR:
					target = (long)file->Seek(0, SEEK_CUR) + offset;
					break;

				case SEEK_END:
					target = (long)size + offset;
					break;

				default:
					break;
			}

			// Seek() clamps, so a request past either end would otherwise report success at
			// a position the caller never asked for.
			if (target < 0 || target > (long)size) {
				return(false);
			}

			return(file->Seek((int)target, SEEK_SET) == target);
		}

		virtual size_t Tell(Rml::FileHandle handle) override
		{
			CCFileClass * file = (CCFileClass *)handle;
			if (file == nullptr) {
				return(0);
			}

			int const position = file->Seek(0, SEEK_CUR);
			return(position < 0 ? 0 : (size_t)position);
		}

		virtual size_t Length(Rml::FileHandle handle) override
		{
			CCFileClass * file = (CCFileClass *)handle;
			if (file == nullptr) {
				return(0);
			}

			int const size = file->Size();
			return(size < 0 ? 0 : (size_t)size);
		}
};

static UIFileInterface _FileInterface;


Rml::FileInterface * UI_File_Interface(void)
{
	return(&_FileInterface);
}
