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
GLayout::GLayout() :
	GView(new BViewRedir(this))
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
			SetPos(*Best, true);
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
	
	if (_View)
	{
		bool Lock = Handle()->LockLooper();
		if (HScroll)
		{
			if (HScroll->Handle()->Parent())
			{
				HScroll->Handle()->RemoveSelf();
			}
			
			Handle()->AddChild(HScroll->Handle());
		}
		
		if (VScroll)
		{
			if (VScroll->Handle()->Parent())
			{
				VScroll->Handle()->RemoveSelf();
			}
	
			Handle()->AddChild(VScroll->Handle());
		}
		if (Lock) Handle()->UnlockLooper();
	}
	
	return Status;
}

bool GLayout::SetScrollBars(bool x, bool y)
{
	if (( (HScroll!=0) ^ x ) OR
		( (VScroll!=0) ^ y ))
	{
		if (x)
		{
			if (NOT HScroll)
			{
				HScroll = new GScrollBar(IDC_HSCROLL, 0, 0, 100, 10, "GLayout->HScroll");
				if (HScroll)
				{
					HScroll->SetNotify(this);
					HScroll->Visible(false);
					
					Handle()->AddChild(HScroll->Handle());
					HScroll->SetParent(this);
				}
			}
		}
		else if (HScroll)
		{
			DeleteObj(HScroll);
		}
		if (y)
		{
			if (NOT VScroll)
			{
				VScroll = new GScrollBar(IDC_VSCROLL, 0, 0, 10, 100, "GLayout->VScroll");
				if (VScroll)
				{
					VScroll->SetNotify(this);
					VScroll->Visible(false);
					
					Handle()->AddChild(VScroll->Handle());
					VScroll->SetParent(this);
				}
			}
		}
		else if (VScroll)
		{
			DeleteObj(VScroll);
		}
		
		OnPosChange();
		Invalidate();
	}
	
	return true;
}

void GLayout::OnPosChange()
{
	GRect r = GView::GetClient(); // (0, 0, X()-1, Y()-1);
	// int Border = (Sunken() OR Raised()) ? _BorderSize : 0;
	// r.Size(Border, Border);
	
	GRect v(r.x2-B_V_SCROLL_BAR_WIDTH+1, r.y1, r.x2, r.y2);
	GRect h(r.x1, r.y2-B_H_SCROLL_BAR_HEIGHT+1, r.x2, r.y2);

	if (VScroll)
	{
		h.x2 = v.x1 - 1;
		VScroll->SetPos(v, true);
		VScroll->Visible(true);
	}
	
	if (HScroll)
	{
		HScroll->SetPos(h, true);
		HScroll->Visible(true);
	}
}

GRect &GLayout::GetClient(bool ClientSpace)
{
	static GRect r;

	r = GView::GetClient(ClientSpace);
	
	if (VScroll)
	{
		r.x2 = VScroll->GetPos().x1 - 1;
	}
	
	if (HScroll)
	{
		r.y2 = HScroll->GetPos().y1 - 1;
	}

	return r;
}

void GLayout::OnNcPaint(GSurface *pDC, GRect &r)
{
}

int GLayout::OnNotify(GViewI *v, int f)
{
	return GView::OnNotify(v, f);
}

int GLayout::OnEvent(BMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CHANGE:
		{
			if (GetParent())
			{
				GetParent()->OnEvent(Msg);
			}
			break;
		}
		case M_VSCROLL:
		{
			if (VScroll)
			{
				OnNotify(VScroll, MsgA(Msg));
			}
			break;
		}
		case M_HSCROLL:
		{
			if (HScroll)
			{
				OnNotify(HScroll, MsgA(Msg));
			}
			break;
		}
	}
	
	if (VScroll) VScroll->OnEvent(Msg);
	if (HScroll) HScroll->OnEvent(Msg);
	return GView::OnEvent(Msg);
}

