#include "LgiDefs.h"
#include "LgiOsDefs.h"
#include "XftFont.h"
#include "GString.h"

class XFontPrivate
{
public:
	XftFont *ttf;
	
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
		ttf = 0;
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
		if (ttf)
		{
			// XftFontClose(XDisplay(), ttf);
			ttf = 0;
		}
	}

	void Update()
	{
		/*
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
			if (ttf)
			{
				XftFontClose(XDisplay(), ttf);
				ttf = 0;
			}
			
			if (Italic AND Bold)
			{
				ttf = XftFontOpen(	XDisplay(),
									0,
									XFT_FAMILY, XftTypeString, Face,
									XFT_SIZE, XftTypeInteger, Height,
									XFT_SLANT, XftTypeInteger, XFT_SLANT_ITALIC,
									XFT_WEIGHT, XftTypeInteger, XFT_WEIGHT_BOLD,
									NULL);
			}
			else if (Bold)
			{
				ttf = XftFontOpen(	XDisplay(),
									0,
									XFT_FAMILY, XftTypeString, Face,
									XFT_SIZE, XftTypeInteger, Height,
									XFT_WEIGHT, XftTypeInteger, XFT_WEIGHT_BOLD,
									NULL);
			}
			else if (Italic)
			{
				ttf = XftFontOpen(	XDisplay(),
									0,
									XFT_FAMILY, XftTypeString, Face,
									XFT_SIZE, XftTypeInteger, Height,
									XFT_SLANT, XftTypeInteger, XFT_SLANT_ITALIC,
									NULL);
			}
			else
			{
				ttf = XftFontOpen(	XDisplay(),
									0,
									XFT_FAMILY, XftTypeString, Face,
									XFT_SIZE, XftTypeInteger, Height,
									NULL);
			}

			if (ttf)
			{
				XGlyphInfo a;
				XftTextExtents8(XDisplay(), ttf, (XftChar8*)"A", 1, &a);
				Ascent = a.y;
				
				#ifdef DBG_FONT_MSG
				printf("\tA: info x,y=%i,%i width,height=%i,%i xoff,yoff=%i,%i\n",
						a.x, a.y,
						a.width, a.height,
						a.xOff, a.yOff);
				#endif
				
				XftTextExtents8(XDisplay(), ttf, (XftChar8*)"Ap_", 3, &a);
				Descent = a.height - Ascent;

				#ifdef DBG_FONT_MSG
				printf("\tp: info x,y=%i,%i width,height=%i,%i xoff,yoff=%i,%i\n",
						a.x, a.y,
						a.width, a.height,
						a.xOff, a.yOff);

				printf("XftFontOpen OK: Face='%s' Size=%i Bold=%i Italic=%i Ascent=%i Descent=%i Height=%i\n",
						Face, Height, Bold, Italic,
						Ascent, Descent, Height);
				#endif
			}			
			else
			{
				printf("XftFontOpen failed: Face='%s' Size=%i Bold=%i Italic=%i\n", Face, Height, Bold, Italic);
			}
		}
		*/
	}
};

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

XftFont *XFont::GetTtf()
{
	if (NOT Data->ttf)
	{
		Data->Update();
	}
	
	return Data->ttf;
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


