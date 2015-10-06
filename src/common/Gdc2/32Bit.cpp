/**
	\file
	\author Matthew Allen
	\date 29/8/1997
	\brief 32 bit primitives
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"
#include "GPixelRops.h"

#undef NonPreMulOver32
#undef NonPreMulAlpha

/////////////////////////////////////////////////////////////////////////////////////////
template<typename Pixel, GColourSpace ColourSpace>
class App32NoAlpha : public GApplicator
{
	union
	{
		uint8 *u8;
		Pixel *p;
	};
	
	GPalette *PalAlpha;

public:
	App32NoAlpha()
	{
		p = NULL;
	}

	int GetVar(int Var)
	{
		return 0;
	
	}

	int SetVar(int Var, NativeInt Value)
	{
		return 0;
	}

	bool SetSurface(GBmpMem *d, GPalette *pal = NULL, GBmpMem *a = NULL)
	{
		if (d && d->Cs == ColourSpace)
		{
			Dest = d;
			Pal = pal;
			u8 = d->Base;
			Alpha = 0;
			return true;
		}
		return false;
	}

	void SetPtr(int x, int y)
	{
		u8 = Dest->Base + (y * Dest->Line);
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
		u8 += Y * Dest->Line;
	}
	
	void Set()
	{
		p->r = p32.r;
		p->g = p32.g;
		p->b = p32.b;
	}
	
	COLOUR Get()
	{
		return Rgb32(p->r, p->g, p->b);
	}
	
	void VLine(int height)
	{
		Pixel cp;
		cp.r = p32.r;
		cp.g = p32.g;
		cp.b = p32.b;
		
		while (height-- > 0)
		{
			*p = cp;
			u8 += Dest->Line;
		}
	}
	
	void Rectangle(int x, int y)
	{
		register Pixel cp;
		cp.r = p32.r;
		cp.g = p32.g;
		cp.b = p32.b;
		
		register int lines = y;
		register int ystep = Dest->Line;
		while (lines-- > 0)
		{
			register Pixel *i = p, *e = i + x;
			while (i < e)
			{
				*i++ = cp;
			}
			u8 += ystep;
		}
	}
	
	template<typename T>
	bool AlphaBlt(GBmpMem *Src, GBmpMem *SrcAlpha)
	{
		uchar *DivLut = Div255Lut;

		for (int y=0; y<Src->y; y++)
		{
			register Pixel *d = p;
			register T *s = (T*) (Src->Base + (y * Src->Line));
			register T *e = s + Src->x;
			register uint8 *a = Src->Base + (y * SrcAlpha->Line);

			while (s < e)
			{
				uint8 sa = *a++;
				if (sa == 255)
				{
					d->r = s->r;
					d->g = s->g;
					d->b = s->b;
				}
				else if (sa > 0)
				{
					uint8 o = 255 - sa;

					// NonPreMulAlpha;
					#define NonPreMulOver32NoAlpha(c)		d->c = ((s->c * sa) + (DivLut[d->c * 255] * o)) / 255
					
					NonPreMulOver32NoAlpha(r);
					NonPreMulOver32NoAlpha(g);
					NonPreMulOver32NoAlpha(b);
				}
				
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = NULL)
	{
		if (!Src)
			return false;
		
		if (!SrcAlpha)
		{
			if (Dest->Cs == Src->Cs)
			{
				register uchar *s = Src->Base;
				for (register int y=0; y<Src->y; y++)
				{
					MemCpy(p, s, Src->x * sizeof(Pixel));
					s += Src->Line;
					u8 += Dest->Line;
				}
			}
			else if (Src->Cs == CsIndex8)
			{
				Pixel map[256];
				for (int i=0; i<256; i++)
				{
					GdcRGB *rgb = SPal ? (*SPal)[i] : NULL;
					if (rgb)
					{
						map[i].r = rgb->r;
						map[i].g = rgb->g;
						map[i].b = rgb->b;
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
					register uint8 *s = Src->Base + (y * Src->Line);
					register Pixel *d = p, *e = d + Src->x;
					while (d < e)
					{
						*d++ = map[*s++];
					}
					u8 += Dest->Line;
				}
			}
			else
			{
				GBmpMem Dst;
				Dst.Base = u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = Dest->Cs;
				Dst.Line = Dest->Line;				
				if (!LgiRopUniversal(&Dst, Src, false))
				{
					return false;
				}
			}
		}
		else
		{
			switch (Src->Cs)
			{
				#define AlphaCase(name) \
					case Cs##name: return AlphaBlt<G##name>(Src, SrcAlpha);

				AlphaCase(Rgb24);
				AlphaCase(Bgr24);
				AlphaCase(Xrgb32);
				AlphaCase(Xbgr32);
				AlphaCase(Rgbx32);
				AlphaCase(Bgrx32);

				AlphaCase(Argb32);
				AlphaCase(Abgr32);
				AlphaCase(Rgba32);
				AlphaCase(Bgra32);
				
				#undef AlphaCase

				default:
					LgiAssert(!"Impl me.");
					break;
			}
		}
		
		return false;
	}
};

template<typename Pixel, GColourSpace ColourSpace>
class App32Alpha : public GApplicator
{
	union
	{
		uint8 *u8;
		Pixel *p;
	};
	
public:
	App32Alpha()
	{
		p = NULL;
	}

	int GetVar(int Var)
	{
		return 0;
	}
	
	int SetVar(int Var, NativeInt Value)
	{
		return 0;
	}

	bool SetSurface(GBmpMem *d, GPalette *pal = NULL, GBmpMem *a = NULL)
	{
		if (d && d->Cs == ColourSpace)
		{
			Dest = d;
			Pal = pal;
			p = (Pixel*) d->Base;
			Alpha = 0;
			return true;
		}
		return false;
	}

	void SetPtr(int x, int y)
	{
		p = (Pixel*) (Dest->Base + (y * Dest->Line) + (x * sizeof(Pixel)));
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
		u8 += Y * Dest->Line;
	}

	#define InitColour() \
		register Pixel cp; \
		if (Dest->PreMul()) \
		{ \
			cp.r = ((int)p32.r * p32.a) / 255; \
			cp.g = ((int)p32.g * p32.a) / 255; \
			cp.b = ((int)p32.b * p32.a) / 255; \
			cp.a = p32.a; \
		} \
		else \
		{ \
			cp.r = p32.r; \
			cp.g = p32.g; \
			cp.b = p32.b; \
			cp.a = p32.a; \
		}

	
	void Set()
	{
		InitColour();
		*p = cp;
	}
	
	COLOUR Get()
	{
		return Rgba32(p->r, p->g, p->b, p->a);
	}
	
	void VLine(int height)
	{
		InitColour();
		
		while (height-- > 0)
		{
			*p = cp;
			u8 += Dest->Line;
		}
	}
	
	void Rectangle(int x, int y)
	{
		InitColour();
		
		register int lines = y;
		register int ystep = Dest->Line;
		while (lines-- > 0)
		{
			register Pixel *i = p, *e = i + x;
			while (i < e)
			{
				*i++ = cp;
			}
			u8 += ystep;
		}
	}
	
	template<typename T>
	bool CopyBlt24(GBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			register Pixel *d = p;
			register T *s = (T*) (Src->Base + (y * Src->Line));
			register T *e = s + Src->x;

			while (s < e)
			{
				d->r = s->r;
				d->g = s->g;
				d->b = s->b;
				d->a = 255;
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}

	template<typename T>
	bool CopyBlt32(GBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			register Pixel *d = p;
			register T *s = (T*) (Src->Base + (y * Src->Line));
			register T *e = s + Src->x;

			while (s < e)
			{
				d->r = s->r;
				d->g = s->g;
				d->b = s->b;
				d->a = s->a;
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	template<typename T>
	bool AlphaBlt24(GBmpMem *Src, GBmpMem *SrcAlpha)
	{
		uchar *DivLut = Div255Lut;

		for (int y=0; y<Src->y; y++)
		{
			register Pixel *d = p;
			register T *s = (T*) (Src->Base + (y * Src->Line));
			register T *e = s + Src->x;
			register uint8 *a = Src->Base + (y * SrcAlpha->Line);

			while (s < e)
			{
				uint8 sa = *a++;
				if (sa == 255)
				{
					d->r = s->r;
					d->g = s->g;
					d->b = s->b;
					d->a = 255;
				}
				else if (sa > 0)
				{
					NpmOver24to32(s, d, sa);
				}
				
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}

	template<typename T>
	bool AlphaBlt32(GBmpMem *Src, GBmpMem *SrcAlpha)
	{
		uchar *DivLut = Div255Lut;

		for (int y=0; y<Src->y; y++)
		{
			register Pixel *d = p;
			register T *s = (T*) (Src->Base + (y * Src->Line));
			register T *e = s + Src->x;
			register uint8 *a = Src->Base + (y * SrcAlpha->Line);

			while (s < e)
			{
				uint8 sa = *a++;
				if (sa == 255)
				{
					d->r = s->r;
					d->g = s->g;
					d->b = s->b;
					d->a = 255;
				}
				else if (sa > 0)
				{
					NpmOver32to32(s, d);
				}
				
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	void ConvertPreMul(GBmpMem *m)
	{
		register uchar *DivLut = Div255Lut;
		for (int y=0; y<m->y; y++)
		{
			register Pixel *d = (Pixel*) (m->Base + (y * m->Line));
			register Pixel *e = d + m->x;
			while (d < e)
			{
				d->r = DivLut[d->r * d->a];
				d->g = DivLut[d->g * d->a];
				d->b = DivLut[d->b * d->a];
				d++;
			}
		}
	}
	
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = NULL)
	{
		if (!Src)
			return false;
		
		if (!SrcAlpha)
		{
			if (Dest->Cs == Src->Cs)
			{
				register uchar *s = Src->Base;
				for (register int y=0; y<Src->y; y++)
				{
					MemCpy(p, s, Src->x * sizeof(Pixel));
					s += Src->Line;
					u8 += Dest->Line;
				}
			}
			else if (Src->Cs == CsIndex8)
			{
				Pixel map[256];
				for (int i=0; i<256; i++)
				{
					GdcRGB *p = SPal ? (*SPal)[i] : NULL;
					if (p)
					{
						map[i].r = p->r;
						map[i].g = p->g;
						map[i].b = p->b;
					}
					else
					{
						map[i].r = map[i].g = map[i].b = i;
					}
					map[i].a = 255;
				}
				
				uint8 *in = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					register uint8 *i = in;
					register Pixel *o = p, *e = p + Src->x;
					while (o < e)
					{
						*o++ = map[*i++];
					}
					
					in += Src->Line;
					u8 += Dest->Line;
				}
			}
			else
			{
				GBmpMem Dst;
				Dst.Base = u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = Dest->Cs;
				Dst.Line = Dest->Line;
				Dst.PreMul(Dest->PreMul());
				if (!LgiRopUniversal(&Dst, Src, false))
				{
					return false;
				}
				if (Dest->PreMul() && !Src->PreMul())
				{
					ConvertPreMul(&Dst);
				}
			}
		}
		else
		{
			switch (Src->Cs)
			{
				#define AlphaCase(name, sz) \
					case Cs##name: return AlphaBlt##sz<G##name>(Src, SrcAlpha);

				AlphaCase(Rgb24, 24);
				AlphaCase(Bgr24, 24);
				AlphaCase(Xrgb32, 24);
				AlphaCase(Xbgr32, 24);
				AlphaCase(Rgbx32, 24);
				AlphaCase(Bgrx32, 24);

				AlphaCase(Argb32, 32);
				AlphaCase(Abgr32, 32);
				AlphaCase(Rgba32, 32);
				AlphaCase(Bgra32, 32);
				
				#undef AlphaCase

				default:
					LgiAssert(!"Impl me.");
					break;
			}
		}
		
		return false;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
GApplicator *GApp32::Create(GColourSpace Cs, int Op)
{
	if (Op == GDC_SET)
	{
		switch (Cs)
		{
			#define AppCase(name, alpha) \
				case Cs##name: return new App32##alpha<G##name, Cs##name>();
			
			AppCase(Rgba32, Alpha);
			AppCase(Bgra32, Alpha);
			AppCase(Argb32, Alpha);
			AppCase(Abgr32, Alpha);
			AppCase(Xrgb32, NoAlpha);
			AppCase(Rgbx32, NoAlpha);
			AppCase(Xbgr32, NoAlpha);
			AppCase(Bgrx32, NoAlpha);
            default: break;
		}
	}
	
	return NULL;
}


