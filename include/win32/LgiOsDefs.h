//
//      FILE:           LgiOsDefs.h (Win32)
//      AUTHOR:         Matthew Allen
//      DATE:           24/9/99
//      DESCRIPTION:    Lgi Win32 OS defines
//
//      Copyright (C) 1999, Matthew Allen
//              fret@memecode.com
//

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#pragma warning(disable : 4251 4275)

#define WIN32GTK                    0
#define WIN32NATIVE                 1

#include <string.h>
#include "LgiInc.h"
#include "LgiDefs.h"
#include "LgiClass.h"

//////////////////////////////////////////////////////////////////
// Includes
#define WIN32_LEAN_AND_MEAN
#include "winsock2.h"
#include "windows.h"
#include "SHELLAPI.H"
#include "COMMDLG.H"
#include "LgiInc.h"

//////////////////////////////////////////////////////////////////
// Typedefs
typedef __w64 int			NativeInt;
typedef __w64 unsigned int	UNativeInt;

typedef HWND				OsWindow;
typedef HWND				OsView;
typedef HANDLE				OsThread;
typedef HANDLE				OsProcess;
typedef char16				OsChar;
typedef HBITMAP				OsBitmap;
typedef HDC					OsPainter;

typedef BOOL (__stdcall *pSHGetSpecialFolderPathA)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);
typedef BOOL (__stdcall *pSHGetSpecialFolderPathW)(HWND hwndOwner, LPWSTR lpszPath, int nFolder, BOOL fCreate);
typedef int (__stdcall *pSHFileOperationA)(LPSHFILEOPSTRUCTA lpFileOp);
typedef int (__stdcall *pSHFileOperationW)(LPSHFILEOPSTRUCTW lpFileOp);
typedef int (__stdcall *p_vscprintf)(const char *format, va_list argptr);

class LgiClass GMessage
{
public:
	int Msg;
	WPARAM a;
	LPARAM b;
	
	typedef LPARAM Param;

	GMessage()
	{
		Msg = a = b = 0;
	}

	GMessage(int M, WPARAM A = 0, LPARAM B = 0)
	{
		Msg = M;
		a = A;
		b = B;
	}
};

class LgiClass OsAppArguments
{
	GAutoWString CmdLine;

	void _Default();

public:
	HINSTANCE hInstance;
	DWORD Pid;
	char16 *lpCmdLine;
	int nCmdShow;

	OsAppArguments();
	OsAppArguments(int Args, char **Arg);

	OsAppArguments &operator =(OsAppArguments &p);

	void Set(char *Utf);
	void Set(int Args, char **Arg);
};

//////////////////////////////////////////////////////////////////
// Defines
#define MsgCode(m)					(m->Msg)
#define MsgA(m)						(m->a)
#define MsgB(m)						(m->b)
#define CreateMsg(m, a, b)			GMessage(m, a, b)
#define IsWin9x						(GApp::Win9x)
#define DefaultOsView(t)			NULL

#if defined(__GNUC__)
#define stricmp						strcasecmp
#define strnicmp					strncasecmp
#endif

// Key redefs
#define VK_PAGEUP					VK_PRIOR
#define VK_PAGEDOWN					VK_NEXT
#define VK_BACKSPACE				VK_BACK

// Sleep the current thread
LgiFunc void LgiSleep(DWORD i);

// Process
typedef DWORD						OsProcessId;
LgiExtern HINSTANCE					_lgi_app_instance;
#define LgiProcessInst()			_lgi_app_instance
extern p_vscprintf					lgi_vscprintf;

// Threads
typedef DWORD						OsThreadId;
typedef CRITICAL_SECTION			OsSemaphore;
#define LgiGetCurrentThread()		GetCurrentThreadId()

// Socket/Network
#define ValidSocket(s)				((s) != INVALID_SOCKET)
typedef SOCKET						OsSocket;

// Run the message loop to process any pending messages
#define LgiYield()					GApp::ObjInstance()->Run(false)

#ifdef _MSC_VER
#define snprintf					_snprintf
#define atoi64						_atoi64
//#define vsnprintf					_vsnprintf
#define vsnwprintf					_vsnwprintf
#endif

#define K_CHAR						0x0

#define LGI_GViewMagic				0x14412662
#define LGI_FileDropFormat			"CF_HDROP"
#define LGI_WideCharset				"ucs-2"
#define LGI_PrintfInt64				"%I64i"
#define LGI_IllegalFileNameChars	"\t\r\n/\\:*?\"<>|"

#define MK_LEFT						MK_LBUTTON
#define MK_RIGHT					MK_RBUTTON
#define MK_MIDDLE					MK_MBUTTON
#define MK_CTRL						MK_CONTROL
	
// Edge types
#define SUNKEN						1
#define RAISED						2
#define CHISEL						3
#define FLAT						4

// Stupid mouse wheel defines don't work. hmmm
#define WM_MOUSEWHEEL				0x020A
#define WHEEL_DELTA					120
#ifndef SPI_GETWHEELSCROLLLINES
#define SPI_GETWHEELSCROLLLINES		104
#endif

// Window flags
#define GWF_VISIBLE					WS_VISIBLE
#define GWF_DISABLED				WS_DISABLED
#define GWF_BORDER					WS_BORDER
#define GWF_TABSTOP					WS_TABSTOP
#define GWF_FOCUS					0x00000001
#define GWF_OVER					0x00000002
#define GWF_DROP_TARGET				0x00000004
#define GWF_SUNKEN					0x00000008
#define GWF_FLAT					0x00000010
#define GWF_RAISED					0x00000020
#define GWF_DIALOG					0x00000040
#define GWF_DESTRUCTOR				0x00000080
#define GWF_QUIT_WND				0x00000100

// Widgets
#define DialogToPixelX(i)			(((i)*Bx)/4)
#define DialogToPixelY(i)			(((i)*By)/8)
#define PixelToDialogX(i)			(((i)*4)/Bx)
#define PixelToDialogY(i)			(((i)*8)/By)

#define DIALOG_X					1.56
#define DIALOG_Y					1.85
#define CTRL_X						1.50
#define CTRL_Y						1.64

// Messages

// Quite a lot of windows stuff uses WM_USER+n where
// n < 0x1A0 or so... so stay out of their way.
	#define M_USER						WM_USER
	#define M_CUT						WM_CUT
	#define M_COPY						WM_COPY
	#define M_PASTE						WM_PASTE
	#define M_COMMAND					WM_COMMAND
	#define M_CLOSE						WM_CLOSE

	// wParam = bool Inside; // is the mouse inside the client area?
	// lParam = MAKELONG(x, y); // mouse location
	#define M_MOUSEENTER				(M_USER+0x1000)
	#define M_MOUSEEXIT					(M_USER+0x1001)

	// wParam = (GView*) Wnd;
	// lParam = (int) Flags;
	#define M_CHANGE					(M_USER+0x1002)

	// wParam = (GView*) Wnd;
	// lParam = (char*) Text; // description from window
	#define M_DESCRIBE					(M_USER+0x1003)

	// return (bool)
	#define M_WANT_DIALOG_PROC			(M_USER+0x1004)

	// wParam = void
	// lParam = (MSG*) Msg;
	#define M_TRANSLATE_ACCELERATOR		(M_USER+0x1005)

	// None
	#define M_TRAY_NOTIFY				(M_USER+0x1006)

	// lParam = Style
	#define M_SET_WND_STYLE				(M_USER+0x1007)

	// lParam = GScrollBar *Obj
	#define M_SCROLL_BAR_CHANGED		(M_USER+0x1008)

	// lParam = HWND of window under mouse
	// This is only sent for non-LGI window in our process
	// because we'd get WM_MOUSEMOVE's for our own windows
	#define M_HANDLEMOUSEMOVE			(M_USER+0x1009)

	// Calls SetWindowPlacement...
	#define M_SET_WINDOW_PLACEMENT		(M_USER+0x100a)

	// Generic delete selection msg
	#define M_DELETE					(M_USER+0x100b)

	/// GThreadWork object completed
	///
	/// MsgA = (GThreadOwner*) Owner;
	/// MsgB = (GThreadWork*) WorkUnit;
	// #define M_GTHREADWORK_COMPELTE		(M_USER+0x100c)

// Directories
#define DIR_CHAR					'\\'
#define DIR_STR						"\\"
#define EOL_SEQUENCE				"\r\n"

#define IsSlash(c)					(((c)=='/')OR((c)=='\\'))
#define IsQuote(c)					(((c)=='\"')OR((c)=='\''))

#define LGI_PATH_SEPARATOR			";"
#define LGI_ALL_FILES				"*.*"
#define LGI_LIBRARY_EXT				"dll"

/////////////////////////////////////////////////////////////////////////////////////
// Typedefs
typedef HWND OsView;

/////////////////////////////////////////////////////////////////////////////////////
// Externs
LgiFunc class GViewI *GWindowFromHandle(OsView hWnd);
LgiFunc int GetMouseWheelLines();
LgiFunc int WinPointToHeight(int Pt);
LgiFunc int WinHeightToPoint(int Ht);
LgiFunc char *GetWin32Folder(int Id);

/// Convert a string d'n'd format to an OS dependant integer.
LgiFunc int FormatToInt(char *s);
/// Convert a Os dependant integer d'n'd format to a string.
LgiFunc char *FormatToStr(int f);


#endif
