/*hdr
**	FILE:			GdcPrim.cpp
**	AUTHOR:		Matthew Allen
**	DATE:			1/3/97
**	DESCRIPTION:	GDC v2.xx device independent primitives
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Gdc2.h"
#include "GdiLeak.h"
#include "GPalette.h"
#include "GVariant.h"

#define LINE_SOLID				0xFFFFFFFF

void GSurface::Init()
{
	OriginX = OriginY = 0;
	pMem = NULL;
	pAlphaDC = 0;
	Flags = 0;
	Clip.ZOff(-1, -1);
	pPalette = NULL;
	pApp = NULL;
	ColourSpace = CsNone;

	for (int i=0; i<GDC_CACHE_SIZE; i++)
	{
		pAppCache[i] = 0;
	}

	LineBits = LINE_SOLID;
	LineMask = LineReset = 0x80000000;
	
	#if defined(__GTK_H__)
	Cairo = 0;
	#endif
}

GSurface::GSurface()
{
	Init();
}

GSurface::GSurface(GSurface *pDC)
{
	Init();
	if (pDC && Create(pDC->X(), pDC->Y(), pDC->GetColourSpace()))
	{
		Blt(0, 0, pDC);
		if (pDC->Palette())
		{
			GPalette *Pal = new GPalette(pDC->Palette());
			if (Pal)
			{
				Palette(Pal, true);
			}
		}
	}
}

GSurface::~GSurface()
{
	#if defined(LINUX) && !defined(LGI_SDL)
	if (Cairo)
	{
		Gtk::cairo_destroy(Cairo);
		Cairo = 0;
	}
	#endif

	DrawOnAlpha(false);

	DeleteObj(pMem);
	DeleteObj(pAlphaDC);

	if (pPalette && (Flags & GDC_OWN_PALETTE))
	{
		DeleteObj(pPalette);
	}

	if (	(Flags & GDC_OWN_APPLICATOR) &&
		!(Flags & GDC_CACHED_APPLICATOR))
	{
		DeleteObj(pApp);
	}

	for (int i=0; i<GDC_CACHE_SIZE; i++)
	{
		DeleteObj(pAppCache[i]);
	}
}

GSurface *GSurface::SubImage(GRect r)
{
	if (!pMem || !pMem->Base)
		return NULL;
		
	GRect clip = r;
	GRect bounds = Bounds();
	clip.Intersection(&bounds);
	if (!clip.Valid())
		return false;

	GAutoPtr<GSurface> s(new GSurface);
	if (!s)
		return NULL;
	
	int BytePx = pMem->GetBits() >> 3;
	s->pMem = new GBmpMem;
	s->pMem->Base = pMem->Base + (pMem->Line * clip.y1) + (BytePx * clip.x1);
	s->pMem->x = clip.X();
	s->pMem->y = clip.Y();
	s->pMem->Line = pMem->Line;
	s->pMem->Cs = pMem->Cs;
	s->pMem->Flags = 0; // Don't own memory.
	
	s->Clip = s->Bounds();
	s->ColourSpace = pMem->Cs;
	s->Op(GDC_SET);
	
	return s.Release();
}

OsBitmap GSurface::GetBitmap()
{
	#if WINNATIVE
	return hBmp;
	#else
	return 0;
	#endif
}

OsPainter GSurface::Handle()
{
	#if WINNATIVE
	return hDC;
	#else
	return 0;
	#endif
}

uchar *GSurface::operator[](int y)
{
	if (pMem &&
		pMem->Base &&
		y >= 0 &&
		y < pMem->y)
	{
		return pMem->Base + (pMem->Line * y);
	}
	
	return 0;
}

void GSurface::Set(int x, int y)
{
	OrgXy(x, y);
	if (x >= Clip.x1 &&
		y >= Clip.y1 &&
		x <= Clip.x2 &&
		y <= Clip.y2)
	{
		pApp->SetPtr(x, y);
		pApp->Set();
		Update(GDC_BITS_CHANGE);
	}
}

COLOUR GSurface::Get(int x, int y)
{
	OrgXy(x, y);

	if (x >= Clip.x1 &&
		y >= Clip.y1 &&
		x <= Clip.x2 &&
		y <= Clip.y2)
	{
		pApp->SetPtr(x, y);
		return pApp->Get();
	}
	return (COLOUR)-1;
}

void GSurface::HLine(int x1, int x2, int y)
{
	OrgXy(x1, y);
	OrgX(x2);

	if (x1 > x2) LgiSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (x1 <= x2 &&
		y >= Clip.y1 &&
		y <= Clip.y2)
	{
		pApp->SetPtr(x1, y);
		if (LineBits == LINE_SOLID)
		{
			pApp->Rectangle(x2 - x1 + 1, 1);
			Update(GDC_BITS_CHANGE);
		}
		else
		{
			for (; x1 <= x2; x1++)
			{
				if (LineMask & LineBits)
				{
					pApp->Set();
				}

				LineMask >>= 1;
				if (!LineMask) LineMask = LineReset;
				pApp->IncX();
			}
			Update(GDC_BITS_CHANGE);
		}
	}
}

void GSurface::VLine(int x, int y1, int y2)
{
	OrgXy(x, y1);
	OrgY(y2);

	if (y1 > y2) LgiSwap(y1, y2);
	if (y1 < Clip.y1) y1 = Clip.y1;
	if (y2 > Clip.y2) y2 = Clip.y2;
	if (y1 <= y2 &&
		x >= Clip.x1 &&
		x <= Clip.x2)
	{
		pApp->SetPtr(x, y1);
		if (LineBits == LINE_SOLID)
		{
			pApp->VLine(y2 - y1 + 1);
			Update(GDC_BITS_CHANGE);
		}
		else
		{
			for (; y1 <= y2; y1++)
			{
				if (LineMask & LineBits)
				{
					pApp->Set();
				}

				LineMask >>= 1;
				if (!LineMask) LineMask = LineReset;
				pApp->IncY();
			}
			Update(GDC_BITS_CHANGE);
		}
	}
}

void GSurface::Line(int x1, int y1, int x2, int y2)
{
	if (x1 == x2)
	{
		VLine(x1, y1, y2);
	}
	else if (y1 == y2)
	{
		HLine(x1, x2, y1);
	}
	else if (Clip.Valid())
	{
		OrgXy(x1, y1);
		OrgXy(x2, y2);

		// angled
		if (y1 > y2)
		{
			LgiSwap(y1, y2);
			LgiSwap(x1, x2);
		}

		GRect Bound(x1, y1, x2, y2);
		
		Bound.Normal();
		if (!Bound.Overlap(&Clip)) return;

		double m = (double) (y2-y1) / (x2-x1);
		double b = (double) y1 - (m*x1);

		int xt = (int) (((double)Clip.y1-b)/m);
		int xb = (int) (((double)Clip.y2-b)/m);

		if (y1 < Clip.y1)
		{
			x1 = xt;
			y1 = Clip.y1;
		}

		if (y2 > Clip.y2)
		{
			x2 = xb;
			y2 = Clip.y2;
		}

		int X1, X2;
		if (x1 < x2) { X1 = x1; X2 = x2; } else { X1 = x2; X2 = x1; }

		if (X1 < Clip.x1)
		{
			if (X2 < Clip.x1) return;

			if (x1 < Clip.x1)
			{
				y1 = (int) (((double) Clip.x1 * m) + b);
				x1 = Clip.x1;
			}
			else
			{
				y2 = (int) (((double) Clip.x1 * m) + b);
				x2 = Clip.x1;
			}
		}
		
		if (X2 > Clip.x2)
		{
			if (X1 > Clip.x2) return;

			if (x1 > Clip.x2)
			{
				y1 = (int) (((double) Clip.x2 * m) + b);
				x1 = Clip.x2;
			}
			else
			{
				y2 = (int) (((double) Clip.x2 * m) + b);
				x2 = Clip.x2;
			}
		}			
		
		int dx = abs(x2-x1);
		int dy = abs(y2-y1);
		int EInc, ENoInc, E, Inc = 1, Pos = 1;
	
		// printf("Dx: %i\nDy: %i\n", dx, dy);

		if (dy < dx)
		{
			// flat
			if (x1 > x2)
			{
				LgiSwap(x1, x2);
				LgiSwap(y1, y2);
			}

			if (y1 > y2)
			{
				Inc = -1;
			}

			EInc = dy + dy;
			E = EInc - dx;
			ENoInc = E - dx;

			pApp->SetPtr(x1, y1);
			if (LineBits == LINE_SOLID)
			{
				for (; x1<=x2; x1++)
				{
					pApp->Set();
					if (E < 0)
					{
						pApp->IncX();
						E += EInc;
					}
					else
					{
						pApp->IncPtr(1, Inc);
						E += ENoInc;
					}
				}
			}
			else
			{
				for (; x1<=x2; x1++)
				{
					if (LineBits & LineMask)
					{
						pApp->Set();
					}
					LineMask >>= 1;
					if (!LineMask) LineMask = LineReset;

					if (E < 0)
					{
						pApp->IncX();
						E += EInc;
					}
					else
					{
						pApp->IncPtr(1, Inc);
						E += ENoInc;
					}
				}
			}
		}
		else
		{
			if (x1 > x2)
			{
				Inc = -1;
			}
		
			// steep
			EInc = dx + dx;
			E = EInc - dy;
			ENoInc = E - dy;
	
			pApp->SetPtr(x1, y1);
			if (LineBits == LINE_SOLID)
			{
				for (; y1<=y2; y1++)
				{
					pApp->Set();
					if (E < 0)
					{
						pApp->IncY();
						E += EInc;
					}
					else
					{
						pApp->IncPtr(Inc, 1);
						E += ENoInc;
					}
				}
			}
			else
			{
				for (; y1<=y2; y1++)
				{
					if (LineBits & LineMask)
					{
						pApp->Set();
					}
					LineMask >>= 1;
					if (!LineMask) LineMask = LineReset;

					if (E < 0)
					{
						pApp->IncY();
						E += EInc;
					}
					else
					{
						pApp->IncPtr(Inc, 1);
						E += ENoInc;
					}
				}
			}
		}

		Update(GDC_BITS_CHANGE);
	}
}

void GSurface::Circle(double Cx, double Cy, double radius)
{
	int cx = (int)Cx;
	int cy = (int)Cy;
	int d = (int) (3 - (2 * radius));
	int x = 0;
	int y = (int) radius;

	Set(cx, cy + y);
	Set(cx, cy - y);
	Set(cx + y, cy);
	Set(cx - y, cy);

	if (d < 0)
	{
		d += (4 * x) + 6;
	}
	else
	{
		d += (4 * (x - y)) + 10;
		y--;
	}

	x++;

	while (x < y)
	{
		Set(cx + x, cy + y);
		Set(cx - x, cy + y);
		Set(cx + x, cy - y);
		Set(cx - x, cy - y);
		Set(cx + y, cy + x);
		Set(cx - y, cy + x);
		Set(cx + y, cy - x);
		Set(cx - y, cy - x);

		if (d < 0)
		{
			d += (4 * x) + 6;
		}
		else
		{
			d += (4 * (x - y)) + 10;
			y--;
		}

		x++;
	}

	if (x == y)
	{
		Set(cx + x, cy + x);
		Set(cx - x, cy + x);
		Set(cx + x, cy - x);
		Set(cx - x, cy - x);
	}

	Update(GDC_BITS_CHANGE);
}

void GSurface::FilledCircle(double Cx, double Cy, double radius)
{
	int cx = (int)Cx;
	int cy = (int)Cy;
	int d = (int) (3 - 2 * radius);
	int x = 0;
	int y = (int) radius;

	// HLine(cx + y, cx - y, cy);
	if (d < 0)
	{
		d += 4 * x + 6;
	}
	else
	{
		HLine(cx, cx, cy + y);
		d += 4 * (x - y) + 10;
		y--;
	}

	while (x < y)
	{
		HLine(cx + y, cx - y, cy + x);
		if (x != 0) HLine(cx + y, cx - y, cy - x);

		if (d < 0)
		{
			d += 4 * x + 6;
		}
		else
		{
			HLine(cx + x, cx - x, cy + y);
			HLine(cx + x, cx - x, cy - y);

			d += 4 * (x - y) + 10;
			y--;
		}

		x++;
	}

	if (x == y)
	{
		HLine(cx + y, cx - y, cy + x);
		HLine(cx + y, cx - y, cy - x);
	}

	Update(GDC_BITS_CHANGE);
}

void GSurface::Box(GRect *a)
{
	if (a)
	{
		GRect b = *a;
		b.Normal();
		if (b.x1 != b.x2 && b.y1 != b.y2)
		{
			HLine(b.x1, b.x2 - 1, b.y1);
			VLine(b.x2, b.y1, b.y2 - 1);
			HLine(b.x1 + 1, b.x2, b.y2);
			VLine(b.x1, b.y1 + 1, b.y2);
		}
		else
		{
			Set(b.x1, b.y1);
		}
	}
	else
	{
		GRect b(0, 0, X()-1, Y()-1);

		HLine(b.x1, b.x2 - 1, b.y1);
		VLine(b.x2, b.y1, b.y2 - 1);
		HLine(b.x1 + 1, b.x2, b.y2);
		VLine(b.x1, b.y1 + 1, b.y2);
	}
}

void GSurface::Box(int x1, int y1, int x2, int y2)
{
	GRect a(x1, y1, x2, y2);
	Box(&a);
}

void GSurface::Rectangle(GRect *a)
{
	GRect b;
	if (a)
		b = a;
	else
		b.ZOff(pMem->x-1, pMem->y-1);

	OrgRgn(b);
	b.Normal();
	b.Bound(&Clip);

	if (b.Valid())
	{
		pApp->SetPtr(b.x1, b.y1);
		pApp->Rectangle(b.X(), b.Y());
		Update(GDC_BITS_CHANGE);
	}
}

void GSurface::Rectangle(int x1, int y1, int x2, int y2)
{
	GRect a(x1, y1, x2, y2);
	Rectangle(&a);
}

void GSurface::Ellipse(double Cx, double Cy, double a, double b)
{
	// TODO: fix this primitive for odd widths and heights
	int cx = (int)Cx;
	int cy = (int)Cy;
	a = floor(a);
	b = floor(b);
	
	long aSq = (long) (a*a);
	long bSq = (long) (b*b);
	long two_aSq = aSq+aSq;
	long two_bSq = bSq+bSq;
	long x=0, y=(long)b, two_xBsq = 0, two_yAsq = y * two_aSq, error = -y * aSq;

	if (aSq && bSq && error)
	{
		while (two_xBsq <= two_yAsq)
		{
			if (x) Set(cx+x, cy+y);
			if (y) Set(cx-x, cy+y);
			if (x) Set(cx+x, cy-y);
			if (y) Set(cx-x, cy-y);
			x++;
	
			two_xBsq += two_bSq;
			error += two_xBsq - bSq;
			if (error >= 0)
			{
				y--;
				two_yAsq -= two_aSq;
				error -= two_yAsq;
			}
		}
	
		x=(long)a; y=0; two_xBsq = x * two_bSq; two_yAsq = 0; error = -x * bSq;
	
		while (two_xBsq >= two_yAsq)
		{
			if (x) Set(cx+x, cy+y);
			if (x) Set(cx-x, cy+y);
			if (y) Set(cx+x, cy-y);
			if (y) Set(cx-x, cy-y);
			y++;
	
			two_yAsq += two_aSq;
			error += two_yAsq - aSq;
			if (error >= 0)
			{
				x--;
				two_xBsq -= two_bSq;
				error -= two_xBsq;
			}
		}
	}
	Update(GDC_BITS_CHANGE);
}

void GSurface::FilledEllipse(double Cx, double Cy, double a, double b)
{
	// TODO: fix this primitive for odd widths and heights
	int cx = (int)Cx;
	int cy = (int)Cy;
	a = floor(a);
	b = floor(b);

	long aSq = (long) (a*a);
	long bSq = (long) (b*b);
	long two_aSq = aSq+aSq;
	long two_bSq = bSq+bSq;
	long x=0, y=(long)b, two_xBsq = 0, two_yAsq = y * two_aSq, error = -y * aSq;

	if (aSq && bSq && error)
	{
		while (two_xBsq <= two_yAsq)
		{
			x++;
			two_xBsq += two_bSq;
			error += two_xBsq - bSq;
			if (error >= 0)
			{
				Line(cx+x-1, cy+y, cx-x+1, cy+y);
				if (y != 0) Line(cx+x-1, cy-y, cx-x+1, cy-y);
	
				y--;
				two_yAsq -= two_aSq;
				error -= two_yAsq;
			}
		}
	
		x=(long)a;
		y=0;
		two_xBsq = x * two_bSq;
		two_yAsq = 0;
		error = -x * bSq;
	
		while (two_xBsq > two_yAsq)
		{
			Line(cx+x, cy+y, cx-x, cy+y);
			if (y != 0) Line(cx+x, cy-y, cx-x, cy-y);
			y++;
	
			two_yAsq += two_aSq;
			error += two_yAsq - aSq;
			if (error >= 0)
			{
				x--;
				two_xBsq -= two_bSq;
				error -= two_xBsq;
			}
		}
	}

	Update(GDC_BITS_CHANGE);
}

struct EDGE {
	int yMin, yMax, x, dWholeX, dX, dY, frac;
};

int nActive, nNextEdge;

void GSurface::Polygon(int nPoints, GdcPt2 *aPoints)
{
	GdcPt2 p0, p1;
	int i, j, gap, x0, x1, y, nEdges;
	EDGE *ET, **GET, **AET;

	/********************************************************************
	 * Add entries to the global edge table.  The global edge table has a
	 * bucket for each scan line in the polygon. Each bucket contains all
	 * the edges whose yMin == yScanline.  Each bucket contains the yMax,
	 * the x coordinate at yMax, and the denominator of the slope (dX)
	 */

	// allocate the tables
	ET = new EDGE[nPoints];
	GET = new EDGE*[nPoints];
	AET = new EDGE*[nPoints];

	if (ET && GET && AET)
	{
		for ( i = 0, nEdges = 0; i < nPoints; i++)
		{
			p0 = aPoints[i];
			p1 = aPoints[(i+1) % nPoints];

			// ignore if this is a horizontal edge
			if (p0.y == p1.y) continue;

			// swap points if necessary to ensure p0 contains yMin
			if (p0.y > p1.y)
			{
				p0 = p1;
				p1 = aPoints[i];
			}

			// create the new edge
			ET[nEdges].yMin = p0.y;
			ET[nEdges].yMax = p1.y;
			ET[nEdges].x = p0.x;
			ET[nEdges].dX = p1.x - p0.x;
			ET[nEdges].dY = p1.y - p0.y;
			ET[nEdges].frac = 0;
			GET[nEdges] = &ET[nEdges];
			nEdges++;
		}

		// sort the GET on yMin
		for (gap = 1; gap < nEdges; gap = 3*gap + 1);
		for (gap /= 3; gap > 0; gap /= 3)
			for (i = gap; i < nEdges; i++)
				for (j = i-gap; j >= 0; j -= gap)
				{
					if (GET[j]->yMin <= GET[j+gap]->yMin)
						break;
					EDGE *t = GET[j];
					GET[j] = GET[j+gap];
					GET[j+gap] = t;
				}

		// initialize the active edge table, and set y to first entering edge
		nActive = 0;
		nNextEdge = 0;
		y = GET[nNextEdge]->yMin;

		/* Now process the edges using the scan line algorithm.  Active edges
		will be added to the Active Edge Table (AET), and inactive edges will
		be deleted.  X coordinates will be updated with incremental integer
		arithmetic using the slope (dY / dX) of the edges. */

		while (nNextEdge < nEdges || nActive)
		{
			/* Move from the ET bucket y to the AET those edges whose yMin == y
			(entering edges) */
			while (nNextEdge < nEdges && GET[nNextEdge]->yMin == y)
				AET[nActive++] = GET[nNextEdge++];

			/* Remove from the AET those entries for which yMax == y (leaving
			edges) */
			i = 0;
			while (i < nActive)
			{
				if (AET[i]->yMax == y)
					memmove(&AET[i], &AET[i+1], sizeof(AET[0]) * (--nActive - i));
				else
					i++;
			}

			/* Now sort the AET on x.  Since the list is usually quite small,
			the sort is implemented as a simple non-recursive shell sort */
			for (gap = 1; gap < nActive; gap = 3*gap + 1);
			for (gap /= 3; gap > 0; gap /= 3)
				for (i = gap; i < nActive; i++)
					for (j = i-gap; j >= 0; j -= gap)
					{
						if (AET[j]->x <= AET[j+gap]->x)
							break;
						EDGE *t = AET[j];
						AET[j] = AET[j+gap];
						AET[j+gap] = t;
					}

			/* Fill in desired pixels values on scan line y by using pairs of x
			coordinates from the AET */

			for (i = 0; i < nActive; i += 2)
			{
				x0 = AET[i]->x;
				x1 = AET[i+1]->x;

				/* Left edge adjustment for positive fraction.  0 is interior. */
				if (AET[i]->frac > 0) x0++;

				// Right edge adjustment for negative fraction.  0 is exterior. */
				if (AET[i+1]->frac <= 0) x1--;

				// Draw interior spans
				if (x1 >= x0)
					HLine(x0, x1, y);
			}

			/* Update all the x coordinates.  Edges are scan converted using a
			modified midpoint algorithm (Bresenham's algorithm reduces to the
			midpoint algorithm for two dimensional lines) */
			for (i = 0; i < nActive; i++)
			{
				EDGE *e = AET[i];
				// update the fraction by dX
				e->frac += e->dX;

				if (e->dX < 0)
					while ( -(e->frac) >= e->dY)
					{
						e->frac += e->dY;
						e->x--;
					}
				else
					while (e->frac >= e->dY)
					{
						e->frac -= e->dY;
						e->x++;
					}
			}
			y++;
		}
	}

	// Release tables
	DeleteArray(ET);
	DeleteArray(GET);
	DeleteArray(AET);
}

void GSurface::Blt(int x, int y, GSurface *Src, GRect *a)
{
	OrgXy(x, y);

	if (Src && Src->pMem && Src->pMem->Base)
	{
		GRect S;
		if (a)	S = *a;
		else	S.ZOff(Src->X()-1, Src->Y()-1);
		S.Offset(Src->OriginX, Src->OriginY);

		GRect SClip = S;
		SClip.Bound(&Src->Clip);

		if (SClip.Valid())
		{
			GRect D = SClip;
			D.Offset(x-S.x1, y-S.y1);

			GRect DClip = D;
			DClip.Bound(&Clip);

			GRect Re = DClip;
			Re.Offset(S.x1-x, S.y1-y);
			SClip.Bound(&Re);

			if (DClip.Valid() && SClip.Valid())
			{
				GBmpMem Bits, Alpha;

				int PixelBytes = GColourSpaceToBits(Src->pMem->Cs) >> 3;
				
				Bits.Base =	Src->pMem->Base +
						(SClip.y1 * Src->pMem->Line) +
						(SClip.x1 * PixelBytes);
				Bits.x = min(SClip.X(), DClip.X());
				Bits.y = min(SClip.Y(), DClip.Y());
				Bits.Line = Src->pMem->Line;
				Bits.Cs = Src->GetColourSpace();
				Bits.Flags = Src->pMem->IsPreMul() ? GBmpMem::BmpPreMulAlpha : 0;

				if (Src->pAlphaDC && !Src->DrawOnAlpha())
				{
					GBmpMem *ASurface = Src->pAlphaDC->pMem;
					Alpha = Bits;
					Alpha.Cs = CsIndex8;
					Alpha.Line = ASurface->Line;
					Alpha.Base =	ASurface->Base +
							(SClip.y1 * ASurface->Line) +
							(SClip.x1);
				}

				pApp->SetPtr(DClip.x1, DClip.y1);
				GPalette *SrcPal = Src->DrawOnAlpha() ? NULL : Src->Palette();
				// printf("\t%p::Blt pApp=%p, %s\n", this, pApp, pApp->GetClass());
				pApp->Blt(&Bits, SrcPal, Alpha.Base ? &Alpha : NULL);
				Update(GDC_BITS_CHANGE);

				if (pApp->GetFlags() & GDC_UPDATED_PALETTE)
				{
					Palette(new GPalette(pApp->GetPal()));
				}
			}
		}
	}
}

#define sqr(a)			((a)*(a))
#define Distance(a, b)		(sqrt(sqr(a.x-b.x)+sqr(a.y-b.y)))
class FPt2 {
public:
	double x, y;
};

typedef FPt2 BPt;

void GSurface::Bezier(int Threshold, GdcPt2 *Pt)
{
	if (Pt)
	{
		int OldPts = 3;
		int NewPts = 0;
		BPt *BufA = new BPt[1024];
		BPt *BufB = new BPt[1024];
		BPt *Old = BufA;
		BPt *New = BufB;

		Threshold = max(Threshold, 1);

		if (!Old || !New) return;
		for (int n=0; n<OldPts; n++)
		{
			Old[n].x = Pt[n].x;
			Old[n].y = Pt[n].y;
			OrgPt(Old[n]);
		}

		do
		{
			NewPts = 1;
			New[0] = Old[0];
			for (int i=1; i<OldPts-1; i += 2)
			{
				New[NewPts].x = (Old[i].x + Old[i-1].x) / 2;
				New[NewPts].y = (Old[i].y + Old[i-1].y) / 2;
				NewPts += 2;

				New[NewPts].x = (Old[i].x + Old[i+1].x) / 2;
				New[NewPts].y = (Old[i].y + Old[i+1].y) / 2;
				NewPts--;

				New[NewPts].x = (New[NewPts+1].x + New[NewPts-1].x) / 2;
				New[NewPts].y = (New[NewPts+1].y + New[NewPts-1].y) / 2;
				NewPts += 2;

				New[NewPts++] = Old[i+1];
			}

			BPt *Temp = Old;
			Old = New;
			New = Temp;
			
			OldPts = NewPts;
		}
		while (Distance(Old[0], Old[1]) > Threshold);

		if (Threshold > 1)
		{
			for (int i=0; i<OldPts-1; i += 4)
			{
				Line(	(int)Old[i].x,
						(int)Old[i].y,
						(int)Old[i+1].x,
						(int)Old[i+1].y);
				Line(	(int)Old[i+1].x,
						(int)Old[i+1].y,
						(int)Old[i+3].x,
						(int)Old[i+3].y);
				Line(	(int)Old[i+3].x,
						(int)Old[i+3].y,
						(int)Old[i+4].x,
						(int)Old[i+4].y);
			}
		}
		else
		{
			for (int i=0; i<OldPts; i++)
			{
				Set(	(int)Old[i].x,
						(int)Old[i].y);
			}
		}

		delete [] BufA;
		delete [] BufB;

		Update(GDC_BITS_CHANGE);
	}
}

class PointStack {

	int Used;
	int Size;
	GdcPt2 *Stack;

	bool SetSize(int s)
	{
		GdcPt2 *Next = new GdcPt2[Size + s];
		if (Next)
		{
			Size += s;
			Used = min(Size, Used);
			memcpy(Next, Stack, sizeof(GdcPt2)*Used);
			DeleteArray(Stack);
			Stack = Next;
			return true;
		}
		return false;
	}

public:
	PointStack()
	{
		Used = 0;
		Size = 1024;
		Stack = new GdcPt2[Size];
	}

	~PointStack()
	{
		DeleteArray(Stack);
	}

	int GetSize() { return Used; }

	void Push(int x, int y)
	{
		if (Used >= Size)
		{
			SetSize(Size+1024);
		}

		if (Stack)
		{
			Stack[Used].x = x;
			Stack[Used].y = y;
			Used++;
		}
	}

	void Pop(int &x, int &y)
	{
		if (Stack && Used > 0)
		{
			Used--;
			x = Stack[Used].x;
			y = Stack[Used].y;
		}
	}
};

// This should return true if 'Pixel' is in the region being filled.
typedef bool (*FillMatchProc)(COLOUR Seed, COLOUR Pixel, COLOUR Border, int Bits);

bool FillMatch_Diff(COLOUR Seed, COLOUR Pixel, COLOUR Border, int Bits)
{
	return Seed == Pixel;
}

bool FillMatch_Near(COLOUR Seed, COLOUR Pixel, COLOUR Border, int Bits)
{
	COLOUR s24 = CBit(24, Seed, Bits);
	COLOUR p24 = CBit(24, Pixel, Bits);
	int Dr = R24(s24) - R24(p24);
	int Dg = G24(s24) - G24(p24);
	int Db = B24(s24) - B24(p24);

	return	(abs(Dr) < Border) &&
			(abs(Dg) < Border) &&
			(abs(Db) < Border);
}

void GSurface::FloodFill(int StartX, int StartY, int Mode, COLOUR Border, GRect *FillBounds)
{
	COLOUR Seed = Get(StartX, StartY);
	if (Seed == 0xffffffff) return; // Doesn't support get pixel

	PointStack Ps;
	GRect Bounds;
	FillMatchProc Proc = 0;
	int Bits = GetBits();

	Bounds.x1 = X();
	Bounds.y1 = Y();
	Bounds.x2 = 0;
	Bounds.y2 = 0;

	Ps.Push(StartX, StartY);

	switch (Mode)
	{
		case GDC_FILL_TO_DIFFERENT:
		{
			Proc = FillMatch_Diff;
			break;
		}
		case GDC_FILL_TO_BORDER:
		{
			break;
		}
		case GDC_FILL_NEAR:
		{
			Proc = FillMatch_Near;
			break;
		}
	}

	if (Proc)
	{
		COLOUR Start = Colour();
		
		if (!Proc(Seed, Start, Border, Bits))
		{
			while (Ps.GetSize() > 0)
			{
				bool Above = true;
				bool Below = true;
				int Ox, Oy;
				Ps.Pop(Ox, Oy);
				int x = Ox, y = Oy;

				// move right loop
				COLOUR c = Get(x, y);

				while (x < X() && Proc(Seed, c, Border, Bits))
				{
					Set(x, y);
					Bounds.Union(x, y);

					if (y > 0)
					{
						c = Get(x, y - 1);

						if (Above)
						{
							if (Proc(Seed, c, Border, Bits))
							{
								Ps.Push(x, y - 1);
								Above = false;
							}
						}
						else if (!Proc(Seed, c, Border, Bits))
						{
							Above = true;
						}
					}

					if (y < Y() - 1)
					{
						c = Get(x, y + 1);

						if (Below)
						{
							if (Proc(Seed, c, Border, Bits))
							{
								Ps.Push(x, y + 1);
								Below = false;
							}
						}
						else if (!Proc(Seed, c, Border, Bits))
						{
							Below = true;
						}
					}

					x++;
					c = Get(x, y);
				}

				// move left loop
				x = Ox;

				Above = !((y > 0) && (Get(x, y - 1) == Seed));
				Below = !((y < Y() - 1) && (Get(x, y + 1) == Seed));

				x--;
				c = Get(x, y);

				while (x >= 0 && Proc(Seed, c, Border, Bits))
				{
					Set(x, y);
					Bounds.Union(x, y);

					if (y > 0)
					{
						c = Get(x, y - 1);

						if (Above)
						{
							if (Proc(Seed, c, Border, Bits))
							{
								Ps.Push(x, y - 1);
								Above = false;
							}
						}
						else if (!Proc(Seed, c, Border, Bits))
						{
							Above = true;
						}
					}

					if (y < Y() - 1)
					{
						c = Get(x, y + 1);

						if (Below)
						{
							if (Proc(Seed, c, Border, Bits))
							{
								Ps.Push(x, y + 1);
								Below = false;
							}
						}
						else if (!Proc(Seed, c, Border, Bits))
						{
							Below = true;
						}
					}

					x--;
					c = Get(x, y);
				}
			}
		}
	}

	if (FillBounds)
	{
		*FillBounds = Bounds;
	}
	Update(GDC_BITS_CHANGE);
}

void GSurface::Arc(double cx, double cy, double radius, double start, double end) {}
void GSurface::FilledArc(double cx, double cy, double radius, double start, double end) {}
void GSurface::StretchBlt(GRect *d, GSurface *Src, GRect *s) {}

bool GSurface::HasAlpha(bool b)
{
	DrawOnAlpha(false);

	if (b)
	{
		if (!pAlphaDC)
		{
			pAlphaDC = new GMemDC;
		}

		if (pAlphaDC && pMem)
		{
			if (!pAlphaDC->Create(pMem->x, pMem->y, CsIndex8))
			{
				DeleteObj(pAlphaDC);
			}
			else
			{
				ClearFlag(Flags, GDC_DRAW_ON_ALPHA);
			}
		}
	}
	else
	{
		DeleteObj(pAlphaDC);
	}

	return (b == HasAlpha());
}

bool GSurface::DrawOnAlpha(bool Draw)
{
	bool Prev = DrawOnAlpha();
    bool Swap = false;

	if (Draw)
	{
		if (!Prev && pAlphaDC && pMem)
		{
			Swap = true;
			SetFlag(Flags, GDC_DRAW_ON_ALPHA);
			PrevOp = Op(GDC_SET);
		}
	}
	else
	{
		if (Prev && pAlphaDC && pMem)
		{
		    Swap = true;
			ClearFlag(Flags, GDC_DRAW_ON_ALPHA);
			Op(PrevOp);
		}
	}

    if (Swap)
    {
		GBmpMem *Temp = pMem;
		pMem = pAlphaDC->pMem;
		pAlphaDC->pMem = Temp;
		
    	#if WINNATIVE
		OsBitmap hTmp = hBmp;
		hBmp = pAlphaDC->hBmp;
		pAlphaDC->hBmp = hTmp;
		OsPainter hP = hDC;
		hDC = pAlphaDC->hDC;
		pAlphaDC->hDC = hP;
		#endif
    }

	return Prev;
}

GApplicator *GSurface::CreateApplicator(int Op, GColourSpace Cs)
{
	GApplicator *pA = NULL;

	if (!Cs)
	{
		if (pMem)
		{
			if (DrawOnAlpha())
			{
				Cs = CsIndex8;
			}
			else if (pMem->Cs)
			{
				Cs = pMem->Cs;
			}
			else
			{
				LgiAssert(!"Memory context has no colour space...");
			}
		}
		else if (IsScreen())
		{
			Cs = GdcD->GetColourSpace();
		}
		else
		{
			LgiAssert(!"No memory context to read colour space from.");
		}
	}
	
	pA = GApplicatorFactory::NewApp(Cs, Op);
	if (pA)
	{
		if (DrawOnAlpha())
		{
			pA->SetSurface(pMem);
		}
		else
		{
			pA->SetSurface(pMem, pPalette, (pAlphaDC) ? pAlphaDC->pMem : 0);
		}
		pA->SetOp(Op);
	}
	else
	{
		GApplicatorFactory::NewApp(Cs, Op);
		const char *CsStr = GColourSpaceToString(Cs);
		LgiTrace("Error: GDeviceContext::CreateApplicator(%i, %x, %s) failed.\n", Op, Cs, CsStr);
		LgiAssert(!"No applicator");
	}

	return pA;
}

bool GSurface::Applicator(GApplicator *pApplicator)
{
	bool Status = false;

	if (pApplicator)
	{
		if (Flags & GDC_OWN_APPLICATOR)
		{
			DeleteObj(pApp)
			Flags &= ~GDC_OWN_APPLICATOR;
		}

		Flags &= ~GDC_CACHED_APPLICATOR;

		pApp = pApplicator;
		if (DrawOnAlpha())
		{
			pApp->SetSurface(pMem);
		}
		else
		{
			pApp->SetSurface(pMem, pPalette, pAlphaDC->pMem);
		}
		pApp->SetPtr(0, 0);

		Status = true;
	}

	return Status;
}

GApplicator *GSurface::Applicator()
{
	return pApp;
}

GRect GSurface::ClipRgn(GRect *Rgn)
{
	GRect Old = Clip;
	
	if (Rgn)
	{
		Clip.x1 = max(0, Rgn->x1 - OriginX);
		Clip.y1 = max(0, Rgn->y1 - OriginY);
		Clip.x2 = min(X()-1, Rgn->x2 - OriginX);
		Clip.y2 = min(Y()-1, Rgn->y2 - OriginY);
	}
	else
	{
		Clip.x1 = 0;
		Clip.y1 = 0;
		Clip.x2 = X()-1;
		Clip.y2 = Y()-1;
	}
	
	return Old;
}

GRect GSurface::ClipRgn()
{
	return Clip;
}

GColour GSurface::Colour(GColour c)
{
	LgiAssert(pApp);
	GColour cPrev(pApp->c, GetBits());
	
	uint32 c32 = c.c32();
	GColourSpace Cs = pApp->GetColourSpace();
	switch (Cs)
	{
		case CsIndex8:
			if (c.GetColourSpace() == CsIndex8)
			{
				pApp->c = c.c8();
			}
			else
			{
				GPalette *p = Palette();
				if (p)
				{
					pApp->c = p->MatchRgb(Rgb32To24(c32));
				}
				else LgiAssert(0);
			}
			break;
		case CsRgb15:
		case CsBgr15:
			pApp->c = Rgb32To15(c32);
			break;
		case CsRgb16:
		case CsBgr16:
			pApp->c = Rgb32To16(c32);
			break;
		case CsRgba32:
		case CsBgra32:
		case CsArgb32:
		case CsAbgr32:
		case CsRgbx32:
		case CsBgrx32:
		case CsXrgb32:
		case CsXbgr32:
		case CsRgba64:
		case CsBgra64:
		case CsArgb64:
		case CsAbgr64:
			pApp->p32.r = R32(c32);
			pApp->p32.g = G32(c32);
			pApp->p32.b = B32(c32);
			pApp->p32.a = A32(c32);
			break;
		case CsRgb24:
		case CsBgr24:
		case CsRgb48:
		case CsBgr48:
			pApp->p24.r = R32(c32);
			pApp->p24.g = G32(c32);
			pApp->p24.b = B32(c32);
			break;
		default:
			LgiAssert(0);
			break;
	}

	return cPrev;
}

COLOUR GSurface::Colour(COLOUR c, int Bits)
{
	GColour n(c, Bits ? Bits : GetBits());
	GColour Prev = Colour(n);
	switch (GetBits())
	{
		case 8:
			return Prev.c8();
		case 24:
			return Prev.c24();
		case 32:
			return Prev.c32();
	}
	return Prev.c32();
}

int GSurface::Op(int NewOp, NativeInt Param)
{
	int PrevOp = (pApp) ? pApp->GetOp() : GDC_SET;
	if (!pApp || PrevOp != NewOp)
	{
		COLOUR cCurrent = (pApp) ? Colour() : 0;

		if (Flags & GDC_OWN_APPLICATOR)
		{
			DeleteObj(pApp);
		}

		if (NewOp < GDC_CACHE_SIZE && !DrawOnAlpha())
		{
			pApp = (pAppCache[NewOp]) ? pAppCache[NewOp] : pAppCache[NewOp] = CreateApplicator(NewOp, GetColourSpace());
			Flags &= ~GDC_OWN_APPLICATOR;
			Flags |= GDC_CACHED_APPLICATOR;
		}
		else
		{
			pApp = CreateApplicator(NewOp, GetColourSpace());
			Flags &= ~GDC_CACHED_APPLICATOR;
			Flags |= GDC_OWN_APPLICATOR;
		}

		if (pApp)
		{
			Colour(cCurrent);
			if (Param >= 0)
				pApp->SetVar(GAPP_ALPHA_A, Param);
		}
		else
		{
			printf("Error: Couldn't create applicator, Op=%i\n", NewOp);
			LgiAssert(0);
		}
	}

	return PrevOp;
}

GPalette *GSurface::Palette()
{
	if (!pPalette && pMem && (pMem->Flags & GDC_ON_SCREEN))
	{
		pPalette = GdcD->GetSystemPalette();
		if (pPalette)
		{
			Flags |= GDC_OWN_PALETTE;
		}
	}

	return pPalette;
}

void GSurface::Palette(GPalette *pPal, bool bOwnIt)
{
	if (pPalette && Flags & GDC_OWN_PALETTE)
	{
		delete pPalette;
	}

	pPalette = pPal;

	if (pPal && bOwnIt)
	{
		Flags |= GDC_OWN_PALETTE;
	}
	else
	{
		Flags &= ~GDC_OWN_PALETTE;
	}

	if (pApp)
	{
		pApp->SetSurface(pMem, pPalette, (pAlphaDC) ? pAlphaDC->pMem : 0);
	}
}

bool GSurface::GetVariant(const char *Name, GVariant &Dst, char *Array)
{
	switch (GStringToProp(Name))
	{
		case SurfaceX:
		{
			Dst = X();
			break;
		}
		case SurfaceY:
		{
			Dst = Y();
			break;
		}
		case SurfaceBits:
		{
			Dst = GetBits();
			break;
		}
		case SurfaceColourSpace:
		{
			Dst = GetColourSpace();
			break;
		}
		case SurfaceIncludeCursor:
		{
			Dst = TestFlag(Flags, GDC_CAPTURE_CURSOR);
			break;
		}
		default:
		{
			return false;
		}
	}

	return true;
}

bool GSurface::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	switch (GStringToProp(Name))
	{
		case SurfaceIncludeCursor:
		{
			if (Value.CastInt32())
				SetFlag(Flags, GDC_CAPTURE_CURSOR);
			else
				ClearFlag(Flags, GDC_CAPTURE_CURSOR);
			break;
		}
		default:
			break;
	}
	
	return false;
}

bool GSurface::CallMethod(const char *Name, GVariant *ReturnValue, GArray<GVariant*> &Args)
{
	return false;
}
