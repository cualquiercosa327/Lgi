/*
**	FILE:			GPanel.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			29/8/99
**	DESCRIPTION:	Scribe Mail Object and UI
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/
#include <stdio.h>
#include "Lgi.h"
#include "GPanel.h"

//////////////////////////////////////////////////////////////////////////////
GPanel::GPanel(char *name, int size, bool open)
{
	Ds = 0;
	if (name) Name(name);
	IsOpen = open;
	_IsToolBar = !IsOpen;
	ClosedSize = SysFont->GetHeight() + 3;
	OpenSize = size;
	Align = GV_EDGE_TOP;
	
	_BorderSize = 1;
	Raised(true);
}

GPanel::~GPanel()
{
	DeleteObj(Ds);
}

int GPanel::CalcWidth()
{
	if (!Ds)
		Ds = new GDisplayString(GetFont(), Name());

	return 30 + (Ds ? Ds->X() : 0);
}

bool GPanel::Open()
{
	return IsOpen;
}

void GPanel::Open(bool i)
{
	if (i != IsOpen)
	{
		IsOpen = i;
		_IsToolBar = !IsOpen;
		SetChildrenVisibility(IsOpen);
		RePour();

		/*
		int ExStyle = GetWindowLong(Handle(), GWL_EXSTYLE);
		SetWindowLong(	Handle(),
						GWL_EXSTYLE,
						(ExStyle & ~WS_EX_CONTROLPARENT) | (IsOpen ? WS_EX_CONTROLPARENT : 0));
		*/
	}
}

int GPanel::Alignment()
{
	return Align;
}

void GPanel::Alignment(int i)
{
	Align = i;
}

int GPanel::GetClosedSize()
{
	return ClosedSize;
}

void GPanel::SetClosedSize(int i)
{
	ClosedSize = i;
}

int GPanel::GetOpenSize()
{
	return OpenSize;
}

void GPanel::SetOpenSize(int i)
{
	OpenSize = i;
}

bool GPanel::Attach(GViewI *Wnd)
{
	bool Status = GLayout::Attach(Wnd);
	if (Status)
	{
		AttachChildren();
	}

	SetChildrenVisibility(IsOpen);
	return Status;
}

bool GPanel::Pour(GRegion &r)
{
	int Sx = CalcWidth();
	GRect *Best = 0;
	if (Open())
	{
		Best = FindLargest(r);
	}
	else
	{
		Best = FindSmallestFit(r, Sx, ClosedSize);
		if (!Best)
		{
			Best = FindLargest(r);
		}
	}

	if (Best)
	{
		GRect r = *Best;
		if (OpenSize > 0)
		{
			int Size = ((Open()) ? OpenSize : ClosedSize);
			int Limit = 30;
			if (TestFlag(Align, GV_EDGE_RIGHT) OR
				TestFlag(Align, GV_EDGE_LEFT))
			{
				Limit = min(Size, r.X()-1);
			}
			else /* if (TestFlag(Align, GV_EDGE_BOTTOM) OR
					 TextFlag(Align, GV_EDGE_TOP)) */
			{
				Limit = min(Size, r.Y()-1);
			}
			
			if (Align & GV_EDGE_RIGHT)
			{
				r.x1 = r.x2 - Limit;
			}
			else if (Align & GV_EDGE_BOTTOM)
			{
				r.y1 = r.y2 - Limit;
			}
			else if (Align & GV_EDGE_LEFT)
			{
				r.x2 = r.x1 + Limit;
			}
			else // if (Align & GV_EDGE_TOP)
			{
				r.y2 = r.y1 + Limit;
			}
			
			if (!Open())
			{
				r.x2 = r.x1 + Sx - 1;
			}
		}
		else
		{
			r.y2 = r.y1 - OpenSize;
		}

		SetPos(r, true);

		return true;
	}
	return false;
}

int GPanel::OnNotify(GViewI *Ctrl, int Flags)
{
	if (GetParent())
	{
		return GetParent()->OnNotify(Ctrl, Flags);
	}

	return 0;
}

void GPanel::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();

	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);

	SysFont->Transparent(false);
	SysFont->Fore(0);
	SysFont->Back(LC_MED);
	if (!Open())
	{
		// title
		if (!Ds)
			Ds = new GDisplayString(GetFont(), Name());
		if (Ds)
			Ds->Draw(pDC, r.x1 + 20, r.y1 + 1);
	}

	if (OpenSize > 0)
	{
		// Draw thumb
		ThumbPos.ZOff(8, 8);
		ThumbPos.Offset(r.x1 + 3, r.y1 + 3);
		
		pDC->Colour(LC_LOW, 24);
		pDC->Box(&ThumbPos);
		pDC->Colour(LC_WHITE, 24);
		pDC->Rectangle(ThumbPos.x1+1, ThumbPos.y1+1, ThumbPos.x2-1, ThumbPos.y2-1);
		pDC->Colour(LC_SHADOW, 24);
		pDC->Line(	ThumbPos.x1+2,
					ThumbPos.y1+4,
					ThumbPos.x1+6,
					ThumbPos.y1+4);
		
		if (!Open())
		{
			pDC->Line(	ThumbPos.x1+4,
						ThumbPos.y1+2,
						ThumbPos.x1+4,
						ThumbPos.y1+6);
		}
	}
}

void GPanel::OnMouseClick(GMouse &m)
{
	if (OpenSize > 0 AND
		m.Left() AND
		m.Down() AND
		ThumbPos.Overlap(m.x, m.y))
	{
		Open(!IsOpen);
		if (GetParent())
		{
			OnNotify(this, 0);
		}
	}
}

void GPanel::RePour()
{
	GWindow *Top = dynamic_cast<GWindow*>(GetWindow());
	if (Top)
	{
		Top->Pour();
	}
}

void GPanel::SetChildrenVisibility(bool i)
{
	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		if (i AND
			!w->IsAttached())
		{
			w->Attach(this);
		}

		w->Visible(i);
	}
}

