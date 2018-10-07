#include "Lgi.h"
#include "GPalette.h"

const GColour GColour::Black(0, 0, 0);
const GColour GColour::White(255, 255, 255);
const GColour GColour::Red(255, 0, 0);
const GColour GColour::Green(0, 192, 0);
const GColour GColour::Blue(0, 0, 255);

GColour::GColour()
{
	space = CsNone;
	flat = 0;
	pal = NULL;
}

GColour::GColour(uint8 idx8, GPalette *palette)
{
	c8(idx8, palette);
}

GColour::GColour(int r, int g, int b, int a)
{
	c32(Rgba32(r, g, b, a));
}

GColour::GColour(uint32 c, int bits, GPalette *palette)
{
	Set(c, bits, palette);
}

int GColour::HlsValue(double fN1, double fN2, double fHue) const
{
	if (fHue > 360.0) fHue -= 360.0;
	else if (fHue < 0.0) fHue += 360.0;

	if (fHue < 60.0) return (int) ((fN1 + (fN2 - fN1) * fHue / 60.0) * 255.0 + 0.5);
	else if (fHue < 180.0) return (int) ((fN2 * 255.0) + 0.5);
	else if (fHue < 240.0) return (int) ((fN1 + (fN2 - fN1) * (240.0 - fHue) / 60.0) * 255.0 + 0.5);
	
	return (int) ((fN1 * 255.0) + 0.5);
}

GColourSpace GColour::GetColourSpace()
{
	return space;
}

bool GColour::IsValid()
{
	return space != CsNone;
}

void GColour::Empty()
{
	space = CsNone;
}

bool GColour::IsTransparent()
{
	if (space == System32BitColourSpace)
		return rgb.a == 0;
	else if (space == CsIndex8)
		return !pal || index >= pal->GetSize();
	return space == CsNone;
}

void GColour::Rgb(int r, int g, int b, int a)
{
	c32(Rgba32(r, g, b, a));
}

void GColour::Set(uint32 c, int bits, GPalette *palette)
{
	pal = 0;
	switch (bits)
	{
		case 8:
		{
			index = c;
			space = CsIndex8;
			pal = palette;
			break;
		}
		case 15:
		{
			space = System32BitColourSpace;
			rgb.r = Rc15(c);
			rgb.g = Gc15(c);
			rgb.b = Bc15(c);
			rgb.a = 255;
			break;
		}
		case 16:
		{
			space = System32BitColourSpace;
			rgb.r = Rc16(c);
			rgb.g = Gc16(c);
			rgb.b = Bc16(c);
			rgb.a = 255;
			break;
		}
		case 24:
		case 48:
		{
			space = System32BitColourSpace;
			rgb.r = R24(c);
			rgb.g = G24(c);
			rgb.b = B24(c);
			rgb.a = 255;
			break;
		}
		case 32:
		case 64:
		{
			space = System32BitColourSpace;
			rgb.r = R32(c);
			rgb.g = G32(c);
			rgb.b = B32(c);
			rgb.a = A32(c);
			break;
		}
		default:
		{
			space = System32BitColourSpace;
			flat = 0;
			LgiTrace("Error: Unable to set colour %x, %i\n", c, bits);
			LgiAssert(!"Not a known colour depth.");
		}
	}
}

uint32 GColour::Get(int bits)
{
	switch (bits)
	{
		case 8:
			if (space == CsIndex8)
				return index;
			LgiAssert(!"Not supported.");
			break;
		case 24:
			return c24();
		case 32:
			return c32();
	}
	
	return 0;
}

uint8 GColour::r() const
{
	return R32(c32());
}

uint8 GColour::g() const
{
	return G32(c32());
}

uint8 GColour::b() const
{
	return B32(c32());
}

uint8 GColour::a() const
{
	return A32(c32());
}

uint8 GColour::c8() const
{
	return index;
}

void GColour::c8(uint8 c, GPalette *p)
{
	space = CsIndex8;
	pal = p;
	index = c;
}

uint32 GColour::c24() const
{
	if (space == System32BitColourSpace)
	{
		return Rgb24(rgb.r, rgb.g, rgb.b);
	}
	else if (space == CsIndex8)
	{
		if (pal)
		{
			// colour palette lookup
			if (index < pal->GetSize())
			{
				GdcRGB *c = (*pal)[index];
				if (c)
				{
					return Rgb24(c->r, c->g, c->b);
				}
			}

			return 0;
		}
		
		return Rgb24(index, index, index); // monochome
	}
	else if (space == CsHls32)
	{
		return Rgb32To24(c32());
	}

	// Black...
	return 0;
}

void GColour::c24(uint32 c)
{
	space = System32BitColourSpace;
	rgb.r = R24(c);
	rgb.g = G24(c);
	rgb.b = B24(c);
	rgb.a = 255;
	pal = NULL;
}

uint32 GColour::c32() const
{
	if (space == System32BitColourSpace)
	{
		return Rgba32(rgb.r, rgb.g, rgb.b, rgb.a);
	}
	else if (space == CsIndex8)
	{
		if (pal)
		{
			// colour palette lookup
			if (index < pal->GetSize())
			{
				GdcRGB *c = (*pal)[index];
				if (c)
				{
					return Rgb32(c->r, c->g, c->b);
				}
			}

			return 0;
		}
		
		return Rgb32(index, index, index); // monochome
	}
	else if (space == CsHls32)
	{
		// Convert from HLS back to RGB
		GColour tmp = *this;
		tmp.ToRGB();
		return tmp.c32();
	}

	// Transparent?
	return 0;
}

void GColour::c32(uint32 c)
{
	space = System32BitColourSpace;
	pal = NULL;
	rgb.r = R32(c);
	rgb.g = G32(c);
	rgb.b = B32(c);
	rgb.a = A32(c);
}

GColour GColour::Mix(GColour Tint, float RatioOfTint) const
{
	COLOUR c1 = c32();
	COLOUR c2 = Tint.c32();
	
	double RatioThis = 1.0 - RatioOfTint;
	
	int r = (int) ((R32(c1) * RatioThis) + (R32(c2) * RatioOfTint));
	int g = (int) ((G32(c1) * RatioThis) + (G32(c2) * RatioOfTint));
	int b = (int) ((B32(c1) * RatioThis) + (B32(c2) * RatioOfTint));
	int a = (int) ((A32(c1) * RatioThis) + (A32(c2) * RatioOfTint));
	
	return GColour(r, g, b, a);
}

uint32 GColour::GetH()
{
	ToHLS();
	return hls.h;
}

bool GColour::HueIsUndefined()
{
	ToHLS();
	return hls.h == HUE_UNDEFINED;
}

uint32 GColour::GetL()
{
	ToHLS();
	return hls.l;
}

uint32 GColour::GetS()
{
	ToHLS();
	return hls.s;
}

bool GColour::ToHLS()
{
	if (space == CsHls32)
		return true;

	uint32 nMax, nMin, nDelta, c = c32();
	int R = R32(c), G = G32(c), B = B32(c);
	double fHue;

	nMax = MAX(R, MAX(G, B));
	nMin = MIN(R, MIN(G, B));

	if (nMax == nMin)
		return false;

	hls.l = (nMax + nMin) / 2;
	if (hls.l < 128)
		hls.s = (uchar) ((255.0 * ((double)(nMax - nMin)) / (double)(nMax + nMin)) + 0.5);
	else
		hls.s = (uchar) ((255.0 * ((double)(nMax - nMin)) / (double)(511 - nMax - nMin)) + 0.5);

	nDelta = nMax - nMin;

	if (R == nMax)
		fHue = ((double) (G - B)) / (double) nDelta;
	else if (G == nMax)
		fHue = 2.0 + ((double) (B - R)) / (double) nDelta;
	else
		fHue = 4.0 + ((double) (R - G)) / (double) nDelta;

	fHue *= 60;
	if (fHue < 0.0)
		fHue += 360.0;

	hls.h = (uint16) (fHue + 0.5);
	space = CsHls32;
	pal = NULL;
	return true;
}

void GColour::SetHLS(uint16 h, uint8 l, uint8 s)
{
	space = CsHls32;
	hls.h = h;
	hls.l = l;
	hls.s = s;
}

void GColour::ToRGB()
{
	GHls32 Hls = hls;
	if (Hls.s == 0)
	{
		Rgb(0, 0, 0);
	}
	else
	{
		while (Hls.h >= 360)
			Hls.h -= 360;			
		while (hls.h < 0)
			Hls.h += 360;
		
		double fHue = (double) Hls.h, fM1, fM2;
		double fLightness = ((double) Hls.l) / 255.0;
		double fSaturation = ((double) Hls.s) / 255.0;
		
		if (Hls.l < 128)
			fM2 = fLightness * (1 + fSaturation);
		else
			fM2 = fLightness + fSaturation - (fLightness * fSaturation);
		
		fM1 = 2.0 * fLightness - fM2;

		Rgb(HlsValue(fM1, fM2, fHue + 120.0),
			HlsValue(fM1, fM2, fHue),
			HlsValue(fM1, fM2, fHue - 120.0));
	}
}

int GColour::GetGray(int BitDepth)
{
	if (BitDepth == 8)
	{
		int R = r() * 77;
		int G = g() * 151;
		int B = b() * 28;
		return (R + G + B) >> 8;
	}
	
	double Scale = 1 << BitDepth;
	int R = (int) ((double) r() * (0.3 * Scale) + 0.5);
	int G = (int) ((double) g() * (0.59 * Scale) + 0.5);
	int B = (int) ((double) b() * (0.11 * Scale) + 0.5);
	return (R + G + B) >> BitDepth;
}

uint32 GColour::GetNative()
{
	#ifdef WIN32
	if (space == CsIndex8)
	{
		if (pal && index < pal->GetSize())
		{
			GdcRGB *c = (*pal)[index];
			if (c)
			{
				return RGB(c->r, c->g, c->b);
			}
		}
		
		return RGB(index, index, index);
	}
	else if (space == System32BitColourSpace)
	{
		return RGB(rgb.r, rgb.g, rgb.b);
	}
	else
	{
		LgiAssert(0);
	}
	#else
	LgiAssert(0);
	#endif		
	return c32();
}

char *GColour::GetStr()
{
	static char Buf[4][32];
	static int Idx = 0;
	int b = Idx++;
	if (Idx >= 4) Idx = 0;
	switch (space)
	{
		case System32BitColourSpace:
			if (rgb.a == 0xff)
			{
				sprintf_s(	Buf[b], 32,
							"rgb(%i,%i,%i)",
							rgb.r,
							rgb.g,
							rgb.b);
			}
			else
			{
				sprintf_s(	Buf[b], 32,
							"rgba(%i,%i,%i,%i)",
							rgb.r,
							rgb.g,
							rgb.b,
							rgb.a);
			}
			break;
		case CsIndex8:
			sprintf_s(	Buf[b], 32,
						"index(%i)",
						index);
			break;
		case CsHls32:
			sprintf_s(	Buf[b], 32,
						"hls(%i,%i,%i)",
						hls.h,
						hls.l,
						hls.s);
			break;
		default:
			sprintf_s(	Buf[b], 32,
						"unknown(%i)",
						space);
			break;
	}

	return Buf[b];
}

bool GColour::SetStr(const char *str)
{
	if (!str)
		return false;

	GString Str = str;
	char *s = strchr(Str, '(');
	if (!s)
		return false;
	char *e = strchr(s + 1, ')');
	if (!s)
		return false;

	*s = 0;
	*e = 0;
	GString::Array Comp = GString(s+1).Split(",");
	if (!Stricmp(Str.Get(), "rgb"))
	{
		if (Comp.Length() == 3)
			Rgb((int)Comp[0].Int(), (int)Comp[1].Int(), (int)Comp[2].Int());
		else if (Comp.Length() == 4)
			Rgb((int)Comp[0].Int(), (int)Comp[1].Int(), (int)Comp[2].Int(), (int)Comp[3].Int());
		else
			return false;
	}
	else if (!Stricmp(Str.Get(), "hls"))
	{
		if (Comp.Length() == 3)
			SetHLS((uint16) Comp[0].Int(), (uint8) Comp[1].Int(), (uint8) Comp[2].Int());
		else
			return false;
	}
	else if (!Stricmp(Str.Get(), "index"))
	{
		if (Comp.Length() == 1)
		{
			index = (uint8) Comp[0].Int();
			space = CsIndex8;
		}
		else return false;
	}
	else return false;

	return true;
}
