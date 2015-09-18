/**
	\file
	\author Matthew Allen
	\date 19/12/1997
	\brief Gui class definitions
	Copyright (C) 1997-2004, <a href="mailto:fret@memecode.com">Matthew Allen</a>
*/

/////////////////////////////////////////////////////////////////////////////////////
// Includes
#ifndef __GUI_H
#define __GUI_H

#if defined BEOS
#include <string.h>
#endif

#include "GMutex.h"
#include "LgiOsClasses.h"
#include "GMem.h"
#include "GArray.h"
#include "LgiCommon.h"
#include "GXmlTree.h"

#ifndef WIN32
#include "GDragAndDrop.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////
// Externs
extern long MouseWatcher(void *Ptr);
extern bool LgiCheckFile(char *Path, int PathSize);
LgiFunc bool LgiPostEvent(OsView Wnd, int Event, GMessage::Param a = 0, GMessage::Param b = 0);
LgiFunc GViewI *GetNextTabStop(GViewI *v, bool Back);
/// Converts an OS error code into a text string
LgiClass GAutoString LgiErrorCodeToString(uint32 ErrorCode);
#if defined(MAC) && !defined(COCOA)
LgiFunc void DumpHnd(HIViewRef v, int depth = 0);
#endif

/// Virtual base class for receiving events
class LgiClass GTarget
{
public:
	virtual GMessage::Result OnEvent(GMessage *Msg) { return 0; }
};

/////////////////////////////////////////////////////////////////////////////////
#if WINNATIVE
typedef DWORD						OsProcessId;
#else
typedef int							OsProcessId;
#endif

/// Returns the current process ID
#define LgiProcessId()				(LgiApp->GetProcessId())

/// Returns a pointer to the GApp object.
///
/// \warning Don't use this before you have created your GApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#define LgiApp						(GApp::ObjInstance())

/// Returns a system font pointer.
///
/// \warning Don't use this before you have created your GApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#define SysFont						(LgiApp->SystemNormal)

/// Returns a bold system font pointer.
///
/// \warning Don't use this before you have created your GApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#define SysBold						(LgiApp->SystemBold)

/// Exits the application right now!
///
/// \warning This will cause data loss if you have any unsaved data. Equivilant to exit(0).
LgiFunc void LgiExitApp();

/// Closes the application gracefully.
///
/// This actually causes GApp::Run() to stop processing message and return.
#define LgiCloseApp()				LgiApp->Exit(false)

#if defined(LINUX) && !defined(LGI_SDL)
#define ThreadCheck()				LgiAssert(InThread())
#else
#define ThreadCheck()
#endif

/// Optional arguments to the GApp object
struct GAppArguments
{
	/// Don't initialize the skinning engine.
	bool NoSkin;
};

/// \brief Singleton class for handling application wide settings and methods
///
/// This should be the first class you create, passing in the arguments from the
/// operating system. And once your initialization is complete the 'Run' method
/// is called to enter the main application loop that processes messages for the
/// life time of the application.
class LgiClass GApp : virtual public GAppI,
	public GBase,
	public OsApplication
{
	friend class GView;

protected:
	// private member vars
	class GAppPrivate *d;
	
	#if defined LGI_SDL
	
	void OnSDLEvent(GMessage *m);
	
	#elif defined WIN32

	CRITICAL_SECTION StackTraceSync;
	friend LONG __stdcall _ExceptionFilter_Redir(LPEXCEPTION_POINTERS e);
	LONG __stdcall _ExceptionFilter(LPEXCEPTION_POINTERS e, char *ProductId);
	friend class GWin32Class;
	List<GWin32Class> *GetClasses();

	#elif defined ATHEOS

	char *_AppFile;

	#elif defined BEOS

	void RefsReceived(BMessage *Msg);
	
	#elif defined LINUX
	
	friend class GClipBoard;
	
	virtual void OnEvents();
	void DeleteMeLater(GViewI *v);
	void SetClipBoardContent(OsView Hnd, GVariant &v);
	bool GetClipBoardContent(OsView Hnd, GVariant &v, GArray<char*> &Types);
	
	#endif

	friend class GMouseHook;
	static GMouseHook *MouseHook;

public:
	// Static publics
	
	/// Use 'LgiApp' to return a pointer to the GApp object
	static GApp *ObjInstance();
	static class GSkinEngine *SkinEngine;

	// public member vars
	
	/// The system font
	GFont *SystemNormal;
	
	/// The system font in bold
	GFont *SystemBold;
	
	/// Pointer to the applications main window
	GWindow *AppWnd;

	/// Returns true if the GApp object initialized correctly
	bool IsOk();
	
	/// Returns this processes ID
	OsProcessId GetProcessId();
	
	/// Returns the thread currently running the active message loop
	OsThreadId GetGuiThread();
	
	/// Returns the number of CPU cores the machine has
	int GetCpuCount();

	/// Construct the object
	GApp
	(
		/// The arguments passed in by the OS.
		OsAppArguments &AppArgs,
		/// The application's name.
		const char *AppName,
		/// Optional args
		GAppArguments *ObjArgs = 0
	);

	/// Destroys the object
	virtual ~GApp();

	/// Returns the version of Lgi used. String returned is in the form '#.#.#'
	const char *GetLgiVersion() { return LGI_VER; }

	/// Resets the arguments
	void SetAppArgs(OsAppArguments &AppArgs);
	
	/// Returns the arguemnts
	OsAppArguments *GetAppArgs();

	/// Returns the n'th argument as a heap string. Free with DeleteArray(...).
	char *GetArgumentAt(int n);
	
	/// Enters the message loop.
	bool Run
	(
		/// If true this function will return when the application exits (with LgiCloseApp()).
		/// Otherwise if false only pending events will be processed and then the function returns.
		bool Loop = true,
		/// Idle callback
		OnIdleProc IdleCallback = NULL,
		/// Param for IdleCallback
		void *IdleParam = NULL
	);
	
	/// Event called to process the command line
	void OnCommandLine();
	
	/// Event called to process files dropped on the application
	void OnReceiveFiles(GArray<char*> &Files);

	/// Event called to process URLs given to the application
	void OnUrl(const char *Url);
	
	/// Exits the event loop with the code specified
	void Exit
	(
		/// The application exit code.
		int Code = 0
	);
	
	/// \brief Parses the command line for a switch
	/// \return true if the option exists.
	bool GetOption
	(
		/// The option to look for.
		const char *Option,
		/// The buffer to receive the value.
		GAutoString &Buf
	);

	/// \brief Parses the command line for a switch
	/// \return true if the option exists.
	bool GetOption
	(
		/// The option to look for.
		const char *Option,
		/// The buffer to receive the value of the command line parameter or NULL if you don't care.
		char *Dst = 0,
		/// The buffer size in bytes
		int DstSize = 0
	);
	
	/// Gets the application conf stored in lgi.conf
	GXmlTag *GetConfig(const char *Tag);

	/// Sets a single tag in the config. (Not written to disk)
	void SetConfig(GXmlTag *Tag);

	/// Gets the control with the keyboard focus
	GViewI *GetFocus();
	
	/// Gets the MIME type of a file
	/// \returns the mime type or NULL if unknown.
	GAutoString GetFileMimeType
	(
		/// The file to identify
		const char *File
	);

    /// Gets the applications that can handle a file of a certain mime type
	bool GetAppsForMimeType(char *Mime, GArray<GAppInfo*> &Apps);
		
	/// Get a system metric
	int32 GetMetric
	(
		/// One of #LGI_MET_DECOR_X, #LGI_MET_DECOR_Y
		LgiSystemMetric Metric
	);

	/// Get the mouse hook instance
	GMouseHook *GetMouseHook();

	/// Gets the singleton symbol lookup class
	class GSymLookup *GetSymLookup();

	/// \returns true if the process is running with elevated permissions
	bool IsElevated();

	/// Gets the font cache
	class GFontCache *GetFontCache();

	// OS Specific
	#if defined(LGI_SDL)

		/// This keeps track of the dirty rectangle and issues a M_INVALIDATE
		/// event when needed to keep the screen up to date.
		bool InvalidateRect(GRect &r);
	
	#elif defined(WIN32)

		HINSTANCE GetInstance();
		int GetShow();

		/// \returns true if the application is running under Wine on Linux. This is useful to know
		/// if you need to work around missing functionality in the Wine implementation.
		bool IsWine();

	#elif defined(LINUX)
	
		class GLibrary *GetWindowManagerLib();
		void RegisterHandle(GView *v);
		void UnregisterHandle(GView *v);
		bool InThread();
		
	#endif
};

/// \brief The base class for all windows in the GUI.
///
/// This is the core object that all on screen windows inherit from. It encapsulates
/// a HWND on Win32, a BView on BeOS, a Window on X11 + Mac OS X. Used by itself it's not a
/// top level window, for that see the GWindow class.
///
/// To create a top level window see GWindow or GDialog.
///
/// For a GView with scroll bars use GLayout.
///
class LgiClass GView : virtual public GViewI, virtual public GBase
{
	friend		class GWindow;
	friend		class GLayout;
	friend		class GControl;
	friend		class GMenu;
	friend		class GSubMenu;
	friend		class GWnd;
	friend		class GScrollBar;
	friend		class GFileTarget;
	friend		class GDialog;
	friend		class GDragDropTarget;
	friend		class GPopup;

	friend		bool SysOnKey(GView *w, GMessage *m);

	#if defined(__GTK_H__)

	friend Gtk::gboolean lgi_widget_expose(Gtk::GtkWidget *widget, Gtk::GdkEventExpose *e);
    friend Gtk::gboolean lgi_widget_click(Gtk::GtkWidget *widget, Gtk::GdkEventButton *ev);
    friend Gtk::gboolean lgi_widget_motion(Gtk::GtkWidget *widget, Gtk::GdkEventMotion *ev);
	friend Gtk::gboolean GViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent  *event, GView *view);
	friend Gtk::gboolean PopupEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, class GPopup *This);
	friend Gtk::gboolean GtkViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, GView *This);
	
	virtual Gtk::gboolean OnGtkEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event);

	#elif defined WIN32

	friend		class GWin32Class;
	friend		LRESULT CALLBACK DlgRedir(OsView hWnd, UINT m, WPARAM a, LPARAM b);
	static		void CALLBACK TimerProc(OsView hwnd, UINT uMsg, UINT_PTR idEvent, uint32 dwTime);

	#elif defined MAC
	
	#if defined(COCOA)
	#else
	friend OSStatus LgiWindowProc(EventHandlerCallRef, EventRef, void *);
	friend OSStatus LgiRootCtrlProc(EventHandlerCallRef, EventRef, void *);
	friend OSStatus CarbonControlProc(EventHandlerCallRef, EventRef, void *);
	friend OSStatus GViewProc(EventHandlerCallRef, EventRef, void *);
	friend OSStatus LgiViewDndHandler(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
	#endif

	#elif defined BEOS

	friend		class GButtonRedir;
	friend		class _OsEditFrame;
	friend		class BViewRedir;
	friend		long _lgi_pulse_thread(void *ptr);
	friend 		GView *_lgi_search_children(GView *v, int &x, int &y);

	#endif

	#if defined(LGI_SDL)
	friend Uint32 SDL_PulseCallback(Uint32 interval, GView *v);
	#endif


	GRect				Pos;
	int					_InLock;

protected:
	class GViewPrivate	*d;

	OsView				_View; // OS specific handle to view object
	GView				*_Window;
	GMutex				*_Lock;
	uint16				_BorderSize;
	uint16				_IsToolBar;
	int					WndFlags;

	static GViewI		*_Capturing;
	static GViewI		*_Over;
	
	#if defined WINNATIVE

	uint32 GetStyle();
	void SetStyle(uint32 i);
	uint32 GetExStyle();
	void SetExStyle(uint32 i);
	uint32 GetDlgCode();
	void SetDlgCode(uint32 i);

    /// \brief Gets the win32 class passed to CreateWindowEx()
	const char *GetClassW32();
	/// \brief Sets the win32 class passed to CreateWindowEx()
	void SetClassW32(const char *s);
	/// \brief Creates a class to pass to CreateWindowEx(). If this methed is not
	/// explicitly called then the string from GetClass() is used to create a class,
	/// which is usually the name of the object.
	GWin32Class *CreateClassW32(const char *Class = 0, HICON Icon = 0, int AddStyles = 0);

	virtual int		SysOnNotify(int Code) { return 0; }

	#elif defined BEOS

	struct OsMouseInfo;
	friend long _lgi_mouse_thread(OsMouseInfo *Info);

	OsMouseInfo		*_MouseInfo;
	OsThread		_CaptureThread;
	OsThread		_PulseThread;
	int				_PulseRate;
	BWindow			*_QuitMe;

	void _Key(const char *bytes, int32 numBytes, bool down);
	virtual bool QuitRequested() {}
	
	#elif defined MAC
	
	OsView _CreateCustomView();
	bool _Attach(GViewI *parent);
	#if defined(COCOA)
	#else
	virtual bool _OnGetInfo(HISize &size, HISize &line, HIRect &bounds, HIPoint &origin) { return false; }
	virtual void _OnScroll(HIPoint &origin) {}
	#endif
	
	#endif


	// Complex Region searches
	
	/// Finds the largest rectangle in the region
	GRect *FindLargest(GRegion &r);

	/// Finds the smallest rectangle that would fit a window 'Sx' by 'Sy'
	GRect *FindSmallestFit(GRegion &r, int Sx, int Sy);

	/// Finds the largest rectangle on the  specified
	GRect *FindLargestEdge
	(
		/// The region to search
		GRegion &r,
		/// The edge to look at:
		/// \sa GV_EDGE_TOP, GV_EDGE_RIGHT, GV_EDGE_BOTTOM or GV_EDGE_LEFT
		int Edge
	);

	virtual void _Delete();
	GViewI *FindReal(GdcPt2 *Offset = 0);
	bool HandleCapture(GView *Wnd, bool c);
	
	virtual void	_Paint(GSurface *pDC = 0, int Ox = 0, int Oy = 0);

	#if !WINNATIVE

	GView *&PopupChild();
	virtual bool	_Mouse(GMouse &m, bool Move);
	void			_Focus(bool f);

	#endif

	virtual bool OnViewMouse(GView *v, GMouse &m) { return true; }
	virtual bool OnViewKey(GView *v, GKey &k) { return false; }
	virtual void OnNcPaint(GSurface *pDC, GRect &r);

	/// List of children views.
	friend class GViewIter;
	List<GViewI>	Children;

public:
	/// \brief Creates a view/window.
	///
	/// On non-Win32 platforms the default argument is the class that redirects the
	/// C++ virtual event handlers to the GView handlers. Which is usually the
	/// 'DefaultOsView' class. If you pass NULL in a DefaultOsView will be created to
	/// do the job. On BeOS you can subclass the native controls by passing in an
	/// instance of the BView based class.
	GView
	(
		/// The handle that the OS knows the window by
		OsView wnd = 0
	);

	/// Destructor
	virtual ~GView();

	/// Returns the OS handle of the view
	OsView Handle() { return _View; }

	/// Returns the ptr to a GView
	GView *GetGView() { return this; }
	
	/// Returns the OS handle of the top level window
	virtual OsWindow WindowHandle();

	// Attaching windows / heirarchy
	bool AddView(GViewI *v, int Where = -1);
	bool DelView(GViewI *v);
	bool HasView(GViewI *v);
	GViewIterator *IterateViews();
	
	/// \brief Attaches the view to a parent view.
	///
	/// Each GView starts in an un-attached state. When you attach it to a Parent GView
	/// the view gains a OS-specific handle and becomes visible on the screen (if the
	/// Visible() property is TRUE). However if a view is inserted into the Children list
	/// of a GView and it's parent pointer is set correctly it will still paint on the
	/// screen without the OS knowing about it. This is known in Lgi as a "virtual window"
	/// and is primarily used to cut down on windowing resources. Mouse clicks are handled
	/// by the parent window and passed down to the virtual children. Virtual children
	/// are somewhat limited. They can't receive focus, or participate in drag and drop
	/// operations. If you want to see an example have a look at the GToolBar code.
	virtual bool Attach
	(
		/// The parent view or NULL for a top level window
		GViewI *p
	);
	
	/// Attachs all the views in the Children list if not already attached.
	virtual bool AttachChildren();
	
	/// Detachs a window from it's parent.
	virtual bool Detach();
	
	/// Returns true if the window is attached
	virtual bool IsAttached();
	
	/// Destroys the window async
	virtual void Quit(bool DontDelete = false);
	
	// Properties
	
	/// Gets the top level window that this view belongs to
	GWindow *GetWindow();
	
	/// Gets the parent view.
	GViewI *GetParent();

	/// \brief Sets the parent view.
	///
	/// This doesn't attach the window so that it will display. You should use GView::Attach for that.
	virtual void SetParent(GViewI *p);

	/// Script handler to receive UI events.
	GEventsI *Script;
	bool OnScriptEvent(GViewI *Ctrl) { return false; }

	/// Sends a notification to the notify target or the parent chain
	void SendNotify(int Data = 0);
	
	/// Gets the window that receives event notifications
	GViewI *GetNotify();

	/// \brief Sets the view to receive event notifications.
	///
	/// The notify window will receive events when this view changes. By
	/// default the parent view receives the events.
	virtual void SetNotify(GViewI *n);

	/// \brief Each top level window (GWindow) has a lock. By calling this function
	/// you lock the whole GWindow and all it's children.
	bool Lock
	(
		/// The file name of the caller
		const char *file,
		/// The line number of the caller
		int line,
		/// The timeout in milli-seconds or -1 to block until locked.
		int TimeOut = -1
	);
	
	/// Unlocks the GWindow and that this view belongs to.
	void Unlock();

	/// Called to process every message received by this window.
	GMessage::Result OnEvent(GMessage *Msg);

	/// true if the view is enabled
	bool Enabled();
	
	/// Sets the enabled state
	void Enabled(bool e);
	
	/// true if the view is visible
	bool Visible();
	
	/// Hides/Shows the view
	void Visible
	(
		/// True if you want to show the view, False to hide the view/
		bool v
	);

	/// true if the view has keyboard focus
	bool Focus();
	
	/// Sets the keyboard focus state on the view.
	void Focus(bool f);
	
	/// Get/Set the drop source
	GDragDropSource *DropSource(GDragDropSource *Set = NULL);

	/// Get/Set the drop target
	GDragDropTarget *DropTarget(GDragDropTarget *Set = NULL);
	
	/// Sets the drop target state of this view
	bool DropTarget(bool t);

	/// \brief Gives this view a 1 or 2 px sunken border.
	///
	/// The size is set by the _BorderSize member variable. This border is
	/// not considered part of the client area. Mouse and drawing coordinates
	/// do not take it into account.
	bool Sunken();
	
	/// Sets a sunken border around the control
	void Sunken(bool i);

	/// true if the view has a flat border
	bool Flat();
	
	/// Sets the flat border state
	void Flat(bool i);
	
	/// \brief true if the view has a raised border
	///
	/// The size is set by the _BorderSize member variable. This border is
	/// not considered part of the client area. Mouse and drawing coordinates
	/// do not take it into account.
	bool Raised();
	
	/// Sets the raised border state
	void Raised(bool i);

	/// Draws an OS themed border
	void DrawThemeBorder(GSurface *pDC, GRect &r);

	/// \brief true if the control is currently executing in the GUI thread
	///
	/// Some OS functions are not thread safe, and can only be called in the GUI
	/// thread. In the Linux implementation the GUI thread can change from time
	/// to time. On Win32 it stays the same. In any case if this function returns
	/// true it's safe to do just about anything.
	bool InThread();
	
	/// \brief Asyncronously posts an event to be received by this window
	///
	/// This calls PostMessage on Win32 and XSendEvent on X11. XSendEvent is called
	/// with a ClientMessage with the a and b parameters in the data section.
	bool PostEvent
	(
		/// The command ID.
		/// \sa Should be M_USER or higher for custom events.
		int Cmd,
		/// The first 32-bits of data. Equivilent to wParam on Win32.
		GMessage::Param a = 0,
		/// The second 32-bits of data. Equivilent to lParam on Win32.
		GMessage::Param b = 0
	);

	/// \brief Sets the utf-8 text associated with this view
	///
	/// Name and NameW are interchangable. Using them in any order will convert the
	/// text between utf-8 and wide to satify any requirement. Generally once the opposing
	/// version of the string is required both the utf-8 and wide copies of the string
	/// remain cached in RAM until the Name is changed.
	bool Name(const char *n);
	
	/// Returns the utf-8 text associated with this view
	char *Name();
	
	/// Sets the wide char text associated with this view
	virtual bool NameW(const char16 *n);

	/// \brief Returns the wide char text associated with this view
	///
	/// On Win32 the wide characters are 16 bits, on unix systems they are 32-bit 
	/// characters.
	virtual char16 *NameW();

	/// \brief Gets the font this control should draw with.
	///
	/// The default font is the system font, owned by the GApp object.
	virtual GFont *GetFont();
	
	/// \brief Sets the font for this control
	///
	/// The lifetime of the font passed in is the responsibility of the caller.
	/// The GView object assumes the pointer will be valid at all times.
	virtual void SetFont(GFont *Fnt, bool OwnIt = false);

	/// Returns the cursor that should be displayed for the given location
	/// \returns a cursor type. i.e. LCUR_Normal from LgiDefs.h
	LgiCursor GetCursor(int x, int y);
	
	/*
	/// \brief Sets the mouse cursor to display when the mouse is over this control.
	///
	/// This currently only works on Win32, as I can't get the X11 cursor functions to
	/// work. They seem horribly broken. (Surprise surprise)
	bool SetCursor
	(
		/// The cursor to change to.
		/// \sa the defines starting with LCUR_Normal from LgiDefs.h
		LgiCursor Cursor
	);
	*/

	/// \brief Get the position of the view relitive to it's parent.
	virtual GRect &GetPos() { return Pos; }
	/// Get the client region of the window relitive to itself (ie always 0,0-x,y)
	virtual GRect &GetClient(bool InClientSpace = true);
	/// Set the position of the view in terms of it's parent
	virtual bool SetPos(GRect &p, bool Repaint = false);
	/// Gets the width of the view in pixels
	int X() { return Pos.X(); }
	/// Gets the height of the view in pixels.
	int Y() { return Pos.Y(); }
	/// Gets the minimum size of the view
	GdcPt2 GetMinimumSize();
	/// \brief Set the minimum size of the view.
	///
	/// Only works for top level windows.
	void SetMinimumSize(GdcPt2 Size);	

    /// Sets the style of the control
    bool SetCssStyle(const char *CssStyle);
    /// Gets the style of the control
    class GCss *GetCss(bool Create = false);
    /// Sets the style of the control (will take ownership of 'css')
    void SetCss(GCss *css);
    /// Sets the CSS foreground or background colour
	bool SetColour(GColour &c, bool Fore);

	/// The class' name. Should be overriden in child classes to return the
	/// right class name. Mostly used for debugging, but in the win32 port it
	/// is also the default WIN32 class name passed to RegisterClass() in 
	/// GView::CreateClass().
	///
	/// \returns the Class' name for debugging
	const char *GetClass() { return "GView"; }

	/// \brief Captures all mouse events to this view
	///
	/// Once you have mouse capture all mouse events will be passed to this
	/// view. i.e. during a mouse click.
	bool Capture(bool c);
	/// true if this view is capturing mouse events.
	bool IsCapturing();
	/// \brief Gets the current mouse location
	/// \return true on success
	bool GetMouse
	(
		/// The mouse location information returned
		GMouse &m,
		/// Get the location in screen coordinates
		bool ScreenCoords = false
	);

	/// \brief Gets the ID associated with the view
	///
	/// The ID of a view is designed to associate controls defined in resource
	/// files with a object at runtime via a C header file define.
	int GetId();
	/// Sets the view's ID.
	void SetId(int i);
	/// true if this control is a tab stop.
	bool GetTabStop();
	/// \brief Sets whether this control is a tab stop.
	///
	/// A top stop is a control that receives focus if the user scrolls through the controls
	/// with the tab key.
	void SetTabStop(bool b);
	/// Gets the integer representation of the view's contents
	virtual int64 Value() { return 0; }
	/// Sets the integer representation of the view's contents
	virtual void Value(int64 i) {}
	/// Find a view by it's os handle
	virtual GViewI *FindControl(OsView hnd);
	/// Returns the view by it's ID
	virtual GViewI *FindControl
	(
		// The ID to look for
		int Id
	);

	/// Gets the value of the control identified by the ID
	int64 GetCtrlValue(int Id);
	/// Sets the value of the control identified by the ID
	void SetCtrlValue(int Id, int64 i);
	/// Gets the name (text) of the control identified by the ID
	char *GetCtrlName(int Id);
	/// Sets the name (text) of the control identified by the ID
	void SetCtrlName(int Id, const char *s);
	/// Gets the enabled state of the control identified by the ID
	bool GetCtrlEnabled(int Id);
	/// Sets the enabled state of the control identified by the ID
	void SetCtrlEnabled(int Id, bool Enabled);
	/// Gets the visible state of the control identified by the ID
	bool GetCtrlVisible(int Id);
	/// Sets the visible state of the control identified by the ID
	void SetCtrlVisible(int Id, bool Visible);

	/// Causes the given area of the view to be repainted to update the screen
	bool Invalidate
	(
		/// The rectangle of the view to repaint, or NULL for the entire view
		GRect *r = NULL,
		/// true if you want to wait for the update to happen
		bool Repaint = false,
		/// false to update in client coordinates, true to update the non client region
		bool NonClient = false
	);

	/// Causes the given area of the view to be repainted to update the screen
	bool Invalidate
	(
		/// The region of the view to repaint
		GRegion *r,
		/// true if you want to wait for the update to happen
		bool Repaint = false,
		/// false to update in client coordinates, true to update the non client region
		bool NonClient = false
	);

	/// true if the mouse event is over the view
	bool IsOver(GMouse &m);
	/// returns the sub window located at the point x,y	
	GViewI *WindowFromPoint(int x, int y, bool Debug = false);
	/// Sets a timer to call the OnPulse() event
	void SetPulse
	(
		/// The milliseconds between calls to OnPulse() or -1 to disable
		int Ms = -1
	);
	/// Convert a point form view coordinates to screen coordinates
	void PointToScreen(GdcPt2 &p);
	/// Convert a point form screen coordinates to view coordinates
	void PointToView(GdcPt2 &p);
	/// Get the x,y offset from the virtual window to the first real view in the parent chain
	bool WindowVirtualOffset(GdcPt2 *Offset);	
	/// Get the size of the window borders
	GdcPt2 &GetWindowBorderSize();
	/// Layout all the child views
	virtual bool Pour
	(
		/// The available space to lay out the views into
		GRegion &r
	) { return false; }

	/// The mouse was clicked over this view
	void OnMouseClick
	(
		/// The event parameters
		GMouse &m
	);
	/// Mouse moves into the area over the control
	void OnMouseEnter
	(
		/// The event parameters
		GMouse &m
	);
	/// Mouse leaves the area over the control
	void OnMouseExit
	(
		/// The event parameters
		GMouse &m
	);
	/// The mouse moves over the control
	void OnMouseMove
	(
		/// The event parameters
		GMouse &m
	);
	/// The mouse wheel was scrolled.
	bool OnMouseWheel
	(
		/// The amount scrolled
		double Lines
	);
	/// A key was pressed while this view has focus
	bool OnKey(GKey &k);
	/// The view is attached
	void OnCreate();
	/// The view is detached
	void OnDestroy();
	/// The view gains or loses the keyboard focus
	void OnFocus
	(
		/// True if the control is receiving focus
		bool f
	);
	/// \brief Called every so often by the timer system.
	/// \sa SetPulse()
	void OnPulse();
	/// Called when the view position changes
	void OnPosChange();
	/// Called on a top level window when something requests to close the window
	/// \returns true if it's ok to continue shutting down.
	bool OnRequestClose
	(
		/// True if the operating system is shutting down.
		bool OsShuttingDown
	);
	/// Return the type of cursor that should be visible when the mouse is at x,y
	/// e.g. #LCUR_Normal
	int OnHitTest
	(
		/// The x coordinate in view coordinates
		int x,
		/// The y coordinate in view coordinates
		int y
	);
	/// Called when the contents of the Children list have changed.
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	/// Called to paint the onscreen representation of the view
	void OnPaint(GSurface *pDC);
	/// \brief Called when a child view or view with it's SetNotify() set to this window changes.
	///
	/// The event by default will bubble up to the GWindow at the top of the window heirarchy visiting
	/// each GView on the way. If it reaches a GView that processes it then the event stops propergating
	/// up the heirarchy.
	int OnNotify(GViewI *Ctrl, int Flags);
	/// Called when a menu command is activated by the user.
	int OnCommand(int Cmd, int Event, OsView Wnd);
	/// Called after the view is attached to a new parent
	void OnAttach();
	
	/// Called to get layout information for the control. It's called
	/// up to 3 times to collect various dimensions:
	/// 1) PreLayout: Get the maximum width, and optionally the minimum width.
	///		Called with both widths set to zero.
	///		Must fill out Inf.Width.Max. Use -1 to fill all available space.
	///		Optionally fill out the min width.
	/// 2) Layout: Called to work out row height.
	///		Called with:
	///			- Width.Min unchanged from previous call.
	///			- Width.Max is limited to Cell size.
	///		Must fill out Inf.Height.Max.
	///		Min height currently not used.
	/// 3) PostLayout: Called to position view in cell.
	///		Not called.
	bool OnLayout(GViewLayoutInfo &Inf) { return false; }

	#if defined(_DEBUG)
	bool _Debug;
	void Debug();
	void _Dump(int Depth = 0);
	#endif
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Control or View window

/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_TOP				0x0001
/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_RIGHT			0x0002
/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_BOTTOM			0x0004
/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_LEFT			0x0008

/// Id of the vertical scroll bar in a GLayout control
#define IDC_VSCROLL				14000
/// Id of the horizontal scroll bar in a GLayout control
#define IDC_HSCROLL				14001

#ifdef MAC
#define XPLATFORM_GLAYOUT		1
#else
#define XPLATFORM_GLAYOUT		0
#endif

/// \brief A GView with scroll bars
///
/// This class adds scroll bars to the standard GView base class. The scroll bars can be
/// directly accessed using the VScroll and HScroll member variables. Although you should
/// always do a NULL check on the pointer before using, if the scroll bar is not activated
/// using GLayout::SetScrollBars then VScroll and/or HScroll will by NULL. When the scroll
/// bar is used to scroll the GLayout control you will receive an event on GView::OnNotify
/// with the control ID of the scrollbar, which is either #IDC_VSCROLL or #IDC_HSCROLL.
class LgiClass GLayout : public GView
{
	friend class GScroll;
	friend class GView;

	// Private variables
	bool			_SettingScrollBars;
	bool			_PourLargest;

protected:
	/// The vertical scroll bar
	GScrollBar		*VScroll;

	/// The horizontal scroll bar
	GScrollBar		*HScroll;

	/// Sets which of the scroll bars is visible
	virtual bool SetScrollBars
	(
		/// Make the horizontal scroll bar visible
		bool x,
		/// Make the vertical scroll bar visible
		bool y
	);
	
	#if defined(XPLATFORM_GLAYOUT)
	void AttachScrollBars();
	bool _SetScrollBars(bool x, bool y);
	#endif
	#if defined(MAC) && !XPLATFORM_GLAYOUT
	friend class GLayoutScrollBar;
	HISize Line;
	
	OsView RealWnd;
	bool _OnGetInfo(HISize &size, HISize &line, HIRect &bounds, HIPoint &origin);
	void _OnScroll(HIPoint &origin);
	void OnScrollConfigure();
	#endif

public:
	GLayout();
	~GLayout();

	const char *GetClass() { return "GLayout"; }

	/// Gets the current scroll bar values.
	virtual void GetScrollPos(int &x, int &y);
	/// Sets the current scroll bar values
	virtual void SetScrollPos(int x, int y);

	/// Gets the "pour largest" setting
	bool GetPourLargest();
	/// \brief Sets the "pour largest" setting
	///
	/// When "pour largest" is switched on the pour function automatically
	/// lays the control into the largest rectangle available. This is useful
	/// for putting a single GView into a splitter pane or a tab view and having
	/// it just take up all the space.
	void SetPourLargest(bool i);

	/// Handles the incoming events.
	GMessage::Result OnEvent(GMessage *Msg);

	/// Lay out all the children views into the client area according to their
	/// own internal rules. Space is given in a first come first served basis.
	bool Pour(GRegion &r);

	// Impl
	#if defined(__GTK_H__) || !defined(WINNATIVE)

	bool Attach(GViewI *p);
	bool Detach();
	GRect &GetClient(bool InClientSpace = true);
	void OnCreate();
	
	#if defined(MAC) && !XPLATFORM_GLAYOUT

	bool Invalidate(GRect *r = NULL, bool Repaint = false, bool NonClient = false);
	bool Focus();
	void Focus(bool f);
	bool SetPos(GRect &p, bool Repaint = false);

	#else
	
	void OnPosChange();
	int OnNotify(GViewI *c, int f);
	void OnNcPaint(GSurface *pDC, GRect &r);

	#endif
	#endif
	
	GViewI *FindControl(int Id);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Menus
#include "GMenu.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

/// The available states for a top level window
enum GWindowZoom
{
	/// Minimized
	GZoomMin,
	/// Restored/Normal
	GZoomNormal,
	/// Maximized
	GZoomMax
};

enum GWindowHookType
{
	GNoEvents = 0,
	/// \sa GWindow::RegisterHook()
	GMouseEvents = 1,
	/// \sa GWindow::RegisterHook()
	GKeyEvents = 2,
	/// \sa GWindow::RegisterHook()
	GKeyAndMouseEvents = GMouseEvents | GKeyEvents,
};

/// A top level window.
class LgiClass GWindow :
	public GView
#ifndef WIN32
	, public GDragDropTarget
#endif
{
	friend class BViewRedir;
	friend class GView;
	friend class GButton;
	friend class XWindow;
	friend class GDialog;
	friend class GApp;
	#if defined(MAC) && !defined(COCOA)
	friend pascal OSStatus LgiWindowProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
	#endif

	bool _QuitOnClose;

protected:
	#if WINNATIVE

	GRect OldPos;
	GWindow *_Dialog;

	#else

	OsWindow Wnd;
	void _OnViewDelete();
	void _SetDynamic(bool i);

	#endif

	#if defined BEOS

	friend class GMenu;
	friend class GView;

	#elif defined __GTK_H__

	friend class GMenu;	
	
	Gtk::GtkWidget *_Root, *_VBox, *_MenuBar;
	void _Paint(GSurface *pDC = 0, int Ox = 0, int Oy = 0);
	void OnGtkDelete();
	Gtk::gboolean OnGtkEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event);

	#endif
	
	#if defined(MAC) && !defined(LGI_SDL)
	void _Delete();
	#endif

	/// The default button
	GViewI *_Default;

	/// The menu on the window
	GMenu *Menu;

	class GWindowPrivate *d;

	void SetChildDialog(GDialog *Dlg);
	void SetDragHandlers(bool On);

public:
	#ifdef __GTK_H__
	GWindow(Gtk::GtkWidget *w = 0);
	#else
	GWindow();
	#endif
	~GWindow();

	const char *GetClass() { return "GWindow"; }

	/// Lays out the child views into the client area.
	virtual void Pour();

	/// Returns the current menu object
	GMenu *GetMenu() { return Menu; }
	
	/// Set the menu object.
	void SetMenu(GMenu *m) { Menu = m; }
	
	/// Set the window's icon
	bool SetIcon(const char *FileName);
	
	/// Gets the "quit on close" setting.
	bool GetQuitOnClose() { return _QuitOnClose; }

	/// \brief Sets the "quit on close" setting.
	///
	/// When this is switched on the application will quit the main message
	/// loop when this GWindow is closed. This is really useful for your
	/// main application window. Otherwise the UI will disappear but the
	/// application is still running.
	void SetQuitOnClose(bool i) { _QuitOnClose = i; }

	bool GetSnapToEdge();
	void SetSnapToEdge(bool b);
	
	/// Gets the current zoom setting
	GWindowZoom GetZoom();
	
	/// Sets the current zoom
	void SetZoom(GWindowZoom i);
	
	/// Raises the window to the top of the stack.
	void Raise();

	/// Moves a top level window on screen.
	void MoveOnScreen();
	/// Moves a top level to the center of the screen
	void MoveToCenter();
	/// Moves a top level window to where the mouse is
	void MoveToMouse();
	/// Moves the window to somewhere on the same screen as 'wnd'
	bool MoveSameScreen(GViewI *wnd);

	// Focus setting
	GViewI *GetFocus();
	enum FocusType
	{
		GainFocus,
		LoseFocus,
		ViewDelete
	};
	void SetFocus(GViewI *ctrl, FocusType type);
	
	/// Registers a watcher to receive OnView... messages before they
	/// are passed through to the intended recipient.
	bool RegisterHook
	(
		/// The target view.
		GView *Target,
		/// Combination of #GMouseEvents and #GKeyEvents OR'd together.
		GWindowHookType EventType,
		/// Not implemented
		int Priority = 0
	);

	/// Unregisters a hook target
	bool UnregisterHook(GView *Target);

	/// Gets the default view
	GViewI *GetDefault();
	/// Sets the default view
	void SetDefault(GViewI *v);

	/// Saves/loads the window's state, e.g. position, minimized/maximized etc
	bool SerializeState
	(
		/// The data store for reading/writing
		GDom *Store,
		/// The field name to use for storing settings under
		const char *FieldName,
		/// TRUE if loading the settings into the window, FALSE if saving to the store.
		bool Load
	);

	////////////////////// Events ///////////////////////////////
	
	/// Called when the window zoom state changes.
	virtual void OnZoom(GWindowZoom Action) {}
	
	/// Called when the tray icon is clicked. (if present)
	virtual void OnTrayClick(GMouse &m);

	/// Called when the tray icon menu is about to be displayed.
	virtual void OnTrayMenu(GSubMenu &m) {}
	/// Called when the tray icon menu item has been selected.
	virtual void OnTrayMenuResult(int MenuId) {}
	
	/// Called when files are dropped on the window.
	virtual void OnReceiveFiles(GArray<char*> &Files) {}
	
	/// Called when a URL is sent to the window
	virtual void OnUrl(const char *Url) {};

	///////////////// Implementation ////////////////////////////
	void OnPosChange();
	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	bool HandleViewMouse(GView *v, GMouse &m);
	bool HandleViewKey(GView *v, GKey &k);
	bool OnRequestClose(bool OsShuttingDown);
	bool Obscured();
	bool Visible();
	void Visible(bool i);
	bool IsActive();
	GRect &GetPos();

	#if !WINNATIVE
	
	bool Attach(GViewI *p);

	// Props
	OsWindow WindowHandle() { return Wnd; }
	bool Name(const char *n);
	char *Name();
	bool SetPos(GRect &p, bool Repaint = false);
	GRect &GetClient(bool InClientSpace = true);
	
	// D'n'd
	int WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState);
	int OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState);

	// Events
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	void OnCreate();
	virtual void OnFrontSwitch(bool b);

	#endif
	
	#if defined(MAC) && !defined(LGI_SDL)
	
	bool &CloseRequestDone();
	bool PostEvent(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0);
	void Quit(bool DontDelete = false);
	#ifndef COCOA
	OSErr HandlerCallback(DragTrackingMessage *tracking, DragRef theDrag);
	#endif
	int OnCommand(int Cmd, int Event, OsView Wnd);
	GViewI *WindowFromPoint(int x, int y, bool Debug = false);
	
	#elif defined __GTK_H__
	
	void OnMap(bool m);
	
	#endif
};

////////////////////////////////////////////////////////////////////////////

/// Puts a tool tip on screen when the mouse wanders over a region.
class LgiClass GToolTip : public GView
{
	class GToolTipPrivate *d;

public:
	GToolTip();
	~GToolTip();

	/// Create a tip
	int NewTip
	(
		/// The text to display
		char *Name,
		/// The region the mouse has to be in to trigger the tip
		GRect &Pos
	);
	
	/// Delete the tip.
	void DeleteTip(int Id);

	bool Attach(GViewI *p);
};

////////////////////////////////////////////////////////////////////////////
// Dialog stuff
#include "LgiWidgets.h"

////////////////////////////////////////////////////////////////////////////
// Progress meters stuff
#include "Progress.h"
#include "GProgress.h"

////////////////////////////////////////////////////////////////////////
#include "GFileSelect.h"
#include "GFindReplaceDlg.h"
#include "GToolBar.h"
#include "GThread.h"

////////////////////////////////////////////////////////////////////////////////////////////////

/// Displays 2 views side by side
class LgiClass GSplitter : public GLayout
{
	class GSplitterPrivate *d;

	void		CalcRegions(bool Follow = false);
	bool		OverSplit(int x, int y);

public:
	GSplitter();
	~GSplitter();

	const char *GetClass() { return "GSplitter"; }

	/// Get the position of the split in px
	int64 Value(); // Use to set/get the split position
	
	/// Sets the position of the split
	void Value(int64 i);

	/// True if the split is vertical
	bool IsVertical();
	
	/// Sets the split to horizontal or vertical
	void IsVertical(bool v);

	/// True if the split follows the opposite 
	bool DoesSplitFollow();
	
	/// Sets the split to follow the opposite 
	void DoesSplitFollow(bool i);

	/// Return the left/top view
	GView *GetViewA();

	/// Detach the left/top view
	void DetachViewA();

	/// Sets the left/top view
	void SetViewA(GView *a, bool Border = true);

	/// Return the right/bottom view
	GView *GetViewB();

	/// Detach the right/bottom view
	void DetachViewB();

	/// Sets the right/bottom view
	void SetViewB(GView *b, bool Border = true);

	/// Get the size of the bar that splits the views
	int BarSize();
	
	/// Set the bar size
	void BarSize(int i);

	bool Border();
	void Border(bool i);
	GViewI *FindControl(OsView hCtrl);

	bool Attach(GViewI *p);
	bool Pour(GRegion &r);
	void OnPaint(GSurface *pDC);
	void OnPosChange();
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	void OnMouseExit(GMouse &m);
	int OnHitTest(int x, int y);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	LgiCursor GetCursor(int x, int y);
};

////////////////////////////////////////////////////////////////////////////////////////////////
#define STATUSBAR_SEPARATOR			4
#define GSP_SUNKEN					0x0001

class LgiClass GStatusBar : public GLayout
{
	friend class GStatusPane;

protected:
	void RePour();

public:
	GStatusBar();
	~GStatusBar();

	const char *GetClass() { return "GStatusBar"; }
	bool Pour(GRegion &r);
	void OnPaint(GSurface *pDC);

	GStatusPane *AppendPane(const char *Text, int Width);
	bool AppendPane(GStatusPane *Pane);
};

class LgiClass GStatusPane :
	public GView
{
	friend class GStatusBar;

protected:
	int		Flags;
	int		Width;
	GSurface *pDC;

public:
	GStatusPane();
	~GStatusPane();

	const char *GetClass() { return "GStatusPane"; }
	char *Name() { return GBase::Name(); }
	bool Name(const char *n);
	void OnPaint(GSurface *pDC);

	int GetWidth();
	void SetWidth(int x);
	bool Sunken();
	void Sunken(bool i);
	GSurface *Bitmap();
	void Bitmap(GSurface *pdc);
};

/////////////////////////////////////////////////////////////////////////////////////////////
class LgiClass GCommand : public GBase //, public GFlags
{
	int			Flags;
	bool		PrevValue;

public:
	int			Id;
	GToolButton	*ToolButton;
	GMenuItem	*MenuItem;
	GKey		*Accelerator;
	char		*TipHelp;
	
	GCommand();
	~GCommand();

	bool Enabled();
	void Enabled(bool e);
	bool Value();
	void Value(bool v);
};

/////////////////////////////////////////////////////////////////////////
/// Put an icon in the system tray
class LgiClass GTrayIcon :
	public GBase
	// public GFlags
{
	friend class GTrayWnd;
	class GTrayIconPrivate *d;

public:
	/// Constructor
	GTrayIcon
	(
		/// The owner GWindow
		GWindow *p
	);
	
	~GTrayIcon();

	/// Add an icon to the list
	bool Load(const TCHAR *Str);
	
	/// Is it visible?
	bool Visible();

	/// Show / Hide the tray icon
	void Visible(bool v);

	/// The index of the icon visible
	int64 Value();

	/// Set the index of the icon you want visible
	void Value(int64 v);

	/// Call this in your window's OnEvent handler
	virtual GMessage::Result OnEvent(GMessage *Msg);
};

//////////////////////////////////////////////////////////////////
#include "GInput.h"
#include "GPrinter.h"

//////////////////////////////////////////////////////////////////

/// \brief A BeOS style alert window, kinda like a Win32 MessageBox
///
/// The best thing about this class is you can name the buttons very specifically.
/// It's always non-intuitive to word a question to the user in such a way so thats
/// it's obvious to answer with "Ok" or "Cancel". But if the user gets a question
/// with customised "actions" as buttons they'll love you.
///
/// The button pressed is returned as a index from the DoModal() function. Starting
/// at '1'. i.e. Btn2 -> returns 2.
class LgiClass GAlert : public GDialog
{
public:
	/// Constructor
	GAlert
	(
		/// The parent view
		GViewI *parent,
		/// The dialog title
		const char *Title,
		/// The body of the message
		const char *Text,
		/// The first button text
		const char *Btn1,
		/// The [optional] 2nd buttons text
		const char *Btn2 = 0,
		/// The [optional] 3rd buttons text
		const char *Btn3 = 0
	);

    void SetAppModal();
	int OnNotify(GViewI *Ctrl, int Flags);
};

/// Timer class to help do something every so often
class LgiClass DoEvery
{
	int64 LastTime;
	int64 Period;

public:
	/// Constructor
	DoEvery
	(
		/// Timeout in ms
		int p = 1000
	);
	
	/// Reset the timer
	void Init
	(
		/// Timeout in ms
		int p = -1
	);

	/// Returns true when the time has expired. Resets automatically.
	bool DoNow();
};

/// \brief Factory for creating view's by name.
///
/// Inherit from this to add a new factory to create objects. Override
/// NewView() to create your control.
class LgiClass GViewFactory
{
	/** \brief Create a view by name

		\code
		if (strcmp(Class, "MyControl") == 0)
		{
			return new MyControl;
		}
		\endcode
	*/
	virtual GView *NewView
	(
		/// The name of the class to create
		const char *Class,
		/// The initial position of the view
		GRect *Pos,
		/// The initial text of the view
		const char *Text
	) = 0;

public:
	GViewFactory();
	virtual ~GViewFactory();

	/// Create a view by name.
	static GView *Create(const char *Class, GRect *Pos = 0, const char *Text = 0);
};

//////////////////////////////////////////////////////////

// Colour
LgiFunc void LgiInitColours();
LgiFunc COLOUR LgiColour(int Colour);

// Graphics
LgiFunc void LgiDrawBox(GSurface *pDC, GRect &r, bool Sunken, bool Fill);
LgiFunc void LgiWideBorder(GSurface *pDC, GRect &r, LgiEdge Type);
LgiFunc void LgiThinBorder(GSurface *pDC, GRect &r, LgiEdge Type);
LgiFunc void LgiFlatBorder(GSurface *pDC, GRect &r, int Width = -1);

// Helpers
#ifdef __GTK_H__
extern Gtk::gboolean GtkViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, GView *This);
#endif

#ifdef LINUX
/// Ends a x windows startup session
LgiFunc void LgiFinishXWindowsStartup(class GViewI *Wnd);
#endif

/// \brief Displays a message box
/// \returns The button clicked. The return value is one of #IDOK, #IDCANCEL, #IDYES or #IDNO.
LgiFunc int LgiMsg
(
	/// The parent view or NULL if none available
	GViewI *Parent,
	/// The message's text. This is a printf format string that you can pass arguments to
	const char *Msg,
	/// The title of the message box window
	const char *Title = 0,
	/// The type of buttons below the message. Can be one of:
	/// #MB_OK, #MB_OKCANCEL, #MB_YESNO or #MB_YESNOCANCEL.
	int Type = MB_OK,
	...
);

/// Contains all the infomation about a display/monitor attached to the system.
/// \sa LgiGetDisplays
struct GDisplayInfo
{
	/// The position and dimensions of the display. On windows the left/upper 
	/// most display will be positioned at 0,0 and each furthur display will have 
	/// co-ordinates that join to one edge of that initial rectangle.
	GRect r;
	/// The number of bits per pixel
	int BitDepth;
	/// The refreash rate
	int Refresh;
	/// The device's path, system specific
	char *Device;
	/// A descriptive name of the device, usually the video card
	char *Name;
	/// The name of any attached monitor
	char *Monitor;

	GDisplayInfo()
	{
		r.ZOff(-1, -1);
		BitDepth = 0;
		Refresh = 0;
		Device = 0;
		Name = 0;
		Monitor = 0;
	}

	~GDisplayInfo()
	{
		DeleteArray(Device);
		DeleteArray(Name);
		DeleteArray(Monitor);
	}
};

/// Returns infomation about the displays attached to the system.
/// \returns non-zero on success.
LgiFunc bool LgiGetDisplays
(
	/// [out] The array of display info structures. The caller should free these
	/// objects using Displays.DeleteObjects().
	GArray<GDisplayInfo*> &Displays,
	/// [out] Optional bounding rectangle of all displays. Can be NULL if your don't
	/// need that information.
	GRect *AllDisplays = 0
);

/// This class makes it easy to profile a function and write out timings at the end
class LgiClass GProfile
{
	struct Sample
	{
		uint64 Time;
		const char *Name;
		
		Sample(uint64 t = 0, const char *n = 0)
		{
			Time = t;
			Name = n;
		}
	};
	
	GArray<Sample> s;
	int MinMs;
	
public:
	GProfile(const char *Name);
	virtual ~GProfile();
	
	void HideResultsIfBelow(int Ms);
	virtual void Add(const char *Name);
};

#endif


