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
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32And : public GdcApp32
{
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32Or : public GdcApp32
{
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32Xor : public GdcApp32
{
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

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

	uint32 *p = Ptr;
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
		uint32 *p = Ptr.u32;
		uint32 *e = p + x;
		while (p < e)
		{
			*p++ = c;
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
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
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
							c[i].r = p->R;
							c[i].g = p->G;
							c[i].b = p->B;
						}
						else
						{
							c[i].r = i;
							c[i].g = i;
							c[i].b = i;
						}
						c[i].a = 0xff;
					}

					/*
					if (SrcAlpha)
					{
						for (int y=0; y<Src->y; y++)
						{
							uchar *S = Src->Base + (y * Src->Line);
							uchar *A = SrcAlpha->Base + (y * SrcAlpha->Line);
							uint32 *D = (uint32*)Ptr;

							for (int x=0; x<Src->x; x++)
							{
								if (*A == 0)
								{
								}
								else
								{
									*D = Rgb32(
								}
							}

							Ptr += Dest->Line;
						}
					}
					else
					*/
					{
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
			case CsRgb15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (Src->Line * y));
					System32BitPixel *d = Ptr.p;
					System32BitPixel *e = d + Src->x;
					
					while (d < e)
					{
						d->r = Rc15(*s);
						d->g = Gc15(*s);
						d->b = Bc15(*s);
						s++;
						d++;
					}
					
					Ptr.u8 += Dest->Line;
				}
				break;
			}
			case CsRgb16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (Src->Line * y));
					System32BitPixel *d = Ptr.p;
					System32BitPixel *e = d + Src->x;
					
					while (d < e)
					{
						d->r = Rc16(*s);
						d->g = Gc16(*s);
						d->b = Bc16(*s);
						s++;
						d++;
					}
					
					Ptr.u8 += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) ((char*)Src->Base + (y * Src->Line));
					System32BitPixel *d = Ptr.p;
					System32BitPixel *e = d + Src->x;
					
					while (d < e)
					{
						d->r = s->r;
						d->g = s->g;
						d->b = s->b;
						d->a = 255;
						d++;
						s++;
					}

					Ptr.u8 += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemCpy(Ptr.p, s, Src->x << 2);
					s += Src->Line;
					Ptr.u8 += Dest->Line;
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
