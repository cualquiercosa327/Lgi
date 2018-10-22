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
class App32Base : public GApplicator
{
protected:
	union
	{
		uint8 *u8;
		Pixel *p;
	};
	
	GPalette *PalAlpha;

public:
	App32Base()
	{
		p = NULL;
	}

	int GetVar(int Var) { return 0; }
	int SetVar(int Var, NativeInt Value) { return 0; }

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
};

template<typename Pixel, GColourSpace ColourSpace>
class App32NoAlpha : public App32Base<Pixel, ColourSpace>
{
public:
	void Set()
	{
		this->p->r = this->p32.r;
		this->p->g = this->p32.g;
		this->p->b = this->p32.b;
	}
	
	COLOUR Get()
	{
		return Rgb32(this->p->r,
					this->p->g,
					this->p->b);
	}
	
	void VLine(int height)
	{
		REG Pixel cp;
		cp.r = this->p32.r;
		cp.g = this->p32.g;
		cp.b = this->p32.b;
		
		while (height-- > 0)
		{
			*this->p = cp;
			this->u8 += this->Dest->Line;
		}
	}
	
	void Rectangle(int x, int y)
	{
		REG Pixel cp;
		cp.r = this->p32.r;
		cp.g = this->p32.g;
		cp.b = this->p32.b;
		
		REG int lines = y;
		REG ssize_t ystep = this->Dest->Line;
		while (lines-- > 0)
		{
			REG Pixel *i = this->p, *e = i + x;
			while (i < e)
			{
				*i++ = cp;
			}
			this->u8 += ystep;
		}
	}
	
	template<typename T>
	bool AlphaBlt(GBmpMem *Src, GBmpMem *SrcAlpha)
	{
		uchar *DivLut = Div255Lut;

		for (int y=0; y<Src->y; y++)
		{
			REG Pixel *d = this->p;
			REG T *s = (T*) (Src->Base + (y * Src->Line));
			REG T *e = s + Src->x;
			REG uint8 *a = Src->Base + (y * SrcAlpha->Line);

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

			this->u8 += this->Dest->Line;
		}
		
		return true;
	}
	
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = NULL)
	{
		if (!Src)
			return false;
		
		if (!SrcAlpha)
		{
			if (this->Dest->Cs == Src->Cs)
			{
				REG uchar *s = Src->Base;
				for (REG int y=0; y<Src->y; y++)
				{
					MemCpy(this->p, s, Src->x * sizeof(Pixel));
					s += Src->Line;
					this->u8 += this->Dest->Line;
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
					REG uint8 *s = Src->Base + (y * Src->Line);
					REG Pixel *d = this->p, *e = d + Src->x;
					while (d < e)
					{
						*d++ = map[*s++];
					}
					this->u8 += this->Dest->Line;
				}
			}
			else
			{
				GBmpMem Dst;
				Dst.Base = this->u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = this->Dest->Cs;
				Dst.Line = this->Dest->Line;				
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
class App32Alpha : public App32Base<Pixel, ColourSpace>
{
public:
	#define InitColour() \
		REG Pixel cp; \
		if (this->Dest->PreMul()) \
		{ \
			cp.r = ((int)this->p32.r * this->p32.a) / 255; \
			cp.g = ((int)this->p32.g * this->p32.a) / 255; \
			cp.b = ((int)this->p32.b * this->p32.a) / 255; \
			cp.a = this->p32.a; \
		} \
		else \
		{ \
			cp.r = this->p32.r; \
			cp.g = this->p32.g; \
			cp.b = this->p32.b; \
			cp.a = this->p32.a; \
		}

	
	void Set()
	{
		InitColour();
		*this->p = cp;
	}
	
	COLOUR Get()
	{
		return Rgba32(this->p->r, this->p->g, this->p->b, this->p->a);
	}
	
	void VLine(int height)
	{
		InitColour();
		
		while (height-- > 0)
		{
			*this->p = cp;
			this->u8 += this->Dest->Line;
		}
	}
	
	void Rectangle(int x, int y)
	{
		InitColour();
		
		REG int lines = y;
		REG ssize_t ystep = this->Dest->Line;
		while (lines-- > 0)
		{
			REG Pixel *i = this->p, *e = i + x;
			while (i < e)
			{
				*i++ = cp;
			}
			this->u8 += ystep;
		}
	}
	
	template<typename T>
	bool CopyBlt24(GBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			REG Pixel *d = this->p;
			REG T *s = (T*) (Src->Base + (y * Src->Line));
			REG T *e = s + Src->x;

			while (s < e)
			{
				d->r = s->r;
				d->g = s->g;
				d->b = s->b;
				d->a = 255;
				s++;
				d++;
			}

			this->u8 += this->Dest->Line;
		}
		
		return true;
	}

	template<typename T>
	bool CopyBlt32(GBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			REG Pixel *d = this->p;
			REG T *s = (T*) (Src->Base + (y * Src->Line));
			REG T *e = s + Src->x;

			while (s < e)
			{
				d->r = s->r;
				d->g = s->g;
				d->b = s->b;
				d->a = s->a;
				s++;
				d++;
			}

			this->u8 += this->Dest->Line;
		}
		
		return true;
	}
	
	template<typename T>
	bool AlphaBlt24(GBmpMem *Src, GBmpMem *SrcAlpha)
	{
		uchar *DivLut = Div255Lut;

		for (int y=0; y<Src->y; y++)
		{
			REG Pixel *d = this->p;
			REG T *s = (T*) (Src->Base + (y * Src->Line));
			REG T *e = s + Src->x;
			REG uint8 *a = Src->Base + (y * SrcAlpha->Line);

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
					OverNpm24toNpm32(s, d, sa);
				}
				
				s++;
				d++;
			}

			this->u8 += this->Dest->Line;
		}
		
		return true;
	}

	template<typename T>
	bool AlphaBlt32(GBmpMem *Src, GBmpMem *SrcAlpha)
	{
		REG uchar *DivLut = Div255Lut;

		for (int y=0; y<Src->y; y++)
		{
			REG Pixel *d = this->p;
			REG T *s = (T*) (Src->Base + (y * Src->Line));
			REG T *e = s + Src->x;
			REG uint8 *a = Src->Base + (y * SrcAlpha->Line);

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
					OverNpm32toNpm32(s, d);
				}
				
				s++;
				d++;
			}

			this->u8 += this->Dest->Line;
		}
		
		return true;
	}
	
	void ConvertPreMul(GBmpMem *m)
	{
		REG uchar *DivLut = Div255Lut;
		for (int y=0; y<m->y; y++)
		{
			REG Pixel *d = (Pixel*) (m->Base + (y * m->Line));
			REG Pixel *e = d + m->x;
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
		
		if (Src->Cs == CsIndex8)
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
			REG uchar *DivLut = Div255Lut;
			for (int y=0; y<Src->y; y++)
			{
				REG uint8 *i = in;
				REG Pixel *o = this->p, *e = this->p + Src->x;
				
				if (SrcAlpha)
				{
					REG uint8 *alpha = SrcAlpha->Base + (y * SrcAlpha->Line);
					while (o < e)
					{
						REG Pixel *src = map + *i++;
						src->a = *alpha++;
						if (src->a == 255)
						{
							o->r = src->r;
							o->g = src->g;
							o->b = src->b;
							o->a = 255;
						}
						else if (src->a > 0)
						{
							OverNpm32toNpm32(src, o);
						}
						o++;
					}
					
					if (this->Dest->PreMul())
					{
						o = this->p;
						while (o < e)
						{
							o->r = DivLut[o->r * o->a];
							o->g = DivLut[o->g * o->a];
							o->b = DivLut[o->b * o->a];
							o++;
						}
					}
				}
				else
				{
					while (o < e)
					{
						*o++ = map[*i++];
					}
				}
				
				in += Src->Line;
				this->u8 += this->Dest->Line;
			}
		}
		else if (!SrcAlpha)
		{
			if (this->Dest->Cs == Src->Cs)
			{
				REG uchar *s = Src->Base;
				for (REG int y=0; y<Src->y; y++)
				{
					MemCpy(this->p, s, Src->x * sizeof(Pixel));
					s += Src->Line;
					this->u8 += this->Dest->Line;
				}
			}
			else
			{
				GBmpMem Dst;
				Dst.Base = this->u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = this->Dest->Cs;
				Dst.Line = this->Dest->Line;
				Dst.PreMul(this->Dest->PreMul());
				if (!LgiRopUniversal(&Dst, Src, false))
				{
					return false;
				}
				if (this->Dest->PreMul() && !Src->PreMul())
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
 //, a = this->p32.a
#define CreateOpNoAlpha(name, opcode, alpha)													\
	template<typename Pixel, GColourSpace ColourSpace>											\
	class App32##name##alpha : public App32Base<Pixel, ColourSpace>								\
	{																							\
	public:																						\
		void Set() { this->p->r opcode this->p32.r; this->p->g opcode this->p32.g; 				\
					this->p->b opcode this->p32.b; }											\
		COLOUR Get() { return Rgb32(this->p->r, this->p->g, this->p->b); }						\
		void VLine(int height)																	\
		{																						\
			REG uint8 r = this->p32.r, g = this->p32.g, b = this->p32.b;	\
			while (height-- > 0)																\
			{																					\
				this->p->r opcode r; this->p->g opcode g; this->p->b opcode b;					\
				this->u8 += this->Dest->Line;													\
			}																					\
		}																						\
		void Rectangle(int x, int y)															\
		{																						\
			REG uint8 r = this->p32.r, g = this->p32.g, b = this->p32.b;						\
			REG int lines = y;																	\
			REG ssize_t ystep = this->Dest->Line;													\
			while (lines-- > 0)																	\
			{																					\
				REG Pixel *i = this->p, *e = i + x;												\
				while (i < e)																	\
				{																				\
					i->r opcode r; i->g opcode g; i->b opcode b;								\
					i++;																		\
				}																				\
				this->u8 += ystep;																\
			}																					\
		}																						\
		bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0) { return false; }			\
	};

#define CreateOpAlpha(name, opcode, alpha)														\
	template<typename Pixel, GColourSpace ColourSpace>											\
	class App32##name##alpha : public App32Base<Pixel, ColourSpace>								\
	{																							\
	public:																						\
		void Set() { this->p->r opcode this->p32.r; this->p->g opcode this->p32.g; 				\
					 this->p->b opcode this->p32.b; this->p->a opcode this->p32.a; }			\
		COLOUR Get() { return Rgba32(this->p->r, this->p->g, this->p->b, this->p->a); }			\
		void VLine(int height)																	\
		{																						\
			REG uint8 r = this->p32.r, g = this->p32.g, b = this->p32.b, a = this->p32.a;	\
			while (height-- > 0)																\
			{																					\
				this->p->r opcode r; this->p->g opcode g; this->p->b opcode b;					\
				this->p->a opcode a;															\
				this->u8 += this->Dest->Line;													\
			}																					\
		}																						\
		void Rectangle(int x, int y)															\
		{																						\
			REG uint8 r = this->p32.r, g = this->p32.g, b = this->p32.b, a = this->p32.a;	\
			REG int lines = y;																\
			REG ssize_t ystep = this->Dest->Line;												\
			while (lines-- > 0)																	\
			{																					\
				REG Pixel *i = this->p, *e = i + x;										\
				while (i < e)																	\
				{																				\
					i->r opcode r; i->g opcode g; i->b opcode b;								\
					i->a opcode a;																\
					i++;																		\
				}																				\
				this->u8 += ystep;																\
			}																					\
		}																						\
		bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0) { return false; }			\
	};


CreateOpNoAlpha(And, &=, NoAlpha)
CreateOpAlpha(And, &=, Alpha)
CreateOpNoAlpha(Or, |=, NoAlpha)
CreateOpAlpha(Or, |=, Alpha)
CreateOpNoAlpha(Xor, ^=, NoAlpha)
CreateOpAlpha(Xor, ^=, Alpha)

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
            
            #undef AppCase
		}
	}
	else if (Op == GDC_OR)
	{
		switch (Cs)
		{
			#define AppCase(name, alpha) \
				case Cs##name: return new App32Or##alpha<G##name, Cs##name>();
			
			AppCase(Rgba32, Alpha);
			AppCase(Bgra32, Alpha);
			AppCase(Argb32, Alpha);
			AppCase(Abgr32, Alpha);
			AppCase(Xrgb32, NoAlpha);
			AppCase(Rgbx32, NoAlpha);
			AppCase(Xbgr32, NoAlpha);
			AppCase(Bgrx32, NoAlpha);
            default: break;

            #undef AppCase
		}
	}
	else if (Op == GDC_AND)
	{
		switch (Cs)
		{
			#define AppCase(name, alpha) \
				case Cs##name: return new App32And##alpha<G##name, Cs##name>();
			
			AppCase(Rgba32, Alpha);
			AppCase(Bgra32, Alpha);
			AppCase(Argb32, Alpha);
			AppCase(Abgr32, Alpha);
			AppCase(Xrgb32, NoAlpha);
			AppCase(Rgbx32, NoAlpha);
			AppCase(Xbgr32, NoAlpha);
			AppCase(Bgrx32, NoAlpha);
            default: break;

            #undef AppCase
		}
	}
	else if (Op == GDC_XOR)
	{
		switch (Cs)
		{
			#define AppCase(name, alpha) \
				case Cs##name: return new App32Xor##alpha<G##name, Cs##name>();
			
			AppCase(Rgba32, Alpha);
			AppCase(Bgra32, Alpha);
			AppCase(Argb32, Alpha);
			AppCase(Abgr32, Alpha);
			AppCase(Xrgb32, NoAlpha);
			AppCase(Rgbx32, NoAlpha);
			AppCase(Xbgr32, NoAlpha);
			AppCase(Bgrx32, NoAlpha);
            default: break;

            #undef AppCase
		}
	}
	
	return NULL;
}


