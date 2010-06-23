#include <string.h>
#include "LgiDefs.h"
#include "LgiOsDefs.h"
#include "PangoFonts.h"
#include "GString.h"

class XFontMetricsPrivate
{
public:
	XFont *f;
	int Ascent;
	int Descent;
	double ScaleX, ScaleY;
	Pango::PangoFontMetrics *m;
};

XFontMetrics::XFontMetrics(XFont *f)
{
	Data = new XFontMetricsPrivate;
	Data->f = f;
	f->GetScale(Data->ScaleX, Data->ScaleY);

	if (Data->f AND Data->f->Handle())
	{
		Data->Ascent = f->GetAscent();
		Data->Descent = f->GetDescent();
	}
	else
	{
		Data->Ascent = 16;
		Data->Descent = 0;
	}

	Data->m = Pango::pango_font_get_metrics((Pango::PangoFont*)f->Handle(), 0);
	if (!Data->m)
		printf("%s:%i - pango_font_get_metrics failed.\n", _FL);
}

XFontMetrics::~XFontMetrics()
{
	if (Data->m)
		Pango::pango_font_metrics_unref(Data->m);
	DeleteObj(Data);
}

int XFontMetrics::ascent()
{
	return Data->Ascent * Data->ScaleY;
}

int XFontMetrics::descent()
{
	return Data->Descent * Data->ScaleY;
}

int XFontMetrics::width(uchar i)
{
	if (Data->f AND
		i > 0)
	{
		if (Data->f->Handle())
		{
			// XGlyphInfo Info;
			// XftTextExtents8(XDisplay(), Data->f->Handle(), (XftChar8*)&i, 1, &Info);
			// return Info.width * Data->ScaleX;
		}
	}

	return 0;
}

int XFontMetrics::width(char *str, int len)
{
	if (Data->f AND
		Data->f->Handle() AND
		str)
	{
		if (len < 0)
		{
			len = strlen(str);
		}

		if (len > 8 << 10)
		{
			printf("Warning: You are tring to measure %i characters, is this right? (XFontMetrics::width)\n", len);
		}

		if (Data->f->Handle())
		{
			// XGlyphInfo Info;
			// XftTextExtentsUtf8(XDisplay(), Data->f->Handle(), (XftChar8*)str, len, &Info);
			// return ((uint)(ushort)Info.xOff) * Data->ScaleX;
		}
	}
	else
	{
		printf("%s:%i - Parameters invalid.\n", __FILE__, __LINE__);
	}

	return 0;
}

int XFontMetrics::width(char16 *str, int len)
{
	if (Data->f AND
		str)
	{
		if (len < 0)
		{
			len = StrlenW(str);
		}

		if (len > 8 << 10)
		{
			printf("Warning: You are tring to measure %i characters, is this right? (XFontMetrics::width)\n", len);
		}

		if (Data->f->Handle())
		{
			/*
			XGlyphInfo Info;
			XftTextExtents32(XDisplay(), Data->f->Handle(), (XftChar32*)str, len, &Info);
			
			#if 0
			printf("XFontMetrics::width len=%i x=%i y=%i w=%i h=%i xo=%i yo=%i\n",
					len,
					Info.x, Info.y,
					Info.width, Info.height,
					Info.xOff, Info.yOff);
			#endif

			return ((ushort)Info.xOff) * Data->ScaleX;
			*/
		}
		else
		{
			printf("%s:%i - No TTF handle, Font=%s,%i.\n", __FILE__, __LINE__,
				Data->f->GetFamily(),
				Data->f->GetPointSize());
		}
	}
	else
	{
		printf("%s:%i - Parameters invalid.\n", __FILE__, __LINE__);
	}

	return 0;
}

int XFontMetrics::height()
{
	return (Data->Ascent + Data->Descent) * Data->ScaleY;
}

uchar *XFontMetrics::GetCoverage(uchar *Map, int Max)
{
	/*
	XftFont *Font = Data->f->Handle();
	if (Font)
	{
		int Bytes = (Max + 1) >> 3;
		if (NOT Map)
		{
			Map = new uchar[Bytes];
		}
		if (Map)
		{
			#ifdef XFT_DEBUG_COVERAGE
			int Start = LgiCurrentTime();
			#endif

			// Initialize the glyph map
			int One = 128 / 8;
			int Zero = Bytes;
			memset(Map, 0xff, One);
			memset(Map+One, 0, Zero-One);

			// Calculate coverage
			for (int i=128; i<Max; i++)
			{
				#if XFT_VERSION >= 20000 // eariler versions of Xft didn't support this function??
				// if (XftCharExists(XDisplay(), Font, i))
				#else
				if (XftGlyphExists(XDisplay(), Font, i)) // not as accurate, but whatever..
				#endif
				{
					Map[i>>3] |= 1 << (i & 7);
				}
			}

			#ifdef XFT_DEBUG_COVERAGE
			// Print converage time..
			printf("XFontMetrics::GetCoverage Font=%s,%i Time=%i ms\n",
				Data->f->GetFamily(),
				Data->f->GetPointSize(),
				LgiCurrentTime()-Start);

			// Show glyph coverage...
			for (int n=0; n<Max/8/16; n+=16)
			{
				printf("\t%02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X %02.2X\n",
					Map[n+0], Map[n+1], Map[n+2], Map[n+3], Map[n+4], Map[n+5], Map[n+6], Map[n+7],
					Map[n+8], Map[n+9], Map[n+10], Map[n+11], Map[n+12], Map[n+13], Map[n+14], Map[n+15]);
			}
			#endif
		}
	}
	*/

	return Map;
}

///////////////////////////////////////////////////////////////////////////////////////
struct PangoSys
{
	Pango::PangoFontMap *Map;
	Pango::PangoContext *Ctx;
	
	PangoSys()
	{
		Map = Pango::pango_cairo_font_map_get_default();
		if (!Map)
			LgiAssert(!"pango_cairo_font_map_get_default failed.\n");
		
		Ctx = Pango::pango_cairo_font_map_create_context((Pango::PangoCairoFontMap*)Map);
		if (!Ctx)
			LgiAssert(!"pango_cairo_font_map_create_context failed.\n");
	}
};

class XFontPrivate
{
public:
	OsFnt Fnt;
	static PangoSys *Sys;
	
	char *Face;
	int Height;
	bool Bold;
	bool Italic;
	bool Underline;

	int Ascent;
	int Descent;
	
	double ScaleX, ScaleY;

	XFontPrivate()
	{
		Fnt = 0;
		Face = 0;
		Height = 0;
		Bold = Italic = Underline = false;
		Ascent = 9;
		Descent = 3;
		ScaleX = 1.0;
		ScaleY = 1.0;
	}

	~XFontPrivate()
	{
		Empty();
		DeleteArray(Face);
	}

	void Empty()
	{
		if (Fnt)
		{
			// XftFontClose(XDisplay(), Fnt);
			Fnt = 0;
		}
	}

	void Update()
	{
		if (Face AND Height > 0)
		{
			// Strip spaces out of font face name
			char FaceNoSpace[128], *Out = FaceNoSpace;
			for (char *In=Face; *In; In++)
			{
				if (*In != ' ') *Out++ = *In;
			}
			*Out++ = 0;

			// height
			char Ht[32];
			sprintf(Ht, "%i", max(Height,8) * 10);

			// Build font description string
			char s[256];
			
			// Try Xft font
			Empty();
			if (!Sys)
				Sys = new PangoSys;
			
			Pango::PangoFontDescription *Desc = Pango::pango_font_description_new();
			if (Desc)
			{
				Pango::pango_font_description_set_family(Desc, Face);
				Pango::pango_font_description_set_size(Desc, Height * PANGO_SCALE);
				if (Bold)
					Pango::pango_font_description_set_weight(Desc, Pango::PANGO_WEIGHT_BOLD);
				
				Fnt = Pango::pango_font_map_load_font(	Sys->Map,
														Sys->Ctx,
														Desc);
				
				Pango::pango_font_description_free(Desc);
			}
			if (!Fnt)
				printf("pango_font_map_load_font failed: Face='%s' Size=%i Bold=%i Italic=%i\n", Face, Height, Bold, Italic);
			else
			{
				Pango::PangoFontMetrics *m = Pango::pango_font_get_metrics(Fnt, 0);
				if (!m)
					printf("pango_font_get_metrics failed.\n");
				else
				{
					Ascent = Pango::pango_font_metrics_get_ascent(m);
					Descent = Pango::pango_font_metrics_get_descent(m);
					/*
					printf("Ascent=%i Descent=%i\n",
							(Ascent+PANGO_SCALE-1)/PANGO_SCALE,
							(Descent+PANGO_SCALE-1)/PANGO_SCALE); */
					Pango::pango_font_metrics_unref(m);
				}
			}
		}
	}
};

PangoSys *XFontPrivate::Sys = 0;

XFont::XFont()
{
	Data = new XFontPrivate;
}

XFont::~XFont()
{
	DeleteObj(Data);
}

int XFont::GetAscent()
{
	// Yes I know, whats the '2' here mean?
	// Well, this makes things look much more like windows, so
	// at the moment, all the fonts have 2 pixels of blank space
	// at the top of the text, which for the most part just looks
	// normal to me.
	return Data->Ascent + 2;
}

int XFont::GetDescent()
{
	return Data->Descent;
}

void *XFont::Handle()
{
	if (!Data->Fnt)
	{
		Data->Update();
	}
	
	return Data->Fnt;
}

char *XFont::GetFamily()
{
	return Data->Face;
}

int XFont::GetPointSize()
{
	return Data->Height;
}

bool XFont::GetBold()
{
	return Data->Bold;
}

bool XFont::GetItalic()
{
	return Data->Italic;
}

bool XFont::GetUnderline()
{
	return Data->Underline;
}

XFont &XFont::operator =(XFont &f)
{
	DeleteArray(Data->Face);
	Data->Face = NewStr(f.Data->Face);
	Data->Height = f.Data->Height;
	Data->Bold = f.Data->Bold;
	Data->Italic = f.Data->Italic;
	Data->Underline = f.Data->Underline;

	Data->Empty();

	return *this;
}

void XFont::GetScale(double &x, double &y)
{
	x = Data->ScaleX;
	y = Data->ScaleY;
}

void XFont::SetPainter(OsPainter p)
{
	if (p)
	{
		// p->GetScale(Data->ScaleX, Data->ScaleY);
	}
}

void XFont::SetFamily(char *face)
{
	char *f = NewStr(face);
	DeleteArray(Data->Face);
	Data->Face = f;
	Data->Empty();
}

void XFont::SetPointSize(int height)
{
	Data->Height = height;
	Data->Empty();
}

void XFont::SetBold(bool bold)
{
	Data->Bold = bold;
	Data->Empty();
}

void XFont::SetItalic(bool italic)
{
	Data->Italic = italic;
	Data->Empty();
}

void XFont::SetUnderline(bool underline)
{
	Data->Underline = underline;
}


