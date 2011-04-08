#include <stdlib.h>

#include "Lgi.h"
#include "GToken.h"

/////////////////////////////////////////////////////////////////////
// Private growable class for binary compatability
class GFontSystemPrivate
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
};

/////////////////////////////////////////////////////////////////////
static bool FontSystemDone = false;
GFontSystem *GFontSystem::Me = 0;
GFontSystem *GFontSystem::Inst()
{
	if (!Me AND !FontSystemDone)
		new GFontSystem;
	
	return Me;
}

GFontSystem::GFontSystem()
{
	Me = this;
	d = new GFontSystemPrivate;

	// Glyph sub setup
	int Os = LgiGetOs();

	d->SubSupport =	(Os == LGI_OS_LINUX) OR
					(Os == LGI_OS_WIN9X) OR
					(Os == LGI_OS_WINNT); //  AND Rev == 0);  // WinXP does it's own glyph substitution
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

	AllFonts.DeleteArrays();
	SubFonts.DeleteArrays();
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
	if (!d->CheckedConfig AND LgiApp)
	{
		GXmlTag *FontSys = LgiApp->GetConfig("font_system");
		if (FontSys)
		{
			char *GlyphSub;
			if (GlyphSub = FontSys->GetAttr("glyph_sub"))
			{
				d->DefaultGlyphSub = atoi(GlyphSub);
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

#ifdef WIN32
int CALLBACK _EnumFonts(ENUMLOGFONT FAR *lpelf,
						NEWTEXTMETRIC FAR *lpntm,
						int FontType,
						LPARAM lParam)
{
	List<char> *p = (List<char>*) lParam;
	if (p)
	{
		p->Insert(LgiFromNativeCp(lpelf->elfLogFont.lfFaceName));
	}
	return true;
}
#endif

int StringSort(const char *a, const char *b, int Data)
{
	if (a AND b) return stricmp(a, b);
	return 0;
}

bool GFontSystem::EnumerateFonts(List<const char> &Fonts)
{
	if (!AllFonts.First())
	{
		#if defined WIN32

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
		
		#elif defined MAC

		// ATSFontFilter myFontFilter;
		ATSFontFamilyIterator myFamilyIterator;
		OSStatus status = ATSFontFamilyIteratorCreate (	kATSFontContextLocal,
														NULL,
														NULL,
														kATSOptionFlagsUnRestrictedScope,
														&myFamilyIterator);		 
		while (status == noErr)
		{
			ATSFontFamilyRef myFamilyRef;
			status = ATSFontFamilyIteratorNext (myFamilyIterator,
												&myFamilyRef);
	 
			if (status == noErr)
			{
				CFStringRef Name;
				status = ATSFontFamilyGetName(myFamilyRef, kATSOptionFlagsUnRestrictedScope, &Name);
				
				char n[256];
				if (CFStringGetCString(Name, n, sizeof(n), kCFStringEncodingUTF8))
				{
					AllFonts.Insert(NewStr(n));
				}
			}
			else if (status == kATSIterationScopeModified)
			{
				status = ATSFontFamilyIteratorReset (
								kATSFontContextLocal,
								NULL,
								NULL,
								kATSOptionFlagsUnRestrictedScope,
								&myFamilyIterator);
				
				// Add your code here to take any actions needed because of the
				// reset operation.
			}
		}
		status = ATSFontFamilyIteratorRelease(&myFamilyIterator);
		
		#endif

		AllFonts.Sort(StringSort, 0);
	}

	if (AllFonts.First() AND &AllFonts != &Fonts)
	{
		for (const char *s=AllFonts.First(); s; s=AllFonts.Next())
		{
			Fonts.Insert(NewStr(s));
		}
		return true;
	}

	return false;
}

GFont *GFontSystem::GetBestFont(char *Str)
{
	GFont *MatchingFont = 0;

	if (d->SubSupport)
	{
		char16 *s = LgiNewUtf8To16(Str);
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
					if (h->GetGlyphMap() AND
						_HasUnicodeGlyph(h->GetGlyphMap(), *i))
					{
						Chars++;
					}
				}

				if (!MatchingFont OR
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

GFont *GFontSystem::GetGlyph(int u, GFont *UserFont)
{
	if (u > MAX_UNICODE OR !UserFont)
	{
		return 0;
	}

	// Check app font
	if (!d->SubSupport OR
		(UserFont->GetGlyphMap() AND
		_HasUnicodeGlyph(UserFont->GetGlyphMap(), u)))
	{
		return UserFont;
	}

	// Check LUT
	GFont *Has = 0;
	if (Lut[u])
	{
		Has = Font[Lut[u]];
		LgiAssert(Has);
		if (!Has)
		{
			LgiTrace("%s:%i - Font table missing pointer. u=%i Lut[u]=%i\n", __FILE__, __LINE__, u, Lut[u]);
			Has = UserFont;
		}
	}
	else if (d->Used < 255 AND !d->FontTableLoaded)
	{
		// Add fonts to Lut...
		if (!SubFonts.First())
		{
			#if LGI_EXCEPTIONS
			try
			{
			#endif
				if (GFontSystem::Inst()->EnumerateFonts(SubFonts))
				{
					// Reorder font list to prefer certain known as good fonts or
					// avoid certain bad fonts.
					List<const char> Ascend, Descend;
					
					if (LgiGetOs() == LGI_OS_WINNT)
					{
						Ascend.Insert("Microsoft Sans Serif");
						Ascend.Insert("Arial Unicode MS");
						Ascend.Insert("Verdana");
						Ascend.Insert("Tahoma");

						Descend.Insert("Bookworm");
						Descend.Insert("Christmas Tree");
						Descend.Insert("MingLiU");
					}
					else if (LgiGetOs() == LGI_OS_WIN9X)
					{
						Ascend.Insert("MingLiU");
					}
					
					if (LgiGetOs() == LGI_OS_LINUX)
					{
						// Windows fonts are much better than anything Linux 
						// has to offer.
						Ascend.Insert("Verdana");
						Ascend.Insert("Tahoma");
						Ascend.Insert("Arial Unicode MS");
						
						// Most linux fonts suck... and the rest aren't much
						// good either
						Descend.Insert("AR PL *");
						Descend.Insert("Baekmuk *");
						Descend.Insert("console8*");
						Descend.Insert("Courier*");
						Descend.Insert("Fangsong*");
						Descend.Insert("Kochi*");
						Descend.Insert("MiscFixed");
						Descend.Insert("Serto*");
						Descend.Insert("Standard Symbols*");
						Descend.Insert("Nimbus*");
					}

					// Prefer these fonts...
					List<const char> Temp;
					const char *p;
					for (p=Ascend.First(); p; p=Ascend.Next())
					{
						for (const char *f=SubFonts.First(); f; )
						{
							if (MatchStr(p, f))
							{
								SubFonts.Delete(f);
								Temp.Insert(f);
								f = SubFonts.Current();
							}
							else
							{
								f = SubFonts.Next();
							}
						}
					}
					for (p=Temp.First(); p; p=Temp.Next())
					{
						SubFonts.Insert(p, 0);
					}

					// Avoid these fonts...
					Temp.Empty();
					for (p=Descend.First(); p; p=Descend.Next())
					{
						for (const char *f=SubFonts.First(); f; )
						{
							if (MatchStr(p, f))
							{
								SubFonts.Delete(f);
								Temp.Insert(f);
								f = SubFonts.Current();
							}
							else
							{
								f = SubFonts.Next();
							}
						}
					}
					for (p=Temp.First(); p; p=Temp.Next())
					{
						SubFonts.Insert(p);
					}

					// Delete fonts prefixed with '@' to the end, as they are for
					// vertical rendering... and aren't suitable for what LGI uses
					// fonts for.
					for (const char *f=SubFonts.First(); f; )
					{
						if (*f == '@')
						{
							SubFonts.Delete(f);
							DeleteObj((char*&)f);
							f = SubFonts.Current();
						}
						else
						{
							f = SubFonts.Next();
						}
					}
				}
			#if LGI_EXCEPTIONS
			}
			catch (...)
			{
				LgiTrace("%s:%i - Font enumeration crashed.\n", __FILE__, __LINE__);
			}
			#endif
		}

		#if LGI_EXCEPTIONS
		try
		{
		#endif
			const char *s;
			while (	(d->Used < CountOf(Font) - 1) AND
					(s = SubFonts.First()))
			{
				SubFonts.Delete(s);

				GFont *n = new GFont;
				if (n)
				{
					int LutIndex = d->Used;

					*n = *UserFont;
					n->Face(s);
					DeleteArray((char*&)s);
					n->Create();
					Font[d->Used++] = n;

					if (n->GetGlyphMap())
					{
						// Insert all the characters of this font into the LUT
						// so that we can map from a character back to the font
						for (int k=0; k<=MAX_UNICODE; k++)
						{
							if (!Lut[k] AND
								_HasUnicodeGlyph(n->GetGlyphMap(), k))
							{
								Lut[k] = LutIndex;
							}
						}

						if (_HasUnicodeGlyph(n->GetGlyphMap(), u))
						{
							Has = n;
							LgiAssert(Has);
							break;
						}
					}
				}
				else
				{
					DeleteArray((char*&)s);
				}
			}
		#if LGI_EXCEPTIONS
		}
		catch (...)
		{
			LgiTrace("%s:%i - Glyph search crashed.\n", __FILE__, __LINE__);
		}
		#endif

		if (!SubFonts.First())
		{
			d->FontTableLoaded = true;
		}
	}

	return Has;
}


