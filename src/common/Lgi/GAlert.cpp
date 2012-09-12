// Lgi.cpp
#include "Lgi.h"
#include "GTextLabel.h"
#include "GButton.h"

//////////////////////////////////////////////////////////////////////////////
#define CMD_BASE		100

GAlert::GAlert(	GViewI *parent,
				const char *Title,
				const char *Text,
				const char *Btn1,
				const char *Btn2,
				const char *Btn3)
{
	GText *t = 0;
	Children.Insert(t = new GText(-1, 8, 8, -1, -1, (char*)Text));
	if (t)
	{
		// Setup dialog
		SetParent(parent);
		Name((char*)Title);

		List<GButton> Btns;
		List<const char> Names;
		if (Btn1) Names.Insert(Btn1);
		if (Btn2) Names.Insert(Btn2);
		if (Btn3) Names.Insert(Btn3);
		int i = 1, Tx = 0;
		for (const char *n=Names.First(); n; n=Names.Next())
		{
			GDisplayString ds(SysFont, (char*)n);
			int x = ds.X();
			GButton *v;
			Btns.Insert(v = new GButton(CMD_BASE + i++, 0, 0, 30 + x, 20, (char*)n));
			Tx += v->X() + ((i>1) ? 10 : 0);
		}
		
		int x = LgiApp->GetMetric(LGI_MET_DECOR_X) + 16;
		int y = LgiApp->GetMetric(LGI_MET_DECOR_Y) + 20 + 8 + 16;
		GRect r;
		if (t)
		{
			x += max(Tx, t->X());
			y += t->Y();
			r.ZOff(x, y);
		}
		SetPos(r);
		MoveToCenter();

		// Setup controls
		int Cx = X() / 2;
		int Bx = Cx - (Tx / 2);
		for (GButton *b=Btns.First(); b; b=Btns.Next())
		{
			GRect r;
			r.ZOff(b->X()-1, b->Y()-1);
			r.Offset(Bx, t->GetPos().y2 + 8);
			b->SetPos(r);
			Children.Insert(b);
			Bx += b->X() + 10;
		}
	}
}

void GAlert::SetAppModal()
{
    #if WIN32NATIVE
    SetExStyle(GetExStyle() | WS_EX_TOPMOST);
    #elif !defined(_MSC_VER)
    #warning "Impl me."
    #endif
}

int GAlert::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case CMD_BASE+1:
		case CMD_BASE+2:
		case CMD_BASE+3:
		{
			EndModal(Ctrl->GetId() - CMD_BASE);
			break;
		}
	}

	return 0;
}
