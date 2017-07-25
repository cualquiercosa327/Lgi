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
		Ptr.u8 += Y * Dest->Line;
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
		register union
		{
			Pixel Px;
			uint16 u;
		};
		Px.r = R16(this->c);
		Px.g = G16(this->c);
		Px.b = B16(this->c);
		
		*this->Ptr.u16 = u;
	}
	
	void VLine(int height)
	{
		register union
		{
			Pixel Px;
			uint16 u;
		};
		Px.r = R16(this->c);
		Px.g = G16(this->c);
		Px.b = B16(this->c);
		
		while (height--)
		{
			*this->Ptr.u16 = u;
			this->Ptr.u8 += this->Dest->Line;
		}
	}

	void Rectangle(int x, int y)
	{
		register union
		{
			Pixel Px;
			uint16 u;
		};
		Px.r = R16(this->c);
		Px.g = G16(this->c);
		Px.b = B16(this->c);

		while (y--)
		{
			uint16 *d = this->Ptr.u16;
			uint16 *e = d + x;
			
			while (d < e)
				*d++ = u;
			
			this->Ptr.u8 += this->Dest->Line;
		}
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
	{
		if (!Src)
			return false;

		switch (Src->Cs)
		{
			case CsIndex8:
			{
				Pixel map[256];
				if (SPal)
				{
					GdcRGB *p = (*SPal)[0];
					for (int i=0; i<256; i++, p++)
					{
						map[i].r = G8bitTo5bit(p->r);
						map[i].g = G8bitTo6bit(p->g);
						map[i].b = G8bitTo5bit(p->b);
					}
				}
				else
				{
					for (int i=0; i<256; i++)
					{
						map[i].r = G8bitTo5bit(i);
						map[i].g = G8bitTo6bit(i);
						map[i].b = G8bitTo5bit(i);
					}
				}

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = ((uchar*)Src->Base) + (y * Src->Line);
					Pixel *d = this->Ptr.p;

					for (int x=0; x<Src->x; x++)
					{
						*d++ = map[*s++];
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