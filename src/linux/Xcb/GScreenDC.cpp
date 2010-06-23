/*hdr
**	FILE:			GScreenDC.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			14/10/2000
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Lgi.h"
namespace Pango {
#include "cairo-xcb-xrender.h"
}

class GScreenPrivate
{
public:
	int x, y, Bits;
	OsPainter *Gc;
	xcb_drawable_t h;
	xcb_pixmap_t Pix;
	bool Own;
	COLOUR Col;
	GRect Client;

	GScreenPrivate()
	{
		x = y = Bits = 0;
		Pix = 0;
		h = 0;
		Own = false;
		Col = 0;
		Gc = 0;
		Client.ZOff(-1, -1);
	}
	
	~GScreenPrivate()
	{
		if (Pix)
		{
			xcb_free_pixmap(XcbConn(), Pix);
			Pix = 0;
		}
	}
};

// Translates are cumulative... so we undo the last one before resetting it.
#define UnTranslate()		// d->p.translate(OriginX, OriginY);
#define Translate()			// d->p.translate(-OriginX, -OriginY);

/////////////////////////////////////////////////////////////////////////////////////////////////////
GScreenDC::GScreenDC()
{
	d = new GScreenPrivate;
}

GScreenDC::GScreenDC(int x, int y, int bits)
{
	d = new GScreenPrivate;

	d->h = d->Pix = xcb_generate_id(XcbConn());
	if (!d->h)
		printf("%s:%i - xcb_generate_id failed.\n", _FL);
	else
	{
		#ifdef _DEBUG
		xcb_void_cookie_t c =	xcb_create_pixmap_checked
		#else
								xcb_create_pixmap
		#endif
			(XcbConn(), d->Bits = bits, d->Pix, XcbScreen()->root, d->x = x, d->y = y);
		#ifdef _DEBUG
		if (!XcbCheck(c))
		{
			printf("\tparams=%i,%i,%i\n", d->x, d->y, d->Bits);
		}
		#endif
	}	
}

GScreenDC::GScreenDC(xcb_drawable_t Drawable)
{
	d = new GScreenPrivate;
	d->h = Drawable;
	d->Own = false;
}

GScreenDC::GScreenDC(GView *view, void *param)
{
	d = new GScreenPrivate;
	if (view)
	{
		d->x = view->X();
		d->y = view->Y();
		d->Bits = XcbScreen()->root_depth;
		d->h = view->Handle();
		d->Own = false;

		if (!d->h)
		{
			GViewI *r = view->FindReal(0);
			GView *rg = r ? r->GetGView() : 0;
			if (rg)
			{
				d->Gc = &rg->XcbGC;
				d->h = rg->Handle();
				
				printf("Attaching GScreenDC to real wnd\n");
			}
			else printf("%s:%i - Error, no real view.\n", _FL);
		}
		else
		{
			d->Gc = &view->XcbGC;
		}
		
		if (!(*d->Gc))
		{
			*d->Gc = xcb_generate_id(XcbConn());
			
			uint32_t v = 0;
			XcbCheck(xcb_create_gc(XcbConn(), Handle(), XcbScreen()->root, XCB_GC_GRAPHICS_EXPOSURES, &v));
		}
	}
}

GScreenDC::~GScreenDC()
{
	UnTranslate();
	if (d->Gc)
	{
		xcb_flush(XcbConn());
	}
	DeleteObj(d);
}

extern xcb_visualtype_t *get_root_visual_type(xcb_screen_t *s);

Pango::cairo_surface_t *GScreenDC::GetSurface(bool Render)
{
	if (!Surface)
	{
		if (Render)
		{
			Pango::xcb_render_pictforminfo_t format;
			
			Surface = Pango::cairo_xcb_surface_create_with_xrender_format(XcbConn(),
				      								GetDrawable(),
				      								XcbScreen(),
				      								&format,
				      								d->x,
				      								d->y);
			if (!Surface)
				printf("%s:%i - cairo_xcb_surface_create_with_xrender_format failed.\n", _FL);
		}
		else
		{
			Surface = Pango::cairo_xcb_surface_create(XcbConn(),
													GetDrawable(),
													get_root_visual_type(XcbScreen()),
													d->x,
													d->y);
			if (!Surface)
				printf("%s:%i - cairo_xcb_surface_create failed.\n", _FL);
		}
	}
	
	return Surface;
}

GdcPt2 GScreenDC::GetSize()
{
	return GdcPt2(d->x, d->y);
}

xcb_drawable_t GScreenDC::GetDrawable()
{
  return d->h;
}

void GScreenDC::SetClient(GRect *c)
{
	if (c)
	{
		d->Client = *c;
	}
	else
	{
		d->Client.ZOff(-1, -1);
	}
	
	OriginX = -d->Client.x1;
	OriginY = -d->Client.y1;
}

GRect *GScreenDC::GetClient()
{
	return &d->Client;
}

OsPainter GScreenDC::Handle()
{
	OsPainter p = d->Gc ? *d->Gc : 0;
	if (!p)
	{
		printf("%p::Handle d->Gc=%p Gc=%p\n", this, d->Gc, d->Gc?*d->Gc:0);
	}
	
	LgiAssert(p);
	return p;
}

uint GScreenDC::LineStyle()
{
	return GSurface::LineStyle();
}

uint GScreenDC::LineStyle(uint32 Bits, uint32 Reset)
{
	return GSurface::LineStyle(Bits);
}

int GScreenDC::GetFlags()
{
	return 0;
}

void GScreenDC::GetOrigin(int &x, int &y)
{
	return GSurface::GetOrigin(x, y);
}

void GScreenDC::SetOrigin(int x, int y)
{
	UnTranslate();
	GSurface::SetOrigin(x, y);
	Translate();
}

GRect GScreenDC::ClipRgn()
{
	return Clip;
}

GRect GScreenDC::ClipRgn(GRect *c)
{
	GRect Prev = Clip;
	UnTranslate();

	if (c)
	{
		Clip = *c;

		xcb_rectangle_t r = {c->x1-OriginX, c->y1-OriginY, c->X(), c->Y()};
		xcb_void_cookie_t c =
			xcb_set_clip_rectangles_checked (
				XcbConn(),
				XCB_CLIP_ORDERING_UNSORTED,
				*d->Gc,
				0,
				0,
				1,
				&r);
		XcbCheck(c);
	}
	else
	{
		xcb_rectangle_t r = {0, 0, X(), Y()};
		xcb_void_cookie_t c =
			xcb_set_clip_rectangles_checked (
				XcbConn(),
				XCB_CLIP_ORDERING_UNSORTED,
				*d->Gc,
				0,
				0,
				1,
				&r);
		XcbCheck(c);
	}

	Translate();
	return Prev;
}

COLOUR GScreenDC::Colour()
{
	return d->Col;
}

COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = Colour();

	d->Col = c;

	c = CBit(GetBits(), c, Bits>0 ? Bits : GdcD->GetBits());
	
	uint32_t Values[2] = {c, c};
	XcbCheck(xcb_change_gc(XcbConn(), Handle(), XCB_GC_FOREGROUND|XCB_GC_BACKGROUND, Values));
	
	return Prev;
}

int GScreenDC::Op(int ROP)
{
	int Prev = Op();

	switch (ROP)
	{
		case GDC_SET:
		{
			//d->p.setRasterOp(XPainter::CopyROP);
			break;
		}
		case GDC_OR:
		{
			//d->p.setRasterOp(XPainter::OrROP);
			break;
		}
		case GDC_AND:
		{
			//d->p.setRasterOp(XPainter::AndROP);
			break;
		}
		case GDC_XOR:
		{
			//d->p.setRasterOp(XPainter::XorROP);
			break;
		}
	}

	return Prev;
}

int GScreenDC::Op()
{
	/*
	switch (d->p.rasterOp())
	{
		case XPainter::CopyROP:
		{
			return GDC_SET;
			break;
		}
		case XPainter::OrROP:
		{
			return GDC_OR;
			break;
		}
		case XPainter::AndROP:
		{
			return GDC_AND;
			break;
		}
		case XPainter::XorROP:
		{
			return GDC_XOR;
			break;
		}
	}
	*/

	return GDC_SET;
}

int GScreenDC::X()
{
	return d->Client.Valid() ? d->Client.X() : d->x;
}

int GScreenDC::Y()
{
	return d->Client.Valid() ? d->Client.Y() : d->y;
}

int GScreenDC::GetBits()
{
	return d->Bits;
}

GPalette *GScreenDC::Palette()
{
	return 0;
}

void GScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
}

void GScreenDC::Set(int x, int y)
{
	//d->p.drawPoint(x, y);
}

COLOUR GScreenDC::Get(int x, int y)
{
	return 0;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	xcb_point_t p[2] = {{x1-OriginX, y-OriginY}, {x2-OriginX, y-OriginY}};
	XcbCheck(xcb_poly_line(XcbConn(), XCB_COORD_MODE_ORIGIN, d->h, Handle(), 2, p));
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	xcb_point_t p[2] = {{x-OriginX, y1-OriginY}, {x-OriginX, y2-OriginY}};
	XcbCheck(xcb_poly_line(XcbConn(), XCB_COORD_MODE_ORIGIN, d->h, Handle(), 2, p));
}

void GScreenDC::Line(int x1, int y1, int x2, int y2)
{
	xcb_point_t p[2] = {{x1-OriginX, y1-OriginY}, {x2-OriginX, y2-OriginY}};
	XcbCheck(xcb_poly_line(XcbConn(), XCB_COORD_MODE_ORIGIN, d->h, Handle(), 2, p));
}

void GScreenDC::Circle(double cx, double cy, double radius)
{
	//d->p.drawArc(cx, cy, radius);
}

void GScreenDC::FilledCircle(double cx, double cy, double radius)
{
	//d->p.fillArc(cx, cy, radius);
}

void GScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	//d->p.drawArc(cx, cy, radius, start, end);
}

void GScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	//d->p.fillArc(cx, cy, radius, start, end);
}

void GScreenDC::Ellipse(double cx, double cy, double x, double y)
{
}

void GScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
}

void GScreenDC::Box(int x1, int y1, int x2, int y2)
{
	Line(x1, y1, x2-1, y1);
	Line(x2, y1, x2, y2-1);
	Line(x2, y2, x1+1, y2);
	Line(x1, y1+1, x1, y2);
}

void GScreenDC::Box(GRect *a)
{
	if (a)
	{
		Box(a->x1, a->y1, a->x2, a->y2);
	}
	else
	{
		Box(0, 0, X()-1, Y()-1);
	}
}

void GScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	if (x2 >= x1 &&
		y2 >= y1)
	{
		xcb_rectangle_t r = {x1-OriginX, y1-OriginY, x2-x1+1, y2-y1+1};
		xcb_poly_fill_rectangle(XcbConn(), d->h, Handle(), 1, &r);
	}
}

void GScreenDC::Rectangle(GRect *a)
{
	if (a)
	{
		if (a->X() > 0 &&
			a->Y() > 0)
		{
			xcb_rectangle_t r = {a->x1-OriginX, a->y1-OriginY, a->X(), a->Y()};
			xcb_poly_fill_rectangle(XcbConn(), d->h, Handle(), 1, &r);
		}
	}
	else
	{
		xcb_rectangle_t r = {-OriginX, -OriginY, X(), Y()};
		xcb_poly_fill_rectangle(XcbConn(), d->h, Handle(), 1, &r);
	}
}

void GScreenDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
	{
		printf("%s:%i - No source.\n", _FL);
		return;
	}
	
	if (Src->IsScreen())
	{
		printf("%s:%i - Can't do screen->screen blt.\n", _FL);
		return;
	}
		
	// memory -> screen blt
	GBlitRegions br(this, x, y, Src, a);
	if (!br.Valid())
	{
		/*
		printf("%s:%i - Invalid clip region:\n"
				"\tDst=%i,%i @ %i,%i Src=%i,%i @ %s\n",
				_FL,
				X(), Y(),
				x, y,
				Src->X(), Src->Y(),
				a ? a->GetStr() : 0);
		br.Dump();
		*/
		return;
	}

	if (Src->GetBits() != GetBits())
	{
		// Do on the fly depth conversion...
		GMemDC Tmp(br.SrcClip.X(), br.SrcClip.Y(), GetBits());
		Tmp.Blt(0, 0, Src, &br.SrcClip);
		Blt(x, y, &Tmp);
	}
	else if ((*Src)[0])
	{
		#if 0

		xcb_render_pictforminfo_t format;
		Pango::cairo_surface_t *s =
			cairo_xcb_surface_create_with_xrender_format(XcbConn(),
					      								GetDrawable(xcb_render_pictforminfo_t),
					      								XcbScreen(),
					      								&format,
					      								Src->X(),
					      								Src->Y());
		if (s)
		{
			cairo_set_source_surface(cr, GetSurface(), x, y);
			cairo_paint(cr);
			cairo_surface_destroy(s);
		}
		
		#else
		if (br.SrcClip.X() == Src->X() &&
			br.SrcClip.Y() == Src->Y())
		{
			// Whole image blt
			xcb_void_cookie_t c =
			#ifdef _DEBUG
				xcb_put_image_checked(	
			#else
				xcb_put_image(
			#endif
					XcbConn(),
					XCB_IMAGE_FORMAT_Z_PIXMAP,
					d->h,
					*d->Gc,						
					Src->X(),
					Src->Y(),
					x-OriginX,
					y-OriginY,
					0,
					Src->GetBits(),
					Src->GetLine() * Src->Y(),
					(*Src)[0]);
			XcbCheck(c);
			// printf("Src=%i,%i,%i Line=%i -> %i,%i\n", Src->X(), Src->Y(), Src->GetBits(), Src->GetLine(), x, y);
		}
		else
		{
			// Sub image copy
			int Pixel = Src->GetBits() == 24 ? Pixel24::Size : Src->GetBits() / 8;
			int Line = Pixel * br.SrcClip.X();
			for (int Y=0; Y<br.SrcClip.Y(); Y++)
			{
				xcb_put_image(	XcbConn(),
								XCB_IMAGE_FORMAT_Z_PIXMAP,
								d->h,
								*d->Gc,
								br.SrcClip.X(),
								1,
								x-OriginX,
								y+Y-OriginY,
								0,
								Src->GetBits(),
								Line,
								(*Src)[br.SrcClip.y1+Y] + (br.SrcClip.x1 * Pixel));
			}
		}
		#endif
	}
}

void GScreenDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
}

void GScreenDC::Polygon(int Points, GdcPt2 *Data)
{
}

void GScreenDC::Bezier(int Threshold, GdcPt2 *Pt)
{
}

void GScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, GRect *Bounds)
{
}

