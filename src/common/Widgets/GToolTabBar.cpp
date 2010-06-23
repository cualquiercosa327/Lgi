/*hdr
**      FILE:           ToolTabBar.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           22/3/2000
**      DESCRIPTION:    Toolbar of tabs
**
**      Copyright (C) 1997-1998 Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GToolTabBar.h"

/////////////////////////////////////////////////////////////////////////
GToolTab::GToolTab()
	: GToolButton(16, 16)
{
}

GToolTab::~GToolTab()
{
}

void GToolTab::OnPaint(GSurface *pDC)
{
	#ifndef WIN32
	GToolTabBar *p = dynamic_cast<GToolTabBar*>(GetParent());
	if (p)
	{
		p->_PaintTab(pDC, this);
	}
	#endif
}

/////////////////////////////////////////////////////////////////////////
GToolTabBar::GToolTabBar()
{
	GRect r(0, 0, 100, 100);
	SetPos(r);
	Current = 0;
	FitToArea = false;
	Border = true;
	
	#ifdef BEOS
	GetBView()->SetViewColor(216,216,216);
	#endif	
}

GToolTabBar::~GToolTabBar()
{
}

void GToolTabBar::OnButtonClick(GToolButton *Btn)
{
	GToolBar::OnButtonClick(Btn);
	OnChange(Btn);
}

void GToolTabBar::OnChange(GToolButton *Btn)
{
	// Clear out any current controls
	if (Current)
	{
		for (GView *w = Current->Attached.First(); w; w = Current->Attached.First())
		{
			Current->Attached.Delete(w);
			w->Detach();
			delete w;
		}

		Current = 0;
	}

	// Attach new button
	if (Btn)
	{
		Current = dynamic_cast<GToolTab*>(Btn);
		if (Current AND Current->AttachControls(this))
		{
			for (GViewI *w = Current->Children.First(); w; w = Current->Children.First())
			{
				GRect r = w->GetPos();
				r.Offset(Client.x1, Client.y1 + 2);
				w->SetPos(r);

				w->Attach(this);

				Current->Attached.Insert(dynamic_cast<GView*>(w));
				Current->Children.Delete();
			}
		}
	}
}

void GToolTabBar::_PaintTab(GSurface *pDC, GToolTab *Tab)
{
	#ifndef WIN32
	GRect t(0, 0, Tab->X()-1, Tab->Y()-1);
	#else
	GRect t = Tab->GetPos();
	#endif
	
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&t);
	
	if (!Tab->GetDown())
	{
		t.Size(2, 2);
		if (IsVertical())
		{
			t.x2++;
		}
		else
		{
			t.y2++;
		}
	}
	else
	{
		pDC->Colour(LC_MED, 24);
		if (IsVertical())
		{
			pDC->Rectangle(t.x1+1, t.y1+1, t.x2, t.y2-1);
		}
		else
		{
			pDC->Rectangle(t.x1+1, t.y1+1, t.x2-1, t.y2);
		}
	}

	// draw lines around tab
	pDC->Colour(LC_LIGHT, 24);
	if (IsVertical())
	{
		pDC->Line(t.x1, t.y1+1, t.x1, t.y2-1);
		pDC->Line(t.x1+1, t.y1, t.x2, t.y1);
		pDC->Colour(LC_LOW, 24);
		pDC->Line(t.x1+1, t.y2, t.x2, t.y2);
	}
	else
	{
		pDC->Line(t.x1+1, t.y1, t.x2-1, t.y1);
		pDC->Line(t.x1, t.y1+1, t.x1, t.y2);
		pDC->Colour(LC_LOW, 24);
		pDC->Line(t.x2, t.y1+1, t.x2, t.y2);
	}

	// draw icon
	int Off = Tab->GetDown() ? 2 : 1;
	GetImageList()->Draw(pDC, t.x1 + Off, t.y1 + Off, Tab->Image(), 0);
}

void GToolTabBar::OnPaint(GSurface *pDC)
{
	if (GetImageList())
	{
		GRect r(0, 0, GView::X()-1, GView::Y()-1);
		int Off = 4 + ((Border) ? 6 : 0);

		if (Border)
		{
			// Draw border
			LgiThinBorder(pDC, r, RAISED);
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(&r);
			r.Size(5, 5);
		}
		else
		{
			pDC->Colour(LC_MED, 24);
			if (IsVertical())
			{
				pDC->Rectangle(r.x1, r.y1, r.x1 + GetBx() + Off, r.y2);
			}
			else
			{
				pDC->Rectangle(r.x1, r.y1, r.x2, r.y1 + GetBy() + Off);
			}
		}

		// Draw rest of the box
		if (IsVertical())
		{
			r.x1 = GetBx() + Off;
		}
		else
		{
			r.y1 = GetBy() + Off;
		}
		LgiThinBorder(pDC, r, RAISED);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);

		// Draw tabs
		GViewI *w;
		for (w = Children.Last(); w; w = Children.Prev())
		{
			GToolTab *Tab = dynamic_cast<GToolTab*>(w);
			if (Tab AND !Tab->GetDown())
			{
				Tab->SetPos(Tab->GetPos());
				_PaintTab(pDC, Tab);
			}
		}
		for (w = Children.Last(); w; w = Children.Prev())
		{
			GToolTab *Tab = dynamic_cast<GToolTab*>(w);
			if (Tab)
			{
				if (Tab->GetDown())
				{
					Tab->SetPos(Tab->GetPos());
					_PaintTab(pDC, Tab);
				}
			}	
		}
	}	
}

bool GToolTabBar::Pour(GRegion &r)
{
	bool Status = false;

	if (GetImageList())
	{
		Tab.x1 = Tab.y1 = Tab.x2 = Tab.y2 = (Border) ? 6 : 0;
		int x = (IsVertical()) ? Tab.y1 : Tab.x1;
		int y = (IsVertical()) ? Tab.x1 : Tab.y1;
		
		for (GViewI *w = Children.First(); w; w = Children.Next())
		{
			GToolButton *Btn = dynamic_cast<GToolButton*>(w);
			if (Btn)
			{
				if (Btn->GetId() == IDM_SEPARATOR)
				{
					if (IsVertical())
					{
						y += 10;
					}
					else
					{
						x += 10;
					}
				}
				else
				{
					if (Btn->GetId() == IDM_BREAK)
					{
						if (IsVertical())
						{
							x += GetBx() + 4;
							y = Tab.y1;
						}
						else
						{
							x =  Tab.x1;
							y += GetBy() + 4;
						}
					}

					GRect Rgn(x, y, x + GetBx() + 4, y + GetBy() + 4);
					Btn->SetPos(Rgn);
					if (IsVertical())
					{
						y += GetBy() + 1;
					}
					else
					{
						x += GetBx() + 1;
					}
				}

			}
		}

		if (IsVertical())
		{
			Tab.x2 = x + ((Border) ? 12 : 0);
			Tab.y2 = y + 50;
		}
		else
		{
			Tab.x2 = x + 50;
			Tab.y2 = y + ((Border) ? 12 : 0);
		}

		GRect *Best = FindLargest(r);
		if (Best)
		{
			GRect p;
			if (FitToArea)
			{
				p.Set(0, 0, Best->X()-1, Best->Y()-1);
			}
			else
			{
				if (IsVertical())
				{
					p.Set(0, 0, 200, Tab.Y());
				}
				else
				{
					p.Set(0, 0, Tab.X(), 200);
				}
			}

			Client = GetClient();
			if (IsVertical())
			{
				Client.x1 += GetBx() + 4;
			}
			else
			{
				Client.y1 += GetBx() + 4;
			}

			p.Offset(Best->x1, Best->y1);
			p.Bound(Best);
			SetPos(p);

			Status = true;
		}
	}

	return Status;
}

int GToolTabBar::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Current)
	{
		return Current->OnNotify(Ctrl, Flags);
	}

	return 0;
}

void GToolTabBar::OnCreate()
{
	GToolButton *Btn = dynamic_cast<GToolButton*>(Children.First());
	if (Btn)
	{
		Btn->Value(true);
		OnChange(Btn);
	}
}
