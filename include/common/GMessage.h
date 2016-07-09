#ifndef _GMESSAGE_H_
#define _GMESSAGE_H_

enum LgiMessages
{
	#if defined(WINDOWS)

		// Quite a lot of windows stuff uses WM_USER+n where
		// n < 0x1A0 or so... so stay out of their way.
		M_USER						= WM_USER,
		M_CUT						= WM_CUT,
		M_COPY						= WM_COPY,
		M_PASTE						= WM_PASTE,
		M_COMMAND					= WM_COMMAND,
		M_CLOSE						= WM_CLOSE,

		// wParam = bool Inside; // is the mouse inside the client area?
		// lParam = MAKELONG(x, y); // mouse location
		M_MOUSEENTER				= M_USER + 0x1000,
		M_MOUSEEXIT,

		// return (bool)
		M_WANT_DIALOG_PROC,

		// wParam = void
		// lParam = (MSG*) Msg;
		M_TRANSLATE_ACCELERATOR,

		// None
		M_TRAY_NOTIFY,

		// lParam = Style
		M_SET_WND_STYLE,

		// lParam = GScrollBar *Obj
		M_SCROLL_BAR_CHANGED,

		// lParam = HWND of window under mouse
		// This is only sent for non-LGI window in our process
		// because we'd get WM_MOUSEMOVE's for our own windows
		M_HANDLEMOUSEMOVE,

		// Calls SetWindowPlacement...
		M_SET_WINDOW_PLACEMENT,


		// Log message back to GUI thread
		M_LOG_TEXT,

		// Set the visibility of the window
		M_SET_VISIBLE,

		/// Sent from a worker thread when calling GText::Name
		M_TEXT_UPDATE_NAME,

		/// Send when a window is losing it's mouse capture. Usually
		// because something else has requested it.
		M_LOSING_CAPTURE,
	
	#elif defined(MAC)

		/// Base point for system messages.
		M_SYSTEM						= 0,
		
		/// Message that indicates the user is trying to close a top level window.
		M_CLOSE							= (M_SYSTEM+92),

		/// \brief Mouse enter event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEENTER					= (M_SYSTEM+900),

		/// \brief Mouse exit event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEEXIT,

		// return (bool)
		M_WANT_DIALOG_PROC,

		M_MENU,
		M_COMMAND,
		M_DRAG_DROP,

		M_TRAY_NOTIFY,
		M_CUT,
		M_COPY,
		M_PASTE,
		M_PULSE,
		M_MOUSE_TRACK_UP,
		M_GTHREADWORK_COMPELTE,
		M_TEXT_UPDATE_NAME,
		M_SET_VISIBLE,
		M_SETPOS, // A=(GRect*)Rectangle

		/// Minimum value for application defined message ID's
		M_USER							= (M_SYSTEM+1000),
	
	#elif defined(LINUX)

		/// Base point for system messages.
		M_SYSTEM						= 0x03f0,
		/// Message that indicates the user is trying to close a top level window.
		M_CLOSE,
		/// Implemented to handle invalid requests in the GUI thread.
		M_X11_INVALIDATE,
		/// Implemented to handle paint requests in the GUI thread.
		M_X11_REPARENT,

		/// Minimum value for application defined message ID's
		M_USER							= 0x0400,

		/// \brief Mouse enter event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEENTER,

		/// \brief Mouse exit event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEEXIT,

		/// \brief GView change notification
		///
		/// a = (GView*) Wnd;\n
		/// b = (int) Flags; // Specific to each GView
		M_CHANGE,

		/// \brief Pass a text message up to the UI to descibe whats happening
		///
		/// a = (GView*) Wnd;\n
		/// b = (char*) Text; // description from window
		M_DESCRIBE,

		// return (bool)
		M_WANT_DIALOG_PROC,

		M_MENU,
		M_COMMAND,
		M_DRAG_DROP,

		M_TRAY_NOTIFY,
		M_CUT,
		M_COPY,
		M_PASTE,
		M_GTHREADWORK_COMPELTE,
		
		/// Implemented to handle timer events in the GUI thread.
		M_PULSE,
		M_SET_VISIBLE,
		
		/// Sent from a worker thread when calling GText::Name
		M_TEXT_UPDATE_NAME,
	
	#else
	
		M_USER = 1000,
	
	#endif
	
	M_DESCRIBE,
	M_CHANGE,
	M_DELETE,
	M_TABLE_LAYOUT,
	M_URL
};

class LgiClass GMessage
{
public:
	#if defined(WINDOWS)
	typedef LPARAM Param;
	typedef LRESULT Result;
	#else
	typedef NativeInt Param;
	typedef NativeInt Result;
	#endif

	int m;
	#if defined(WINDOWS)
	HWND hWnd;
	WPARAM a;
	LPARAM b;
	#else
	Param a;
	Param b;
	#endif

	#ifdef LINUX
	bool OwnEvent;
	Gtk::GdkEvent *Event;
	GMessage(Gtk::GdkEvent *e);
	bool Send(OsView Wnd);
	#endif

	GMessage()
	{
		#if defined(WINDOWS)
		hWnd = 0;
		#endif
		m = 0;
		a = 0;
		b = 0;
	}

	GMessage
	(
		int M,
		#if defined(WINDOWS)
		WPARAM A = 0, LPARAM B = 0
		#else
		Param A = 0, Param B = 0
		#endif
	)
	{
		#if defined(WINDOWS)
		hWnd = 0;
		#endif
		m = M;
		a = A;
		b = B;
	}
	
	int Msg() { return m; }
	#if defined(WINDOWS)
	WPARAM A() { return a; }
	LPARAM B() { return b; }
	#else
	Param A() { return a; }
	Param B() { return b; }
	#endif
	void Set(int m, Param a, Param b);
};

// These are deprecated.
#define MsgCode(msg)					((msg)->m)
#define MsgA(msg)						((msg)->a)
#define MsgB(msg)						((msg)->b)
#define CreateMsg(m, a, b)				GMessage(m, a, b)
// extern GMessage CreateMsg(int m, int a = 0, int b = 0);

#endif