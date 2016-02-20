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
#include "GPalette.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	OsBitmap	Bmp;
	OsPainter	View;

	GMemDCPrivate()
	{
		Bmp = 0;
		View = 0;
	}
	
	~GMemDCPrivate()
	{
		DeleteObj(Bmp);
	}
};

GMemDC::GMemDC(int x, int y, GColourSpace cs)
{
	d = new GMemDCPrivate;
	if (x && y && cs)
	{
		Create(x, y, cs);
	}
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	if (pDC && Create(pDC->X(), pDC->Y(), pDC->GetBits()))
	{
		Blt(0, 0, pDC);
		if (pDC->Palette())
		{
			GPalette *Pal = new GPalette(pDC->Palette());
			if (Pal)
			{
				Palette(Pal, TRUE);
			}
		}
	}
}

GMemDC::~GMemDC()
{
	DeleteObj(d);
}

OsPainter GMemDC::Handle()
{
	return d->View;
}

OsBitmap GMemDC::GetBitmap()
{
	return d->Bmp;
}

void GMemDC::SetClient(GRect *c)
{
}

bool GMemDC::SupportsAlphaCompositing()
{
	return true;
}

void GMemDC::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
}

#include "Lgi.h"

GColourSpace BeosColourSpaceToLgi(color_space cs)
{
	switch (cs)
	{
		// linear color space (little endian)
		case B_RGB32: // BGR-		-RGB 8:8:8:8
			return CsBgrx32;
		case B_RGBA32: // BGRA		ARGB 8:8:8:8
			return CsBgra32;
		case B_RGB24: // BGR		 RGB 8:8:8
			return CsBgr24;
		case B_RGB16: // BGR		 RGB 5:6:5
			return CsBgr16;
		case B_RGB15: // BGR-		-RGB 1:5:5:5
			return CsBgr15;
		case B_RGBA15: // BGRA		ARGB 1:5:5:5
			return CsBgr15;
		case B_CMAP8: // 256 color index table
			return CsIndex8;
		case B_GRAY8: // 256 greyscale table
			return CsIndex8;
		// B_GRAY1				= 0x0001,	// Each bit represents a single pixel
	
		// linear color space (big endian)
		case B_RGB32_BIG: // -RGB		BGR- 8:8:8:8
			return CsXrgb32;
		case B_RGBA32_BIG: // ARGB		BGRA 8:8:8:8
			return CsArgb32;
		case B_RGB24_BIG: //  RGB		BGR  8:8:8
			return CsRgb24;
		case B_RGB16_BIG: //  RGB		BGR  5:6:5
			return CsRgb16;
		case B_RGB15_BIG: // -RGB		BGR- 5:5:5:1
			return CsRgb15;
		case B_RGBA15_BIG: // ARGB		BGRA 5:5:5:1
			return CsRgb15;
	
		// non linear color space -- incidently, all with 8 bits per value
		// Note, BBitmap and BView do not support all of these!
	
		/*
		// Loss / saturation points:
		//  Y		16 - 235 (absolute)
		//  Cb/Cr	16 - 240 (center 128)
	
		B_YCbCr422			= 0x4000,	// Y0  Cb0 Y1  Cr0
										// Y2  Cb2 Y3  Cr4
		B_YCbCr411			= 0x4001,	// Cb0 Y0  Cr0 Y1
										// Cb4 Y2  Cr4 Y3
										// Y4  Y5  Y6  Y7
		B_YCbCr444			= 0x4003,	// Y   Cb  Cr
		B_YCbCr420			= 0x4004,	// Non-interlaced only
			// on even scan lines: Cb0  Y0  Y1  Cb2 Y2  Y3
			// on odd scan lines:  Cr0  Y0  Y1  Cr2 Y2  Y3
	
		// Extrema points are:
		//  Y 0 - 207 (absolute)
		//  U -91 - 91 (offset 128)
		//  V -127 - 127 (offset 128)
	
		// Note that YUV byte order is different from YCbCr; use YCbCr, not YUV,
		// when that's what you mean!
		B_YUV422			= 0x4020,	// U0  Y0  V0  Y1
										// U2  Y2  V2  Y3
		B_YUV411			= 0x4021,	// U0  Y0  Y1  V0  Y2  Y3
										// U4  Y4  Y5  V4  Y6  Y7
		B_YUV444			= 0x4023,	// U0  Y0  V0  U1  Y1  V1
		B_YUV420			= 0x4024,	// Non-interlaced only
			// on even scan lines: U0  Y0  Y1  U2 Y2  Y3
			// on odd scan lines:  V0  Y0  Y1  V2 Y2  Y3
		B_YUV9				= 0x402C,
		B_YUV12				= 0x402D,
	
		B_UVL24				= 0x4030,	// UVL
		B_UVL32				= 0x4031,	// UVL-
		B_UVLA32			= 0x6031,	// UVLA
	
		// L lightness, a/b color-opponent dimensions
		B_LAB24				= 0x4032,	// Lab
		B_LAB32				= 0x4033,	// Lab-
		B_LABA32			= 0x6033,	// LabA
	
		// Red is at hue 0
		B_HSI24				= 0x4040,	// HSI
		B_HSI32				= 0x4041,	// HSI-
		B_HSIA32			= 0x6041,	// HSIA
	
		B_HSV24				= 0x4042,	// HSV
		B_HSV32				= 0x4043,	// HSV-
		B_HSVA32			= 0x6043,	// HSVA
	
		B_HLS24				= 0x4044,	// HLS
		B_HLS32				= 0x4045,	// HLS-
		B_HLSA32			= 0x6045,	// HLSA
	
		B_CMY24				= 0xC001,	// CMY
		B_CMY32				= 0xC002,	// CMY-
		B_CMYA32			= 0xE002,	// CMYA
		B_CMYK32			= 0xC003,	// CMYK
	
		// Compatibility declarations
		B_MONOCHROME_1_BIT	= B_GRAY1,
		B_GRAYSCALE_8_BIT	= B_GRAY8,
		B_COLOR_8_BIT		= B_CMAP8,
		B_RGB_32_BIT		= B_RGB32,
		B_RGB_16_BIT		= B_RGB15,
		B_BIG_RGB_32_BIT	= B_RGB32_BIG,
		B_BIG_RGB_16_BIT	= B_RGB15_BIG
		*/
		default:
			LgiTrace("%s:%i - Unknown BEOS colour space: 0x%x\n", _FL, cs);
			break;
	}
	
	return CsNone;
}

bool GMemDC::Create(int x, int y, GColourSpace Cs, int Flags)
{
	bool Status = FALSE;
	
	if (x < 1) x = 1;
	if (y < 1) y = 1;

	color_space Mode = B_RGB24;
	switch (Cs)
	{
		case CsIndex8:
		{
			Mode = B_CMAP8;
			break;
		}
		case CsRgb15:
		case CsBgr15:
		{
			Mode = B_RGB15;
			break;
		}
		case CsRgb16:
		case CsBgr16:
		{
			Mode = B_RGB16;
			break;
		}
		case CsRgb24:
		case CsBgr24:
		{
			Mode = B_RGB32;
			break;
		}
		case CsRgba32:
		case CsBgra32:
		case CsArgb32:
		case CsAbgr32:
		{
			Mode = B_RGB32;
			break;
		}
		default:
		{
			LgiTrace("%s:%i - Error: unsupported colour space '%s'\n",
				_FL,
				GColourSpaceToString(Cs));
			break;
		}
	}

	DeleteObj(d->Bmp);

	BRect r(0, 0, x-1, y-1);
	d->Bmp = new BBitmap(r, Mode, true, true);
	if (d->Bmp)
	{
		d->View = new BView(r, "GMemDC", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
		if (d->View)
		{
			d->Bmp->AddChild(d->View);
		}
		
		pMem = new GBmpMem;
		if (pMem)
		{
			pMem->Base = (uchar*) d->Bmp->Bits();
			pMem->x = x;
			pMem->y = y;
			pMem->Line = d->Bmp->BytesPerRow();
			pMem->Flags = 0;
			
			color_space Bcs = d->Bmp->ColorSpace();
			ColourSpace = pMem->Cs = BeosColourSpaceToLgi(Bcs);
			Status = ColourSpace != CsNone;

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
			
			if (!pApp)
			{
				LgiTrace("Couldn't create applicator (%ix%i,%i)\n", x, y, Cs);
				LgiAssert(0);
			}

			Clip.ZOff(x-1, y-1);
		}
	}

	return Status;
}

void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (Src)
	{
		if (Src->IsScreen())
		{
			// screen to memory Blt
		}
		else
		{
			// memory to memory Blt
			GSurface::Blt(x, y, Src, a);
		}
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
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
	if (y1 <= y2 &&
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
