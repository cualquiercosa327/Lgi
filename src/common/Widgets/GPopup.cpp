#include <stdio.h>
#include "Lgi.h"
#include "GPopup.h"
#include "GSkinEngine.h"
#ifdef LINUX
#include "GMenuImpl.h"
#endif

#define POPUP_DELETE		1
#define POPUP_VISIBLE		2
#define POPUP_HIDE			3

/////////////////////////////////////////////////////////////////////////////////////
#ifndef WH_MOUSE_LL
#define WH_MOUSE_LL        14
#endif

#if !defined(MAKELONG)
#define MAKELONG(low, high) \
	( ((low) & 0xffff) | ((high) << 16) )
#endif

#if defined(__GTK_H__)
using namespace Gtk;
class GMouseHookPrivate *HookPrivate = 0;
#include "LgiWidget.h"
		
OsView WindowFromPoint(int x, int y)
{
	/*
	OsView Root = XcbScreen()->root;

	OsView w1 = Root;
	OsView w2 = Root;
	int x1 = x, y1 = y, x2, y2;
	xcb_translate_coordinates_reply_t *r;

	// printf("WindowFromPoint(%i,%i)\n", x, y);
	while (r = xcb_translate_coordinates_reply(	XcbConn(),
												xcb_translate_coordinates(XcbConn(), w1, w2, x1, y1),
												0))
	{
		// printf("\t%x,%x,%x %i,%i->%i,%i\n", w1,w2,r->child,x1,y1,r->dst_x, r->dst_y);
		if (r->child == 0)
		{
			return w2;
		}

		w1 = w2;
		w2 = r->child;
		x1 = r->dst_x;
		y1 = r->dst_y;
	}
	*/
	
	return 0;
}

bool GetWindowRect(OsView Wnd, GRect &rc)
{
	/*
	xcb_get_geometry_cookie_t c =
		xcb_get_geometry(XcbConn(), Wnd);
	xcb_get_geometry_reply_t *r =
		xcb_get_geometry_reply(XcbConn(), c, 0);
	if (r)
	{
		rc.Set(r->x, r->y, r->x+r->width-1, r->y+r->height-1);
		return true;
	}
	*/

	return false;
}

bool ScreenToClient(OsView Wnd, GdcPt2 &p)
{
	/*
	xcb_translate_coordinates_reply_t *r;
	if (r = xcb_translate_coordinates_reply(XcbConn(),
											xcb_translate_coordinates(XcbConn(),
												Wnd,
												XcbScreen()->root,
												p.x,
												p.y),
											0))
	{
		p.x += r->dst_x;
		p.y += r->dst_y;
		return true;
	}
	*/

	return false;
}

bool IsWindow(OsView Wnd)
{
	// Do any available validation of the Wnd we can here
	#ifdef XWIN
	// return XWidget::Find(Wnd) != 0;
	#else
	return true;
	#endif
}

#elif defined MAC

bool IsWindow(OsView v)
{
	return true;
}

#endif

uint32 LgiGetViewPid(OsView View)
{
	#if WIN32NATIVE
	DWORD hWndProcess = 0;
	GetWindowThreadProcessId(View, &hWndProcess);
	return hWndProcess;
	#endif
	
	// FIXME: Linux and BeOS
	
	return LgiProcessId();
}

class GMouseHookPrivate : public GMutex, public ::GThread
{
public:
	bool Loop;
	OsView hMouseOver;
	List<GPopup> Popups;

	GMouseHookPrivate() : GMutex("MouseHookLock"), GThread("MouseHook")
	{
		Loop = false;
		hMouseOver = 0;

		#ifdef XWIN
		HookPrivate = this;
		// XWidget::DestroyNotifyHandlers[0] = XWinDestroyHandler;
		#endif
		
		#ifdef MAC
		LgiTrace("Mouse hook thread not running! (FIXME)\n");
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
			while (!IsExited())
			{
				LgiSleep(10);
			}
		}

		#ifdef XWIN
		// XWidget::DestroyNotifyHandlers[0] = 0;
		HookPrivate = 0;
		#endif
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
			if (LockWithTimeout(500, _FL))
			{
				GMouse m;
				v.GetMouse(m, true);

				if (m.Down() && !Old.Down()) 
				{
					// Down click....
					int64 Now = LgiCurrentTime();
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
						printf("Popup loop %i,%i,%i\n",
							w != Over,
							w->Visible(),
							w->Start < Now - 100);
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
							// FIXME: Linux/BeOS/Mac

							// Scan the window under the mouse up the parent tree
							POINT p = { m.x, m.y };
							for (HWND hnd = WindowFromPoint(p); hnd; hnd = ::GetParent(hnd))
							{
								// Is this a popup?
								ULONG Style = GetWindowLong(hnd, GWL_STYLE);
								if (!TestFlag(Style, WS_POPUP))
								{
									// No it's a normal window, so close the popup
									LgiTrace("Popup: Got a click on a WS_POPUP\n");
									break;
								}

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
							}
							#elif defined LINUX
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
									
									GMenuItem *it = Sub->GetSub()->GetParent();
									GSubMenu *mn = it ? it->GetParent() : 0;
									MenuClickImpl *impl = mn ? mn->Handle() : 0;
									v = impl ? impl->View() : 0;
								}
								else v = 0;
							}
							#endif

							if (Close)
							{
								w->Visible(false);
							}
						}
					}
				}

				if (m.x != Old.x ||
					m.y != Old.y)
				{
					// Mouse moved...
					OsView hOver = 0;

					#if WIN32NATIVE

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
							printf("No Rect for over\n");
						}
						if (!ScreenToClient(hOver, p))
						{
							printf("No conversion for point.\n");
						}
					}
					else
					{
						// printf("No hOver\n");
					}
					
					#elif defined(MAC) || defined(BEOS)
					
					// FIXME
					GdcPt2 p(0, 0);
					GRect rc(0, 0, -1, -1);
					// LgiAssert(0);
					
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
						if (hMouseOver AND IsWindow(hMouseOver))
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

						if (hMouseOver AND IsWindow(hMouseOver))
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

					#if WIN32NATIVE
					if (hWndProcess == hProcess)
					{
						// This code makes sure that non-LGI windows generate mouse move events
						// for the mouse hook system...

						// First find the parent LGI window
						HWND hWnd = hMouseOver;
						GViewI *v = 0;
						while (hWnd AND !(v = GWindowFromHandle(hMouseOver)))
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

				Unlock();
			}

			LgiSleep(40);
		}

		return 0;
	}
};

#ifdef XWIN
/*
void XWinDestroyHandler(XWidget *w)
{
	if (HookPrivate AND w)
	{
		if (HookPrivate->Lock(_FL))
		{
			if (HookPrivate->hMouseOver == w->handle())
			{
				HookPrivate->hMouseOver = 0;
			}
			
			HookPrivate->Unlock();
		}
		else printf("%s:%i - error\n", __FILE__, __LINE__);
	}
	else printf("%s:%i - error\n", __FILE__, __LINE__);
}
*/
#endif

GMouseHook::GMouseHook()
{
	d = new GMouseHookPrivate;
}

GMouseHook::~GMouseHook()
{
	d->Lock(_FL);
	DeleteObj(d);
}

bool GMouseHook::OnViewKey(GView *v, GKey &k)
{
	bool Status = false;

	if (d->Lock(_FL))
	{
		GView *l = d->Popups.Last();
		
		/*
		if (k.c16 == 13)
			printf("GMouseHook::OnViewKey d->p.Items=%i l=%p\n", d->p.Length(), l);
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
};

GPopup::GPopup(GView *owner)
{
	d = new GPopupPrivate;
	Start = 0;
	Cancelled = false;

    #ifdef __GTK_H__
    Wnd = 0;
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

#if defined MAC
bool GPopup::SetPos(GRect &p, bool Repaint)
{
	GdcPt2 o(0, 0);
	if (GetWindow()) GetWindow()->PointToScreen(o);
	GRect r = p;
	r.Offset(-o.x, -o.y);
	return GView::SetPos(r, Repaint);
}

GRect &GPopup::GetPos()
{
	static GRect p;
	p = GView::GetPos();
	GdcPt2 o(0, 0);
	if (GetWindow()) GetWindow()->PointToScreen(o);
	// p.Offset(o.x, o.y);
	return p;
}
#endif

#ifdef __GTK_H__
static
gboolean
GPopupFocusOut( GtkWidget       *widget,
				GdkEventFocus   *ev,
				GPopup          *This)
{
    This->Visible(false);
	return FALSE;
}

static
gboolean
GPopupExpose(	GtkWidget		*widget,
				GdkEventExpose	*ev,
				GtkWidget		*top)
{
	if (gtk_window_get_resizable(GTK_WINDOW(top)))
	{
		gtk_window_resize(GTK_WINDOW(top), widget->allocation.width, widget->allocation.height);
		gtk_window_set_resizable(GTK_WINDOW(top), FALSE);
	}
	return FALSE;
}

static void
PopupPos(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, GdcPt2 *user_data)
{
    *x = user_data->x;
    *y = user_data->y;    
}

void PopupDetach(GtkWidget *attach_widget, GtkMenu *menu)
{
    LgiTrace("PopupDetach\n");
}

void PopupCancel(GtkMenuShell *menushell, gpointer user_data)
{
    LgiTrace("PopupCancel\n");
}

void PopupSelect(GtkMenuShell *menushell, gpointer user_data)
{
    LgiTrace("PopupSelect\n");
}

void PopupGrabBroken(GtkWidget *widget, GdkEventGrabBroken *ev, GPopup *popup)
{
    popup->Visible(false);
}

gboolean PopupButtonEvent(GtkWidget *widget, GdkEventButton *e, GPopup *popup)
{
    LgiTrace("PopupButtonPress cur_grab=%p\n", gtk_grab_get_current());
    gtk_grab_remove(widget);
    
    GtkWindow *WidgetParent = (GtkWindow*)gtk_widget_get_parent_window(widget);
    GtkWindow *PopupParent = (GtkWindow*)gtk_widget_get_parent_window(popup->Handle());
    bool Over = WidgetParent == PopupParent;
    if (!Over)
    {
        popup->Visible(false);
    }
    
    return TRUE;            
}

gboolean PopupMapEvent(GtkWidget *widget, GdkEvent *e, GPopup *Wnd)
{
	printf("Adding grab for popup...\n");
    gtk_grab_add(widget);
	return FALSE;
}

#endif

bool GPopup::Attach(GViewI *p)
{
	#if defined MAC && !defined COCOA
	
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
			else printf("%s:%i - error\n", _FL);
		}
		else printf("%s:%i - error\n", _FL);
	}
	else printf("%s:%i - error\n", _FL);
	
	return false;
	
	#else
	
	if (p) SetParent(p);
	else p = GetParent();
	if (p)
	{
		#if WIN32NATIVE

		SetStyle(WS_POPUP);
		GView::Attach(GetParent());
		
		AttachChildren();

		#elif defined __GTK_H__
		
		if (!Wnd)
		{
		    Wnd = gtk_window_new(GTK_WINDOW_POPUP);
		    // Wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		    gtk_window_set_decorated(GTK_WINDOW(Wnd), FALSE);
		    gtk_window_set_type_hint(GTK_WINDOW(Wnd), GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU);
    		gtk_widget_add_events(Wnd, gtk_widget_get_events(Wnd) | GDK_BUTTON_PRESS_MASK);

            g_signal_connect (G_OBJECT(Wnd),
                            "button-press-event",
                            G_CALLBACK(PopupButtonEvent),
                            this);
            g_signal_connect (G_OBJECT(Wnd),
                            "map-event",
                            G_CALLBACK(PopupMapEvent),
                            this);
		}

		if (Wnd)
		{
		    // LgiTrace("Attaching Popup at %s\n", Pos.GetStr());
			gtk_window_move(GTK_WINDOW(Wnd), Pos.x1, Pos.y1);
		    gtk_window_set_default_size(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
		}

        if (!_View)
        {
		    _View = lgi_widget_new(this, Pos.X(), Pos.Y(), false);
		    gtk_container_add(GTK_CONTAINER(Wnd), _View);
		}		
		#endif

		if (!_Window)
		{
			if (Owner)
			{
				_Window = Owner->GetWindow();
			}
			else
			{
				_Window = p->GetWindow();
			}
		}
	}
	else printf("%s:%i - Error, no parent.\n", _FL);

	return Handle() != 0;

	#endif
}

void GPopup::Visible(bool i)
{
	bool Was = GView::Visible();

	#if defined __GTK_H__
	if (i && !Wnd)
	{
		if (!Attach(0))
		    return;
	}
	
    if (Wnd)
    {
	    if (i)
	        gtk_widget_show_all(Wnd);
	    else
	    {
	        gtk_grab_remove(Wnd);
	        gtk_widget_hide(Wnd);
	        // gdk_pointer_ungrab(GDK_CURRENT_TIME);
	    }

		GView::Visible(i);
	}
	#else
	if (!Handle() && i)
		GView::Attach(0);
	AttachChildren();
	GView::Visible(i);
	#endif	
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
	}
}

bool GPopup::Visible()
{
    #if defined __GTK_H__
    if (Wnd)
    {
        GView::Visible(gtk_widget_get_visible(Wnd));
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

	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_BUTTON))
	{
		GMemDC Mem(r.X(), r.Y(), 24);

		GViewFill *f = GetBackgroundFill();
		if (f)
			f->Fill(&Mem);
		else
		{
			Mem.Colour(LC_MED, 24);
			Mem.Rectangle();
		}

		GApp::SkinEngine->DrawBtn(&Mem, r, IsOpen(), Enabled());
		pDC->Blt(0, 0, &Mem);
		r.Size(2, 2);
		r.x2 -= 2;
	}
	else
	{
		LgiWideBorder(pDC, r, IsOpen() ? SUNKEN : RAISED);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
		if (Focus())
		{
			#if WIN32NATIVE
			DrawFocusRect(pDC->Handle(), &((RECT)r));
			#else
			pDC->Colour(LC_LOW, 24);
			pDC->Box(&r);
			#endif
		}
	}

	int Cx = r.x2 - 7;
	int Cy = r.y1 + ((r.Y() - 3) >> 1);
	pDC->Colour(Enabled() AND Popup ? LC_TEXT : LC_LOW, 24);
	if (IsOpen())
	{
		Cx++;
		Cy++;
	}
	for (int i=0; i<3; i++)
	{
		pDC->Line(Cx+i, Cy+i, Cx+5-i, Cy+i);
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
	if (k.IsChar AND (k.c16 == ' ' || k.c16 == VK_RETURN))
	{
		if (k.Down())
		{
			Activate();
		}
		
		return true;
	}
	else if (k.vkey == VK_ESCAPE)
	{
		if (k.Down() AND IsOpen())
		{
			Activate();
		}
		
		return true;
	}
	
	return false;
}

void GDropDown::OnMouseClick(GMouse &m)
{
	if (Popup AND m.Down())
	{
		Focus(true);
		Activate();
	}
}

int GDropDown::OnNotify(GViewI *c, int f)
{
	if (c == Popup)
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
