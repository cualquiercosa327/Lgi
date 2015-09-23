/// \file
///	\author Matthew Allen
/// \created 24/2/1997
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Lgi.h"
#include "GdiLeak.h"
#include "GPalette.h"

#include <ApplicationServices/ApplicationServices.h>

#define LGI_RAD					(360/(2*LGI_PI))

GPalette::GPalette()
{
	Data = 0;
	Lut = 0;
}

GPalette::~GPalette()
{
	DeleteArray(Lut);
	DeleteArray(Data);
}

GPalette::GPalette(GPalette *pPal)
{
	Data = 0;
	Lut = 0;
	Set(pPal);
}

GPalette::GPalette(uchar *pPal, int s)
{
	Data = 0;
	Lut = 0;
	Set(pPal, s);
}

void GPalette::Set(int Idx, int r, int g, int b)
{
    GdcRGB *rgb = (*this)[Idx];
    if (rgb)
    {
        rgb->r = r;
        rgb->g = g;
        rgb->b = b;
    }
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

bool GPalette::Update()
{
	return false;
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

int GPalette::GetSize()
{
	return Size;
}

GdcRGB *GPalette::operator [](int i)
{
	return i >= 0 && i < Size && Data != 0 ? Data + i : 0;
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
			// GdcRGB *p = (*this)[0];

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
		ulong bestcolor = 0;
		ulong curdist;
		long rdist;
		long gdist;
		long bdist;

		for (int i = 0; i < GetSize(); i++)
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
		#ifndef MAC
		p->Flags = 0;
		#endif
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
				#ifndef MAC
				p->Flags = 0;
				#endif
				p++;
			}
		}
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

			sprintf(Buf, "JASC-PAL\r\n%4.4X\r\n%i\r\n", GetSize(), GetSize());
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

////////////////////////////////////////////////////////////////////////////////////////////////////
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
						r->r = R24(d->c[i].c24);
						r->g = G24(d->c[i].c24);
						r->b = B24(d->c[i].c24);
						#ifndef MAC
						r->Flags = 0; // PC_RESERVED;
						#endif
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
			return i;
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
	GdcDevice *Device;

	// Current mode info
	int ScrX;
	int ScrY;
	int ScrBits;
	GColourSpace Cs;

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
		pSysPal = 0;

		// Get the screen size/rez
		CGDirectDisplayID ScreenId = CGMainDisplayID();
		CGRect r = CGDisplayBounds(ScreenId);
		ScrX = (int)r.size.width;
		ScrY = (int)r.size.height;
        
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
        
        CFStringRef pixEnc = CGDisplayModeCopyPixelEncoding(mode);
        if (CFStringCompare(pixEnc,
							CFSTR(IO32BitDirectPixels),
							kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
            ScrBits = 32;
			Cs = System32BitColourSpace;
		}
        else if (CFStringCompare(pixEnc,
								CFSTR(IO16BitDirectPixels),
								kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
            ScrBits = 16;
			Cs = System16BitColourSpace;
		}
        else if (CFStringCompare(pixEnc,
								CFSTR(IO8BitIndexedPixels),
								kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
            ScrBits = 8;
			Cs = CsIndex8;
		}
        else
		{
            LgiAssert(0);
		}
		
		char PixEnc[256];
		if (CFStringGetCString(pixEnc, PixEnc, sizeof(PixEnc), kCFStringEncodingUTF8))
		{
			printf("Cs = '%s'\n", PixEnc);
		}
        
		// Palette information
		GammaCorrection = 1.0;

		// Options
		ZeroObj(OptVal);
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
	GColourSpaceTest();

	pInstance = this;
	d = new GdcDevicePrivate(this);
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

int GdcDevice::GetBits()
{
	return d->ScrBits;
}

GColourSpace GdcDevice::GetColourSpace()
{
	return d->Cs;
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
	// bool SetOpt = true;

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
	COLOUR C = 0;

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

////////////////////////////////////////////////////////////////////////////////////////
/*
GSurface *GInlineBmp::Create()
{
	GSurface *pDC = new GMemDC;
	if (pDC->Create(X,
					Y,
					#ifdef MAC
					32
					#else
					Bits
					#endif
					))
	{
		int Line = X * Bits / 8;
		for (int y=0; y<Y; y++)
		{
			#ifdef MAC
			switch (Bits)
			{
				case 16:
				{
					uint32 *s = (uint32*) ( ((uchar*)Data) + (y * Line) );
					System32BitPixel *d = (System32BitPixel*) (*pDC)[y];
					System32BitPixel *e = d + X;
					while (d < e)
					{
						uint32 n = LgiSwap32(*s);
						s++;
						
						uint16 a = n >> 16;
						a = LgiSwap16(a);
						d->r = Rc16(a);
						d->g = Gc16(a);
						d->b = Bc16(a);
						d->a = 255;
						d++;
						
						if (d >= e)
							break;

						uint16 b = n & 0xffff;
						b = LgiSwap16(b);
						d->r = Rc16(b);
						d->g = Gc16(b);
						d->b = Bc16(b);
						d->a = 255;
						d++;
					}
					break;
				}
				case 32:
				{
					memcpy((*pDC)[y], ((uchar*)Data) + (y * Line), Line);
					break;
				}
			}
			#else				
			memcpy((*pDC)[y], ((uchar*)Data) + (y * Line), Line);
			#endif
		}
	}

	return pDC;
}
*/
