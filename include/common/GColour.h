/// \file
/// \author Matthew Allen
/// \created 3/2/2011
#ifndef _GCOLOUR_H_
#define _GCOLOUR_H_

#include "GColourSpace.h"

/// A colour definition
class LgiClass GColour
{
protected:
	class GPalette *pal;
	union {
		uint32 flat;
		uint8  index;
		System32BitPixel rgb;
		GHls32 hls;
	};
	GColourSpace space;

	int HlsValue(double fN1, double fN2, double fHue);

public:
	/// Transparent
	GColour();
	/// Indexed colour
	GColour(uint8 idx8, GPalette *palette);
	/// True colour
	GColour(int r, int g, int b, int a = 255);
	/// Conversion from COLOUR
	GColour(uint32 c, int bits, GPalette *palette = NULL);
	GColourSpace GetColourSpace();
	
	bool IsValid();
	void Empty();
	bool Transparent();
	/// Sets the colour to a rgb(a) value
	void Rgb(int r, int g, int b, int a = 255);
	/// Sets the colour
	void Set(uint32 c, int bits, GPalette *palette = NULL);
	uint32 Get(int bits);
	/// Gets the red component (0-255)
	uint8 r();
	/// Gets the green component (0-255)
	uint8 g();
	/// Gets the blue component (0-255)
	uint8 b();
	/// Gets the alpha component (0-255)
	uint8 a();
	// Gets the indexed colour
	uint8 c8();
	// Sets indexed colour
	void c8(uint8 c, GPalette *p);
	// Get as 24 bit colour
	uint32 c24();
	/// Set 24 bit colour
	void c24(uint32 c);
	/// Get 32 bit colour
	uint32 c32();
	/// Set 32 bit colour
	void c32(uint32 c);
	/// Mixes 'Tint' with the current colour and returns it 
	/// without modifying the object.
	GColour Mix(GColour Tint, float RatioOfTint = 0.5);
	// Hue Lum Sat methods
	uint32 GetH();
	bool HueIsUndefined();
	uint32 GetL();
	uint32 GetS();	
	bool ToHLS();	
	void SetHLS(uint16 h, uint8 l, uint8 s);
	int GetGray(int BitDepth = 8);
	uint32 GetNative();
	char *GetStr();
};

#endif