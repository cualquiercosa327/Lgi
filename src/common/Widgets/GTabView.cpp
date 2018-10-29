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
#include "GPath.h"

enum TabViewStyle
{
	TvWinXp, // Skin
	TvWin7,
	TvMac,
};

#define MAC_STYLE_RADIUS		7
#define MAC_DBL_BUF				1
#define TAB_TXT_PAD				3

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
#define cFocusFore			GColour(LC_FOCUS_SEL_FORE, 24)
#define cFocusBack			GColour(LC_FOCUS_SEL_BACK, 24)

////////////////////////////////////////////////////////////////////////////////////////////
class GTabViewPrivate
{
public:
	// General
	int Current;
	GRect TabClient;
	bool PourChildren;

	// Painting
	GRect Inset, Tabs;
	int TabsHeight;
	double TabsBaseline;
	int Depth;
	TabViewStyle Style;
	enum ResType
	{
		ResWhite,
		Res248,
		ResSel,
		ResMax
	};
	GAutoPtr<GSurface> Corners[ResMax];
	GColour cBorder, cFill;
	
	// Scrolling
	int Scroll;			// number of buttons scrolled off the left of the control
	GRect LeftBtn;	// scroll left button
	GRect RightBtn;	// scroll right button

	GTabViewPrivate()
	{
		Depth = 0;
		TabsHeight = 0;
		TabsBaseline = 0.0;
		PourChildren = true;
		Current = 0;
		TabClient.ZOff(-1, -1);

		Scroll = 0;
		LeftBtn.ZOff(-1, -1);
		RightBtn.ZOff(-1, -1);

		Style = TvMac;
	}

	bool DrawCircle(GAutoPtr<GSurface> &Dc, GColour c)
	{
		if (Dc)
			return true;

		double r = 7.0;
		int x = (int)(r * 2.0);
		if (!Dc.Reset(new GMemDC(x, x, System32BitColourSpace)))
			return false;

		Dc->Colour(0, 32);
		Dc->Rectangle();

		GPath p;
		p.Circle(r, r, r-0.7);
		p.SetFillRule(FILLRULE_ODDEVEN);
		GSolidBrush s(c);
		p.Fill(Dc, s);
		p.Empty();

		p.Circle(r, r, r);
		p.Circle(r, r, r - 1.0);
		p.SetFillRule(FILLRULE_ODDEVEN);
		// GSolidBrush s2(GColour(0xcb, 0xcb, 0xcb));
		GBlendStop Stops[2] = {
			{0.0, Rgb32(0xcb, 0xcb, 0xcb)},
			{1.0, Rgb32(0xaa, 0xaa, 0xaa)}
		};
		GPointF a(4, 4), b(9, 9);
		GLinearBlendBrush s2(a, b, CountOf(Stops), Stops);
		p.Fill(Dc, s2);

		Dc->ConvertPreMulAlpha(true);
		return true;
	}

	void CreateCorners()
	{
		GAutoPtr<GSurface> &White = Corners[ResWhite];
		GAutoPtr<GSurface> &c248 = Corners[Res248];
		GAutoPtr<GSurface> &Sel = Corners[ResSel];

		DrawCircle(White, GColour::White);
		DrawCircle(c248, GColour(248, 248, 248));
		DrawCircle(Sel, cFocusBack);
	}
};

struct GTabPagePriv
{
	GTabPage *Tab;
	bool NonDefaultFont;
	GAutoPtr<GDisplayString> Ds;

	GTabPagePriv(GTabPage *t) : Tab(t)
	{
		NonDefaultFont = false;
	}

	GDisplayString *GetDs()
	{
		char *Text = Tab->Name();
		if (Text && !Ds)
		{
			GFont *f = NULL;
			auto s = Tab->GetCss();
			NonDefaultFont = s ? s->HasFontStyle() : false;
			if (NonDefaultFont)
			{
				if ((f = new GFont))
				{
					if (f->CreateFromCss(s))
						Tab->SetFont(f, true);
					else
						DeleteObj(f);
				}
			}
			else
			{
				Tab->SetFont(f = SysFont);
			}
		
			if (f)
				Ds.Reset(new GDisplayString(f, Text));
			else
				LgiAssert(!"no font.");
		}

		return Ds;
	}
};

class TabIterator : public GArray<GTabPage*>
{
public:
	TabIterator(List<GViewI> &l)
	{
		for (auto c : l)
		{
			auto p = dynamic_cast<GTabPage*>(c);
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
	return d->TabsHeight + (TAB_TXT_PAD << 1);
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
	if (d->Style == TvMac)
	{
		d->TabClient = CalcInset();
		d->TabClient.Size(2, 2); // The inset border
		d->TabClient.y1 = d->Tabs.y2 + 1; // The tab strip

		GTabPage *p = Children.Length() ? GetCurrent() : NULL;
		if (p && p->GetCss())
		{
			// Inset by any padding
			GCss::Len l;
			d->TabClient.x1 += (l = p->GetCss()->PaddingLeft()).IsValid()   ? l.ToPx(d->TabClient.X(), GetFont()) : 0;
			d->TabClient.y1 += (l = p->GetCss()->PaddingTop()).IsValid()    ? l.ToPx(d->TabClient.Y(), GetFont()) : 0;
			d->TabClient.x2 -= (l = p->GetCss()->PaddingRight()).IsValid()  ? l.ToPx(d->TabClient.X(), GetFont()) : 0;
			d->TabClient.y2 -= (l = p->GetCss()->PaddingBottom()).IsValid() ? l.ToPx(d->TabClient.Y(), GetFont()) : 0;
		}
	}
	else
	{
		d->TabClient = GView::GetClient();
		d->TabClient.Offset(-d->TabClient.x1, -d->TabClient.y1);
		d->TabClient.Size(2, 2);
		d->TabClient.y1 += TabY();
	}
	

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

	if (m.Down())
		Focus(true);
	
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

void GTabView::OnAttach()
{
	d->Depth = 0;
	GViewI *p = this;
	while ((p = p->GetParent()))
	{
		GTabView *tv = dynamic_cast<GTabView*>(p);
		if (tv)
			d->Depth++;
	}

	GColour cDialog(LC_MED, 24);
			
	auto mul = pow(0.91f, 1+d->Depth);
	auto c = (int) ((double)cDialog.r() * mul);
	d->cBorder.Rgb(c, c, c);
			
	mul = pow(0.959f, 1+d->Depth);
	c = (int) ((double)cDialog.r() * mul);
	d->cFill.Rgb(c, c, c);

	auto *Css = GetCss(true);
	if (Css)
	{
		if (!Css->BackgroundColor().IsValid())
			Css->BackgroundColor(GCss::ColorDef(d->cFill));
	}
}

GRect &GTabView::CalcInset()
{
	GRect Padding(0, 0, 0, 0);
	d->Inset = GetClient();
	auto f = GetFont();
	if (GetCss())
	{
		GCss::Len l;
		if ((l = GetCss()->PaddingLeft()).IsValid())   Padding.x1 = l.ToPx(d->Inset.X(), f);
		if ((l = GetCss()->PaddingTop()).IsValid())    Padding.y1 = l.ToPx(d->Inset.Y(), f);
		if ((l = GetCss()->PaddingRight()).IsValid())  Padding.x2 = l.ToPx(d->Inset.X(), f);
		if ((l = GetCss()->PaddingBottom()).IsValid()) Padding.y2 = l.ToPx(d->Inset.Y(), f);
	}

	int TabTextY = 0;
	d->TabsBaseline = 0;
	TabIterator Tabs(Children);
	for (auto t : Tabs)
	{
		auto Ds = t->d->GetDs();
		if (Ds)
		{
			TabTextY = MAX(TabTextY, Ds->Y());
			auto Fnt = Ds->GetFont();
			d->TabsBaseline = MAX(d->TabsBaseline, Fnt->Ascent());
		}
	}
	if (!TabTextY)
		TabTextY = f->GetHeight();

	d->TabsHeight = TabTextY;
	d->Inset.x1 += Padding.x1;
	d->Inset.x2 -= Padding.x2;
	d->Inset.y1 += Padding.y1;
	d->Inset.y2 -= Padding.y2;

	d->Tabs.ZOff(d->Inset.X() - 20, TabY() - 1);
	d->Tabs.Offset(d->Inset.x1 + 10, d->Inset.y1);

	d->Inset.y1 += d->TabsHeight / 2;

	return d->Inset;
}

void GTabView::OnStyleChange()
{
	TabIterator Tabs(Children);
	for (auto t : Tabs)
		t->OnStyleChange();
	Invalidate();
}

void GTabView::OnPaint(GSurface *pDC)
{
	TabIterator it(Children);
	if (d->Current >= it.Length())
		Value(it.Length() - 1);

	if (d->Style == TvMac)
	{
		CalcInset();

		GView *Pv = GetParent() ? GetParent()->GetGView() : NULL;
		GColour NoPaint = (Pv ? Pv : this)->StyleColour(GCss::PropBackgroundColor, GColour(LC_MED, 24));
		if (!NoPaint.IsTransparent())
		{
			pDC->Colour(NoPaint);
			pDC->Rectangle();
		}

		#ifdef LGI_CARBON

			HIRect Bounds = d->Inset;

			HIThemeTabPaneDrawInfo Info;
			Info.version = 1;
			Info.state = Enabled() ? kThemeStateActive : kThemeStateInactive;
			Info.direction = kThemeTabNorth;
			Info.size = kHIThemeTabSizeNormal;
			Info.kind = kHIThemeTabKindNormal;
			Info.adornment = kHIThemeTabPaneAdornmentNormal;

			OSStatus e = HIThemeDrawTabPane(&Bounds, &Info, pDC->Handle(), kHIThemeOrientationNormal);

		#else

			// Draw the inset area at 'd->Inset'
			GRect Bounds = d->Inset;
			pDC->Colour(d->cBorder);
			pDC->Box(&Bounds);
			Bounds.Size(1, 1);
			pDC->Box(&Bounds);
			Bounds.Size(1, 1);
			pDC->Colour(d->cFill);
			pDC->Rectangle(&Bounds);

		#endif

		int x = d->Tabs.x1, y = d->Tabs.y1;
		#ifndef LGI_CARBON
		GSurface *pScreen = pDC;
		#endif
		auto f = GetFont();
		for (unsigned i = 0; i < it.Length(); i++)
		{
			auto Tab = it[i];
			auto Foc = Focus();
			auto TabTxt = Tab->Name();
			GDisplayString *ds = Tab->d->GetDs();
			bool First = i == 0;
			bool Last = i == it.Length() - 1;
			bool IsCurrent = d->Current == i;

			GRect r(0, 0, ds->X() + 23, d->Tabs.Y() - 1);
			r.Offset(x, y);

			#ifdef LGI_CARBON

				HIRect TabRc = r;
				HIThemeTabDrawInfo TabInfo;
				HIRect Label;
			
				TabInfo.version = 1;
				TabInfo.style = IsCurrent ? (Foc ? kThemeTabFront : kThemeTabNonFrontPressed) : kThemeTabNonFront;
				TabInfo.direction = Info.direction;
				TabInfo.size = Info.size;
				TabInfo.adornment = Info.adornment;
				TabInfo.kind = Info.kind;
				if (it.Length() == 1)
					TabInfo.position = kHIThemeTabPositionOnly;
				else if (First)
					TabInfo.position = kHIThemeTabPositionFirst;
				else if (Last)
					TabInfo.position = kHIThemeTabPositionLast;
				else
					TabInfo.position = kHIThemeTabPositionMiddle;
			
				e = HIThemeDrawTab(&TabRc, &TabInfo, pDC->Handle(), kHIThemeOrientationNormal, &Label);
			
				r = Label;
			
			#else

				GColour cTopEdge(203, 203, 203);
				GColour cBottomEdge(170, 170, 170);
				GColour cTabFill = IsCurrent ? (Foc ? cFocusBack : GColour(248, 248, 248)) : GColour::White;
				GColour cInterTabBorder(231, 231, 231);
				GRect b = r;

				#if MAC_DBL_BUF
				GMemDC Mem;
				if (First || Last)
				{
					if (Mem.Create(r.X(), r.Y(), System32BitColourSpace))
					{
						pDC = &Mem;
						b.Offset(-b.x1, -b.y1);
					}
				}
				#endif

				d->CreateCorners();

				pDC->Colour(cTopEdge);
				pDC->Line(b.x1, b.y1, b.x2, b.y1); // top edge
				if (i == 0)
				{
					pDC->Line(b.x1, b.y1, b.x1, b.y2); // left edge
				}
				else
				{
					pDC->Colour(cInterTabBorder);
					pDC->Line(b.x1, b.y1+1, b.x1, b.y2+1); // left edge
				}
				pDC->Colour(cBottomEdge);
				pDC->Line(b.x2, b.y2, b.x1, b.y2); // bottom edge
				if (Last)
				{
					pDC->Line(b.x2, b.y2, b.x2, b.y1); // right edge
				}
				else
				{
					pDC->Colour(cInterTabBorder);
					pDC->Line(b.x2, b.y2-1, b.x2, b.y1+1); // right edge between tabs
				}
				b.Size(1, 1);
				
				pDC->Colour(cTabFill);
				pDC->Rectangle(&b);

				GRect Clip00(0, 0, MAC_STYLE_RADIUS-1, MAC_STYLE_RADIUS-1);
				auto Res = IsCurrent ? (Foc ? GTabViewPrivate::ResSel : GTabViewPrivate::Res248) : GTabViewPrivate::ResWhite;
				if (First)
				{
					GRect Clip01 = Clip00.Move(0, r.Y() - Clip00.Y());
					
					#if MAC_DBL_BUF
					// Erase the areas we will paint over
					pDC->Op(GDC_SET);
					pDC->Colour(pScreen->SupportsAlphaCompositing() ? GColour(0, 32) : NoPaint);
					pDC->Rectangle(&Clip00);
					pDC->Colour(pScreen->SupportsAlphaCompositing() ? GColour(0, 32) : d->cFill);
					pDC->Rectangle(&Clip01);
					#endif
					
					// Draw in the rounded corners
					pDC->Op(GDC_ALPHA);
					pDC->Blt(Clip00.x1, Clip00.y1, d->Corners[Res], Clip00);
					pDC->Blt(Clip01.x1, Clip01.y1, d->Corners[Res], Clip00.Move(0, MAC_STYLE_RADIUS));
				}

				if (Last)
				{
					GRect Clip10 = Clip00.Move(r.X() - Clip00.X(), 0);
					GRect Clip11 = Clip00.Move(Clip10.x1, r.Y() - Clip00.Y());
					
					#if MAC_DBL_BUF
					// Erase the areas we will paint over
					pDC->Op(GDC_SET);
					pDC->Colour(pScreen->SupportsAlphaCompositing() ? GColour(0, 32) : NoPaint);
					pDC->Rectangle(&Clip10);
					pDC->Colour(pScreen->SupportsAlphaCompositing() ? GColour(0, 32) : d->cFill);
					pDC->Rectangle(&Clip11);
					#endif
					
					// Draw in the rounded corners
					pDC->Op(GDC_ALPHA);
					pDC->Blt(Clip10.x1, Clip10.y1, d->Corners[Res], Clip00.Move(MAC_STYLE_RADIUS, 0));
					pDC->Blt(Clip11.x1, Clip11.y1, d->Corners[Res], Clip00.Move(MAC_STYLE_RADIUS, MAC_STYLE_RADIUS));
				}

				#if MAC_DBL_BUF
				if (First || Last)
				{
					if (pScreen->SupportsAlphaCompositing())
						pScreen->Op(GDC_ALPHA);
					pScreen->Blt(r.x1, r.y1, &Mem);
				}
				#endif
				pDC = pScreen;

			#endif
			
			GFont *tf = ds->GetFont();
			double TfAscent = tf->Ascent();
			int BaselineOff = (int) (d->TabsBaseline - tf->Ascent());
			tf->Transparent(true);

			GCss::ColorDef Fore;
			if (Tab->GetCss())
				Fore = Tab->GetCss()->Color();
			tf->Fore(Fore.IsValid() ? (GColour)Fore : 
					IsCurrent && Foc ? cFocusFore : GColour(LC_TEXT, 24));
			ds->Draw(pDC, r.x1 + (r.X() - ds->X()) / 2, r.y1 + TAB_TXT_PAD + BaselineOff, &r);
			
			it[i]->TabPos = r;
			x += r.X()
				#ifdef LGI_CARBON
				+ (i ? -1 : 2) // Fudge factor to make it look nice, wtf apple?
				#endif
				;
		}
		
		#if 0
		pDC->Colour(GColour::Green);
		pDC->Line(0, 0, pDC->X()-1, pDC->Y()-1);
		#endif
	}
	else if (d->Style == TvWinXp)
	{
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
	}
	else LgiAssert(!"Not impl.");
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
				(void*)a->Handle(),
				(void*)b->Handle());
	}
	else
	{
		Str[0] = 0;
	}
	return Str;
}

GTabPage::GTabPage(const char *name) : ResObject(Res_Tab)
{
	d = new GTabPagePriv(this);

	GRect r(0, 0, 1000, 1000);
	SetPos(r);
	Name(name);
	Button = false;
	TabCtrl = 0;
	TabPos.ZOff(-1, -1);
	BtnPos.ZOff(-1, -1);
	GetCss(true)->Padding("4px");

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

	LgiResources::StyleElement(this);
}

GTabPage::~GTabPage()
{
	delete d;
}

int GTabPage::GetTabPx()
{
	GDisplayString *Ds = d->GetDs();

	int Px = TAB_MARGIN_X << 1;
	if (Ds)
	{
		Px += Ds->X();
		if (Button)
			Px += CLOSE_BTN_GAP + CLOSE_BTN_SIZE;
	}
	
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
	d->Ds.Reset();
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
		pDC->Line(r.x1, r.y1+1, r.x1, r.y2);
	else
		pDC->Line(r.x1, r.y1+1, r.x1, r.y2-1);
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
	if (TabCtrl && TabCtrl->d->Style == TvMac)
		return GColour(226, 226, 226); // 207?
	else
		return GColour(LC_MED, 24);
}

void GTabPage::OnStyleChange()
{
	d->Ds.Reset();
}

void GTabPage::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	GColour Bk = StyleColour(GCss::PropBackgroundColor, TabCtrl ? TabCtrl->d->cFill : GColour(LC_MED, 24), 1);
	pDC->Colour(Bk);
	pDC->Rectangle(&r);
	
	#if 0
	pDC->Colour(GColour::Red);
	pDC->Line(0, 0, pDC->X()-1, pDC->Y()-1);
	#endif
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

	/* This isn't needed if the controls properly inherit colours.
	if (TabCtrl && TabCtrl->d->Style == TvMac)
		// Sigh
		for (auto c : Children)
			c->GetCss(true)->BackgroundColor(GCss::ColorDef(GetBackground()));
	*/

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

	
