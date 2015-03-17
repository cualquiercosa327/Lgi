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
#include <math.h>

#include "Lgi.h"
#include "GdiLeak.h"
#include "GPalette.h"

/****************************** Defines *************************************************************************************/

#define LGI_RAD					(360/(2*LGI_PI))

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
		System32BitPixel *c = (System32BitPixel*) &bits[y * bm.bmWidthBytes];
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
	hPal = NULL;
	Lut = 0;
}

GPalette::GPalette(GPalette *pPal)
{
	Data = 0;
	hPal = NULL;
	Lut = 0;
	Set(pPal);
}

GPalette::GPalette(uchar *pPal, int s)
{
	Data = 0;
	hPal = NULL;
	Lut = 0;
	if (pPal || s > 0)
		Set(pPal, s);
}

GPalette::~GPalette()
{
	DeleteArray(Lut);
	DeleteArray(Data);
	if (hPal) DeleteObject(hPal);
}

void GPalette::Set(GPalette *pPal)
{
    if (pPal == this)
        return;

	DeleteArray(Data);
	if (hPal) DeleteObject(hPal);

	if (pPal && pPal->Data)
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

void GPalette::Set(int Index, int r, int g, int b)
{
	GdcRGB *rgb = (*this)[Index];
	if (rgb)
	{
		rgb->r = r;
		rgb->g = g;
		rgb->b = b;
	}
}

bool GPalette::Update()
{
	if (Data && hPal)
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

	return (Data != 0) && (hPal != 0);
}

int GPalette::GetSize()
{
	return (Data) ? Data->palNumEntries : 0;
}

GdcRGB *GPalette::operator [](int i)
{
	return (i >= 0 && i < GetSize() && Data) ? (GdcRGB*) (Data->palPalEntry + i) : 0;
}

void GPalette::SwapRAndB()
{
	if (Data)
	{
		for (int i=0; i<GetSize(); i++)
		{
			uchar n = (*this)[i]->r;
			(*this)[i]->r = (*this)[i]->b;
			(*this)[i]->b = n;
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
			rdist = Entry[i].r - r;
			gdist = Entry[i].g - g;
			bdist = Entry[i].b - b;
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
		p->r = i;
		p->g = i;
		p->b = i;
		p->a = 0;
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
				p->r = r * 51;
				p->g = g * 51;
				p->b = b * 51;
				p->a = 0;
				p++;
			}
		}
	}
	
	bool b = Update();
	LgiAssert(b);
}


void TrimWhite(char *s)
{
	char *White = " \r\n\t";
	char *c = s;
	while (*c && strchr(White, *c)) c++;
	if (c != s)
	{
		strcpy(s, c);
	}

	c = s + strlen(s) - 1;
	while (c > s && strchr(White, *c))
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
		for (int i=0; i<GetSize() && !F.Eof(); i++)
		{
			F.ReadStr(Buf, sizeof(Buf));
			GdcRGB *p = (*this)[i];
			if (p)
			{
				p->r = atoi(strtok(Buf, " "));
				p->g = atoi(strtok(NULL, " "));
				p->b = atoi(strtok(NULL, " "));

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

			sprintf_s(Buf, sizeof(Buf), "JASC-PAL\r\n%04.4X\r\n%i\r\n", GetSize(), GetSize());
			F.Write(Buf, strlen(Buf));

			for (int i=0; i<GetSize(); i++)
			{
				GdcRGB *p = (*this)[i];
				if (p)
				{
					sprintf_s(Buf, sizeof(Buf), "%i %i %i\r\n", p->r, p->g, p->b);
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
			if (a->r != b->r ||
				a->g != b->g ||
				a->b != b->b)
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
	Cs = CsNone;
}

GBmpMem::~GBmpMem()
{
	if (Base && (Flags & GDC_OWN_MEMORY))
	{
		delete [] Base;
	}
}

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
			s = new GMemDC(il->X(), il->Y(), System24BitColourSpace);
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
					if (i < 10 || i > 246)
					{
						r->R = i;
						r->G = 0;
						r->B = 0;
						r->Flags = PC_EXPLICIT;
					}
					else
					{
					*/
						r->r = R24(d->c[i].c24);
						r->g = G24(d->c[i].c24);
						r->b = B24(d->c[i].c24);
						r->a = 0; // PC_RESERVED;
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
		if (d->c[i].Used &&
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
	GColourSpace ColourSpace;

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
		switch (ScrBits)
		{
			case 8:
				ColourSpace = CsIndex8;
				break;
			case 15:
				ColourSpace = System15BitColourSpace;
				break;
			case 16:
				ColourSpace = System16BitColourSpace;
				break;
			case 24:
				ColourSpace = System24BitColourSpace;
				break;
			case 32:
				ColourSpace = System32BitColourSpace;
				break;
			default:
				LgiAssert(!"Unknown colour space.");
				ColourSpace = CsNone;
				break;
		}

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
					p[i].r = pal[i].peRed;
					p[i].g = pal[i].peGreen;
					p[i].b = pal[i].peBlue;
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
	return (Opt >= 0 && Opt < GDC_MAX_OPTION) ? d->OptVal[Opt] : 0;
}

int GdcDevice::SetOption(int Opt, int Value)
{
	int Prev = d->OptVal[Opt];
	if (Opt >= 0 && Opt < GDC_MAX_OPTION)
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

GColourSpace GdcDevice::GetColourSpace()
{
	return d->ColourSpace;
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
						GdcRGB *n = (*d->pSysPal)[Current];						
						n->r = R24(Rgb24);
						n->g = G24(Rgb24);
						n->b = B24(Rgb24);
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
	LgiAssert(_Factories >= 0 && _Factories < CountOf(_Factory));
	if (_Factories < CountOf(_Factory) - 1)
	{
		_Factory[_Factories++] = this;
	}
}

GApplicatorFactory::~GApplicatorFactory()
{
	LgiAssert(_Factories >= 0 && _Factories < CountOf(_Factory));
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

GApplicator *GApplicatorFactory::NewApp(GColourSpace Cs, int Op)
{
	LgiAssert(_Factories >= 0 && _Factories < CountOf(_Factory));
	for (int i=0; i<_Factories; i++)
	{
		GApplicator *a = _Factory[i]->Create(Cs, Op);
		if (a) return a;
	}

	return 0;
}
