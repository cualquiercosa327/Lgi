/*
**	FILE:		GuiViews.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		18/7/98
**	DESCRIPTION:	Standard Views
**
**	Copyright (C) 1998, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GScrollBar.h"

//////////////////////////////////////////////////////////////////////////////
GLayout::GLayout() : GView(0)
{
	_SettingScrollBars = 0;
	_PourLargest = 0;
	VScroll = 0;
	HScroll = 0;

    #if WINNATIVE
	SetExStyle(GetExStyle() | WS_EX_CONTROLPARENT);
	#endif
}

GLayout::~GLayout()
{
	DeleteObj(VScroll);
	DeleteObj(HScroll);
}

GViewI *GLayout::FindControl(int Id)
{
	if (VScroll && VScroll->GetId() == Id)
		return VScroll;
	if (HScroll && HScroll->GetId() == Id)
		return HScroll;

	return GView::FindControl(Id);
}

void GLayout::GetScrollPos(int &x, int &y)
{
	if (HScroll)
	{
		x = HScroll->Value();
	}
	else
	{
		x = 0;
	}

	if (VScroll)
	{
		y = VScroll->Value();
	}
	else
	{
		y = 0;
	}
}

void GLayout::SetScrollPos(int x, int y)
{
	if (HScroll)
	{
		HScroll->Value(x);
	}

	if (VScroll)
	{
		VScroll->Value(y);
	}
}

bool GLayout::SetScrollBars(bool x, bool y)
{
	bool Status = false;

	if (!_SettingScrollBars)
	{
		_SettingScrollBars = true;

		if (y)
		{
			if (!VScroll)
			{
				VScroll = new GScrollBar;
				if (VScroll)
				{
					VScroll->SetVertical(true);
					VScroll->SetParent(this);
					VScroll->SetId(IDC_VSCROLL);
					Status = true;
				}
			}
		}
		else
		{
			if (VScroll)
			{
				DeleteObj(VScroll);
				Status = true;
			}
		}

		if (x)
		{
			if (!HScroll)
			{
				HScroll = new GScrollBar;
				if (HScroll)
				{
					HScroll->SetVertical(false);
					HScroll->SetParent(this);
					HScroll->SetId(IDC_HSCROLL);
					Status = true;
				}
			}
		}
		else
		{
			if (HScroll)
			{
				DeleteObj(HScroll);
				Status = true;
			}
		}

		_SettingScrollBars = false;
	}

	return Status;
}

bool GLayout::GetPourLargest()
{
	return _PourLargest;
}

void GLayout::SetPourLargest(bool i)
{
	_PourLargest = i;
}

GCss::Len &SelectValid(GCss::Len &a, GCss::Len &b, GCss::Len &c)
{
	if (a.IsValid()) return a;
	if (b.IsValid()) return b;
	return c;
}

bool GLayout::Pour(GRegion &r)
{
	if (!_PourLargest)
		return false;

	GRect *Largest = FindLargest(r);
	if (!Largest)
		return false;

	GRect p = *Largest;
	GCss *css = GetCss();
	if (css)
	{
		GCss::Len margin = css->Margin();
		GCss::Len s;
		GCss::Len zero;
		GFont *f = GetFont();
		
		s = css->MarginTop();
		p.x1 += SelectValid(s, margin, zero).ToPx(r.X(), f);
		
		s = css->MarginTop();
		p.y1 += SelectValid(s, margin, zero).ToPx(r.Y(), f);
		
		s = css->MarginRight();
		p.x2 -= SelectValid(s, margin, zero).ToPx(r.X(), f);
		
		s = css->MarginBottom();
		p.y2 -= SelectValid(s, margin, zero).ToPx(r.Y(), f);
	
		if ((s = css->Width()).IsValid())
			p.x2 = p.x1 + s.ToPx(r.X(), f) - 1;
		if ((s = css->Height()).IsValid())
			p.y2 = p.y1 + s.ToPx(r.Y(), f) - 1;
	}
	
	if (p.Valid())
	{	
		SetPos(p, true);
	}
	else
	{
		LgiAssert(0);
		return false;
	}

	return true;
}

GMessage::Result GLayout::OnEvent(GMessage *Msg)
{
	if (VScroll) VScroll->OnEvent(Msg);
	if (HScroll) HScroll->OnEvent(Msg);
	GMessage::Result Status = GView::OnEvent(Msg);
	if (MsgCode(Msg) == M_CHANGE &&
		Status == -1 &&
		GetParent())
	{
		Status = GetParent()->OnEvent(Msg);
	}
	return Status;
}


