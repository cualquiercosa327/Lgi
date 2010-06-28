/*hdr
**	FILE:			GMemDC.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			14/10/2000
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GString.h"
using namespace Gtk;

#define UnTranslate()		// if (d->p) d->p->translate(OriginX, OriginY);
#define Translate()			// if (d->p) d->p->translate(-OriginX, -OriginY);

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define ROUND_UP(bits) (((bits) + 7) / 8)

class GMemDCPrivate
{
public:
	GRect Client;
	GdkImage *Img;
	cairo_surface_t *Surface;

    GMemDCPrivate()
    {
		Client.ZOff(-1, -1);
		Img = 0;
		Surface = 0;
    }

    ~GMemDCPrivate()
    {
		if (Img)
		{
			g_object_unref(Img);
		}
		if (Surface)
		{
			cairo_surface_destroy(Surface);
		}
	}
};

GMemDC::GMemDC(int x, int y, int bits)
{
	d = new GMemDCPrivate;
	if (bits > 0)
		Create(x, y, bits);
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	
	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetBits()))
	{
		Blt(0, 0, pDC);
	}
}

GMemDC::~GMemDC()
{
	Empty();
	DeleteObj(d);
}

cairo_t *GMemDC::GetCairo()
{
	if (!Cairo)
	{
		cairo_format_t fmt = CAIRO_FORMAT_ARGB32;
		switch (d->Img->depth)
		{
			case 8: fmt = CAIRO_FORMAT_A8; break;
			case 24: fmt = CAIRO_FORMAT_RGB24; break;
			case 32: fmt = CAIRO_FORMAT_ARGB32; break;
			default:
				LgiAssert(!"Not a bit depth that cairo supports");
				return 0;
		}

		if (!d->Surface)
		{
			d->Surface = cairo_image_surface_create_for_data((uchar*)d->Img->mem, fmt, d->Img->width, d->Img->height, d->Img->bpl);
			LgiAssert(d->Surface);
		}

		if (d->Surface)
		{
			Cairo = cairo_create(d->Surface);
			LgiAssert(Cairo);
		}
	}

	return Cairo;
}

GdkImage *GMemDC::GetImage()
{
    return d->Img;
}

GdcPt2 GMemDC::GetSize()
{
	return GdcPt2(pMem->x, pMem->y);
}

void GMemDC::SetClient(GRect *c)
{
	if (c)
	{
		d->Client = *c;
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		OriginX = 0;
		OriginY = 0;
	}
}

void GMemDC::Empty()
{
	if (d->Img)
	{
		g_object_unref(d->Img);
		d->Img = 0;
	}
}

bool GMemDC::Lock()
{
	bool Status = false;

	/*
	// Converts a server pixmap to a local bitmap
	if (d->Pix)
	{
		Status = d->ConvertToBmp();
		pMem->Base = d->Bmp->Addr(0);

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
			pApp = CreateApplicator(NewOp, pMem->Bits);
			Flags &= ~GDC_CACHED_APPLICATOR;
			Flags |= GDC_OWN_APPLICATOR;
		}

		LgiAssert(pApp);
	}
	*/
	
	return Status;
}

bool GMemDC::Unlock()
{
	bool Status = false;

	/*
	// Converts the local image to a server pixmap
	if (d->Bmp)
	{
		Status = d->ConvertToPix();
		
		if (Status)
		{
			for (int i=0; i<GDC_CACHE_SIZE; i++)
			{
				if (pAppCache[i] && pAppCache[i] == pApp) pApp = 0;
				DeleteObj(pAppCache[i]);
			}
			DeleteObj(pApp);
			
			pMem->Base = 0;
			
			Status = true;
		}
	}
	*/

	return Status;
}

void GMemDC::SetOrigin(int x, int y)
{
	Handle();
	UnTranslate();
	GSurface::SetOrigin(x, y);
	Translate();
}

bool GMemDC::Create(int x, int y, int Bits, int LineLen, bool KeepData)
{
	if (x < 1 OR y < 1 OR Bits < 1) return false;
	if (Bits < 8) Bits = 8;

	Empty();
	
	GdkVisual Vis = *gdk_visual_get_system();
	if (Bits == 8)
	{
	    Vis.type = GDK_VISUAL_DIRECT_COLOR;
	    Vis.depth = 8;
	}
	
	d->Img = gdk_image_new(	GDK_IMAGE_FASTEST,
							&Vis,
							x,
							y);
	if (!d->Img)
	{
		LgiTrace("%s:%i - Error: failed to create image (%i,%i,%i)\n", _FL, x, y, Bits);
		return false;
	}
	
	if (!pMem)
		pMem = new GBmpMem;
	if (!pMem)
		return false;

	pMem->x = x;
	pMem->y = y;
	pMem->Bits = d->Img->bits_per_pixel;
	pMem->Line = d->Img->bpl;
	pMem->Flags = 0;
	pMem->Base = (uchar*)d->Img->mem;

	printf("Created gdk image %ix%i @ %i bpp line=%i (%i) ptr=%p Vis=%p\n",
		pMem->x,
		pMem->y,
		pMem->Bits,
		pMem->Line,
		pMem->Bits*pMem->x/8,
		pMem->Base,
		Vis);

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
		pApp = CreateApplicator(NewOp, pMem->Bits);
		Flags &= ~GDC_CACHED_APPLICATOR;
		Flags |= GDC_OWN_APPLICATOR;
	}

	if (!pApp)
	{
		printf("GMemDC::Create(%i,%i,%i,%i,%i) No Applicator.\n", x, y, Bits, LineLen, KeepData);
		LgiAssert(0);
	}

	Clip.ZOff(X()-1, Y()-1);

	return true;
}

void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
		return;

	bool Status = false;
	GBlitRegions br(this, x, y, Src, a);
	if (!br.Valid())
		return;

	GScreenDC *Screen;
	if (Screen = Src->IsScreen())
	{
		if (pMem->Base)
		{
			// Screen -> Memory
			Status = true;
		}
		
		if (!Status)
		{
			Colour(Rgb24(255, 0, 255), 24);
			Rectangle();
		}
	}
	else if ((*Src)[0])
	{
		// Memory -> Memory (Source alpha used)
		GSurface::Blt(x, y, Src, a);
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
    LgiAssert(!"Not implemented");
}

void GMemDC::HLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LgiSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (x1 <= x2 AND
		y >= Clip.y1 AND
		y <= Clip.y2 AND
		pApp)
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
	if (y1 <= y2 AND
		x >= Clip.x1 AND
		x <= Clip.x2 AND
		pApp)
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
