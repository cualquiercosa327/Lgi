/**
	\file
	\author Matthew Allen
	\brief GTK specific types and defines.
 */

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#include <string.h>
#include "assert.h"
#include "LgiDefs.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#ifdef WIN32
	#include "winsock2.h"
	#define WIN32GTK                    1
	#define WINNATIVE					0
	#ifdef _WIN64
		#define LGI_64BIT				1
	#else
		#define LGI_32BIT				1
	#endif
#else
	#define _MULTI_THREADED
	#include <pthread.h>
	#define LINUX						1
	#define XP_CTRLS					1
	#define POSIX						1

	#ifdef __arm__
		#define LGI_RPI					1
		#define LGI_32BIT				1
	#elif __GNUC__
	#if __x86_64__ || __ppc64__
		#define LGI_64BIT				1
	#else
		#define LGI_32BIT				1
	#endif
	#endif
#endif

#undef stricmp

#include "LgiInc.h"
namespace Gtk {
#include <gtk/gtk.h>
#ifdef WIN32
#include <gdk/gdkwin32.h>
#endif
}

// System

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

class LgiClass OsAppArguments
{
	struct OsAppArgumentsPriv *d;

public:
	int Args;
	char **Arg;

	OsAppArguments(int args, const char **arg);
	~OsAppArguments();

	void Set(char *CmdLine);
	OsAppArguments &operator =(OsAppArguments &a);
};

// Process
#ifdef WIN32
typedef HANDLE							OsProcess;
#else
typedef int								OsProcess;
#endif
typedef int								OsProcessId;
typedef Gtk::GtkWidget					*OsView;
typedef Gtk::GtkWindow					*OsWindow;
typedef char							OsChar;
typedef Gtk::cairo_t					*OsPainter;
typedef Gtk::PangoFontDescription       *OsFont;
typedef void							*OsBitmap;

#define LgiGetCurrentProcess()			getpid()

// Because of namespace issues you can't use the built in GTK casting macros.
// So this is basically the same thing:
// e.g.
//		Gtk::GtkContainer *c = GtkCast(widget, gtk_container, GtkContainer);
//
#define GtkCast(obj, gtype, ctype)		((Gtk::ctype*)g_type_check_instance_cast( (Gtk::GTypeInstance*)obj, Gtk::gtype##_get_type() ))

#define GtkVer(major, minor)			( (GTK_MAJOR_VERSION > major) || (GTK_MAJOR_VERSION == major && GTK_MINOR_VERSION >= minor) )

class OsApplication
{
	class OsApplicationPriv *d;

protected:
	static OsApplication *Inst;
	
public:
	OsApplication(int Args, char **Arg);
	~OsApplication();
	
	static OsApplication *GetInst() { LgiAssert(Inst); return Inst; }
};

#define XcbConn()					(OsApplication::GetInst()->GetConn())
#define XcbScreen()					(OsApplication::GetInst()->GetScreen())
#define XcbCheck(cookie)			(OsApplication::GetInst()->Check(cookie, __FILE__, __LINE__))
#ifdef _DEBUG
#define XcbDebug(cookie)			(OsApplication::GetInst()->Check(cookie, __FILE__, __LINE__))
#else
#define XcbDebug(cookie)			cookie
#endif

// Threads
#ifdef WIN32

typedef HANDLE  					OsThread;
typedef DWORD						OsThreadId;
typedef CRITICAL_SECTION			OsSemaphore;
#define LgiGetCurrentThread()		GetCurrentThreadId()

#else

typedef pthread_t					OsThread;
typedef pid_t						OsThreadId;
typedef pthread_mutex_t				OsSemaphore;
#define LgiGetCurrentThread()		pthread_self()
LgiFunc OsThreadId					GetCurrentThreadId();

#endif

#include "GMessage.h"

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
LgiFunc void LgiSleep(uint32 i);
#endif

#ifndef WIN32
#define atoi64						atoll
#else
#define atoi64						_atoi64
#endif

#define _snprintf					snprintf
#define _vsnprintf					vsnprintf
#define wcscpy_s(dst, len, src)		wcsncpy(dst, src, len)

/// Process any pending messages in the applications message que and then return.
#define LgiYield()					LgiApp->Run(false)

#define K_CHAR						0x0

/// Drag and drop format for a file
#define LGI_FileDropFormat			"text/uri-list"
#define LGI_StreamDropFormat		"application/x-file-stream" // FIXME
#define LGI_IllegalFileNameChars	"\t\r\n/\\:*?\"<>|"
#ifdef WINDOWS
#define LGI_WideCharset				"ucs-2"
#else
#define LGI_WideCharset				"utf-32"
#endif
#define LGI_PrintfInt64				"%Li"
#define LGI_PrintfHex64				"%Lx"

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
	#define LGI_LIBRARY_EXT			"so"
	/// The standard executable extension
	#define LGI_EXECUTABLE_EXT		""
#endif
/// The standard end of line string for Linux
#define EOL_SEQUENCE				"\n"
/// Tests a char for being a slash
#define IsSlash(c)					(((c)=='/')||((c)=='\\'))
/// Tests a char for being a quote
#define IsQuote(c)					(((c)=='\"')||((c)=='\''))

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


#define abs(a)						( (a) < 0 ? -(a) : (a) )

#ifndef WIN32

	#include <gdk/gdkkeysyms-compat.h>

	typedef enum {
		/* The keyboard syms have been cleverly chosen to map to ASCII */
		VK_UNKNOWN		= 0,
		VK_FIRST		= 0,
		VK_BACKSPACE	= 8,
		VK_TAB			= 9,
		VK_CLEAR		= GDK_Clear,
		VK_RETURN		= 13,
		VK_PAUSE		= GDK_Pause,
		VK_ESCAPE		= GDK_Escape,
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
		VK_LEFTBRACKET	= GDK_bracketleft,
		VK_BACKSLASH	= GDK_backslash,
		VK_RIGHTBRACKET	= GDK_bracketright,
		VK_CARET		= 94,
		VK_UNDERSCORE	= GDK_underscore,
		VK_BACKQUOTE	= 96,
		VK_a			= GDK_a,
		VK_b			= GDK_b,
		VK_c			= GDK_c,
		VK_d			= GDK_d,
		VK_e			= GDK_e,
		VK_f			= GDK_f,
		VK_g			= GDK_g,
		VK_h			= GDK_h,
		VK_i			= GDK_i,
		VK_j			= GDK_j,
		VK_k			= GDK_k,
		VK_l			= GDK_l,
		VK_m			= GDK_m,
		VK_n			= GDK_n,
		VK_o			= GDK_o,
		VK_p			= GDK_p,
		VK_q			= GDK_q,
		VK_r			= GDK_r,
		VK_s			= GDK_s,
		VK_t			= GDK_t,
		VK_u			= GDK_u,
		VK_v			= GDK_v,
		VK_w			= GDK_w,
		VK_x			= GDK_x,
		VK_y			= GDK_y,
		VK_z			= GDK_z,
		/* End of ASCII mapped keysyms */

		/* Numeric keypad */
		VK_KP_ENTER		= GDK_KP_Enter,
		VK_KP0			= GDK_KP_0,
		VK_KP1			= GDK_KP_1,
		VK_KP2			= GDK_KP_2,
		VK_KP3			= GDK_KP_3,
		VK_KP4			= GDK_KP_4,
		VK_KP5			= GDK_KP_5,
		VK_KP6			= GDK_KP_6,
		VK_KP7			= GDK_KP_7,
		VK_KP8			= GDK_KP_8,
		VK_KP9			= GDK_KP_9,
		VK_KP_PERIOD	= GDK_KP_Decimal,
		VK_KP_DELETE	= GDK_KP_Delete,
		VK_KP_MULTIPLY	= GDK_KP_Multiply,
		VK_KP_PLUS		= GDK_KP_Add,
		VK_KP_MINUS		= GDK_KP_Subtract,
		VK_KP_DIVIDE	= GDK_KP_Divide,
		VK_KP_EQUALS	= GDK_KP_Equal,

		/* Arrows + Home/End pad */
		VK_HOME			= GDK_Home,
		VK_LEFT			= GDK_Left,
		VK_UP			= GDK_Up,
		VK_RIGHT		= GDK_Right,
		VK_DOWN			= GDK_Down,
		VK_PAGEUP		= GDK_Page_Up,
		VK_PAGEDOWN		= GDK_Page_Down,
		VK_END			= GDK_End,
		VK_INSERT		= GDK_Insert,

		/* Function keys */
		VK_F1			= GDK_F1,
		VK_F2			= GDK_F2,
		VK_F3			= GDK_F3,
		VK_F4			= GDK_F4,
		VK_F5			= GDK_F5,
		VK_F6			= GDK_F6,
		VK_F7			= GDK_F7,
		VK_F8			= GDK_F8,
		VK_F9			= GDK_F9,
		VK_F10			= GDK_F10,
		VK_F11			= GDK_F11,
		VK_F12			= GDK_F12,
		VK_F13			= GDK_F13,
		VK_F14			= GDK_F14,
		VK_F15			= GDK_F15,

		/* Key state modifier keys */
		VK_NUMLOCK		= GDK_Num_Lock,
		VK_CAPSLOCK		= GDK_Caps_Lock,
		VK_SCROLLOCK	= GDK_Scroll_Lock,
		VK_LSHIFT		= GDK_Shift_L,
		VK_RSHIFT		= GDK_Shift_R,
		VK_LCTRL		= GDK_Control_L,
		VK_RCTRL		= GDK_Control_R,
		VK_LALT			= GDK_Alt_L,
		VK_RALT			= GDK_Alt_R,
		VK_LMETA		= GDK_Hyper_L,
		VK_RMETA		= GDK_Hyper_R,
		VK_LSUPER		= GDK_Super_L, /* "Windows" key */
		VK_RSUPER		= GDK_Super_R,

		/* Miscellaneous function keys */
		VK_HELP			= GDK_Help,
		VK_PRINT		= GDK_Print,
		VK_SYSREQ		= GDK_Sys_Req,
		VK_BREAK		= GDK_Break,
		VK_MENU			= GDK_Menu,
		VK_UNDO			= GDK_Undo,
		VK_REDO			= GDK_Redo,
		VK_EURO			= GDK_EuroSign, /* Some european keyboards */
		VK_COMPOSE		= GDK_Multi_key, /* Multi-key compose key */
		VK_MODE			= GDK_Mode_switch, /* "Alt Gr" key (could be wrong) */
		VK_DELETE		= GDK_Delete,
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
	LgiFunc int WinPointToHeight(int Pt, HDC hDC = NULL);
	LgiFunc int WinHeightToPoint(int Ht, HDC hDC = NULL);
	LgiExtern class GString WinGetSpecialFolderPath(int Id);

	typedef BOOL (__stdcall *pSHGetSpecialFolderPathA)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);
	typedef BOOL (__stdcall *pSHGetSpecialFolderPathW)(HWND hwndOwner, LPWSTR lpszPath, int nFolder, BOOL fCreate);
	typedef int (__stdcall *pSHFileOperationA)(LPSHFILEOPSTRUCTA lpFileOp);
	typedef int (__stdcall *pSHFileOperationW)(LPSHFILEOPSTRUCTW lpFileOp);
	typedef int (__stdcall *p_vscprintf)(const char *format, va_list argptr);

	#if _MSC_VER >= 1400
		#define snprintf sprintf_s
	#endif

	/// Convert a string d'n'd format to an OS dependant integer.
	LgiFunc int FormatToInt(char *s);
	/// Convert a Os dependant integer d'n'd format to a string.
	LgiFunc char *FormatToStr(int f);

#endif

#endif

