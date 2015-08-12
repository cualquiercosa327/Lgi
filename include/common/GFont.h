/// \file
/// \author Matthew Allen

#ifndef _GDCFONT_H_
#define _GDCFONT_H_

#include "string.h"
#include "GRect.h"
#include "LgiOsClasses.h"
#include "GColour.h"
#include "GCapabilities.h"

//////////////////////////////////////////////////////////////
// Defines

#ifndef WIN32

// font weights
#define FW_DONTCARE						0 
#define FW_THIN							100
#define FW_EXTRALIGHT					200
#define FW_ULTRALIGHT					200
#define FW_LIGHT						300
/// The default font weight
#define FW_NORMAL						400
#define FW_REGULAR						400
#define FW_MEDIUM						500
#define FW_SEMIBOLD						600
#define FW_DEMIBOLD						600
/// Bold font weight
#define FW_BOLD							700
#define FW_EXTRABOLD					800
#define FW_ULTRABOLD					800
#define FW_HEAVY						900
#define FW_BLACK						900

// quality
/// Default font quality
#define DEFAULT_QUALITY					0
/// Specifically anti-aliased font
#define ANTIALIASED_QUALITY				1
/// Specifically not anti-alias font
#define NONANTIALIASED_QUALITY			2

#elif defined WIN32

#define WESTEUROPE_CHARSET				BALTIC_CHARSET		// ??? don't know

#endif

// OS specific font type
#if defined __GTK_H__

	#include "LgiOsClasses.h"
	#define PrevOsChar(Ptr)	Ptr--
	#define NextOsChar(Ptr)	Ptr++

#elif defined(WIN32)

	#define PrevOsChar(Ptr)	Ptr--
	#define NextOsChar(Ptr)	Ptr++

#elif defined(BEOS)

	typedef BFont *OsFont;
	#define PrevOsChar(Ptr)	LgiPrevUtf8((char*&)Ptr)
	#define NextOsChar(Ptr)	LgiNextUtf8((char*&)Ptr)

#endif

#define MAX_UNICODE						0xffff // maximum unicode char I can handle
#define _HasUnicodeGlyph(map, u)		( (map[(u)>>3] & (1 << ((u) & 7))) != 0  )

//////////////////////////////////////////////////////////////
// Classes
class GFontType;

/// Font parameters collection
class LgiClass GTypeFace
{
protected:
	class GTypeFacePrivate *d;

	// Methods
	virtual void _OnPropChange(bool c) {} // if false then it's just a property change

public:
	GTypeFace();
	virtual ~GTypeFace();

	/// Sets the font face name
	void Face(const char *s);
	/// Sets the point size
	void PointSize(int i);
	/// Sets the tab size in device units (pixels)
	void TabSize(int i);
	/// Sets the quality resquested, use one of #DEFAULT_QUALITY, #ANTIALIASED_QUALITY or #NONANTIALIASED_QUALITY.
	void Quality(int i);

	/// Sets the foreground colour as a 24 bit RGB value
	void Fore(COLOUR c);
	void Fore(GColour c);

	/// Sets the background colour as a 24 bit RGB value. In most systems this is not important,
	/// but on BeOS the anti-aliasing is done from the foreground colour to the background colour
	/// with no regards for what is underneath the rendered text, thus you need to set the back
	/// colour correctly
	void Back(COLOUR c);
	void Back(GColour c);

    /// Get the whitespace rendering colour
    GColour WhitespaceColour();
    /// Sets the rendering colour of whitespace characters when GDisplayString::ShowVisibleTab() is true.
    void WhitespaceColour(GColour c);

	/// Sets the font's weight, use one the weight defines in GFont.h, e.g. #FW_NORMAL, #FW_BOLD
	void SetWeight(int Weight);
	/// Set a bold font
	void Bold(bool i) { SetWeight(i ? FW_BOLD : FW_NORMAL); }
	/// Sets the font to italic
	void Italic(bool i);
	/// Draws with an underline
	void Underline(bool i);
	/// Makes the text have no background.
	void Transparent(bool i);
	/// Turns glyph substitution on or off
	void SubGlyphs(bool i);

	/// \returns the font face
	char *Face();
	/// \returns the point size
	int PointSize();
	/// \returns the tabsize in pixels
	int TabSize();
	/// \returns the quality setting
	int Quality();
	/// \returns the foreground colour.
	GColour Fore();
	/// \returns the background colour.
	GColour Back();
	/// \returns the font weight.
	int GetWeight();
	/// \returns true if this is a bold font.
	bool Bold() { return GetWeight() >= FW_BOLD; }
	/// \returns true if this is a italic font.
	bool Italic();
	/// \returns true if this font is drawn with an underline.
	bool Underline();
	/// \returns true if no background will be drawn.
	bool Transparent();
	/// \returns true if glyph substitution will be done.
	bool SubGlyphs();
	/// \returns the amount of space above the baseline.
	double Ascent();
	/// \returns the amount of space below the baseline.
	double Descent();
	/// \returns the amount of normally unused space at the top of the Ascent.
	double Leading();

	/// \returns true if the font types are the same
	bool operator ==(GTypeFace &t);

	/// Set the foreground and background in 24-bit colour.
	/// \sa GTypeFace::Fore() and GTypeFace::Back()
	virtual void Colour(COLOUR Fore, COLOUR Back = 0xFFFFFFFF);

	/// Set the foreground and background colour.
	virtual void Colour(GColour Fore, GColour Back);
};

/// \brief Font class.
class LgiClass GFont :
	public GTypeFace
{
	friend class GFontSystem;

	class GFontPrivate *d;

	// Methods
	bool IsValid();
	void _OnPropChange(bool Change);
	char16 *_ToUnicode(char *In, int &Len);
	bool GetOwnerUnderline();

	#if defined(WINNATIVE)
	friend class GDisplayString;

	void _Measure(int &x, int &y, OsChar *Str, int Len);
	int _CharAt(int x, OsChar *Str, int Len);
	void _Draw(GSurface *pDC, int x, int y, OsChar *Str, int Len, GRect *r, GColour &fore);
	#endif

public:
	/// Construct from face/pt size.
	GFont
	(
		/// Font face name
		const char *face = 0,
		/// Point size of the font
		int point = -1
	);
	/// Construct from OS font handle
	GFont(OsFont Handle);
	/// Construct from type information
	GFont(GFontType &Type);
	/// Copy constructor
	GFont(GFont &Fnt);
	~GFont();

	/// Creates a new font handle with the specified face / pt size
	bool Create
	(
		/// The new font face
		const char *Face = 0,
		/// The pt size
		int PtSize = -1,
		/// An OS specific parameter. This is typically a Win32 HDC when creating a font
		/// for printing.
		NativeInt Param = 0
	);
	/// Creates a new font from type infomation
	bool Create(GFontType *Type, NativeInt Param = 0);
	/// Clears any handles and memory associated with the object.
	bool Destroy();
	/// Returns the OS font handle
	OsFont Handle();
	/// Copies the font
	GFont &operator =(GFont &f);
	/// Returns the pixel height of the font
	int GetHeight();
	/// Gets the creation parameter passed in (0 by default).
	NativeInt GetParam();
	/// Get supported glyphs
	uchar *GetGlyphMap();
	/// Converts printable characters to unicode.
	GAutoString ConvertToUnicode(char16 *Input, int Len = -1);
	
	#ifdef BEOS
	GdcPt2 StringBounds(const char *s, int len = -1);
	#endif
};

/// Font type information and system font query tools.
class LgiClass GFontType
{
	friend class GFont;
	friend class GTypeFace;

protected:
	#if defined WIN32
	LOGFONT Info;
	#else
	GTypeFace Info;
	#endif

public:
	GFontType(const char *face = 0, int pointsize = 0);
	virtual ~GFontType();

	#ifdef WIN32
	LOGFONT *Handle() { return &Info; }
	#else
	GTypeFace *Handle() { return &Info; }
	#endif

	/// Gets the type face name
	char *GetFace();
	
	/// Sets the type face name
	void SetFace(const char *Face);
	
	/// Sets the point size
	int GetPointSize();
	
	/// Sets the point size
	void SetPointSize(int PointSize);

	/// Put a user interface for the user to edit the font def
	bool DoUI(GView *Parent);
	
	/// Describe the font to the user as a string
	bool GetDescription(char *Str, int SLen);
	
	/// Read/Write the font def to storage
	// bool Serialize(ObjProperties *Options, char *OptName, bool Write);
	/// Read/Write the font def to storage
	bool Serialize(GDom *Options, const char *OptName, bool Write);

	/// Read the font from the LGI config
	bool GetConfigFont(const char *Tag);
	
	/// Read the font from LGI config (if there) and then the system settings
	bool GetSystemFont(const char *Which);

	/// Read from OS reference
	bool GetFromRef(OsFont Handle);

	/// Create a font based on this font def
	virtual GFont *Create(NativeInt Param = 0);
};

/// Charset definitions
enum GCharSetType
{
	CpNone,
	CpMapped,
	CpUtf8,
	CpUtf16,
	CpUtf32,
	CpIconv,
	CpWindowsDb
};

/// Charset information class
class LgiClass GCharset
{
public:
	/// Standard charset name
	const char *Charset;
	/// Human description
	const char *Description;
	/// 128 shorts that map the 0x80-0xff range to unicode
	short *UnicodeMap;
	/// Charset's name for calling iconv
	const char *IconvName;
	/// Comma separated list of alternate names used for this charset
	const char *AlternateNames;
	/// General type of the charset
	GCharSetType Type;

	/// Constructor
	GCharset(const char *cp = 0, const char *des = 0, short *map = 0, const char *alt = 0);

	/// Returns true if the charset is a unicode variant
	bool IsUnicode();
	/// Gets the iconv name
	const char *GetIconvName();
	/// Returns whether Lgi can convert to/from this charset at the moment.
	bool IsAvailable();
};

/// Charset table manager class
class LgiClass GCharsetSystem
{
	struct GCharsetSystemPriv *d;

public:
	GCharsetSystem();
	~GCharsetSystem();

	// Get the charset info
	GCharset *GetCsInfo(const char *Cp);
	GCharset *GetCsList();
};

/// Returns information about a charset.
LgiFunc GCharset *LgiGetCsInfo(const char *Cs);
/// Returns the start of an array of supported charsets, terminated by
/// one with a NULL 'Charset' member. 
LgiFunc GCharset *LgiGetCsList();
/// Returns the charset that best fits the input data
LgiFunc const char *LgiDetectCharset
(
	/// The input text
	const char *Utf8,
	/// The byte length of the input text
	int Len = -1,
	/// An optional list of prefered charsets to look through first
	List<char> *Prefs = 0
);

#ifndef LGI_STATIC
/// Overall font system class
class LgiClass GFontSystem : public GCapabilityClient
{
	friend class GApp;
	friend class GDisplayString;

	static GFontSystem *Me;

private:
	// System Font List
	List<const char> AllFonts;
	List<const char> SubFonts; // Fonts yet to be scanned for substitution

	// Missing Glyph Substitution
	uchar Lut[MAX_UNICODE+1];
	GFont *Font[256];
	class GFontSystemPrivate *d;

public:
	/// Get a pointer to the font system.
	static GFontSystem *Inst();

	// Object
	GFontSystem();
	~GFontSystem();

	/// Enumerate all installed fonts
	bool EnumerateFonts(List<const char> &Fonts);

	/// Returns whether the current Lgi implementation supports glyph sub
	bool GetGlyphSubSupport();
	/// Returns whether glyph sub is currently turned on
	bool GetDefaultGlyphSub();
	/// Turns the glyph sub feature on or off
	void SetDefaultGlyphSub(bool i);
	/// Returns a font that can render the specified unicode code point
	GFont *GetGlyph
	(
		/// A utf-32 character
		int u,
		/// The base font used for rendering
		GFont *UserFont
	);
	/// This looks for a font that can contains the most glyphs for a given string,
	/// ideally it can render the whole thing. But the next best alternative is returned
	/// when no font matches all characters in the string.
	GFont *GetBestFont(char *Str);

	#ifdef __GTK_H__
	
	// Pango stuff
	Gtk::PangoFontMap *GetFontMap();
	Gtk::PangoContext *GetContext();
	
	#endif

	/// \returns true if iconv services are available.
	bool HasIconv(bool Quiet);
	/// Converts a string into a buffer using iconv
	int IconvConvert(const char *OutCs, char *Out, int OutLen, const char *InCs, const char *&In, int InLen);
	/// Converts a string into a stream using iconv
	int IconvConvert(const char *OutCs, GStreamI *Out, const char *InCs, const char *&In, int InLen);
};
#endif

#ifdef LINUX
extern bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize);
#endif


#endif
