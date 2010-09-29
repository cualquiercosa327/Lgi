#include <stdio.h>
#include "Lgi.h"
#include "xapplication.h"
#include "Gdc2.h"
#include "GDragAndDrop.h"

/////////////////////////////////////////////////////////////////
class DesktopWidget : public XWidget
{
public:
	int width()
	{
		return XDisplayWidth(XDisplay(), 0);
	}

	int height()
	{
		return XDisplayHeight(XDisplay(), 0);
	}

} *Desktop = 0;

/////////////////////////////////////////////////////////////////
int ErrorHandler(Display *d, XErrorEvent *e)
{
	char Str[256] = "No Description";
	XGetErrorText(d, e->error_code, Str, strlen(Str));
	printf("XLib failed request. Type=%i Code=%i (%s)\n", e->type, e->error_code, Str);
	
	#if 0
	int *i = 0;
	*i = 1;
	#endif
	
	return 0;
}

int FormatToInt(char *s)
{
	int a = XInternAtom(XObject::XDisplay(), s, false);
	printf("format %s->%i\n", s, a);
	return a;
}

char *FormatToStr(int f)
{
	static char Buf[256];
	char *s = XGetAtomName(XObject::XDisplay(), f);
	if (s)
	{
		strsafecpy(Buf, s, sizeof(Buf));
		printf("format %i->%s\n", f, Buf);
		XFree(s);
		return Buf;
	}
	else
	{
		printf("%s:%i - error converting '%i' to string.\n", __FILE__, __LINE__, f);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////
class XApplicationPrivate
{
public:
	int Depth;
	int Code;
	XWidget *Focus;
	XWidget *FocusWindow;
	int OverGrabbed;
	OsThreadId GuiThread;
	XIM InputMethod;

	// Last mouse down click info
	uint64 LastButtonPressTime[3];
	int LastMouseX, LastMouseY;

	XApplicationPrivate()
	{
		InputMethod = 0;
		GuiThread = LgiGetCurrentThread();
		Depth = 0;
		Code = 0;
		Focus = FocusWindow = 0;
		OverGrabbed = -1;
		ZeroObj(LastButtonPressTime);
		LastMouseX = LastMouseY = 0;
	}
	
	~XApplicationPrivate()
	{
	}
};

/////////////////////////////////////////////////////////////////
XApplication *XApplication::_App = 0;
XWidget *XApplication::WantsFocus = 0;

XApplication::XApplication(int args, char **arg) : GSemaphore("XApplication")
{
	d = new XApplicationPrivate;
	_App = this;
	if (NOT XInitThreads())
	{
		printf("Error: This application requires a thread aware version of Xlib.\n");
		return;
	}
	
	Dsp = XOpenDisplay(0);
	if (NOT Dsp)
	{
		printf("Error: This application requires XWindows.\n");
		return;
	}
	
#if 0
	printf("Warning: Running XSynchronize\n");
	XSynchronize(Dsp, 0);
#endif

	SelectionSink = 0;
	DndTarget = 0;
	ClipText = 0;
	ClipImage = 0;

	char *Atoms[] = 
	{
		"WM_DELETE_WINDOW",
		"WM_PROTOCOLS",
		"CLIPBOARD",
		"XdndEnter",
		"XdndPosition",
		"XdndStatus",
		"XdndLeave",
		"XdndDrop",
		"XdndFinished",
		"XdndSelection",
		"_NET_WM_ICON",
		"CARDINAL",
	};
	ZeroObj(XAtom);
	XInternAtoms(Dsp, Atoms, CountOf(Atoms), false, XAtom);

	XSetErrorHandler(ErrorHandler);
	
	// Setup input method
	char ClassName[256] = "LgiApp";
	d->InputMethod = XOpenIM(Dsp, NULL, ClassName, ClassName);
	if (NOT d->InputMethod)
	{
		printf("%s:%i - XOpenIM failed.\n", __FILE__, __LINE__);
	}

	/*
		d->InputContext = pXCreateIC(d->InputMethod,
									XNClientWindow, WMwindow,
									XNFocusWindow, WMwindow,
									XNInputStyle, XIMPreeditNothing  | XIMStatusNothing,
									XNResourceName, classname,
									XNResourceClass, classname,
									NULL);

	*/
	
}

XApplication::~XApplication()
{
	if (d->InputMethod)
	{
		XCloseIM(d->InputMethod);
		d->InputMethod = 0;
	}
	
	SetClipText(0, 0);
	DeleteObj(Desktop);
	if (Dsp) XCloseDisplay(Dsp);
	DeleteObj(d);
}

XIM XApplication::GetInputMethod()
{
	return d->InputMethod;
}

bool XApplication::IsOk()
{
	bool Status =	this != 0 AND
					d != 0 AND
					Dsp;
	LgiAssert(Status);
	return Status;
}

XWidget *XApplication::GetKeyTarget()
{
	/*
	XWidget *w = Popups.Last();
	if (w)
	{
		// If the focus is on the last (top-most) popup then return that
		for (XWidget *f = d->Focus; f; f = f->parentWidget())
		{
			if (f == w)
			{
				return d->Focus;
			}
		}
	}
	*/
		
	// Return the current focus
	return d->Focus;
}

uint64 XApplication::GetLastButtonPressTime(int Button, int x, int y)
{
	if (Button >= 0 AND
		Button < CountOf(d->LastButtonPressTime) AND
		abs(x - d->LastMouseX) < 5 AND
		abs(y - d->LastMouseY) < 5)
	{
		return d->LastButtonPressTime[Button];
	}
	
	return 0;
}

XWidget *XApplication::GetFocusWindow()
{
	return d->FocusWindow;
}

XWidget *XApplication::GetFocus()
{
	return d->Focus;
}

void XApplication::SetFocus(XWidget *q)
{
    if (d->Focus)
	{
	    d->Focus->focusOutEvent(q->_event);
	}
	
	d->Focus = q;
	
    if (d->Focus)
	{
	    d->Focus->focusInEvent(q->_event);
	}
}

void XApplication::SetSelectionSink(XEventSink *s)
{
	SelectionSink = s;
}

void XApplication::EmptyClip()
{
	DeleteArray(ClipText);
	if (ClipImage)
	{
		XFreePixmap(Dsp, ClipImage);
		ClipImage = 0;
	}
}

void XApplication::SetClipText(XWidget *w, char *s)
{
	EmptyClip();
	if (w AND s)
	{
		XWidget::OwnsClipboard = w;
		ClipText = NewStr(s);
	}
}

void XApplication::SetClipImage(XWidget *w, GSurface *pDC)
{
	EmptyClip();
	if (w AND pDC)
	{
		XWidget::OwnsClipboard = w;
		
		MapWinUsage(w->handle());
		ClipImage = XCreatePixmap(Dsp, w->handle(), pDC->X(), pDC->Y(), GdcD->GetBits());
		if (ClipImage)
		{
			GMemDC *pSrc = dynamic_cast<GMemDC*>(pDC);
			GMemDC *pMem = 0;
			if (NOT pSrc OR pDC->GetBits() != GdcD->GetBits())
			{
				pMem = new GMemDC(pDC->X(), pDC->Y(), GdcD->GetBits());
				if (pMem)
				{
					pMem->Blt(0, 0, pDC);
				}
				pSrc = pMem;
			}
			
			XGCValues v;
			GC Gc = XCreateGC(Dsp, ClipImage, 0, &v);
			if (Gc)
			{
				XPutImage(Dsp, ClipImage, Gc, pSrc->GetBitmap()->GetImage(), 0, 0, 0, 0, pDC->X(), pDC->Y());
				XFreeGC(Dsp, Gc);
			}
			else
			{
				printf("%s,%i - XCreateGC failed.\n", __FILE__, __LINE__);
			}
			
			DeleteObj(pMem);
		}
	}
}

XWidget *XApplication::desktop()
{
	if (NOT Desktop)
	{
		Desktop = new DesktopWidget;
	}
	return Desktop;
}

void XApplication::OnDeleteWidget(XWidget *w)
{
	if (XWidget::OwnsClipboard == w)
	{
		XWidget::OwnsClipboard = XWidget::FirstWidget();
		if (XWidget::OwnsClipboard)
		{
			MapWinUsage(XWidget::OwnsClipboard->handle());
			XSetSelectionOwner(Dsp, XA_Clipboard, XWidget::OwnsClipboard->handle(), CurrentTime);
		}
	}
	
	if (d->Focus == w)
	{
	    d->Focus = 0;
	}
}

void _AddStop(XWidget *p, XList<XWidget> &Stops)
{
	XList<XWidget> *c = p->childrenWidgets();
	if (c)
	{
		for (XWidget *w=c->First(); w; w=c->Next())
		{
			if (w->isVisible() AND w->isEnabled())
			{
				if (w->isTabStop())
				{
					Stops.Insert(w);
				}

				_AddStop(w, Stops);
			}
		}
	}
}

void XApplication::OnTabKey(XWidget *w, int Dir)
{
	XWidget *Top = w;
	while (Top->parentWidget())
	{
		Top = Top->parentWidget();
	}
	
	XList<XWidget> Stops;
	_AddStop(Top, Stops);
	
	int Cur = 0;
	for (XWidget *n=Stops.First(); n AND n!=w; n=Stops.Next())
	{
		Cur++;
	}
	
	if (Stops.Items() > 1)
	{
		int New = (Cur + Dir) % Stops.Items();
		
		// printf("tab %i->%i\n", Cur, New);
		
		XWidget *NewFocus = Stops.ItemAt(New);
		if (NewFocus)
		{
			NewFocus->setFocus();
		}
	}
}

void _PrintProps(Display *d, Window w)
{
	#ifdef _DEBUG
	int Props = 0;
	Atom *Prop = XListProperties(d, w, &Props);
	if (Props AND Prop)
	{
		for (int i=0; i<Props; i++)
		{
			printf("\t[%i]='%s'\n", i, XGetAtomName(d, Prop[i]));
		}
	}
	#endif
}

#define GetIngoreState(w) \
	bool Ignore = false; \
	if (w) \
	{ \
		XMainWindow *_wnd = w->GetWindow(); \
		if (XWidget::GetMouseGrabber() == 0 AND _wnd) \
			Ignore = _wnd->GetIgnoreInput(); \
	}

void XApplication::OnEvent(XEvent *Event)
{
	// Process the XDND selection
	XWidget *Current = XWidget::Find(Event->xany.window);
	if (DndTarget)
	{
		char *Type;
		if (Type = DndAccepted.First())
		{
			// printf("Got select\n");
			uchar *Data = 0;
			Atom ActualType = 0;
			int Format = 0;
			ulong Items = 0, Bytes = 0;

			MapWinUsage(Current->handle());
			if (XGetWindowProperty(	XDisplay(),
									Current->handle(),
									XA_XdndSelection,
									0, // offset
									1024, // len
									true, // delete
									FormatToInt(Type), // required type
									&ActualType,
									&Format,
									&Items,
									&Bytes,
									&Data) == XSuccess AND Data)
			{
				// printf("Drop: Format=%i Len=%i Type='%s' Property='%s' Target=%p\n", Format, Items, Type, Data, DndTarget);

				if (DndTarget)
				{
					GVariant d;
					d.SetBinary(Items, Data);
					DndTarget->OnDrop(Type, &d, GdcPt2(), 0);
				}

				XFree(Data);
			}
			else
			{
				printf("%s:%i - XDND: didn't get prop.\n", __FILE__, __LINE__);
			}
		}
		else
		{
			printf("%s:%i - No DndAccepted type.\n", __FILE__, __LINE__);
		}

		DndTarget->OnDragExit();
		DndTarget = 0;
	}
	else
	{
		printf("%s:%i - No DndTarget.\n", __FILE__, __LINE__);
	}
	
	SelectionSink = 0;
}

OsThreadId XApplication::GetGuiThread()
{
	return d->GuiThread;
}

void XApplication::processEvents()
{
	static int LastTime = 0;
	XEvent Event;
	
	if (LastTime)
	{
		int Now = LgiCurrentTime();
		if (Now > LastTime + 1000)
		{
			printf("Message Loop blocked for %i ms\n", Now - LastTime);
		}
		LastTime = Now;
	}
	else
	{
		LastTime = LgiCurrentTime();
	}
	
	d->GuiThread = LgiGetCurrentThread();
	
	while (XPending(Dsp))
	{
		XNextEvent(Dsp, &Event);
		onEvent(Event);
	}
}

void XApplication::onEvent(XEvent &Event)
{
	XlibEvent q(&Event);

	// FIXME: Need an O(1) lookup here... for large apps it's gonna start hurting.

	XWidget *Current = XWidget::Find(Event.xany.window);
	if (Current)
	{
		Current->_event = &q;

		switch (q.type())
		{
			case Expose:
			{
				// Clear out any Expose events waiting in the queue
				// We only want to process the last one.
				GRegion *c = new GRegion;
				do
				{
					if (Event.xexpose.width > 0 AND
						Event.xexpose.height > 0)
					{
						GRect r(	Event.xexpose.x,
									Event.xexpose.y,
									Event.xexpose.x + Event.xexpose.width,
									Event.xexpose.y + Event.xexpose.height);
						c->Union(&r);
					}
				}
				while (XCheckTypedWindowEvent(Dsp, Event.xexpose.window, Expose, &Event));
				
				/*
				XRectangle *x = new XRectangle[c.GetSize()];
				GRect *r = c[0];
				if (x AND r)
				{
					for (int i=0; i<c.GetSize(); i++)
					{
						x[i].x = r[i].x1;
						x[i].y = r[i].y1;
						x[i].width = r[i].X();
						x[i].height = r[i].Y();
					}
				}
				*/

				Current->_SetClipRgn(c); // c.GetSize(), x);
				Current->_paint(&Event.xexpose, &q);
				break;
			}
			case ConfigureNotify:
			{
				while (XCheckTypedWindowEvent(Dsp, Event.xexpose.window, ConfigureNotify, &Event))
				{
				}
				
				Current->_resize(&Event.xconfigure, &q);
				break;
			}
			case MapNotify:
			{
				Current->_map(true);
				break;
			}
			case UnmapNotify:
			{
				Current->_map(false);
				break;
			}
			case ButtonPress:
			{
				GetIngoreState(Current);
				if (NOT Ignore)
				{
					if (Event.xbutton.button < 4)
					{
						int x = Event.xbutton.x;
						int y = Event.xbutton.y;
	
						/*
						// tell popups about the click
						for (QPopup *p=Popups.First(); p; p=Popups.Next())
						{
							if (p->isVisible())
							{
								OsPoint m(Event.xbutton.x_root, Event.xbutton.y_root);
								m = p->mapFromGlobal(m);
								Event.xbutton.x = m.x;
								Event.xbutton.y = m.y;
	
								// printf("ButtonPress popup=%p visible=%i pt=%i,%i\n", p, p->isVisible(), m.x(), m.y());
	
								if (p != Current)
								{
									p->mousePressEvent(&q);
								}
								// else it'll get the event below anyway
							}							
						}
						*/
						
						// tell the actual window
						Event.xbutton.x = x;
						Event.xbutton.y = y;
						Current->mousePressEvent(&q);
	
						d->LastButtonPressTime[Event.xbutton.button-1] = LgiCurrentTime();
						d->LastMouseX = Event.xbutton.x_root;
						d->LastMouseY = Event.xbutton.y_root;
					}
					else
					{
						Current->wheelEvent(&q);
					}
				}
				break;
			}
			case ButtonRelease:
			{
				GetIngoreState(Current);
				if (NOT Ignore)
				{
					if (Event.xbutton.button < 4)
					{
						Current->mouseReleaseEvent(&q);
					}
					else
					{
						Current->wheelEvent(&q);
					}
				}
				break;
			}
			case MotionNotify:
			{
				GetIngoreState(Current);
				if (NOT Ignore)
				{
					// Clear old MotionNotify's still in the que
					while (XCheckTypedWindowEvent(Dsp, Event.xmotion.window, MotionNotify, &Event))
					{
					}
					
					// When we have grabbed the mouse we stop getting the normal enter and leave
					// messages. Now for buttons and menus that rely on these events we must generate
					// them here artificially for at least the window that has grabbed the mouse.
					if (XWidget::MouseGrabber)
					{
						int Over = XWidget::MouseGrabber->IsMouseOver(&q);
						
						if (d->OverGrabbed >= 0 AND	// if d->OverGrabbed < 0 then it's not initialized yet for this 
							(d->OverGrabbed ^ Over))// grab session.
						{
							if (Over)
							{
								XWidget::MouseGrabber->enterEvent(&q);
							}
							else
							{
								XWidget::MouseGrabber->leaveEvent(&q);
							}
						}

						d->OverGrabbed = Over;
					}
					else
					{
						// Reset the over grabbed status for next time the mouse is grabbed
						d->OverGrabbed = -1;
					}
					
					Current->mouseMoveEvent(&q);
				}
				break;
			}
			case EnterNotify:
			{
				GetIngoreState(Current);
				if (NOT Ignore)
				{
					Current->enterEvent(&q);
				}
				break;
			}
			case LeaveNotify:
			{
				GetIngoreState(Current);
				if (NOT Ignore)
				{
					Current->leaveEvent(&q);
				}
				break;
			}
			case FocusIn:
			{
				if (Event.xfocus.mode == NotifyNormal)
				{
					d->FocusWindow = Current;
					if (NOT d->Focus OR
						d->Focus->GetWindow() != Current)
					{
						// No focus...
						// Initialize some sort of focus in this window
						XMainWindow *Wnd = Current->GetWindow();
						
						if (Wnd AND (d->Focus = Wnd->GetLastFocus()))
						{
							// Window already had a focus, just return the
							// focus to the previous control
						}
						else
						{
							// No focus in that window, or the control no
							// longer exists, so find a new one.
							Wnd->DefaultFocus();
						}
					}
					
					if (d->Focus)
					{
						// Pass focus event along to GView
						d->Focus->focusInEvent(&q);
					}
				}
				break;
			}
			case FocusOut:
			{
				if (Event.xfocus.mode == NotifyNormal)
				{
					d->FocusWindow = 0;
					if (d->Focus)
					{
						// Pass focus event along to GView
						d->Focus->focusOutEvent(&q);
					}
				}
				break;
			}
			case ReparentNotify:
			{
				GMessage e(M_X11_REPARENT, (int)Event.xreparent.parent);
				if (Current)
				{
					/*
					printf("ReparentNotify event=%p window=%p parent=%p\n",
						Event.xreparent.event,
						Event.xreparent.window,
						Event.xreparent.parent);
					*/
					
					Current->customEvent(&e);
				}
				break;
			}
			case ClientMessage:
			{
				if (Event.xclient.message_type == XA_WmProtocols)
				{
					XlibEvent e(&Event);

					// Window manager message
					if (Event.xclient.data.l[0] == XA_WmDeleteWindow)
					{
						Current->closeEvent(&e);
					}
				}
				else if (Event.xclient.message_type == XA_XdndEnter)
				{
					Window Src = Event.xclient.data.l[0];
					int Version = Event.xclient.data.l[1] >> 24;
					bool MoreThan3 = Event.xclient.data.l[1] & 1;
					GView *View = (GView*)Current->_GetWndPtr();
					GDragDropTarget *Dst = dynamic_cast<GDragDropTarget*>(View);

					// printf("XA_XdndEnter MoreThan3=%i\n", MoreThan3);
					if (MoreThan3)
					{
						uchar *Data = 0;
						Atom ActualType = 0;
						int Format = 0;
						ulong Items = 0, Bytes = 0;

						if (XGetWindowProperty(	XDisplay(),
												Src,
												FormatToInt("XdndTypeList"),
												0, // offset
												1024, // len
												false, // delete
												AnyPropertyType, // required type
												&ActualType,
												&Format,
												&Items,
												&Bytes,
												&Data) == XSuccess AND Data)
						{
							// printf("Got type list\n", Items);
							for (int i=0; i<Items; i++)
							{
								// printf("\t[%i]='%s'\n", i, FormatToStr( ((Atom*)Data)[i] ));
							}
						}
					}
					else
					{
						/*
						printf("XdndEnter\n"
								"\t[0]='%s'\n"
								"\t[1]='%s'\n"
								"\t[2]='%s'\n",
								Event.xclient.data.l[2]!=XNone ? XGetAtomName(Dsp, Event.xclient.data.l[2]) : "0",
								Event.xclient.data.l[3]!=XNone ? XGetAtomName(Dsp, Event.xclient.data.l[3]) : "0",
								Event.xclient.data.l[4]!=XNone ? XGetAtomName(Dsp, Event.xclient.data.l[4]) : "0"
								);
						*/
					}

					DndAccepted.DeleteArrays();
					DndTypes.DeleteArrays();
					for (int i=2; i<=4; i++)
					{
						if (Event.xclient.data.l[i] != XNone)
						{
							char *s = NewStr(FormatToStr(Event.xclient.data.l[i]));
							if (ValidStr(s))
							{
								DndTypes.Insert(s);
							}
						}
					}
				}
				else if (Event.xclient.message_type == XA_XdndPosition)
				{
					Window Src = Event.xclient.data.l[0];
					GdcPt2 p(Event.xclient.data.l[2] >> 16, Event.xclient.data.l[2] & 0xffff);
					int Time = Event.xclient.data.l[3];
					int Action = Event.xclient.data.l[4];
					GView *View = (GView*)Current->_GetWndPtr();
					
					if (View)
					{
						View->PointToView(p);

						GDragDropTarget *Dst = 0;
						GViewI *At = View ? View->WindowFromPoint(p.x, p.y) : 0;
						while (At AND NOT (Dst = dynamic_cast<GDragDropTarget*>(At)))
						{
							At = At->GetParent();
						}

						if (Dst != DndTarget)
						{
							if (DndTarget)
							{
								DndTarget->OnDragExit();
							}
							else if (Dst)
							{
								Dst->OnDragEnter();
							}

							DndTarget = Dst;

							/*
							printf("XdndPosition Dst=%p Pos=%i,%i Time=%i Action=%s\n",
									Dst,
									p.x,
									p.y,
									Time,
									Action?XGetAtomName(Dsp, Action):0);
							*/
						}

						// printf("XdndPosition on wnd '%s' p=%i,%i DndTarget=%p\n", View->Name(), p.x, p.y, DndTarget);
						if (DndTarget AND Current)
						{
							// Make coordinates relitive to the client
							for (GViewI *v=At; v AND v != View; v = v->GetParent())
							{
								GRect r = v->GetPos();
								p.x -= r.x1;
								p.y -= r.y1;
							}

							XWidget *Cur = Current;

							// Ask it whether it will accept the drop
							DndAccepted.DeleteArrays();
							for (char *s=DndTypes.First(); s; s=DndTypes.Next())
							{
								DndAccepted.Insert(NewStr(s));
							}
							
							int Accept = Dst->WillAccept(DndAccepted, p, 0);
							// printf("->WillAccept(%i,%i) = %i (Type: %s)\n", p.x, p.y, Accept, DndAccepted.First());

							// Return status to the caller
							XEvent s;
							s.xclient.type = ClientMessage;
							s.xclient.message_type = XA_XdndStatus;
							s.xclient.display = Dsp;
							s.xclient.window = Cur->handle();
							s.xclient.format = 32;
							s.xclient.data.l[0] = Cur->handle();
							s.xclient.data.l[1] = (Accept ? 1 : 0) | 2;
							s.xclient.data.l[2] = 0;
							s.xclient.data.l[3] = (View->X() << 16) | View->Y();
							s.xclient.data.l[4] = Action;
							
							MapWinUsage(Cur->handle());
							// printf("%s:%i - XSendEvent\n", __FILE__, __LINE__);
							XSendEvent(Dsp, Src, false, XNone, &s);
						}
						else
						{
							// printf("XdndPosition received with no target.\n");
						}
					}
				}
				else if (Event.xclient.message_type == XA_XdndStatus)
				{
					// printf("XdndStatus\n");
				}
				else if (Event.xclient.message_type == XA_XdndLeave)
				{
					// printf("XdndLeave\n");

					if (DndTarget)
					{
						DndTarget->OnDragExit();
						DndTarget = 0;
					}
					else
					{
						printf("%s:%i - No target for XdndLeave\n", __FILE__, __LINE__);
					}
				}
				else if (Event.xclient.message_type == XA_XdndDrop)
				{
					// printf("XdndDrop\n");
					bool Converted = false;

					if (DndTarget)
					{
						SelectionSink = 0;

						char *Type;
						if (Type = DndAccepted.First())
						{
							// Ask for selection
							/*
							printf("Asking for selection, type='%s' src=%p owner=%p\n",
									Type,
									Event.xclient.data.l[0],
									XGetSelectionOwner(Dsp, XA_XdndSelection));
							*/

							SelectionSink = this;
							MapWinUsage(Current->handle());
							XConvertSelection(	Dsp,
												XA_XdndSelection,
												FormatToInt(Type),
												XA_XdndSelection,
												Current->handle(),
												Event.xclient.data.l[2]); // Event.xclient.data.l[2]
							Converted = true;
						}
						else
						{
							printf("XDND: Target didn't select a drop type.\n");
						}

						if (NOT SelectionSink)
						{
							DndTarget->OnDragExit();
							DndTarget = 0;
						}
					}

					// Send XdndFinished
					XEvent s;
					s.xclient.type = ClientMessage;
					s.xclient.message_type = XA_XdndFinished;
					s.xclient.display = Dsp;
					s.xclient.window = Current->handle();
					s.xclient.format = 32;
					s.xclient.data.l[0] = Current->handle();
					
					MapWinUsage(Current->handle());
					printf("%s:%i - XSendEvent\n", __FILE__, __LINE__);
					XSendEvent(Dsp, Event.xclient.data.l[2], false, XNone, &s);
				}
				else if (Event.xclient.message_type == XA_XdndFinished)
				{
					// printf("XdndFinished\n");
				}
				else
				{
					// printf("received event for window=%i\n", Event.xany.window);
					GMessage e(&Event);
					if (Current)
					{
						/*
						printf("xclient.data.l = { %i,%i,%i,%i,%i }\n",
							Event.xclient.data.l[0],
							Event.xclient.data.l[1],
							Event.xclient.data.l[2],
							Event.xclient.data.l[3],
							Event.xclient.data.l[4]);
							
						GMessage *m = dynamic_cast<GMessage*>(&e);
						if (m)
						{
							printf("m=%p a=%i b=%i\n", m, m->a(), m->b());
						}
						*/
							
						Current->customEvent(&e);
					}
				}
				break;
			}
			case KeyPress:
			{
				GetIngoreState(Current);
				if (NOT Ignore)
				{
					XWidget *Receiver = GetKeyTarget();
					if (NOT Receiver) Receiver = Current;

					if (Receiver)
					{
						bool p = Receiver->keyPressEvent(&q);
						if (q.unicode(Receiver->GetInputContext()) == 9 AND NOT p)
						{
							OnTabKey(Receiver, ((q.state() & 1) == 0) ? 1 : -1);
						}
					}
				}
				else
				{
					// printf("KeyPress: Ingore\n");
				}				
				break;
			}
			case KeyRelease:
			{
				GetIngoreState(Current);
				if (NOT Ignore)
				{
					XWidget *Receiver = GetKeyTarget();
					if (NOT Receiver) Receiver = Current;
					if (Receiver)
					{
						Receiver->keyReleaseEvent(&q);
					}
				}
				else
				{
					// printf("KeyRelease: Ingore\n");
				}				
				break;
			}
			case SelectionRequest:
			{
				char *selection = XGetAtomName(Dsp, Event.xselectionrequest.selection);
				char *target = XGetAtomName(Dsp, Event.xselectionrequest.target);
				char *property = XGetAtomName(Dsp, Event.xselectionrequest.property);

				#if 0
				printf("SelectionRequest requested by %p, received by %p, sel='%s' target='%s' prop='%s'\n",
						Event.xselectionrequest.requestor,
						Event.xselectionrequest.owner,
						selection, target, property);
				#endif
				
				if (selection AND
					target)
				{
					if (stricmp(selection, "CLIPBOARD") == 0)
					{
						XEvent s;
						ZeroObj(s);
						s.xselection.type = SelectionNotify;
						s.xselection.display = Dsp;
						s.xselection.requestor = Event.xselectionrequest.requestor;
						s.xselection.selection = Event.xselectionrequest.selection;
						s.xselection.target = Event.xselectionrequest.target;
						s.xselection.property = XNone;
						s.xselection.time = Event.xselectionrequest.time;
	
						if (ClipText)
						{
							if (stricmp(target, "UTF8_STRING") == 0 OR
								stricmp(target, "UTF-8") == 0)
							{
								XChangeProperty(Dsp,
												Event.xselectionrequest.requestor,
												Event.xselectionrequest.property,
												Event.xselectionrequest.target,
												8,
												PropModeReplace,
												(uchar*)ClipText,
												strlen(ClipText));
								s.xselection.property = Event.xselectionrequest.property;
							}
							else if (stricmp(target, "TARGETS") == 0)
							{
								char *t[2] = { "UTF8_STRING", "UTF-8" };
								Atom a[2];
								XInternAtoms(Dsp, t, 2, false, a);
								XChangeProperty(Dsp,
												Event.xselectionrequest.requestor,
												Event.xselectionrequest.property,
												Event.xselectionrequest.target,
												32,
												PropModeReplace,
												(uchar*)a,
												sizeof(a));
								s.xselection.property = Event.xselectionrequest.property;											
							}
							else
							{
								printf("%s,%i - Unsupported text target format '%s'.\n", __FILE__, __LINE__, target);
							}
						}
						else if (ClipImage)
						{
							if (stricmp(target, "PIXMAP") == 0)
							{
								XChangeProperty(Dsp,
												Event.xselectionrequest.requestor,
												Event.xselectionrequest.property,
												Event.xselectionrequest.target,
												8,
												PropModeReplace,
												(uchar*)&ClipImage,
												sizeof(ClipImage));
								s.xselection.property = Event.xselectionrequest.property;
							}
							else
							{
								// printf("%s,%i - Unsupported bitmap target format '%s'.\n", __FILE__, __LINE__, target);
							}
						}
						else
						{
							printf("%s,%i: I have no data for the clipboard.\n", __FILE__, __LINE__);
						}
	
						MapWinUsage(Event.xselectionrequest.requestor);
						XSendEvent(	Dsp,
									Event.xselectionrequest.requestor,
									true,
									XNone,
									&s);
					}
					/*
					else if (stricmp(selection, "XdndSelection") == 0)
					{
						printf("Send XdndSelection, target='%s' txt=%s\n", target);
					}
					*/
					else
					{
						printf("XApplication::OnEvent: Unsupported SelectionRequest '%s' received.\n", selection);
					}
				}
				else
				{
					printf("%s,%i - No selection or target.\n", __FILE__, __LINE__);
				}
				break;
			}
			case SelectionNotify:
			{
				if (SelectionSink)
				{
					SelectionSink->OnEvent(&Event);
				}
				break;
			}
			case SelectionClear:
			{
				EmptyClip();
				break;
			}
			case PropertyNotify:
			{
				Current->propertyEvent(&q);
				break;
			}
		}

		if (Current) Current->_event = 0;
	}
	else
	{
		switch (q.type())
		{
			case ButtonPress:
			{
				if (Event.xbutton.button < 4)
				{
					printf("ButtonPress with no current.\n");

					int x = Event.xbutton.x;
					int y = Event.xbutton.y;

					/*
					// tell popups about the click
					for (QPopup *p=Popups.First(); p; p=Popups.Next())
					{
						if (p->isVisible())
						{
							OsPoint m(Event.xbutton.x_root, Event.xbutton.y_root);
							m = p->mapFromGlobal(m);
							Event.xbutton.x = m.x;
							Event.xbutton.y = m.y;

							p->mousePressEvent(&q);
						}
					}
					*/

					d->LastButtonPressTime[Event.xbutton.button-1] = LgiCurrentTime();
				}				
				break;
			}
			case ButtonRelease:
			{
				break;
			}
			case MappingNotify:
			{
				// printf("Got MappingNotify...\n");
				switch (Event.xmapping.request)
				{
					case MappingModifier:
					case MappingKeyboard:
					{
						// printf("Calling XRefreshKeyboardMapping...\n");
						XRefreshKeyboardMapping(&Event.xmapping);
						break;
					}
				}
				break;
			}
			case Expose:
			{
				// printf("Expose with no current. win=%p\n", Event.xany.window);
				break;
			}
			case ClientMessage:
			{
				/*
				printf("Got windowless client message, wnd=%p, msg=%s\n",
					Event.xany.window,
					Event.xclient.message_type ? XGetAtomName(Dsp, Event.xclient.message_type) : 0);
				*/
				break;
			}
		}
	}

	if (Lock(_FL))
	{
		XWidget *w;
		if (w = Delete.First())
		{
			Delete.Delete(w);
			DeleteObj(w);
		}
		Unlock();
	}

	Current = 0;
}

bool XApplication::GetSelection(Window Wnd, Atom Selection, Atom Format, uchar *&Data, ulong &Len)
{
	bool Status = false;
	Atom Incr = XInternAtom(Dsp, "INCR", false);
	XConvertSelection(Dsp, Selection, Format, Selection, Wnd, CurrentTime);

	// Do a mini message loop...
	XEvent e;
	bool GotEvent = false;
	int Start = LgiCurrentTime();
	while (LgiCurrentTime() < Start + 1000)
	{
		// Pass SelectionRequest's through in case we own the clipboard
		while (XCheckTypedEvent(Dsp, SelectionRequest, &e))
		{
			onEvent(e);
		}
		
		// Check for response.
		if (XCheckTypedWindowEvent(Dsp, Wnd, SelectionNotify, &e))
		{
			GotEvent = true;
			break;
		}
		
		LgiSleep(5);
	}

	if (GotEvent)
	{
		Data = 0;
		Len = 0;
		Atom ActualType = 0;
		int Format = 0;
		ulong Items = 0;

		if (e.xselection.property AND
			XGetWindowProperty(	XDisplay(),
								Wnd,
								e.xselection.property,
								0, // offset
								102400, // len
								true, // delete
								AnyPropertyType, // required type
								&ActualType,
								&Format,
								&Items,
								&Len,
								&Data) == XSuccess)
		{
			if (Data)
			{
				if (ActualType == Incr)
				{
					// Do incremental transfer
					GStringPipe p;
					
					while (true)
					{
						if (XGetWindowProperty(	XDisplay(),
												Wnd,
												e.xselection.property,
												0, 102400, true, // delete
												AnyPropertyType, // required type
												&ActualType,
												&Format,
												&Items,
												&Len,
												&Data) == XSuccess)
						{
							// printf("Data=%p Format=%i Items=%i\n", Data, Format, Items);
							if (Data AND Items > 0)
							{
								p.Push((char*) Data, (Items * Format) >> 3);
							}
							else
							{
								break;
							}
						}
						else
						{
							printf("%s:%i - Error\n", __FILE__, __LINE__);
							break;
						}
					}
					
					Len = p.GetSize();
					Data = (uchar*)p.NewStr();
					Status = Data != 0;
				}
				else
				{
					// Do atomic transfer
					Len = Format * Items / 8;
					Status = true;
				}
			}
			else
			{
				printf("GetSelection: XGetWindowProperty OK on %p, but Data==0.\n"
						"\tLen=%i, Type=%s, Format=%i, Items=%i\n",
						Wnd,
						Len,
						ActualType ? FormatToStr(ActualType) : (char*)"(none)",
						Format,
						Items);
			}
		}
		else
		{
			// printf("GetSelection: XGetWindowProperty failed.\n");
		}
	}
	else
	{
		printf("GetSelection: Didn't get notify.\n");
	}

	return Status;
}

int XApplication::exec()
{
	enter_loop();
	return d->Code;
}

void XApplication::exit(int code)
{
	d->Code = code;
	exit_loop();
}

void XApplication::enter_loop()
{
	if (Lock(_FL))
	{
		int D = ++d->Depth;
		Unlock();

		bool Loop = true;
		bool Run = false;
		while (Loop)
		{
			if (Lock(_FL))
			{
				Loop = d->Depth >= D;
				Run = d->Depth == D;
				if (Run)
				{
					// printf("Thread=%i Loop=%i Run=%i d->Depth=%i D=%i\n", LgiGetCurrentThread(), Loop, Run, d->Depth, D);
				}
				Unlock();
			}
			
			if (Run)
			{
				processEvents();
				LgiSleep(5);
			}
			else
			{
				LgiSleep(50);
			}
		}
	}
	else
	{
		printf("enter_loop couldn't lock app!!\n");
	}
}

void XApplication::exit_loop()
{
	if (Lock(_FL))
	{
		d->Depth--;
		Unlock();
	}
	else
	{
		printf("exit_loop couldn't lock app!!\n");
	}
}

void XApplication::postEvent(XWidget *o, XlibEvent *e)
{
	if (o AND
		e AND
		e->GetEvent())
	{
		// prep event
		e->GetEvent()->xany.window = o->handle();
		e->GetEvent()->xany.display = _App->Dsp;
		e->GetEvent()->xclient.format = 32;

		// send to window
		MapWinUsage(o->handle());
		
#if 0
		// GMessage *gm = dynamic_cast<GMessage*>(e);
		// printf("%s:%i - XSendEvent m=%i hnd=%p\n", __FILE__, __LINE__, gm ? gm->m() : 0, o->handle());
#endif
		
		XSendEvent(_App->Dsp, o->handle(), false, XNone, e->GetEvent());
		XSync(_App->Dsp, 0);
	}
}

int XApplication::wheelScrollLines()
{
	return 3;
}

