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
	};

public:
	GdcApp15()
	{
		u8 = NULL;
	}

	const char *GetClass() { return "GdcApp15"; }

	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
	{
		if (d && d->Cs == Cs)
		{
			Dest = d;
			Pal = p;
			u8 = d->Base;
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
		u8 = Dest->Base + ((y * Dest->Line) + x + x);
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
		p->r = R15(c);
		p->g = G15(c);
		p->b = B15(c);
	}
	
	COLOUR Get()
	{
		return *u16;
	}
};

template<typename Pixel, GColourSpace Cs>
class GdcApp15Set : public GdcApp15<Pixel, Cs>
{
public:
	const char *GetClass() { return "GdcApp15Set"; }

	#define InitSet15() \
		REG union { \
			Pixel px; \
			uint16 upx; \
		}; \
		REG union { \
			uint8 *d8; \
			uint16 *d16; \
		}; \
		px.r = R15(this->c); \
		px.g = G15(this->c); \
		px.b = B15(this->c); \
		REG ssize_t line = this->Dest->Line; \
		d8 = this->u8;

	void VLine(int height)
	{
		InitSet15();
		while (height--)
		{
			*d16 = upx;
			d8 += line;
		}
		this->u8 = d8;
	}

	void Rectangle(int x, int y)
	{
		InitSet15();
		while (y--)
		{
			REG uint16 *d = d16;
			REG uint16 *e = d + x;
			while (d < e)
			{
				*d++ = upx;
			}
			
			d8 += line;
		}
		this->u8 = d8;
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
				ZeroObj(map);
				for (int i=0; i<256; i++)
				{
					GdcRGB *p = SPal ? (*SPal)[i] : NULL;
					if (p)
					{
						map[i].r = G8bitTo5bit(p->r);
						map[i].g = G8bitTo5bit(p->g);
						map[i].b = G8bitTo5bit(p->b);
					}
					else
					{
						map[i].r = G8bitTo5bit(i);
						map[i].g = G8bitTo5bit(i);
						map[i].b = G8bitTo5bit(i);
					}
				}

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = ((uchar*)Src->Base) + (y * Src->Line);
					Pixel *d = this->p;

					for (int x=0; x<Src->x; x++)
					{
						*d++ = map[*s++];
					}

					this->u8 += this->Dest->Line;
				}
				break;
			}
			default:
			{
				GBmpMem Dst;
				Dst.Base = this->u8;
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

GApplicator *GApp15::Create(GColourSpace Cs, int Op)
{
	if (Op == GDC_SET)
	{
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
	}
	
	return NULL;
}

