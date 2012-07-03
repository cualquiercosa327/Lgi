/**
	\file
	\author Matthew Allen
	\date 20/2/1997
	\brief GDC v2.xx header
*/

#ifndef __GDC2_H_
#define __GDC2_H_

#include <stdio.h>

// sub-system headers
#include "LgiOsDefs.h"				// Platform specific
#include "LgiInc.h"
#include "LgiClass.h"
#include "Progress.h"				// Xp
#include "GFile.h"					// Platform specific
#include "GMem.h"					// Platform specific
#include "Core.h"					// Platform specific
#include "GContainers.h"
#include "GCapabilities.h"

// Alpha Blting
#ifdef WIN32
#include "wingdi.h"
#endif
#ifndef AC_SRC_OVER
#define AC_SRC_OVER					0
#endif
#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA				1
#endif
#include "GLibrary.h"

//				Defines

/// The default gamma curve (none) used by the gamma LUT GdcDevice::GetGamma
#define LGI_DEFAULT_GAMMA				1.0

#ifndef LGI_PI
/// The value of PI
#define LGI_PI						3.141592654
#endif

/// Converts degrees to radians
#define LGI_DegToRad(i)				((i)*LGI_PI/180)

/// Converts radians to degrees
#define LGI_RadToDeg(i)				((i)*180/LGI_PI)

#if defined(WIN32) && !defined(_WIN64)
/// Use intel assembly instructions, comment out for porting
#define GDC_USE_ASM
#endif

/// Blending mode: overwrite
#define GDC_SET						0
/// Blending mode: bitwise AND with background
#define GDC_AND						1
/// Blending mode: bitwise OR with background
#define GDC_OR						2
/// Blending mode: bitwise XOR with background
#define GDC_XOR						3
/// Blending mode: alpha blend with background
#define GDC_ALPHA					4
#define GDC_REMAP					5
#define GDC_MAXOP					6
#define GDC_CACHE_SIZE				4

// Channel types
#define GDCC_MONO					0
#define GDCC_GREY					1
#define GDCC_INDEX					2
#define GDCC_R						3
#define GDCC_G						4
#define GDCC_B						5
#define GDCC_ALPHA					6

// Native data formats
#define GDC_8BIT					0
#define GDC_16BIT					1
#define GDC_24BIT					2
#define GDC_32BIT					3
#define GDC_MAXFMT					4

// Colour spaces
#define GDC_8I						0	// 8 bit paletted
#define GDC_5R6G5B					1	// 16 bit
#define GDC_8B8G8B					2	// 24 bit
#define GDC_8A8R8G8B				3	// 32 bit
#define GDC_MAXSPACE				4

// Update types
#define GDC_PAL_CHANGE				0x1
#define GDC_BITS_CHANGE				0x2

// Flood fill types

/// GSurface::FloodFill to a different colour
#define GDC_FILL_TO_DIFFERENT		0
/// GSurface::FloodFill to a certain colour
#define GDC_FILL_TO_BORDER			1
/// GSurface::FloodFill while colour is near to the seed colour
#define GDC_FILL_NEAR				2

// Gdc options

/// Used in GdcApp8Set::Blt when doing colour depth reduction to 8 bit.
#define GDC_REDUCE_TYPE				0
	/// No conversion
	#define REDUCE_NONE					0
	/// Nearest colour pixels
	#define REDUCE_NEAREST				1
	/// Halftone the pixels
	#define REDUCE_HALFTONE				2
	/// Error diffuse the pixels
	#define REDUCE_ERROR_DIFFUSION		3
	/// Not used.
	#define REDUCE_DL1					4
/// Not used.
#define GDC_HALFTONE_BASE_INDEX		1
/// When in 8-bit defined the behaviour of GdcDevice::GetColour
#define GDC_PALETTE_TYPE			2
	/// Allocate colours from the palette
	#define PALTYPE_ALLOC				0
	/// Use an RGB cube
	#define PALTYPE_RGB_CUBE			1
	/// Use a HSL palette
	#define PALTYPE_HSL					2
/// Converts images to the specified bit-depth on load, does nothing if 0
#define GDC_PROMOTE_ON_LOAD			3
#define GDC_MAX_OPTION				4

//	Flags
// Surface
#define GDC_OWN_MEMORY				0x0001
#define GDC_ON_SCREEN				0x0002
#define GDC_ALPHA_CHANNEL			0x0004
#define GDC_UPDATED_PALETTE			0x0008

// BaseDC
#define GDC_OWN_APPLICATOR			0x0001
#define GDC_CACHED_APPLICATOR		0x0002
#define GDC_OWN_PALETTE				0x0004
#define GDC_DRAW_ON_ALPHA			0x0008

// Region types
#define GDC_RGN_NONE				0	// No clipping
#define GDC_RGN_SIMPLE				1	// Single rectangle
#define GDC_RGN_COMPLEX				2	// Many rectangles

// Error codes
#define GDCERR_NONE					0
#define GDCERR_ERROR				1
#define GDCERR_CANT_SET_SCAN_WIDTH	2
#define GDC_INVALIDMODE				-1

// Font display flags
#define GDCFNT_TRANSPARENT			0x0001	// not set - SOLID
#define GDCFNT_UNDERLINE			0x0002

// Palette file types
#define GDCPAL_JASC					1
#define GDCPAL_MICROSOFT			2

// Misc
#define BMPWIDTH(bits)				((((bits)+31)/32)*4)


//				Typedefs

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

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#endif

#if defined WIN32

	#ifndef __BIG_ENDIAN__
	#define C24R					0
	#define C24G					1
	#define C24B					2
	#else
	#define C24R					2
	#define C24G					1
	#define C24B					0
	#endif

	class LgiClass Pixel24
	{
	public:
		static int Size;
	
		uchar b, g, r;
		
		Pixel24 *Next() { return this + 1; }
	};

	#ifndef __BIG_ENDIAN__
	#define C32B					0
	#define C32G					1
	#define C32R					2
	#define C32A					3
	#else
	#define C32B					3
	#define C32G					2
	#define C32R					1
	#define C32A					0
	#endif

	class LgiClass Pixel32
	{
	public:
		static int Size;

		uchar b, g, r, a;
		
		Pixel32 *Next() { return this + 1; }
	};

#elif defined LINUX

	#ifndef __BIG_ENDIAN__
	#define C24B					0
	#define C24G					1
	#define C24R					2
	#else
	#define C24B					2
	#define C24G					1
	#define C24R					0
	#endif

	class LgiClass Pixel24
	{
	public:
		static int Size;

		uchar b, g, r;

		Pixel24 *Next() { return (Pixel24*) ((char*)this + Size); }
	};

	#ifndef __BIG_ENDIAN__
	#define C32B					0
	#define C32G					1
	#define C32R					2
	#define C32A					3
	#else
	#define C32B					3
	#define C32G					2
	#define C32R					1
	#define C32A					0
	#endif

	class LgiClass Pixel32
	{
	public:
		static int Size;

		uchar b, g, r, a;

		Pixel32 *Next() { return this + 1; }
	};

#elif defined BEOS

	#ifndef __BIG_ENDIAN__
	#define C24B					0
	#define C24G					1
	#define C24R					2
	#else
	#define C24B					2
	#define C24G					1
	#define C24R					0
	#endif

	class LgiClass Pixel24
	{
	public:
		static int Size;

		uchar b, g, r;

		Pixel24 *Next() { return this + 1; }
	};

	#ifndef __BIG_ENDIAN__
	#define C32B					0
	#define C32G					1
	#define C32R					2
	#define C32A					3
	#else
	#define C32B					3
	#define C32G					2
	#define C32R					1
	#define C32A					0
	#endif

	class LgiClass Pixel32
	{
	public:
		static int Size;

		uchar b, g, r, a;

		Pixel32 *Next() { return this + 1; }
	};

#elif defined MAC

	#ifdef __BIG_ENDIAN__
	#define C24B					2
	#define C24G					1
	#define C24R					0
	#else
	#define C24B					0
	#define C24G					1
	#define C24R					2
	#endif

	class LgiClass Pixel24
	{
	public:
		static int Size;

		uchar r, g, b;

		Pixel24 *Next() { return this + 1; }
	};

	
	#ifdef __BIG_ENDIAN__
	#define C32A					0
	#define C32B					1
	#define C32G					2
	#define C32R					3
	#else
	#define C32A					3
	#define C32B					2
	#define C32G					1
	#define C32R					0
	#endif

	class LgiClass Pixel32
	{
	public:
		static int Size;
		uchar r, g, b, a;

		Pixel32 *Next() { return this + 1; }
	};

#endif

typedef union
{
	uint8 *u8;
	int i;
	
	struct Pixel16
	{
	    uint16 r : 5;
	    uint16 g : 6;
	    uint16 b : 5;
	} *px16;
	
	Pixel24 *px24;
	Pixel32 *px32;

}	GPixelPtr;

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
/// Returns true if the hue is undefined in a HLS color
/// \sa #Hls32
#define HlsIsUndefined(Hls)			(H32(Hls) == HUE_UNDEFINED)

// Look up tables
#define Div255Lut					(GdcDevice::GetInst()->GetDiv255())

/// Converts a rgb24 colour to a hls32
/// \sa #Hls32
LgiFunc COLOUR RgbToHls(COLOUR Rgb24);
/// Converts a hls32 colour to a rgb24
/// \sa #Hls32
LgiFunc COLOUR HlsToRgb(COLOUR Hsl32);

#ifdef WIN32
#pragma pack(pop, before_pack)
#endif

//				Classes
class GFilter;
class GSurface;

#include "GRect.h"
#include "GFont.h"

/// 2d Point
class LgiClass GdcPt2
{
public:
	int x, y;

	GdcPt2(int Ix = 0, int Iy = 0)
	{
		x = Ix;
		y = Iy;
	}

	GdcPt2(const GdcPt2 &p)
	{
		x = p.x;
		y = p.y;
	}

	bool Inside(GRect &r)
	{
		return	(x >= r.x1) AND
				(x <= r.x2) AND
				(y >= r.y1) AND
				(y <= r.y2);
	}
};

/// 3d Point
class LgiClass GdcPt3
{
public:
	int x, y, z;
};

#include "GColour.h"

class LgiClass GBmpMem
{
public:
	uchar *Base;
	int x, y, Bits, Line;
	int Flags;

	GBmpMem();
	~GBmpMem();
};


#define GAPP_ALPHA_A			1
#define GAPP_ALPHA_PAL			2
#define GAPP_BACKGROUND			3
#define GAPP_ANGLE				4
#define GAPP_BOUNDS				5

/// \brief Class to draw onto a memory bitmap
///
/// This class assumes that all clipping is done by the layer above.
/// It can then implement very simple loops to do the work of filling
/// pixels
class LgiClass GApplicator
{
protected:
	GBmpMem *Dest;
	GBmpMem *Alpha;
	GPalette *Pal;
	int Op;

public:
	COLOUR c;				// main colour

	GApplicator() { c = 0; }
	GApplicator(COLOUR Colour) { c = Colour; }
	virtual ~GApplicator() { }

	/// Get a parameter
	virtual int GetVar(int Var) { return 0; }
	/// Set a parameter
	virtual int SetVar(int Var, NativeInt Value) { return 0; }

	/// Sets the operator
	void SetOp(int o) { Op = o; }
	/// Gets the operator
	int GetOp() { return Op; }
	/// Gets the bit depth
	int GetBits() { return (Dest) ? Dest->Bits : 0; }
	/// Gets the flags in operation
	int GetFlags() { return (Dest) ? Dest->Flags : 0; }
	/// Gets the palette
	GPalette *GetPal() { return Pal; }

	/// Sets the bitmap to write onto
	virtual bool SetSurface(GBmpMem *d, GPalette *p = 0, GBmpMem *a = 0) = 0; // sets Dest, returns FALSE on error
	/// Sets the current position to an x,y
	virtual void SetPtr(int x, int y) = 0;			// calculates Ptr from x, y
	/// Moves the current position one pixel left
	virtual void IncX() = 0;
	/// Moves the current position one scanline down
	virtual void IncY() = 0;
	/// Offset the current position
	virtual void IncPtr(int X, int Y) = 0;

	/// Sets the pixel at the current location with the current colour
	virtual void Set() = 0;
	/// Gets the colour of the pixel at the current location
	virtual COLOUR Get() = 0;
	/// Draws a vertical line from the current position down 'height' scanlines
	virtual void VLine(int height) = 0;
	/// Draws a rectangle starting from the current position, 'x' pixels across and 'y' pixels down
	virtual void Rectangle(int x, int y) = 0;
	/// Copies bitmap data to the current position
	virtual bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0) = 0;
};

/// Creates applications from parameters.
class LgiClass GApplicatorFactory
{
public:
	GApplicatorFactory();
	virtual ~GApplicatorFactory();

	/// Find the application factory and create the appropriate object.
	static GApplicator *NewApp(int Bits, int Op);
	virtual GApplicator *Create(int Bits, int Op) { return NULL; }
};

class LgiClass GApp15 : public GApplicatorFactory
{
public:
	GApplicator *Create(int Bits, int Op);
};

class LgiClass GApp16 : public GApplicatorFactory
{
public:
	GApplicator *Create(int Bits, int Op);
};

class LgiClass GApp24 : public GApplicatorFactory
{
public:
	GApplicator *Create(int Bits, int Op);
};

class LgiClass GApp32 : public GApplicatorFactory
{
public:
	GApplicator *Create(int Bits, int Op);
};

class LgiClass GApp8 : public GApplicatorFactory
{
public:
	GApplicator *Create(int Bits, int Op);
};

class GAlphaFactory : public GApplicatorFactory
{
public:
	GApplicator *Create(int Bits, int Op);
};

#define OrgX(x)			x -= OriginX
#define OrgY(y)			y -= OriginY
#define OrgXy(x, y)		x -= OriginX; y -= OriginY
#define OrgPt(p)		p.x -= OriginX; p.y -= OriginY
#define OrgRgn(r)		r.Offset(-OriginX, -OriginY)

/// Base class API for graphics operations
class LgiClass GSurface
{
	friend class GFilter;
	friend class GView;
	friend class GWindow;
	friend class GVariant;

	void Init();
	
	int             _Refs;

protected:
	int				Flags;
	int				PrevOp;
	GRect			Clip;
	GBmpMem			*pMem;
	GSurface		*pAlphaDC;
	GPalette		*pPalette;
	GApplicator		*pApp;
	GApplicator		*pAppCache[GDC_CACHE_SIZE];
	int				OriginX, OriginY;

	// Protected functions
	GApplicator		*CreateApplicator(int Op = GDC_SET, int Bits = 0);
	uint32		LineBits;
	uint32		LineMask;
	uint32		LineReset;

	#if WIN32NATIVE
	OsPainter	hDC;
	OsBitmap	hBmp;
	#elif defined __GTK_H__
	// Gtk::cairo_surface_t *Surface;
	Gtk::cairo_t *Cairo;
	#endif

public:
	GSurface();
	GSurface(GSurface *pDC);
	virtual ~GSurface();

	// Win32
	#if defined(__GTK_H__)

	/// Returns the drawable for the surface
	virtual Gtk::GdkDrawable *GetDrawable() { return 0; }

	// Returns a cairo surface for the drawing context, creating one if required.
	// The surface is owned by this GSurface and will be free'd when deleted.
	// virtual Gtk::cairo_surface_t *GetSurface(bool Render) { return 0; }
	
	/// Returns the cairo drawing context, creating one if required.
	/// The context is owned by this GSurface and will be free'd when deleted.
	virtual Gtk::cairo_t *GetCairo() { return Cairo; }

	/// Gets the drawable size, regardless of clipping or client rect
	virtual GdcPt2 GetSize() = 0;

	#elif defined(WIN32)

	virtual HDC StartDC() { return hDC; }
	virtual void EndDC() {}
	
	#elif defined MAC
	
	virtual CGColorSpaceRef GetColourSpace() { return 0; }
	
	#endif

	virtual OsBitmap GetBitmap();
	virtual OsPainter Handle();
	virtual void SetClient(GRect *c) { }

	// Creation
	virtual bool Create(int x, int y, int Bits, int LineLen = 0, bool KeepData = false) { return false; }
	virtual void Update(int Flags) {}

	// Alpha channel	
	/// Returns true if this Surface has an alpha channel
	virtual bool HasAlpha() { return pAlphaDC != 0; }
	/// Creates or destroys the alpha channel for this surface
	virtual bool HasAlpha(bool b);
	/// Returns true if we are drawing on the alpha channel
	bool DrawOnAlpha() { return ((Flags & GDC_DRAW_ON_ALPHA) != 0); }
	/// True if you want to edit the alpha channel rather than the colour bits
	bool DrawOnAlpha(bool Draw);
	/// Returns the surface of the alpha channel.
	GSurface *AlphaDC() { return pAlphaDC; }

	// Applicator
	virtual bool Applicator(GApplicator *pApp);
	virtual GApplicator *Applicator();

	// Palette
	virtual GPalette *Palette();
	virtual void Palette(GPalette *pPal, bool bOwnIt = true);

	// Clip region
	virtual GRect ClipRgn(GRect *Rgn);
	virtual GRect ClipRgn();

	/// Gets the current colour
	virtual COLOUR Colour() { return pApp->c; }
	/// Sets the current colour
	virtual COLOUR Colour
	(
		/// The new colour
		COLOUR c,
		/// The bit depth of the new colour or 0 to indicate the depth is the same as the current Surface
		int Bits = 0
	);
	/// Sets the current colour
	virtual GColour Colour
	(
		/// The new colour
		GColour c
	);
	/// Gets the current blending mode in operation
	virtual int Op() { return (pApp) ? pApp->GetOp() : 0; }
	/// Sets the current blending mode in operation
	/// \sa GDC_SET, GDC_AND, GDC_OR, GDC_XOR and GDC_ALPHA
	virtual int Op(int Op);
	/// Gets the width in pixels
	virtual int X() { return (pMem) ? pMem->x : 0; }
	/// Gets the height in pixels
	virtual int Y() { return (pMem) ? pMem->y : 0; }
	/// Gets the bounds of the image as a GRect
	GRect Bounds() { return GRect(0, 0, X()-1, Y()-1); }
	/// Gets the length of a scanline in bytes
	virtual int GetLine() { return (pMem) ? pMem->Line : 0; }
	/// Returns the horizontal resolution of the device
	virtual int DpiX() { return 100; }
	/// Returns the vertical resolution of the device
	virtual int DpiY() { return 100; }
	/// Gets the bits per pixel
	virtual int GetBits() { return (pMem) ? pMem->Bits : 0; }
	/// Gets the bytes per pixels
	virtual int PixelSize() { return GetBits() == 24 ? Pixel24::Size : GetBits() >> 3; }
	virtual int GetFlags() { return Flags; }
	/// Returns true if the surface is on the screen
	virtual class GScreenDC *IsScreen() { return 0; }
	/// Returns true if the surface is for printing
	virtual bool IsPrint() { return false; }
	/// Returns a pointer to the start of a scanline, or NULL if not available
	virtual uchar *operator[](int y);

	/// Gets the surface origin
	virtual void GetOrigin(int &x, int &y) { x = OriginX; y = OriginY; }
	/// Sets the surface origin
	virtual void SetOrigin(int x, int y) { OriginX = x; OriginY = y; }
	
	/// Sets a pixel with the current colour
	virtual void Set(int x, int y);
	/// Gets a pixel (doesn't work on some types of image, i.e. GScreenDC)
	virtual COLOUR Get(int x, int y);

	// Line

	/// Draw a horizontal line in the current colour
	virtual void HLine(int x1, int x2, int y);
	/// Draw a vertical line in the current colour
	virtual void VLine(int x, int y1, int y2);
	/// Draw a line in the current colour
	virtual void Line(int x1, int y1, int x2, int y2);
	
	virtual uint LineStyle(uint32 Bits, uint32 Reset = 0x80000000)
	{
		uint32 B = LineBits;
		LineBits = Bits;
		LineMask = LineReset = Reset;
		return B;
	}
	virtual uint LineStyle() { return LineBits; }

	// Curve

	/// Stroke a circle in the current colour
	virtual void Circle(double cx, double cy, double radius);
	/// Fill a circle in the current colour
	virtual void FilledCircle(double cx, double cy, double radius);
	/// Stroke an arc in the current colour
	virtual void Arc(double cx, double cy, double radius, double start, double end);
	/// Fill an arc in the current colour
	virtual void FilledArc(double cx, double cy, double radius, double start, double end);
	/// Stroke an ellipse in the current colour
	virtual void Ellipse(double cx, double cy, double x, double y);
	/// Fill an ellipse in the current colour
	virtual void FilledEllipse(double cx, double cy, double x, double y);

	// Rectangular

	/// Stroke a rectangle in the current colour
	virtual void Box(int x1, int y1, int x2, int y2);
	/// Stroke a rectangle in the current colour
	virtual void Box
	(
		/// The rectangle, or NULL to stroke the edge of the entire surface
		GRect *a = NULL
	);
	/// Fill a rectangle in the current colour
	virtual void Rectangle(int x1, int y1, int x2, int y2);
	/// Fill a rectangle in the current colour
	virtual void Rectangle
	(
		/// The rectangle, or NULL to fill the entire surface
		GRect *a = NULL
	);
	/// Copy an image onto the surface
	virtual void Blt
	(
		/// The destination x coord
		int x,
		/// The destination y coord
		int y,
		/// The source surface
		GSurface *Src,
		/// The optional area of the source to use, if not specified the whole source is used
		GRect *a = NULL
	);
	/// Not implemented
	virtual void StretchBlt(GRect *d, GSurface *Src, GRect *s);

	// Other

	/// Fill a polygon in the current colour
	virtual void Polygon(int Points, GdcPt2 *Data);
	/// Stroke a bezier in the current colour
	virtual void Bezier(int Threshold, GdcPt2 *Pt);
	/// Flood fill in the current colour (doesn't work on a GScreenDC)
	virtual void FloodFill
	(
		/// Start x coordinate
		int x,
		/// Start y coordinate
		int y,
		/// Use #GDC_FILL_TO_DIFFERENT, #GDC_FILL_TO_BORDER or #GDC_FILL_NEAR
		int Mode,
		/// Fill colour
		COLOUR Border = 0,
		/// The bounds of the filled area or NULL if you don't care
		GRect *Bounds = NULL
	);
};

/// \brief An implemenation of GSurface to draw onto the screen.
///
/// This is the class given to GView::OnPaint() most of the time. Which most of
/// the time doesn't matter unless your doing something unusual.
class LgiClass GScreenDC : public GSurface
{
	class GScreenPrivate *d;

public:
	GScreenDC();
	virtual ~GScreenDC();

	// OS Sepcific
	#if WIN32NATIVE

		GScreenDC(GViewI *view);
		GScreenDC(HWND hwnd);
		GScreenDC(HDC hdc, HWND hwnd, bool Release = false);
		GScreenDC(HBITMAP hBmp, int Sx, int Sy);

		bool Create(HDC hdc);
		void SetSize(int x, int y);

	#else

		/// Construct a wrapper to draw on a window
		GScreenDC(GView *view, void *Param = 0);
		
		#if defined MAC
		
		GScreenDC(GWindow *wnd, void *Param = 0);
		GRect GetPos();
		
		#else // GTK
		
		/// Constructs a server size pixmap
		GScreenDC(int x, int y, int bits);		
		/// Constructs a wrapper around a drawable
		GScreenDC(Gtk::GdkDrawable *Drawable);
		/// Constructs a DC for drawing on a window
		GScreenDC(OsView View);
		
		Gtk::GdkDrawable *GetDrawable();
		Gtk::cairo_t *GetCairo();
		// Gtk::cairo_surface_t *GetSurface(bool Render);
		GdcPt2 GetSize();
		
		#endif

		GView *GetView();
		OsPainter Handle();
		int GetFlags();
		GRect *GetClient();

	#endif

	// Properties
	void GetOrigin(int &x, int &y);
	void SetOrigin(int x, int y);

	GRect ClipRgn();
	GRect ClipRgn(GRect *Rgn);
	void SetClient(GRect *c);

	COLOUR Colour();
	COLOUR Colour(COLOUR c, int Bits = 0);
	GColour Colour(GColour c);

	int Op();
	int Op(int Op);

	int X();
	int Y();

	GPalette *Palette();
	void Palette(GPalette *pPal, bool bOwnIt = true);

	uint LineStyle();
	uint LineStyle(uint Bits, uint32 Reset = 0x80000000);

	int GetBits();
	GScreenDC *IsScreen() { return this; }
	uchar *operator[](int y) { return NULL; }

	// Primitives
	void Set(int x, int y);
	COLOUR Get(int x, int y);
	void HLine(int x1, int x2, int y);
	void VLine(int x, int y1, int y2);
	void Line(int x1, int y1, int x2, int y2);
	void Circle(double cx, double cy, double radius);
	void FilledCircle(double cx, double cy, double radius);
	void Arc(double cx, double cy, double radius, double start, double end);
	void FilledArc(double cx, double cy, double radius, double start, double end);
	void Ellipse(double cx, double cy, double x, double y);
	void FilledEllipse(double cx, double cy, double x, double y);
	void Box(int x1, int y1, int x2, int y2);
	void Box(GRect *a);
	void Rectangle(int x1, int y1, int x2, int y2);
	void Rectangle(GRect *a = NULL);
	void Blt(int x, int y, GSurface *Src, GRect *a = NULL);
	void StretchBlt(GRect *d, GSurface *Src, GRect *s = NULL);
	void Polygon(int Points, GdcPt2 *Data);
	void Bezier(int Threshold, GdcPt2 *Pt);
	void FloodFill(int x, int y, int Mode, COLOUR Border = 0, GRect *Bounds = NULL);
};

/// \brief Blitting region helper class, can calculate the right source and dest rectangles
/// for a blt operation including propagating clipping back to the source rect.
class GBlitRegions
{
	// Raw image bounds
	GRect SrcBounds;
	GRect DstBounds;
	
	// Unclipped blit regions
	GRect SrcBlt;
	GRect DstBlt;

public:
	/// Clipped blit region in destination co-ords
	GRect SrcClip;
	/// Clipped blit region in source co-ords
	GRect DstClip;

	/// Calculate the rectangles.
	GBlitRegions
	(
		/// Destination surface
		GSurface *Dst,
		/// Destination blt x offset
		int x1,
		/// Destination blt y offset
		int y1,
		/// Source surface
		GSurface *Src,
		/// [Optional] Crop the source surface first, else whole surface is blt
		GRect *SrcRc = 0
	)
	{
		// Calc full image bounds
		if (Src) SrcBounds.Set(0, 0, Src->X()-1, Src->Y()-1);
		else SrcBounds.ZOff(-1, -1);
		if (Dst) DstBounds.Set(0, 0, Dst->X()-1, Dst->Y()-1);
		else DstBounds.ZOff(-1, -1);
		
		// Calc full sized blt regions
		if (SrcRc)
		{
			SrcBlt = *SrcRc;
			SrcBlt.Bound(&SrcBounds);
		}
		else SrcBlt = SrcBounds;

		DstBlt = SrcBlt;
		DstBlt.Offset(x1-DstBlt.x1, y1-DstBlt.y1);

		// Dest clipped to dest bounds
		DstClip = DstBlt;
		DstClip.Bound(&DstBounds);
		
		// Now map the dest clipping back to the source
		SrcClip = SrcBlt;
		SrcClip.x1 += DstClip.x1 - DstBlt.x1;
		SrcClip.y1 += DstClip.y1 - DstBlt.y1;
		SrcClip.x2 -= DstBlt.x2 - DstClip.x2;
		SrcClip.y2 -= DstBlt.y2 - DstClip.y2;
	}

	/// Returns non-zero if both clipped rectangles are valid.
	bool Valid()
	{
		return DstClip.Valid() && SrcClip.Valid();
	}
	
	void Dump()
	{
		printf("SrcBounds: %s\n", SrcBounds.GetStr());
		printf("DstBounds: %s\n", DstBounds.GetStr());
		
		printf("SrcBlt: %s\n", SrcBlt.GetStr());
		printf("DstBlt: %s\n", DstBlt.GetStr());

		printf("SrcClip: %s\n", SrcClip.GetStr());
		printf("DstClip: %s\n", DstClip.GetStr());
	}
};

#ifdef MAC
class CGImg
{
	class CGImgPriv *d;

	void Create(int x, int y, int Bits, int Line, uchar *data, uchar *palette, GRect *r);

public:
	CGImg(int x, int y, int Bits, int Line, uchar *data, uchar *palette, GRect *r);
	CGImg(GSurface *pDC);
	~CGImg();
	
	operator CGImageRef();
};
#endif

/// \brief An implemenation of GSurface to draw into a memory bitmap.
///
/// This class uses a block of memory to represent an image. You have direct
/// pixel access as well as higher level functions to manipulate the bits.
class LgiClass GMemDC : public GSurface
{
protected:
	class GMemDCPrivate *d;

	#if defined WIN32
	PBITMAPINFO	GetInfo();
	#endif

public:
	/// Creates a memory bitmap
	GMemDC
	(
		/// The width
		int x = 0,
		/// The height
		int y = 0,
		/// The bit depth of the pixels
		int bits = 0
	);
	GMemDC(GSurface *pDC);
	virtual ~GMemDC();

	#if WIN32NATIVE
	
		HDC StartDC();
		void EndDC();
		void Update(int Flags);
		void UpsideDown(bool upsidedown);
		
		// need this to tell the HDC about the
		// new clipping region
		GRect ClipRgn(GRect *Rgn);

	#else

		GRect ClipRgn() { return Clip; }

		#if defined MAC
				
		OsPainter Handle();
		OsBitmap GetBitmap();
		CGColorSpaceRef GetColourSpace();
		CGImg *GetImg(GRect *Sub = 0);
		GRect ClipRgn(GRect *Rgn);
		
		#else // GTK

		Gtk::GdkImage *GetImage();
		GdcPt2 GetSize();
		Gtk::cairo_t *GetCairo();

		#endif
		
	#endif

	void SetClient(GRect *c);

	/// Locks the bits for access. GMemDC's start in the locked state.
	bool Lock();
	/// Unlocks the bits to optimize for display. While the bitmap is unlocked you
	/// can't access the data for read or write. On linux this converts the XImage
	/// to pixmap. On other systems it doesn't do much. As a general rule if you
	/// don't need access to a bitmap after creating / loading it then unlock it.
	bool Unlock();
	
	void SetOrigin(int x, int y);
	void Empty();

	bool Create(int x, int y, int Bits, int LineLen = 0, bool KeepData = false);
	void Blt(int x, int y, GSurface *Src, GRect *a = NULL);
	void StretchBlt(GRect *d, GSurface *Src, GRect *s = NULL);

	void HLine(int x1, int x2, int y, COLOUR a, COLOUR b);
	void VLine(int x, int y1, int y2, COLOUR a, COLOUR b);
};

/// \brief An implemenation of GSurface to print to a printer.
///
/// This class redirects standard graphics calls to print a page.
///
/// \sa GPrinter
class LgiClass GPrintDC
#if defined WIN32
	: public GScreenDC
#else
	: public GSurface
#endif
{
	#ifdef __GTK_H__
	friend class PrintPainter;
	double Xc(int x);
	double Yc(int y);
	#endif

	class GPrintDCPrivate *d;

public:
	GPrintDC(void *Handle, const char *PrintJobName);
	~GPrintDC();

	bool IsPrint() { return true; }

	int X();
	int Y();
	int GetBits();

	/// Returns the horizontal DPI of the printer or 0 on error
	int DpiX();

	/// Returns the vertical DPI of the printer or 0 on error
	int DpiY();

	/// Call before any graphics commands to start a page.
	bool StartPage();
	/// Call to finish a page after drawing some graphics.
	void EndPage();

	#if defined __GTK_H__

	OsPainter Handle();
	COLOUR Colour();
	COLOUR Colour(COLOUR c, int Bits = 0);
	GColour Colour(GColour c);
	GdcPt2 GetSize() { return GdcPt2(X(), Y()); }

	void Set(int x, int y);
	COLOUR Get(int x, int y);
	void HLine(int x1, int x2, int y);
	void VLine(int x, int y1, int y2);
	void Line(int x1, int y1, int x2, int y2);
	void Circle(double cx, double cy, double radius);
	void FilledCircle(double cx, double cy, double radius);
	void Arc(double cx, double cy, double radius, double start, double end);
	void FilledArc(double cx, double cy, double radius, double start, double end);
	void Ellipse(double cx, double cy, double x, double y);
	void FilledEllipse(double cx, double cy, double x, double y);
	void Box(int x1, int y1, int x2, int y2);
	void Box(GRect *a = NULL);
	void Rectangle(int x1, int y1, int x2, int y2);
	void Rectangle(GRect *a = NULL);
	void Blt(int x, int y, GSurface *Src, GRect *a = NULL);
	void StretchBlt(GRect *d, GSurface *Src, GRect *s);
	void Polygon(int Points, GdcPt2 *Data);
	void Bezier(int Threshold, GdcPt2 *Pt);
	void FloodFill(int x, int y, int Mode, COLOUR Border = 0, GRect *Bounds = NULL);
	
	#endif

};

//////////////////////////////////////////////////////////////////////////////
class LgiClass GGlobalColour
{
	class GGlobalColourPrivate *d;

public:
	GGlobalColour();
	~GGlobalColour();

	// Add all the colours first
	COLOUR AddColour(COLOUR c24);
	bool AddBitmap(GSurface *pDC);
	bool AddBitmap(GImageList *il);

	// Then call this
	bool MakeGlobalPalette();

	// Which will give you a palette that
	// includes everything
	GPalette *GetPalette();

	// Convert a bitmap to the global palette
	COLOUR GetColour(COLOUR c24);
	bool RemapBitmap(GSurface *pDC);
};

#ifdef WIN32
typedef int (__stdcall *MsImg32_AlphaBlend)(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
#endif

/// Main singleton graphics device class. Holds all global data for graphics rendering.
class LgiClass GdcDevice : public GCapabilityClient
{
	friend class GScreenDC;
	friend class GImageList;

	static GdcDevice *pInstance;
	class GdcDevicePrivate *d;
	
	#ifdef WIN32
	MsImg32_AlphaBlend AlphaBlend;
	#endif

public:
	GdcDevice();
	~GdcDevice();
	static GdcDevice *GetInst() { return pInstance; }

	/// Returns the current screen bit depth
	int GetBits();
	/// Returns the current screen width
	int X();
	/// Returns the current screen height
	int Y();

	GGlobalColour *GetGlobalColour();

	/// Set a global graphics option
	int GetOption(int Opt);
	/// Get a global graphics option
	int SetOption(int Opt, int Value);

	/// 256 lut for squares
	ulong *GetCharSquares();
	/// Divide by 255 lut, 64k entries long.
	uchar *GetDiv255();

	// Palette/Colour
	void SetGamma(double Gamma);
	double GetGamma();

	// Palette
	void SetSystemPalette(int Start, int Size, GPalette *Pal);
	GPalette *GetSystemPalette();
	void SetColourPaletteType(int Type);	// Type = PALTYPE_xxx define
	COLOUR GetColour(COLOUR Rgb24, GSurface *pDC = NULL);

    // File I/O
    
    /// \brief Loads a image from a file
    ///
    /// This function uses the compiled in codecs, some of which require external
    /// shared libraries / DLL's to function. Other just need the right source to
    /// be compiled in.
    ///
    /// Lgi comes with the following image codecs:
    /// <ul>
    ///  <li> Windows or OS/2 Bitmap: GdcBmp (GFilter.cpp)
    ///  <li> PCX: GdcPcx (Pcx.cpp)
    ///  <li> GIF: GdcGif (Gif.cpp and Lzw.cpp)
    ///  <li> JPEG: GdcJpeg (Jpeg.cpp + <a href='http://www.memecode.com/scribe/extras.php'>libjpeg</a> library)
    ///  <li> PNG: GdcPng (Png.cpp + <a href='http://www.memecode.com/scribe/extras.php'>libpng</a> library)
    /// </ul>
    /// 
    GSurface *Load
    (
        /// The full path of the file
        char *FileName,
    	/// [Optional] Enable OS based loaders
        bool UseOSLoader = true
    );
    
    /// Save an image to a file.
    bool Save
    (
        /// The file to write to
        char *Name,
        /// The pixels to store
        GSurface *pDC
    );
};

/// \brief Defines a bitmap inline in C++ code.
///
/// The easiest way I know of create the raw data for an GInlineBmp
/// is to use <a href='http://www.memecode.com/image.php'>i.Mage</a> to
/// load a file or create a image and then use the Edit->Copy As Code
/// menu. Then paste into your C++ and put a uint32 array declaration
/// around it. Then point the Data member to the uint32 array. Just be
/// sure to get the dimensions right.
///
/// I use this for embeding resource images directly into the code so
/// that a) they load instantly and b) they can't get lost as a separate file.
class LgiClass GInlineBmp
{
public:
	/// The width of the image.
	int X;
	/// The height of the image.
	int Y;
	/// The bitdepth of the image (8, 15, 16, 24, 32).
	int Bits;
	/// Pointer to the raw data.
	uint32 *Data;

	/// Creates a memory DC of the image.
	GSurface *Create();
};

// file filter support
#include "GFilter.h"

// globals
#define GdcD			GdcDevice::GetInst()

/// Converts a context to a different bit depth
LgiFunc GSurface *ConvertDC
(
	/// The source image
	GSurface *pDC,
	/// The destination bit depth
	int Bits
);

/// Wrapper around GdcDevice::Load
/// \deprecated Use GdcDevice::Load directly in new code.
LgiFunc GSurface *LoadDC(char *Name, bool UseOSLoader = true);

/// Wrapper around GdcDevice::Save
/// \deprecated Use GdcDevice::Save directly in new code.
LgiFunc bool WriteDC(char *Name, GSurface *pDC);

/// Converts a colour to a different bit depth
LgiFunc COLOUR CBit(int DstBits, COLOUR c, int SrcBits = 24, GPalette *Pal = 0);

#ifdef __cplusplus
/// blends 2 colours by the amount specified
LgiClass GColour GdcMixColour(GColour a, GColour b, float HowMuchA = 0.5);
#endif

/// blends 2 24bit colours by the amount specified
LgiFunc COLOUR GdcMixColour(COLOUR a, COLOUR b, float HowMuchA = 0.5);

/// Turns a colour into an 8 bit grey scale representation
LgiFunc COLOUR GdcGreyScale(COLOUR c, int Bits = 24);

/// Colour reduction option to define what palette to go to
enum GColourReducePalette
{
	CR_PAL_NONE = -1,
    CR_PAL_CUBE = 0,
    CR_PAL_OPT,
    CR_PAL_FILE
};

/// Colour reduction option to define how to deal with reduction error
enum GColourReduceMatch
{
	CR_MATCH_NONE = -1,
    CR_MATCH_NEAR = 0,
    CR_MATCH_HALFTONE,
    CR_MATCH_ERROR
};

/// Colour reduction options
class GReduceOptions
{
public:
	/// Type of palette
	GColourReducePalette PalType;
	
	/// Reduction error handling
	GColourReduceMatch MatchType;
	
	/// 1-256
	int Colours;
	
	/// Specific palette to reduce to
	GPalette *Palette;

	GReduceOptions()
	{
		Palette = 0;
		Colours = 256;
		PalType = CR_PAL_NONE;
		MatchType = CR_MATCH_NONE;
	}
};

/// Reduces a images colour depth
LgiFunc bool GReduceBitDepth(GSurface *pDC, int Bits, GPalette *Pal = 0, GReduceOptions *Reduce = 0);

struct GColourStop
{
	COLOUR Colour;
	double Pos;
};

/// Draws a horizontal or vertical gradient
LgiFunc void LgiFillGradient(GSurface *pDC, GRect &r, bool Vert, GArray<GColourStop> &Stops);

#ifdef WIN32
/// Draws a windows HICON onto a surface at Dx, Dy
LgiFunc void LgiDrawIcon(GSurface *pDC, int Dx, int Dy, HICON ico);
#endif

#endif
