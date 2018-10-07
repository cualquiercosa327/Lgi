//
//      FILE:           LgiOsDefs.h (Mac)
//      AUTHOR:         Matthew Allen
//      DATE:           1/1/2013
//      DESCRIPTION:    Lgi cocoa header
//
//      Copyright (C) 2013, Matthew Allen
//              fret@memecode.com
//

#ifndef __LGI_MAC_OS_DEFS_H
#define __LGI_MAC_OS_DEFS_H

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "LgiInc.h"
#include "LgiDefs.h"
#include "GAutoPtr.h"
#include "LgiClass.h"
#include "pthread.h"

//////////////////////////////////////////////////////////////////
// Includes
#include "LgiInc.h"

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

//////////////////////////////////////////////////////////////////
// Typedefs
typedef struct _OsWindow
{
	#ifdef __OBJC__
	NSWindow *w;
	#else
	NativeInt unused;
	#endif
}	*OsWindow;

typedef struct _OsView
{
	#ifdef __OBJC__
	NSView *v;
	#else
	NativeInt unused;
	#endif
}	*OsView;

typedef pthread_t           OsThread;
typedef uint64_t			OsThreadId;
typedef pthread_mutex_t		OsSemaphore;
typedef uint16				OsChar;
typedef int					OsProcessId;
typedef int					OsProcess;
typedef CGContextRef		OsPainter;
typedef CGContextRef		OsBitmap;
typedef CTFontRef			OsFont;

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

// Text system
#define USE_CORETEXT				1

// System defines
#define POSIX						1
#define LGI_COCOA					1
#define LGI_64BIT					1

// Process
typedef int							OsProcess;
typedef int							OsProcessId;
#define LgiGetCurrentProcess()		getpid()

// Threads
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
#define LGI_FileDropFormat			"furl" // typeFileURL
#define LGI_StreamDropFormat		"" // kPasteboardTypeFileURLPromise
#define LGI_LgiDropFormat			"lgi "
#define LGI_WideCharset				"utf-32"
#define LPrintfInt64				"%lli"
#define LPrintfHex64				"%llx"
#define LPrintfSizeT				"%zu"
#define LPrintfSSizeT				"%zi"
#define atoi64						atoll
#define sprintf_s					snprintf
#define vsprintf_s					vsnprintf
#define LGI_IllegalFileNameChars	"/" // FIXME: what other characters should be in here?
#define LGI_EXECUTABLE_EXT			// Empty

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

// Carbon user events
#define GViewThisPtr				'gvtp'
#define kEventClassUser				'user'
#define kEventUser					1
#define kEventParamLgiEvent			'Lgie'
#define kEventParamLgiA				'Lgia'
#define kEventParamLgiB				'Lgib'
/// Base point for system messages.
#define M_SYSTEM					0
/// Message that indicates the user is trying to close a top level window.
#define M_CLOSE						(M_SYSTEM+92)

/// Minimum value for application defined message ID's
#define M_USER						(M_SYSTEM+1000)

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
#define M_PULSE						(M_USER+112)
#define M_DELETE					(M_USER+113)
#define M_SET_VISIBLE				(M_USER+114)
#define M_TEXT_UPDATE_NAME			(M_USER+115)

/// GThreadWork object completed
///
/// MsgA = (GThreadOwner*) Owner;
/// MsgB = (GThreadWork*) WorkUnit;
#define M_GTHREADWORK_COMPELTE		(M_USER+114)

/// Standard ID for an "Ok" button.
/// \sa LgiMsg
#define IDOK						1
/// Standard ID for a "Cancel" button.
/// \sa LgiMsg
#define IDCANCEL					2
/// Standard ID for a "Yes" button.
/// \sa LgiMsg
#define IDYES						3
/// Standard ID for a "No" button.
/// \sa LgiMsg
#define IDNO						4

/// Standard message box with an Ok button.
/// \sa LgiMsg
#define MB_OK						5
/// Standard message box with Ok and Cancel buttons.
/// \sa LgiMsg
#define MB_OKCANCEL					6
/// Standard message box with Yes and No buttons.
/// \sa LgiMsg
#define MB_YESNO					7
/// Standard message box with Yes, No and Cancel buttons.
/// \sa LgiMsg
#define MB_YESNOCANCEL				8

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

/// Convert a string d'n'd format to an OS dependant integer.
LgiFunc int FormatToInt(char *s);
/// Convert a Os dependant integer d'n'd format to a string.
LgiFunc char *FormatToStr(int f);


#endif
