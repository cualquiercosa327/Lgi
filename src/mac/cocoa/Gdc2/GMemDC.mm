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
#include "GdiLeak.h"
#include "GPalette.h"

#define AlphaType		kCGImageAlphaPremultipliedLast

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	NSImage *Img;
	NSBitmapImageRep *Bmp;
	GRect Client;
	
	GMemDCPrivate()
	{
		Img = nil;
		Bmp = nil;
		Client.ZOff(-1, -1);
	}
	
	~GMemDCPrivate()
	{
		Empty();
	}
	
	void Empty()
	{
		if (Img)
		{
			[Img release];
			Img = nil;
		}
		Bmp = nil;
	}
};

GMemDC::GMemDC(int x, int y, GColourSpace cs, int flags)
{
	d = new GMemDCPrivate;
	pMem = 0;
	
	if (x && y && cs)
	{
		Create(x, y, cs, flags);
	}
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	pMem = 0;
	
	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetColourSpace()) )
	{
		if (pDC->Palette())
		{
			Palette(new GPalette(pDC->Palette()));
		}
		
		Blt(0, 0, pDC);
		
		if (pDC->AlphaDC() &&
			HasAlpha(true))
		{
			pAlphaDC->Blt(0, 0, pDC->AlphaDC());
		}
	}
}

GMemDC::~GMemDC()
{
	DeleteObj(pMem);
	DeleteObj(d);
}

bool GMemDC::SupportsAlphaCompositing()
{
	return true;
}

GRect GMemDC::ClipRgn(GRect *Rgn)
{
	GRect Old = Clip;
	
	if (Rgn)
	{
		GRect Dc(0, 0, X()-1, Y()-1);
		
		Clip = *Rgn;
		Clip.Offset(-OriginX, -OriginY);
		Clip.Bound(&Dc);
	}
	else
	{
		Clip.ZOff(X()-1, Y()-1);
	}
	
	return Old;
}

OsBitmap GMemDC::GetBitmap()
{
	return OsBitmap(d->Img);
}

OsPainter GMemDC::Handle()
{
	return NULL;
}

bool GMemDC::Lock()
{
	return true;
}

bool GMemDC::Unlock()
{
	return true;
}

bool GMemDC::Create(int x, int y, GColourSpace Cs, int Flags)
{
	bool Status = false;
	
	d->Empty();
	
	if (x > 0 && y > 0 && Cs != CsNone)
	{
		int Bits = GColourSpaceToBits(Cs);
		// int LineLen = ((Bits * x + 31) / 32) * 4;
		if (Bits > 16)
		{
		}
		
		pMem = new GBmpMem;
		if (pMem)
		{
			pMem->Flags = 0;
			pMem->x = x;
			pMem->y = y;
			pMem->Cs = CsNone;

			NSSize Sz = {(double)x, (double)y};
			d->Img = [[NSImage alloc] initWithSize:Sz];
			/*
			d->Bmp = [NSBitmapImageRep imageRepWithData:[d->Img TIFFRepresentation]];
			if (d->Bmp && !pMem->Base)
			{
				pMem->Line = ((Bits * x + 31) / 32) * 4;
				pMem->Flags |= GBmpMem::BmpOwnMemory;
				pMem->Cs = Cs;
			}
			*/
			
			ColourSpace = pMem->Cs;
			
			int NewOp = (pApp) ? Op() : GDC_SET;
			
			if ((Flags & GDC_OWN_APPLICATOR) && !(Flags & GDC_CACHED_APPLICATOR))
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
			
			Clip.ZOff(X()-1, Y()-1);
			
			Status = true;
		}
	}
	
	return Status;
}

#if 0
CGColorSpaceRef GMemDC::GetColourSpaceRef()
{
	return d->Cs;
}
#endif

void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
		return;
	
	if (Src->IsScreen())
	{
		LgiAssert(!"Impl me.");
	}
	else
	{
		GSurface::Blt(x, y, Src, a);
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
	if (Src)
	{
		GRect DestR;
		if (d)
		{
			DestR = *d;
		}
		else
		{
			DestR.ZOff(X()-1, Y()-1);
		}
		
		GRect SrcR;
		if (s)
		{
			SrcR = *s;
		}
		else
		{
			SrcR.ZOff(Src->X()-1, Src->Y()-1);
		}
		
	}
}

void GMemDC::HorzLine(int x1, int x2, int y, COLOUR a, COLOUR b)
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

void GMemDC::VertLine(int x, int y1, int y2, COLOUR a, COLOUR b)
{
	if (y1 > y2) LgiSwap(y1, y2);
	
	if (y1 < Clip.y1) y1 = Clip.y1;
	if (y2 > Clip.y2) y2 = Clip.y2;
	if (	y1 <= y2 &&
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

void GMemDC::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
}

void GMemDC::SetClient(GRect *c)
{
	if (c)
	{
		GRect Doc(0, 0, pMem->x-1, pMem->y-1);
		Clip = d->Client = *c;
		Clip.Bound(&Doc);
		
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		OriginX = 0;
		OriginY = 0;
		Clip.ZOff(pMem->x-1, pMem->y-1);
	}
}
