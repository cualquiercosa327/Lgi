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
	int			Op;
	GRect		Client;
	uint32		Col;
	SDL_Surface *Screen;
	
	GScreenPrivate()
	{
		Op = GDC_SET;
		Client.ZOff(-1, -1);
	}
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
	
	d->Screen = (SDL_Surface*)Param;
	LgiAssert(d->Screen);
	if (d->Screen)
	{
		SDL_LockSurface(d->Screen);

		pMem = new GBmpMem;
		LgiAssert(pMem != NULL);
		if (pMem)
		{
			pMem->Base = (uchar*)d->Screen->pixels;
			pMem->x = d->Screen->w;
			pMem->y = d->Screen->h;
			pMem->Line = d->Screen->pitch;
			pMem->Cs = ColourSpace;
		}

		pApp = CreateApplicator(d->Op, ColourSpace);
		LgiAssert(pApp != NULL);
		
		Clip.ZOff(d->Screen->w-1, d->Screen->h-1);
	}
}

GScreenDC::~GScreenDC()
{
	if (d->Screen)
	{
		/*
		GBgra32 *p = (GBgra32*) d->Screen->pixels;
		memset(p, 0, 4*4);
		p[0].r = 255;
		p[1].g = 255;
		p[2].b = 255;
		p[3].a = 255;
		*/

		SDL_UnlockSurface(d->Screen);
		SDL_UpdateRect(d->Screen, 0, 0, 0, 0);
	}
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
	if (c)
	{
		ClipRgn(c);
		SetOrigin(-c->x1, -c->y1);
		d->Client = *c;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		SetOrigin(0, 0);
		ClipRgn(NULL);
	}
}

GPalette *GScreenDC::Palette()
{
	return GSurface::Palette();
}

void GScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
	GSurface::Palette(pPal, bOwnIt);
}

int GScreenDC::X()
{
	if (d->Client.Valid())
		return d->Client.X();

	return d->Screen->w;
}

int GScreenDC::Y()
{
	if (d->Client.Valid())
		return d->Client.Y();

	return d->Screen->h;
}

int GScreenDC::GetBits()
{
	return d->Screen ? d->Screen->format->BitsPerPixel : 0;
}

bool GScreenDC::SupportsAlphaCompositing()
{
	// Windows does support blending screen content with bitmaps that have alpha
	return true;
}

uint GScreenDC::LineStyle(uint32 Bits, uint32 Reset)
{
	return LineBits = Bits;
}

uint GScreenDC::LineStyle()
{
	return LineBits;
}

#if 0
COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = d->Col;
	d->Col = CBit(32, c, Bits);
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

void GScreenDC::Set(int x, int y)
{
}

COLOUR GScreenDC::Get(int x, int y)
{
	return 0;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	// Swap x coords to be in order
	if (x1 > x2)
	{
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	
	// Do clipping
	if (x2 < 0 || x1 >= d->Screen->w || y < 0 || y >= d->Screen->w)
		return;
	if (x1 < 0)
		x1 = 0;
	if (x2 >= d->Screen->w)
		x2 = d->Screen->w - 1;
	
	// Do drawing...
	register GPointer p;
	p.u8 = (uint8*)d->Screen->pixels;
	Uint16 ystep = d->Screen->pitch;
	Uint16 xstep = d->Screen->format->BytesPerPixel;
	p.u8 += (y * ystep) + (x1 * xstep);
	register int px = x2 - x1 + 1;
	register uint32 col = d->Col;
	switch (xstep)
	{
		case 4:
		{
			while (px--)
				*p.u32++ = col;
		}
		default:
		{
			LgiAssert(0);
		}
	}
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	// Swap y coords to be in order
	if (y1 > y2)
	{
		int t = y1;
		y1 = y2;
		y2 = t;
	}

	// Do clipping
	if (y2 < 0 || y1 >= d->Screen->h || x < 0 || x >= d->Screen->w)
		return;
	if (y1 < 0)
		y1 = 0;
	if (y2 >= d->Screen->h)
		y2 = d->Screen->h - 1;
	
	// Do drawing...
	register GPointer p;
	p.u8 = (uint8*)d->Screen->pixels;
	register Uint16 ystep = d->Screen->pitch;
	Uint16 xstep = d->Screen->format->BytesPerPixel;
	p.u8 += (y1 * ystep) + (x * xstep);
	register int py = y2 - y1 + 1;
	register uint32 col = d->Col;
	switch (xstep)
	{
		case 4:
		{
			while (py--)
			{
				*p.u32 = col;
				p.u8 += ystep;
			}
		}
		default:
		{
			LgiAssert(0);
		}
	}
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
	GRect r(x1, y1, x2, y2);
	Box(&r);
}

void GScreenDC::Box(GRect *a)
{
	GRect b;
	if (a)
		b = *a;
	else
		b.ZOff(X()-1, Y()-1);

	HLine(b.x1, b.x2, b.y1);
	if (b.Y() > 0)
	{
		HLine(b.x1, b.x2, b.y2);
	}
	if (b.Y() > 1)
	{
		VLine(b.x1, b.y1+1, b.y2-1);
		VLine(b.x2, b.y1+1, b.y2-1);
	}
}

void GScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	SDL_Rect r = { x1, y1, x2 - x1 + 1, y2 - y1 + 1 };
	SDL_FillRect(d->Screen, &r, d->Col);
}

void GScreenDC::Rectangle(GRect *a)
{
	GRect b;
	if (a)
		b = *a;
	else
		b.ZOff(X()-1, Y()-1);

	SDL_Rect r = b;
	SDL_FillRect(d->Screen, &r, d->Col);
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

#endif