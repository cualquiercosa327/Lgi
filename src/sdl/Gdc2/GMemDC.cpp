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
#include "GPalette.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	void		*pBits;
	bool		UpsideDown;
	GRect		Client;

	GMemDCPrivate()
	{
		pBits = 0;
		Client.ZOff(-1, -1);
		UpsideDown = false;
	}
	
	~GMemDCPrivate()
	{
	}
};

GMemDC::GMemDC(int x, int y, GColourSpace cs)
{
	d = new GMemDCPrivate;
	ColourSpace = CsNone;
	pMem = 0;

	if (x > 0 && y > 0)
	{
		Create(x, y, cs);
	}
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	ColourSpace = CsNone;
	pMem = 0;

	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetBits()) )
	{
		if (pDC->Palette())
		{
			Palette(new GPalette(pDC->Palette()));
		}

		Blt(0, 0, pDC);

		if (pDC->AlphaDC() &&
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

OsBitmap GMemDC::GetBitmap()
{
	return NULL;
}

OsPainter GMemDC::Handle()
{
	return NULL;
}

void GMemDC::SetClient(GRect *c)
{
	if (c)
	{
		/*
		if (hDC)
		{
			SetWindowOrgEx(hDC, 0, 0, NULL);
			SelectClipRgn(hDC, 0);
			
			HRGN hRgn = CreateRectRgn(c->x1, c->y1, c->x2+1, c->y2+1);
			if (hRgn)
			{
				SelectClipRgn(hDC, hRgn);
				DeleteObject(hRgn);
			}

			SetWindowOrgEx(hDC, -c->x1, -c->y1, NULL);
		}
		*/

		GRect Doc(0, 0, pMem->x-1, pMem->y-1);
		Clip = d->Client = *c;
		Clip.Bound(&Doc);
		
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);

		/*
		if (hDC)
		{
			SetWindowOrgEx(hDC, 0, 0, NULL);
			SelectClipRgn(hDC, 0);
		}
		*/
		OriginX = 0;
		OriginY = 0;
		Clip.ZOff(pMem->x-1, pMem->y-1);
	}
}

bool GMemDC::Lock()
{
	return true;
}

bool GMemDC::Unlock()
{
	return true;
}

GRect GMemDC::ClipRgn(GRect *Rgn)
{
	GRect Prev = Clip;

	if (Rgn)
	{
		GdcPt2 Origin(0, 0);
		/*
		if (hDC)
		    GetWindowOrgEx(hDC, &Origin);
		else
		*/
		{
			Origin.x = OriginX;
			Origin.y = OriginY;
		}

		Clip.x1 = max(Rgn->x1 - Origin.x, 0);
		Clip.y1 = max(Rgn->y1 - Origin.y, 0);
		Clip.x2 = min(Rgn->x2 - Origin.x, pMem->x-1);
		Clip.y2 = min(Rgn->y2 - Origin.y, pMem->y-1);

		/*
		HRGN hRgn = CreateRectRgn(Clip.x1, Clip.y1, Clip.x2+1, Clip.y2+1);
		if (hRgn)
		{
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}
		*/
	}
	else
	{
		Clip.x1 = 0;
		Clip.y1 = 0;
		Clip.x2 = X()-1;
		Clip.y2 = Y()-1;

		// SelectClipRgn(hDC, NULL);
	}

	return Prev;
}

bool GMemDC::SupportsAlphaCompositing()
{
	return true;
}

bool GMemDC::Create(int x, int y, int Bits, int LineLen, bool KeepData)
{
	GColourSpace cs = GdcD->GetColourSpace();
	switch (Bits)
	{
		case 8:
			cs = CsIndex8;
			break;
		case 15:
			cs = System15BitColourSpace;
			break;
		case 16:
			cs = System16BitColourSpace;
			break;
		case 24:
			cs = System24BitColourSpace;
			break;
		case 32:
			cs = System32BitColourSpace;
			break;
		case 48:
			cs = CsRgb48;
			break;
		case 64:
			cs = CsRgba64;
			break;
		default:
			LgiAssert(0);
			break;
	}

	return Create(x, y, cs, LineLen, KeepData);
}

bool GMemDC::Create(int x, int y, GColourSpace Cs, int LineLen, bool KeepData)
{
	bool Status = false;
	GBmpMem *pOldMem = pMem;

	DrawOnAlpha(false);
	DeleteObj(pAlphaDC);

	pMem = NULL;

	if (x > 0 && y > 0)
	{
		int Bits = GColourSpaceToBits(Cs);
		int Colours = Bits <= 8 ? 1 << Bits : 0;

		{
			// Non-native colour space support...
			ColourSpace = Cs;
			if (ColourSpace)
			{
				// Non-native image data
				pMem = new GBmpMem;
				if (pMem)
				{
					pMem->x = x;
					pMem->y = y;
					pMem->Line = ((x * Bits + 31) / 32) << 2;
					pMem->Cs = ColourSpace;
					pMem->Flags = GDC_OWN_MEMORY;
					pMem->Base = new uchar[pMem->y * pMem->Line];
					
					Status = pMem->Base != NULL;
				}
			}
		}

		if (Status)
		{
			int NewOp = (pApp) ? Op() : GDC_SET;

			if ( (Flags & GDC_OWN_APPLICATOR) &&
				!(Flags & GDC_CACHED_APPLICATOR))
			{
				DeleteObj(pApp);
			}

			for (int i=0; i<GDC_CACHE_SIZE; i++)
			{
				DeleteObj(pAppCache[i]);
			}

			if (NewOp < GDC_CACHE_SIZE && !DrawOnAlpha())
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

			if (KeepData)
			{
				GApplicator *MyApp = CreateApplicator(GDC_SET);
				if (MyApp)
				{
					GBmpMem Temp = *pOldMem;
					Temp.x = min(pMem->x, pOldMem->x);
					Temp.y = min(pMem->y, pOldMem->y);

					MyApp->SetSurface(pMem);
					MyApp->c = 0;
					MyApp->SetPtr(0, 0);
					MyApp->Rectangle(pMem->x, pMem->y);
					// MyApp->Blt(&Temp, pPalette, pPalette);

					DeleteObj(MyApp);
				}
				else
				{
					Status = false;
				}
			}
		}

		if (!Status)
		{
			DeleteObj(pMem)
		}
	}

	DeleteObj(pOldMem);

	return Status;
}


void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
    LgiAssert(Src);
    if (!Src)
        return;

	if (Src->IsScreen())
	{
		GRect b;
		GRect Bounds(0, 0, GdcD->X()-1, GdcD->Y()-1);
		if (a)
		{
			b = *a;
			b.Bound(&Bounds);
		}
		else
		{
			b = Bounds;
		}

	}
	else
	{
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
	if (	x1 <= x2 &&
		y >= Clip.y1 &&
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
	if (	y1 <= y2 &&
		x >= Clip.x1 &&
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
