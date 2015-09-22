#ifndef _G_COLOUR_SPACE_H_
#define _G_COLOUR_SPACE_H_

/// Colour component type
enum GComponentType
{
	CtNone,			// 0
	CtIndex,		// 1
	CtRed,			// 2
	CtGreen,		// 3
	CtBlue,			// 4
	CtAlpha,		// 5
	CtPad,			// 6
	CtHue,			// 7
	CtSaturation,	// 8
	CtLuminance,	// 9
	CtCyan,			// 10
	CtMagenta,		// 11
	CtYellow,		// 12
	CtBlack			// 13
};

// Component construction: 4bits type, 4bits size. 8 bits per component.
#define GDC_COLOUR_SPACE_1(type, size) \
	((type << 4) | (size))
#define GDC_COLOUR_SPACE_3(t1, s1, t2, s2, t3, s3) \
	( \
		((t1 << 20) | (s1 << 16)) | \
		((t2 << 12) | (s2 << 8)) | \
		((t3 << 4) | (s3 << 0)) \
	)
#define GDC_COLOUR_SPACE_4(t1, s1, t2, s2, t3, s3, t4, s4) \
	( \
		((t1 << 28) | (s1 << 24)) | \
		((t2 << 20) | (s2 << 16)) | \
		((t3 << 12) | (s3 << 8)) | \
		((t4 << 4) | (s4 << 0)) \
	)

/// Defines a specific colour space
enum GColourSpace
{
	CsNone = 0,

	// uint8 types
	CsIndex1 = GDC_COLOUR_SPACE_1(CtIndex, 1), // Monochrome
	CsIndex4 = GDC_COLOUR_SPACE_1(CtIndex, 4), // 16 colour
	CsIndex8 = GDC_COLOUR_SPACE_1(CtIndex, 8), // 256 colour
	CsAlpha8 = GDC_COLOUR_SPACE_1(CtAlpha, 8), // Alpha channel only

	// uint16 types
	CsArgb15 = GDC_COLOUR_SPACE_4(CtAlpha, 1, CtRed, 5, CtGreen, 5, CtBlue, 5),
	CsRgb15 = GDC_COLOUR_SPACE_3(CtRed, 5, CtGreen, 5, CtBlue, 5),
	CsAbgr15 = GDC_COLOUR_SPACE_4(CtAlpha, 1, CtBlue, 5, CtGreen, 5, CtRed, 5),
	CsBgr15 = GDC_COLOUR_SPACE_3(CtBlue, 5, CtGreen, 5, CtRed, 5),
	CsRgb16 = GDC_COLOUR_SPACE_3(CtRed, 5, CtGreen, 6, CtBlue, 5),
	CsBgr16 = GDC_COLOUR_SPACE_3(CtBlue, 5, CtGreen, 6, CtRed, 5),

	// 24 bit types
	CsRgb24 = GDC_COLOUR_SPACE_3(CtRed, 8, CtGreen, 8, CtBlue, 8),
	CsBgr24 = GDC_COLOUR_SPACE_3(CtBlue, 8, CtGreen, 8, CtRed, 8),

	// 24 bits with padding
	CsRgbx32 = GDC_COLOUR_SPACE_4(CtRed, 8, CtGreen, 8, CtBlue, 8, CtPad, 8),
	CsBgrx32 = GDC_COLOUR_SPACE_4(CtBlue, 8, CtGreen, 8, CtRed, 8, CtPad, 8),
	CsXrgb32 = GDC_COLOUR_SPACE_4(CtPad, 8, CtRed, 8, CtGreen, 8, CtBlue, 8),
	CsXbgr32 = GDC_COLOUR_SPACE_4(CtPad, 8, CtBlue, 8, CtGreen, 8, CtRed, 8),

	// 32 bit types
	CsRgba32 = GDC_COLOUR_SPACE_4(CtRed, 8, CtGreen, 8, CtBlue, 8, CtAlpha, 8),
	CsBgra32 = GDC_COLOUR_SPACE_4(CtBlue, 8, CtGreen, 8, CtRed, 8, CtAlpha, 8),
	CsArgb32 = GDC_COLOUR_SPACE_4(CtAlpha, 8, CtRed, 8, CtGreen, 8, CtBlue, 8),
	CsAbgr32 = GDC_COLOUR_SPACE_4(CtAlpha, 8, CtBlue, 8, CtGreen, 8, CtRed, 8),

	// 16 bit component depth (size==0 means 16 bit)
	CsBgr48 = GDC_COLOUR_SPACE_3(CtBlue, 0, CtGreen, 0, CtRed, 0),
	CsRgb48 = GDC_COLOUR_SPACE_3(CtRed, 0, CtGreen, 0, CtBlue, 0),
	CsBgra64 = GDC_COLOUR_SPACE_4(CtBlue, 0, CtGreen, 0, CtRed, 0, CtAlpha, 0),
	CsRgba64 = GDC_COLOUR_SPACE_4(CtRed, 0, CtGreen, 0, CtBlue, 0, CtAlpha, 0),
	CsAbgr64 = GDC_COLOUR_SPACE_4(CtAlpha, 0, CtBlue, 0, CtGreen, 0, CtRed, 0),
	CsArgb64 = GDC_COLOUR_SPACE_4(CtAlpha, 0, CtRed, 0, CtGreen, 0, CtBlue, 0),

	// other colour spaces
	CsHls32 = GDC_COLOUR_SPACE_4(CtHue, 0 /*16*/, CtRed, 8, CtGreen, 8, CtBlue, 8),
	CsCmyk32 = GDC_COLOUR_SPACE_4(CtCyan, 8, CtMagenta, 8, CtYellow, 8, CtBlack, 8),
};

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#endif

#ifdef _MSC_VER
#define LEAST_SIG_BIT_FIRST		1
#define LEAST_SIG_BYTE_FIRST	0
#else
#define LEAST_SIG_BIT_FIRST		0
#define LEAST_SIG_BYTE_FIRST	0
#endif


struct GRgb15 {
	#if LEAST_SIG_BIT_FIRST
	uint16 b : 5;
	uint16 g : 5;
	uint16 r : 5;
	uint16 pad : 1;
	#else
	uint16 pad : 1;
	uint16 r : 5;
	uint16 g : 5;
	uint16 b : 5;
	#endif
};

struct GArgb15 {
	#if LEAST_SIG_BIT_FIRST
	uint16 b : 5;
	uint16 g : 5;
	uint16 r : 5;
	uint16 a : 1;
	#else
	uint16 a : 1;
	uint16 r : 5;
	uint16 g : 5;
	uint16 b : 5;
	#endif
};

struct GBgr15 {
	#if LEAST_SIG_BIT_FIRST
	uint16 r : 5;
	uint16 g : 5;
	uint16 b : 5;
	uint16 pad : 1;
	#else
	uint16 pad : 1;
	uint16 b : 5;
	uint16 g : 5;
	uint16 r : 5;
	#endif
};

struct GAbgr15 {
	#if LEAST_SIG_BIT_FIRST
	uint16 r : 5;
	uint16 g : 5;
	uint16 b : 5;
	uint16 a : 1;
	#else
	uint16 a : 1;
	uint16 b : 5;
	uint16 g : 5;
	uint16 r : 5;
	#endif
};

struct GRgb16 {
	#if !LEAST_SIG_BIT_FIRST
	uint16 r : 5;
	uint16 g : 6;
	uint16 b : 5;
	#else
	uint16 b : 5;
	uint16 g : 6;
	uint16 r : 5;
	#endif
};

struct GBgr16 {
	#if LEAST_SIG_BIT_FIRST
	uint16 r : 5;
	uint16 g : 6;
	uint16 b : 5;
	#else
	uint16 b : 5;
	uint16 g : 6;
	uint16 r : 5;
	#endif
};

struct GRgb24 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 r, g, b;
	#else
	uint8 b, g, r;
	#endif
};

struct GBgr24 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 b, g, r;
	#else
	uint8 r, g, b;
	#endif
};

struct GRgba32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 r, g, b, a;
	#else
	uint8 a, b, g, r;
	#endif
};

struct GBgra32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 b, g, r, a;
	#else
	uint8 a, r, g, b;
	#endif
};

struct GArgb32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 a, r, g, b;
	#else
	uint8 b, g, r, a;
	#endif
};

struct GAbgr32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 a, b, g, r;
	#else
	uint8 r, g, b, a;
	#endif
};

struct GXrgb32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 pad, r, g, b;
	#else
	uint8 b, g, r, pad;
	#endif
};

struct GRgbx32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 r, g, b, pad;
	#else
	uint8 pad, b, g, r;
	#endif
};

struct GXbgr32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 pad, b, g, r;
	#else
	uint8 r, g, b, pad;
	#endif
};

struct GBgrx32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 b, g, r, pad;
	#else
	uint8 pad, r, g, b;
	#endif
};

#ifdef WIN32
#pragma pack(push, 2)
#endif
struct GRgb48 {
	#if LEAST_SIG_BYTE_FIRST
	uint16 r, g, b;
	#else
	uint16 b, g, r;
	#endif
};

struct GBgr48 {
	#if LEAST_SIG_BYTE_FIRST
	uint16 b, g, r;
	#else
	uint16 r, g, b;
	#endif
};

struct GRgba64 {
	#if LEAST_SIG_BYTE_FIRST
	uint16 r, g, b, a;
	#else
	uint16 a, b, g, r;
	#endif
};

struct GBgra64 {
	#if LEAST_SIG_BYTE_FIRST
	uint16 b, g, r, a;
	#else
	uint16 a, r, g, b;
	#endif
};

struct GArgb64 {
	#if LEAST_SIG_BYTE_FIRST
	uint16 a, r, g, b;
	#else
	uint16 b, g, r, a;
	#endif
};

struct GAbgr64 {
	#if LEAST_SIG_BYTE_FIRST
	uint16 a, b, g, r;
	#else
	uint16 r, g, b, a;
	#endif
};
#ifdef WIN32
#pragma pack(pop)
#endif

struct GHls32 {
	uint16 h;
	uint8 l, s;
};

struct GCmyk32 {
	#if LEAST_SIG_BYTE_FIRST
	uint8 c, m, y, k;
	#else
	uint8 k, y, m, c;
	#endif
};

struct GColourComponent
{
	// ((type << 4) | (size))
	uint8 Bits;
	
	GComponentType Type() { return (GComponentType) ((Bits >> 4) & 0xf); }
	void Type(GComponentType t) { Bits = (Bits & 0xf) | ((uint8)t << 4); }
	uint8 Size() { return Bits & 0xf; }
	void Size(uint8 t) { Bits = (Bits & 0xf0) | (t & 0xf); }
};

union GColourSpaceBits
{
	uint32 All;
	GColourComponent Bits[4];
	
	GColourComponent &operator[](int i)
	{
		static int Reverse = -1;
		if (Reverse < 0)
		{
			GColourSpaceBits t;
			t.All = 0;
			t.Bits[0].Type(CtIndex);
			if (t.All == 0x10)
				Reverse = 0;
			else if (t.All == (0x10<<24))
				Reverse = 1;
			else
				LgiAssert(0);
		}
		if (Reverse == 1)
			return Bits[3-i];
		return Bits[i];
	}
};

#ifdef WIN32
#pragma pack(pop, before_pack)
#endif

/// Converts a colour space into a string for debugging/reporting.
LgiFunc const char *GColourSpaceToString(GColourSpace cs);

/// Works out how many bits required for a pixel in a particular colour space.
LgiFunc int GColourSpaceToBits(GColourSpace ColourSpace);

/// /returns the number of channels in the colour space
LgiFunc int GColourSpaceChannels(GColourSpace Cs);

/// /returns true if the colour space has an alpha channel
LgiFunc bool GColourSpaceHasAlpha(GColourSpace Cs);

/// Converts a bit-depth into the default colour space for that depth. Used mostly
/// in interfacing old bit-depth based code to newer colour space code.
LgiFunc GColourSpace GBitsToColourSpace(int Bits);

/// Converts a string representation into a colour space.
LgiFunc GColourSpace GStringToColourSpace(const char *c);

#ifdef __GTK_H__
/// Converts a GTK visual to a Lgi colour space.
LgiFunc GColourSpace GdkVisualToColourSpace(Gtk::GdkVisual *v, int output_bits);
#endif

// These definitions are provide as a convenience when converting old code to the
// GColourSpace system. However you should not use them for new code, as some systems
// can have different colour spaces depending on OS version or hardware configuration.
#if defined(WIN32) || defined(LINUX) || defined(BEOS)

	#define System15BitColourSpace CsBgr15
	typedef GBgr15 System15BitPixel;

	#define System16BitColourSpace CsBgr16
	typedef GBgr16 System16BitPixel;

	#define System24BitColourSpace CsBgr24
	typedef GBgr24 System24BitPixel;

	#define System32BitColourSpace CsBgra32
	typedef GBgra32 System32BitPixel;

#else

	#define System15BitColourSpace CsRgb15
	typedef GRgb15 System15BitPixel;

	#define System16BitColourSpace CsRgb16
	typedef GRgb16 System16BitPixel;

	#define System24BitColourSpace CsRgb24
	typedef GRgb24 System24BitPixel;

	#define System32BitColourSpace CsRgba32
	typedef GRgba32 System32BitPixel;

#endif

typedef union
{
	uint8 *u8;
	uint16 *u16;
	uint32 *u32;
	int i;
	
	GRgb15  *rgb15;
	GBgr15  *bgr15;
	GRgb16  *rgb16;
	GBgr16  *bgr16;
	GRgb24  *rgb24;
	GBgr24  *bgr24;
	GRgba32 *rgba32;
	GArgb32 *argb32;
	GHls32  *hls32;
	GCmyk32 *cmyk32;
	
	System15BitPixel *s15;
	System16BitPixel *s16;
	System24BitPixel *s24;
	System32BitPixel *s32;

}	GPixelPtr;


/** \brief 32bit colour of varing bit depth. If no associated depth is given, 32 bits is assumed.

	\code
	COLOUR typedef description
                31             24  23             16  15              8  7               0
                |-|-|-|-|-|-|-|-|  |-|-|-|-|-|-|-|-|  |-|-|-|-|-|-|-|-|  |-|-|-|-|-|-|-|-|

    Standard:
    8bit mode:	|------------------------ unused ---------------------|  |---- index ----|
    15bit mode:	|----------------- unused ------------|T|-- red --|-- green ---|-- blue -|
    16bit mode:	|------------- unused -------------|  |-- red --|--- green ----|-- blue -|
    24bit mode:	|---- unused ---|  |----- blue ----|  |---- green ----|  |----- red -----|
	32bit mode: |---- alpha ----|  |----- blue ----|  |---- green ----|  |----- red -----|
	\endcode

    Note:		In 15bit mode the T bit can toggle between 8-bit indexing and RGB, or it 
				can also just be a 1 bit alpha channel.
*/
typedef uint32						COLOUR;

/// Alpha value, 0 - transparent, 255 - solid
typedef uint8						ALPHA;

// colour conversion defines
// RGB colour space
#define BitWidth(bits, cropbits)	( (((bits)+(cropbits)-1)/(cropbits)) * 4 )

#define C24R					2
#define C24G					1
#define C24B					0

#define C32A					3
#define C32R					2
#define C32G					1
#define C32B					0


/// Create a 15bit COLOUR from components
#define Rgb15(r, g, b)				( ((r&0xF8)<<7) | ((g&0xF8)<<2) | ((b&0xF8)>>3))
#define Rgb32To15(c32)				( ((c32&0xF8)>>3) | ((c32&0xF800)>>6) | ((c32&0xF80000)>>9) )
#define Rgb24To15(c24)				( (B24(c24)>>3) | ((G24(c24)<<2)&0x3E0) | ((R24(c24)<<7)&0x7C00) )
#define Rgb16To15(c16)				( ((c16&0xFFC0)>>1) | (c16&0x1F) )
/// Get the red component of a 15bit COLOUR
#define R15(c15)					( (uchar) ((c15>>10)&0x1F) )
/// Get the green component of a 15bit COLOUR
#define G15(c15)					( (uchar) ((c15>>5)&0x1F) )
/// Get the blue component of a 15bit COLOUR
#define B15(c15)					( (uchar) ((c15)&0x1F) )

#define Rc15(c)						( (((c) & 0x7C00) >> 7) | (((c) & 0x7C00) >> 12) )
#define Gc15(c)						( (((c) & 0x03E0) >> 2) | (((c) & 0x03E0) >> 7) )
#define Bc15(c)						( (((c) & 0x001F) << 3) | (((c) & 0x001F) >> 2) )

/// Create a 16bit COLOUR from components
#define Rgb16(r, g, b)				( ((r&0xF8)<<8) | ((g&0xFC)<<3) | ((b&0xF8)>>3))
#define Rgb32To16(c32)				( ((c32&0xF8)>>3) | ((c32&0xFC00)>>5) | ((c32&0xF80000)>>8) )
#define Rgb24To16(c24)				( (B24(c24)>>3) | ((G24(c24)<<3)&0x7E0) | ((R24(c24)<<8)&0xF800) )
#define Rgb15To16(c15)				( ((c15&0x7FE0)<<1) | (c15&0x1F) )
/// Get the red component of a 16bit COLOUR
#define R16(c16)					( (uchar) ((c16>>11)&0x1F) )
/// Get the green component of a 16bit COLOUR
#define G16(c16)					( (uchar) ((c16>>5)&0x3F) )
/// Get the blue component of a 16bit COLOUR
#define B16(c16)					( (uchar) ((c16)&0x1F) )

#define Rc16(c)						( (((c) & 0xF800) >> 8) | (((c) & 0xF800) >> 13) )
#define Gc16(c)						( (((c) & 0x07E0) >> 3) | (((c) & 0x07E0) >> 9) )
#define Bc16(c)						( (((c) & 0x001F) << 3) | (((c) & 0x001F) >> 2) )

#define G5bitTo8Bit(c)				( ((c) << 3) | ((c) >> 2) )
#define G6bitTo8Bit(c)				( ((c) << 2) | ((c) >> 4) )
#define G8bitTo16Bit(c)				( ((uint16)(c) << 8) | ((c) & 0xff) )

/// Get the red component of a 24bit COLOUR
#define R24(c24)					( ((c24)>>(C24R*8)) & 0xff )
/// Get the green component of a 24bit COLOUR
#define G24(c24)					( ((c24)>>(C24G*8)) & 0xff  )
/// Get the blue component of a 24bit COLOUR
#define B24(c24)					( ((c24)>>(C24B*8)) & 0xff  )

/// Create a 24bit COLOUR from components
#define Rgb24(r, g, b)				( (((r)&0xFF) << (C24R*8)) | (((g)&0xFF) << (C24G*8)) | (((b)&0xFF) << (C24B*8)) )
#define Rgb32To24(c32)				Rgb24(R32(c32), G32(c32), B32(c32))
#define Rgb15To24(c15)				( ((c15&0x7C00)>>7) | ((c15&0x3E0)<<6) | ((c15&0x1F)<<19) )
#define Rgb16To24(c16)				( ((c16&0xF800)>>8) | ((c16&0x7E0)<<5) | ((c16&0x1F)<<19) )

// 32
//				31              24 23              16 15              8  7               0
//				|-|-|-|-|-|-|-|-|  |-|-|-|-|-|-|-|-|  |-|-|-|-|-|-|-|-|  |-|-|-|-|-|-|-|-|
// Windows		|---- alpha ----|  |----- red -----|  |---- green ----|  |----- blue ----|
// Linux		|----- blue ----|  |---- green ----|  |----- red -----|  |---- alpha ----|

/// Get the red component of a 32bit COLOUR
#define R32(c32)					( (uchar) (c32 >> (C32R << 3)) )
/// Get the green component of a 32bit COLOUR
#define G32(c32)					( (uchar) (c32 >> (C32G << 3)) )
/// Get the blue component of a 32bit COLOUR
#define B32(c32)					( (uchar) (c32 >> (C32B << 3)) )
/// Get the opacity/alpha component of a 32bit COLOUR
#define A32(c32)					( (uchar) (c32 >> (C32A << 3)) )

#define RgbPreMul(c, a)				( Div255Lut[(c)*a] )
#define Rgb32(r, g, b)				( (((r)&0xFF)<<(C32R<<3)) | (((g)&0xFF)<<(C32G<<3)) | (((b)&0xFF)<<(C32B<<3)) | (0xFF<<(C32A<<3)) )
#define Rgba32(r, g, b, a)			( (((r)&0xFF)<<(C32R<<3)) | (((g)&0xFF)<<(C32G<<3)) | (((b)&0xFF)<<(C32B<<3)) | (((a)&0xFF)<<(C32A<<3)) )
#define Rgbpa32(r, g, b, a)			( (RgbPreMul(r, a)<<(C32R<<3)) | (RgbPreMul(g, a)<<(C32G<<3)) | (RgbPreMul(b, a)<<(C32B<<3)) | ((a&0xFF)<<(C32A<<3)) )
#define Rgb24To32(c24)				Rgba32( R24(c24), G24(c24), B24(c24), 255 )
#define Rgb15To32(c15)				( ((c15&0x7C00)<<9) | ((c15&0x3E0)<<6) | ((c15&0x1F)<<3) | (0xFF<<(C32A*8)) )
#define Rgb16To32(c16)				( ((c16&0xF800)<<8) | ((c16&0x7E0)<<5) | ((c16&0x1F)<<3) | (0xFF<<(C32A*8)) )

// HLS colour space tools

/// Builds a complete 32bit HLS colour from it's components
///
/// The hue (H) component varies from 0 to 359
/// The lightness (L) component varies from 0 (black), thru 128 (colour) to 255 (white)
/// The saturation (S) component varies from 0 (grey) thru to 255 (full colour)
///
/// \sa HlsToRgb(), RgbToHls()
#define Hls32(h, l, s)				( ((h)<<16) | ((l)<<8) | (s) )
/// Extracts the hue component from the 32-bit HLS colour
/// \sa #Hls32
#define H32(c)						( ((c)>>16)&0xFFFF )
/// Extracts the lightness component from the 32-bit HLS colour
/// \sa #Hls32
#define L32(c)						( ((c)>>8)&0xFF )
/// Extracts the saturation component from the 32-bit HLS colour
/// \sa #Hls32
#define S32(c)						( (c)&0xFF )
/// The value of an undefined hue
/// \sa #Hls32
#define HUE_UNDEFINED				1024

#endif