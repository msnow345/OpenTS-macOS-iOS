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
OPENTS_SHIM_HANDLE(HCURSOR);
OPENTS_SHIM_HANDLE(HICON);
OPENTS_SHIM_HANDLE(HMENU);
OPENTS_SHIM_HANDLE(HKEY);
OPENTS_SHIM_HANDLE(HMONITOR);
OPENTS_SHIM_HANDLE(HACCEL);
typedef void *HGDIOBJ;
typedef HINSTANCE HMODULE;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;

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
typedef struct _NMHDR_SHIM { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR_SHIM;
typedef struct _TVITEM_SHIM { UINT mask; HTREEITEM hItem; UINT state, stateMask; LPSTR pszText; int cchTextMax, iImage, iSelectedImage, cChildren; LPARAM lParam; } TVITEM_SHIM;
typedef struct _NMTREEVIEWA { NMHDR_SHIM hdr; UINT action; TVITEM_SHIM itemOld, itemNew; POINT ptDrag; } NMTREEVIEWA, NMTREEVIEW, *LPNMTREEVIEW;
typedef struct DLGTEMPLATE { DWORD style, dwExtendedStyle; WORD cdit; short x, y, cx, cy; } DLGTEMPLATE, *LPDLGTEMPLATE;
typedef const DLGTEMPLATE *LPCDLGTEMPLATE;

// SAL-style annotations the tree spells out on parameters.
#define IN
#define OUT
#define OPTIONAL
#ifndef CONST
#define CONST const
#endif

#define E_POINTER ((HRESULT)0x80004003L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define CLSCTX_ALL 23
#define CLSCTX_INPROC_SERVER 1
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7
#define MB_OK 0x0
#define MB_OKCANCEL 0x1
#define MB_YESNO 0x4
#define MB_ICONSTOP 0x10
#define MB_ICONERROR 0x10
#define MB_ICONQUESTION 0x20
#define MB_ICONEXCLAMATION 0x30
#define MB_ICONINFORMATION 0x40
#define MB_SETFOREGROUND 0x10000
#define MB_TASKMODAL 0x2000
#define MB_SYSTEMMODAL 0x1000
#define MB_APPLMODAL 0x0

// ---------------------------------------------------------------------------
// Window messages
// ---------------------------------------------------------------------------
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_ENABLE 0x000A
#define WM_SETREDRAW 0x000B
#define WM_SETTEXT 0x000C
#define WM_GETTEXT 0x000D
#define WM_GETTEXTLENGTH 0x000E
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SHOWWINDOW 0x0018
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_MOUSEACTIVATE 0x0021
#define WM_GETMINMAXINFO 0x0024
#define WM_SETFONT 0x0030
#define WM_GETFONT 0x0031
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_CONTEXTMENU 0x007B
#define WM_DISPLAYCHANGE 0x007E
#define WM_NCDESTROY 0x0082
#define WM_NCHITTEST 0x0084
#define WM_NCPAINT 0x0085
#define WM_GETDLGCODE 0x0087
#define WM_NCMOUSEMOVE 0x00A0
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_DEADCHAR 0x0103
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_SYSDEADCHAR 0x0107
#define WM_KEYLAST 0x0109
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_HSCROLL 0x0114
#define WM_VSCROLL 0x0115
#define WM_CTLCOLORMSGBOX 0x0132
#define WM_CTLCOLOREDIT 0x0133
#define WM_CTLCOLORLISTBOX 0x0134
#define WM_CTLCOLORBTN 0x0135
#define WM_CTLCOLORDLG 0x0136
#define WM_CTLCOLORSCROLLBAR 0x0137
#define WM_CTLCOLORSTATIC 0x0138
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_XBUTTONDBLCLK 0x020D
#define WM_MOUSELAST 0x020E
#define WM_MOVING 0x0216
#define WM_CAPTURECHANGED 0x0215
#define WM_DRAWITEM 0x002B
#define WM_MEASUREITEM 0x002C
#define WM_DELETEITEM 0x002D
#define WM_COMPAREITEM 0x0039
#define WM_HELP 0x0053
#define WM_NOTIFY 0x004E
#define WM_USER 0x0400
#define WM_APP 0x8000

#define HTTRANSPARENT (-1)
#define HTNOWHERE 0
#define HTCLIENT 1
#define HTCAPTION 2

#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2

#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010

#define SC_SIZE 0xF000
#define SC_CLOSE 0xF060
#define SC_SCREENSAVE 0xF140
#define SC_MONITORPOWER 0xF170
#define MF_BYCOMMAND 0x00000000
#define MF_BYPOSITION 0x00000400
#define MF_GRAYED 0x00000001

// ---------------------------------------------------------------------------
// Window and class styles
// ---------------------------------------------------------------------------
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#define CS_OWNDC 0x0020

#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_DISABLED 0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_VSCROLL 0x00200000L
#define WS_HSCROLL 0x00100000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_GROUP 0x00020000L
#define WS_TABSTOP 0x00010000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX)
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_TOOLWINDOW 0x00000080L

#define BS_PUSHBUTTON 0x00000000L
#define BS_CHECKBOX 0x00000002L
#define BS_AUTOCHECKBOX 0x00000003L
#define BS_RADIOBUTTON 0x00000004L
#define BS_GROUPBOX 0x00000007L
#define BS_OWNERDRAW 0x0000000BL
#define ES_MULTILINE 0x0004L
#define ES_PASSWORD 0x0020L
#define SS_CENTER 0x00000001L
#define SS_RIGHT 0x00000002L
#define LBS_NOTIFY 0x0001L
#define LBS_NOSEL 0x4000L
#define LBS_MULTIPLESEL 0x0008L

#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWL_ID (-12)
#define GWL_USERDATA (-21)
#define GWL_WNDPROC (-4)
#define GWL_HINSTANCE (-6)
#define GWL_HWNDPARENT (-8)
#define GWLP_WNDPROC (-4)
#define GWLP_HINSTANCE (-6)
#define GWLP_HWNDPARENT (-8)
#define GWLP_USERDATA (-21)
#define GWLP_ID (-12)
#define DWLP_MSGRESULT 0
#define DWLP_DLGPROC (DWLP_MSGRESULT + sizeof(LRESULT))
#define DWLP_USER (DWLP_DLGPROC + sizeof(DLGPROC))

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9

#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010
#define SWP_SHOWWINDOW 0x0040
#define SWP_NOOWNERZORDER 0x0200

#define GW_HWNDFIRST 0
#define GW_HWNDLAST 1
#define GW_HWNDNEXT 2
#define GW_HWNDPREV 3
#define GW_OWNER 4
#define GW_CHILD 5

#define HWND_DESKTOP ((HWND)0)
#define HWND_TOP ((HWND)0)
#define HWND_TOPMOST ((HWND)-1)

#define RDW_INVALIDATE 0x0001
#define RDW_INTERNALPAINT 0x0002
#define RDW_ERASE 0x0004
#define RDW_UPDATENOW 0x0100
#define RDW_FRAME 0x0400
#define RDW_ALLCHILDREN 0x0080

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define PM_NOYIELD 0x0002

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXBORDER 5
#define SM_CYBORDER 6
#define SM_CXFULLSCREEN 16
#define SM_CYFULLSCREEN 17
#define SM_SWAPBUTTON 23
#define SM_CXDRAG 68
#define SM_CYDRAG 69

#define MOD_ALT 0x0001
#define MOD_CONTROL 0x0002
#define MOD_SHIFT 0x0004

#define IDC_ARROW ((LPCSTR)(ULONG_PTR)32512)
#define IDC_NO ((LPCSTR)(ULONG_PTR)32648)
#define IDI_APPLICATION ((LPCSTR)(ULONG_PTR)32512)
#define RT_DIALOG ((LPCSTR)(ULONG_PTR)5)
#define MB_ICONWARNING 0x30

#define HELP_CONTEXTMENU 0x000a
#define HELP_CONTEXTPOPUP 0x0026

#define MONITOR_DEFAULTTONULL 0x0
#define MONITOR_DEFAULTTOPRIMARY 0x1
#define MONITOR_DEFAULTTONEAREST 0x2

// Button, edit, list box, combo box and scroll bar messages
#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define BM_GETSTATE 0x00F2
#define BM_SETSTATE 0x00F3
#define BST_UNCHECKED 0x0000
#define BST_CHECKED 0x0001
#define BN_CLICKED 0
#define BN_DBLCLK 5
#define ODT_MENU 1
#define ODT_LISTBOX 2
#define ODT_COMBOBOX 3
#define ODT_BUTTON 4
#define ODT_STATIC 5

#define EM_GETSEL 0x00B0
#define EM_SETSEL 0x00B1
#define EM_POSFROMCHAR 0x00D6
#define EM_SETLIMITTEXT 0x00C5
#define EN_SETFOCUS 0x0100
#define EN_KILLFOCUS 0x0200
#define EN_CHANGE 0x0300
#define EN_MAXTEXT 0x0501

#define LB_ADDSTRING 0x0180
#define LB_INSERTSTRING 0x0181
#define LB_DELETESTRING 0x0182
#define LB_RESETCONTENT 0x0184
#define LB_SETSEL 0x0185
#define LB_SETCURSEL 0x0186
#define LB_GETSEL 0x0187
#define LB_GETCURSEL 0x0188
#define LB_GETTEXT 0x0189
#define LB_GETTEXTLEN 0x018A
#define LB_GETCOUNT 0x018B
#define LB_SELECTSTRING 0x018C
#define LB_FINDSTRING 0x018F
#define LB_SELITEMRANGE 0x019B
#define LB_GETSELCOUNT 0x0190
#define LB_GETSELITEMS 0x0191
#define LB_SETITEMDATA 0x019A
#define LB_GETITEMDATA 0x0199
#define LB_GETITEMRECT 0x0198
#define LB_SETTOPINDEX 0x0197
#define LB_GETTOPINDEX 0x018E
#define LB_FINDSTRINGEXACT 0x01A2
#define LB_SETITEMHEIGHT 0x01A0
#define LB_GETITEMHEIGHT 0x01A1
#define LB_ERR (-1)
#define LBN_SELCHANGE 1
#define LBN_DBLCLK 2

#define CB_GETEDITSEL 0x0140
#define CB_ADDSTRING 0x0143
#define CB_DELETESTRING 0x0144
#define CB_GETCOUNT 0x0146
#define CB_GETCURSEL 0x0147
#define CB_GETLBTEXT 0x0148
#define CB_INSERTSTRING 0x014A
#define CB_RESETCONTENT 0x014B
#define CB_FINDSTRING 0x014C
#define CB_SETCURSEL 0x014E
#define CB_SHOWDROPDOWN 0x014F
#define CB_GETITEMDATA 0x0150
#define CB_SETITEMDATA 0x0151
#define CB_GETDROPPEDCONTROLRECT 0x0152
#define CB_SETITEMHEIGHT 0x0153
#define CB_GETITEMHEIGHT 0x0154
#define CB_GETDROPPEDSTATE 0x0157
#define CB_GETTOPINDEX 0x015b
#define CB_SETTOPINDEX 0x015c
#define CB_ERR (-1)
#define CBN_SELCHANGE 1

#define SBM_SETPOS 0x00E0
#define SBM_GETPOS 0x00E1
#define SBM_SETRANGE 0x00E2
#define SBM_SETSCROLLINFO 0x00E9
#define SB_LINEUP 0
#define SB_LINEDOWN 1
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK 5
#define SB_ENDSCROLL 8
#define SIF_RANGE 0x0001
#define SIF_PAGE 0x0002
#define SIF_POS 0x0004
#define SIF_ALL 0x0017

typedef struct tagSCROLLINFO { UINT cbSize, fMask; int nMin, nMax; UINT nPage; int nPos, nTrackPos; } SCROLLINFO, *LPSCROLLINFO;
typedef struct tagPOINTS { SHORT x, y; } POINTS;
typedef struct tagWINDOWPOS { HWND hwnd, hwndInsertAfter; int x, y, cx, cy; UINT flags; } WINDOWPOS, *LPWINDOWPOS;
typedef struct tagMONITORINFO { DWORD cbSize; RECT rcMonitor, rcWork; DWORD dwFlags; } MONITORINFO, *LPMONITORINFO;
typedef struct tagHELPINFO { UINT cbSize; int iContextType, iCtrlId; HANDLE hItemHandle; DWORD_PTR dwContextId; POINT MousePos; } HELPINFO, *LPHELPINFO;

#define MAKEPOINTS(l) (*((POINTS *)&(l)))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define IS_SURROGATE_PAIR(hi, lo) ((hi) >= 0xd800 && (hi) <= 0xdbff && (lo) >= 0xdc00 && (lo) <= 0xdfff)
#ifndef TEXT
#define TEXT(s) s
#endif

// ---------------------------------------------------------------------------
// Window management
// ---------------------------------------------------------------------------
extern "C" {
ATOM RegisterClass(WNDCLASS const * cls);
HWND CreateWindowEx(DWORD exstyle, LPCSTR classname, LPCSTR windowname, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
BOOL DestroyWindow(HWND window);
BOOL ShowWindow(HWND window, int command);
BOOL ShowWindowAsync(HWND window, int command);
BOOL UpdateWindow(HWND window);
BOOL MoveWindow(HWND window, int x, int y, int width, int height, BOOL repaint);
BOOL SetWindowPos(HWND window, HWND after, int x, int y, int cx, int cy, UINT flags);
BOOL GetClientRect(HWND window, LPRECT rect);
BOOL GetWindowRect(HWND window, LPRECT rect);
BOOL ClientToScreen(HWND window, LPPOINT point);
BOOL ScreenToClient(HWND window, LPPOINT point);
int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count);
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD exstyle);
LONG_PTR GetWindowLong(HWND window, int index);
LONG_PTR SetWindowLong(HWND window, int index, LONG_PTR value);
LONG_PTR GetWindowLongPtr(HWND window, int index);
LONG_PTR SetWindowLongPtr(HWND window, int index, LONG_PTR value);
BOOL SetWindowText(HWND window, LPCSTR text);
int GetWindowText(HWND window, LPSTR text, int max);
int GetWindowTextLength(HWND window);
int GetClassName(HWND window, LPSTR name, int max);
BOOL IsWindow(HWND window);
BOOL IsWindowVisible(HWND window);
BOOL IsWindowEnabled(HWND window);
BOOL IsChild(HWND parent, HWND child);
BOOL EnableWindow(HWND window, BOOL enable);
HWND GetParent(HWND window);
HWND GetWindow(HWND window, UINT command);
HWND GetTopWindow(HWND window);
HWND GetDesktopWindow(void);
HWND GetActiveWindow(void);
HWND SetActiveWindow(HWND window);
HWND GetFocus(void);
HWND SetFocus(HWND window);
BOOL SetForegroundWindow(HWND window);
BOOL BringWindowToTop(HWND window);
HWND FindWindow(LPCSTR classname, LPCSTR windowname);
HWND WindowFromPoint(POINT point);
HWND ChildWindowFromPoint(HWND parent, POINT point);
BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM param);
BOOL InvalidateRect(HWND window, RECT const * rect, BOOL erase);
BOOL ValidateRect(HWND window, RECT const * rect);
BOOL GetUpdateRect(HWND window, LPRECT rect, BOOL erase);
BOOL RedrawWindow(HWND window, RECT const * rect, HRGN region, UINT flags);
BOOL CloseWindow(HWND window);
HMENU GetMenu(HWND window);
HMENU GetSystemMenu(HWND window, BOOL revert);
BOOL EnableMenuItem(HMENU menu, UINT item, UINT enable);
BOOL RegisterHotKey(HWND window, int id, UINT modifiers, UINT key);
BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom);
BOOL IntersectRect(LPRECT dest, RECT const * a, RECT const * b);
BOOL PtInRect(RECT const * rect, POINT point);
int GetSystemMetrics(int index);
HMONITOR MonitorFromWindow(HWND window, DWORD flags);
BOOL GetMonitorInfo(HMONITOR monitor, LPMONITORINFO info);
int GetWindowContextHelpId(HWND window);
int MessageBox(HWND window, LPCSTR text, LPCSTR caption, UINT type);
int MessageBoxIndirect(MSGBOXPARAMS const * params);

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------
BOOL GetMessage(LPMSG msg, HWND window, UINT filtermin, UINT filtermax);
BOOL PeekMessage(LPMSG msg, HWND window, UINT filtermin, UINT filtermax, UINT remove);
BOOL TranslateMessage(MSG const * msg);
LRESULT DispatchMessage(MSG const * msg);
BOOL PostMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT SendMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
void PostQuitMessage(int code);
LRESULT DefWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT CallWindowProc(WNDPROC proc, HWND window, UINT message, WPARAM wparam, LPARAM lparam);
int TranslateAccelerator(HWND window, HACCEL table, LPMSG msg);
UINT_PTR SetTimer(HWND window, UINT_PTR id, UINT elapse, TIMERPROC proc);
BOOL KillTimer(HWND window, UINT_PTR id);

// ---------------------------------------------------------------------------
// Cursor, keyboard and capture
// ---------------------------------------------------------------------------
BOOL GetCursorPos(LPPOINT point);
BOOL SetCursorPos(int x, int y);
HCURSOR SetCursor(HCURSOR cursor);
int ShowCursor(BOOL show);
BOOL ClipCursor(RECT const * rect);
HWND SetCapture(HWND window);
BOOL ReleaseCapture(void);
HWND GetCapture(void);
SHORT GetAsyncKeyState(int key);
SHORT GetKeyState(int key);
UINT MapVirtualKey(UINT code, UINT type);
int GetKeyNameText(LONG param, LPSTR name, int size);
HCURSOR LoadCursor(HINSTANCE instance, LPCSTR name);
HICON LoadIcon(HINSTANCE instance, LPCSTR name);
HCURSOR CreateIconIndirect(ICONINFO * info);
BOOL DestroyCursor(HCURSOR cursor);
BOOL DestroyIcon(HICON icon);
int LoadString(HINSTANCE instance, UINT id, LPSTR buffer, int max);

// ---------------------------------------------------------------------------
// Dialogs and controls. Every dialog in the tree is a Win32 resource template, which
// UI_DESIGN step 13 replaces; these report failure so that a caller takes its own
// no-dialog path rather than believing in a window that was never created.
// ---------------------------------------------------------------------------
HWND CreateDialogIndirectParam(HINSTANCE instance, LPCDLGTEMPLATE templ, HWND parent, DLGPROC proc, LPARAM param);
HWND CreateDialogParam(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param);
INT_PTR DialogBoxParam(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param);
BOOL EndDialog(HWND dialog, INT_PTR result);
BOOL IsDialogMessage(HWND dialog, LPMSG msg);
HWND GetDlgItem(HWND dialog, int id);
int GetDlgCtrlID(HWND control);
BOOL SetDlgItemText(HWND dialog, int id, LPCSTR text);
UINT GetDlgItemText(HWND dialog, int id, LPSTR text, int max);
BOOL CheckDlgButton(HWND dialog, int id, UINT check);
UINT IsDlgButtonChecked(HWND dialog, int id);
LRESULT SendDlgItemMessage(HWND dialog, int id, UINT message, WPARAM wparam, LPARAM lparam);
}

#define SetWindowTextA SetWindowText
#define GetWindowTextA GetWindowText
#define LoadStringA LoadString
#define MessageBoxA MessageBox
#define GetClassNameA GetClassName
#define FindWindowA FindWindow

// ---------------------------------------------------------------------------
// GDI. The engine draws its own frame into system-memory surfaces and presents it
// through bgfx, so the only GDI users left are the legacy dialog layer and the
// tactical font path. Nothing here draws.
// ---------------------------------------------------------------------------
#define BI_RGB 0
#define BI_BITFIELDS 3
#define DIB_RGB_COLORS 0
#define DIB_PAL_COLORS 1
#define SRCCOPY 0x00CC0020
#define BLACKNESS 0x00000042
#define COLORONCOLOR 3
#define HALFTONE 4
#define TRANSPARENT 1
#define OPAQUE 2
#define TA_LEFT 0
#define TA_RIGHT 2
#define TA_CENTER 6
#define TA_TOP 0
#define DT_LEFT 0x00000000
#define DT_CENTER 0x00000001
#define DT_VCENTER 0x00000004
#define DT_SINGLELINE 0x00000020
#define GM_COMPATIBLE 1
#define GM_ADVANCED 2
#define MWT_IDENTITY 1
#define VREFRESH 116
#define BITSPIXEL 12
#define WHITE_BRUSH 0
#define BLACK_BRUSH 4
#define NULL_BRUSH 5
#define SYSTEM_FONT 13
#define FW_NORMAL 400
#define FW_BOLD 700
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define OUT_RASTER_PRECIS 6
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define PROOF_QUALITY 2
#define DEFAULT_PITCH 0
#define FF_DONTCARE 0
#define FF_SWISS 32

typedef struct tagLOGFONTA {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    CHAR lfFaceName[32];
} LOGFONTA, LOGFONT, *LPLOGFONT;

typedef struct tagTEXTMETRICA {
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading;
    LONG tmAveCharWidth, tmMaxCharWidth, tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
    CHAR tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
    BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
} TEXTMETRICA, TEXTMETRIC, *LPTEXTMETRIC;

extern "C" {
HDC GetDC(HWND window);
int ReleaseDC(HWND window, HDC dc);
HDC CreateCompatibleDC(HDC dc);
BOOL DeleteDC(HDC dc);
int SaveDC(HDC dc);
BOOL RestoreDC(HDC dc, int state);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ object);
BOOL DeleteObject(HGDIOBJ object);
int GetObject(HGDIOBJ object, int size, LPVOID buffer);
HGDIOBJ GetStockObject(int index);
HBRUSH CreateSolidBrush(COLORREF color);
HBITMAP CreateBitmap(int width, int height, UINT planes, UINT bits, void const * data);
HBITMAP CreateDIBSection(HDC dc, BITMAPINFO const * info, UINT usage, void ** bits, HANDLE section, DWORD offset);
HFONT CreateFont(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline, DWORD strikeout, DWORD charset, DWORD outprecision, DWORD clipprecision, DWORD quality, DWORD pitch, LPCSTR face);
HFONT CreateFontIndirect(LOGFONT const * font);
int GetDeviceCaps(HDC dc, int index);
BOOL BitBlt(HDC dest, int x, int y, int width, int height, HDC source, int sx, int sy, DWORD rop);
BOOL StretchBlt(HDC dest, int x, int y, int width, int height, HDC source, int sx, int sy, int swidth, int sheight, DWORD rop);
int SetStretchBltMode(HDC dc, int mode);
int SetDIBitsToDevice(HDC dc, int x, int y, DWORD width, DWORD height, int sx, int sy, UINT start, UINT lines, void const * bits, BITMAPINFO const * info, UINT usage);
BOOL TextOut(HDC dc, int x, int y, LPCSTR text, int length);
int DrawText(HDC dc, LPCSTR text, int length, LPRECT rect, UINT format);
BOOL GetTextExtentPoint32(HDC dc, LPCSTR text, int length, LPSIZE size);
BOOL GetTextMetrics(HDC dc, LPTEXTMETRIC metrics);
UINT SetTextAlign(HDC dc, UINT align);
COLORREF SetTextColor(HDC dc, COLORREF color);
COLORREF GetTextColor(HDC dc);
COLORREF SetBkColor(HDC dc, COLORREF color);
COLORREF GetBkColor(HDC dc);
int SetBkMode(HDC dc, int mode);
int GetBkMode(HDC dc);
int SetGraphicsMode(HDC dc, int mode);
BOOL SetViewportOrgEx(HDC dc, int x, int y, LPPOINT previous);
BOOL SetWindowOrgEx(HDC dc, int x, int y, LPPOINT previous);
BOOL DPtoLP(HDC dc, LPPOINT points, int count);
BOOL FillRect(HDC dc, RECT const * rect, HBRUSH brush);
BOOL PatBlt(HDC dc, int x, int y, int width, int height, DWORD rop);
void GdiFlush(void);
BOOL EnumDisplaySettings(LPCSTR device, DWORD mode, DEVMODE * settings);
}

// ---------------------------------------------------------------------------
// Kernel services
// ---------------------------------------------------------------------------
#define ERROR_SUCCESS 0L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_FILE_NOT_FOUND 2L
#define WAIT_OBJECT_0 0L
#define WAIT_TIMEOUT 258L
#define WAIT_FAILED 0xFFFFFFFFL
#define MUTEX_ALL_ACCESS 0x1F0001L
#define INFINITE 0xFFFFFFFFL
#define GENERIC_READ 0x80000000L
#define GENERIC_WRITE 0x40000000L
#define FILE_SHARE_READ 0x00000001L
#define FILE_SHARE_WRITE 0x00000002L
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INVALID_SET_FILE_POINTER 0xFFFFFFFFL
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFL
#define FILE_ATTRIBUTE_READONLY 0x00000001L
#define FILE_ATTRIBUTE_HIDDEN 0x00000002L
#define FILE_ATTRIBUTE_SYSTEM 0x00000004L
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010L
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020L
#define FILE_ATTRIBUTE_NORMAL 0x00000080L
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100L
#define CP_ACP 0
#define CP_OEMCP 1
#define CP_UTF8 65001
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01
#define LANG_USER_DEFAULT 0x0400
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#define TIME_NOSECONDS 0x0002
#define TIME_NOMINUTESORSECONDS 0x0001
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)0x80000002)
#define KEY_READ 0x20019
#define SRWLOCK_INIT { NULL }

typedef struct _COORD { SHORT X, Y; } COORD;
typedef struct _SMALL_RECT { SHORT Left, Top, Right, Bottom; } SMALL_RECT;
typedef struct _CONSOLE_SCREEN_BUFFER_INFO { COORD dwSize, dwCursorPosition; WORD wAttributes; SMALL_RECT srWindow; COORD dwMaximumWindowSize; } CONSOLE_SCREEN_BUFFER_INFO;
typedef struct _RTL_OSVERSIONINFOW { ULONG dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId; WCHAR szCSDVersion[128]; } RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW;
typedef BYTE *PBYTE;

extern "C" {
DWORD GetLastError(void);
void SetLastError(DWORD code);
HANDLE CreateMutex(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCSTR name);
HANDLE OpenMutex(DWORD access, BOOL inherit, LPCSTR name);
DWORD WaitForSingleObject(HANDLE object, DWORD milliseconds);
BOOL ReleaseMutex(HANDLE mutex);
BOOL CloseHandle(HANDLE object);
DWORD GetCurrentProcessId(void);
DWORD GetCurrentThreadId(void);
BOOL IsDebuggerPresent(void);
void OutputDebugString(LPCSTR text);
void Sleep(DWORD milliseconds);
HMODULE GetModuleHandle(LPCSTR name);
HMODULE LoadLibrary(LPCSTR name);
BOOL FreeLibrary(HMODULE module);
FARPROC GetProcAddress(HMODULE module, LPCSTR name);
DWORD GetModuleFileName(HMODULE module, LPSTR name, DWORD size);
HLOCAL LocalFree(HLOCAL memory);
LPWSTR GetCommandLineW(void);
LPWSTR * CommandLineToArgvW(LPCWSTR commandline, int * count);
UINT GetACP(void);
UINT GetOEMCP(void);

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes, DWORD disposition, DWORD flags, HANDLE templatefile);
BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD size, LPDWORD read, LPOVERLAPPED overlapped);
BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD size, LPDWORD written, LPOVERLAPPED overlapped);
DWORD SetFilePointer(HANDLE file, LONG distance, LONG * distancehigh, DWORD method);
BOOL DeleteFileA(LPCSTR name);
BOOL CopyFile(LPCSTR from, LPCSTR to, BOOL failifexists);
BOOL CreateDirectory(LPCSTR path, LPSECURITY_ATTRIBUTES attributes);
BOOL SetCurrentDirectory(LPCSTR path);
DWORD GetFileAttributesA(LPCSTR name);
HANDLE FindFirstFile(LPCSTR name, LPWIN32_FIND_DATA data);
BOOL FindNextFile(HANDLE find, LPWIN32_FIND_DATA data);
BOOL FindClose(HANDLE find);
LONG CompareFileTime(FILETIME const * a, FILETIME const * b);
BOOL FileTimeToLocalFileTime(FILETIME const * file, LPFILETIME local);
BOOL FileTimeToSystemTime(FILETIME const * file, LPSYSTEMTIME system);
BOOL SystemTimeToFileTime(SYSTEMTIME const * system, LPFILETIME file);
void GetSystemTime(LPSYSTEMTIME system);

BOOL AllocConsole(void);
HWND GetConsoleWindow(void);
HANDLE GetStdHandle(DWORD which);
BOOL SetConsoleTitle(LPCSTR title);
BOOL WriteConsole(HANDLE console, void const * buffer, DWORD length, LPDWORD written, LPVOID reserved);
BOOL GetConsoleScreenBufferInfo(HANDLE console, CONSOLE_SCREEN_BUFFER_INFO * info);

void InitializeSRWLock(SRWLOCK * lock);
void AcquireSRWLockExclusive(SRWLOCK * lock);
void ReleaseSRWLockExclusive(SRWLOCK * lock);

DWORD GetFileVersionInfoSize(LPCSTR name, LPDWORD handle);
BOOL GetFileVersionInfo(LPCSTR name, DWORD handle, DWORD length, LPVOID data);
BOOL VerQueryValue(LPCVOID block, LPCSTR path, LPVOID * buffer, UINT * length);
HRSRC FindResource(HMODULE module, LPCSTR name, LPCSTR type);
HGLOBAL LoadResource(HMODULE module, HRSRC resource);
LPVOID LockResource(HGLOBAL resource);
DWORD SizeofResource(HMODULE module, HRSRC resource);
LONG RegOpenKeyEx(HKEY key, LPCSTR subkey, DWORD options, DWORD access, HKEY * result);
LONG RegQueryValueEx(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size);
LONG RegCloseKey(HKEY key);
}

#define DeleteFile DeleteFileA
#define GetFileAttributes GetFileAttributesA
#define CreateFile CreateFileA
#define GetFileVersionInfoSizeA GetFileVersionInfoSize
#define GetFileVersionInfoA GetFileVersionInfo
#define VerQueryValueA VerQueryValue
#define GetModuleFileNameA GetModuleFileName
#define LoadLibraryA LoadLibrary
#define GetModuleHandleA GetModuleHandle
#define OutputDebugStringA OutputDebugString
#define SetConsoleTitleA SetConsoleTitle
#define WriteConsoleA WriteConsole
#define RegOpenKeyExA RegOpenKeyEx
#define RegQueryValueExA RegQueryValueEx
#define CreateMutexA CreateMutex
#define OpenMutexA OpenMutex
#define FindFirstFileA FindFirstFile
#define FindNextFileA FindNextFile
#define CreateDirectoryA CreateDirectory
#define SetCurrentDirectoryA SetCurrentDirectory
#define CopyFileA CopyFile

// ---------------------------------------------------------------------------
// The remaining entry points the tree names, grouped with the ones above by role.
// ---------------------------------------------------------------------------
typedef struct _XFORM { FLOAT eM11, eM12, eM21, eM22, eDx, eDy; } XFORM;

extern "C" {
int MultiByteToWideChar(UINT codepage, DWORD flags, LPCSTR source, int sourcelength, LPWSTR dest, int destlength);
int WideCharToMultiByte(UINT codepage, DWORD flags, LPCWSTR source, int sourcelength, LPSTR dest, int destlength, LPCSTR defaultchar, LPBOOL useddefault);
void GetLocalTime(LPSYSTEMTIME time);
int GetTimeFormat(DWORD locale, DWORD flags, SYSTEMTIME const * time, LPCSTR format, LPSTR buffer, int size);
int GetDateFormat(DWORD locale, DWORD flags, SYSTEMTIME const * time, LPCSTR format, LPSTR buffer, int size);
DWORD FormatMessage(DWORD flags, LPCVOID source, DWORD id, DWORD language, LPSTR buffer, DWORD size, void * arguments);
BOOL SetStdHandle(DWORD which, HANDLE handle);
BOOL SetConsoleCP(UINT codepage);
BOOL SetConsoleOutputCP(UINT codepage);
BOOL SetConsoleScreenBufferSize(HANDLE console, COORD size);
BOOL DeleteMenu(HMENU menu, UINT position, UINT flags);
BOOL WinHelp(HWND window, LPCSTR help, UINT command, ULONG_PTR data);
int ToUnicode(UINT key, UINT scan, BYTE const * state, LPWSTR buffer, int size, UINT flags);
HWND GetNextDlgTabItem(HWND dialog, HWND control, BOOL previous);
BOOL ModifyWorldTransform(HDC dc, XFORM const * transform, DWORD mode);
}

#define SNDMSG SendMessage
#define SendMessageA SendMessage
#define PostMessageA PostMessage
#define GetWindowLongPtrA GetWindowLongPtr
#define SetWindowLongPtrA SetWindowLongPtr
#define GetWindowLongA GetWindowLong
#define SetWindowLongA SetWindowLong
#define MultiByteToWideCharA MultiByteToWideChar
#define GetTimeFormatA GetTimeFormat
#define GetDateFormatA GetDateFormat
#define FormatMessageA FormatMessage
#define WinHelpA WinHelp
#define GetKeyNameTextA GetKeyNameText
#define SetDlgItemTextA SetDlgItemText
#define GetDlgItemTextA GetDlgItemText
#define CreateFontA CreateFont
#define TextOutA TextOut
#define DrawTextA DrawText
#define GetTextExtentPoint32A GetTextExtentPoint32
#define EnumDisplaySettingsA EnumDisplaySettings
