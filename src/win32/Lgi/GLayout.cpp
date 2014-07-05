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

bool GLayout::Pour(GRegion &r)
{

	if (_PourLargest)
	{
		GRect *Largest = FindLargest(r);
		if (Largest)
		{
			GRect p = *Largest;
			if (GetCss())
			{
				GCss::Len sz = GetCss()->Width();
				if (sz.IsValid())
					p.x2 = p.x1 + sz.ToPx(r.X(), GetFont()) - 1;
				sz = GetCss()->Height();
				if (sz.IsValid())
					p.y2 = p.y1 + sz.ToPx(r.Y(), GetFont()) - 1;
			}
			if (p.Valid())
			{	
				SetPos(p, true);
				return true;
			}
			else LgiAssert(0);
		}
	}

	return false;
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

