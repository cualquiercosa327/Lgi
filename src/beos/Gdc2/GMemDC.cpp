/*hdr
**	FILE:			GMemDC.h
**	AUTHOR:			Matthew Allen
**	DATE:			27/11/2001
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>
#include "Gdc2.h"
#include "GPalette.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	OsBitmap	Bmp;
	OsPainter	View;

	GMemDCPrivate()
	{
		Bmp = 0;
		View = 0;
	}
	
	~GMemDCPrivate()
	{
		DeleteObj(Bmp);
	}
};

GMemDC::GMemDC(int x, int y, GColourSpace cs)
{
	d = new GMemDCPrivate;
	if (x && y && cs)
	{
		Create(x, y, cs);
	}
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	if (pDC && Create(pDC->X(), pDC->Y(), pDC->GetBits()))
	{
		Blt(0, 0, pDC);
		if (pDC->Palette())
		{
			GPalette *Pal = new GPalette(pDC->Palette());
			if (Pal)
			{
				Palette(Pal, TRUE);
			}
		}
	}
}

GMemDC::~GMemDC()
{
	DeleteObj(d);
}

/*
OsPainter GMemDC::Handle()
{
	return d->View;
}
*/

OsBitmap GMemDC::GetBitmap()
{
	return d->Bmp;
}

void GMemDC::SetClient(GRect *c)
{
}

bool GMemDC::SupportsAlphaCompositing()
{
	return true;
}

void GMemDC::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
}

#include "Lgi.h"

bool GMemDC::Create(int x, int y, GColourSpace Cs, int Flags)
{
	bool Status = FALSE;
	
	if (x < 1) x = 1;
	if (y < 1) y = 1;

	color_space Mode = B_RGB24;
	switch (Cs)
	{
		case CsIndex8:
		{
			Mode = B_CMAP8;
			break;
		}
		case CsRgb15:
		{
			Mode = B_RGB15;
			break;
		}
		case CsRgb16:
		{
			Mode = B_RGB16;
			break;
		}
		case CsRgb24:
		{
			Mode = B_RGB32;
			break;
		}
		case CsRgba32:
		{
			Mode = B_RGB32;
			break;
		}
	}

	DeleteObj(d->Bmp);
	BRect r(0, 0, x-1, y-1);
	d->Bmp = new BBitmap(r, Mode, true, true);
	if (d->Bmp)
	{
		d->View = new BView(r, "GMemDC", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
		if (d->View)
		{
			d->Bmp->AddChild(d->View);
		}
		
		pMem = new GBmpMem;
		if (pMem)
		{
			pMem->Base = (uchar*) d->Bmp->Bits();
			pMem->x = x;
			pMem->y = y;
			ColourSpace = pMem->Cs = Cs;
			pMem->Line = d->Bmp->BytesPerRow();
			pMem->Flags = 0;
			
			// Msg("Base: %p x: %i y: %i Bits: %i Line: %i", pMem->Base, pMem->x, pMem->y, pMem->Bits, pMem->Line);
			Status = true;

			int NewOp = (pApp) ? Op() : GDC_SET;

			if ( (Flags & GDC_OWN_APPLICATOR) &&
				!(Flags & GDC_CACHED_APPLICATOR))
			{
				DeleteObj(pApp);
			}

			for (int i=0; i<GDC_CACHE_SIZE; i++)
			{
				DeleteObj(pAppCache[i]);
			}

			if (NewOp < GDC_CACHE_SIZE && !DrawOnAlpha())
			{
				pApp = (pAppCache[NewOp]) ? pAppCache[NewOp] : pAppCache[NewOp] = CreateApplicator(NewOp);
				Flags &= ~GDC_OWN_APPLICATOR;
				Flags |= GDC_CACHED_APPLICATOR;
			}
			else
			{
				pApp = CreateApplicator(NewOp);
				Flags &= ~GDC_CACHED_APPLICATOR;
				Flags |= GDC_OWN_APPLICATOR;
			}
			
			if (!pApp)
			{
				LgiTrace("Couldn't create applicator (%ix%i,%i)\n", x, y, Cs);
				LgiAssert(0);
			}

			Clip.ZOff(x-1, y-1);
		}
	}

	return Status;
}

void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (Src)
	{
		if (Src->IsScreen())
		{
			// screen to memory Blt
		}
		else
		{
			// memory to memory Blt
			GSurface::Blt(x, y, Src, a);
		}
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
}

void GMemDC::HLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LgiSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (	x1 <= x2 &&
		y >= Clip.y1 &&
		y <= Clip.y2)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x1, y);
		for (; x1 <= x2; x1++)
		{
			if (x1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncX();
		}

		pApp->c = Prev;
	}
}

void GMemDC::VLine(int x, int y1, int y2, COLOUR a, COLOUR b)
{
	if (y1 > y2) LgiSwap(y1, y2);
	
	if (y1 < Clip.y1) y1 = Clip.y1;
	if (y2 > Clip.y2) y2 = Clip.y2;
	if (y1 <= y2 &&
		x >= Clip.x1 &&
		x <= Clip.x2)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x, y1);
		for (; y1 <= y2; y1++)
		{
			if (y1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncY();
		}

		pApp->c = Prev;
	}
}

bool GMemDC::Lock()
{
	return true;
}

bool GMemDC::Unlock()
{
	return true;
}

