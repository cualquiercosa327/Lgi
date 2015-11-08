/**
	\file
	\author Matthew Allen
	\date 24/2/2014
	\brief 48 bit primitives
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

#define BytePtr	((uint8*&)Ptr)
#undef NonPreMulOver48
#define NonPreMulOver48(c)	d->c = ((s->c * sa) + ((d->c * 0xffff) / 0xffff * o)) / 0xffff

template<typename Pixel, GColourSpace ColourSpace>
class App48 : public GApplicator
{
	union
	{
		uint8 *u8;
		Pixel *p;
	};
	
	int Op;
	int ConstAlpha;
	GPalette *PalAlpha;

public:
	App48(int op)
	{
		Op = op;
		p = NULL;
		ConstAlpha = 0xffff;
		PalAlpha = NULL;
	}

	int GetVar(int Var)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A:
			{
				return ConstAlpha;
				break;
			}
			default:
			{
				LgiAssert(!"impl me.");
				break;
			}
		}
		return 0;
	}
	
	int SetVar(int Var, NativeInt Value)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A:
			{
				ConstAlpha = Value;
				break;
			}
			case GAPP_ALPHA_PAL:
			{
				PalAlpha = (GPalette*)Value;
				break;
			}
			default:
			{
				LgiAssert(!"impl me.");
				break;
			}
		}
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
	
	void Set()
	{
		p->r = ((uint16)p24.r << 8) | p24.r;
		p->g = ((uint16)p24.g << 8) | p24.g;
		p->b = ((uint16)p24.b << 8) | p24.b;
	}
	
	COLOUR Get()
	{
		return Rgb24(p->r >> 8, p->g >> 8, p->b >> 8);
	}
	
	void VLine(int height)
	{
		Pixel cp;
		cp.r = ((uint16)p24.r << 8) | p24.r;
		cp.g = ((uint16)p24.g << 8) | p24.g;
		cp.b = ((uint16)p24.b << 8) | p24.b;
		
		while (height-- > 0)
		{
			*p = cp;
			u8 += Dest->Line;
		}
	}
	
	void Rectangle(int x, int y)
	{
		Pixel cp;
		cp.r = ((uint16)p24.r << 8) | p24.r;
		cp.g = ((uint16)p24.g << 8) | p24.g;
		cp.b = ((uint16)p24.b << 8) | p24.b;
		
		while (y-- > 0)
		{
			Pixel *i = p, *e = i + x;
			while (i < e)
			{
				*i++ = cp;
			}
			u8 += Dest->Line;
		}
	}
	
	template<typename T>
	bool CopyBlt(GBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			Pixel *d = p;
			T *s = (T*) (Src->Base + (y * Src->Line));
			T *e = s + Src->x;

			while (s < e)
			{
				d->r = s->r;
				d->g = s->g;
				d->b = s->b;
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	template<typename T>
	bool AlphaBlt(GBmpMem *Src, GBmpMem *SrcAlpha)
	{
		uchar *DivLut = Div255Lut;

		for (int y=0; y<Src->y; y++)
		{
			Pixel *d = p;
			T *s = (T*) (Src->Base + (y * Src->Line));
			T *e = s + Src->x;
			uint16 *a = (uint16*) (Src->Base + (y * SrcAlpha->Line));

			while (s < e)
			{
				uint16 sa = *a++;
				if (sa == 255)
				{
					d->r = s->r;
					d->g = s->g;
					d->b = s->b;
				}
				else if (sa > 0)
				{
					uint16 o = 0xffff - sa;
					d->r = ((s->r * sa) + (d->r * o)) / 0xffff;
					d->g = ((s->g * sa) + (d->g * o)) / 0xffff;
					d->b = ((s->b * sa) + (d->b * o)) / 0xffff;
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
			if (Src->Cs == CsIndex8)
			{
				Pixel map[256];
				for (int i=0; i<256; i++)
				{
					GdcRGB *rgb = SPal ? (*SPal)[i] : NULL;
					if (rgb)
					{
						map[i].r = G8bitTo16bit(rgb->r);
						map[i].g = G8bitTo16bit(rgb->g);
						map[i].b = G8bitTo16bit(rgb->b);
					}
					else
					{
						map[i].r = G8bitTo16bit(i);
						map[i].g = G8bitTo16bit(i);
						map[i].b = G8bitTo16bit(i);
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
				if (!LgiRopUniversal(&Dst, Src, Op==GDC_ALPHA))
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

				default:
					LgiAssert(!"Impl me.");
					break;
			}
		}
		
		return false;
	}
};

class G48BitFactory : public GApplicatorFactory
{
public:
	GApplicator *Create(GColourSpace Cs, int Op)
	{
		switch (Cs)
		{
			#define Case48(name) \
				case Cs##name: \
					return new App48<G##name, Cs##name>(Op);
			
			Case48(Rgb48);
			Case48(Bgr48);
			default:
				break;
		}
		return 0;
	}
} App48Factory;
