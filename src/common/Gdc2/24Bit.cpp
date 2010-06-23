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

/// 24 bit rgb applicators
class LgiClass GdcApp24 : public GApplicator
{
protected:
	Pixel24 *Ptr;

public:
	GdcApp24()
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

class LgiClass GdcApp24Set : public GdcApp24 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp24And : public GdcApp24 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp24Or : public GdcApp24 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp24Xor : public GdcApp24 {
public:
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GApp24::Create(int Bits, int Op)
{
	if (Bits == 24)
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
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////
#define GetRGB(c) uchar R = R24(c); uchar G = G24(c); uchar B = B24(c);

bool GdcApp24::SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
{
	if (d AND d->Bits == 24)
	{
		Dest = d;
		Pal = p;
		Ptr = (Pixel24*) d->Base;
		Alpha = 0;
		return true;
	}
	return false;
}

void GdcApp24::SetPtr(int x, int y)
{
	LgiAssert(Dest && Dest->Base);
	Ptr = (Pixel24*) ((char*)Dest->Base + (y * Dest->Line) + (x * Pixel24::Size));
}

void GdcApp24::IncX()
{
	((char*&)Ptr) += Pixel24::Size;
}

void GdcApp24::IncY()
{
	((char*&)Ptr) += Dest->Line;
}

void GdcApp24::IncPtr(int X, int Y)
{
	((char*&)Ptr) += (Y * Dest->Line) + (X * Pixel24::Size);
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
		Pixel24 *p = Ptr;
		Pixel24 *e = (Pixel24*) (((char*)Ptr) + (x * Pixel24::Size));
		while (p < e)
		{
			p->r = R;
			p->g = G;
			p->b = B;
			p = p->Next();
		}
		((char*&)Ptr) += Dest->Line;
	}
}

bool GdcApp24Set::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Bits)
		{
			case 8:
			{
				if (SPal)
				{
					Pixel24 c[256];

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
							c[i].r = 0;
							c[i].g = 0;
							c[i].b = 0;
						}
					}

					for (int y=0; y<Src->y; y++)
					{
						Pixel24 *d = Ptr;
						uchar *s = Src->Base + (y * Src->Line);
						uchar *e = s + Src->x;

						while (s < e)
						{
							d->r = c[*s].r;
							d->g = c[*s].g;
							d->b = c[*s].b;
							s++;
							d = d->Next();
						}

						((char*&)Ptr) += Dest->Line;
					}
				}
				else
				{
					for (int y=0; y<Src->y; y++)
					{
						Pixel24 *d = Ptr;
						uchar *s = Src->Base + (y * Src->Line);
						uchar *e = s + Src->x;

						while (s < e)
						{
							d->r = *s;
							d->g = *s;
							d->b = *s++;
							d = d->Next();
						}

						((char*&)Ptr) += Dest->Line;
					}
				}
				break;
			}
			case 15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (Src->Line * y));
					ushort *e = s + Src->x;
					Pixel24 *d = Ptr;

					while (s < e)
					{
						d->r = Rc15(*s);
						d->g = Gc15(*s);
						d->b = Bc15(*s);

						s++;
						d = d->Next();
					}

					((char*&)Ptr) += Dest->Line;
				}
				break;
			}
			case 16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (Src->Line * y));
					ushort *e = s + Src->x;
					Pixel24 *d = Ptr;

					while (s < e)
					{
						d->b = ((*s & 0x001F) << 3) | ((*s & 0x001C) >> 2);
						d->g = ((*s & 0x07E0) >> 3) | ((*s & 0x0600) >> 9);
						d->r = ((*s & 0xF800) >> 8) | ((*s & 0xE000) >> 13);

						s++;
						d = d->Next();
					}

					((char*&)Ptr) += Dest->Line;
				}
				break;
			}
			case 24:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemCpy(Ptr, s, Src->x * Pixel24::Size);
					s += Src->Line;
					((char*&)Ptr) += Dest->Line;
				}
				break;
			}
			case 32:
			{
				for (int y=0; y<Src->y; y++)
				{
					Pixel24 *d = (Pixel24*) Ptr;
					Pixel32 *s = (Pixel32*) (Src->Base + (y * Src->Line));
					Pixel32 *e = s + Src->x;

					while (s < e)
					{
						d->r = s->r;
						d->g = s->g;
						d->b = s->b;
						s++;
						d = d->Next();
					}

					((char*&)Ptr) += Dest->Line;
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
			Ptr = Ptr->Next();
		}
		((char*&)Ptr) += Dest->Line - (x * Pixel24::Size);
	}
}

bool GdcApp24Or::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
	{
		switch (Src->Bits)
		{
			case 24:
			{
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemOr(Ptr, s, Src->x * Pixel24::Size);
					s += Src->Line;
					((char*&)Ptr) += Dest->Line;
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
		((char*&)Ptr) += Dest->Line;
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
			Ptr = Ptr->Next();
		}
		((char*&)Ptr) += Dest->Line - (x * Pixel24::Size);
	}
}

bool GdcApp24And::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src AND Src->Bits == Dest->Bits)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemAnd(Ptr, s, Src->x * Pixel24::Size);
			s += Src->Line;
			((char*&)Ptr) += Dest->Line;
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
		((char*&)Ptr) += Dest->Line;
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
			Ptr = Ptr->Next();
		}
		((char*&)Ptr) += Dest->Line - (x * Pixel24::Size);
	}
}

bool GdcApp24Xor::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src AND Src->Bits == Dest->Bits)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemXor(Ptr, s, Src->x * Pixel24::Size);
			s += Src->Line;
			((char*&)Ptr) += Dest->Line;
		}
	}
	return true;
}
