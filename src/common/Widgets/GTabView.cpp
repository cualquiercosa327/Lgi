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
#include "GDisplayString.h"
#include "GTableLayout.h"
#include "LgiRes.h"

#if defined(MAC) && !defined(COCOA) && !defined(LGI_SDL)
#define MAC_PAINT	1
#else
#define MAC_PAINT	0
#endif

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

#define TAB_MARGIN_X		6
#define CLOSE_BTN_SIZE		8
#define CLOSE_BTN_GAP		8

////////////////////////////////////////////////////////////////////////////////////////////
class GTabViewPrivate
{
public:
	// General
	int Current;
	GRect TabClient;
	bool PourChildren;
	GRect Inset;
	
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
GTabView::GTabView(int id, int x, int y, int cx, int cy, const char *name, int Init) :
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

	#if WINNATIVE
	SetDlgCode(DLGC_WANTARROWS);
	#elif defined BEOS
	Handle()->SetFlags(	Handle()->Flags() |
						B_FULL_UPDATE_ON_RESIZE);
	#endif
	LgiResources::StyleElement(this);
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

size_t GTabView::GetTabs()
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
	return GetFont()->GetHeight() + 4;
}

void GTabView::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (!Attaching)
	{
		TabIterator c(Children);
		if (d->Current >= c.Length())
			d->Current = (int)c.Length() - 1;

		if (Handle())
			Invalidate();
	}
}

#if WINNATIVE
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
		if ((Ctrl = it[i]->FindControl(Id)))
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

		d->Current = (int)MIN(i, (ssize_t)it.Length()-1);
		OnPosChange();

		GTabPage *p = it[d->Current];
		if (p && IsAttached())
		{
			p->Attach(this);
			p->Visible(true);
		}

		Invalidate();
		SendNotify(GNotifyValueChanged);
	}
}

GMessage::Result GTabView::OnEvent(GMessage *Msg)
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
	ssize_t Index = it.IndexOf(Page);
	if (Index >= 0)
	{
		if (Index == d->Current)
		{
			Status = Page->Detach();
			LgiAssert(Status);
		}
		else
		{
			Status = DelView(Page);
			LgiAssert(Status);
		}
		
		delete Page;
		Value(Index);
	}
	
	return Status;
}

GRect &GTabView::GetTabClient()
{
	d->TabClient = GView::GetClient();
	d->TabClient.Offset(-d->TabClient.x1, -d->TabClient.y1);

	#if MAC_PAINT
	d->TabClient.Size(8, 8);
	#else
	d->TabClient.Size(2, 2);
	#endif
	
	d->TabClient.y1 += TabY();

	return d->TabClient;
}

int GTabView::HitTest(GMouse &m)
{
	if (d->LeftBtn.Overlap(m.x, m.y))
	{
		return LeftScrollBtn;
	}
	else if (d->RightBtn.Overlap(m.x, m.y))
	{
		return RightScrollBtn;
	}
	else
	{
		// int Hit = -1;
		TabIterator it(Children);
		for (int i=0; i<it.Length(); i++)
		{
			GTabPage *p = it[i];
			if (p->TabPos.Overlap(m.x, m.y))
				return i;
		}
	}
	
	return NoBtn;
}

void GTabView::OnMouseClick(GMouse &m)
{
	bool DownLeft = m.Down() || m.Left();
	int Result = HitTest(m);
	
	if (Result == LeftScrollBtn)
	{
		if (DownLeft)
		{
			d->Scroll++;
			Invalidate();
		}
	}
	else if (Result == RightScrollBtn)
	{
		if (DownLeft)
		{
			d->Scroll = MAX(0, d->Scroll-1);
			Invalidate();
		}
	}
	else if (Result >= 0)
	{
		TabIterator it(Children);
		GTabPage *p = it[Result];
		if (p)
		{
			if (p->HasButton() &&
				p->BtnPos.Overlap(m.x, m.y))
			{
				if (DownLeft)
				{
					p->OnButtonClick(m);
					// The tab can delete itself after this event
					return;
				}
			}
			else
			{
				// We set this before firing the event, otherwise the
				// code seeing the notication gets the old value.
				if (DownLeft)
					Value(Result);

				p->OnTabClick(m);
			}
		}
	}

	if (DownLeft)
		Focus(true);
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

	#if MAC_PAINT
	
		GRect Margin(6, 6, 6, 6);

		GColour NoPaint = StyleColour(GCss::PropBackgroundColor, LC_MED);
		if (!NoPaint.IsTransparent())
		{
			pDC->Colour(NoPaint);
			pDC->Rectangle();
		}
		
		auto f = GetFont();
		d->Inset = GetClient();
		int FnHalf = (f->GetHeight() + 3) / 2;
		d->Inset.x1 += Margin.x1;
		d->Inset.x2 -= Margin.x2;
		d->Inset.y1 += Margin.y1 + FnHalf;
		d->Inset.y2 -= Margin.y2;
		HIRect Bounds = d->Inset;

		HIThemeTabPaneDrawInfo Info;
		Info.version = 1;
		Info.state = Enabled() ? kThemeStateActive : kThemeStateInactive;
		Info.direction = kThemeTabNorth;
		Info.size = kHIThemeTabSizeNormal;
		Info.kind = kHIThemeTabKindNormal;
		Info.adornment = kHIThemeTabPaneAdornmentNormal;

		OSStatus e = HIThemeDrawTabPane(&Bounds, &Info, pDC->Handle(), kHIThemeOrientationNormal);

		int x = 20, y = d->Inset.y1 - FnHalf;
		for (unsigned i = 0; i < it.Length(); i++)
		{
			GDisplayString ds(f, it[i]->Name());

			GRect r(0, 0, ds.X() + 23, ds.Y() + 3);
			r.Offset(x, y);
			HIRect TabRc = r;
			HIThemeTabDrawInfo TabInfo;
			HIRect Label;
			
			TabInfo.version = 1;
			TabInfo.style = d->Current == i ? kThemeTabNonFrontPressed : kThemeTabNonFront;
			TabInfo.direction = Info.direction;
			TabInfo.size = Info.size;
			TabInfo.adornment = Info.adornment;
			TabInfo.kind = Info.kind;
			if (it.Length() == 1)
				TabInfo.position = kHIThemeTabPositionOnly;
			else if (i == 0)
				TabInfo.position = kHIThemeTabPositionFirst;
			else if (i == it.Length() - 1)
				TabInfo.position = kHIThemeTabPositionLast;
			else
				TabInfo.position = kHIThemeTabPositionMiddle;
			
			e = HIThemeDrawTab(&TabRc, &TabInfo, pDC->Handle(), kHIThemeOrientationNormal, &Label);
			
			r = Label;
			
			f->Transparent(true);
			f->Fore(GColour(LC_TEXT, 24));
			ds.Draw(pDC, r.x1 + (r.X() - ds.X()) / 2, r.y1 + (r.Y() - ds.Y()) / 2, &r);
			
			it[i]->TabPos = r;
			x += r.X() +
				(i ? -1 : 2); // Fudge factor to make it look nice, wtf apple?
		}
	
	#else

		if (GApp::SkinEngine &&
			TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_TABVIEW))
		{
			GSkinState State;
			State.pScreen = pDC;
			State.MouseOver = false;
			GApp::SkinEngine->OnPaint_GTabView(this, &State);
		}
		else
		{
			GRect r = GetTabClient();

			r.Size(-2, -2);
			LgiWideBorder(pDC, r, DefaultRaisedEdge);

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
					int Wid = p->GetTabPx();
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
				LgiWideBorder(pDC, r, DefaultRaisedEdge);

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
				LgiWideBorder(pDC, r, DefaultRaisedEdge);

				int x = r.x1 + (r.X() >> 1) - 2;
				int y = r.y1 + (r.Y() >> 1) - 1;
				pDC->Colour(LC_TEXT, 24);
				for (int i=0; i<4; i++)
				{
					pDC->Line(x+i, y-i, x+i, y+i);
				}
			}
		}
	
	#endif
}

void GTabView::OnPosChange()
{
	GetTabClient();
	if (Children.Length())
	{
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
			else
			{
				GAutoPtr<GViewIterator> It(p->IterateViews());
				if (It->Length() == 1)
				{
					GTableLayout *tl = dynamic_cast<GTableLayout*>(It->First());
					if (tl)
					{
						GRect r = p->GetClient();
						r.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
						tl->SetPos(r);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////
char *_lgi_gview_cmp(GView *a, GView *b)
{
	static char Str[256];
	if (a && b)
	{
		sprintf_s(Str, sizeof(Str),
				"GView: %p,%p Hnd: %p,%p",
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
	Button = false;
	TabCtrl = 0;
	TabPos.ZOff(-1, -1);
	BtnPos.ZOff(-1, -1);

	#if defined BEOS
	if (Handle())
	{
		Handle()->SetViewColor(216, 216, 216);
		Handle()->SetResizingMode(B_FOLLOW_ALL_SIDES);
		Handle()->SetFlags(Handle()->Flags() & ~B_NAVIGABLE);
	}
	#elif WINNATIVE
	SetStyle(GetStyle() | WS_CLIPCHILDREN);
	CreateClassW32(GetClass(), 0, CS_HREDRAW | CS_VREDRAW);
	#endif

	#if MAC_PAINT
	GetCss(true)->BackgroundColor(GCss::ColorDef(GetBackground()));
	#endif

	LgiResources::StyleElement(this);
}

GTabPage::~GTabPage()
{
}

int GTabPage::GetTabPx()
{
	char *Text = Name();
	GDisplayString ds(GetFont(), Text);
	int Px = ds.X() + (TAB_MARGIN_X << 1);
	if (Button)
		Px += CLOSE_BTN_GAP + CLOSE_BTN_SIZE;
	return Px;
}

bool GTabPage::HasButton()
{
	return Button;
}

void GTabPage::HasButton(bool b)
{
	Button = b;
	if (GetParent())
		GetParent()->Invalidate();
}

void GTabPage::OnButtonClick(GMouse &m)
{
	if (GetId() > 0)
		SendNotify(GNotifyTabPage_ButtonClick);
}

void GTabPage::OnTabClick(GMouse &m)
{
	GViewI *v = GetId() > 0 ? this : GetParent();
	v->SendNotify(GNotifyItem_Click);
}

void GTabPage::OnButtonPaint(GSurface *pDC)
{
	#if MAC_PAINT
	
	#else
	
	// The default is a close button
	GColour Low(LC_LOW, 24);
	GColour Mid(LC_MED, 24);
	Mid = Mid.Mix(Low);
	
	pDC->Colour(Mid);
	pDC->Line(BtnPos.x1, BtnPos.y1+1, BtnPos.x2-1, BtnPos.y2);
	pDC->Line(BtnPos.x1+1, BtnPos.y1, BtnPos.x2, BtnPos.y2-1);
	pDC->Line(BtnPos.x1, BtnPos.y2-1, BtnPos.x2-1, BtnPos.y1);
	pDC->Line(BtnPos.x1+1, BtnPos.y2, BtnPos.x2, BtnPos.y1+1);

	pDC->Colour(Low);
	pDC->Line(BtnPos.x1+1, BtnPos.y1+1, BtnPos.x2-1, BtnPos.y2-1);
	pDC->Line(BtnPos.x2-1, BtnPos.y1+1, BtnPos.x1+1, BtnPos.y2-1);
	
	#endif
}

char *GTabPage::Name()
{
	return GBase::Name();
}

bool GTabPage::Name(const char *name)
{
	bool Status = GView::Name(name);
	if (GetParent())
		GetParent()->Invalidate();
	return Status;
}

void GTabPage::PaintTab(GSurface *pDC, bool Selected)
{
	#if MAC_PAINT

	#else
	
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
	
	int Cx = r.x1 + TAB_MARGIN_X;
	char *t = Name();
	if (t)
	{
		GFont *f = GetFont();
		GDisplayString ds(f, t);
		f->Colour(LC_TEXT, LC_MED);
		f->Transparent(true);
		
		int y = r.y1 + ((r.Y()-ds.Y())/2);
		ds.Draw(pDC, Cx, y);
		
		if (TabCtrl->Focus() && Selected)
		{
			r.Set(Cx, y, Cx+ds.X(), y+ds.Y());
			r.Size(-2, -1);
			r.y1++;
			pDC->Colour(LC_LOW, 24);
			pDC->Box(&r);
		}

		Cx += ds.X() + CLOSE_BTN_GAP;
	}
	
	if (Button)
	{
		BtnPos.ZOff(CLOSE_BTN_SIZE-1, CLOSE_BTN_SIZE-1);
		BtnPos.Offset(Cx, r.y1 + ((r.Y()-BtnPos.Y()) / 2));
		OnButtonPaint(pDC);
	}
	else BtnPos.ZOff(-1, -1);
	
	#endif
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

GMessage::Result GTabPage::OnEvent(GMessage *Msg)
{
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

GColour GTabPage::GetBackground()
{
	#if MAC_PAINT
	return GColour(207, 207, 207);
	#else
	return GColour(LC_MED, 24);
	#endif
}

void GTabPage::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	GColour Bk = StyleColour(GCss::PropBackgroundColor, LC_MED);
	pDC->Colour(Bk);
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
		while ((v = ch->First()))
		{
			v->Detach();
			DelView(v);
		}
		DeleteObj(ch);
	}
	
	bool Status = GLgiRes::LoadFromResource(Res, this, 0, &n);
	if (ValidStr(n))
		Name(n);

	#if MAC_PAINT
	// Sigh
	for (auto c : Children)
		c->GetCss(true)->BackgroundColor(GCss::ColorDef(GetBackground()));
	#endif

	if (IsAttached())
		AttachChildren();

	return Status;
}

void GTabPage::Select()
{
	if (GetParent())
	{
		GAutoPtr<GViewIterator> i(GetParent()->IterateViews());
		ssize_t Idx = i->IndexOf(this);
		if (Idx >= 0)
		{
			GetParent()->Value(Idx);
		}
	}
}
