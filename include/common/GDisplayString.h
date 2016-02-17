#ifndef _GDISPLAY_STRING_H_
#define _GDISPLAY_STRING_H_

#if defined(LGI_SDL)
#elif defined(LINUX)
	namespace Pango
	{
	#include "pango/pango.h"
	#include "pango/pangocairo.h"
	}
#endif

#define LGI_DSP_STR_CACHE		1

/// \brief Cache for text measuring, glyph substitution and painting
///
/// To paint text onto the screen several stages need to be implemented to
/// properly support unicode on multiple platforms. This class addresses all
/// of those needs and then allows you to cache the results to reduce text
/// related workload.
///
/// The first stage is converting text into the native format for the 
/// OS's API. This usually involved converting the text to wide characters for
/// Linux or Windows, or Utf-8 for BeOS. Then the text is converted into runs of
/// characters that can be rendered in the same font. If glyph substitution is
/// required to render the characters a separate run is used with a different
/// font ID. Finally you can measure or paint these runs of text. Also tab characters
/// are expanded to the current tab size setting.
class LgiClass GDisplayString
{
	GSurface *pDC;
	OsChar *Str;
	GFont *Font;
	uint32 x, y, len;
	int xf, yf;
	int DrawOffsetF;
	
	// Flags
	uint8 LaidOut : 1;
	uint8 AppendDots : 1;
	uint8 VisibleTab : 1;

	#if LGI_DSP_STR_CACHE
	// Wide char cache
	GAutoWString StrCache;
	#endif
	
	#if defined(LGI_SDL)
	
	GAutoPtr<GMemDC> Img;
	
	#elif defined(MAC)
	
	#if USE_CORETEXT
		CTLineRef Hnd;
		CFAttributedStringRef AttrStr;
		void CreateAttrStr();
	#else
		ATSUTextLayout Hnd;
		ATSUTextMeasurement fAscent;
		ATSUTextMeasurement fDescent;
	#endif
	
	#elif defined(__GTK_H__)
	
	Gtk::PangoLayout *Hnd;
	int LastTabOffset;
	void UpdateTabs(int Offset, int Size, bool Debug = false);
	
	#elif defined(WINNATIVE) || defined(BEOS)
	
	class CharInfo
	{
	public:
		OsChar *Str;
		uint16 Len;
		uint16 X;
		uint8 FontId;
		int8 SizeDelta;

		CharInfo()
		{
			Str = 0;
			Len = 0;
			X = 0;
			FontId = 0;
			SizeDelta = 0;
		}
	};
	GArray<CharInfo> Info;

	#endif

	void Layout(bool Debug = false);

public:
	/// Constructor
	GDisplayString
	(
		/// The base font. Must not be destroyed during the lifetime of this object.
		GFont *f,
		/// Utf-8 input string
		const char *s,
		/// Number of bytes in the input string or -1 for NULL terminated.
		int l = -1,
		GSurface *pdc = 0
	);
	/// Constructor
	GDisplayString
	(
		/// The base font. Must not be destroyed during the lifetime of this object.
		GFont *f,
		/// A wide character input string
		const char16 *s,
		/// The number of characters in the input string (NOT the number of bytes) or -1 for NULL terminated
		int l = -1,
		GSurface *pdc = 0
	);
	virtual ~GDisplayString();
	
	GDisplayString &operator=(const GDisplayString &s)
	{
		LgiAssert(0);
		return *this;
	}
	
	/// \returns the draw offset used to calculate tab spacing (in Px)
	int GetDrawOffset();
	/// Sets the draw offset used to calculate tab spacing (in Px)
	void SetDrawOffset(int Px);

	/// Returns the ShowVisibleTab setting.
	/// Treats Unicode-2192 (left arrow) as a tab char
	bool ShowVisibleTab();
	/// Sets the ShowVisibleTab setting.
	/// Treats Unicode-2192 (left arrow) as a tab char
	void ShowVisibleTab(bool i);

	/// \returns the font used to create the layout
	GFont *GetFont() { return Font; };

	/// Fits string into 'width' pixels, truncating with '...' if it's not going to fit
	void TruncateWithDots
	(
		/// The number of pixels the string has to fit
		int Width
	);
	/// Returns true if the string is trucated
	bool IsTruncated();

	/// Returns the chars in the OsChar string
	int Length();
	/// Sets the number of chars in the OsChar string
	void Length(int NewLen);

	/// Returns the pointer to the native string
	operator const char16*()
	{
		#ifdef LGI_DSP_STR_CACHE
		return StrCache;
		#else
		return Str;
		#endif
	}

	// API that use full pixel sizes:

		/// Returns the width of the whole string
		int X();
		/// Returns the height of the whole string
		int Y();
		/// Returns the width and height of the whole string
		GdcPt2 Size();
		/// Returns the number of characters that fit in 'x' pixels.
		int CharAt(int x);

		/// Draws the string onto a device surface
		void Draw
		(
			/// The output device
			GSurface *pDC,
			/// The x coordinate of the top left corner of the output box
			int x,
			/// The y coordinate of the top left corner of the output box
			int y,
			/// An optional clipping rectangle. If the font is not transparent this rectangle will be filled with the background colour.
			GRect *r = NULL
		);

	// API that uses fractional pixel sizes.
		
		enum
		{
			#if defined LGI_SDL
				FShift = 6,
				FScale = 1 << FShift,
			#elif defined __GTK_H__
				/// This is the value for 1 pixel.
				FScale = PANGO_SCALE,
				/// This is bitwise shift to convert between integer and fractional
				FShift = 10
			#elif defined MAC
				/// This is the value for 1 pixel.
				FScale = 0x10000,
				/// This is bitwise shift to convert between integer and fractional
				FShift = 16
			#else
				/// This is the value for 1 pixel.
				FScale = 1,
				/// This is bitwise shift to convert between integer and fractional
				FShift = 0
			#endif
		};
		
		/// \returns the draw offset used to calculate tab spacing (in fractional units)
		int GetDrawOffsetF();
		/// Sets the draw offset used to calculate tab spacing (in fractional units)
		void SetDrawOffsetF(int Fpx);	
		/// \returns the fractional width of the string
		int FX();
		/// \returns the fractional height of the string
		int FY();
		/// \returns both the fractional width and height of the string
		GdcPt2 FSize();
		/// Draws the string using fractional co-ordinates.
		void FDraw
		(
			/// The output device
			GSurface *pDC,
			/// The fractional x coordinate of the top left corner of the output box
			int fx,
			/// The fractional y coordinate of the top left corner of the output box
			int fy,
			/// [Optional] fractional clipping rectangle. If the font is not transparent 
			/// this rectangle will be filled with the background colour.
			GRect *frc = NULL,
			/// [Optional] print debug info
			bool Debug = false
		);
};

#endif
