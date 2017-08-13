#include "Lgi.h"
#include "GPopup.h"
#include "GDateTimeCtrls.h"
#include "GDisplayString.h"

GDatePopup::GDatePopup(GView *owner) : GPopup(owner)
{
	SetNotify(owner);
	SetParent(owner);
	Owner = owner;
	
	LDateTime Now;
	if (owner)
		Now.SetDate(owner->Name());
	else
		Now.SetNow();
	Mv.Set(&Now);

	GRect r(0, 0, 150, 130);
	SetPos(r);
}

GDatePopup::~GDatePopup()
{
	if (Owner)
	{
		Owner->OnChildrenChanged(this, false);
		Owner->Invalidate();
	}
}

LDateTime GDatePopup::Get()
{
	return Mv.Get();
}

void GDatePopup::Set(LDateTime &Ts)
{
	Mv.Set(&Ts);
}

void GDatePopup::OnPaint(GSurface *pDC)
{
	// Border
	GRect r = GetClient();
	LgiWideBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(LC_MED, 24);
	pDC->Line(r.x2, r.y1, r.x2, r.y2);
	pDC->Line(r.x1, r.y2, r.x2-1, r.y2);
	r.x2--;
	r.y2--;
	LgiThinBorder(pDC, r, DefaultSunkenEdge);

	// Layout
	Caption = r;
	Caption.y2 = Caption.y1 + SysFont->GetHeight() + 2;
	Prev = Next = Caption;
	Prev.x2 = Prev.x1 + Prev.Y();
	Next.x1 = Next.x2 - Next.Y();
	Date = r;
	Date.y1 = Caption.y2 + 1;

	// Caption bar
	r = Caption;
	LgiThinBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);
	char *Title = Mv.Title();
	if (Title)
	{
		SysFont->Transparent(true);
		SysFont->Colour(LC_TEXT, LC_MED);
		GDisplayString ds(SysFont, Title);
		ds.Draw(pDC, r.x1 + (r.X()-ds.X())/2, r.y1);
	}

	// Arrows
	int CntY = Prev.y1 + ((Prev.Y()-8)/2);
	int Px = Prev.x2 - 6;
	int Nx = Next.x1 + 4;
	pDC->Colour(LC_TEXT, 24);
	for (int i=0; i<5; i++)
	{
		pDC->Line(	Px-i, CntY+i,
					Px-i, CntY+8-i);
		pDC->Line(	Nx+i, CntY+i,
					Nx+i, CntY+8-i);
	}

	// Date space
	pDC->Colour(LC_WORKSPACE, 24);
	pDC->Rectangle(&Date);

	Date.Size(3, 3);
	r = Date;
	Cx = r.X() / Mv.X();
	Cy = r.Y() / Mv.Y();
	for (int y=0; y<Mv.Y(); y++)
	{
		for (int x=0; x<Mv.X(); x++)
		{
			int Px = x * Cx;
			int Py = y * Cy;
			Mv.SelectCell(x, y);

			if (Mv.IsSelected())
			{
				pDC->Colour(LC_FOCUS_SEL_BACK, 24);
				pDC->Rectangle(r.x1 + Px, r.y1 + Py, r.x1 + Px + Cx - 2, r.y1 + Py + Cy - 2);
				SysFont->Colour(LC_FOCUS_SEL_FORE, LC_FOCUS_SEL_BACK);
			}
			else
			{
				if (Mv.IsToday())
				{
					SysFont->Colour(Rgb24(192, 0, 0), LC_WORKSPACE);
				}
				else
				{
					SysFont->Colour(Mv.IsMonth() ? LC_TEXT : LC_MED, LC_WORKSPACE);
				}
			}

			GDisplayString ds(SysFont, Mv.Day());
			ds.Draw(pDC, r.x1 + Px + 2, r.y1 + Py + 2);
		}
	}
}

void GDatePopup::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Next.Overlap(m.x, m.y))
		{
			LDateTime &c = Mv.Get();
			c.AddMonths(1);
			Mv.Set(&c);
			Invalidate();
		}
		else if (Prev.Overlap(m.x, m.y))
		{
			LDateTime &c = Mv.Get();
			c.AddMonths(-1);
			Mv.Set(&c);
			Invalidate();
		}
		else if (Date.Overlap(m.x, m.y))
		{
			int x = (m.x - Date.x1) / Cx;
			int y = (m.y - Date.y1) / Cy;
			Mv.SetCursor(x, y);
			Invalidate();
		}
	}
	else
	{
		if (Date.Overlap(m.x, m.y))
		{
			GViewI *n = GetNotify() ? GetNotify() : GetParent();
			if (n)
			{
				char s[64];
				Mv.Get().GetDate(s, sizeof(s));
				n->Name(s);
				n->SendNotify(GNotifyValueChanged);
			}
			Visible(false);
			return;
		}
	}

	GPopup::OnMouseClick(m);
}

void GDatePopup::Move(int Dx, int Dy)
{
	int x = 0, y = 0;
	Mv.GetCursor(x, y);
	Mv.SetCursor(x + Dx, y + Dy);
	Invalidate();
}

bool GDatePopup::OnKey(GKey &k)
{
	switch (k.c16)
	{
		case VK_ESCAPE:
		{
			if (!k.Down())
			{
				Visible(false);
			}
			return true;
			break;
		}
		case VK_UP:
		{
			if (k.Down())
			{
				Move(0, -1);
			}
			return true;
			break;
		}
		case VK_DOWN:
		{
			if (k.Down())
			{
				Move(0, 1);
			}
			return true;
			break;
		}
		case VK_LEFT:
		{
			if (k.Down())
			{
				Move(-1, 0);
			}
			return true;
			break;
		}
		case VK_RIGHT:
		{
			if (k.Down())
			{
				Move(1, 0);
			}
			return true;
			break;
		}
		case VK_RETURN:
		{
			if (k.Down() && k.IsChar)
			{
				if (Owner)
				{
					Owner->SendNotify(GNotifyValueChanged);
					// Popup->SetDate(Mv.Date(true));
				}
				Visible(false);
			}

			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
GDateDropDown::GDateDropDown() : ResObject(Res_Custom), GDropDown(-1, 0, 0, 10, 10, 0)
{
	DateSrc = 0;
	SetPopup(Drop = new GDatePopup(this));
}

int GDateDropDown::OnNotify(GViewI *Wnd, int Flags)
{
	if (Wnd == (GViewI*)Drop)
	{
		char s[256];
		LDateTime Ts = Drop->Get();
		Ts.Get(s, sizeof(s));
		SetDate(s);
	}
	
	return GDropDown::OnNotify(Wnd, Flags);
}

void GDateDropDown::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (Wnd == (GViewI*)Drop && !Attaching)
	{
		Drop = NULL;
	}
}

bool GDateDropDown::OnLayout(GViewLayoutInfo &Inf)
{
    if (!Inf.Width.Max)
    {
        Inf.Width.Min = Inf.Width.Max = 20;
    }
    else if (!Inf.Height.Max)
    {
        Inf.Height.Min = Inf.Height.Max = SysFont->GetHeight() + 6;
    }
    else return false;
    
    return true;
}

void GDateDropDown::SetDate(char *d)
{
	GViewI *n = GetNotify();
	if (n && d)
	{
		LDateTime New;
		char *Old = n->Name();
		if (Old)
		{
			New.Set(Old);
		}
		else
		{
			New.SetTime("12:0:0");
		}

		New.SetDate(d);
			
		char Buf[256];
		New.Get(Buf, sizeof(Buf));

		n->Name(Buf);

		GViewI *Nn = n->GetNotify() ? n->GetNotify() : n->GetParent();
		if (Nn)
		{
			Nn->OnNotify(n, 0);
		}
	}
}

void GDateDropDown::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Drop)
		{
			// Set it's date from our current value
			GViewI *n = GetNotify();
			if (n)
			{
				LDateTime New;
				char *Old = n->Name();
				if (!ValidStr(Old) && DateSrc)
				{
					Old = DateSrc->Name();
				}
				if (Old &&
					New.Set(Old))
				{
					Drop->Set(New);
				}
			}
		}
	}

	GDropDown::OnMouseClick(m);
}

class GDatePopupFactory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (Class &&
			_stricmp(Class, "GDateDropDown") == 0)
		{
			return new GDateDropDown;
		}

		return 0;
	}
} DatePopupFactory;
