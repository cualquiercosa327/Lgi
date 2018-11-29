#ifndef _GFONT_PRIV_H_
#define _GFONT_PRIV_H_

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

#endif