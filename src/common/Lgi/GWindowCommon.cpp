//
//  GWindowCommon.cpp
//  LgiCarbon
//
//  Created by Matthew Allen on 11/09/14.
//
//
#include "Lgi.h"

void GWindow::MoveOnScreen()
{
	GRect p = GetPos();
	GArray<GDisplayInfo*> Displays;
	GRect Screen(0, 0, -1, -1);

	if (
		#if WINNATIVE
		!IsZoomed(Handle()) &&
		!IsIconic(Handle()) &&
		#endif
		LgiGetDisplays(Displays))
	{
		int Best = -1;
		int Pixels = 0;
		int Close = 0x7fffffff;
		for (int i=0; i<Displays.Length(); i++)
		{
			GRect o = p;
			o.Bound(&Displays[i]->r);
			if (o.Valid())
			{
				int Pix = o.X()*o.Y();
				if (Best < 0 || Pix > Pixels)
				{
					Best = i;
					Pixels = Pix;
				}
			}
			else if (Pixels == 0)
			{
				int n = Displays[i]->r.Near(p);
				if (n < Close)
				{
					Best = i;
					Close = n;
				}
			}
		}

		if (Best >= 0)
			Screen = Displays[Best]->r;
	}

	if (!Screen.Valid())
		Screen.Set(0, 0, GdcD->X()-1, GdcD->Y()-1);

	if (p.x2 >= Screen.x2)
		p.Offset(Screen.x2 - p.x2, 0);
	if (p.y2 >= Screen.y2)
		p.Offset(0, Screen.y2 - p.y2);
	if (p.x1 < Screen.x1)
		p.Offset(Screen.x1 - p.x1, 0);
	if (p.y1 < Screen.y1)
		p.Offset(0, Screen.y1 - p.y1);

	SetPos(p, true);

	Displays.DeleteObjects();
}

void GWindow::MoveToCenter()
{
	GRect Screen(0, 0, GdcD->X()-1, GdcD->Y()-1);
	GRect p = GetPos();

	p.Offset(-p.x1, -p.y1);
	p.Offset((Screen.X() - p.X()) / 2, (Screen.Y() - p.Y()) / 2);

	SetPos(p, true);
}

void GWindow::MoveToMouse()
{
	GMouse m;
	if (GetMouse(m, true))
	{
		GRect p = GetPos();

		p.Offset(-p.x1, -p.y1);
		p.Offset(m.x-(p.X()/2), m.y-(p.Y()/2));

		SetPos(p, true);
		MoveOnScreen();
	}
}

bool GWindow::MoveSameScreen(GViewI *wnd)
{
	if (!wnd)
	{
		LgiAssert(0);
		return false;
	}
	
	GRect p = wnd->GetPos();
	int cx = p.x1 + (p.X() >> 2);
	int cy = p.y1 + (p.Y() >> 2);
	
	GRect np = GetPos();
	np.Offset(cx - np.x1, cy - np.y1);
	SetPos(np);
	
	MoveOnScreen();
	return true;
}
