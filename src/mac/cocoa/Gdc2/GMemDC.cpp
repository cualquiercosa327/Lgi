/*hdr
**	FILE:			GMemDC.h
**	AUTHOR:			Matthew Allen
**	DATE:			27/11/2001
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GdiLeak.h"

#define AlphaType		kCGImageAlphaPremultipliedLast

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	uchar *Data;
	GRect Client;
	GAutoPtr<uchar, true> BitsMem;

	GMemDCPrivate()
	{
		Data = 0;
	}
	
	~GMemDCPrivate()
	{
		Empty();
	}
	
	void Empty()
	{
		BitsMem.Reset();
	}
};

GMemDC::GMemDC(int x, int y, int bits)
{
	d = new GMemDCPrivate;
	pMem = 0;

	if (x AND y AND bits)
	{
		Create(x, y, bits);
	}
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	pMem = 0;

	if (pDC AND
		Create(pDC->X(), pDC->Y(), pDC->GetBits()) )
	{
		if (pDC->Palette())
		{
			Palette(new GPalette(pDC->Palette()));
		}

		Blt(0, 0, pDC);

		if (pDC->AlphaDC() AND
			HasAlpha(true))
		{
			pAlphaDC->Blt(0, 0, pDC->AlphaDC());
		}
	}
}

GMemDC::~GMemDC()
{
	DeleteObj(pMem);
	DeleteObj(d);
}

GRect GMemDC::ClipRgn(GRect *Rgn)
{
	GRect Old = Clip;
	
	if (Rgn)
	{
		GRect Dc(0, 0, X()-1, Y()-1);
		
		Clip = *Rgn;
		Clip.Offset(-OriginX, -OriginY);
		Clip.Bound(&Dc);
	}
	else
	{
		Clip.ZOff(X()-1, Y()-1);
	}
	
	return Old;
}

OsBitmap GMemDC::GetBitmap()
{
	return 0;
}

OsPainter GMemDC::Handle()
{
	return NULL;
}

bool GMemDC::Lock()
{
	return true;
}

bool GMemDC::Unlock()
{
	return true;
}

/*
void ReleaseData(void *info, const void *data, size_t size)
{
	GMemDCPrivate *d = (GMemDCPrivate*)info;
	DeleteArray(d->Data);
}
*/

bool GMemDC::Create(int x, int y, int Bits, int LineLen, bool KeepData)
{
	bool Status = false;

	d->Empty();
	
	if (x > 0 AND
		y > 0 AND
		Bits > 0)
	{
		if (LineLen == 0)
		{
			int Align = 16 * 8;
			LineLen = ((Bits * x + (Align - 1)) / Align) * (Align / 8);
		}
		
		if (d->BitsMem.Reset(new uchar[LineLen * y]))
		{
			d->Data = d->BitsMem;
			LgiAssert(!"Fix me");
			if (Bits <= 8)
			{
				// d->Cs = CGColorSpaceCreateDeviceGray();
			}
			else
			{
				// d->Cs = CGColorSpaceCreateWithName(kCGColorSpaceUserRGB);
			}
		}
		else
		{
			printf("%s:%i - Failed to allocate bitmap data.\n", __FILE__, __LINE__);
		}
	}
	
	return Status;
}

void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
		return;

	if (Src->IsScreen())
	{
		if (Src)
		{
			GRect b;
			if (a)
			{
				b = *a;
			}
			else
			{
				b.ZOff(Src->X()-1, Src->Y()-1);
			}

		}
	}
	else
	{
		/*
		if (Src->GetBits() == 8)
		{
			GPalette *p = Src->Palette();
			printf("\tpal=%p[%i] ", p, p?p->GetSize():0);
			if (p)
			{
				for (int i=0; i<p->GetSize() AND i<16; i++)
				{
					GdcRGB *r = (*p)[i];
					if (r)
					{
						printf("%x,%x,%x ", r->R, r->G, r->B);
					}
				}
			}
			printf("\n");
		}
		*/
		
		GSurface::Blt(x, y, Src, a);
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
	if (Src)
	{
		GRect DestR;
		if (d)
		{
			DestR = *d;
		}
		else
		{
			DestR.ZOff(X()-1, Y()-1);
		}

		GRect SrcR;
		if (s)
		{
			SrcR = *s;
		}
		else
		{
			SrcR.ZOff(Src->X()-1, Src->Y()-1);
		}

	}
}

void GMemDC::HLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LgiSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (	x1 <= x2 AND
		y >= Clip.y1 AND
		y <= Clip.y2)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x1, y);
		for (; x1 <= x2; x1++)
		{
			if (x1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncX();
		}

		pApp->c = Prev;
	}
}

void GMemDC::VLine(int x, int y1, int y2, COLOUR a, COLOUR b)
{
	if (y1 > y2) LgiSwap(y1, y2);
	
	if (y1 < Clip.y1) y1 = Clip.y1;
	if (y2 > Clip.y2) y2 = Clip.y2;
	if (	y1 <= y2 AND
		x >= Clip.x1 AND
		x <= Clip.x2)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x, y1);
		for (; y1 <= y2; y1++)
		{
			if (y1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncY();
		}

		pApp->c = Prev;
	}
}

void GMemDC::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
}

void GMemDC::SetClient(GRect *c)
{
	if (c)
	{
		GRect Doc(0, 0, pMem->x-1, pMem->y-1);
		Clip = d->Client = *c;
		Clip.Bound(&Doc);
		
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		OriginX = 0;
		OriginY = 0;
		Clip.ZOff(pMem->x-1, pMem->y-1);
	}
}
