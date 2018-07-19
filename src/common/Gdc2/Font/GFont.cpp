/*hdr
**	FILE:			GFont.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			5/5/97
**	DESCRIPTION:	Gdc2 Font Support
**
**	Copyright (C) 1997-2002, Matthew Allen
**		fret@memecode.com
*/

#ifndef WIN32
#define _WIN32_WINNT	0x500
#endif

//////////////////////////////////////////////////////////////////////
// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Lgi.h"
#include "GVariant.h"
#include "GFontSelect.h"
#include "GDisplayString.h"
#include "GdiLeak.h"
#include "GDisplayString.h"
#include "GStringClass.h"
#include "GCss.h"

#ifdef FontChange
#undef FontChange
#endif

#define MAC_FONT_SIZE_OFFSET	0

//////////////////////////////////////////////////////////////////////
// Helpers

#if defined(LGI_SDL)

class FreetypeLib
{
	FT_Library  lib;
	FT_Error	err;
	
public:
	FreetypeLib()
	{
		err = FT_Init_FreeType(&lib);
		if (err)
		{
			LgiAssert(0);
		}
	}
	
	~FreetypeLib()
	{
		if (!err)
		{
			FT_Done_FreeType(lib);
		}
	}
	
	FT_Library Handle()
	{
		return lib;
	}
	
	GString GetVersion()
	{
		FT_Int amajor = 0, aminor = 0, apatch = 0;
		FT_Library_Version(lib, &amajor, &aminor, &apatch);
		GString s;
		s.Printf("%i.%i.%i", amajor, aminor, apatch);
		return s;
	}
	
} Freetype2;

GString GetFreetypeLibraryVersion()
{
	return Freetype2.GetVersion();
}

#elif defined(WIN32)

#ifndef __GNUC__
#include <mbctype.h>
#endif

int WinPointToHeight(int Pt, HDC hDC)
{
	int Ht = 0;

	HWND hDestktop = NULL;
	if (!hDC)
		hDC = GetDC(hDestktop = GetDesktopWindow());
	
	if (hDC)
		Ht = -MulDiv(Pt, GetDeviceCaps(hDC, LOGPIXELSY), 72);

	if (hDestktop)
		ReleaseDC(hDestktop, hDC);

	return Ht;
}

int WinHeightToPoint(int Ht, HDC hDC)
{
	int Pt = 0;

	HWND hDestktop = NULL;
	if (!hDC)
		hDC = GetDC(hDestktop = GetDesktopWindow());
	
	if (hDC)
		Pt = -MulDiv(Ht, 72, GetDeviceCaps(hDC, LOGPIXELSY));

	if (hDestktop)
		ReleaseDC(hDestktop, hDC);

	return Pt;
}

#elif defined(MAC)

// CTFontCreateUIFontForLanguage
// #include <HIToolbox/HITheme.h>
#include <CoreText/CTFont.h>

#endif

//////////////////////////////////////////////////////////////////////
// Classes
class GTypeFacePrivate
{
public:
	// Type
	char *_Face;			// type face
	int _PtSize;			// point size
	int _Weight;
	bool _Italic;
	bool _Underline;
	char *_CodePage;
	int _Quality;

	// Output
	GColour _Fore;
	GColour _Back;
	GColour WhiteSpace; // Can be empty (if so it's calculated)
	int _TabSize;
	bool _Transparent;
	bool _SubGlyphs;

	// Backs
	bool IsSymbol;

	// Props
	double _Ascent, _Descent, _Leading;

	GTypeFacePrivate()
	{
		IsSymbol = false;
		_Ascent = _Descent = _Leading = 0.0;
		_Face = 0;
		_PtSize = 8;
		_Weight = FW_NORMAL;
		_Italic = false;
		_Underline = false;
		_CodePage = NewStr("utf-8");
		_Fore.Rgb(0, 0, 0);
		_Back.Rgb(255, 255, 255);
		_TabSize = 32;
		_Transparent = false;
		_Quality = DEFAULT_QUALITY;
		_SubGlyphs = GFontSystem::Inst()->GetDefaultGlyphSub();
	}

	~GTypeFacePrivate()
	{
		DeleteArray(_Face);
		DeleteArray(_CodePage);
	}
};

GTypeFace::GTypeFace()
{
	d = new GTypeFacePrivate;
}

GTypeFace::~GTypeFace()
{
	DeleteObj(d);
}

bool GTypeFace::operator ==(GTypeFace &t)
{
	if ((Face() == 0) ^ (t.Face() == 0))
		return false;
	if (Face() && t.Face() && stricmp(Face(), t.Face()) != 0)
		return false;
	if (PointSize() != t.PointSize())
		return false;
	if (GetWeight() != t.GetWeight())
		return false;
	if (Italic() != t.Italic())
		return false;
	if (Underline() != t.Underline())
		return false;
	return true;
}

// set
void GTypeFace::Face(const char *s)
{
	if (s &&
		s != d->_Face &&
		stricmp(s, d->_Face?d->_Face:(char*)"") != 0)
	{
		DeleteArray(d->_Face);
		d->_Face = NewStr(s);
		LgiAssert(d->_Face != NULL);
		_OnPropChange(true);
	}
}

void GTypeFace::PointSize(int i)
{
	if (d->_PtSize != i)
	{
		d->_PtSize = i;
		_OnPropChange(true);
	}
}

void GTypeFace::TabSize(int i)
{
	d->_TabSize = MAX(i, 8);
	_OnPropChange(false);
}

void GTypeFace::Quality(int i)
{
	if (d->_Quality != i)
	{
		d->_Quality = i;
		_OnPropChange(true);
	}
}

GColour GTypeFace::WhitespaceColour()
{
	if (d->WhiteSpace.IsValid())
		return d->WhiteSpace;
	
	return d->_Back.Mix(d->_Fore, LGI_WHITESPACE_WEIGHT);
}

void GTypeFace::WhitespaceColour(GColour c)
{
	d->WhiteSpace = c;
	_OnPropChange(false);
}

void GTypeFace::Fore(COLOUR c)
{
	d->_Fore.c24(c);
	_OnPropChange(false);
}

void GTypeFace::Fore(GColour c)
{
	d->_Fore = c;
	_OnPropChange(false);
}

void GTypeFace::Back(COLOUR c)
{
	d->_Back.c24(c);
	_OnPropChange(false);
}

void GTypeFace::Back(GColour c)
{
	d->_Back = c;
	_OnPropChange(false);
}

void GTypeFace::SetWeight(int i)
{
	if (d->_Weight != i)
	{
		d->_Weight = i;
		_OnPropChange(true);
	}
}

void GTypeFace::Italic(bool i)
{
	if (d->_Italic != i)
	{
		d->_Italic = i;
		_OnPropChange(true);
	}
}

void GTypeFace::Underline(bool i)
{
	if (d->_Underline != i)
	{
		d->_Underline = i;
		_OnPropChange(true);
	}
}

void GTypeFace::Transparent(bool i)
{
	d->_Transparent = i;
	_OnPropChange(false);
}

void GTypeFace::Colour(COLOUR Fore, COLOUR Back)
{
	d->_Fore.c24(Fore);
	d->_Back.c24(Back);
	_OnPropChange(false);
}

void GTypeFace::Colour(GColour Fore, GColour Back)
{
	LgiAssert(Fore.IsValid());
	d->_Fore = Fore;
	d->_Back = Back;
	// Transparent(Back.Transparent());
	_OnPropChange(false);
}

void GTypeFace::SubGlyphs(bool i)
{
	if (!i || GFontSystem::Inst()->GetGlyphSubSupport())
	{
		d->_SubGlyphs = i;
		_OnPropChange(false);
	}
}

////////////////////////
// get
char *GTypeFace::Face()
{
	return d->_Face;
}

int GTypeFace::PointSize()
{
	return d->_PtSize;
}

int GTypeFace::TabSize()
{
	return d->_TabSize;
}

int GTypeFace::Quality()
{
	return d->_Quality;
}

GColour GTypeFace::Fore()
{
	return d->_Fore;
}

GColour GTypeFace::Back()
{
	return d->_Back;
}

int GTypeFace::GetWeight()
{
	return d->_Weight;
}

bool GTypeFace::Italic()
{
	return d->_Italic;
}

bool GTypeFace::Underline()
{
	return d->_Underline;
}

bool GTypeFace::Transparent()
{
	return d->_Transparent;
}

bool GTypeFace::SubGlyphs()
{
	return d->_SubGlyphs;
}

double GTypeFace::Ascent()
{
	return d->_Ascent;
}

double GTypeFace::Descent()
{
	return d->_Descent;
}

double GTypeFace::Leading()
{
	return d->_Leading;
}

////////////////////////////////////////////////////////////////////
// GFont class, the implemention
#if defined MAC && !defined COCOA
typedef ATSUTextMeasurement OsTextSize;
#else
typedef double OsTextSize;
#endif

class GFontPrivate
{
public:
	#ifdef WINDOWS
	static GAutoPtr<GLibrary> Gdi32;
	#endif

	// Data
	OsFont			hFont;
	int				Height;
	bool			Dirty;
	char			*Cp;
	GSurface		*pSurface;
	bool			OwnerUnderline;

	// Glyph substitution
	uchar			*GlyphMap;
	static class GlyphCache *Cache;

	#ifdef BEOS
	// Beos glyph sizes
	uint16			CharX[128]; // Widths of ascii
	LHashTbl<IntKey<int>,int> UnicodeX; // Widths of any other characters
	#endif

	#ifdef MAC
	CFDictionaryRef Attributes;
	#endif

	#ifdef __GTK_H__
	Gtk::PangoContext *PangoCtx;
	#endif

	GFontPrivate()
	{
		hFont = 0;
		pSurface = NULL;
		#if defined WIN32
		OwnerUnderline = false;
		#endif
		#if defined(MAC)
		Attributes = NULL;
		#endif
		#ifdef __GTK_H__
		PangoCtx = NULL;
		#endif

		GlyphMap = 0;
		Dirty = true;
		Height = 0;
		Cp = 0;
	}
	
	~GFontPrivate()
	{
		#if defined(MAC)
		if (Attributes)
			CFRelease(Attributes);
		#endif
		#ifdef __GTK_H__
		if (PangoCtx)
			g_object_unref(PangoCtx);
		#endif
	}
};

#ifdef WINDOWS
GAutoPtr<GLibrary> GFontPrivate::Gdi32;
#endif

GFont::GFont(const char *face, int point)
{
	d = new GFontPrivate;
	if (face && point > 0)
	{
		Create(face, point);
	}
}

GFont::GFont(OsFont Handle)
{
	d = new GFontPrivate;
	GFontType Type;
	if (Type.GetFromRef(Handle))
	{
		Create(&Type);
	}
}

GFont::GFont(GFontType &Type)
{
	d = new GFontPrivate;
	Create(&Type);
}

GFont::GFont(GFont &Fnt)
{
	d = new GFontPrivate;
	*this = Fnt;
}

GFont::~GFont()
{
	Destroy();
	DeleteObj(d);
}

bool GFont::CreateFromCss(const char *Css)
{
	if (!Css)
		return false;
	
	GCss c;
	c.Parse(Css);
	return CreateFromCss(&c);
}

bool GFont::CreateFromCss(GCss *Css)
{
	if (!Css)
		return false;
	
	GCss::StringsDef Fam = Css->FontFamily();
	if (Fam.Length())
		Face(Fam[0]);

	GCss::Len Sz = Css->FontSize();
	if (Sz.Type == GCss::LenPt)
		PointSize((int)(Sz.Value+0.5));
	else if (Sz.IsValid())
		LgiAssert(!"Impl me.");

	GCss::FontWeightType w = Css->FontWeight();
	if (w == GCss::FontWeightBold)
		Bold(true);
	
	GCss::FontStyleType s = Css->FontStyle();
	if (s == GCss::FontStyleItalic)
		Italic(true);

	GCss::TextDecorType dec = Css->TextDecoration();
	if (dec == GCss::TextDecorUnderline)
		Underline(true);

	return Create();
}

bool GFont::Destroy()
{
	bool Status = true;
	
	if (d->hFont)
	{
		#if LGI_SDL
			FT_Done_Face(d->hFont);
		#elif defined(WIN32)
			DeleteObject(d->hFont);
		#elif defined MAC
			#if USE_CORETEXT
			CFRelease(d->hFont);
			#else
			ATSUDisposeStyle(d->hFont);
			#endif
		#elif defined LINUX
			if (d->PangoCtx)
			{
				g_object_unref(d->PangoCtx);
				d->PangoCtx = NULL;
			}
			Gtk::pango_font_description_free(d->hFont);
		#elif defined BEOS
			DeleteObj(d->hFont);
		#else
			LgiAssert(0);
		#endif
		
		d->hFont = 0;
	}
	DeleteArray(d->GlyphMap);
	
	return Status;
}

#if defined(MAC) && !COCOA
CFDictionaryRef GFont::GetAttributes()
{
	return d->Attributes;
}
#endif

#ifdef BEOS
#include "GUtf8.h"
GdcPt2 GFont::StringBounds(const char *s, int len)
{
	GdcPt2 Sz(0, GetHeight());
	GArray<uint32> CacheMiss;
	
	if (s)
	{
		GUtf8Ptr p(s);
		GUtf8Ptr end(s + (len < 0 ? strlen(s) : len);
		while (p < end)
		{
			uint32 c = p;
			if (c < 0x80)
			{
				Sz.x += d->CharX[c];
			}
			else
			{
				int cx = d->UnicodeX.Find(c);
				if (cx)
				{
					Sz.x += cx;
				}
				else
				{
					CacheMiss.Add(c);
					printf("%s:%i - Char cache miss: %i (0x%x)\n", _FL, c, c);
					// LgiAssert(!"Impl me.");
				}
			}
			
			p++;
		}
	}
	
	return Sz;
}
#endif

uchar *GFont::GetGlyphMap()
{
	return d->GlyphMap;
}

bool GFont::GetOwnerUnderline()
{
	return d->OwnerUnderline;
}

void GFont::_OnPropChange(bool FontChange)
{
	if (FontChange)
	{
		Destroy();
	}
}

OsFont GFont::Handle()
{
	return d->hFont;
}

int GFont::GetHeight()
{
	if (!d->hFont)
	{
		Create();
	}
	
	LgiAssert(d->Height != 0);
	return d->Height;
}

bool GFont::IsValid()
{
	bool Status = false;

	// recreate font
	#ifdef WIN32
	if (!d->hFont)
	{
		Status = Create(Face(), PointSize());
	}
	#else
	if (d->Dirty)
	{
		Status = Create(Face(), PointSize());
		d->Dirty = false;
	}
	#endif
	else
	{
		Status = true;
	}

	return Status;
}

#ifdef WIN32
#define ENCODING_TABLE_SIZE 8

class type_4_cmap
{
public:
	uint16 format;
	uint16 length;
	uint16 language;
	uint16 seg_count_x_2;
	uint16 search_range;
	uint16 entry_selector;
	uint16 range_shift;
//	uint16 reserved;
	uint16 arrays[1];

	uint16 SegCount()
	{
		return seg_count_x_2 / 2;
	}

	uint16 *GetIdRangeOffset()
	{
		return arrays + 1 + (SegCount()*3);
	}

	uint16 *GetStartCount()
	{
		return arrays + 1 + SegCount();
	}

	uint16 *GetEndCount()
	{
		/* Apparently the reseved spot is not reserved for 
			 the end_count array!? */
		return arrays;
	}
};

class cmap_encoding_subtable
{
public:
	uint16 platform_id;
	uint16 encoding_id;
	uint32 offset;
};

#define INT16_SWAP(i)					(	((i)>>8) | (((i)&0xff)<<8) )
#define INT32_SWAP(i)					(	( ((i)&0x000000ff) << 24) |		\
											( ((i)&0x0000ff00) << 8 ) |		\
											( ((i)&0x00ff0000) >> 8 ) |		\
											( ((i)&0xff000000) >> 24) )
#define MAKE_TT_TABLE_NAME(c1, c2, c3, c4) \
	(((uint32)c4) << 24 | ((uint32)c3) << 16 | ((uint32)c2) << 8 | ((uint32)c1))
#define CMAP							(MAKE_TT_TABLE_NAME('c','m','a','p'))
#define CMAP_HEADER_SIZE				4
#define APPLE_UNICODE_PLATFORM_ID		0
#define MACINTOSH_PLATFORM_ID			1
#define ISO_PLATFORM_ID					2
#define MICROSOFT_PLATFORM_ID			3
#define SYMBOL_ENCODING_ID				0
#define UNICODE_ENCODING_ID				1
#define UCS4_ENCODING_ID				10

type_4_cmap *GetUnicodeTable(HFONT hFont, uint16 &Length)
{
	bool Status = false;
	type_4_cmap *Table = 0;

	HDC hDC = GetDC(0);
	if (hDC)
	{
		HFONT Old = (HFONT)SelectObject(hDC, hFont);

		uint16 Tables = 0;
		uint32 Offset = 0;

		// Get The number of encoding tables, at offset 2
		int32 Res = GetFontData(hDC, CMAP, 2, &Tables, sizeof(uint16));
		if (Res == sizeof (uint16))
		{
			Tables = INT16_SWAP(Tables);
			cmap_encoding_subtable *Sub = (cmap_encoding_subtable*)malloc(ENCODING_TABLE_SIZE*Tables);
			if (Sub)
			{
				Res = GetFontData(hDC, CMAP, CMAP_HEADER_SIZE, Sub, ENCODING_TABLE_SIZE*Tables);
				if (Res == ENCODING_TABLE_SIZE*Tables)
				{
					for (int i = 0; i < Tables; i++)
					{
						Sub[i].platform_id = INT16_SWAP(Sub[i].platform_id);
						Sub[i].encoding_id = INT16_SWAP(Sub[i].encoding_id);
						Sub[i].offset = INT32_SWAP(Sub[i].offset);

						if (Sub[i].platform_id == MICROSOFT_PLATFORM_ID &&
							Sub[i].encoding_id == UNICODE_ENCODING_ID)
						{
							Offset = Sub[i].offset;
						}
					}
				}
				free(Sub);
			}
		}

		if (Offset)
		{
			Length = 0;
			uint Res = GetFontData(hDC, CMAP, Offset + 2, &Length, sizeof(uint16));
			if (Res == sizeof(uint16))
			{
				Length = INT16_SWAP(Length);
				Table = (type_4_cmap*)malloc(Length);
				Res = GetFontData(hDC, CMAP, Offset, Table, Length);
				if (Res == Length)
				{
					Table->format = INT16_SWAP(Table->format);
					Table->length = INT16_SWAP(Table->length);
					Table->language = INT16_SWAP(Table->language);
					Table->seg_count_x_2 = INT16_SWAP(Table->seg_count_x_2);
					Table->search_range = INT16_SWAP(Table->search_range);
					Table->entry_selector = INT16_SWAP(Table->entry_selector);
					Table->range_shift = INT16_SWAP(Table->range_shift);

					if (Table->format == 4)
					{
						uint16 *tbl_end = (uint16 *)((char *)Table + Length);
						uint16 *tbl = Table->arrays;

						while (tbl < tbl_end)
						{
							*tbl = INT16_SWAP(*tbl);
							tbl++;
						}

						Status = true;
					}
				}
			}
		}

		SelectObject(hDC, Old);
		ReleaseDC(0, hDC);
	}

	if (!Status)
	{
		free(Table);
		Table = 0;
	}

	return Table;
}
#endif

GSurface *GFont::GetSurface()
{
	return d->pSurface;
}

bool GFont::Create(const char *face, int height, GSurface *pSurface)
{
	bool FaceChanging = false;
	bool SizeChanging = false;
	bool ValidInitFaceSize = ValidStr(Face()) && PointSize() > 0;

	if (face)
	{
		if (!Face() || strcmp(Face(), face) != 0)
		{
			FaceChanging = true;
		}
		
		Face(face);
	}
	
	if (height > 0)
	{
		SizeChanging = GTypeFace::d->_PtSize != height;
		GTypeFace::d->_PtSize = height;
	}
	
	if ((SizeChanging || FaceChanging) && this == SysFont && ValidInitFaceSize)
	{
		LgiTrace("Warning: Changing sysfont definition.\n");
	}
	
	if (this == SysFont)
	{
		printf("Setting sysfont up '%s' %i\n", Face(), PointSize());
	}

	#if LGI_SDL
	
	GString FaceName;
	#if defined(WIN32)
	const char *Ext = "ttf";
	GString FontPath = "c:\\Windows\\Fonts";
	#elif defined(LINUX)
	const char *Ext = "ttf";
	GString FontPath = "/usr/share/fonts/truetype";
	#elif defined(MAC)
	const char *Ext = "ttc";
	GString FontPath = "/System/Library/Fonts";
	#else
	#error "Put your font path here"
	#endif
	GFile::Path p = FontPath.Get();
	FaceName.Printf("%s.%s", Face(), Ext);
	p += FaceName;
	GString Full = p.GetFull();
	
	if (!FileExists(Full))
	{
		GArray<char*> Files;
		GArray<const char*> Extensions;
		GString Pattern;
		Pattern.Printf("*.%s", Ext);
		Extensions.Add(Pattern.Get());
		LgiRecursiveFileSearch(FontPath, &Extensions, &Files, NULL, NULL, NULL, NULL);
		char *Match = NULL;
		for (unsigned i=0; i<Files.Length(); i++)
		{
			if (stristr(Files[i], FaceName))
				Match = Files[i];
		}
		
		if (Match)
			Full = Match;
		else
			LgiTrace("%s:%i - The file '%s' doesn't exist.\n", _FL, Full.Get());
		Files.DeleteArrays();
	}
	
	FT_Error error = FT_New_Face(Freetype2.Handle(),
								 Full,
								 0,
								 &d->hFont);
	if (error)
	{
		LgiTrace("%s:%i - FT_New_Face failed with %i\n", _FL, error);
	}
	else
	{
		int Dpi = LgiScreenDpi();
		int PtSize = PointSize();
		int PxSize = (int) (PtSize * Dpi / 72.0);
		
		error = FT_Set_Char_Size(	d->hFont,		/* handle to face object           */
									0,				/* char_width in 1/64th of points  */
									PtSize*64,		/* char_height in 1/64th of points */
									Dpi,			/* horizontal device resolution    */
									Dpi);
		if (error)
		{
			LgiTrace("%s:%i - FT_Set_Char_Size failed with %i\n", _FL, error);
		}
		
		d->Height = (int) (ceil((double)d->hFont->height * PxSize / d->hFont->units_per_EM) + 0.0001);
		GTypeFace::d->_Ascent = (double)d->hFont->ascender * PxSize / d->hFont->units_per_EM;
		LgiAssert(d->Height > GTypeFace::d->_Ascent);
		GTypeFace::d->_Descent = d->Height - GTypeFace::d->_Ascent;
		
		return true;
	}

	#elif WINNATIVE

	if (d->hFont)
	{
		DeleteObject(d->hFont);
		d->hFont = 0;
	}
	
	d->pSurface = pSurface;
	HDC hDC = pSurface ? pSurface->Handle() : GetDC(0);
	int Win32Height = WinPointToHeight(PointSize(), hDC);
	
	GTypeFace::d->IsSymbol = GTypeFace::d->_Face &&
								(
									stristr(GTypeFace::d->_Face, "wingdings") ||
									stristr(GTypeFace::d->_Face, "symbol")
								);
	int Cs;
	if (GTypeFace::d->IsSymbol)
	{
		Cs = SYMBOL_CHARSET;
	}
	else
	{
		Cs = ANSI_CHARSET;
	}

	d->OwnerUnderline = Face() &&
						stricmp(Face(), "Courier New") == 0 && 
						(PointSize() == 8 || PointSize() == 9) &&
						GTypeFace::d->_Underline;

	GAutoWString wFace(Utf8ToWide(GTypeFace::d->_Face));
	d->hFont = ::CreateFont(Win32Height,
							0,
							0,
							0,
							GTypeFace::d->_Weight,
							GTypeFace::d->_Italic,
							d->OwnerUnderline ? false : GTypeFace::d->_Underline,
							false,
							Cs,
							OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS,
							GTypeFace::d->_Quality,
							FF_DONTCARE,
							wFace);
	
	if (d->hFont)
	{
		HFONT hFnt = (HFONT) SelectObject(hDC, d->hFont);

		TEXTMETRIC tm;
		ZeroObj(tm);
		if (GetTextMetrics(hDC, &tm))
		{
			d->Height = tm.tmHeight;
			GTypeFace::d->_Ascent = tm.tmAscent;
			GTypeFace::d->_Descent = tm.tmDescent;
			GTypeFace::d->_Leading = tm.tmInternalLeading;
		}
		else
		{
			SIZE Size = {0, 0};
			GetTextExtentPoint32(hDC, L"Ag", 2, &Size);
			d->Height = Size.cy;
		}

		uint Bytes = (MAX_UNICODE + 1) >> 3;
		if (!d->GlyphMap)
		{
			d->GlyphMap = new uchar[Bytes];
		}

		if (d->GlyphMap)
		{
			memset(d->GlyphMap, 0, Bytes);

			GArray<int> OsVer;
			int OsType = LgiGetOs(&OsVer);
			if (OsType == LGI_OS_WIN9X ||
				OsVer[0] < 5) // GetFontUnicodeRanges is supported on >= Win2k
			{
				bool HideUnihan = false;

				LgiAssert(sizeof(type_4_cmap)==16);
				uint16 Length = 0;
				type_4_cmap *t = GetUnicodeTable(Handle(), Length);
				if (t)
				{
					uint16 SegCount = t->seg_count_x_2 / 2;
					uint16 *EndCount = t->GetEndCount();
					uint16 *StartCount = t->GetStartCount();
					uint16 *IdRangeOffset = t->GetIdRangeOffset();

					for (int i = 0; i<SegCount; i++)
					{
						if (IdRangeOffset[i] == 0)
						{
							for (uint u = StartCount[i]; u <= EndCount[i]; u++)
							{
								if (HideUnihan &&
									u >= 0x3400 &&
									u <= 0x9FAF)
								{
									// APPROXIMATE
								}
								else
								{
									// EXACT
								}

								if ((u >> 3) < Bytes)
								{
									d->GlyphMap[u>>3] |= 1 << (u & 7);
								}
							}
						}
						else
						{
							uint16 *End = (uint16*) (((char*)t)+Length);
							ssize_t IdBytes = End - IdRangeOffset;

							for (uint u = StartCount[i]; u <= EndCount[i] && IdRangeOffset[i] != 0xffff; u++)
							{
								uint id = *(IdRangeOffset[i]/2 + (u - StartCount[i]) + &IdRangeOffset[i]);
								if (id)
								{
									if (HideUnihan &&
										u >= 0x3400 &&
										u <= 0x9FAF)
									{
										// APPROXIMATE
									}
									else
									{
										// EXACT
									}

									if ((u >> 3) < Bytes)
									{
										d->GlyphMap[u>>3] |= 1 << (u & 7);
									}
								}
							}
						}
					}
				}
				else
				{
					// not a TTF? assume that it can handle 00-ff in the current ansi cp
					/*
					char *Cp = LgiAnsiToLgiCp();
					for (int i=0; i<=0x7f; i++)
					{
						char16 u;
						uchar c = i;
						void *In = &c;
						int Size = sizeof(c);
						LgiBufConvertCp(&u, "ucs-2", sizeof(u), In, Cp, Size);

						if ((u >> 3) < Bytes)
						{
							GlyphMap[u>>3] |= 1 << (u & 7);
						}
					}
					*/
				}
			}
			else
			{
				typedef DWORD (WINAPI *Proc_GetFontUnicodeRanges)(HDC, LPGLYPHSET);

				if (!d->Gdi32)
					d->Gdi32.Reset(new GLibrary("Gdi32"));
				if (d->Gdi32)
				{
					Proc_GetFontUnicodeRanges GetFontUnicodeRanges = (Proc_GetFontUnicodeRanges)d->Gdi32->GetAddress("GetFontUnicodeRanges");
					if (GetFontUnicodeRanges)
					{
						DWORD BufSize = GetFontUnicodeRanges(hDC, 0);
						LPGLYPHSET Set = (LPGLYPHSET) new char[BufSize];
						if (Set && GetFontUnicodeRanges(hDC, Set) > 0)
						{
							for (DWORD i=0; i<Set->cRanges; i++)
							{
								WCRANGE *Range = Set->ranges + i;
								for (int n=0; n<Range->cGlyphs; n++)
								{
									DWORD u = Range->wcLow + n;
									if (u >> 3 < Bytes)
									{
										d->GlyphMap[u>>3] |= 1 << (u & 7);
									}
								}
							}
						}

						DeleteArray((char*&)Set);
					}
				}
				
				if (GTypeFace::d->IsSymbol)
				{
					// Lies! It's all Lies! Symbol doesn't support non-breaking space.
					int u = 0xa0;
					d->GlyphMap[u >> 3] &= ~(1 << (u & 7));
				}
			}

			if (d->GlyphMap)
			{
				memset(d->GlyphMap, 0xff, 128/8);
			}
		}

		SelectObject(hDC, hFnt);
	}

	if (!pSurface) ReleaseDC(0, hDC);

	return (d->hFont != 0);
	
	#elif defined BEOS

	if (Face())
	{
		d->hFont = new BFont;
		d->hFont->SetSize(PointSize());
	
		int f = 0;
		
		if (Bold())
			f |= B_BOLD_FACE;
		
		if (Italic())
			f |= B_ITALIC_FACE;
		
		if (Underline())
			f |= B_UNDERSCORE_FACE;
		
		if (!f)
			f |= B_REGULAR_FACE;
	
		d->hFont->SetFamilyAndFace(Face(), f);
	
		font_height h;
		d->hFont->GetHeight(&h);
		GTypeFace::d->_Ascent = h.ascent;
		GTypeFace::d->_Descent = h.descent;
		d->Height = ceil(h.ascent + h.descent);
		
		// Create glyph size cache of the first 128 characters. StringWidth is too slow
		// to do for every string at runtime.
		d->CharX[0] = 0;
		for (int i=1; i<128; i++)
		{
			char str[] = {i, 0};
			d->CharX[i] = (int)d->hFont->StringWidth(str, 1);
		}
	}
	
	return true;

	#elif defined __GTK_H__
	
	Destroy();
	
	d->hFont = Gtk::pango_font_description_new();
	if (!d->hFont)
		printf("%s:%i - pango_font_description_new failed: Face='%s' Size=%i Bold=%i Italic=%i\n",
			_FL, Face(), PointSize(), Bold(), Italic());
	else if (!ValidStr(Face()))
		printf("%s:%i - No font face.\n", _FL);
	else if (PointSize() < 2)
		printf("%s:%i - Invalid point size: %i.\n", _FL, PointSize());
	else
	{
		Gtk::pango_font_description_set_family(d->hFont, Face());
		Gtk::pango_font_description_set_size(d->hFont, (double)PointSize() * PANGO_SCALE);
		if (Bold())
			Gtk::pango_font_description_set_weight(d->hFont, Gtk::PANGO_WEIGHT_BOLD);
		
		// printf("Creating pango font %s, %i\n", Face(), PointSize());
		
		// Get metrics for this font...
		Gtk::GtkPrintContext *PrintCtx = pSurface ? pSurface->GetPrintContext() : NULL;
		Gtk::PangoContext *SysCtx = GFontSystem::Inst()->GetContext();
		if (PrintCtx)
			d->PangoCtx = gtk_print_context_create_pango_context(PrintCtx);
		Gtk::PangoFontMetrics *m = Gtk::pango_context_get_metrics(d->PangoCtx ? d->PangoCtx : SysCtx, d->hFont, 0);
		if (!m)
			printf("pango_font_get_metrics failed.\n");
		else
		{
			GTypeFace::d->_Ascent = (double)Gtk::pango_font_metrics_get_ascent(m) / PANGO_SCALE;
			GTypeFace::d->_Descent = (double)Gtk::pango_font_metrics_get_descent(m) / PANGO_SCALE;
			d->Height = ceil(GTypeFace::d->_Ascent + GTypeFace::d->_Descent + 1/*hack the underscores to work*/);
			
			// printf("Created '%s:%i' %f + %f = %i\n", Face(), PointSize(), GTypeFace::d->_Ascent, GTypeFace::d->_Descent, d->Height);

			#if 1
			if (PrintCtx)
			{
				LgiTrace("GFont::Create %s,%f (%i,%i,%i) (%.1f,%.1f,%i)\n",
					Gtk::pango_font_description_get_family(d->hFont),
					(double)Gtk::pango_font_description_get_size(d->hFont) / PANGO_SCALE,
					Gtk::pango_font_metrics_get_ascent(m),
					Gtk::pango_font_metrics_get_descent(m),
					PANGO_SCALE,
					GTypeFace::d->_Ascent,
					GTypeFace::d->_Descent,
					d->Height);
			}
			#endif

			Gtk::pango_font_metrics_unref(m);
			
			return true;
		}
	}
	
	#elif defined MAC
	
		Destroy();
		
		// OSStatus e;

		if (this == SysFont)
			LgiTrace("%s:%i - WARNING: you are re-creating the system font... this is bad!!!!\n", _FL);

		#if USE_CORETEXT

			if (Face())
			{
				if (d->Attributes)
					CFRelease(d->Attributes);

				CGFloat Size = PointSize();
				GString sFamily(Face());
				GString sBold("Bold");
				int keys = 1;
				CFStringRef key[5]  = {	kCTFontFamilyNameAttribute };
				CFTypeRef values[5] = {	sFamily.CreateStringRef() };
                if (!values[0])
                    return false;

				if (Bold())
				{
					key[keys] = kCTFontStyleNameAttribute;
					values[keys++] = sBold.CreateStringRef();
				}

				CFDictionaryRef FontAttrD = CFDictionaryCreate(	kCFAllocatorDefault,
																(const void**)&key,
																(const void**)&values,
																keys,
																&kCFTypeDictionaryKeyCallBacks,
																&kCFTypeDictionaryValueCallBacks);
				if (FontAttrD)
				{
					CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(FontAttrD);
					if (descriptor)
					{
						d->hFont = CTFontCreateWithFontDescriptor(descriptor, Size, NULL);
					}
					else LgiAssert(0);
				}
				else LgiAssert(0);
				
				if (d->hFont)
				{
					GTypeFace::d->_Ascent = CTFontGetAscent(d->hFont);
					GTypeFace::d->_Descent = CTFontGetDescent(d->hFont);
					GTypeFace::d->_Leading = CTFontGetLeading(d->hFont);
					d->Height = ceil(GTypeFace::d->_Ascent +
									 GTypeFace::d->_Descent +
									 GTypeFace::d->_Leading);
					
					
					int keys = 2;
					CFStringRef key[5] = {	kCTFontAttributeName,
											kCTForegroundColorFromContextAttributeName };
					CFTypeRef values[5] = {	d->hFont,
											kCFBooleanTrue };

					if (Underline())
					{
						key[keys] = kCTUnderlineStyleAttributeName;
						CTUnderlineStyle u = kCTUnderlineStyleSingle;
						values[keys++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &u);
					}
					
					d->Attributes = CFDictionaryCreate(	kCFAllocatorDefault,
														(const void**)&key,
														(const void**)&values,
														keys,
														&kCFTypeDictionaryKeyCallBacks,
														&kCFTypeDictionaryValueCallBacks);
					
					return true;
				}
			}

		#else

			if (Face() && !(e = ATSUCreateStyle(&d->hFont)))
			{
				// Lookup ID
				if (!Face())
				{
					LgiAssert(!"No font face");
					return false;
				}
				
				char *sFace = Face();
				CFStringRef fontName = CFStringCreateWithBytes(	kCFAllocatorDefault,
																(UInt8*)sFace,
																strlen(sFace),
																kCFStringEncodingUTF8,
																false);
				if (!fontName)
					return false;
				
				ATSFontRef atsFont = ATSFontFindFromName(fontName, kATSOptionFlagsDefault);
				CFRelease(fontName);
				ATSUFontID font = (ATSUFontID)atsFont;
				if (e)
					printf("%s:%i - Error getting font id (e=%i)\n", _FL, (int)e);
				else
				{
					Fixed Size;
					Size = PointSize() << 16;
					Boolean IsBold = Bold();
					Boolean IsItalic = Italic();
					Boolean IsUnder = Underline();
					
					// Set style attr
					ATSUAttributeTag Tags[]			= {kATSUFontTag, kATSUSizeTag, kATSUQDItalicTag, kATSUQDUnderlineTag, kATSUQDBoldfaceTag};
					ATSUAttributeValuePtr Values[]	= {&font,        &Size,        &IsItalic,        &IsUnder,            &IsBold};
					ByteCount Lengths[]				= {sizeof(font), sizeof(Size), sizeof(IsItalic), sizeof(IsUnder),     sizeof(IsBold)};
					if (!(e = ATSUSetAttributes(d->hFont,
												CountOf(Tags),
												Tags,
												Lengths,
												Values)))
					{
						GDisplayString ds(this, "A");
						d->Height = ds.Y();
						return true;
					}
				}
			}
			else
			{
				printf("%s:%i - Error creating font (e=%i)\n", _FL, (int)e);
			}

		#endif
	
	#endif

	return false;
}

char16 *GFont::_ToUnicode(char *In, int &Len)
{
	char16 *WStr = 0;
	
	if (In &&
		Len > 0)
	{
		WStr = (char16*)LgiNewConvertCp(LGI_WideCharset, In, GTypeFace::d->_CodePage, Len);
		if (WStr)
		{
			int l = StrlenW(WStr);
			if (l < Len)
			{
				Len = l;
			}
		}
	}

	return WStr;
}

#if defined WINNATIVE

bool GFont::Create(GFontType *LogFont, GSurface *pSurface)
{
	if (d->hFont)
	{
		DeleteObject(d->hFont);
		d->hFont = 0;
	}

	if (LogFont)
	{
		// set props
		PointSize(WinHeightToPoint(LogFont->Info.lfHeight));
		GString uFace = LogFont->Info.lfFaceName;
		if (ValidStr(uFace))
		{
			Face(uFace);
			Quality(LogFont->Info.lfQuality);
			Bold(LogFont->Info.lfWeight >= FW_BOLD);
			Italic(LogFont->Info.lfItalic != FALSE);
			Underline(LogFont->Info.lfUnderline != FALSE);

			// create the handle
			Create(0, 0, pSurface);
		}
	}

	return (d->hFont != 0);
}
#elif defined BEOS

bool GFont::Create(GFontType *LogFont, GSurface *pSurface)
{
	bool Status = false;

	if (LogFont)
	{
		Face(LogFont->Info.Face());
		PointSize(LogFont->Info.PointSize());
		Italic(LogFont->Info.Italic());
		Underline(LogFont->Info.Underline());
		Bold(LogFont->Info.Bold());

		Status = true;
	}
	
	return Status;
}

#else

bool GFont::Create(GFontType *LogFont, GSurface *pSurface)
{
	if (LogFont)
	{
		return Create(LogFont->GetFace(), LogFont->GetPointSize(), pSurface);
	}

	return false;
}
#endif

GFont &GFont::operator =(GFont &f)
{
	Face(f.Face());
	PointSize(f.PointSize());
	TabSize(f.TabSize());
	Quality(f.Quality());
	Fore(f.Fore());
	Back(f.Back());
	SetWeight(f.GetWeight());
	Italic(f.Italic());
	Underline(f.Underline());
	Transparent(f.Transparent());
	
	return *this;
}

///////////////////////////////////////////////////////////////////////
GFontType::GFontType(const char *face, int pointsize)
{
	#if defined WINNATIVE

	ZeroObj(Info);
	if (face)
	{
		swprintf_s(Info.lfFaceName, CountOf(Info.lfFaceName), L"%S", face);
	}
	if (pointsize)
	{
		Info.lfHeight = WinHeightToPoint(pointsize);
	}

	#else

	if (face) Info.Face(face);
	if (pointsize) Info.PointSize(pointsize);

	#endif
}

GFontType::~GFontType()
{
}

char *GFontType::GetFace()
{
	#ifdef WINNATIVE
	Buf = Info.lfFaceName;
	return Buf;
	#else
	return Info.Face();
	#endif
}

void GFontType::SetFace(const char *Face)
{
	#ifdef WINNATIVE
	if (Face)
	{
		swprintf_s(Info.lfFaceName, CountOf(Info.lfFaceName), L"%S", Face);
	}
	else
	{
		Info.lfFaceName[0] = 0;
	}
	#else
	Info.Face(Face);
	#endif
}

int GFontType::GetPointSize()
{
	#ifdef WINNATIVE
	return WinHeightToPoint(Info.lfHeight);
	#else
	return Info.PointSize();
	#endif
}

void GFontType::SetPointSize(int PointSize)
{
	#ifdef WINNATIVE
	Info.lfHeight = WinPointToHeight(PointSize);
	#else
	Info.PointSize(PointSize);
	#endif
}

bool GFontType::DoUI(GView *Parent)
{
	bool Status = false;

	#if defined WIN32
	void *i = &Info;
	int bytes = sizeof(Info);
	#else
	char i[128];
	int bytes = sprintf_s(i, sizeof(i), "%s,%i", Info.Face(), Info.PointSize());
	#endif

	GFontSelect Dlg(Parent, i, bytes);
	if (Dlg.DoModal() == IDOK)
	{
		#ifdef WIN32
		Dlg.Serialize(i, sizeof(Info), true);
		#else
		if (Dlg.Face) Info.Face(Dlg.Face);
		Info.PointSize(Dlg.Size);
		#endif

		Status = true;
	}

	return Status;
}

bool GFontType::GetDescription(char *Str, int SLen)
{
	if (Str && GetFace())
	{
		sprintf_s(Str, SLen, "%s, %i pt", GetFace(), GetPointSize());
		return true;
	}

	return false;
}

/*
bool GFontType::Serialize(ObjProperties *Options, char *OptName, bool Write)
{
	bool Status = false;

	if (Options && OptName)
	{
		#if defined WIN32
		if (Write)
		{
			Status = Options->Set(OptName, &Info, sizeof(Info));
		}
		else
		{
			void *Data = 0;
			int Len = 0;
			if (Options->Get(OptName, Data, Len) &&
				Len == sizeof(Info) &&
				Data)
			{
				memcpy(&Info, Data, Len);
				Status = true;
			}
		}
		#else
		if (Write)
		{
			char Temp[128];
			sprintf_s(Temp, sizeof(Temp), "%s,%i pt", Info.Face(), Info.PointSize());
			Status = Options->Set(OptName, Temp);
		}
		else
		{
			char *Temp = 0;
			if (Options->Get(OptName, Temp) && ValidStr(Temp))
			{
				char *OurCopy = NewStr(Temp);
				if (OurCopy)
				{
					char *Comma = strchr(OurCopy, ',');
					if (Comma)
					{
						*Comma++ = 0;
						Info.Face(OurCopy);
						Info.PointSize(atoi(Comma));
						Status = true;
					}

					DeleteArray(OurCopy);
				}
			}
		}
		#endif
	}

	return Status;
}
*/

bool GFontType::Serialize(GDom *Options, const char *OptName, bool Write)
{
	bool Status = false;

	if (Options && OptName)
	{
		GVariant v;
		#if defined WINNATIVE
		if (Write)
		{
			v.SetBinary(sizeof(Info), &Info);
			Status = Options->SetValue(OptName, v);
		}
		else
		{
			if (Options->GetValue(OptName, v))
			{
				if (v.Type == GV_BINARY &&
					v.Value.Binary.Length == sizeof(Info))
				{
					memcpy(&Info, v.Value.Binary.Data, sizeof(Info));
					Status = ValidStrW(Info.lfFaceName);
				}
			}
		}
		#else
		if (Write)
		{
			char Temp[128];
			sprintf_s(Temp, sizeof(Temp), "%s,%i pt", Info.Face(), Info.PointSize());
			Status = Options->SetValue(OptName, v = Temp);
		}
		else
		{
			if (Options->GetValue(OptName, v) && ValidStr(v.Str()))
			{
				char *Comma = strchr(v.Str(), ',');
				if (Comma)
				{
					*Comma++ = 0;
					int PtSize = atoi(Comma);
					
					Info.Face(v.Str());
					Info.PointSize(PtSize);
					// printf("FontTypeSer getting '%s' = '%s' pt %i\n", OptName, v.Str(), PtSize);
					Status = true;
				}
			}
		}
		#endif
	}

	return Status;
}

bool GFontType::GetConfigFont(const char *Tag)
{
	bool Status = false;

	GXmlTag *Font = LgiApp->GetConfig(Tag);
	if (Font)
	{
		// read from config file
		char *f, *p;
		if ((f = Font->GetAttr("Face")) && (p = Font->GetAttr("Point")))
		{
			SetFace(f);
			SetPointSize(atoi(p));
			Status = true;
		}
	}

	return Status;
}

class GFontTypeCache
{
	char DefFace[64];
	int DefSize;
	char Face[64];
	int Size;

public:
	GFontTypeCache(char *defface, int defsize)
	{
		ZeroObj(Face);
		Size = -1;
		strcpy_s(DefFace, sizeof(DefFace), defface);
		DefSize = defsize;
	}
	
	char *GetFace(char *Type = 0)
	{
		#ifdef LINUX
		if (!IsInit())
		{
			char f[256];
			int s;
			if (_GetSystemFont(Type, f, sizeof(f), s))
			{
				strcpy_s(Face, sizeof(Face), f);
				Size = s;
			}
			else
			{
				Face[0] = 0;
				Size = 0;
			}
		}
		#endif

		return ValidStr(Face) ? Face : DefFace;
	}
	
	int GetSize()
	{
		return Size > 0 ? Size : DefSize;
	}
	
	bool IsInit()
	{
		return Size >= 0;
	}
};

#ifdef MAC

bool MacGetSystemFont(GTypeFace &Info, CTFontUIFontType Which)
{
	CTFontRef ref = CTFontCreateUIFontForLanguage(Which, 0.0, NULL);
	if (!ref)
		return false;

	bool Status = false;
	CFStringRef name = CTFontCopyFamilyName(ref);
	if (name)
	{
		CGFloat sz = CTFontGetSize(ref);
		GString face(name);

		Info.Face(face);
		Info.PointSize((int)sz + MAC_FONT_SIZE_OFFSET);

		CFRelease(name);
		Status = true;
	}

	CFRelease(ref);

	return Status;
}

#endif

bool GFontType::GetSystemFont(const char *Which)
{
	bool Status = false;

	if (!Which)
	{
		LgiAssert(!"No param supplied.");
		return false;
	}
	
	#if LGI_SDL

	#elif defined WINNATIVE

	// Get the system settings
	NONCLIENTMETRICS info;
	info.cbSize = sizeof(info);
	#if (WINVER >= 0x0600)
	GArray<int> Ver;
	if (LgiGetOs(&Ver) == LGI_OS_WIN32 &&
		Ver[0] <= 5)
		info.cbSize -= 4;
	#endif
	BOOL InfoOk = SystemParametersInfo(	SPI_GETNONCLIENTMETRICS,
										info.cbSize,
										&info,
										0);
	if (!InfoOk)
	{
		LgiTrace("%s:%i - SystemParametersInfo failed with 0x%x (info.cbSize=%i, os=%i, %i)\n",
			_FL, GetLastError(),
			info.cbSize,
			LgiGetOs(), LGI_OS_WIN9X);
	}

	// Convert windows font height into points
	int Height = WinHeightToPoint(info.lfMessageFont.lfHeight);

	#elif defined __GTK_H__

	// Define some defaults.. in case the system settings aren't there
	static char DefFont[64] =
	#ifdef __CYGWIN__
		"Luxi Sans";
	#else
		"Sans";
	#endif
	int DefSize = 10;
	int Offset = 0;

	static bool First = true;
	if (First)
	{
		bool ConfigFontUsed = false;
		char p[MAX_PATH];
		LgiGetSystemPath(LSP_HOME, p, sizeof(p));
		LgiMakePath(p, sizeof(p), p, ".lgi.conf");
		if (FileExists(p))
		{
			GAutoString a(ReadTextFile(p));
			if (a)
			{
				GString s;
				s = a.Get();
				GString::Array Lines = s.Split("\n");
				for (int i=0; i<Lines.Length(); i++)
				{
					if (Lines[i].Find("=")>=0)
					{
						GString::Array p = Lines[i].Split("=", 1);
						if (!_stricmp(p[0].Lower(), "font"))
						{
							GString::Array d = p[1].Split(":");
							if (d.Length() > 1)
							{
								strcpy_s(DefFont, sizeof(DefFont), d[0]);
								int PtSize = d[1].Int();
								if (PtSize > 0)
								{
									DefSize = PtSize;
									ConfigFontUsed = true;
									
									printf("Config font %s : %i\n", DefFont, DefSize);
								}
							}
						}
					}
				}
			}
			else printf("Can't read '%s'\n", p);
		}
		
		if (!ConfigFontUsed)
		{	
			Gtk::GtkStyle *s = Gtk::gtk_style_new();
			if (s)
			{
				const char *fam = Gtk::pango_font_description_get_family(s->font_desc);
				if (fam)
				{
					strcpy_s(DefFont, sizeof(DefFont), fam);
				}
				else printf("%s:%i - pango_font_description_get_family failed.\n", _FL);

				if (Gtk::pango_font_description_get_size_is_absolute(s->font_desc))
				{
					float Px = Gtk::pango_font_description_get_size(s->font_desc) / PANGO_SCALE;
					float Dpi = (float)LgiScreenDpi();
					DefSize = (Px * 72.0) / Dpi;
					printf("pango px=%f, Dpi=%f\n", Px, Dpi);
				}
				else
				{
					DefSize = Gtk::pango_font_description_get_size(s->font_desc) / PANGO_SCALE;
				}
				
				g_object_unref(s);
			}
			else printf("%s:%i - gtk_style_new failed.\n", _FL);
		}
		
		First = false;
	}
	
	#endif

	if (!_stricmp(Which, "System"))
	{
		Status = GetConfigFont("Font-System");
		if (!Status)
		{
			// read from system
			#if LGI_SDL

				#if defined(WIN32)
				Info.Face("Tahoma");
				Info.PointSize(11);
				Status = true;
				#elif defined(MAC)
				Info.Face("LucidaGrande");
				Info.PointSize(11);
				Status = true;
				#elif defined(LINUX)
				Info.Face("Sans");
				Info.PointSize(11);
				Status = true;
				#else
				#error fix me
				#endif
			
			#elif defined WINNATIVE

				if (InfoOk)
				{
					// Copy the font metrics
					memcpy(&Info, &info.lfMessageFont, sizeof(Info));
					Status = true;
				}
				else LgiTrace("%s:%i - Info not ok.\n", _FL);

			#elif defined BEOS

				// BeOS has no system wide setting so give a valid default
				Info.Face("Swis721 BT");
				Info.PointSize(11);
				Status = true;

			#elif defined __GTK_H__

				Info.Face(DefFont);
				Info.PointSize(DefSize);
				Status = true;
		
			#elif defined MAC
			

				#if USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontControlContent);

				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeSmallSystemFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size);
					Status = true;
					
					// printf("System=%s,%i\n", Info.Face(), Size);
				}
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Menu"))
	{
		Status = GetConfigFont("Font-Menu");
		if (!Status)
		{
			#if LGI_SDL
			
			LgiAssert(!"Impl me.");
			
			#elif defined WINNATIVE

			if (InfoOk)
			{
				// Copy the font metrics
				memcpy(&Info, &info.lfMenuFont, sizeof(Info));
				Status = true;
			}

			#elif defined BEOS

			// BeOS has no system wide setting so give a valid default
			Info.Face("Swis721 BT");
			Info.PointSize(11);
			Status = true;

			#elif defined __GTK_H__

			Info.Face(DefFont);
			Info.PointSize(DefSize);
			Status = true;

			#elif defined MAC && !defined COCOA
			
				#if USE_CORETEXT

					Status = MacGetSystemFont(Info, kCTFontUIFontMenuItem);

				#else

					Str255 Name;
					SInt16 Size;
					Style St;
					OSStatus e = GetThemeFont(	kThemeMenuItemFont,
												smSystemScript,
												Name,
												&Size,
												&St);
					if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
					else
					{
						Info.Face(p2c(Name));
						Info.PointSize(Size);
						Status = true;
					}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Caption"))
	{
		Status = GetConfigFont("Font-Caption");
		if (!Status)
		{
			#if LGI_SDL
			
				LgiAssert(!"Impl me.");
			
			#elif defined WINNATIVE

				if (InfoOk)
				{
					// Copy the font metrics
					memcpy(&Info, &info.lfCaptionFont, sizeof(Info));
					Status = true;
				}
			
			#elif defined BEOS

				// BeOS has no system wide setting so give a valid default
				Info.Face("Swis721 BT");
				Info.PointSize(11);
				Status = true;

			#elif defined LINUX

			#elif defined __GTK_H__

				Info.Face(DefFont);
				Info.PointSize(DefSize-1);
				Status = true;

			#elif defined MAC && !defined COCOA
			
				#if USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontToolbar);
			
				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeToolbarFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size);
					Status = true;
				}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Status"))
	{
		Status = GetConfigFont("Font-Status");
		if (!Status)
		{
			#if LGI_SDL
			
				LgiAssert(!"Impl me.");
			
			#elif defined WINNATIVE
			
			if (InfoOk)
			{
				// Copy the font metrics
				memcpy(&Info, &info.lfStatusFont, sizeof(Info));
				Status = true;
			}
			
			#elif defined BEOS

			// BeOS has no system wide setting so give a valid default
			Info.Face("Swis721 BT");
			Info.PointSize(11);
			Status = true;

			#elif defined __GTK_H__

			Info.Face(DefFont);
			Info.PointSize(DefSize);
			Status = true;

			#elif defined MAC && !defined COCOA
			
				#if USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontSystemDetail);
			
				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeToolbarFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size);
					Status = true;
				}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Small"))
	{
		Status = GetConfigFont("Font-Small");
		if (!Status)
		{
			#if LGI_SDL
			
				LgiAssert(!"Impl me.");
			
			#elif defined WINNATIVE

			if (InfoOk)
			{
				// Copy the font metrics
				memcpy(&Info, &info.lfSmCaptionFont, sizeof(Info));
				if (LgiGetOs() == LGI_OS_WIN9X && _wcsicmp(Info.lfFaceName, L"ms sans serif") == 0)
				{
					SetFace("Arial");
				}

				// Make it a bit smaller than the system font
				Info.lfHeight = WinPointToHeight(WinHeightToPoint(info.lfMessageFont.lfHeight)-1);
				Info.lfWeight = FW_NORMAL;

				Status = true;
			}

			#elif defined BEOS

			// BeOS has no system wide setting so give a valid default
			Info.Face("Swis721 BT");
			Info.PointSize(9);
			Status = true;

			#elif defined __GTK_H__

			Info.Face(DefFont);
			Info.PointSize(DefSize-1);
			Status = true;
			
			#elif defined MAC && !defined COCOA
			
				#if USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontSmallSystem);
			
				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeSmallSystemFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size - 2);
					Status = true;
				}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Fixed"))
	{
		Status = GetConfigFont("Font-Fixed");
		if (!Status)
		{
			#if LGI_SDL
			
				LgiAssert(!"Impl me.");
			
			#elif defined WINNATIVE

			// SetFace("Courier New");
			SetFace("Consolas");
			Info.lfHeight = WinPointToHeight(10);
			Status = true;

			#elif defined BEOS

			Info.Face("Courier10 BT");
			Info.PointSize(12);
			Status = true;

			#elif defined LINUX

			Info.Face("Courier New");
			Info.PointSize(DefSize);
			Status = true;
			
			#elif defined MAC
			
			Status = MacGetSystemFont(Info, kCTFontUIFontUserFixedPitch);

			#endif
		}
	}
	else
	{
		LgiAssert(!"Invalid param supplied.");
	}
	
	// printf("GetSystemFont(%s)=%i %s,%i\n", Which, Status, Info.Face(), Info.PointSize());
	return Status;
}

bool GFontType::GetFromRef(OsFont Handle)
{
	#if defined WIN32
	return GetObject(Handle, sizeof(Info), &Info) == sizeof(Info);
	#else
	// FIXME
	return false;
	#endif
}

GFont *GFontType::Create(GSurface *pSurface)
{
	GFont *New = new GFont;
	if (New)
	{
		if (!New->Create(this, pSurface))
		{
			DeleteObj(New);
		}
		
		if (New)
			LgiAssert(New->GetHeight() > 0);
	}
	return New;
}

char16 WinSymbolToUnicode[256] = 
{
    /*   0 to  15 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*  16 to  31 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*  32 to  47 */ 32, 9998, 9986, 9985, 0, 0, 0, 0, 9742, 9990, 9993, 9993, 0, 0, 0, 0,
    /*  48 to  63 */ 0, 0, 0, 0, 0, 0, 8987, 9000, 0, 0, 0, 0, 0, 0, 9991, 9997,
    /*  64 to  79 */ 9997, 9996, 0, 0, 0, 9756, 9758, 9757, 9759, 0, 9786, 9786, 9785, 0, 9760, 0,
    /*  80 to  95 */ 0, 9992, 9788, 0, 10052, 10014, 10014, 10013, 10016, 10017, 9770, 9775, 2384, 9784, 9800, 9801,
    /*  96 to 111 */ 9802, 9803, 9804, 9805, 9806, 9807, 9808, 9809, 9810, 9811, 38, 38, 9679, 10061, 9632, 9633,
    /* 112 to 127 */ 9633, 10065, 10066, 9674, 9674, 9670, 10070, 9670, 8999, 9043, 8984, 10048, 10047, 10077, 10078, 0,
    /* 128 to 143 */ 9450, 9312, 9313, 9314, 9315, 9316, 9317, 9318, 9319, 9320, 9321, 0, 10102, 10103, 10104, 10105,
    /* 144 to 159 */ 10106, 10107, 10108, 10109, 10110, 10111, 10087, 9753, 9753, 10087, 10087, 9753, 9753, 10087, 8226, 9679,
    /* 160 to 175 */ 160, 9675, 9675, 9675, 9737, 9737, 10061, 9642, 9633, 0, 10022, 9733, 10038, 10039, 10040, 10037,
    /* 176 to 191 */ 0, 0, 10023, 0, 65533, 10026, 10032, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 192 to 207 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10086, 10086, 10086,
    /* 208 to 223 */ 10086, 10086, 10086, 10086, 10086, 9003, 8998, 0, 10146, 0, 0, 0, 10162, 0, 0, 0,
    /* 224 to 239 */ 0, 0, 0, 0, 0, 0, 0, 0, 10132, 0, 0, 0, 0, 0, 0, 8678,
    /* 240 to 255 */ 8680, 8679, 8681, 8660, 8661, 8662, 8663, 8665, 8664, 0, 0, 10007, 10003, 9746, 9745, 0,
};

GAutoString GFont::ConvertToUnicode(char16 *Input, int Len)
{
	GAutoString a;
	
	if (GTypeFace::d->IsSymbol)
	{
		// F***ing wingdings.
		if (Input)
		{
			GStringPipe p(256);
			if (Len < 0)
				Len = StrlenW(Input);
			char16 *c = Input, *e = Input + Len;
			while (c < e)
			{
				if (*c < 256 && WinSymbolToUnicode[*c])
				{
					p.Write(WinSymbolToUnicode + *c, sizeof(char16));
				}
				else
				{
					p.Write(c, sizeof(char16));
				}
				c++;
			}
			
			GAutoWString w(p.NewStrW());
			a.Reset(WideToUtf8(w));
		}
	}
	else
	{
		// Normal utf-8 text...
		a.Reset(WideToUtf8(Input, Len));
	}
	
	return a;
}

