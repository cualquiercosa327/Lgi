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
	GColourSpace CreateCs;

    GMemDCPrivate()
    {
		Client.ZOff(-1, -1);
		Img = 0;
		Surface = 0;
		CreateCs = CsNone;
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

GMemDC::GMemDC(int x, int y, GColourSpace cs)
{
	d = new GMemDCPrivate;
	if (cs != CsNone)
		Create(x, y, cs);
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

cairo_surface_t *GMemDC::GetSurface(GRect &r)
{
	if (!d->Img)
		return NULL;

	cairo_format_t fmt = CAIRO_FORMAT_ARGB32;
	switch (d->Img->depth)
	{
		case 8: fmt = CAIRO_FORMAT_A8; break;
		case 16: fmt = CAIRO_FORMAT_RGB16_565; break;
		case 24: fmt = CAIRO_FORMAT_RGB24; break;
		case 32: fmt = CAIRO_FORMAT_ARGB32; break;
		default:
			printf("%s:%i - '%i' bit depth that cairo supports\n", _FL, d->Img->depth);
			return NULL;
	}

	int pixel_bytes = GColourSpaceToBits(ColourSpace) >> 3;
	return cairo_image_surface_create_for_data(	((uchar*)d->Img->mem) + (r.y1 * d->Img->bpl) + (r.x1 * pixel_bytes),
												fmt,
												r.X(),
												r.Y(),
												d->Img->bpl);
}

cairo_t *GMemDC::GetCairo()
{
	if (!Cairo)
	{
		cairo_format_t fmt = CAIRO_FORMAT_ARGB32;
		int bits;
		if (d->Img)
			bits = d->Img->depth;
		else if (pMem)
			bits = GColourSpaceToBits(pMem->Cs);
		else
			return NULL;

		switch (bits)
		{
			case 8: fmt = CAIRO_FORMAT_A8; break;
			case 16: fmt = CAIRO_FORMAT_RGB16_565; break;
			case 24: fmt = CAIRO_FORMAT_RGB24; break;
			case 32: fmt = CAIRO_FORMAT_ARGB32; break;
			default:
				printf("%s:%i - '%i' bit depth that cairo supports\n", _FL, bits);
				return NULL;
		}

		if (!d->Surface)
		{
			if (d->Img)
			{
				d->Surface = cairo_image_surface_create_for_data((uchar*)d->Img->mem,
																fmt,
																d->Img->width, d->Img->height,
																d->Img->bpl);
			}
			else if (pMem)
			{
				d->Surface = cairo_image_surface_create_for_data((uchar*)pMem->Base,
																fmt,
																pMem->x, pMem->y,
																pMem->Line);
			}
			else return NULL;
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

bool GMemDC::SupportsAlphaCompositing()
{
	// We can blend RGBA into memory buffers, mostly because the code is in Lgi not GTK.
	return true;
}

GdkImage *GMemDC::GetImage()
{
    return d->Img;
}

GdcPt2 GMemDC::GetSize()
{
	return GdcPt2(pMem->x, pMem->y);
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
	return false;
}

bool GMemDC::Unlock()
{
	return false;
}

void GMemDC::SetOrigin(int x, int y)
{
	Handle();
	UnTranslate();
	GSurface::SetOrigin(x, y);
	Translate();
}

GColourSpace GMemDC::GetCreateCs()
{
	// Sometimes the colour space we get is different to the requested colour space.
	// This function returns the original requested colour space.
	return d->CreateCs;
}

bool GMemDC::Create(int x, int y, GColourSpace Cs, int Flags)
{
	int Bits = GColourSpaceToBits(Cs);
	if (x < 1 || y < 1 || Bits < 1)
		return false;
	if (Bits < 8)
		Bits = 8;

	Empty();
	d->CreateCs = Cs;
	
	GdkVisual Fallback;
	GdkVisual *Vis = gdk_visual_get_system();
	GColourSpace VisCs = Vis ? GdkVisualToColourSpace(Vis, Bits) : CsNone;
	if (Bits == 8)
	{
		GdkVisual *Vis8 = gdk_visual_get_best_with_depth(8);
		if (Vis8)
		{
			// Yay an 8 bit visual!
		    Vis = Vis8;
		}
		else
		{
			Vis = NULL;
	    }
	}
	
	if (Vis &&
		VisCs != Cs &&
		(Flags & SurfaceRequireExactCs) != 0)
	{
		Vis = NULL;
	}
	
	if (Vis)
	{
		d->Img = gdk_image_new(	GDK_IMAGE_FASTEST,
								Vis,
								x,
								y);
	}
	else
	{
		d->Img = NULL;
	}

	if (!pMem)
		pMem = new GBmpMem;
	if (!pMem)
		return false;

	pMem->x = x;
	pMem->y = y;
	pMem->Flags = 0;
	if (d->Img)
	{
		// Use the GdkImage memory
		pMem->Line = d->Img->bpl;
		pMem->Base = (uchar*)d->Img->mem;
		pMem->Cs = GdkVisualToColourSpace(d->Img->visual, d->Img->bits_per_pixel);
	}
	else
	{
		// Generate our own memory
		pMem->Line = (((pMem->x * Bits) + 31) / 32) << 2;
		pMem->Base = new uchar[pMem->y * pMem->Line];
		pMem->Cs = Cs;
		pMem->Flags |= GBmpMem::BmpOwnMemory;
	}
	
	ColourSpace = pMem->Cs;

	#if 0
	if (Vis && d->Img)
		printf("GMemDC::Create(%i,%i,%i) gdk_image_new(vis=%i,%i,%i,%i) img(%i,%i,%p) cs=%s\n",
			x, y, Bits,
			Vis->depth, Vis->byte_order, Vis->colormap_size, Vis->bits_per_rgb,
			d->Img->bits_per_pixel,
			d->Img->bpl,
			d->Img->mem,
			GColourSpaceToString(ColourSpace));
	#endif

	#if 0
	printf("Created gdk image %ix%i @ %i bpp line=%i (%i) ptr=%p Vis=%p\n",
		pMem->x,
		pMem->y,
		pMem->Bits,
		pMem->Line,
		pMem->Bits*pMem->x/8,
		pMem->Base,
		Vis);
	#endif

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
		pApp = CreateApplicator(NewOp, pMem->Cs);
		Flags &= ~GDC_CACHED_APPLICATOR;
		Flags |= GDC_OWN_APPLICATOR;
	}

	if (!pApp)
	{
		printf("GMemDC::Create(%i,%i,%i) No Applicator.\n", x, y, Bits);
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
			GdkWindow *root_window = gdk_get_default_root_window();
			if (root_window)
			{
				gint x_orig, y_orig;
				gint width, height;

				gdk_drawable_get_size(root_window, &width, &height);      
				gdk_window_get_origin(root_window, &x_orig, &y_orig);
				gdk_drawable_copy_to_image(	root_window,
											d->Img,
											br.SrcClip.x1,
											br.SrcClip.y1,
											x,
											y,
											br.SrcClip.X(),
											br.SrcClip.Y());
				
				// Call the capture screen handler to draw anything between the screen and
				// cursor layers
				OnCaptureScreen();
				
				if (TestFlag(Flags, GDC_CAPTURE_CURSOR))
				{
					// Capture the cursor as well...
				}
				
				Status = true;
			}
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
	if (x1 <= x2 &&
		y >= Clip.y1 &&
		y <= Clip.y2 &&
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
	if (y1 <= y2 &&
		x >= Clip.x1 &&
		x <= Clip.x2 &&
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
