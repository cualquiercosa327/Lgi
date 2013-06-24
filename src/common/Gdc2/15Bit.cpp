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

/// 15 bit rgb applicators
class LgiClass GdcApp15 : public GApplicator
{
protected:
	uchar *Ptr;

public:
	GdcApp15()
	{
		Ptr = 0;
	}

	const char *GetClass() { return "GdcApp15"; }
	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a);
	void SetPtr(int x, int y);
	void IncX();
	void IncY();
	void IncPtr(int X, int Y);
	COLOUR Get();
};

class LgiClass GdcApp15Set : public GdcApp15 {
public:
	const char *GetClass() { return "GdcApp15Set"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp15And : public GdcApp15 {
public:
	const char *GetClass() { return "GdcApp15And"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp15Or : public GdcApp15 {
public:
	const char *GetClass() { return "GdcApp15Or"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp15Xor : public GdcApp15 {
public:
	const char *GetClass() { return "GdcApp15Xor"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GApp15::Create(GColourSpace Cs, int Op)
{
	if (Cs == CsRgb15)
	{
		switch (Op)
		{
			case GDC_SET:
				return new GdcApp15Set;
			case GDC_AND:
				return new GdcApp15And;
			case GDC_OR:
				return new GdcApp15Or;
			case GDC_XOR:
				return new GdcApp15Xor;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////
#define sPtr			((ushort*)Ptr)

bool GdcApp15::SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
{
	if (d AND d->Cs == CsRgb15)
	{
		Dest = d;
		Pal = p;
		Ptr = d->Base;
		Alpha = 0;
		return true;
	}
	return false;
}

void GdcApp15::SetPtr(int x, int y)
{
	LgiAssert(Dest AND Dest->Base);
	Ptr = Dest->Base + ((y * Dest->Line) + x + x);
}

void GdcApp15::IncX()
{
	Ptr += 2;
}

void GdcApp15::IncY()
{
	Ptr += Dest->Line;
}

void GdcApp15::IncPtr(int X, int Y)
{
	Ptr += (Y * Dest->Line) + X + X;
}

COLOUR GdcApp15::Get()
{
	return *sPtr;
}

// 15 bit set sub functions
void GdcApp15Set::Set()
{
	*sPtr = c;
}

void GdcApp15Set::VLine(int height)
{
	while (height--)
	{
		*sPtr = c;
		Ptr += Dest->Line;
	}
}

void GdcApp15Set::Rectangle(int x, int y)
{
#if defined(GDC_USE_ASM) && defined(_MSC_VER)

// this duplicates the colour twice in eax to allow us to fill
// two pixels at per write. this means we are using the whole
// 32-bit bandwidth to the video card :)

	if (y > 0)
	{
		if (x > 1)
		{
			uchar *p = Ptr;
			COLOUR fill = c | (c << 15);
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
		for (int n=0; n<x; n++) sPtr[n] = c;
		Ptr += Dest->Line;
	}
#endif
}

bool GdcApp15Set::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		/*
		printf("GdcApp15Set::Blt Src(%p, %i,%i,%i) Dest(%i,%i,%i)\n",
				Src->Base, Src->x, Src->y, Src->Bits,
				Dest->x, Dest->y, Dest->Bits);
		*/
		
		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				ushort c[256];
				if (SPal)
				{
					GdcRGB *p = (*SPal)[0];
					for (int i=0; i<256; i++, p++)
					{
						c[i] = Rgb24To15(Rgb24(p->R, p->G, p->B));
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
					ushort *d = (ushort*) Ptr;

					for (int x=0; x<Src->x; x++)
					{
						*d++ = c[*s++];
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsRgb15:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemCpy(Ptr, s, Src->x * 2);
					s += Src->Line;
					Ptr += Dest->Line;
				}
				break;
			}
			case CsRgb16:
			{
				ushort *s = (ushort*) Src->Base;
				ushort *d = (ushort*) Ptr;

				for (int y=0; y<Src->y; y++)
				{
					ushort *NextS = (ushort*) (((uchar*) s) + Src->Line);
					ushort *NextD = (ushort*) (((uchar*) d) + Dest->Line);

					for (int x=0; x<Src->x; x++)
					{
						*d = Rgb16To15(*s);
						d++;
						s++;
					}

					s = NextS;
					d = NextD;
				}

				Ptr = (uchar*) d;
				break;
			}
			case CsBgr24:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						*d++ = Rgb15(s->r, s->g, s->b);
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsArgb32:
			{
				ulong *s = (ulong*) Src->Base;
				ushort *d = (ushort*) Ptr;

				for (int y=0; y<Src->y; y++)
				{
					ulong *NextS = (ulong*) (((uchar*) s) + Src->Line);
					ushort *NextD = (ushort*) (((uchar*) d) + Dest->Line);

					for (int x=0; x<Src->x; x++)
					{
						*d = Rgb32To15(*s);
						d++;
						s++;
					}

					s = NextS;
					d = NextD;
				}

				Ptr = (uchar*) d;
				break;
			}
		}
	}
	return true;
}

// 15 bit or sub functions
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
	if (Src AND Src->Cs == Dest->Cs)
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
	if (Src AND Src->Cs == Dest->Cs)
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
	if (Src AND Src->Cs == Dest->Cs)
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
