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

	GView *View;
	OsView v;
	GdkDrawable *d;
	GdkGC *gc;

	GScreenPrivate()
	{
		View = NULL;
		x = y = Bits = 0;
		Own = false;
		v = 0;
		d = NULL;
		gc = NULL;
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
	d->x = GdcD->X();
	d->y = GdcD->Y();
}

/*
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
	
	// printf("%s:%i %p, %ix%i, %i\n", _FL, View, d->x, d->y, d->Bits);
}
*/

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
	d->d = Drawable;
	if (d->gc = gdk_gc_new(Drawable))
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

GScreenDC::GScreenDC(GView *view, void *param)
{
	d = new GScreenPrivate;
	d->View = view;
	if (view)
	{
		OsView v = view->Handle();
		if (v)
		{
			d->v = v;
			d->d = v->window;	
			d->x = v->allocation.width;
			d->y = v->allocation.height;	
			if (d->gc = gdk_gc_new(v->window))
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
	
			/*
			d->d = v->window;
			if (d->gc = gdk_gc_new(v->window))
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
			*/
		}
		else
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
    else
    {
    	printf("%s:%i - No view?\n", _FL);
    }
}

GScreenDC::~GScreenDC()
{
	UnTranslate();
	
	DeleteObj(d);
}

OsPainter GScreenDC::Handle()
{
	if (!Cairo)
	{
		Cairo = gdk_cairo_create(d->d);
		if (Cairo)
		{
			// cairo_reset_clip(Cairo);

			double x1, y1, x2, y2;
			cairo_clip_extents (Cairo, &x1, &y1, &x2, &y2);
			
			int x = (int) (x2 - x1);
			int y = (int) (y2 - y1);
			
			if (d->View &&
				d->View->_Debug)
			{
				int width, height;
				gdk_drawable_get_size (d->d, &width, &height);
				
				printf("%s:%i %s %g,%g,%g,%g %i,%i  %i,%i  %i,%i\n",
					_FL,
					d->View ? d->View->GetClass() : NULL,
					x1,y1,x2,y2,
					x,y,
					d->x, d->y,
					width, height);
			}
		}
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

bool GScreenDC::GetClient(GRect *c)
{
	if (!c)
		return false;

	*c = d->Client;
	return true;
}

void GScreenDC::SetClient(GRect *c)
{
	if (c)
	{
		d->Client = *c;

        GdkRectangle r = {c->x1, c->y1, c->X(), c->Y()};
        gdk_gc_set_clip_rectangle(d->gc, &r);

		OriginX = -c->x1;
		OriginY = -c->y1;	
	}
	else
	{
		OriginX = 0;
		OriginY = 0;	

		d->Client.ZOff(-1, -1);

        GdkRectangle r = {0, 0, X(), Y()};
        gdk_gc_set_clip_rectangle(d->gc, &r);
	}
}

GRect *GScreenDC::GetClient()
{
	return &d->Client;
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
	GColour col(c, Bits ? Bits : GetBits());
	return Colour(col).Get(GetBits());
}

GColour GScreenDC::Colour(GColour c)
{
	GColour Prev = d->Col;
	d->Col = c;
	if (d->gc)
	{
		GdkColor col;
		
		col.pixel = 0;
		
		col.red = c.r();
		col.red |= col.red << 8;
		
		col.green = c.g();
		col.green |= col.green << 8;

		col.blue = c.b();
		col.blue |= col.blue << 8;
		
		// printf("Setting Col %x, %x, %x\n", col.red, col.green, col.blue);
		
		gdk_gc_set_rgb_fg_color(d->gc, &col);
		gdk_gc_set_rgb_bg_color(d->gc, &col);
	}

	return Prev;
}

int GScreenDC::Op(int ROP, NativeInt Param)
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
	gdk_draw_arc(d->d, d->gc, false,
			 	cx - radius, cy - radius,
			 	radius * 2.0,
			 	radius * 2.0,
			 	0,
			 	360 * 64);
}

void GScreenDC::FilledCircle(double cx, double cy, double radius)
{
	gdk_draw_arc(d->d, d->gc, true,
			 	cx - radius, cy - radius,
			 	radius * 2.0,
			 	radius * 2.0,
			 	0,
			 	360 * 64);
}

void GScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	gdk_draw_arc(d->d, d->gc, false,
			 	cx - radius, cy - radius,
			 	radius * 2.0,
			 	radius * 2.0,
			 	start * 64.0,
			 	end * 64.0);
}

void GScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	gdk_draw_arc(d->d, d->gc, true,
			 	cx - radius, cy - radius,
			 	radius * 2.0,
			 	radius * 2.0,
			 	start * 64.0,
			 	end * 64.0);
}

void GScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	gdk_draw_arc(d->d, d->gc, false,
			 	cx - (x / 2), cy - (y / 2),
			 	x,
			 	y,
			 	0,
			 	360 * 64);
}

void GScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	gdk_draw_arc(d->d, d->gc, true,
			 	cx - (x / 2), cy - (y / 2),
			 	x,
			 	y,
			 	0,
			 	360 * 64);
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

void GScreenDC::Polygon(int Points, GdcPt2 *Data)
{
	if (Data)
	{
		::GArray<GdkPoint> pt;
		for (int p=0; p<Points; p++)
		{
			GdkPoint &out = pt.New();
			out.x = Data[p].x;
			out.y = Data[p].y;
		}
		
		gdk_draw_polygon(d->d,
						 d->gc,
						 true,
						 &pt.First(),
						 pt.Length());
	}
}

void GScreenDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
	{
		LgiTrace("%s:%i - No source.\n", _FL);
		return;
	}
	
	if (Src->IsScreen())
	{
		LgiTrace("%s:%i - Can't do screen->screen blt.\n", _FL);
		return;
	}
		
	// memory -> screen blt
	GRect RealClient = d->Client;
	int Dx, Dy;
	Dx = x - OriginX;
	Dy = y - OriginY;
	d->Client.ZOff(-1, -1); // Clear this so the blit rgn calculation uses the
							// full context size rather than just the client.
	GBlitRegions br(this, Dx, Dy, Src, a);
	d->Client = RealClient;
	if (!br.Valid())
	{
		return;
	}

	GMemDC *Mem;
	if (Mem = dynamic_cast<GMemDC*>(Src))
	{
		GMemDC Tmp;
		if (Mem->GetCreateCs() != GetColourSpace() &&
			Mem->GetBits() > 16 &&
			GetBits() <= 16)
		{
			// Do an on the fly colour space conversion... this is slow though
			if (Tmp.Create(br.SrcClip.X(), br.SrcClip.Y(), GetColourSpace()))
			{
				Tmp.Blt(0, 0, Mem, &br.SrcClip);
				printf("On the fly Mem->Scr conversion: %s->%s\n",
					GColourSpaceToString(Mem->GetColourSpace()),
					GColourSpaceToString(GetColourSpace()));

				Mem = &Tmp;
				br.SrcClip = Tmp.Bounds();
			}
			else
			{
				printf("Failed to Mem->Scr Blt: %s->%s\n",
					GColourSpaceToString(Mem->GetColourSpace()),
					GColourSpaceToString(GetColourSpace()));
				return;
			}
		}

		if (d->d && d->gc && Mem->GetImage())
		{
			gdk_draw_image( d->d,
							d->gc,
							Mem->GetImage(),
							br.SrcClip.x1, br.SrcClip.y1,
							Dx, Dy,
							br.SrcClip.X(), br.SrcClip.Y());
		}
		else
		{
			LgiTrace("%s:%i - Error missing d=%p, gc=%p, img=%p\n",
				_FL, d->d, d->gc, Mem->GetImage());
		}
	}
}

void GScreenDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
	LgiAssert(0);
}

void GScreenDC::Bezier(int Threshold, GdcPt2 *Pt)
{
	LgiAssert(0);
}

void GScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, GRect *Bounds)
{
	LgiAssert(0);
}

