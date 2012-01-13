/*
**	FILE:		Gdc2.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		24/2/97
**	DESCRIPTION:	GDC v2.xx
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes ************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <dos.h>
#include <math.h>

#include "Lgi.h"
#include "GdiLeak.h"

/****************************** Defines *************************************************************************************/

#define LGI_RAD					(360/(2*LGI_PI))

/****************************** Globals *************************************************************************************/
int Pixel24::Size = 3;
int Pixel32::Size = 4;

/****************************** Helper Functions ****************************************************************************/
void LgiDrawIcon(GSurface *pDC, int Dx, int Dy, HICON ico)
{
	ICONINFO iconinfo;
	GetIconInfo(ico, &iconinfo);
	BITMAP bm, msk;
	GetObject(iconinfo.hbmColor, sizeof(bm), &bm);
	GetObject(iconinfo.hbmMask, sizeof(msk), &msk);
	GArray<uint8> bits, mask;
	int bmp_bpp = bm.bmPlanes * bm.bmBitsPixel;
	int msk_bpp = msk.bmPlanes * msk.bmBitsPixel;
	bits.Length(bm.bmWidthBytes * bm.bmHeight);
	mask.Length(msk.bmWidthBytes * msk.bmHeight);
	GetBitmapBits(iconinfo.hbmColor, bits.Length(), &bits[0]);
	GetBitmapBits(iconinfo.hbmMask, mask.Length(), &mask[0]);

	bool HasAlpha = false;
	int y;
	for (y=0; !HasAlpha && y<bm.bmHeight; y++)
	{
		Pixel32 *c = (Pixel32*) &bits[y * bm.bmWidthBytes];
		for (int x=0; x<bm.bmWidth; x++)
		{
			if (c[x].a > 0)
			{
				HasAlpha = true;
				break;
			}
		}
	}					

	for (y=0; y<bm.bmHeight; y++)
	{
		uint32 *c = (uint32*) &bits[y * bm.bmWidthBytes];
		uint8 *m = &mask[y * msk.bmWidthBytes];
		for (int x=0; x<bm.bmWidth; x++)
		{
			int bit = 1 << (7 - (x&7));
			if (!(m[x>>3] & bit))
			{
				if (HasAlpha)
					pDC->Colour(c[x], 32);
				else
					pDC->Colour(c[x] | Rgba32(0, 0, 0, 255), 32);
				pDC->Set(Dx + x, Dy + y);
			}				
		}
	}					
}

/****************************** Classes *************************************************************************************/
GPalette::GPalette()
{
	Data = 0;
	hPal = 0;
	Lut = 0;
}

GPalette::~GPalette()
{
	DeleteArray(Lut);
	DeleteArray(Data);
	if (hPal) DeleteObject(hPal);
}

GPalette::GPalette(GPalette *pPal)
{
	Data = 0;
	hPal = 0;
	Lut = 0;
	Set(pPal);
}

GPalette::GPalette(uchar *pPal, int s)
{
	Data = 0;
	hPal = 0;
	Lut = 0;
	Set(pPal, s);
}

void GPalette::Set(GPalette *pPal)
{
    if (pPal == this)
        return;

	DeleteArray(Data);
	if (hPal) DeleteObject(hPal);

	if (pPal AND pPal->Data)
	{
		int Len = sizeof(LOGPALETTE) + (pPal->Data->palNumEntries * sizeof(GdcRGB));
		Data = (LOGPALETTE*) new char[Len];
		if (Data)
		{
			memcpy(Data, pPal->Data, Len);
			hPal = CreatePalette(Data);
		}
	}
}

void GPalette::Set(uchar *pPal, int s)
{
	DeleteArray(Data);
	if (hPal) DeleteObject(hPal);

	int Len = sizeof(LOGPALETTE) + (s * sizeof(GdcRGB));
	Data = (LOGPALETTE*) new char[Len];
	if (Data)
	{
		Data->palVersion = 0x300; 
		Data->palNumEntries = s;

		if (pPal)
		{
			for (int i=0; i<GetSize(); i++)
			{
				Data->palPalEntry[i].peRed = *pPal++;
				Data->palPalEntry[i].peGreen = *pPal++;
				Data->palPalEntry[i].peBlue = *pPal++;
				Data->palPalEntry[i].peFlags = 0;
			}
		}
		else
		{
			memset(Data->palPalEntry, 0, s * sizeof(GdcRGB));
		}

		hPal = CreatePalette(Data);
	}
}

bool GPalette::Update()
{
	if (Data AND hPal)
	{
		return SetPaletteEntries(hPal, 0, GetSize(), Data->palPalEntry);
	}

	return FALSE;
}

bool GPalette::SetSize(int s)
{
	int Len = sizeof(LOGPALETTE) + (s * sizeof(GdcRGB));
	LOGPALETTE *NewData = (LOGPALETTE*) new char[Len];
	if (NewData)
	{
		NewData->palVersion = 0x300; 
		NewData->palNumEntries = s;

		int Common = min(s, (Data) ? Data->palNumEntries : 0);
		memset(NewData->palPalEntry, 0, s * sizeof(NewData->palPalEntry[0]));
		if (Common > 0)
		{
			memcpy(NewData->palPalEntry, Data->palPalEntry, Common * sizeof(NewData->palPalEntry[0]));
		}

		DeleteArray(Data);
		if (hPal) DeleteObject(hPal);
		Data = NewData;
		hPal = CreatePalette(Data);
	}

	return (Data != 0) AND (hPal != 0);
}

int GPalette::GetSize()
{
	return (Data) ? Data->palNumEntries : 0;
}

GdcRGB *GPalette::operator [](int i)
{
	return (i >= 0 AND i < GetSize() AND Data) ? (GdcRGB*) (Data->palPalEntry + i) : 0;
}

void GPalette::SwapRAndB()
{
	if (Data)
	{
		for (int i=0; i<GetSize(); i++)
		{
			uchar n = (*this)[i]->R;
			(*this)[i]->R = (*this)[i]->B;
			(*this)[i]->B = n;
		}
	}

	Update();
}

uchar *GPalette::MakeLut(int Bits)
{
	if (Bits)
	{
		if (!Lut)
		{
			GdcRGB *p = (*this)[0];

			int Size = 1 << Bits;
			switch (Bits)
			{
				case 15:
				{
					Lut = new uchar[Size];
					if (Lut)
					{
						for (int i=0; i<Size; i++)
						{
							int r = (R15(i) << 3) | (R15(i) >> 2);
							int g = (G15(i) << 3) | (G15(i) >> 2);
							int b = (B15(i) << 3) | (B15(i) >> 2);

							Lut[i] = MatchRgb(Rgb24(r, g, b));
						}
					}
					break;
				}
				case 16:
				{
					Lut = new uchar[Size];
					if (Lut)
					{
						for (int i=0; i<Size; i++)
						{
							int r = (R16(i) << 3) | (R16(i) >> 2);
							int g = (G16(i) << 2) | (G16(i) >> 3);
							int b = (B16(i) << 3) | (B16(i) >> 2);

							Lut[i] = MatchRgb(Rgb24(r, g, b));
						}
					}
					break;
				}
			}
		}
	}
	else
	{
		DeleteArray(Lut);
	}

	return Lut;
}

int GPalette::MatchRgb(COLOUR Rgb)
{
	if (Data)
	{
		GdcRGB *Entry = (*this)[0];
		/*
		int r = (R24(Rgb) & 0xF8) + 4;
		int g = (G24(Rgb) & 0xF8) + 4;
		int b = (B24(Rgb) & 0xF8) + 4;
		*/
		int r = R24(Rgb);
		int g = G24(Rgb);
		int b = B24(Rgb);
		ulong *squares = GdcD->GetCharSquares();
		ulong mindist = 200000;
		ulong bestcolor;
		ulong curdist;
		long rdist;
		long gdist;
		long bdist;

		for (int i = 0; i < Data->palNumEntries; i++)
		{
			rdist = Entry[i].R - r;
			gdist = Entry[i].G - g;
			bdist = Entry[i].B - b;
			curdist = squares[rdist] + squares[gdist] + squares[bdist];

			if (curdist < mindist)
			{
				mindist = curdist;
				bestcolor = i;
			}
		}

		return bestcolor;
	}

	return 0;
}

void GPalette::CreateGreyScale()
{
	SetSize(256);
	GdcRGB *p = (*this)[0];

	for (int i=0; i<256; i++)
	{
		p->R = i;
		p->G = i;
		p->B = i;
		p->Flags = 0;
		p++;
	}
}

void GPalette::CreateCube()
{
	SetSize(216);
	GdcRGB *p = (*this)[0];

	for (int r=0; r<6; r++)
	{
		for (int g=0; g<6; g++)
		{
			for (int b=0; b<6; b++)
			{
				p->R = r * 51;
				p->G = g * 51;
				p->B = b * 51;
				p->Flags = 0;
				p++;
			}
		}
	}
}


void TrimWhite(char *s)
{
	char *White = " \r\n\t";
	char *c = s;
	while (*c AND strchr(White, *c)) c++;
	if (c != s)
	{
		strcpy(s, c);
	}

	c = s + strlen(s) - 1;
	while (c > s AND strchr(White, *c))
	{
		*c = 0;
		c--;
	}
}

bool GPalette::Load(GFile &F)
{
	bool Status = FALSE;
	char Buf[256];
	
	F.ReadStr(Buf, sizeof(Buf));
	TrimWhite(Buf);
	if (strcmp(Buf, "JASC-PAL") == 0)
	{
		// is JASC palette
		// skip hex length
		F.ReadStr(Buf, sizeof(Buf));

		// read decimal length
		F.ReadStr(Buf, sizeof(Buf));
		SetSize(atoi(Buf));
		for (int i=0; i<GetSize() AND !F.Eof(); i++)
		{
			F.ReadStr(Buf, sizeof(Buf));
			GdcRGB *p = (*this)[i];
			if (p)
			{
				p->R = atoi(strtok(Buf, " "));
				p->G = atoi(strtok(NULL, " "));
				p->B = atoi(strtok(NULL, " "));

			}

			Status = TRUE;
		}
	}
	else
	{
		// check for microsoft format
	}

	return Status;
}

bool GPalette::Save(GFile &F, int Format)
{
	bool Status = FALSE;

	switch (Format)
	{
		case GDCPAL_JASC:
		{
			char Buf[256];

			sprintf(Buf, "JASC-PAL\r\n%04.4X\r\n%i\r\n", GetSize(), GetSize());
			F.Write(Buf, strlen(Buf));

			for (int i=0; i<GetSize(); i++)
			{
				GdcRGB *p = (*this)[i];
				if (p)
				{
					sprintf(Buf, "%i %i %i\r\n", p->R, p->G, p->B);
					F.Write(Buf, strlen(Buf));
				}
			}

			Status = true;
			break;
		}
	}

	return Status;
}

bool GPalette::operator ==(GPalette &p)
{
	if (GetSize() == p.GetSize())
	{
		GdcRGB *a = (*this)[0];
		GdcRGB *b = p[0];

		for (int i=0; i<GetSize(); i++)
		{
			if (	a->R != b->R OR
				a->G != b->G OR
				a->B != b->B)
			{
				return FALSE;
			}

			a++;
			b++;
		}

		return TRUE;
	}

	return FALSE;
}

bool GPalette::operator !=(GPalette &p)
{
	return !((*this) == p);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GBmpMem::GBmpMem()
{
	Base = 0;
	Flags = 0;
}

GBmpMem::~GBmpMem()
{
	if (Base AND (Flags & GDC_OWN_MEMORY))
	{
		delete [] Base;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/*
GDeviceContext::GDeviceContext()
{
	OriginX = OriginY = 0;
	pMem = NULL;
	pAlphaDC = 0;
	Flags = 0;
	Clip.ZOff(0, 0);
	pPalette = NULL;
	pApp = NULL;

	for (int i=0; i<GDC_CACHE_SIZE; i++)
	{
		pAppCache[i] = 0;
	}
}

GDeviceContext::~GDeviceContext()
{
	DrawOnAlpha(FALSE);
	DeleteObj(pMem);
	DeleteObj(pAlphaDC);

	if (pPalette AND (Flags & GDC_OWN_PALETTE))
	{
		DeleteObj(pPalette);
	}

	if (	(Flags & GDC_OWN_APPLICATOR) AND
		NOT (Flags & GDC_CACHED_APPLICATOR))
	{
		DeleteObj(pApp);
	}

	for (int i=0; i<GDC_CACHE_SIZE; i++)
	{
		DeleteObj(pAppCache[i]);
	}
}

bool GDeviceContext::IsAlpha(bool b)
{
	DrawOnAlpha(FALSE);

	if (b)
	{
		if (NOT pAlphaDC)
		{
			pAlphaDC = new GMemDC;
		}

		if (pAlphaDC AND pMem)
		{
			if (NOT pAlphaDC->Create(pMem->x, pMem->y, 8))
			{
				DeleteObj(pAlphaDC);
			}
			else
			{
				ClearFlag(Flags, GDC_DRAW_ON_ALPHA);
			}
		}
	}
	else
	{
		DeleteObj(pAlphaDC);
	}

	return (b == IsAlpha());
}

bool GDeviceContext::DrawOnAlpha(bool Draw)
{
	bool Prev = DrawOnAlpha();

	if (Draw)
	{
		if (NOT Prev AND pAlphaDC AND pMem)
		{
			GBmpMem *Temp = pMem;
			pMem = pAlphaDC->pMem;
			pAlphaDC->pMem = Temp;
			SetFlag(Flags, GDC_DRAW_ON_ALPHA);

			// remove current application, we may be changing bit depth
			if (Flags & GDC_OWN_APPLICATOR)
			{
				DeleteObj(pApp);
			}
			else pApp = 0;

			// set the new applicator
			PrevOp = Op(GDC_SET);
		}
	}
	else
	{
		if (Prev AND pAlphaDC AND pMem)
		{
			GBmpMem *Temp = pMem;
			pMem = pAlphaDC->pMem;
			pAlphaDC->pMem = Temp;
			ClearFlag(Flags, GDC_DRAW_ON_ALPHA);

			Op(PrevOp);
		}
	}

	return Prev;
}

GApplicator *GDeviceContext::CreateApplicator(int Op, int Bits)
{
	GApplicator *pA = NULL;

	if (NOT Bits AND pMem)
	{
		if (DrawOnAlpha())
		{
			Bits = 8;
		}
		else
		{
			Bits = pMem->Bits;
		}
	}

	pA = GApplicatorFactory::NewApp(Bits, Op);
	if (NOT pA)
	{
		LgiTrace("%s:%i - Failed to create application (op=%i)\n", Op);
		LgiAssert(0);
	}

	if (pA AND pMem)
	{
		if (DrawOnAlpha())
		{
			pA->SetSurface(pMem);
		}
		else
		{
			pA->SetSurface(pMem, pPalette, (pAlphaDC) ? pAlphaDC->pMem : 0);
		}
		pA->SetOp(Op);
	}

	return pA;
}

bool GDeviceContext::Applicator(GApplicator *pApplicator)
{
	bool Status = FALSE;

	if (pApplicator)
	{
		if (Flags & GDC_OWN_APPLICATOR)
		{
			DeleteObj(pApp)
			Flags &= ~GDC_OWN_APPLICATOR;
		}

		Flags &= ~GDC_CACHED_APPLICATOR;

		pApp = pApplicator;
		if (DrawOnAlpha())
		{
			pApp->SetSurface(pMem);
		}
		else
		{
			pApp->SetSurface(pMem, pPalette, pAlphaDC->pMem);
		}
		pApp->SetPtr(0, 0);

		Status = TRUE;
	}

	return Status;
}

GApplicator *GDeviceContext::Applicator()
{
	return pApp;
}

GRect GDeviceContext::ClipRgn(GRect *Rgn)
{
	GRect Old = Clip;
	
	if (Rgn)
	{
		Clip.x1 = max(0, Rgn->x1);
		Clip.y1 = max(0, Rgn->y1);
		Clip.x2 = min(X()-1, Rgn->x2);
		Clip.y2 = min(Y()-1, Rgn->y2);
	}
	else
	{
		Clip.x1 = 0;
		Clip.y1 = 0;
		Clip.x2 = X()-1;
		Clip.y2 = Y()-1;
	}
	
	return Old;
}

GRect GDeviceContext::ClipRgn()
{
	return Clip;
}

COLOUR GDeviceContext::Colour(COLOUR c, int Bits)
{
	LgiAssert(pApp);

	COLOUR cPrev = pApp->c;

	if (Bits)
	{
		pApp->c = CBit(GetBits(), c, Bits, pPalette);
	}
	else
	{
		pApp->c = c;
	}

	return cPrev;
}

int GDeviceContext::Op(int NewOp)
{
	int PrevOp = (pApp) ? pApp->GetOp() : GDC_SET;
	if (NOT pApp OR PrevOp != NewOp)
	{
		COLOUR cCurrent = (pApp) ? Colour() : 0;

		if (Flags & GDC_OWN_APPLICATOR)
		{
			DeleteObj(pApp);
		}

		if (NewOp < GDC_CACHE_SIZE AND NOT DrawOnAlpha())
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

		if (pApp)
		{
			Colour(cCurrent);
		}
		else
		{
			printf("Error: Couldn't create applicator, Op=%i\n", NewOp);
			LgiAssert(0);
		}
	}

	if (pApp)
	{
		pApp->SetSurface(pMem, pPalette, (pAlphaDC) ? pAlphaDC->pMem : 0);
	}

	return PrevOp;
}

GPalette *GDeviceContext::Palette()
{
	if (NOT pPalette AND pMem AND (pMem->Flags & GDC_ON_SCREEN))
	{
		pPalette = GdcD->GetSystemPalette();
		if (pPalette)
		{
			Flags |= GDC_OWN_PALETTE;
		}
	}

	return pPalette;
}

void GDeviceContext::Palette(GPalette *pPal, bool bOwnIt)
{
	if (pPalette AND Flags & GDC_OWN_PALETTE)
	{
		delete pPalette;
	}

	pPalette = pPal;

	if (pPal AND bOwnIt)
	{
		Flags |= GDC_OWN_PALETTE;
	}
	else
	{
		Flags &= ~GDC_OWN_PALETTE;
	}

	if (pApp)
	{
		pApp->SetSurface(pMem, pPalette, (pAlphaDC) ? pAlphaDC->pMem : 0);
	}
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////
class GlobalColourEntry
{
public:
	COLOUR c24;
	bool Fixed;
	bool Used;

	GlobalColourEntry()
	{
		c24 = 0;
		Fixed = false;
		Used = false;
	}
};

class GGlobalColourPrivate
{
public:
	GlobalColourEntry c[256];
	GPalette *Global;
	List<GSurface> Cache;
	int FirstUnused;
	
	int FreeColours()
	{
		int f = 0;
		
		for (int i=0; i<256; i++)
		{
			if (!c[i].Used)
			{
				f++;
			}
		}

		return f;
	}

	GGlobalColourPrivate()
	{
		FirstUnused = 0;
		Global = 0;
		#ifdef WIN32
		if (GdcD->GetBits() <= 8)
		{
			PALETTEENTRY e[256];
			HDC hdc = CreateCompatibleDC(0);
			int Entries = GetSystemPaletteEntries(hdc, 0, 1 << GdcD->GetBits(), e);
			for (int i=0; i<10; i++)
			{
				c[i].c24 = Rgb24(e[i].peRed, e[i].peGreen, e[i].peBlue);
				c[i].Fixed = true;
				c[i].Used = true;

				c[255-i].c24 = Rgb24(e[255-i].peRed, e[255-i].peGreen, e[255-i].peBlue);
				c[255-i].Fixed = true;
				c[255-i].Used = true;
			}
			DeleteDC(hdc);
			FirstUnused = 10;
		}
		#endif
	}

	~GGlobalColourPrivate()
	{
		Cache.DeleteObjects();
	}
};

GGlobalColour::GGlobalColour()
{
	d = new GGlobalColourPrivate;
}

GGlobalColour::~GGlobalColour()
{
	DeleteObj(d);
}

COLOUR GGlobalColour::AddColour(COLOUR c24)
{
	for (int i=0; i<256; i++)
	{
		if (d->c[i].c24 == c24)
		{
			#ifdef WIN32
			return PALETTEINDEX(i);
			#else
			return i;
			#endif
		}
	}

	if (d->FirstUnused >= 0)
	{
		d->c[d->FirstUnused].c24 = c24;
		d->c[d->FirstUnused].Used = true;
		d->c[d->FirstUnused].Fixed = true;
		#ifdef WIN32
		return PALETTEINDEX(d->FirstUnused++);
		#else
		return d->FirstUnused++;
		#endif
	}

	return c24;
}

bool GGlobalColour::AddBitmap(GSurface *pDC)
{
	if (pDC)
	{
		GSurface *s = new GMemDC(pDC);
		if (s)
		{
			d->Cache.Insert(s);
			return true;
		}
	}

	return false;
}

void KeyBlt(GSurface *To, GSurface *From, COLOUR Key)
{
	int Bits = From->GetBits();
	GPalette *Pal = From->Palette();

	for (int y=0; y<From->Y(); y++)
	{
		for (int x=0; x<From->X(); x++)
		{
			COLOUR c = From->Get(x, y);
			if (c != Key)
			{
				To->Colour(CBit(To->GetBits(), c, Bits, Pal));
				To->Set(x, y);
			}
		}
	}
}

bool GGlobalColour::AddBitmap(GImageList *il)
{
	if (il)
	{
		GSurface *s = new GMemDC(il);
		if (s)
		{
			// Cache the full colour bitmap
			d->Cache.Insert(s);

			// Cache the disabled alpha blending bitmap
			s = new GMemDC(il->X(), il->Y(), 24);
			if (s)
			{
				s->Op(GDC_ALPHA);
				GApplicator *pApp = s->Applicator();
				if (pApp) pApp->SetVar(GAPP_ALPHA_A, 40);
				s->Blt(0, 0, il);
				d->Cache.Insert(s);
			}
			return true;
		}
	}

	return false;
}

bool GGlobalColour::MakeGlobalPalette()
{
	if (!d->Global)
	{
		d->Global = new GPalette(0, 256);
		if (d->Global)
		{
			for (GSurface *pDC = d->Cache.First(); pDC; pDC = d->Cache.Next())
			{
				for (int y=0; y<pDC->Y(); y++)
				{
					for (int x=0; x<pDC->X(); x++)
					{
						COLOUR c = CBit(24, pDC->Get(x, y), pDC->GetBits(), pDC->Palette());
						AddColour(c);
					}
				}
			}

			for (int i=0; i<256; i++)
			{
				GdcRGB *r = (*d->Global)[i];
				if (r)
				{
					/*
					if (i < 10 OR i > 246)
					{
						r->R = i;
						r->G = 0;
						r->B = 0;
						r->Flags = PC_EXPLICIT;
					}
					else
					{
					*/
						r->R = R24(d->c[i].c24);
						r->G = G24(d->c[i].c24);
						r->B = B24(d->c[i].c24);
						r->Flags = 0; // PC_RESERVED;
					// }
				}
			}

			d->Global->Update();
			d->Cache.DeleteObjects();
		}
	}

	return d->Global != 0;
}

GPalette *GGlobalColour::GetPalette()
{
	if (!d->Global)
	{
		MakeGlobalPalette();
	}

	return d->Global;
}

COLOUR GGlobalColour::GetColour(COLOUR c24)
{
	for (int i=0; i<256; i++)
	{
		if (d->c[i].Used AND
			d->c[i].c24 == c24)
		{
			return PALETTEINDEX(i);
		}
	}

	return c24;
}

bool GGlobalColour::RemapBitmap(GSurface *pDC)
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////
class GdcDevicePrivate
{
public:
	// class GLibrary *Iconv;
	// Iconv startup
	// extern GLibrary *_LgiStartIconv();
	// Iconv = _LgiStartIconv();
	// DeleteObj(Iconv);

	GdcDevice *Device;

	// Current mode info
	int ScrX;
	int ScrY;
	int ScrBits;

	// Palette
	double GammaCorrection;
	uchar GammaTable[256];
	GPalette *pSysPal;
	GGlobalColour *GlobalColour;

	// Data
	ulong *CharSquareData;
	uchar *Div255;

	// Options
	int OptVal[GDC_MAX_OPTION];

	// Alpha
	#if defined WIN32
	GLibrary MsImg;
	#elif defined LINUX
	int Pixel24Size;
	#endif

	GdcDevicePrivate(GdcDevice *d)
	#ifdef WIN32
	 : MsImg("msimg32")
	#endif
	{
		Device = d;
		GlobalColour = 0;
		ZeroObj(OptVal);

		// Palette information
		GammaCorrection = 1.0;

		// Get mode stuff
		HWND hDesktop = GetDesktopWindow();
		HDC hScreenDC = GetDC(hDesktop);

		ScrX = GetSystemMetrics(SM_CXSCREEN);
		ScrY = GetSystemMetrics(SM_CYSCREEN);
		ScrBits = GetDeviceCaps(hScreenDC, BITSPIXEL);

		int Colours = 1 << ScrBits;
		pSysPal = (ScrBits <= 8) ? new GPalette(0, Colours) : 0;
		if (pSysPal)
		{
			GdcRGB *p = (*pSysPal)[0];
			if (p)
			{
				PALETTEENTRY pal[256];
				GetSystemPaletteEntries(hScreenDC, 0, Colours, pal);
				for (int i=0; i<Colours; i++)
				{
					p[i].R = pal[i].peRed;
					p[i].G = pal[i].peGreen;
					p[i].B = pal[i].peBlue;
				}
			}
		}

		ReleaseDC(hDesktop, hScreenDC);

		// Options
		// OptVal[GDC_REDUCE_TYPE] = REDUCE_ERROR_DIFFUSION;
		// OptVal[GDC_REDUCE_TYPE] = REDUCE_HALFTONE;
		OptVal[GDC_REDUCE_TYPE] = REDUCE_NEAREST;
		OptVal[GDC_HALFTONE_BASE_INDEX] = 0;

		// Calcuate lookups
		CharSquareData = new ulong[255+255+1];
		if (CharSquareData)
		{
			for (int i = -255; i <= 255; i++)
			{
				CharSquareData[i+255] = i*i;
			}
		}

		// Divide by 255 lookup, real handy for alpha blending 8 bit components
		int Size = (255 * 255) * 2;
		Div255 = new uchar[Size];
		if (Div255)
		{
			for (int i=0; i<Size; i++)
			{
				int n = (i + 128) / 255;
				Div255[i] = min(n, 255);
			}
		}

	}

	~GdcDevicePrivate()
	{
		DeleteObj(GlobalColour);
		DeleteArray(CharSquareData);
		DeleteArray(Div255);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
GdcDevice *GdcDevice::pInstance = 0;

GdcDevice::GdcDevice()
{
	LgiAssert(pInstance == 0);

	pInstance = this;
	AlphaBlend = 0;
	d = new GdcDevicePrivate(this);
	// Alpha proc
	if (d->MsImg.IsLoaded())
	{
		AlphaBlend = (MsImg32_AlphaBlend) d->MsImg.GetAddress("AlphaBlend");
	}
	SetGamma(LGI_DEFAULT_GAMMA);
	d->GlobalColour = new GGlobalColour;
}

GdcDevice::~GdcDevice()
{
	DeleteObj(d);
	pInstance = NULL;
}

int GdcDevice::GetOption(int Opt)
{
	return (Opt >= 0 AND Opt < GDC_MAX_OPTION) ? d->OptVal[Opt] : 0;
}

int GdcDevice::SetOption(int Opt, int Value)
{
	int Prev = d->OptVal[Opt];
	if (Opt >= 0 AND Opt < GDC_MAX_OPTION)
		d->OptVal[Opt] = Value;
	return Prev;
}

ulong *GdcDevice::GetCharSquares()
{
	return (d->CharSquareData) ? d->CharSquareData + 255 : 0;
}

uchar *GdcDevice::GetDiv255()
{
	return d->Div255;
}

GGlobalColour *GdcDevice::GetGlobalColour()
{
	return d->GlobalColour;
}

int GdcDevice::GetBits()
{
	return d->ScrBits;
}

int GdcDevice::X()
{
	return d->ScrX;
}

int GdcDevice::Y()
{
	return d->ScrY;
}

void GdcDevice::SetGamma(double Gamma)
{
	d->GammaCorrection = Gamma;
	for (int i=0; i<256; i++)
	{
		d->GammaTable[i] = (uchar) (pow(((double)i)/256, Gamma) * 256);
	}
}

double GdcDevice::GetGamma()
{
	return d->GammaCorrection;
}

void GdcDevice::SetSystemPalette(int Start, int Size, GPalette *pPal)
{
	/*
	if (pPal)
	{
		uchar Pal[768];
		uchar *Temp = Pal;
		uchar *System = Palette + (Start * 3);
		GdcRGB *P = (*pPal)[Start];
		
		for (int i=0; i<Size; i++, P++)
		{
			*Temp++ = GammaTable[*System++ = P->R] >> PalShift;
			*Temp++ = GammaTable[*System++ = P->G] >> PalShift;
			*Temp++ = GammaTable[*System++ = P->B] >> PalShift;
		}
		
		SetPaletteBlockDirect(Pal, Start, Size * 3);
	}
	*/
}

GPalette *GdcDevice::GetSystemPalette()
{
	return d->pSysPal;
}

void GdcDevice::SetColourPaletteType(int Type)
{
	bool SetOpt = TRUE;

	/*
	switch (Type)
	{
		case PALTYPE_ALLOC:
		{
			SetPalIndex(0, 0, 0, 0);
			SetPalIndex(255, 255, 255, 255);
			break;
		}
		case PALTYPE_RGB_CUBE:
		{
			uchar Pal[648];
			uchar *p = Pal;

			for (int r=0; r<6; r++)
			{
				for (int g=0; g<6; g++)
				{
					for (int b=0; b<6; b++)
					{
						*p++ = r * 51;
						*p++ = g * 51;
						*p++ = b * 51;
					}
				}
			}

			SetPalBlock(0, 216, Pal);
			SetPalIndex(255, 0xFF, 0xFF, 0xFF);
			break;
		}
		case PALTYPE_HSL:
		{
			for (int h = 0; h<16; h++)
			{
			}
			break;
		}
		default:
		{
			SetOpt = FALSE;
		}
	}

	if (SetOpt)
	{
		SetOption(GDC_PALETTE_TYPE, Type);
	}
	*/
}

COLOUR GdcDevice::GetColour(COLOUR Rgb24, GSurface *pDC)
{
	int Bits = (pDC) ? pDC->GetBits() : GetBits();
	COLOUR C;

	switch (Bits)
	{
		case 8:
		{
			switch (GetOption(GDC_PALETTE_TYPE))
			{
				case PALTYPE_ALLOC:
				{
					static uchar Current = 1;

					Rgb24 &= 0xFFFFFF;
					if (Rgb24 == 0xFFFFFF)
					{
						C = 0xFF;
					}
					else if (Rgb24 == 0)
					{
						C = 0;
					}
					else if (d->pSysPal)
					{
						(*d->pSysPal)[Current]->Set(R24(Rgb24), G24(Rgb24), B24(Rgb24));
						C = Current++;
						if (Current == 255) Current = 1;
					}
					else
					{
					    C = 0;
					}
					break;
				}
				case PALTYPE_RGB_CUBE:
				{
					uchar r = (R24(Rgb24) + 25) / 51;
					uchar g = (G24(Rgb24) + 25) / 51;
					uchar b = (B24(Rgb24) + 25) / 51;
					
					C = (r*36) + (g*6) + b;
					break;
				}
				case PALTYPE_HSL:
				{
					C = 0;
					break;
				}
			}
			break;
		}
		case 16:
		{
			C = Rgb24To16(Rgb24);
			break;
		}
		case 24:
		{
			C = Rgb24;
			break;
		}
		case 32:
		{
			C = Rgb24To32(Rgb24);
			break;
		}
	}

	return C;
}

/*
GSprite::GSprite()
{
	Sx = Sy = Bits = 0;
	PosX = PosY = 0;
	HotX = HotY = 0;
	Visible = FALSE;
	pScreen = pBack = pMask = pColour = pTemp = 0;
}

GSprite::~GSprite()
{
	Delete();
}

void GSprite::SetXY(int x, int y)
{
	PosX = x;
	PosY = y;
}

bool GSprite::Create(GSurface *pScr, int x, int y, int SrcBitDepth, uchar *Colour, uchar *Mask)
{
	bool Status = FALSE;

	pScreen = pScr;
	if (pScreen AND Colour AND Mask)
	{
		Delete();

		if (SetSize(x, y, pScreen->GetBits()))
		{
			COLOUR cBlack = 0;
			COLOUR cWhite = 0xFFFFFFFF;

			switch (SrcBitDepth)
			{
				case 1:
				{
					int Scan = (Sx + 7) / 8;

					for (int y=0; y<Sy; y++)
					{
						uchar *c = Colour + (y * Scan);
						uchar *m = Mask + (y * Scan);

						for (int x=0; x<Sx; x++)
						{
							int Div = x >> 3;
							int Mod = x & 0x7;

							pColour->Colour((c[Div] & (0x80 >> Mod)) ? cWhite : cBlack);
							pColour->Set(x, y);

							pMask->Colour((m[Div] & (0x80 >> Mod)) ? cWhite : cBlack);
							pMask->Set(x, y);
						}
					}

					Status = TRUE;
					break;
				}
			}
		}

		if (NOT Status)
		{
			Delete();
		}
	}

	return Status;
}

bool GSprite::Create(GSurface *pScr, GSprite *pSpr)
{
	bool Status = FALSE;

	pScreen = pSpr->pScreen;
	if (pScreen)
	{
		Delete();

		if (SetSize(pSpr->X(), pSpr->Y(), pSpr->GetBits()))
		{
			HotX = pSpr->GetHotX();
			HotY = pSpr->GetHotY();

			pBack->Blt(0, 0, pSpr->pBack, NULL);
			pMask->Blt(0, 0, pSpr->pMask, NULL);
			pColour->Blt(0, 0, pSpr->pColour, NULL);

			Status = TRUE;
		}

		if (NOT Status)
		{
			Delete();
		}
	}

	return Status;
}

bool GSprite::CreateKey(GSurface *pScr, int x, int y, int Bits, uchar *Colour, int Key)
{
	bool Status = FALSE;

	pScreen = pScr;
	if (pScreen AND Colour)
	{
		Delete();

		if (SetSize(x, y, Bits))
		{
			COLOUR cBlack = 0;
			COLOUR cWhite = 0xFFFFFFFF;

			switch (Bits)
			{
				case 8:
				{
					for (int y=0; y<Sy; y++)
					{
						uchar *c = Colour + (y * Sx);

						for (int x=0; x<Sx; x++, c++)
						{
							pColour->Colour(*c);
							pColour->Set(x, y);

							pMask->Colour((*c != Key) ? cWhite : cBlack);
							pMask->Set(x, y);
						}
					}

					Status = TRUE;
					break;
				}
				case 16:
				case 15:
				{
					for (int y=0; y<Sy; y++)
					{
						ushort *c = ((ushort*) Colour) + (y * Sx);

						for (int x=0; x<Sx; x++, c++)
						{
							pColour->Colour(*c);
							pColour->Set(x, y);

							pMask->Colour((*c != Key) ? cWhite : cBlack);
							pMask->Set(x, y);
						}
					}

					Status = TRUE;
					break;
				}
				case 24:
				{
					for (int y=0; y<Sy; y++)
					{
						uchar *c = Colour + (y * Sx * 3);

						for (int x=0; x<Sx; x++, c+=3)
						{
							COLOUR p = Rgb24(c[2], c[1], c[0]);
							pColour->Colour(p);
							pColour->Set(x, y);

							pMask->Colour((p != Key) ? cWhite : cBlack);
							pMask->Set(x, y);
						}
					}

					Status = TRUE;
					break;
				}
				case 32:
				{
					for (int y=0; y<Sy; y++)
					{
						ulong *c = ((ulong*) Colour) + (y * Sx);

						for (int x=0; x<Sx; x++, c++)
						{
							pColour->Colour(*c);
							pColour->Set(x, y);

							pMask->Colour((*c != Key) ? cWhite : cBlack);
							pMask->Set(x, y);
						}
					}

					Status = TRUE;
					break;
				}
			}
		}

		if (NOT Status)
		{
			Delete();
		}
	}

	return Status;
}

bool GSprite::SetSize(int x, int y, int BitSize, GSurface *pScr)
{
	bool Status = FALSE;
	if (pScr)
	{
		pScreen = pScr;
	}

	if (pScreen)
	{
		Sx = x;
		Sy = y;
		Bits = BitSize;
		HotX = 0;
		HotY = 0;
		DrawMode = 0;

		pBack = new GMemDC;
		pMask = new GMemDC;
		pColour = new GMemDC;
		pTemp = new GMemDC;

		if (pBack AND pMask AND pColour AND pTemp)
		{
			if (	pBack->Create(x, y, Bits) AND
				pMask->Create(x, y, Bits) AND
				pColour->Create(x, y, Bits) AND
				pTemp->Create(x<<1, y<<1, Bits))
			{
				if (pScreen->GetBits() == 8)
				{
					pTemp->Palette(new GPalette(pScreen->Palette()));
				}

				Status = TRUE;
			}
		}
	}

	return Status;
};

void GSprite::Delete()
{
	SetVisible(FALSE);
	DeleteObj(pBack);
	DeleteObj(pMask);
	DeleteObj(pColour);
	DeleteObj(pTemp);
}

void GSprite::SetHotPoint(int x, int y)
{
	HotX = x;
	HotY = y;
}

void GSprite::SetVisible(bool v)
{
	if (pScreen)
	{
		if (Visible)
		{
			if (NOT v)
			{
				// Hide
				int Op = pScreen->Op(GDC_SET);
				pScreen->Blt(PosX-HotX, PosY-HotY, pBack, NULL);
				pScreen->Op(Op);
				Visible = FALSE;
			}
		}
		else
		{
			if (v)
			{
				// Show
				GRect s, n;

				s.ZOff(Sx-1, Sy-1);
				n = s;
				s.Offset(PosX-HotX, PosY-HotY);
				pBack->Blt(0, 0, pScreen, &s);
				
				pTemp->Op(GDC_SET);
				pTemp->Blt(0, 0, pBack);

				if (DrawMode == 3)
				{
					int Op = pTemp->Op(GDC_ALPHA);
					pTemp->Blt(0, 0, pColour, NULL);
					pTemp->Op(Op);
				}
				else
				{
					int OldOp = pTemp->Op(GDC_AND);
					pTemp->Blt(0, 0, pMask, NULL);
					if (DrawMode == 1)
					{
						pTemp->Op(GDC_OR);
					}
					else
					{
						pTemp->Op(GDC_XOR);
					}
					pTemp->Blt(0, 0, pColour, NULL);
					pTemp->Op(OldOp);
				}

				int OldOp = pScreen->Op(GDC_SET);
				pScreen->Blt(PosX-HotX, PosY-HotY, pTemp, &n);
				pScreen->Op(OldOp);
				Visible = TRUE;
			}
		}
	}
}

void GSprite::Draw(int x, int y, COLOUR Back, int Mode, GSurface *pDC)
{
	if (pScreen)
	{
		if (NOT pDC)
		{
			pDC = pScreen;
		}

		if (Mode == 3)
		{
			pDC->Blt(x-HotX, y-HotY, pColour, NULL);
		}
		else if (Mode == 2)
		{
			int Op = pScreen->Op(GDC_AND);
			pDC->Blt(x-HotX, y-HotY, pMask, NULL);
			pDC->Op(GDC_OR);
			pDC->Blt(x-HotX, y-HotY, pColour, NULL);
			pDC->Op(Op);
		}
		else
		{
			pBack->Colour(Back);
			pBack->Rectangle(0, 0, pBack->X(), pBack->Y());
			
			if (Mode == 0)
			{
				pBack->Op(GDC_AND);
				pBack->Blt(0, 0, pColour, NULL);
				pBack->Op(GDC_XOR);
				pBack->Blt(0, 0, pMask, NULL);
			}
			else if (Mode == 1)
			{
				pBack->Op(GDC_AND);
				pBack->Blt(0, 0, pMask, NULL);
				pBack->Op(GDC_OR);
				pBack->Blt(0, 0, pColour, NULL);
			}

			pDC->Blt(x-HotX, y-HotY, pBack, NULL);
		}
	}
}

void GSprite::Move(int x, int y, bool WaitRetrace)
{
	if (	pScreen AND
		(x != PosX OR
		y != PosY))
	{
		if (Visible)
		{
			GRect New, Old;
			
			New.ZOff(Sx-1, Sy-1);
			Old = New;
			New.Offset(x - HotX, y - HotY);
			Old.Offset(PosX - HotX, PosY - HotY);
			
			if (New.Overlap(&Old))
			{
				GRect Both;
				
				Both.Union(&Old, &New);
				
				if (pTemp)
				{
					// Get working area of screen
					pTemp->Blt(0, 0, pScreen, &Both);

					// Restore the back of the sprite
					pTemp->Blt(Old.x1-Both.x1, Old.y1-Both.y1, pBack, NULL);

					// Update any dependent graphics
					DrawOnMove(pTemp, x, y, HotX-Both.x1, HotX-Both.y1);

					// Get the new background
					GRect Bk = New;
					Bk.Offset(-Both.x1, -Both.y1);
					pBack->Blt(0, 0, pTemp, &Bk);
		
					// Paint the new sprite
					int NewX = New.x1 - Both.x1;
					int NewY = New.y1 - Both.y1;

					if (DrawMode == 3)
					{
						int Op = pTemp->Op(GDC_ALPHA);
						pTemp->Blt(NewX, NewY, pColour, NULL);
						pTemp->Op(Op);
					}
					else
					{
						int OldOp = pTemp->Op(GDC_AND);
						pTemp->Blt(NewX, NewY, pMask, NULL);

						if (DrawMode == 1)
						{
							pTemp->Op(GDC_OR);
						}
						else
						{
							pTemp->Op(GDC_XOR);
						}

						pTemp->Blt(NewX, NewY, pColour, NULL);
						pTemp->Op(OldOp);
					}
		
					// Update all under sprite info on screen
					DrawOnMove(pScreen, x, y, 0, 0);

					// Put it all back on the screen
					GRect b = Both;
					b.Offset(-b.x1, -b.y1);

					int OldOp = pScreen->Op(GDC_SET);
					pScreen->Blt(Both.x1, Both.y1, pTemp, &b);
					pScreen->Op(OldOp);

					PosX = x;
					PosY = y;
				}
			}
			else
			{
				SetVisible(false);
			
				DrawOnMove(pScreen, x, y, 0, 0);
				PosX = x;
				PosY = y;
			
				SetVisible(true);
			}
		}
		else
		{
			PosX = x;
			PosY = y;
		}
	}
}
*/

//////////////////////////////////////////////////////////////////////////////////////////////
static int _Factories;
static GApplicatorFactory *_Factory[16];

GApp8 Factory8;
GApp15 Factory15;
GApp16 Factory16;
GApp24 Factory24;
GApp32 Factory32;
GAlphaFactory FactoryAlpha;

GApplicatorFactory::GApplicatorFactory()
{
	LgiAssert(_Factories >= 0 AND _Factories < CountOf(_Factory));
	if (_Factories < CountOf(_Factory) - 1)
	{
		_Factory[_Factories++] = this;
	}
}

GApplicatorFactory::~GApplicatorFactory()
{
	LgiAssert(_Factories >= 0 AND _Factories < CountOf(_Factory));
	for (int i=0; i<_Factories; i++)
	{
		if (_Factory[i] == this)
		{
			_Factory[i] = _Factory[_Factories-1];
			_Factories--;
			break;
		}
	}
}

GApplicator *GApplicatorFactory::NewApp(int Bits, int Op)
{
	LgiAssert(_Factories >= 0 AND _Factories < CountOf(_Factory));
	for (int i=0; i<_Factories; i++)
	{
		GApplicator *a = _Factory[i]->Create(Bits, Op);
		if (a) return a;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
GSurface *GInlineBmp::Create()
{
	GSurface *pDC = new GMemDC;
	if (pDC->Create(X, Y, Bits))
	{
		int Line = X * Bits / 8;
		for (int y=0; y<Y; y++)
		{
			memcpy((*pDC)[y], ((uchar*)Data) + (y * Line), Line);
		}
	}

	return pDC;
}
