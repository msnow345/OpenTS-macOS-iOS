#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef _WINDOWS_
#define _WINDOWS_
#endif

#define WINAPI
#define APIENTRY
#define CALLBACK
#define WINAPIV
#define __cdecl
#define PASCAL

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned int UINT;
typedef int INT;
typedef short SHORT;
typedef unsigned short USHORT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef wchar_t WCHAR;
typedef float FLOAT;
typedef void VOID;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef std::intptr_t INT_PTR;
typedef std::uintptr_t UINT_PTR;
typedef std::intptr_t LONG_PTR;
typedef std::uintptr_t ULONG_PTR;
typedef ULONG_PTR DWORD_PTR;
typedef std::size_t SIZE_T;

typedef void *LPVOID;
typedef const void *LPCVOID;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef wchar_t *LPWSTR;
typedef const wchar_t *LPCWSTR;
typedef char *LPTSTR;
typedef const char *LPCTSTR;
typedef BYTE *LPBYTE;
typedef WORD *LPWORD;
typedef DWORD *LPDWORD;
typedef INT *LPINT;
typedef LONG *LPLONG;
typedef BOOL *LPBOOL;

typedef void *HANDLE;
#define OPENTS_SHIM_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
OPENTS_SHIM_HANDLE(HWND);
OPENTS_SHIM_HANDLE(HINSTANCE);
OPENTS_SHIM_HANDLE(HDC);
OPENTS_SHIM_HANDLE(HBITMAP);
OPENTS_SHIM_HANDLE(HPALETTE);
OPENTS_SHIM_HANDLE(HBRUSH);
OPENTS_SHIM_HANDLE(HPEN);
OPENTS_SHIM_HANDLE(HFONT);
OPENTS_SHIM_HANDLE(HRGN);
OPENTS_SHIM_HANDLE(HGDIOBJ);
OPENTS_SHIM_HANDLE(HCURSOR);
OPENTS_SHIM_HANDLE(HICON);
OPENTS_SHIM_HANDLE(HMENU);
OPENTS_SHIM_HANDLE(HKEY);
OPENTS_SHIM_HANDLE(HMODULE);
OPENTS_SHIM_HANDLE(HMONITOR);
OPENTS_SHIM_HANDLE(HGLOBAL);
OPENTS_SHIM_HANDLE(HLOCAL);
OPENTS_SHIM_HANDLE(HACCEL);
typedef HINSTANCE HMODULE_ALIAS;

typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef LONG HRESULT;
typedef DWORD COLORREF;
typedef DWORD *LPCOLORREF;
typedef WORD ATOM;

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL (CALLBACK *WNDENUMPROC)(HWND, LPARAM);
typedef void (CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);
typedef int (CALLBACK *FARPROC)();
typedef int (CALLBACK *PROC)();

typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT, *PPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *LPSIZE;
typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *LPRECT, *PRECT;
typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG, *LPMSG;
typedef struct tagPAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT, *LPPAINTSTRUCT;
typedef union _LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; }; LONGLONG QuadPart; } LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER { struct { DWORD LowPart; DWORD HighPart; }; ULONGLONG QuadPart; } ULARGE_INTEGER;
typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME, *LPSYSTEMTIME;
typedef struct _SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; } SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
typedef struct _OVERLAPPED { ULONG_PTR Internal, InternalHigh; union { struct { DWORD Offset, OffsetHigh; }; void *Pointer; }; HANDLE hEvent; } OVERLAPPED, *LPOVERLAPPED;
typedef struct _RTL_CRITICAL_SECTION { void *opaque[8]; } CRITICAL_SECTION, *LPCRITICAL_SECTION;
typedef struct _GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID, IID, CLSID, UUID;
typedef const GUID &REFGUID;
typedef const GUID &REFIID;
typedef const GUID &REFCLSID;

#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | (((WORD)((BYTE)(b))) << 8)))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r)) | (((WORD)((BYTE)(g))) << 8) | (((DWORD)((BYTE)(b))) << 16)))
#define MAKEINTRESOURCE(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    CHAR cFileName[MAX_PATH]; CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, WIN32_FIND_DATA, *LPWIN32_FIND_DATAA, *LPWIN32_FIND_DATA;

typedef struct tagDRAWITEMSTRUCT {
    UINT CtlType, CtlID; UINT itemID, itemAction, itemState;
    HWND hwndItem; HDC hDC; RECT rcItem; ULONG_PTR itemData;
} DRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;

typedef struct tagMEASUREITEMSTRUCT { UINT CtlType, CtlID, itemID, itemWidth, itemHeight; ULONG_PTR itemData; } MEASUREITEMSTRUCT, *LPMEASUREITEMSTRUCT;

#pragma pack(push, 1)
typedef struct tagBITMAPFILEHEADER { WORD bfType; DWORD bfSize; WORD bfReserved1, bfReserved2; DWORD bfOffBits; } BITMAPFILEHEADER, *LPBITMAPFILEHEADER;
#pragma pack(pop)
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount;
    DWORD biCompression, biSizeImage; LONG biXPelsPerMeter, biYPelsPerMeter;
    DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagRGBQUAD { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
typedef struct tagBITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO, *LPBITMAPINFO;
typedef struct tagBITMAP { LONG bmType, bmWidth, bmHeight, bmWidthBytes; WORD bmPlanes, bmBitsPixel; LPVOID bmBits; } BITMAP;
typedef struct tagDIBSECTION { BITMAP dsBm; BITMAPINFOHEADER dsBmih; DWORD dsBitfields[3]; HANDLE dshSection; DWORD dsOffset; } DIBSECTION;
typedef struct _ICONINFO { BOOL fIcon; DWORD xHotspot, yHotspot; HBITMAP hbmMask, hbmColor; } ICONINFO;
typedef struct tagWNDCLASSA { UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName, lpszClassName; } WNDCLASSA, WNDCLASS, *LPWNDCLASS;
typedef struct tagMSGBOXPARAMSA { UINT cbSize; HWND hwndOwner; HINSTANCE hInstance; LPCSTR lpszText, lpszCaption; DWORD dwStyle; LPCSTR lpszIcon; DWORD_PTR dwContextHelpId; void *lpfnMsgBoxCallback; DWORD dwLanguageId; } MSGBOXPARAMSA, MSGBOXPARAMS;
typedef struct _devicemodeA { CHAR dmDeviceName[32]; WORD dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; DWORD dmFields; DWORD dmPelsWidth, dmPelsHeight, dmBitsPerPel, dmDisplayFrequency; } DEVMODEA, DEVMODE;
typedef struct _RTL_SRWLOCK { void *Ptr; } SRWLOCK;
OPENTS_SHIM_HANDLE(HRSRC);
OPENTS_SHIM_HANDLE(HIMAGELIST);
OPENTS_SHIM_HANDLE(HTREEITEM);
typedef struct _NMTREEVIEWA { void *opaque; } NMTREEVIEWA, *LPNMTREEVIEW;
typedef struct DLGTEMPLATE { DWORD style, dwExtendedStyle; WORD cdit; short x, y, cx, cy; } DLGTEMPLATE, *LPDLGTEMPLATE;
typedef const DLGTEMPLATE *LPCDLGTEMPLATE;
