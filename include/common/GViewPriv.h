#ifndef _GVIEW_PRIV_
#define _GVIEW_PRIV_

#if WINNATIVE
	#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x501
	#endif

	#include "commctrl.h"
	#include "Uxtheme.h"
#endif

#ifdef MAC
extern OsThread LgiThreadInPaint;
#endif

#define PAINT_VIRTUAL_CHILDREN	1
#define DEBUG_CAPTURE			0

extern bool In_SetWindowPos;
extern GMouse &lgi_adjust_click(GMouse &Info,
								GViewI *Wnd,
								bool Capturing = false,
								bool Debug = false);

class GViewIter : public GViewIterator
{
	GView *v;
	List<GViewI>::I i;

public:
	GViewIter(GView *view);
	~GViewIter() {}
	GViewI *First();
	GViewI *Last();
	GViewI *Next();
	GViewI *Prev();
	size_t Length();
	ssize_t IndexOf(GViewI *view);
	GViewI *operator [](ssize_t Idx);
};

#if !WINNATIVE
#include "LThreadEvent.h"

class GPulseThread : public LThread
{
	GView *View;
	int Length;
	LThreadEvent Event;

public:
	bool Loop;

	GPulseThread(GView *view, int len) : LThread("GPulseThread"), Event("GPulseThread")
	{
		LgiAssert(view);
		
		Loop = true;
		View = view;
		Length = len;
		
		Run();
	}
	
	~GPulseThread()
	{
		Loop = false;
		View = NULL;
		printf("Signal pulse %i\n", GetId());
		Event.Signal();

		while (!IsExited())
			LgiSleep(0);
		printf("Pulse done %i\n", GetId());
	}
	
	int Main()
	{
		while (Loop && LgiApp)
		{
			auto s = Event.Wait(Length);
			if (!Loop || s == LThreadEvent::WaitError)
				break;
			
			if (View && !View->PostEvent(M_PULSE))
				Loop = false;
		}
		
		return 0;
	}
};
#endif

enum GViewFontType
{
	/// The GView has a pointer to an externally owned font.
	GV_FontPtr,
	/// The GView owns the font object, and must free it.
	GV_FontOwned,
	/// The GApp's font cache owns the object. In this case,
	/// calling GetCssStyle on the GView will invalidate the
	/// font ptr causing it to be re-calculated.
	GV_FontCached,
};

class GViewPrivate
{
public:
	// General
	int				CtrlId;
	GDragDropSource	*DropSource;
	GDragDropTarget	*DropTarget;
	bool			IsThemed;

	// Heirarchy
	GViewI			*ParentI;
	GView			*Parent;
	GViewI			*Notify;

	// Size
	GdcPt2			MinimumSize;
	
	// Font
	GFont			*Font;
	GViewFontType	FontOwnType;
	
	// Style
	GAutoPtr<GCss>  Css;
	GString::Array	Classes;

	// Event dispatch handle
	int				SinkHnd;

	// OS Specific
	#if WINNATIVE

		static OsView	hPrevCapture;
		int				WndStyle;				// Win32 hWnd Style
		int				WndExStyle;				// Win32 hWnd ExStyle
		int				WndDlgCode;				// Win32 Dialog Code (WM_GETDLGCODE)
		GString			WndClass;
		UINT_PTR		TimerId;
		HTHEME			hTheme;

	#else

		// Cursor
		GPulseThread	*Pulse;
		GView			*Popup;
		bool			TabStop;
		bool			WantsFocus;
		int				WantsPulse;

		#if defined __GTK_H__
		bool			InPaint;
		bool			GotOnCreate;
		GString			ActualClass;
		#elif defined(MAC) && !defined(COCOA)
		static HIObjectClassRef BaseClass;
		#endif

	#endif

	#if defined(MAC)
		#ifdef COCOA
		#else
		EventHandlerRef DndHandler;
		GAutoString AcceptedDropFormat;
		#endif
	#endif
	
	#if defined(LGI_SDL)
	SDL_TimerID PulseId;
	int PulseLength;
	#endif
	
	GViewPrivate();
	~GViewPrivate();
	
	GView *GetParent()
	{
		if (Parent)
			return Parent;
		
		if (ParentI)
			return ParentI->GetGView();

		return 0;
	}
};

#endif
