/// \file
/// \author Matthew Allen
#include <stdio.h>
#include <math.h>

#include "Lgi.h"

class GScreenPrivate
{
public:
	CGContextRef Ctx;
	GWindow *Wnd;
	GView *View;
	GRect Rc;
	GArray<GRect> Stack;
	GColour c;
	int Bits;
	int Op;
	
	void Init()
	{
		Op = GDC_ALPHA;
		Ctx = nil;
		Wnd = NULL;
		View = NULL;
		Bits = GdcD->GetBits();
		View = 0;
		Rc.ZOff(-1, -1);
	}
	
	void SetContext(GView *v)
	{
		Rc = v->GetClient();
		// printf("SetContext: %s -> %s\n", v->GetClass(), Rc.GetStr());
		
		Ctx = [NSGraphicsContext currentContext].CGContext;
		if (Ctx)
		{
			#if 1
			CGAffineTransform flipVertical = CGAffineTransformMake(1, 0, 0, -1, 0, Rc.Y());
			CGContextConcatCTM(Ctx, flipVertical);
			#endif
		}
	}
	
	GScreenPrivate()
	{
		Init();
	}

	GScreenPrivate(GWindow *w)
	{
		Init();
		Wnd = w;
		SetContext(w);
	}

	GScreenPrivate(GView *v)
	{
		Init();
		View = v;
		SetContext(v);
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
	d = new GScreenPrivate(w);
	d->Wnd = w;
}

GScreenDC::GScreenDC(GView *v, void *param)
{
	d = new GScreenPrivate(v);
}

GScreenDC::GScreenDC(GPrintDcParams *params)
{
	d = new GScreenPrivate;
}

GScreenDC::~GScreenDC()
{
	DeleteObj(d);
}

GView *GScreenDC::GetView()
{
	return d->View;
}

bool GScreenDC::GetClient(GRect *c)
{
	if (!c)
		return false;
	
	*c = d->Rc;
	return true;
}

void GScreenDC::SetClient(GRect *c)
{
	#if 0
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
			
			CGRect rect = *c;
			rect.origin.x -= Ox;
			rect.origin.y -= Oy;
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
	#endif
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
	GSurface::SetOrigin(x, y);
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
		CGContextClipToRect(d->Ctx, c);
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
	GColour Prev = d->c;

	d->c = c;
	
	if (d->Ctx)
	{
		float r = (float)d->c.r()/255.0;
		float g = (float)d->c.g()/255.0;
		float b = (float)d->c.b()/255.0;
		float a = (float)d->c.a()/255.0;
		
		CGContextSetRGBFillColor(d->Ctx, r, g, b, a);
		CGContextSetRGBStrokeColor(d->Ctx, r, g, b, a);
	}

	return Prev;
}

COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	GColour Prev = d->c;

	d->c.Set(c, Bits);
	if (d->Ctx)
	{
		float r = (float)d->c.r()/255.0;
		float g = (float)d->c.g()/255.0;
		float b = (float)d->c.b()/255.0;
		float a = (float)d->c.a()/255.0;
		
		CGContextSetRGBFillColor(d->Ctx, r, g, b, a);
		CGContextSetRGBStrokeColor(d->Ctx, r, g, b, a);
	}

	return CBit(d->Bits, Prev.c32(), 32);
}

bool GScreenDC::SupportsAlphaCompositing()
{
	return true;
}

COLOUR GScreenDC::Colour()
{
	return CBit(d->Bits, d->c.c32(), 32);
}

int GScreenDC::Op()
{
	return d->Op;
}

int GScreenDC::Op(int Op, NativeInt Param)
{
	return d->Op = Op;
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
		CGRect r = {{(double)x, (double)y}, {1.0, 1.0}};
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
		CGRect r = {{(double)x1, (double)y}, {x2-x1+1.0, 1.0}};
		CGContextFillRect(d->Ctx, r);
	}
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{(double)x, (double)y1}, {1.0, y2-y1+1.0}};
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
			CGRect r = {{(double)x1, (double)y1}, {x2-x1+1.0, 1.0}};
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
			CGRect r = {{(double)x1, (double)y1}, {1.0, y2-y1+1.0}};
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
		CGRect r = {{x1+0.5, y1+0.5}, {(double)x2-x1, (double)y2-y1}};
		CGContextSetLineWidth(d->Ctx, 1.0);
		CGContextStrokeRect(d->Ctx, r);
	}
}

void GScreenDC::Box(GRect *a)
{
	if (d->Ctx && a)
	{
		CGRect r = {{a->x1+0.5, a->y1+0.5}, {(double)a->x2-a->x1, (double)a->y2-a->y1}};
		CGContextSetLineWidth(d->Ctx, 1.0);
		CGContextStrokeRect(d->Ctx, r);
	}
}

void GScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{(double)x1, (double)y1}, {x2-x1+1.0, y2-y1+1.0}};
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

		CGRect r = {{(double)a->x1, (double)a->y1}, {a->x2-a->x1+1.0, a->y2-a->y1+1.0}};
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
						CGRect r;
						r.origin.x = x;
						r.origin.y = -y;
						r.size.width = b.X();
						r.size.height = b.Y();
						CGImageRef Img = *i;
						
						CGContextSaveGState(d->Ctx);
						CGContextTranslateCTM(d->Ctx, 0.0f, b.Y());
						CGContextScaleCTM(d->Ctx, 1.0f, -1.0f);
						
						CGContextDrawImage(d->Ctx, r, Img);
						
						CGContextRestoreGState(d->Ctx);
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

