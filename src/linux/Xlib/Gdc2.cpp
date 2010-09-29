/*
**	FILE:			Gdc2.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			9/10/2000
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

/****************************** Defines *************************************************************************************/

#define LGI_PI				3.141592654
#define LGI_RAD				(360/(2*LGI_PI))

/****************************** Globals *************************************************************************************/
int Pixel24::Size = 3;
int Pixel32::Size = 4;

/****************************** Helper Functions ****************************************************************************/
COLOUR RgbToHls(COLOUR Rgb24)
{
	int nMax;
	int nMin;
	int R = R24(Rgb24);
	int G = G24(Rgb24);
	int B = B24(Rgb24);
	int H, L, S;
	bool Undefined = false;

	nMax = max(R, max(G, B));
	nMin = min(R, min(G, B));
	L = (nMax + nMin) / 2;

	if (nMax == nMin)
	{
		H = HUE_UNDEFINED;
		S = 0;
		Undefined = true;
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

GSurface *ConvertDC(GSurface *pDC, int Bits)
{
	GSurface *pNew = new GMemDC;
	if (pNew AND pNew->Create(pDC->X(), pDC->Y(), Bits))
	{
		pNew->Blt(0, 0, pDC);
		DeleteObj(pDC);
		return pNew;
	}
	return pDC;
}

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
				if (NOT Pal OR NOT (p = (*Pal)[c]))
				{
					Grey.R = Grey.G = Grey.B = c & 0xFF;
					p = &Grey;
				}

				switch (DstBits)
				{
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
				int R = R15(c);
				int G = G15(c);
				int B = B15(c);

				R = (R << 3) | (R >> 2);
				G = (G << 3) | (G >> 2);
				B = (B << 3) | (B >> 2);

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
				int G = ((c>>3)&0xFC) | ((c>>9)&0x3);
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
						return Rgb16To15(c);
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
							printf("Error: CBit 24->8 with no palette.\n");
							return 0;
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

COLOUR GdcGreyScale(COLOUR c, int Bits)
{
	COLOUR c24 = CBit(24, c, Bits);

	int r = (R24(c24) * 76) / 255;
	int g = (G24(c24) * 150) / 255;
	int b = (B24(c24) * 29) / 255;

	return r + g + b;
}

COLOUR GdcMixColour(COLOUR c1, COLOUR c2, double HowMuchC1)
{
	double HowMuchC2 = 1.0 - HowMuchC1;
	int r = (int) ((R24(c1)*HowMuchC1) + (R24(c2)*HowMuchC2));
	int g = (int) ((G24(c1)*HowMuchC1) + (G24(c2)*HowMuchC2));
	int b = (int) ((B24(c1)*HowMuchC1) + (B24(c2)*HowMuchC2));
	return Rgb24(r, g, b);
}

/****************************** Classes *************************************************************************************/
GPalette::GPalette()
{
	Data = 0;
	Size = 0;
}

GPalette::GPalette(GPalette *pPal)
{
	Size = 0;
	Data = 0;
	Set(pPal);
}

GPalette::GPalette(uchar *pPal, int s)
{
	Size = 0;
	Data = 0;
	Set(pPal, s);
}

GPalette::~GPalette()
{
	DeleteArray(Data);
	Size = 0;
}

int GPalette::GetSize()
{
	return Size;
}

GdcRGB *GPalette::operator[](int i)
{
	return (i>=0 AND i<Size) ? Data + i : 0;
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
				Data[i].R = *pPal++;
				Data[i].G = *pPal++;
				Data[i].B = *pPal++;
			}
		}

		Size = s;
	}
}

bool GPalette::Update()
{
	return true;
}

bool GPalette::SetSize(int s)
{
	GdcRGB *New = new GdcRGB[s];
	if (New)
	{
		memset(New, 0, s * sizeof(GdcRGB));
		if (Data)
		{
			memcpy(New, Data, min(s, Size)*sizeof(GdcRGB));
		}

		DeleteArray(Data);
		Data = New;
		Size = s;
		return true;
	}

	return false;
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
	SetSize(256);
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

	for (int i=216; i<256; i++)
	{
		(*this)[i]->R = 0;
		(*this)[i]->G = 0;
		(*this)[i]->B = 0;
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
	bool Status = false;
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
		for (int i=0; i<GetSize() AND NOT F.Eof(); i++)
		{
			F.ReadStr(Buf, sizeof(Buf));
			GdcRGB *p = (*this)[i];
			if (p)
			{
				p->R = atoi(strtok(Buf, " "));
				p->G = atoi(strtok(NULL, " "));
				p->B = atoi(strtok(NULL, " "));

			}

			Status = true;
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
	bool Status = false;

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
				return false;
			}

			a++;
			b++;
		}

		return true;
	}

	return false;
}

bool GPalette::operator !=(GPalette &p)
{
	return NOT ((*this) == p);
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
	DrawOnAlpha(false);

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
	DrawOnAlpha(false);

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
	else
	{
		printf("Error: GDeviceContext::CreateApplicator(%i,%i) failed.\n", Op, Bits);
	}

	return pA;
}

bool GDeviceContext::Applicator(GApplicator *pApplicator)
{
	bool Status = false;

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

		Status = true;
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
		Clip.x1 = max(0, Rgn->x1 - OriginX);
		Clip.y1 = max(0, Rgn->y1 - OriginY);
		Clip.x2 = min(X()-1, Rgn->x2 - OriginX);
		Clip.y2 = min(Y()-1, Rgn->y2 - OriginY);
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

////////////////////////////////////////////////////////////////////////////////////////////
class GdcDevicePrivate
#ifdef LINUX
	: public XObject
#endif
{
public:
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
	#endif

	GdcDevicePrivate(GdcDevice *d)
	#ifdef WIN32
	 : MsImg("msimg32")
	#endif
	{
		Device = d;
		GlobalColour = 0;

		// Palette information
		GammaCorrection = 1.0;

		#if defined WIN32
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
		#elif defined LINUX
		// Get mode stuff
		ScrX = 1024;
		ScrY = 768;
		ScrBits = 16;

		XWidget *dt = LgiApp->desktop();
		if (dt)
		{
			ScrX = dt->width();
			ScrY = dt->height();
		}

		XWindowAttributes a;
		XGetWindowAttributes(XDisplay(), RootWindow(XDisplay(), 0), &a);
		ScrBits = a.depth;
		// printf("Screen: %i x %i @ %i bpp\n", ScrX, ScrY, ScrBits);

		int Colours = 1 << ScrBits;
		pSysPal = (ScrBits <= 8) ? new GPalette(0, Colours) : 0;
		if (pSysPal)
		{
		}

		// Work out the size of a 24bit pixel
		Pixel24::Size = 4; // a decent default
		char *Data =(char*) malloc(4);
		XImage *Img = XCreateImage(XDisplay(), DefaultVisual(XDisplay(), 0), 24, ZPixmap, 0, Data, 1, 1, 32, 4);
		if (Img)
		{
			Pixel24::Size = Img->bits_per_pixel >> 3;
			XDestroyImage(Img);
		}
		// printf("Pixel24Size=%i\n", Pixel24Size);
		OptVal[GDC_PROMOTE_ON_LOAD] = ScrBits;
		#endif

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
	d = new GdcDevicePrivate(this);
	pInstance = this;
}

GdcDevice::~GdcDevice()
{
	// delete stuff
	pInstance = NULL;
	DeleteObj(d);
}

int GdcDevice::GetOption(int Opt)
{
	if (Opt >= 0 AND Opt < GDC_MAX_OPTION)
	{
		return d->OptVal[Opt];
	}

	LgiAssert(0);
	return 0;
}

int GdcDevice::SetOption(int Opt, int Value)
{
	int Prev = d->OptVal[Opt];
	if (Opt >= 0 AND Opt < GDC_MAX_OPTION)
	{
		d->OptVal[Opt] = Value;
	}
	else
	{
		LgiAssert(0);
	}
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
	bool SetOpt = true;

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
			SetOpt = false;
		}
	}

	if (SetOpt)
	{
		SetOption(GDC_PALETTE_TYPE, Type);
	}
	*/
}

COLOUR GdcDevice::GetColour(COLOUR Rgb24, GDeviceContext *pDC)
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
						(*d->pSysPal)[Current]->Set(R24(Rgb24), G24(Rgb24), B24(Rgb24));
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

GSprite::GSprite()
{
	Sx = Sy = Bits = 0;
	PosX = PosY = 0;
	HotX = HotY = 0;
	Visible = false;
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
	bool Status = false;

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

					Status = true;
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
	bool Status = false;

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

			Status = true;
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
	bool Status = false;

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

					Status = true;
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

					Status = true;
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

					Status = true;
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

					Status = true;
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
	bool Status = false;
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

				Status = true;
			}
		}
	}

	return Status;
};

void GSprite::Delete()
{
	SetVisible(false);
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
				Visible = false;
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
				Visible = true;
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
			if (NOT c[i].Used)
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
