
/// \file
/// \author Matthew Allen, fret@memecode.com
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Lgi.h"
#include "GPalette.h"

#define LGI_PI				3.141592654
#define LGI_RAD				(360/(2*LGI_PI))

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
	return (i>=0 && i<Size) ? Data + i : 0;
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
		p->a = 0;
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
				p->a = 0;
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


void TrimWhite(char *s)
{
	const char *White = " \r\n\t";
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
					sprintf(Buf, "%i %i %i\r\n", p->r, p->g, p->b);
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
	if (Base && (Flags & GDC_OWN_MEMORY))
	{
		delete [] Base;
	}
}

struct PfComponent
{
	GComponentType Type;
	int Bits;
	int Pos;
	
	void Set(GComponentType type, uint32 Mask, int Loss)
	{
		Type = type;
		Bits = 0;
		Pos = 0;
		while (Mask)
		{
			if (Mask & 1)
				Bits++;
			else
				Pos++;
			Mask >>= 1;
		}
		
		if (Loss > 0)
		{
			Type = CtPad;
			Bits = Loss;
		}
	}
};

int ComponenetCmp(PfComponent *a, PfComponent *b)
{
	if (a->Type == CtPad ^ b->Type == CtPad)
		return (b->Type == CtPad) - (a->Type == CtPad);
	return (b->Pos + b->Bits) - (a->Pos + a->Bits);
}

GColourSpace PixelFormat2ColourSapce(SDL_PixelFormat *pf)
{
	GColourSpaceBits cs;
	
	cs.All = 0;
	cs.Bits[0].Type = 1;
	cs.Bits[0].Size = 2;
	cs.Bits[1].Type = 3;
	cs.Bits[1].Size = 4;
	bool Reverse = cs.All == 0x00003412;
	
	if (pf->BytesPerPixel <= 1)
	{
		cs.Bits[0].Type = CtIndex;
		cs.Bits[0].Size = pf->BitsPerPixel;
	}
	else
	{	
		GArray<PfComponent> a;
		a.New().Set(CtRed, pf->Rmask, pf->Rloss);
		a.New().Set(CtGreen, pf->Gmask, pf->Gloss);
		a.New().Set(CtBlue, pf->Bmask, pf->Bloss);
		a.New().Set(CtAlpha, pf->Amask, pf->Aloss);
		a.Sort(ComponenetCmp);	

		for (unsigned i=0; i<a.Length(); i++)
		{
			int idx = Reverse ? 3 - i : i;
			cs.Bits[idx].Type = a[i].Type;
			cs.Bits[idx].Size = a[i].Bits;
		}
	}
	
	return (GColourSpace)cs.All;
}

////////////////////////////////////////////////////////////////////////////////////////////
class GdcDevicePrivate
{
public:
	GdcDevice *Device;
	SDL_Surface *Screen;

	// Current mode info
	int ScrX;
	int ScrY;
	int ScrBits;
	GColourSpace ScrColourSpace;

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
		const SDL_VideoInfo *vi = SDL_GetVideoInfo();
		
		if ((Screen = SDL_SetVideoMode(320, 240, 0, SDL_DOUBLEBUF)) == NULL)
			LgiTrace("%s:%i - SDL_SetVideoMode failed.\n", _FL);

		Device = d;
		GlobalColour = 0;
	    	ZeroObj(OptVal);
	    	ScrX = ScrY = 0;
	    	ScrBits = 0;

		// Palette information
		GammaCorrection = 1.0;

		// Get mode stuff
		ScrX = Screen ? Screen->w : 0;
		ScrY = Screen ? Screen->h : 0;
		ScrBits = Screen ? Screen->format->BitsPerPixel : 0;
		ScrColourSpace = Screen ? PixelFormat2ColourSapce(Screen->format) : System32BitColourSpace;
		
		printf("Screen: %i x %i @ %i bpp (%s)\n", ScrX, ScrY, ScrBits, GColourSpaceToString(ScrColourSpace));
		
		// printf("Pixel24Size=%i\n", Pixel24Size);
		OptVal[GDC_PROMOTE_ON_LOAD] = ScrBits;

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

//////////////////////////////////////////////////////////////
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

SDL_Surface *GdcDevice::Handle()
{
	return d->Screen;
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
	return d->ScrColourSpace;
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
						GdcRGB *p = (*d->pSysPal)[Current];
						p->r = R24(Rgb24);
						p->g = G24(Rgb24);
						p->b = B24(Rgb24);
						
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

////////////////////////////////////////////////////////////////////////////////////////////
