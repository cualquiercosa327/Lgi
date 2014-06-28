/// \file
/// \author Matthew Allen
#include <stdio.h>
#include <math.h>

#include "Lgi.h"

class GScreenPrivate
{
public:
	GWindow *Wnd;
	GView *View;
	CGContextRef Ctx;
	GRect Rc;
	GArray<GRect> Stack;
	COLOUR Cur;
	int Bits;
	
	GScreenPrivate()
	{
		Wnd = 0;
		Bits = GdcD->GetBits();
		Cur = Rgb32(0, 0, 0);
		View = 0;
		Ctx = 0;
		Rc.ZOff(-1, -1);
	}
	
	GRect Client()
	{
		GRect r = Rc;
		r.Offset(-r.x1, -r.y1);
		return r;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
GScreenDC::GScreenDC()
{
	d = new GScreenPrivate;
}

GScreenDC::GScreenDC(GWindow *w, void *param)
{
	d = new GScreenPrivate;
	d->Wnd = w;
	d->Ctx = (CGContextRef)param;
	if (d->Ctx)
	{
		Rect r;
		if (GetWindowBounds(d->Wnd->WindowHandle(), kWindowContentRgn, &r))
		{
			printf("%s:%i - GetWindowBounds failed\n", _FL);
		}
		else
		{
			d->Rc = r;
			d->Rc.Offset(-d->Rc.x1, -d->Rc.y1);
		}
		
		// CGContextTranslateCTM (d->Ctx, 0, d->Rc.Y()-1); 
		// CGContextScaleCTM (d->Ctx, 1.0, -1.0); 
		CGContextSetLineWidth(d->Ctx, 1.0);
	}
	else
	{
		printf("%s:%i - No context.\n", __FILE__, __LINE__);
	}
}

GScreenDC::GScreenDC(GView *v, void *param)
{
	d = new GScreenPrivate;
	d->View = v;
	d->Ctx = (CGContextRef)param;
	if (d->Ctx)
	{
		HIRect r;
		if (d->View && !HIViewGetBounds(d->View->Handle(), &r))
		{
			d->Rc.ZOff((int)r.size.width, (int)r.size.height);
		}
		else
		{
			// dock icon
			d->Rc.ZOff(127, 127);
		}
		
		CGContextSetLineWidth(d->Ctx, 1.0);
	}
	else
	{
		printf("%s:%i - No context.\n", _FL);
	}
}

GScreenDC::~GScreenDC()
{
	DeleteObj(d);
}

bool GScreenDC::SupportsAlphaCompositing()
{
	return true;
}

GView *GScreenDC::GetView()
{
	return d->View;
}

void GScreenDC::SetClient(GRect *c)
{
	// 'c' is in absolute coordinates
	if (d->Ctx)
	{
		if (c)
		{
			CGContextSaveGState(d->Ctx);

			int Ox = 0, Oy = 0;

			d->Stack.Add(d->Rc);
			d->Rc = *c;
			GRect &Last = d->Stack[d->Stack.Length()-1];
			Ox += Last.x1;
			Oy += Last.y1;

			#if 0
			char sp[64];
			memset(sp, ' ', (d->Stack.Length()-1)<<2);
			sp[(d->Stack.Length()-1)<<2] = 0;
			printf("%sSetClient %s %i,%i (", sp, c ? c->GetStr() : "", Ox, Oy);
			for (int i=0; i<d->Stack.Length(); i++)
			{
				printf("%s|", d->Stack[i].GetStr());
			}
			printf(")\n");
			#endif
			
			CGRect rect = {{c->x1-Ox, c->y1-Oy}, {c->X(), c->Y()}};
			CGContextClipToRect(d->Ctx, rect);
			CGContextTranslateCTM(d->Ctx, rect.origin.x, rect.origin.y);
		}
		else
		{
			d->Rc = d->Stack[d->Stack.Length()-1];
			d->Stack.Length(d->Stack.Length()-1);
			CGContextRestoreGState(d->Ctx);
		}
		
	}
	else printf("%s:%i - No context?\n", _FL);
}

int GScreenDC::GetFlags()
{
	return 0;
}

OsPainter GScreenDC::Handle()
{
	return d->Ctx;
}

void GScreenDC::GetOrigin(int &x, int &y)
{
	GSurface::GetOrigin(x, y);
}

void GScreenDC::SetOrigin(int x, int y)
{
	if (d->Ctx && (OriginX != 0 || OriginY != 0))
	{
		CGContextTranslateCTM(d->Ctx, -OriginX, -OriginY);
	}

	GSurface::SetOrigin(x, y);

	if (d->Ctx)
	{
		CGContextTranslateCTM(d->Ctx, x, y);
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

GRect GScreenDC::ClipRgn(GRect *Rgn)
{
	GRect Prev = Clip;

	if (Rgn)
	{
		GRect c = *Rgn;
		GRect Client = d->Client();
		c.Bound(&Client);
		
		CGContextSaveGState(d->Ctx);
		CGRect rect = {{c.x1, c.y1}, {c.X(), c.Y()}};
		CGContextClipToRect(d->Ctx, rect);
	}
	else
	{
		CGContextRestoreGState(d->Ctx);			
	}

	return Prev;
}

GRect GScreenDC::ClipRgn()
{
	return Clip;
}

GColour GScreenDC::Colour(GColour c)
{
	GColour Prev(d->Cur, 32);

	d->Cur = c.c32();
	if (d->Ctx)
	{
		float r = (float)R32(d->Cur)/255.0;
		float g = (float)G32(d->Cur)/255.0;
		float b = (float)B32(d->Cur)/255.0;
		float a = (float)A32(d->Cur)/255.0;
		
		CGContextSetRGBFillColor(d->Ctx, r, g, b, a);
		CGContextSetRGBStrokeColor(d->Ctx, r, g, b, a);
	}

	return Prev;
}

COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = d->Cur;

	d->Cur = CBit(32, c, Bits);
	if (d->Ctx)
	{
		float r = (float)R32(d->Cur)/255.0;
		float g = (float)G32(d->Cur)/255.0;
		float b = (float)B32(d->Cur)/255.0;
		float a = (float)A32(d->Cur)/255.0;
		
		CGContextSetRGBFillColor(d->Ctx, r, g, b, a);
		CGContextSetRGBStrokeColor(d->Ctx, r, g, b, a);
	}

	return Prev;
}

int GScreenDC::Op(int Op)
{
	int Prev = GDC_SET;
	return Prev;
}

COLOUR GScreenDC::Colour()
{
	return CBit(d->Bits, d->Cur, 32);
}

int GScreenDC::Op()
{
	return GDC_SET;
}

int GScreenDC::X()
{
	return d->Rc.X();
}

int GScreenDC::Y()
{
	return d->Rc.Y();
}

int GScreenDC::GetBits()
{
	return d->Bits;
}

void GScreenDC::Set(int x, int y)
{
	if (d->Ctx)
	{
		CGRect r = {{x, y}, {1.0, 1.0}};
		CGContextFillRect(d->Ctx, r);
	}
}

COLOUR GScreenDC::Get(int x, int y)
{
	return 0;
}

uint GScreenDC::LineStyle(uint32 Bits, uint32 Reset)
{
	return LineBits;
}

uint GScreenDC::LineStyle()
{
	return LineBits;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	if (d->Ctx)
	{
		CGRect r = {{x1, y}, {x2-x1+1.0, 1.0}};
		CGContextFillRect(d->Ctx, r);
	}
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{x, y1}, {1.0, y2-y1+1.0}};
		CGContextFillRect(d->Ctx, r);
	}
}

void GScreenDC::Line(int x1, int y1, int x2, int y2)
{
	if (d->Ctx)
	{
		if (y1 == y2)
		{
			// Horizontal
			if (x2 < x1)
			{
				int i = x1;
				x1 = x2;
				x2 = i;
			}
			CGRect r = {{x1, y1}, {x2-x1+1.0, 1.0}};
			CGContextFillRect(d->Ctx, r);
		}
		else if (x1 == x2)
		{
			// Vertical
			if (y2 < y1)
			{
				int i = y1;
				y1 = y2;
				y2 = i;
			}
			CGRect r = {{x1, y1}, {1.0, y2-y1+1.0}};
			CGContextFillRect(d->Ctx, r);
		}
		else
		{
			CGPoint p[] = {{x1+0.5, y1+0.5}, {x2+0.5, y2+0.5}};
			CGContextBeginPath(d->Ctx);
			CGContextAddLines(d->Ctx, p, 2);
			CGContextStrokePath(d->Ctx);
		}
	}
}

void GScreenDC::Circle(double cx, double cy, double radius)
{
	if (d->Ctx)
	{
		CGRect r = {{cx-radius, cy-radius}, {radius*2.0, radius*2.0}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextStrokePath(d->Ctx);
	}
}

void GScreenDC::FilledCircle(double cx, double cy, double radius)
{
	if (d->Ctx)
	{
		CGRect r = {{cx-radius, cy-radius}, {radius*2.0, radius*2.0}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextFillPath(d->Ctx);
	}
}

void GScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
}

void GScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
}

void GScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	if (d->Ctx)
	{
		CGRect r = {{cx-x, cy-y}, {x*2.0, y*2.0}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextStrokePath(d->Ctx);
	}
}

void GScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	if (d->Ctx)
	{
		CGRect r = {{cx-x, cy-y}, {x*2.0, y*2.0}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextFillPath(d->Ctx);
	}
}

void GScreenDC::Box(int x1, int y1, int x2, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{x1+0.5, y1+0.5}, {x2-x1, y2-y1}};
		CGContextSetLineWidth(d->Ctx, 1.0);
		CGContextStrokeRect(d->Ctx, r);
	}
}

void GScreenDC::Box(GRect *a)
{
	if (d->Ctx && a)
	{
		CGRect r = {{a->x1+0.5, a->y1+0.5}, {a->x2-a->x1, a->y2-a->y1}};
		CGContextSetLineWidth(d->Ctx, 1.0);
		CGContextStrokeRect(d->Ctx, r);
	}
}

void GScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{x1, y1}, {x2-x1+1.0, y2-y1+1.0}};
		CGContextFillRect(d->Ctx, r);
	}
}

void GScreenDC::Rectangle(GRect *a)
{
	if (d->Ctx)
	{
		GRect c;
		if (!a)
		{
			c = d->Client();
			a = &c;
		}

		CGRect r = {{a->x1, a->y1}, {a->x2-a->x1+1.0, a->y2-a->y1+1.0}};
		CGContextFillRect(d->Ctx, r);
	}
}

void GScreenDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (Src && d->Ctx)
	{
		GRect b;
		if (a)
		{
			b = *a;
			GRect f(0, 0, Src->X()-1, Src->Y()-1);
			b.Bound(&f);
		}
		else
		{
			b.ZOff(Src->X()-1, Src->Y()-1);
		}
		
		if (b.Valid())
		{
			if (Src->IsScreen())
			{
				// Scroll region...
			}
			else
			{
				// Blt mem->screen
				GMemDC *Mem = dynamic_cast<GMemDC*>(Src);
				if (Mem)
				{
					CGImg *i = Mem->GetImg(a ? &b : 0);
					if (i)
					{
						HIRect r;
						r.origin.x = x;
						r.origin.y = y;
						r.size.width = b.X();
						r.size.height = b.Y();
						CGImageRef Img = *i;
						
						HIViewDrawCGImage(d->Ctx, &r, Img);
						
						DeleteObj(i);
					}
				}
			}
		}
	}
}

void GScreenDC::StretchBlt(GRect *dst, GSurface *Src, GRect *s)
{
	if (Src)
	{
		GRect DestR;
		if (dst)
		{
			DestR = *dst;
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

void GScreenDC::Polygon(int Points, GdcPt2 *Data)
{
}

void GScreenDC::Bezier(int Threshold, GdcPt2 *Pt)
{
}

void GScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, GRect *r)
{
}

