#include <stdio.h>

#include "Lgi.h"
#include "GColour.h"

class GColourPopup : public GPopup
{
	GColour *Colour;

public:
	GColourPopup(GColour *Parent) : GPopup(Parent)
	{
		SetParent(Colour = Parent);
		GRect r(0, 0, 100, 16 + 4);
		SetPos(r);
	}

	void OnPaint(GSurface *pDC)
	{
		GRect r = GetClient();
		LgiWideBorder(pDC, r, RAISED);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);

		SysFont->Transparent(true);
		if (Colour->Presets.Length())
		{
			char s[64];
			
			SysFont->Colour(LC_BLACK, LC_MED);
			GDisplayString ds(SysFont, LgiLoadString(L_COLOUR_NONE, "No Colour"));
			ds.Draw(pDC, r.x1 + 2, r.y1 + 2);

			for (int i=0; i<Colour->Presets.Length(); i++)
			{
				int y = r.y1 + ((i+1) * 16);
				COLOUR p32 = Colour->Presets[i];

				pDC->Colour(p32, 32);
				pDC->Rectangle(r.x1+1, y+1, r.x1 + 15, y+15);
				
				sprintf(s, "%02.2X,%02.2X,%02.2X", R32(p32), G32(p32), B32(p32));
				GDisplayString ds(SysFont, s);
				ds.Draw(pDC, r.x1 + 20, y + 2);
			}
		}
	}

	void OnMouseClick(GMouse &m)
	{
		GRect r = GetClient();
		r.Size(2, 2);
		if (r.Overlap(m.x, m.y))
		{
			int i = m.y / 16;
			if (i == 0)
			{
				Colour->Value(0);
			}
			else
			{
				Colour->Value(Colour->Presets[i-1]);
			}

			Visible(false);
		}
	}

	int64 Value() { return Colour->Value(); }
	void Value(int64 i) { Colour->Value(i); }
};

GColour::GColour(GArray<COLOUR> *col32) :
	ResObject(Res_Custom), GDropDown(-1, 0, 0, 10, 10, 0)
{
	c32 = Rgb32(0, 0, 255);

	SetPopup(new GColourPopup(this));
	if (col32)
		SetColourList(col32);
}

void GColour::SetColourList(GArray<COLOUR> *col32)
{
	if (col32)
	{
		Presets = *col32;
	}

	if (GetPopup())
	{
		GRect r(0, 0, 100, (max(Presets.Length()+1, 1) * 16) + 4);
		GetPopup()->SetPos(r);
	}
}

int64 GColour::Value()
{
	return c32;
}

void GColour::Value(int64 i)
{
	if (c32 != (COLOUR)i)
	{
		c32 = (COLOUR)i;
		Invalidate();

		GViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
		{
			n->OnNotify(this, 0);
		}
	}
}

void GColour::OnPaint(GSurface *pDC)
{
	GDropDown::OnPaint(pDC);

	GRect r = GetClient();
	r.x2 -= 9;
	r.Size(5, 5);
	if (IsOpen()) r.Offset(1, 1);

	bool HasColour = Enabled() AND c32 != 0;
	pDC->Colour(HasColour ? LC_BLACK : LC_LOW, 24);
	pDC->Box(&r);

	r.Size(1, 1);
	pDC->Colour(HasColour ? c32 : Rgb24To32(LC_LOW), 32);
	if (HasColour)
	{
		pDC->Rectangle(&r);
	}
	else
	{
		pDC->Line(r.x1, r.y1, r.x2, r.y2);
		pDC->Line(r.x1, r.y2, r.x2, r.y1);
	}
}

class GColourFactory : public GViewFactory
{
	GView *NewView(char *Class, GRect *Pos, char *Text)
	{
		if (Class AND
			stricmp(Class, "GColour") == 0)
		{
			return new GColour;
		}

		return 0;
	}

} ColourFactory;
