#include "Lgi.h"
#include "xapplication.h"
#include "GString.h"
#include "xpopup.h"
#include "LgiClasses.h"

GArray<Window> Deleted;

XWidgetDestroyNotify XWidget::DestroyNotifyHandlers[4] = {0, 0, 0, 0};

enum MapState
{
	Unmapped,
	Mapping,
	Unmapping,
	Mapped,	
};

class XWidgetPrivate : public XObject
{
public:
	enum WaitState
	{
		WaitNone,
		WaitMapped,
		WaitExpose,
		WaitExiting
	} State;
	
	char *StateName(WaitState s)
	{
		switch (s)
		{
			case WaitNone: return "WaitNone";
			case WaitMapped: return "WaitMapped";
			case WaitExpose: return "WaitExpose";
			case WaitExiting: return "WaitExiting";
		}
		
		return 0;
	}
	
	static OsPoint *DecorationSize;

	XWidget *Widget;
	XWidget *Parent;
	Window Win;
	char *Text;
	int x, y, w, h;
	bool PosDirty, InitResize;
	XIC InputContext;

	bool Exposed;				// true if the widget has received an expose event (ie visible)
	bool Enabled;				// our own enabled start
	bool TabStop;				// is a tabstop
	int WantKeys;
	bool Debug;					// Print debugging info for this window
	bool AddToList;
	bool Popup;

	// Mapping State
	MapState Map;
	char *MapName()
	{
		switch (Map)
		{
			case Unmapped: return "Unmapped";
			case Mapping: return "Mapping";
			case Unmapping: return "Unmapping";
			case Mapped: return "Mapped";
		}
		return "(error)";
	}

	/*
	int Rects;					// Clip region
	XRectangle *Rect;
	*/
	GRegion *Clip;

	XList<XWidget> Children;

	XWidget *Top()
	{
	    XWidget *p = Widget;
		while (p->d->Parent) p = p->d->Parent;
		return p;
	}

	XWidgetPrivate(XWidget *This, bool top, Window Existing)
	{
		InputContext = 0;
		PosDirty = false;
		TabStop = false;
		WantKeys = 0;
		Widget = This;
		Parent = 0;
		Text = 0;
		x = y = -3000;
		w = h = 1;
		Map = Unmapped;
		Enabled = true;
		Exposed = false;
		Debug = false;
		State = WaitNone;
		Clip = 0;
		AddToList = true;
		Popup = false;
		InitResize = false;

		if (Existing)
		{
			Win = Existing;
		}
		else
		{
			// Create the window
			XSetWindowAttributes v;
			v.cursor = XNone;
			v.background_pixmap = XNone;
			v.event_mask =			ExposureMask |
									KeyPressMask | KeyReleaseMask |
									ButtonPressMask | ButtonReleaseMask |
									PointerMotionMask |
									EnterWindowMask | LeaveWindowMask |
									StructureNotifyMask |
									PropertyChangeMask |
									(top ? FocusChangeMask : 0);

			Win = XCreateWindow(	XDisplay(),
									RootWindow(XDisplay(), 0),
									x, y, w, h,
									0,								// border width
									CopyFromParent,					// depth
									InputOutput,					// class
									XDefaultVisual(XDisplay(), 0),	// visual
									CWBackPixmap | CWEventMask | CWCursor,
									&v);
		}
	}

	~XWidgetPrivate()
	{
		Destroy();
	}

	void Destroy()
	{
		if (InputContext)
		{
			XDestroyIC(InputContext);
			InputContext = 0;
		}
		
		if (Win)
		{
			// DebugLog(_FL, "destroy\n");

			if (WaitForNone(3000))
			{
				if (XWidget::MouseGrabber == Widget)
				{
					printf("%s:%i - Ungrabbing window to delete it.\n");
					Widget->ungrabMouse();
				}
				
				for (int i=0; i<CountOf(XWidget::DestroyNotifyHandlers); i++)
				{
					if (XWidget::DestroyNotifyHandlers[i])
						XWidget::DestroyNotifyHandlers[i](Widget);
				}
				
				// printf("XDestroyWindow %p\n", Win);
				if (Deleted.IndexOf(Win) < 0)
					Deleted.Add(Win);
				
				XDestroyWindow(XDisplay(), Win);
				Win = 0;
			}
			else
			{
				printf("XWidgetPrivate::Destroy() couldn't get exit loops.\n");
				#ifdef _DEBUG
				int *i = 0;
				*i = 0;
				#endif
			}
		}

		DeleteArray(Text);
		DeleteObj(Clip);
	}

	bool ParentChainMap()
	{
		for (XWidgetPrivate *p=this; p; p=p->Parent ? p->Parent->d : 0)
		{
			if (NOT p->Map == Mapped AND NOT p->Map == Mapping) return false;
		}
		
		return true;
	}
	
	bool WaitForNone(int Time)
	{
		if (State != WaitNone)
		{
			WaitState Old = State; 
			State = WaitExiting;
			
			int Start = LgiCurrentTime();
			while (State != WaitNone)
			{
				LgiSleep(5);

				int Now = LgiCurrentTime();
				if (Now > Start + Time)
				{
					printf("%s,%i - WaitNone timeout at +%ims (%i) State=%s Txt=%s\n", __FILE__, __LINE__, Now-Start, Time, StateName(Old), Text);
					break;
				}
			}
		}

		return State == WaitNone;
	}

	bool WaitForMapped(int Time)
	{
		int Start = LgiCurrentTime();
		if (Map == Mapping AND State != WaitMapped)
		{
			if (ParentChainMap())
			{
				if (WaitForNone(Time))
				{
					State = WaitMapped;
					while (	State == WaitMapped AND
							(Map == Mapping OR Map == Unmapping) AND
							LgiCurrentTime() < Start + Time)
					{
						XApp()->processEvents();
					}
					
					if (Map == Mapping)
					{
						printf("%s,%i - WaitForMapped timed out (%i,%i).\n", __FILE__, __LINE__, Mapping, Mapped);
					}
					
					State = WaitNone;
				}
				else
				{
					printf("%s,%i - WaitForNone timed out.\n", __FILE__, __LINE__);
				}
			}
			else
			{
				// printf("%s,%i - WaitForMapped, parent isn't visible yet.\n", __FILE__, __LINE__);
			}
		}

		return Map == Mapped;
	}

	void WaitForExpose(int Time)
	{
		if (Mapped AND WaitForNone(Time))
		{
			int Start = LgiCurrentTime();
			
			State = WaitExpose;
			while (State == WaitExpose AND Mapped)
			{
				XEvent e;
				MapWinUsage(Win);
				if (XCheckTypedWindowEvent(XDisplay(), Win, Expose, &e))
				{
					Start = LgiCurrentTime();
					XApp()->onEvent(e);

					// We got our expose, now leave
					break;
				}
				else
				{
					LgiSleep(5);

					int Now = LgiCurrentTime();
					if (LgiCurrentTime() > Start + Time)
					{
						break;
					}
				}
			}
			
			State = WaitNone;
		}
	}
	
	void PollExpose()
	{
		XEvent e;
		while (XCheckTypedEvent(XDisplay(), Expose, &e))
		{
			XApp()->onEvent(e);
		}
	}

	OsPoint *GetDecorationSize()
	{
		if (NOT Parent AND
			NOT DecorationSize)
		{
			Window Root = 0, Wind = 0, Parent = 0, *Child = 0;
			int tx = 0, ty = 0;
			uint Children;

			MapWinUsage(Win);
			XQueryTree(XDisplay(), Win, &Root, &Parent, &Child, &Children);
			if (Child) XFree(Child);
			if (Parent)
			{
				XQueryTree(XDisplay(), Parent, &Root, &Parent, &Child, &Children);
				if (Child) XFree(Child);
				if (Parent)
				{
					MapWinUsage(Win);
					XTranslateCoordinates(	XDisplay(),
											Parent,
											Win,
											0,
											0,
											&tx, &ty,
											&Wind);

					DecorationSize = NEW(OsPoint(tx, ty));
				}
				else
				{
					// printf("%s,%i - No parent.\n", __FILE__, __LINE__);
				}
			}
			else
			{
				// printf("%s,%i - No parent.\n", __FILE__, __LINE__);
			}
		}
		
		return DecorationSize;
	}
};

class XWidgetList : public GSemaphore
{
	XList<XWidget> Widgets;

public:
	XWidget *Find(Window f)
	{
		XWidget *Status = 0;
	
		if (Lock(_FL))
		{
			for (XWidget *w = Widgets.First(); w; w = Widgets.Next())
			{
				if (w->d->Win == f)
				{
					Status = w;
					break;
				}
			}
		
			Unlock();
		}

		return Status;
	}
	
	XWidget *First()
	{
		XWidget *Status = 0;
		
		if (Lock(_FL))
		{
			Status = Widgets.First();
			Unlock();
		}
		
		return Status;
	}
	
	void Insert(XWidget *w)
	{
		if (Lock(_FL))
		{
			Widgets.Insert(w);
			Unlock();
		}
	}

	void Delete(XWidget *w)
	{
		if (Lock(_FL))
		{
			Widgets.Delete(w);
			Unlock();
		}
	}
	
} AllWidgets;

OsPoint *XWidgetPrivate::DecorationSize = 0;
XWidget *XWidget::OwnsClipboard = 0;

XWidget::XWidget(Window Existing, bool AddToList)
{
	_event = 0;
	d = NEW(XWidgetPrivate(this, false, Existing));
	
	d->AddToList = AddToList;
	if (AddToList)
	{
		AllWidgets.Insert(this);
	}
}

XWidget::XWidget(char *name, bool top)
{
	_event = 0;
	d = NEW(XWidgetPrivate(this, top, 0));
	
	if (XApp()->Lock(_FL))
	{
		AllWidgets.Insert(this);
		XApp()->Unlock();
	}
}

XWidget::~XWidget()
{
	if (d->AddToList AND
		XApp()->Lock(_FL))
	{
		AllWidgets.Delete(this);
		XApp()->Unlock();
	}
		
	DebugLog(_FL, "~XWidget\n");

	if (XApplication::WantsFocus == this)
	{
		XApplication::WantsFocus = 0;
	}
	
	if (MouseGrabber == this)
	{
		MouseGrabber->ungrabMouse();
		MouseGrabber = 0;
	}

	if (d->Parent)
	{
		d->Parent->d->Children.Delete(this);
	}

	for (XWidget *c=d->Children.First(); c; c=d->Children.Next())
	{
		// printf("~XWidget clearing childs ptr to me=%p, child=%p childdata=%p\n", this, c, c->d);
		if (c->d)
		{
			c->d->Parent = 0;
		}
	}

	XApp()->OnDeleteWidget(this);
	DeleteObj(d);
}

#include <stdarg.h>
int XWidget::DebugLog(char *file, int line, char *msg, ...)
{
	if (d->Debug)
	{
		va_list Arg;
		va_start(Arg, msg);
		char *f = strrchr(file, DIR_CHAR);
		printf("%p(%s:%i) ", this, f ? f + 1 : file, line);
		int ch = vprintf(msg, Arg);
		va_end(Arg);
		return ch;
	}
	
	return -1;
}

XIC XWidget::GetInputContext()
{
	if (NOT d->InputContext)
	{
		char *ClassName = "LgiApp";
		
		d->InputContext = XCreateIC(XApp()->GetInputMethod(),
									XNClientWindow, handle(),
									XNFocusWindow, handle(),
									XNInputStyle, XIMPreeditNothing  | XIMStatusNothing,
									XNResourceName, ClassName,
									XNResourceClass, ClassName,
									NULL);

	}
	
	return d->InputContext;
}

XMainWindow *XWidget::GetWindow()
{
	XWidget *Top = this;

	while (Top AND Top->d->Parent)
	{
		Top = Top->d->Parent;
	}	
	
	return dynamic_cast<XMainWindow*>(Top);
}

XWidget *XWidget::FirstWidget()
{
	return AllWidgets.First();
}

XWidget *XWidget::Find(Window f)
{
	return AllWidgets.Find(f);
}

void XWidget::detachHandle()
{
	d->Win = 0;
}

Window XWidget::handle()
{
	if (Deleted.HasItem(d->Win))
	{
		printf("%s:%i - deleted handle!!! (%p)\n", __FILE__, __LINE__, d->Win);
	}

	return d ? d->Win : 0;
}

bool XWidget::IsPopup()
{
	for (XWidget *w = this; w; w = w->d->Parent)
	{
		if (d->Popup)
		{
			return true;
		}
	}
	
	return false;
}

bool XWidget::IsMouseOver(XlibEvent *e)
{
	if (e)
	{
		OsPoint p;
		p = mapToGlobal(p);
		GRect r;
		r.ZOff(width()-1, height()-1);
		r.Offset(p.x, p.y);
		return r.Overlap(e->ScreenX(), e->ScreenY());
	}

	return false;
}

void XWidget::_SetDeleteMe()
{
	if (NOT XApp()->Delete.HasItem(this))
	{
		XApp()->Delete.Insert(this);
	}
}

void XWidget::_dump(int i)
{
	d->Debug = true;
	DebugLog(_FL, "XWidget name=%s exposed=%i map=%s\n", d->Text, d->Exposed, d->MapName());
}

GRegion *XWidget::_GetClipRgn()
{
	return d->Clip;
}

void XWidget::_SetClipRgn(GRegion *c)
{
	DeleteObj(d->Clip);
	d->Clip = c;
}

bool XWidget::_isdebug()
{
	return d->Debug;
}

void XWidget::_paint(XExposeEvent *e, XlibEvent *q)
{
	LgiAssert(this != 0 AND this->d != 0);
	
	DebugLog(_FL, "_paint pos=%i,%i-%i,%i map=%s\n", d->x, d->y, d->w, d->h, d->MapName());

	if (!d->InitResize)
	{
		d->InitResize = true;
		DebugLog(_FL, "    init resizing\n");
		resizeEvent(0);
	}

	if (d->Map == Mapping)
	{
		d->Map = Mapped;
	}
	
	if (d->Map == Mapped)
	{
		d->Exposed = true;

		if (this == XApplication::WantsFocus)
		{
			// at least we're garenteed to be visible
			XApplication::WantsFocus->setFocus();
			XApplication::WantsFocus = 0;
		}

		paintEvent(q);
	}
}

bool XWidget::_wait_mapped(int timeout)
{
	return d->WaitForMapped(timeout);
}

void XWidget::_map(bool m)
{
	DebugLog(_FL, "_map(%i) PosDirty=%i\n", m, d->PosDirty);

	d->Map = m ? Mapped : Unmapped;

	if (m)
	{
		if (d->PosDirty)
		{
			DebugLog(_FL, "re-setting pos in _map %i,%i,%i,%i resize=%i\n",
				d->x, d->y, d->w, d->h, d->InitResize);
				
			setGeometry(d->x, d->y, d->w, d->h);
		}
	}
	else
	{
		d->Exposed = false;
	}
}

// Notification of window resized
void XWidget::_resize(XConfigureEvent *e, XlibEvent *q)
{
	if (d->Map != Mapped)
	{
		DebugLog(_FL, "dropping pre-map resize\n");
		return;
	}

	DebugLog(_FL, "_resize() Par=%p Pos=%i,%i,%i,%i Dirty=%i, Map=%s\n",
		d->Parent, e->x, e->y, e->width, e->height,
		d->PosDirty, d->MapName());


	if (NOT d->Parent)
	{
		// OsPoint *DecSize = d->GetDecorationSize();

		if (d->x != e->x OR
			d->y != e->y OR
			d->w != e->width OR
			d->h != e->height)
		{
			d->x = e->x;
			d->y = e->y;
			d->w = e->width;
			d->h = e->height;
			
			resizeEvent(q);
		}
	}
}

int XWidget::wantKeys()
{
	return d->WantKeys;
}

void XWidget::wantKeys(int keys)
{
	d->WantKeys = keys;
}

bool XWidget::GetPopup()
{
	return d->Popup;
}

void XWidget::SetPopup(bool b)
{
	d->Popup = b;
	
	XSetWindowAttributes a;
	a.override_redirect = true;
	MapWinUsage(d->Win);
	XChangeWindowAttributes(XWidget::XDisplay(), d->Win, CWOverrideRedirect, &a);
}

bool XWidget::isTabStop()
{
	return d->TabStop;
}

void XWidget::isTabStop(bool i)
{
	d->TabStop = i;
}

bool XWidget::hasFocus()
{
    XWidget *p = d->Top();

	// printf("p=%p FocusWindow=%p Top=%p\n", p, XApp()->GetFocusWindow(), d->Top());
	if (p AND
		(XApp()->GetFocusWindow() == p OR XApp()->GetFocusWindow() == 0))
	{
		if (d->Top() == p)
		{
			return XApp()->GetFocus() == this;
		}
	}
	
	return false;
}

void XWidget::setFocus()
{
    XWidget *p = d->Top();
    
	if (p)
	{
		if (p->d->Win AND
			p->d->Exposed AND
			p->d->Map == Mapped)
		{
			MapWinUsage(p->d->Win);
			int e = XSetInputFocus(	XDisplay(),
									p->d->Win,
									XNone,
									CurrentTime);
		}
		else
		{
			XApplication::WantsFocus = this;
		}
		
		if (GetWindow())
		{
			GetWindow()->SetLastFocus(this);
		}
	}
	
	XApp()->SetFocus(this);
}

void XWidget::clearFocus()
{
	XApp()->SetFocus(0);
}

int XWidget::height()
{
	return d->h;
}

int XWidget::width()
{
	return d->w;
}

void XWidget::setIcon(char *Path)
{
	if (Path)
	{
		int Old = GdcD->SetOption(GDC_PROMOTE_ON_LOAD, 0);
		GSurface *Icon = LoadDC(Path);
		GdcD->SetOption(GDC_PROMOTE_ON_LOAD, Old);
		if (Icon)
		{
			int Len = Icon->X() * Icon->Y() + 2;
			uint32 *Buf = NEW(uint32[Len]);
			if (Buf)
			{
				// Slap the X,Y in the first 2 32bit values
				Buf[0] = Icon->X();
				Buf[1] = Icon->Y();

				// Convert the bits to the right ARGB format.
				for (int y=0; y<Icon->Y(); y++)
				{
					uint32 *p = Buf + (y * Icon->X()) + 2;
					for (int x=0; x<Icon->X(); x++)
					{
						uint32 c = Icon->GetBits() == 32 ? Icon->Get(x, y) : CBit(32, Icon->Get(x, y), Icon->GetBits(), Icon->Palette());
						p[x] = (A32(c) << 24) | (R32(c) << 16) | (G32(c) << 8) | B32(c);
					}
				}

				// Set the property
				MapWinUsage(d->Win);
				XChangeProperty(XDisplay(),
								d->Win,
								XApp()->XA_NetWmIcon,
								XApp()->XA_Cardinal,
								32,
								PropModeReplace,
								(uchar*)Buf,
								Len);

				DeleteArray(Buf);
			}

			DeleteObj(Icon);
		}
	}
}

void XWidget::setGeometry(int X, int Y, int W, int H)
{
	int OldX = d->x;
	int OldY = d->y;
	int OldW = d->w;
	int OldH = d->h;
	bool Changed =	d->x != X OR
					d->y != Y OR
					d->w != W OR
					d->h != H;
	
	if (Changed)
	{
		// DebugLog(_FL, "setGeo (%i,%i-%i,%i) -> (%i,%i-%i,%i) Mapping=%s\n", d->x, d->y, d->w, d->h, X, Y, W, H, d->MapName());
	}
	
	d->x = X;
	d->y = Y;
	d->w = max(W, 1);
	d->h = max(H, 1);

	if (d->Map != Mapped)
	{
		DebugLog(_FL, "setGeo not mapped yet (%i,%i-%i,%i) Mapping=%s\n", d->x, d->y, d->w, d->h, d->MapName());
		d->PosDirty = true;
		return;
	}

	if (d->Win)
	{
		MapWinUsage(d->Win);

		DebugLog(_FL, "XMoveResizeWindow(%i,%i-%i,%i) old=%i,%i-%i,%i Mapping=%s\n",
			d->x, d->y, d->w, d->h,
			OldX, OldY, W, H, d->MapName());
		int Err = XMoveResizeWindow(XDisplay(), d->Win, d->x, d->y, d->w, d->h);
		if (Err != 1)
		{
			printf("%s,%i - XMoveResizeWindow failed: %s\n", __FILE__, __LINE__, XErr(Err));
		}

		XConfigureEvent e;
		e.type = ConfigureNotify;
		XlibEvent q((XEvent*) &e);
		resizeEvent(&q);
		
		if (d->Map == Mapping)
		{
			d->PosDirty = d->PosDirty OR Changed;
			DebugLog(_FL, "setGeo while mapping, PosDirty=%i, newpos=%i,%i,%i,%i\n",
				d->PosDirty, X, Y, W, H);
		}
		else
		{
			d->PosDirty = false;
		}
	}
	else
	{
		printf("%s,%i - Not setting pos, map=%s (%i,%i - %i,%i)\n",
				__FILE__, __LINE__,
				d->MapName(),
				d->x, d->y, d->w, d->h);
	}
}

void XWidget::GetDecorationSize(int &x, int &y)
{
	OsPoint *Ds = d->GetDecorationSize();
	x = Ds ? Ds->x : 0;
	y = Ds ? Ds->y : 0;
}

GRect &XWidget::geometry()
{
	static GRect r;
	r.Set(d->x, d->y, d->x + d->w - 1, d->y + d->h - 1);
	return r;
}

void XWidget::setBackgroundMode(ViewBackground b)
{
}

void XWidget::repaint()
{
	if (d->Win AND d->Map == Mapped)
	{
		DebugLog(_FL, "repaint\n");

		MapWinUsage(d->Win);
		int e = XClearArea(	XDisplay(),
							d->Win,
							0, 0,
							d->w + 1, d->h + 1,
							true);
		// printf("XClearArea(%i)=%i\n", d->Win, e);
		d->PollExpose();
	}
}

void XWidget::repaint(int x, int y, int wid, int ht)
{
	if (d->Win AND d->Map == Mapped)
	{
		DebugLog(_FL, "repaint(x, y)\n");

		GRect r(x, y, x+wid, y+ht);
		GRect k(0, 0, d->w, d->h);
		if (r.Overlap(&k))
		{
			MapWinUsage(d->Win);
			int e = XClearArea(	XDisplay(),
								d->Win,
								x, y,
								wid + 1, ht + 1,
								true);
			// printf("XClearArea(%i, %i, %i, %i, %i)=%i\n", d->Win, x, y, wid + 1, ht + 1, e);
			d->PollExpose();
		}
		else
		{
			/*
			printf("XWidget::repaint(%i,%i,%i,%i) No overlap (%i,%i)\n",
				x, y, wid, ht,
				d->w, d->h);
			*/
		}
	}
}

void XWidget::update()
{
	if (d->Win AND
		d->Map == Mapped)
	{
		DebugLog(_FL, "update\n");

		MapWinUsage(d->Win);
		XClearArea(	XDisplay(),
					d->Win,
					0, 0,
					d->w + 1, d->h + 1,
					true);
	}
}

void XWidget::update(int x, int y, int wid, int ht)
{
	if (d->Win AND
		d->Map == Mapped)
	{
		DebugLog(_FL, "update(%i,%i-%i,%i)\n", x, y, wid, ht);

		GRect r(x, y, x+wid, y+ht);
		GRect k(0, 0, d->w, d->h);
		if (r.Overlap(&k))
		{
			MapWinUsage(d->Win);
			XClearArea(	XDisplay(),
						d->Win,
						x, y,
						wid + 1, ht + 1,
						true);
		}
	}
}

bool XWidget::isEnabled()
{
	return d->Enabled;
}

void XWidget::setEnabled(bool i)
{
	d->Enabled = i;
}

bool XWidget::isVisible()
{
	return d->Map == Mapped;
}

void XWidget::show(bool Raise)
{
	if (d->Win AND NOT d->WaitForMapped(200))
	{
		int e;
		
		DebugLog(_FL, "show\n");
		
		if (NOT parentWidget())
		{
			XSizeHints *s = XAllocSizeHints();
			if (s)
			{
				s->flags = USPosition | USSize;
				s->x = d->x;
				s->y = d->y;
				s->width = d->w;
				s->height = d->h;
				
				MapWinUsage(d->Win);
				XSetWMNormalHints(XDisplay(), d->Win, s);
				
				XFree(s);
			}
		}
		else
		{
			d->PosDirty = true;
		}

		if (Raise)
		{
			MapWinUsage(d->Win);
			e = XMapRaised(XDisplay(), d->Win);
		}
		else
		{
			MapWinUsage(d->Win);
			e = XMapWindow(XDisplay(), d->Win);
		}
		
		if (e != 1)
		{
			printf("%s,%i - XMapWindow=%s\n", __FILE__, __LINE__, XErr(e));
		}
		
		d->Map = Mapping;
	}
	else
	{
		printf("XWidget::show() failed, Win=%i Map=%s Text=%s\n", d->Win, d->MapName(), d->Text);
	}
}

void XWidget::hide()
{
	if (d->Win AND
		(d->Map == Mapped OR d->Map == Mapping))
	{
		d->Exposed = false;
		d->Map = Unmapping;

		DebugLog(_FL, "hide\n");

		MapWinUsage(d->Win);
		XUnmapWindow(XDisplay(), d->Win);
	}
}

OsPoint &XWidget::mapFromGlobal(OsPoint &p)
{
	if (d->Win)
	{
		Window Win;
		int x, y;

		MapWinUsage(d->Win);
		if (XTranslateCoordinates(	XDisplay(),
									RootWindow(XDisplay(), 0),
									d->Win,
									p.x,
									p.y,
									&x, &y,
									&Win))
		{
			p.set(x, y);
		}
	}

	return p;
}

OsPoint &XWidget::mapToGlobal(OsPoint &p)
{
	if (d->Win)
	{
		Window Win;
		int x, y;

		MapWinUsage(d->Win);
		if (XTranslateCoordinates(	XDisplay(),
									d->Win,
									RootWindow(XDisplay(), 0),
									p.x,
									p.y,
									&x, &y,
									&Win))
		{
			p.set(x, y);
		}
	}

	return p;
}

void XWidget::setParentWidget(XWidget *p)
{
	d->Parent = p;
}

XWidget *XWidget::parentWidget()
{
	return d->Parent;
}

XList<XWidget> *XWidget::childrenWidgets()
{
	return &d->Children;
}

bool XWidget::reparent(XWidget *parent, OsPoint &p, bool showNow)
{
	Window ParWin = parent ? parent->d->Win : RootWindow(XDisplay(), 0);

	if (NOT ParWin)
	{
		printf("Error: XWidget::reparent(%p, [%i,%i], %i)\n", parent, p.x, p.y, showNow);
		printf("\tParent->Win=%i\n", parent ? parent->d->Win : 0);
	}
	else
	{
		// Remove from existing location
		if (d->Parent)
		{
			// printf("delete parent ptr this=%p d->Parent=%p d->Parent->d=%p\n", this, d->Parent, d->Parent->d);
			if (d->Parent->d) d->Parent->d->Children.Delete(this);
			else printf("delete parent ptr has bad data part this=%p parent=%p\n", this, d->Parent);
		}

		if (d->Win)
		{
			hide();
		}

		// Add to new location
		d->x = p.x;
		d->y = p.y;
		d->Parent = parent;

		if (d->Parent)
		{
			XList<XWidget> *l = &d->Parent->d->Children;
			
			bool Has = false;
			for (XWidget *	w = l->First();
							w;
							w = l->Next())
			{
				if (w == this)
				{
					Has = true;
					break;
				}
			}
			
			if (NOT Has)
			{
				d->Parent->d->Children.Insert(this);
			}
		}

		DebugLog(_FL, "reparent(%i,[%i,%i],%i)\n", ParWin, p.x, p.y, showNow);
		
		MapWinUsage(d->Win);
		XReparentWindow(XDisplay(), d->Win, ParWin, d->x, d->y);

		if (showNow)
		{
			show();
		}

		return true;
	}

	return false;
}

char *XWidget::getText()
{
	return d->Text;
}

void XWidget::setText(char *text)
{
	char *t = NewStr(text);
	DeleteArray(d->Text);
	d->Text = t;

	DebugLog(_FL, "settext\n", this);
	
	MapWinUsage(d->Win);
	XStoreName(XDisplay(), d->Win, d->Text ? d->Text : "");
	
	update();
}

int XWidget::value()
{
	return 0;
}

void XWidget::setValue(int i)
{
}

XWidget *XWidget::MouseGrabber = 0;

void XWidget::grabMouse()
{
	if (d->Win)
	{
		DebugLog(_FL, "grab mouse win=%i\n", d->Win);

		if (MouseGrabber)
		{
			// printf("Ungrabbing previous grab to grab on new window.\n");
			MouseGrabber->ungrabMouse();
		}
		
		MapWinUsage(d->Win);
		int e;
		if ((e = XGrabPointer(	XDisplay(),
								d->Win,
								false,				// owner events
								ButtonPressMask |
									ButtonReleaseMask |
									PointerMotionMask |
									EnterWindowMask |
									LeaveWindowMask |
									FocusChangeMask,// event mask
								GrabModeAsync,		// pointer mode
								GrabModeAsync,		// keyboard mode
								XNone,				// confine to
								XNone,				// cursor
								CurrentTime)) == GrabSuccess)
		{
			MouseGrabber = this;
		}
		else
		{
			printf("Error: XWidget::grabMouse failed (%s). Win=%i Map=%s Exposed=%i\n", XErr(e), d->Win, d->MapName(), d->Exposed);
		}
	}
}

void XWidget::ungrabMouse()
{
	if (d->Win)
	{
		DebugLog(_FL, "ungrab mouse\n");
		
		XUngrabPointer(XDisplay(), CurrentTime);
		MouseGrabber = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
template <class q>
void XView<q>::OnClick(XlibEvent *e, bool down, bool dbl, bool move)
{
	if (v)
	{
		GMouse m;

		m.Target = v;
		m.Flags = 0;
		m.x = e->x();
		m.y = e->y();
		m.Down(e->down());
		m.Double(e->doubleclick());

		if (e->button() & XWidget::LeftButton) m.Left(true);
		if (e->button() & XWidget::RightButton) m.Right(true);
		if (e->button() & XWidget::MidButton) m.Middle(true);

		if (e->button() & XWidget::ShiftButton) m.Shift(true);
		if (e->button() & XWidget::ControlButton) m.Ctrl(true);
		if (e->button() & XWidget::AltButton) m.Alt(true);

		v->_Mouse(m, move);
	}
}

template <class q>
bool XView<q>::OnKey(XlibEvent *e, bool down)
{
	bool Status = false;

	if (v)
	{
		GKey k;

		k.vkey = e->getsym();
		if (k.vkey > 0xffff)
		{
			k.c16 = (int)k.vkey;
		}
		else
		{		
			k.c16 = e->unicode(XView<q>::GetInputContext());
		}
		k.IsChar = false;
		k.Flags = 0;
		k.Down(down);
		k.Data = 0;

		if (e->button() & XWidget::ShiftButton) k.Shift(true);
		if (e->button() & XWidget::AltButton) k.Alt(true);
		if (e->button() & XWidget::ControlButton)
		{
			k.Ctrl(true);
			
			/*
			if (k.c16 >= VK_F1 AND k.c16 <= VK_F15)
				k.Down(true);
			*/
		}

#if 0
LgiTrace("XView<q>::OnKey ch=%i(%c) char=%i down=%i ctrl=%i alt=%i shift=%i\n",
	k.c16, k.c16 >= ' ' AND k.c16 < 127 ? k.c16 : '.', k.IsChar, k.Down(), k.Ctrl(), k.Alt(), k.Shift());
#endif

		GWindow *w = v->GetWindow();
		if (w)
		{
			Status = w->HandleViewKey(v, k);
			
			k.IsChar = e->ischar();
			if (NOT k.IsChar AND k.c16 >= VK_KP0 AND k.c16 <= VK_KP_EQUALS)
			{
				// Generate char version of keystroke
				switch (k.c16)
				{
					case VK_KP0: k.c16 = '0'; break;
					case VK_KP1: k.c16 = '1'; break;
					case VK_KP2: k.c16 = '2'; break;
					case VK_KP3: k.c16 = '3'; break;
					case VK_KP4: k.c16 = '4'; break;
					case VK_KP5: k.c16 = '5'; break;
					case VK_KP6: k.c16 = '6'; break;
					case VK_KP7: k.c16 = '7'; break;
					case VK_KP8: k.c16 = '8'; break;
					case VK_KP9: k.c16 = '9'; break;
					case VK_KP_PERIOD: k.c16 = '.'; break;
					case VK_KP_DIVIDE: k.c16 = '/'; break;
					case VK_KP_MULTIPLY: k.c16 = '*'; break;
					case VK_KP_MINUS: k.c16 = '-'; break;
					case VK_KP_PLUS: k.c16 = '+'; break;
					case VK_KP_ENTER: k.c16 = '\r'; break;
					case VK_KP_EQUALS: k.c16 = '='; break;
					default:
						LgiAssert(0);
						return false;
						break;
				}
				
				k.IsChar = true;
			}
			if (k.IsChar)
			{
				Status |= w->HandleViewKey(v, k);
			}
		}
		else
		{
			printf("%s:%i - No window.\n", __FILE__, __LINE__);
		}
	}
		
	return Status;
}

template <class q>
	XView<q>::XView(GView *view, bool p) : q()
	{
		v = view;
		_Paint = p;
		setBackgroundMode(NoBackground);
	}

template <class q>
	XView<q>::XView(GView *view, Window Existing) : q(Existing)
	{
		v = view;
		_Paint = true;
		setBackgroundMode(NoBackground);
	}

template <class q>
	XView<q>::~XView()
	{
		if (v)
		{
			v->_View = 0;
			DeleteObj(v);
		}
	}

template <class q>
	void XView<q>::_SetWndPtr(void *p)
	{
		v = (GView*)p;
	}
	
template <class q>
	void *XView<q>::_GetWndPtr()
	{
		return v;
	}

// Events
void XWidget::resizeEvent(XlibEvent *e)
{
	if (!d->InitResize && d->Debug)
	{
		printf("%s:%i - XWidget::resizeEvent InitResize set\n", _FL);
		d->InitResize = true;
	}
}

template <class q>
	void XView<q>::resizeEvent(XlibEvent *e)
	{
		if (e)
			q::resizeEvent(e);
			
		if (v)
			v->OnPosChange();
	}

template <class q>
	void XView<q>::paintEvent(XlibEvent *e)
	{
		if (_Paint) q::paintEvent(e);
		
		if (v)
		{
			v->_Paint();
		}
	}

template <class q>
	void XView<q>::customEvent(XlibEvent *e)
	{
		GMessage *m = dynamic_cast<GMessage*>(e);
		if (m AND v)
		{
			v->OnEvent(m);
		}
	}

template <class q>
	void XView<q>::notifyEvent(int i)
	{
		if (v)
		{
			GViewI *n = v->GetNotify() ? v->GetNotify() : v->GetParent();
			if (n)
			{
				n->OnNotify(v, i);
			}
		}
	}

	// Mouse events
template <class q>
	void XView<q>::mousePressEvent(XlibEvent *e)
	{
		q::mousePressEvent(e);
		OnClick(e, true, false, false);
	}

template <class q>
	void XView<q>::mouseDoubleClickEvent(XlibEvent *e)
	{
		q::mouseDoubleClickEvent(e);
		OnClick(e, true, true, false);
	}

template <class q>
	void XView<q>::mouseReleaseEvent(XlibEvent *e)
	{
		q::mouseReleaseEvent(e);
		OnClick(e, false, false, false);
	}

template <class q>
	void XView<q>::mouseMoveEvent(XlibEvent *e)
	{
		q::mouseMoveEvent(e);
		OnClick(e, false, false, true);
	}

template <class q>
	void XView<q>::leaveEvent(XlibEvent *e)
	{
		q::leaveEvent(e);

		GMouse m;
		m.x = m.y = m.Flags = 0;
		if (v) v->OnMouseExit(m);
	}

template <class q>
	void XView<q>::enterEvent(XlibEvent *e)
	{
		q::enterEvent(e);

		GMouse m;
		m.x = m.y = m.Flags = 0;
		if (v) v->OnMouseEnter(m);
	}

template <class q>
	void XView<q>::wheelEvent(XlibEvent *e)
	{
		q::wheelEvent(e);

		if (v) v->OnMouseWheel(- ((double) e->delta() * XApplication::wheelScrollLines() / 120) );
	}

	// Focus events
template <class q>
	void XView<q>::focusInEvent(XlibEvent *e)
	{
		q::focusInEvent(e);
		if (v) v->OnFocus(true);
	}

template <class q>
	void XView<q>::focusOutEvent(XlibEvent *e)
	{
		q::focusOutEvent(e);
		if (v) v->OnFocus(false);
	}

	// Keyboard events
template <class q>
	bool XView<q>::keyPressEvent(XlibEvent *e)
	{
		q::keyPressEvent(e);
		return OnKey(e, true);
	}

template <class q>
	bool XView<q>::keyReleaseEvent(XlibEvent *e)
	{
		q::keyReleaseEvent(e);
		return OnKey(e, false);
	}

// Instances
template class XView<XWidget>;
