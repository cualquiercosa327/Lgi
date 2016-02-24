#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#undef min
#undef max

#include <AppKit.h>
#include <InterfaceKit.h>
#include <GameKit.h>
#include <Path.h>

#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "LgiInc.h"

#define min(a, b) ((a)<(b)?(a):(b))
#define max(a, b) ((a)>(b)?(a):(b))


//////////////////////////////////////////////////////////////////
// Typedefs
typedef	signed long long int		int64;
typedef unsigned long long int		uint64;

typedef BWindow						*OsWindow;
typedef BView						*OsView;
typedef BBitmap						*OsBitmap;
typedef thread_id					OsThread;
typedef team_id						OsProcess;
typedef char						OsChar;
typedef BView						*OsPainter;
typedef BFont						*OsFont;
typedef int							OsProcessId;

class LgiClass OsAppArguments
{
public:
	int Args;
	const char **Arg;

	OsAppArguments(int args, const char **arg)
	{
		Args = args;
		Arg = arg;
	}
};

class LgiClass GMessage : public BMessage
{
public:
	typedef int32 Param;
	typedef int32 Result;
	
	GMessage() {}
	GMessage(int m, Param a, Param b)
	{
		what = m;
		AddInt32("Lgi.a", a);
		AddInt32("Lgi.b", b);
	}
	
	Param Msg() 
	{
		return what;
	}

	Param A()
	{
		return GetInt32("Lgi.a", (int32)-1);
	}

	Param B()
	{
		return GetInt32("Lgi.b", (int32)-1);
	}
};

/////////////////////////////////////////////////////////////////
// Defines
#define _DEBUG					1
#define XP_CTRLS				1

#ifndef BEOS
#error "BEOS must be defined in your project, use -DBEOS."
#endif
#define MAKEINTRESOURCE(i)		((char*)(i))

// Threads
typedef thread_id					OsThreadId;
typedef sem_id						OsSemaphore;
#define LgiGetCurrentThread()		find_thread(0)

// assert
#ifdef _DEBUG
extern void _lgi_assert(bool b, char *test, char *file, int line);
#define LgiAssert(b)			_lgi_assert(b, #b, __FILE__, __LINE__)
#else
#define LgiAssert(b)			
#endif

#define LgiSleep(i)				snooze(i*1000)
#define LgiYield()				LgiApp->Run(false)

// Network
// #include "NetworkKit.h"
#define ValidSocket(s)			((s)>=0)
#define INVALID_SOCKET			-1
typedef int OsSocket;

#define K_CHAR					0x0
#define SND_ASYNC 1
#define DOUBLE_CLICK_THRESHOLD	5
#define LGI_FileDropFormat		"Something?"
#define LGI_WideCharset			"utf-32"
#define LGI_LIBRARY_EXT			"so"
#define LGI_PrintfInt64			"%"B_PRIi64
#define LGI_PrintfHex64			"%"B_PRIx64
#define LGI_EXECUTABLE_EXT		""
#define LGI_IllegalFileNameChars	"\t\r\n/\\:*?\"<>|"


#define IDOK					1
#define IDCANCEL				2
#define IDYES					3
#define IDNO					4

#define MB_OK					5
#define MB_OKCANCEL				6
#define MB_YESNO				7
#define MB_YESNOCANCEL			8

#define MB_SYSTEMMODAL			0x1000

#define MK_LEFT					B_PRIMARY_MOUSE_BUTTON
#define MK_RIGHT				B_SECONDARY_MOUSE_BUTTON
#define MK_MIDDLE				B_TERTIARY_MOUSE_BUTTON
#define MK_CTRL					0x08
#define MK_ALT					0x10
#define MK_SHIFT				0x20

// Window flags
#define GWF_VISIBLE				0x00000001
#define GWF_ENABLED				0x00000002
#define GWF_FOCUS				0x00000004
#define GWF_OVER				0x00000008
#define GWF_DROP_TARGET			0x00000010
#define GWF_SUNKEN				0x00000020
#define GWF_FLAT				0x00000040
#define GWF_RAISED				0x00000080
#define GWF_BORDER				0x00000100
#define GWF_DIALOG				0x00000200
#define GWF_DESTRUCTOR			0x00000400
#define GWF_QUIT_WND			0x00000800
#define GWF_DISABLED			0x00001000

// Edge types
#define SUNKEN					1
#define RAISED					2
#define CHISEL					3
#define FLAT					4

// Widgets
#define CTRL_SUNKEN				0x80000000
#define CTRL_FLAT				0x40000000
#define CTRL_RAISED				0x20000000

// Directories
#define DIR_CHAR				'/'
#define DIR_STR					"/"
#define EOL_SEQUENCE			"\n"
#define LGI_PATH_SEPARATOR		":"

#define IsSlash(c)				(((c)=='/')||((c)=='\\'))
#define IsQuote(c)				(((c)=='\"')||((c)=='\''))

#define LGI_ALL_FILES			"*"

// Messages
#define M_USER					(0xF0000000) // how strange, this isn't defined in BeOS?? :P

// wParam = bool Inside; // is the mouse inside the client area?
// lParam = MAKELONG(x, y); // mouse location
#define M_MOUSEENTER			(M_USER+0x100)
#define M_MOUSEEXIT				(M_USER+0x101)

// wParam = (GView*) Wnd;
// lParam = (int) Flags;
#define M_CHANGE				(M_USER+0x102)

// wParam = (GView*) Wnd;
// lParam = (char*) Text; // description from window
#define M_DESCRIBE				(M_USER+0x103)

// return (bool)
#define M_WANT_DIALOG_PROC		(M_USER+0x104)

#define M_MENU					(M_USER+0x105)
#define M_COMMAND				(M_USER+0x106)
#define M_DRAG_DROP				(M_USER+0x107)
#define M_VSCROLL				(M_USER+0x108)
#define M_HSCROLL				(M_USER+0x109)
#define M_PULSE					(M_USER+0x10a)
#define M_CLOSE					(M_USER+0x10b)
#define M_CUT					B_CUT
#define M_COPY					B_COPY
#define M_PASTE					B_PASTE

// Mouse msgs
#define LGI_MOUSE_CLICK			(M_USER+0x10b)		// BPoint pos;
													// int32 flags;
#define LGI_MOUSE_MOVE			(M_USER+0x10c)		// BPoint pos;
													// int32 flags;
#define LGI_MOUSE_ENTER			(M_USER+0x10d)
#define LGI_MOUSE_EXIT			(M_USER+0x10e)
#define M_SET_VISIBLE			(M_USER+0x10f)
#define M_TEXT_UPDATE_NAME		(M_USER+0x110)

// Dialog stuff
#define IDOK					1
#define IDCANCEL				2

// Keys
#define VK_DELETE				B_DELETE
#define VK_SHIFT				B_SHIFT_KEY
#define VK_ESCAPE				B_ESCAPE
#define VK_RETURN				B_ENTER
#define VK_BACKSPACE			B_BACKSPACE
#define VK_RIGHT				B_RIGHT_ARROW
#define VK_LEFT					B_LEFT_ARROW
#define VK_UP					B_UP_ARROW
#define VK_DOWN					B_DOWN_ARROW
#define VK_PAGEUP				B_PAGE_UP
#define VK_PAGEDOWN				B_PAGE_DOWN
#define VK_HOME					B_HOME
#define VK_END					B_END
#define VK_INSERT				B_INSERT
#define VK_TAB					B_TAB

// GKey flags
#define LGI_VKEY_CTRL			B_CONTROL_KEY	// 0x0004
#define LGI_VKEY_ALT			B_OPTION_KEY	// 0x0040
#define LGI_VKEY_SHIFT			B_SHIFT_KEY		// 0x0001

// GMouse flags
#define LGI_VMOUSE_LEFT			B_PRIMARY_MOUSE_BUTTON		// 0x0001
#define LGI_VMOUSE_MIDDLE		B_TERTIARY_MOUSE_BUTTON		// 0x0004
#define LGI_VMOUSE_RIGHT		B_SECONDARY_MOUSE_BUTTON	// 0x0002

#define LGI_VMOUSE_CTRL			0x08
#define LGI_VMOUSE_ALT			0x10
#define LGI_VMOUSE_SHIFT		0x20

#define LGI_VMOUSE_DOWN			0x40
#define LGI_VMOUSE_DOUBLE		0x80

// The BeOS defines actually overlap some other keys
// so I don't use them, instead I just defined my own block
// of codes
#define VK_F1					0x11
#define VK_F2					0x12
#define VK_F3					0x13
#define VK_F4					0x14
#define VK_F5					0x15
#define VK_F6					0x16
#define VK_F7					0x17
#define VK_F8					0x18
#define VK_F9					0x19
#define VK_F10					0x01
#define VK_F11					0x02
#define VK_F12					0x03

/////////////////////////////////////////////////////////////////////////////////////
#define MsgCode(m) m->what
extern GMessage::Param MsgA(GMessage *m);
extern GMessage::Param MsgB(GMessage *m);
extern GMessage CreateMsg(int m, int a, int b);

/////////////////////////////////////////////////////////////////////////////////////
// BeOS specific
#define LGI_DRAW_VIEW_FLAGS		(B_POINTER_EVENTS | B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE)
#define LGI_NODRAW_VIEW_FLAGS	(B_POINTER_EVENTS | B_NAVIGABLE | B_FRAME_EVENTS)

extern int stricmp(char *a, char *b);
#define sprintf_s				snprintf
#define stricmp					strcasecmp
#define _stricmp				strcasecmp
#define _strnicmp				strncasecmp
#define atoi64					atoll
#define vsprintf_s				vsnprintf

#endif
