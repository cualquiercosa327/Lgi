/*hdr
**	FILE:		Gdc2.h
**	AUTHOR:		Matthew Allen
**	DATE:		20/2/97
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

class GScreenPrivate
{
public:
	GRect		Client;
	uint32		Col;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
GScreenDC::GScreenDC()
{
	d = new GScreenPrivate;
	ColourSpace = CsNone;
}

GScreenDC::GScreenDC(GView *view, void *Param)
{
	d = new GScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();
}

GScreenDC::~GScreenDC()
{
	DeleteObj(d);
}

OsPainter GScreenDC::Handle()
{
	return NULL;
}

int GScreenDC::GetFlags()
{
	return 0;
}

bool GScreenDC::GetClient(GRect *c)
{
	if (!c) return false;
	*c = d->Client;
	return true;
}

void GScreenDC::SetClient(GRect *c)
{
	/*
	if (c)
	{
		SetWindowOrgEx(hDC, 0, 0, NULL);
		SelectClipRgn(hDC, 0);
		
		HRGN hRgn = CreateRectRgn(c->x1, c->y1, c->x2+1, c->y2+1);
		if (hRgn)
		{
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}

		SetWindowOrgEx(hDC, -c->x1, -c->y1, NULL);
		d->Client = *c;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		SetWindowOrgEx(hDC, 0, 0, NULL);
		SelectClipRgn(hDC, 0);
	}
	*/
}

void GScreenDC::GetOrigin(int &x, int &y)
{
	/*
	POINT pt;
	if (GetWindowOrgEx(hDC, &pt))
	{
		x = pt.x;
		y = pt.y;
	}
	else
	*/
	{
		x = y = 0;
	}
}

void GScreenDC::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
	/*
	if (hDC)
	{
		SetWindowOrgEx(hDC, x, y, NULL);
	}
	*/
}

GPalette *GScreenDC::Palette()
{
	return GSurface::Palette();
}

void GScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
	GSurface::Palette(pPal, bOwnIt);
}

GRect GScreenDC::ClipRgn(GRect *Rgn)
{
	GRect Prev = Clip;

	if (Rgn)
	{
		GdcPt2 Origin;
		/*
		GetWindowOrgEx(hDC, &Origin);

		Clip.x1 = max(Rgn->x1 - Origin.x, 0);
		Clip.y1 = max(Rgn->y1 - Origin.y, 0);
		Clip.x2 = min(Rgn->x2 - Origin.x, d->Sx-1);
		Clip.y2 = min(Rgn->y2 - Origin.y, d->Sy-1);

		HRGN hRgn = CreateRectRgn(Clip.x1, Clip.y1, Clip.x2+1, Clip.y2+1);
		if (hRgn)
		{
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}
		*/
	}
	else
	{
		Clip.x1 = 0;
		Clip.y1 = 0;
		Clip.x2 = X()-1;
		Clip.y2 = Y()-1;

		// SelectClipRgn(hDC, NULL);
	}

	return Prev;
}

GRect GScreenDC::ClipRgn()
{
	return Clip;
}

COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = d->Col;

	return Prev;
}

GColour GScreenDC::Colour(GColour c)
{
	GColour cPrev(d->Col, GetBits());
	Colour(c.c32(), 32);
	return cPrev;
}

int GScreenDC::Op(int Op, NativeInt Param)
{
	return 0;
}

COLOUR GScreenDC::Colour()
{
	return d->Col;
}

int GScreenDC::Op()
{
	return 0;
}

int GScreenDC::X()
{
	if (d->Client.Valid())
		return d->Client.X();

	return 0;
}

int GScreenDC::Y()
{
	if (d->Client.Valid())
		return d->Client.Y();

	return 0;
}

int GScreenDC::GetBits()
{
	return GdcD->GetBits();
}

bool GScreenDC::SupportsAlphaCompositing()
{
	// Windows does support blending screen content with bitmaps that have alpha
	return true;
}

void GScreenDC::Set(int x, int y)
{
	
}

COLOUR GScreenDC::Get(int x, int y)
{
	return 0;
}

uint GScreenDC::LineStyle(uint32 Bits, uint32 Reset)
{
	return LineBits = Bits;
}

uint GScreenDC::LineStyle()
{
	return LineBits;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	if (x1 > x2)
	{
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	/*
	MoveToEx(hDC, x1, y, NULL);
	LineTo(hDC, x2 + 1, y);
	*/
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	if (y1 > y2)
	{
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	/*
	MoveToEx(hDC, x, y1, NULL);
	LineTo(hDC, x, y2 + 1);
	*/
}

void GScreenDC::Line(int x1, int y1, int x2, int y2)
{
	/*
	MoveToEx(hDC, x1, y1, NULL);
	LineTo(hDC, x2, y2);
	uint32 WinCol = RGB( R24(d->Col), G24(d->Col), B24(d->Col) );
	SetPixel(hDC, x2, y2, WinCol);
	*/
}

void GScreenDC::Circle(double cx, double cy, double radius)
{
	/*
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Ellipse(	hDC,
				(int)floor(cx - radius),
				(int)floor(cy - radius),
				(int)ceil(cx + radius),
				(int)ceil(cy + radius));
	SelectObject(hDC, hTemp);
	*/
}

void GScreenDC::FilledCircle(double cx, double cy, double radius)
{
	/*
	::Ellipse(	hDC,
				(int)floor(cx - radius),
				(int)floor(cy - radius),
				(int)ceil(cx + radius),
				(int)ceil(cy + radius)); */
}

void GScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	int StartX = (int)(cx + (cos(start) * radius));
	int StartY = (int)(cy + (int)(sin(start) * radius));
	int EndX = (int)(cx + (int)(cos(end) * radius));
	int EndY = (int)(cy + (int)(sin(end) * radius));

	/*
	::Arc(	hDC,
			(int)floor(cx - radius),
			(int)floor(cy - radius),
			(int)ceil(cx + radius),
			(int)ceil(cy + radius),
			StartX, StartY,
			EndX, EndY);
	*/
}

void GScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	int StartX = (int)(cx + (cos(start) * radius));
	int StartY = (int)(cy + (int)(sin(start) * radius));
	int EndX = (int)(cx + (int)(cos(end) * radius));
	int EndY = (int)(cy + (int)(sin(end) * radius));

	/*
	::Pie(	hDC,
			(int)floor(cx - radius),
			(int)floor(cy - radius),
			(int)ceil(cx + radius),
			(int)ceil(cy + radius),
			StartX, StartY,
			EndX, EndY);
	*/
}

void GScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	/*
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Ellipse(	hDC,
				(int)floor(cx - x),
				(int)floor(cy - y),
				(int)ceil(cx + x),
				(int)ceil(cy + y)
				);
	SelectObject(hDC, hTemp);
	*/
}

void GScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	/*
	::Ellipse(	hDC,
				(int)floor(cx - x),
				(int)floor(cy - y),
				(int)ceil(cx + x),
				(int)ceil(cy + y)
				);
	*/
}

void GScreenDC::Box(int x1, int y1, int x2, int y2)
{
	/*
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Rectangle(hDC, x1, y1, x2+1, y2+1);
	SelectObject(hDC, hTemp);
	*/
}

void GScreenDC::Box(GRect *a)
{
	/*
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	if (a)
	{
		::Rectangle(hDC, a->x1, a->y1, a->x2+1, a->y2+1);
	}
	else
	{
		::Rectangle(hDC, 0, 0, X(), Y());
	}
	SelectObject(hDC, hTemp);
	*/
}

void GScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	// ::Rectangle(hDC, x1, y1, x2+1, y2+1);
}

void GScreenDC::Rectangle(GRect *a)
{
	GRect b;
	if (a)
	{
		b = *a;
	}
	else
	{
		b.ZOff(X()-1, Y()-1);
	}

	// ::Rectangle(hDC, b.x1, b.y1, b.x2+1, b.y2+1);
}

void GScreenDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
}

void GScreenDC::StretchBlt(GRect *dst, GSurface *Src, GRect *s)
{
}

void GScreenDC::Polygon(int Points, GdcPt2 *Data)
{
}

void GScreenDC::Bezier(int Threshold, GdcPt2 *Pt)
{
}

void GScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, GRect *r)
{
	// ::FloodFill(hDC, x, y, d->Col);
}

