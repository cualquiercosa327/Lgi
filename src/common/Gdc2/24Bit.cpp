/**
	\file
	\author Matthew Allen
	\date 21/3/1997
	\brief 24 bit primitives
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

#define BytePtr	((uint8*&)Ptr)
#undef NonPreMulOver24
#define NonPreMulOver24(c)	d->c = ((s->c * sa) + (DivLut[d->c * 255] * o)) / 255

/// 24 bit rgb applicators
class LgiClass GdcApp24 : public GApplicator
{
protected:
	System24BitPixel *Ptr;

public:
	GdcApp24()
	{
		Ptr = NULL;
	}

	const char *GetClass() { return "GdcApp24"; }
	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a);
	void SetPtr(int x, int y);
	void IncX();
	void IncY();
	void IncPtr(int X, int Y);
	COLOUR Get();
};

class LgiClass GdcApp24Set : public GdcApp24 {
public:
	const char *GetClass() { return "GdcApp24Set"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp24And : public GdcApp24 {
public:
	const char *GetClass() { return "GdcApp24And"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp24Or : public GdcApp24 {
public:
	const char *GetClass() { return "GdcApp24Or"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp24Xor : public GdcApp24 {
public:
	const char *GetClass() { return "GdcApp24Xor"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

template<typename Pixel, GColourSpace ColourSpace>
class App24 : public GApplicator
{
	union
	{
		uint8 *u8;
		Pixel *p;
	};
	
	int ConstAlpha;
	GPalette *PalAlpha;

public:
	App24()
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
		p->r = p24.r;
		p->g = p24.g;
		p->b = p24.b;
	}
	
	COLOUR Get()
	{
		return Rgb24(p->r, p->g, p->b);
	}
	
	void VLine(int height)
	{
		Pixel cp;
		cp.r = p24.r;
		cp.g = p24.g;
		cp.b = p24.b;
		
		while (height-- > 0)
		{
			*p = cp;
			u8 += Dest->Line;
		}
	}
	
	void Rectangle(int x, int y)
	{
		Pixel cp;
		cp.r = p24.r;
		cp.g = p24.g;
		cp.b = p24.b;
		
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
			uint8 *a = Src->Base + (y * SrcAlpha->Line);

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
					NonPreMulOver24(r);
					NonPreMulOver24(g);
					NonPreMulOver24(b);
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
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemCpy(p, s, Src->x * 3);
					s += Src->Line;
					u8 += Dest->Line;
				}
			}
			else
			{
				switch (Src->Cs)
				{
					#define CopyCase(name) \
						case Cs##name: return CopyBlt<G##name>(Src);

					CopyCase(Rgb24);
					CopyCase(Bgr24);
					CopyCase(Xrgb32);
					CopyCase(Xbgr32);
					CopyCase(Rgbx32);
					CopyCase(Bgrx32);

					CopyCase(Argb32);
					CopyCase(Abgr32);
					CopyCase(Rgba32);
					CopyCase(Bgra32);
					default:
						LgiAssert(!"Impl me.");
						break;
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

GApplicator *GApp24::Create(GColourSpace Cs, int Op)
{
	if (Cs == System24BitColourSpace)
	{
		switch (Op)
		{
			case GDC_SET:
				return new GdcApp24Set;
			case GDC_AND:
				return new GdcApp24And;
			case GDC_OR:
				return new GdcApp24Or;
			case GDC_XOR:
				return new GdcApp24Xor;
		}
	}
	else
	{
		switch (Cs)
		{
			#define Case24(name) \
				case Cs##name: \
					return new App24<G##name, Cs##name>();
			
			Case24(Rgb24);
			Case24(Bgr24);
			Case24(Xrgb32);
			Case24(Rgbx32);
			Case24(Xbgr32);
			Case24(Bgrx32);
            default: break;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////
#define GetRGB(c) uchar R = R24(c); uchar G = G24(c); uchar B = B24(c);

bool GdcApp24::SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
{
	if (d && d->Cs == System24BitColourSpace)
	{
		Dest = d;
		Pal = p;
		Ptr = (System24BitPixel*) d->Base;
		Alpha = 0;
		return true;
	}
	return false;
}

void GdcApp24::SetPtr(int x, int y)
{
	LgiAssert(Dest && Dest->Base);
	Ptr = (System24BitPixel*) ((uint8*)Dest->Base + (y * Dest->Line) + (x * 3));
}

void GdcApp24::IncX()
{
	((char*&)Ptr) += 3;
}

void GdcApp24::IncY()
{
	((char*&)Ptr) += Dest->Line;
}

void GdcApp24::IncPtr(int X, int Y)
{
	((char*&)Ptr) += (Y * Dest->Line) + (X * 3);
}

COLOUR GdcApp24::Get()
{
	return Rgb24(Ptr->r, Ptr->g, Ptr->b);
}

// 24 bit set sub functions
void GdcApp24Set::Set()
{
	GetRGB(c);
	Ptr->r = R;
	Ptr->g = G;
	Ptr->b = B;
}

void GdcApp24Set::VLine(int height)
{
	GetRGB(c);
	while (height--)
	{
		Ptr->r = R;
		Ptr->g = G;
		Ptr->b = B;
		((char*&)Ptr) += Dest->Line;
	}
}

void GdcApp24Set::Rectangle(int x, int y)
{
	GetRGB(c);
	while (y--)
	{
		System24BitPixel *p = Ptr;
		System24BitPixel *e = Ptr + x;
		while (p < e)
		{
			p->r = R;
			p->g = G;
			p->b = B;
			p++;
		}
		((char*&)Ptr) += Dest->Line;
	}
}

bool GdcApp24Set::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				if (SPal)
				{
					System24BitPixel c[256];

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
							c[i].r = 0;
							c[i].g = 0;
							c[i].b = 0;
						}
					}

					for (int y=0; y<Src->y; y++)
					{
						System24BitPixel *d = Ptr;
						uchar *s = Src->Base + (y * Src->Line);
						uchar *e = s + Src->x;

						while (s < e)
						{
							d->r = c[*s].r;
							d->g = c[*s].g;
							d->b = c[*s].b;
							s++;
							d++;
						}

						((uint8*&)Ptr) += Dest->Line;
					}
				}
				else
				{
					for (int y=0; y<Src->y; y++)
					{
						System24BitPixel *d = Ptr;
						uchar *s = Src->Base + (y * Src->Line);
						uchar *e = s + Src->x;

						while (s < e)
						{
							d->r = *s;
							d->g = *s;
							d->b = *s++;
							d++;
						}

						((uint8*&)Ptr) += Dest->Line;
					}
				}
				break;
			}
			case System15BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (Src->Line * y));
					ushort *e = s + Src->x;
					System24BitPixel *d = Ptr;

					while (s < e)
					{
						d->r = Rc15(*s);
						d->g = Gc15(*s);
						d->b = Bc15(*s);

						s++;
						d++;
					}

					((uint8*&)Ptr) += Dest->Line;
				}
				break;
			}
			case System16BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (Src->Line * y));
					ushort *e = s + Src->x;
					System24BitPixel *d = Ptr;

					while (s < e)
					{
						d->b = ((*s & 0x001F) << 3) | ((*s & 0x001C) >> 2);
						d->g = ((*s & 0x07E0) >> 3) | ((*s & 0x0600) >> 9);
						d->r = ((*s & 0xF800) >> 8) | ((*s & 0xE000) >> 13);

						s++;
						d++;
					}

					((uint8*&)Ptr) += Dest->Line;
				}
				break;
			}
			case System24BitColourSpace:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemCpy(Ptr, s, Src->x * 3);
					s += Src->Line;
					((uint8*&)Ptr) += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System24BitPixel *d = (System24BitPixel*) Ptr;
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					System32BitPixel *e = s + Src->x;

					while (s < e)
					{
						d->r = s->r;
						d->g = s->g;
						d->b = s->b;
						s++;
						d++;
					}

					((uint8*&)Ptr) += Dest->Line;
				}
				break;
			}
			case CsBgr48:
			{
				for (int y=0; y<Src->y; y++)
				{
					System24BitPixel *d = (System24BitPixel*) Ptr;
					GBgr48 *s = (GBgr48*) (Src->Base + (y * Src->Line));
					GBgr48 *e = s + Src->x;

					while (s < e)
					{
						d->r = s->r >> 8;
						d->g = s->g >> 8;
						d->b = s->b >> 8;
						s++;
						d++;
					}

					((uint8*&)Ptr) += Dest->Line;
				}
				break;
			}
		}
	}
	return true;
}

// 24 bit or sub functions
void GdcApp24Or::Set()
{
	GetRGB(c);
	Ptr->b |= B;
	Ptr->g |= G;
	Ptr->r |= R;
}

void GdcApp24Or::VLine(int height)
{
	GetRGB(c);
	while (height--)
	{
		Ptr->b |= B;
		Ptr->g |= G;
		Ptr->r |= R;
		((char*&)Ptr) += Dest->Line;
	}
}

void GdcApp24Or::Rectangle(int x, int y)
{
	GetRGB(c);
	while (y--)
	{
		for (int n=0; n<x; n++)
		{
			Ptr->b |= B;
			Ptr->g |= G;
			Ptr->r |= R;
			Ptr++;
		}
		BytePtr += Dest->Line - (x * 3);
	}
}

bool GdcApp24Or::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Cs)
		{
			default:
				break;
			case System24BitColourSpace:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemOr(Ptr, s, Src->x * 3);
					s += Src->Line;
					BytePtr += Dest->Line;
				}
				break;
			}
		}
	}
	return true;
}

// 24 bit AND sub functions
void GdcApp24And::Set()
{
	GetRGB(c);
	Ptr->b &= B;
	Ptr->g &= G;
	Ptr->r &= R;
}

void GdcApp24And::VLine(int height)
{
	GetRGB(c);
	while (height--)
	{
		Ptr->b &= B;
		Ptr->g &= G;
		Ptr->r &= R;
		BytePtr += Dest->Line;
	}
}

void GdcApp24And::Rectangle(int x, int y)
{
	GetRGB(c);
	while (y--)
	{
		for (int n=0; n<x; n++)
		{
			Ptr->b &= B;
			Ptr->g &= G;
			Ptr->b &= R;
			Ptr++;
		}
		BytePtr += Dest->Line - (x * 3);
	}
}

bool GdcApp24And::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemAnd(Ptr, s, Src->x * 3);
			s += Src->Line;
			BytePtr += Dest->Line;
		}
	}
	return true;
}

// 24 bit XOR sub functions
void GdcApp24Xor::Set()
{
	GetRGB(c);
	Ptr->b ^= B;
	Ptr->g ^= G;
	Ptr->r ^= R;
}

void GdcApp24Xor::VLine(int height)
{
	GetRGB(c);
	while (height--)
	{
		Ptr->b ^= B;
		Ptr->g ^= G;
		Ptr->r ^= R;
		BytePtr += Dest->Line;
	}
}

void GdcApp24Xor::Rectangle(int x, int y)
{
	GetRGB(c);
	while (y--)
	{
		for (int n=0; n<x; n++)
		{
			Ptr->b ^= B;
			Ptr->g ^= G;
			Ptr->r ^= R;
			Ptr++;
		}
		BytePtr += Dest->Line - (x * 3);
	}
}

bool GdcApp24Xor::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemXor(Ptr, s, Src->x * 3);
			s += Src->Line;
			BytePtr += Dest->Line;
		}
	}
	return true;
}
