#include <stdio.h>
#include "Lgi.h"
#include "GPopup.h"
#include "GSkinEngine.h"
#include "GDisplayString.h"
#include "GThreadEvent.h"

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

#elif defined MAC

bool IsWindow(OsView v)
{
	return true;
}

#elif defined BEOS

static bool IsWindow(OsView Wnd)
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

class GMouseHookPrivate : public ::GMutex, public ::GThread
{
public:
	bool Loop;
	OsView hMouseOver;
	List<GPopup> Popups;
	#ifdef MAC
	OsView ViewHandle;
	GThreadEvent Event;
	#endif

	GMouseHookPrivate() : GMutex("MouseHookLock"), GThread("MouseHook")
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
		LgiPostEvent(h, c, a, b);
	}

	int Main()
	{
		GMouse Old;
		GView v;

		while (Loop)
		{
			#ifdef MAC
			
			// Wait for the down click...
			GThreadEvent::WaitStatus s = Event.Wait();
			if (!Loop || s != GThreadEvent::WaitSignaled)
			{
				break;
			}
			
			// Now loop for events...
			GMouse Cur, Prev;
			Prev.Down(true);
			do
			{
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

							// LgiTrace("    Close=%i\n", Close);
							if (Close)
							{
								// LgiTrace("Sending M_SET_VISIBLE(%i) to %s\n", false, w->GetClass());
								w->PostEvent(M_SET_VISIBLE, false);
							}
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
				
				#elif defined(MAC) || defined(BEOS)
				
				// Not implemented.
				
				#else

				#error "Not Impl."

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

#ifdef WIN32
LRESULT CALLBACK GMouseHook::MouseProc(int Code, WPARAM a, LPARAM b)
{
	return 0;
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
{
	d = new GPopupPrivate;
	Start = 0;
	Cancelled = false;

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
		
		_Window = Owner->GetWindow();
		SetNotify(Owner);
	}
	
	GView::Visible(false);
}

GPopup::~GPopup()
{
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

	Children.DeleteObjects();
	DeleteObj(d);
}

GMessage::Param GPopup::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		#if WINNATIVE
		case WM_DESTROY:
		{
			// LgiTrace("Popup Destroyed.\n");
			break;
		}
		case M_SET_VISIBLE:
		{
			Visible(Msg->a);
			break;
		}
		#endif
	}
	
	return GView::OnEvent(Msg);
}

void GPopup::TakeFocus(bool Take)
{
	d->TakeFocus = Take;
}

#if defined MAC
bool GPopup::SetPos(GRect &p, bool Repaint)
{
	GdcPt2 o(0, 0);
	if (GetWindow())
        GetWindow()->PointToScreen(o);
	GRect r = p;
	r.Offset(-o.x, -o.y);
	return GView::SetPos(r, Repaint);
}

GRect &GPopup::GetPos()
{
	static GRect p;
	p = GView::GetPos();
	GdcPt2 o(0, 0);
	if (GetWindow())
        GetWindow()->PointToScreen(o);
	// p.Offset(o.x, o.y);
	return p;
}
#endif

#ifdef __GTK_H__
gboolean PopupEvent(GtkWidget *widget, GdkEvent *event, GPopup *This)
{
	switch (event->type)
	{
		case GDK_DELETE:
		{
			// delete This;
			break;
		}
		case GDK_DESTROY:
		{
			// LgiTrace("PopupEvent.Destroy\n");
			delete This;
			return TRUE;
		}
		case GDK_CONFIGURE:
		{
			GdkEventConfigure *c = (GdkEventConfigure*)event;
			This->Pos.Set(c->x, c->y, c->x+c->width-1, c->y+c->height-1);
			This->OnPosChange();
			
			// LgiTrace("PopupEvent.Config\n");
			return FALSE;
			break;
		}
		case GDK_FOCUS_CHANGE:
		{
			// LgiTrace("PopupEvent.Focus(%i)\n", event->focus_change.in);
			This->OnFocus(event->focus_change.in);
			break;
		}
		case GDK_CLIENT_EVENT:
		{
			GMessage m(event);
			// LgiTrace("PopupEvent.Client\n");
			This->OnEvent(&m);
			break;
		}
		case GDK_BUTTON_PRESS:
		{
			// LgiTrace("PopupEvent.Button\n");
			break;
		}
		case GDK_EXPOSE:
		{
			GScreenDC s(This->Handle());
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
	#if defined MAC
	
		#if !defined COCOA
		if (p)
		{
			GWindow *w = p->GetWindow();
			if (w)
			{
				if (GView::Attach(w))
				{
					HIViewRef p = HIViewGetSuperview(_View);
					HIViewRef f = HIViewGetFirstSubview(p);
					if (f != _View)
					{
						HIViewSetZOrder(_View, kHIViewZOrderAbove, 	f);
					}
					
					AttachChildren();
					return true;
				}
				else LgiTrace("%s:%i - error\n", _FL);
			}
			else LgiTrace("%s:%i - error\n", _FL);
		}
		else LgiTrace("%s:%i - error\n", _FL);
		#endif
	
	return false;
	
	#else
	
	if (p) SetParent(p);
	else p = GetParent();
	if (p)
	{
		#if WINNATIVE

		SetStyle(WS_POPUP);
		GView::Attach(GetParent());
		
		AttachChildren();

		#elif defined __GTK_H__
		
		if (!Wnd)
		{
		    // Wnd = gtk_window_new(GTK_WINDOW_POPUP);
		    Wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		    gtk_window_set_decorated(GTK_WINDOW(Wnd), FALSE);
		    // gtk_window_set_type_hint(GTK_WINDOW(Wnd), GDK_WINDOW_TYPE_HINT_COMBO);
    		gtk_widget_add_events(Wnd, GDK_ALL_EVENTS_MASK);

			#if 1
			GViewI *p = GetParent();
			GtkWidget *toplevel = gtk_widget_get_toplevel(p->Handle());
			if (GTK_IS_WINDOW(toplevel))
			{
				gtk_window_set_transient_for(GTK_WINDOW(Wnd), GTK_WINDOW(toplevel));
			}
			#endif

            #if 1
            // printf("Popup connect Wnd=%p, this=%p\n", GTK_WIDGET(Wnd), this);
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
								"destroy",
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
			#endif
		}

		if (Wnd && Pos.Valid())
		{
			gtk_window_set_default_size(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
			gtk_window_move(GTK_WINDOW(Wnd), Pos.x1, Pos.y1);
		}

		#if 1
        if (!_View)
        {
		    _View = lgi_widget_new(this, Pos.X(), Pos.Y(), true);
		    gtk_container_add(GTK_CONTAINER(Wnd), _View);
		}
		#endif

		#endif

		if (!_Window)
		{
			if (Owner)
				_Window = Owner->GetWindow();
			else
				_Window = p->GetWindow();
		}
	}
	else LgiTrace("%s:%i - Error, no parent.\n", _FL);

	return Handle() != 0;

	#endif
}

void GPopup::Visible(bool i)
{
	bool HadFocus = false;
	bool Was = GView::Visible();

	// LgiStackTrace("GPopup::Visible(%i)\n", i);

	#if defined __GTK_H__
	if (i && !Wnd)
	{
		if (!Attach(0))
		    return;
	}
	
	GView::Visible(i);
    if (Wnd)
    {
	    if (i)
	    {
	        gtk_widget_show_all(Wnd);
			gtk_window_move(GTK_WINDOW(Wnd), Pos.x1, Pos.y1);
			// gtk_window_resize(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
	    }
	    else
	    {
	        gtk_widget_hide(Wnd);
	    }
	}
	#else
	if (!Handle() && i)
	{
		#if WINNATIVE
		SetStyle(WS_POPUP);
		#endif
		GView::Attach(0);
	}

	if (!_Window && Owner)
	{
		_Window = Owner->GetWindow();
	}
	
	AttachChildren();

	#ifdef WIN32
	// See if we or a child window has the focus...
	for (HWND hWnd = GetFocus(); hWnd; hWnd = ::GetParent(hWnd))
	{
		if (hWnd == Handle())
		{
			HadFocus = true;
			break;
		}
	}
	
	// LgiTrace("%s HasFocus=%i\n", GetClass(), HadFocus);
	
	if (d->TakeFocus || !i)
		GView::Visible(i);
	else
		ShowWindow(Handle(), SW_SHOWNA);
	#else
	HadFocus = Focus();
	GView::Visible(i);
	#endif
	#endif
	
	#if 1
	if (i)
	{
		Start = LgiCurrentTime();

		GMouseHook *Hook = LgiApp->GetMouseHook();
		if (Hook) Hook->RegisterPopup(this);

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
		if (Hook) Hook->UnregisterPopup(this);

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

	return GView::Visible();
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

		GApp::SkinEngine->DrawBtn(&Mem, r, IsOpen(), Enabled());
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
