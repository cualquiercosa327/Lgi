/**
	\file
	\author Matthew Allen
	\brief Linux specific types and defines.
 */

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#include "assert.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#define _MULTI_THREADED
#include <pthread.h>

#undef stricmp

#include "LgiInc.h"

// System
#define LINUX						1
#define XP_CTRLS					1
#define POSIX						1

typedef enum {
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	VK_UNKNOWN		= 0,
	VK_FIRST		= 0,
	VK_BACKSPACE	= 8,
	VK_TAB			= 9,
	VK_CLEAR		= 12,
	VK_RETURN		= 13,
	VK_PAUSE		= 19,
	VK_ESCAPE		= 27,
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
	VK_DELETE		= 127,
	/* End of ASCII mapped keysyms */

	/* International keyboard syms */
	VK_WORLD_0		= 160,		/* 0xA0 */
	VK_WORLD_1		= 161,
	VK_WORLD_2		= 162,
	VK_WORLD_3		= 163,
	VK_WORLD_4		= 164,
	VK_WORLD_5		= 165,
	VK_WORLD_6		= 166,
	VK_WORLD_7		= 167,
	VK_WORLD_8		= 168,
	VK_WORLD_9		= 169,
	VK_WORLD_10		= 170,
	VK_WORLD_11		= 171,
	VK_WORLD_12		= 172,
	VK_WORLD_13		= 173,
	VK_WORLD_14		= 174,
	VK_WORLD_15		= 175,
	VK_WORLD_16		= 176,
	VK_WORLD_17		= 177,
	VK_WORLD_18		= 178,
	VK_WORLD_19		= 179,
	VK_WORLD_20		= 180,
	VK_WORLD_21		= 181,
	VK_WORLD_22		= 182,
	VK_WORLD_23		= 183,
	VK_WORLD_24		= 184,
	VK_WORLD_25		= 185,
	VK_WORLD_26		= 186,
	VK_WORLD_27		= 187,
	VK_WORLD_28		= 188,
	VK_WORLD_29		= 189,
	VK_WORLD_30		= 190,
	VK_WORLD_31		= 191,
	VK_WORLD_32		= 192,
	VK_WORLD_33		= 193,
	VK_WORLD_34		= 194,
	VK_WORLD_35		= 195,
	VK_WORLD_36		= 196,
	VK_WORLD_37		= 197,
	VK_WORLD_38		= 198,
	VK_WORLD_39		= 199,
	VK_WORLD_40		= 200,
	VK_WORLD_41		= 201,
	VK_WORLD_42		= 202,
	VK_WORLD_43		= 203,
	VK_WORLD_44		= 204,
	VK_WORLD_45		= 205,
	VK_WORLD_46		= 206,
	VK_WORLD_47		= 207,
	VK_WORLD_48		= 208,
	VK_WORLD_49		= 209,
	VK_WORLD_50		= 210,
	VK_WORLD_51		= 211,
	VK_WORLD_52		= 212,
	VK_WORLD_53		= 213,
	VK_WORLD_54		= 214,
	VK_WORLD_55		= 215,
	VK_WORLD_56		= 216,
	VK_WORLD_57		= 217,
	VK_WORLD_58		= 218,
	VK_WORLD_59		= 219,
	VK_WORLD_60		= 220,
	VK_WORLD_61		= 221,
	VK_WORLD_62		= 222,
	VK_WORLD_63		= 223,
	VK_WORLD_64		= 224,
	VK_WORLD_65		= 225,
	VK_WORLD_66		= 226,
	VK_WORLD_67		= 227,
	VK_WORLD_68		= 228,
	VK_WORLD_69		= 229,
	VK_WORLD_70		= 230,
	VK_WORLD_71		= 231,
	VK_WORLD_72		= 232,
	VK_WORLD_73		= 233,
	VK_WORLD_74		= 234,
	VK_WORLD_75		= 235,
	VK_WORLD_76		= 236,
	VK_WORLD_77		= 237,
	VK_WORLD_78		= 238,
	VK_WORLD_79		= 239,
	VK_WORLD_80		= 240,
	VK_WORLD_81		= 241,
	VK_WORLD_82		= 242,
	VK_WORLD_83		= 243,
	VK_WORLD_84		= 244,
	VK_WORLD_85		= 245,
	VK_WORLD_86		= 246,
	VK_WORLD_87		= 247,
	VK_WORLD_88		= 248,
	VK_WORLD_89		= 249,
	VK_WORLD_90		= 250,
	VK_WORLD_91		= 251,
	VK_WORLD_92		= 252,
	VK_WORLD_93		= 253,
	VK_WORLD_94		= 254,
	VK_WORLD_95		= 255,		/* 0xFF */

	/* Numeric keypad */
	VK_KP0			= 0x10000,
	VK_KP1,
	VK_KP2,
	VK_KP3,
	VK_KP4,
	VK_KP5,
	VK_KP6,
	VK_KP7,
	VK_KP8,
	VK_KP9,
	VK_KP_PERIOD,
	VK_KP_DIVIDE,
	VK_KP_MULTIPLY,
	VK_KP_MINUS,
	VK_KP_PLUS,
	VK_KP_ENTER,
	VK_KP_EQUALS,

	/* Arrows + Home/End pad */
	VK_UP,
	VK_DOWN,
	VK_RIGHT,
	VK_LEFT,
	VK_INSERT,
	VK_HOME,
	VK_END,
	VK_PAGEUP,
	VK_PAGEDOWN,

	/* Function keys */
	VK_F1,
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
	VK_NUMLOCK,
	VK_CAPSLOCK,
	VK_SCROLLOCK,
	VK_RSHIFT,
	VK_LSHIFT,
	VK_RCTRL,
	VK_LCTRL,
	VK_RALT,
	VK_LALT,
	VK_RMETA,
	VK_LMETA,
	VK_LSUPER,		/* Left "Windows" key */
	VK_RSUPER,		/* Right "Windows" key */
	VK_MODE,		/* "Alt Gr" key */
	VK_COMPOSE,		/* Multi-key compose key */

	/* Miscellaneous function keys */
	VK_HELP,
	VK_PRINT,
	VK_SYSREQ,
	VK_BREAK,
	VK_MENU,
	VK_POWER,		/* Power Macintosh power key */
	VK_EURO,		/* Some european keyboards */
	VK_UNDO,		/* Atari keyboard has Undo */

	/* Add any other keys here */

	VK_LAST
} LgiKey;

class OsAppArguments
{
public:
	int Args;
	char **Arg;
};

// Process
typedef int							OsProcess;
typedef int							OsProcessId;

// Windows
#include "xmainwindow.h"
typedef XMainWindow					*OsWindow;
typedef XWidget						*OsView;

// Threads
typedef pthread_t					OsThreadId;
typedef pthread_mutex_t				OsSemaphore;
#define LgiGetCurrentThread()		pthread_self()

// Sockets
#define ValidSocket(s)				((s)>=0)
#define INVALID_SOCKET				-1
typedef int OsSocket;

/// Sleep the current thread for i milliseconds.
#define LgiSleep(i)					_lgi_sleep(i)
LgiFunc void _lgi_sleep(int i);

#define atoi64						atoll
#define _snprintf					snprintf
#define _vsnprintf					vsnprintf

/// Process any pending messages in the applications message que and then return.
#define LgiYield()					LgiApp->Run(false)

#define K_CHAR						0x0

/// Drag and drop format for a file
#define LGI_FileDropFormat			"text/uri-list"

#define LGI_WideCharset				"utf-32"
#define LGI_PrintfInt64				"%Li"

#define SND_ASYNC					1

#define DOUBLE_CLICK_THRESHOLD		5

// Window flags
#define GWF_VISIBLE					0x00000001
#define GWF_ENABLED					0x00000002
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
#define IsSlash(c)					(((c)=='/')OR((c)=='\\'))
/// Tests a char for being a quote
#define IsQuote(c)					(((c)=='\"')OR((c)=='\''))
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
/// Implemented to handle timer events in the GUI thread.
#define M_X11_PULSE					(M_SYSTEM+3)
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

// Keys

/*
#define VK_F1						1
#define VK_F2						2
#define VK_F3						3
#define VK_F4						4
#define VK_F5						5
#define VK_F6						6
#define VK_F7						7
#define VK_BACK						8
#define VK_TAB						9
#define VK_F8						11
#define VK_F9						12
#define VK_RETURN					13
#define VK_F10						14
#define VK_F11						15
#define VK_F12						16
#define VK_SHIFT					17
#define VK_ESCAPE					18
#define VK_RIGHT					19
#define VK_LEFT						20
#define VK_UP						21
#define VK_DOWN						22
#define VK_PRIOR					23
#define VK_NEXT						24
#define VK_HOME						25
#define VK_END						26
#define VK_INSERT					27
#define VK_DELETE					28
#define VK_CTRL						29
#define VK_ALT						30
*/

#define abs(a)						( (a) < 0 ? -(a) : (a) )

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

