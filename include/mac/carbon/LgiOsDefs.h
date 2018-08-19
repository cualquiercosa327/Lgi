//
//      FILE:           LgiOsDefs.h (Mac)
//      AUTHOR:         Matthew Allen
//      DATE:           1/12/2005
//      DESCRIPTION:    Lgi Mac OS X defines
//
//      Copyright (C) 2005, Matthew Allen
//              fret@memecode.com
//

#ifndef __LGI_MAC_OS_DEFS_H
#define __LGI_MAC_OS_DEFS_H

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <Carbon/Carbon.h>
#include <wchar.h>
#include "LgiInc.h"
#include "LgiDefs.h"
#include "GAutoPtr.h"
#include "LgiClass.h"
#include "pthread.h"

//////////////////////////////////////////////////////////////////
// Includes
#include "LgiInc.h"

/// This turns on the Core Text implementation.
/// If '0' the old ATSUI implementation is used.
#define USE_CORETEXT		1

#ifndef CARBON
#define CARBON				1
#endif

#define LGI_32BIT			1

//////////////////////////////////////////////////////////////////
// Typedefs
typedef WindowRef			OsWindow;
typedef ControlRef			OsView;
typedef pthread_t           OsThread;
typedef UniChar				OsChar;
typedef CGContextRef		OsPainter;
typedef CGContextRef		OsBitmap;
typedef int					OsProcessId;

#if USE_CORETEXT
typedef CTFontRef			OsFont;
#else
typedef ATSUStyle			OsFont;
#endif

#include "GMessage.h"

class OsAppArguments
{
	struct OsAppArgumentsPriv *d;

public:
	int Args;
	const char **Arg;

	OsAppArguments(int args = 0, const char **arg = 0);
	~OsAppArguments();

	void Set(char *CmdLine);
	OsAppArguments &operator =(OsAppArguments &a);
};

//////////////////////////////////////////////////////////////////
// Defines
#define _stricmp					strcasecmp
#define _strnicmp					strncasecmp

// System defines
#define POSIX						1
#define LGI_CARBON					1

// Process
typedef int							OsProcess;
typedef int							OsProcessId;
#define LgiGetCurrentProcess()		getpid()

// Threads
typedef uint64_t					OsThreadId;
typedef pthread_mutex_t				OsSemaphore;
#define LgiGetCurrentThread()		pthread_self()
LgiFunc OsThreadId					GetCurrentThreadId();

// Sockets
#define ValidSocket(s)				((s)>=0)
#define INVALID_SOCKET				-1
typedef int OsSocket;

// Sleep the current thread
LgiFunc void LgiSleep(uint32 i);

// Run the message loop to process any pending messages
#define LgiYield()					GApp::ObjInstance()->Run(false)

#define LGI_GViewMagic				0x14412662
#define LGI_FileDropFormat			"public.file-url" // typeFileURL
#define LGI_StreamDropFormat		"phfs" // kDragFlavorTypePromiseHFS
#define LGI_LgiDropFormat			"lgi "
#define LGI_WideCharset				"utf-32"
#define LGI_PrintfInt64				"%lli"
#define LGI_PrintfHex64				"%llx"
#define atoi64						atoll
#define sprintf_s					snprintf
#define vsprintf_s					vsnprintf
#define swprintf_s					swprintf
#define wcscpy_s(dst, len, src)		wcsncpy(dst, src, len)
#define LGI_IllegalFileNameChars	"/:" // FIXME: what other characters should be in here?

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
#define ODS_SELECTED				0x1
#define ODS_DISABLED				0x2
#define ODS_CHECKED					0x4

/// Edge type: Sunken
#define SUNKEN						1
/// Edge type: Raised
#define RAISED						2
/// Edge type: Chiseled
#define CHISEL						3
/// Edge type: Flat
#define FLAT						4

/// The directory separator character on Linux as a char
#define DIR_CHAR					'/'
/// The directory separator character on Linux as a string
#define DIR_STR						"/"
/// The standard end of line string for Linux
#define EOL_SEQUENCE				"\n"
/// Tests a char for being a slash
#define IsSlash(c)					(((c)=='/')||((c)=='\\'))
/// Tests a char for being a quote
#define IsQuote(c)					(((c)=='\"')||((c)=='\''))
/// The path list separator character for Linux
#define LGI_PATH_SEPARATOR			":"
/// The pattern that matches all files in Linux
#define LGI_ALL_FILES				"*"
/// The stardard extension for dynamically linked code
#define LGI_LIBRARY_EXT				"dylib"
/// The standard executable extension
#define LGI_EXECUTABLE_EXT			""

// Carbon user events
#define GViewThisPtr				'gvtp'
#define kEventClassUser				'User'
#define kEventUser					1
#define kEventParamLgiEvent			'Lgie'
#define kEventParamLgiA				'Lgia'
#define kEventParamLgiB				'Lgib'

/// ID's returned by LgiMsg.
/// \sa LgiMsg
enum MessageBoxResponse
{
	IDOK = 1,
	IDCANCEL = 2,
	IDABORT = 3,
	IDRETRY = 4,
	IDIGNORE = 5,
	IDYES = 6,
	IDNO = 7,
	IDTRYAGAIN = 10,
	IDCONTINUE = 11,
};

/// Standard message box types.
/// \sa LgiMsg
enum MessageBoxType
{
	MB_OK = 0,
	MB_OKCANCEL = 1,
	MB_ABORTRETRYIGNORE = 2,
	MB_YESNOCANCEL = 3,
	MB_YESNO = 4,
	MB_RETRYCANCEL = 5,
	MB_CANCELTRYCONTINUE = 6
};

#define MB_SYSTEMMODAL				0x1000

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

// Keys
#define VK_F1						1
#define VK_F2						2
#define VK_ENTER					3
#define VK_F3						4
#define VK_F4						5
#define VK_F5						6
#define VK_F6						7
#define VK_BACKSPACE				8
#define VK_TAB						9
#define VK_F7						11
#define VK_F8						12
#define VK_RETURN					13
#define VK_F9						14
#define VK_F10						15
#define VK_F11						16
#define VK_F12						17
#define VK_SHIFT					18
#define VK_PAGEUP					19
#define VK_PAGEDOWN					20
#define VK_HOME						21
#define VK_END						22
#define VK_INSERT					23
#define VK_DELETE					24
#define VK_APPS						25

#define VK_ESCAPE					27
#define VK_LEFT						28
#define VK_RIGHT					29
#define VK_UP						30
#define VK_DOWN						31


/////////////////////////////////////////////////////////////////////////////////////
// Externs
LgiFunc GView *GWindowFromHandle(OsView hWnd);
LgiFunc int GetMouseWheelLines();
LgiFunc int WinPointToHeight(int Pt);
LgiFunc int WinHeightToPoint(int Ht);
LgiFunc int stricmp(const char *a, const char *b);
LgiFunc char *strlwr(char *a);
LgiFunc char *strupr(char *a);
LgiFunc char *p2c(unsigned char *s);
LgiFunc void c2p255(Str255 &d, char *s);
LgiFunc char *CFStringToUtf8(CFStringRef r);
LgiFunc CFStringRef Utf8ToCFString(char *s, int len = -1);

/// Convert a string d'n'd format to an OS dependant integer.
LgiFunc int FormatToInt(char *s);
/// Convert a Os dependant integer d'n'd format to a string.
LgiFunc char *FormatToStr(int f);


#endif
