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

#undef NonPreMulOver32
#undef NonPreMulAlpha
#define NonPreMulOver32(c)	d->c = ((s->c * sa) + (DivLut[d->c * da] * o)) / d->a
#define NonPreMulAlpha		d->a = (d->a + sa) - DivLut[d->a * sa]

/// 32 bit rgb applicators
class LgiClass GdcApp32 : public GApplicator
{
protected:
	union {
		uint8 *u8;
		uint32 *u32;
		System32BitPixel *p;
	} Ptr;

public:
	const char *GetClass() { return "GdcApp32"; }
	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a);
	void SetPtr(int x, int y);
	void IncX();
	void IncY();
	void IncPtr(int X, int Y);
	COLOUR Get();
};

class LgiClass GdcApp32Set : public GdcApp32
{
public:
	const char *GetClass() { return "GdcApp32Set"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32And : public GdcApp32
{
public:
	const char *GetClass() { return "GdcApp32And"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32Or : public GdcApp32
{
public:
	const char *GetClass() { return "GdcApp32Or"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32Xor : public GdcApp32
{
public:
	const char *GetClass() { return "GdcApp32Xor"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

/////////////////////////////////////////////////////////////////////////////////////////
template<typename Pixel, GColourSpace ColourSpace>
class App32NoAlpha : public GApplicator
{
	union
	{
		uint8 *u8;
		Pixel *p;
	};
	
	int ConstAlpha;
	GPalette *PalAlpha;

public:
	App32NoAlpha()
	{
		p = NULL;
		ConstAlpha = 255;
		PalAlpha = NULL;
	}

	int GetVar(int Var) { LgiAssert(0); return 0; }
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
					MemCpy(p, s, Src->x * 3);
					s += Src->Line;
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
				if (!LgiRopUniversal(&Dst, Src))
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

template<typename Pixel, GColourSpace ColourSpace>
class App32Alpha : public GApplicator
{
	union
	{
		uint8 *u8;
		Pixel *p;
	};
	
	int ConstAlpha;
	GPalette *PalAlpha;

public:
	App32Alpha()
	{
		p = NULL;
		ConstAlpha = 255;
		PalAlpha = NULL;
	}

	int GetVar(int Var) { LgiAssert(0); return 0; }
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
		p->r = p32.r;
		p->g = p32.g;
		p->b = p32.b;
		p->a = p32.a;
	}
	
	COLOUR Get()
	{
		return Rgba32(p->r, p->g, p->b, p->a);
	}
	
	void VLine(int height)
	{
		Pixel cp;
		cp.r = p32.r;
		cp.g = p32.g;
		cp.b = p32.b;
		cp.a = p32.a;
		
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
		cp.a = p32.a;
		
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
					d->a = 255;
				}
				else if (sa > 0)
				{
					uint8 o = 255 - sa;
					int da = d->a;

					NonPreMulAlpha;
					NonPreMulOver32(r);
					NonPreMulOver32(g);
					NonPreMulOver32(b);
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
					MemCpy(p, s, Src->x * 3);
					s += Src->Line;
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
				if (!LgiRopUniversal(&Dst, Src))
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

/////////////////////////////////////////////////////////////////////////////////////////
GApplicator *GApp32::Create(GColourSpace Cs, int Op)
{
	if (Cs == System32BitColourSpace)
	{
		switch (Op)
		{
			case GDC_SET:
				return new GdcApp32Set;
			case GDC_AND:
				return new GdcApp32And;
			case GDC_OR:
				return new GdcApp32Or;
			case GDC_XOR:
				return new GdcApp32Xor;
		}
	}
	else
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


///////////////////////////////////////////////////////////////////////////////////////////
#define AddPtr(p, i)		p = (uint32*) (((char*)(p))+(i))

bool GdcApp32::SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
{
	if (d && d->Cs == System32BitColourSpace)
	{
		Dest = d;
		Pal = p;
		Ptr.u8 = d->Base;
		Alpha = 0;
		return true;
	}
	else LgiAssert(0);
	
	return false;
}

void GdcApp32::SetPtr(int x, int y)
{
	LgiAssert(Dest && Dest->Base);
	Ptr.u8 = (Dest->Base + ((y * Dest->Line) + (x << 2)));
}

void GdcApp32::IncX()
{
	Ptr.p++;
}

void GdcApp32::IncY()
{
	Ptr.u8 += Dest->Line;
}

void GdcApp32::IncPtr(int X, int Y)
{
	Ptr.u8 += (Y * Dest->Line) + (X << 2);
}

COLOUR GdcApp32::Get()
{
	return Rgba32(Ptr.p->r, Ptr.p->g, Ptr.p->b, Ptr.p->a);
}

// 32 bit set sub functions
void GdcApp32Set::Set()
{
	*Ptr.u32 = c;
}

void GdcApp32Set::VLine(int height)
{
	while (height--)
	{
		*Ptr.u32 = c;
		Ptr.u8 += Dest->Line;
	}
}

void GdcApp32Set::Rectangle(int x, int y)
{
#if defined(GDC_USE_ASM) && defined(_MSC_VER)

	uint32 *p = Ptr.u32;
	int Line = Dest->Line;
	COLOUR fill = c; // System24BitPixel

	if (x && y)
	{
		_asm {
			mov esi, p
			mov eax, fill
			mov edx, Line
		LoopY:	mov edi, esi
			add esi, edx
			mov ecx, x
		LoopX:	mov [edi], eax
			add edi, 4
			dec ecx
			jnz LoopX
			dec y
			jnz LoopY
		}
	}
#else
	while (y--)
	{
		register uint32 *p = Ptr.u32;
		register uint32 *e = p + x;
		register uint32 fill = c;
		while (p < e)
		{
			*p++ = fill;
		}
		Ptr.u8 += Dest->Line;
	}
#endif
}

bool GdcApp32Set::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Cs)
		{
			case CsIndex8:
			{
				if (SPal)
				{
					System32BitPixel c[256];
					for (int i=0; i<256; i++)
					{
						GdcRGB *p = (*SPal)[i];
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
						c[i].a = 0xff;
					}
					
					for (int y=0; y<Src->y; y++)
					{
						uchar *s = (uchar*) (Src->Base + (Src->Line * y));
						System32BitPixel *d = Ptr.p;
						System32BitPixel *e = d + Src->x;
						
						while (d < e)
						{
							*d++ = c[*s++];
						}

						Ptr.u8 += Dest->Line;
					}
				}
				else
				{
					for (int y=0; y<Src->y; y++)
					{
						uchar *s = (uchar*) (Src->Base + (Src->Line * y));
						System32BitPixel *d = Ptr.p;
						System32BitPixel *e = d + Src->x;

						while (d < e)
						{
							d->r = *s;
							d->g = *s;
							d->b = *s;
							s++;
							d++;
						}

						Ptr.u8 += Dest->Line;
					}
				}
				break;
			}
			default:
			{
				GBmpMem Dst;
				Dst.Base = Ptr.u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = Dest->Cs;
				Dst.Line = Dest->Line;				
				if (!LgiRopUniversal(&Dst, Src))
				{
					return false;
				}
				break;
			}
		}
	}
	return true;
}

// 32 bit or sub functions
void GdcApp32Or::Set()
{
	*Ptr.u32 |= c;
}

void GdcApp32Or::VLine(int height)
{
	while (height--)
	{
		*Ptr.u32 |= c;
		Ptr.u8 += Dest->Line;
	}
}

void GdcApp32Or::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++)
			*Ptr.u32++ |= c;
		Ptr.u8 += Dest->Line - (x << 2);
	}
}

bool GdcApp32Or::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Cs)
		{
			default:
				LgiAssert(0);
				break;
			case System32BitColourSpace:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemOr(Ptr.u8, s, Src->x << 2);
					s += Src->Line;
					Ptr.u8 += Dest->Line;
				}
				break;
			}
		}
	}
	return true;
}

// 32 bit AND sub functions
void GdcApp32And::Set()
{
	*Ptr.u32 &= c;
}

void GdcApp32And::VLine(int height)
{
	while (height--)
	{
		*Ptr.u32 &= c;
		Ptr.u8 += Dest->Line;
	}
}

void GdcApp32And::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++)
			*Ptr.u32++ &= c;
		Ptr.u8 += Dest->Line - (x << 2);
	}
}

bool GdcApp32And::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemAnd(Ptr.u8, s, Src->x << 2);
			s += Src->Line;
			Ptr.u8 += Dest->Line;
		}
	}
	return true;
}

// 32 bit XOR sub functions
void GdcApp32Xor::Set()
{
	*Ptr.u32 ^= c;
}

void GdcApp32Xor::VLine(int height)
{
	while (height--)
	{
		*Ptr.u32 ^= c;
		Ptr.u8 += Dest->Line;
	}
}

void GdcApp32Xor::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++)
			*Ptr.u32++ ^= c;
		Ptr.u8 += Dest->Line;
	}
}

bool GdcApp32Xor::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemXor(Ptr.u8, s, Src->x << 2);
			s += Src->Line;
			Ptr.u8 += Dest->Line;
		}
	}
	return true;
}
