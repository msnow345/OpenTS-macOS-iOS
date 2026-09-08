/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// Every string a player reads comes through Fetch_String, which asks LoadString for a
// resource compiled from language.rc. Only the resource compiler builds that resource, so
// on a host without one the same strings arrive as a flat data file generated from the same
// script at build time by cmake/StringTable.cmake. The file is UTF-8, because the resource
// script declares code page 65001 and the generator copies its bytes through unchanged.
//
// The format is length-prefixed so that a string containing a newline needs no escaping:
//
//   OPENTS-STRINGS 1\n
//   <count>\n
//   <id> <byte length>\n<bytes>\n      (repeated <count> times)

namespace
{

char const * const STRING_TABLE_FILE = "Language.dat";
char const * const STRING_TABLE_MAGIC = "OPENTS-STRINGS 1";

typedef std::unordered_map<unsigned int, std::string> StringMap;


// The table sits beside the executable, which is where the language library it replaces is
// looked for as well. A run from elsewhere still finds it through the working directory.
std::vector<std::filesystem::path> Table_Candidates(void)
{
	std::vector<std::filesystem::path> candidates;

	char executable[MAX_PATH];
	if (GetModuleFileName(NULL, executable, sizeof(executable)) != 0) {
		std::filesystem::path beside(executable);
		beside.replace_filename(STRING_TABLE_FILE);
		candidates.push_back(beside);
	}

	candidates.push_back(std::filesystem::path(STRING_TABLE_FILE));
	return(candidates);
}


bool Read_Whole_File(std::filesystem::path const & path, std::string & content)
{
	std::FILE * file = std::fopen(path.c_str(), "rb");
	if (file == NULL) {
		return(false);
	}

	content.clear();

	char buffer[8192];
	std::size_t got;
	while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
		content.append(buffer, got);
	}

	bool const ok = (std::ferror(file) == 0);
	std::fclose(file);
	return(ok);
}


// Reads one line and leaves the cursor past its terminator. A record's text is never read
// this way, since only its declared length says where it ends.
bool Next_Line(std::string const & content, std::size_t & cursor, std::string & line)
{
	if (cursor >= content.size()) {
		return(false);
	}

	std::size_t const end = content.find('\n', cursor);
	if (end == std::string::npos) {
		return(false);
	}

	line.assign(content, cursor, end - cursor);
	cursor = end + 1;
	return(true);
}


void Load_String_Table(StringMap & strings)
{
	std::string content;
	bool found = false;
	for (std::filesystem::path const & candidate : Table_Candidates()) {
		if (Read_Whole_File(candidate, content)) {
			found = true;
			break;
		}
	}

	if (!found) {
		return;
	}

	std::size_t cursor = 0;
	std::string line;

	if (!Next_Line(content, cursor, line) || line != STRING_TABLE_MAGIC) {
		return;
	}

	if (!Next_Line(content, cursor, line)) {
		return;
	}

	long const count = std::strtol(line.c_str(), NULL, 10);

	for (long record = 0; record < count; record++) {
		if (!Next_Line(content, cursor, line)) {
			break;
		}

		char * after = NULL;
		unsigned long const id = std::strtoul(line.c_str(), &after, 10);
		if (after == line.c_str()) {
			break;
		}

		long const length = std::strtol(after, NULL, 10);
		if (length < 0 || cursor + (std::size_t)length > content.size()) {
			break;
		}

		strings[(unsigned int)id] = content.substr(cursor, (std::size_t)length);
		cursor += (std::size_t)length + 1;
	}
}


// A global constructor in the engine asks for a string before this translation unit's own
// statics would have been built, so the table is built on the first request. It is never torn
// down either, because a string can just as easily be asked for from a static destructor.
StringMap const & Table(void)
{
	static StringMap * strings = NULL;
	if (strings == NULL) {
		strings = new StringMap();
		Load_String_Table(*strings);
	}

	return(*strings);
}


// A buffer too small to hold the whole string truncates it, as the Windows call does. The
// text is UTF-8, so the cut is pulled back off a continuation byte rather than left to
// split a code point.
std::size_t Whole_Code_Points(char const * text, std::size_t length)
{
	while (length > 0 && ((unsigned char)text[length] & 0xC0) == 0x80) {
		length--;
	}

	return(length);
}

}


extern "C" int LoadString(HINSTANCE instance, UINT id, LPSTR buffer, int max)
{
	(void)instance;

	if (buffer == NULL || max <= 0) {
		return(0);
	}

	buffer[0] = '\0';

	StringMap const & strings = Table();

	StringMap::const_iterator const entry = strings.find(id);
	if (entry == strings.end()) {
		return(0);
	}

	std::size_t length = entry->second.size();
	if (length > (std::size_t)(max - 1)) {
		length = Whole_Code_Points(entry->second.c_str(), (std::size_t)(max - 1));
	}

	std::memcpy(buffer, entry->second.c_str(), length);
	buffer[length] = '\0';
	return((int)length);
}
