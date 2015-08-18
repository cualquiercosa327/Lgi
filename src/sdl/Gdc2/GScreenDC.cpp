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
	ColourSpace = CsNone;
	
	d->Screen = (SDL_Surface*)Param;
	LgiAssert(d->Screen);
	if (d->Screen)
	{
		ColourSpace = PixelFormat2ColourSpace(d->Screen->format);
		
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
	return d->Screen;
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
