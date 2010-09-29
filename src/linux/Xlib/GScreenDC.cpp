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

#include "Gdc2.h"

class GScreenPrivate
{
public:
	XPainter p;
	XWidget *View;
	COLOUR Col;

	GScreenPrivate()
	{
		View = 0;
		Col = 0;
	}
};

// Translates are cumulative... so we undo the last one before resetting it.
#define UnTranslate()		d->p.translate(OriginX, OriginY);
#define Translate()			d->p.translate(-OriginX, -OriginY);

/////////////////////////////////////////////////////////////////////////////////////////////////////
GScreenDC::GScreenDC()
{
	d = new GScreenPrivate;
}

GScreenDC::GScreenDC(OsView view, void *param)
{
	d = new GScreenPrivate;
	d->View = view;
	d->p.Begin(d->View);
}

GScreenDC::~GScreenDC()
{
	UnTranslate();
	d->p.End();
	DeleteObj(d);
}

void GScreenDC::SetClient(GRect *c)
{
	LgiAssert(0);
}

XPainter *GScreenDC::Handle()
{
	return &d->p;
}

bool GScreenDC::IsScreen()
{
	return true;
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
	// UnTranslate();

	if (c)
	{
		Clip = *c;
		d->p.EmptyClip();
		d->p.PushClip(	Clip.x1,
						Clip.y1,
						Clip.x2,
						Clip.y2);
	}
	else
	{
		d->p.EmptyClip();
	}

	// Translate();
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
	// QColor q(R24(c), G24(c), B24(c));

	d->p.setFore(c);
	d->p.setBack(c);

	return Prev;
}

int GScreenDC::Op(int ROP)
{
	int Prev = Op();

	switch (ROP)
	{
		case GDC_SET:
		{
			d->p.setRasterOp(XPainter::CopyROP);
			break;
		}
		case GDC_OR:
		{
			d->p.setRasterOp(XPainter::OrROP);
			break;
		}
		case GDC_AND:
		{
			d->p.setRasterOp(XPainter::AndROP);
			break;
		}
		case GDC_XOR:
		{
			d->p.setRasterOp(XPainter::XorROP);
			break;
		}
	}

	return Prev;
}

int GScreenDC::Op()
{
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

	return GDC_SET;
}

int GScreenDC::X()
{
	return d->p.X();
}

int GScreenDC::Y()
{
	return d->p.Y();
}

int GScreenDC::GetBits()
{
	return GdcD->GetBits();
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
	d->p.drawPoint(x, y);
}

COLOUR GScreenDC::Get(int x, int y)
{
	return 0;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	d->p.drawLine(x1, y, x2, y);
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	d->p.drawLine(x, y1, x, y2);
}

void GScreenDC::Line(int x1, int y1, int x2, int y2)
{
	d->p.drawLine(x1, y1, x2, y2);
}

void GScreenDC::Circle(double cx, double cy, double radius)
{
	d->p.drawArc(cx, cy, radius);
}

void GScreenDC::FilledCircle(double cx, double cy, double radius)
{
	d->p.fillArc(cx, cy, radius);
}

void GScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	d->p.drawArc(cx, cy, radius, start, end);
}

void GScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	d->p.fillArc(cx, cy, radius, start, end);
}

void GScreenDC::Ellipse(double cx, double cy, double x, double y)
{
}

void GScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
}

void GScreenDC::Box(int x1, int y1, int x2, int y2)
{
	d->p.drawLine(x1, y1, x2-1, y1);
	d->p.drawLine(x2, y1, x2, y2-1);
	d->p.drawLine(x2, y2, x1+1, y2);
	d->p.drawLine(x1, y1+1, x1, y2);
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
	if (x2 >= x1 AND
		y2 >= y1)
	{
		d->p.drawRect(x1, y1, x2-x1+1, y2-y1+1);
	}
}

void GScreenDC::Rectangle(GRect *a)
{
	if (a)
	{
		if (a->X() > 0 AND
			a->Y() > 0)
		{
			d->p.drawRect(a->x1, a->y1, a->X(), a->Y());
		}
	}
	else
	{
		d->p.drawRect(0, 0, X(), Y());
	}
}

void GScreenDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (Src)
	{
		if (Src->IsScreen())
		{
		}
		else
		{
			GRect s;
			if (!a)
			{
				s.ZOff(Src->X()-1, Src->Y()-1);
				a = &s;
			}

			// memory -> screen blt
			if (Src->GetBitmap())
			{
				// XImage's
				if (Src->GetBits() != GetBits())
				{
					// Do on the fly depth conversion...
					GSurface *t = new GMemDC;
					if (t)
					{
						if (t->Create(a->X(), a->Y(), GetBits()))
						{
							t->GSurface::Blt(0, 0, Src, a);
							
							d->p.drawImage(	x + a->x1, y + a->y1,
											*t->GetBitmap(),
											0, 0, a->X(), a->Y(),
											XBitmapImage::ColorOnly);
						}
						
						DeleteObj(t);
					}
				}
				else
				{
					d->p.drawImage(x, y, *Src->GetBitmap(), a->x1, a->y1, a->X(), a->Y(), XBitmapImage::ColorOnly);
				}
			}
			else
			{
				// Pixmap -> Screen
				GMemDC *Mem = dynamic_cast<GMemDC*>(Src);
				if (Mem AND Mem->GetPixmap())
				{
					GRect *Client = Handle()->GetClient();
					int Ox = Client ? Client->x1 : 0;
					int Oy = Client ? Client->y1 : 0;

					XObject o;
					Display *Dsp = o.XDisplay();
					
					Pixmap Src = Mem->GetPixmap();
					Pixmap Msk = Mem->GetPixmapMask();
					
					XGCValues v;
					v.clip_x_origin = -a->x1 + (x - OriginX) + Ox;
					v.clip_y_origin = -a->y1 + (y - OriginY) + Oy;
					v.clip_mask = Mem->GetPixmapMask();
					int Flags = 0;
					if (v.clip_mask)
					{
						Flags |= GCClipMask | GCClipXOrigin | GCClipYOrigin;
					}
					
					GC Gc = XCreateGC(	Dsp,
										Src,
										Flags,
										&v);
					if (Gc)
					{
						XCopyArea(	Dsp,
									Src,
									d->p.Handle()->handle(),
									Gc,
									a->x1, a->y1, a->X(), a->Y(),
									x - OriginX + Ox, y - OriginY + Oy);
						XFreeGC(Dsp, Gc);
					}
				}
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

