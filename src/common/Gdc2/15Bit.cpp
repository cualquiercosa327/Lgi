/**
	\file
	\author Matthew Allen
	\date 30/11/1999
	\brief 15 bit primitives
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

/// 15 bit rgb applicators
template<typename Pixel, GColourSpace Cs>
class GdcApp15 : public GApplicator
{
protected:
	union
	{
		uint8 *u8;
		uint16 *u16;
		Pixel *p;
	}	Ptr;

public:
	GdcApp15()
	{
		Ptr.u8 = NULL;
	}

	const char *GetClass() { return "GdcApp15"; }

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
		else
		{
			LgiAssert(0);
		}
		
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
		Ptr.u8 += Y * Dest->Line;
	}
	
	COLOUR Get()
	{
		return *Ptr.u16;
	}
};

template<typename Pixel, GColourSpace Cs>
class GdcApp15Set : public GdcApp15<Pixel, Cs>
{
public:
	const char *GetClass() { return "GdcApp15Set"; }

	void Set()
	{
		*this->Ptr.u16 = this->c;
	}
	
	void VLine(int height)
	{
		while (height--)
		{
			*this->Ptr.u16 = this->c;
			this->Ptr.u8 += this->Dest->Line;
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
					shl eax, 15
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
				this->Ptr.u16[n] = this->c;
			
			this->Ptr.u8 += this->Dest->Line;
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
						c[i] = Rgb24To15(Rgb24(p->r, p->g, p->b));
					}
				}
				else
				{
					for (int i=0; i<256; i++)
					{
						c[i] = Rgb15(i, i, i);
					}
				}

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = ((uchar*)Src->Base) + (y * Src->Line);
					uint16 *d = this->Ptr.u16;

					for (int x=0; x<Src->x; x++)
					{
						*d++ = c[*s++];
					}

					this->Ptr.u8 += this->Dest->Line;
				}
				break;
			}
			default:
			{
				GBmpMem Dst;
				Dst.Base = this->Ptr.u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = this->Dest->Cs;
				Dst.Line = this->Dest->Line;				
				if (!LgiRopUniversal(&Dst, Src, false))
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
class GdcApp15And : public GdcApp15<Pixel, Cs> {
public:
	const char *GetClass() { return "GdcApp15And"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

template<typename Pixel, GColourSpace Cs>
class GdcApp15Or : public GdcApp15<Pixel, Cs> {
public:
	const char *GetClass() { return "GdcApp15Or"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

template<typename Pixel, GColourSpace Cs>
class GdcApp15Xor : public GdcApp15<Pixel, Cs> {
public:
	const char *GetClass() { return "GdcApp15Xor"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GApp15::Create(GColourSpace Cs, int Op)
{
	if (Op != GDC_SET)
		return NULL;

	switch (Cs)
	{
		case CsRgb15:
			return new GdcApp15Set<GRgb15, CsRgb15>;
		case CsBgr15:
			return new GdcApp15Set<GBgr15, CsBgr15>;
		case CsArgb15:
			return new GdcApp15Set<GArgb15, CsArgb15>;
		case CsAbgr15:
			return new GdcApp15Set<GAbgr15, CsAbgr15>;
		default:
			break;
	}
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////
// 15 bit or sub functions
/*
void GdcApp15Or::Set()
{
	*sPtr |= c;
}

void GdcApp15Or::VLine(int height)
{
	while (height--)
	{
		*sPtr |= c;
		Ptr += Dest->Line;
	}
}

void GdcApp15Or::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) sPtr[n] |= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp15Or::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
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

// 15 bit AND sub functions
void GdcApp15And::Set()
{
	*sPtr &= c;
}

void GdcApp15And::VLine(int height)
{
	while (height--)
	{
		*sPtr &= c;
		Ptr += Dest->Line;
	}
}

void GdcApp15And::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) sPtr[n] &= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp15And::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
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

// 15 bit XOR sub functions
void GdcApp15Xor::Set()
{
	*sPtr ^= c;
}

void GdcApp15Xor::VLine(int height)
{
	while (height--)
	{
		*sPtr ^= c;
		Ptr += Dest->Line;
	}
}

void GdcApp15Xor::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) sPtr[n] ^= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp15Xor::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
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