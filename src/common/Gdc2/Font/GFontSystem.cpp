#include <stdlib.h>
#include <locale.h>

#include "Lgi.h"
#include "GToken.h"
#include "GLibrary.h"
#include "GLibraryUtils.h"

#if defined(LGI_STATIC) || defined(BEOS)
#undef HAS_ICONV
#endif

#if HAS_ICONV
//
// Get 'iconv.h' from http://www.gnu.org/software/libiconv
// Current download at time of writing:
//    http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.8.tar.gz
//
// Then add whatever include path to your project or development
// settings. Otherwise you can build without extended charset
// support by changing the HAS_ICONV define in Lgi.h to '0'
//
// Linux should always build with iconv, on windows you may or 
// may not want to bother depending on what sort of app your
// writing.
//
#include "iconv.h"

#if defined(WIN32)
typedef const char IconvChar;
#else
typedef char IconvChar;
#endif
#endif

/////////////////////////////////////////////////////////////////////
// Private growable class for binary compatability
class GFontSystemPrivate : public GLibrary
{
public:
	bool DefaultGlyphSub;
	int Used, Refs;
	bool FontTableLoaded;
	bool SubSupport;
	bool CheckedConfig;
	
	#ifdef __GTK_H__
	Gtk::PangoFontMap *Map;
	Gtk::PangoContext *Ctx;
	
	GFontSystemPrivate()
	{
		Map = Gtk::pango_cairo_font_map_get_default();
		if (!Map)
			LgiAssert(!"pango_cairo_font_map_get_default failed.\n");
		
		Ctx = Gtk::pango_cairo_font_map_create_context((Gtk::PangoCairoFontMap*)Map);
		if (!Ctx)
			LgiAssert(!"pango_cairo_font_map_create_context failed.\n");
	}
	#endif

	#if HAS_ICONV
	#ifdef WIN32

	DynFunc2(iconv_t,	libiconv_open, const char*, tocode, const char*, fromcode);
	DynFunc5(size_t,	libiconv, iconv_t, cd, IconvChar**, inbuf, size_t*, inbytesleft, char**, outbuf, size_t*, outbytesleft);
	DynFunc1(int,		libiconv_close, iconv_t, cd);

	#elif !defined(MAC)

	// Use glibc I guess
	iconv_t libiconv_open(const char *tocode, const char *fromcode)
	{
		return ::iconv_open(tocode, fromcode);
	}

	size_t libiconv(iconv_t cd, IconvChar** inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
	{
		return ::iconv(cd, inbuf, inbytesleft, outbuf, outbytesleft);
	}

	int libiconv_close(iconv_t cd)
	{
		return ::iconv_close(cd);
	}

	bool IsLoaded()
	{
		return true;
	}

	#endif
	#endif // HAS_ICONV
};

/////////////////////////////////////////////////////////////////////
static bool FontSystemDone = false;
GFontSystem *GFontSystem::Me = 0;
GFontSystem *GFontSystem::Inst()
{
	if (!Me && !FontSystemDone)
		new GFontSystem;
	
	return Me;
}

GFontSystem::GFontSystem()
{
	Me = this;
	d = new GFontSystemPrivate;

	// Glyph sub setup
	int Os = LgiGetOs();

	d->SubSupport =	(Os == LGI_OS_LINUX) ||
					(Os == LGI_OS_WIN64) ||
					(Os == LGI_OS_WIN32); //  && Rev == 0);  // WinXP does it's own glyph substitution
	d->DefaultGlyphSub = d->SubSupport;
	d->CheckedConfig = false;
	d->FontTableLoaded = false;
	d->Used = 1;		// the '0th' font spot isn't used
						// because Lut[Char] == 0 means no font
						// available
	d->Refs = 0;
	ZeroObj(Lut);		// Clear the table to 'no font'
	ZeroObj(Font);		// Initialize the list of fonts to empty
}

GFontSystem::~GFontSystem()
{
	// Clean up all our resources
	for (int i=0; i<d->Used; i++)
	{
		DeleteObj(Font[i]);
	}

	DeleteObj(d);
	Me = 0;
	FontSystemDone = true;
}

#ifdef __GTK_H__
Gtk::PangoFontMap *GFontSystem::GetFontMap()
{
	return d->Map;
}

Gtk::PangoContext *GFontSystem::GetContext()
{
	return d->Ctx;
}
#endif

bool GFontSystem::GetGlyphSubSupport()
{
	return d->SubSupport;
}

bool GFontSystem::GetDefaultGlyphSub()
{
	if (!d->CheckedConfig && LgiApp)
	{
		GXmlTag *FontSys = LgiApp->GetConfig("font_system");
		if (FontSys)
		{
			char *GlyphSub;
			if ((GlyphSub = FontSys->GetAttr("glyph_sub")))
			{
				d->DefaultGlyphSub = atoi(GlyphSub) != 0;
			}
		}
		d->CheckedConfig = true;
	}

	return d->DefaultGlyphSub;
}

void GFontSystem::SetDefaultGlyphSub(bool i)
{
	if (d->SubSupport)
		d->DefaultGlyphSub = i;
}

#ifdef WINNATIVE
int CALLBACK _EnumFonts(ENUMLOGFONT FAR *lpelf,
						NEWTEXTMETRIC FAR *lpntm,
						int FontType,
						LPARAM lParam)
{
	GString::Array *p = (GString::Array*) lParam;
	if (p)
		p->New() = lpelf->elfLogFont.lfFaceName;
	return true;
}
#endif

int StringSort(GString *a, GString *b)
{
	if (a && b) return stricmp(*a, *b);
	return 0;
}

bool GFontSystem::EnumerateFonts(GString::Array &Fonts)
{
	Fonts.SetFixedLength(false);
	if (!AllFonts.Length())
	{
		#if defined WINNATIVE

		HDC hDC = CreateCompatibleDC(NULL);
		if (hDC)
		{
			EnumFontFamilies(	hDC,
								NULL,
								(FONTENUMPROC) _EnumFonts,
								(LPARAM) &AllFonts);

			DeleteDC(hDC);
		}

		#elif defined BEOS

		int32 numFamilies = count_font_families();
		for (int32 i = 0; i < numFamilies; i++)
		{
			font_family Temp;
			uint32 flags;

			if (get_font_family(i, &Temp, &flags) == B_OK)
			{
				AllFonts.Insert(NewStr(Temp));
			}
		}

		#elif defined __GTK_H__

	    Gtk::PangoFontFamily ** families;
	    int n_families;
	    Gtk::PangoFontMap * fontmap;

	    fontmap = Gtk::pango_cairo_font_map_get_default();
	    Gtk::pango_font_map_list_families (fontmap, & families, & n_families);
	    for (int i = 0; i < n_families; i++)
	    {
	        Gtk::PangoFontFamily * family = families[i];
	        const char * family_name;

	        family_name = Gtk::pango_font_family_get_name (family);
	        AllFonts.New() = family_name;
	    }
	    Gtk::g_free (families);

		#elif defined XWIN

		XObject o;
		XftFontSet *Set = XftListFonts(	o.XDisplay(), 0,
										0,
										XFT_FAMILY,
										0);
		if (Set)
		{
			for (int i=0; i<Set->nfont; i++)
			{
				char s[256];
				if (XftNameUnparse(Set->fonts[i], s, sizeof(s)))
				{
					AllFonts.Insert(NewStr(s));
				}
			}
			
			XftFontSetDestroy(Set);
		}
		
		#elif defined MAC && !defined COCOA

		CFArrayRef fontFamilies = CTFontManagerCopyAvailableFontFamilyNames();
		if (fontFamilies)
		{
			for(CFIndex i = 0; i < CFArrayGetCount(fontFamilies); i++)
			{
				CFStringRef fontName = (CFStringRef) CFArrayGetValueAtIndex(fontFamilies, i);
				if (fontName)
				{
					AllFonts.New() = fontName;
				}
			}
			CFRelease(fontFamilies);
		}
		
		#endif

		AllFonts.Sort(StringSort);
	}

	Fonts = AllFonts;
	return true;
}

bool GFontSystem::HasIconv(bool Quiet)
{
	if (d->IsLoaded())
		return true;

	bool Status = d->Load("libiconv-1.9.1." LGI_LIBRARY_EXT);
	if (!Status && !Quiet)
	{
	    if (!NeedsCapability("libiconv"))
	    {
	        static bool Warn = true;
	        if (Warn)
	        {
	            Warn = false;
	            LgiAssert(!"Iconv is not available");
	        }
	    }
	}
	return Status;
}

#if defined MAC && !defined COCOA
// This converts a normal charset to an Apple encoding ID
static CFStringEncoding CharsetToEncoding(const char *cs)
{
	CFStringRef InputCs = CFStringCreateWithCString(0, cs, kCFStringEncodingUTF8);
	if (!InputCs)
		return kCFStringEncodingUTF8; // Um what to do here?
	CFStringEncoding enc = CFStringConvertIANACharSetNameToEncoding(InputCs);
	CFRelease(InputCs);
	return enc;
}
#endif


ssize_t GFontSystem::IconvConvert(const char *OutCs, GStreamI *Out, const char *InCs, const char *&In, ssize_t InLen)
{
	LgiAssert(InLen > 0);

	if (!Out || !In)
		return 0;
	    
#if defined(MAC)

	#if !defined COCOA
	char Buf[2 << 10];
	CFStringEncoding InEnc = CharsetToEncoding(InCs);
	CFStringEncoding OutEnc = CharsetToEncoding(OutCs);
	if (InEnc != kCFStringEncodingInvalidId &&
		OutEnc != kCFStringEncodingInvalidId)
	{
		CFStringRef r = CFStringCreateWithBytes(0, (const UInt8 *)In, InLen, InEnc, false);
		if (r)
		{
			CFRange g = { 0, CFStringGetLength(r) };
			CFIndex used = 0;
			CFIndex ret;
			while ((ret = CFStringGetBytes(r, g, OutEnc, '?', false, (UInt8*)Buf, sizeof(Buf), &used)) > 0 && g.length > 0)
			{
				// char16 *b = (char16*)Buf;
				Out->Write(Buf, used);
				g.location += ret;
				g.length -= ret;
			}

			CFRelease(r);
		}
		else return 0;
	}
	else return 0;
	#endif

#elif HAS_ICONV

	if (!HasIconv(false))
	{
		return 0;
	}

	char Buf[2 << 10];
	iconv_t Conv;
	if ((NativeInt)(Conv = d->libiconv_open(OutCs, InCs)) >= 0)
	{
		char *i = (char*)In;
		LgiAssert((NativeInt)Conv != 0xffffffff);

		while (InLen)
		{
			char *o = (char*)Buf;
			size_t OutLen = sizeof(Buf);
			ssize_t OldInLen = InLen;
			size_t InSz = InLen;
			ssize_t s = d->libiconv(Conv, (IconvChar**)&i, &InSz, &o, &OutLen);
			InLen = InSz;
			Out->Write((uchar*)Buf, sizeof(Buf) - OutLen);
			if (OldInLen == InLen) break;
		}

		d->libiconv_close(Conv);
	}
	else
	{
		LgiTrace("Iconv won't load.\n");
		return 0;
	}

#endif

	return 1;
}

ssize_t GFontSystem::IconvConvert(const char *OutCs, char *Out, ssize_t OutLen, const char *InCs, const char *&In, ssize_t InLen)
{
	ssize_t Status = 0;

    if (!Out || !In || !HasIconv(false))
        return 0;

#if defined(MAC)

	#if !defined COCOA
	CFStringEncoding InEnc = CharsetToEncoding(InCs);
	CFStringEncoding OutEnc = CharsetToEncoding(OutCs);
	if (InEnc != kCFStringEncodingInvalidId &&
		OutEnc != kCFStringEncodingInvalidId)
	{
		CFStringRef r = CFStringCreateWithBytes(0, (const UInt8 *)In, InLen, InEnc, false);
		if (r)
		{
			CFRange g = { 0, CFStringGetLength(r) };
			CFIndex ret = CFStringGetBytes(r, g, OutEnc, '?', false, (UInt8*)Out, OutLen, 0);
			CFRelease(r);
			return ret;
		}
	}
	#endif

#elif HAS_ICONV

	// Set locale yet?
	static bool sl = false;
	if (!sl)
	{
		sl = true;
		setlocale(LC_ALL, "");
	}

	// Iconv conversion
	// const char *InCs = InInfo->GetIconvName();
	// const char *OutCs = OutInfo->GetIconvName();
	iconv_t Conv;
	if ((Conv = d->libiconv_open(OutCs, InCs)) >= 0)
	{
		size_t InSz = InLen;
		size_t OutSz = OutLen;
		char *o = Out;
		char *i = (char*)In;

		// Convert
		char *Start = o;
		ssize_t s = d->libiconv(Conv, (IconvChar**)&i, &InSz, &o, &OutSz);
		InLen = InSz;
		OutLen = OutSz;
		d->libiconv_close(Conv);

		In = (const char*)i;
		Status = (NativeInt)o-(NativeInt)Out;
	}
	else
	{
		LgiTrace("Iconv not present/won't load.\n");
	}

#endif

	return Status;
}

GFont *GFontSystem::GetBestFont(char *Str)
{
	GFont *MatchingFont = 0;

	if (d->SubSupport)
	{
		char16 *s = Utf8ToWide(Str);
		if (s)
		{
			// Make list of possible fonts
			List<GFont> Possibles;
			char16 *i;
			for (i = s; *i; i++)
			{
				GFont *Font = GetGlyph(*i, SysFont);
				if (Font)
				{
					bool Has = false;
					for (GFont *h=Possibles.First(); h; h=Possibles.Next())
					{
						if (h == Font)
						{
							Has = true;
							break;
						}
					}
					if (!Has)
					{
						Possibles.Insert(Font);
					}
				}
			}

			// Choose best match amongst possibles
			int MatchingChars = 0;
			for (GFont *h=Possibles.First(); h; h=Possibles.Next())
			{
				int Chars = 0;
				for (i = s; *i; i++)
				{
					if (h->GetGlyphMap() &&
						_HasUnicodeGlyph(h->GetGlyphMap(), *i))
					{
						Chars++;
					}
				}

				if (!MatchingFont ||
					Chars > MatchingChars)
				{
					MatchingFont = h;
				}
			}

			DeleteArray(s);	
		}
	}

	return MatchingFont;
}

typedef LHashTbl<ConstStrKey<char,false>,int> FontMap;
DeclGArrayCompare(FontNameCmp, GString, FontMap)
{
	int ap = param->Find(*a);
	int bp = param->Find(*b);
	return bp - ap;
}

bool GFontSystem::AddFont(GAutoPtr<GFont> Fnt)
{
	if (!Fnt)
		return false;

	if (d->Used >= CountOf(Font))
		return false;

	Fnt->Create();

	auto *Map = Fnt->GetGlyphMap();
	if (Map)
	{
		uint8 Used = d->Used;

		// Insert all the characters of this font into the LUT
		// so that we can map from a character back to the font
		for (int k=0; k<=MAX_UNICODE; k += 8)
		{
			// unroll the loop for maximum speed..
			uint8 m = Map[k >> 3];
			
			#define TestLut(i) \
				if (!Lut[k+i] && (m & (1 << i))) \
					Lut[k+i] = Used;
			TestLut(0);
			TestLut(1);
			TestLut(2);
			TestLut(3);
			TestLut(4);
			TestLut(5);
			TestLut(6);
			TestLut(7);
		}
	}

	Font[d->Used++] = Fnt.Release();
	return true;
}

GFont *GFontSystem::GetGlyph(uint32 u, GFont *UserFont)
{
	if (u > MAX_UNICODE || !UserFont)
	{
		LgiAssert(!"Invalid character");
		return 0;
	}

	// Check app font
	if (!d->SubSupport ||
		(UserFont->GetGlyphMap() &&
		_HasUnicodeGlyph(UserFont->GetGlyphMap(), u)))
	{
		return UserFont;
	}

	// Check LUT
	GFont *Has = 0;
	if (Lut[u])
	{
		Has = Font[Lut[u]];
		LgiAssert(Has != NULL);
		if (!Has)
		{
			LgiTrace("%s:%i - Font table missing pointer. u=%i Lut[u]=%i\n", _FL, u, Lut[u]);
			Has = UserFont;
		}
	}
	else if (d->Used < 255 && !d->FontTableLoaded)
	{
		// Add fonts to Lut...
		if (!SubFonts.Length())
		{
			#if LGI_EXCEPTIONS
			try
			{
			#endif
				FontMap Pref(0, 0);
				if (GFontSystem::Inst()->EnumerateFonts(SubFonts))
				{
					// Reorder font list to prefer certain known as good fonts or
					// avoid certain bad fonts.
					if (LgiGetOs() == LGI_OS_WIN32 ||
						LgiGetOs() == LGI_OS_WIN64)
					{
						Pref.Add("Microsoft Sans Serif", 1);
						Pref.Add("Arial Unicode MS", 1);
						Pref.Add("Verdana", 1);
						Pref.Add("Tahoma", 1);

						Pref.Add("Bookworm", -1);
						Pref.Add("Christmas Tree", -1);
						Pref.Add("MingLiU", -1);
					}
					
					if (LgiGetOs() == LGI_OS_LINUX)
					{
						// Windows fonts are much better than anything Linux 
						// has to offer.
						Pref.Add("Verdana", 1);
						Pref.Add("Tahoma", 1);
						Pref.Add("Arial Unicode MS", 1);
						
						// Most linux fonts suck... and the rest aren't much
						// good either
						Pref.Add("AR PL *", -1);
						Pref.Add("Baekmuk *", -1);
						Pref.Add("console8*", -1);
						Pref.Add("Courier*", -1);
						Pref.Add("Fangsong*", -1);
						Pref.Add("Kochi*", -1);
						Pref.Add("MiscFixed", -1);
						Pref.Add("Serto*", -1);
						Pref.Add("Standard Symbols*", -1);
						Pref.Add("Nimbus*", -1);
					}

					// Prefer these fonts...
					SubFonts.Sort<FontMap>(FontNameCmp, &Pref);

					// Delete fonts prefixed with '@' to the end, as they are for
					// vertical rendering... and aren't suitable for what LGI uses
					// fonts for.
					for (unsigned i=0; i<SubFonts.Length(); i++)
					{
						if (SubFonts[i](0) == '@')
							SubFonts.DeleteAt(i--, true);
					}
				}
			#if LGI_EXCEPTIONS
			}
			catch (...)
			{
				LgiTrace("%s:%i - Font enumeration crashed.\n", _FL);
			}
			#endif
		}

		#if LGI_EXCEPTIONS
		try
		{
		#endif
			// Start loading in fonts from the remaining 'SubFonts' container until we find the char we're looking for, 
			// but only for a max of 10ms. This may result in a few missing glyphs but reduces the max time a display
			// string takes to layout
			auto Start = LgiCurrentTime();
			int Used = d->Used;
			while (SubFonts.Length() > 0 && (LgiCurrentTime() - Start) < 10)
			{
				GString f = SubFonts[0];
				SubFonts.DeleteAt(0, true);

				if (d->Used >= CountOf(Font))
				{
					// No more space
					SubFonts.Empty();
					break;
				}

				GAutoPtr<GFont> Fnt(new GFont);
				if (Fnt)
				{
					*Fnt.Get() = *UserFont;
					Fnt->Face(f);
					if (AddFont(Fnt))
					{
						GFont *Prev = Font[d->Used - 1];
						if (_HasUnicodeGlyph(Prev->GetGlyphMap(), u))
						{
							Has = Prev;
							LgiAssert(Has != NULL);
							break;
						}
					}
				}
			}

			LgiTrace("Loaded %i fonts for glyph sub.\n", d->Used - Used);

		#if LGI_EXCEPTIONS
		}
		catch (...)
		{
			LgiTrace("%s:%i - Glyph search crashed.\n", _FL);
		}
		#endif

		if (!SubFonts.Length())
		{
			d->FontTableLoaded = true;
		}
	}

	return Has;
}


