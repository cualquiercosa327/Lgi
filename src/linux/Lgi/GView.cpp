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

////////////////////////////////////////////////////////////////////////////
bool LgiIsKeyDown(int Key)
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GViewPrivate::CursorSet = false;
GView *GViewPrivate::LastCursor = 0;

GViewPrivate::GViewPrivate()
{
    #if defined LINUX
	MapState = GView::Unmapped;
	#endif
	CursorId = 0;
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
	
	#if defined XWIN
	// XCursor = 0;
	#endif
}

GViewPrivate::~GViewPrivate()
{
	LgiAssert(Pulse == 0);
}

void GView::_Delete()
{
	// Remove static references to myself
	if (_Over == this) _Over = 0;
	if (_Capturing == this) _Capturing = 0;

	if (LgiApp AND LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	// Clean up OS view
	if (_View)
	{
	    #ifdef LINUX
		LgiApp->UnregisterHandle(this);
		#endif

		printf("destroy %p parent=%p, iscon=%p\n", _View, gtk_widget_get_parent(_View), GTK_IS_CONTAINER(gtk_widget_get_parent(_View)));
		gtk_widget_destroy(_View);
		_View = 0;
	}
	
	// Misc
	SetPulse();
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

	Detach();
}


GView *&GView::PopupChild()
{
	return d->Popup;
}

bool GView::_Mouse(GMouse &m, bool Move)
{
	#if 0
	if (!Move)
	{
		m.Trace("_Mouse");
		GArray<GViewI*> _m;
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
			printf("%s%s %s\n", s, i->GetClass(), i->GetPos().GetStr());
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

	if (0)
	{
		char msg[256];
		sprintf(msg, "_mouse cap=%p move=%i", _Capturing, Move);
		m.Trace(msg);
	}

	if (_Capturing)
	{
		if (Move)
		{
			GViewI *c = _Capturing;
			d->CursorSet = false;
			c->OnMouseMove(lgi_adjust_click(m, c));
			
			if (!d->CursorSet)
			{
				c->SetCursor(LCUR_Normal);
			}
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
			/*
			if (d->LastCursor AND
				d->LastCursor != this)
			{
				printf("XUndefineCursor on old view.\n");
				XUndefineCursor(Handle()->XDisplay(), d->LastCursor->Handle()->handle());
				d->LastCursor = 0;
			}
			*/
			
			bool Change = false;
			GViewI *o = WindowFromPoint(m.x, m.y);
			if (_Over != o)
			{
				if (_Over)
				{
					_Over->OnMouseExit(lgi_adjust_click(m, _Over));
				}

				// printf("Over change %s -> %s\n", _Over?_Over->GetClass():0, o?o->GetClass():0);
				_Over = o;

				if (_Over)
				{
					_Over->OnMouseEnter(lgi_adjust_click(m, _Over));
				}
			}
		}
			
		GView *Target = dynamic_cast<GView*>(_Over ? _Over : this);

		GRect Client = Target->GView::GetClient(false);
		/*
		if (Target->Raised() || Target->Sunken())
		{
			Client.Offset(Target->_BorderSize, Target->_BorderSize);
		}
		*/
		
		m = lgi_adjust_click(m, Target, !Move);
		if (!Client.Valid() || Client.Overlap(m.x, m.y))
		{
			if (Move)
			{
				d->CursorSet = false;
				Target->OnMouseMove(m);
				if (!d->CursorSet)
				{
					Target->SetCursor(LCUR_Normal);
				}
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

/*
void GView::OnNcPaint(GSurface *pDC, GRect &r)
{
	int Border = Sunken() OR Raised() ? _BorderSize : 0;
	if (Border == 2)
	{
		LgiWideBorder(pDC, r, Sunken() ? SUNKEN : RAISED);
	}
	else if (Border == 1)
	{
		LgiThinBorder(pDC, r, Sunken() ? SUNKEN : RAISED);
	}
	// r.Offset(Border, Border);
}
*/

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
	if (DontDelete)
	{
		Visible(false);
	}
	else
	{
		Detach();
		#ifdef LINUX
		LgiApp->DeleteMeLater(this);
		#endif
	}
}

bool GView::SetCursor(int CursorId)
{
	d->CursorSet = true;
	GView *Wnd = GetWindow();
	
	if (Wnd AND
		Wnd->Handle() AND
		Wnd->d->CursorId != CursorId)
	{
		/*
		Display *Dsp = Wnd->Handle()->XDisplay();

		int Items = CountOf(CursorMetrics);
		if (CursorId > 0 AND CursorId <= Items)
		{
			if (Wnd->d->XCursor)
			{
				XUndefineCursor(Dsp, Wnd->Handle()->handle());
				XFreeCursor(Dsp, Wnd->d->XCursor);
				Wnd->d->XCursor = 0;
			}

			GSurface *Mem = Cursors.Create();
			if (Mem)
			{
				CursorInfo *Metrics = CursorMetrics + CursorId - 1;
				Ximg Src(Metrics->Pos.X(), Metrics->Pos.Y(), 1);
				Ximg Mask(Metrics->Pos.X(), Metrics->Pos.Y(), 1);
				
				for (int y=0; y<Src.Y(); y++)
				{
					for (int x=0; x<Src.X(); x++)
					{
						COLOUR c = Mem->Get(Metrics->Pos.x1 + x, Metrics->Pos.y1 + y);
						Src.Set(x, y, c == 1 ? 1 : 0);
						Mask.Set(x, y, c < 2);
					}
				}
				
				Xpix PSrc(Handle()->handle(), &Src);
				Xpix PMask(Handle()->handle(), &Mask);
				
				XColor Fore, Back;
				ZeroObj(Fore);
				ZeroObj(Back);
				Fore.red = Fore.green = Fore.blue = 0xffff;
				Back.red = Back.green = Back.blue = 0;
				Wnd->d->XCursor = XCreatePixmapCursor
				(
					Dsp,
					PSrc,
					PMask,
					&Fore,
					&Back,
					Metrics->HotSpot.x - Metrics->Pos.x1,
					Metrics->HotSpot.y - Metrics->Pos.y1
				);
				
				if (Wnd->d->XCursor)
				{
					XDefineCursor(Dsp, Wnd->Handle()->handle(), Wnd->d->XCursor);
				}
				
				DeleteObj(Mem);
			}
			else return false;
		}
		else
		{
			if (Wnd->d->XCursor)
			{
				XDefineCursor(Dsp, Wnd->Handle()->handle(), 0);
				XFlush(Dsp);
				
				XFreeCursor(Dsp, Wnd->d->XCursor);
				Wnd->d->XCursor = 0;
			}			
		}
		
		Wnd->d->CursorId = CursorId;
		Wnd->d->LastCursor = this;
		*/
		
		return true;
	}
	else
	{
		// printf("%s:%i - Param error"\n", __FILE__, __LINE__);
	}
	
	return false;
}

bool GView::SetPos(GRect &p, bool Repaint)
{
	if (p == Pos)
		return true;

	Pos = p;

	if (_View)
	{
		int o = 0;
		
		GView *Par = d->GetParent();
		if (Par AND (Par->Sunken() OR Par->Raised()))
		{
			o = Par->_BorderSize;
		}
		
		// gtk_widget_set_size_request(_View, Pos.X(), Pos.Y());
		if (gtk_widget_get_parent(_View))
		{   
		    lgi_widget_setsize(_View, Pos.X(), Pos.Y());
			lgi_widget_setchildpos(	gtk_widget_get_parent(_View),
									_View,
									Pos.x1 + o,
									Pos.y1 + o);
		}
	}

	OnPosChange();
	return true;
}

bool GView::Invalidate(GRect *r, bool Repaint, bool Frame)
{
	if (IsAttached())
	{
		if (InThread())
		{
			GRect Client = Frame ? GView::GetPos() : GView::GetClient(false);

			static bool Repainting = false;
			
			if (!Repainting)
			{
				Repainting = true;
				if (r)
				{
					GRect cr = *r;
					cr.Offset(Client.x1, Client.y1);
					Gtk::GdkRectangle r = cr;
            		gdk_window_invalidate_rect(gtk_widget_get_window(_View), &r, FALSE);
				}
				else
				{
					Gtk::GdkRectangle r = {0, 0, Pos.X(), Pos.Y()};
            		gdk_window_invalidate_rect(gtk_widget_get_window(_View), &r, FALSE);
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
				
				PostEvent(M_X11_INVALIDATE, (int)p, 0);
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
			Up.Offset(p->GetPos().x1, p->GetPos().y1);
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

int GView::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
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
			if (p AND p->View == this)
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
			GViewI *Ctrl = dynamic_cast<GViewI*>((GViewI*) MsgA(Msg));
			if (Ctrl)
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
			break;
		}
	}

	return 0;
}

void GView::PointToScreen(GdcPt2 &p)
{
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
	if (_View)
	{
	    gint x, y;
	    if (gtk_widget_translate_coordinates(   _View,
	                                            GTK_WIDGET(GetWindow()->Handle()),
	                                            p.x, p.y,
	                                            &x, &y))
	    {
			p.x += x;
			p.y += y;
			
			gtk_window_get_position (WindowHandle(), &x, &y);

			p.x += x;
			p.y += y;
		}
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
		else
		{
			printf("%s:%i - No Real view.\n", __FILE__, __LINE__);
		}
	}
}

bool GView::GetMouse(GMouse &m, bool ScreenCoords)
{
	if (_View || !GetParent())
	{
		#if defined __GTK_H__
		
		/*
		xcb_query_pointer_reply_t *r;
		xcb_query_pointer_cookie_t c = xcb_query_pointer(XcbConn(), _View ? _View : XcbScreen()->root);
		if (r = xcb_query_pointer_reply(XcbConn(), c, 0))
		{
			if (ScreenCoords)
			{
				m.x = r->root_x;
				m.y = r->root_y;
			}
			else
			{
				m.x = r->win_x;
				m.y = r->win_y;
			}
			
			m.Down(	TestFlag(r->mask, XCB_BUTTON_MASK_1) ||
					TestFlag(r->mask, XCB_BUTTON_MASK_2) ||
					TestFlag(r->mask, XCB_BUTTON_MASK_3));
			m.Left(TestFlag(r->mask, XCB_BUTTON_MASK_1));
			m.Middle(TestFlag(r->mask, XCB_BUTTON_MASK_2));
			m.Right(TestFlag(r->mask, XCB_BUTTON_MASK_3));
			
			free(r);
			return true;
		}
		// else printf("%s:%i - xcb_query_pointer(%x) failed\n", _FL, _View);
		*/
		
		#endif
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
	return	_View && _View->window;
}

bool GView::Attach(GViewI *parent)
{
	bool Status = false;

	SetParent(parent);
	if (!_View)
	{
		_View = lgi_widget_new(this, Pos.X(), Pos.Y(), false);
	}
	
	if (_View)
	{
		int o = 0;
		{
			GView *Par = d->GetParent();
			if (Par AND (Par->Sunken() OR Par->Raised()))
			{
				o = Par->_BorderSize;
			}
		}
		
		if (parent)
		{
			GtkWidget *p = parent->Handle();

			GWindow *w;
			if (w = dynamic_cast<GWindow*>(parent))
				p = w->Root;
			
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
		
		if (Focus())
		{
			ClearFlag(WndFlags, GWF_FOCUS);
			Focus(true);
			// printf("%s:%i - Setting initial focus to %s\n", _FL, GetClass());
		}

		OnCreate();
		Status = true;
	}

	if (d->Parent && !d->Parent->HasView(this))
	{
		d->Parent->AddView(this);
		d->Parent->OnChildrenChanged(this, true);
	}
	
	return Status;
}

bool GView::Detach()
{
	bool Status = false;

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

	if (_View)
	{
	    #ifdef LINUX
		LgiApp->UnregisterHandle(this);
		#endif
		gtk_widget_destroy(_View);
		_View = 0;
	}

	Status = true;

	return Status;
}

GViewI *GView::FindControl(OsView hCtrl)
{
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

void GView::SetIgnoreInput(bool ignore)
{
}

GView::MappingState GView::GetMapState()
{
	return d->MapState;
}

void GView::OnMap(bool m)
{
	d->MapState = m ? Mapped : Unmapped;
	if (m)
		OnAttach();
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

