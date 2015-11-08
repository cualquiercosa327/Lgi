/**
	\file
	\author Matthew Allen
	\date 4/3/1998
	\brief Draw mode effects
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPath.h"
#include "GPixelRops.h"
#include "GPalette.h"

// #define Div255(a)	DivLut[a]
#define Div255(a)	((a)/255)

template<typename T>
void CreatePaletteLut(T *c, GPalette *Pal, int Scale = 255)
{
	if (Scale < 255)
	{
		uchar *DivLut = Div255Lut;

		for (int i=0; i<256; i++)
		{
			GdcRGB *p = (Pal) ? (*Pal)[i] : 0;
			if (p)
			{
				c[i].r = DivLut[p->r * Scale];
				c[i].g = DivLut[p->g * Scale];
				c[i].b = DivLut[p->b * Scale];
			}
			else
			{
				c[i].r = DivLut[i * Scale];
				c[i].g = c[i].r;
				c[i].b = c[i].r;
			}
		}
	}
	else if (Scale)
	{
		for (int i=0; i<256; i++)
		{
			GdcRGB *p = (Pal) ? (*Pal)[i] : 0;
			if (p)
			{
				c[i].r = p->r;
				c[i].g = p->g;
				c[i].b = p->b;
			}
			else
			{
				c[i].r = i;
				c[i].g = i;
				c[i].b = i;
			}
		}
	}
	else
	{
		memset(c, 0, sizeof(*c) * 256);
	}
}

/// Alpha blending applicators
class GAlphaApp : public GApplicator
{
protected:
	uchar alpha, oma;
	int Bits, Bytes;
	uchar *Ptr;
	uchar *APtr;

	const char *GetClass() { return "GAlphaApp"; }

public:
	GAlphaApp()
	{
		Op = GDC_ALPHA;
		alpha = 0xFF;
		oma = 0;
		Bits = 8;
		Bytes = 1;
		Ptr = 0;
		APtr = 0;
	}

	int GetVar(int Var)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A: return alpha;
		}
		return 0;
	}

	int SetVar(int Var, NativeInt Value)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A:
			{
				int Old = alpha;
				alpha = Value;
				oma = 0xFF - alpha;
				return Old;
			}
			case GAPP_ALPHA_PAL:
			{

			}
		}
		return 0;
	}

	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
	{
		if (d && d->GetBits() == Bits)
		{
			Dest = d;
			Pal = p;
			Ptr = d->Base;
			Alpha = a;
			APtr = Alpha ? Alpha->Base : 0;

			return true;
		}
		return false;
	}

	void SetPtr(int x, int y)
	{
		LgiAssert(Dest && Dest->Base);
		Ptr = Dest->Base + ((y * Dest->Line) + (x * Bytes));
		if (APtr)
			APtr = Alpha->Base + ((y * Alpha->Line) + x);
	}

	void IncX()
	{
		Ptr += Bytes;
		if (APtr)
			APtr++;
	}

	void IncY()
	{
		Ptr += Dest->Line;
		if (APtr)
			APtr += Alpha->Line;
	}

	void IncPtr(int X, int Y)
	{
		Ptr += (Y * Dest->Line) + (X * Bytes);
		if (APtr)
			APtr += (Y * Dest->Line) + X;
	}

	COLOUR Get()
	{
		switch (Bytes)
		{
			case 1:
				return *((uint8*)Ptr);
			case 2:
				return *((uint16*)Ptr);
			case 3:
				return Rgb24(Ptr[2], Ptr[1], Ptr[0]);
			case 4:
				return *((uint32*)Ptr);
		}
		return 0;
	}
};

class GdcApp8Alpha : public GAlphaApp
{
	char Remap[256];
	uchar *DivLut;

public:
	GdcApp8Alpha();
	int SetVar(int Var, NativeInt Value);

	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);

	template<typename Pixel>
	void AlphaBlt15(GBmpMem *Src, GPalette *DPal, uchar *Lut)
	{
		System24BitPixel dc[256];
		CreatePaletteLut(dc, DPal, oma);
		
		if (!Lut) Lut = DPal->MakeLut(15);

		for (int y=0; y<Src->y; y++)
		{
			Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
			uchar *d = Ptr;
			uchar *e = d + Src->x;

			while (d < e)
			{
				System24BitPixel *dst = dc + *d;
				int r = dst->r + DivLut[G5bitTo8bit(s->r) * alpha];
				int g = dst->g + DivLut[G5bitTo8bit(s->g) * alpha];
				int b = dst->b + DivLut[G5bitTo8bit(s->b) * alpha];
				*d++ = Lut[Rgb15(r, g, b)];
				s++;						
			}

			Ptr += Dest->Line;
		}
	}

	template<typename Pixel>
	void AlphaBlt16(GBmpMem *Src, GPalette *DPal, uchar *Lut)
	{
		System24BitPixel dc[256];
		CreatePaletteLut(dc, DPal, oma);
		
		if (!Lut) Lut = DPal->MakeLut(15);

		for (int y=0; y<Src->y; y++)
		{
			Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
			uchar *d = Ptr;
			uchar *e = d + Src->x;

			while (d < e)
			{
				System24BitPixel *dst = dc + *d;
				int r = dst->r + DivLut[G5bitTo8bit(s->r) * alpha];
				int g = dst->g + DivLut[G6bitTo8bit(s->g) * alpha];
				int b = dst->b + DivLut[G5bitTo8bit(s->b) * alpha];
				*d++ = Lut[Rgb15(r, g, b)];
				s++;						
			}

			Ptr += Dest->Line;
		}
	}

	template<typename Pixel>
	void AlphaBlt24(GBmpMem *Src, GPalette *DPal, uchar *Lut)
	{
		System24BitPixel dc[256];
		CreatePaletteLut(dc, DPal, oma);
		
		if (!Lut) Lut = DPal->MakeLut(15);

		for (int y=0; y<Src->y; y++)
		{
			Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
			uchar *d = Ptr;
			uchar *e = d + Src->x;

			while (d < e)
			{
				System24BitPixel *dst = dc + *d;
				int r = dst->r + DivLut[s->r * alpha];
				int g = dst->g + DivLut[s->g * alpha];
				int b = dst->b + DivLut[s->b * alpha];
				*d++ = Lut[Rgb15(r, g, b)];
				s++;						
			}

			Ptr += Dest->Line;
		}
	}

	template<typename Pixel>
	void AlphaBlt32(GBmpMem *Src, GPalette *DPal, uchar *Lut)
	{
		GdcRGB *dc = (*DPal)[0];
		if (!Lut) Lut = DPal->MakeLut(15);

		for (int y=0; y<Src->y; y++)
		{
			Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
			uchar *d = Ptr;
			uchar *e = d + Src->x;

			while (d < e)
			{
				GdcRGB dst = dc[*d];
				OverNpm32toNpm24(s, &dst);
				*d++ = Lut[Rgb15(dst.r, dst.g, dst.b)];
				s++;						
			}

			Ptr += Dest->Line;
		}
	}

	template<typename Pixel>
	void AlphaBlt48(GBmpMem *Src, GPalette *DPal, uchar *Lut)
	{
		System24BitPixel dc[256];
		CreatePaletteLut(dc, DPal, oma);
		
		if (!Lut) Lut = DPal->MakeLut(15);

		for (int y=0; y<Src->y; y++)
		{
			Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
			uchar *d = Ptr;
			uchar *e = d + Src->x;

			while (d < e)
			{
				System24BitPixel dst = dc[*d];
				GRgba32 src = { s->r >> 8, s->g >> 8, s->b >> 8, alpha };
				OverNpm32toNpm24(&src, &dst);
				*d++ = Lut[Rgb15(dst.r, dst.g, dst.b)];
				s++;
			}

			Ptr += Dest->Line;
		}
	}

	template<typename Pixel>
	void AlphaBlt64(GBmpMem *Src, GPalette *DPal, uchar *Lut)
	{
		System24BitPixel dc[256];
		CreatePaletteLut(dc, DPal, 0xff);
		
		if (!Lut) Lut = DPal->MakeLut(15);

		for (int y=0; y<Src->y; y++)
		{
			Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
			uchar *d = Ptr;
			uchar *e = d + Src->x;

			while (d < e)
			{
				System24BitPixel dst = dc[*d];
				OverNpm64toNpm24(s, &dst);
				*d++ = Lut[Rgb15(dst.r, dst.g, dst.b)];
				s++;						
			}

			Ptr += Dest->Line;
		}
	}
};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha : public GApplicator
{
protected:
	union {
		uint8 *u8;
		uint16 *u16;
		uint32 *u32;
		Pixel *p;
	};
	
	uint8 alpha, one_minus_alpha;
	
public:
	GdcAlpha()
	{
		p = NULL;
		alpha = 0xff;
		one_minus_alpha = 0;
	}

	const char *GetClass() { return "GdcAlpha"; }

	int GetVar(int Var)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A: return alpha;
		}
		return 0;
	}

	int SetVar(int Var, NativeInt Value)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A:
			{
				int Old = alpha;
				alpha = Value;
				one_minus_alpha = 0xFF - alpha;
				return Old;
			}
		}
		return 0;
	}

	bool SetSurface(GBmpMem *d, GPalette *p = 0, GBmpMem *a = 0)
	{
		if (d && d->Cs == ColourSpace)
		{
			Dest = d;
			Pal = p;
			u8 = d->Base;
			Alpha = a;

			return true;
		}
		else LgiAssert(0);
		
		return false;
	}
	
	void SetPtr(int x, int y)
	{
		u8 = Dest->Base + (Dest->Line * y);
		p += x;
	}
	
	void IncX()
	{
		p++;
	}
	
	void IncY()
	{
		u8 += Dest->Line;
	}
	
	void IncPtr(int X, int Y)
	{
		p += X;
		u8 += Dest->Line * Y;
	}

	COLOUR Get()
	{
		return Rgb24(p->r, p->g, p->b);
	}
};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha15 : public GdcAlpha<Pixel, ColourSpace>
{
public:
	const char *GetClass() { return "GdcAlpha15"; }

	#define Div2040(c) ((c) / 2040)

	#define Setup15() \
		register uint8 r = Rc15(this->c); \
		register uint8 g = Gc15(this->c); \
		register uint8 b = Bc15(this->c); \
		register uint8 a = this->alpha; \
		register uint8 oma = 255 - a; \
		register Pixel *d = this->p

	#define Comp15() \
		d->r = Div2040((G5bitTo8bit(d->r) * oma) + (r * a)); \
		d->g = Div2040((G5bitTo8bit(d->g) * oma) + (g * a)); \
		d->b = Div2040((G5bitTo8bit(d->b) * oma) + (b * a))

	void Set()
	{
		Setup15();
		Comp15();
	}

	void VLine(int height)
	{
		Setup15();
		register int line = this->Dest->Line;

		while (height--)
		{
			Comp15();
			d = (Pixel*)(((uint8*)d) + line);
		}
		this->p = d;
	}

	void Rectangle(int x, int y)
	{
		Setup15();
		register int line = this->Dest->Line;

		while (y--)
		{
			d = this->p;
			register Pixel *e = d + x;
			while (d < e)
			{
				Comp15();
				d++;
			}

			this->u8 += line;
		}
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
	{
		if (!Src)
			return false;
	
		if (Src->Cs == CsIndex8)
		{
			Pixel map[256];
			if (SPal)
			{
				GdcRGB *p = (*SPal)[0];
				for (int i=0; i<256; i++, p++)
				{
					map[i].r = G8bitTo5bit(p->r);
					map[i].g = G8bitTo5bit(p->g);
					map[i].b = G8bitTo5bit(p->b);
				}
			}
			else
			{
				for (int i=0; i<256; i++)
				{
					map[i].r = G8bitTo5bit(i);
					map[i].g = G8bitTo5bit(i);
					map[i].b = G8bitTo5bit(i);
				}
			}

			for (int y=0; y<Src->y; y++)
			{
				uchar *s = ((uchar*)Src->Base) + (y * Src->Line);
				Pixel *d = this->p;

				for (int x=0; x<Src->x; x++)
				{
					*d++ = map[*s++];
				}

				this->u8 += this->Dest->Line;
			}
			return true;
		}
		else
		{
			GBmpMem Dst;
			Dst.Base = this->u8;
			Dst.x = Src->x;
			Dst.y = Src->y;
			Dst.Cs = this->Dest->Cs;
			Dst.Line = this->Dest->Line;
			return LgiRopUniversal(&Dst, Src, true);
		}
	}

};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha16 : public GdcAlpha<Pixel, ColourSpace>
{
public:
	const char *GetClass() { return "GdcAlpha16"; }

	#define Div2040(c) ((c) / 2040)
	#define Div1020(c) ((c) / 1020)

	#define Setup16() \
		register uint8 r = Rc16(this->c); \
		register uint8 g = Gc16(this->c); \
		register uint8 b = Bc16(this->c); \
		register uint8 a = this->alpha; \
		register uint8 oma = 255 - a; \
		register Pixel *d = this->p

	#define Comp16() \
		d->r = Div2040((G5bitTo8bit(d->r) * oma) + (r * a)); \
		d->g = Div1020((G6bitTo8bit(d->g) * oma) + (g * a)); \
		d->b = Div2040((G5bitTo8bit(d->b) * oma) + (b * a))

	void Set()
	{
		Setup16();
		Comp16();
	}

	void VLine(int height)
	{
		Setup16();
		register int line = this->Dest->Line;

		while (height--)
		{
			Comp16();
			d = (Pixel*)(((uint8*)d) + line);
		}
		this->p = d;
	}

	void Rectangle(int x, int y)
	{
		Setup16();
		register int line = this->Dest->Line;

		while (y--)
		{
			d = this->p;
			register Pixel *e = d + x;
			while (d < e)
			{
				Comp16();
				d++;
			}

			this->u8 += line;
		}
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
	{
		if (!Src)
			return false;
	
		if (Src->Cs == CsIndex8)
		{
			Pixel map[256];
			if (SPal)
			{
				GdcRGB *p = (*SPal)[0];
				for (int i=0; i<256; i++, p++)
				{
					map[i].r = G8bitTo5bit(p->r);
					map[i].g = G8bitTo6bit(p->g);
					map[i].b = G8bitTo5bit(p->b);
				}
			}
			else
			{
				for (int i=0; i<256; i++)
				{
					map[i].r = G8bitTo5bit(i);
					map[i].g = G8bitTo6bit(i);
					map[i].b = G8bitTo5bit(i);
				}
			}

			for (int y=0; y<Src->y; y++)
			{
				uchar *s = ((uchar*)Src->Base) + (y * Src->Line);
				Pixel *d = this->p;

				for (int x=0; x<Src->x; x++)
				{
					*d++ = map[*s++];
				}

				this->u8 += this->Dest->Line;
			}
			return true;
		}
		else
		{
			GBmpMem Dst;
			Dst.Base = this->u8;
			Dst.x = Src->x;
			Dst.y = Src->y;
			Dst.Cs = this->Dest->Cs;
			Dst.Line = this->Dest->Line;
			return LgiRopUniversal(&Dst, Src, true);
		}
	}
};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha24 : public GdcAlpha<Pixel, ColourSpace>
{
public:
	const char *GetClass() { return "GdcAlpha24"; }

	#define InitComposite24() \
		uchar *DivLut = Div255Lut; \
		register uint8 a = this->alpha; \
		register uint8 oma = this->one_minus_alpha; \
		register int r = this->p24.r * a; \
		register int g = this->p24.g * a; \
		register int b = this->p24.b * a
	#define InitFlat24() \
		Pixel px; \
		px.r = this->p24.r; \
		px.g = this->p24.g; \
		px.b = this->p24.b
	#define Composite24(ptr) \
		ptr->r = DivLut[(oma * ptr->r) + r]; \
		ptr->g = DivLut[(oma * ptr->g) + g]; \
		ptr->b = DivLut[(oma * ptr->b) + b]

	void Set()
	{
		InitComposite24();
		Composite24(this->p);
	}
	
	void VLine(int height)
	{
		if (this->alpha == 255)
		{
			InitFlat24();
			while (height-- > 0)
			{
				*this->p = px;
				this->u8 += this->Dest->Line;
			}
		}
		else if (this->alpha > 0)
		{
			InitComposite24();
			while (height-- > 0)
			{
				Composite24(this->p);
				this->u8 += this->Dest->Line;
			}
		}
	}
	
	void Rectangle(int x, int y)
	{
		if (this->alpha == 0xff)
		{
			InitFlat24();
			while (y-- > 0)
			{
				register Pixel *s = this->p;
				register Pixel *e = s + x;
				while (s < e)
				{
					*this->p = px;
					s++;
				}
				this->u8 += this->Dest->Line;
			}
		}
		else if (this->alpha > 0)
		{
			InitComposite24();
			while (y-- > 0)
			{
				register Pixel *s = this->p;
				register Pixel *e = s + x;

				while (s < e)
				{
					Composite24(s);
					s++;
				}
				this->u8 += this->Dest->Line;
			}
		}
	}
	
	template<typename SrcPx>
	void CompositeBlt24(GBmpMem *Src)
	{
		uchar *Lut = Div255Lut;
		register uint8 a = this->alpha;
		register uint8 oma = this->one_minus_alpha;
		
		for (int y=0; y<Src->y; y++)
		{
			Pixel *dst = this->p;
			Pixel *dst_end = dst + Src->x;
			SrcPx *src = (SrcPx*)(Src->Base + (y * Src->Line));
			if (a == 0xff)
			{
				while (dst < dst_end)
				{
					// No source alpha, just copy blt
					dst->r = src->r;
					dst->g = src->g;
					dst->b = src->b;
					dst++;
					src++;
				}
			}
			else if (a > 0)
			{
				while (dst < dst_end)
				{
					// No source alpha, but apply our local alpha
					dst->r = Lut[(dst->r * oma) + (src->r * a)];
					dst->g = Lut[(dst->g * oma) + (src->g * a)];
					dst->b = Lut[(dst->b * oma) + (src->b * a)];
					dst++;
					src++;
				}
			}
			
			this->u8 += this->Dest->Line;
		}
	}
	
	template<typename SrcPx>
	void CompositeBlt32(GBmpMem *Src)
	{
		uchar *Lut = Div255Lut;
		register uint8 a = this->alpha;
		
		if (a == 0xff)
		{
			// Apply the source alpha only
			for (int y=0; y<Src->y; y++)
			{
				Pixel *dst = this->p;
				Pixel *dst_end = dst + Src->x;
				SrcPx *src = (SrcPx*)(Src->Base + (y * Src->Line));
				while (dst < dst_end)
				{
					register uint8 sa = src->a;
					register uint8 soma = 0xff - sa;
					dst->r = Lut[(dst->r * soma) + (src->r * sa)];
					dst->g = Lut[(dst->g * soma) + (src->g * sa)];
					dst->b = Lut[(dst->b * soma) + (src->b * sa)];
					dst++;
					src++;
				}
				
				this->u8 += this->Dest->Line;
			}
		}
		else if (a > 0)
		{
			// Apply source alpha AND our local alpha
			for (int y=0; y<Src->y; y++)
			{
				Pixel *dst = this->p;
				Pixel *dst_end = dst + Src->x;
				SrcPx *src = (SrcPx*)(Src->Base + (y * Src->Line));
				while (dst < dst_end)
				{
					register uint8 sa = Lut[a * src->a];
					register uint8 soma = 0xff - sa;
					dst->r = Lut[(dst->r * soma) + (src->r * sa)];
					dst->g = Lut[(dst->g * soma) + (src->g * sa)];
					dst->b = Lut[(dst->b * soma) + (src->b * sa)];
					dst++;
					src++;
				}
				
				this->u8 += this->Dest->Line;
			}
		}
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0)
	{
		if (!Src)
			return false;

		if (SrcAlpha)
		{
			LgiAssert(!"Impl me.");
		}
		else
		{
			switch (Src->Cs)
			{
				case CsIndex8:
				{
					Pixel map[256];
					for (int i=0; i<256; i++)
					{
						GdcRGB *p = (SPal) ? (*SPal)[i] : NULL;
						if (p)
						{
							map[i].r = p->r;
							map[i].g = p->g;
							map[i].b = p->b;
						}
						else
						{
							map[i].r = i;
							map[i].g = i;
							map[i].b = i;
						}
					}

					for (int y=0; y<Src->y; y++)
					{
						uchar *s = ((uchar*)Src->Base) + (y * Src->Line);
						Pixel *d = this->p;

						for (int x=0; x<Src->x; x++)
						{
							*d++ = map[*s++];
						}

						this->u8 += this->Dest->Line;
					}
					return true;
				}
				
				#define Blt24Case(name, size) \
					case Cs##name: \
						CompositeBlt##size<G##name>(Src); \
						break
				
				Blt24Case(Rgb24, 24);
				Blt24Case(Bgr24, 24);
				Blt24Case(Rgbx32, 24);
				Blt24Case(Bgrx32, 24);
				Blt24Case(Xrgb32, 24);
				Blt24Case(Xbgr32, 24);

				Blt24Case(Rgba32, 32);
				Blt24Case(Bgra32, 32);
				Blt24Case(Argb32, 32);
				Blt24Case(Abgr32, 32);
				#undef Blt24Case
				default:
				{
					GBmpMem Dst;
					Dst.Base = this->u8;
					Dst.x = Src->x;
					Dst.y = Src->y;
					Dst.Cs = this->Dest->Cs;
					Dst.Line = this->Dest->Line;				
					return LgiRopUniversal(&Dst, Src, true);
					break;
				}
			}
		}
		
		return false;
	}
};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha32 : public GdcAlpha<Pixel, ColourSpace>
{
public:
	#define InitComposite32() \
		uchar *DivLut = Div255Lut; \
		register int a = DivLut[this->alpha * this->p32.a]; \
		register int r = this->p32.r * a; \
		register int g = this->p32.g * a; \
		register int b = this->p32.b * a; \
		register uint8 oma = 0xff - a
	#define InitFlat32() \
		Pixel px; \
		px.r = this->p32.r; \
		px.g = this->p32.g; \
		px.b = this->p32.b; \
		px.a = this->p32.a
	#define Composite32(ptr) \
		ptr->r = DivLut[(oma * ptr->r) + r]; \
		ptr->g = DivLut[(oma * ptr->g) + g]; \
		ptr->b = DivLut[(oma * ptr->b) + b]; \
		ptr->a = (a + ptr->a) - DivLut[a * ptr->a]

	const char *GetClass() { return "GdcAlpha32"; }

	void Set()
	{
		InitComposite32();
		Composite32(this->p);
	}
	
	void VLine(int height)
	{
		int sa = Div255Lut[this->alpha * this->p32.a];
		if (sa == 0xff)
		{
			InitFlat32();
			while (height-- > 0)
			{
				*this->p = px;
				this->u8 += this->Dest->Line;
			}
		}
		else if (sa > 0)
		{
			InitComposite32();
			while (height-- > 0)
			{
				Composite32(this->p);
				this->u8 += this->Dest->Line;
			}
		}
	}
	
	void Rectangle(int x, int y)
	{
		int sa = Div255Lut[this->alpha * this->p32.a];
		if (sa == 0xff)
		{
			// Fully opaque
			InitFlat32();
			while (y--)
			{
				Pixel *d = this->p;
				Pixel *e = d + x;
				while (d < e)
				{
					*d++ = px;
				}

				this->u8 += this->Dest->Line;
			}
		}
		else if (sa > 0)
		{
			// Translucent
			InitComposite32();
			while (y--)
			{
				Pixel *d = this->p;
				Pixel *e = d + x;
				while (d < e)
				{
					Composite32(d);
					d++;
				}

				this->u8 += this->Dest->Line;
			}
		}
	}
	
	template<typename SrcPx>
	void PmBlt32(GBmpMem *Src)
	{
		register uchar *DivLut = Div255Lut;
		for (int y=0; y<Src->y; y++)
		{
			register Pixel *d = this->p, *e = d + Src->x;
			register SrcPx *s = (SrcPx*)(Src->Base + (Src->Line * y));

			while (d < e)
			{
				OverPm32toPm32(s, d);
				d++;
				s++;
			}
			
			this->u8 += this->Dest->Line;
		}
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0)
	{
		if (!Src) return 0;
		register uchar *DivLut = Div255Lut;
		uchar lookup[256];
		register uint8 a = this->alpha;
		register uint8 oma = this->one_minus_alpha;
		for (int i=0; i<256; i++)
		{
			lookup[i] = DivLut[i * this->alpha];
		}

		if (SrcAlpha)
		{
			switch (Src->Cs)
			{
				case CsIndex8:
				{
					System24BitPixel c[256];
					CreatePaletteLut(c, SPal);

					for (int y=0; y<Src->y; y++)
					{
						uchar *s = (uchar*) (Src->Base + (y * Src->Line));
						uchar *sa = (uchar*) (SrcAlpha->Base + (y * SrcAlpha->Line));
						System24BitPixel *sc;
						Pixel *d = this->p;
						uchar a, o;

						for (int x=0; x<Src->x; x++)
						{
							a = lookup[*sa++];
							if (a == 255)
							{
								sc = c + *s;
								d->r = sc->r;
								d->g = sc->g;
								d->b = sc->b;
								d->a = 255;
							}
							else if (a)
							{
								sc = c + *s;
								o = 0xff - a;
								d->r = DivLut[(d->r * o) + (sc->r * a)];
								d->g = DivLut[(d->g * o) + (sc->g * a)];
								d->b = DivLut[(d->b * o) + (sc->b * a)];
								d->a = (a + d->a) - DivLut[a * d->a];
							}

							s++;
							d++;
						}

						this->u8 += this->Dest->Line;
					}
					break;
				}
				case System15BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						ushort *s = (ushort*) (Src->Base + (y * Src->Line));
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
						Pixel *d = this->p;

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = Rc15(*s);
								d->g = Gc15(*s);
								d->b = Bc15(*s);
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * Rc15(*s)) + (o * d->r)];
								d->g = DivLut[(a * Gc15(*s)) + (o * d->g)];
								d->b = DivLut[(a * Bc15(*s)) + (o * d->b)];
							}

							s++;
							d++;
						}

						this->u8 += this->Dest->Line;
					}
					break;
				}
				case System16BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						ushort *s = (ushort*) (Src->Base + (y * Src->Line));
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
						Pixel *d = this->p;

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = Rc16(*s);
								d->g = Gc16(*s);
								d->b = Bc16(*s);
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * Rc16(*s)) + (o * d->r)];
								d->g = DivLut[(a * Gc16(*s)) + (o * d->g)];
								d->b = DivLut[(a * Bc16(*s)) + (o * d->b)];
							}

							s++;
							d++;
						}

						this->u8 += this->Dest->Line;
					}
					break;
				}
				case System24BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
						Pixel *d = this->p;
						System24BitPixel *s = (System24BitPixel*) (Src->Base + (y * Src->Line));

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = s->r;
								d->g = s->g;
								d->b = s->b;
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * s->r) + (o * d->r)];
								d->g = DivLut[(a * s->g) + (o * d->g)];
								d->b = DivLut[(a * s->b) + (o * d->b)];
							}

							d++;
							s++;
						}

						this->u8 += this->Dest->Line;
					}
					break;
				}
				case System32BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						Pixel *d = this->p;
						Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = s->r;
								d->g = s->g;
								d->b = s->b;
								d->a = 255;
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * s->r) + (o * d->r)];
								d->g = DivLut[(a * s->g) + (o * d->g)];
								d->b = DivLut[(a * s->b) + (o * d->b)];
								d->a = (s->a + d->a) - DivLut[s->a * d->a];
							}

							d++;
							s++;
						}

						this->u8 += this->Dest->Line;
					}
					break;
				}
				default:
					return false;
			}
			
			return true;
		}

		if (this->Dest->PreMul() || Src->PreMul())
		{
			switch (Src->Cs)
			{
				case CsRgba32:
					PmBlt32<GRgba32>(Src);
					return true;
				case CsBgra32:
					PmBlt32<GBgra32>(Src);
					return true;
				case CsArgb32:
					PmBlt32<GArgb32>(Src);
					return true;
				case CsAbgr32:
					PmBlt32<GAbgr32>(Src);
					return true;
				default:
					break;
			}
		}

		switch (Src->Cs)
		{
			default:
			{
				GBmpMem Dst;
				Dst.Base = this->u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = this->Dest->Cs;
				Dst.Line = this->Dest->Line;
				if (!LgiRopUniversal(&Dst, Src, true))
				{
					return false;
				}
				break;
			}
			case CsIndex8:
			{
				System24BitPixel c[256];
				CreatePaletteLut(c, SPal, this->alpha);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = (uchar*) (Src->Base + (y * Src->Line));
					System24BitPixel *sc;
					Pixel *d = this->p;

					if (this->alpha == 255)
					{
						for (int x=0; x<Src->x; x++)
						{
							sc = c + *s++;
							
							d->r = sc->r;
							d->g = sc->g;
							d->b = sc->b;
							d->a = 255;

							d++;
						}
					}
					else if (this->alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							sc = c + *s++;

							d->r = sc->r + DivLut[d->r * oma];
							d->g = sc->g + DivLut[d->g * oma];
							d->b = sc->b + DivLut[d->b * oma];
							d->a = (a + d->a) - DivLut[a * d->a];

							d++;
						}
					}

					this->u8 += this->Dest->Line;
				}
				break;
			}
		}

		return true;
	}
};

GApplicator *GAlphaFactory::Create(GColourSpace Cs, int Op)
{
	if (Op != GDC_ALPHA)
		return NULL;

	switch (Cs)
	{
		#define Case(name, px) \
			case Cs##name: \
				return new GdcAlpha##px<G##name, Cs##name>()

		Case(Rgb15, 15);
		Case(Bgr15, 15);

		Case(Rgb16, 16);
		Case(Bgr16, 16);

		Case(Rgb24, 24);
		Case(Bgr24, 24);
		Case(Rgbx32, 24);
		Case(Bgrx32, 24);
		Case(Xrgb32, 24);
		Case(Xbgr32, 24);

		Case(Rgba32, 32);
		Case(Bgra32, 32);
		Case(Argb32, 32);
		Case(Abgr32, 32);

		#undef Case
		case CsIndex8:
			return new GdcApp8Alpha;
		default:
			LgiTrace("%s:%i - Unknown colour space: 0x%x %s\n",
					_FL,
					Cs,
					GColourSpaceToString(Cs));
			// LgiAssert(0);
			break;
	}

	return 0;
}

GdcApp8Alpha::GdcApp8Alpha()
{
	Bits = 8;
	Bytes = 1;
	Op = GDC_ALPHA;
	ZeroObj(Remap);
	DivLut = Div255Lut;
}

int GdcApp8Alpha::SetVar(int Var, NativeInt Value)
{
	int Status = GAlphaApp::SetVar(Var, Value);

	switch (Var)
	{
		case GAPP_ALPHA_PAL:
		{
			GPalette *Pal = (GPalette*)Value;

			if (Pal && alpha < 255)
			{
				GdcRGB *p = (*Pal)[0];
				GdcRGB *Col = (*Pal)[c&0xFF];

				for (int i=0; i<Pal->GetSize(); i++)
				{
					COLOUR Rgb = Rgb24(	Div255((oma * p[i].r) + (alpha * Col->r)),
										Div255((oma * p[i].g) + (alpha * Col->g)),
										Div255((oma * p[i].b) + (alpha * Col->b)));

					Remap[i] = Pal->MatchRgb(Rgb);
				}
			}
			else
			{
				for (int i=0; i<256; i++)
				{
					Remap[i] = c;
				}
			}
			break;
		}
	}

	return Status;
}

void GdcApp8Alpha::Set()
{
	*Ptr = Remap[*Ptr];
	if (APtr)
	{
		*APtr += DivLut[(255 - *APtr) * alpha];
	}
}

void GdcApp8Alpha::VLine(int y)
{
	while (y--)
	{
		*Ptr = Remap[*Ptr];
		Ptr += Dest->Line;

		if (APtr)
		{
			*APtr += DivLut[(255 - *APtr) * alpha];
			APtr += Alpha->Line;
		}
	}
}

void GdcApp8Alpha::Rectangle(int x, int y)
{
	while (y--)
	{
		uchar *p = Ptr;
		uchar *e = Ptr + x;
		while (p < e)
		{
			*p = Remap[*p];
			p++;
		}
		Ptr += Dest->Line;

		if (APtr)
		{
			uchar *a = APtr;
			e = a + x;
			while (a < e)
			{
				*a += DivLut[(255 - *a) * alpha];
				a++;
			}
			APtr += Alpha->Line;
		}
	}
}

bool GdcApp8Alpha::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (!Src)
		return false;
	if (!SPal)
		SPal = Pal;

	GPalette Grey;
	GPalette *DPal;
	if (Pal)
	{
		DPal = Pal;
	}
	else
	{
		Grey.CreateGreyScale();
		DPal = &Grey;
	}

	uchar *DivLut = Div255Lut;
	uchar *Lut = 0;

	uchar lookup[256];
	for (int i=0; i<256; i++)
	{
		lookup[i] = (i * (int)alpha) / 255;
	}

	if (SrcAlpha)
	{
		// Per pixel source alpha
		GdcRGB *SRgb = (*SPal)[0];
		GdcRGB *DRgb = (*DPal)[0];
		if (!SRgb || !DRgb) return false;

		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				System24BitPixel sc[256];
				CreatePaletteLut(sc, SPal);
				
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;
					int r, g, b;

					Lut = DPal->MakeLut(15);

					for (int x=0; x<Src->x; x++)
					{
						uchar a = lookup[*sa];
						System24BitPixel *src = sc + *s;

						if (a == 255)
						{
							r = src->r;
							g = src->g;
							b = src->b;
						}
						else if (a)
						{
							uchar o = 0xff - a;
							System24BitPixel *dst = dc + *d;
							r = DivLut[(dst->r * o) + (src->r * a)];
							g = DivLut[(dst->g * o) + (src->g * a)];
							b = DivLut[(dst->b * o) + (src->b * a)];
						}

						*d++ = Lut[Rgb15(r, g, b)];
						sa++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System15BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;
					uchar *end = d + Src->x;

					while (d < end)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[*s];
						}
						else if (a)
						{
							uchar o = 255 - a;
							System24BitPixel *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (Rc15(*s) * a)];
							int g = DivLut[(dst->g * o) + (Gc15(*s) * a)];
							int b = DivLut[(dst->b * o) + (Bc15(*s) * a)];
							*d = Lut[Rgb15(r, g, b)];
						}
						
						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System16BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;
					uchar *end = d + Src->x;

					while (d < end)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[Rgb16To15(*s)];
						}
						else if (a)
						{
							uchar o = 255 - a;
							System24BitPixel *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (Rc16(*s) * a)];
							int g = DivLut[(dst->g * o) + (Gc16(*s) * a)];
							int b = DivLut[(dst->b * o) + (Bc16(*s) * a)];
							*d = Lut[Rgb15(r, g, b)];
						}
						
						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				GBgr24 dc[256];
				CreatePaletteLut(dc, DPal, 255);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;

					for (int x=0; x<Src->x; x++)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[Rgb15(s->r, s->g, s->b)];
						}
						else if (a)
						{
							uchar o = 255 - a;
							GBgr24 *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (s->r * a)];
							int g = DivLut[(dst->g * o) + (s->g * a)];
							int b = DivLut[(dst->b * o) + (s->b * a)];
							*d = Lut[Rgb15(r, g, b)];
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal, 255);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;

					for (int x=0; x<Src->x; x++)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[Rgb15(s->r, s->g, s->b)];
						}
						else if (a)
						{
							uchar o = 255 - a;
							System24BitPixel *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (s->r * a)];
							int g = DivLut[(dst->g * o) + (s->g * a)];
							int b = DivLut[(dst->b * o) + (s->b * a)];
							*d = Lut[Rgb15(r, g, b)];
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}
	else
	{
		// Global alpha level
		GdcRGB *SRgb = (*SPal)[0];
		GdcRGB *DRgb = (*DPal)[0];
		if (!SRgb || !DRgb) return false;

		switch (Src->Cs)
		{
			default:
			{
				LgiTrace("%s:%i - Not impl.\n", _FL);
				break;
			}
			case CsIndex8:
			{
				if (alpha == 255)
				{
					// do a straight blt
					for (int y=0; y<Src->y; y++)
					{
						uchar *s = Src->Base + (y * Src->Line);
						uchar *d = Ptr;

						memcpy(d, s, Src->x);
						Ptr += Dest->Line;
					}
				}
				else if (alpha)
				{
					System24BitPixel sc[256];
					CreatePaletteLut(sc, SPal, alpha);
					
					System24BitPixel dc[256];
					CreatePaletteLut(dc, DPal, oma);

					if (!Lut) Lut = DPal->MakeLut(15);

					for (int y=0; y<Src->y; y++)
					{
						uchar *s = Src->Base + (y * Src->Line);
						uchar *d = Ptr;

						for (int x=0; x<Src->x; x++, s++, d++)
						{
							System24BitPixel *src = sc + *s;
							System24BitPixel *dst = dc + *d;
							int r = src->r + dst->r;
							int g = src->g + dst->g;
							int b = src->b + dst->b;
							*d = Lut[Rgb15(r, g, b)];
						}

						Ptr += Dest->Line;
					}
				}
				break;
			}
			
			#define Case(Px, Sz) \
				case Cs##Px: \
					AlphaBlt##Sz<G##Px>(Src, DPal, Lut); \
					break

			Case(Rgb15, 15);
			Case(Bgr15, 15);
			Case(Rgb16, 16);
			Case(Bgr16, 16);
			Case(Rgb24, 24);
			Case(Bgr24, 24);
			Case(Rgbx32, 24);
			Case(Bgrx32, 24);
			Case(Xrgb32, 24);
			Case(Xbgr32, 24);
			Case(Rgba32, 32);
			Case(Bgra32, 32);
			Case(Argb32, 32);
			Case(Abgr32, 32);
			Case(Rgb48, 48);
			Case(Bgr48, 48);
			Case(Rgba64, 64);
			Case(Bgra64, 64);
			Case(Argb64, 64);
			Case(Abgr64, 64);
			
			#undef Case
		}
	}

	return false;
}

