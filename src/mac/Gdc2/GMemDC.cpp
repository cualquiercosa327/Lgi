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
class CGImgPriv
{
public:
	CGImageRef Img;
	CGColorSpaceRef Cs;
	CGDataProviderRef Prov;
	
	CGImgPriv()
	{
		Img = 0;
		Cs = 0;
		Prov = 0;
	}
	
	~CGImgPriv()
	{
		if (Img)
		{
			CGImageRelease(Img);
			Img = 0;
		}
		
		if (Cs)
		{
			CGColorSpaceRelease(Cs);
			Cs = 0;	
		}
		
		if (Prov)
		{
			CGDataProviderRelease(Prov);
			Prov = 0;
		}
	}
};

void ReleaseCGImg(void *info, const void *data, size_t size)
{
}

CGImg::CGImg(GSurface *pDC)
{
	d = new CGImgPriv;
	if (pDC)
	{
		uchar *a = (*pDC)[0];
		uchar *b = (*pDC)[1];
		if (a && b)
			Create(pDC->X(), pDC->Y(), pDC->GetBits(), b - a, a, 0, 0);
	}
}

CGImg::CGImg(int x, int y, int Bits, int Line, uchar *data, uchar *palette, GRect *r)
{
	d = new CGImgPriv;
	Create(x, y, Bits, Line, data, palette, r);
}

void CGImg::Create(int x, int y, int Bits, int Line, uchar *data, uchar *palette, GRect *r)
{
	GRect All(0, 0, x-1, y-1);
	GRect B;
	if (r)
	{
		B = *r;
		B.Bound(&All);
	}
	else
	{
		B = All;
	}
	
	int Bytes = Bits / 8;
	uchar *Base = data + (Line * B.y1) + (Bytes * B.x1);

	if (Bits <= 8)
	{
		int Entries = 1 << Bits;
		CGColorSpaceRef Rgb = CGColorSpaceCreateDeviceRGB();
		if (Rgb)
		{
			d->Cs = CGColorSpaceCreateIndexed(Rgb, Entries - 1, palette);
			CGColorSpaceRelease(Rgb);
		}
	}
	else
	{
		d->Cs = CGColorSpaceCreateWithName(kCGColorSpaceUserRGB);
	}
	
	if (d->Cs)
	{
		d->Prov = CGDataProviderCreateWithData(d, Base, Line * y, ReleaseCGImg);
		if (d->Prov)
		{
			d->Img = CGImageCreate
			(
				B.X(),
				B.Y(),
				Bits == 16 ? 5 : 8,
				Bits,
				Line,
				d->Cs,
				Bits == 32 ? AlphaType : kCGImageAlphaNone,
				d->Prov,
				0,
				false,
				kCGRenderingIntentDefault
			);
			if (!d->Img)
			{
				printf("%s:%i - CGImageCreate failed.\n", __FILE__, __LINE__);
			}
		}
		else
		{
			printf("%s:%i - CGDataProviderCreateWithData failed.\n", __FILE__, __LINE__);
		}
	}
	else
	{
		printf("%s:%i - ColourSpace creation failed.\n", __FILE__, __LINE__);
	}
}

CGImg::~CGImg()
{
	DeleteObj(d);
}

CGImg::operator CGImageRef()
{
	return d->Img;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	uchar *Data;
	CGContextRef Bmp;
	CGColorSpaceRef Cs;
	GRect Client;

	GMemDCPrivate()
	{
		Cs = 0;
		Data = 0;
		Bmp = 0;
	}
	
	~GMemDCPrivate()
	{
		Empty();
	}
	
	void Empty()
	{
		if (Bmp)
		{
			CGContextRelease(Bmp);
			Bmp = 0;
		}
		
		if (Cs)
		{
			CGColorSpaceRelease(Cs);
			Cs = 0;
		}
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

CGImg *GMemDC::GetImg(GRect *Sub)
{
	if (pMem)
	{
		uchar *rgb = pPalette ? (uchar*)((*pPalette)[0]) : 0;
		
		return new CGImg
					(
						pMem->x,
						pMem->y,
						pMem->Bits,
						pMem->Line,
						d->Data,
						rgb,
						Sub
					);
	}
}

OsBitmap GMemDC::GetBitmap()
{
	return 0;
}

OsPainter GMemDC::Handle()
{
	return d->Bmp;
}

bool GMemDC::Lock()
{
	return true;
}

bool GMemDC::Unlock()
{
	return true;
}

void ReleaseData(void *info, const void *data, size_t size)
{
	GMemDCPrivate *d = (GMemDCPrivate*)info;
	DeleteArray(d->Data);
}

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
		
		d->Data = new uchar[LineLen * y];
		if (d->Data)
		{
			if (Bits <= 8)
			{
				d->Cs = CGColorSpaceCreateDeviceGray();
			}
			else
			{
				d->Cs = CGColorSpaceCreateWithName(kCGColorSpaceUserRGB);
			}

			d->Bmp = CGBitmapContextCreate
				(
					d->Data,
					x,
					y,
					Bits == 16 ? 5 : 8,
					LineLen,
					d->Cs,
					Bits == 32 ? AlphaType : kCGImageAlphaNone
				);
			if (d->Bmp)
			{
				//CGContextTranslateCTM (d->Bmp, 0, y); 
				//CGContextScaleCTM (d->Bmp, 1.0, -1.0); 

				pMem = new GBmpMem;
				if (pMem)
				{
					pMem->Base = d->Data; ((uchar*)d->Data) + (LineLen * (y - 1));
					pMem->x = x;
					pMem->y = y;
					pMem->Line = LineLen;
					pMem->Bits = Bits;
					pMem->Flags = GDC_ON_SCREEN;
					
					int NewOp = (pApp) ? Op() : GDC_SET;

					if ((Flags & GDC_OWN_APPLICATOR) AND !(Flags & GDC_CACHED_APPLICATOR))
					{
						DeleteObj(pApp);
					}

					for (int i=0; i<GDC_CACHE_SIZE; i++)
					{
						DeleteObj(pAppCache[i]);
					}

					if (NewOp < GDC_CACHE_SIZE AND !DrawOnAlpha())
					{
						pApp = (pAppCache[NewOp]) ? pAppCache[NewOp] : pAppCache[NewOp] = CreateApplicator(NewOp);
						Flags &= ~GDC_OWN_APPLICATOR;
						Flags |= GDC_CACHED_APPLICATOR;
					}
					else
					{
						pApp = CreateApplicator(NewOp);
						Flags &= ~GDC_CACHED_APPLICATOR;
						Flags |= GDC_OWN_APPLICATOR;
					}

					Clip.ZOff(X()-1, Y()-1);
					
					Status = true;
				}
			}
			else
			{
				printf("%s:%i - CGBitmapContextCreate(%i,%i,%i[,%i]) failed, Cs=%p.\n", __FILE__, __LINE__, x, y, Bits, LineLen, d->Cs);
			}
		}
		else
		{
			printf("%s:%i - Failed to allocate bitmap data.\n", __FILE__, __LINE__);
		}
	}
	
	return Status;
}

CGColorSpaceRef GMemDC::GetColourSpace()
{
	return d->Cs;
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
		d->Client = *c;
		OriginX = -c->x1;
		OriginY = -c->y1;
		Clip.ZOff(c->X()-1, c->Y()-1);
	}
	else
	{
		d->Client.ZOff(-1, -1);
		OriginX = 0;
		OriginY = 0;
		Clip.ZOff(pMem->x-1, pMem->y-1);
	}
}
