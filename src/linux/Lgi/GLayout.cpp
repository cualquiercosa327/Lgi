/*
**	FILE:			GLayout.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			18/7/1998
**	DESCRIPTION:	Standard Views
**
**	Copyright (C) 1998-2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GScrollBar.h"
#include "GNotifications.h"

#define M_SET_SCROLL		(M_USER + 0x2000)

//////////////////////////////////////////////////////////////////////////////
GLayout::GLayout()
{
	_PourLargest = false;
	VScroll = 0;
	HScroll = 0;
}

GLayout::~GLayout()
{
	DeleteObj(HScroll);
	DeleteObj(VScroll);
}

GViewI *GLayout::FindControl(int Id)
{
	if (VScroll && VScroll->GetId() == Id)
		return VScroll;
	if (HScroll && HScroll->GetId() == Id)
		return HScroll;

	return GView::FindControl(Id);
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
		GRect *Best = FindLargest(r);
		if (Best)
		{
			SetPos(*Best);
			return true;
		}
	}

	return false;
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

bool GLayout::Attach(GViewI *p)
{
	bool Status = GView::Attach(p);
	AttachScrollBars();
	return Status;
}

bool GLayout::Detach()
{
	return GView::Detach();
}

void GLayout::OnCreate()
{
	AttachScrollBars();
	OnPosChange();
}

void GLayout::AttachScrollBars()
{
	if (HScroll && !HScroll->IsAttached())
	{
		GRect r = HScroll->GetPos();
		HScroll->Attach(this);
		HScroll->SetNotify(this);
	}

	if (VScroll && !VScroll->IsAttached())
	{
		GRect r = VScroll->GetPos();
		VScroll->Attach(this);
		VScroll->SetNotify(this);
	}
}

bool GLayout::SetScrollBars(bool x, bool y)
{
	#ifdef M_SET_SCROLL
	if (x ^ (HScroll != NULL)
		||
		y ^ (VScroll != NULL))
	{
		return PostEvent(M_SET_SCROLL, x, y);
	}
	#else
	_SetScrollBars(x, y);
	#endif

	return true;
}

bool GLayout::_SetScrollBars(bool x, bool y)
{
	static bool Processing = false;

	if (!Processing &&
		(((HScroll!=0) ^ x ) || ((VScroll!=0) ^ y )) )
	{
		Processing = true;

		if (x)
		{
			if (!HScroll)
			{
				HScroll = new GScrollBar(IDC_HSCROLL, 0, 0, 100, 10, "GLayout->HScroll");
				if (HScroll)
				{
					HScroll->SetVertical(false);
					HScroll->Visible(false);
				}
			}
		}
		else
		{
			DeleteObj(HScroll);
		}
		if (y)
		{
			if (!VScroll)
			{
				VScroll = new GScrollBar(IDC_VSCROLL, 0, 0, 10, 100, "GLayout->VScroll");
				if (VScroll)
				{
					VScroll->Visible(false);
				}
			}
		}
		else if (VScroll)
		{
			DeleteObj(VScroll);
		}

		AttachScrollBars();
		OnPosChange();
		Invalidate();

		Processing = false;
	}
	
	return true;
}

int GLayout::OnNotify(GViewI *c, int f)
{
	return GView::OnNotify(c, f);
}

void GLayout::OnPosChange()
{
	GRect r = GView::GetClient();
	GRect v(r.x2-SCROLL_BAR_SIZE+1, r.y1, r.x2, r.y2);
	GRect h(r.x1, r.y2-SCROLL_BAR_SIZE+1, r.x2, r.y2);
	if (VScroll && HScroll)
	{
		h.x2 = v.x1 - 1;
		v.y2 = h.y1 - 1;
	}
	
	if (VScroll)
	{
		VScroll->Visible(true);
		VScroll->SetPos(v, true);
	}
	if (HScroll)
	{
		HScroll->Visible(true);
		HScroll->SetPos(h, true);
	}
}

void GLayout::OnNcPaint(GSurface *pDC, GRect &r)
{
	GView::OnNcPaint(pDC, r);
	
	if (VScroll && VScroll->Visible())
	{
		r.x2 = VScroll->GetPos().x1 - 1;
	}

	if (HScroll && HScroll->Visible())
	{
		r.y2 = HScroll->GetPos().y1 - 1;
	}
	
	if (VScroll && VScroll->Visible() &&
		HScroll && HScroll->Visible())
	{
		// Draw square at the end of each scroll bar
		GRect s(	VScroll->GetPos().x1, HScroll->GetPos().y1,
					VScroll->GetPos().x2, HScroll->GetPos().y2);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&s);
	}
}

GRect &GLayout::GetClient(bool ClientSpace)
{
	static GRect r;
	r = GView::GetClient(ClientSpace);

	if (VScroll && VScroll->Visible())
	{
		r.x2 = VScroll->GetPos().x1 - 1;
	}

	if (HScroll && HScroll->Visible())
	{
		r.y2 = HScroll->GetPos().y1 - 1;
	}
	
	return r;
}

GMessage::Param GLayout::OnEvent(GMessage *Msg)
{
	#ifdef M_SET_SCROLL
	if (MsgCode(Msg) == M_SET_SCROLL)
	{
		_SetScrollBars(Msg->A(), Msg->B());
		
		if (HScroll)
			HScroll->SendNotify(GNotifyScrollBar_Create);
		if (VScroll)
			VScroll->SendNotify(GNotifyScrollBar_Create);
		return 0;
	}
	#endif

	if (VScroll) VScroll->OnEvent(Msg);
	if (HScroll) HScroll->OnEvent(Msg);
	int Status = GView::OnEvent(Msg);
	if (MsgCode(Msg) == M_CHANGE &&
		Status == -1 &&
		GetParent())
	{
		Status = GetParent()->OnEvent(Msg);
	}

	return Status;
}

