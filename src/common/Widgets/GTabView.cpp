/*hdr
**      FILE:           GTabView.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           20/10/2000
**      DESCRIPTION:    Lgi self-drawn tab control
**
**      Copyright (C) 2000 Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GTabView.h"
#include "GSkinEngine.h"

#ifdef WIN32

#ifdef TOOL_VLOW
#undef TOOL_VLOW
#endif

#ifdef TOOL_LOW
#undef TOOL_LOW
#endif

#ifdef TOOL_HIGH
#undef TOOL_HIGH
#endif

#ifdef TOOL_VHIGH
#undef TOOL_VHIGH
#endif


#define TOOL_VLOW	GetSysColor(COLOR_3DDKSHADOW)
#define TOOL_LOW	GetSysColor(COLOR_3DSHADOW)
#define TOOL_HIGH	GetSysColor(COLOR_3DLIGHT)
#define TOOL_VHIGH	GetSysColor(COLOR_3DHILIGHT)

#endif

////////////////////////////////////////////////////////////////////////////////////////////
class GTabViewPrivate
{
public:
	// General
	int Current;
	GRect TabClient;
	bool PourChildren;
	
	// Scrolling
	int Scroll;			// number of buttons scrolled off the left of the control
	GRect LeftBtn;	// scroll left button
	GRect RightBtn;	// scroll right button

	GTabViewPrivate()
	{
		PourChildren = true;
		Current = 0;
		TabClient.ZOff(-1, -1);

		Scroll = 0;
		LeftBtn.ZOff(-1, -1);
		RightBtn.ZOff(-1, -1);
	}
};

class TabIterator : public GArray<GTabPage*>
{
public:
	TabIterator(List<GViewI> &l)
	{
		for (GViewI *c = l.First(); c; c = l.Next())
		{
			GTabPage *p = dynamic_cast<GTabPage*>(c);
			if (p) Add(p);
		}

		fixed = true;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
// Tab Control
GTabView::GTabView(int id, int x, int y, int cx, int cy, char *name, int Init) :
	ResObject(Res_TabView)
{
	d = new GTabViewPrivate;
	d->Current = Init;

	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	_BorderSize = 0;
	SetTabStop(true);
	SetPourLargest(true);

	#if WIN32NATIVE
	SetDlgCode(DLGC_WANTARROWS);
	#elif defined BEOS
	Handle()->SetFlags(	Handle()->Flags() |
						B_FULL_UPDATE_ON_RESIZE);
	#endif
}

GTabView::~GTabView()
{
	DeleteObj(d);
}

bool GTabView::GetPourChildren()
{
	return d->PourChildren;
}

void GTabView::SetPourChildren(bool b)
{
	d->PourChildren = b;
}

GTabPage *GTabView::TabAt(int Idx)
{
	TabIterator i(Children);
	return i[Idx];
}

int GTabView::GetTabs()
{
	return Children.Length();
}

GTabPage *GTabView::GetCurrent()
{
	TabIterator i(Children);
	return i[d->Current];
}

int GTabView::TabY()
{
	return max(16, GetFont()->GetHeight()) + 4;
}

void GTabView::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
}

#if WIN32NATIVE
GViewI *GTabView::FindControl(HWND hCtrl)
{
	GViewI *Ctrl = 0;

	if (hCtrl == Handle())
	{
		return this;
	}

	TabIterator it(Children);
	for (int i=0; i<it.Length(); i++)
	{
		if (Ctrl = it[i]->FindControl(hCtrl))
			return Ctrl;
	}

	return 0;
}
#endif

GViewI *GTabView::FindControl(int Id)
{
	if (GetId() == Id)
	{
		return this;
	}

	GViewI *Ctrl;
	TabIterator it(Children);
	for (int i=0; i<it.Length(); i++)
	{
		if (Ctrl = it[i]->FindControl(Id))
			return Ctrl;
	}

	return 0;
}

bool GTabView::Attach(GViewI *parent)
{
	bool Status = GView::Attach(parent);
	if (Status)
	{
		TabIterator it(Children);
		GTabPage *p = d->Current < it.Length() ? it[d->Current] : 0;
		if (p)
		{
			OnPosChange();
			p->Attach(this);
		}
		
		for (int i=0; i<it.Length(); i++)
		{
			it[i]->_Window = _Window;
		}
	}

	return Status;
}

int64 GTabView::Value()
{
	return d->Current;
}

void GTabView::OnCreate()
{
	TabIterator it(Children);
	GTabPage *p = d->Current < it.Length() ? it[d->Current] : 0;
	if (p)
	{
		p->Attach(this);
		p->Visible(true);
	}
}

void GTabView::Value(int64 i)
{
	if (i != d->Current)
	{
		// change tab
		TabIterator it(Children);
		GTabPage *Old = it[d->Current];
		if (Old)
		{
			Old->Visible(false);
		}

		d->Current = i;
		OnPosChange();

		GTabPage *p = it[d->Current];
		if (p && IsAttached())
		{
			p->Attach(this);
			p->Visible(true);
		}

		Invalidate();
		SendNotify(d->Current);
	}
}

int GTabView::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

int GTabView::OnNotify(GViewI *Ctrl, int Flags)
{
	if (GetParent())
	{
		return GetParent()->OnNotify(Ctrl, Flags);
	}

	return 0;
}

bool GTabView::Append(GTabPage *Page, int Where)
{
	if (Page)
	{
		Page->TabCtrl = this;
		Page->_Window = _Window;
		if (IsAttached() && Children.Length() == 1)
		{
			Page->Attach(this);
			OnPosChange();
		}
		else
		{
			Page->Visible(Children.Length() == d->Current);
			AddView(Page);
		}

		Invalidate();

		return true;
	}
	return false;
}

GTabPage *GTabView::Append(const char *name, int Where)
{
	GTabPage *Page = new GTabPage(name);
	if (Page)
	{
		Page->TabCtrl = this;
		Page->_Window = _Window;
		Page->SetParent(this);

		if (IsAttached() && Children.Length() == 0)
		{
			Page->Attach(this);
			OnPosChange();
		}
		else
		{
			Page->Visible(Children.Length() == d->Current);
			AddView(Page);
		}

		Invalidate();
	}
	return Page;
}

bool GTabView::Delete(GTabPage *Page)
{
	bool Status = false;

	TabIterator it(Children);
	int Index = it.IndexOf(Page);
	if (Index >= 0)
	{
		if (Index == d->Current)
		{
			Page->Detach();
		}
		
		bool b = DelView(Page);
		LgiAssert(b);
		Status = false;
	}
	
	return Status;
}

GRect &GTabView::GetTabClient()
{
	d->TabClient = GView::GetClient();
	d->TabClient.Offset(-d->TabClient.x1, -d->TabClient.y1);
	d->TabClient.Size(2, 2);
	d->TabClient.y1 += TabY();

	return d->TabClient;
}

void GTabView::OnMouseClick(GMouse &m)
{
	// m.Trace("GTabView::OnMouseClick");

	if (m.Down())
	{
		if (m.Left())
		{
			if (d->LeftBtn.Overlap(m.x, m.y))
			{
				d->Scroll++;
				Invalidate();
			}
			else if (d->RightBtn.Overlap(m.x, m.y))
			{
				d->Scroll = max(0, d->Scroll-1);
				Invalidate();
			}
			else
			{
				int Hit = -1;
				TabIterator it(Children);
				for (int i=0; i<it.Length(); i++)
				{
					if (it[i]->TabPos.Overlap(m.x, m.y))
					{
						Hit = i;
						break;
					}
				}
				
				if (Hit >= 0)
				{
					Value(Hit);
				}
			}

			Focus(true);
		}
	}
}

bool GTabView::OnKey(GKey &k)
{
	if (k.Down())
	{
		switch (k.vkey)
		{
			case VK_LEFT:
			{
				if (d->Current > 0)
				{
					TabIterator it(Children);
					GTabPage *p = it[d->Current - 1];
					if (p && !p->TabPos.Valid())
					{
						if (d->Scroll) d->Scroll--;
					}

					Value(d->Current - 1);
				}
				return true;
				break;
			}
			case VK_RIGHT:
			{
				TabIterator it(Children);
				if (d->Current < it.Length() - 1)
				{
					GTabPage *p = it[d->Current + 1];
					if (p && !p->TabPos.Valid())
					{
						d->Scroll++;
					}
					
					Value(d->Current + 1);
				}
				return true;
				break;
			}
		}
	}
	
	return false;
}

void GTabView::OnFocus(bool f)
{
	TabIterator it(Children);
	GTabPage *p = it[d->Current];
	if (p)
	{
		GRect r = p->TabPos;
		r.y2 += 2;
		Invalidate(&r);
	}
}

void GTabView::OnPaint(GSurface *pDC)
{
	TabIterator it(Children);
	if (d->Current >= it.Length())
		Value(it.Length() - 1);

	if (GApp::SkinEngine AND
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_TABVIEW))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.MouseOver = false;
		State.Text = 0;
		GApp::SkinEngine->OnPaint_GTabView(this, &State);
	}
	else
	{
		GRect r = GetTabClient();

		r.Size(-2, -2);
		LgiWideBorder(pDC, r, RAISED);

		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(0, 0, X()-1, d->TabClient.y1-3);

		GTabPage *Sel = 0;
		int x = r.x1;
		
		if (d->Scroll)
		{
			d->RightBtn.ZOff(12, TabY() - 2);
			x = d->RightBtn.x2 + 4;
		}
		else
		{
			d->RightBtn.ZOff(-1, -1);
		}
		d->LeftBtn.ZOff(-1, -1);
		
		for (int n=0; n<it.Length(); n++)
		{
			GTabPage *p = it[n];
			if (n < d->Scroll)
			{
				p->TabPos.ZOff(-1, -1);
			}
			else
			{
				char *Text = p->Name();
				GDisplayString ds(p->GetFont(), Text);
				int Wid = ds.X() + 13;
				p->TabPos.ZOff(Wid, TabY()-3);
				p->TabPos.Offset(x, 2);
				
				if (p->TabPos.x2 > r.x2 - 16)
				{
					d->LeftBtn.x2 = X()-1;
					d->LeftBtn.x1 = d->LeftBtn.x2 - 12;
					d->LeftBtn.y1 = 0;
					d->LeftBtn.y2 = TabY() - 2;
					
					p->TabPos.ZOff(-1, -1);
					break;
				}
				
				if (d->Current != n)
				{
					p->PaintTab(pDC, false);
				}
				else
				{
					Sel = p;
				}
				x += Wid+1;
			}
		}
		
		if (!it.Length())
		{
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(&r);
		}
		
		if (Sel)
		{
			Sel->PaintTab(pDC, true);
		}
		
		if (d->LeftBtn.Valid())
		{
			r = d->LeftBtn;
			LgiWideBorder(pDC, r, RAISED);

			int x = r.x1 + (r.X() >> 1) + 1;
			int y = r.y1 + (r.Y() >> 1) - 1;
			pDC->Colour(LC_TEXT, 24);
			for (int i=0; i<4; i++)
			{
				pDC->Line(x-i, y-i, x-i, y+i);
			}
		}
		if (d->RightBtn.Valid())
		{
			r = d->RightBtn;
			LgiWideBorder(pDC, r, RAISED);

			int x = r.x1 + (r.X() >> 1) - 2;
			int y = r.y1 + (r.Y() >> 1) - 1;
			pDC->Colour(LC_TEXT, 24);
			for (int i=0; i<4; i++)
			{
				pDC->Line(x+i, y-i, x+i, y+i);
			}
		}
	}
}

void GTabView::OnPosChange()
{
	GetTabClient();
	TabIterator it(Children);
	GTabPage *p = it[d->Current];
	if (p)
	{
		p->SetPos(d->TabClient, true);
		if (d->PourChildren)
		{
			GRect r = d->TabClient;
			r.Offset(-r.x1, -r.y1);
			r.Size(5, 5);
			GRegion Rgn(r);

			GAutoPtr<GViewIterator> It(p->IterateViews());
			for (GViewI *c = It->First(); c; c = It->Next())
			{
				c->Pour(Rgn);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////
char *_lgi_gview_cmp(GView *a, GView *b)
{
	static char Str[256];
	if (a AND b)
	{
		sprintf(Str,
				"GView: %p,%p Hnd: %x,%x",
				dynamic_cast<GView*>(a),
				dynamic_cast<GView*>(b),
				a->Handle(),
				b->Handle());
	}
	else
	{
		Str[0] = 0;
	}
	return Str;
}


GTabPage::GTabPage(const char *name) : ResObject(Res_Tab)
{
	GRect r(0, 0, 1000, 1000);
	SetPos(r);
	Name(name);
	TabCtrl = 0;
	TabPos.ZOff(-1, -1);

	#if defined BEOS
	Handle()->SetViewColor(216, 216, 216);
	Handle()->SetResizingMode(B_FOLLOW_ALL_SIDES);
	Handle()->SetFlags(Handle()->Flags() & ~B_NAVIGABLE);
	#elif WIN32NATIVE
	SetStyle(GetStyle() | WS_CLIPCHILDREN);
	CreateClassW32(GetClass(), 0, CS_HREDRAW | CS_VREDRAW);
	#endif
}

GTabPage::~GTabPage()
{
	int asd=0;
}

void GTabPage::PaintTab(GSurface *pDC, bool Selected)
{
	GRect r = TabPos;
	if (Selected)
	{
		r.Size(-2, -2);
	}
	
	pDC->Colour(LC_LIGHT, 24);
	
	bool First = false;
	if (TabCtrl)
	{
		TabIterator it(TabCtrl->Children);
		First = it[0] == this;
	}

	if (First)
	{
		pDC->Line(r.x1, r.y1+1, r.x1, r.y2);
	}
	else
	{
		pDC->Line(r.x1, r.y1+1, r.x1, r.y2-1);
	}
	pDC->Line(r.x1+1, r.y1, r.x2-1, r.y1);

	pDC->Colour(LC_HIGH, 24);
	pDC->Line(r.x1+1, r.y1+1, r.x1+1, r.y2);
	pDC->Line(r.x1+1, r.y1+1, r.x2-1, r.y1+1);

	pDC->Colour(LC_LOW, 24);
	pDC->Line(r.x2-1, r.y1+1, r.x2-1, r.y2);

	pDC->Colour(LC_SHADOW, 24);
	pDC->Line(r.x2, r.y1+1, r.x2, r.y2-1);

	r.x2 -= 2;
	r.x1 += 2;
	r.y1 += 2;
	
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);
	pDC->Set(r.x1, r.y1);
	pDC->Set(r.x2, r.y1);
	
	char *t = Name();
	if (t)
	{
		GFont *f = GetFont();
		GDisplayString ds(f, t);
		f->Colour(LC_TEXT, LC_MED);
		f->Transparent(true);
		
		int x = r.x1 + ((r.X()-ds.X())/2), y = r.y1 + ((r.Y()-ds.Y())/2);
		ds.Draw(pDC, x, y);
		
		if (TabCtrl->Focus() AND Selected)
		{
			r.Set(x, y, x+ds.X(), y+ds.Y());
			r.Size(-2, -1);
			r.y1++;
			pDC->Colour(LC_LOW, 24);
			pDC->Box(&r);
		}
	}
}

bool GTabPage::Attach(GViewI *parent)
{
	bool Status = false;

	if (TabCtrl)
	{
		if (!IsAttached())
		{
			Status = GView::Attach(parent);
		}
		else
		{
			Status = true;
		}
		
		for (GViewI *w = Children.First(); w; w = Children.Next())
		{
			if (!w->IsAttached())
			{
				w->Attach(this);
				w->SetNotify(TabCtrl->GetParent());
			}
		}
	}

	return Status;
}

char *GTabPage::Name()
{
	return GBase::Name();
}

bool GTabPage::Name(const char *name)
{
	bool Status = GView::Name(name);
	if (GetParent()) GetParent()->Invalidate();
	return Status;
}

int GTabPage::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		#ifdef WIN32
		#endif
	}

	return GView::OnEvent(Msg);
}

void GTabPage::Append(GViewI *Wnd)
{
	if (Wnd)
	{
		Wnd->SetNotify(TabCtrl);

		if (IsAttached() && TabCtrl)
		{
			Wnd->Attach(this);

			GTabView *v = dynamic_cast<GTabView*>(GetParent());
			if (v && v->GetPourChildren())
			{
				v->OnPosChange();
			}
		}
		else if (!HasView(Wnd))
		{
			AddView(Wnd);
		}
		else LgiAssert(0);
	}
}

bool GTabPage::Remove(GViewI *Wnd)
{
	if (Wnd)
	{
		LgiAssert(HasView(Wnd));
		Wnd->Detach();
		return true;
	}
	return false;
}

void GTabPage::OnPaint(GSurface *pDC)
{	
	GRect r(0, 0, X()-1, Y()-1);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);
}

void GTabPage::OnFocus(bool b)
{
}

bool GTabPage::OnKey(GKey &k)
{
	return false;
}

bool GTabPage::LoadFromResource(int Res)
{
	GAutoString n;

	GViewIterator *ch = IterateViews();
	if (ch)
	{
		GViewI *v;
		while (v = ch->First())
		{
			v->Detach();
			DelView(v);
		}
		DeleteObj(ch);
	}
	
	bool Status = GLgiRes::LoadFromResource(Res, this, 0, &n);
	if (ValidStr(n))
		Name(n);

	if (IsAttached())
		AttachChildren();

	return Status;
}




