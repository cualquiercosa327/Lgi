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

	int HlsValue(double fN1, double fN2, double fHue) const;

public:
	static const GColour Black;
	static const GColour White;
	static const GColour Red;
	static const GColour Green;
	static const GColour Blue;

	/// Transparent
	GColour();
	/// Indexed colour
	GColour(uint8 idx8, GPalette *palette);
	/// True colour
	GColour(int r, int g, int b, int a = 255);
	/// Conversion from COLOUR
	GColour(uint32 c, int bits, GPalette *palette = NULL);
	GColourSpace GetColourSpace();
	
	bool operator ==(const GColour &c)
	{
		return c32() == c.c32();
	}
	
	bool IsValid();
	void Empty();
	bool Transparent();
	/// Sets the colour to a rgb(a) value
	void Rgb(int r, int g, int b, int a = 255);
	/// Sets the colour
	void Set(uint32 c, int bits, GPalette *palette = NULL);
	uint32 Get(int bits);
	/// Gets the red component (0-255)
	uint8 r() const;
	/// Gets the green component (0-255)
	uint8 g() const;
	/// Gets the blue component (0-255)
	uint8 b() const;
	/// Gets the alpha component (0-255)
	uint8 a() const;
	// Gets the indexed colour
	uint8 c8() const;
	// Sets indexed colour
	void c8(uint8 c, GPalette *p);
	// Get as 24 bit colour
	uint32 c24() const;
	/// Set 24 bit colour
	void c24(uint32 c);
	/// Get 32 bit colour
	uint32 c32() const;
	/// Set 32 bit colour
	void c32(uint32 c);
	/// Mixes 'Tint' with the current colour and returns it 
	/// without modifying the object.
	GColour Mix(GColour Tint, float RatioOfTint = 0.5);
	// Hue Lum Sat methods
	
	/// Returns the hue value (0-359)
	uint32 GetH();
	/// Returns whether the hue is value or not
	bool HueIsUndefined();
	/// Returns the luminance
	uint32 GetL();
	/// Returns the saturation
	uint32 GetS();	
	/// Converts the colour space to HLS
	bool ToHLS();	
	/// Sets the colour to a HLS value
	void SetHLS
	(
		/// (0-359)
		uint16 h,
		/// (0-255)
		uint8 l,
		/// (0-255)
		uint8 s
	);
	int GetGray(int BitDepth = 8);
	uint32 GetNative();
	char *GetStr();
	
	#ifdef BEOS
	operator rgb_color()
	{
		rgb_color c;
		c.red = r();
		c.green = g();
		c.blue = b();
		return c;
	}
	#endif
};

#endif
