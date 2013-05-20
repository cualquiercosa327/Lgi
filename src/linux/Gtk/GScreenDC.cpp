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
using namespace Gtk;

class GScreenPrivate
{
public:
	int x, y, Bits;
	bool Own;
	GColour Col;
	GRect Client;

	OsView v;
	GdkDrawable *d;
	GdkGC *gc;

	GScreenPrivate()
	{
		x = y = Bits = 0;
		Own = false;
		v = 0;
		d = 0;
		gc = 0;
		Client.ZOff(-1, -1);
	}
	
	~GScreenPrivate()
	{
		if (gc)
			g_object_unref((Gtk::GObject*)g_type_check_instance_cast((Gtk::GTypeInstance*)gc, G_TYPE_OBJECT));
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

GScreenDC::GScreenDC(OsView View)
{
	d = new GScreenPrivate;
	d->v = View;
	d->d = View->window;	
	d->x = View->allocation.width;
	d->y = View->allocation.height;	
	if (d->gc = gdk_gc_new(View->window))
	{
	    GdkScreen *s = gdk_gc_get_screen(d->gc);
	    if (s)
	    {
	        GdkVisual *v = gdk_screen_get_system_visual(s);
	        if (v)
	        {
	            d->Bits = v->depth;
		        ColourSpace = GdkVisualToColourSpace(v, v->depth);
	        }
	    }
	}	
}

GScreenDC::GScreenDC(int x, int y, int bits)
{
	d = new GScreenPrivate;
	d->x = x;
	d->y = y;
	d->Bits = bits;
}

GScreenDC::GScreenDC(Gtk::GdkDrawable *Drawable)
{
	d = new GScreenPrivate;
	d->Own = false;
}

GScreenDC::GScreenDC(GView *view, void *param)
{
	d = new GScreenPrivate;
	if (view)
	{
		d->x = view->X();
		d->y = view->Y();
		d->Bits = 0;
		d->Own = false;

	    GdkScreen *s = gdk_display_get_default_screen(gdk_display_get_default());
	    if (s)
	    {
	        GdkVisual *v = gdk_screen_get_system_visual(s);
	        if (v)
	        {
	            d->Bits = v->depth;
		        ColourSpace = GdkVisualToColourSpace(v, v->depth);
	        }
	    }
    }
}

GScreenDC::~GScreenDC()
{
	UnTranslate();
	
	DeleteObj(d);
}

Gtk::cairo_t *GScreenDC::GetCairo()
{
	if (!Cairo)
	{
		Cairo = gdk_cairo_create(d->d);
	}
	return Cairo;
}

bool GScreenDC::SupportsAlphaCompositing()
{
	// GTK/X11 doesn't seem to support alpha compositing.
	return false;
}

GdcPt2 GScreenDC::GetSize()
{
	return GdcPt2(d->x, d->y);
}

Gtk::GdkDrawable *GScreenDC::GetDrawable()
{
  return 0;
}

void GScreenDC::SetClient(GRect *c)
{
	if (c)
	{
		d->Client = *c;

        GdkRectangle r = {c->x1, c->y1, c->X(), c->Y()};
        gdk_gc_set_clip_rectangle(d->gc, &r);
	}
	else
	{
		d->Client.ZOff(-1, -1);

        GdkRectangle r = {0, 0, X(), Y()};
        gdk_gc_set_clip_rectangle(d->gc, &r);
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
	return 0;
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
		// LgiTrace("Setting clip %s client=%s\n", Clip.GetStr(), d->Client.GetStr());

        GdkRectangle r = {c->x1+d->Client.x1, c->y1+d->Client.y1, c->X(), c->Y()};
        gdk_gc_set_clip_rectangle(d->gc, &r);
	}
	else
	{
	    Clip.ZOff(-1, -1);
		// LgiTrace("Removing clip\n");

        GdkRectangle r = {d->Client.x1, d->Client.y1, X(), Y()};
        gdk_gc_set_clip_rectangle(d->gc, &r);
	}

	Translate();
	return Prev;
}

COLOUR GScreenDC::Colour()
{
	return d->Col.Get(GetBits());
}

COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = Colour();

	d->Col.Set(c, Bits ? Bits : d->Bits);

	if (d->gc)
	{
		int r = d->Col.r();
		int g = d->Col.g();
		int b = d->Col.b();
		GdkColor col = { 0, (r<<8)|r, (g<<8)|g, (b<<8)|b };
		gdk_gc_set_rgb_fg_color(d->gc, &col);
		gdk_gc_set_rgb_bg_color(d->gc, &col);
	}

	return Prev;
}

GColour GScreenDC::Colour(GColour c)
{
	GColour Prev = d->Col;
	d->Col = c;
	if (d->gc)
	{
		int r = c.r();
		int g = c.g();
		int b = c.b();
		GdkColor col = { 0, (r<<8)|r, (g<<8)|g, (b<<8)|b };
		gdk_gc_set_rgb_fg_color(d->gc, &col);
		gdk_gc_set_rgb_bg_color(d->gc, &col);
	}

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
	gdk_draw_point(d->d, d->gc, x-OriginX, y-OriginY);
}

COLOUR GScreenDC::Get(int x, int y)
{
	return 0;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	gdk_draw_line(d->d, d->gc, x1-OriginX, y-OriginY, x2-OriginX, y-OriginY);
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	gdk_draw_line(d->d, d->gc, x-OriginX, y1-OriginY, x-OriginX, y2-OriginY);
}

void GScreenDC::Line(int x1, int y1, int x2, int y2)
{
	gdk_draw_line(d->d, d->gc, x1-OriginX, y1-OriginY, x2-OriginX, y2-OriginY);
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
	gdk_draw_rectangle(d->d, d->gc, false, x1-OriginX, y1-OriginY, x2-x1, y2-y1);
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
		gdk_draw_rectangle(d->d, d->gc, true, x1-OriginX, y1-OriginY, x2-x1+1, y2-y1+1);
	}
}

void GScreenDC::Rectangle(GRect *a)
{
	if (a)
	{
		if (a->X() > 0 &&
			a->Y() > 0)
		{
			gdk_draw_rectangle(d->d, d->gc, true, a->x1-OriginX, a->y1-OriginY, a->X(), a->Y());
		}
	}
	else
	{
		gdk_draw_rectangle(d->d, d->gc, true, -OriginX, -OriginY, X(), Y());
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
	GBlitRegions br(this, x-OriginX, y-OriginY, Src, a);
	if (!br.Valid())
	{
		return;
	}

	GMemDC *Mem;
	if (Mem = dynamic_cast<GMemDC*>(Src))
	{
		/*
		printf("Doing Mem(%s)->Screen(%s) Blt\n",
			GColourSpaceToString(Mem->GetColourSpace()),
			GColourSpaceToString(GetColourSpace()));
		*/
			
		switch (Mem->GetColourSpace())
		{
			/* This doesn't work anyway :-(
			case CsRgba32:
			case CsBgra32:
			case CsArgb32:
			case CsAbgr32:
			{
				cairo_surface_t *src = Mem->GetSurface(br.SrcClip);
				if (src)
				{
					int px = x - OriginX;
					int py = y - OriginY;
					
					cairo_t *dest = gdk_cairo_create(d->d);
					cairo_set_source_surface(dest, src, px, py);
					// cairo_set_source_rgb(dest, 1, 0, 0);
					cairo_rectangle(dest,
									px, py,
									br.DstClip.X(),
									br.DstClip.Y());
					cairo_fill(dest);
					cairo_destroy(dest);
					cairo_surface_destroy(src);
					break;
				}
				// else fall through
			}
			*/
			default:
			{
				gdk_draw_image( d->d,
								d->gc,
								Mem->GetImage(),
								br.SrcClip.x1, br.SrcClip.y1,
								x-OriginX, y-OriginY,
								br.SrcClip.X(), br.SrcClip.Y());
				break;
			}
		}
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

