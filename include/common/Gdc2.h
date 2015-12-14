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
#include "GRefCount.h"

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

// GSurface Flags
#define GDC_ON_SCREEN				0x0002
#define GDC_ALPHA_CHANNEL			0x0004
#define GDC_UPDATED_PALETTE			0x0008
#define GDC_CAPTURE_CURSOR			0x0010
#define GDC_OWN_APPLICATOR			0x0020
#define GDC_CACHED_APPLICATOR		0x0040
#define GDC_OWN_PALETTE				0x0080
#define GDC_DRAW_ON_ALPHA			0x0100

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
#define BMPWIDTH(bits)				((((bits)+31)/32)<<2)

#include "GColourSpace.h"

// Look up tables
#define Div255Lut					(GdcDevice::GetInst()->GetDiv255())

//				Classes
class GFilter;
class GSurface;

#include "GRect.h"
#include "GFont.h"
#include "GPoint.h"
#include "GColour.h"

class LgiClass GBmpMem
{
public:
	enum GdcMemFlags
	{
		BmpOwnMemory = 0x1,
		BmpPreMulAlpha = 0x2,
	};

	uchar *Base;
	int x, y, Line;
	GColourSpace Cs;
	int Flags;

	GBmpMem();
	~GBmpMem();
	
	bool PreMul()
	{
		return (Flags & BmpPreMulAlpha) != 0;
	}

	bool PreMul(bool set)
	{
		if (set) Flags |= BmpPreMulAlpha;
		else Flags &= ~BmpPreMulAlpha;
		return PreMul();
	}

	bool OwnMem()
	{
		return (Flags & BmpOwnMemory) != 0;
	}

	bool OwnMem(bool set)
	{
		if (set) Flags |= BmpOwnMemory;
		else Flags &= ~BmpOwnMemory;
		return OwnMem();
	}

	int GetBits()
	{
		return GColourSpaceToBits(Cs);
	}
	
	void GetMemoryExtents(uchar *&Start, uchar *&End)
	{
		if (Line < 0)
		{
			Start = Base + (y * Line);
			End = Base + Line;
		}
		else
		{
			Start = Base;
			End = Base + (y * Line);
		}
	}
	
	bool Overlap(GBmpMem *Mem)
	{
		uchar *ThisStart, *ThisEnd;
		GetMemoryExtents(ThisStart, ThisEnd);

		uchar *MemStart, *MemEnd;
		GetMemoryExtents(MemStart, MemEnd);
		
		if (ThisEnd < MemStart)
			return false;
		if (ThisStart > MemEnd)
			return false;

		return true;
	}
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
	union
	{
		COLOUR c;				// main colour
		System24BitPixel p24;
		System32BitPixel p32;
	};

	GApplicator() { c = 0; Dest = NULL; Alpha = NULL; Pal = NULL; }
	GApplicator(COLOUR Colour) { c = Colour; }
	virtual ~GApplicator() { }

	virtual const char *GetClass() { return "GApplicator"; }

	/// Get a parameter
	virtual int GetVar(int Var) { return 0; }
	/// Set a parameter
	virtual int SetVar(int Var, NativeInt Value) { return 0; }

	GColourSpace GetColourSpace() { return Dest ? Dest->Cs : CsNone; }

	/// Sets the operator
	void SetOp(int o, int Param = -1) { Op = o; }
	/// Gets the operator
	int GetOp() { return Op; }
	/// Gets the bit depth
	int GetBits() { return (Dest) ? GColourSpaceToBits(Dest->Cs) : 0; }
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
	static GApplicator *NewApp(GColourSpace Cs, int Op);
	virtual GApplicator *Create(GColourSpace Cs, int Op) = 0;
};

class LgiClass GApp15 : public GApplicatorFactory
{
public:
	GApplicator *Create(GColourSpace Cs, int Op);
};

class LgiClass GApp16 : public GApplicatorFactory
{
public:
	GApplicator *Create(GColourSpace Cs, int Op);
};

class LgiClass GApp24 : public GApplicatorFactory
{
public:
	GApplicator *Create(GColourSpace Cs, int Op);
};

class LgiClass GApp32 : public GApplicatorFactory
{
public:
	GApplicator *Create(GColourSpace Cs, int Op);
};

class LgiClass GApp8 : public GApplicatorFactory
{
public:
	GApplicator *Create(GColourSpace Cs, int Op);
};

class GAlphaFactory : public GApplicatorFactory
{
public:
	GApplicator *Create(GColourSpace Cs, int Op);
};

#define OrgX(x)			x -= OriginX
#define OrgY(y)			y -= OriginY
#define OrgXy(x, y)		x -= OriginX; y -= OriginY
#define OrgPt(p)		p.x -= OriginX; p.y -= OriginY
#define OrgRgn(r)		r.Offset(-OriginX, -OriginY)

/// Base class API for graphics operations
class LgiClass GSurface : public GRefCount, public GDom
{
	friend class GFilter;
	friend class GView;
	friend class GWindow;
	friend class GVariant;

	void Init();
	
protected:
	int				Flags;
	int				PrevOp;
	GRect			Clip;
	GColourSpace	ColourSpace;
	GBmpMem			*pMem;
	GSurface		*pAlphaDC;
	GPalette		*pPalette;
	GApplicator		*pApp;
	GApplicator		*pAppCache[GDC_CACHE_SIZE];
	int				OriginX, OriginY;

	// Protected functions
	GApplicator		*CreateApplicator(int Op, GColourSpace Cs = CsNone);
	uint32			LineBits;
	uint32			LineMask;
	uint32			LineReset;

	#if WINNATIVE
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
	virtual GdcPt2 GetSize() { GdcPt2 p; return p; }

	#elif defined(WINNATIVE)

	virtual HDC StartDC() { return hDC; }
	virtual void EndDC() {}
	
	#elif defined MAC
	
	#ifdef COCOA
	#else
	virtual CGColorSpaceRef GetColourSpaceRef() { return 0; }
	#endif
	
	#endif

	virtual OsBitmap GetBitmap();
	virtual OsPainter Handle();
	virtual void SetClient(GRect *c) {}
	virtual bool GetClient(GRect *c) { return false; }

	// Creation
	enum SurfaceCreateFlags
	{
		SurfaceCreateNone,
		SurfaceRequireNative,
		SurfaceRequireExactCs,
	};
	virtual bool Create(int x, int y, GColourSpace Cs, int Flags = SurfaceCreateNone) { return false; }
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

	/// Lowers the alpha of the whole image to Alpha/255.0.
	/// Only works on bitmaps with an alpha channel (i.e. CsRgba32 or it's variants)
	bool SetConstantAlpha(uint8 Alpha);

	// Create sub-images (that reference the memory of this object)
	GSurface *SubImage(GRect r);
	GSurface *SubImage(int x1, int y1, int x2, int y2)
	{
		GRect r(x1, y1, x2, y2);
		return SubImage(r);
	}

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
	virtual int Op() { return (pApp) ? pApp->GetOp() : GDC_SET; }
	/// Sets the current blending mode in operation
	/// \sa GDC_SET, GDC_AND, GDC_OR, GDC_XOR and GDC_ALPHA
	virtual int Op(int Op, NativeInt Param = -1);
	/// Gets the width in pixels
	virtual int X() { return (pMem) ? pMem->x : 0; }
	/// Gets the height in pixels
	virtual int Y() { return (pMem) ? pMem->y : 0; }
	/// Gets the bounds of the image as a GRect
	GRect Bounds() { return GRect(0, 0, X()-1, Y()-1); }
	/// Gets the length of a scanline in bytes
	virtual int GetRowStep() { return (pMem) ? pMem->Line : 0; }
	/// Returns the horizontal resolution of the device
	virtual int DpiX() { return 100; }
	/// Returns the vertical resolution of the device
	virtual int DpiY() { return 100; }
	/// Gets the bits per pixel
	virtual int GetBits() { return (pMem) ? GColourSpaceToBits(pMem->Cs) : 0; }
	/// Gets the colour space of the pixels
	virtual GColourSpace GetColourSpace() { return ColourSpace; }
	/// Gets any flags associated with the surface
	virtual int GetFlags() { return Flags; }
	/// Returns true if the surface is on the screen
	virtual class GScreenDC *IsScreen() { return 0; }
	/// Returns true if the surface is for printing
	virtual bool IsPrint() { return false; }
	/// Returns a pointer to the start of a scanline, or NULL if not available
	virtual uchar *operator[](int y);

	/// Returns true if this surface supports alpha compositing when using Blt
	virtual bool SupportsAlphaCompositing() { return false; }
	/// \returns whether if pixel data is pre-multiplied alpha
	virtual bool IsPreMultipliedAlpha();
	/// Converts the pixel data between pre-mul alpha or non-pre-mul alpha
	virtual bool ConvertPreMulAlpha(bool ToPreMul);

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
	
	/// Some surfaces only support specific line styles (e.g. GDI/Win)
	enum LineStyles
	{
		LineNone = 0x0,
		LineSolid = 0xffffffff,
		LineAlternate = 0xaaaaaaaa,
		LineDash = 0xf0f0f0f0,
		LineDot = 0xcccccccc,
		LineDashDot = 0xF33CCF30,
		LineDashDotDot = 0xf0ccf0cc,
	};
	
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

	// GDom interface
	bool GetVariant(const char *Name, GVariant &Value, char *Array = NULL);
	bool SetVariant(const char *Name, GVariant &Value, char *Array = NULL);
	bool CallMethod(const char *Name, GVariant *ReturnValue, GArray<GVariant*> &Args);
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
	#if WINNATIVE

		GScreenDC(GViewI *view);
		GScreenDC(HWND hwnd);
		GScreenDC(HDC hdc, HWND hwnd, bool Release = false);
		GScreenDC(HBITMAP hBmp, int Sx, int Sy);

		bool Create(HDC hdc);
		void SetSize(int x, int y);

	#else

		/// Construct a wrapper to draw on a window
		GScreenDC(GView *view, void *Param = 0);
		
		#if defined MAC && !defined(LGI_SDL)
		
		GScreenDC(GWindow *wnd, void *Param = 0);
		GRect GetPos();
        void PushState();
        void PopState();
		
		#elif defined(__GTK_H__)
		
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
		
		#elif defined(BEOS)
		
		GScreenDC(BView *view);
		
		#endif

		GView *GetView();
		OsPainter Handle();
		int GetFlags();
		GRect *GetClient();

	#endif

	// Properties
	bool GetClient(GRect *c);
	void SetClient(GRect *c);

	int X();
	int Y();

	GPalette *Palette();
	void Palette(GPalette *pPal, bool bOwnIt = true);

	uint LineStyle();
	uint LineStyle(uint Bits, uint32 Reset = 0x80000000);

	int GetBits();
	GScreenDC *IsScreen() { return this; }
	bool SupportsAlphaCompositing();

	#ifndef LGI_SDL
	uchar *operator[](int y) { return NULL; }

	void GetOrigin(int &x, int &y);
	void SetOrigin(int x, int y);
	GRect ClipRgn();
	GRect ClipRgn(GRect *Rgn);
	COLOUR Colour();
	COLOUR Colour(COLOUR c, int Bits = 0);
	GColour Colour(GColour c);

	int Op();
	int Op(int Op, NativeInt Param = -1);

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
	#endif
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

#if defined(MAC) && !defined(COCOA)
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

	#if defined WINNATIVE
	PBITMAPINFO	GetInfo();
	#endif
	
	// This is called between capturing the screen and overlaying the
	// cursor in GMemDC::Blt(x, y, ScreenDC, Src). It can be used to
	// overlay effects between the screen and cursor layers.
	virtual void OnCaptureScreen() {}

public:
	/// Creates a memory bitmap
	GMemDC
	(
		/// The width
		int x = 0,
		/// The height
		int y = 0,
		/// The colour space to use. CsNone will default to the 
		/// current screen colour space.
		GColourSpace cs = CsNone
	);
	GMemDC(GSurface *pDC);
	virtual ~GMemDC();

	#if WINNATIVE
	
		HDC StartDC();
		void EndDC();
		void Update(int Flags);
		void UpsideDown(bool upsidedown);
		
	#else

		GRect ClipRgn() { return Clip; }

		#if defined MAC
				
		OsPainter Handle();
		OsBitmap GetBitmap();
		#if !defined(COCOA) && !defined(LGI_SDL)
		CGColorSpaceRef GetColourSpaceRef();
		CGImg *GetImg(GRect *Sub = 0);
		#endif
		
		#elif defined(__GTK_H__)

		Gtk::GdkImage *GetImage();
		GdcPt2 GetSize();
		Gtk::cairo_t *GetCairo();
		Gtk::cairo_surface_t *GetSurface(GRect &r);
		GColourSpace GetCreateCs();

		#elif defined(BEOS) || defined(LGI_SDL)

		OsBitmap GetBitmap();
		OsPainter Handle();

		#endif
		
	#endif

	// Set new clipping region
	GRect ClipRgn(GRect *Rgn);

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
	bool SupportsAlphaCompositing();
	
	bool Create(int x, int y, GColourSpace Cs, int Flags = SurfaceCreateNone);
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

/// This class is useful for double buffering in an OnPaint handler...
class GDoubleBuffer
{
	GSurface *Screen;
	GMemDC Mem;

public:
	GDoubleBuffer(GSurface *&pDC)
	{
		if (pDC &&
			Mem.Create(pDC->X(), pDC->Y(), pDC->GetColourSpace()))
		{
			Screen = pDC;
			pDC = &Mem;
		}
		else
			Screen = NULL;		
	}
	
	~GDoubleBuffer()
	{
		if (Screen)
			Screen->Blt(0, 0, &Mem);
	}
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

	/// Returns the colour space of the screen
	GColourSpace GetColourSpace();
	/// Returns the current screen bit depth
	int GetBits();
	/// Returns the current screen width
	int X();
	/// Returns the current screen height
	int Y();
	/// Returns the size of the screen as a rectangle.
	GRect Bounds() { return GRect(0, 0, X()-1, Y()-1); }

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
        const char *FileName,
    	/// [Optional] Enable OS based loaders
        bool UseOSLoader = true
    );

	/// The stream version of the file loader...
    GSurface *Load
    (
        /// The full path of the file
        GStream *In,
        /// [Optional] File name hint for selecting a filter
        const char *Name = NULL,
    	/// [Optional] Enable OS based loaders
        bool UseOSLoader = true
    );
    
    /// Save an image to a file.
    bool Save
    (
        /// The file to write to
        const char *Name,
        /// The pixels to store
        GSurface *pDC
    );
    
    #if LGI_SDL
    SDL_Surface *Handle();
    #endif
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
	GSurface *Create(uint32 TransparentPx = 0xffffffff);
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
LgiFunc GSurface *LoadDC(const char *Name, bool UseOSLoader = true);

/// Wrapper around GdcDevice::Save
/// \deprecated Use GdcDevice::Save directly in new code.
LgiFunc bool WriteDC(const char *Name, GSurface *pDC);

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

/// Row copy operator for full RGB (8 bit components)
LgiFunc bool LgiRopRgb
(
	// Pointer to destination pixel buffer
	uint8 *Dst,
	// Destination colour space (must be 8bit components)
	GColourSpace DstCs,
	// Pointer to source pixel buffer (if this overlaps 'Dst', set 'Overlap' to true)
	uint8 *Src,
	// Source colour space (must be 8bit components)
	GColourSpace SrcCs,
	// Number of pixels to convert
	int Px,
	// Whether to composite using alpha or copy blt
	bool Composite
);

/// Universal bit blt method
LgiFunc bool LgiRopUniversal(GBmpMem *Dst, GBmpMem *Src, bool Composite);

/// Gets the screens DPI
LgiFunc int LgiScreenDpi();

/// Find the bounds of an image.
/// \return true if there is some non-transparent image	 in 'rc'
LgiFunc bool LgiFindBounds
(
	/// [in] The image
	GSurface *pDC,
	/// [in/out] Starts off as the initial bounds to search.
	/// Returns the non-background area.
	GRect *rc
);

#if defined(LGI_SDL)
LgiFunc GColourSpace PixelFormat2ColourSpace(SDL_PixelFormat *pf);
#endif

#endif
