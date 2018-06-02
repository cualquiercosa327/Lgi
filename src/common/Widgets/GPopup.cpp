#include <stdio.h>
#include "Lgi.h"
#include "GPopup.h"
#include "GSkinEngine.h"
#include "GDisplayString.h"
#include "LThreadEvent.h"

enum PopupNotifications
{
    POPUP_DELETE = 1,
    POPUP_VISIBLE,
    POPUP_HIDE,
};

/////////////////////////////////////////////////////////////////////////////////////
#ifndef WH_MOUSE_LL
#define WH_MOUSE_LL        14
#endif

#if !defined(MAKELONG)
#define MAKELONG(low, high) ( ((low) & 0xffff) | ((high) << 16) )
#endif

#if defined(__GTK_H__)
using namespace Gtk;
class GMouseHookPrivate *HookPrivate = 0;
#include "LgiWidget.h"
		
bool IsWindow(OsView Wnd)
{
	// Do any available validation of the Wnd we can here
	#ifdef XWIN
	// return XWidget::Find(Wnd) != 0;
	#else
	return true;
	#endif
}

OsView WindowFromPoint(int x, int y)
{
	return NULL;
}

bool GetWindowRect(OsView Wnd, GRect &rc)
{
	return false;
}

bool ScreenToClient(OsView Wnd, GdcPt2 &p)
{
	return false;
}

#elif !defined(WINNATIVE)

bool IsWindow(OsView v)
{
	return true;
}

#endif

uint32 LgiGetViewPid(OsView View)
{
	#if WINNATIVE
	DWORD hWndProcess = 0;
	GetWindowThreadProcessId(View, &hWndProcess);
	return hWndProcess;
	#endif
	
	// FIXME: Linux and BeOS
	
	return LgiProcessId();
}

class GMouseHookPrivate : public ::LMutex, public ::LThread
{
public:
	bool Loop;
	OsView hMouseOver;
	List<GPopup> Popups;
	#ifdef MAC
	OsView ViewHandle;
	LThreadEvent Event;
	#endif

	GMouseHookPrivate() : LMutex("MouseHookLock"), LThread("MouseHook")
	{
		Loop = false;
		hMouseOver = 0;

		#ifdef MAC
		ViewHandle = NULL;
		#endif

		#if defined(LINUX)
		// LgiTrace("Mouse hook thread not running! (FIXME)\n");
		#else
		Loop = true;
		Run();
		#endif
	}

	~GMouseHookPrivate()
	{
		if (Loop)
		{
			Loop = false;
			#ifdef MAC
			Event.Signal();
			#endif
			
			while (!IsExited())
			{
				LgiSleep(10);
			}
		}
	}
	
	void PostEvent(OsView h, int c, int a, int b)
	{
		LgiPostEvent(h, c, (GMessage::Param) a, (GMessage::Param) b);
	}

	int Main()
	{
		GMouse Old;
		GView v;

		while (Loop)
		{
			#if defined(MAC) && !defined(LGI_SDL)
			
			// Wait for the down click...
			LThreadEvent::WaitStatus s = Event.Wait();
			if (!Loop || s != LThreadEvent::WaitSignaled)
			{
				break;
			}
			
			// Now loop for events...
			GMouse Cur, Prev;
			Prev.Down(true);
			do
			{
				#if COCOA
				#warning FIXME
				#else
				HIPoint p;
				HIGetMousePosition(kHICoordSpaceScreenPixel, NULL, &p);
				Cur.x = (int)p.x;
				Cur.y = (int)p.y;
				Cur.SetModifer(GetCurrentKeyModifiers());
				Cur.SetButton(GetCurrentEventButtonState());
				Cur.Down(Cur.Left() || Cur.Right() || Cur.Middle());
				
				// Cur.Trace("MouseHook");
				
				if (!Cur.Down() && Prev.Down())
				{
					// Up click...
					if (ViewHandle)
						LgiPostEvent(ViewHandle, M_MOUSE_TRACK_UP, 0, 0);
					else
						printf("%s:%i - No mouse hook view for up click.\n", _FL);
				}
				#endif
				
				Prev = Cur;
				LgiSleep(30);
			}
			while (Loop && Cur.Down());

			#else
			
			GMouse m;
			v.GetMouse(m, true);

			if (LockWithTimeout(500, _FL))
			{
				if (m.Down() && !Old.Down()) 
				{
					// Down click....
					uint64 Now = LgiCurrentTime();
					GPopup *Over = 0;
					GPopup *w;
					for (w = Popups.First(); w; w = Popups.Next())
					{
						if (w->GetPos().Overlap(m.x, m.y))
						{
							Over = w;
							break;
						}
					}
					
					for (w = Popups.First(); w; w = Popups.Next())
					{
						#if 0
						LgiTrace("PopupLoop: Over=%p w=%p, w->Vis=%i, Time=%i\n",
							Over,
							w,
							w->Visible(),
							(int) (Now - w->Start));
						#endif
							
						if (w != Over &&
							w->Visible() &&
							w->Start < Now - 100)
						{
							bool Close = true;

							#if defined WIN32
							// This is a bit of a hack to prevent GPopup's with open context menus from
							// closing when the user clicks on the context menu.
							//
							// FIXME: Linux/BeOS

							// Scan the window under the mouse up the parent tree
							POINT p = { m.x, m.y };
							bool HasPopupInParents = false;
							for (HWND hnd = WindowFromPoint(p); hnd; hnd = ::GetParent(hnd))
							{
								// Is this a popup?
								ULONG Style = GetWindowLong(hnd, GWL_STYLE);
								if (TestFlag(Style, WS_POPUP))
								{
									// No it's a normal window, so close the popup
									HasPopupInParents = true;
									break;
								}
							}
							
							Close = !HasPopupInParents;
							
							/*
							// Are we over a popup?
							RECT rc;
							GetWindowRect(hnd, &rc);
							GRect gr = rc;
							if (gr.Overlap(m.x, m.y))
							{
								// Yes, so don't close it
								LgiTrace("Popup: Got a click on a GPopup\n");
								Close = false;
								break;
							}
							*/

							#elif defined LINUX
							/*
							for (GViewI *v = Over; v; )
							{
								SubMenuImpl *Sub = dynamic_cast<SubMenuImpl*>(v);
								if (Sub)
								{
									if (v == w)
									{
										Close = false;
										break;
									}
									
									::GMenuItem *it = Sub->GetSub()->GetParent();
									GSubMenu *mn = it ? it->GetParent() : 0;
									MenuClickImpl *impl = mn ? mn->Handle() : 0;
									v = impl ? impl->View() : 0;
								}
								else v = 0;
							}
							*/
							#endif

							if (Close)
								w->PostEvent(M_SET_VISIBLE, (GMessage::Param)false);
						}
					}
				}

				Unlock();
			}

			if (m.x != Old.x ||
				m.y != Old.y)
			{
				// Mouse moved...
				OsView hOver = 0;

				#if WINNATIVE

				POINT WinPt = { m.x, m.y };
				hOver = WindowFromPoint(WinPt);

				RECT WinRect;
				GetClientRect(hOver, &WinRect);
				ScreenToClient(hOver, &WinPt);
				GRect rc = WinRect;
				GdcPt2 p(WinPt.x, WinPt.y);

				#elif defined __GTK_H__
				
				hOver = WindowFromPoint(m.x, m.y);
				GRect rc;
				GdcPt2 p(m.x, m.y);
				if (hOver)
				{
					if (!GetWindowRect(hOver, rc))
					{
						LgiTrace("No Rect for over\n");
					}
					if (!ScreenToClient(hOver, p))
					{
						LgiTrace("No conversion for point.\n");
					}
				}
				else
				{
					// LgiTrace("No hOver\n");
				}
				
				#else
				
				// Not implemented.
				GdcPt2 p;
				GRect rc;
				
				#endif

				// is the mouse inside the client area?
				bool Inside = ! (p.x < 0 ||
								p.y < 0 ||
								p.x >= rc.X() ||
								p.y >= rc.Y());
				
				OsView hWnd = (Inside) ? hOver : 0;

				uint32 hProcess = LgiProcessId();
				uint32 hWndProcess = 0;
				if (hWnd != hMouseOver)
				{
					// Window has changed
					if (hMouseOver && IsWindow(hMouseOver))
					{
						// Window is LOSING mouse
						// Using 'post' because send can cause a deadlock.
						hWndProcess = LgiGetViewPid(hMouseOver);
						if (hWndProcess == hProcess)
						{
							PostEvent(hMouseOver, M_MOUSEEXIT, 0, MAKELONG((short) p.x, (short) p.y));
						}
					}

					// Set current window
					hMouseOver = hWnd;

					if (hMouseOver && IsWindow(hMouseOver))
					{
						// Window is GETTING mouse
						// Using 'post' because send can cause a deadlock.
						hWndProcess = LgiGetViewPid(hMouseOver);
						if (hWndProcess == hProcess)
						{
							PostEvent(hMouseOver, M_MOUSEENTER, Inside, MAKELONG((short) p.x, (short) p.y));
						}
					}
				}
				else
				{
					hWndProcess = LgiGetViewPid(hMouseOver);
				}

				#if WINNATIVE
				if (hWndProcess == hProcess)
				{
					// This code makes sure that non-LGI windows generate mouse move events
					// for the mouse hook system...

					// First find the parent LGI window
					HWND hWnd = hMouseOver;
					GViewI *v = 0;
					while (hWnd && !(v = GWindowFromHandle(hMouseOver)))
					{
						HWND w = ::GetParent(hWnd);
						if (w)
							hWnd = w;
						else break;
					}

					// Get the window...
					GWindow *w = v ? v->GetWindow() : 0;

					// Post the event to the window
					PostEvent(w ? w->Handle() : hWnd, M_HANDLEMOUSEMOVE, MAKELONG((short) p.x, (short) p.y), (LPARAM)hMouseOver);
				}
				#endif
			}

			Old = m;

			LgiSleep(40);
			#endif
		}

		return 0;
	}
};

GMouseHook::GMouseHook()
{
	d = new GMouseHookPrivate;
}

GMouseHook::~GMouseHook()
{
	d->Lock(_FL);
	DeleteObj(d);
}

void GMouseHook::TrackClick(GView *v)
{
	#ifdef MAC
	if (v)
	{
		d->ViewHandle = v->Handle();
		if (d->ViewHandle)
		{
			d->Event.Signal();
		}
		else printf("%s:%i - No view handle.\n", _FL);
	}
	else printf("%s:%i - No view ptr.\n", _FL);
	#endif
}

bool GMouseHook::OnViewKey(GView *v, GKey &k)
{
	bool Status = false;

	if (d->Lock(_FL))
	{
		GView *l = d->Popups.Last();
		
		/*
		if (k.c16 == 13)
			LgiTrace("GMouseHook::OnViewKey d->p.Items=%i l=%p\n", d->p.Length(), l);
		*/

		if (l)
		{
			if (l->OnKey(k))
			{
				Status = true;
			}
		}
		
		d->Unlock();
	}
	
	return Status;
}

void GMouseHook::RegisterPopup(GPopup *p)
{
	if (d->Lock(_FL))
	{
		if (!d->Popups.HasItem(p))
		{
			d->Popups.Insert(p);
		}
		d->Unlock();
	}
}

void GMouseHook::UnregisterPopup(GPopup *p)
{
	if (d->Lock(_FL))
	{
		d->Popups.Delete(p);
		d->Unlock();
	}
}

#if defined(WIN32)
LRESULT CALLBACK GMouseHook::MouseProc(int Code, WPARAM a, LPARAM b)
{
	return 0;
}
#elif defined(CARBON)
WindowRef CreateBorderlessWindow()
{
	Rect r = {0,0,100,100};
	WindowRef wr;
	OSStatus e = CreateNewWindow
				(
					kDocumentWindowClass,
					(WindowAttributes)
					(
						kWindowStandardHandlerAttribute |
						kWindowCompositingAttribute |
						kWindowNoShadowAttribute |
						kWindowNoTitleBarAttribute
					),
					&r,
					&wr
				);
	if (e)
	{
		LgiTrace("%s:%i - Error: Creating popup window: %i\n", _FL, e);
		return NULL;
	}
	return wr;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////
class GPopupPrivate
{
public:
	bool TakeFocus;
	
	GPopupPrivate()
	{
		TakeFocus = true;
	}
};

#if !WINNATIVE
::GArray<GPopup*> GPopup::CurrentPopups;
#endif

GPopup::GPopup(GView *owner)
	#ifdef CARBON
	: GWindow(CreateBorderlessWindow())
	#endif
{
	d = new GPopupPrivate;
	Start = 0;
	Cancelled = false;
	#ifdef _DEBUG
	// _Debug = true;
	#endif

    #ifdef __GTK_H__
    Wnd = NULL;
    #endif
    #if !WINNATIVE
    CurrentPopups.Add(this);
    #endif

	if ((Owner = owner))
	{
		#ifndef WIN32
		Owner->PopupChild() = this;
		#endif
		
		#ifndef MAC
		_Window = Owner->GetWindow();
		#endif
		SetNotify(Owner);
	}
	
	GView::Visible(false);
}

GPopup::~GPopup()
{
	// LgiTrace("GPopup::~GPopup %p %p\n", this, d);

    #if !WINNATIVE
	CurrentPopups.Delete(this);
	#endif
	SendNotify(POPUP_DELETE);

	if (Owner)
	{
		#ifndef WIN32
		Owner->PopupChild() = 0;
		#endif
		
		#ifdef MAC
		GDropDown *dd = dynamic_cast<GDropDown*>(Owner);
		if (dd)
		{
			dd->Popup = 0;
		}
		#endif
	}

    #ifdef __GTK_H__
    Detach();
    if (Wnd)
        gtk_widget_destroy(Wnd);
    #endif

	GMouseHook *Hook = LgiApp->GetMouseHook();
	if (Hook) Hook->UnregisterPopup(this);

	for (GViewI *c; c = Children.First(); )
		delete c;
	DeleteObj(d);
}

GMessage::Result GPopup::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		#if WINNATIVE
		case WM_DESTROY:
		{
			// LgiTrace("Popup Destroyed.\n");
			break;
		}
		#endif
		case M_SET_VISIBLE:
		{
			Visible(Msg->A());
			break;
		}
	}
	
	return GView::OnEvent(Msg);
}

void GPopup::TakeFocus(bool Take)
{
	d->TakeFocus = Take;
}

#ifdef __GTK_H__
gboolean PopupDestroy(GtkWidget *widget, GPopup *This)
{
	delete This;
	return true;
}

gboolean PopupEvent(GtkWidget *widget, GdkEvent *event, GPopup *This)
{
	switch (event->type)
	{
		case GDK_CONFIGURE:
		{
			GdkEventConfigure *c = (GdkEventConfigure*)event;
			This->Pos.Set(c->x, c->y, c->x+c->width-1, c->y+c->height-1);
			This->OnPosChange();
			return FALSE;
			break;
		}
		case GDK_FOCUS_CHANGE:
		{
			This->OnFocus(event->focus_change.in);
			break;
		}
		case GDK_CLIENT_EVENT:
		{
			GMessage m(event);
			This->OnEvent(&m);
			break;
		}
		case GDK_BUTTON_PRESS:
		{
			break;
		}
		case GDK_EXPOSE:
		{
			GScreenDC s(This);
			This->OnPaint(&s);
			break;
		}
		default:
		{
			// LgiTrace("Unhandled PopupEvent type %i\n", event->type);
			break;
		}
	}
    return TRUE;
}

#endif

bool GPopup::Attach(GViewI *p)
{
	#if defined(CARBON)
	
	return GWindow::Attach(NULL);
	
	#else
	
	if (p) SetParent(p);
	else p = GetParent();

	#if WINNATIVE

	SetStyle(WS_POPUP);
	GView::Attach(p);
	AttachChildren();

	#elif defined __GTK_H__
	
	if (!Wnd)
	{
	    Wnd = gtk_window_new(GTK_WINDOW_POPUP);
	    
	    gtk_window_set_decorated(GTK_WINDOW(Wnd), FALSE);
		gtk_widget_add_events(Wnd, GDK_ALL_EVENTS_MASK);

		if (!p)
			p = Owner;
		GtkWidget *toplevel = p ? gtk_widget_get_toplevel(p->Handle()) : NULL;
		if (GTK_IS_WINDOW(toplevel))
			gtk_window_set_transient_for(GTK_WINDOW(Wnd), GTK_WINDOW(toplevel));
		else
		{
			LgiTrace("%s:%i - toplevel isn't window?\n", _FL);
			return false;
		}

        g_signal_connect(	G_OBJECT(Wnd),
							"button-press-event",
							G_CALLBACK(PopupEvent),
							this);
        g_signal_connect(	G_OBJECT(Wnd),
							"focus-in-event",
							G_CALLBACK(PopupEvent),
							this);
        g_signal_connect(	G_OBJECT(Wnd),
							"focus-out-event",
							G_CALLBACK(PopupEvent),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"delete_event",
							G_CALLBACK(PopupEvent),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"configure-event",
							G_CALLBACK(PopupEvent),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"button-press-event",
							G_CALLBACK(PopupEvent),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"client-event",
							G_CALLBACK(PopupEvent),
							this);
	}

	if (Wnd && Pos.Valid())
	{
		gtk_window_set_default_size(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
		gtk_window_move(GTK_WINDOW(Wnd), Pos.x1, Pos.y1);
	}

    if (!_View)
    {
	    _View = lgi_widget_new(this, Pos.X(), Pos.Y(), true);
	    gtk_container_add(GTK_CONTAINER(Wnd), _View);
	}

	#endif

	if (!_Window)
	{
		if (Owner)
			_Window = Owner->GetWindow();
		else
			_Window = p->GetWindow();
	}

	return Handle() != 0;

	#endif
}

void GPopup::Visible(bool i)
{
	#if defined __GTK_H__
	
		if (i && !Wnd)
		{
			if (!Attach(0))
			{
				printf("%s:%i - Attach failed.\n", _FL);
			    return;
			}
		}
		
		GView::Visible(i);
	    if (Wnd)
	    {
		    if (i)
		    {
		        gtk_widget_show_all(Wnd);
				gtk_window_move(GTK_WINDOW(Wnd), Pos.x1, Pos.y1);
				gtk_window_resize(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
				// printf("%s:%i - Showing Wnd %s.\n", _FL, Pos.GetStr());
		    }
		    else
		    {
		        gtk_widget_hide(Wnd);
				// printf("%s:%i - Hiding Wnd.\n", _FL);
		    }
		}
		else printf("%s:%i - No Wnd.\n", _FL);
		
	#else

		#ifdef WINNATIVE
		bool HadFocus = false;
		#endif
	
		#ifdef LGI_SDL
			GWindow *TopWnd = LgiApp->AppWnd;
			if (i && TopWnd)
			{
				if (!TopWnd->HasView(this))
					TopWnd->AddView(this);
			}
		#else

			if (!Handle() && i)
			{
				#if WINNATIVE
				SetStyle(WS_POPUP);
				#endif
				Attach(NULL);
			}
		#endif

		if (!_Window && Owner)
		{
			_Window = Owner->GetWindow();
		}
		
		AttachChildren();

		#ifdef WINNATIVE
		
			// See if we or a child window has the focus...
			for (HWND hWnd = GetFocus(); hWnd; hWnd = ::GetParent(hWnd))
			{
				if (hWnd == Handle())
				{
					HadFocus = true;
					break;
				}
			}
			
			if (d->TakeFocus || !i)
				GView::Visible(i);
			else
				ShowWindow(Handle(), SW_SHOWNA);
	
		#elif defined(CARBON)
	
			SetAlwaysOnTop(true);
			GWindow::Visible(i);
	
		#else
		
			bool HadFocus = Focus();
			GView::Visible(i);
	
		#endif

	#endif
	
	#if 1
	
		if (i)
		{
			Start = LgiCurrentTime();

			GMouseHook *Hook = LgiApp->GetMouseHook();
			if (Hook)
				Hook->RegisterPopup(this);

			if (!_Window)
			{
				if (Owner)
				{
					_Window = Owner->GetWindow();
				}
				else if (GetParent())
				{
					_Window = GetParent()->GetWindow();
				}
			}
		}
		else
		{
			GMouseHook *Hook = LgiApp->GetMouseHook();
			if (Hook)
				Hook->UnregisterPopup(this);

			SendNotify(POPUP_HIDE);

			/*
			#if WINNATIVE
			// This is required to re-focus the owner.
			// If the popup or a child window gets focus at some point. The
			// owner doesn't get focus when we close... weird I know.
			if (Owner && HadFocus)
			{
				LgiTrace("%s Setting owner focus %s\n", GetClass(), Owner->GetClass());
				Owner->Focus(true);
			}
			#endif
			*/
		}
		
	#endif

	#ifdef LGI_SDL

		if (TopWnd)
			TopWnd->Invalidate();

	#endif
}

bool GPopup::Visible()
{
    #if defined __GTK_H__
    
	    if (Wnd)
	    {
			GView::Visible(
							#if GtkVer(2, 18)
							gtk_widget_get_visible(Wnd)
							#else
							(GTK_OBJECT_FLAGS (Wnd) & GTK_VISIBLE) != 0
							#endif
							);
	    }
	    
    #endif

	#if defined(CARBON)
	bool v = GWindow::Visible();
	#else
	bool v = GView::Visible();
	#endif
	return v;
}

/////////////////////////////////////////////////////////////////////////////////////
GDropDown::GDropDown(int Id, int x, int y, int cx, int cy, GPopup *popup)
{
	SetId(Id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	if ((Popup = popup))
		Popup->SetNotify(this);
	SetTabStop(true);
}

GDropDown::~GDropDown()
{
	DeleteObj(Popup);
}

void GDropDown::OnFocus(bool f)
{
	Invalidate();
}

GPopup *GDropDown::GetPopup()
{
	return Popup;
}

void GDropDown::SetPopup(GPopup *popup)
{
	DeleteObj(Popup);
	Popup = popup;
	Invalidate();
}

bool GDropDown::IsOpen()
{
	return Popup && Popup->Visible();
}

void GDropDown::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();
	r.Offset(-r.x1, -r.y1);
	if (!r.Valid())
		return;

	#if defined(MAC) && !defined(COCOA) && !defined(LGI_SDL)
	GColour NoPaintColour(LC_MED, 24);
	if (GetCss())
	{
		GCss::ColorDef NoPaint = GetCss()->NoPaintColor();
		if (NoPaint.Type == GCss::ColorRgb)
			NoPaintColour.Set(NoPaint.Rgb32, 32);
		else if (NoPaint.Type == GCss::ColorTransparent)
			NoPaintColour.Empty();
	}
	if (!NoPaintColour.IsTransparent())
	{
		pDC->Colour(NoPaintColour);
		pDC->Rectangle();
	}
	
	GRect rc = GetClient();
	rc.x1 += 2;
	rc.y2 -= 1;
	rc.x2 -= 1;
	HIRect Bounds = rc;
	HIThemeButtonDrawInfo Info;
	HIRect LabelRect;
	
	Info.version = 0;
	Info.state = Enabled() ? kThemeStateActive : kThemeStateInactive;
	Info.kind = kThemePushButton;
	Info.value = kThemeButtonOff;
	Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;
	
	OSStatus e = HIThemeDrawButton(	  &Bounds,
									  &Info,
									  pDC->Handle(),
									  kHIThemeOrientationNormal,
									  &LabelRect);
	
	if (e) printf("%s:%i - HIThemeDrawButton failed %li\n", _FL, e);
	#else
	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_BUTTON))
	{
		GMemDC Mem(r.X(), r.Y(), System24BitColourSpace);

		GCss::ColorDef f;
		if (GetCss())
			f = GetCss()->BackgroundColor();
		if (f.Type == GCss::ColorRgb)
			Mem.Colour(f.Rgb32, 32);
		else
			Mem.Colour(LC_MED, 24);
		Mem.Rectangle();

		GApp::SkinEngine->DrawBtn(&Mem, r, NULL, IsOpen(), Enabled());
		pDC->Blt(0, 0, &Mem);
		r.Size(2, 2);
		r.x2 -= 2;
	}
	else
	{
		LgiWideBorder(pDC, r, IsOpen() ? DefaultSunkenEdge : DefaultRaisedEdge);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
		if (Focus())
		{
			#if WINNATIVE
			DrawFocusRect(pDC->Handle(), &((RECT)r));
			#else
			pDC->Colour(LC_LOW, 24);
			pDC->Box(&r);
			#endif
		}
	}
	#endif

    int ArrowWidth = 5;
    double Aspect = (double)r.X() / r.Y();
	int Cx = Aspect < 1.2 ? r.x1 + ((r.X() - ArrowWidth) >> 1) : r.x2 - (ArrowWidth << 1);
	int Cy = r.y1 + ((r.Y() - 3) >> 1);
	pDC->Colour(Enabled() && Popup ? LC_TEXT : LC_LOW, 24);
	if (IsOpen())
	{
		Cx++;
		Cy++;
	}
	for (int i=0; i<3; i++)
	{
		pDC->Line(Cx+i, Cy+i, Cx+ArrowWidth-i, Cy+i);
	}

	char *Nm = Name();
	if (Nm)
	{
		GDisplayString Ds(SysFont, Nm);
		SysFont->Colour(LC_TEXT, LC_MED);
		SysFont->Transparent(true);
		int Offset = IsOpen() ? 1 : 0;
		Ds.Draw(pDC, (Cx-Ds.X())/2+Offset+r.x1, (Y()-Ds.Y())/2+Offset);
	}
}

void GDropDown::Activate()
{
	if (IsOpen())
	{
		// Hide
		Popup->Visible(false);
	}
	else // Show
	{
		// Locate under myself
		GdcPt2 p(X()-1, Y());
		PointToScreen(p);
		GRect r(p.x-Popup->X()+1, p.y, p.x, p.y+Popup->Y()-1);
		
		// Show the popup
		if (!Popup->IsAttached())
		{
			Popup->Attach(this);
		}

		Popup->Cancelled = false;
		Popup->SetPos(r);
		Popup->Visible(true);
	}

	Invalidate();
}

bool GDropDown::OnKey(GKey &k)
{
	if (k.IsChar && (k.c16 == ' ' || k.c16 == VK_RETURN))
	{
		if (k.Down())
		{
			Activate();
		}
		
		return true;
	}
	else if (k.vkey == VK_ESCAPE)
	{
		if (k.Down() && IsOpen())
		{
			Activate();
		}
		
		return true;
	}
	
	return false;
}

void GDropDown::OnMouseClick(GMouse &m)
{
	if (Popup && m.Down())
	{
		Focus(true);
		Activate();
	}
}

int GDropDown::OnNotify(GViewI *c, int f)
{
	if (c == (GViewI*)Popup)
	{
		switch (f)
		{
			case POPUP_DELETE:
			{
				Popup = 0;
				break;
			}
			case POPUP_VISIBLE:
			{
				break;
			}
			case POPUP_HIDE:
			{
				Invalidate();
				OnPopupClose();
				break;
			}
		}
	}

	return false;
}
