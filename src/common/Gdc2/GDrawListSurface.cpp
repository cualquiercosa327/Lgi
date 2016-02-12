#include "Lgi.h"
#include "GDisplayString.h"
#include "GDrawListSurface.h"

struct LCmd
{
	virtual ~LCmd() {}
	virtual bool OnPaint(GDrawListSurfacePriv *d, GSurface *pDC) = 0;
};

struct GDrawListSurfacePriv : public GArray<LCmd*>
{
	int x, y, Bits;
	int DpiX, DpiY;
	GColour Fore, Back;
	GSurface *CreationSurface;
	GFont *Font;

	GDrawListSurfacePriv()
	{
		x = y = 0;
		CreationSurface = NULL;
		Font = NULL;
	}

	~GDrawListSurfacePriv()
	{
		DeleteObjects();
	}
	
	bool OnPaint(GSurface *pDC)
	{
		for (unsigned i=0; i<Length(); i++)
		{
			LCmd *c = (*this)[i];
			if (c && !c->OnPaint(this, pDC))
			{
				return false;
			}
		}
		
		return true;
	}
};

struct LCmdTxt : public LCmd
{
	GdcPt2 p;
	GDisplayString *Ds;

	LCmdTxt(int x, int y, GDisplayString *ds) : p(x, y)
	{
		Ds = ds;
	}
	
	bool OnPaint(GDrawListSurfacePriv *d, GSurface *pDC)
	{
		GFont *f = Ds->GetFont();
		f->Transparent(!d->Back.IsValid());
		f->Colour(d->Fore, d->Back);
		Ds->Draw(pDC, p.x, p.y);		
		return true;
	}
};

struct LCmdBlt : public LCmd
{
	GdcPt2 p;
	GSurface *Img;
	GRect Dst;
	GRect Src;
	bool Stretch;

	LCmdBlt(int X, int Y, GSurface *img, GRect *dst, GRect *src, bool stretch = false) :
		p(X, Y)
	{
		Img = img;
		if (dst)
			Dst = *dst;
		else
			Dst.ZOff(-1,-1);
		if (src)
			Src = *src;
		else
			Src.ZOff(-1, -1);
		Stretch = stretch;
		if (Stretch)
			LgiAssert(Src.Valid() && Dst.Valid());
	}
	
	bool OnPaint(GDrawListSurfacePriv *d, GSurface *pDC)
	{
		if (Stretch)
			pDC->StretchBlt(&Dst, Img, &Src);
		else
			pDC->Blt(p.x, p.y, Img, Src.Valid() ? &Src : NULL);
		return true;
	}
};

struct LCmdRect : public LCmd
{
	GRect r;
	bool Filled;
	
	LCmdRect(GRect &rc, bool filled)
	{
		r = rc;
		Filled = filled;
	}

	bool OnPaint(GDrawListSurfacePriv *d, GSurface *pDC)
	{
		if (Filled)
			pDC->Rectangle(&r);
		else
			pDC->Box(&r);
		return true;
	}
};

struct LCmdPixel : public LCmd
{
	GdcPt2 p;
	
	LCmdPixel(int x, int y) : p(x, y)
	{
	}

	bool OnPaint(GDrawListSurfacePriv *d, GSurface *pDC)
	{
		pDC->Set(p.x, p.y);
		return true;
	}
};

struct LCmdColour : public LCmd
{
	GColour c;
	
	LCmdColour(GColour &col)
	{
		c = col;
	}

	bool OnPaint(GDrawListSurfacePriv *d, GSurface *pDC)
	{
		pDC->Colour(c);
		return true;
	}
};

struct LCmdLine : public LCmd
{
	GdcPt2 s, e;
	
	LCmdLine(int x1, int y1, int x2, int y2) : s(x1, y1), e(x2, y2)
	{
	}

	bool OnPaint(GDrawListSurfacePriv *d, GSurface *pDC)
	{
		pDC->Line(s.x, s.y, e.x, e.y);
		return true;
	}
};

GDrawListSurface::GDrawListSurface(int Width, int Height, GColourSpace Cs)
{
	d = new GDrawListSurfacePriv;
	d->x = Width;
	d->y = Height;
	d->DpiX = d->DpiY = LgiScreenDpi();
	d->Bits = GColourSpaceToBits(Cs);
	d->Fore = GColour::Black;
	ColourSpace = Cs;
}

GDrawListSurface::GDrawListSurface(GSurface *FromSurface)
{
	d = new GDrawListSurfacePriv;
	d->x = FromSurface->X();
	d->y = FromSurface->Y();
	d->DpiX = FromSurface->DpiX();
	d->DpiY = FromSurface->DpiX();
	ColourSpace = FromSurface->GetColourSpace();
	d->Bits = GColourSpaceToBits(ColourSpace);
	d->Fore = GColour::Black;
	d->CreationSurface = FromSurface;
}

GDrawListSurface::~GDrawListSurface()
{
	delete d;
}

int GDrawListSurface::Length()
{
	return d->Length();
}

bool GDrawListSurface::OnPaint(GSurface *Dest)
{
	return d->OnPaint(Dest);
}

GFont *GDrawListSurface::GetFont()
{
	return d->Font;
}

void GDrawListSurface::SetFont(GFont *Font)
{
	d->Font = Font;
}

GColour GDrawListSurface::Background()
{
	return d->Back;
}

GColour GDrawListSurface::Background(GColour c)
{
	GColour Prev = d->Back;
	d->Back = c;
	return Prev;	
}

GDisplayString *GDrawListSurface::Text(int x, int y, const char *Str, int Len)
{
	if (!d->Font)
	{
		LgiAssert(!"No font.");
		return NULL;
	}
	
	GDisplayString *Ds = new GDisplayString(d->Font, Str, Len, d->CreationSurface);
	if (!Ds)
	{
		LgiAssert(0);
		return NULL;
	}
	
	LCmdTxt *Txt = new LCmdTxt(x, y, Ds);
	if (!Txt)
	{
		delete Ds;
		return NULL;
	}
	
	d->Add(Txt);
	return Ds;
}

GRect GDrawListSurface::ClipRgn()
{
	return Clip;
}

GRect GDrawListSurface::ClipRgn(GRect *Rgn)
{
	Clip = *Rgn;
	return Clip;
}

COLOUR GDrawListSurface::Colour()
{
	switch (d->Bits)
	{
		case 8:
			return d->Fore.c8();
		case 15:
			return CBit(d->Bits, d->Fore.c24());
		case 24:
			return d->Fore.c24();
		case 32:
			return d->Fore.c32();
	}
	
	LgiAssert(0);
	return 0;
}

COLOUR GDrawListSurface::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = Colour();
	d->Fore.Set(c, Bits);
	
	LCmdColour *cmd = new LCmdColour(d->Fore);
	if (cmd)
		d->Add(cmd);
	else
		LgiAssert(0);
	
	return Prev;
}

GColour GDrawListSurface::Colour(GColour c)
{
	GColour Prev = d->Fore;
	d->Fore = c;

	LCmdColour *cmd = new LCmdColour(d->Fore);
	if (cmd)
		d->Add(cmd);
	else
		LgiAssert(0);

	return Prev;
}

int GDrawListSurface::X()
{
	return d->x;
}

int GDrawListSurface::Y()
{
	return d->y;
}

int GDrawListSurface::GetRowStep()
{
	int Row = (d->Bits * d->x + 7) >> 3;
	return Row;
}

int GDrawListSurface::DpiX()
{
	return d->DpiX;
}

int GDrawListSurface::DpiY()
{
	return d->DpiY;
}

int GDrawListSurface::GetBits()
{
	return d->Bits;
}

void GDrawListSurface::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
}

void GDrawListSurface::Set(int x, int y)
{
	LCmdPixel *c = new LCmdPixel(x, y);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

void GDrawListSurface::HLine(int x1, int x2, int y)
{
	LCmdLine *c = new LCmdLine(x1, y, x2, y);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

void GDrawListSurface::VLine(int x, int y1, int y2)
{
	LCmdLine *c = new LCmdLine(x, y1, x, y2);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

void GDrawListSurface::Line(int x1, int y1, int x2, int y2)
{
	LCmdLine *c = new LCmdLine(x1, y1, x2, y2);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

uint GDrawListSurface::LineStyle(uint32 Bits, uint32 Reset)
{
	LgiAssert(!"Impl me.");
	return 0;
}

void GDrawListSurface::Circle(double cx, double cy, double radius)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::FilledCircle(double cx, double cy, double radius)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::Arc(double cx, double cy, double radius, double start, double end)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::FilledArc(double cx, double cy, double radius, double start, double end)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::Ellipse(double cx, double cy, double x, double y)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::FilledEllipse(double cx, double cy, double x, double y)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::Box(int x1, int y1, int x2, int y2)
{
	GRect r(x1, y1, x2, y2);
	LCmdRect *c = new LCmdRect(r, false);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

void GDrawListSurface::Box(GRect *r)
{
	LCmdRect *c = new LCmdRect(r ? r : Bounds(), false);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

void GDrawListSurface::Rectangle(int x1, int y1, int x2, int y2)
{
	GRect r(x1, y1, x2, y2);
	LCmdRect *c = new LCmdRect(r, true);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

void GDrawListSurface::Rectangle(GRect *r)
{
	LCmdRect *c = new LCmdRect(r ? r : Bounds(), true);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);
}

void GDrawListSurface::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
	{
		LgiAssert(0);
		return;
	}
	
	LCmdBlt *c = new LCmdBlt(x, y, Src, a, false);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);	
}

void GDrawListSurface::StretchBlt(GRect *drc, GSurface *Src, GRect *s)
{
	if (!d || !Src || !s)
	{
		LgiAssert(0);
		return;
	}
	
	LCmdBlt *c = new LCmdBlt(drc->x1, drc->y1, Src, drc, s, true);
	if (c)
		d->Add(c);
	else
		LgiAssert(0);	
}

void GDrawListSurface::Polygon(int Points, GdcPt2 *Data)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::Bezier(int Threshold, GdcPt2 *Pt)
{
	LgiAssert(!"Impl me.");
}

void GDrawListSurface::FloodFill(int x, int y, int Mode, COLOUR Border, GRect *Bounds)
{
	LgiAssert(!"Impl me.");
}

