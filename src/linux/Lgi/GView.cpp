/*hdr
**      FILE:           GView.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           23/4/98
**      DESCRIPTION:    Linux GView Implementation
**
**      Copyright (C) 1998-2003, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GEdit.h"
#include "GViewPriv.h"
#include "GPopup.h"

using namespace Gtk;
#include "LgiWidget.h"

#define ADJ_LEFT					1
#define ADJ_RIGHT					2
#define ADJ_UP						3
#define ADJ_DOWN					4

struct X11_INVALIDATE_PARAMS
{
	GView *View;
	GRect Rgn;
	bool Repaint;
};

#if GtkVer(2, 14)
#else
#define gtk_widget_get_window(widget) ((widget)->window)
#endif

struct CursorInfo
{
public:
	GRect Pos;
	GdcPt2 HotSpot;
}
CursorMetrics[] =
{
	// up arrow
	{ GRect(0, 0, 8, 15),			GdcPt2(4, 0) },
	// cross hair
	{ GRect(20, 0, 38, 18),			GdcPt2(29, 9) },
	// hourglass
	{ GRect(40, 0, 51, 15),			GdcPt2(45, 8) },
	// I beam
	{ GRect(60, 0, 66, 17),			GdcPt2(63, 8) },
	// N-S arrow
	{ GRect(80, 0, 91, 16),			GdcPt2(85, 8) },
	// E-W arrow
	{ GRect(100, 0, 116, 11),		GdcPt2(108, 5) },
	// NW-SE arrow
	{ GRect(120, 0, 132, 12),		GdcPt2(126, 6) },
	// NE-SW arrow
	{ GRect(140, 0, 152, 12),		GdcPt2(146, 6) },
	// 4 way arrow
	{ GRect(160, 0, 178, 18),		GdcPt2(169, 9) },
	// Blank
	{ GRect(0, 0, 0, 0),			GdcPt2(0, 0) },
	// Vertical split
	{ GRect(180, 0, 197, 16),		GdcPt2(188, 8) },
	// Horizontal split
	{ GRect(200, 0, 216, 17),		GdcPt2(208, 8) },
	// Hand
	{ GRect(220, 0, 233, 13),		GdcPt2(225, 0) },
	// No drop
	{ GRect(240, 0, 258, 18),		GdcPt2(249, 9) },
	// Copy drop
	{ GRect(260, 0, 279, 19),		GdcPt2(260, 0) },
	// Move drop
	{ GRect(280, 0, 299, 19),		GdcPt2(280, 0) },
};

// CursorData is a bitmap in an array of uint32's. This is generated from a graphics file:
// ./Code/cursors.png
//
// The pixel values are turned into C code by a program called i.Mage:
//	http://www.memecode.com/image.php
//
// Load the graphic into i.Mage and then go Edit->CopyAsCode
// Then paste the text into the CursorData variable at the bottom of this file.
//
// This saves a lot of time finding and loading an external resouce, and even having to
// bundle extra files with your application. Which have a tendancy to get lost along the
// way etc.
extern uint32 CursorData[];
GInlineBmp Cursors =
{
	300, 20, 8, CursorData
};

////////////////////////////////////////////////////////////////////////////
void _lgi_yield()
{
	LgiApp->Run(false);
}

bool LgiIsKeyDown(int Key)
{
	LgiAssert(0);
	return false;
}

GKey::GKey(int vkey, int flags)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GViewPrivate::GViewPrivate()
{
	Parent = 0;
	ParentI = 0;
	Notify = 0;
	CtrlId = -1;
	DropTarget = 0;
	Font = 0;
	FontOwn = false;
	Popup = 0;
	TabStop = 0;
	Pulse = 0;
	InPaint = false;
}

GViewPrivate::~GViewPrivate()
{
	LgiAssert(Pulse == 0);
}

void GView::_Focus(bool f)
{
	ThreadCheck();
	
	if (f)
		SetFlag(WndFlags, GWF_FOCUS);
	else
		ClearFlag(WndFlags, GWF_FOCUS);

	OnFocus(f);	
	Invalidate();

	if (_View)
		gtk_widget_grab_focus(_View);
}

void GView::_Delete()
{
	ThreadCheck();
	SetPulse();

	// Remove static references to myself
	if (_Over == this)
		_Over = 0;
	if (_Capturing == this)
		_Capturing = 0;
	
	GWindow *Wnd = GetWindow();
	if (Wnd && Wnd->GetFocus() == static_cast<GViewI*>(this))
		Wnd->SetFocus(this, GWindow::ViewDelete);

	if (LgiApp && LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	// Misc
	Pos.ZOff(-1, -1);

	// Heirarchy
	GViewI *c;
	while (c = Children.First())
	{
		if (c->GetParent() != (GViewI*)this)
		{
			printf("%s:%i - ~GView error, %s not attached correctly: %p(%s) Parent: %p(%s)\n",
				_FL, c->GetClass(), c, c->Name(), c->GetParent(), c->GetParent() ? c->GetParent()->Name() : "");
			Children.Delete(c);
		}

		DeleteObj(c);
	}

	// Clean up OS view
	if (_View)
	{
	    #ifdef LINUX
		LgiApp->UnregisterHandle(this);
		#endif

		gtk_widget_destroy(_View);
		_View = 0;
	}

	Detach();
}


GView *&GView::PopupChild()
{
	return d->Popup;
}

void LgiToGtkCursor(GViewI *v, LgiCursor c)
{
	static LgiCursor CurrentCursor = LCUR_Normal;

	if (!v || c == CurrentCursor)
		return;
	
	CurrentCursor = c;
	GdkCursorType type = GDK_ARROW;
	switch (c)
	{
		// No cursor
		#ifdef GDK_BLANK_CURSOR
		case LCUR_Blank:
			type = GDK_BLANK_CURSOR;
			break;
		#endif
		/// Normal arrow
		case LCUR_Normal:
			type = GDK_ARROW;
			break;
		/// Upwards arrow
		case LCUR_UpArrow:
			type = GDK_SB_UP_ARROW;
			break;
		/// Downwards arrow
		case LCUR_DownArrow:
			type = GDK_SB_DOWN_ARROW;
			break;
		/// Left arrow
		case LCUR_LeftArrow:
			type = GDK_SB_LEFT_ARROW;
			break;
		/// Right arrow
		case LCUR_RightArrow:
			type = GDK_SB_RIGHT_ARROW;
			break;
		/// Crosshair
		case LCUR_Cross:
			type = GDK_CROSSHAIR;
			break;
		/// Hourglass/watch
		case LCUR_Wait:
			type = GDK_WATCH;
			break;
		/// Ibeam/text entry
		case LCUR_Ibeam:
			type = GDK_XTERM;
			break;
		/// Vertical resize (|)
		case LCUR_SizeVer:
			type = GDK_DOUBLE_ARROW;
			break;
		/// Horizontal resize (-)
		case LCUR_SizeHor:
			type = GDK_SB_H_DOUBLE_ARROW;
			break;
		/// Diagonal resize (/)
		case LCUR_SizeBDiag:
			type = GDK_BOTTOM_LEFT_CORNER;
			break;
		/// Diagonal resize (\)
		case LCUR_SizeFDiag:
			type = GDK_BOTTOM_RIGHT_CORNER;
			break;
		/// All directions resize
		case LCUR_SizeAll:
			type = GDK_FLEUR;
			break;
		/// A pointing hand
		case LCUR_PointingHand:
			type = GDK_HAND2;
			break;
		/// A slashed circle
		case LCUR_Forbidden:
			type = GDK_X_CURSOR;
			break;

		default:
			type = GDK_ARROW;
			break;

		/*			
		case LCUR_SplitV:
		case LCUR_SplitH:
		case LCUR_DropCopy:
		case LCUR_DropMove:
			LgiAssert(0);
			break;
		*/
	}
	
	GWindow *Wnd = v->GetWindow();
	OsView h = Wnd ? Wnd->Handle() : v->Handle();
	
	LgiAssert(v->InThread());
	LgiAssert(h->window);
	if (type == GDK_ARROW)
	{
		gdk_window_set_cursor(h->window, NULL);
		// printf("gdk_window_set_cursor(%s, NULL)\n", v->GetClass());
	}
	else
	{
		GdkCursor *cursor = gdk_cursor_new_for_display(gdk_display_get_default(), type);
		if (cursor)
		{
			gdk_window_set_cursor(h->window, cursor);
			// printf("gdk_window_set_cursor(%s, cursor)\n", v->GetClass());
		}
		else
		{
			gdk_window_set_cursor(h->window, NULL);
			// printf("gdk_window_set_cursor(%s, gdk_cursor_new_for_display fail)\n", v->GetClass());
		}
	}
}

bool GView::_Mouse(GMouse &m, bool Move)
{
	ThreadCheck();
	
	#if 0
	if (!Move)
	{
		m.Trace("_Mouse");
		::GArray<GViewI*> _m;
		for (GViewI *i=this; i; i=i->GetParent())
		{
			_m.Add(i);
		}
		for (int n=0; n<_m.Length(); n++)
		{
			GViewI *i=_m[_m.Length()-1-n];
			char s[256];
			ZeroObj(s);
			memset(s, ' ', (n+1)*2);
			LgiTrace("%s%s %s\n", s, i->GetClass(), i->GetPos().GetStr());
		}
	}
	#endif

	if
	(
		!_View
		||
		(
			GetWindow()
			&&
			!GetWindow()->HandleViewMouse(this, m)
		)
	)
	{
		return false;
	}

	#if 0
	{
		char msg[256];
		sprintf(msg, "_mouse cap=%p move=%i", _Capturing, Move);
		m.Trace(msg);
	}
	#endif

	GViewI *cap = _Capturing;
	if (_Capturing)
	{
		if (Move)
		{
			GMouse Local = lgi_adjust_click(m, _Capturing);
			LgiToGtkCursor(_Capturing, _Capturing->GetCursor(Local.x, Local.y));
			_Capturing->OnMouseMove(Local); // This can set _Capturing to NULL
		}
		else
		{
			_Capturing->OnMouseClick(lgi_adjust_click(m, _Capturing));
		}
	}
	else
	{
		if (Move)
		{
			bool Change = false;
			GViewI *o = WindowFromPoint(m.x, m.y);
			if (_Over != o)
			{
				if (_Over)
					_Over->OnMouseExit(lgi_adjust_click(m, _Over));
				_Over = o;
				if (_Over)
					_Over->OnMouseEnter(lgi_adjust_click(m, _Over));
			}
		}
			
		GView *Target = dynamic_cast<GView*>(_Over ? _Over : this);

		GRect Client = Target->GView::GetClient(false);
		
		m = lgi_adjust_click(m, Target, !Move);
		if (!Client.Valid() || Client.Overlap(m.x, m.y))
		{
			LgiToGtkCursor(Target, Target->GetCursor(m.x, m.y));

			if (Move)
			{
				Target->OnMouseMove(m);
			}
			else
			{
				#if 0
				if (!Move)
				{
					char Msg[256];
					sprintf(Msg, "_Mouse Target %s", Target->GetClass());
					m.Trace(Msg);
				}
				#endif
				Target->OnMouseClick(m);
			}
		}
		else if (!Move)
		{
			printf("%s:%i - Click outside %s %s %i,%i\n", _FL, Target->GetClass(), Client.GetStr(), m.x, m.y);
		}
	}
	
	return true;
}

gboolean GtkViewCallback(GtkWidget *widget, GdkEvent *event, GView *This)
{
	#if 0
	printf("GtkViewCallback, widget=%p, event=%p, event=%x, This=%p(%s\"%s\")\n",
		widget, event,
		event->type, This, (NativeInt)This > 0x1000 ? This->GetClass() : 0, (NativeInt)This > 0x1000 ? This->Name() : 0);
	#endif
	
	if (event->type < 0 || event->type > 1000)
		return false;

	return This->OnGtkEvent(widget, event);
}

gboolean GView::OnGtkEvent(GtkWidget *widget, GdkEvent *event)
{
	printf("GView::OnGtkEvent ?????\n");
	return false;
}

GRect &GView::GetClient(bool ClientSpace)
{
	int Edge = (Sunken() || Raised()) ? _BorderSize : 0;

	static GRect c;
	if (ClientSpace)
	{
		c.ZOff(Pos.X() - 1 - (Edge<<1), Pos.Y() - 1 - (Edge<<1));
	}
	else
	{
		c.ZOff(Pos.X()-1, Pos.Y()-1);
		c.Size(Edge, Edge);
	}
	return c;
}

void GView::Quit(bool DontDelete)
{
	ThreadCheck();
	
	if (DontDelete)
	{
		Visible(false);
	}
	else
	{
		delete this;
	}
}

bool GView::SetPos(GRect &p, bool Repaint)
{
	ThreadCheck();
	
	/*
	if (p == Pos)
		return true;
	*/

	Pos = p;

	if (_View)
	{
		int o = 0;
		
		GView *Par = d->GetParent();
		if (Par && (Par->Sunken() || Par->Raised()))
		{
			o = Par->_BorderSize;
		}
		
		GtkWidget *GtkPar;
		if (GTK_IS_WINDOW(_View))
		{
			gtk_window_move(GTK_WINDOW(_View), Pos.x1, Pos.y1);
			gtk_window_resize(GTK_WINDOW(_View), Pos.X(), Pos.Y());
		}
		else if (GtkPar = gtk_widget_get_parent(_View))
		{
			if (LGI_IS_WIDGET(GtkPar))
			{
				lgi_widget_setsize(_View, Pos.X(), Pos.Y());
				lgi_widget_setchildpos(	GtkPar,
										_View,
										Pos.x1 + o,
										Pos.y1 + o);
			}
			else
			{
				LgiTrace("%s:%i - Error: Can't set object position, parent is: %s\n", _FL, G_OBJECT_TYPE_NAME(GtkPar));
			}
		}
	}

	OnPosChange();
	return true;
}

bool GView::Invalidate(GRect *r, bool Repaint, bool Frame)
{
	if (IsAttached())
	{
		if (InThread() && !d->InPaint)
		{
			GRect Client;
			if (Frame)
				Client.ZOff(Pos.X()-1, Pos.Y()-1);
			else
				Client = GView::GetClient(false);

			static bool Repainting = false;
			
			if (!Repainting)
			{
				Repainting = true;
				GdkWindow *hnd = gtk_widget_get_window(_View);
				if (hnd)
				{
					if (r)
					{
						GRect cr = *r;
						cr.Offset(Client.x1, Client.y1);
						Gtk::GdkRectangle r = cr;
	            		gdk_window_invalidate_rect(hnd, &r, FALSE);
					}
					else
					{
						Gtk::GdkRectangle r = {0, 0, Pos.X(), Pos.Y()};
	            		gdk_window_invalidate_rect(hnd, &r, FALSE);
					}
				}
				Repainting = false;
			}
		}
		else
		{
			X11_INVALIDATE_PARAMS *p = new X11_INVALIDATE_PARAMS;
			if (p)
			{
				if (r)
				{
					p->Rgn = *r;
				}
				else
				{
					p->Rgn.ZOff(-1, -1);
				}
				p->Repaint = Repaint;
				p->View = this;
				
				PostEvent(M_X11_INVALIDATE, (GMessage::Param)p, 0);
			}
		}
		
		return true;
	}
	else
	{
		GRect Up;
		GViewI *p = this;

		if (r)
		{
			Up = *r;
		}
		else
		{
			Up.Set(0, 0, Pos.X()-1, Pos.Y()-1);
		}

		while (p && !p->IsAttached())
		{
			GRect pos = p->GetPos();
			Up.Offset(pos.x1, pos.y1);
			p = p->GetParent();
		}

		if (p && p->IsAttached())
		{
			return p->Invalidate(&Up, Repaint);
		}
	}

	return false;
}

void GView::SetPulse(int Length)
{
	ThreadCheck();
	
	if (d->Pulse)
	{
		d->Pulse->Delete();
		d->Pulse = 0;
	}
	
	if (Length > 0)
	{
		d->Pulse = new GPulseThread(this, Length);
	}
}

GMessage::Param GView::OnEvent(GMessage *Msg)
{
	ThreadCheck();
	
	int Id;
	switch (Id = Msg->Msg())
	{
		#if 0 // def M_GTHREADWORK_COMPELTE
		case M_GTHREADWORK_COMPELTE:
		{
			GThreadOwner *Owner = (GThreadOwner*) MsgA(Msg);
			GThreadWork *WorkUnit = (GThreadWork*) MsgB(Msg);
			Owner->OnComplete(WorkUnit);
			DeleteObj(WorkUnit);
			break;
		}
		#endif
		case M_X11_INVALIDATE:
		{
			X11_INVALIDATE_PARAMS *p = (X11_INVALIDATE_PARAMS*)MsgA(Msg);
			if (p && p->View == this)
			{
				Invalidate(p->Rgn.Valid() ? &p->Rgn : 0, p->Repaint);
				DeleteObj(p);
			}
			break;
		}
		case M_PULSE:
		{
			OnPulse();
			break;
		}
		case M_CHANGE:
		{
			GViewI *Ctrl;
			if (GetViewById(MsgA(Msg), Ctrl))
			{
				return OnNotify(Ctrl, MsgB(Msg));
			}
			break;
		}
		case M_COMMAND:
		{
			int32 a=MsgA(Msg);
			return OnCommand(a&0xFFFF, a>>16, (OsView) MsgB(Msg));
		}
		default:
		{
			int asd=0;
			break;
		}
	}

	return 0;
}

void GView::PointToScreen(GdcPt2 &p)
{
	ThreadCheck();
	
	GViewI *c = this;

	// Find real parent
	while (c->GetParent() && !dynamic_cast<GWindow*>(c))
	{
		GRect n = c->GetPos();
		p.x += n.x1;
		p.y += n.y1;
		c = c->GetParent();
	}
	
	if (c && c->WindowHandle())
	{
	    gint x = 0, y = 0;
		gdk_window_get_origin(GTK_WIDGET(c->WindowHandle())->window, &x, &y);
		p.x += x;
		p.y += y;
	}
	else
	{
		printf("%s:%i - No real view to map to screen.\n", _FL);
	}
}

void GView::PointToView(GdcPt2 &p)
{
	ThreadCheck();
	
	if (_View)
	{
		gint x = 0, y = 0;
		gdk_window_get_origin(GetWindow()->Handle()->window, &x, &y);
		p.x -= x;
		p.y -= y;
		
		GViewI *w = GetWindow();
		for (GViewI *i = this; i && i != w; i = i->GetParent())
		{
			GRect pos = i->GetPos();
			const char *cls = i->GetClass();
			p.x -= pos.x1;
			p.y -= pos.y1;
		}
		
		GRect cli = GetClient(false);
		p.x -= cli.x1;
		p.y -= cli.y1;
	}
	else if (GetParent())
	{
		// Virtual window
		int Sx = 0, Sy = 0;
		GViewI *v;
		// Work out the virtual offset
		for (v = this; v && !v->Handle(); v = v->GetParent())
		{
			Sx += v->GetPos().x1;
			Sy += v->GetPos().y1;
		}
		if (v && v->Handle())
		{
			// Get the point relative to the first real parent
			v->PointToView(p);
			
			// Move point back into virtual space
			p.x -= Sx;
			p.y -= Sy;
		}
		else LgiTrace("%s:%i - No Real view for %s\n", _FL, GetClass());
	}
}

bool GView::GetMouse(GMouse &m, bool ScreenCoords)
{
	ThreadCheck();
	
	if (_View)
	{
		gint x = 0, y = 0;
		GdkModifierType mask;
		GdkScreen *wnd_scr = gtk_window_get_screen(GTK_WINDOW(WindowHandle()));
		GdkDisplay *wnd_dsp = wnd_scr ? gdk_screen_get_display(wnd_scr) : NULL;
		gdk_display_get_pointer(wnd_dsp,
								&wnd_scr,
								&x, &y,
								&mask);
		if (!ScreenCoords)
		{
			GdcPt2 p(x, y);
			PointToView(p);
			m.x = p.x;
			m.y = p.y;
		}
		else
		{
			m.x = x;
			m.y = y;
		}
		m.SetModifer(mask);
		
		return true;
	}
	else if (GetParent())
	{
		bool s = GetParent()->GetMouse(m, ScreenCoords);
		if (s)
		{
			if (!ScreenCoords)
			{
				m.x -= Pos.x1;
				m.y -= Pos.y1;
			}
			return true;
		}
	}
	
	return false;
}

bool GView::IsAttached()
{
	return	_View && _View->parent;
}

bool GView::Attach(GViewI *parent)
{
	ThreadCheck();
	
	bool Status = false;

	SetParent(parent);
	GView *Parent = d->GetParent();
	_Window = Parent ? Parent->_Window : this;
	
	if (!_View)
	{
		_View = lgi_widget_new(this, Pos.X(), Pos.Y(), false);
	}
	
	if (_View)
	{
		int o = 0;
		{
			GView *Par = d->GetParent();
			if (Par && (Par->Sunken() || Par->Raised()))
			{
				o = Par->_BorderSize;
			}
		}
		
		if (parent)
		{
			GtkWidget *p = parent->Handle();

			GWindow *w;
			if (w = dynamic_cast<GWindow*>(parent))
				p = w->_Root;
			
			if (p && gtk_widget_get_parent(_View) != p)
			{
				lgi_widget_add(GTK_CONTAINER(p), _View);
				lgi_widget_setchildpos(p, _View, Pos.x1 + o, Pos.y1 + o);
			}
		}

		if (Visible())
		{
			gtk_widget_show(_View);
		}
		
		if (DropTarget())
		{
			DropTarget(true);
		}
		
		if (TestFlag(WndFlags, GWF_FOCUS))
		{
			// LgiTrace("OnCreate Focus %s\n", GetClass());
			gtk_widget_grab_focus(_View);
		}

		OnCreate();
		OnAttach();
		Status = true;
	}

	if (d->Parent && !d->Parent->HasView(this))
	{
		if (!d->Parent->HasView(this))
			d->Parent->AddView(this);
		d->Parent->OnChildrenChanged(this, true);
	}
	
	return Status;
}

bool GView::Detach()
{
	ThreadCheck();
	
	// Detach view
	GViewI *Par = GetParent();
	if (Par)
	{
		// Events
		Par->DelView(this);
		Par->OnChildrenChanged(this, false);
	}
	
	d->Parent = 0;
	d->ParentI = 0;

	{
		int Count = Children.Length();
		if (Count)
		{
			int Detached = 0;
			GViewI *c, *prev = NULL;

			while (c = Children.First())
			{
				LgiAssert(!prev || c != prev);
				if (c->GetParent())
					c->Detach();
				else
					Children.Delete(c);
				Detached++;
				prev = c;
			}
			LgiAssert(Count == Detached);
		}
	}

	if (_View)
	{
	    #ifdef LINUX
		LgiApp->UnregisterHandle(this);
		#endif
		LgiAssert(_View->object.parent_instance.g_type_instance.g_class);
		gtk_widget_destroy(_View);
		_View = 0;
	}

	return true;
}

GViewI *GView::FindControl(OsView hCtrl)
{
	ThreadCheck();
	
	if (Handle() == hCtrl)
	{
		return this;
	}

	List<GViewI>::I it = Children.Start();
	for (GViewI *c = *it; c; c = *++it)
	{
		GViewI *Ctrl = c->FindControl(hCtrl);
		if (Ctrl)
		{
			return Ctrl;
		}
	}
	return 0;
}

LgiCursor GView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

///////////////////////////////////////////////////////////////////
bool LgiIsMounted(char *Name)
{
	return false;
}

bool LgiMountVolume(char *Name)
{
	return false;
}

////////////////////////////////
uint32 CursorData[] = {
0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x01010101, 0x02020202, 0x02020202, 0x02010101, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x01020202, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x02020202, 0x02020202, 0x02020101, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x01000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x01020202, 0x02020201, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x00010102, 0x00000000, 0x02020101, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010001, 0x00010001, 0x01000001, 0x02020202, 0x02020202, 0x00010102, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 
0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01000102, 0x00000100, 0x02010000, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020100, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 
0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00010101, 0x01000000, 0x02020202, 0x00000001, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00000102, 0x01010100, 0x01010101, 0x01000000, 0x02020202, 0x02020201, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 
0x00000001, 0x02010000, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x01010102, 0x01010001, 0x02020101, 0x02020202, 0x02020202, 0x01020201, 0x02010000, 0x02020102, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x01000001, 0x02020101, 0x02020202, 0x02020202, 0x00010202, 0x01010000, 0x01020202, 0x00000001, 0x02020201, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01000001, 0x01010101, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 
0x00000000, 0x00000000, 0x02020201, 0x02020101, 0x00000102, 0x00000100, 0x02020201, 0x02020202, 0x01000001, 0x01000000, 0x01020202, 0x02020201, 0x02020202, 0x02020202, 0x02020201, 0x02010001, 0x02010202, 0x02020202, 0x01020202, 0x01020201, 0x02010000, 0x02010102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x01010000, 0x02020101, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x00000102, 0x02020100, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x02010001, 0x00000001, 0x00010201, 0x02020201, 0x02020202, 0x02010001, 0x00000001, 0x00010201, 0x02020201, 0x02020202, 0x01020202, 0x02020201, 0x02010001, 0x01010202, 0x02020202, 0x00010202, 0x01020201, 0x02010000, 0x01000102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02010102, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x00000102, 0x00000001, 0x02020201, 0x00010202, 0x02020100, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 
0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01010100, 0x01010101, 0x01000000, 0x02020202, 0x01000001, 0x01000000, 0x01020202, 0x02020201, 0x02020202, 0x02020101, 0x00000102, 0x00000100, 0x02020201, 0x02020202, 0x00010202, 0x02020201, 0x02010001, 0x00010202, 0x02020201, 0x00000102, 0x01010101, 0x01010000, 0x00000101, 0x02020201, 0x01010101, 0x01010101, 0x01010100, 0x01010101, 0x02020201, 0x01000001, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x00000001, 0x00000101, 0x02020100, 0x00010202, 0x02010000, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 
0x02020202, 0x02020202, 0x01010102, 0x01010101, 0x01010001, 0x01010101, 0x02020101, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020100, 0x01020202, 0x02010000, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020201, 0x02020202, 0x02020201, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x01010101, 0x01010001, 0x00010101, 0x02020100, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020100, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x00000001, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x00010202, 0x02010000, 0x01020202, 0x02010000, 0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 
0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x01020202, 0x02020100, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02010000, 0x00000102, 0x01010101, 0x01010000, 0x00000101, 0x02020201, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x00000102, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x01020202, 
0x01000000, 0x01020202, 0x02010000, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x01010102, 0x01010101, 0x01010001, 0x01010101, 0x02020101, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x01020202, 0x02020201, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x01010101, 0x01010001, 0x00010101, 0x02020100, 0x00010202, 0x01020201, 0x02010000, 0x01000102, 0x02020202, 0x01010101, 0x01010101, 0x01010100, 0x01010101, 
0x02020201, 0x00010202, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x00000001, 0x01020201, 0x02010000, 0x00000001, 0x01000000, 0x02010101, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02010101, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x01000001, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01000001, 0x01010101, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x01020202, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00010202, 0x02020201, 0x02010001, 0x00010202, 0x02020201, 0x01020202, 
0x01020201, 0x02010000, 0x02010102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x00000001, 0x02020201, 0x00000102, 0x00010100, 0x02010000, 0x00000001, 0x01000001, 0x01020202, 0x01010101, 0x01010101, 0x00000001, 0x01000001, 0x01020202, 0x01010101, 0x01010101, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01000102, 0x01000001, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 
0x02020201, 0x02020202, 0x01020202, 0x02020201, 0x02010001, 0x01010202, 0x02020202, 0x02020202, 0x01020201, 0x02010000, 0x02020102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x00000102, 0x02020201, 0x00010202, 0x00010000, 0x02020100, 0x01000001, 0x01000001, 0x01020202, 0x00000000, 0x01000000, 0x01000001, 0x01000001, 0x01020202, 0x00000000, 0x01000000, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010001, 0x00000000, 0x01000100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02010001, 0x02010202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x02020100, 0x01020202, 0x00000000, 0x02020100, 0x02010001, 0x00000102, 0x01020201, 0x01010000, 0x01000001, 0x02010001, 0x00000102, 0x01020201, 0x01010100, 0x01000001, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 
0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01010102, 0x01010001, 0x02020101, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01010202, 0x01010101, 0x02020202, 0x02020202, 0x00010202, 0x01010000, 0x01020202, 0x00000001, 0x02020201, 0x02020101, 0x00000102, 0x01020201, 0x00000100, 0x01000100, 0x02020101, 0x00000102, 0x01020201, 0x01000100, 0x01000100, 0x01020202, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x01010101, 0x02020202, 
0x02020202, 0x00010102, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00010101, 0x01000000, 0x02020202, 0x02020202, 0x00010202, 0x01020100, 0x00000100, 0x01000000, 0x02020202, 0x00010202, 0x01020100, 0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00010202, 0x01020100, 0x00000100, 0x01000100, 0x02020202, 0x00010202, 0x01020100, 
0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010101, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010102, 0x00000000, 0x02020101, 0x02020202, 
0x02020202, 0x01020202, 0x01020201, 0x01010000, 0x01000001, 0x02020202, 0x01020202, 0x01020201, 0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x01010101, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x01010101, 
};

