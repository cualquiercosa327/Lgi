/**
	\file
	\author Matthew Allen
	\date 30/11/1999
	\brief 16 bit primitives
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

/// 16 bit rgb applicators
template<typename Pixel, GColourSpace Cs>
class GdcApp16 : public GApplicator
{
protected:
	union
	{
		uint8 *u8;
		uint16 *u16;
		Pixel *p;
	}	Ptr;

public:
	GdcApp16()
	{
		Ptr.u8 = NULL;
	}

	const char *GetClass() { return "GdcApp16"; }

	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
	{
		if (d && d->Cs == Cs)
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

	void SetPtr(int x, int y)
	{
		LgiAssert(Dest && Dest->Base);
		Ptr.u8 = Dest->Base + ((y * Dest->Line) + x + x);
	}

	void IncX()
	{
		Ptr.p++;
	}
	
	void IncY()
	{
		Ptr.u8 += Dest->Line;
	}
	
	void IncPtr(int X, int Y)
	{
		Ptr.u16 += X;
		Ptr.u8 += Y;
	}
	
	COLOUR Get()
	{
		return *Ptr.u16;
	}
};

template<typename Pixel, GColourSpace Cs>
class GdcApp16Set : public GdcApp16<Pixel, Cs>
{
public:
	const char *GetClass() { return "GdcApp16Set"; }
	void Set()
	{
		*Ptr.u16 = c;
	}
	
	void VLine(int height)
	{
		while (height--)
		{
			*Ptr.u16 = c;
			Ptr.u8 += Dest->Line;
		}
	}

	void Rectangle(int x, int y)
	{
	#if defined(GDC_USE_ASM) && defined(_MSC_VER)

	// this duplicates the colour twice in eax to allow us to fill
	// two pixels at per write. this means we are using the whole
	// 32-bit bandwidth to the video card :)

		if (y > 0)
		{
			if (x > 1)
			{
				uchar *p = Ptr.u8;
				COLOUR fill = c | (c << 16);
				int Line = Dest->Line;

				_asm {
					mov esi, p
					mov eax, fill
					mov edx, Line
					mov bx, ax
					shl eax, 16
					mov ax, bx
				LoopY:	mov edi, esi
					add esi, edx
					mov ecx, x
					shr ecx, 1
				LoopX:	mov [edi], eax
					add edi, 4
					dec ecx
					jnz LoopX
					test x, 1
					jz Next
					mov [edi], ax
				Next:	dec y
					jnz LoopY
				}
			}
			else if (x == 1)
			{
				VLine(y);
			}
		}
	#else
		while (y--)
		{
			for (int n=0; n<x; n++)
				Ptr.u16[n] = c;
			
			Ptr.u8 += Dest->Line;
		}
	#endif
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
	{
		if (!Src)
			return false;

		switch (Src->Cs)
		{
			case CsIndex8:
			{
				ushort c[256];
				if (SPal)
				{
					GdcRGB *p = (*SPal)[0];
					for (int i=0; i<256; i++, p++)
					{
						c[i] = Rgb24To16(Rgb24(p->r, p->g, p->b));
					}
				}
				else
				{
					for (int i=0; i<256; i++)
					{
						c[i] = Rgb16(i, i, i);
					}
				}

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = ((uchar*)Src->Base) + (y * Src->Line);
					uint16 *d = Ptr.u16;

					for (int x=0; x<Src->x; x++)
					{
						*d++ = c[*s++];
					}

					Ptr.u8 += Dest->Line;
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

		return true;
	}
	
};

template<typename Pixel, GColourSpace Cs>
class GdcApp16And : public GdcApp16<Pixel, Cs> {
public:
	const char *GetClass() { return "GdcApp16And"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

template<typename Pixel, GColourSpace Cs>
class GdcApp16Or : public GdcApp16<Pixel, Cs> {
public:
	const char *GetClass() { return "GdcApp16Or"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

template<typename Pixel, GColourSpace Cs>
class GdcApp16Xor : public GdcApp16<Pixel, Cs> {
public:
	const char *GetClass() { return "GdcApp16Xor"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GApp16::Create(GColourSpace Cs, int Op)
{
	if (Cs == CsRgb16)
	{
		switch (Op)
		{
			case GDC_SET:
				return new GdcApp16Set<GRgb16, CsRgb16>;
			/*
			case GDC_AND:
				return new GdcApp16And<GRgb16, CsRgb16>;
			case GDC_OR:
				return new GdcApp16Or<GRgb16, CsRgb16>;
			case GDC_XOR:
				return new GdcApp16Xor<GRgb16, CsRgb16>;
			*/
		}
	}
	else if (Cs == CsBgr16)
	{
		switch (Op)
		{
			case GDC_SET:
				return new GdcApp16Set<GBgr16, CsBgr16>;
			/*
			case GDC_AND:
				return new GdcApp16And<GBgr16, CsBgr16>;
			case GDC_OR:
				return new GdcApp16Or<GBgr16, CsBgr16>;
			case GDC_XOR:
				return new GdcApp16Xor<GBgr16, CsBgr16>;
			*/
		}
	}
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////
// 16 bit or sub functions
/*
void GdcApp16Or::Set()
{
	*sPtr |= c;
}

void GdcApp16Or::VLine(int height)
{
	while (height--)
	{
		*sPtr |= c;
		Ptr += Dest->Line;
	}
}

void GdcApp16Or::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) sPtr[n] |= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp16Or::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemOr(Ptr, s, Src->x * 2);
			s += Src->Line;
			Ptr += Dest->Line;
		}
	}
	return true;
}

// 16 bit AND sub functions
void GdcApp16And::Set()
{
	*sPtr &= c;
}

void GdcApp16And::VLine(int height)
{
	while (height--)
	{
		*sPtr &= c;
		Ptr += Dest->Line;
	}
}

void GdcApp16And::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) sPtr[n] &= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp16And::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemAnd(Ptr, s, Src->x * 2);
			s += Src->Line;
			Ptr += Dest->Line;
		}
	}
	return true;
}

// 16 bit XOR sub functions
void GdcApp16Xor::Set()
{
	*sPtr ^= c;
}

void GdcApp16Xor::VLine(int height)
{
	while (height--)
	{
		*sPtr ^= c;
		Ptr += Dest->Line;
	}
}

void GdcApp16Xor::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) sPtr[n] ^= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp16Xor::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemXor(Ptr, s, Src->x * 2);
			s += Src->Line;
			Ptr += Dest->Line;
		}
	}
	return true;
}
*/