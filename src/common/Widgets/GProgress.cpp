#include "Lgi.h"
#include "GProgress.h"

///////////////////////////////////////////////////////////////////////////////////////////
GProgress::GProgress(int id, int x, int y, int cx, int cy, char *name) :
	ResObject(Res_Progress)
{
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	if (name) GControl::Name(name);

	c = Rgb24(50, 150, 255);
}

GProgress::~GProgress()
{
}

void GProgress::SetLimits(int64 l, int64 h)
{
	Low = l;
	High = h;
}

void GProgress::Value(int64 v)
{
	if (Val != v)
	{
		Val = v;
		Invalidate();
	}
}

int64 GProgress::Value()
{
	return Val;
}

int GProgress::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GProgress::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	LgiThinBorder(pDC, r, SUNKEN);

	if (High != 0)
	{
		int Pos = (int) (((double) (r.X()-1) * (((double) Val - Low) / High)));
		if (Pos > 0)
		{
			COLOUR High = Rgb24( (R24(c)+255)/2, (G24(c)+255)/2, (B24(c)+255)/2 );
			COLOUR Low = Rgb24( (R24(c)+0)/2, (G24(c)+0)/2, (B24(c)+0)/2 );
			GRect p = r;

			p.x2 = p.x1 + Pos;
			r.x1 = p.x2 + 1;

			pDC->Colour(Low, 24);
			pDC->Line(p.x2, p.y2, p.x2, p.y1);
			pDC->Line(p.x2, p.y2, p.x1, p.y2);

			pDC->Colour(High, 24);
			pDC->Line(p.x1, p.y1, p.x2, p.y1);
			pDC->Line(p.x1, p.y1, p.x1, p.y2);

			p.Size(1, 1);

			GViewFill *f = GetBackgroundFill();
			if (f)
			{
				f->Fill(pDC, &p);
			}
			else
			{
				pDC->Colour(c, 24);
				pDC->Rectangle(&p);
			}
		}
	}

	if (r.Valid())
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
	}
}
