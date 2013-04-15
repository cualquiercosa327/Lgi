/// \file
/// \author Matthew Allen
/// \created 3/2/2011
#ifndef _GCOLOUR_H_
#define _GCOLOUR_H_

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#endif

/// RGB Colour
class LgiClass GdcRGB
{
public:
	uchar R, G, B;
	#ifndef MAC
	uchar Flags;
	#endif

	void Set(uchar r, uchar g, uchar b, uchar f = 0)
	{
		R = r;
		G = g;
		B = b;
		#ifndef MAC
		Flags = f;
		#endif
	}
};

#ifdef WIN32
#pragma pack(pop, before_pack)
#endif

/// Palette of colours
class LgiClass GPalette
{
protected:
	#if WIN32NATIVE
	HPALETTE	hPal;
	LOGPALETTE	*Data;
	#else
	int			Size;
	GdcRGB		*Data;
	#endif
	uchar *Lut;

public:
	GPalette();
	virtual ~GPalette();

	#if WIN32NATIVE
	HPALETTE Handle() { return hPal; }
	#endif

	GPalette(GPalette *pPal);	
	GPalette(uchar *pPal, int s = 256);
	void Set(GPalette *pPal);
	void Set(uchar *pPal, int s = 256);

	int GetSize();
	GdcRGB *operator [](int i);
	bool Update();
	bool SetSize(int s = 256);
	void SwapRAndB();
	int MatchRgb(COLOUR Rgb);
	void CreateCube();
	void CreateGreyScale();
	bool Load(GFile &F);
	bool Save(GFile &F, int Format);
	uchar *MakeLut(int Bits = 16);

	bool operator ==(GPalette &p);
	bool operator !=(GPalette &p);
};

/// A colour definition
class LgiClass GColour
{
public:
	enum Space
	{
		ColIdx8,
		ColRgba32,
		ColHls32,
	};

protected:
	class GPalette *pal;
	union {
		uint8  p8;
		uint32 p32;
		struct
		{
			uint8 s;
			uint8 l;
			int16 h;
		} hls;
	};
	Space space;

	int HlsValue(double fN1, double fN2, double fHue)
	{
		if (fHue > 360.0) fHue -= 360.0;
		else if (fHue < 0.0) fHue += 360.0;

		if (fHue < 60.0) return (int) ((fN1 + (fN2 - fN1) * fHue / 60.0) * 255.0 + 0.5);
		else if (fHue < 180.0) return (int) ((fN2 * 255.0) + 0.5);
		else if (fHue < 240.0) return (int) ((fN1 + (fN2 - fN1) * (240.0 - fHue) / 60.0) * 255.0 + 0.5);
		
		return (int) ((fN1 * 255.0) + 0.5);
	}

public:
	/// Transparent
	GColour()
	{
		c32(0);
	}

	/// Indexed colour
	GColour(uint8 idx8, GPalette *palette)
	{
		c8(idx8, palette);
	}

	/// True colour
	GColour(int r, int g, int b, int a = 255)
	{
		c32(Rgba32(r, g, b, a));
	}

	/// Conversion from COLOUR
	GColour(COLOUR c, int bits)
	{
		Set(c, bits);
	}

	bool Transparent()
	{
		if (space == ColRgba32)
			return A32(p32) == 0;
		else if (space == ColIdx8)
			return !pal || p8 < pal->GetSize();
		return true;
	}

	/// Sets the colour to a rgb(a) value
	void Rgb(int r, int g, int b, int a = 255)
	{
		c32(Rgba32(r, g, b, a));
	}

	/// Sets the colour
	void Set(COLOUR c, int bits)
	{
		pal = 0;
		switch (bits)
		{
			case 8:
			{
				p8 = c;
				space = ColIdx8;
				break;
			}
			case 15:
			{
				space = ColRgba32;
				p32 = Rgb15To32(c);
				break;
			}
			case 16:
			{
				space = ColRgba32;
				p32 = Rgb16To32(c);
				break;
			}
			case 24:
			{
				space = ColRgba32;
				p32 = Rgb24To32(c);
				break;
			}
			case 32:
			{
				space = ColRgba32;
				p32 = c;
				break;
			}
			default:
			{
				space = ColRgba32;
				p32 = 0;
				LgiAssert(!"Not a known colour depth.");
			}
		}
	}
	
	COLOUR Get(int bits)
	{
		switch (bits)
		{
			case 8:
				if (space == ColIdx8)
					return p8;
				LgiAssert(!"Not supported.");
				break;
			case 24:
				return c24();
			case 32:
				return c32();
		}
		
		return 0;
	}

	/// Gets the red component (0-255)
	uint8 r()
	{
		return R32(c32());
	}

	/// Gets the green component (0-255)
	uint8 g()
	{
		return G32(c32());
	}
	
	/// Gets the blue component (0-255)
	uint8 b()
	{
		return B32(c32());
	}

	/// Gets the alpha component (0-255)
	uint8 a()
	{
		return A32(c32());
	}

	// Gets the indexed colour
	uint8 c8()
	{
		return p8;
	}

	// Sets indexed colour
	void c8(uint8 c, GPalette *p)
	{
		space = ColIdx8;
		pal = p;
		p8 = c;
	}

	// Get as 24 bit colour
	uint32 c24()
	{
		if (space == ColRgba32)
		{
			return Rgb32To24(p32);
		}
		else if (space == ColIdx8)
		{
			if (pal)
			{
				// colour palette lookup
				if (p8 < pal->GetSize())
				{
					GdcRGB *c = (*pal)[p8];
					if (c)
					{
						return Rgb24(c->R, c->G, c->B);
					}
				}

				return 0;
			}
			
			return Rgb24(p8, p8, p8); // monochome
		}
		else if (space == ColHls32)
		{
			return Rgb32To24(c32());
		}

		// Black...
		return 0;
	}

	/// Set 24 bit colour
	void c24(COLOUR c)
	{
		space = ColRgba32;
		p32 = Rgb24To32(c);
		pal = 0;
	}

	/// Get 32 bit colour
	COLOUR c32()
	{
		if (space == ColRgba32)
		{
			return p32;
		}
		else if (space == ColIdx8)
		{
			if (pal)
			{
				// colour palette lookup
				if (p8 < pal->GetSize())
				{
					GdcRGB *c = (*pal)[p8];
					if (c)
					{
						return Rgb32(c->R, c->G, c->B);
					}
				}

				return 0;
			}
			
			return Rgb32(p8, p8, p8); // monochome
		}
		else if ((space = ColHls32))
		{
			// Convert from HLS back to RGB
			if (hls.s == 0)
			{
				p32 = Rgb32(0, 0, 0);
			}
			else
			{
				while (hls.h >= 360)
					hls.h -= 360;			
				while (hls.h < 0)
					hls.h += 360;
			
				double fHue = (double) hls.h, fM1, fM2;
				double fLightness = ((double) hls.l) / 255.0;
				double fSaturation = ((double) hls.s) / 255.0;
			
				if (hls.l < 128)
					fM2 = fLightness * (1 + fSaturation);
				else
					fM2 = fLightness + fSaturation - (fLightness * fSaturation);
			
				fM1 = 2.0 * fLightness - fM2;

				int R = HlsValue(fM1, fM2, fHue + 120.0);
				int G = HlsValue(fM1, fM2, fHue);
				int B = HlsValue(fM1, fM2, fHue - 120.0);
				p32 = Rgb32(R, G, B);
			}
			pal = NULL;
			space = ColRgba32;
			return p32;
		}

		// Transparent?
		return 0;
	}

	/// Set 32 bit colour
	void c32(COLOUR c)
	{
		space = ColRgba32;
		pal = 0;
		p32 = c;
	}
	
	/// Mixes 'Tint' with the current colour and returns it 
	/// without modifying the object.
	GColour Mix(GColour Tint, float RatioOfTint = 0.5)
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
	
	// Hue Lum Sat methods
	uint32 GetH()
	{
		ToHLS();
		return hls.h;
	}

	bool HueIsUndefined()
	{
		ToHLS();
		return hls.h == HUE_UNDEFINED;
	}

	uint32 GetL()
	{
		ToHLS();
		return hls.l;
	}

	uint32 GetS()
	{
		ToHLS();
		return hls.s;
	}
	
	bool ToHLS()
	{
		if (space == ColHls32)
			return true;

		uint32 nMax, nMin, nDelta, c = c32();
		int R = R32(c), G = G32(c), B = B32(c);
		double fHue;

		nMax = max(R, max(G, B));
		nMin = min(R, min(G, B));

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
		space = ColHls32;
		pal = NULL;
		return true;
	}
	
	void SetHLS(uint16 h, uint8 l, uint8 s)
	{
		space = ColHls32;
		hls.h = h;
		hls.l = l;
		hls.s = s;
	}
	
	int GetGray(int BitDepth = 8)
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
};

#endif