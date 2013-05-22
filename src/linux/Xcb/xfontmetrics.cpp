#include "Xft.h"
#include "Xrender.h"

#include "LgiDefs.h"
#include "LgiOsDefs.h"
#include "XftFont.h"
#include "GString.h"

// #define XFT_DEBUG_COVERAGE

class XFontMetricsPrivate
{
public:
	XFont *f;
	int Ascent;
	int Descent;
	double ScaleX, ScaleY;
};

XFontMetrics::XFontMetrics(XFont *f)
{
	Data = new XFontMetricsPrivate;
	Data->f = f;
	f->GetScale(Data->ScaleX, Data->ScaleY);

	if (Data->f AND Data->f->GetTtf())
	{
		Data->Ascent = f->GetAscent();
		Data->Descent = f->GetDescent();
	}
	else
	{
		Data->Ascent = 16;
		Data->Descent = 0;
	}

	/* Old X11 font support
	if (Data->f AND Data->f->GetStruct())
	{
		int Dir;
		XCharStruct Char;
		XTextExtents(Data->f->GetStruct(), "A", 1, &Dir, &Data->Ascent, &Data->Descent, &Char);
	}
	*/
}

XFontMetrics::~XFontMetrics()
{
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
		if (Data->f->GetTtf())
		{
			XGlyphInfo Info;
			// XftTextExtents8(XDisplay(), Data->f->GetTtf(), (XftChar8*)&i, 1, &Info);
			return Info.width * Data->ScaleX;
		}
	}

	return 0;
}

int XFontMetrics::width(char *str, int len)
{
	if (Data->f AND
		Data->f->GetTtf() AND
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

		if (Data->f->GetTtf())
		{
			XGlyphInfo Info;
			// XftTextExtentsUtf8(XDisplay(), Data->f->GetTtf(), (XftChar8*)str, len, &Info);
			return ((uint)(ushort)Info.xOff) * Data->ScaleX;
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

		if (Data->f->GetTtf())
		{
			XGlyphInfo Info;
			// XftTextExtents32(XDisplay(), Data->f->GetTtf(), (XftChar32*)str, len, &Info);
			
			#if 0
			printf("XFontMetrics::width len=%i x=%i y=%i w=%i h=%i xo=%i yo=%i\n",
					len,
					Info.x, Info.y,
					Info.width, Info.height,
					Info.xOff, Info.yOff);
			#endif

			return ((ushort)Info.xOff) * Data->ScaleX;
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
	XftFont *Font = Data->f->GetTtf();
	if (Font)
	{
		int Bytes = (Max + 1) >> 3;
		if (!Map)
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
			printf("XFontMetrics::GetCoverage Font=%s,%i Time=%i ms\n", Data->f->GetFamily(), Data->f->GetPointSize(), LgiCurrentTime()-Start);

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

	return Map;
}
