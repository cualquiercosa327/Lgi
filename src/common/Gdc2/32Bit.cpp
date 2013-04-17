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

#define MyBits(b)	((b)==32)

/// 32 bit rgb applicators
class LgiClass GdcApp32 : public GApplicator {
protected:
	uint32 *Ptr;

public:
	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a);
	void SetPtr(int x, int y);
	void IncX();
	void IncY();
	void IncPtr(int X, int Y);
	COLOUR Get();
};

class LgiClass GdcApp32Set : public GdcApp32 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32And : public GdcApp32 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32Or : public GdcApp32 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp32Xor : public GdcApp32 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GApp32::Create(int Bits, int Op)
{
	if (MyBits(Bits))
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
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////
#define AddPtr(p, i)		p = (uint32*) (((char*)(p))+(i))

bool GdcApp32::SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
{
	if (d && MyBits(d->Bits))
	{
		Dest = d;
		Pal = p;
		Ptr = (uint32*) d->Base;
		Alpha = 0;
		return true;
	}
	return false;
}

void GdcApp32::SetPtr(int x, int y)
{
	LgiAssert(Dest && Dest->Base);
	Ptr = (uint32*) (Dest->Base + ((y * Dest->Line) + (x << 2)));
}

void GdcApp32::IncX()
{
	Ptr++;
}

void GdcApp32::IncY()
{
	AddPtr(Ptr, Dest->Line);
}

void GdcApp32::IncPtr(int X, int Y)
{
	AddPtr(Ptr, (Y * Dest->Line) + (X << 2));
}

COLOUR GdcApp32::Get()
{
	return *Ptr;
}

// 32 bit set sub functions
void GdcApp32Set::Set()
{
	*Ptr = c;
}

void GdcApp32Set::VLine(int height)
{
	while (height--)
	{
		*Ptr = c;
		AddPtr(Ptr, Dest->Line);
	}
}

void GdcApp32Set::Rectangle(int x, int y)
{
#if defined(GDC_USE_ASM) && defined(_MSC_VER)

	uint32 *p = Ptr;
	int Line = Dest->Line;
	COLOUR fill = c;

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
		uint32 *p = Ptr;
		uint32 *e = p + x;
		while (p < e)
		{
			*p++ = c;
		}
		AddPtr(Ptr, Dest->Line);
	}
#endif
}

bool GdcApp32Set::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Bits)
		{
			case 8:
			{
				if (SPal)
				{
					Pixel32 c[256];
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
							Pixel32 *d = (Pixel32*) Ptr;
							Pixel32 *e = d + Src->x;
							
							while (d < e)
							{
								*d++ = c[*s++];
							}

							((uchar*&)Ptr) += Dest->Line;
						}
					}
				}
				else
				{
					for (int y=0; y<Src->y; y++)
					{
						uchar *s = (uchar*) (Src->Base + (Src->Line * y));
						Pixel32 *d = (Pixel32*) Ptr;
						Pixel32 *e = d + Src->x;

						while (d < e)
						{
							d->r = *s;
							d->g = *s;
							d->b = *s;
							s++;
							d++;
						}

						((uchar*&)Ptr) += Dest->Line;
					}
				}
				break;
			}
			case 15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (Src->Line * y));
					Pixel32 *d = (Pixel32*) Ptr;
					Pixel32 *e = d + Src->x;
					
					while (d < e)
					{
						d->r = Rc15(*s);
						d->g = Gc15(*s);
						d->b = Bc15(*s);
						s++;
						d++;
					}
					
					((uchar*&)Ptr) += Dest->Line;
				}
				break;
			}
			case 16:
			{
				COLOUR c;
				ushort *s = (ushort*) Src->Base;

				for (int y=0; y<Src->y; y++)
				{
					uchar *NextS = ((uchar*) s) + Src->Line;
					uchar *NextD = ((uchar*) Ptr) + Dest->Line;
					
					for (int x=0; x<Src->x; x++)
					{
						c = *s++;
						*Ptr++ = Rgb16To32(c);
					}

					s = (ushort*) NextS;
					Ptr = (uint32*) NextD;
				}
				break;
			}
			case 24:
			{
				for (int y=0; y<Src->y; y++)
				{
					Pixel24 *s = (Pixel24*) ((char*)Src->Base + (y * Src->Line));
					Pixel32 *d = (Pixel32*) Ptr;
					Pixel32 *e = d + Src->x;
					
					while (d < e)
					{
						d->r = s->r;
						d->g = s->g;
						d->b = s->b;
						d->a = 255;
						d++;
						s = s->Next();
					}

					((char*&)Ptr) += Dest->Line;
				}
				break;
			}
			case 32:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemCpy(Ptr, s, Src->x << 2);
					s += Src->Line;
					AddPtr(Ptr, Dest->Line);
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
	*Ptr |= c;
}

void GdcApp32Or::VLine(int height)
{
	while (height--)
	{
		*Ptr |= c;
		AddPtr(Ptr, Dest->Line);
	}
}

void GdcApp32Or::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) *Ptr++ |= c;
		AddPtr(Ptr, Dest->Line - (x << 2));
	}
}

bool GdcApp32Or::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Bits)
		{
			case 32:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemOr(Ptr, s, Src->x << 2);
					s += Src->Line;
					AddPtr(Ptr, Dest->Line);
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
	*Ptr &= c;
}

void GdcApp32And::VLine(int height)
{
	while (height--)
	{
		*Ptr &= c;
		AddPtr(Ptr, Dest->Line);
	}
}

void GdcApp32And::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) *Ptr++ &= c;
		AddPtr(Ptr, Dest->Line - (x << 2));
	}
}

bool GdcApp32And::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src AND Src->Bits == Dest->Bits)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemAnd(Ptr, s, Src->x << 2);
			s += Src->Line;
			AddPtr(Ptr, Dest->Line);
		}
	}
	return true;
}

// 32 bit XOR sub functions
void GdcApp32Xor::Set()
{
	*Ptr ^= c;
}

void GdcApp32Xor::VLine(int height)
{
	while (height--)
	{
		*Ptr ^= c;
		AddPtr(Ptr, Dest->Line);
	}
}

void GdcApp32Xor::Rectangle(int x, int y)
{
	while (y--)
	{
		uint32 *p = (uint32*)Ptr;
		for (int n=0; n<x; n++)
			*p++ ^= c;
		AddPtr(Ptr, Dest->Line);
	}
}

bool GdcApp32Xor::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src AND Src->Bits == Dest->Bits)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemXor(Ptr, s, Src->x << 2);
			s += Src->Line;
			AddPtr(Ptr, Dest->Line);
		}
	}
	return true;
}
