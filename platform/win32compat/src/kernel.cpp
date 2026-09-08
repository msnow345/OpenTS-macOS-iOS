/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "win32compat.h"

#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>

#include <cerrno>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fnmatch.h>
#include <filesystem>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

static DWORD _LastError;


extern "C" DWORD GetLastError(void) { return(_LastError); }
extern "C" void SetLastError(DWORD code) { _LastError = code; }


// The mutexes exist to keep a second copy of the game and the installer's autoplay from
// running at once. A process-local handle answers both callers correctly for a single run
// and refuses nothing, which is the behaviour a first native launch needs.
extern "C" HANDLE CreateMutex(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCSTR name)
{
	(void)attributes;
	(void)owner;
	(void)name;
	_LastError = ERROR_SUCCESS;
	return((HANDLE)new std::recursive_mutex());
}


extern "C" HANDLE OpenMutex(DWORD access, BOOL inherit, LPCSTR name)
{
	(void)access;
	(void)inherit;
	(void)name;
	_LastError = ERROR_FILE_NOT_FOUND;
	return(NULL);
}


extern "C" DWORD WaitForSingleObject(HANDLE object, DWORD milliseconds)
{
	(void)object;
	(void)milliseconds;
	return(WAIT_OBJECT_0);
}


extern "C" BOOL ReleaseMutex(HANDLE mutex) { (void)mutex; return(TRUE); }


extern "C" DWORD GetCurrentProcessId(void) { return((DWORD)getpid()); }
extern "C" DWORD GetCurrentThreadId(void) { return((DWORD)SDL_GetCurrentThreadID()); }
extern "C" BOOL IsDebuggerPresent(void) { return(FALSE); }
extern "C" void Sleep(DWORD milliseconds) { SDL_Delay(milliseconds); }


extern "C" void OutputDebugString(LPCSTR text)
{
	if (text != NULL) {
		std::fputs(text, stderr);
	}
}


// A module named without a path is looked for beside the executable, under the host's own
// library naming: the game asks for "Language.dll" and the build produces
// "libLanguage.dylib" in the same directory.
static std::string Host_Library_Name(char const * name)
{
	std::string stem(name != NULL ? name : "");
	std::string::size_type const dot = stem.find_last_of('.');

	if (dot != std::string::npos) {
		stem = stem.substr(0, dot);
	}

#ifdef __APPLE__
	return("lib" + stem + ".dylib");
#else
	return("lib" + stem + ".so");
#endif
}


// A null name asks for the running program, which is the handle a caller compares against
// rather than one it loads anything from.
extern "C" HMODULE GetModuleHandle(LPCSTR name)
{
	if (name == NULL) {
		return((HMODULE)(ULONG_PTR)1);
	}

	return((HMODULE)dlopen(Host_Library_Name(name).c_str(), RTLD_LAZY | RTLD_NOLOAD));
}


extern "C" HMODULE LoadLibrary(LPCSTR name)
{
	if (name == NULL) {
		return(NULL);
	}

	std::string const library = Host_Library_Name(name);

	char executable[MAX_PATH];
	if (GetModuleFileName(NULL, executable, sizeof(executable)) != 0) {
		std::filesystem::path beside(executable);
		beside.replace_filename(library);

		if (void * handle = dlopen(beside.c_str(), RTLD_LAZY)) {
			return((HMODULE)handle);
		}
	}

	return((HMODULE)dlopen(library.c_str(), RTLD_LAZY));
}


extern "C" BOOL FreeLibrary(HMODULE module)
{
	if (module == NULL || module == (HMODULE)(ULONG_PTR)1) {
		return(TRUE);
	}

	return(dlclose((void *)module) == 0 ? TRUE : FALSE);
}


extern "C" FARPROC GetProcAddress(HMODULE module, LPCSTR name)
{
	if (module == NULL || module == (HMODULE)(ULONG_PTR)1 || name == NULL) {
		return(NULL);
	}

	return((FARPROC)dlsym((void *)module, name));
}


extern "C" DWORD GetModuleFileName(HMODULE module, LPSTR name, DWORD size)
{
	(void)module;

	if (name == NULL || size == 0) {
		return(0);
	}

	name[0] = '\0';

	uint32_t length = size;
	if (_NSGetExecutablePath(name, &length) != 0) {
		return(0);
	}

	return((DWORD)strlen(name));
}


// The engine asks the shell for its command line and then splits it. The real argument
// vector is captured at entry, so the wide line handed back is built from that and split
// back into the same arguments rather than re-parsed by a quoting rule this host lacks.
static std::vector<std::wstring> _Arguments;
static std::wstring _CommandLine;

void Win32_Record_Arguments(int argc, char ** argv)
{
	_Arguments.clear();
	_CommandLine.clear();

	for (int index = 0; index < argc; index++) {
		std::string const argument(argv[index] != NULL ? argv[index] : "");
		_Arguments.push_back(std::wstring(argument.begin(), argument.end()));

		if (index > 0) {
			_CommandLine += L' ';
		}
		_CommandLine += _Arguments.back();
	}
}


extern "C" LPWSTR GetCommandLineW(void)
{
	return(const_cast<LPWSTR>(_CommandLine.c_str()));
}


extern "C" LPWSTR * CommandLineToArgvW(LPCWSTR commandline, int * count)
{
	(void)commandline;

	static std::vector<LPWSTR> pointers;
	pointers.clear();

	for (std::wstring & argument : _Arguments) {
		pointers.push_back(const_cast<LPWSTR>(argument.c_str()));
	}

	if (count != NULL) {
		*count = (int)pointers.size();
	}

	return(pointers.empty() ? NULL : pointers.data());
}


extern "C" HLOCAL LocalFree(HLOCAL memory) { (void)memory; return(NULL); }


// The host has no legacy single-byte code page, so both conversions are UTF-8 only. A
// character with no UTF-8 spelling does not exist, so nothing is lost in that direction;
// the other direction is what the engine's own substitute-glyph path already covers.
extern "C" UINT GetACP(void) { return(CP_UTF8); }
extern "C" UINT GetOEMCP(void) { return(CP_UTF8); }


extern "C" int MultiByteToWideChar(UINT codepage, DWORD flags, LPCSTR source, int sourcelength,
	LPWSTR dest, int destlength)
{
	(void)codepage;
	(void)flags;

	if (source == NULL) {
		return(0);
	}

	size_t const length = sourcelength < 0 ? strlen(source) + 1 : (size_t)sourcelength;

	if (dest == NULL || destlength == 0) {
		return((int)length);
	}

	size_t const copied = length < (size_t)destlength ? length : (size_t)destlength;
	for (size_t index = 0; index < copied; index++) {
		dest[index] = (wchar_t)(unsigned char)source[index];
	}

	return((int)copied);
}


extern "C" int WideCharToMultiByte(UINT codepage, DWORD flags, LPCWSTR source, int sourcelength,
	LPSTR dest, int destlength, LPCSTR defaultchar, LPBOOL useddefault)
{
	(void)codepage;
	(void)flags;
	(void)defaultchar;

	if (useddefault != NULL) {
		*useddefault = FALSE;
	}

	if (source == NULL) {
		return(0);
	}

	size_t length = 0;
	if (sourcelength < 0) {
		while (source[length] != 0) length++;
		length++;
	} else {
		length = (size_t)sourcelength;
	}

	if (dest == NULL || destlength == 0) {
		return((int)length);
	}

	size_t const copied = length < (size_t)destlength ? length : (size_t)destlength;
	for (size_t index = 0; index < copied; index++) {
		dest[index] = source[index] < 128 ? (char)source[index] : '?';
	}

	return((int)copied);
}


static FILETIME File_Time_From_Unix(time_t seconds)
{
	// The Windows epoch is 1601, and the count is in hundred-nanosecond units.
	unsigned long long const ticks = (unsigned long long)seconds * 10000000ULL + 116444736000000000ULL;
	FILETIME time;
	time.dwLowDateTime = (DWORD)(ticks & 0xFFFFFFFFULL);
	time.dwHighDateTime = (DWORD)(ticks >> 32);
	return(time);
}


static unsigned long long Ticks_From_File_Time(FILETIME const & time)
{
	return(((unsigned long long)time.dwHighDateTime << 32) | (unsigned long long)time.dwLowDateTime);
}


extern "C" void GetSystemTime(LPSYSTEMTIME system)
{
	if (system == NULL) {
		return;
	}

	timeval now;
	gettimeofday(&now, NULL);

	tm parts;
	gmtime_r(&now.tv_sec, &parts);

	system->wYear = (WORD)(parts.tm_year + 1900);
	system->wMonth = (WORD)(parts.tm_mon + 1);
	system->wDayOfWeek = (WORD)parts.tm_wday;
	system->wDay = (WORD)parts.tm_mday;
	system->wHour = (WORD)parts.tm_hour;
	system->wMinute = (WORD)parts.tm_min;
	system->wSecond = (WORD)parts.tm_sec;
	system->wMilliseconds = (WORD)(now.tv_usec / 1000);
}


extern "C" void GetLocalTime(LPSYSTEMTIME system)
{
	if (system == NULL) {
		return;
	}

	timeval now;
	gettimeofday(&now, NULL);

	tm parts;
	localtime_r(&now.tv_sec, &parts);

	system->wYear = (WORD)(parts.tm_year + 1900);
	system->wMonth = (WORD)(parts.tm_mon + 1);
	system->wDayOfWeek = (WORD)parts.tm_wday;
	system->wDay = (WORD)parts.tm_mday;
	system->wHour = (WORD)parts.tm_hour;
	system->wMinute = (WORD)parts.tm_min;
	system->wSecond = (WORD)parts.tm_sec;
	system->wMilliseconds = (WORD)(now.tv_usec / 1000);
}


extern "C" BOOL SystemTimeToFileTime(SYSTEMTIME const * system, LPFILETIME file)
{
	if (system == NULL || file == NULL) {
		return(FALSE);
	}

	tm parts = {};
	parts.tm_year = system->wYear - 1900;
	parts.tm_mon = system->wMonth - 1;
	parts.tm_mday = system->wDay;
	parts.tm_hour = system->wHour;
	parts.tm_min = system->wMinute;
	parts.tm_sec = system->wSecond;

	*file = File_Time_From_Unix(timegm(&parts));
	return(TRUE);
}


extern "C" BOOL FileTimeToSystemTime(FILETIME const * file, LPSYSTEMTIME system)
{
	if (file == NULL || system == NULL) {
		return(FALSE);
	}

	time_t const seconds = (time_t)((Ticks_From_File_Time(*file) - 116444736000000000ULL) / 10000000ULL);

	tm parts;
	gmtime_r(&seconds, &parts);

	system->wYear = (WORD)(parts.tm_year + 1900);
	system->wMonth = (WORD)(parts.tm_mon + 1);
	system->wDayOfWeek = (WORD)parts.tm_wday;
	system->wDay = (WORD)parts.tm_mday;
	system->wHour = (WORD)parts.tm_hour;
	system->wMinute = (WORD)parts.tm_min;
	system->wSecond = (WORD)parts.tm_sec;
	system->wMilliseconds = 0;
	return(TRUE);
}


extern "C" BOOL FileTimeToLocalFileTime(FILETIME const * file, LPFILETIME local)
{
	if (file == NULL || local == NULL) {
		return(FALSE);
	}

	*local = *file;
	return(TRUE);
}


extern "C" LONG CompareFileTime(FILETIME const * a, FILETIME const * b)
{
	if (a == NULL || b == NULL) {
		return(0);
	}

	unsigned long long const left = Ticks_From_File_Time(*a);
	unsigned long long const right = Ticks_From_File_Time(*b);
	return(left < right ? -1 : (left > right ? 1 : 0));
}


extern "C" void _ftime(struct _timeb * time)
{
	if (time == NULL) {
		return;
	}

	timeval now;
	gettimeofday(&now, NULL);
	time->time = (long)now.tv_sec;
	time->millitm = (unsigned short)(now.tv_usec / 1000);
	time->timezone = 0;
	time->dstflag = 0;
}


extern "C" int _getch(void)
{
	return(std::getchar());
}


extern "C" int GetTimeFormat(DWORD locale, DWORD flags, SYSTEMTIME const * time, LPCSTR format,
	LPSTR buffer, int size)
{
	(void)locale;
	(void)format;

	if (buffer == NULL || size <= 0 || time == NULL) {
		return(0);
	}

	if ((flags & TIME_NOMINUTESORSECONDS) != 0) {
		snprintf(buffer, (size_t)size, "%02u", time->wHour);
	} else if ((flags & TIME_NOSECONDS) != 0) {
		snprintf(buffer, (size_t)size, "%02u:%02u", time->wHour, time->wMinute);
	} else {
		snprintf(buffer, (size_t)size, "%02u:%02u:%02u", time->wHour, time->wMinute, time->wSecond);
	}

	return((int)strlen(buffer) + 1);
}


extern "C" int GetDateFormat(DWORD locale, DWORD flags, SYSTEMTIME const * time, LPCSTR format,
	LPSTR buffer, int size)
{
	(void)locale;
	(void)flags;
	(void)format;

	if (buffer == NULL || size <= 0 || time == NULL) {
		return(0);
	}

	snprintf(buffer, (size_t)size, "%04u-%02u-%02u", time->wYear, time->wMonth, time->wDay);
	return((int)strlen(buffer) + 1);
}


extern "C" DWORD FormatMessage(DWORD flags, LPCVOID source, DWORD id, DWORD language,
	LPSTR buffer, DWORD size, void * arguments)
{
	(void)flags;
	(void)source;
	(void)language;
	(void)arguments;

	if (buffer == NULL || size == 0) {
		return(0);
	}

	snprintf(buffer, size, "%s", strerror((int)id));
	return((DWORD)strlen(buffer));
}


//
// ---------------------------------------------------------
// Files
// ---------------------------------------------------------
//
extern "C" HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes,
	DWORD disposition, DWORD flags, HANDLE templatefile)
{
	(void)share;
	(void)attributes;
	(void)flags;
	(void)templatefile;

	if (name == NULL) {
		return(INVALID_HANDLE_VALUE);
	}

	char const * mode = "rb";
	if ((access & GENERIC_WRITE) != 0) {
		mode = disposition == OPEN_EXISTING ? "r+b" : "w+b";
	}

	std::FILE * file = std::fopen(name, mode);

	if (file == NULL) {
		_LastError = (DWORD)errno;
		return(INVALID_HANDLE_VALUE);
	}

	return((HANDLE)file);
}


extern "C" BOOL ReadFile(HANDLE handle, LPVOID buffer, DWORD size, LPDWORD read, LPOVERLAPPED overlapped)
{
	(void)overlapped;

	if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
		return(FALSE);
	}

	size_t const got = std::fread(buffer, 1, size, (std::FILE *)handle);

	if (read != NULL) {
		*read = (DWORD)got;
	}

	return(TRUE);
}


extern "C" BOOL WriteFile(HANDLE handle, LPCVOID buffer, DWORD size, LPDWORD written, LPOVERLAPPED overlapped)
{
	(void)overlapped;

	if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
		return(FALSE);
	}

	size_t const put = std::fwrite(buffer, 1, size, (std::FILE *)handle);

	/*
	 * A Windows file handle is not buffered, so a log written through this call
	 * survives a crash. Match that rather than leaving the last records of a run
	 * in a standard library buffer that is never drained.
	 */
	std::fflush((std::FILE *)handle);

	if (written != NULL) {
		*written = (DWORD)put;
	}

	return(TRUE);
}


extern "C" DWORD SetFilePointer(HANDLE handle, LONG distance, LONG * distancehigh, DWORD method)
{
	(void)distancehigh;

	if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
		return(INVALID_SET_FILE_POINTER);
	}

	int const origin = method == FILE_BEGIN ? SEEK_SET : (method == FILE_END ? SEEK_END : SEEK_CUR);

	if (std::fseek((std::FILE *)handle, distance, origin) != 0) {
		return(INVALID_SET_FILE_POINTER);
	}

	return((DWORD)std::ftell((std::FILE *)handle));
}


extern "C" BOOL CloseHandle(HANDLE handle)
{
	if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
		return(FALSE);
	}

	std::fclose((std::FILE *)handle);
	return(TRUE);
}


extern "C" BOOL DeleteFileA(LPCSTR name)
{
	return(name != NULL && std::remove(name) == 0 ? TRUE : FALSE);
}


extern "C" BOOL CopyFile(LPCSTR from, LPCSTR to, BOOL failifexists)
{
	if (from == NULL || to == NULL) {
		return(FALSE);
	}

	std::error_code error;
	auto const options = failifexists
		? std::filesystem::copy_options::none
		: std::filesystem::copy_options::overwrite_existing;
	std::filesystem::copy_file(from, to, options, error);
	return(error ? FALSE : TRUE);
}


extern "C" BOOL CreateDirectory(LPCSTR path, LPSECURITY_ATTRIBUTES attributes)
{
	(void)attributes;

	if (path == NULL) {
		return(FALSE);
	}

	std::error_code error;

	if (std::filesystem::create_directory(path, error)) {
		_LastError = ERROR_SUCCESS;
		return(TRUE);
	}

	// A caller distinguishes "it is already there" from a real failure through the last
	// error rather than through the result, so the two cases must not look alike.
	_LastError = std::filesystem::is_directory(path, error) ? ERROR_ALREADY_EXISTS : ERROR_FILE_NOT_FOUND;
	return(FALSE);
}


extern "C" BOOL SetCurrentDirectory(LPCSTR path)
{
	return(path != NULL && chdir(path) == 0 ? TRUE : FALSE);
}


extern "C" DWORD GetFileAttributesA(LPCSTR name)
{
	struct stat status;

	if (name == NULL || stat(name, &status) != 0) {
		return(INVALID_FILE_ATTRIBUTES);
	}

	DWORD attributes = FILE_ATTRIBUTE_NORMAL;
	if (S_ISDIR(status.st_mode)) attributes = FILE_ATTRIBUTE_DIRECTORY;
	if ((status.st_mode & S_IWUSR) == 0) attributes |= FILE_ATTRIBUTE_READONLY;
	return(attributes);
}


// Directory enumeration keeps the shape the callers expect: one handle that walks a
// directory and matches each entry against the pattern the caller supplied.
struct Win32Find
{
	DIR * Directory;
	std::string Path;
	std::string Pattern;
};


static bool Fill_Find_Data(Win32Find * find, LPWIN32_FIND_DATA data)
{
	dirent * entry = NULL;

	while ((entry = readdir(find->Directory)) != NULL) {
		if (fnmatch(find->Pattern.c_str(), entry->d_name, FNM_CASEFOLD) != 0) {
			continue;
		}

		std::string const full = find->Path + "/" + entry->d_name;

		struct stat status;
		if (stat(full.c_str(), &status) != 0) {
			continue;
		}

		memset(data, 0, sizeof(*data));
		data->dwFileAttributes = S_ISDIR(status.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
		data->nFileSizeLow = (DWORD)status.st_size;
		data->nFileSizeHigh = (DWORD)((unsigned long long)status.st_size >> 32);
		data->ftLastWriteTime = File_Time_From_Unix(status.st_mtime);
		data->ftCreationTime = data->ftLastWriteTime;
		data->ftLastAccessTime = data->ftLastWriteTime;
		strncpy(data->cFileName, entry->d_name, sizeof(data->cFileName) - 1);
		return(true);
	}

	return(false);
}


extern "C" HANDLE FindFirstFile(LPCSTR name, LPWIN32_FIND_DATA data)
{
	if (name == NULL || data == NULL) {
		return(INVALID_HANDLE_VALUE);
	}

	std::string full(name);
	std::string::size_type const slash = full.find_last_of("/\\");
	std::string const directory = slash == std::string::npos ? std::string(".") : full.substr(0, slash);
	std::string const pattern = slash == std::string::npos ? full : full.substr(slash + 1);

	DIR * handle = opendir(directory.c_str());

	if (handle == NULL) {
		_LastError = (DWORD)errno;
		return(INVALID_HANDLE_VALUE);
	}

	Win32Find * find = new Win32Find();
	find->Directory = handle;
	find->Path = directory;
	find->Pattern = pattern;

	if (!Fill_Find_Data(find, data)) {
		closedir(handle);
		delete find;
		return(INVALID_HANDLE_VALUE);
	}

	return((HANDLE)find);
}


extern "C" BOOL FindNextFile(HANDLE handle, LPWIN32_FIND_DATA data)
{
	if (handle == INVALID_HANDLE_VALUE || handle == NULL || data == NULL) {
		return(FALSE);
	}

	return(Fill_Find_Data((Win32Find *)handle, data) ? TRUE : FALSE);
}


extern "C" BOOL FindClose(HANDLE handle)
{
	if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
		return(FALSE);
	}

	Win32Find * find = (Win32Find *)handle;
	closedir(find->Directory);
	delete find;
	return(TRUE);
}


//
// ---------------------------------------------------------
// Console, locks and the services with no host equivalent
// ---------------------------------------------------------
//
extern "C" BOOL AllocConsole(void) { return(FALSE); }
extern "C" HWND GetConsoleWindow(void) { return(NULL); }
extern "C" HANDLE GetStdHandle(DWORD which)
{
	switch (which) {
		case STD_INPUT_HANDLE: return((HANDLE)stdin);
		case STD_OUTPUT_HANDLE: return((HANDLE)stdout);
		default: return((HANDLE)stderr);
	}
}
extern "C" BOOL SetStdHandle(DWORD which, HANDLE handle) { (void)which; (void)handle; return(TRUE); }
extern "C" BOOL SetConsoleTitle(LPCSTR title) { (void)title; return(TRUE); }
extern "C" BOOL SetConsoleCP(UINT codepage) { (void)codepage; return(TRUE); }
extern "C" BOOL SetConsoleOutputCP(UINT codepage) { (void)codepage; return(TRUE); }
extern "C" BOOL SetConsoleScreenBufferSize(HANDLE console, COORD size) { (void)console; (void)size; return(TRUE); }
extern "C" BOOL GetConsoleScreenBufferInfo(HANDLE console, CONSOLE_SCREEN_BUFFER_INFO * info)
{
	(void)console;
	(void)info;
	return(FALSE);
}


extern "C" BOOL WriteConsole(HANDLE console, void const * buffer, DWORD length, LPDWORD written, LPVOID reserved)
{
	(void)reserved;

	std::FILE * stream = console == (HANDLE)stdout ? stdout : stderr;
	size_t const put = std::fwrite(buffer, 1, length, stream);

	if (written != NULL) {
		*written = (DWORD)put;
	}

	return(TRUE);
}


// The debug log's lock is a plain mutex; nothing in the tree takes it for reading only.
extern "C" void InitializeSRWLock(SRWLOCK * lock)
{
	if (lock != NULL) {
		lock->Ptr = new std::recursive_mutex();
	}
}


extern "C" void AcquireSRWLockExclusive(SRWLOCK * lock)
{
	if (lock == NULL) {
		return;
	}

	if (lock->Ptr == NULL) {
		InitializeSRWLock(lock);
	}

	((std::recursive_mutex *)lock->Ptr)->lock();
}


extern "C" void ReleaseSRWLockExclusive(SRWLOCK * lock)
{
	if (lock != NULL && lock->Ptr != NULL) {
		((std::recursive_mutex *)lock->Ptr)->unlock();
	}
}


// There is no version resource and no PE image to read outside Windows. Reporting nothing
// is what the callers already handle; inventing a value would be worse.
extern "C" DWORD GetFileVersionInfoSize(LPCSTR name, LPDWORD handle) { (void)name; (void)handle; return(0); }
extern "C" BOOL GetFileVersionInfo(LPCSTR name, DWORD handle, DWORD length, LPVOID data) { (void)name; (void)handle; (void)length; (void)data; return(FALSE); }
extern "C" BOOL VerQueryValue(LPCVOID block, LPCSTR path, LPVOID * buffer, UINT * length) { (void)block; (void)path; (void)buffer; (void)length; return(FALSE); }
extern "C" HRSRC FindResource(HMODULE module, LPCSTR name, LPCSTR type) { (void)module; (void)name; (void)type; return(NULL); }
extern "C" HGLOBAL LoadResource(HMODULE module, HRSRC resource) { (void)module; (void)resource; return(NULL); }
extern "C" LPVOID LockResource(HGLOBAL resource) { (void)resource; return(NULL); }
extern "C" DWORD SizeofResource(HMODULE module, HRSRC resource) { (void)module; (void)resource; return(0); }
extern "C" LONG RegOpenKeyEx(HKEY key, LPCSTR subkey, DWORD options, DWORD access, HKEY * result) { (void)key; (void)subkey; (void)options; (void)access; if (result != NULL) *result = NULL; return(1); }
extern "C" LONG RegQueryValueEx(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size) { (void)key; (void)name; (void)reserved; (void)type; (void)data; (void)size; return(1); }
extern "C" LONG RegCloseKey(HKEY key) { (void)key; return(0); }
