/*
**	FILE:		Gdc2.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		24/2/97
**	DESCRIPTION:	GDC v2.xx
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

///////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

///////////////////////////////////////////////////////////////////////
#define RAD					(360/(2*LGI_PI))

///////////////////////////////////////////////////////////////////////
COLOUR RgbToHls(COLOUR Rgb24)
{
	int nMax;
	int nMin;
	int R = R24(Rgb24);
	int G = G24(Rgb24);
	int B = B24(Rgb24);
	int H, L, S;
	bool Undefined = FALSE;

	nMax = max(R, max(G, B));
	nMin = min(R, min(G, B));
	L = (nMax + nMin) / 2;

	if (nMax == nMin)
	{
		H = HUE_UNDEFINED;
		S = 0;
		Undefined = TRUE;
	}
	else
	{
		double fHue;
		int nDelta;

		if (L < 128)
		{
			S = (uchar) ((255.0 * ((double)(nMax - nMin)) / (double)(nMax + nMin)) + 0.5);
		}
		else
		{
			S = (uchar) ((255.0 * ((double)(nMax - nMin)) / (double)(511 - nMax - nMin)) + 0.5);
		}
	
		nDelta = nMax - nMin;
	
		if (R == nMax)
		{
			fHue = ((double) (G - B)) / (double) nDelta;
		}
		else if (G == nMax)
		{
			fHue = 2.0 + ((double) (B - R)) / (double) nDelta;
		}
		else
		{
			fHue = 4.0 + ((double) (R - G)) / (double) nDelta;
		}
	
		fHue *= 60;
		
		if (fHue < 0.0)
		{
			fHue += 360.0;
		}
	
		H = (short) (fHue + 0.5);
	}

	return Hls32(H, L, S);
}

extern int HlsValue(double fN1, double fN2, double fHue);
int HlsValue(double fN1, double fN2, double fHue)
{
	int nValue;
	
	if (fHue > 360.0)
	{
		fHue -= 360.0;
	}
	else if (fHue < 0.0)
	{
		fHue += 360.0;
	}

	if (fHue < 60.0)
	{
		nValue = (int) ((fN1 + (fN2 - fN1) * fHue / 60.0) * 255.0 + 0.5);
	}
	else if (fHue < 180.0)
	{
		nValue = (int) ((fN2 * 255.0) + 0.5);
	}
	else if (fHue < 240.0)
	{
		nValue = (int) ((fN1 + (fN2 - fN1) * (240.0 - fHue) / 60.0) * 255.0 + 0.5);
	}
	else
	{
		nValue = (int) ((fN1 * 255.0) + 0.5);
	}

	return (nValue);
}

COLOUR HlsToRgb(COLOUR Hls)
{
	int H = H32(Hls);
	int L = L32(Hls);
	int S = S32(Hls);
	int R, G, B;

	if (S == 0)
	{
		R = 0;
		G = 0;
		B = 0;
	}
	else
	{
		double fM1;
		double fM2;
		double fHue;
		double fLightness;
		double fSaturation;

		while (H >= 360)
		{
			H -= 360;
		}
	
		while (H < 0)
		{
			H += 360;
		}
	
		fHue = (double) H;
		fLightness = ((double) L) / 255.0;
		fSaturation = ((double) S) / 255.0;
	
		if (L < 128)
		{
			fM2 = fLightness * (1 + fSaturation);
		}
		else
		{
			fM2 = fLightness + fSaturation - (fLightness * fSaturation);
		}
	
		fM1 = 2.0 * fLightness - fM2;

		R = HlsValue(fM1, fM2, fHue + 120.0);
		G = HlsValue(fM1, fM2, fHue);
		B = HlsValue(fM1, fM2, fHue - 120.0);
	}

	return Rgb24(R, G, B);
}

COLOUR GdcMixColour(COLOUR c1, COLOUR c2, double HowMuchC1)
{
	double HowMuchC2 = 1.0 - HowMuchC1;
	int r = (R24(c1)*HowMuchC1) + (R24(c2)*HowMuchC2);
	int g = (G24(c1)*HowMuchC1) + (G24(c2)*HowMuchC2);
	int b = (B24(c1)*HowMuchC1) + (B24(c2)*HowMuchC2);
	return Rgb24(r, g, b);
}

/*
COLOUR CBit(int DstBits, COLOUR c, int SrcBits, GPalette *Pal)
{
	if (SrcBits == DstBits)
	{
		return c;
	}
	else
	{
		COLOUR c24 = 0;

		switch (SrcBits)
		{
			case 8:
			{
				GdcRGB Grey, *p = 0;
				if (!Pal || !(p = (*Pal)[c]))
				{
					Grey.R = Grey.G = Grey.B = c & 0xFF;
					p = &Grey;
				}

				switch (DstBits)
				{
					case 15:
					{
						return Rgb15(p->R, p->G, p->B);
					}
					case 16:
					{
						return Rgb16(p->R, p->G, p->B);
					}
					case 24:
					{
						return Rgb24(p->R, p->G, p->B);
					}
					case 32:
					{
						return Rgb32(p->R, p->G, p->B);
					}
				}
				break;
			}
			
			case 15:
			{
				int R = ((c>>7)&0xF8) | ((c>>12)&0x7);
				int G = ((c>>2)&0xF8) | ((c>>8)&0x7);
				int B = ((c<<3)&0xF8) | ((c>>2)&0x7);

				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(Rgb24(R, G, B));
						}
						break;
					}
					case 16:
					{
						return Rgb16(R, G, B);
					}
					case 24:
					{
						return Rgb24(R, G, B);
					}
					case 32:
					{
						return Rgb32(R, G, B);
					}
				}
				break;
			}

			case 16:
			{
				int R = ((c>>8)&0xF8) | ((c>>13)&0x7);
				int G = ((c>>3)&0xFC) | ((c>>8)&0x3);
				int B = ((c<<3)&0xF8) | ((c>>2)&0x7);

				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(Rgb24(R, G, B));
						}
						break;
					}
					case 15:
					{
						return Rgb15(R, G, B);
					}
					case 24:
					{
						return Rgb24(R, G, B);
					}
					case 32:
					{
						return Rgb32(R, G, B);
					}
				}
				break;
			}

			case 24:
			{
				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(c);
						}
						else
						{
							color_map *CMap = (color_map*) system_colors();
							if (CMap)
							{
								return CMap->index_map[Rgb24To15(c)];
							}
						}
						break;
					}
					case 15:
					{
						return Rgb24To15(c);
					}
					case 16:
					{
						return Rgb24To16(c);
					}
					case 32:
					{
						return Rgb24To32(c);
					}
				}
				break;
			}

			case 32:
			{
				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(Rgb32To24(c));
						}
						break;
					}
					case 15:
					{
						return Rgb32To15(c);
					}
					case 16:
					{
						return Rgb32To16(c);
					}
					case 24:
					{
						return Rgb32To24(c);
					}
				}
				break;
			}
		}
	}

	return c;
}
*/

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

/****************************** Classes *************************************************************************************/
GPalette::GPalette()
{
	Data = 0;
	Size = 0;
}

GPalette::~GPalette()
{
	DeleteArray(Data);
	Size = 0;
}

GPalette::GPalette(GPalette *pPal)
{
	Size = 0;
	Data = 0;
	Set(pPal);
}

GPalette::GPalette(uchar *pPal, int s = 256)
{
	Size = 0;
	Data = 0;
	Set(pPal, s);
}

void GPalette::Set(GPalette *pPal)
{
	DeleteArray(Data);
	Size = 0;
	if (pPal)
	{
		if (pPal->Data)
		{
			Data = new GdcRGB[pPal->Size];
			if (Data)
			{
				memcpy(Data, pPal->Data, sizeof(GdcRGB) * pPal->Size);
			}
		}
		
		Size = pPal->Size;
	}
}

void GPalette::Set(uchar *pPal, int s)
{
	DeleteArray(Data);
	Size = 0;
	Data = new GdcRGB[s];
	if (Data)
	{
		if (pPal)
		{
			for (int i=0; i<s; i++)
			{
				Data[i].r = *pPal++;
				Data[i].g = *pPal++;
				Data[i].b = *pPal++;
			}
		}

		Size = s;
	}
}

int GPalette::GetSize()
{
	return Size;

}

GdcRGB *GPalette::operator[](int i)
{
	return (i>=0 && i<Size) ? Data + i : 0;
}

bool GPalette::Update()
{
	return TRUE;
}

bool GPalette::SetSize(int s)
{
	int Len = sizeof(GdcRGB) * s;
	Data = new GdcRGB[Len];
	if (Data)
	{
		Size = s;
		for (int i=0; i<GetSize(); i++)
		{
			(*this)[i]->r = 0;
			(*this)[i]->g = 0;
			(*this)[i]->b = 0;
		}
	}

	return (Data != 0);
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
	uchar *Lut = 0;
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

	return Lut;
}

int GPalette::MatchRgb(COLOUR Rgb)
{
	if (Data)
	{
		GdcRGB *Entry = (*this)[0];
		int r = (R24(Rgb) & 0xF8) + 4;
		int g = (G24(Rgb) & 0xF8) + 4;
		int b = (B24(Rgb) & 0xF8) + 4;
		ulong *squares = GdcD->GetCharSquares();
		ulong mindist = 200000;
		ulong bestcolor;
		ulong curdist;
		long rdist;
		long gdist;
		long bdist;

		for (int i = 0; i < Size; i++)
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
		p++;
	}
}

void GPalette::CreateCube()
{
	SetSize(256);
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
				p++;
			}
		}
	}

	for (int i=216; i<256; i++)
	{
		(*this)[i]->r = 0;
		(*this)[i]->g = 0;
		(*this)[i]->b = 0;
	}
}


extern void TrimWhite(char *s);
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

	if (Format == GDCPAL_JASC)
	{
		char Buf[256];

		strcpy(Buf, "JASC-PAL\r\n");
		F.Write(Buf, strlen(Buf));
		sprintf(Buf, "%04.4X\r\n%i\r\n", GetSize(), GetSize());
		F.Write(Buf, strlen(Buf));

		for (int i=0; i<GetSize(); i++)
		{
			GdcRGB *p = (*this)[i];
			if (p)
			{
				sprintf(Buf, "%i %i %i\r\n", p->r, p->g, p->b);
				F.Write(Buf, strlen(Buf));
			}
		}

		Status = TRUE;
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
}

GBmpMem::~GBmpMem()
{
	if (Base != NULL &&
		(Flags & BmpOwnMemory) != 0)
	{
		delete [] Base;
		Base = NULL;
	}
}

#include "GraphicsCard.h"

class GdcDevicePrivate
{
public:
	GdcDevice *Device;

	// Current mode info
	int ScrX;
	int ScrY;
	int ScrBits;
	GColourSpace ScrCs;

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

	GdcDevicePrivate(GdcDevice *d)
	{
		Device = d;
		GlobalColour = 0;
		ZeroObj(OptVal);

		// Palette information
		GammaCorrection = 1.0;
		ScrCs = CsNone;

		// Get mode stuff
		{
			BScreen Scr;
			BRect r = Scr.Frame();
		
			ScrX = r.right - r.left + 1;
			ScrY = r.bottom - r.top + 1;
			switch (Scr.ColorSpace())
			{
				case B_RGB32:
				case B_RGBA32:
				case B_RGB32_BIG:
				case B_RGBA32_BIG:
				{
					ScrBits = 32;
					ScrCs = CsRgba32;
					break;
				}
				case B_RGB24:
				case B_RGB24_BIG:
				{
					ScrBits = 24;
					ScrCs = CsRgb24;
					break;
				}
				case B_RGB16:
				case B_RGB16_BIG:
				{
					ScrBits = 16;
					ScrCs = CsRgb16;
					break;
				}
				case B_RGB15:
				case B_RGBA15:
				case B_RGB15_BIG:
				case B_RGBA15_BIG:
				{
					ScrBits = 16;
					ScrCs = CsRgb15;
					break;
				}
				case B_CMAP8:
				case B_GRAY8:
				{
					ScrBits = 8;
					ScrCs = CsIndex8;
					break;
				}
				case B_GRAY1:
				{
					ScrBits = 1;
					ScrCs = CsIndex1;
					break;
				}
			}
		}

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

GdcDevice *GdcDevice::pInstance = 0;

GdcDevice::GdcDevice()
{
	GColourSpaceTest();
	pInstance = this;
	d = new GdcDevicePrivate(this);
}

GdcDevice::~GdcDevice()
{
	pInstance = NULL;
	DeleteObj(d);
}

ulong *GdcDevice::GetCharSquares()
{
	return d->CharSquareData;
}

uchar *GdcDevice::GetDiv255()
{
	return d->Div255;
}

int GdcDevice::GetOption(int Opt)
{
	if (Opt >= 0 && Opt < GDC_MAX_OPTION)
	{
		return d->OptVal[Opt];
	}

	LgiAssert(0);
	return 0;
}

int GdcDevice::SetOption(int Opt, int Value)
{
	int Prev = d->OptVal[Opt];
	if (Opt >= 0 && Opt < GDC_MAX_OPTION)
	{
		d->OptVal[Opt] = Value;
	}
	else
	{
		LgiAssert(0);
	}
	return Prev;
}

GColourSpace GdcDevice::GetColourSpace()
{
	return d->ScrCs;
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

GGlobalColour *GdcDevice::GetGlobalColour()
{
	return d->GlobalColour;
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
}

GPalette *GdcDevice::GetSystemPalette()
{
	return d->pSysPal;
}

void GdcDevice::SetColourPaletteType(int Type)
{
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
					else
					{
						GdcRGB *n = (*d->pSysPal)[Current];
						n->r = R24(Rgb24);
						n->g = G24(Rgb24);
						n->b = B24(Rgb24);
						C = Current++;
						if (Current == 255) Current = 1;
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	return c24;
}

bool GGlobalColour::AddBitmap(GSurface *pDC)
{
	return false;
}

bool GGlobalColour::AddBitmap(GImageList *il)
{
	return false;
}

bool GGlobalColour::MakeGlobalPalette()
{
	return 0;
}

GPalette *GGlobalColour::GetPalette()
{
	return 0;
}

COLOUR GGlobalColour::GetColour(COLOUR c24)
{
	return c24;
}

bool GGlobalColour::RemapBitmap(GSurface *pDC)
{
	return false;
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
