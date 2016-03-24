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
#include "GScrollBar.h"
#include "GEdit.h"
#include "GViewPriv.h"
#include "GUtf8.h"
#include "GCss.h"

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

struct LgiCursorInfo
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

////////////////////////////////////////////////////////////////////////////
GView *GWindowFromHandle(OsView h)
{
	if (!h)
		return 0;
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////
void _lgi_yield()
{
	LgiApp->Run(false);
}

bool LgiIsKeyDown(int Key)
{
	LgiAssert(!"Not impl.");
	return false;
}

bool LgiIsMounted(char *Name)
{
	LgiAssert(!"Not impl.");
	return false;
}

bool LgiMountVolume(char *Name)
{
	LgiAssert(!"Not impl.");
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GViewPrivate::CursorSet = false;
GView *GViewPrivate::LastCursor = 0;

GViewPrivate::GViewPrivate()
{
	TabStop = false;
	CursorId = 0;
	Parent = 0;
	ParentI = 0;
	Notify = 0;
	CtrlId = -1;
	DropTarget = 0;
	Font = 0;
	FontOwn = false;
	Popup = 0;
	Pulse = 0;
		
	static bool First = true;
	if (First)
	{
		First = false;
	}
}
	
GViewPrivate::~GViewPrivate()
{
	DeleteObj(Popup);
	LgiAssert(Pulse == 0);
}

void GView::_Delete()
{
	if (_Over == this) _Over = 0;
	if (_Capturing == this) _Capturing = 0;

	if (LgiApp && LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	SetPulse();
	Pos.ZOff(-1, -1);

	GViewI *c;
	while (c = Children.First())
	{
		if (c->GetParent() != (GViewI*)this)
		{
			printf("Error: ~GView, child not attached correctly: %p(%s) Parent: %p(%s)\n",
				c, c->Name(),
				c->GetParent(), c->GetParent() ? c->GetParent()->Name() : "");
			Children.Delete(c);
		}

		DeleteObj(c);
	}

	Detach();
	
	if (_View)
	{
		_View = 0;
	}
}

GView *&GView::PopupChild()
{
	return d->Popup;
}

GViewI *_Find(GViewI *v, OsView h)
{
	if (v->Handle() == h)
		return v;
	
	GViewI *r = 0;
	GViewIterator *i = v->IterateViews();
	if (i)
	{
		for (GViewI *c = i->First(); c && !r; c = i->Next())
		{
			r = _Find(c, h);
		}
		DeleteObj(i);
	}
	
	return r;
}

bool GView::_Mouse(GMouse &m, bool Move)
{
	#if 0
	if (!Move)
	{
		static bool First = true;
		if (First)
		{
			First = false;
			//_DumpHeirarchy(GetWindow());			
			//_Dump(0, GetWindow(), HIViewGetRoot(GetWindow()->WindowHandle()));
		}
		
		printf("_Mouse %p,%s %i,%i down=%i move=%i\n",
			this, GetClass(),
			m.x, m.y,
			m.Down(), Move);
	}
	#endif

	GWindow *Wnd = GetWindow();

	if (_Capturing)
	{
		if (_Capturing != m.Target)
		{
			m.ToScreen();
			m.Target = _Capturing;
			m.ToView();
		}
		
		if (Move)
		{
			GViewI *c = _Capturing;
			d->CursorSet = false;
			
			c->OnMouseMove(m);
			
			if (!d->CursorSet)
			{
				c->SetCursor(LCUR_Normal);
			}
		}
		else
		{
			if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<GView*>(m.Target), m))
			{
				m.Target->OnMouseClick(m);
			}
		}
	}
	else
	{
		if (_Over != this)
		{
			if (_Over)
			{
				_Over->OnMouseExit(m);
			}

			_Over = this;

			if (_Over)
			{
				_Over->OnMouseEnter(m);
			}
		}
			
		GView *Target = dynamic_cast<GView*>(_Over ? _Over : this);
		GLayout *Lo = dynamic_cast<GLayout*>(Target);
		GRect Client = Lo ? Lo->GetClient(false) : Target->GView::GetClient(false);

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
				if (!Wnd || Wnd->HandleViewMouse(Target, m))
				{
					Target->OnMouseClick(m);
				}
			}
		}
		else return false;
	}
	
	return true;
}

GRect &GView::GetClient(bool ClientSpace)
{
	int Edge = (Sunken() || Raised()) ? _BorderSize : 0;

	static GRect c;

	c = Pos;
	c.Offset(-c.x1, -c.y1);
	if (ClientSpace)
	{
		c.x2 -= Edge << 1;
		c.y2 -= Edge << 1;
	}
	else
	{
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
		delete this;
	}
}

bool GView::SetCursor(LgiCursor CursorId)
{
	GView *Wnd = GetWindow();
	Wnd->d->CursorSet = true;
	if (Wnd && Wnd->d->CursorId != CursorId)
	{
		Wnd->d->CursorId = CursorId;
		
		return true;
	}
	
	return false;
}

bool GView::SetPos(GRect &p, bool Repaint)
{
	Pos = p;

	if (_View)
	{
		int o = 0;
		
		if (d->Parent && (d->Parent->Sunken() || d->Parent->Raised()))
		{
			GView *p = dynamic_cast<GView*>(d->Parent);
			if (p)
				o = p->_BorderSize;
		}
		
	}
	else if (GetParent())
		OnPosChange();

	return true;
}

bool GView::Invalidate(GRect *r, bool Repaint, bool Frame)
{
	for (GViewI *v = this; v; v = v->GetParent())
	{
		if (!v->Visible())
			return true;
	}
	
	if (r && !r->Valid())
		return false;

	#if 1
	
	if (_View)
	{
		return true;
	}
	
	#else
	
// printf("Inval %p,%s,%p r=%s repaint=%i\n", this, GetClassName(), _View, r?r->GetStr():0, Repaint);
	if (_View || WindowHandle())
	{
		GRect Client = Frame ? GView::GetPos() : GView::GetClient();
		HIViewRef ViewRef = 0;
		if (WindowHandle())
		{
			OSStatus e = HIViewFindByID(HIViewGetRoot(WindowHandle()), kHIViewWindowContentID, &ViewRef);
			if (e)
			{
				LgiTrace("%s:%i - Getting content view failed (e=%i)\n", __FILE__, __LINE__, e);
			}
		}
		else
		{
			ViewRef = _View;
		}
		
		if (Repaint)
		{
			static bool Repainting = false;
			if (!Repainting)
			{
				Repainting = true;
				if (r)
				{
					GRect cr = *r;
					cr.Offset(Client.x1, Client.y1);
					
					RgnHandle a = NewRgn();
					SetRectRgn(a, cr.x1, cr.y1, cr.x2, cr.y2);
					OSStatus e = HIViewSetNeedsDisplayInRegion(ViewRef, a, true);
					DisposeRgn(a);
					if (e)
					{
						LgiTrace("%s:%i - HIViewSetNeedsDisplay failed (%i)\n", __FILE__, __LINE__, e);
					}
					else
					{
						printf("HIViewSetNeedsDisplayInRegion(%s) on %s\n", r->GetStr(), GetClassName());
					}
				}
				else
				{
					Rect a = GetClient();
					OSStatus e = HIViewSetNeedsDisplay(ViewRef, true);
					if (e)
					{
						LgiTrace("%s:%i - HIViewSetNeedsDisplay failed (%i)\n", __FILE__, __LINE__, e);
					}
					else
					{
						printf("HIViewSetNeedsDisplay() on %s\n", GetClassName());
					}
				}
				
				// HiViewRender(ViewRef);
				
				Repainting = false;
			}
		}
		else
		{
			if (r)
			{
				GRect cr = *r;
				cr.Offset(Client.x1, Client.y1);
				Rect a = cr;
				OSStatus e = HIViewSetNeedsDisplay(ViewRef, true);

				if (!e)
				{
					printf("HIViewSetNeedsDisplay() on %s\n", GetClassName());
				}
			}
			else
			{
				OSStatus e = HIViewSetNeedsDisplay(ViewRef, true);

				if (!e)
				{
					printf("HIViewSetNeedsDisplay() on %s\n", GetClassName());
				}
			}
		}
		
		return true;
	}
	#endif

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

		while (p && !dynamic_cast<GWindow*>(p) && !p->Handle())
		{
			GRect r = p->GetPos();
			Up.Offset(r.x1, r.y1);
			p = p->GetParent();
		}

		if (p && p->Handle())
		{
			LgiAssert(p != this);
				
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
	switch (Msg->m)
	{
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
			return OnCommand(MsgA(Msg), 0, (OsView) MsgB(Msg));
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
	int Ox = 0, Oy = 0;

	// Find offset to window
	while (c && c != c->GetWindow())
	{
		#if 0
		GRect cli = c->GetClient(false);
		GRect pos = c->GetPos(); 
		Ox += pos.x1 + cli.x1;
		Oy += pos.y1 + cli.y1;
		#else
		GRect pos = c->GetPos(); 
		Ox += pos.x1;
		Oy += pos.y1;
		#endif
		
		c = c->GetParent();
	}
	
	if (c && c->WindowHandle())
	{
	}
	else
	{
		printf("%s:%i - No window handle to map to screen. c=%p\n", __FILE__, __LINE__, c);
	}
}

void GView::PointToView(GdcPt2 &p)
{
	GViewI *c = this;
	int Ox = 0, Oy = 0;

	// Find offset to window
	while (c && c != c->GetWindow())
	{
		GView *gv = c->GetGView();
		GRect cli = gv ? gv->GView::GetClient(false) : c->GetClient(false);
		GRect pos = c->GetPos(); 
		Ox += pos.x1 + cli.x1;
		Oy += pos.y1 + cli.y1;
		c = c->GetParent();
	}
	
	// Apply window position
	if (c && c->WindowHandle())
	{
	}
	else
	{
		printf("%s:%i - No window handle to map to view. c=%p\n", __FILE__, __LINE__, c);
	}
}

bool GView::GetMouse(GMouse &m, bool ScreenCoords)
{
	if (_View || !GetParent())
	{
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
	if (GetWindow() == this)
	{
		return WindowHandle() != 0;
	}

	if (Handle() && d->Parent)
	{
	}
	
	return false;
}

/*
void GView::OnNcPaint(GSurface *pDC, GRect &r)
{
	int Border = Sunken() || Raised() ? _BorderSize : 0;
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

void BuildTabStops(GArray<GViewI*> &Stops, GViewI *v)
{
	if (v && v->Visible())
	{
		if (v->GetTabStop() && v->Enabled())
			Stops.Add(v);

		GViewIterator *it = v->IterateViews();
		if (it)
		{
			for (GViewI *c = it->First(); c; c = it->Next())
			{
				BuildTabStops(Stops, c);
			}
			DeleteObj(it);
		}
	}
}

void NextTabStop(GViewI *v, int dir)
{
	GArray<GViewI*> Stops;
	BuildTabStops(Stops, v->GetWindow());
	int Idx = Stops.IndexOf(v);
	if (Idx >= 0)
		Idx += dir;
	else
		Idx = 0;
		
	if (Idx < 0) Idx = Stops.Length() - 1;
	else if (Idx >= Stops.Length()) Idx = 0;
	if (Idx < Stops.Length())
		Stops[Idx]->Focus(true);
}

void SetDefaultFocus(GViewI *v)
{
	GArray<GViewI*> Stops;
	BuildTabStops(Stops, v->GetWindow());
	
	if (Stops.Length())
	{
		int Set = -1;
		for (int i=0; i<Stops.Length(); i++)
		{
			if (Stops[i]->Focus())
			{
				Set = i;
				break;
			}
		}
		
		if (Set < 0)
			Set = 0;
	
		Stops[Set]->Focus(true);
	}
}

int VirtualKeyToLgi(int Virt)
{
	switch (Virt)
	{
		// various
		case 122: return VK_F1;
		case 120: return VK_F2;
		case 99: return VK_F3;
		case 118: return VK_F4;
		case 96: return VK_F5;
		case 97: return VK_F6;
		case 98: return VK_F7;
		case 100: return VK_F8;
		case 101: return VK_F9;
		case 109: return VK_F10;
		case 103: return VK_F11;
		case 110: return VK_APPS;
		case 111: return VK_F12;
		
		case 123: return VK_LEFT;
		case 124: return VK_RIGHT;
		case 125: return VK_DOWN;
		case 126: return VK_UP;
		case 114: return VK_INSERT;
		case 116: return VK_PAGEUP;
		case 121: return VK_PAGEDOWN;

		case 53: return VK_ESCAPE;
		case 51: return VK_BACKSPACE;
		case 117: return VK_DELETE;
		
		case 115: return VK_HOME;
		case 119: return VK_END;
		
		// whitespace
		case 76: return VK_RETURN;
		case 36: return '\r';
		case 48: return '\t';
		case 49: return ' ';
		
		// delimiters
		case 27: return '-';
		case 24: return '=';
		case 42: return '\\';
		case 47: return '.';
		case 50: return '`';
		
		// digits
		case 18: return '1';
		case 19: return '2';
		case 20: return '3';
		case 21: return '4';
		case 23: return '5';
		case 22: return '6';
		case 26: return '7';
		case 28: return '8';
		case 25: return '9';
		case 29: return '0';
		
		// alpha
		case 0: return 'a';
		case 11: return 'b';
		case 8: return 'c';
		case 2: return 'd';
		case 14: return 'e';
		case 3: return 'f';
		case 5: return 'g';
		case 4: return 'h';
		case 34: return 'i';
		case 38: return 'j';
		case 40: return 'k';
		case 37: return 'l';
		case 46: return 'm';
		case 45: return 'n';
		case 31: return 'o';
		case 35: return 'p';
		case 12: return 'q';
		case 15: return 'r';
		case 1: return 's';
		case 17: return 't';
		case 32: return 'u';
		case 9: return 'v';
		case 13: return 'w';
		case 7: return 'x';
		case 16: return 'y';
		case 6: return 'z';
		
		default:
			printf("%s:%i - unimplemented virt->lgi code mapping: %d\n", _FL, (unsigned)Virt);
			break;
	}
	
	return 0;
}

static int GetIsChar(GKey &k, int mods)
{
	return k.IsChar =	(mods & 0x100) == 0 &&
	(
		k.c16 >= ' ' ||
		k.c16 == VK_RETURN ||
		k.c16 == VK_TAB ||
		k.c16 == VK_BACKSPACE
	);
}

bool GView::_Attach(GViewI *parent)
{
	bool Status = false;

	d->Parent = (d->ParentI = parent) ? parent->GetGView() : 0;

	LgiAssert(!_InLock);
	_Window = d->GetParent() ? d->GetParent()->GetWindow() : 0;
	if (_Window)
		_Lock = _Window->_Lock;
	
	if (d->GetParent() &&
		d->GetParent()->IsAttached())
	{
		GWindow *w = GetWindow();
		if (w)
		{
			if (_View)
			{
				// Set the view position
				SetPos(Pos);				
			}
		}
		else printf("%s:%i - No window to attach to.\n", _FL);
	}
	else
	{
		// Virtual attach
		printf("Warning: Virtual Attach %s->%s\n", d->Parent ? d->Parent->GetClass() : 0, GetClass());
		Status = true;
	}

	return Status;
}

bool GView::Attach(GViewI *parent)
{
	if (_Attach(parent))
	{
		if (d->Parent && !d->Parent->HasView(this))
		{
			d->Parent->AddView(this);
		}
		
		d->Parent->OnChildrenChanged(this, true);
		return true;
	}
	
	LgiTrace("%s:%i - Attaching '%s' failed.\n", _FL, GetClass());
	return false;
}

bool GView::Detach()
{
	bool Status = false;

	// Detach view
	if (_Window)
	{
		GWindow *Wnd = dynamic_cast<GWindow*>(_Window);
		if (Wnd)
			Wnd->SetFocus(this, GWindow::ViewDelete);
		_Window = NULL;
	}
	if (d->Parent)
	{
		// Remove the view from the parent
		// Events
		int D = 0;
		while (d->Parent->HasView(this))
		{
			d->Parent->DelView(this);
			D++;
		}

		if (D > 1)
		{
			printf("%s:%i - Error: View %s(%p) was in %s(%p) list %i times.\n",
				_FL, GetClass(), this, d->Parent->GetClass(), d->Parent, D);
		}

		d->Parent->OnChildrenChanged(this, false);
		d->Parent = 0;
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
