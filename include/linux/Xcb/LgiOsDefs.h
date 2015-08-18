/**
	\file
	\author Matthew Allen
	\brief Linux specific types and defines.
 */

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#include <string.h>
#include "assert.h"
#include "LgiDefs.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#define _MULTI_THREADED
#include <pthread.h>

#undef stricmp

#include "LgiInc.h"
#include "xcb/xcb.h"

namespace Pango
{
#include "pango/pango.h"
#include "pango/pangocairo.h"
#include "cairo/cairo-xcb.h"
}

// System
#define LINUX						1
#define XP_CTRLS					1
#define POSIX						1

#undef Status
#undef Success
#undef None
#undef Above
#undef Below

#define XStatus		int
#define XSuccess	0
#define XAbove		0
#define XBelow		1
#define XNone		0L

class OsAppArguments
{
	struct OsAppArgumentsPriv *d;

public:
	int Args;
	char **Arg;

	OsAppArguments(int args, char **arg);
	~OsAppArguments();

	void Set(char *CmdLine);
	OsAppArguments &operator =(OsAppArguments &a);
};

// Process
typedef int								OsProcess;
typedef int								OsProcessId;
typedef xcb_window_t					OsView;
typedef xcb_window_t					OsWindow;
typedef char							OsChar;
typedef xcb_gcontext_t					OsPainter;
typedef Pango::PangoFontDescription*	OsFont;
typedef xcb_pixmap_t					OsBitmap;

class OsApplication
{
	class OsApplicationPriv *d;

protected:
	static OsApplication *Inst;
	xcb_connection_t *c;
	xcb_screen_t *s;
	
public:
	OsApplication(char Args, char **Arg);
	~OsApplication();
	
	static OsApplication *GetInst() { LgiAssert(Inst); return Inst; }
	xcb_connection_t *GetConn() { return c; };
	xcb_screen_t *GetScreen() { return s; }
	bool Check(xcb_void_cookie_t c, char *file, int line);
};

#define XcbConn()					(OsApplication::GetInst()->GetConn())
#define XcbScreen()					(OsApplication::GetInst()->GetScreen())
#define XcbCheck(cookie)			(OsApplication::GetInst()->Check(cookie, __FILE__, __LINE__))
#ifdef _DEBUG
#define XcbDebug(cookie)			(OsApplication::GetInst()->Check(cookie, __FILE__, __LINE__))
#else
#define XcbDebug(cookie)			cookie
#endif

class LgiClass GMessage
{
	xcb_generic_event_t *e;

public:
    typedef int Param;

	GMessage(xcb_generic_event_t *ev = 0);
	GMessage(int m, Param a = 0, Param b = 0);
	~GMessage();

	int ResponseType() { return e ? e->response_type & ~0x80 : 0; }
	int Type();
	Param A();
	Param B();
	void Set(int m, Param a, Param b);
	xcb_generic_event_t *GetEvent() { return e; }
	void SetEvent(xcb_generic_event_t *e);
	bool Send(OsView Wnd);

	#define DefCast(Obj, Id) \
		operator Obj*() \
		{ \
			if (ResponseType() == Id) \
				return (Obj*)e; \
			LgiAssert(!"Not a valid cast."); \
			return 0; \
		}
		
	DefCast(xcb_key_press_event_t, XCB_KEY_PRESS)
	DefCast(xcb_key_release_event_t, XCB_KEY_RELEASE)
	DefCast(xcb_button_press_event_t, XCB_BUTTON_PRESS)
	DefCast(xcb_button_release_event_t, XCB_BUTTON_RELEASE)
	DefCast(xcb_motion_notify_event_t, XCB_MOTION_NOTIFY)
	DefCast(xcb_enter_notify_event_t, XCB_ENTER_NOTIFY)
	DefCast(xcb_leave_notify_event_t, XCB_LEAVE_NOTIFY)
	DefCast(xcb_focus_in_event_t, XCB_FOCUS_IN)
	DefCast(xcb_focus_out_event_t, XCB_FOCUS_OUT)
	DefCast(xcb_keymap_notify_event_t, XCB_KEYMAP_NOTIFY)
	DefCast(xcb_expose_event_t, XCB_EXPOSE)
	DefCast(xcb_graphics_exposure_event_t, XCB_GRAPHICS_EXPOSURE)
	DefCast(xcb_no_exposure_event_t, XCB_NO_EXPOSURE)
	DefCast(xcb_visibility_notify_event_t, XCB_VISIBILITY_NOTIFY)
	DefCast(xcb_create_notify_event_t, XCB_CREATE_NOTIFY)
	DefCast(xcb_unmap_notify_event_t, XCB_UNMAP_NOTIFY)
	DefCast(xcb_map_notify_event_t, XCB_MAP_NOTIFY)
	DefCast(xcb_map_request_event_t, XCB_MAP_REQUEST)
	DefCast(xcb_reparent_notify_event_t, XCB_REPARENT_NOTIFY)
	DefCast(xcb_configure_notify_event_t, XCB_CONFIGURE_NOTIFY)
	DefCast(xcb_configure_request_event_t, XCB_CONFIGURE_REQUEST)
	DefCast(xcb_gravity_notify_event_t, XCB_GRAVITY_NOTIFY)
	DefCast(xcb_circulate_notify_event_t, XCB_CIRCULATE_NOTIFY)
	DefCast(xcb_property_notify_event_t, XCB_PROPERTY_NOTIFY)
	DefCast(xcb_selection_request_event_t, XCB_SELECTION_REQUEST)
	DefCast(xcb_selection_notify_event_t, XCB_SELECTION_NOTIFY)
	DefCast(xcb_colormap_notify_event_t, XCB_COLORMAP_NOTIFY)
	DefCast(xcb_client_message_event_t, XCB_CLIENT_MESSAGE)
	DefCast(xcb_mapping_notify_event_t, XCB_MAPPING_NOTIFY)
};

class XcbAtom
{
	xcb_atom_t Int;
	char *Str;

	xcb_intern_atom_cookie_t ic;
	xcb_get_atom_name_cookie_t sc;

	void SetStr(char *s, int len)
	{
		if (Str = new char[len+1])
		{
			memcpy(Str, s, len);
			Str[len] = 0;
		}
	}
	
public:
	XcbAtom(char *name, bool only_if_exists = false)
	{
		Int = 0;
		SetStr(name, strlen(name));
		ic = xcb_intern_atom (XcbConn(), only_if_exists, strlen(name), name);
	}
	
	XcbAtom(xcb_atom_t atom)
	{
		Int = atom;
		Str = 0;
		sc = xcb_get_atom_name(XcbConn(), atom);
	}
	
	~XcbAtom()
	{
		DeleteArray(Str);
	}
	
	char *Name()
	{
		if (!Str)
		{
			xcb_get_atom_name_reply_t *r =
				xcb_get_atom_name_reply(XcbConn(), sc, 0);
			if (r)
			{
				SetStr((char*)xcb_get_atom_name_name(r), xcb_get_atom_name_name_length(r));
				free(r);
			}
		}
	
		return Str;
	}
	
	xcb_atom_t Atom()
	{
		if (!Int)
		{
			xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(XcbConn(), ic, 0);
			if (r)
			{
				Int = r->atom;
				free(r);
			}
		}

		return Int;
	}
};

#define MsgCode(Msg)				Msg->Type()
#define MsgA(Msg)					Msg->A()
#define MsgB(Msg)					Msg->B()

// Threads
typedef pthread_t					OsThreadId;
typedef pthread_mutex_t				OsSemaphore;
#define LgiGetCurrentThread()		pthread_self()

// Sockets
#define ValidSocket(s)				((s)>=0)
#define INVALID_SOCKET				-1
typedef int OsSocket;

/// Sleep the current thread for i milliseconds.
LgiFunc void LgiSleep(uint32 i);

#define atoi64						atoll
#define _snprintf					snprintf
#define _vsnprintf					vsnprintf

/// Process any pending messages in the applications message que and then return.
#define LgiYield()					LgiApp->Run(false)

#define K_CHAR						0x0

/// Drag and drop format for a file
#define LGI_FileDropFormat			"text/uri-list"
#define LGI_IllegalFileNameChars	"\t\r\n/\\:*?\"<>|"
#define LGI_WideCharset				"utf-32"
#define LGI_PrintfInt64				"%Li"

#define SND_ASYNC					1

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
#define LGI_LIBRARY_EXT				"so"

/// Base point for system messages.
#define M_SYSTEM					(1000)
/// Message that indicates the user is trying to close a top level window.
#define M_CLOSE						(M_SYSTEM+1)
/// Implemented to handle invalide requests in the GUI thread.
#define M_X11_INVALIDATE			(M_SYSTEM+2)
/// Implemented to handle paint requests in the GUI thread.
#define M_X11_REPARENT				(M_SYSTEM+4)

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
#define M_DELETE					(M_USER+112)
#define M_GTHREADWORK_COMPELTE		(M_USER+113)
/// Implemented to handle timer events in the GUI thread.
#define M_PULSE						(M_USER+114)

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

typedef enum {
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	VK_UNKNOWN		= 0,
	VK_FIRST		= 0,
	VK_BACKSPACE	= 0xff08,
	VK_TAB			= 0xff09,
	VK_CLEAR		= 12,
	VK_RETURN		= 0xff0d,
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


/////////////////////////////////////////////////////////////////////////////////////
// Externs
#ifndef __CYGWIN__
LgiFunc char *strnistr(char *a, char *b, int n);
LgiFunc int strnicmp(char *a, char *b, int i);
LgiFunc char *strupr(char *a);
LgiFunc char *strlwr(char *a);
LgiFunc int stricmp(char *a, char *b);
#endif

LgiFunc int stricmp(char *a, char *b);

/// Convert a string d'n'd format to an OS dependant integer.
LgiFunc int FormatToInt(char *s);
/// Convert a Os dependant integer d'n'd format to a string.
LgiFunc char *FormatToStr(int f);

#endif

