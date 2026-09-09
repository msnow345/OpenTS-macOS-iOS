#pragma once
#include <windows.h>

// Common controls exist only in the legacy dialog layer, which UI_DESIGN step 13 replaces.
// The window class names are declared so that a template that names one still compiles.
#define TRACKBAR_CLASS "msctls_trackbar32"
#define PROGRESS_CLASS "msctls_progress32"
#define HOTKEY_CLASS "msctls_hotkey32"
#define WC_TREEVIEW "SysTreeView32"
#define WC_LISTVIEW "SysListView32"
#define WC_TABCONTROL "SysTabControl32"

#define TBM_GETPOS (WM_USER)
#define TBM_GETRANGEMIN (WM_USER + 1)
#define TBM_GETRANGEMAX (WM_USER + 2)
#define TBM_SETPOS (WM_USER + 5)
#define TBM_SETRANGE (WM_USER + 6)
#define TB_LINEUP 0
#define TB_LINEDOWN 1
#define TB_THUMBPOSITION 4
#define TB_THUMBTRACK 5
#define TB_ENDTRACK 8

#define PBM_SETRANGE (WM_USER + 1)
#define PBM_SETPOS (WM_USER + 2)

#define HKM_SETHOTKEY (WM_USER + 1)
#define HKM_GETHOTKEY (WM_USER + 2)

#define TV_FIRST 0x1100
#define TVM_SELECTITEM (TV_FIRST + 11)
#define TVM_GETNEXTITEM (TV_FIRST + 10)
#define TVM_GETINDENT (TV_FIRST + 6)
#define TVM_GETEDITCONTROL (TV_FIRST + 15)
#define TVGN_ROOT 0
#define TVGN_NEXT 1
#define TVGN_PREVIOUS 2
#define TVGN_CARET 9
#define TVGN_FIRSTVISIBLE 5
#define TVGN_NEXTVISIBLE 6
#define TVGN_PREVIOUSVISIBLE 7
#define TVGN_DROPHILITE 8
#define TVE_COLLAPSE 0x0001
#define TVE_EXPAND 0x0002
#define TVIF_TEXT 0x0001
#define TVIF_IMAGE 0x0002
#define TVIF_PARAM 0x0004
#define TVIF_STATE 0x0008
#define TVIF_HANDLE 0x0010
#define TVIF_SELECTEDIMAGE 0x0020
#define TVIS_EXPANDED 0x0020
#define TVIS_SELECTED 0x0002

typedef struct tagTVITEMA {
    UINT mask; HTREEITEM hItem; UINT state, stateMask;
    LPSTR pszText; int cchTextMax, iImage, iSelectedImage, cChildren; LPARAM lParam;
} TVITEMA, TVITEM, TV_ITEM;

typedef struct tagNMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR;
typedef struct tagTVDISPINFOA { NMHDR hdr; TVITEMA item; } NMTVDISPINFOA, NMTVDISPINFO;

#define TreeView_SelectItem(hwnd, item) ((BOOL)SendMessage((hwnd), TVM_SELECTITEM, TVGN_CARET, (LPARAM)(HTREEITEM)(item)))
#define TreeView_SelectDropTarget(hwnd, item) ((BOOL)SendMessage((hwnd), TVM_SELECTITEM, TVGN_DROPHILITE, (LPARAM)(HTREEITEM)(item)))
#define TreeView_SelectSetFirstVisible(hwnd, item) ((BOOL)SendMessage((hwnd), TVM_SELECTITEM, TVGN_FIRSTVISIBLE, (LPARAM)(HTREEITEM)(item)))
#define TreeView_GetNextItem(hwnd, item, code) ((HTREEITEM)SendMessage((hwnd), TVM_GETNEXTITEM, (WPARAM)(code), (LPARAM)(HTREEITEM)(item)))
#define TreeView_GetRoot(hwnd) TreeView_GetNextItem((hwnd), NULL, TVGN_ROOT)
#define TreeView_GetNextSibling(hwnd, item) TreeView_GetNextItem((hwnd), (item), TVGN_NEXT)
#define TreeView_GetFirstVisible(hwnd) TreeView_GetNextItem((hwnd), NULL, TVGN_FIRSTVISIBLE)
#define TreeView_GetNextVisible(hwnd, item) TreeView_GetNextItem((hwnd), (item), TVGN_NEXTVISIBLE)
#define TreeView_GetPrevVisible(hwnd, item) TreeView_GetNextItem((hwnd), (item), TVGN_PREVIOUSVISIBLE)
#define TreeView_GetIndent(hwnd) ((int)SendMessage((hwnd), TVM_GETINDENT, 0, 0))
#define TreeView_GetEditControl(hwnd) ((HWND)SendMessage((hwnd), TVM_GETEDITCONTROL, 0, 0))

#define LVM_FIRST 0x1000
#define LVM_GETCOLUMNWIDTH (LVM_FIRST + 29)
#define LVM_SETCOLUMNWIDTH (LVM_FIRST + 30)
#define ListView_GetColumnWidth(hwnd, index) ((int)SendMessage((hwnd), LVM_GETCOLUMNWIDTH, (WPARAM)(int)(index), 0))
#define ListView_SetColumnWidth(hwnd, index, width) ((BOOL)SendMessage((hwnd), LVM_SETCOLUMNWIDTH, (WPARAM)(int)(index), MAKELPARAM((width), 0)))

#define TCM_FIRST 0x1300
#define TCM_GETITEMCOUNT (TCM_FIRST + 4)
#define TCM_GETCURSEL (TCM_FIRST + 11)
#define TCM_GETITEMRECT (TCM_FIRST + 10)
#define TCM_SETITEMSIZE (TCM_FIRST + 41)
#define TCIF_TEXT 0x0001
typedef struct tagTCITEMA { UINT mask; DWORD dwState, dwStateMask; LPSTR pszText; int cchTextMax, iImage; LPARAM lParam; } TCITEMA, TC_ITEM;
#define TabCtrl_GetItemCount(hwnd) ((int)SendMessage((hwnd), TCM_GETITEMCOUNT, 0, 0))
#define TabCtrl_GetCurSel(hwnd) ((int)SendMessage((hwnd), TCM_GETCURSEL, 0, 0))
#define TabCtrl_GetItemRect(hwnd, index, rect) ((BOOL)SendMessage((hwnd), TCM_GETITEMRECT, (WPARAM)(int)(index), (LPARAM)(RECT *)(rect)))

extern "C" {
void InitCommonControls(void);
BOOL ImageList_BeginDrag(HIMAGELIST list, int image, int x, int y);
BOOL ImageList_DragEnter(HWND lock, int x, int y);
BOOL ImageList_DragMove(int x, int y);
BOOL ImageList_DragShowNolock(BOOL show);
void ImageList_EndDrag(void);
BOOL ImageList_Destroy(HIMAGELIST list);
}

#define TVM_GETITEM (TV_FIRST + 12)
#define TVM_SETITEM (TV_FIRST + 13)
#define TVM_EXPAND (TV_FIRST + 2)
#define TVM_GETITEMRECT (TV_FIRST + 4)
#define TVM_CREATEDRAGIMAGE (TV_FIRST + 18)
#define TreeView_GetItem(hwnd, item) ((BOOL)SendMessage((hwnd), TVM_GETITEM, 0, (LPARAM)(TVITEM *)(item)))
#define TreeView_SetItem(hwnd, item) ((BOOL)SendMessage((hwnd), TVM_SETITEM, 0, (LPARAM)(TVITEM const *)(item)))
#define TreeView_Expand(hwnd, item, code) ((BOOL)SendMessage((hwnd), TVM_EXPAND, (WPARAM)(code), (LPARAM)(HTREEITEM)(item)))
#define TreeView_GetItemRect(hwnd, item, rect, partial) (*(HTREEITEM *)(rect) = (item), (BOOL)SendMessage((hwnd), TVM_GETITEMRECT, (WPARAM)(BOOL)(partial), (LPARAM)(RECT *)(rect)))
#define TreeView_CreateDragImage(hwnd, item) ((HIMAGELIST)SendMessage((hwnd), TVM_CREATEDRAGIMAGE, 0, (LPARAM)(HTREEITEM)(item)))

#define TCM_GETITEM (TCM_FIRST + 5)
#define TabCtrl_GetItem(hwnd, index, item) ((BOOL)SendMessage((hwnd), TCM_GETITEM, (WPARAM)(int)(index), (LPARAM)(TC_ITEM *)(item)))
