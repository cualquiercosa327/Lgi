/** \file
	\author Matthew Allen
	\date 20/3/1997
	\brief 16 bit primitives
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"

/// 16 bit rgb applicators
class LgiClass GdcApp16 : public GApplicator
{
protected:
	uchar *Ptr;

public:
	GdcApp16()
	{
		Ptr = 0;	
	}
	
	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a);
	void SetPtr(int x, int y);
	void IncX();
	void IncY();
	void IncPtr(int X, int Y);
	COLOUR Get();
};

class LgiClass GdcApp16Set : public GdcApp16
{
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp16And : public GdcApp16
{
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp16Or : public GdcApp16
{
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp16Xor : public GdcApp16
{
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GApp16::Create(int Bits, int Op)
{
	if (Bits == 16)
	{
		switch (Op)
		{
			case GDC_SET:
				return new GdcApp16Set;
			case GDC_AND:
				return new GdcApp16And;
			case GDC_OR:
				return new GdcApp16Or;
			case GDC_XOR:
				return new GdcApp16Xor;
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
#define sPtr			((ushort*)Ptr)

bool GdcApp16::SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
{
	if (d AND d->Bits == 16)
	{
		Dest = d;
		Pal = p;
		Ptr = d->Base;
		Alpha = 0;
		return true;
	}
	return false;
}

void GdcApp16::SetPtr(int x, int y)
{
	if (Dest AND Dest->Base)
	{
		Ptr = Dest->Base + ((y * Dest->Line) + x + x);
	}
	else
	{
		Ptr = 0;
		LgiAssert(0);
	}
}

void GdcApp16::IncX()
{
	Ptr += 2;
}

void GdcApp16::IncY()
{
	Ptr += Dest->Line;
}

void GdcApp16::IncPtr(int X, int Y)
{
	Ptr += (Y * Dest->Line) + X + X;
}

COLOUR GdcApp16::Get()
{
	return *sPtr;
}

// 16 bit set sub functions
void GdcApp16Set::Set()
{
	*sPtr = c;
}

void GdcApp16Set::VLine(int height)
{
	while (height--)
	{
		*sPtr = c;
		Ptr += Dest->Line;
	}
}

void GdcApp16Set::Rectangle(int x, int y)
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
		for (int n=0; n<x; n++) sPtr[n] = c;
		Ptr += Dest->Line;
	}
#endif
}

bool GdcApp16Set::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Bits)
		{
			case 8:
			{
				ushort c[256];
				GdcRGB *p;
				if (SPal AND (p = (*SPal)[0]))
				{
					for (int i=0; i<256 AND i<SPal->GetSize(); i++, p++)
					{
						c[i] = Rgb24To16(Rgb24(p->R, p->G, p->B));
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
					uchar *s = Src->Base + (y * Src->Line);
					ushort *d = (ushort*) Ptr;

					for (int x=0; x<Src->x; x++)
					{
						*d++ = c[*s++];
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case 15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						*d++ = Rgb15To16(*s);
						s++;
					}

					((char*&)Ptr) += Dest->Line;
				}
				break;
			}
			case 16:
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
			case 24:
			{
				for (int y=0; y<Src->y; y++)
				{
					GRgb24 *s = (GRgb24*) ((char*)Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						*d++ = Rgb16(s->r, s->g, s->b);
						s++;
					}

					((char*&)Ptr) += Dest->Line;
				}
				break;
			}
			case 32:
			{
				ulong *s = (ulong*) Src->Base;
				ushort *d = (ushort*) Ptr;

				for (int y=0; y<Src->y; y++)
				{
					ulong *NextS = (ulong*) (((uchar*) s) + Src->Line);
					ushort *NextD = (ushort*) (((uchar*) d) + Dest->Line);

					for (int x=0; x<Src->x; x++)
					{
						*d = Rgb32To16(*s);
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

// 16 bit or sub functions
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
	if (Src AND Src->Bits == Dest->Bits)
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
	if (Src)
	{
		if (Src->Bits == Dest->Bits)
		{
			uchar *s = Src->Base;
			for (int y=0; y<Src->y; y++)
			{
				MemAnd(Ptr, s, Src->x * 2);
				s += Src->Line;
				Ptr += Dest->Line;
			}
		}
		else if (Src->Bits == 8)
		{
			uchar *s = Src->Base;
			for (int y=0; y<Src->y; y++)
			{
				uchar *Dp = Ptr;
				uchar *End = s + Src->x;

				for (uchar *Sp = s; Sp<End; Sp++)
				{
					*Dp++ &= *Sp;
					*Dp++ &= *Sp;
				}

				s += Src->Line;
				Ptr += Dest->Line;
			}
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
	if (Src AND Src->Bits == Dest->Bits)
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
