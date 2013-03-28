#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "Lgi.h"
#include "GHtml.h"
#include "GHtmlPriv.h"
#include "GToken.h"
#include "GScrollBar.h"
#include "GVariant.h"
#include "GFindReplaceDlg.h"
#include "GUtf8.h"
#include "Emoji.h"
#include "GClipBoard.h"
#include "GButton.h"
#include "GEdit.h"
#include "GCombo.h"
#include "GdcTools.h"

#define DEBUG_TABLE_LAYOUT			0
#define LUIS_DEBUG					0
#define CRASH_TRACE					0
#ifdef MAC
#define GHTML_USE_DOUBLE_BUFFER		0
#else
#define GHTML_USE_DOUBLE_BUFFER		1
#endif
#define GT_TRANSPARENT				0x00000000
#ifndef IDC_HAND
#define IDC_HAND					MAKEINTRESOURCE(32649)
#endif
#define M_JOBS_LOADED				(M_USER+4000)

#undef CellSpacing
#define DefaultCellSpacing			0
#define DefaultCellPadding			3

#ifdef MAC
#define MinimumPointSize			9
#define MinimumBodyFontSize			12
#else
#define MinimumPointSize			8
#define MinimumBodyFontSize			11
#endif

#define DefaultPointSize			11
#define DefaultBodyMargin			"5px"
#define DefaultImgSize				17
#define DefaultMissingCellColour	GT_TRANSPARENT // Rgb32(0xf0,0xf0,0xf0)
#ifdef _DEBUG
#define DefaultTableBorder			Rgb32(0xf8, 0xf8, 0xf8)
#else
#define DefaultTableBorder			GT_TRANSPARENT
#endif
#define DefaultTextColour			Rgb32(0, 0, 0)
#define ShowNbsp					0

static char WordDelim[]	=			".,<>/?[]{}()*&^%$#@!+|\'\"";
static char16 WhiteW[] =			{' ', '\t', '\r', '\n', 0};

#define SkipWhiteSpace(s)			while (*s && IsWhiteSpace(*s)) s++;

static char DefaultCss[] = {
"a				{ color: blue; text-decoration: underline; }"
"body			{ margin: 8px; }"
"strong			{ font-weight: bolder; }"
"pre				{ font-family: monospace }"
"h1				{ font-size: 2em; margin: .67em 0px; }"
"h2				{ font-size: 1.5em; margin: .75em 0px; }"
"h3				{ font-size: 1.17em; margin: .83em 0px; }"
"h4, p,"
"blockquote, ul,"
"fieldset, form,"
"ol, dl, dir,"
"menu            { margin: 1.12em 0px; }"
"h5              { font-size: .83em; margin: 1.5em 0px; }"
"h6              { font-size: .75em; margin: 1.67em 0px; }"
"strike, del		{ text-decoration: line-through; }"
"hr              { border: 1px inset; }"
"center          { text-align: center; }"
"h1, h2, h3, h4,"
"h5, h6, b,"
"strong          { font-weight: bolder; }"
};

#define IsBlock(d)		((d) == DispBlock)

//////////////////////////////////////////////////////////////////////
using namespace Html1;

namespace Html1
{

class GHtmlPrivate
{
public:
	GHashTbl<const char*, bool> Loading;
	GHtmlStaticInst Inst;
	bool CursorVis;
	GRect CursorPos;
	bool WordSelectMode;
	GdcPt2 Content;
	bool LinkDoubleClick;
	GAutoString OnLoadAnchor;
	bool DecodeEmoji;
	GAutoString EmojiImg;
	bool IsParsing;
	int NextCtrlId;
	uint64 SetScrollTime;
	
	// Find settings
	GAutoWString FindText;
	bool MatchCase;
	
	// This UID is used to match data load events with their source document.
	// Sometimes data will arrive after the document that asked for it has
	// already been unloaded. So by assigned each document an UID we can check
	// the job UID against it and discard old data.
	uint32 DocumentUid;

	GHtmlPrivate()
	{
		IsParsing = false;
		DocumentUid = 0;
		LinkDoubleClick = true;
		WordSelectMode = false;
		NextCtrlId = 2000;
		SetScrollTime = 0;
		CursorVis = false;
		CursorPos.ZOff(-1, -1);

		char EmojiPng[MAX_PATH];
		#ifdef MAC
		LgiGetExeFile(EmojiPng, sizeof(EmojiPng));
		LgiMakePath(EmojiPng, sizeof(EmojiPng), EmojiPng, "Contents/Resources/Emoji.png");
		#else
		LgiGetSystemPath(LSP_APP_INSTALL, EmojiPng, sizeof(EmojiPng));
		LgiMakePath(EmojiPng, sizeof(EmojiPng), EmojiPng, "resources/emoji.png");
		#endif
		if (FileExists(EmojiPng))
		{
			DecodeEmoji = true;
			EmojiImg.Reset(NewStr(EmojiPng));
		}
		else
			DecodeEmoji = false;
	}

	~GHtmlPrivate()
	{
	}
};

};

//////////////////////////////////////////////////////////////////////
static GInfo TagInfo[] =
{
	{TAG_B,				"b",			0,			TI_NONE},
	{TAG_I,				"i",			0,			TI_NONE},
	{TAG_U,				"u",			0,			TI_NONE},
	{TAG_P,				"p",			0,			TI_BLOCK},
	{TAG_BR,			"br",			0,			TI_NEVER_CLOSES},
	{TAG_HR,			"hr",			0,			TI_BLOCK | TI_NEVER_CLOSES},
	{TAG_OL,			"ol",			0,			TI_BLOCK},
	{TAG_UL,			"ul",			0,			TI_BLOCK},
	{TAG_LI,			"li",			"ul",		TI_BLOCK},
	{TAG_FONT,			"font",			0,			TI_NONE},
	{TAG_A,				"a",			0,			TI_NONE},
	{TAG_TABLE,			"table",		0,			TI_BLOCK | TI_NO_TEXT | TI_TABLE},
	{TAG_TR,			"tr",			"table",	TI_BLOCK | TI_NO_TEXT | TI_TABLE},
	{TAG_TD,			"td",			"tr",		TI_BLOCK | TI_TABLE},
	{TAG_HEAD,			"head",			"html",		TI_NONE},
	{TAG_BODY,			"body",			0,			TI_BLOCK | TI_NO_TEXT},
	{TAG_IMG,			"img",			0,			TI_NEVER_CLOSES},
	{TAG_HTML,			"html",			0,			TI_BLOCK | TI_NO_TEXT},
	{TAG_DIV,			"div",			0,			TI_BLOCK},
	{TAG_SPAN,			"span",			0,			TI_NONE},
	{TAG_CENTER,		"center",		0,			TI_NONE},
	{TAG_META,			"meta",			0,			TI_NONE},
	{TAG_TBODY,			"tbody",		0,			TI_NONE},
	{TAG_STYLE,			"style",		0,			TI_NONE},
	{TAG_SCRIPT,		"script",		0,			TI_NONE},
	{TAG_STRONG,		"strong",		0,			TI_NONE},
	{TAG_BLOCKQUOTE,	"blockquote",	0,			TI_BLOCK},
	{TAG_PRE,			"pre",			0,			TI_BLOCK},
	{TAG_H1,			"h1",			0,			TI_BLOCK},
	{TAG_H2,			"h2",			0,			TI_BLOCK},
	{TAG_H3,			"h3",			0,			TI_BLOCK},
	{TAG_H4,			"h4",			0,			TI_BLOCK},
	{TAG_H5,			"h5",			0,			TI_BLOCK},
	{TAG_H6,			"h6",			0,			TI_BLOCK},
	{TAG_IFRAME,		"iframe",		0,			TI_BLOCK},
	{TAG_LINK,			"link",			0,			TI_NONE},
	{TAG_BIG,			"big",			0,			TI_NONE},
	{TAG_SELECT,		"select",		0,			TI_NONE},
	{TAG_INPUT,			"input",		0,			TI_NEVER_CLOSES},
	{TAG_LABEL,			"label",		0,			TI_NONE},
	{TAG_FORM,			"form",			0,			TI_NONE},
	{TAG_UNKNOWN,		0,				0,			TI_NONE},
};

static GHashTbl<const char*, GInfo*> TagMap(TAG_LAST * 3, false, NULL, NULL);
static GInfo *UnknownTag = NULL;

static GInfo *GetTagInfo(const char *Tag)
{
	GInfo *i;

	if (!Tag)
		return 0;

    if (TagMap.Length() == 0)
    {
	    for (i = TagInfo; i->Tag; i++)
	    {
	        TagMap.Add(i->Tag, i);

	        if (i->Id == TAG_TD)
	            TagMap.Add("th", i);
	    }
	    
	    UnknownTag = i;
	    LgiAssert(UnknownTag->Id == TAG_UNKNOWN);
	}

	i = TagMap.Find(Tag);
	return i ? i : UnknownTag;
}

static bool Is8Bit(char *s)
{
	while (*s)
	{
		if (((uchar)*s) & 0x80)
			return true;
		s++;
	}
	return false;
}

static char *ParseName(char *s, char **Name)
{
	SkipWhiteSpace(s);
	char *Start = s;
	while (*s && (IsAlpha(*s) || strchr("!-:", *s) || IsDigit(*s)))
	{
		s++;
	}
	if (Name)
	{
		int Len = s - Start;
		if (Len > 0)
		{
			*Name = NewStr(Start, Len);
		}
	}
	
	return s;
}

static char *ParsePropValue(char *s, char *&Value)
{
	Value = 0;
	if (s)
	{
		if (strchr("\"\'", *s))
		{
			char Delim = *s++;
			char *Start = s;
			while (*s && *s != Delim) s++;
			Value = NewStr(Start, s - Start);
			s++;
		}
		else
		{
			char *Start = s;
			while (*s && !IsWhiteSpace(*s) && *s != '>') s++;
			Value = NewStr(Start, s - Start);
		}
	}

	return s;
}

static bool ParseColour(const char *s, GCss::ColorDef &c)
{
	if (s)
	{
		int m;

		if (*s == '#')
		{
			s++;

			ParseHexColour:
			int i = htoi(s);
			int l = strlen(s);
			if (l == 3)
			{
				int r = i >> 8;
				int g = (i >> 4) & 0xf;
				int b = i & 0xf;

				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgb32(r | (r<<4), g | (g << 4), b | (b << 4));
			}
			else if (l == 4)
			{
				int r = (i >> 12) & 0xf;
				int g = (i >> 8) & 0xf;
				int b = (i >> 4) & 0xf;
				int a = i & 0xf;
				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgba32(	r | (r <<4 ),
									g | (g << 4),
									b | (b << 4),
									a | (a << 4));
			}
			else if (l == 6)
			{
				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgb32(i >> 16, (i >> 8) & 0xff, i & 0xff);
			}
			else if (l == 8)
			{
				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgba32(i >> 24, (i >> 16) & 0xff, (i >> 8) & 0xff, i & 0xff);
			}
			else
			{
				return false;
			}
			
			return true;
		}
		else if ((m = GHtmlStatic::Inst->ColourMap.Find(s)) >= 0)
		{
			c.Type = GCss::ColorRgb;
			c.Rgb32 = Rgb24To32(m);
			return true;
		}
		else if (!strnicmp(s, "rgb", 3))
		{
			s += 3;
			SkipWhiteSpace(s);
			if (*s == '(')
			{
				s++;
				GArray<uint8> Col;
				while (Col.Length() < 3)
				{
					SkipWhiteSpace(s);
					if (IsDigit(*s))
					{
						Col.Add(atoi(s));
						while (*s && IsDigit(*s)) s++;
						SkipWhiteSpace(s);
						if (*s == ',') s++;
					}
					else break;
				}

				SkipWhiteSpace(s);
				if (*s == ')' && Col.Length() == 3)
				{
					c.Type = GCss::ColorRgb;
					c.Rgb32 = Rgb32(Col[0], Col[1], Col[2]);
					return true;
				}
			}
		}
		else if (IsDigit(*s) || (tolower(*s) >= 'a' && tolower(*s) <= 'f'))
		{
			goto ParseHexColour;
		}
	}

	return false;
}

static char *ParsePropList(char *s, GTag *Obj, bool &Closed)
{
	while (s && *s && *s != '>')
	{
		while (*s && IsWhiteSpace(*s)) s++;
		if (*s == '>') break;

		// get name
		char *Name = 0;
		char *n = ParseName(s, &Name);		
		if (*n == '/')
		{
			Closed = true;
		}		
		if (n == s)
		{
			s = ++n;
		}
		else
		{
			s = n;
		}

		while (*s && IsWhiteSpace(*s)) s++;

		if (*s == '=')
		{
			// get value
			s++;
			while (*s && IsWhiteSpace(*s)) s++;

			char *Value = 0;
			s = ParsePropValue(s, Value);

			if (Value && Name)
			{
				Obj->Set(Name,  Value);
			}

			DeleteArray(Value);
		}

		DeleteArray(Name);
	}

	if (*s == '>') s++;

	return s;
}

//////////////////////////////////////////////////////////////////////
namespace Html1 {

class InputButton : public GButton
{
	GTag *Tag;
	
public:
	InputButton(GTag *tag, int Id, const char *Label) : GButton(Id, 0, 0, -1, -1, Label)
	{
		Tag = tag;
	}
	
	void OnClick()
	{
		Tag->OnClick();
	}
};

class GFontCache
{
	GHtml *Owner;
	List<GFont> Fonts;

public:
	GFontCache(GHtml *owner)
	{
		Owner = owner;
	}

	~GFontCache()
	{
		Fonts.DeleteObjects();
	}

	GFont *FontAt(int i)
	{
		return Fonts.ItemAt(i);
	}
	
	GFont *FindMatch(GFont *m)
	{
		for (GFont *f = Fonts.First(); f; f = Fonts.Next())
		{
			if (*f == *m)
			{
				return f;
			}
		}
		
		return 0;
	}

	GFont *GetFont(GCss *Style)
	{
		if (!Style)
			return false;
		
		GFont *Default = Owner->GetFont();
		GCss::StringsDef Face = Style->FontFamily();
		if (Face.Length() < 1 || !ValidStr(Face[0]))
		{
			Face.Empty();
			Face.Add(NewStr(Default->Face()));
		}
		GCss::Len Size = Style->FontSize();
		GCss::FontWeightType Weight = Style->FontWeight();
		bool IsBold =	Weight == GCss::FontWeightBold ||
						Weight == GCss::FontWeightBolder ||
						Weight > GCss::FontWeight400;
		bool IsItalic = Style->FontStyle() == GCss::FontStyleItalic;
		bool IsUnderline = Style->TextDecoration() == GCss::TextDecorUnderline;
		double PtSize = 0.0;

		if (Size.Type == GCss::LenInherit)
		{
			Size.Type = GCss::LenPt;
			Size.Value = (float)Default->PointSize();
		}

		GFont *f = 0;
		if (Size.Type == GCss::LenPx)
		{
		    int RequestPx = (int)Size.Value;
			GArray<int> Map; // map of point-sizes to heights
			int NearestPoint = 0;
			int Diff = 1000;
			#define PxHeight(fnt) (fnt->GetHeight() - (int)(fnt->Leading() + 0.5))

			// Look for cached fonts of the right size...
			for (f=Fonts.First(); f; f=Fonts.Next())
			{
				if (f->Face() &&
					stricmp(f->Face(), Face[0]) == 0 &&
					f->Bold() == IsBold &&
					f->Italic() == IsItalic &&
					f->Underline() == IsUnderline)
				{
				    int PtSize = f->PointSize();
				    int Height = PxHeight(f);
					Map[PtSize] = Height;

					if (!NearestPoint)
						NearestPoint = f->PointSize();
					else
					{
						int NearDiff = abs(Map[NearestPoint] - RequestPx);
						int CurDiff = abs(f->GetHeight() - RequestPx);
						if (CurDiff < NearDiff)
						{
							NearestPoint = f->PointSize();
						}
					}
					
					if (RequestPx < PxHeight(f) &&
					    f->PointSize() == MinimumPointSize)
				    {
				        return f;
				    }

					Diff = PxHeight(f) - RequestPx;
					if (abs(Diff) < 2)
					{
						return f;
					}
				}
			}

			// Find the correct font size...
			PtSize = Size.Value;
			if (PtSize < MinimumPointSize)
				PtSize = MinimumPointSize;
			do
			{
				GAutoPtr<GFont> Tmp(new GFont);
				
				Tmp->Bold(IsBold);
				Tmp->Italic(IsItalic);
				Tmp->Underline(IsUnderline);
				
				if (!Tmp->Create(Face[0], (int)PtSize))
					break;
				
				int ActualHeight = PxHeight(Tmp);
				Diff = ActualHeight - RequestPx;
				if (abs(Diff) <= 1)
				{
					Fonts.Insert(f = Tmp.Release());
					LgiAssert(f->Face());
					return f;
				}

				if (Diff > 0)
				{
					if (PtSize > MinimumPointSize)
						PtSize--;
					else
					    break;
				}
				else
					PtSize++;
			}
			while (PtSize > MinimumPointSize && PtSize < 100);
		}
		else if (Size.Type == GCss::LenPt)
		{
			double Pt = max(MinimumPointSize, Size.Value);
			for (f=Fonts.First(); f; f=Fonts.Next())
			{
				if (f->Face() &&
					stricmp(f->Face(), Face[0]) == 0 &&
					f->PointSize() == Pt &&
					f->Bold() == IsBold &&
					f->Italic() == IsItalic &&
					f->Underline() == IsUnderline)
				{
					// Return cached font
					return f;
				}
			}

			PtSize = Pt;
		}
		else if (Size.Type == GCss::LenPercent)
		{
			// Most of the percentages will be resolved in the "Apply" stage
			// of the CSS calculations, any that appear here have no "font-size"
			// in their parent tree, so we just use the default font size times
			// the requested percent
			PtSize = Size.Value * Default->PointSize() / 100.0;
			if (PtSize < MinimumPointSize)
				PtSize = MinimumPointSize;
		}
		else if (Size.Type == GCss::LenEm)
		{
			// Most of the relative sizes will be resolved in the "Apply" stage
			// of the CSS calculations, any that appear here have no "font-size"
			// in their parent tree, so we just use the default font size times
			// the requested percent
			PtSize = Size.Value * Default->PointSize();
			if (PtSize < MinimumPointSize)
				PtSize = MinimumPointSize;
		}
		else if (Size.Type == GCss::LenNormal)
		{
			return Fonts.First();
		}
		else if (Size.Type == GCss::SizeXXSmall ||
				Size.Type == GCss::SizeXSmall ||
				Size.Type == GCss::SizeSmall ||
				Size.Type == GCss::SizeMedium ||
				Size.Type == GCss::SizeLarge ||
				Size.Type == GCss::SizeXLarge ||
				Size.Type == GCss::SizeXXLarge)
		{
			double Table[] =
			{
				0.4, // SizeXXSmall
				0.5, // SizeXSmall
				0.7, // SizeSmall
				1.0, // SizeMedium
				1.3, // SizeLarge
				1.7, // SizeXLarge
				2.0, // SizeXXLarge
			};

			int Idx = Size.Type-GCss::SizeXXSmall;
			LgiAssert(Idx >= 0 && Idx < CountOf(Table));
			PtSize = Default->PointSize() * Table[Idx];
			if (PtSize < MinimumPointSize)
				PtSize = MinimumPointSize;
		}
		else if (Size.Type == GCss::SizeSmaller)
		{
			PtSize = Default->PointSize() - 1;
		}
		else if (Size.Type == GCss::SizeLarger)
		{
			PtSize = Default->PointSize() + 1;
		}
		else LgiAssert(!"Not impl.");

		if (f = new GFont)
		{
			char *ff = ValidStr(Face[0]) ? Face[0] : Default->Face();
			f->Face(ff);
			f->PointSize((int) (PtSize ? PtSize : Default->PointSize()));
			f->Bold(IsBold);
			f->Italic(IsItalic);
			f->Underline(IsUnderline);
			
			// printf("Add cache font %s,%i %i,%i,%i\n", f->Face(), f->PointSize(), f->Bold(), f->Italic(), f->Underline());
			if (!f->Create((char*)0, 0))
			{
				// Broken font...
				f->Face(Default->Face());
				GFont *DefMatch = FindMatch(f);
				// printf("Falling back to default face for '%s:%i', DefMatch=%p\n", ff, f->PointSize(), DefMatch);
				if (DefMatch)
				{
					DeleteObj(f);
					return DefMatch;
				}
				else
				{
					if (!f->Create((char*)0, 0))
					{
						DeleteObj(f);
						return Fonts.First();
					}
				}
			}

			// Not already cached
			Fonts.Insert(f);
			LgiAssert(f->Face());
			return f;
		}

		return 0;
	}
};

class GFlowRegion
{
	List<GFlowRect> Line;	// These pointers aren't owned by the flow region
							// When the line is finish, all the tag regions
							// will need to be vertically aligned

	struct GFlowStack
	{
		int LeftAbs;
		int RightAbs;
		int TopAbs;
	};
	GArray<GFlowStack> Stack;

public:
	GHtml *Html;
	int x1, x2;					// Left and right margins
	int y1;						// Current y position
	int y2;						// Maximum used y position
	int cx;						// Current insertion point
	int my;						// How much of the area above y2 was just margin
	int max_cx;					// Max value of cx

	GFlowRegion(GHtml *html)
	{
		Html = html;
		x1 = x2 = y1 = y2 = cx = my = max_cx = 0;
	}

	GFlowRegion(GHtml *html, GRect r)
	{
		Html = html;
		max_cx = cx = x1 = r.x1;
		y1 = y2 = r.y1;
		x2 = r.x2;
		my = 0;
	}

	GFlowRegion(GFlowRegion &r)
	{
		Html = r.Html;
		x1 = r.x1;
		x2 = r.x2;
		y1 = r.y1;
		y2 = r.y2;
		max_cx = cx = r.cx;
		my = r.my;
	}

	int X()
	{
		return x2 - cx;
	}

	void X(int newx)
	{
		x2 = x1 + newx - 1;
	}

	GFlowRegion &operator +=(GRect r)
	{
		x1 += r.x1;
		cx += r.x1;
		x2 -= r.x2;
		y1 += r.y1;
		y2 += r.y1;
		
		return *this;
	}

	GFlowRegion &operator -=(GRect r)
	{
		x1 -= r.x1;
		cx -= r.x1;
		x2 += r.x2;
		y1 += r.y2;
		y2 += r.y2;
	
		return *this;
	}
	
	void FinishLine(bool Margin = false);
	void EndBlock();
	void Insert(GFlowRect *Tr);
	GRect *LineBounds();

	void Indent(GFont *Font,
				GCss::Len Left,
				GCss::Len Top,
				GCss::Len Right,
				GCss::Len Bottom,
				bool IsMargin)
	{
		GFlowRegion This(*this);
		GFlowStack &Fs = Stack.New();

		Fs.LeftAbs = Left.IsValid() ? ResolveX(Left, Font, IsMargin) : 0;
		Fs.RightAbs = Right.IsValid() ? ResolveX(Right, Font, IsMargin) : 0;
		Fs.TopAbs = Top.IsValid() ? ResolveY(Top, Font, IsMargin) : 0;

		x1 += Fs.LeftAbs;
		cx += Fs.LeftAbs;
		x2 -= Fs.RightAbs;
		y1 += Fs.TopAbs;
		y2 += Fs.TopAbs;
		if (IsMargin)
			my += Fs.TopAbs;
	}

	void Outdent(GFont *Font,
				GCss::Len Left,
				GCss::Len Top,
				GCss::Len Right,
				GCss::Len Bottom,
				bool IsMargin)
	{
		GFlowRegion This = *this;

		#if 0
		int LeftAbs = Left.GetPrevAbs();
		int RightAbs = Right.GetPrevAbs();
		int BottomAbs = Bottom.Get(&This, Font);

		x1 -= LeftAbs;
		cx -= LeftAbs;
		x2 += RightAbs;
		// y1 += BottomAbs;
		y2 += BottomAbs;
		if (Margin)
			my += BottomAbs;
		#else
		int len = Stack.Length();
		if (len > 0)
		{
			GFlowStack &Fs = Stack[len-1];

			int BottomAbs = Bottom.IsValid() ? ResolveY(Bottom, Font, IsMargin) : 0;

			x1 -= Fs.LeftAbs;
			cx -= Fs.LeftAbs;
			x2 += Fs.RightAbs;
			// y1 += Fs.BottomAbs;
			y2 += BottomAbs;
			if (IsMargin)
				my += BottomAbs;

			Stack.Length(len-1);
		}
		else LgiAssert(!"Nothing to pop.");
		#endif
	}

	int ResolveX(GCss::Len l, GFont *f, bool IsMargin)
	{
		int ScreenDpi = 96; // Haha, where should I get this from?

		switch (l.Type)
		{
			default:
			case GCss::LenInherit:
				return X();
			case GCss::LenPx:
				return min((int)l.Value, X());
			case GCss::LenPt:
				return (int) (l.Value * ScreenDpi / 72.0);
			case GCss::LenCm:
				return (int) (l.Value * ScreenDpi / 2.54);
			case GCss::LenEm:
			{
				if (!f)
				{
					LgiAssert(!"No font?");
					f = SysFont;
				}
				return (int)(l.Value * f->GetHeight());
			}
			case GCss::LenEx:
			{
				if (!f)
				{
					LgiAssert(!"No font?");
					f = SysFont;
				}
				return (int) (l.Value * f->GetHeight() / 2); // More haha, who uses 'ex' anyway?
			}
			case GCss::LenPercent:
			{
				int my_x = X();
				int px = (int) (l.Value * my_x / 100.0);
				return px;
			}
			case GCss::LenAuto:
			{
				if (IsMargin)
					return 0;
				else
					return X();
				break;
			}
		}

		return 0;
	}

	int ResolveY(GCss::Len l, GFont *f, bool IsMargin)
	{
		int ScreenDpi = 96; // Haha, where should I get this from?

		switch (l.Type)
		{
			case GCss::LenInherit:
			case GCss::LenAuto:
			case GCss::LenNormal:
			case GCss::LenPx:
				return (int)l.Value;

			case GCss::LenPt:
			{
				return (int) (l.Value * ScreenDpi / 72.0);
			}
			case GCss::LenCm:
			{
				return (int) (l.Value * ScreenDpi / 2.54);
			}
			case GCss::LenEm:
			{
				if (!f)
				{
					f = SysFont;
					LgiAssert(!"No font");
				}
				return (int) (l.Value * f->GetHeight());
			}
			case GCss::LenEx:
			{
				if (!f)
				{
					f = SysFont;
					LgiAssert(!"No font");
				}
				return (int) (l.Value * f->GetHeight() / 2); // More haha, who uses 'ex' anyway?
			}
			case GCss::LenPercent:
				return (int)l.Value;
			default:
				LgiAssert(!"Not supported.");
		}

		return 0;
	}
};

};

//////////////////////////////////////////////////////////////////////
static bool ParseDistance(char *s, float &d, char *units = 0)
{
	if (!s)
		return false;

	while (*s && IsWhiteSpace(*s)) s++;

	if (!IsDigit(*s) && !strchr("-.", *s))
		return false;

	d = atof(s);

	while (*s && (IsDigit(*s) || strchr("-.", *s))) s++;
	while (*s && IsWhiteSpace(*s)) s++;
	
	char _units[128];
	char *o = units = units ? units : _units;
	while (*s && (IsAlpha(*s) || *s == '%'))
	{
		*o++ = *s++;
	}
	*o++ = 0;

	return true;
}

GLength::GLength()
{
	d = 0;
	PrevAbs = 0;
	u = GCss::LenInherit;
}

GLength::GLength(char *s)
{
	Set(s);
}

bool GLength::IsValid()
{
	return u != GCss::LenInherit;
}

bool GLength::IsDynamic()
{
	return u == GCss::LenPercent || d == 0.0;
}

GLength::operator float ()
{
	return d;
}

GLength &GLength::operator =(float val)
{
	d = val;
	u = GCss::LenPx;
	return *this;
}

GCss::LengthType GLength::GetUnits()
{
	return u;
}

void GLength::Set(char *s)
{
	if (ValidStr(s))
	{
		char Units[256] = "";
		if (ParseDistance(s, d, Units))
		{
			if (Units[0])
			{
				if (strchr(Units, '%'))
				{
					u = GCss::LenPercent;
				}
				else if (stristr(Units, "pt"))
				{
					u = GCss::LenPt;
				}
				else if (stristr(Units, "em"))
				{
					u = GCss::LenEm;
				}
				else if (stristr(Units, "ex"))
				{
					u = GCss::LenEx;
				}
				else
				{
					u = GCss::LenPx;
				}
			}
			else
			{
				u = GCss::LenPx;
			}
		}
	}
}

float GLength::Get(GFlowRegion *Flow, GFont *Font, bool Lock)
{
	switch (u)
	{
		case GCss::LenEm:
		{
			return PrevAbs = d * (Font ? Font->GetHeight() : 14);
			break;
		}
		case GCss::LenEx:
		{
			return PrevAbs = (Font ? Font->GetHeight() * d : 14) / 2;
			break;
		}
		case GCss::LenPercent:
		{
			if (Lock || PrevAbs == 0.0)
			{
				return PrevAbs = (Flow->X() * d / 100);
			}
			else
			{
				return PrevAbs;
			}
			break;
		}
	}

	double FlowX = Flow ? Flow->X() : d;
	return PrevAbs = min(FlowX, d);
}

GLine::GLine()
{
	LineStyle = -1;
	LineReset = 0x80000000;
}

GLine::~GLine()
{
}

GLine &GLine::operator =(int i)
{
	d = (float)i;
	return *this;
}

void GLine::Set(char *s)
{
	GToken t(s, " \t");
	LineReset = 0x80000000;
	LineStyle = -1;
	char *Style = 0;

	for (int i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		if
		(
			*c == '#'
			||
			GHtmlStatic::Inst->ColourMap.Find(c)
		)
		{
			ParseColour(c, Colour);
		}
		else if (strnicmp(c, "rgb(", 4) == 0)
		{
			char Buf[256];
			strcpy(Buf, c);
			while (!strchr(c, ')') &&
					(c = t[++i]))
			{
				strcat(Buf, c);
			}
			ParseColour(Buf, Colour);
		}
		else if (IsDigit(*c))
		{
			GLength::Set(c);
		}
		else if (stricmp(c, "none") == 0)
		{
			Style = 0;
		}
		else if (	stricmp(c, "dotted") == 0 ||
					stricmp(c, "dashed") == 0 ||
					stricmp(c, "solid") == 0 ||
					stricmp(c, "float") == 0 ||
					stricmp(c, "groove") == 0 ||
					stricmp(c, "ridge") == 0 ||
					stricmp(c, "inset") == 0 ||
					stricmp(c, "outse") == 0)
		{
			Style = c;
		}
		else
		{
			// ???
		}
	}

	if (Style && stricmp(Style, "dotted") == 0)
	{
		switch ((int)d)
		{
			case 2:
			{
				LineStyle = 0xcccccccc;
				break;
			}
			case 3:
			{
				LineStyle = 0xe38e38;
				LineReset = 0x800000;
				break;
			}
			case 4:
			{
				LineStyle = 0xf0f0f0f0;
				break;
			}
			case 5:
			{
				LineStyle = 0xf83e0;
				LineReset = 0x80000;
				break;
			}
			case 6:
			{
				LineStyle = 0xfc0fc0;
				LineReset = 0x800000;
				break;
			}
			case 7:
			{
				LineStyle = 0xfe03f80;
				LineReset = 0x8000000;
				break;
			}
			case 8:
			{
				LineStyle = 0xff00ff00;
				break;
			}
			case 9:
			{
				LineStyle = 0x3fe00;
				LineReset = 0x20000;
				break;
			}
			default:
			{
				LineStyle = 0xaaaaaaaa;
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////
GRect GTag::GetRect(bool Client)
{
	GRect r(Pos.x, Pos.y, Pos.x + Size.x - 1, Pos.y + Size.y - 1);
	if (!Client)
	{
		for (GTag *p = Parent; p; p=p->Parent)
		{
			r.Offset(p->Pos.x, p->Pos.y);
		}
	}
	return r;
}

GCss::LengthType GTag::GetAlign(bool x)
{
	for (GTag *t = this; t; t = t->Parent)
	{
		GCss::Len l;
		
		if (x)
		{
			if (TagId == TAG_TD && XAlign)
				l.Type = XAlign;
			else
				l = TextAlign();
		}
		else
		{
			l = VerticalAlign();
		}
		
		if (l.Type != LenInherit)
		{
			return l.Type;
		}

		if (t->TagId == TAG_TABLE)
			break;
	}
	
	return LenInherit;
}

//////////////////////////////////////////////////////////////////////
bool GTag::HasChild(GTag *c)
{
	List<GTag>::I it = Tags.Start();
	for (GTag *t = *it; t; t = *++it)
	{
		if (t == c || t->HasChild(c))
			return true;
	}
	return false;
}

bool GTag::Attach(GTag *Child, int Idx)
{
	if (TagId == CONTENT)
	{
		LgiAssert(!"Can't nest content tags.");
		return false;
	}

	if (!Child)
	{
		LgiAssert(!"Can't insert NULL tag.");
		return false;
	}

	Child->Detach();
	Child->Parent = this;
	if (!Tags.HasItem(Child))
	{
		Tags.Insert(Child, Idx);
	}

	return true;
}

void GTag::Detach()
{
	if (Parent)
	{
		Parent->Tags.Delete(this);
		Parent = 0;
	}
}

//////////////////////////////////////////////////////////////////////
void GFlowRegion::EndBlock()
{
	if (cx > x1)
	{
		FinishLine();
	}
}

void GFlowRegion::FinishLine(bool Margin)
{
	/*
	GRect *b = LineBounds();
	if (b)
	{
		for (GFlowRect *Tr=Line.First(); Tr; Tr=Line.Next())
		{
			GRect n = *Tr;
			// int Base = b->y1 - n.y1;
			int Oy = Tr->Tag->AbsY();
			n.Offset(0, Oy);
			Tr->Offset(0, b->y2 - n.y2); // - Base
			// y2 = max(y2, Tr->y2);
		}
	}
	*/

	if (y2 > y1)
	{
		my = Margin ? y2 - y1 : 0;
		y1 = y2;
	}
	else
	{
		int fy = Html->DefFont()->GetHeight();
		my = Margin ? fy : 0;
		y1 += fy;
	}
	cx = x1;
	y2 = y1;

	Line.Empty();
}

GRect *GFlowRegion::LineBounds()
{
	GFlowRect *Prev = Line.First();
	GFlowRect *r=Prev;
	if (r)
	{
		GRect b;
		
		b = *r;
		int Ox = r->Tag->AbsX();
		int Oy = r->Tag->AbsY();
		b.Offset(Ox, Oy);

		// int Ox = 0, Oy = 0;
		while (r = Line.Next())
		{
			GRect c = *r;
			Ox = r->Tag->AbsX();
			Oy = r->Tag->AbsY();
			c.Offset(Ox, Oy);

			/*
			Ox += r->Tag->Pos.x - Prev->Tag->Pos.x;
			Oy += r->Tag->Pos.y - Prev->Tag->Pos.y;
			c.Offset(Ox, Oy);
			*/

			b.Union(&c);
			Prev = r;
		}

		static GRect Rgn;
		Rgn = b;
		return &Rgn;
	}

	return 0;
}

void GFlowRegion::Insert(GFlowRect *Tr)
{
	if (Tr)
	{
		Line.Insert(Tr);
		// y2 = max(y2, Tr->y2);
	}
}

//////////////////////////////////////////////////////////////////////
bool GTag::Selected = false;

GTag::GTag(GHtml *h, GTag *p) : Attr(0, false)
{
	Ctrl = 0;
	CtrlType = CtrlNone;
	TipId = 0;
	Disp = DispInline;
	Html = h;
	Parent = p;
	if (Parent)
	{
		Parent->Tags.Insert(this);
	}
	
	ImageResized = false;
	XAlign = GCss::LenInherit;
	Cursor = -1;
	Selection = -1;
	Font = 0;
	Tag = 0;
	HtmlId = NULL;
	// TableBorder = 0;
	Cells = 0;
	WasClosed = false;
	TagId = CONTENT;
	Info = 0;
	MinContent = 0;
	MaxContent = 0;
	Pos.x = Pos.y = 0;

	#ifdef _DEBUG
	Debug = false;
	#endif
}

GTag::~GTag()
{
	if (Html->Cursor == this)
	{
		Html->Cursor = 0;
	}
	if (Html->Selection == this)
	{
		Html->Selection = 0;
	}
	if (Html->PrevTip == this)
	{
		Html->PrevTip = 0;
	}

	DeleteObj(Ctrl);
	Tags.DeleteObjects();
	Attr.DeleteArrays();

	DeleteArray(Tag);
	DeleteObj(Cells);
}

void GTag::OnChange(PropType Prop)
{
}

bool GTag::OnClick()
{
	if (!Html->Environment)
		return false;

	const char *OnClick = NULL;
	if (Get("onclick", OnClick))
	{
		Html->Environment->OnExecuteScript((char*)OnClick);
	}
	else
	{
		OnNotify(0);
	}

	return true;
}

void GTag::Set(const char *attr, const char *val)
{
	char *existing = Attr.Find(attr);
	if (existing) DeleteArray(existing);

	if (val)
		Attr.Add(attr, NewStr(val));
}

bool GTag::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	return false;
}

bool GTag::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	if (!stricmp(Name, "innerHTML"))
	{
		// Clear out existing tags..
		Tags.DeleteObjects();
	
		char *Doc = Value.CastString();
		if (Doc)
		{
			// Create new tags...
			bool BackOut = false;
			
			while (Doc && *Doc)
			{
				GTag *t = new GTag(Html, this);
				if (t)
				{
					Doc = t->ParseHtml(Doc, 1, false, &BackOut);
					if (!Doc)
						break;
				}
				else break;
			}
		}
	}
	else
	{
		Set(Name, Value.CastString());
		SetStyle();
	}

	Html->ViewWidth = -1;
	return true;
}

int GTag::GetTextStart()
{
	if (PreText())
	{
		GFlowRect *t = TextPos[1];
		if (t)
			return t->Text - Text();
	}
	else
	{
		GFlowRect *t = TextPos[0];
		if (t)
		{
			LgiAssert(t->Text >= Text() && t->Text <= Text()+2);
			return t->Text - Text();
		}
	}

	return 0;
}

static bool TextToStream(GStream &Out, char16 *Text)
{
	if (!Text)
		return true;

	uint8 Buf[256];
	uint8 *s = Buf;
	int Len = sizeof(Buf);
	while (*Text)
	{
		#define WriteExistingContent() \
			if (s > Buf) \
				Out.Write(Buf, s - Buf); \
			s = Buf; \
			Len = sizeof(Buf); \
			Buf[0] = 0;

		if (*Text == '<' || *Text == '>')
		{
			WriteExistingContent();
			Out.Print("&%ct;", *Text == '<' ? 'l' : 'g');
		}
		else if (*Text == 0xa0)
		{
			WriteExistingContent();
			Out.Write((char*)"&nbsp;", 6);
		}
		else
		{
			LgiUtf32To8(*Text, s, Len);
			if (Len < 16)
			{
				WriteExistingContent();
			}
		}

		Text++;
	}

	if (s > Buf)
		Out.Write(Buf, s - Buf);

	return true;
}

bool GTag::CreateSource(GStringPipe &p, int Depth, bool LastWasBlock)
{
	// char *t8 = LgiNewUtf16To8(Text());
	char *Tabs = new char[Depth+1];
	memset(Tabs, '\t', Depth);
	Tabs[Depth] = 0;

	if (Tag)
	{
		if (IsBlock(Disp))
		{
			p.Print("\n%s<%s", Tabs, Tag);
		}
		else
		{
			p.Print("<%s", Tag);
		}
	}

	if (Attr.Length())
	{
		const char *a;
		for (char *v = Attr.First(&a); v; v = Attr.Next(&a))
		{
			if (stricmp(a, "style"))
				p.Print(" %s=\"%s\"", a, v);
		}
	}
	if (Props.Length())
	{
		GAutoString s = ToString();
		p.Print(" style=\"%s\"", s.Get());
	}

	if (Tags.Length())
	{
		if (Tag)
		{
			p.Write((char*)">", 1);
			TextToStream(p, Text());
		}

		bool Last = IsBlock(Disp);
		for (GTag *c = Tags.First(); c; c = Tags.Next())
		{
			c->CreateSource(p, Parent ? Depth+1 : 0, Last);
			Last = IsBlock(c->Disp);
		}

		if (Tag)
		{
			if (IsBlock(Disp))
			{
				p.Print("\n%s</%s>\n", Tabs, Tag);
			}
			else
			{
				p.Print("</%s>", Tag);
			}
		}
	}
	else if (Tag)
	{
		if (Text())
		{
			p.Write((char*)">", 1);
			TextToStream(p, Text());
			p.Print("</%s>", Tag);
		}
		else
		{
			p.Print("/>\n");
		}
	}
	else
	{
		TextToStream(p, Text());
	}

	DeleteArray(Tabs);

	return true;
}

void GTag::SetTag(const char *NewTag)
{
	DeleteArray(Tag);
	Tag = NewStr(NewTag);

	if (Info = GetTagInfo(Tag))
	{
		TagId = Info->Id;
		Disp = (Info->Flags & TI_BLOCK) ? DispBlock : DispInline;
		SetStyle();
	}
}

void GTag::_TraceOpenTags()
{
	GStringPipe p;
	for (GTag *t=Html->OpenTags.First(); t; t=Html->OpenTags.Next())
	{
		p.Print(", %s", t->Tag);
		if (t->HtmlId)
		{
			p.Print("#%s", t->HtmlId);
		}
	}
	char *s = p.NewStr();
	if (s)
	{
		LgiTrace("Open tags = '%s'\n", s + 2);
		DeleteArray(s);
	}
}

COLOUR GTag::_Colour(bool f)
{
	for (GTag *t = this; t; t = t->Parent)
	{
		ColorDef c = f ? t->Color() : t->BackgroundColor();
		if (c.Type != ColorInherit)
		{
			return c.Rgb32;
		}

		if (!f && t->TagId == TAG_TABLE)
			break;
	}

	return GT_TRANSPARENT;
}

void GTag::CopyClipboard(GBytePipe &p)
{
	if (!Parent)
	{
		Selected = false;
	}

	int Min = -1;
	int Max = -1;

	if (Cursor >= 0 && Selection >= 0)
	{
		Min = min(Cursor, Selection);
		Max = max(Cursor, Selection);
	}
	else if (Selected)
	{
		Max = max(Cursor, Selection);
	}
	else
	{
		Min = max(Cursor, Selection);
	}

	int Off = -1;
	int Chars = 0;
	if (Min >= 0 && Max >= 0)
	{
		Off = Min;
		Chars = Max - Min;
	}
	else if (Min >= 0)
	{
		Off = Min;
		Chars = StrlenW(Text()) - Min;
		Selected = true;
	}
	else if (Max >= 0)
	{
		Off = 0;
		Chars = Max;
		Selected = false;
	}
	else if (Selected)
	{
		Off = 0;
		Chars = StrlenW(Text());
	}

	if (Off >= 0 && Chars > 0)
	{
		p.Write((uchar*) (Text() + Off), Chars * sizeof(char16));
	}

	if (Selected)
	{
		switch (TagId)
		{
			case TAG_BR:
			{
				char16 NL[] = {'\n', 0};
				p.Write((uchar*) NL, sizeof(char16));
				break;
			}
			case TAG_P:
			{
				char16 NL[] = {'\n', '\n', 0};
				p.Write((uchar*) NL, sizeof(char16) * 2);
				break;
			}
		}
	}

	for (GTag *c = Tags.First(); c; c = Tags.Next())
	{
		c->CopyClipboard(p);
	}
}

static
char*
_DumpColour(GCss::ColorDef c)
{
	static char Buf[4][32];
	static int Cur = 0;
	char *b = Buf[Cur++];
	if (Cur == 4) Cur = 0;

	if (c.Type == GCss::ColorInherit)
		strcpy(b, "Inherit");
	else
		sprintf(b, "%2.2x,%2.2x,%2.2x(%2.2x)", R32(c.Rgb32),G32(c.Rgb32),B32(c.Rgb32),A32(c.Rgb32));

	return b;
}

void GTag::_Dump(GStringPipe &Buf, int Depth)
{
	char Tab[64];
	char s[1024];
	memset(Tab, '\t', Depth);
	Tab[Depth] = 0;

	char Trs[1024] = "";
	for (int i=0; i<TextPos.Length(); i++)
	{
		GFlowRect *Tr = TextPos[i];
		
		GAutoString Utf8(LgiNewUtf16To8(Tr->Text, Tr->Len*sizeof(char16)));
		if (Utf8)
		{
			int Len = strlen(Utf8);
			if (Len > 40)
			{
				Utf8[40] = 0;
			}
		}
		else if (Tr->Text)
		{
			Utf8.Reset(NewStr(""));
		}

		int Len = strlen(Trs);
		snprintf(Trs+Len, sizeof(Trs)-Len-1, "Tr(%i,%i %ix%i '%s') ", Tr->x1, Tr->y1, Tr->X(), Tr->Y(), Utf8.Get());
	}
	
	char *ElementName = TagId == CONTENT ? (char*)"Content" :
				(TagId == ROOT ? (char*)"Root" : Tag);

	snprintf(s, sizeof(s)-1,
			"%sStartTag=%s%c%s (%i) Pos=%i,%i Size=%i,%i Color=%s/%s %s\r\n",
			Tab,
			ElementName,
			HtmlId ? '#' : ' ',
			HtmlId ? HtmlId : (char*)"",
			WasClosed,
			Pos.x, Pos.y,
			Size.x, Size.y,
			_DumpColour(Color()), _DumpColour(BackgroundColor()),
			Trs);
	Buf.Push(s);

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->_Dump(Buf, Depth+1);
	}

	if (Tags.Length())
	{
		snprintf(s, sizeof(s)-1, "%sEnd '%s'\r\n", Tab, ElementName);
		Buf.Push(s);
	}
}

GAutoWString GTag::DumpW()
{
	GStringPipe Buf;
	// Buf.Print("Html pos=%s\n", Html?Html->GetPos().GetStr():0);
	_Dump(Buf, 0);
	GAutoString a(Buf.NewStr());
	GAutoWString w(LgiNewUtf8To16(a));
	return w;
}

GFont *GTag::NewFont()
{
	GFont *f = new GFont;
	if (f)
	{
		GFont *Cur = GetFont();
		if (Cur) *f = *Cur;
		else *f = *Html->DefFont();
	}
	return f;
}

GFont *GTag::GetFont()
{
	if (!Font)
	{
		if (PropAddress(PropFontFamily) != 0                    ||
			FontSize().Type             != LenInherit           ||
			FontStyle()                 != FontStyleInherit     ||
			FontVariant()               != FontVariantInherit   ||
			FontWeight()                != FontWeightInherit    ||
			TextDecoration()            != TextDecorInherit)
		{
			GCss c = *this;

            GCss::PropMap Map;
            Map.Add(PropFontFamily, new GCss::PropArray);
			Map.Add(PropFontSize, new GCss::PropArray);
			Map.Add(PropFontStyle, new GCss::PropArray);
			Map.Add(PropFontVariant, new GCss::PropArray);
			Map.Add(PropFontWeight, new GCss::PropArray);
			Map.Add(PropTextDecoration, new GCss::PropArray);

			for (GTag *t = Parent; t; t = t->Parent)
			{
				if (!c.InheritCollect(*t, Map))
					break;
			}
			
			c.InheritResolve(Map);
			
			Map.DeleteObjects();

			if (Font = Html->FontCache->GetFont(&c))
				return Font;
		}
		else
		{
			GTag *t = this;
			while (!t->Font && t->Parent)
			{
				t = t->Parent;
			}

			if (t->Font)
				return t->Font;
		}
		
		Font = Html->DefFont();
	}
	
	return Font;
}

GTag *GTag::PrevTag()
{
	if (Parent)
	{
		int i = Parent->Tags.IndexOf(this);
		if (i >= 0)
		{
			return Parent->Tags.ItemAt(i-1);
		}
	}

	return 0;
}

void GTag::Invalidate()
{
	GRect p = GetRect();
	for (GTag *t=Parent; t; t=t->Parent)
	{
		p.Offset(t->Pos.x, t->Pos.y);
	}
	Html->Invalidate(&p);
}

GTag *GTag::IsAnchor(GAutoString *Uri)
{
	GTag *a = 0;
	for (GTag *t = this; t; t = t->Parent)
	{
		if (t->TagId == TAG_A)
		{
			a = t;
			break;
		}
	}

	if (a && Uri)
	{
		const char *u = 0;
		if (a->Get("href", u))
		{
			GAutoWString w(CleanText(u, strlen(u)));
			if (w)
			{
				Uri->Reset(LgiNewUtf16To8(w));
			}
		}
	}

	return a;
}

bool GTag::OnMouseClick(GMouse &m)
{
	bool Processed = false;

	// char msg[256];
	if (m.IsContextMenu())
	{
		GAutoString Uri;
		GTag *a = IsAnchor(&Uri);
		if (a && ValidStr(Uri))
		{
			GSubMenu RClick;

			#define IDM_COPY_LINK	100
			if (Html->GetMouse(m, true))
			{
				int Id = 0;

				RClick.AppendItem(LgiLoadString(L_COPY_LINK_LOCATION, "&Copy Link Location"), IDM_COPY_LINK, Uri != 0);
				if (Html->GetEnv())
					Html->GetEnv()->AppendItems(&RClick);

				switch (Id = RClick.Float(Html, m.x, m.y))
				{
					case IDM_COPY_LINK:
					{
						GClipBoard Clip(Html);
						Clip.Text(Uri);
						break;
					}
					default:
					{
						if (Html->GetEnv())
						{
							Html->GetEnv()->OnMenu(Html, Id, a);
						}
						break;
					}
				}
			}

			Processed = true;
		}
	}
	else if (m.Down() && m.Left())
	{
		#ifdef _DEBUG
		if (m.Ctrl())
		{
			GAutoString Style = ToString();
			GStringPipe p(256);
			p.Print("Tag: %s\n", Tag ? Tag : "CONTENT");
			if (Class.Length())
			{
				p.Print("Class(es): ");
				for (int i=0; i<Class.Length(); i++)
				{
					p.Print("%s%s", i?", ":"", Class[i]);
				}
				p.Print("\n");
			}
			if (HtmlId)
			{
				p.Print("Id: %s\n", HtmlId);
			}
			p.Print("Pos: %i,%i   Size: %i,%i\n\n", Pos.x, Pos.y, Size.x, Size.y);
			p.Print("Style:\n", Style.Get());
			GToken s(Style, "\n");
			for (int i=0; i<s.Length(); i++)
				p.Print("    %s\n", s[i]);
			
			p.Print("\nParent tags:\n");
			GDisplayString Sp(SysFont, " ");
			for (GTag *t=Parent; t && t->Parent; t=t->Parent)
			{
				GStringPipe Tmp;
				Tmp.Print("    %s", t->Tag ? t->Tag : "CONTENT");
				if (t->HtmlId)
				{
					Tmp.Print("#%s", t->HtmlId);
				}
				for (int i=0; i<t->Class.Length(); i++)
				{
					Tmp.Print(".%s", t->Class[i]);
				}
				GAutoString Txt(Tmp.NewStr());
				p.Print("%s", Txt.Get());
				GDisplayString Ds(SysFont, Txt);
				int Px = 170 - Ds.X();
				int Chars = Px / Sp.X();
				for (int c=0; c<Chars; c++)
					p.Print(" ");

				p.Print("(%i,%i)-(%i,%i)\n",
					t->Pos.x,
					t->Pos.y,
					t->Size.x,
					t->Size.y);
			}
			
			GAutoString a(p.NewStr());
			
			LgiMsg(	Html,
					"%s",
					Html->GetClass(),
					MB_OK,
					a.Get());
		}
		else
		#endif
		{
			GAutoString Uri;
			
			if (Html &&
				Html->Environment)
			{
				if (IsAnchor(&Uri))
				{
					if (!Html->d->LinkDoubleClick || m.Double())
					{
						Html->Environment->OnNavigate(Uri);
						Processed = true;
					}
				}
				else
				{
					Processed = OnClick();
				}
			}
		}
	}

	return Processed;
}

GTag *GTag::GetBlockParent(int *Idx)
{
	if (IsBlock(Disp))
	{
		if (Idx)
			*Idx = 0;

		return this;
	}

	for (GTag *t = this; t; t = t->Parent)
	{
		if (IsBlock(t->Parent->Disp))
		{
			if (Idx)
			{
				*Idx = t->Parent->Tags.IndexOf(t);
			}

			return t->Parent;
		}
	}

	return 0;
}

GTag *GTag::GetAnchor(char *Name)
{
	if (!Name)
		return 0;

	const char *n;
	if (IsAnchor(0) && Get("name", n) && n && !stricmp(Name, n))
	{
		return this;
	}

	List<GTag>::I it = Tags.Start();
	for (GTag *t=*it; t; t=*++it)
	{
		GTag *Result = t->GetAnchor(Name);
		if (Result) return Result;
	}

	return 0;
}

GTag *GTag::GetTagByName(const char *Name)
{
	if (Name)
	{
		if (Tag && stricmp(Tag, Name) == 0)
		{
			return this;
		}

		for (GTag *t=Tags.First(); t; t=Tags.Next())
		{
			GTag *Result = t->GetTagByName(Name);
			if (Result) return Result;
		}
	}

	return 0;
}

static int IsNearRect(GRect *r, int x, int y)
{
	if (r->Overlap(x, y))
	{
		return 0;
	}
	else if (x >= r->x1 && x <= r->x2)
	{
		if (y < r->y1)
			return r->y1 - y;
		else
			return y - r->y2;
	}
	else if (y >= r->y1 && y <= r->y2)
	{
		if (x < r->x1)
			return r->x1 - x;
		else
			return x - r->x2;
	}

	int64 dx = 0;
	int64 dy = 0;
	
	if (x < r->x1)
	{
		if (y < r->y1)
		{
			// top left
			dx = r->x1 - x;
			dy = r->y1 - y;
		}
		else
		{
			// bottom left
			dx = r->x1 - x;
			dy = y - r->y2;
		}
	}
	else
	{
		if (y < r->y1)
		{
			// top right
			dx = x - r->x2;
			dy = r->y1 - y;
		}
		else
		{
			// bottom right
			dx = x - r->x2;
			dy = y - r->y2;
		}
	}

	return (int) sqrt( (double) ( (dx * dx) + (dy * dy) ) );
}

int GTag::NearestChar(GFlowRect *Tr, int x, int y)
{
	GFont *f = GetFont();
	if (f)
	{
		GDisplayString ds(f, Tr->Text, Tr->Len);
		int c = ds.CharAt(x - Tr->x1);
		
		if (Tr->Text == PreText())
		{
			return 0;
		}
		else
		{
			char16 *t = Tr->Text + c;
			int Len = StrlenW(Text());
			if (t >= Text() &&
				t <= Text() + Len)
			{
				return (t - Text()) - GetTextStart();
			}
			else
			{
				LgiTrace("%s:%i - Error getting char at position.\n", _FL);
			}
		}
	}

	return -1;
}

GTagHit GTag::GetTagByPos(int x, int y)
{
	GTagHit r;
	
	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		if (t->Pos.x >= 0 &&
			t->Pos.y >= 0)
		{
			GTagHit hit = t->GetTagByPos(x - t->Pos.x, y - t->Pos.y);
			if (hit < r)
			{
				r = hit;
			}
		}
	}

	if (TagId == TAG_IMG)
	{
		GRect img(0, 0, Size.x - 1, Size.y - 1);
		if (img.Overlap(x, y))
		{
			r.Hit = this;
			r.Block = 0;
			r.Near = 0;
		}
	}
	else if (Text())
	{
		for (int i=0; i<TextPos.Length(); i++)
		{
			GFlowRect *Tr = TextPos[i];
			
			GTagHit hit;
			hit.Hit = this;
			hit.Block = Tr;
			hit.Near = IsNearRect(Tr, x, y);
			LgiAssert(hit.Near >= 0);
			if (hit < r)
			{
				r = hit;
				r.Index = NearestChar(Tr, x, y);
			}
		}
	}

	if (x >= 0 &&
		y >= 0 &&
		x < Size.x &&
		y < Size.y &&
		(!r.Hit || r.Near > 0))
	{
		r.Hit = this;
		r.Near = 0;
		r.Block = NULL;
	}

	return r;
}

int GTag::OnNotify(int f)
{
	if (!Ctrl)
		return 0;
	
	switch (CtrlType)
	{
		case CtrlSubmit:
		{
			GTag *Form = this;
			while (Form && Form->TagId != TAG_FORM)
				Form = Form->Parent;
			if (Form)
				Html->OnSubmitForm(Form);
			break;
		}
		default:
		{
			CtrlValue = Ctrl->Name();
			break;
		}
	}
	
	return 0;
}

void GTag::CollectFormValues(GHashTbl<const char*,char*> &f)
{
	if (CtrlType != CtrlNone)
	{
		const char *Name;
		if (Get("name", Name))
		{
			char *Existing = f.Find(Name);
			if (Existing)
				DeleteArray(Existing);

			char *Val = CtrlValue.Str();
			if (Val)
			{
				GStringPipe p(256);
				for (char *v = Val; *v; v++)
				{
					if (*v == ' ')
						p.Write("+", 1);
					else if (IsAlpha(*v) || IsDigit(*v) || *v == '_' || *v == '.')
						p.Write(v, 1);
					else
						p.Print("%%%02.2X", *v);
				}
				f.Add(Name, p.NewStr());
			}
			else
			{
				f.Add(Name, NewStr(""));
			}
		}
	}

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->CollectFormValues(f);
	}
}

GTag *GTag::FindCtrlId(int Id)
{
	if (Ctrl && Ctrl->GetId() == Id)
		return this;

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		GTag *f = t->FindCtrlId(Id);
		if (f)
			return f;
	}
	
	return NULL;
}

void GTag::Find(int TagType, GArray<GTag*> &Out)
{
	if (TagId == TagType)
	{
		Out.Add(this);
	}
	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->Find(TagType, Out);
	}
}

void GTag::SetImage(const char *Uri, GSurface *Img)
{
	if (Img)
	{
		if (TagId != TAG_IMG)
		{
			ImageDef *Def = (ImageDef*)GCss::Props.Find(PropBackgroundImage);
			if (Def)
			{
				Def->Type = ImageOwn;
				DeleteObj(Def->Img);
				Def->Img = Img;
			}
		}
		else
		{
			Image.Reset(Img);
			
			GRect r = XSubRect();
			if (r.Valid())
			{
				GAutoPtr<GSurface> t(new GMemDC(r.X(), r.Y(), Image->GetBits()));
				if (t)
				{
					t->Blt(0, 0, Image, &r);
					Image = t;
				}
			}
		}		

		for (GTag *t = this; t; t = t->Parent)
		{
			t->MinContent = 0;
			t->MaxContent = 0;
		}
	}
	else
	{
		Html->d->Loading.Add(Uri, this);
	}
}

void GTag::LoadImage(const char *Uri)
{
	#if 1
	GDocumentEnv::LoadJob *j = Html->Environment->NewJob();
	if (j)
	{
		LgiAssert(Html);
		j->Uri.Reset(NewStr(Uri));
		j->View = Html;
		j->UserData = this;
		j->UserUid = Html->d->DocumentUid;

		GDocumentEnv::LoadType Result = Html->Environment->GetContent(j);
		if (Result == GDocumentEnv::LoadImmediate)
		{
			SetImage(Uri, j->pDC.Release());
		}
		DeleteObj(j);
	}
	#endif
}

void GTag::LoadImages()
{
	const char *Uri = 0;
	if (Html->Environment &&
		!Image &&
		Get("src", Uri))
	{
		LoadImage(Uri);
	}

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->LoadImages();
	}
}

void GTag::ImageLoaded(char *uri, GSurface *Img, int &Used)
{
	const char *Uri = 0;
	if (!Image &&
		Get("src", Uri))
	{
		if (strcmp(Uri, uri) == 0)
		{
			if (Used == 0)
			{
				SetImage(Uri, Img);
			}
			else
			{
				SetImage(Uri, new GMemDC(Img));
			}
			Used++;
		}
	}

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->ImageLoaded(uri, Img, Used);
	}
}

/// This code matches a all the parts of a selector
bool GTag::MatchFullSelector(GCss::Selector *Sel)
{
	bool Complex = Sel->Combs.Length() > 0;
	int CombIdx = Complex ? Sel->Combs.Length() - 1 : 0;
	int StartIdx = (Complex) ? Sel->Combs[CombIdx] + 1 : 0;
	
	bool Match = MatchSimpleSelector(Sel, StartIdx);
	if (!Match)
		return false;

	if (Complex)
	{
		GTag *CurrentParent = Parent;
		
		for (; CombIdx >= 0; CombIdx--)
		{
			if (CombIdx >= Sel->Combs.Length())
				break;

			StartIdx = Sel->Combs[CombIdx];
			LgiAssert(StartIdx > 0);

			if (StartIdx >= Sel->Parts.Length())
				break;
			
			GCss::Selector::Part &p = Sel->Parts[StartIdx];
			switch (p.Type)
			{
				case GCss::Selector::CombChild:
				{
					// LgiAssert(!"Not impl.");
					return false;
					break;
				}
				case GCss::Selector::CombAdjacent:
				{
					// LgiAssert(!"Not impl.");
					return false;
					break;
				}
				case GCss::Selector::CombDesc:
				{
					// Does the parent match the previous simple selector
					int PrevIdx = StartIdx - 1;
					while (PrevIdx > 0 && Sel->Parts[PrevIdx-1].IsSel())
					{
						PrevIdx--;
					}
					bool ParMatch = false;
					for (; !ParMatch && CurrentParent; CurrentParent = CurrentParent->Parent)
					{
						ParMatch = CurrentParent->MatchSimpleSelector(Sel, PrevIdx);
					}
					if (!ParMatch)
						return false;
					break;
				}
				default:
				{
					LgiAssert(!"This must be a comb.");
					return false;
					break;
				}
			}
		}
	}

	return Match;
}

/// This code matches a simple part of a selector, i.e. no combinatorial operators involved.
bool GTag::MatchSimpleSelector
(
	/// The full selector.
	GCss::Selector *Sel,
	/// The start index of the simple selector parts. Stop at the first comb operator or the end of the parts.
	int PartIdx
)
{
	for (int n = PartIdx; n<Sel->Parts.Length(); n++)
	{
		GCss::Selector::Part &p = Sel->Parts[n];
		switch (p.Type)
		{
			case GCss::Selector::SelType:
			{
				if (!Tag || stricmp(Tag, p.Value))
					return false;
				break;
			}
			case GCss::Selector::SelUniversal:
			{
				// Match everything
				return true;
				break;
			}
			case GCss::Selector::SelAttrib:
			{
				if (!p.Value)
					return false;

				char *e = strchr(p.Value, '=');
				if (!e)
					return false;

				GAutoString Var(NewStr(p.Value, e - p.Value));
				const char *TagVal;
				if (!Get(Var, TagVal))
					return false;

				GAutoString Val(NewStr(e + 1));
				if (stricmp(Val, TagVal))
					return false;
				break;
			}
			case GCss::Selector::SelClass:
			{
				// Check the class matches
				if (Class.Length() == 0)
					return false;

				bool Match = false;
				for (int i=0; i<Class.Length(); i++)
				{
					if (!stricmp(Class[i], p.Value))
					{
						Match = true;
						break;
					}
				}
				if (!Match)
					return false;
				break;
			}
			case GCss::Selector::SelMedia:
			{
				return false;
				break;
			}
			case GCss::Selector::SelID:
			{
				const char *Id;
				if (!Get("id", Id) || stricmp(Id, p.Value))
					return false;
				break;
			}
			case GCss::Selector::SelPseudo:
			{
				const char *Href = NULL;
				if
				(
					(
						TagId == TAG_A
						&&
						p.Value && !stricmp(p.Value, "link")
						&&
						Get("href", Href)
					)
					||
					(
						p.Value
						&&
						*p.Value == '-'
					)
				)
					break;
					
				return false;
				break;
			}
			default:
			{
				// Comb operator, so return the current match value
				return true;
			}
		}
	}
	
	return true;
}

// After CSS has changed this function scans through the CSS and applies any rules
// that match the current tag.
void GTag::Restyle()
{
	int i;

	GArray<GCss::SelArray*> Maps;
	GCss::SelArray *s;
	if (s = Html->CssStore.TypeMap.Find(Tag))
		Maps.Add(s);
	if (HtmlId && (s = Html->CssStore.IdMap.Find(HtmlId)))
		Maps.Add(s);
	for (i=0; i<Class.Length(); i++)
	{
		if (s = Html->CssStore.ClassMap.Find(Class[i]))
			Maps.Add(s);
	}

	for (i=0; i<Maps.Length(); i++)
	{
		GCss::SelArray *s = Maps[i];
		for (int i=0; i<s->Length(); i++)
		{
			GCss::Selector *Sel = (*s)[i];
			
			if (MatchFullSelector(Sel))
			{
				SetCssStyle(Sel->Style);
			}
		}
	}
	
	if (Display() != DispInherit)
		Disp = Display();	

	#if 1 && defined(_DEBUG)
	if (Debug)
	{
		GAutoString Style = ToString();
		LgiTrace(">>>> %s <<<<:\n%s\n\n", Tag, Style);
	}
	#endif
}

void GTag::SetStyle()
{
	const static float FntMul[] =
	{
		0.6f,	// size=1
		0.89f,	// size=2
		1.0f,	// size=3
		1.2f,	// size=4
		1.5f,	// size=5
		2.0f,	// size=6
		3.0f	// size=7
	};

	const char *s = 0;
	#ifdef _DEBUG
	if (Get("debug", s))
		Debug = atoi(s);
	#endif

	if (Get("Color", s))
	{
		ColorDef Def;
		if (ParseColour(s, Def))
		{
			Color(Def);
		}
	}

	if (Get("Background", s) ||
		Get("bgcolor", s))
	{
		ColorDef Def;
		if (ParseColour(s, Def))
		{
			BackgroundColor(Def);
		}
		else
		{
			GCss::ImageDef Img;
			
			Img.Type = ImageUri;
			Img.Uri.Reset(NewStr(s));
			
			BackgroundImage(Img);
			BackgroundRepeat(RepeatBoth);
		}
	}

	switch (TagId)
	{
		case TAG_LINK:
		{
			const char *Type, *Href;
			if (Html->Environment &&
				Get("type", Type) &&
				Get("href", Href))
			{
				if (!stricmp(Type, "text/css"))
				{
					GDocumentEnv::LoadJob *j = Html->Environment->NewJob();
					if (j)
					{
						LgiAssert(Html);
						GTag *t = this;
						
						j->Uri.Reset(NewStr(Href));
						j->View = Html;
						j->UserData = t;
						j->UserUid = Html->d->DocumentUid;
						// j->Pref = GDocumentEnv::LoadJob::FmtFilename;

						GDocumentEnv::LoadType Result = Html->Environment->GetContent(j);
						if (Result == GDocumentEnv::LoadImmediate)
						{
							if (j->Stream)
							{
								uint64 Len = j->Stream->GetSize();
								if (Len > 0)
								{
									GAutoString a(new char[Len+1]);
									int r = j->Stream->Read(a, Len);
									a[r] = 0;

									Html->AddCss(a.Release());
								}
							}
							else LgiAssert(!"Not impl.");
						}

						DeleteObj(j);
					}
				}
			}
			break;
		}
		case TAG_BLOCKQUOTE:
		{
			MarginTop(Len("8px"));
			MarginBottom(Len("8px"));
			MarginLeft(Len("16px"));

			if (Get("Type", s))
			{
				if (stricmp(s, "cite") == 0)
				{
					BorderLeft(BorderDef("1px solid blue"));
					PaddingLeft(Len("0.5em"));
					
					ColorDef Def;
					Def.Type = ColorRgb;
					Def.Rgb32 = Rgb32(0x80, 0x80, 0x80);
					Color(Def);
				}
			}
			break;
		}
		case TAG_P:
		{
			MarginBottom(Len("1em"));
			break;
		}
		case TAG_A:
		{
			const char *Href;
			if (Get("href", Href))
			{
				ColorDef c;
				c.Type = ColorRgb;
				c.Rgb32 = Rgb32(0, 0, 255);
				Color(c);
				TextDecoration(TextDecorUnderline);

				/* FIXME
				if (s = Html->CssMap.Find("a"))
					SetCssStyle(s);
				if (s = Html->CssMap.Find("a:link"))
					SetCssStyle(s);
				*/
			}
			break;
		}
		case TAG_TABLE:
		{
			if (Get("border", s))
			{
				BorderDef b;
				if (b.Parse(s))
				{
					BorderLeft(b);
					BorderRight(b);
					BorderTop(b);
					BorderBottom(b);
				}
			}
			break;
		}
		case TAG_TD:
		{
			GTag *Table = GetTable();
			if (Table)
			{
				const char *s = "1px";
				Len l = Table->_CellPadding();
				if (!l.IsValid())
					l.Parse(s);
				PaddingLeft(l);
				PaddingRight(l);
				PaddingTop(l);
				PaddingBottom(l);
			}
			break;
		}
		case TAG_BODY:
		{
			PaddingLeft(Len(DefaultBodyMargin));
			PaddingTop(Len(DefaultBodyMargin));
			PaddingRight(Len(DefaultBodyMargin));
			
			if (Get("text", s))
			{
				ColorDef c;
				if (c.Parse(s))
				{
					Color(c);
				}
			}
			break;
		}
		case TAG_OL:
		case TAG_UL:
		{
			MarginLeft(Len("16px"));
			break;
		}
	}

	Get("id", HtmlId);

	if (Get("class", s))
	{
		Class.Parse(s);
	}

	Restyle();

	if (Get("style", s))
	{
		SetCssStyle(s);
	}

	if (Get("width", s))
	{
		Len l;
		if (l.Parse(s, ParseRelaxed))
		{
			Width(l);
			
			Len tmp = Width();
			if (tmp.Value == 0.0 && tmp.Type == LenPx)
			{
				int asd= 0;
			}
		}
	}
	if (Get("height", s))
	{
		Len l;
		if (l.Parse(s, ParseRelaxed))
			Height(l);
	}
	if (Get("align", s))
	{
		if (stricmp(s, "left") == 0) TextAlign(Len(AlignLeft));
		else if (stricmp(s, "right") == 0) TextAlign(Len(AlignRight));
		else if (stricmp(s, "center") == 0) TextAlign(Len(AlignCenter));
	}
	if (Get("valign", s))
	{
		if (stricmp(s, "top") == 0) VerticalAlign(Len(VerticalTop));
		else if (stricmp(s, "middle") == 0) VerticalAlign(Len(VerticalMiddle));
		else if (stricmp(s, "bottom") == 0) VerticalAlign(Len(VerticalBottom));
	}

	switch (TagId)
	{
	    case TAG_BIG:
	    {
            GCss::Len l;
            l.Type = SizeLarger;
	        FontSize(l);
	        break;
	    }
		case TAG_META:
		{
			char *Cs = 0;

			const char *s;
			if (Get("http-equiv", s) &&
				stricmp(s, "Content-Type") == 0)
			{
				const char *ContentType;
				if (Get("content", ContentType))
				{
					char *CharSet = stristr(ContentType, "charset=");
					if (CharSet)
					{
						ParsePropValue(CharSet + 8, Cs);
					}
				}
			}

			if (Get("name", s) && stricmp(s, "charset") == 0 && Get("content", s))
			{
				Cs = NewStr(s);
			}
			else if (Get("charset", s))
			{
				Cs = NewStr(s);
			}

			if (Cs)
			{
				if (Cs &&
					stricmp(Cs, "utf-16") != 0 &&
					stricmp(Cs, "utf-32") != 0 &&
					LgiGetCsInfo(Cs))
				{
					DeleteArray(Html->DocCharSet);
					Html->DocCharSet = NewStr(Cs);
				}

				DeleteArray(Cs);
			}
			break;
		}
		case TAG_BODY:
		{
			if (BackgroundColor().Type != ColorInherit)
			{
				Html->SetBackColour(BackgroundColor().Rgb32);
			}
			
			GFont *f = GetFont();
			if (FontSize().Type == LenInherit)
			{
       		    FontSize(Len(LenPt, (float)f->PointSize()));
			}

			const char *Back;
			if (Get("background", Back) && Html->Environment)
			{
				LoadImage(Back);
			}
			break;
		}
		case TAG_HEAD:
		{
			Display(Disp = DispNone);
			break;
		}
		case TAG_PRE:
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				LgiAssert(ValidStr(Type.GetFace()));
				FontFamily(StringsDef(Type.GetFace()));
			}
			break;
		}
		case TAG_TABLE:
		{
			Len l;
			const char *s;
			if (Get("cellspacing", s) &&
				l.Parse(s, ParseRelaxed))
			{
				BorderSpacing(l);
			}
			if (Get("cellpadding", s) &&
				l.Parse(s, ParseRelaxed))
			{
				_CellPadding(l);
			}

			if (Get("align", s))
			{
				Len l;
				if (l.Parse(s))
					XAlign = l.Type;
			}
			break;
		}
		case TAG_TR:
			break;
		case TAG_TD:
		{
			const char *s;
			if (Get("colspan", s))
				Span.x = atoi(s);
			else
				Span.x = 1;
			if (Get("rowspan", s))
				Span.y = atoi(s);
			else
				Span.y = 1;
			
			if (Get("align", s))
			{
				Len l;
				if (l.Parse(s))
					XAlign = l.Type;
			}
			break;
		}
		case TAG_IMG:
		{
			const char *Uri;
			if (Html->Environment &&
				Get("src", Uri))
			{
				LoadImage(Uri);
			}
			break;
		}
		case TAG_H1:
		{
			char s[32];
			sprintf(s, "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[5]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H2:
		{
			char s[32];
			sprintf(s, "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[4]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H3:
		{
			char s[32];
			sprintf(s, "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[3]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H4:
		{
			char s[32];
			sprintf(s, "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[2]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H5:
		{
			char s[32];
			sprintf(s, "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[1]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H6:
		{
			char s[32];
			sprintf(s, "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[0]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_FONT:
		{
			const char *s = 0;
			if (Get("Face", s))
			{
				char16 *cw = CleanText(s, strlen(s), true);
				char *c8 = LgiNewUtf16To8(cw);
				DeleteArray(cw);
				GToken Faces(c8, ",");
				DeleteArray(c8);
				char *face = TrimStr(Faces[0]);
				if (ValidStr(face))
				{
					FontFamily(face);
					DeleteArray(face);
				}
				else
				{
					LgiTrace("%s:%i - No face for font tag.\n", __FILE__, __LINE__);
				}
			}

			if (Get("Size", s))
				FontSize(Len(s));
			break;
		}
		case TAG_STRONG:
		case TAG_B:
		{
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_I:
		{
			FontStyle(FontStyleItalic);
			break;
		}
		case TAG_U:
		{
			TextDecoration(TextDecorUnderline);
			break;
		}
		case TAG_SELECT:
		{
			LgiAssert(!Ctrl);
			Ctrl = new GCombo(Html->d->NextCtrlId++, 0, 0, 100, SysFont->GetHeight() + 8, NULL);
			CtrlType = CtrlSelect;
			break;
		}
		case TAG_INPUT:
		{
			LgiAssert(!Ctrl);

			const char *Type, *Value = NULL;
			Get("value", Value);
			GAutoWString CleanValue(Value ? CleanText(Value, strlen(Value), true, true) : NULL);
			if (CleanValue)
			{
				CtrlValue = CleanValue;
			}
			
			if (Get("type", Type))
			{
				if (!stricmp(Type, "password")) CtrlType = CtrlPassword;
				else if (!stricmp(Type, "email")) CtrlType = CtrlEmail;
				else if (!stricmp(Type, "text")) CtrlType = CtrlText;
				else if (!stricmp(Type, "button")) CtrlType = CtrlButton;
				else if (!stricmp(Type, "submit")) CtrlType = CtrlSubmit;
				else if (!stricmp(Type, "hidden")) CtrlType = CtrlHidden;

				DeleteObj(Ctrl);
				if (CtrlType == CtrlEmail ||
					CtrlType == CtrlText ||
					CtrlType == CtrlPassword)
				{
					GEdit *Ed;
					GAutoString UtfCleanValue(LgiNewUtf16To8(CleanValue));
					if (Ctrl = Ed = new GEdit(Html->d->NextCtrlId++, 0, 0, 60, SysFont->GetHeight() + 8, UtfCleanValue))
					{
						Ed->Sunken(false);
						Ed->Password(CtrlType == CtrlPassword);
					}						
				}
				else if (CtrlType == CtrlButton ||
						 CtrlType == CtrlSubmit)
				{
					GAutoString UtfCleanValue(LgiNewUtf16To8(CleanValue));
					if (UtfCleanValue)
					{
						Ctrl = new InputButton(this, Html->d->NextCtrlId++, UtfCleanValue);
					}
				}
			}
			break;
		}
	}
	
	if (Ctrl)
	{
		GFont *f = GetFont();
		if (f)
			Ctrl->SetFont(f, false);
	}

	if (Disp == DispBlock && Html->Environment)
	{
		GCss::ImageDef Img = BackgroundImage();
		if (Img.Type == ImageUri)
		{
			LoadImage(Img.Uri);
		}
	}
}

void GTag::SetCssStyle(const char *Style)
{
	if (Style)
	{
		// Strip out comments
		char *Comment = 0;
		while (Comment = strstr((char*)Style, "/*"))
		{
			char *End = strstr(Comment+2, "*/");
			if (!End)
				break;
			for (char *c = Comment; c<=End+2; c++)
				*c = ' ';
		}

		// Parse CSS
		const char *Ptr = Style;
		GCss::Parse(Ptr, GCss::ParseRelaxed);

		// Update display setting cache
		if (Display() != DispInherit)
			Disp = Display();	
	}
}

char16 *GTag::CleanText(const char *s, int Len, bool ConversionAllowed, bool KeepWhiteSpace)
{
	static const char *DefaultCs = "iso-8859-1";
	char16 *t = 0;

	if (s && Len > 0)
	{
		bool Has8 = false;
		if (Len >= 0)
		{
			for (int n = 0; n < Len; n++)
			{
				if (s[n] & 0x80)
				{
					Has8 = true;
					break;
				}
			}
		}
		else
		{
			for (int n = 0; s[n]; n++)
			{
				if (s[n] & 0x80)
				{
					Has8 = true;
					break;
				}
			}
		}
		
		bool DocAndCsTheSame = false;
		if (Html->DocCharSet && Html->Charset)
		{
			DocAndCsTheSame = stricmp(Html->DocCharSet, Html->Charset) == 0;
		}

		if (Html->DocCharSet &&
			Html->Charset &&
			!Html->OverideDocCharset)
		{
			char *DocText = (char*)LgiNewConvertCp(Html->DocCharSet, s, Html->Charset, Len);
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, DocText, Html->DocCharSet, -1);
			DeleteArray(DocText);
		}
		else if (Html->DocCharSet)
		{
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, Html->DocCharSet, Len);
		}
		else
		{
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, Html->Charset ? Html->Charset : DefaultCs, Len);
		}

		if (t && ConversionAllowed)
		{
			char16 *o = t;
			for (char16 *i=t; *i; )
			{
				switch (*i)
				{
					case '&':
					{
						i++;
						
						if (*i == '#')
						{
							// Unicode Number
							char n[32] = "", *p = n;
							
							i++;
							
							if (*i == 'x' || *i == 'X')
							{
								// Hex number
								i++;
								while (	*i &&
										(
											IsDigit(*i) ||
											(*i >= 'A' && *i <= 'F') ||
											(*i >= 'a' && *i <= 'f')
										) &&
										(p - n) < 31)
								{
									*p++ = *i++;
								}
							}
							else
							{
								// Decimal number
								while (*i && IsDigit(*i) && (p - n) < 31)
								{
									*p++ = *i++;
								}
							}
							*p++ = 0;
							
							char16 Ch = atoi(n);
							if (Ch)
							{
								*o++ = Ch;
							}
							
							if (*i && *i != ';')
								i--;
						}
						else
						{
							// Named Char
							char16 *e = i;
							while (*e && IsAlpha(*e) && *e != ';')
							{
								e++;
							}
							
							GAutoWString Var(NewStrW(i, e-i));							
							char16 Char = GHtmlStatic::Inst->VarMap.Find(Var);
							if (Char)
							{
								*o++ = Char;
								i = e;
							}
							else
							{
								i--;
								*o++ = *i;
							}
						}
						break;
					}
					case '\r':
					{
						break;
					}
					case ' ':
					case '\t':
					case '\n':
					{
						if (KeepWhiteSpace)
						{
							*o++ = *i;
						}
						else
						{
							*o++ = ' ';

							// Skip furthur whitespace
							while (i[1] && IsWhiteSpace(i[1]))
							{
								i++;
							}
						}
						break;
					}
					default:
					{
						// Normal char
						*o++ = *i;
						break;
					}
				}

				if (*i) i++;
				else break;
			}
			*o++ = 0;
		}
	}

	if (t && !*t)
	{
		DeleteArray(t);
	}

	return t;
}

char *GTag::ParseText(char *Doc)
{
	ColorDef c;
	c.Type = ColorRgb;
	c.Rgb32 = LC_WORKSPACE;
	BackgroundColor(c);
	
	TagId = TAG_BODY;
	Tag = NewStr("body");
	Info = GetTagInfo(Tag);
	char *OriginalCp = NewStr(Html->Charset);

	Html->SetBackColour(Rgb24To32(LC_WORKSPACE));
	
	GStringPipe Utf16;
	char *s = Doc;
	while (true)
	{
		if (*s == '\r')
		{
			s++;
		}
		else if (*s == '<')
		{
			// Process tag
			char *e = s;
			e++;
			while (*e && *e != '>')
			{
				if (*e == '\"' || *e == '\'')
				{
					char *q = strchr(e + 1, *e);
					if (q) e = q + 1;
					else e++;
				}
				else e++;
			}
			if (*e == '>') e++;

			// Output tag
			Html->SetCharset("iso-8859-1");
			char16 *t = CleanText(s, e - s, false);
			if (t)
			{
				Utf16.Push(t);
				DeleteArray(t);
			}
			s = e;
		}
		else if (!*s || *s == '\n')
		{
			// Output previous line
			char16 *Line = Utf16.NewStrW();
			if (Line)
			{
				GTag *t = new GTag(Html, this);
				if (t)
				{
					t->Color(ColorDef(LC_TEXT));
					t->Text(Line);
				}
			}

			if (*s == '\n')
			{
				s++;
				GTag *t = new GTag(Html, this);
				if (t)
				{
					t->TagId = TAG_BR;
					t->Tag = NewStr("br");
					t->Info = GetTagInfo(t->Tag);
				}
			}
			else break;
		}
		else
		{
			// Seek end of text
			char *e = s;
			while (*e && *e != '\r' && *e != '\n' && *e != '<') e++;
			
			// Output text
			Html->SetCharset(OriginalCp);
			GAutoWString t(CleanText(s, e - s, false));
			if (t)
			{
				Utf16.Push(t);
			}			
			s = e;
		}
	}
	
	Html->SetCharset(OriginalCp);
	DeleteArray(OriginalCp);

	return 0;
}

bool GTag::ConvertToText(TextConvertState &State)
{
	const static char *Rule = "------------------------------------------------------";
	int DepthInc = 0;

	switch (TagId)
	{
		case TAG_P:
			if (State.GetPrev())
				State.NewLine();
			break;
		case TAG_UL:
		case TAG_OL:
			DepthInc = 2;
			break;
	}

	if (ValidStrW(Txt))
	{
		for (int i=0; i<State.Depth; i++)
			State.Write("  ", 2);
		
		if (TagId == TAG_LI)
			State.Write("* ", 2);

		GAutoString u(LgiNewUtf16To8(Txt));
		int u_len = u ? strlen(u) : 0;
		State.Write(u, u_len);
	}
	
	State.Depth += DepthInc;
	
	List<GTag>::I it = Tags.Start();
	for (GTag *c = *it; c; c = *++it)
	{
		c->ConvertToText(State);
	}

	State.Depth -= DepthInc;
	
	if (IsBlock(Disp))
	{
		if (State.CharsOnLine)
			State.NewLine();
	}
	else
	{
		switch (TagId)
		{
			case TAG_A:
			{
				// Emit the link to the anchor if it's different from the text of the span...
				const char *Href;
				if (Get("href", Href) &&
					ValidStrW(Txt))
				{
					if (strnicmp(Href, "mailto:", 7) == 0)
						Href += 7;
						
					int HrefLen = strlen(Href);
					GAutoWString h(CleanText(Href, HrefLen));
					if (h && StrcmpW(h, Txt) != 0)
					{
						// Href different from the text of the link
						State.Write(" (", 2);
						State.Write(Href, HrefLen);
						State.Write(")", 1);
					}
				}
				break;
			}
			case TAG_HR:
			{
				State.Write(Rule, strlen(Rule));
				State.NewLine();
				break;
			}
			case TAG_BR:
			{
				State.NewLine();
				break;
			}
			default:
				break;
		}
	}
	
	return true;
}

char *GTag::NextTag(char *s)
{
	while (s && *s)
	{
		char *n = strchr(s, '<');
		if (n)
		{
			if (!n[1])
				return NULL;

			if (IsAlpha(n[1]) || strchr("!/", n[1]) || n[1] == '?')
			{
				return n;
			}

			s = n + 1;
		}
		else break;
	}

	return 0;
}

void GHtml::CloseTag(GTag *t)
{
	if (!t)
		return;

	OpenTags.Delete(t);
}

static void SkipNonDisplay(char *&s)
{
	while (*s)
	{
		SkipWhiteSpace(s);

		if (s[0] == '<' &&
			s[1] == '!' &&
			s[2] == '-' &&
			s[3] == '-')
		{
			s += 4;
			char *e = strstr(s, "-->");
			if (e)
				s = e + 3;
			else
			{
				s += strlen(s);
				break;
			}
		}
		else break;
	}
}

char *GTag::ParseHtml(char *Doc, int Depth, bool InPreTag, bool *BackOut)
{
	#if CRASH_TRACE
	LgiTrace("::ParseHtml Doc='%.10s'\n", Doc);
	#endif

	if (Depth >= 1024)
	{
		// Bail
		return Doc + strlen(Doc);
	}

	bool IsFirst = true;
	for (char *s=Doc; s && *s; )
	{
		char *StartTag = s;

		if (*s == '<')
		{
			if (s[1] == '?')
			{
				// Dynamic content
				s += 2;
				while (*s && IsWhiteSpace(*s)) s++;

				if (strnicmp(s, "xml:namespace", 13) == 0)
				{
					// Ignore Outlook generated HTML tag
					char *e = strchr(s, '/');
					while (e)
					{
						if (e[1] == '>'
							||
							(e[1] == '?' && e[2] == '>'))
						{
							if (e[1] == '?')
								s = e + 3;
							else
								s = e + 2;
							break;
						}

						e = strchr(e + 1, '/');
					}

					if (!e)
						LgiAssert(0);
				}
				else
				{
					char *Start = s;
					while (*s && (!(s[0] == '?' && s[1] == '>')))
					{
						if (strchr("\'\"", *s))
						{
							char d = *s++;
							s = strchr(s, d);
							if (s) s++;
							else break;
						}
						else s++;
					}

					if (s)
					{
						if (s[0] == '?' &&
							s[1] == '>' &&
							Html->Environment)
						{
							char *e = s - 1;
							while (e > Start && IsWhiteSpace(*e)) e--;
							e++;
							char *Code = NewStr(Start, e - Start);
							if (Code)
							{
								char *Result = Html->Environment->OnDynamicContent(Code);
								if (Result)
								{
									char *p = Result;
									do
									{
										GTag *c = new GTag(Html, this);
										if (c)
										{
											p = c->ParseHtml(p, Depth + 1, InPreTag);
										}
										else break;
									}
									while (ValidStr(p));

									DeleteArray(Result);
								}

								DeleteArray(Code);
							}

							s += 2;
						}
					}
				}
			}
			else if (s[1] == '!' &&
					s[2] == '-' &&
					s[3] == '-')
			{
				// Comment
				s = strstr(s, "-->");
				if (s) s += 3;
			}
			else if (s[1] == '!' &&
					s[2] == '[')
			{
				// Parse conditional...
				char *StartTag = s;
				s += 3;
				char *Cond = 0;
				s = ParseName(s, &Cond);
				if (!Cond)
				{
					while (*s && *s != ']')
						s++;
					if (*s == ']') s++;
					if (*s == '>') s++;
					return s;
				}

				bool IsEndIf = false;
				if (!stricmp(Cond, "if"))
				{
					if (!IsFirst)
					{
						DeleteArray(Cond);
						s = StartTag;
						goto DoChildTag;
					}

					TagId = CONDITIONAL;
					SkipWhiteSpace(s);
					char *Start = s;
					while (*s && *s != ']')
						s++;
					Condition.Reset(NewStr(Start, s-Start));
					Tag = NewStr("[if]");
					Info = GetTagInfo(Tag);
					
					Display(Disp = DispNone);
				}
				else if (!stricmp(Cond, "endif"))
				{
					IsEndIf = true;
				}
				DeleteArray(Cond);
				if (*s == ']') s++;
				if (*s == '>') s++;
				if (IsEndIf)
					return s;
			}
			else if (s[1] == '!')
			{
				s += 2;
				s = strchr(s, '>');
				if (s)
					s++;
				else
					return NULL;
			}
			else if (IsAlpha(s[1]))
			{
				// Start tag
				if (Parent && IsFirst)
				{
					// My tag
					s = ParseName(++s, &Tag);
					if (!Tag)
					{
					    if (BackOut)
						    *BackOut = true;
						return s;
					}

					bool TagClosed = false;
					s = ParsePropList(s, this, TagClosed);

					if (stricmp("th", Tag) == 0)
					{
						DeleteArray(Tag);
						Tag = NewStr("td");
					}
					
					Info = GetTagInfo(Tag);
					if (Info)
					{
						TagId = Info->Id;
						Disp = TestFlag(Info->Flags, TI_BLOCK) || (Tag && Tag[0] == '!') ? DispBlock : DispInline;
						if (TagId == TAG_PRE)
						{
							InPreTag = true;
						}
					}

					if (IsBlock(Disp) || TagId == TAG_BR)
					{
						SkipNonDisplay(s);
					}

					switch (TagId)
					{
						case TAG_SCRIPT:
						{
							char *End = stristr(s, "</script>");
							if (End)
							{
								if (Html->Environment)
								{
									*End = 0;
									const char *Lang = 0, *Type = 0;
									Get("language", Lang);
									Get("type", Type);
									Html->Environment->OnCompileScript(s, Lang, Type);
									*End = '<';
								}

								s = End;
							}
							break;
						}
						case TAG_TABLE:
						{
							if (Parent->TagId == TAG_TABLE)
							{
								// Um no...
								if (BackOut)
								{
									GTag *l = Html->OpenTags.Last();
									if (l && l->TagId == TAG_TABLE)
									{
										Html->CloseTag(l);
									}

									*BackOut = true;
									return StartTag;
								}
							}
							break;
						}
						case TAG_IFRAME:
						{
							const char *Src;
							if (Get("src", Src))
							{
								GDocumentEnv::LoadJob *j = Html->Environment->NewJob();
								if (j)
								{
									LgiAssert(Html);
									j->Uri.Reset(NewStr(Src));
									j->View = Html;
									j->UserData = this;
									j->UserUid = Html->d->DocumentUid;

									GDocumentEnv::LoadType Result = Html->Environment->GetContent(j);
									if (Result == GDocumentEnv::LoadImmediate)
									{
										if (j->Stream)
										{
											uint64 Len = j->Stream->GetSize();
											if (Len > 0)
											{
												GAutoString a(new char[Len+1]);
												int r = j->Stream->Read(a, Len);
												a[r] = 0;
												
												GTag *Child = new GTag(Html, this);
												if (Child)
												{
													bool BackOut = false;
													Child->ParseHtml(a, Depth + 1, false, &BackOut);
												}
											}
										}
									}
									DeleteObj(j);
								}
							}
							break;
						}
					}
					
					SetStyle();

					if (TagId == TAG_STYLE)
					{
						char *End = stristr(s, "</style>");
						if (End)
						{
							char *Css = NewStr(s, End - s);
							if (Css)
							{
								Html->AddCss(Css);
							}

							s = End;
						}
						else
						{
							// wtf?
						}
					}

					/* FIXME???
					if (TagId == TAG_P)
					{
						GTag *p;
						if (p = Html->GetOpenTag("p"))
						{
							return s;
						}
					}
					*/

					if (TagClosed || Info->NeverCloses())
					{
						return s;
					}

					if (Info->ReattachTo)
					{
						GToken T(Info->ReattachTo, ",");
						const char *Reattach = Info->ReattachTo;
						for (int i=0; i<T.Length(); i++)
						{
							if (Parent->Tag &&
								stricmp(Parent->Tag, T[i]) == 0)
							{
								Reattach = 0;
								break;
							}
						}

						if (Reattach)
						{
							if (TagId == TAG_HEAD)
							{
								// Ignore it..
								return s;
							}
							else
							{
								GTag *Last = 0;
								for (GTag *t=Html->OpenTags.Last(); t; t=Html->OpenTags.Prev())
								{
									if (t->Tag)
									{
										if (stricmp(t->Tag, Tag) == 0)
										{
											Html->CloseTag(t);
											t = Html->OpenTags.Current();
											if (!t) t = Html->OpenTags.Last();
										}

										if (t && t->Tag && stricmp(t->Tag, Reattach) == 0)
										{
											Last = t;
											break;
										}
									}

									if (!t || t->TagId == TAG_TABLE)
										break;
								}

								if (Last)
								{
									Last->Attach(this);
								}
							}
						}
					}

					Html->OpenTags.Insert(this);
				}
				else
				{
					// Child tag
					DoChildTag:
					GTag *c = new GTag(Html, this);
					if (c)
					{
						bool BackOut = false;

						s = c->ParseHtml(s, Depth + 1, InPreTag, &BackOut);
						if (BackOut)
						{
							c->Detach();
							DeleteObj(c);
							return s;
						}
						else if (IsBlock(c->Disp))
						{
							GTag *Last;
							while (c->Tags.Length())
							{
								Last = c->Tags.Last();

								if (Last->TagId == CONTENT &&
									!ValidStrW(Last->Text()))
								{
									Last->Detach();
									DeleteObj(Last);
								}
								else break;
							}
						}
					}
				}
			}
			else if (s[1] == '/')
			{
				// End tag
				char *PreTag = s;
				s += 2;

				// This code segment detects out of order HTML tags
				// and skips them. If we didn't do this then the parser
				// would get stuck on a Tag which has already been closed
				// and would return to the very top of the recursion.
				//
				// e.g.
				//		<font>
				//			<b>
				//			</font>
				//		</b>
				char *EndBracket = strchr(s, '>');
				if (EndBracket)
				{
					char *e = EndBracket;
					while (e > s && strchr(WhiteSpace, e[-1]))
						e--;
					char Prev = *e;
					*e = 0;
					GTag *Open = Html->GetOpenTag(s);
					*e = Prev;
					
					if (Open)
					{
						Open->WasClosed = true;
					}
					else
					{
						s = EndBracket + 1;
						continue;
					}
				}
				else
				{
					s += strlen(s);
					continue;
				}

				if (Tag)
				{
					// Compare against our tag
					char *t = Tag;
					while (*s && *t && toupper(*s) == toupper(*t))
					{
						s++;
						t++;
					}

					SkipWhiteSpace(s);

					if (*s == '>')
					{
						GTag *t;
						while (t = Html->OpenTags.Last())
						{
							Html->CloseTag(t);
							if (t == this)
							{
								break;
							}
						}
						s++;

						if (IsBlock(Disp) || TagId == TAG_BR)
						{
							SkipNonDisplay(s);
						}

						if (Parent)
						{
							return s;
						}
					}
				}
				else
					LgiAssert(!"This should not happen?");

				if (Parent)
				{
					return PreTag;
				}
			}
			else
			{
				goto PlainText;
			}
		}
		else if (*s)
		{
			// Text child
			PlainText:
			char *n = NextTag(s);
			int Len = n ? n - s : strlen(s);
			GAutoWString WStr(CleanText(s, Len, true, InPreTag));
			if (WStr && *WStr)
			{
				// This loop processes the text into lengths that need different treatment
				enum TxtClass
				{
					TxtNone,
					TxtEmoji,
					TxtEol,
					TxtNull,
				};

				char16 *Start = WStr;
				GTag *Child;
				for (char16 *c = WStr; true; c++)
				{
					TxtClass Cls = TxtNone;
					if (Html->d->DecodeEmoji && *c >= EMOJI_START && *c <= EMOJI_END)
						Cls = TxtEmoji;
					else if (InPreTag && *c == '\n')
						Cls = TxtEol;
					else if (!*c)
						Cls = TxtNull;

					if (Cls)
					{
						if (c > Start)
						{
							// Emit the text before the point of interest...
							GAutoWString Cur;
							if (Start == WStr && !*c)
							{
								// Whole string
								Cur = WStr;
							}
							else
							{
								// Sub-string
								Cur.Reset(NewStrW(Start, c - Start));
							}

							if (Tags.Length() == 0 &&
								(!Info || !Info->NoText()) &&
								!Text())
							{
								Text(Cur.Release());
							}
							else if (Child = new GTag(Html, this))
							{
								Child->Text(Cur.Release());
							}
						}

						// Now process the text of interest...
						if (Cls == TxtEmoji)
						{
							// Emit the emoji image
							GTag *img = new GTag(Html, this);
							if (img)
							{
								img->Tag = NewStr("img");
								if (img->Info = GetTagInfo(img->Tag))
									img->TagId = img->Info->Id;

								GRect rc;
								EMOJI_CH2LOC(*c, rc);

								img->Set("src", Html->d->EmojiImg);

								char css[256];
								sprintf(css, "x-rect: rect(%i,%i,%i,%i);", rc.y1, rc.x2, rc.y2, rc.x1);
								img->Set("style", css);
								img->SetStyle();
							}
							Start = c + 1;
						}
						else if (Cls == TxtEol)
						{
							// Emit the <br> tag
							GTag *br = new GTag(Html, this);
							if (br)
							{
								br->Tag = NewStr("br");
								if (br->Info = GetTagInfo(br->Tag))
									br->TagId = br->Info->Id;
							}
							Start = c + 1;
						}
					}
					
					// Check for the end of string...
					if (!*c)
						break;
				}

			}

			s = n;
		}

		IsFirst = false;
	}

	#if CRASH_TRACE
	LgiTrace("::ParseHtml end\n");
	#endif

	return 0;
}

bool GTag::OnUnhandledColor(GCss::ColorDef *def, const char *&s)
{
	const char *e = s;
	while (*e && (IsText(*e) || *e == '_'))
		e++;

	char tmp[256];
	int len = e - s;
	memcpy(tmp, s, len);
	tmp[len] = 0;
	int m = GHtmlStatic::Inst->ColourMap.Find(tmp);
	s = e;

	if (m >= 0)
	{
		def->Type = GCss::ColorRgb;
		def->Rgb32 = Rgb24To32(m);
		return true;
	}

	return false;
}

void GTag::ZeroTableElements()
{
	if (TagId == TAG_TABLE ||
		TagId == TAG_TR ||
		TagId == TAG_TD)
	{
		Size.x = 0;
		Size.y = 0;
		MinContent = 0;
		MaxContent = 0;

		for (GTag *t = Tags.First(); t; t = Tags.Next())
		{
			t->ZeroTableElements();
		}
	}
}

GdcPt2 GTag::GetTableSize()
{
	GdcPt2 s(0, 0);
	
	if (Cells)
	{
		Cells->GetSize(s.x, s.y);
	}

	return s;
}

GTag *GTag::GetTableCell(int x, int y)
{
	GTag *t = this;
	while (	t &&
			!t->Cells &&
			t->Parent)
	{
		t = t->Parent;
	}
	
	if (t && t->Cells)
	{
		return t->Cells->Get(x, y);
	}

	return 0;
}

// This function gets the largest and smallest peice of content
// in this cell and all it's children.
bool GTag::GetWidthMetrics(uint16 &Min, uint16 &Max)
{
	bool Status = true;

	// Break the text into words and measure...
	if (Text())
	{
		int MinContent = 0;
		int MaxContent = 0;
		
		GFont *f = GetFont();
		if (f)
		{
			for (char16 *s = Text(); s && *s; )
			{
				// Skip whitespace...
				while (*s && StrchrW(WhiteW, *s)) s++;
				
				// Find end of non-whitespace
				char16 *e = s;
				while (*e && !StrchrW(WhiteW, *e)) e++;
				
				// Find size of the word
				int Len = e - s;
				GDisplayString ds(f, s, Len);
				int X = ds.X();
				MinContent = max(MinContent, X);
				MaxContent += X + 4;
				
				// Move to the next word.
				s = (*e) ? e + 1 : 0;
			}
		}
		
		int Add = (int)(MarginLeft().Value +
					MarginRight().Value +
					PaddingLeft().Value +
					PaddingRight().Value);
		Min = max(Min, MinContent) + Add;
		Max =	max(Max, MaxContent) + Add;
	}

	// Specific tag handling?
	switch (TagId)
	{
		case TAG_IMG:
		{
			Len w = Width();
			if (w.IsValid())
			{
				int x = (int) w.Value;
				Min = max(Min, x);
				Max = max(Max, x);
			}
			else if (Image)
			{
				Min = Max = Image->X();
			}
			else
			{
				Min = max(Min, Size.x);
				Max = max(Max, Size.x);
			}
			break;
		}
		case TAG_TD:
		{
			Len w = Width();
			if (w.IsValid())
			{
				if (w.IsDynamic())
				{
					Min = max(Min, (int)w.Value);
					Max = max(Max, (int)w.Value);
				}
				else
				{
					Min = Max = (int)w.Value;
				}
			}
			break;
		}
		case TAG_TABLE:
		{
			Len w = Width();
			if (w.IsValid() && !w.IsDynamic())
			{
				// Fixed width table...
				Min = Max = (int)w.Value;
				return true;
			}
			else
			{
				GdcPt2 s;
				GCellStore c(this);
				c.GetSize(s.x, s.y);

				// Auto layout table
				GArray<int> ColMin, ColMax;
				for (int y=0; y<s.y; y++)
				{
					for (int x=0; x<s.x;)
					{
						GTag *t = c.Get(x, y);
						if (t)
						{
							uint16 a = 0, b = 0;
							if (t->GetWidthMetrics(a, b))
							{
								ColMin[x] = max(ColMin[x], a);
								ColMax[x] = max(ColMax[x], b);
							}
							
							x += t->Span.x;
						}
						else break;
					}
				}
				
				int MinSum = 0, MaxSum = 0;
				for (int i=0; i<s.x; i++)
				{
					MinSum += ColMin[i];
					MaxSum += ColMax[i];
				}
				
				Min = max(Min, MinSum);
				Max = max(Max, MaxSum);
				return true;
			}
			break;
		}
	}

	GTag *c;
	if (c = Tags.First())
	{
		uint16 Width = 0;
		for (; c; c = Tags.Next())
		{
			uint16 x = 0;
			Status &= c->GetWidthMetrics(Min, x);
			if (c->TagId == TAG_BR)
			{
				Max = max(Max, Width);
				Width = 0;
			}
			else
			{
				Width += x;
			}
		}
		Max = max(Max, Width);
	}

	switch (TagId)
	{
		case TAG_TD:
		{
			int Add = (int) (PaddingLeft().Value + PaddingRight().Value);
			Min += Add;
			Max += Add;
			break;
		}
	}
	
	return Status;
}

static void DistributeSize(GArray<int> &a, int Start, int Span, int Size, int Border)
{
	// Calculate the current size of the cells
	int Cur = -Border;
	for (int i=0; i<Span; i++)
	{
		Cur += a[Start+i] + Border;
	}

	// Is there more to allocate?
	if (Cur < Size)
	{
		// Distribute size amongst the rows
		int Needs = Size - Cur;
		int Part = Needs / Span;
		for (int k=0; k<Span; k++)
		{
			int Add = k < Span - 1 ? Part : Needs;
			a[Start+k] = a[Start+k] + Add;
			Needs -= Add;
		}
	}
}

template <class T>
T Sum(GArray<T> &a)
{
	T s = 0;
	for (int i=0; i<a.Length(); i++)
		s += a[i];
	return s;
}

void GTag::LayoutTable(GFlowRegion *f)
{
	GdcPt2 s;

	if (!Cells)
	{
		Cells = new GCellStore(this);
		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Cells && Debug)
			Cells->Dump();
		#endif
	}
	if (Cells)
	{
		Cells->GetSize(s.x, s.y);
		if (s.x == 0 || s.y == 0)
		{
			return;
		}

		ZeroTableElements();
		Len BdrSpacing = BorderSpacing();
		int CellSpacing = BdrSpacing.IsValid() ? (int)BdrSpacing.Value : 0;
		GCss::Len Wid = Width();
		int AvailableX = f->ResolveX(Wid, Font, false);
		
		if (MaxWidth().IsValid())
		{
		    int m = f->ResolveX(MaxWidth(), Font, false);
		    if (m < AvailableX)
		        AvailableX = m;
		}
		
		GCss::Len Border = BorderLeft();
		int BorderX1 = Border.IsValid() ? f->ResolveX(Border, Font, false) : 0;
		Border = BorderRight();
		int BorderX2 = Border.IsValid() ? f->ResolveX(Border, Font, false) : 0;
		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Debug)
			LgiTrace("AvailableX=%i, BorderX1=%i, BorderX2=%i\n", AvailableX, BorderX1, BorderX2);
		#endif

		// The col and row sizes
		GArray<int> MinCol, MaxCol, MaxRow;
		GArray<bool> FixedCol;
		
		// Size detection pass
		int y;
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						Len Wid = t->Width();
						if (!t->Width().IsDynamic() &&
							!t->MinContent &&
							!t->MaxContent)
						{
							if (FixedCol.Length() < x)
							{
								FixedCol[x] = false;
							}
							
							if (t->Width().IsValid())
							{
								t->MinContent = t->MaxContent = f->ResolveX(t->Width(), GetFont(), false);
								FixedCol[x] = true;
							}
							else if (!t->GetWidthMetrics(t->MinContent, t->MaxContent))
							{
								t->MinContent = 16;
								t->MaxContent = 16;
							}
							
							#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
							if (Debug)
								LgiTrace("Content[%i,%i] min=%i max=%i\n", x, y, t->MinContent, t->MaxContent);
							#endif
						}

						if (t->Span.x == 1)
						{
							MinCol[x] = max(MinCol[x], t->MinContent);
							MaxCol[x] = max(MaxCol[x], t->MaxContent);
						}
					}
					
					x += t->Span.x;
				}
				else break;
			}
		}
		
		// How much space used so far?
		int TotalX = BorderX1 + BorderX2 + CellSpacing;
		int x;
		for (x=0; x<s.x; x++)
		{
			TotalX += MinCol[x] + CellSpacing;
		}

		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Debug)
			LgiTrace("Detect: TotalX=%i\n", TotalX);
		#endif
		
		// Process dynamic width cells
		GArray<float> Percents;
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						if (t->Width().IsDynamic() &&
							!t->MinContent &&
							!t->MaxContent)
						{	
							Len w = t->Width();
							if (w.Type == LenPercent)
							{
								Percents[t->Cell.x] = max(w.Value, Percents[t->Cell.x]);
							}
							
							float Total = Sum<float>(Percents);
							if (Total > 100.0)
							{
								// Yarrrrh. The web be full of incongruity.
								float Sub = Total - 100.0;
								Percents[Percents.Length()-1] -= Sub;

								char p[32];
								const char *s = p;
								sprintf(p, "%.1f%%", Percents[Percents.Length()-1]);
								t->Width().Parse(s);
							}

							t->GetWidthMetrics(t->MinContent, t->MaxContent);
							
							int x = w.IsValid() ? f->ResolveX(w, Font, false) : 0;
							//t->MinContent = max(x, t->MinContent);
							t->MaxContent = max(x, t->MaxContent);
							
							#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
							if (Debug)
								LgiTrace("DynWidth [%i,%i] = %i->%i\n", t->Cell.x, t->Cell.y, t->MinContent, t->MaxContent);
							#endif
						}
						else
						{
							uint16 Min = t->MinContent;
							uint16 Max = t->MaxContent;
							t->GetWidthMetrics(Min, Max);
							if (Min > t->MinContent)
								t->MinContent = Min;
							if (Min > t->MaxContent)
								t->MaxContent = Min;
						}

						if (t->Span.x == 1)
						{
							MinCol[x] = max(MinCol[x], t->MinContent);
							MaxCol[x] = max(MaxCol[x], t->MaxContent);
						}
					}
					
					x += t->Span.x;
				}
				else break;
			}
		}

		TotalX = BorderX1 + BorderX2 + CellSpacing;
		for (x=0; x<s.x; x++)
		{
			TotalX += MinCol[x] + CellSpacing;
		}

		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Debug)
			LgiTrace("Dynamic: TotalX=%i\n", TotalX);
		
		#define DumpCols() \
		if (Debug) \
		{ \
			LgiTrace("%s:%i - Columns TotalX=%i AvailableX=%i\n", _FL, TotalX, AvailableX); \
			for (int i=0; i<MinCol.Length(); i++) \
			{ \
				LgiTrace("\t[%i] = %i/%i\n", i, MinCol[i], MaxCol[i]); \
			} \
		}

		DumpCols();
		#else
		#define DumpCols()
		#endif

		// Process spanned cells
		if (TotalX < AvailableX)
		{
			for (y=0; y<s.y; y++)
			{
				for (int x=0; x<s.x; )
				{
					GTag *t = Cells->Get(x, y);
					if (t && t->Cell.x == x && t->Cell.y == y)
					{
						if (t->Span.x > 1 || t->Span.y > 1)
						{
							int i;
							int ColMin = -CellSpacing;
							int ColMax = -CellSpacing;
							for (i=0; i<t->Span.x; i++)
							{
								ColMin += MinCol[x + i] + CellSpacing;
								ColMax += MaxCol[x + i] + CellSpacing;
							}

							// Generate an array of unfixed column indexes
							GArray<int> Unfixed;
							for (i = 0; i < t->Span.x; i++)
							{
								if (!FixedCol[x+i])
								{
									Unfixed[Unfixed.Length()] = x + i;
								}
							}
							
							// Bump out minimums
							if (ColMin < t->MinContent)
							{
								int Total = t->MinContent - ColMin;

								if (Unfixed.Length())
								{
									int Add = Total / Unfixed.Length();
									for (i=0; i<Unfixed.Length(); i++)
									{
										int a = i ? Add : Total - (Add * (Unfixed.Length() - 1));
										
										MinCol[Unfixed[i]] = MinCol[Unfixed[i]] + a;
										TotalX += a;
									}
								}
								else
								{
									int Add = Total / t->Span.x;
									for (i=0; i<t->Span.x; i++)
									{
										int a = i ? Add : Total - (Add * (t->Span.x - 1));
										MinCol[x + i] = MinCol[x + i] + a;
										TotalX += a;
									}
								}
							}

							// Bump out maximums
							if (t->MaxContent > ColMin)
							{
								int MaxAdd = t->MaxContent - ColMin;
								int MaxAvail = AvailableX > TotalX ? AvailableX - TotalX : 0;
								int Total = min(MaxAdd, MaxAvail);
								
								/*
								printf("bumpmax %i,%i mincol=%i add=%i avail=%i total=%i Unfixed=%i\n",
									x, y,
									ColMin,
									MaxAdd,
									MaxAvail,
									Total,
									Unfixed.Length());
								*/

								if (Unfixed.Length())
								{
									int Add = Total / Unfixed.Length();
									for (i=0; i<Unfixed.Length(); i++)
									{
										int a = i ? Add : Total - (Add * (Unfixed.Length() - 1));

										MinCol[Unfixed[i]] = MinCol[Unfixed[i]] + a;
										TotalX += a;
									}
								}
								else
								{
									int Add = Total / t->Span.x;
									for (i=0; i<t->Span.x; i++)
									{
										int a = i ? Add : Total - (Add * (t->Span.x - 1));
										MinCol[x + i] = MinCol[x + i] + a;
										TotalX += a;
									}
								}
							}
						}

						x += t->Span.x;
					}
					else break;
				}
			}
		}

		DumpCols();
		
		// Deallocate space if overused
		if (TotalX > AvailableX)
		{
			// Take some from the largest column
			int Largest = 0;
			for (int i=0; i<s.x; i++)
			{
				if (MinCol[i] > MinCol[Largest])
				{
					Largest = i;
				}
			}
			int Take = TotalX - AvailableX;
			if (Take < MinCol[Largest])
			{
				MinCol[Largest] = MinCol[Largest] - Take;
				TotalX -= Take;
			}
		}

		DumpCols();

		// Allocate any unused but available space...
		if (TotalX < AvailableX)
		{
			// Some available space still
			// printf("Alloc unused space, Total=%i Available=%i\n", TotalX, AvailableX);

			// Allocate to columns that fully fit
			int LaidOut = 0;
			for (int x=0; x<s.x; x++)
			{
				int NeedsX = MaxCol[x] - MinCol[x];
				// printf("Max-Min: %i-%i = %i\n", MaxCol[x], MinCol[x], NeedsX);
				if (NeedsX > 0 && NeedsX < AvailableX - TotalX)
				{
					MinCol[x] = MinCol[x] + NeedsX;
					TotalX += NeedsX;
					LaidOut++;
				}
			}

			// Allocate to columns that still need room
			int RemainingCols = s.x - LaidOut;
			if (RemainingCols > 0)
			{
				int SpacePerCol = (AvailableX - TotalX) / RemainingCols;
				if (SpacePerCol > 0)
				{
					for (int x=0; x<s.x; x++)
					{
						int NeedsX = MaxCol[x] - MinCol[x];
						if (NeedsX > 0)
						{
							MinCol[x] = MinCol[x] + SpacePerCol;
							TotalX += SpacePerCol;
						}
					}
				}
			}
			
			#if 1
			if (TotalX < AvailableX)
			#else
			if (TotalX < AvailableX && Width().IsValid())
			#endif
			{
				// Force allocation of space

				// Add some to the largest column
				int Largest = 0;
				for (int i=0; i<s.x; i++)
				{
					if (MinCol[i] > MinCol[Largest])
					{
						Largest = i;
					}
				}
				int Add = AvailableX - TotalX;
				MinCol[Largest] = MinCol[Largest] + Add;
				TotalX += Add;
			}
		}

		DumpCols();

		// Allocate remaining space if explicit table width
		if (Width().IsValid() &&
			TotalX < AvailableX)
		{
			int Add = (AvailableX - TotalX) / s.x;
			for (int x=0; x<s.x; x++)
			{
				MinCol[x] = MinCol[x] + Add;
				TotalX += Add;
			}
		}

		DumpCols();
		
		// Layout cell contents to get the height of all the cells
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						GRect Box(0, 0, -CellSpacing, 0);
						for (int i=0; i<t->Span.x; i++)
						{
							Box.x2 += MinCol[x+i] + CellSpacing;
						}
						
						GFlowRegion r(Html, Box);
						int Rx = r.X();
						t->OnFlow(&r);
						// t->Size.y += t->PaddingBottom().Value;
						
						if (t->Height().IsValid() &&
							t->Height().Type != LenPercent)
						{
							int h = f->ResolveY(t->Height(), Font, false);
							t->Size.y = max(h, t->Size.y);

							DistributeSize(MaxRow, y, t->Span.y, t->Size.y, CellSpacing);
						}

						#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
						if (Debug)
							LgiTrace("[%i,%i]=%i,%i Rx=%i\n", t->Cell.x, t->Cell.y, t->Size.x, t->Size.y, Rx);
						#endif
					}

					x += t->Span.x;
				}
				else break;
			}
		}

		// Calculate row height
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						if (!(t->Height().IsValid() && t->Height().Type != LenPercent))
						{
							DistributeSize(MaxRow, y, t->Span.y, t->Size.y, CellSpacing);
						}
					}

					x += t->Span.x;
				}
				else break;
			}			
		}
		
		// Cell positioning
		int LeftMargin = (int) (BorderLeft().Value + CellSpacing);
		int Cx = LeftMargin;
		int Cy = (int) (BorderTop().Value + CellSpacing);
		
		for (y=0; y<s.y; y++)
		{
			GTag *Prev = 0;
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (!t && Prev)
				{
					// Add missing cell
					GTag *Row = Prev->Parent;
					if (Row && Row->TagId == TAG_TR)
					{
						t = new GTag(Html, Row);
						if (t)
						{
							t->TagId = TAG_TD;
							t->Tag = NewStr("td");
							t->Info = GetTagInfo("td");
							t->Cell.x = x;
							t->Cell.y = y;
							t->Span.x = 1;
							t->Span.y = 1;
							t->BackgroundColor(ColorDef(DefaultMissingCellColour));

							Cells->Set(this);
						}
						else break;
					}
					else break;
				}
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						t->Pos.x = Cx;
						t->Pos.y = Cy;
						t->Size.x = -CellSpacing;
						for (int i=0; i<t->Span.x; i++)
						{
							int w = MinCol[x + i] + CellSpacing;
							t->Size.x += w;
							Cx += w;
						}
						t->Size.y = -CellSpacing;						
						for (int n=0; n<t->Span.y; n++)
						{
							t->Size.y += MaxRow[y+n] + CellSpacing;
						}
						
						Size.x = max(Cx + (int)BorderRight().Value, Size.x);
					}
					else
					{
						Cx += t->Size.x + CellSpacing;
					}
					
					x += t->Span.x;
				}
				else break;
				Prev = t;
			}
			
			Cx = LeftMargin;
			Cy += MaxRow[y] + CellSpacing;
		}

		DumpCols();

		switch (XAlign ? XAlign : Parent->GetAlign(true))
		{
			case AlignCenter:
			{
				int Ox = (f->X()-Size.x) >> 1;
				Pos.x = f->x1 + max(Ox, 0);
				break;
			}
			case AlignRight:
			{
				Pos.x = f->x2 - Size.x;
				break;
			}
			default:
			{
				Pos.x = f->x1;
				break;
			}
		}
		Pos.y = f->y1;

		Size.y = Cy + (int)BorderBottom().Value;
	}
}

GRect GTag::ChildBounds()
{
	GRect b(0, 0, -1, -1);

	GTag *t = Tags.First();
	if (t)
	{
		b = t->GetRect();
		while (t = Tags.Next())
		{
			GRect c = t->GetRect();
			b.Union(&c);
		}
	}

	return b;
}

int GTag::AbsX()
{
	int a = 0;
	for (GTag *t=this; t; t=t->Parent)
	{
		a += t->Pos.x;
	}
	return a;
}

int GTag::AbsY()
{
	int a = 0;
	for (GTag *t=this; t; t=t->Parent)
	{
		a += t->Pos.y;
	}
	return a;
}

void GTag::SetSize(GdcPt2 &s)
{
	Size = s;
}

GArea::~GArea()
{
	DeleteObjects();
}

GRect GArea::Bounds()
{
	GRect n(0, 0, -1, -1);

	for (int i=0; i<Length(); i++)
	{
		GFlowRect *r = (*this)[i];
		if (r)
		{
			if (i)
				n.Union(r);
			else
				n = r;
		}
	}
	
	return n;
}

GRect *GArea::TopRect(GRegion *c)
{
	GRect *Top = 0;
	
	for (int i=0; i<c->Length(); i++)
	{
		GRect *r = (*c)[i];
		if (!Top || (r && (r->y1 < Top->y1)))
		{
			Top = r;
		}
	}

	return Top;
}

void GArea::FlowText(GTag *Tag, GFlowRegion *Flow, GFont *Font, char16 *Text, GCss::LengthType Align)
{
	if (!Flow || !Text || !Font)
		return;

	char16 *Start = Text;
	int FullLen = StrlenW(Text);

	#if 1
	if (!Tag->Html->GetReadOnly() && !*Text)
	{
		GFlowRect *Tr = new GFlowRect;
		Tr->Tag = Tag;
		Tr->Text = Text;
		Tr->x1 = Tr->x2 = Flow->cx;
		Tr->y1 = Flow->y1;
		Tr->y2 = Tr->y1 + Font->GetHeight();
		Flow->y2 = max(Flow->y2, Tr->y2+1);
		Add(Tr);
		Flow->Insert(Tr);
		return;				
	}
	#endif

	while (*Text)
	{
		GFlowRect *Tr = new GFlowRect;
		if (!Tr)
			break;

		Tr->Tag = Tag;
	Restart:
		Tr->x1 = Flow->cx;
		Tr->y1 = Flow->y1;

		#if 1 // I removed this at one stage but forget why.
		
		// Remove white space at start of line.
		if (Flow->x1 == Flow->cx && *Text == ' ')
		{
			Text++;
			if (!*Text)
			{
				DeleteObj(Tr);
				break;
			}
		}
		#endif
		
		Tr->Text = Text;

		GDisplayString ds(Font, Text, min(1024, FullLen - (Text-Start)));
		int Chars = ds.CharAt(Flow->X());
		bool Wrap = false;
		if (Text[Chars])
		{
			// Word wrap

			// Seek back to the nearest break opportunity
			int n = Chars;
			while (n > 0 && !StrchrW(WhiteW, Text[n]))
				n--;

			if (n == 0)
			{
				if (Flow->x1 == Flow->cx)
				{
					// Already started from the margin and it's too long to 
					// fit across the entire page, just let it hang off the right edge.

					// Seek to the end of the word
					for (Tr->Len = Chars; Text[Tr->Len] && !StrchrW(WhiteW, Text[Tr->Len]); Tr->Len++)
					{
					}

					// Wrap...
					if (*Text == ' ') Text++;
				}
				else
				{
					// Not at the start of the margin
					Flow->FinishLine();							
					goto Restart;
				}
			}
			else
			{
				Tr->Len = n;
				LgiAssert(Tr->Len > 0);
			}

			Wrap = true;
		}
		else
		{
			// Fits..
			Tr->Len = Chars;
			LgiAssert(Tr->Len > 0);
		}

		GDisplayString ds2(Font, Tr->Text, Tr->Len);
		Tr->x2 = ds2.X();
		Tr->y2 = ds2.Y();
		if (Wrap)
		{
			Flow->cx = Flow->x1;
			Flow->y1 += Tr->y2 + 1;
			Tr->x2 = Flow->x2 - Tag->RelX();
		}
		else
		{
			Tr->x2 += Tr->x1 - 1;
			Flow->cx = Tr->x2 + 1;
		}
		Tr->y2 += Tr->y1;
		Flow->y2 = max(Flow->y2, Tr->y2 + 1);
		
		Add(Tr);
		Flow->Insert(Tr);
		
		Text += Tr->Len;
		if (Wrap)
		{
			while (*Text == ' ')
				Text++;
		}

		Tag->Size.x = max(Tag->Size.x, Tr->x2);
		Tag->Size.y = max(Tag->Size.y, Tr->y2);
		Flow->max_cx = max(Flow->max_cx, Tr->x2);

		if (Tr->Len == 0)
			break;
	}
}

void GTag::OnFlow(GFlowRegion *InputFlow)
{
	if (Disp == DispNone)
		return;

	GFlowRegion *Flow = InputFlow;
	GFont *f = GetFont();
	GFlowRegion Local(Html);
	bool Restart = true;
	int BlockFlowWidth = 0;
	const char *ImgAltText = NULL;
	int BlockInlineX[3];

	Size.x = 0;
	Size.y = 0;

	switch (TagId)
	{
		case TAG_IFRAME:
		{
			GFlowRegion Temp = *Flow;
			Flow->EndBlock();
			Flow->Indent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);

			// Flow children
			for (GTag *t=Tags.First(); t; t=Tags.Next())
			{
				t->OnFlow(&Temp);

				if (TagId == TAG_TR)
				{
					Temp.x2 -= min(t->Size.x, Temp.X());
				}
			}

			Flow->Outdent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);
			BoundParents();
			return;
			break;
		}
		case TAG_IMG:
		{
			Restart = false;

			Pos.y = Flow->y1;
			
			if (Width().IsValid())
			{
				Size.x = Flow->ResolveX(Width(), GetFont(), false);
			}
			else if (Image)
			{
				Size.x = Image->X();
			}
			else if (Get("alt", ImgAltText) && ValidStr(ImgAltText))
			{
				GDisplayString a(Html->GetFont(), ImgAltText);
				Size.x = a.X() + 4;
			}
			else
			{
				Size.x = DefaultImgSize;
			}
			
			GCss::LengthType a = GetAlign(true);
			switch (a)
			{
				case AlignCenter:
				{
					int Fx = Flow->x2 - Flow->x1;
					Pos.x = Flow->x1 + ((Fx - Size.x) / 2);
					break;
				}
				case AlignRight:
				{
					Pos.x = Flow->x2 - Size.x;
					break;
				}
				default:
				{
					Pos.x = Flow->cx;
					break;
				}
			}
			break;
		}
		case TAG_HR:
		{
			Flow->FinishLine();
			
			Pos.x = Flow->x1;
			Pos.y = Flow->y1 + 7;
			Size.x = Flow->X();
			Size.y = 2;
			
			Flow->cx ++;
			Flow->y2 += 16;

			Flow->FinishLine();
			return;
			break;
		}
		case TAG_TABLE:
		{
			Flow->EndBlock();

			Flow->Indent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);

			LayoutTable(Flow);

			Flow->y1 += Size.y;
			Flow->y2 = Flow->y1;
			Flow->cx = Flow->x1;

			Flow->Outdent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);
			BoundParents();
			return;
		}
	}

	#ifdef _DEBUG
	if (Debug)
	{
		int asd=0;
	}
	#endif
	
	int OldFlowMy = Flow->my;
	if (Disp == DispBlock || Disp == DispInlineBlock)
	{
		// This is a block level element, so end the previous non-block elements
		if (Disp == DispBlock)
		{		
			Flow->EndBlock();
		}
		
		BlockFlowWidth = Flow->X();
		if (TagId == TAG_P)
		{
			if (!OldFlowMy && Text())
			{
				Flow->FinishLine(true);
			}
		}

		// Indent the margin...
		Flow->Indent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);

		// Set the width if any
		if (Disp == DispBlock)
		{
			if (TagId != TAG_TD && Width().IsValid())
				Size.x = Flow->ResolveX(Width(), f, false);
			else
				Size.x = Flow->X();

			if (MaxWidth().IsValid())
			{
				int Px = Flow->ResolveX(MaxWidth(), GetFont(), false);
				if (Size.x > Px)
					Size.x = Px;
			}

			Pos.x = Flow->x1;
		}
		else
		{
			Size.x = Flow->X(); // Not correct but give maximum space
			Pos.x = Flow->cx;
		}
		Pos.y = Flow->y1;

		Flow->y1 -= Pos.y;
		Flow->y2 -= Pos.y;
			
		if (Disp == DispBlock)
		{
			Flow->x1 -= Pos.x;
			Flow->x2 = Flow->x1 + Size.x;
			Flow->cx -= Pos.x;

			Flow->Indent(f, GCss::BorderLeft(), GCss::BorderTop(), GCss::BorderRight(), GCss::BorderBottom(), false);
			Flow->Indent(f, PaddingLeft(), PaddingTop(), PaddingRight(), PaddingBottom(), false);
		}
		else
		{
			BlockInlineX[0] = Flow->x1;
			BlockInlineX[1] = Flow->cx;
			BlockInlineX[2] = Flow->x2;
			Flow->x1 = 0;
			Flow->x2 = Size.x;
			Flow->cx = 0;
			
			Flow->cx += Flow->ResolveX(BorderLeft(), GetFont(), false);
			Flow->y1 += Flow->ResolveY(BorderTop(), GetFont(), false);
			
			Flow->cx += Flow->ResolveX(PaddingLeft(), GetFont(), false);
			Flow->y1 += Flow->ResolveY(PaddingTop(), GetFont(), false);
		}
	}

	if (f)
	{
		// Clear the previous text layout...
		TextPos.DeleteObjects();
		
		if (TagId == TAG_LI)
		{
			// Insert the list marker
			if (!PreText())
			{
				GCss::ListStyleTypes s = Parent->ListStyleType();
				if (s == ListInherit)
				{
					if (Parent->TagId == TAG_OL)
						s = ListDecimal;
					else if (Parent->TagId == TAG_UL)
						s = ListDisc;
				}
				
				switch (s)
				{
					case ListDecimal:
					{
						int Index = Parent->Tags.IndexOf(this);
						char Txt[32];
						sprintf(Txt, "%i. ", Index + 1);
						PreText(LgiNewUtf8To16(Txt));
						break;
					}
					case ListDisc:
					{
						PreText(NewStrW(GHtmlListItem));
						break;
					}
				}
			}

			if (PreText())
				TextPos.FlowText(this, Flow, f, PreText(), AlignLeft);
		}

		if (Text())
		{
			// Flow in the rest of the text...
			TextPos.FlowText(this, Flow, f, Text(), GetAlign(true));
		}
	}

	// Flow children
	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->OnFlow(Flow);

		if (TagId == TAG_TR)
		{
			Flow->x2 -= min(t->Size.x, Flow->X());
		}
	}

	switch (TagId)
	{
		case TAG_SELECT:
		case TAG_INPUT:
		{
			if (Ctrl)
			{
				GRect r = Ctrl->GetPos();

				if (Width().IsValid())
					Size.x = Flow->ResolveX(Width(), GetFont(), false);
				else
					Size.x = r.X();
				if (Height().IsValid())
					Size.y = Flow->ResolveY(Height(), GetFont(), false);
				else
					Size.y = r.Y();
				
				if (Html->IsAttached() && !Ctrl->IsAttached())
					Ctrl->Attach(Html);
			}

			Flow->cx += Size.x;
			Flow->y2 = max(Flow->y2, Flow->y1 + Size.y - 1);
			break;
		}
		case TAG_IMG:
		{
			Len Ht = Height();
			if (Ht.IsValid() && Ht.Type != LenAuto)
			{
				if (Image)
					Size.y = Ht.ToPx(Image->Y(), GetFont());
				else
					Size.y = Flow->ResolveY(Ht, f, false);
			}
			else if (Image)
			{
				Size.y = Image->Y();
			}
			else if (ValidStr(ImgAltText))
			{
				Size.y = Html->GetFont()->GetHeight() + 4;
			}
			else
			{
				Size.y = DefaultImgSize;
			}

			Flow->cx += Size.x;
			Flow->y2 = max(Flow->y2, Flow->y1 + Size.y - 1);
			break;
		}
		case TAG_TR:
		{
			Flow->x2 = Flow->x1 + Local.X();
			break;
		}
		case TAG_BR:
		{
			int OldFlowY2 = Flow->y2;
			Flow->FinishLine();
			Size.y = Flow->y2 - OldFlowY2;
			Flow->y2 = max(Flow->y2, Flow->y1 + Size.y - 1);
			break;
		}
		/* This doesn't make sense....
		case TAG_TD:
		{
			Size.x = Flow->X();
			break;
		}
		*/
	}

	if (Disp == DispBlock || Disp == DispInlineBlock)
	{		
		GCss::Len Ht = Height();
		if (Ht.IsValid())
		{			
			int HtPx = Flow->ResolveY(Ht, GetFont(), false);
			if (HtPx > Flow->y2)
				Flow->y2 = HtPx;
		}

		if (Disp == DispBlock)
		{
			Flow->EndBlock();

			int OldFlowSize = Flow->x2 - Flow->x1 + 1;
			Flow->Outdent(f, PaddingLeft(), PaddingTop(), PaddingRight(), PaddingBottom(), false);
			Flow->Outdent(f, GCss::BorderLeft(), GCss::BorderTop(), GCss::BorderRight(), GCss::BorderBottom(), false);
			Flow->Outdent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);
			
			int NewFlowSize = Flow->x2 - Flow->x1 + 1;
			int Diff = NewFlowSize - OldFlowSize;
			if (Diff)
				Flow->max_cx += Diff;
			
			Size.y = Flow->y2;
			Flow->y1 = Flow->y2;
			Flow->x2 = Flow->x1 + BlockFlowWidth;

			// I dunno, there should be a better way... :-(
			if (MarginLeft().Type == LenAuto &&
				MarginRight().Type == LenAuto)
			{
				int OffX = (Flow->x2 - Flow->x1 - Size.x) >> 1;
				if (OffX > 0)
					Pos.x += OffX;
			}
		}
		else
		{
			Flow->cx += Flow->ResolveX(PaddingRight(), GetFont(), false);
			Flow->cx += Flow->ResolveX(BorderRight(), GetFont(), false);
			Size.x = Flow->cx;
			Flow->cx += Flow->ResolveX(MarginRight(), GetFont(), true);
			Flow->x1 = BlockInlineX[0] - Pos.x;
			Flow->cx = BlockInlineX[1] + Flow->cx - Pos.x;
			Flow->x2 = BlockInlineX[2] - Pos.x;

			if (Height().IsValid())
			{
				Size.y = Flow->ResolveY(Height(), GetFont(), false);
				int MarginY2 = Flow->ResolveX(MarginBottom(), GetFont(), true);
				Flow->y2 = max(Flow->y1 + Size.y + MarginY2 - 1, Flow->y2);
			}
			else
			{
				Flow->y2 += Flow->ResolveX(PaddingBottom(), GetFont(), false);
				Flow->y2 += Flow->ResolveX(BorderBottom(), GetFont(), false);
				Size.y = Flow->y2 - Flow->y1 + 1;
			}
		}
	}

	BoundParents();

	if (Restart)
	{
		Flow->x1 += Pos.x;
		Flow->x2 += Pos.x;
		Flow->cx += Pos.x;

		Flow->y1 += Pos.y;
		Flow->y2 += Pos.y;
	}
}

bool GTag::PeekTag(char *s, char *tag)
{
	bool Status = false;

	if (s && tag)
	{
		if (*s == '<')
		{
			char *t = 0;
			ParseName(++s, &t);
			if (t)
			{
				Status = stricmp(t, tag) == 0;
			}

			DeleteArray(t);
		}
	}

	return Status;
}

GTag *GTag::GetTable()
{
	GTag *t = 0;
	for (t=Parent; t && t->TagId != TAG_TABLE; t = t->Parent);
	return t;
}

void GTag::BoundParents()
{
	if (Parent)
	{
		for (GTag *n=this; n; n=n->Parent)
		{
			if (n->Parent)
			{
				if (n->Parent->TagId == TAG_IFRAME)
					break;
				n->Parent->Size.x = max(n->Parent->Size.x, n->Pos.x + n->Size.x);
				n->Parent->Size.y = max(n->Parent->Size.y, n->Pos.y + n->Size.y);
			}
		}
	}
}

struct DrawBorder
{
	GSurface *pDC;
	uint32 LineStyle;
	uint32 LineReset;
	uint32 OldStyle;

	DrawBorder(GSurface *pdc, GCss::BorderDef &d)
	{
		LineStyle = 0xffffffff;
		LineReset = 0x80000000;

		if (d.Style == GCss::BorderDotted)
		{
			switch ((int)d.Value)
			{
				case 2:
				{
					LineStyle = 0xcccccccc;
					break;
				}
				case 3:
				{
					LineStyle = 0xe38e38;
					LineReset = 0x800000;
					break;
				}
				case 4:
				{
					LineStyle = 0xf0f0f0f0;
					break;
				}
				case 5:
				{
					LineStyle = 0xf83e0;
					LineReset = 0x80000;
					break;
				}
				case 6:
				{
					LineStyle = 0xfc0fc0;
					LineReset = 0x800000;
					break;
				}
				case 7:
				{
					LineStyle = 0xfe03f80;
					LineReset = 0x8000000;
					break;
				}
				case 8:
				{
					LineStyle = 0xff00ff00;
					break;
				}
				case 9:
				{
					LineStyle = 0x3fe00;
					LineReset = 0x20000;
					break;
				}
				default:
				{
					LineStyle = 0xaaaaaaaa;
					break;
				}
			}
		}

		pDC = pdc;
		OldStyle = pDC->LineStyle();
	}

	~DrawBorder()
	{
		pDC->LineStyle(OldStyle);
	}
};

void GTag::OnPaintBorder(GSurface *pDC, GRect *Px)
{
	GArray<GRect> r;

	switch (Disp)
	{
		case DispInlineBlock:
		case DispBlock:
		{
			r[0].ZOff(Size.x-1, Size.y-1);
			break;
		}
		case DispInline:
		{
			for (int i=0; i<TextPos.Length(); i++)
			{
				r.New() = *(TextPos[i]);
			}
			break;
		}
	}

	if (Px)
		Px->ZOff(0, 0);

	for (int i=0; i<r.Length(); i++)
	{
		GRect &rc = r[i];
		
		BorderDef b;
		if ((b = BorderLeft()).IsValid())
		{
			pDC->Colour(b.Color.Rgb32, 32);
			DrawBorder db(pDC, b);
			for (int i=0; i<b.Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x1 + i, rc.y1, rc.x1 + i, rc.y2);
				if (Px)
					Px->x1++;
			}
		}
		if ((b = BorderTop()).IsValid())
		{
			pDC->Colour(b.Color.Rgb32, 32);
			DrawBorder db(pDC, b);
			for (int i=0; i<b.Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x1, rc.y1 + i, rc.x2, rc.y1 + i);
				if (Px)
					Px->y1++;
			}
		}
		if ((b = BorderRight()).IsValid())
		{
			pDC->Colour(b.Color.Rgb32, 32);
			DrawBorder db(pDC, b);
			for (int i=0; i<b.Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x2 - i, rc.y1, rc.x2 - i, rc.y2);
				if (Px)
					Px->x2++;
			}			
		}
		if ((b = BorderBottom()).IsValid())
		{
			pDC->Colour(b.Color.Rgb32, 32);
			DrawBorder db(pDC, b);
			for (int i=0; i<b.Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x1, rc.y2 - i, rc.x2, rc.y2 - i);
				if (Px)
					Px->y2++;
			}
		}
	}
}

static void FillRectWithImage(GSurface *pDC, GRect *r, GSurface *Image, GCss::RepeatType Repeat)
{
	int Px = 0, Py = 0;
	int Old = pDC->Op(GDC_ALPHA);

	switch (Repeat)
	{
		default:
		case GCss::RepeatBoth:
		{
			for (int y=0; y<r->Y(); y += Image->Y())
			{
				for (int x=0; x<r->X(); x += Image->X())
				{
					pDC->Blt(Px + x, Py + y, Image);
				}
			}
			break;
		}
		case GCss::RepeatX:
		{
			for (int x=0; x<r->X(); x += Image->X())
			{
				pDC->Blt(Px + x, Py, Image);
			}
			break;
		}
		case GCss::RepeatY:
		{
			for (int y=0; y<r->Y(); y += Image->Y())
			{
				pDC->Blt(Px, Py + y, Image);
			}
			break;
		}
		case GCss::RepeatNone:
		{
			pDC->Blt(Px, Py, Image);
			break;
		}
	}

	pDC->Op(Old);
}

void GTag::OnPaint(GSurface *pDC)
{
	if (Display() == DispNone) return;

	int Px, Py;
	pDC->GetOrigin(Px, Py);

	#ifdef _DEBUG
	if (Debug)
	{
		int asd=0;
	}
	#endif

	switch (TagId)
	{
		case TAG_INPUT:
		case TAG_SELECT:
		{
			if (Ctrl)
			{
				int Sx = 0, Sy = 0;
				int LineY = GetFont()->GetHeight();
				Html->GetScrollPos(Sx, Sy);
				Sx *= LineY;
				Sy *= LineY;
				
				GRect r(0, 0, Size.x-1, Size.y-1), Px;
				OnPaintBorder(pDC, &Px);
				if (!dynamic_cast<GButton*>(Ctrl))
				{
					r.x1 += Px.x1;
					r.y1 += Px.y1;
					r.x2 -= Px.x2;
					r.y2 -= Px.y2;
				}
				r.Offset(AbsX() - Sx, AbsY() - Sy);
				Ctrl->SetPos(r);
			}
			
			if (TagId == TAG_SELECT)
				return;
			break;
		}
		case TAG_BODY:
		{
			Selected = false;
			
			if (Image)
			{
				COLOUR b = GetBack();
				if (b != GT_TRANSPARENT)
				{
					pDC->Colour(b, 32);
					// pDC->Rectangle(Pos.x, Pos.y, Pos.x+Size.x, Pos.y+Size.y);
				}

				GRect r;
				r.ZOff(Size.x-1, Size.y-1);
				FillRectWithImage(pDC, &r, Image, BackgroundRepeat());
			}
			break;
		}
		case TAG_HEAD:
		{
			// Nothing under here to draw.
			return;
		}
		case TAG_HR:
		{
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(0, 0, Size.x - 1, Size.y - 1);
			break;
		}
		case TAG_TR:
		case TAG_TBODY:
		case TAG_META:
		{
			// Draws nothing...
			break;
		}
		case TAG_IMG:
		{
			GRect Clip(0, 0, Size.x-1, Size.y-1);
			pDC->ClipRgn(&Clip);
			
			if (Image)
			{
				if ((Width().IsValid() || Height().IsValid()) && !ImageResized)
				{
					if (Size.x != Image->X() ||
						Size.y != Image->Y())
					{
						ImageResized = true;
						GAutoPtr<GSurface> r(new GMemDC(Size.x, Size.y, Image->GetBits()));
						if (r)
						{
							ResampleDC(r, Image);
							Image = r;
						}
					}
				}
				int Old = pDC->Op(GDC_ALPHA);
				pDC->Blt(0, 0, Image);
				pDC->Op(Old);
			}
			else if (Size.x > 1 && Size.y > 1)
			{
				GRect b(0, 0, Size.x-1, Size.y-1);
				GColour Fill(GdcMixColour(LC_MED, LC_LIGHT, 0.2f), 24);
				GColour Border(LC_MED, 24);

				// Border
				pDC->Colour(Border);
				pDC->Box(&b);
				b.Size(1, 1);
				pDC->Box(&b);
				b.Size(1, 1);
				pDC->Colour(Fill);
				pDC->Rectangle(&b);

				const char *Alt;
				GColour Red(GdcMixColour(Rgb24(255, 0, 0), Fill.c24(), 0.3f), 24);
				if (Get("alt", Alt) && ValidStr(Alt))
				{
					GDisplayString Ds(Html->GetFont(), Alt);
					Html->GetFont()->Colour(Red, Fill);
					Ds.Draw(pDC, 2, 2, &b);
				}
				else if (Size.x >= 16 && Size.y >= 16)
				{
					// Red 'x'
					int Cx = b.x1 + (b.X()/2);
					int Cy = b.y1 + (b.Y()/2);
					GRect c(Cx-4, Cy-4, Cx+4, Cy+4);
					
					pDC->Colour(Red);
					pDC->Line(c.x1, c.y1, c.x2, c.y2);
					pDC->Line(c.x1, c.y2, c.x2, c.y1);
					pDC->Line(c.x1, c.y1 + 1, c.x2 - 1, c.y2);
					pDC->Line(c.x1 + 1, c.y1, c.x2, c.y2 - 1);
					pDC->Line(c.x1 + 1, c.y2, c.x2, c.y1 + 1);
					pDC->Line(c.x1, c.y2 - 1, c.x2 - 1, c.y1);
				}
			}
			
			pDC->ClipRgn(0);
			break;
		}
		case TAG_TABLE:
		{
			if (Html->Environment)
			{
				GCss::ImageDef Img = BackgroundImage();
				if (Img.Type >= ImageOwn)
				{
					GRect Clip(0, 0, Size.x-1, Size.y-1);
					pDC->ClipRgn(&Clip);
					
					GRect r;
					r.ZOff(Size.x-1, Size.y-1);
					FillRectWithImage(pDC, &r, Img.Img, BackgroundRepeat());
				}
			}

			OnPaintBorder(pDC);

			if (DefaultTableBorder != GT_TRANSPARENT)
			{
				GRect r((int)BorderLeft().Value,
				        (int)BorderTop().Value,
				        Size.x-(int)BorderRight().Value-1,
				        Size.y-(int)BorderBottom().Value-1); 

				#if 1
				GRegion c(r);
				
				if (Cells)
				{
					List<GTag> AllTd;
					Cells->GetAll(AllTd);
					for (GTag *t=AllTd.First(); t; t=AllTd.Next())
					{
						r.Set(0, 0, t->Size.x-1, t->Size.y-1);
						for (; t && t!=this; t=t->Parent)
						{
							r.Offset(t->Pos.x, t->Pos.y);
						}
						c.Subtract(&r);
					}
				}

				if (BackgroundColor().Type == ColorInherit)
					pDC->Colour(DefaultTableBorder, 32);
				else
					pDC->Colour(BackgroundColor().Rgb32, 32);
				
				for (GRect *p=c.First(); p; p=c.Next())
				{
					pDC->Rectangle(p);
				}
				
				#else
				
				pDC->Colour(DefaultTableBorder, 32);
				pDC->Rectangle(&r);

				#endif				
			}
			break;
		}
		default:
		{
			ColorDef _back = BackgroundColor();
			COLOUR fore = GetFore();
			COLOUR back = (_back.Type == ColorInherit && Disp == DispBlock) ? GetBack() : _back.Rgb32;

			if (Disp == DispBlock && Html->Environment)
			{
				GCss::ImageDef Img = BackgroundImage();
				if (Img.Img)
				{
					GRect Clip(0, 0, Size.x-1, Size.y-1);
					pDC->ClipRgn(&Clip);
					
					GRect r;
					r.ZOff(Size.x-1, Size.y-1);
					FillRectWithImage(pDC, &r, Img.Img, BackgroundRepeat());
					
					back = GT_TRANSPARENT;
				}
			}

			if (back != GT_TRANSPARENT)
			{
				bool IsAlpha = A32(back) < 0xff;
				int Op = GDC_SET;
				if (IsAlpha)
				{
					Op = pDC->Op(GDC_ALPHA);
				}
				pDC->Colour(back, 32);

				if (Disp == DispBlock || Disp == DispInlineBlock)
				{
					pDC->Rectangle(0, 0, Size.x-1, Size.y-1);
				}
				else
				{
					for (int i=0; i<TextPos.Length(); i++)
					{
						pDC->Rectangle(TextPos[i]);
					}
				}

				if (IsAlpha)
				{
					pDC->Op(Op);
				}
			}

			OnPaintBorder(pDC);

			GFont *f = GetFont();
			if (f && TextPos.Length())
			{
				int LineHtOff = 0;
				GCss::Len LineHt = LineHeight();
				if (LineHt.IsValid() && Disp != DispInline)
				{
					Len PadTop = PaddingTop(), PadBot = PaddingBottom();
					int AvailY = Size.y -
								(PadTop.IsValid() ? PadTop.ToPx(Size.y, GetFont()) : 0) -
								(PadBot.IsValid() ? PadBot.ToPx(Size.y, GetFont()) : 0);
					int LineHtPx = LineHt.ToPx(Size.y, f);
					int FontHt = f->GetHeight();
					if (LineHtPx > AvailY)
						LineHtPx = AvailY;
						
					LineHtOff = LineHtPx > FontHt ? max(0, ((LineHtPx - FontHt) >> 1) - 1) : 0;
				}
				
				#define FontColour(s) \
				f->Transparent(!s); \
				if (s) \
					f->Colour(LC_FOCUS_SEL_FORE, LC_FOCUS_SEL_BACK); \
				else \
					f->Colour(	fore != GT_TRANSPARENT ? Rgb32To24(fore) : Rgb32To24(DefaultTextColour), \
								_back.Type != ColorInherit ? Rgb32To24(_back.Rgb32) : Rgb32To24(LC_WORKSPACE));

				if (Html->HasSelection() &&
					(Selection >= 0 || Cursor >= 0) &&
					Selection != Cursor)
				{
					int Min = -1;
					int Max = -1;

					int Base = GetTextStart();
					if (Cursor >= 0 && Selection >= 0)
					{
						Min = min(Cursor, Selection) + Base;
						Max = max(Cursor, Selection) + Base;
					}
					else if (Selected)
					{
						Max = max(Cursor, Selection) + Base;
					}
					else
					{
						Min = max(Cursor, Selection) + Base;
					}

					GRect CursorPos;
					CursorPos.ZOff(-1, -1);

					for (int i=0; i<TextPos.Length(); i++)
					{
						GFlowRect *Tr = TextPos[i];
						int Start = Tr->Text - Text();
						int Done = 0;
						int x = Tr->x1;

						if (Tr->Len == 0)
						{
							// Is this a selection edge point?
							if (!Selected && Min == 0)
							{
								Selected = !Selected;
							}
							else if (Selected && Max == 0)
							{
								Selected = !Selected;
							}

							if (Cursor >= 0)
							{
								// Is this the cursor, then draw it and save it's position
								if (Cursor == Start + Done - Base)
								{
									Html->d->CursorPos.Set(x, Tr->y1 + LineHtOff, x + 1, Tr->y2 - LineHtOff);

									if (Html->d->CursorPos.x1 > Tr->x2)
										Html->d->CursorPos.Offset(Tr->x2 - Html->d->CursorPos.x1, 0);

									CursorPos = Html->d->CursorPos;
									Html->d->CursorPos.Offset(AbsX(), AbsY());
								}
							}
							break;
						}

						while (Done < Tr->Len)
						{
							int c = Tr->Len - Done;

							FontColour(Selected);

							// Is this a selection edge point?
							if (		!Selected &&
										Min - Start >= Done &&
										Min - Start < Done + Tr->Len)
							{
								Selected = !Selected;
								c = Min - Start - Done;
							}
							else if (	Selected &&
										Max - Start >= Done &&
										Max - Start <= Tr->Len)
							{
								Selected = !Selected;
								c = Max - Start - Done;
							}

							// Draw the text run
							GDisplayString ds(f, Tr->Text + Done, c);
							ds.Draw(pDC, x, Tr->y1 + LineHtOff);
							x += ds.X();
							Done += c;

							// Is this is end of the tag?
							if (Tr->Len == Done)
							{
								// Is it also a selection edge?
								if (		!Selected &&
											Min - Start == Done)
								{
									Selected = !Selected;
								}
								else if (	Selected &&
											Max - Start == Done)
								{
									Selected = !Selected;
								}
							}

							if (Cursor >= 0)
							{
								// Is this the cursor, then draw it and save it's position
								if (Cursor == Start + Done - Base)
								{
									Html->d->CursorPos.Set(x, Tr->y1 + LineHtOff, x + 1, Tr->y2 - LineHtOff);

									if (Html->d->CursorPos.x1 > Tr->x2)
										Html->d->CursorPos.Offset(Tr->x2 - Html->d->CursorPos.x1, 0);

									CursorPos = Html->d->CursorPos;
									Html->d->CursorPos.Offset(AbsX(), AbsY());
								}
							}

						}
					}

					if (Html->d->CursorVis && CursorPos.Valid())
					{
						pDC->Colour(LC_TEXT, 24);
						pDC->Rectangle(&CursorPos);
					}
				}
				else if (Cursor >= 0)
				{
					FontColour(Selected);

					int Base = GetTextStart();
					for (int i=0; i<TextPos.Length(); i++)
					{
						GFlowRect *Tr = TextPos[i];
						int Pos = (Tr->Text - Text()) - Base;

						GDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1 + LineHtOff);

						if
						(
							(
								Tr->Text == PreText()
								&&
								!ValidStrW(Text())
							)
							||
							(
								Cursor >= Pos
								&&
								Cursor <= Pos + Tr->Len
							)
						)
						{
							int Off = Tr->Text == PreText() ? StrlenW(PreText()) : Cursor - Pos;
							pDC->Colour(LC_TEXT, 24);
							GRect c;
							if (Off)
							{
								GDisplayString ds(f, Tr->Text, Off);
								int x = ds.X();
								if (x >= Tr->X())
									x = Tr->X()-1;
								c.Set(Tr->x1 + x, Tr->y1, Tr->x1 + x + 1, Tr->y1 + f->GetHeight());
							}
							else
							{
								c.Set(Tr->x1, Tr->y1, Tr->x1 + 1, Tr->y1 + f->GetHeight());
							}
							Html->d->CursorPos = c;
							if (Html->d->CursorVis)
								pDC->Rectangle(&c);
							Html->d->CursorPos.Offset(AbsX(), AbsY());
						}
					}
				}
				else
				{
					FontColour(Selected);

					for (int i=0; i<TextPos.Length(); i++)
					{
						GFlowRect *Tr = TextPos[i];
						GDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1 + LineHtOff);
						
						#if 0
						int Y = ds.Y();
						pDC->Colour(GColour(0, 0, 255));
						pDC->Box(Tr->x1, Tr->y1 + LineHtOff, Tr->x2, Tr->y1 + LineHtOff + Y - 1);
						#endif
					}
				}
			}
			break;
		}
	}

	List<GTag>::I TagIt = Tags.Start();
	for (GTag *t=*TagIt; t; t=*++TagIt)
	{
		pDC->SetOrigin(Px - t->Pos.x, Py - t->Pos.y);
		t->OnPaint(pDC);
		pDC->SetOrigin(Px, Py);
	}
}

//////////////////////////////////////////////////////////////////////
GHtml::GHtml(int id, int x, int y, int cx, int cy, GDocumentEnv *e)
	:
	GDocView(e),
	ResObject(Res_Custom)
{
	d = new GHtmlPrivate;
	SetReadOnly(true);
	ViewWidth = -1;
	MemDC = 0;
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Cursor = 0;
	Selection = 0;
	SetBackColour(Rgb24To32(LC_WORKSPACE));
	PrevTip = 0;
	DocCharSet = 0;

	_New();
}

GHtml::~GHtml()
{
	_Delete();
	DeleteArray(DocCharSet);
	DeleteObj(d);

	if (JobSem.Lock(_FL))
	{
		JobSem.Jobs.DeleteObjects();
		JobSem.Unlock();
	}
}

void GHtml::_New()
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)'\n", __FILE__, __LINE__, this, Source);
	#endif

	d->Content.x = d->Content.y = 0;
	Tag = 0;
	Source = 0;
	DeleteArray(DocCharSet);
	// if (!Charset) Charset = NewStr("utf-8");

	IsHtml = true;
	FontCache = new GFontCache(this);
	SetScrollBars(false, false);
}

void GHtml::_Delete()
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)='%30.30s'\n", _FL, this, Source, Source);
	#endif

	LgiAssert(!d->IsParsing);

	SetBackColour(Rgb24To32(LC_WORKSPACE));

	CssStore.Empty();
	OpenTags.Empty();
	DeleteArray(Source);
	DeleteObj(Tag);
	DeleteObj(FontCache);
	DeleteObj(MemDC);
}

GFont *GHtml::DefFont()
{
	return GetFont();
}

void GHtml::AddCss(char *Css)
{
	const char *c = Css;
	CssStore.Parse(c);
	DeleteArray(Css);
}

void GHtml::Parse()
{
	if (!Tag)
	{
		Tag = new GTag(this, 0);
	}

	SetBackColour(Rgb24To32(LC_WORKSPACE));
	if (Tag)
	{
		Tag->TagId = ROOT;
		OpenTags.Empty();

		if (IsHtml)
		{
			GTag *c;

			Tag->ParseHtml(Source, 0);

			// Add body tag if not specified...
			GTag *Html = Tag->GetTagByName("html");
			if (!Html)
			{
				if (Html = new GTag(this, 0))
				{
					Html->SetTag("html");
					
					while (c = Tag->Tags.First())
						Html->Attach(c);
					
					Tag->Attach(Html);
				}
			}

			if (Html)
			{
				GTag *Body = Tag->GetTagByName("body");
				if (!Body)
				{
					if (Body = new GTag(this, 0))
					{
						Body->SetTag("body");
						Html->Attach(Body);
					}
				}

				if (Body)
				{
					if (Body->Parent != Html)
					{
						Html->Attach(Body);
					}

					if (Tag->Text())
					{
						GTag *Content = new GTag(this, 0);
						if (Content)
						{
							Content->Text(NewStrW(Tag->Text()));
							Tag->Text(0);
							Body->Attach(Content, 0);
						}
					}

					#if 1
					for (GTag *t = Html->Tags.First(); t; )
					{
						if (t->Tag && t->Tag[0] == '!')
						{
							Tag->Attach(t, 0);
							t = Html->Tags.Current();
						}
						else if (t->TagId != TAG_HEAD &&
								t != Body)
						{
							if (t->TagId == TAG_HTML)
							{
								GTag *c;
								while (c = t->Tags.First())
								{
									Html->Attach(c, 0);
								}

								t->Detach();
								DeleteObj(t);
							}
							else
							{
								t->Detach();
								Body->Attach(t);
							}

							t = Html->Tags.Current();
						}
						else
						{
							t = Html->Tags.Next();
						}
					}
					#endif					

					if (Environment)
					{
						const char *OnLoad;
						if (Body->Get("onload", OnLoad))
						{
							Environment->OnExecuteScript((char*)OnLoad);
						}
					}
				}
			}
		}
		else
		{
			Tag->ParseText(Source);
		}
	}

	ViewWidth = -1;
	Invalidate();
}

bool GHtml::NameW(const char16 *s)
{
	GAutoPtr<char, true> utf(LgiNewUtf16To8(s));
	return Name(utf);
}

char16 *GHtml::NameW()
{
	GBase::Name(Source);
	return GBase::NameW();
}

bool GHtml::Name(const char *s)
{
	d->DocumentUid++;

	#if 0
	GFile Out;
	if (s && Out.Open("~\\Desktop\\html-input.html", O_WRITE))
	{
		Out.SetSize(0);
		Out.Write(s, strlen(s));
		Out.Close();
	}
	#endif

	LgiAssert(LgiApp->GetGuiThread() == LgiGetCurrentThread());

	_Delete();
	_New();

	Source = NewStr(s);

	if (Source)
	{
		IsHtml = false;

		// Detect HTML
		char *s = Source;
		while (s = strchr(s, '<'))
		{
			char *t = 0;
			s = ParseName(++s, &t);
			if (t && GetTagInfo(t))
			{
				DeleteArray(t);
				IsHtml = true;
				break;
			}
			DeleteArray(t);
		}

		#if 0
		GFile f;
		f.Open("c:\\temp\\broken.html", O_WRITE);
		{
			f.Write(Source, strlen(Source));
			f.Close();
		}
		#endif

		// Parse
		d->IsParsing = true;
		Parse();
		d->IsParsing = false;
	}

	Invalidate();	

	return true;
}

char *GHtml::Name()
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)='%30.30s'\n", _FL, this, Source, Source);
	#endif

	if (!Source)
	{
		GStringPipe s(1024);
		Tag->CreateSource(s);
		Source = s.NewStr();
	}

	return Source;
}

GMessage::Result GHtml::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_COPY:
		{
			Copy();
			break;
		}
		case M_JOBS_LOADED:
		{
			bool Update = false;
			
			if (JobSem.Lock(_FL))
			{
				for (int i=0; i<JobSem.Jobs.Length(); i++)
				{
					GDocumentEnv::LoadJob *j = JobSem.Jobs[i];
					GDocView *Me = this;
					
					if (j->View == Me &&
						j->UserUid == d->DocumentUid &&
						j->UserData != NULL)
					{
						Html1::GTag *r = static_cast<Html1::GTag*>(j->UserData);
						
						// Check the tag is still in our tree...
						if (Tag->HasChild(r))
						{
							// Process the returned data...
							if (r->TagId == TAG_IMG && j->pDC)
							{
								r->SetImage(j->Uri, j->pDC.Release());
								ViewWidth = 0;
								Update = true;
							}
							else if (r->TagId == TAG_LINK && j->Stream)
							{
								int64 Size = j->Stream->GetSize();
								GAutoString Style(new char[Size+1]);
								int rd = j->Stream->Read(Style, Size);
								if (rd > 0)
								{
									Style[rd] = 0;									
									AddCss(Style.Release());									
									ViewWidth = 0;
									Update = true;
								}
							}
						}
					}
					// else it's from another (historical) HTML control, ignore
				}
				JobSem.Jobs.DeleteObjects();
				JobSem.Unlock();
			}

			if (Update)
			{
				OnPosChange();
				Invalidate();
			}
			break;
		}
	}

	return GDocView::OnEvent(Msg);
}

int GHtml::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_VSCROLL:
		{
			Invalidate();
			break;
		}
		default:
		{
			GTag *Ctrl = Tag ? Tag->FindCtrlId(c->GetId()) : NULL;
			if (Ctrl)
				return Ctrl->OnNotify(f);
			break;
		}
	}

	return GLayout::OnNotify(c, f);
}

void GHtml::OnPosChange()
{
	GLayout::OnPosChange();
	if (ViewWidth != X())
	{
		Invalidate();
	}
}

GdcPt2 GHtml::Layout()
{
	GRect Client = GetClient();
	if (Tag && ViewWidth != Client.X())
	{
		GRect Client = GetClient();
		GFlowRegion f(this, Client);

		// Flow text, width is different
		Tag->OnFlow(&f);
		ViewWidth = Client.X();;
		d->Content.x = f.max_cx + 1;
		d->Content.y = f.y2;

		// Set up scroll box
		int Sy = f.y2 > Y();
		int LineY = GetFont()->GetHeight();

		uint64 Now = LgiCurrentTime();
		if (Now - d->SetScrollTime > 100)
		{
			d->SetScrollTime = Now;
			SetScrollBars(false, Sy);
			if (Sy && VScroll)
			{
				int y = Y();
				int p = max(y / LineY, 1);
				int fy = f.y2 / LineY;
				VScroll->SetPage(p);
				VScroll->SetLimits(0, fy);
			}
		}
		else
		{
			LgiTrace("%s - Dropping SetScroll, loop detected: %i ms\n", GetClass(), (int)(Now - d->SetScrollTime));
		}
	}

	return d->Content;
}

void GHtml::OnPaint(GSurface *ScreenDC)
{
	#if LGI_EXCEPTIONS
	try
	{
	#endif
		GRect Client = GetClient();
		GRect p = GetPos();

		#if GHTML_USE_DOUBLE_BUFFER
		if (!MemDC ||
			(MemDC->X() < Client.X() || MemDC->Y() < Client.Y()))
		{
			DeleteObj(MemDC);
			MemDC = new GMemDC;
			if (MemDC && !MemDC->Create(Client.X() + 10, Client.Y() + 10, 32)) // GdcD->GetBits()
			{
				DeleteObj(MemDC);
			}
		}
		#endif

		GSurface *pDC = MemDC ? MemDC : ScreenDC;

		COLOUR Back = GetBackColour();
		pDC->Colour(Back, 32);
		pDC->Rectangle();

		if (Tag)
		{
			Layout();

			if (VScroll)
			{
				int LineY = GetFont()->GetHeight();
				int Vs = VScroll->Value();
				pDC->SetOrigin(0, Vs * LineY);
			}

			Tag->OnPaint(pDC);
		}

		#if GHTML_USE_DOUBLE_BUFFER
		if (MemDC)
		{
			pDC->SetOrigin(0, 0);
			#if 0
			pDC->Colour(Rgb24(255, 0, 0), 24);
			pDC->Line(0, 0, X()-1, Y()-1);
			pDC->Line(X()-1, 0, 0, Y()-1);
			#endif
			ScreenDC->Blt(0, 0, MemDC);
		}
		#endif
	#if LGI_EXCEPTIONS
	}
	catch (...)
	{
		LgiTrace("GHtml paint crash\n");
	}
	#endif

	if (d->OnLoadAnchor && VScroll)
	{
		GAutoString a = d->OnLoadAnchor;
		GotoAnchor(a);
		LgiAssert(d->OnLoadAnchor == 0);
	}
}

GTag *GHtml::GetOpenTag(char *Tag)
{
	if (Tag)
	{
		for (GTag *t=OpenTags.Last(); t; t=OpenTags.Prev())
		{
			if (t->Tag)
			{
				if (stricmp(t->Tag, Tag) == 0)
				{
					return t;
				}

				if (stricmp(t->Tag, "table") == 0)
				{
					// stop looking... we don't close tags outside
					// the table from inside.
					break;
				}				
			}
		}
	}

	return 0;
}

bool GHtml::HasSelection()
{
	if (Cursor && Selection)
	{
		return	Cursor->Cursor >= 0 &&
				Selection->Selection >= 0 &&
				!(Cursor == Selection && Cursor->Cursor == Selection->Selection);
	}

	return false;
}

void GHtml::UnSelectAll()
{
	bool i = false;
	if (Cursor)
	{
		Cursor->Cursor = -1;
		i = true;
	}
	if (Selection)
	{
		Selection->Selection = -1;
		i = true;
	}
	if (i)
	{
		Invalidate();
	}
}

void GHtml::SelectAll()
{
}

GTag *GHtml::GetLastChild(GTag *t)
{
	if (t)
	{
		for (GTag *i = t->Tags.Last(); i; )
		{
			GTag *c = i->Tags.Last();
			if (c)
				i = c;
			else
				return i;
		}
	}

	return 0;
}

GTag *GHtml::PrevTag(GTag *t)
{
	// This returns the previous tag in the tree as if all the tags were
	// listed via recursion using "in order".

	// Walk up the parent chain looking for a prev
	for (GTag *p = t; p; p = p->Parent)
	{
		// Does this tag have a parent?
		if (p->Parent)
		{
			// Prev?
			int Idx = p->Parent->Tags.IndexOf(p);
			GTag *Prev = p->Parent->Tags[Idx - 1];
			if (Prev)
			{
				GTag *Last = GetLastChild(Prev);
				return Last ? Last : Prev;
			}
			else
			{
				return p->Parent;
			}
		}
	}

	return 0;
}

GTag *GHtml::NextTag(GTag *t)
{
	// This returns the next tag in the tree as if all the tags were
	// listed via recursion using "in order".

	// Does this have a child tag?
	if (t->Tags.First())
	{
		return t->Tags.First();
	}
	else
	{
		// Walk up the parent chain
		for (GTag *p = t; p; p = p->Parent)
		{
			// Does this tag have a next?
			if (p->Parent)
			{
				int Idx = p->Parent->Tags.IndexOf(p);
				GTag *Next = p->Parent->Tags[Idx + 1];
				if (Next)
				{
					return Next;
				}
			}
		}
	}

	return 0;
}

int GHtml::GetTagDepth(GTag *Tag)
{
	// Returns the depth of the tag in the tree.
	int n = 0;
	for (GTag *t = Tag; t; t = t->Parent)
	{
		n++;
	}
	return n;
}

bool GHtml::IsCursorFirst()
{
	// Returns true if the cursor is before the selection point.
	//
	// There are 2 points in the selection, the start and the end, the
	// cursor can be the start or the end, there selection is always the
	// other end of the block.
	if (Cursor && Selection)
	{
		if (Cursor == Selection)
		{
			return Cursor->Cursor < Selection->Selection;
		}
		else
		{
			int CDepth = GetTagDepth(Cursor);
			int SDepth = GetTagDepth(Selection);
			GTag *Cur = Cursor;
			GTag *Sel = Selection;
			while (Sel && SDepth > CDepth)
			{
				Sel = Sel->Parent;
				SDepth--;
			}
			while (Cur && CDepth > SDepth)
			{
				Cur = Cur->Parent;
				CDepth--;
			}
			if (Cur && Sel)
			{
				int CIdx = Cur->Parent ? Cur->Parent->Tags.IndexOf(Cur) : 0;
				int SIdx = Sel->Parent ? Sel->Parent->Tags.IndexOf(Sel) : 0;
				if (CIdx < SIdx)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void GHtml::SetLoadImages(bool i)
{
	if (i ^ GetLoadImages())
	{
		GDocView::SetLoadImages(i);
		SendNotify(GTVN_SHOW_IMGS_CHANGED);

		if (GetLoadImages() && Tag)
		{
			Tag->LoadImages();
		}
	}
}

char *GHtml::GetSelection()

{
	char *s = 0;

	GBytePipe p;

	Tag->CopyClipboard(p);

	int Len = p.GetSize();
	if (Len > 0)
	{
		char16 *t = (char16*)p.New(sizeof(char16));
		if (t)
		{
			int Len = StrlenW(t);
			for (int i=0; i<Len; i++)
			{
				if (t[i] == 0xa0) t[i] = ' ';
			}
			s = LgiNewUtf16To8(t);
			DeleteArray(t);
		}
	}

	return s;
}

bool GHtml::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;
	if (!stricmp(Name, "ShowImages"))
	{
		SetLoadImages(Value.CastBool());
	}
	else return false;
	
	return true;
}

bool GHtml::Copy()
{
	GAutoString s(GetSelection());
	if (s)
	{
		GClipBoard c(this);
		
		GAutoWString w(LgiNewUtf8To16(s));
		if (w) c.TextW(w);
		c.Text(s, w == 0);

		return true;
	}
	else LgiTrace("%s:%i - Error: no selection\n", _FL);

	return false;
}

static bool FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	GHtml *h = (GHtml*)User;
	return h->OnFind(Dlg);
}

void BuildTagList(GArray<GTag*> &t, GTag *Tag)
{
	t.Add(Tag);
	for (GTag *c = Tag->Tags.First(); c; c = Tag->Tags.Next())
	{
		BuildTagList(t, c);
	}
}

static void FormEncode(GStringPipe &p, const char *c)
{
	const char *s = c;
	while (*c)
	{
		while (*c && *c != ' ')
			c++;
		
		if (c > s)
		{
			p.Write(s, c - s);
			c = s;
		}
		if (*c == ' ')
		{
			p.Write("+", 1);
			s = c;
		}
		else break;
	}
}

bool GHtml::OnSubmitForm(GTag *Form)
{
	if (!Form || !Environment)
	{
		LgiAssert(!"Bad param");
		return false;
	}

	const char *Method = NULL;
	const char *Action = NULL;
	if (!Form->Get("method", Method) ||
		!Form->Get("action", Action))
	{
		LgiAssert(!"Missing form action/method");
		return false;
	}
		
	GHashTbl<const char*,char*> f;
	Form->CollectFormValues(f);
	bool Status = false;
	if (!stricmp(Method, "post"))
	{
		GStringPipe p(256);
		const char *Field;
		bool First = true;
		for (char *Val = f.First(&Field); Val; Val = f.Next(&Field))
		{
			if (First)
				First = false;
			else
				p.Write("&", 1);
			
			FormEncode(p, Field);
			p.Write("=", 1);
			FormEncode(p, Val);
		}
		
		GAutoPtr<const char, true> Data(p.NewStr());
		Status = Environment->OnPostForm(Action, Data);
	}
	else if (!stricmp(Method, "get"))
	{
		Status = Environment->OnNavigate(Action);
	}
	else
	{
		LgiAssert(!"Bad form method.");
	}
	
	f.DeleteArrays();
	return Status;
}

bool GHtml::OnFind(GFindReplaceCommon *Params)
{
	bool Status = false;

	if (Params)
	{
		if (!Params->Find)
			return Status;

		d->FindText.Reset(LgiNewUtf8To16(Params->Find));
		d->MatchCase = Params->MatchCase;
	}		

	if (!Cursor)
		Cursor = Tag;

	if (Cursor && d->FindText)
	{
		GArray<GTag*> Tags;
		BuildTagList(Tags, Tag);
		int Start = Tags.IndexOf(Cursor);
		for (int i=1; i<Tags.Length(); i++)
		{
			int Idx = (Start + i) % Tags.Length();
			GTag *s = Tags[Idx];

			if (s->Text())
			{
				char16 *Hit;
				
				if (d->MatchCase)
					Hit = StrstrW(s->Text(), d->FindText);
				else
					Hit = StristrW(s->Text(), d->FindText);
				
				if (Hit)
				{
					// found something...
					UnSelectAll();

					Selection = Cursor = s;
					Cursor->Cursor = Hit - s->Text();
					Selection->Selection = Cursor->Cursor + StrlenW(d->FindText);
					OnCursorChanged();
					
					if (VScroll)
					{
						// Scroll the tag into view...
						int y = s->AbsY();
						int LineY = GetFont()->GetHeight();
						int Val = y / LineY;
						VScroll->Value(Val);
					}
					
					Invalidate();
					Status = true;
					break;
				}
			}
		}
	}

	return Status;
}

bool GHtml::OnKey(GKey &k)
{
	bool Status = false;
	
	if (k.Down())
	{
		int Dy = 0;
		int LineY = GetFont()->GetHeight();
		int Page = GetClient().Y() / LineY;

		switch (k.c16)
		{
			case 'f':
			case 'F':
			{
				if (k.Modifier())
				{
					GFindDlg Dlg(this, 0, FindCallback, this);
					Dlg.DoModal();
					Status = true;
				}
				break;
			}
			case VK_F3:
			{
				OnFind(NULL);
				break;
			}
			case 'c':
			case 'C':
			#ifdef WIN32
			case VK_INSERT:
			#endif
			{
				printf("Got 'c', mod=%i\n", k.Modifier());
				if (k.Modifier())
				{
					Copy();
					Status = true;
				}
				break;
			}
			case VK_UP:
			{
				Dy = -1;
				Status = true;
				break;
			}
			case VK_DOWN:
			{
				Dy = 1;
				Status = true;
				break;
			}
			case VK_PAGEUP:
			{
				Dy = -Page;
				Status = true;
				break;
			}
			case VK_PAGEDOWN:
			{
				Dy = Page;
				Status = true;
				break;
			}
			case VK_HOME:
			{
				Dy = VScroll ? -VScroll->Value() : 0;
				Status = true;
				break;
			}
			case VK_END:
			{
				if (VScroll)
				{
					int64 Low, High;
					VScroll->Limits(Low, High);
					Dy = (High - Page) - VScroll->Value();
				}
				Status = true;
				break;
			}
		}

		if (Dy && VScroll)
		{
			VScroll->Value(VScroll->Value() + Dy);
			Invalidate();
		}
	}

	return Status;
}

int GHtml::ScrollY()
{
	return GetFont()->GetHeight() * (VScroll ? VScroll->Value() : 0);
}

void GHtml::OnMouseClick(GMouse &m)
{
	Capture(m.Down());
	SetPulse(m.Down() ? 200 : -1);

	if (m.Down())
	{
		Focus(true);

		int Offset = ScrollY();
		bool TagProcessedClick = false;
		int Index = -1;

		GTag *Over = GetTagByPos(m.x, m.y + Offset, &Index);
		if (m.Left() && !m.IsContextMenu())
		{
			if (m.Double())
			{
				d->WordSelectMode = true;

				if (Cursor)
				{
					// Extend the selection out to the current word's boundries.
					Selection = Cursor;
					Selection->Selection = Cursor->Cursor;

					if (Cursor->Text())
					{
						int Base = Cursor->GetTextStart();
						char16 *Text = Cursor->Text() + Base;
						while (Text[Cursor->Cursor])
						{
							char16 c = Text[Cursor->Cursor];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Cursor->Cursor++;
						}
					}
					if (Selection->Text())
					{
						int Base = Selection->GetTextStart();
						char16 *Sel = Selection->Text() + Base;
						while (Selection->Selection > 0)
						{
							char16 c = Sel[Selection->Selection - 1];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Selection->Selection--;
						}
					}

					Invalidate();
				}
			}
			else if (Over)
			{
				d->WordSelectMode = false;
				UnSelectAll();
				Selection = Cursor = Over;
				if (Cursor)
				{
					Selection->Selection = Cursor->Cursor = Index;
				}
				OnCursorChanged();
			}
		}

		if (Over)
		{
			TagProcessedClick = Over->OnMouseClick(m);
		}
		#ifdef _DEBUG
		else if (m.Left() && m.Ctrl())
		{
			LgiMsg(this, "No tag under the cursor.", GetClass());
		}
		#endif

		if (!TagProcessedClick &&
			m.IsContextMenu())
		{
			GSubMenu *RClick = new GSubMenu;
			if (RClick)
			{
				#define IDM_DUMP			100
				#define IDM_COPY_SRC		101
				#define IDM_VIEW_SRC		102
				#define IDM_EXTERNAL		103
				#define IDM_COPY			104
				#define IDM_VIEW_IMAGES		105
				#define IDM_CHARSET_BASE	1000

				RClick->AppendItem					(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
				GMenuItem *Vs = RClick->AppendItem	(LgiLoadString(L_VIEW_SOURCE, "View Source"), IDM_VIEW_SRC, Source != 0);
				RClick->AppendItem					(LgiLoadString(L_COPY_SOURCE, "Copy Source"), IDM_COPY_SRC, Source != 0);
				GMenuItem *Load = RClick->AppendItem(LgiLoadString(L_VIEW_IMAGES, "View External Images"), IDM_VIEW_IMAGES, true);
				if (Load) Load->Checked(GetLoadImages());
				RClick->AppendItem					(LgiLoadString(L_VIEW_IN_DEFAULT_BROWSER, "View in Default Browser"), IDM_EXTERNAL, Source != 0);
				GSubMenu *Cs = RClick->AppendSub	(LgiLoadString(L_CHANGE_CHARSET, "Change Charset"));
				if (Cs)
				{
					int n=0;
					for (GCharset *c = LgiGetCsList(); c->Charset; c++, n++)
					{
						Cs->AppendItem(c->Charset, IDM_CHARSET_BASE + n, c->IsAvailable());
					}
				}
				
				#ifdef _DEBUG
				RClick->AppendSeparator();
				RClick->AppendItem("Dump", IDM_DUMP, Tag != 0);
				#endif

				if (Vs)
				{
					Vs->Checked(!IsHtml);
				}

				if (GetMouse(m, true))
				{
					int Id = RClick->Float(this, m.x, m.y);
					switch (Id)
					{
						case IDM_COPY:
						{
							Copy();
							break;
						}
						case IDM_VIEW_SRC:
						{
							LgiCheckHeap();
							if (Vs)
							{
								DeleteObj(Tag);
								IsHtml = !IsHtml;
								Parse();
							}
							LgiCheckHeap();
							break;
						}
						case IDM_COPY_SRC:
						{
							if (Source)
							{
								GClipBoard c(this);
								if (Is8Bit(Source))
								{
									GAutoWString w((char16*)LgiNewConvertCp(LGI_WideCharset, Source, DocCharSet ? DocCharSet : (char*)"windows-1252"));
									if (w)
										c.TextW(w);
								}
								else
								{
									c.Text(Source);
								}
							}
							break;
						}
						case IDM_VIEW_IMAGES:
						{
							SetLoadImages(!GetLoadImages());
							break;
						}
						case IDM_DUMP:
						{
							if (Tag)
							{
								GAutoWString s = Tag->DumpW();
								if (s)
								{
									GClipBoard c(this);
									c.TextW(s);
								}
							}
							break;
						}
						case IDM_EXTERNAL:
						{
							char Path[256];
							if (Source && LgiGetSystemPath(LSP_TEMP, Path, sizeof(Path)))
							{
								char f[32];
								sprintf(f, "_%i.html", LgiRand(1000000));
								LgiMakePath(Path, sizeof(Path), Path, f);
								
								LgiCheckHeap();

								GFile F;
								if (F.Open(Path, O_WRITE))
								{
									GStringPipe Ex;
									bool Error = false;
									
									F.SetSize(0);

									for (char *s=Source; s && *s;)
									{
										char *cid = stristr(s, "cid:");
										while (cid && !strchr("\'\"", cid[-1]))
										{
											cid = stristr(cid+1, "cid:");
										}

										if (cid)
										{
											char Delim = cid[-1];
											char *e = strchr(cid, Delim);
											if (e)
											{
												*e = 0;
												if (strchr(cid, '\n'))
												{
													*e = Delim;
													Error = true;
													break;
												}
												else
												{
													char File[MAX_PATH] = "";
													if (Environment)
													{
														GDocumentEnv::LoadJob *j = Environment->NewJob();
														if (j)
														{
															j->Uri.Reset(NewStr(cid));
															j->View = this;
															j->Pref = GDocumentEnv::LoadJob::FmtFilename;
															j->UserUid = d->DocumentUid;

															GDocumentEnv::LoadType Result = Environment->GetContent(j);
															if (Result == GDocumentEnv::LoadImmediate)
															{
																if (j->Filename)
																	strsafecpy(File, j->Filename, sizeof(File));
															}
															DeleteObj(j);
														}
													}
													
													*e = Delim;
													Ex.Push(s, cid - s);
													if (File[0])
													{
														char *d;
														while (d = strchr(File, '\\'))
														{
															*d = '/';
														}
														
														Ex.Push("file:///");
														Ex.Push(File);
													}
													s = e;
												}
											}
											else
											{
												Error = true;
												break;
											}
										}
										else
										{
											Ex.Push(s);
											break;
										}
									}

									LgiCheckHeap();

									if (!Error)
									{
										char *Final = Ex.NewStr();
										if (Final)
										{
											F.Write(Final, strlen(Final));
											DeleteArray(Final);
											F.Close();
											LgiExecute(Path);
										}
									}
								}
							}
							break;
						}
						default:
						{
							if (Id >= IDM_CHARSET_BASE)
							{
								GCharset *c = LgiGetCsList() + (Id - IDM_CHARSET_BASE);
								if (c->Charset)
								{
									Charset.Reset(NewStr(c->Charset));
									OverideDocCharset = true;

									char *Src = Source;
									Source = 0;
									_Delete();
									_New();
									Source = Src;
									Parse();

									Invalidate();

									SendNotify(GTVN_CODEPAGE_CHANGED);
								}								
							}
							break;
						}
					}
				}

				DeleteObj(RClick);
			}
		}
	}
	else // Up Click
	{
		if (Selection && Cursor &&
			Selection == Cursor &&
			Selection->Selection == Cursor->Cursor)
		{
			Selection->Selection = -1;
			Selection = 0;
		}
	}
}

GTag *GHtml::GetTagByPos(int x, int y, int *Index)
{
	if (Tag)
	{
		GTagHit Hit = Tag->GetTagByPos(x, y);
		if (Hit.Hit && Hit.Near < 30)
		{
			if (Index) *Index = Hit.Index;
			return Hit.Hit;
		}
	}

	return 0;
}

bool GHtml::OnMouseWheel(double Lines)
{
	if (VScroll)
	{
		VScroll->Value(VScroll->Value() + (int)Lines);
		Invalidate();
	}
	
	return true;
}

int GHtml::OnHitTest(int x, int y)
{
	return -1;
}

void GHtml::OnMouseMove(GMouse &m)
{
	int Offset = ScrollY();
	int Index = -1;
	GTag *Tag = GetTagByPos(m.x, m.y + Offset, &Index);
	if (Tag)
	{
		if (PrevTip &&
			PrevTip != Tag)
		{			
			Tip.DeleteTip(PrevTip->TipId);
			PrevTip->TipId = 0;
			PrevTip = 0;
		}

		GAutoString Uri;
		if (Tag->IsAnchor(&Uri))
		{
			GRect c = GetClient();
			c.Offset(-c.x1, -c.y1);
			if (c.Overlap(m.x, m.y) && ValidStr(Uri))
			{
				GLayout::SetCursor(LCUR_PointingHand);
			}

			if (Uri)
			{
				if (!Tip.GetParent())
				{
					Tip.Attach(this);
				}

				GRect r = Tag->GetRect(false);
				r.Offset(0, -Offset);
				PrevTip = Tag;
				PrevTip->TipId = Tip.NewTip(Uri, r);
			}
		}

		if (Cursor && Tag->TagId != TAG_BODY && IsCapturing())
		{
			if (!Selection)
			{
				Selection = Cursor;
				Selection->Selection = Cursor->Cursor;
				Cursor = Tag;
				Cursor->Cursor = Index;
				OnCursorChanged();
				Invalidate();
			}
			else if ((Cursor != Tag) ||
					 (Cursor->Selection != Index))
			{
				if (Cursor)
				{
					Cursor->Cursor = -1;
				}

				Cursor = Tag;
				Cursor->Cursor = Index;

				if (d->WordSelectMode && Cursor->Text())
				{
					int Base = Cursor->GetTextStart();
					if (IsCursorFirst())
					{
						// Extend the cursor up the document to include the whole word
						while (Cursor->Cursor > 0)
						{
							char16 c = Cursor->Text()[Base + Cursor->Cursor - 1];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Cursor->Cursor--;
						}
					}
					else
					{
						// Extend the cursor down the document to include the whole word
						while (Cursor->Text()[Base + Cursor->Cursor])
						{
							char16 c = Cursor->Text()[Base + Cursor->Cursor];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Cursor->Cursor++;
						}
					}
				}

				OnCursorChanged();
				Invalidate();
			}
		}
	}
}

void GHtml::OnPulse()
{
	if (VScroll && IsCapturing())
	{
		int Fy = DefFont() ? DefFont()->GetHeight() : 16;
		GMouse m;
		if (GetMouse(m, false))
		{
			GRect c = GetClient();
			int Lines = 0;
			
			if (m.y < c.y1)
			{
				// Scroll up
				Lines = (c.y1 - m.y + Fy - 1) / -Fy;
			}
			else if (m.y > c.y2)
			{
				// Scroll down
				Lines = (m.y - c.y2 + Fy - 1) / Fy;
			}
			
			if (Lines)
			{
				VScroll->Value(VScroll->Value() +  Lines);
				Invalidate();
			}
		}
	}
}

GRect *GHtml::GetCursorPos()
{
	return &d->CursorPos;
}

void GHtml::SetCursorVis(bool b)
{
	if (d->CursorVis ^ b)
	{
		d->CursorVis = b;
		Invalidate();
	}
}

bool GHtml::GetCursorVis()
{
	return d->CursorVis;
}

GDom *ElementById(GTag *t, char *id)
{
	if (t && id)
	{
		const char *i;
		if (t->Get("id", i) && stricmp(i, id) == 0)
			return t;

		for (GTag *c = t->Tags.First(); c; c = t->Tags.Next())
		{
			GDom *n = ElementById(c, id);
			if (n) return n;
		}
	}

	return 0;
}

GDom *GHtml::getElementById(char *Id)
{
	return ElementById(Tag, Id);
}

bool GHtml::GetLinkDoubleClick()
{
	return d->LinkDoubleClick;
}

void GHtml::SetLinkDoubleClick(bool b)
{
	d->LinkDoubleClick = b;
}

bool GHtml::GetFormattedContent(char *MimeType, GAutoString &Out, GArray<GDocView::ContentMedia> *Media)
{
	if (!MimeType)
	{
		LgiAssert(!"No MIME type for getting formatted content");
		return false;
	}

	if (!stricmp(MimeType, "text/html"))
	{
		// We can handle this type...
		GArray<GTag*> Imgs;
		if (Media)
		{
			// Find all the image tags...
			Tag->Find(TAG_IMG, Imgs);

			// Give them CID's if they don't already have them
			for (int i=0; Imgs.Length(); i++)
			{
				GTag *Img = Imgs[i];
				const char *Cid, *Src;
				if (Img->Get("src", Src) &&
					!Img->Get("cid", Cid))
				{
					char id[256];
					sprintf(id, "%x.%x", (unsigned)LgiCurrentTime(), (unsigned)LgiRand());
					Img->Set("cid", id);
					Img->Get("cid", Cid);
				}

				if (Src && Cid)
				{
					GFile *f = new GFile;
					if (f)
					{
						if (f->Open(Src, O_READ))
						{
							// Add the exported image stream to the media array
							GDocView::ContentMedia &m = Media->New();
							m.Id.Reset(NewStr(Cid));
							m.Stream.Reset(f);
						}
					}
				}
			}
		}

		// Export the HTML, including the CID's from the first step
		Out.Reset(NewStr(Name()));
	}
	else if (!stricmp(MimeType, "text/plain"))
	{
		// Convert DOM tree down to text instead...
		GStringPipe p(512);
		if (Tag)
		{
			GTag::TextConvertState State(&p);
			Tag->ConvertToText(State);
		}
		Out.Reset(p.NewStr());
	}

	return false;
}

void GHtml::OnContent(GDocumentEnv::LoadJob *Res)
{
	if (JobSem.Lock(_FL))
	{
		JobSem.Jobs.Add(Res);
		JobSem.Unlock();
		PostEvent(M_JOBS_LOADED);
	}
}

bool GHtml::GotoAnchor(char *Name)
{
	if (Tag)
	{
		GTag *a = Tag->GetAnchor(Name);
		if (a)
		{
			if (VScroll)
			{
				int LineY = GetFont()->GetHeight();
				int Ay = a->AbsY();
				int Scr = Ay / LineY;
				VScroll->Value(Scr);
				PostEvent(M_CHANGE, (GMessage::Param)(GViewI*)VScroll);
			}
			else
				d->OnLoadAnchor.Reset(NewStr(Name));
		}
	}

	return false;
}

bool GHtml::GetEmoji()
{
	return d->DecodeEmoji;
}

void GHtml::SetEmoji(bool i)
{
	d->DecodeEmoji = i;
}

////////////////////////////////////////////////////////////////////////
class GHtml_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (stricmp(Class, "GHtml") == 0)
		{
			return new GHtml(-1, 0, 0, 100, 100, new GDefaultDocumentEnv);
		}

		return 0;
	}

} GHtml_Factory;

//////////////////////////////////////////////////////////////////////
GCellStore::GCellStore(GTag *Table)
{
	if (!Table)
		return;

	int y = 0;
	GTag *FakeRow = 0;
	GTag *FakeCell = 0;

	GTag *r;
	for (r=Table->Tags.First(); r; r=Table->Tags.Next())
	{
		if (r->TagId == TAG_TR)
		{
			FakeRow = 0;
			FakeCell = 0;
		}
		else if (r->TagId == TAG_TBODY)
		{
			int Index = Table->Tags.IndexOf(r);
			for (GTag *t = r->Tags.First(); t; t = r->Tags.Next())
			{
				Table->Tags.Insert(t, ++Index);
				t->Parent = Table;
			}
			r->Tags.Empty();
		}
		else
		{
			if (!FakeRow)
			{
				if (FakeRow = new GTag(Table->Html, 0))
				{
					FakeRow->Tag = NewStr("tr");
					FakeRow->TagId = TAG_TR;

					int Idx = Table->Tags.IndexOf(r);
					Table->Attach(FakeRow, Idx);
				}
			}
			if (FakeRow)
			{
				if (r->TagId != TAG_TD && !FakeCell)
				{
					if (FakeCell = new GTag(Table->Html, FakeRow))
					{
						FakeCell->Tag = NewStr("td");
						FakeCell->TagId = TAG_TD;
						FakeCell->Span.x = 1;
						FakeCell->Span.y = 1;
					}
				}

				int Idx = Table->Tags.IndexOf(r);
				r->Detach();

				if (r->TagId == TAG_TD)
				{
					FakeRow->Attach(r);
				}
				else
				{
					LgiAssert(FakeCell);
					FakeCell->Attach(r);
				}
				Table->Tags[Idx-1];
			}
		}
	}

	for (r=Table->Tags.First(); r; r=Table->Tags.Next())
	{
		if (r->TagId == TAG_TR)
		{
			int x = 0;
			for (GTag *c=r->Tags.First(); c; c=r->Tags.Next())
			{
				if (c->TagId == TAG_TD)
				{
					while (Get(x, y))
					{
						x++;
					}

					c->Cell.x = x;
					c->Cell.y = y;
					Set(c);
					x += c->Span.x;
				}
			}

			y++;
		}
	}
}

void GCellStore::Dump()
{
	int Sx, Sy;
	GetSize(Sx, Sy);

	LgiTrace("Table %i x %i cells.\n", Sx, Sy);
	for (int x=0; x<Sx; x++)
	{
		LgiTrace("--- %-2i--- ", x);
	}
	LgiTrace("\n");

	for (int y=0; y<Sy; y++)
	{
		int x;
		for (x=0; x<Sx; x++)
		{
			GTag *t = Get(x, y);
			LgiTrace("%p ", t);
		}
		LgiTrace("\n");

		for (x=0; x<Sx; x++)
		{
			GTag *t = Get(x, y);
			char s[256] = "";
			if (t)
			{
				sprintf(s, "%i,%i-%i,%i", t->Cell.x, t->Cell.y, t->Span.x, t->Span.y);
			}
			LgiTrace("%-10s", s);
		}
		LgiTrace("\n");
	}

	LgiTrace("\n");
}

void GCellStore::GetAll(List<GTag> &All)
{
	GHashTbl<void*, bool> Added;
	for (int y=0; y<c.Length(); y++)
	{
		CellArray &a = c[y];
		for (int x=0; x<a.Length(); x++)
		{
			GTag *t = a[x];
			if (t && !Added.Find(t))
			{
				Added.Add(t, true);
				All.Insert(t);
			}
		}
	}
}

void GCellStore::GetSize(int &x, int &y)
{
	x = 0;
	y = c.Length();

	for (int i=0; i<c.Length(); i++)
	{
		x = max(x, c[i].Length());
	}
}

GTag *GCellStore::Get(int x, int y)
{
	if (y >= c.Length())
		return NULL;
	
	CellArray &a = c[y];
	if (x >= a.Length())
		return NULL;
	
	return a[x];
}

bool GCellStore::Set(GTag *t)
{
	if (!t)
		return false;

	for (int y=0; y<t->Span.y; y++)
	{
		for (int x=0; x<t->Span.x; x++)
		{
			// LgiAssert(!c[y][x]);
			c[t->Cell.y + y][t->Cell.x + x] = t;
		}
	}

	return true;
}
