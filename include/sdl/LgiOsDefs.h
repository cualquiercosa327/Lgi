/**
	\file
	\author Matthew Allen
	\brief SDL specific types and defines.
 */

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#include <string.h>
#include "assert.h"
#include "LgiDefs.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#define LGI_SDL				1

#ifdef WIN32
	#define LGI_SDL_WIN		1
	#define WIN32_LEAN_AND_MEAN
	#include "windows.h"
	#include "winsock2.h"
	#include "ShellAPI.h"
#else
	#define LGI_SDL_POSIX	1
	#define _MULTI_THREADED
	#include <pthread.h>
	#define XP_CTRLS		1
	#define POSIX			1
#endif

// Include SDL, if it's missing:
// sudo apt-get install libsdl1.2-dev
#include <SDL.h>

// Include Freetype2, if it's missing:
// sudo apt-get install libfreetype6-dev
#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef _MSC_VER
#pragma warning(disable : 4275 4251)
#endif

#include "LgiInc.h"

// System

class LgiClass OsAppArguments
{
	struct OsAppArgumentsPriv *d;

public:
	int Args;
	char **Arg;

	OsAppArguments(int args, char **arg);
	~OsAppArguments();

	void Set(const char *CmdLine);
	OsAppArguments &operator =(OsAppArguments &a);
};

// Process
#ifdef WINNATIVE
typedef HANDLE							OsProcess;
#else
typedef int								OsProcess;
#endif
typedef int								OsProcessId;
typedef void							*OsView;
typedef void							*OsWindow;
typedef uint32							OsChar;
typedef SDL_Surface						*OsPainter;
typedef FT_Face							OsFont;
typedef SDL_Surface						*OsBitmap;

class OsApplication
{
	class OsApplicationPriv *d;

protected:
	static OsApplication *Inst;
	
public:
	OsApplication(int Args, char **Arg);
	~OsApplication();
	
	static OsApplication *GetInst() { LgiAssert(Inst != NULL); return Inst; }
};

// Threads
#ifdef WIN32

typedef HANDLE  					OsThread;
typedef DWORD						OsThreadId;
typedef CRITICAL_SECTION			OsSemaphore;
#define LgiGetCurrentThread()		GetCurrentThreadId()

#elif defined POSIX

typedef pthread_t					OsThread;
typedef pthread_t					OsThreadId;
typedef pthread_mutex_t				OsSemaphore;
#define LgiGetCurrentThread()		pthread_self()

#else

#error "No impl"

#endif

class LgiClass GMessage
{
public:
    typedef void *Param;
    typedef NativeInt Result;
    SDL_Event Event;

	GMessage(int m = 0, Param pa = 0, Param pb = 0);
	~GMessage();

	int Msg();
	Param A();
	Param B();
	void Set(int m, Param pa, Param pb);
	bool Send(OsView Wnd);
};


#define MsgCode(m)					m->Msg()
#define MsgA(m)						m->A()
#define MsgB(m)						m->B()

#ifndef _MSC_VER
#pragma clang diagnostic ignored "-Wtautological-undefined-compare"
#endif

// Sockets
#define ValidSocket(s)				((s)>=0)
#ifndef WIN32
#define INVALID_SOCKET				-1
#endif
typedef int OsSocket;

/// Sleep the current thread for i milliseconds.
#ifdef WIN32
LgiFunc void LgiSleep(DWORD i);
#else
#define LgiSleep(i)					_lgi_sleep(i)
LgiFunc void _lgi_sleep(int i);
#endif

#ifndef WIN32
#define atoi64						atoll
#else
#define atoi64						_atoi64
#endif

#define _snprintf					snprintf
#define _vsnprintf					vsnprintf

/// Process any pending messages in the applications message que and then return.
#define LgiYield()					LgiApp->Run(false)

#define K_CHAR						0x0

/// Drag and drop format for a file
#define LGI_FileDropFormat			"text/uri-list"
#define LGI_IllegalFileNameChars	"\t\r\n/\\:*?\"<>|"
#ifdef WINDOWS
#define LGI_WideCharset				"ucs-2"
#else
#define LGI_WideCharset				"utf-32"
#endif
#ifdef _MSC_VER
#define LGI_PrintfInt64				"%I64i"
#else
#define LGI_PrintfInt64				"%lli"
#endif

#ifndef SND_ASYNC
#define SND_ASYNC					0x0001
#endif

#define DOUBLE_CLICK_THRESHOLD		5
#define DOUBLE_CLICK_TIME			400

// Window flags
#define GWF_VISIBLE					0x00000001
#define GWF_DISABLED				0x00000002
#define GWF_FOCUS					0x00000004
#define GWF_OVER					0x00000008
#define GWF_DROP_TARGET				0x00000010
#define GWF_SUNKEN					0x00000020
#define GWF_FLAT					0x00000040
#define GWF_RAISED					0x00000080
#define GWF_BORDER					0x00000100
#define GWF_DIALOG					0x00000200
#define GWF_DESTRUCTOR				0x00000400
#define GWF_QUIT_WND				0x00000800

// Menu flags
#ifndef WIN32
#define ODS_SELECTED				0x1
#define ODS_DISABLED				0x2
#define ODS_CHECKED					0x4
#endif

/// Edge type: Sunken
#define SUNKEN						1
/// Edge type: Raised
#define RAISED						2
/// Edge type: Chiseled
#define CHISEL						3
/// Edge type: Flat
#define FLAT						4

#ifdef WIN32
	/// The directory separator character on Linux as a char
	#define DIR_CHAR				'\\'
	/// The directory separator character on Linux as a string
	#define DIR_STR					"\\"
	/// The path list separator character for Linux
	#define LGI_PATH_SEPARATOR		";"
	/// The pattern that matches all files in Linux
	#define LGI_ALL_FILES			"*.*"
	/// The stardard extension for dynamically linked code
	#define LGI_LIBRARY_EXT			"dll"
	/// The standard executable extension
	#define LGI_EXECUTABLE_EXT		".exe"
#else
	/// The directory separator character on Linux as a char
	#define DIR_CHAR				'/'
	/// The directory separator character on Linux as a string
	#define DIR_STR					"/"
	/// The path list separator character for Linux
	#define LGI_PATH_SEPARATOR		":"
	/// The pattern that matches all files in Linux
	#define LGI_ALL_FILES			"*"
	/// The stardard extension for dynamically linked code
	#if defined MAC
	#define LGI_LIBRARY_EXT			"dylib"
	#else
	#define LGI_LIBRARY_EXT			"so"
	#endif
	/// The standard executable extension
	#define LGI_EXECUTABLE_EXT		""
#endif
/// The standard end of line string for Linux
#define EOL_SEQUENCE				"\n"
/// Tests a char for being a slash
#define IsSlash(c)					(((c)=='/')||((c)=='\\'))
/// Tests a char for being a quote
#define IsQuote(c)					(((c)=='\"')||((c)=='\''))

/// Base point for system messages.
#define M_SYSTEM					0x03f0
/// Message that indicates the user is trying to close a top level window.
#define M_CLOSE						(M_SYSTEM+1)
/// Implemented to handle invalide requests in the GUI thread.
#define M_X11_INVALIDATE			(M_SYSTEM+2)
/// Implemented to handle paint requests in the GUI thread.
#define M_X11_REPARENT				(M_SYSTEM+4)

/// Minimum value for application defined message ID's
#define M_USER						0x0400

/// \brief Mouse enter event
///
/// a = bool Inside; // is the mouse inside the client area?\n
/// b = MAKELONG(x, y); // mouse location
#define M_MOUSEENTER				(M_USER+100)

/// \brief Mouse exit event
///
/// a = bool Inside; // is the mouse inside the client area?\n
/// b = MAKELONG(x, y); // mouse location
#define M_MOUSEEXIT					(M_USER+101)

/// \brief GView change notification
///
/// a = (GView*) Wnd;\n
/// b = (int) Flags; // Specific to each GView
#define M_CHANGE					(M_USER+102)

/// \brief Pass a text message up to the UI to descibe whats happening
///
/// a = (GView*) Wnd;\n
/// b = (char*) Text; // description from window
#define M_DESCRIBE					(M_USER+103)

// return (bool)
#define M_WANT_DIALOG_PROC			(M_USER+104)

#define M_MENU						(M_USER+105)
#define M_COMMAND					(M_USER+106)
#define M_DRAG_DROP					(M_USER+107)

#define M_TRAY_NOTIFY				(M_USER+108)
#define M_CUT						(M_USER+109)
#define M_COPY						(M_USER+110)
#define M_PASTE						(M_USER+111)
#define M_DELETE					(M_USER+112)
#define M_GTHREADWORK_COMPELTE		(M_USER+113)
/// Implemented to handle timer events in the GUI thread.
#define M_PULSE						(M_USER+114)
#define M_SET_VISIBLE				(M_USER+115)
#define M_INVALIDATE				(M_USER+116)

/// Standard ID for an "Ok" button.
/// \sa LgiMsg
#define IDOK						1
/// Standard ID for a "Cancel" button.
/// \sa LgiMsg
#define IDCANCEL					2
/// Standard ID for a "Yes" button.
/// \sa LgiMsg
#define IDYES						6
/// Standard ID for a "No" button.
/// \sa LgiMsg
#define IDNO						7

#ifndef WIN32
/// Standard message box with an Ok button.
/// \sa LgiMsg
#define MB_OK						0
/// Standard message box with Ok and Cancel buttons.
/// \sa LgiMsg
#define MB_OKCANCEL					1
/// Standard message box with Yes, No and Cancel buttons.
/// \sa LgiMsg
#define MB_YESNOCANCEL				3
/// Standard message box with Yes and No buttons.
/// \sa LgiMsg
#define MB_YESNO					4

#define MB_SYSTEMMODAL				0x1000
#endif

/// The CTRL key is pressed
/// \sa GKey
#define LGI_VKEY_CTRL				0x001
/// The ALT key is pressed
/// \sa GKey
#define LGI_VKEY_ALT				0x002
/// The SHIFT key is pressed
/// \sa GKey
#define LGI_VKEY_SHIFT				0x004

/// The left mouse button is pressed
/// \sa GMouse
#define LGI_VMOUSE_LEFT				0x008
/// The middle mouse button is pressed
/// \sa GMouse
#define LGI_VMOUSE_MIDDLE			0x010
/// The right mouse button is pressed
/// \sa GMouse
#define LGI_VMOUSE_RIGHT			0x020
/// The ctrl key is pressed
/// \sa GMouse
#define LGI_VMOUSE_CTRL				0x040
/// The alt key is pressed
/// \sa GMouse
#define LGI_VMOUSE_ALT				0x080
/// The shift key is pressed
/// \sa GMouse
#define LGI_VMOUSE_SHIFT			0x100
/// The mouse event is a down click
/// \sa GMouse
#define LGI_VMOUSE_DOWN				0x200
/// The mouse event is a double click
/// \sa GMouse
#define LGI_VMOUSE_DOUBLE			0x400


#define abs(a)						( (a) < 0 ? -(a) : (a) )

#ifndef WIN32
typedef enum {
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	VK_UNKNOWN		= 0,
	VK_FIRST		= 0,
	VK_BACKSPACE	= 8,
	VK_TAB			= 9,
	VK_CLEAR		= 12,
	VK_RETURN		= 13,
	VK_PAUSE		= 19,
	VK_ESCAPE		= 0xff1b,
	VK_SPACE		= 32,
	VK_EXCLAIM		= 33,
	VK_QUOTEDBL		= 34,
	VK_HASH			= 35,
	VK_DOLLAR		= 36,
	VK_AMPERSAND	= 38,
	VK_QUOTE		= 39,
	VK_LEFTPAREN	= 40,
	VK_RIGHTPAREN	= 41,
	VK_ASTERISK		= 42,
	VK_PLUS			= 43,
	VK_COMMA		= 44,
	VK_MINUS		= 45,
	VK_PERIOD		= 46,
	VK_SLASH		= 47,
	VK_0			= 48,
	VK_1			= 49,
	VK_2			= 50,
	VK_3			= 51,
	VK_4			= 52,
	VK_5			= 53,
	VK_6			= 54,
	VK_7			= 55,
	VK_8			= 56,
	VK_9			= 57,
	VK_COLON		= 58,
	VK_SEMICOLON	= 59,
	VK_LESS			= 60,
	VK_EQUALS		= 61,
	VK_GREATER		= 62,
	VK_QUESTION		= 63,
	VK_AT			= 64,
	/* 
	   Skip uppercase letters
	 */
	VK_LEFTBRACKET	= 91,
	VK_BACKSLASH	= 92,
	VK_RIGHTBRACKET	= 93,
	VK_CARET		= 94,
	VK_UNDERSCORE	= 95,
	VK_BACKQUOTE	= 96,
	VK_a			= 97,
	VK_b			= 98,
	VK_c			= 99,
	VK_d			= 100,
	VK_e			= 101,
	VK_f			= 102,
	VK_g			= 103,
	VK_h			= 104,
	VK_i			= 105,
	VK_j			= 106,
	VK_k			= 107,
	VK_l			= 108,
	VK_m			= 109,
	VK_n			= 110,
	VK_o			= 111,
	VK_p			= 112,
	VK_q			= 113,
	VK_r			= 114,
	VK_s			= 115,
	VK_t			= 116,
	VK_u			= 117,
	VK_v			= 118,
	VK_w			= 119,
	VK_x			= 120,
	VK_y			= 121,
	VK_z			= 122,
	/* End of ASCII mapped keysyms */

	/* Numeric keypad */
	VK_KP_ENTER		= 0xff8d,
	VK_KP7			= 0xff95,
	VK_KP4			= 0xff96,
	VK_KP8			= 0xff97,
	VK_KP6			= 0xff98,
	VK_KP2			= 0xff99,
	VK_KP9			= 0xff9a,
	VK_KP3			= 0xff9b,
	VK_KP1			= 0xff9c,
	VK_KP5			= 0xff9d,
	VK_KP0			= 0xff9e,
	VK_KP_PERIOD	= 0xff9f,
	VK_KP_MULTIPLY	= 0xffaa,
	VK_KP_PLUS		= 0xffab,
	VK_KP_MINUS		= 0xffad,
	VK_KP_DIVIDE	= 0xffaf,
	VK_KP_EQUALS	= 0xffbd,

	/* Arrows + Home/End pad */
	VK_HOME			= 0xff50,
	VK_LEFT			= 0xff51,
	VK_UP			= 0xff52,
	VK_RIGHT		= 0xff53,
	VK_DOWN			= 0xff54,
	VK_PAGEUP		= 0xff55,
	VK_PAGEDOWN		= 0xff56,
	VK_END			= 0xff57,
	VK_INSERT		= 0xff63,

	/* Function keys */
	VK_F1			= 0xffbe,
	VK_F2,
	VK_F3,
	VK_F4,
	VK_F5,
	VK_F6,
	VK_F7,
	VK_F8,
	VK_F9,
	VK_F10,
	VK_F11,
	VK_F12,
	VK_F13,
	VK_F14,
	VK_F15,

	/* Key state modifier keys */
	VK_NUMLOCK		= 0xff7f,
	VK_CAPSLOCK		= 0xffe5,
	VK_SCROLLOCK	= 0xff14,
	VK_LSHIFT		= 0xffe1,
	VK_RSHIFT,
	VK_LCTRL		= 0xffe3,
	VK_RCTRL,
	VK_LALT			= 0xffe9,
	VK_RALT,
	VK_LMETA		= 0xffed,
	VK_RMETA,
	VK_LSUPER		= 0xffeb, /* "Windows" key */
	VK_RSUPER,

	/* Miscellaneous function keys */
	VK_HELP			= 0xff6a,
	VK_PRINT		= 0xff61,
	VK_SYSREQ		= 0xff15,
	VK_BREAK		= 0xff6b,
	VK_MENU			= 0xff67,
	VK_UNDO			= 0xff65,
	VK_REDO,
	VK_EURO			= 0x20ac, /* Some european keyboards */
	VK_COMPOSE		= 0xff20, /* Multi-key compose key */
	VK_MODE			= 0xff7e, /* "Alt Gr" key (could be wrong) */
	VK_DELETE		= 0xffff,
	VK_POWER		= 0x10000,	/* Power Macintosh power key */

	/* Add any other keys here */
	VK_LAST
} LgiKey;
#else
#define VK_BACKSPACE    VK_BACK
#define VK_PAGEUP       VK_PRIOR
#define VK_PAGEDOWN     VK_NEXT
#define VK_RALT         VK_MENU
#define VK_LALT         VK_MENU
#define VK_RCTRL        VK_CONTROL
#define VK_LCTRL        VK_CONTROL
#endif

/////////////////////////////////////////////////////////////////////////////////////
// Externs
#define vsprintf_s		vsnprintf
#define swprintf_s		swprintf
#ifndef WIN32 // __CYGWIN__
// LgiFunc char *strnistr(char *a, char *b, int n);
#define _strnicmp strncasecmp // LgiFunc int _strnicmp(char *a, char *b, int i);
LgiFunc char *strupr(char *a);
LgiFunc char *strlwr(char *a);
LgiFunc int stricmp(const char *a, const char *b);
#define _stricmp strcasecmp // LgiFunc int _stricmp(const char *a, const char *b);
#define sprintf_s snprintf
#else
LgiFunc class GViewI *GWindowFromHandle(OsView hWnd);
LgiFunc int GetMouseWheelLines();
#ifdef WINNATIVE
LgiFunc int WinPointToHeight(int Pt, HDC hDC = NULL);
LgiFunc int WinHeightToPoint(int Ht, HDC hDC = NULL);
#endif

typedef BOOL (__stdcall *pSHGetSpecialFolderPathA)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);
typedef BOOL (__stdcall *pSHGetSpecialFolderPathW)(HWND hwndOwner, LPWSTR lpszPath, int nFolder, BOOL fCreate);
typedef int (__stdcall *pSHFileOperationA)(LPSHFILEOPSTRUCTA lpFileOp);
typedef int (__stdcall *pSHFileOperationW)(LPSHFILEOPSTRUCTW lpFileOp);
LgiExtern class GString WinGetSpecialFolderPath(int Id);

typedef int (__stdcall *p_vscprintf)(const char *format, va_list argptr);

#if _MSC_VER >= 1400
#define snprintf sprintf_s
#endif

#endif

/// Convert a string d'n'd format to an OS dependant integer.
LgiFunc int FormatToInt(char *s);
/// Convert a Os dependant integer d'n'd format to a string.
LgiFunc char *FormatToStr(int f);


#endif

