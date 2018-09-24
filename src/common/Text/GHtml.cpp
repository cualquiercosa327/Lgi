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
#include "GUnicode.h"
#include "Emoji.h"
#include "GClipBoard.h"
#include "GButton.h"
#include "GEdit.h"
#include "GCombo.h"
#include "GdcTools.h"
#include "GDisplayString.h"
#include "GPalette.h"
#include "GPath.h"
#include "GCssTools.h"
#include "LgiRes.h"
#include "INet.h"

#define DEBUG_TABLE_LAYOUT			0
#define DEBUG_DRAW_TD				0
#define DEBUG_RESTYLE				0
#define DEBUG_TAG_BY_POS			0
#define DEBUG_SELECTION				0
#define DEBUG_TEXT_AREA				0

#define ENABLE_IMAGE_RESIZING		1
#define DOCUMENT_LOAD_IMAGES		1
#define MAX_RECURSION_DEPTH			300
#define ALLOW_TABLE_GROWTH			1

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
#define DefaultCellPadding			1

#ifdef MAC
#define MinimumPointSize			9
#define MinimumBodyFontSize			12
#else
#define MinimumPointSize			8
#define MinimumBodyFontSize			11
#endif

// #define DefaultFont					"font-family: Times; font-size: 16pt;"
#define DefaultBodyMargin			"5px"
#define DefaultImgSize				16
#define DefaultMissingCellColour	GT_TRANSPARENT // Rgb32(0xf0,0xf0,0xf0)
#define ShowNbsp					0

#define FontPxHeight(fnt)			(fnt->GetHeight() - (int)(fnt->Leading() + 0.5))

#if 0 // def _DEBUG
#define DefaultTableBorder			Rgb32(0xf8, 0xf8, 0xf8)
#else
#define DefaultTableBorder			GT_TRANSPARENT
#endif

#define IsTableCell(id)				( ((id) == TAG_TD) || ((id) == TAG_TH) )
#define GetCssLen(a, b)				a().Type == GCss::LenInherit ? b() : a()

static char WordDelim[]	=			".,<>/?[]{}()*&^%$#@!+|\'\"";
static char16 WhiteW[] =			{' ', '\t', '\r', '\n', 0};

#if 0
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
#endif

template<typename T>
void RemoveChars(T *str, T *remove_list)
{
	T *i = str, *o = str, *c;
	while (*i)
	{
		for (c = remove_list; *c; c++)
		{
			if (*c == *i)
				break;
		}
		if (*c == 0)
			*o++ = *i;
		i++;
	}
	*o++ = NULL;
}

//////////////////////////////////////////////////////////////////////
using namespace Html1;

namespace Html1
{

class GHtmlPrivate
{
public:
	LHashTbl<ConstStrKey<char>, GTag*> Loading;
	GHtmlStaticInst Inst;
	bool CursorVis;
	GRect CursorPos;
	GdcPt2 Content;
	bool WordSelectMode;
	bool LinkDoubleClick;
	GAutoString OnLoadAnchor;
	bool DecodeEmoji;
	GAutoString EmojiImg;
	int NextCtrlId;
	uint64 SetScrollTime;
	int DeferredLoads;

	bool IsParsing;
	bool IsLoaded;
	bool StyleDirty;
	
	// Find settings
	GAutoWString FindText;
	bool MatchCase;
	
	GHtmlPrivate()
	{
		IsLoaded = false;
		StyleDirty = false;
		IsParsing = false;
		LinkDoubleClick = true;
		WordSelectMode = false;
		NextCtrlId = 2000;
		SetScrollTime = 0;
		CursorVis = false;
		CursorPos.ZOff(-1, -1);
		DeferredLoads = 0;

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
			return NULL;
		
		GFont *Default = Owner->GetFont();
		GCss::StringsDef Face = Style->FontFamily();
		if (Face.Length() < 1 || !ValidStr(Face[0]))
		{
			Face.Empty();
			const char *DefFace = Default->Face();
			LgiAssert(ValidStr(DefFace));
			Face.Add(NewStr(DefFace));
		}
		LgiAssert(ValidStr(Face[0]));
		GCss::Len Size = Style->FontSize();
		GCss::FontWeightType Weight = Style->FontWeight();
		bool IsBold =	Weight == GCss::FontWeightBold ||
						Weight == GCss::FontWeightBolder ||
						Weight > GCss::FontWeight400;
		bool IsItalic = Style->FontStyle() == GCss::FontStyleItalic;
		bool IsUnderline = Style->TextDecoration() == GCss::TextDecorUnderline;
		double PtSize = 0.0;

		if (Size.Type == GCss::LenInherit ||
			Size.Type == GCss::LenNormal)
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

			// Look for cached fonts of the right size...
			for (f=Fonts.First(); f; f=Fonts.Next())
			{
				if (f->Face() &&
					_stricmp(f->Face(), Face[0]) == 0 &&
					f->Bold() == IsBold &&
					f->Italic() == IsItalic &&
					f->Underline() == IsUnderline)
				{
				    int PtSize = f->PointSize();
				    int Height = FontPxHeight(f);
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
					
					if (RequestPx < FontPxHeight(f) &&
					    f->PointSize() == MinimumPointSize)
				    {
				        return f;
				    }

					Diff = FontPxHeight(f) - RequestPx;
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
			
			int BestPxDiff = 10000;
			GAutoPtr<GFont> BestFont;
			uint32 FaceIdx = 0;
			
			do
			{
				GAutoPtr<GFont> Tmp(new GFont);
				
				Tmp->Bold(IsBold);
				Tmp->Italic(IsItalic);
				Tmp->Underline(IsUnderline);
				
				if (FaceIdx >= Face.Length() ||
					!Tmp->Create(Face[FaceIdx], (int)PtSize))
				{
					if (FaceIdx < Face.Length())
					{
						FaceIdx++;
						continue;
					}
					else if (!Tmp->Create(SysFont->Face(), (int)PtSize))
					{
						break;
					}
				}
				
				int ActualHeight = FontPxHeight(Tmp);
				Diff = ActualHeight - RequestPx;
				if (Diff == 0)
				{
					// Best possible font size.
					BestFont = Tmp;
					BestPxDiff = Diff;
					break;
				}
				else if (abs(Diff) < BestPxDiff)
				{
					// Getting better... keep going
					BestFont = Tmp;
					BestPxDiff = Diff;
				}
				else
				{
					// Getting worse now... stop
					break;
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

			if (!BestFont)
				return Owner->GetFont();

			Fonts.Insert(f = BestFont.Release());
			if (!f || !f->Face())
			{
				LgiAssert(0);
			}
			return f;
		}
		else if (Size.Type == GCss::LenPt)
		{
			double Pt = MAX(MinimumPointSize, Size.Value);
			for (f=Fonts.First(); f; f=Fonts.Next())
			{
				if (!f->Face() || Face.Length() == 0)
				{
					LgiAssert(0);
					break;
				}
				
				if (f->Face() &&
					_stricmp(f->Face(), Face[0]) == 0 &&
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
		else if (Size.Type == GCss::SizeXXSmall ||
				Size.Type == GCss::SizeXSmall ||
				Size.Type == GCss::SizeSmall ||
				Size.Type == GCss::SizeMedium ||
				Size.Type == GCss::SizeLarge ||
				Size.Type == GCss::SizeXLarge ||
				Size.Type == GCss::SizeXXLarge)
		{
			int Idx = Size.Type-GCss::SizeXXSmall;
			LgiAssert(Idx >= 0 && Idx < CountOf(GCss::FontSizeTable));
			PtSize = Default->PointSize() * GCss::FontSizeTable[Idx];
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

		if ((f = new GFont))
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
			if (!f->Face())
			{
				LgiAssert(0);
			}
			
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
	GdcPt2 MAX;					// Max dimensions

	int InBody;

	GFlowRegion(GHtml *html, bool inbody)
	{
		Html = html;
		x1 = x2 = y1 = y2 = cx = my = 0;
		InBody = inbody;
	}

	GFlowRegion(GHtml *html, GRect r, bool inbody)
	{
		Html = html;
		MAX.x = cx = x1 = r.x1;
		MAX.y = y1 = y2 = r.y1;
		x2 = r.x2;
		my = 0;
		InBody = inbody;
	}

	GFlowRegion(GFlowRegion &r)
	{
		Html = r.Html;
		x1 = r.x1;
		x2 = r.x2;
		y1 = r.y1;
		MAX.x = cx = r.cx;
		MAX.y = y2 = r.y2;
		my = r.my;
		InBody = r.InBody; 
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

	void Indent(GRect &Px,
				bool IsMargin)
	{
		GFlowRegion This(*this);
		GFlowStack &Fs = Stack.New();

		Fs.LeftAbs = Px.x1;
		Fs.RightAbs = Px.x2;
		Fs.TopAbs = Px.y1;

		x1 += Fs.LeftAbs;
		cx += Fs.LeftAbs;
		x2 -= Fs.RightAbs;
		y1 += Fs.TopAbs;
		y2 += Fs.TopAbs;
		
		if (IsMargin)
			my += Fs.TopAbs;
	}

	void Outdent(GRect &Px,
				bool IsMargin)
	{
		GFlowRegion This = *this;

		ssize_t len = Stack.Length();
		if (len > 0)
		{
			GFlowStack &Fs = Stack[len-1];

			int &BottomAbs = Px.y2;

			x1 -= Fs.LeftAbs;
			cx -= Fs.LeftAbs;
			x2 += Fs.RightAbs;
			y2 += BottomAbs;
			if (IsMargin)
				my += BottomAbs;

			Stack.Length(len-1);
		}
		else LgiAssert(!"Nothing to pop.");
	}

	void Outdent(GFont *Font,
				GCss::Len Left,
				GCss::Len Top,
				GCss::Len Right,
				GCss::Len Bottom,
				bool IsMargin)
	{
		GFlowRegion This = *this;

		ssize_t len = Stack.Length();
		if (len > 0)
		{
			GFlowStack &Fs = Stack[len-1];

			int BottomAbs = Bottom.IsValid() ? ResolveY(Bottom, Font, IsMargin) : 0;

			x1 -= Fs.LeftAbs;
			cx -= Fs.LeftAbs;
			x2 += Fs.RightAbs;
			y2 += BottomAbs;
			if (IsMargin)
				my += BottomAbs;

			Stack.Length(len-1);
		}
		else LgiAssert(!"Nothing to pop.");
	}

	int ResolveX(GCss::Len l, GFont *f, bool IsMargin)
	{
		switch (l.Type)
		{
			default:
			case GCss::LenInherit:
				return IsMargin ? 0 : X();
			case GCss::LenPx:
				// return MIN((int)l.Value, X());
				return (int)l.Value;
			case GCss::LenPt:
				return (int) (l.Value * LgiScreenDpi() / 72.0);
			case GCss::LenCm:
				return (int) (l.Value * LgiScreenDpi() / 2.54);
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
			case GCss::SizeSmall:
			{
				return 1; // px
			}
			case GCss::SizeMedium:
			{
				return 2; // px
			}
			case GCss::SizeLarge:
			{
				return 3; // px
			}
		}

		return 0;
	}
	
	bool LimitX(int &x, GCss::Len Min, GCss::Len Max, GFont *f)
	{
		bool Limited = false;
		if (Min.IsValid())
		{
			int Px = Min.ToPx(x2 - x1 + 1, f, false);
			if (Px > x)
			{
				x = Px;
				Limited = true;
			}
		}
		if (Max.IsValid())
		{
			int Px = Max.ToPx(x2 - x1 + 1, f, false);
			if (Px < x)
			{
				x = Px;
				Limited = true;
			}
		}
		return Limited;
	}

	int ResolveY(GCss::Len l, GFont *f, bool IsMargin)
	{
		switch (l.Type)
		{
			case GCss::LenInherit:
			case GCss::LenAuto:
			case GCss::LenNormal:
			case GCss::LenPx:
				return (int)l.Value;
			case GCss::LenPt:
				return (int) (l.Value * LgiScreenDpi() / 72.0);
			case GCss::LenCm:
				return (int) (l.Value * LgiScreenDpi() / 2.54);

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
			{
				LgiAssert(Html != NULL);
				int TotalY = Html ? Html->Y() : 0;
				return (int) (((double)l.Value * TotalY) / 100);
			}
			case GCss::SizeSmall:
			{
				return 1; // px
			}
			case GCss::SizeMedium:
			{
				return 2; // px
			}
			case GCss::SizeLarge:
			{
				return 3; // px
			}
			case GCss::AlignLeft:
			case GCss::AlignRight:
			case GCss::AlignCenter:
			case GCss::AlignJustify:
			case GCss::VerticalBaseline:
			case GCss::VerticalSub:
			case GCss::VerticalSuper:
			case GCss::VerticalTop:
			case GCss::VerticalTextTop:
			case GCss::VerticalMiddle:
			case GCss::VerticalBottom:
			case GCss::VerticalTextBottom:
			{
				// Meaningless in this context
				break;
			}
			default:
			{
				LgiAssert(!"Not supported.");
				break;
			}
		}

		return 0;
	}

	bool LimitY(int &y, GCss::Len Min, GCss::Len Max, GFont *f)
	{
		bool Limited = false;
		int TotalY = Html ? Html->Y() : 0;
		if (Min.IsValid())
		{
			int Px = Min.ToPx(TotalY, f, false);
			if (Px > y)
			{
				y = Px;
				Limited = true;
			}
		}
		if (Max.IsValid())
		{
			int Px = Max.ToPx(TotalY, f, false);
			if (Px < y)
			{
				y = Px;
				Limited = true;
			}
		}
		return Limited;
	}

	GRect ResolveMargin(GCss *Src, GFont *Font)
	{
		GRect r;
		r.x1 = ResolveX(Src->MarginLeft(), Font, true);
		r.y1 = ResolveY(Src->MarginTop(), Font, true);
		r.x2 = ResolveX(Src->MarginRight(), Font, true);
		r.y2 = ResolveY(Src->MarginBottom(), Font, true);
		return r;
	}

	GRect ResolveBorder(GCss *Src, GFont *Font)
	{
		GRect r;
		r.x1 = ResolveX(Src->BorderLeft(), Font, true);
		r.y1 = ResolveY(Src->BorderTop(), Font, true);
		r.x2 = ResolveX(Src->BorderRight(), Font, true);
		r.y2 = ResolveY(Src->BorderBottom(), Font, true);
		return r;
	}

	GRect ResolvePadding(GCss *Src, GFont *Font)
	{
		GRect r;
		r.x1 = ResolveX(Src->PaddingLeft(), Font, true);
		r.y1 = ResolveY(Src->PaddingTop(), Font, true);
		r.x2 = ResolveX(Src->PaddingRight(), Font, true);
		r.y2 = ResolveY(Src->PaddingBottom(), Font, true);
		return r;
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

	d = (float)atof(s);

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
		default: break;
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

	float FlowX = Flow ? Flow->X() : d;
	return PrevAbs = MIN(FlowX, d);
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

	for (unsigned i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		if
		(
			*c == '#'
			||
			GHtmlStatic::Inst->ColourMap.Find(c)
		)
		{
			GHtmlParser::ParseColour(c, Colour);
		}
		else if (_strnicmp(c, "rgb(", 4) == 0)
		{
			char Buf[256];
			strcpy_s(Buf, sizeof(Buf), c);
			while (!strchr(c, ')') &&
					(c = t[++i]))
			{
				strcat(Buf, c);
			}
			GHtmlParser::ParseColour(Buf, Colour);
		}
		else if (IsDigit(*c))
		{
			GLength::Set(c);
		}
		else if (_stricmp(c, "none") == 0)
		{
			Style = 0;
		}
		else if (	_stricmp(c, "dotted") == 0 ||
					_stricmp(c, "dashed") == 0 ||
					_stricmp(c, "solid") == 0 ||
					_stricmp(c, "float") == 0 ||
					_stricmp(c, "groove") == 0 ||
					_stricmp(c, "ridge") == 0 ||
					_stricmp(c, "inset") == 0 ||
					_stricmp(c, "outse") == 0)
		{
			Style = c;
		}
		else
		{
			// ???
		}
	}

	if (Style && _stricmp(Style, "dotted") == 0)
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
		for (GTag *p = ToTag(Parent); p; p=ToTag(p->Parent))
		{
			r.Offset(p->Pos.x, p->Pos.y);
		}
	}
	return r;
}

GCss::LengthType GTag::GetAlign(bool x)
{
	for (GTag *t = this; t; t = ToTag(t->Parent))
	{
		GCss::Len l;
		
		if (x)
		{
			if (IsTableCell(TagId) && Cell && Cell->XAlign)
				l.Type = Cell->XAlign;
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
			// y2 = MAX(y2, Tr->y2);
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
		while ((r = Line.Next()))
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
	if (Tr) Line.Insert(Tr);
}

//////////////////////////////////////////////////////////////////////
GTag::GTag(GHtml *h, GHtmlElement *p) :
	GHtmlElement(p),
	Attr(8, false, NULL, NULL)
{
	Ctrl = 0;
	CtrlType = CtrlNone;
	TipId = 0;
	Display(DispInline);
	Html = h;
	
	ImageResized = false;
	Cursor = -1;
	Selection = -1;
	Font = 0;
	LineHeightCache = -1;
	HtmlId = NULL;
	// TableBorder = 0;
	Cell = NULL;
	TagId = CONTENT;
	Info = 0;
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

	DeleteObj(Ctrl);
	Attr.DeleteArrays();

	DeleteObj(Cell);
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
		Html->Environment->OnExecuteScript(Html, (char*)OnClick);
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
	GDomProperty Fld = LgiStringToDomProp(Name);
	switch (Fld)
	{
		case ObjStyle: // Type: GCssStyle
		{
			Value = &StyleDom;
			return true;
		}
		case ObjTextContent: // Type: String
		{
			Value = Text();
			return true;
		}
		default:
		{
			char *a = Attr.Find(Name);
			if (a)
			{
				Value = a;
				return true;
			}
			break;
		}
	}
	
	return false;
}

bool GTag::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty Fld = LgiStringToDomProp(Name);
	switch (Fld)
	{
		case ObjStyle:
		{
			const char *Defs = Value.Str();
			if (!Defs)
				return false;
				
			return Parse(Defs, ParseRelaxed);
		}
		case ObjTextContent:
		{
			const char *s = Value.Str();
			if (s)
			{
				GAutoWString w(CleanText(s, strlen(s), "utf-8", true, true));
				Txt = w;
				return true;
			}
			break;
		}
		case ObjInnerHtml: // Type: String
		{
			// Clear out existing tags..
			Children.DeleteObjects();
		
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
						Doc = Html->ParseHtml(t, Doc, 1, false, &BackOut);
						if (!Doc)
							break;
					}
					else break;
				}
			}
			else return false;
			break;
		}
		default:
		{
			Set(Name, Value.CastString());
			SetStyle();
			break;
		}
	}

	Html->ViewWidth = -1;
	return true;
}

ssize_t GTag::GetTextStart()
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
	ssize_t Len = sizeof(Buf);
	while (*Text)
	{
		#define WriteExistingContent() \
			if (s > Buf) \
				Out.Write(Buf, (int)(s - Buf)); \
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
	char *Tabs = new char[Depth+1];
	memset(Tabs, '\t', Depth);
	Tabs[Depth] = 0;

	if (ValidStr(Tag))
	{
		if (IsBlock())
		{
			p.Print("%s%s<%s", TagId != TAG_HTML ? "\n" : "", Tabs, Tag.Get());
		}
		else
		{
			p.Print("<%s", Tag.Get());
		}

		if (Attr.Length())
		{
			// const char *a;
			// for (char *v = Attr.First(&a); v; v = Attr.Next(&a))
			for (auto v : Attr)
			{
				if (_stricmp(v.key, "style"))
					p.Print(" %s=\"%s\"", v.key, v.value);
			}
		}
		if (Props.Length())
		{
			GCss *Css = this;
			GCss Tmp;
			#define DelProp(p) \
				if (Css == this) { Tmp = *Css; Css = &Tmp; } \
				Css->DeleteProp(p);
			
			// Clean out any default CSS properties where we can...
			GHtmlElemInfo *i = GHtmlStatic::Inst->GetTagInfo(Tag);
			if (i)
			{
				if (Props.Find(PropDisplay)
					&&
					(
						(!i->Block() && Display() == DispInline)
						||
						(i->Block() && Display() == DispBlock)
					))
				{
					DelProp(PropDisplay);
				}
				switch (TagId)
				{
					default:
						break;
					case TAG_A:
					{
						GCss::ColorDef Blue(GCss::ColorRgb, Rgb32(0, 0, 255));
						if (Props.Find(PropColor) && Color() == Blue)
							DelProp(PropColor);
						if (Props.Find(PropTextDecoration) && TextDecoration() == GCss::TextDecorUnderline)
							DelProp(PropTextDecoration)
						break;
					}
					case TAG_BODY:
					{
						GCss::Len FivePx(GCss::LenPx, 5.0f);
						if (Props.Find(PropPaddingLeft) && PaddingLeft() == FivePx)
							DelProp(PropPaddingLeft)
						if (Props.Find(PropPaddingTop) && PaddingTop() == FivePx)
							DelProp(PropPaddingTop)
						if (Props.Find(PropPaddingRight) && PaddingRight() == FivePx)
							DelProp(PropPaddingRight)
						break;
					}
					case TAG_B:
					{
						if (Props.Find(PropFontWeight) && FontWeight() == GCss::FontWeightBold)
							DelProp(PropFontWeight);
						break;
					}
					case TAG_U:
					{
						if (Props.Find(PropTextDecoration) && TextDecoration() == GCss::TextDecorUnderline)
							DelProp(PropTextDecoration);
						break;
					}
					case TAG_I:
					{
						if (Props.Find(PropFontStyle) && FontStyle() == GCss::FontStyleItalic)
							DelProp(PropFontStyle);
						break;
					}
				}
			}
			
			// Convert CSS props to a string and emit them...
			GAutoString s = Css->ToString();
			if (ValidStr(s))
			{			
				// Clean off any trailing whitespace...
				char *e = s ? s + strlen(s) : NULL;
				while (e && strchr(WhiteSpace, e[-1]))
					*--e = 0;
				
				// Print them to the tags attributes...
				p.Print(" style=\"%s\"", s.Get());
			}
		}
	}

	if (Children.Length() || TagId == TAG_STYLE) // <style> tags always have </style>
	{
		if (Tag)
		{
			p.Write((char*)">", 1);
			TextToStream(p, Text());
		}

		bool Last = IsBlock();
		for (unsigned i=0; i<Children.Length(); i++)
		{
			GTag *c = ToTag(Children[i]);
			c->CreateSource(p, Parent ? Depth+1 : 0, Last);
			Last = c->IsBlock();
		}

		if (Tag)
		{
			if (IsBlock())
			{
				if (Children.Length())
					p.Print("\n%s", Tabs);
			}

			p.Print("</%s>", Tag.Get());
		}
	}
	else if (Tag)
	{
		if (Text())
		{
			p.Write((char*)">", 1);
			TextToStream(p, Text());
			p.Print("</%s>", Tag.Get());
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
	Tag.Reset(NewStr(NewTag));

	if (NewTag)
	{
		Info = Html->GetTagInfo(Tag);
		if (Info)
		{
			TagId = Info->Id;
			Display(Info->Flags & GHtmlElemInfo::TI_BLOCK ? GCss::DispBlock : GCss::DispInline);
		}
	}
	else
	{
		Info = NULL;
		TagId = CONTENT;
	}

	SetStyle();
}

GColour GTag::_Colour(bool f)
{
	for (GTag *t = this; t; t = ToTag(t->Parent))
	{
		ColorDef c = f ? t->Color() : t->BackgroundColor();
		if (c.Type != ColorInherit)
		{
			return GColour(c.Rgb32, 32);
		}

		#if 1
		if (!f || t->TagId == TAG_TABLE)
			break;
		#else
		/*	This implements some basic level of colour inheritance for
			background colours. See test case 'cisra-cqs.html'. */
		if (!f && t->TagId == TAG_TABLE)
			break;
		#endif
	}

	return GColour();
}

void GTag::CopyClipboard(GMemQueue &p, bool &InSelection)
{
	ssize_t Min = -1;
	ssize_t Max = -1;

	if (Cursor >= 0 && Selection >= 0)
	{
		Min = MIN(Cursor, Selection);
		Max = MAX(Cursor, Selection);
	}
	else if (InSelection)
	{
		Max = MAX(Cursor, Selection);
	}
	else
	{
		Min = MAX(Cursor, Selection);
	}

	ssize_t Off = -1;
	ssize_t Chars = 0;
	if (Min >= 0 && Max >= 0)
	{
		Off = Min;
		Chars = Max - Min;
	}
	else if (Min >= 0)
	{
		Off = Min;
		Chars = StrlenW(Text()) - Min;
		InSelection = true;
	}
	else if (Max >= 0)
	{
		Off = 0;
		Chars = Max;
		InSelection = false;
	}
	else if (InSelection)
	{
		Off = 0;
		Chars = StrlenW(Text());
	}

	if (Off >= 0 && Chars > 0)
	{
		p.Write((uchar*) (Text() + Off), Chars * sizeof(char16));
	}

	if (InSelection)
	{
		switch (TagId)
		{
			default: break;
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

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *c = ToTag(Children[i]);
		c->CopyClipboard(p, InSelection);
	}
}

static
char*
_DumpColour(GCss::ColorDef c)
{
	static char Buf[4][32];
	#ifdef _MSC_VER
	static LONG Cur = 0;
	LONG Idx = InterlockedIncrement(&Cur);
	#else
	static int Cur = 0;
	int Idx = __sync_fetch_and_add(&Cur, 1);
	#endif
	char *b = Buf[Idx % 4];
	
	if (c.Type == GCss::ColorInherit)
		strcpy_s(b, 32, "Inherit");
	else
		sprintf_s(b, 32, "%2.2x,%2.2x,%2.2x(%2.2x)", R32(c.Rgb32),G32(c.Rgb32),B32(c.Rgb32),A32(c.Rgb32));

	return b;
}

void GTag::_Dump(GStringPipe &Buf, int Depth)
{
	GString Tabs;
	Tabs.Set(NULL, Depth);
	memset(Tabs.Get(), '\t', Depth);	

	const char *Empty = "";
	char *ElementName = TagId == CONTENT ? (char*)"Content" :
				(TagId == ROOT ? (char*)"Root" : Tag);

	Buf.Print(	"%s%s(%p)%s%s%s (%i) Pos=%i,%i Size=%i,%i Color=%s/%s",
				Tabs.Get(),
				ElementName,
				this,
				HtmlId ? "#" : Empty,
				HtmlId ? HtmlId : Empty,
				#ifdef _DEBUG
				Debug ? " debug" : Empty,
				#else
				Empty,
				#endif
				WasClosed,
				Pos.x, Pos.y,
				Size.x, Size.y,
				_DumpColour(Color()), _DumpColour(BackgroundColor()));

	for (unsigned i=0; i<TextPos.Length(); i++)
	{
		GFlowRect *Tr = TextPos[i];
		
		GAutoString Utf8(WideToUtf8(Tr->Text, Tr->Len));
		if (Utf8)
		{
			size_t Len = strlen(Utf8);
			if (Len > 40)
			{
				Utf8[40] = 0;
			}
		}
		else if (Tr->Text)
		{
			Utf8.Reset(NewStr(""));
		}

		Buf.Print("Tr(%i,%i %ix%i '%s') ", Tr->x1, Tr->y1, Tr->X(), Tr->Y(), Utf8.Get());
	}
	
	Buf.Print("\r\n");
	
	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
		t->_Dump(Buf, Depth+1);
	}

	if (Children.Length())
	{
		Buf.Print("%s/%s\r\n", Tabs.Get(), ElementName);
	}
}

GAutoWString GTag::DumpW()
{
	GStringPipe Buf;
	// Buf.Print("Html pos=%s\n", Html?Html->GetPos().GetStr():0);
	_Dump(Buf, 0);
	GAutoString a(Buf.NewStr());
	GAutoWString w(Utf8ToWide(a));
	return w;
}

GAutoString GTag::DescribeElement()
{
	GStringPipe s(256);
	s.Print("%s", Tag ? Tag.Get() : "CONTENT");
	if (HtmlId)
		s.Print("#%s", HtmlId);
	for (unsigned i=0; i<Class.Length(); i++)
		s.Print(".%s", Class[i].Get());
	return GAutoString(s.NewStr());
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
			GCss c;
			
            GCss::PropMap Map;
            Map.Add(PropFontFamily, new GCss::PropArray);
			Map.Add(PropFontSize, new GCss::PropArray);
			Map.Add(PropFontStyle, new GCss::PropArray);
			Map.Add(PropFontVariant, new GCss::PropArray);
			Map.Add(PropFontWeight, new GCss::PropArray);
			Map.Add(PropTextDecoration, new GCss::PropArray);

			for (GTag *t = this; t; t = ToTag(t->Parent))
			{
				if (!c.InheritCollect(*t, Map))
					break;
			}
			
			c.InheritResolve(Map);
			
			Map.DeleteObjects();

			if ((Font = Html->FontCache->GetFont(&c)))
				return Font;
		}
		else
		{
			GTag *t = this;
			while (!t->Font && t->Parent)
			{
				t = ToTag(t->Parent);
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
		ssize_t i = Parent->Children.IndexOf(this);
		if (i >= 0)
		{
			return ToTag(Parent->Children[i - 1]);
		}
	}

	return 0;
}

void GTag::Invalidate()
{
	GRect p = GetRect();
	for (GTag *t=ToTag(Parent); t; t=ToTag(t->Parent))
	{
		p.Offset(t->Pos.x, t->Pos.y);
	}
	Html->Invalidate(&p);
}

GTag *GTag::IsAnchor(GAutoString *Uri)
{
	GTag *a = 0;
	for (GTag *t = this; t; t = ToTag(t->Parent))
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
			GAutoWString w(CleanText(u, strlen(u), "utf-8"));
			if (w)
			{
				Uri->Reset(WideToUtf8(w));
			}
		}
	}

	return a;
}

bool GTag::OnMouseClick(GMouse &m)
{
	bool Processed = false;

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
			p.Print("Tag: %s\n", Tag ? Tag.Get() : "CONTENT");
			if (Class.Length())
			{
				p.Print("Class(es): ");
				for (unsigned i=0; i<Class.Length(); i++)
				{
					p.Print("%s%s", i?", ":"", Class[i].Get());
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
			for (unsigned i=0; i<s.Length(); i++)
				p.Print("    %s\n", s[i]);
			
			p.Print("\nParent tags:\n");
			GDisplayString Sp(SysFont, " ");
			for (GTag *t=ToTag(Parent); t && t->Parent; t=ToTag(t->Parent))
			{
				GStringPipe Tmp;
				Tmp.Print("    %s", t->Tag ? t->Tag.Get() : "CONTENT");
				if (t->HtmlId)
				{
					Tmp.Print("#%s", t->HtmlId);
				}
				for (unsigned i=0; i<t->Class.Length(); i++)
				{
					Tmp.Print(".%s", t->Class[i].Get());
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
					if (Uri)
					{
						if (!Html->d->LinkDoubleClick || m.Double())
						{
							Html->Environment->OnNavigate(Html, Uri);
							Processed = true;
						}
					}
					
					const char *OnClk = NULL;
					if (!Processed && Get("onclick", OnClk))
					{
						Html->Environment->OnExecuteScript(Html, (char*)OnClk);
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

GTag *GTag::GetBlockParent(ssize_t *Idx)
{
	if (IsBlock())
	{
		if (Idx)
			*Idx = 0;

		return this;
	}

	for (GTag *t = this; t; t = ToTag(t->Parent))
	{
		if (ToTag(t->Parent)->IsBlock())
		{
			if (Idx)
			{
				*Idx = t->Parent->Children.IndexOf(t);
			}

			return ToTag(t->Parent);
		}
	}

	return 0;
}

GTag *GTag::GetAnchor(char *Name)
{
	if (!Name)
		return 0;

	const char *n;
	if (IsAnchor(0) && Get("name", n) && n && !_stricmp(Name, n))
	{
		return this;
	}

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
		GTag *Result = t->GetAnchor(Name);
		if (Result) return Result;
	}

	return 0;
}

GTag *GTag::GetTagByName(const char *Name)
{
	if (Name)
	{
		if (Tag && _stricmp(Tag, Name) == 0)
		{
			return this;
		}

		for (unsigned i=0; i<Children.Length(); i++)
		{
			GTag *t = ToTag(Children[i]);
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

ssize_t GTag::NearestChar(GFlowRect *Tr, int x, int y)
{
	GFont *f = GetFont();
	if (f)
	{
		GDisplayString ds(f, Tr->Text, Tr->Len);
		ssize_t c = ds.CharAt(x - Tr->x1);
		
		if (Tr->Text == PreText())
		{
			return 0;
		}
		else
		{
			char16 *t = Tr->Text + c;
			size_t Len = StrlenW(Text());
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

void GTag::GetTagByPos(GTagHit &TagHit, int x, int y, int Depth, bool InBody, bool DebugLog)
{
	/*
	InBody: Originally I had this test in the code but it seems that some test cases
	have actual content after the body. And testing for "InBody" breaks functionality
	for those cases (see "spam4.html" and the unsubscribe link at the end of the doc).	
	*/

	if (TagId == TAG_IMG)
	{
		GRect img(0, 0, Size.x - 1, Size.y - 1);
		if (/*InBody &&*/ img.Overlap(x, y))
		{
			TagHit.Direct = this;
			TagHit.Block = 0;
		}
	}
	else if (/*InBody &&*/ TextPos.Length())
	{
		for (unsigned i=0; i<TextPos.Length(); i++)
		{
			GFlowRect *Tr = TextPos[i];
			if (!Tr)
				break;
			
			bool SameRow = y >= Tr->y1 && y <= Tr->y2;			
			int Near = IsNearRect(Tr, x, y);
			if (Near >= 0 && Near < 100)
			{
				if
				(
					!TagHit.NearestText
					||
					(
						SameRow
						&&
						!TagHit.NearSameRow
					)
					||
					(
						SameRow == TagHit.NearSameRow
						&&
						Near < TagHit.Near
					)
				)
				{
					TagHit.NearestText = this;
					TagHit.NearSameRow = SameRow;
					TagHit.Block = Tr;
					TagHit.Near = Near;
					TagHit.Index = NearestChar(Tr, x, y);
					
					if (DebugLog)
					{
						LgiTrace("%i:GetTagByPos HitText %s #%s, idx=%i, near=%i, txt='%S'\n",
							Depth,
							Tag.Get(),
							HtmlId,
							TagHit.Index,
							TagHit.Near,
							Tr->Text);
					}

					if (!TagHit.Near)
					{
						TagHit.Direct = this;
						TagHit.LocalCoords.x = x;
						TagHit.LocalCoords.y = y;
					}
				}
			}
		}
	}
	else if
	(
		TagId != TAG_TR &&
		Tag &&
		x >= 0 &&
		y >= 0 &&
		x < Size.x &&
		y < Size.y
		// && InBody
	)
	{
		// Direct hit
		TagHit.Direct = this;
		TagHit.LocalCoords.x = x;
		TagHit.LocalCoords.y = y;

		if (DebugLog)
		{
			LgiTrace("%i:GetTagByPos DirectHit %s #%s, idx=%i, near=%i\n",
				Depth,
				Tag.Get(),
				HtmlId,
				TagHit.Index,
				TagHit.Near);
		}
	}
	
	if (TagId == TAG_BODY)
		InBody = true;

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
		if (t->Pos.x >= 0 &&
			t->Pos.y >= 0)
		{
			t->GetTagByPos(TagHit, x - t->Pos.x, y - t->Pos.y, Depth + 1, InBody, DebugLog);
		}
	}
}

int GTag::OnNotify(int f)
{
	if (!Ctrl || !Html->InThread())
		return 0;
	
	switch (CtrlType)
	{
		case CtrlSubmit:
		{
			GTag *Form = this;
			while (Form && Form->TagId != TAG_FORM)
				Form = ToTag(Form->Parent);
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

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
		t->CollectFormValues(f);
	}
}

GTag *GTag::FindCtrlId(int Id)
{
	if (Ctrl && Ctrl->GetId() == Id)
		return this;

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
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
	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
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
				GAutoPtr<GSurface> t(new GMemDC(r.X(), r.Y(), Image->GetColourSpace()));
				if (t)
				{
					t->Blt(0, 0, Image, &r);
					Image = t;
				}
			}
		}		

		for (unsigned i=0; i<Children.Length(); i++)
		{
			GTag *t = ToTag(Children[i]);
			if (t->Cell)
			{
				t->Cell->MinContent = 0;
				t->Cell->MaxContent = 0;
			}
		}
	}
	else
	{
		Html->d->Loading.Add(Uri, this);
	}
}

void GTag::LoadImage(const char *Uri)
{
	#if DOCUMENT_LOAD_IMAGES
	if (!Html->Environment)
		return;

	GUri u(Uri);
	bool LdImg = Html->GetLoadImages();
	bool IsRemote = u.Protocol &&
					(
						!_stricmp(u.Protocol, "http") ||
						!_stricmp(u.Protocol, "https") ||
						!_stricmp(u.Protocol, "ftp")
					);
	if (IsRemote && !LdImg)
	{
		Html->NeedsCapability("RemoteContent");
		return;
	}

	GDocumentEnv::LoadJob *j = Html->Environment->NewJob();
	if (j)
	{
		LgiAssert(Html != NULL);
		j->Uri.Reset(NewStr(Uri));
		j->Env = Html->Environment;
		j->UserData = this;
		j->UserUid = Html->GetDocumentUid();

		GDocumentEnv::LoadType Result = Html->Environment->GetContent(j);
		if (Result == GDocumentEnv::LoadImmediate)
		{
			SetImage(Uri, j->pDC.Release());
		}
		else if (Result == GDocumentEnv::LoadDeferred)
		{
			Html->d->DeferredLoads++;
		}
				
		DeleteObj(j);
	}
	#endif
}

void GTag::LoadImages()
{
	const char *Uri = 0;
	if (Html->Environment &&
		TagId == TAG_IMG &&
		!Image &&
		Get("src", Uri))
	{
		LoadImage(Uri);
	}

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
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

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
		t->ImageLoaded(uri, Img, Used);
	}
}

struct GTagElementCallback : public GCss::ElementCallback<GTag>
{
	const char *Val;

	const char *GetElement(GTag *obj)
	{
		return obj->Tag;
	}
	
	const char *GetAttr(GTag *obj, const char *Attr)
	{
		if (obj->Get(Attr, Val))
			return Val;
		return NULL;
	}
	
	bool GetClasses(GString::Array &Classes, GTag *obj) 
	{
		Classes = obj->Class;
		return Classes.Length() > 0;
	}
	
	GTag *GetParent(GTag *obj)
	{
		return ToTag(obj->Parent);
	}
	
	GArray<GTag*> GetChildren(GTag *obj)
	{
		GArray<GTag*> c;
		for (unsigned i=0; i<obj->Children.Length(); i++)
			c.Add(ToTag(obj->Children[i]));
		return c;
	}
};

void GTag::RestyleAll()
{
	Restyle();
	for (unsigned i=0; i<Children.Length(); i++)
	{
		GHtmlElement *c = Children[i];
		GTag *t = ToTag(c);
		if (t)
			t->RestyleAll();
	}
}

// After CSS has changed this function scans through the CSS and applies any rules
// that match the current tag.
void GTag::Restyle()
{
	// Use the matching built into the GCss Store.
	GCss::SelArray Styles;
	GTagElementCallback Context;
	if (Html->CssStore.Match(Styles, &Context, this))
	{
		for (unsigned i=0; i<Styles.Length(); i++)
		{
			GCss::Selector *s = Styles[i];
			if (s)
				SetCssStyle(s->Style);
		}
	}
	
	// Do the element specific styles
	const char *s;
	if (Get("style", s))
		SetCssStyle(s);
	
	#if DEBUG_RESTYLE && defined(_DEBUG)
	if (Debug)
	{
		GAutoString Style = ToString();
		LgiTrace(">>>> %s <<<<:\n%s\n\n", Tag.Get(), Style.Get());
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
	{
		if ((Debug = atoi(s)))
		{
			// LgiTrace("Debug Tag: %p '%s'\n", this, Tag ? Tag.Get() : "CONTENT");
		}
	}
	#endif

	if (Get("Color", s))
	{
		ColorDef Def;
		if (GHtmlParser::ParseColour(s, Def))
		{
			Color(Def);
		}
	}

	if (Get("Background", s) ||
		Get("bgcolor", s))
	{
		ColorDef Def;
		if (GHtmlParser::ParseColour(s, Def))
		{
			BackgroundColor(Def);
		}
		else
		{
			GCss::ImageDef Img;
			
			Img.Type = ImageUri;
			Img.Uri = s;
			
			BackgroundImage(Img);
			BackgroundRepeat(RepeatBoth);
		}
	}

	switch (TagId)
	{
		default:
			break;
		case TAG_LINK:
		{
			const char *Type, *Href;
			if (Html->Environment &&
				Get("type", Type) &&
				Get("href", Href) &&
				!_stricmp(Type, "text/css") &&
				!Html->CssHref.Find(Href))
			{
				GDocumentEnv::LoadJob *j = Html->Environment->NewJob();
				if (j)
				{
					LgiAssert(Html != NULL);
					GTag *t = this;
					
					j->Uri.Reset(NewStr(Href));
					j->Env = Html->Environment;
					j->UserData = t;
					j->UserUid = Html->GetDocumentUid();

					GDocumentEnv::LoadType Result = Html->Environment->GetContent(j);
					if (Result == GDocumentEnv::LoadImmediate)
					{
						GStreamI *s = j->GetStream();							
						if (s)
						{
							int Len = (int)s->GetSize();
							if (Len > 0)
							{
								GAutoString a(new char[Len+1]);
								ssize_t r = s->Read(a, Len);
								a[r] = 0;

								Html->CssHref.Add(Href, true);
								Html->OnAddStyle("text/css", a);
							}
						}
					}
					else if (Result == GDocumentEnv::LoadDeferred)
					{
						Html->d->DeferredLoads++;
					}

					DeleteObj(j);
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
				if (_stricmp(s, "cite") == 0)
				{
					BorderLeft(BorderDef(this, "1px solid blue"));
					PaddingLeft(Len("0.5em"));
					
					/*
					ColorDef Def;
					Def.Type = ColorRgb;
					Def.Rgb32 = Rgb32(0x80, 0x80, 0x80);
					Color(Def);
					*/
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
			}
			break;
		}
		case TAG_TABLE:
		{
			Len l;

			if (!Cell)
				Cell = new TblCell;

			if (Get("border", s))
			{
				BorderDef b;
				if (b.Parse(this, s))
				{
					BorderLeft(b);
					BorderRight(b);
					BorderTop(b);
					BorderBottom(b);
				}
			}

			if (Get("cellspacing", s) &&
				l.Parse(s, PropBorderSpacing, ParseRelaxed))
			{
				BorderSpacing(l);
			}
			else
			{
				// BorderSpacing(GCss::Len(GCss::LenPx, 2.0f));
			}

			if (Get("cellpadding", s) &&
				l.Parse(s, Prop_CellPadding, ParseRelaxed))
			{
				_CellPadding(l);
			}

			if (Get("align", s))
			{
				Len l;
				if (l.Parse(s))
					Cell->XAlign = l.Type;
			}
			break;
		}
		case TAG_TD:
		case TAG_TH:
		{
			if (!Cell)
				Cell = new TblCell;

			GTag *Table = GetTable();
			if (Table)
			{
				Len l = Table->_CellPadding();
				if (!l.IsValid())
				{
					l.Type = GCss::LenPx;
					l.Value = DefaultCellPadding;
				}
				PaddingLeft(l);
				PaddingRight(l);
				PaddingTop(l);
				PaddingBottom(l);
			}
			
			if (TagId == TAG_TH)
				FontWeight(GCss::FontWeightBold);
			break;
		}
		case TAG_BODY:
		{
			MarginLeft(Len(Get("leftmargin", s) ? s : DefaultBodyMargin));
			MarginTop(Len(Get("topmargin", s) ? s : DefaultBodyMargin));
			MarginRight(Len(Get("rightmargin", s) ? s : DefaultBodyMargin));
			
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
	}

	if (Get("width", s))
	{
		Len l;
		if (l.Parse(s, PropWidth, ParseRelaxed))
		{
			Width(l);
		}
	}
	if (Get("height", s))
	{
		Len l;
		if (l.Parse(s, PropHeight, ParseRelaxed))
			Height(l);
	}
	if (Get("align", s))
	{
		if (_stricmp(s, "left") == 0) TextAlign(Len(AlignLeft));
		else if (_stricmp(s, "right") == 0) TextAlign(Len(AlignRight));
		else if (_stricmp(s, "center") == 0) TextAlign(Len(AlignCenter));
	}
	if (Get("valign", s))
	{
		if (_stricmp(s, "top") == 0) VerticalAlign(Len(VerticalTop));
		else if (_stricmp(s, "middle") == 0) VerticalAlign(Len(VerticalMiddle));
		else if (_stricmp(s, "bottom") == 0) VerticalAlign(Len(VerticalBottom));
	}

	Get("id", HtmlId);

	if (Get("class", s))
	{
		Class = GString(s).SplitDelimit(" \t");
	}

	Restyle();

	switch (TagId)
	{
		default: break;
	    case TAG_BIG:
	    {
            GCss::Len l;
            l.Type = SizeLarger;
	        FontSize(l);
	        break;
	    }
	    /*
		case TAG_META:
		{
			GAutoString Cs;

			const char *s;
			if (Get("http-equiv", s) &&
				_stricmp(s, "Content-Type") == 0)
			{
				const char *ContentType;
				if (Get("content", ContentType))
				{
					char *CharSet = stristr(ContentType, "charset=");
					if (CharSet)
					{
						char16 *cs = NULL;
						Html->ParsePropValue(CharSet + 8, cs);
						Cs.Reset(WideToUtf8(cs));
						DeleteArray(cs);
					}
				}
			}

			if (Get("name", s) && _stricmp(s, "charset") == 0 && Get("content", s))
			{
				Cs.Reset(NewStr(s));
			}
			else if (Get("charset", s))
			{
				Cs.Reset(NewStr(s));
			}

			if (Cs)
			{
				if (Cs &&
					_stricmp(Cs, "utf-16") != 0 &&
					_stricmp(Cs, "utf-32") != 0 &&
					LgiGetCsInfo(Cs))
				{
					// Html->SetCharset(Cs);
				}
			}
			break;
		}
		*/
		case TAG_BODY:
		{
			GCss::ColorDef Bk = BackgroundColor();
			if (Bk.Type != ColorInherit)
			{
				// Copy the background up to the GHtml wrapper
				Html->GetCss(true)->BackgroundColor(Bk);
			}
			
			/*
			GFont *f = GetFont();
			if (FontSize().Type == LenInherit)
			{
       		    FontSize(Len(LenPt, (float)f->PointSize()));
			}
			*/
			break;
		}
		case TAG_HEAD:
		{
			Display(DispNone);
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
		case TAG_TR:
			break;
		case TAG_TD:
		case TAG_TH:
		{
			LgiAssert(Cell != NULL);
			
			const char *s;
			if (Get("colspan", s))
				Cell->Span.x = atoi(s);
			else
				Cell->Span.x = 1;
			if (Get("rowspan", s))
				Cell->Span.y = atoi(s);
			else
				Cell->Span.y = 1;
			
			Cell->Span.x = MAX(Cell->Span.x, 1);
			Cell->Span.y = MAX(Cell->Span.y, 1);
			
			if (Display() == DispInline ||
				Display() == DispInlineBlock)
			{
				Display(DispBlock); // Inline-block TD??? Nope.
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
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[5]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H2:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[4]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H3:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[3]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H4:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[2]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H5:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[1]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H6:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[0]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_FONT:
		{
			const char *s = 0;
			if (Get("Face", s))
			{
				char16 *cw = CleanText(s, strlen(s), "utf-8", true);
				char *c8 = WideToUtf8(cw);
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
		case TAG_SELECT:
		{
			if (!Html->InThread())
				break;

			LgiAssert(!Ctrl);
			Ctrl = new GCombo(Html->d->NextCtrlId++, 0, 0, 100, SysFont->GetHeight() + 8, NULL);
			CtrlType = CtrlSelect;
			break;
		}
		case TAG_INPUT:
		{
			if (!Html->InThread())
				break;

			LgiAssert(!Ctrl);

			const char *Type, *Value = NULL;
			Get("value", Value);
			GAutoWString CleanValue(Value ? CleanText(Value, strlen(Value), "utf-8", true, true) : NULL);
			if (CleanValue)
			{
				CtrlValue = CleanValue;
			}
			
			if (Get("type", Type))
			{
				if (!_stricmp(Type, "password")) CtrlType = CtrlPassword;
				else if (!_stricmp(Type, "email")) CtrlType = CtrlEmail;
				else if (!_stricmp(Type, "text")) CtrlType = CtrlText;
				else if (!_stricmp(Type, "button")) CtrlType = CtrlButton;
				else if (!_stricmp(Type, "submit")) CtrlType = CtrlSubmit;
				else if (!_stricmp(Type, "hidden")) CtrlType = CtrlHidden;

				DeleteObj(Ctrl);
				if (CtrlType == CtrlEmail ||
					CtrlType == CtrlText ||
					CtrlType == CtrlPassword)
				{
					GEdit *Ed;
					GAutoString UtfCleanValue(WideToUtf8(CleanValue));
					Ctrl = Ed = new GEdit(Html->d->NextCtrlId++, 0, 0, 60, SysFont->GetHeight() + 8, UtfCleanValue);
					if (Ctrl)
					{
						Ed->Sunken(false);
						Ed->Password(CtrlType == CtrlPassword);
					}						
				}
				else if (CtrlType == CtrlButton ||
						 CtrlType == CtrlSubmit)
				{
					GAutoString UtfCleanValue(WideToUtf8(CleanValue));
					if (UtfCleanValue)
					{
						Ctrl = new InputButton(this, Html->d->NextCtrlId++, UtfCleanValue);
					}
				}
			}
			break;
		}
	}

	if (IsBlock())
	{
		GCss::ImageDef bk = BackgroundImage();
		if (bk.Type == GCss::ImageUri &&
			ValidStr(bk.Uri))
		{
			LoadImage(bk.Uri);
		} 
	}
	
	if (Ctrl)
	{
		GFont *f = GetFont();
		if (f)
			Ctrl->SetFont(f, false);
	}

	if (Display() == DispBlock && Html->Environment)
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
		while ((Comment = strstr((char*)Style, "/*")))
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
	}
}

char16 *GTag::CleanText(const char *s, ssize_t Len, const char *SourceCs,  bool ConversionAllowed, bool KeepWhiteSpace)
{
	if (!s || Len <= 0)
		return NULL;

	static const char *DefaultCs = "iso-8859-1";
	char16 *t = 0;
	bool DocAndCsTheSame = false;
	if (Html->DocCharSet && Html->Charset)
	{
		DocAndCsTheSame = _stricmp(Html->DocCharSet, Html->Charset) == 0;
	}

	if (SourceCs)
	{
		t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, SourceCs, Len);
	}
	else if (Html->DocCharSet &&
		Html->Charset &&
		!DocAndCsTheSame &&
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
		t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, Html->Charset.Get() ? Html->Charset.Get() : DefaultCs, Len);
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
								*p++ = (char)*i++;
							}
						}
						else
						{
							// Decimal number
							while (*i && IsDigit(*i) && (p - n) < 31)
							{
								*p++ = (char)*i++;
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
	Tag.Reset(NewStr("body"));
	Info = Html->GetTagInfo(Tag);
	char *OriginalCp = NewStr(Html->Charset);

	GStringPipe Utf16;
	char *s = Doc;
	while (s)
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
			char16 *t = CleanText(s, e - s, NULL, false);
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
					t->Color(ColorDef(ColorRgb, Rgb24To32(LC_TEXT)));
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
					t->Tag.Reset(NewStr("br"));
					t->Info = Html->GetTagInfo(t->Tag);
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
			GAutoWString t(CleanText(s, e - s, NULL, false));
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
		default: break;
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

		GFont *f = GetFont();
		GAutoString u;
		if (f)
			u = f->ConvertToUnicode(Txt);
		else
			u.Reset(WideToUtf8(Txt));
		if (u)
		{
			size_t u_len = strlen(u);
			State.Write(u, u_len);
		}
	}
	
	State.Depth += DepthInc;
	
	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *c = ToTag(Children[i]);
		c->ConvertToText(State);
	}

	State.Depth -= DepthInc;
	
	if (IsBlock())
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
					if (_strnicmp(Href, "mailto:", 7) == 0)
						Href += 7;
						
					size_t HrefLen = strlen(Href);
					GAutoWString h(CleanText(Href, HrefLen, "utf-8"));
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

bool GTag::OnUnhandledColor(GCss::ColorDef *def, const char *&s)
{
	const char *e = s;
	while (*e && (IsText(*e) || *e == '_'))
		e++;

	char tmp[256];
	ssize_t len = e - s;
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
		IsTableCell(TagId))
	{
		Size.x = 0;
		Size.y = 0;
		if (Cell)
		{
			Cell->MinContent = 0;
			Cell->MaxContent = 0;
		}

		for (unsigned i=0; i<Children.Length(); i++)
		{
			GTag *t = ToTag(Children[i]);
			t->ZeroTableElements();
		}
	}
}

void GTag::ResetCaches()
{
	/*
	If during the parse process a callback causes a layout to happen then it's possible
	to have partial information in the GHtmlTableLayout structure, like missing TD cells.
	Because they haven't been parsed yet.
	This is called at the end of the parsing to reset all the cached info in GHtmlTableLayout.
	That way when the first real layout happens all the data is there.
	*/
	if (Cell)
		DeleteObj(Cell->Cells);
	for (size_t i=0; i<Children.Length(); i++)
		ToTag(Children[i])->ResetCaches();
}

GdcPt2 GTag::GetTableSize()
{
	GdcPt2 s(0, 0);
	
	if (Cell && Cell->Cells)
	{
		Cell->Cells->GetSize(s.x, s.y);
	}

	return s;
}

GTag *GTag::GetTableCell(int x, int y)
{
	GTag *t = this;
	while (	t &&
			!t->Cell &&
			!t->Cell->Cells &&
			t->Parent)
	{
		t = ToTag(t->Parent);
	}
	
	if (t && t->Cell && t->Cell->Cells)
	{
		return t->Cell->Cells->Get(x, y);
	}

	return 0;
}

// This function gets the largest and smallest piece of content
// in this cell and all it's children.
bool GTag::GetWidthMetrics(GTag *Table, uint16 &Min, uint16 &Max)
{
	bool Status = true;
	int MarginPx = 0;
	int LineWidth = 0;

	if (Display() == GCss::DispNone)
		return true;

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
				ssize_t Len = e - s;
				if (Len > 0)
				{
					GDisplayString ds(f, s, Len);
					MinContent = MAX(MinContent, ds.X());
				}
				
				// Move to the next word.
				s = (*e) ? e + 1 : 0;
			}

			GDisplayString ds(f, Text());
			LineWidth = MaxContent = ds.X();
		}

		#ifdef _DEBUG
		if (Debug)
		{
			LgiTrace("GetWidthMetrics Font=%p Sz=%i,%i\n", f, MinContent, MaxContent);
		}
		#endif
		
		Min = MAX(Min, MinContent);
		Max = MAX(Max, MaxContent);
	}

	// Specific tag handling?
	switch (TagId)
	{
		default:
		{
			if (IsBlock())
			{
				MarginPx = (int)(BorderLeft().ToPx() +
								BorderRight().ToPx() +
								PaddingLeft().ToPx() +
								PaddingRight().ToPx());
			}
			break;
		}
		case TAG_IMG:
		{
			Len w = Width();
			if (w.IsValid())
			{
				int x = (int) w.Value;
				Min = MAX(Min, x);
				Max = MAX(Max, x);
			}
			else if (Image)
			{
				Min = Max = Image->X();
			}
			else
			{
				Size.x = Size.y = DefaultImgSize;
				Min = MAX(Min, Size.x);
				Max = MAX(Max, Size.x);
			}
			break;
		}
		case TAG_TD:
		case TAG_TH:
		{
			Len w = Width();
			if (w.IsValid())
			{
				if (w.IsDynamic())
				{
					Min = MAX(Min, (int)w.Value);
					Max = MAX(Max, (int)w.Value);
				}
				else
				{
					Max = w.ToPx(0, GetFont());
				}
			}
			else
			{
				GCss::BorderDef BLeft = BorderLeft();
				GCss::BorderDef BRight = BorderRight();
				GCss::Len PLeft = PaddingLeft();
				GCss::Len PRight = PaddingRight();
				
				MarginPx = (int)(PLeft.ToPx()  +
								PRight.ToPx()  + 
								BLeft.ToPx());
				
				if (Table->BorderCollapse() == GCss::CollapseCollapse)
					MarginPx += BRight.ToPx();
			}
			break;
		}
		case TAG_TABLE:
		{
			Len w = Width();
			if (w.IsValid() && !w.IsDynamic())
			{
				// Fixed width table...
				int CellSpacing = BorderSpacing().ToPx(Min, GetFont());
				
				int Px = ((int)w.Value) + (CellSpacing << 1);
				Min = MAX(Min, Px);
				Max = MAX(Max, Px);
				return true;
			}
			else
			{
				GdcPt2 s;
				GHtmlTableLayout c(this);
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
							if (t->GetWidthMetrics(Table, a, b))
							{
								ColMin[x] = MAX(ColMin[x], a);
								ColMax[x] = MAX(ColMax[x], b);
							}
							
							x += t->Cell->Span.x;
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
				
				Min = MAX(Min, MinSum);
				Max = MAX(Max, MaxSum);
				return true;
			}
			break;
		}
	}

	for (unsigned i = 0; i < Children.Length(); i++)
	{
		GTag *c = ToTag(Children[i]);
		uint16 TagMax = 0;
		
		Status &= c->GetWidthMetrics(Table, Min, TagMax);
		LineWidth += TagMax;
		if (c->TagId == TAG_BR ||
			c->TagId == TAG_LI)
		{
			Max = MAX(Max, LineWidth);
			LineWidth = 0;
		}
	}
	Max = MAX(Max, LineWidth);

	Min += MarginPx;
	Max += MarginPx;
	
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
	for (unsigned i=0; i<a.Length(); i++)
		s += a[i];
	return s;
}

void GTag::LayoutTable(GFlowRegion *f, uint16 Depth)
{
	if (!Cell->Cells)
	{
		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Debug)
		{
			//int asd=0;
		}
		#endif
		Cell->Cells = new GHtmlTableLayout(this);
		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Cell->Cells && Debug)
			Cell->Cells->Dump();
		#endif
	}
	if (Cell->Cells)
		Cell->Cells->LayoutTable(f, Depth);
}

void GHtmlTableLayout::AllocatePx(int StartCol, int Cols, int MinPx, bool HasToFillAllAvailable)
{
	// Get the existing total size and size of the column set
	int CurrentTotalX = GetTotalX();
	int CurrentSpanX = GetTotalX(StartCol, Cols);
	int MaxAdditionalPx = AvailableX - CurrentTotalX;
	if (MaxAdditionalPx <= 0)
		return;

	// Calculate the maximum space we have for this column set
	int AvailPx = (CurrentSpanX + MaxAdditionalPx) - BorderX1 - BorderX2;
	
	// Allocate any remaining space...
	int RemainingPx = MaxAdditionalPx;
	GArray<int> Growable, NonGrowable, SizeInherit;
	int GrowablePx = 0;
	for (int x=StartCol; x<StartCol+Cols; x++)
	{
		int DiffPx = MaxCol[x] - MinCol[x];
		if (DiffPx > 0)
		{
			GrowablePx += DiffPx;
			Growable.Add(x);
		}
		else if (MinCol[x] > 0)
		{
			NonGrowable.Add(x);
		}
		else if (MinCol[x] == 0 && CurrentSpanX < AvailPx)
		{
			// Growable.Add(x);
		}
		
		if (SizeCol[x].Type == GCss::LenInherit)
			SizeInherit.Add(x);
	}
	if (GrowablePx < RemainingPx && HasToFillAllAvailable)
	{
		if (Growable.Length() == 0)
		{
			// Add any suitable non-growable columns as well
			for (unsigned i=0; i<NonGrowable.Length(); i++)
			{
				int Col = NonGrowable[i];
				
				GCss::Len c = SizeCol[Col];
				if (c.Type != GCss::LenPercent && c.IsDynamic())
					Growable.Add(Col);
			}
		}
		
		if (Growable.Length() == 0)
		{
			// Still nothing to grow... so just pick the largest column
			int Largest = -1;
			for (int i=StartCol; i<StartCol+Cols; i++)
			{
				if (Largest < 0 || MinCol[i] > MinCol[Largest])
					Largest = i;
			}
			Growable.Add(Largest);
		}
	}
	
	if (Growable.Length())
	{
		// Some growable columns...
		int Added = 0;

		// Reasonably increase the size of the columns...
		for (unsigned i=0; i<Growable.Length(); i++)
		{
			int x = Growable[i];
			int DiffPx = MaxCol[x] - MinCol[x];
			int AddPx = 0;
			if (GrowablePx < RemainingPx && DiffPx > 0)
			{
				AddPx = DiffPx;
			}
			else if (DiffPx > 0)
			{
				double Ratio = (double)DiffPx / GrowablePx;
				AddPx = (int) (Ratio * RemainingPx);
			}
			else
			{
				AddPx = RemainingPx / Growable.Length();
			}
								
			LgiAssert(AddPx >= 0);
			MinCol[x] += AddPx;
			Added += AddPx;
		}

		if (Added < RemainingPx && HasToFillAllAvailable)
		{
			// Still more to add, so
			if (SizeInherit.Length())
			{
				Growable = SizeInherit;
			}
			else
			{
				int Largest = -1;
				for (unsigned i=0; i<Growable.Length(); i++)
				{
					int x = Growable[i];
					if (Largest < 0 || MinCol[x] > MinCol[Largest])
						Largest = x;
				}
				Growable.Length(1);
				Growable[0] = Largest;
			}

			int AddPx = (RemainingPx - Added) / Growable.Length();
			for (unsigned i=0; i<Growable.Length(); i++)
			{
				int x = Growable[i];
				if (i == Growable.Length() - 1)
				{
					MinCol[x] += RemainingPx - Added;
				}
				else
				{
					MinCol[x] += AddPx;
					Added += AddPx;
				}
				LgiAssert(MinCol[x] >= 0);
			}
		}
	}
}

struct ColInfo
{
	int Large;
	int Growable;
	int Idx;
	int Px;
};

int ColInfoCmp(ColInfo *a, ColInfo *b)
{
	int LDiff = b->Large - a->Large;
	int LGrow = b->Growable - a->Growable;
	int LSize = b->Px - a->Px;
	return LDiff + LGrow + LSize;
}

void GHtmlTableLayout::DeallocatePx(int StartCol, int Cols, int MaxPx)
{
	int TotalPx = GetTotalX(StartCol, Cols);
	if (TotalPx <= MaxPx || MaxPx == 0)
		return;
	
	int TrimPx = TotalPx - MaxPx;
	GArray<ColInfo> Inf;
	int HalfMax = MaxPx >> 1;
	unsigned Interesting = 0;
	int InterestingPx = 0;
	
	for (int x=StartCol; x<StartCol+Cols; x++)
	{
		ColInfo &ci = Inf.New();
		ci.Idx = x;
		ci.Px = MinCol[x];
		ci.Large = MinCol[x] > HalfMax;
		ci.Growable = MinCol[x] < MaxCol[x];
		if (ci.Large || ci.Growable)
		{
			Interesting++;
			InterestingPx += ci.Px;
		}
	}
	
	Inf.Sort(ColInfoCmp);
	
	for (unsigned i=0; i<Interesting; i++)
	{
		ColInfo &ci = Inf[i];
		double r = (double)ci.Px / InterestingPx;
		int DropPx = (int) (r * TrimPx);
		if (DropPx < MinCol[ci.Idx])
			MinCol[ci.Idx] -= DropPx;
		else
			break;
	}
}

int GHtmlTableLayout::GetTotalX(int StartCol, int Cols)
{
	if (Cols < 0)
		Cols = s.x;
	
	int TotalX = BorderX1 + BorderX2 + CellSpacing;
	for (int x=StartCol; x<Cols; x++)
		TotalX += MinCol[x] + CellSpacing;
	
	return TotalX;
}

void GHtmlTableLayout::LayoutTable(GFlowRegion *f, uint16 Depth)
{
	GetSize(s.x, s.y);
	if (s.x == 0 || s.y == 0)
	{
		return;
	}

	GFont *Font = Table->GetFont();
	Table->ZeroTableElements();
	MinCol.Length(0);
	MaxCol.Length(0);
	MaxRow.Length(0);
	SizeCol.Length(0);
	
	GCss::Len BdrSpacing = Table->BorderSpacing();
	CellSpacing = BdrSpacing.IsValid() ? (int)BdrSpacing.Value : 0;

	// Resolve total table width.
	TableWidth = Table->Width();
	if (TableWidth.IsValid())
		AvailableX = f->ResolveX(TableWidth, Font, false);
	else
		AvailableX = f->X();

	GCss::Len MaxWidth = Table->MaxWidth();
	if (MaxWidth.IsValid())
	{
	    int Px = f->ResolveX(MaxWidth, Font, false);
	    if (Px < AvailableX)
	        AvailableX = Px;
	}
	
	TableBorder = f->ResolveBorder(Table, Font);
	if (Table->BorderCollapse() != GCss::CollapseCollapse)
		TablePadding = f->ResolvePadding(Table, Font);
	else
		TablePadding.ZOff(0, 0);
	
	BorderX1 = TableBorder.x1 + TablePadding.x1;
	BorderX2 = TableBorder.x2 + TablePadding.x2;
	
	#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
	if (Table->Debug)
		LgiTrace("AvailableX=%i, BorderX1=%i, BorderX2=%i\n", AvailableX, BorderX1, BorderX2);
	#endif

	#ifdef _DEBUG
	if (Table->Debug)
	{
		printf("Table Debug\n");
	}
	#endif

	// Size detection pass
	int y;
	for (y=0; y<s.y; y++)
	{
		for (int x=0; x<s.x; )
		{
			GTag *t = Get(x, y);
			if (t)
			{
				// This is needed for the metrics...
				t->GetFont();
				t->Cell->BorderPx = f->ResolveBorder(t, Font);
				t->Cell->PaddingPx = f->ResolvePadding(t, Font);

				if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
				{
					GCss::DisplayType Disp = t->Display();
					if (Disp == GCss::DispNone)
						continue;

					GCss::Len Content = t->Width();
					if (Content.IsValid() && t->Cell->Span.x == 1)
					{
						if (SizeCol[x].IsValid())
						{
							int OldPx = f->ResolveX(SizeCol[x], Font, false);
							int NewPx = f->ResolveX(Content, Font, false);
							if (NewPx > OldPx)
							{
								SizeCol[x] = Content;
							}
						}
						else
						{
							SizeCol[x] = Content;
						}
					}
					
					if (!t->GetWidthMetrics(Table, t->Cell->MinContent, t->Cell->MaxContent))
					{
						t->Cell->MinContent = 16;
						t->Cell->MaxContent = 16;
					}
					
					#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
					if (Table->Debug)
						LgiTrace("Content[%i,%i] MIN=%i MAX=%i\n", x, y, t->Cell->MinContent, t->Cell->MaxContent);
					#endif

					if (t->Cell->Span.x == 1)
					{
						int BoxPx = t->Cell->BorderPx.x1 +
									t->Cell->BorderPx.x2 +
									t->Cell->PaddingPx.x1 +
									t->Cell->PaddingPx.x2;

						MinCol[x] = MAX(MinCol[x], t->Cell->MinContent + BoxPx);
						MaxCol[x] = MAX(MaxCol[x], t->Cell->MaxContent + BoxPx);
					}
				}
				
				x += t->Cell->Span.x;
			}
			else break;
		}
	}
	
	// How much space used so far?
	int TotalX = GetTotalX();
	if (TotalX > AvailableX)
	{
		// FIXME:
		// Off -> 'cisra-cqs.html' renders correctly.
		// On -> 'cisra_outage.html' renders correctly.
		#if 0
		DeallocatePx(0, MinCol.Length(), AvailableX);
		TotalX = GetTotalX();
		#endif
	}

	#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
	#define DumpCols(msg) \
		if (Table->Debug) \
		{ \
			LgiTrace("%s Ln%i - TotalX=%i AvailableX=%i\n", msg, __LINE__, TotalX, AvailableX); \
			for (unsigned i=0; i<MinCol.Length(); i++) \
				LgiTrace("\t[%i] = %i/%i\n", i, MinCol[i], MaxCol[i]); \
		}

	#else
	#define DumpCols(msg)
	#endif

	DumpCols("AfterSingleCells");

	#ifdef _DEBUG
	if (Table->Debug)
	{
		printf("TableDebug\n");
	}
	#endif

	// Process spanned cells
	for (y=0; y<s.y; y++)
	{
		for (int x=0; x<s.x; )
		{
			GTag *t = Get(x, y);
			if (t && t->Cell->Pos.x == x && t->Cell->Pos.y == y)
			{
				if (t->Cell->Span.x > 1 || t->Cell->Span.y > 1)
				{
					int i;
					int ColMin = -CellSpacing;
					int ColMax = -CellSpacing;
					for (i=0; i<t->Cell->Span.x; i++)
					{
						ColMin += MinCol[x + i] + CellSpacing;
						ColMax += MaxCol[x + i] + CellSpacing;
					}
					
					GCss::Len Width = t->Width();
					if (Width.IsValid())
					{
						int Px = f->ResolveX(Width, Font, false);
						t->Cell->MinContent = MAX(t->Cell->MinContent, Px);
						t->Cell->MaxContent = MAX(t->Cell->MaxContent, Px);
					}

					#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
					if (Table->Debug)
						LgiTrace("Content[%i,%i] MIN=%i MAX=%i\n", x, y, t->Cell->MinContent, t->Cell->MaxContent);
					#endif

					if (t->Cell->MinContent > ColMin)
						AllocatePx(t->Cell->Pos.x, t->Cell->Span.x, t->Cell->MinContent, false);
					if (t->Cell->MaxContent > ColMax)
						DistributeSize(MaxCol, t->Cell->Pos.x, t->Cell->Span.x, t->Cell->MaxContent, CellSpacing);
				}

				x += t->Cell->Span.x;
			}
			else break;
		}
	}

	TotalX = GetTotalX();
	DumpCols("AfterSpannedCells");

	// Sometimes the web page specifies too many percentages:
	// Scale them all.	
	float PercentSum = 0.0f;
	for (int i=0; i<s.x; i++)
	{
		if (SizeCol[i].Type == GCss::LenPercent)
			PercentSum += SizeCol[i].Value;
	}	
	if (PercentSum > 100.0)
	{
		float Ratio = PercentSum / 100.0f;
		for (int i=0; i<s.x; i++)
		{
			if (SizeCol[i].Type == GCss::LenPercent)
				SizeCol[i].Value /= Ratio;
		}
	}
	
	// Do minimum column size from CSS values
	if (TotalX < AvailableX)
	{
		for (int x=0; x<s.x; x++)
		{
			GCss::Len w = SizeCol[x];
			if (w.IsValid())
			{
				int Px = f->ResolveX(w, Font, false);
				
				if (w.Type == GCss::LenPercent)
				{
					MaxCol[x] = Px;
				}
				else if (Px > MinCol[x])
				{
					int RemainingPx = AvailableX - TotalX;
					int AddPx = Px - MinCol[x];
					AddPx = MIN(RemainingPx, AddPx);
					
					TotalX += AddPx;
					MinCol[x] += AddPx;
				}
			}
		}
	}

	TotalX = GetTotalX();
	DumpCols("AfterCssNonPercentageSizes");
	
	if (TotalX > AvailableX)
	{
		#if !ALLOW_TABLE_GROWTH
		// Deallocate space if overused
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

		DumpCols("AfterSpaceDealloc");
		#endif
	}
	else if (TotalX < AvailableX)
	{
		AllocatePx(0, s.x, AvailableX, TableWidth.IsValid());
		DumpCols("AfterRemainingAlloc");
	}

	// Layout cell horizontally and then flow the contents to get 
	// the height of all the cells
	GArray<GRect> RowPad;
	MaxRow.Length(s.y);
	for (y=0; y<s.y; y++)
	{
		int XPos = CellSpacing;
		for (int x=0; x<s.x; )
		{
			GTag *t = Get(x, y);
			if (t)
			{
				if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
				{
					t->Pos.x = XPos;
					t->Size.x = -CellSpacing;
					XPos -= CellSpacing;
					
					RowPad[y].y1 = MAX(RowPad[y].y1, t->Cell->BorderPx.y1 + t->Cell->PaddingPx.y1);
					RowPad[y].y2 = MAX(RowPad[y].y2, t->Cell->BorderPx.y2 + t->Cell->PaddingPx.y2);
					
					GRect Box(0, 0, -CellSpacing, 0);
					for (int i=0; i<t->Cell->Span.x; i++)
					{
						int ColSize = MinCol[x + i] + CellSpacing;
						LgiAssert(ColSize >= 0);
						if (ColSize < 0)
							break;
						t->Size.x += ColSize;
						XPos += ColSize;
						Box.x2 += ColSize;
					}
					
					GCss::Len Ht = t->Height();
					GFlowRegion r(Table->Html, Box, true);

					t->OnFlow(&r, Depth+1);

					if (r.MAX.y > r.y2)
					{
						t->Size.y = MAX(r.MAX.y, t->Size.y);
					}

					
					if (Ht.IsValid() &&
						Ht.Type != GCss::LenPercent)
					{
						int h = f->ResolveY(Ht, Font, false);
						t->Size.y = MAX(h, t->Size.y);

						DistributeSize(MaxRow, y, t->Cell->Span.y, t->Size.y, CellSpacing);
					}
				}

				x += t->Cell->Span.x;
			}
			else break;
		}
	}

	#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
	if (Table->Debug)
	{
		LgiTrace("%s:%i - AfterCellFlow\n", _FL);
		for (unsigned i=0; i<MaxRow.Length(); i++)
		{
			LgiTrace("[%i] = %i\n", i, MaxRow[i]);
		}
	}
	#endif

	// Calculate row height
	for (y=0; y<s.y; y++)
	{
		for (int x=0; x<s.x; )
		{
			GTag *t = Get(x, y);
			if (t)
			{
				if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
				{
					GCss::Len Ht = t->Height();
					if (!(Ht.IsValid() && Ht.Type != GCss::LenPercent))
					{
						DistributeSize(MaxRow, y, t->Cell->Span.y, t->Size.y, CellSpacing);
					}
				}

				x += t->Cell->Span.x;
			}
			else break;
		}			
	}
	
	// Cell positioning
	int Cx = BorderX1 + CellSpacing;
	int Cy = TableBorder.y1 + TablePadding.y1 + CellSpacing;
	
	for (y=0; y<s.y; y++)
	{
		GTag *Prev = 0;
		for (int x=0; x<s.x; )
		{
			GTag *t = Get(x, y);
			if (!t && Prev)
			{
				// Add missing cell
				GTag *Row = ToTag(Prev->Parent);
				if (Row && Row->TagId == TAG_TR)
				{
					t = new GTag(Table->Html, Row);
					if (t)
					{
						t->TagId = TAG_TD;
						t->Tag.Reset(NewStr("td"));
						t->Info = Table->Html->GetTagInfo(t->Tag);
						if ((t->Cell = new GTag::TblCell))
						{
							t->Cell->Pos.x = x;
							t->Cell->Pos.y = y;
							t->Cell->Span.x = 1;
							t->Cell->Span.y = 1;
						}
						t->BackgroundColor(GCss::ColorDef(GCss::ColorRgb, DefaultMissingCellColour));

						Set(Table);
					}
					else break;
				}
				else break;
			}
			if (t)
			{
				if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
				{
					int RowPadOffset =	RowPad[y].y1 -
										t->Cell->BorderPx.y1 -
										t->Cell->PaddingPx.y1;
					
					t->Pos.x = Cx;
					t->Pos.y = Cy + RowPadOffset;
					
					t->Size.x = -CellSpacing;
					for (int i=0; i<t->Cell->Span.x; i++)
					{
						int w = MinCol[x + i] + CellSpacing;
						t->Size.x += w;
						Cx += w;
					}
					t->Size.y = -CellSpacing;						
					for (int n=0; n<t->Cell->Span.y; n++)
					{
						t->Size.y += MaxRow[y+n] + CellSpacing;
					}
					
					Table->Size.x = MAX(Cx + BorderX2, Table->Size.x);

					#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
					if (Table->Debug)
					{
						LgiTrace("cell(%i,%i) = pos(%i,%i)+size(%i,%i)\n",
							t->Cell->Pos.x, t->Cell->Pos.y,
							t->Pos.x, t->Pos.y,
							t->Size.x, t->Size.y);
					}
					#endif
				}
				else
				{
					Cx += t->Size.x + CellSpacing;
				}
				
				x += t->Cell->Span.x;
			}
			else break;
			Prev = t;
		}
		
		Cx = BorderX1 + CellSpacing;
		Cy += MaxRow[y] + CellSpacing;
	}

	switch (Table->Cell->XAlign ? Table->Cell->XAlign : ToTag(Table->Parent)->GetAlign(true))
	{
		case GCss::AlignCenter:
		{
			int fx = f->X();
			int Ox = (fx-Table->Size.x) >> 1;
			Table->Pos.x = f->x1 + MAX(Ox, 0);
			break;
		}
		case GCss::AlignRight:
		{
			Table->Pos.x = f->x2 - Table->Size.x;
			break;
		}
		default:
		{
			Table->Pos.x = f->x1;
			break;
		}
	}
	
	Table->Pos.y = f->y1;
	Table->Size.y = Cy + TablePadding.y2 + TableBorder.y2;
}

GRect GTag::ChildBounds()
{
	GRect b(0, 0, -1, -1);

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
		if (i)
		{
			GRect c = t->GetRect();
			b.Union(&c);
		}
		else
		{
			b = t->GetRect();
		}
	}

	return b;
}

GdcPt2 GTag::AbsolutePos()
{
	GdcPt2 p;
	for (GTag *t=this; t; t=ToTag(t->Parent))
	{
		p += t->Pos;
	}
	return p;
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

	for (unsigned i=0; i<Length(); i++)
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

void GArea::FlowText(GTag *Tag, GFlowRegion *Flow, GFont *Font, int LineHeight, char16 *Text, GCss::LengthType Align)
{
	if (!Flow || !Text || !Font)
		return;

	char16 *Start = Text;
	size_t FullLen = StrlenW(Text);

	#if 1
	if (!Tag->Html->GetReadOnly() && !*Text)
	{
		// Insert a text rect for this tag, even though it's empty.
		// This allows the user to place the cursor on a blank line.
		GFlowRect *Tr = new GFlowRect;
		Tr->Tag = Tag;
		Tr->Text = Text;
		Tr->x1 = Flow->cx;
		Tr->x2 = Tr->x1 + 1;
		Tr->y1 = Flow->y1;
		Tr->y2 = Tr->y1 + Font->GetHeight();
		LgiAssert(Tr->y2 >= Tr->y1);
		Flow->y2 = MAX(Flow->y2, Tr->y2+1);
		Flow->cx = Tr->x2 + 1;

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
		
		// Remove white space at start of line if not in edit mode..
		if (Tag->Html->GetReadOnly() && Flow->x1 == Flow->cx && *Text == ' ')
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

		GDisplayString ds(Font, Text, MIN(1024, FullLen - (Text-Start)));
		ssize_t Chars = ds.CharAt(Flow->X());
		bool Wrap = false;
		if (Text[Chars])
		{
			// Word wrap

			// Seek back to the nearest break opportunity
			ssize_t n = Chars;
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
						;

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
				Wrap = true;
			}

		}
		else
		{
			// Fits..
			Tr->Len = Chars;
			LgiAssert(Tr->Len > 0);
		}

		GDisplayString ds2(Font, Tr->Text, Tr->Len);
		Tr->x2 = ds2.X();
		Tr->y2 = LineHeight > 0 ? LineHeight - 1 : 0;
		
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
		Flow->y2 = MAX(Flow->y2, Tr->y2 + 1);
		
		Add(Tr);
		Flow->Insert(Tr);
		
		Text += Tr->Len;
		if (Wrap)
		{
			while (*Text == ' ')
				Text++;
		}

		Tag->Size.x = MAX(Tag->Size.x, Tr->x2);
		Tag->Size.y = MAX(Tag->Size.y, Tr->y2);
		Flow->MAX.x = MAX(Flow->MAX.x, Tr->x2);
		Flow->MAX.y = MAX(Flow->MAX.y, Tr->y2);

		if (Tr->Len == 0)
			break;
	}
}

char16 htoi(char16 c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	LgiAssert(0);
	return 0;
}

bool GTag::Serialize(GXmlTag *t, bool Write)
{
	GRect pos;
	if (Write)
	{
		// Obj -> Tag
		if (Tag) t->SetAttr("tag", Tag);
		pos.ZOff(Size.x, Size.y);
		pos.Offset(Pos.x, Pos.y);
		t->SetAttr("pos", pos.GetStr());
		t->SetAttr("tagid", TagId);		
		if (Txt)
		{
			GStringPipe p(256);
			for (char16 *c = Txt; *c; c++)
			{
				if (*c > ' ' &&
					*c < 127 &&
					!strchr("%<>\'\"", *c))
					p.Print("%c", (char)*c);
				else
					p.Print("%%%.4x", *c);
			}
			
			GAutoString Tmp(p.NewStr());
			t->SetContent(Tmp);
		}
		if (Props.Length())
		{
			GAutoString CssStyles = ToString();
			LgiAssert(!strchr(CssStyles, '\"'));
			t->SetAttr("style", CssStyles);
		}
		if (Html->Cursor == this)
		{
			LgiAssert(Cursor >= 0);
			t->SetAttr("cursor", (int64)Cursor);
		}
		else LgiAssert(Cursor < 0);
		if (Html->Selection == this)
		{
			LgiAssert(Selection >= 0);
			t->SetAttr("selection", (int64)Selection);
		}
		else LgiAssert(Selection < 0);
		
		for (unsigned i=0; i<Children.Length(); i++)
		{
			GXmlTag *child = new GXmlTag("e");
			GTag *tag = ToTag(Children[i]);
			if (!child || !tag)
			{
				LgiAssert(0);
				return false;
			}
			t->InsertTag(child);
			if (!tag->Serialize(child, Write))
			{
				return false;
			}
		}
	}
	else
	{
		// Tag -> Obj
		Tag.Reset(NewStr(t->GetAttr("tag")));
		TagId = (HtmlTag) t->GetAsInt("tagid");
		pos.SetStr(t->GetAttr("pos"));
		if (pos.Valid())
		{
			Pos.x = pos.x1;
			Pos.y = pos.y1;
			Size.x = pos.x2;
			Size.y = pos.y2;
		}
		if (ValidStr(t->GetContent()))
		{
			GStringPipe p(256);
			char *c = t->GetContent();
			SkipWhiteSpace(c);
			for (; *c && *c > ' '; c++)
			{
				char16 ch;
				if (*c == '%')
				{
					ch = 0;
					for (int i=0; i<4 && *c; i++)
					{
						ch <<= 4;
						ch |= htoi(*++c);
					}
				}
				else ch = *c;
				p.Write(&ch, sizeof(ch));
			}
			Txt.Reset(p.NewStrW());
		}
		const char *s = t->GetAttr("style");
		if (s)
			Parse(s, ParseRelaxed);
		s = t->GetAttr("cursor");
		if (s)
		{
			LgiAssert(Html->Cursor == NULL);
			Html->Cursor = this;
			Cursor = atoi(s);
			LgiAssert(Cursor >= 0);			
		}
		s = t->GetAttr("selection");
		if (s)
		{
			LgiAssert(Html->Selection == NULL);
			Html->Selection = this;
			Selection = atoi(s);
			LgiAssert(Selection >= 0);			
		}
		#ifdef _DEBUG
		s = t->GetAttr("debug");
		if (s && atoi(s) != 0)
			Debug = true;
		#endif
		
		for (int i=0; i<t->Children.Length(); i++)
		{
			GXmlTag *child = t->Children[i];
			if (child->IsTag("e"))
			{
				GTag *tag = new GTag(Html, NULL);
				if (!tag)
				{
					LgiAssert(0);
					return false;
				}
				
				if (!tag->Serialize(child, Write))
				{
					return false;
				}
				
				Attach(tag);
			}
		}
	}
	
	return true;
}

/// This method centers the text in the area given to the tag. Used for inline block elements.
void GTag::CenterText()
{
	if (!Parent)
		return;

	// Find the size of the text elements.
	int ContentPx = 0;
	for (unsigned i=0; i<TextPos.Length(); i++)
	{
		GFlowRect *fr = TextPos[i];
		ContentPx += fr->X();
	}
	
	GFont *f = GetFont();
	int ParentPx = ToTag(Parent)->Size.x;
	int AvailPx = Size.x;

	// Remove the border and padding from the content area
	AvailPx -= BorderLeft().ToPx(ParentPx, f);
	AvailPx -= BorderRight().ToPx(ParentPx, f);
	AvailPx -= PaddingLeft().ToPx(ParentPx, f);
	AvailPx -= PaddingRight().ToPx(ParentPx, f);
	if (AvailPx > ContentPx)
	{
		// Now offset all the regions to the right
		int OffPx = (AvailPx - ContentPx) >> 1;
		for (unsigned i=0; i<TextPos.Length(); i++)
		{
			GFlowRect *fr = TextPos[i];
			fr->Offset(OffPx, 0);
		}
	}
}

void GTag::OnFlow(GFlowRegion *Flow, uint16 Depth)
{
	if (Depth >= MAX_RECURSION_DEPTH)
		return;

	DisplayType Disp = Display();
	if (Disp == DispNone)
		return;

	GFont *f = GetFont();
	GFlowRegion Local(Html, true);
	bool Restart = true;
	int BlockFlowWidth = 0;
	const char *ImgAltText = NULL;
	int BlockInlineX[3];

	Size.x = 0;
	Size.y = 0;
	
	GCssTools Tools(this, f);
	GRect rc(Flow->X(), Html->Y());
	PadPx = Tools.GetPadding(rc);

	switch (TagId)
	{
		default:
			break;
		case TAG_BODY:
		{
			Flow->InBody++;
			break;
		}
		case TAG_IFRAME:
		{
			GFlowRegion Temp = *Flow;
			Flow->EndBlock();
			Flow->Indent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);

			// Flow children
			for (unsigned i=0; i<Children.Length(); i++)
			{
				GTag *t = ToTag(Children[i]);
				t->OnFlow(&Temp, Depth + 1);

				if (TagId == TAG_TR)
				{
					Temp.x2 -= MIN(t->Size.x, Temp.X());
				}
			}

			Flow->Outdent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);
			BoundParents();
			return;
			break;
		}
		case TAG_IMG:
		{
			Size.x = Size.y = 0;
			
			GCss::Len w    = Width();
			GCss::Len h    = Height();
			// GCss::Len MinX = MinWidth();
			// GCss::Len MaxX = MaxWidth();
			GCss::Len MinY = MinHeight();
			GCss::Len MaxY = MaxHeight();
			GAutoPtr<GDisplayString> a;
			int ImgX, ImgY;		
			if (Image)
			{
				ImgX = Image->X();
				ImgY = Image->Y();
			}
			else if (Get("alt", ImgAltText) && ValidStr(ImgAltText))
			{
				GDisplayString a(f, ImgAltText);
				ImgX = a.X() + 4;
				ImgY = a.Y() + 4;
			}
			else
			{
				ImgX = DefaultImgSize;
				ImgY = DefaultImgSize;
			}
			
			double AspectRatio = ImgY != 0 ? (double)ImgX / ImgY : 1.0;
			bool XLimit = false, YLimit = false;
			double Scale = 1.0;

			if (w.IsValid() && w.Type != LenAuto)
			{
				Size.x = Flow->ResolveX(w, GetFont(), false);
				XLimit = true;
			}
			else
			{
				int Fx = Flow->x2 - Flow->x1 + 1;
				if (ImgX > Fx)
				{
					Size.x = Fx; //  * 0.8;
					if (Image)
						Scale = (double) Fx / ImgX;
				}
				else
				{
					Size.x = ImgX;
				}
			}
			XLimit |= Flow->LimitX(Size.x, MinWidth(), MaxWidth(), f);

			if (h.IsValid() && h.Type != LenAuto)
			{
				Size.y = Flow->ResolveY(h, GetFont(), false);
				YLimit = true;
			}
			else
			{
				Size.y = (int) (ImgY * Scale);
			}
			YLimit |= Flow->LimitY(Size.y, MinHeight(), MaxHeight(), f);

			if
			(
				(XLimit ^ YLimit)
				&&
				Image
			)
			{
				if (XLimit)
				{
					Size.y = (int) ceil((double)Size.x / AspectRatio);
				}
				else
				{
					Size.x = (int) ceil((double)Size.y * AspectRatio);
				}
			}
			if (MinY.IsValid())
			{
				int Px = Flow->ResolveY(MinY, GetFont(), false);
				if (Size.y < Px)
					Size.y = Px;
			}
			if (MaxY.IsValid())
			{
				int Px = Flow->ResolveY(MaxY, GetFont(), false);
				if (Size.y > Px)
					Size.y = Px;
			}

			if (Disp == DispInline ||
				Disp == DispInlineBlock)
			{
				Restart = false;

				if (Flow->cx > Flow->x1 &&
					Size.x > Flow->X())
				{
					Flow->FinishLine();
				}

				Pos.y = Flow->y1;
				Flow->y2 = MAX(Flow->y1, Pos.y + Size.y - 1);

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
			
			GCss::Len left = GetCssLen(MarginLeft, Margin);
			GCss::Len top = GetCssLen(MarginTop, Margin);
			GCss::Len right = GetCssLen(MarginRight, Margin);
			GCss::Len bottom = GetCssLen(MarginBottom, Margin);
			Flow->Indent(f, left, top, right, bottom, true);

			LayoutTable(Flow, Depth + 1);

			Flow->y1 += Size.y;
			Flow->y2 = Flow->y1;
			Flow->cx = Flow->x1;
			Flow->my = 0;
			Flow->MAX.y = MAX(Flow->MAX.y, Flow->y2);

			Flow->Outdent(f, left, top, right, bottom, true);
			BoundParents();
			return;
		}
	}

	// int OldFlowMy = Flow->my;
	if (Disp == DispBlock || Disp == DispInlineBlock)
	{
		// This is a block level element, so end the previous non-block elements
		if (Disp == DispBlock)
		{		
			Flow->EndBlock();
		}
		
		BlockFlowWidth = Flow->X();
		
		/*	This code breaks the 'staysz.html' test case.
			What was its original purpose?
		if (TagId == TAG_P)
		{
			if (!OldFlowMy && Text())
			{
				Flow->FinishLine(true);
			}
		}
		*/

		// Indent the margin...
		GCss::Len left = GetCssLen(MarginLeft, Margin);
		GCss::Len top = GetCssLen(MarginTop, Margin);
		GCss::Len right = GetCssLen(MarginRight, Margin);
		GCss::Len bottom = GetCssLen(MarginBottom, Margin);
		Flow->Indent(f, left, top, right, bottom, true);

		// Set the width if any
		if (Disp == DispBlock)
		{
			GCss::Len Wid = Width();
			if (!IsTableCell(TagId) && Wid.IsValid())
				Size.x = Flow->ResolveX(Wid, f, false);
			else if (TagId != TAG_IMG)
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
			Flow->Indent(PadPx, false);
		}
		else
		{
			BlockInlineX[0] = Flow->x1;
			BlockInlineX[1] = Flow->cx;
			BlockInlineX[2] = Flow->x2;
			Flow->x1 = 0;
			Flow->x2 = Size.x;
			Flow->cx = 0;
			
			Flow->cx += Flow->ResolveX(BorderLeft(), GetFont(), true);
			Flow->y1 += Flow->ResolveY(BorderTop(), GetFont(), true);
			
			Flow->cx += Flow->ResolveX(PaddingLeft(), GetFont(), true);
			Flow->y1 += Flow->ResolveY(PaddingTop(), GetFont(), true);
		}
	}
	else
	{
		Flow->Indent(PadPx, false);
	}

	if (f)
	{
		// Clear the previous text layout...
		TextPos.DeleteObjects();

		switch (TagId)
		{
			default:
				break;
			case TAG_LI:
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
						default: break;
						case ListDecimal:
						{
							ssize_t Index = Parent->Children.IndexOf(this);
							char Txt[32];
							sprintf_s(Txt, sizeof(Txt), "%i. ", (int)(Index + 1));
							PreText(Utf8ToWide(Txt));
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
					TextPos.FlowText(this, Flow, f, f->GetHeight(), PreText(), AlignLeft);
				
				break;
			}
			case TAG_IMG:
			{
				if (Disp == DispBlock)
				{
					Flow->cx += Size.x;
					Flow->y2 += Size.y;
				}
				break;
			}
		}

		if (Text() && Flow->InBody)
		{
			// Setup the line height cache
			if (LineHeightCache < 0)
			{
				GCss::PropMap Map;
				GCss Final;
				Map.Add(PropLineHeight, new GCss::PropArray);
				for (GTag *t = this; t; t = t->TagId == TAG_TABLE ? NULL : ToTag(t->Parent))
				{
					if (!Final.InheritCollect(*t, Map))
						break;
				}	
				Final.InheritResolve(Map);
				Map.DeleteObjects();

				GCss::Len CssLineHeight = Final.LineHeight();    
				if (f)
				{
					int FontPx = FontPxHeight(f);
					
					if (!CssLineHeight.IsValid() ||
						CssLineHeight.Type == GCss::LenAuto ||
						CssLineHeight.Type == GCss::LenNormal)
					{
						LineHeightCache = f->GetHeight();
					}
					else
					{					
						LineHeightCache = CssLineHeight.ToPx(FontPx, f);
					}
				}
			}

			// Flow in the rest of the text...
			char16 *Txt = Text();
			GCss::LengthType Align = GetAlign(true);
			TextPos.FlowText(this, Flow, f, LineHeightCache, Txt, Align);
		}
	}

	// Flow children
	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);

		switch (t->Position())
		{
			case PosStatic:
			case PosAbsolute:
			case PosFixed:
			{
				GFlowRegion old = *Flow;
				t->OnFlow(Flow, Depth + 1);
				
				// Try and reset the flow to how it was before...
				Flow->x1 = old.x1;
				Flow->x2 = old.x2;
				Flow->cx = old.cx;
				Flow->y1 = old.y1;
				Flow->y2 = old.y2;
				Flow->MAX.x = MAX(Flow->MAX.x, old.MAX.x);
				Flow->MAX.y = MAX(Flow->MAX.y, old.MAX.y);
				break;
			}
			default:
			{
				t->OnFlow(Flow, Depth + 1);
				break;
			}
		}

		if (TagId == TAG_TR)
		{
			Flow->x2 -= MIN(t->Size.x, Flow->X());
		}
	}

	GCss::LengthType XAlign = LenInherit;
	if (Disp == DispBlock || Disp == DispInlineBlock)
	{		
		GCss::Len Ht = Height();
		GCss::Len MaxHt = MaxHeight();

		bool AcceptHt = !IsTableCell(TagId) && Ht.Type != LenPercent;
		if (AcceptHt)
		{
			if (Ht.IsValid())
			{
				int HtPx = Flow->ResolveY(Ht, GetFont(), false);
				if (HtPx > Flow->y2)
					Flow->y2 = HtPx;
			}
			if (MaxHt.IsValid())
			{
				int MaxHtPx = Flow->ResolveY(MaxHt, GetFont(), false);
				if (MaxHtPx < Flow->y2)
					Flow->y2 = MaxHtPx;
			}
		}

		if (Disp == DispBlock)
		{
			Flow->EndBlock();

			int OldFlowSize = Flow->x2 - Flow->x1 + 1;
			Flow->Outdent(f, PaddingLeft(), PaddingTop(), PaddingRight(), PaddingBottom(), false);
			Flow->Outdent(f, GCss::BorderLeft(), GCss::BorderTop(), GCss::BorderRight(), GCss::BorderBottom(), false);

			Size.y = Flow->y2 > 0 ? Flow->y2 : 0;

			Flow->Outdent(f, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);
			
			int NewFlowSize = Flow->x2 - Flow->x1 + 1;
			int Diff = NewFlowSize - OldFlowSize;
			if (Diff)
				Flow->MAX.x += Diff;
			
			Flow->y1 = Flow->y2;
			Flow->x2 = Flow->x1 + BlockFlowWidth;

			// I dunno, there should be a better way... :-(
			if (MarginLeft().Type == LenAuto &&
				MarginRight().Type == LenAuto)
			{
				XAlign = GCss::AlignCenter;
			}
		}
		else
		{
			GCss::Len Wid = Width();
			int WidPx = Wid.IsValid() ? Flow->ResolveX(Wid, GetFont(), false) : 0;
			
			Flow->cx += Flow->ResolveX(PaddingRight(), GetFont(), true);
			Flow->cx += Flow->ResolveX(BorderRight(), GetFont(), true);
			Size.x = MAX(WidPx, Flow->cx);
			Flow->cx += Flow->ResolveX(MarginRight(), GetFont(), true);
			Flow->x1 = BlockInlineX[0] - Pos.x;
			Flow->cx = BlockInlineX[1] + Flow->cx - Pos.x;
			Flow->x2 = BlockInlineX[2] - Pos.x;

			if (Height().IsValid())
			{
				Size.y = Flow->ResolveY(Height(), GetFont(), false);
				int MarginY2 = Flow->ResolveX(MarginBottom(), GetFont(), true);
				Flow->y2 = MAX(Flow->y1 + Size.y + MarginY2 - 1, Flow->y2);
			}
			else
			{
				Flow->y2 += Flow->ResolveX(PaddingBottom(), GetFont(), true);
				Flow->y2 += Flow->ResolveX(BorderBottom(), GetFont(), true);
				Size.y = Flow->y2;
			}
			
			CenterText();
		}
	}
	else
	{
		Flow->Outdent(PadPx, false);
		
		switch (TagId)
		{
			default: break;
			case TAG_SELECT:
			case TAG_INPUT:
			{
				if (Html->InThread() && Ctrl)
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
				Flow->y2 = MAX(Flow->y2, Flow->y1 + Size.y - 1);
				break;
			}
			case TAG_IMG:
			{
				Flow->cx += Size.x;
				Flow->y2 = MAX(Flow->y2, Flow->y1 + Size.y - 1);
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
				Flow->y2 = MAX(Flow->y2, Flow->y1 + Size.y - 1);
				break;
			}
			case TAG_CENTER:
			{
				int Px = Flow->X();
				for (GHtmlElement **e = NULL; Children.Iterate(e); )
				{
					GTag *t = ToTag(*e);
					if (t->IsBlock())
					{
						if (t->Size.x < Px)
						{
							t->Pos.x = (Px - t->Size.x) >> 1;
						}
					}
				}
				break;
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
		Flow->MAX.y = MAX(Flow->MAX.y, Flow->y2);
	}

	if (XAlign == GCss::AlignCenter)
	{
		int OffX = (Flow->x2 - Flow->x1 - Size.x) >> 1;
		if (OffX > 0)
		{
			Pos.x += OffX;
		}
	}

	if (TagId == TAG_BODY && Flow->InBody > 0)
	{
		Flow->InBody--;
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
			Html->ParseName(++s, &t);
			if (t)
			{
				Status = _stricmp(t, tag) == 0;
			}

			DeleteArray(t);
		}
	}

	return Status;
}

GTag *GTag::GetTable()
{
	GTag *t = 0;
	for (t=ToTag(Parent); t && t->TagId != TAG_TABLE; t = ToTag(t->Parent))
		;
	return t;
}

void GTag::BoundParents()
{
	if (!Parent)
		return;

	GTag *np;
	for (GTag *n=this; n; n = np)
	{
		np = ToTag(n->Parent);

		if (!np ||
			n->Parent->TagId == TAG_IFRAME)
			break;
				
		np->Size.x = MAX(np->Size.x, n->Pos.x + n->Size.x);
		np->Size.y = MAX(np->Size.y, n->Pos.y + n->Size.y);
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

void GTag::GetInlineRegion(GRegion &rgn)
{
	if (TagId == TAG_IMG)
	{
		GRect rc(0, 0, Size.x-1, Size.y-1);
		rc.Offset(Pos.x, Pos.y);
		rgn.Union(&rc);
	}
	else
	{
		for (unsigned i=0; i<TextPos.Length(); i++)
		{
			GRect rc = *(TextPos[i]);
			rgn.Union(&rc);
		}
	}
	
	for (unsigned c=0; c<Children.Length(); c++)
	{
		GTag *ch = ToTag(Children[c]);
		ch->GetInlineRegion(rgn);
	}
}

class CornersImg : public GMemDC
{
public:
	int Px, Px2;

	CornersImg(	float RadPx,
				GRect *BorderPx,
				GCss::BorderDef **defs,
				GColour &Back,
				bool DrawBackground)
	{
		Px = 0;
		Px2 = 0;

		//Radius.Type != GCss::LenInherit &&			
		if (RadPx > 0.0f)
		{
			Px = (int)ceil(RadPx);
			Px2 = Px << 1;
			
			if (Create(Px2, Px2, System32BitColourSpace))
			{
				#if 1
				Colour(0, 32);
				#else
				Colour(GColour(255, 0, 255));
				#endif
				Rectangle();

				GPointF ctr(Px, Px);
				GPointF LeftPt(0.0, Px);
				GPointF TopPt(Px, 0.0);
				GPointF RightPt(X(), Px);
				GPointF BottomPt(Px, Y());
				int x_px[4] = {BorderPx->x1, BorderPx->x2, BorderPx->x2, BorderPx->x1};
				int y_px[4] = {BorderPx->y1, BorderPx->y1, BorderPx->y2, BorderPx->y2};
				GPointF *pts[4] = {&LeftPt, &TopPt, &RightPt, &BottomPt};
				
				// Draw border parts..
				for (int i=0; i<4; i++)
				{
					int k = (i + 1) % 4;

					// Setup the stops					
					GBlendStop stops[2] = { {0.0, 0}, {1.0, 0} };
					uint32 iColour = defs[i]->Color.IsValid() ? defs[i]->Color.Rgb32 : Back.c32();
					uint32 kColour = defs[k]->Color.IsValid() ? defs[k]->Color.Rgb32 : Back.c32();
					if (defs[i]->IsValid() && defs[k]->IsValid())
					{
						stops[0].c32 = iColour;
						stops[1].c32 = kColour;
					}
					else if (defs[i]->IsValid())
					{
						stops[0].c32 = stops[1].c32 = iColour;
					}
					else
					{
						stops[0].c32 = stops[1].c32 = kColour;
					}
					
					// Create a brush
					GLinearBlendBrush br
					(
						*pts[i],
						*pts[k],
						2,
						stops
					);
					
					// Setup the clip
					GRect clip(	(int)MIN(pts[i]->x, pts[k]->x),
								(int)MIN(pts[i]->y, pts[k]->y),
								(int)MAX(pts[i]->x, pts[k]->x)-1,
								(int)MAX(pts[i]->y, pts[k]->y)-1);
					ClipRgn(&clip);
					
					// Draw the arc...
					GPath p;
					p.Circle(ctr, Px);
					if (defs[i]->IsValid() || defs[k]->IsValid())
						p.Fill(this, br);
					
					// Fill the background
					p.Empty();
					p.Ellipse(ctr, Px-x_px[i], Px-y_px[i]);
					if (DrawBackground)
					{
						GSolidBrush br(Back);
						p.Fill(this, br);					
					}
					else
					{
						GEraseBrush br;
						p.Fill(this, br);
					}
					
					ClipRgn(NULL);
				}
				
				#if 0
				static int count = 0;
				GString file;
				file.Printf("c:\\temp\\img-%i.bmp", ++count);
				GdcD->Save(file, Corners);
				#endif
			}
		}
	}
};

void GTag::PaintBorderAndBackground(GSurface *pDC, GColour &Back, GRect *BorderPx)
{
	GArray<GRect> r;
	GRect BorderPxRc;
	bool DrawBackground = !Back.IsTransparent();

	#ifdef _DEBUG
	if (Debug)
	{
		//int asd=0;
	}
	#endif

	if (!BorderPx)
		BorderPx = &BorderPxRc;
	BorderPx->ZOff(0, 0);

	// Get all the border info and work out the pixel sizes.
	GFont *f = GetFont();
	BorderDef Left = BorderLeft();
	BorderPx->x1 = Left.ToPx(Size.x, f);
	BorderDef Top = BorderTop();
	BorderPx->y1 = Top.ToPx(Size.x, f);
	BorderDef Right = BorderRight();
	BorderPx->x2 = Right.ToPx(Size.x, f);
	BorderDef Bottom = BorderBottom();
	BorderPx->y2 = Bottom.ToPx(Size.x, f);
	GCss::BorderDef *defs[4] = {&Left, &Top, &Right, &Bottom};

	// Work out the rectangles
	switch (Display())
	{
		case DispInlineBlock:
		case DispBlock:
		{
			r[0].ZOff(Size.x-1, Size.y-1);
			break;
		}
		case DispInline:
		{
			GRegion rgn;
			GetInlineRegion(rgn);
			// rgn.Simplify(false);
			for (int i=0; i<rgn.Length(); i++)
			{
				GRect rc = *rgn[i];
				if (BorderPx)
				{
					rc.x1 -= BorderPx->x1 + PadPx.x1;
					rc.y1 -= BorderPx->y1 + PadPx.y1;
					rc.x2 += BorderPx->x2 + PadPx.x2;
					rc.y2 += BorderPx->y2 + PadPx.y2;
				}
				r.New() = rc;
			}
			break;
		}
		default:
			return;
	}

	// If we are drawing rounded corners, draw them into a memory context
	GAutoPtr<CornersImg> Corners;
	int Px = 0, Px2 = 0;
	GCss::Len Radius = BorderRadius();
	float RadPx = Radius.Type == GCss::LenPx ? Radius.Value : Radius.ToPx(Size.x, GetFont());
	bool HasRadius = Radius.Type != GCss::LenInherit && RadPx > 0.0f;

	/*
	if (Radius.Type != GCss::LenInherit &&
		RadPx > 0.0f)
	{
		Px = (int)ceil(RadPx);
		Px2 = Px << 1;
		if (Px2 > Size.y)
		{
			Px = Size.y / 2;
			Px2 = Px << 1;
		}		
		
		if (Corners.Reset(new GMemDC(Px2, Px2, System32BitColourSpace)))
		{
			#if 1
			Corners->Colour(0, 32);
			#else
			Corners->Colour(GColour(255, 0, 255));
			#endif
			Corners->Rectangle();

			GPointF ctr(Px, Px);
			GPointF LeftPt(0.0, Px);
			GPointF TopPt(Px, 0.0);
			GPointF RightPt(Corners->X(), Px);
			GPointF BottomPt(Px, Corners->Y());
			int x_px[4] = {BorderPx->x1, BorderPx->x2, BorderPx->x2, BorderPx->x1};
			int y_px[4] = {BorderPx->y1, BorderPx->y1, BorderPx->y2, BorderPx->y2};
			GPointF *pts[4] = {&LeftPt, &TopPt, &RightPt, &BottomPt};
			GCss::BorderDef *defs[4] = {&Left, &Top, &Right, &Bottom};
			
			// Draw border parts..
			for (int i=0; i<4; i++)
			{
				int k = (i + 1) % 4;

				// Setup the stops					
				GBlendStop stops[2] = { {0.0, 0}, {1.0, 0} };
				uint32 iColour = defs[i]->Color.IsValid() ? defs[i]->Color.Rgb32 : Back.c32();
				uint32 kColour = defs[k]->Color.IsValid() ? defs[k]->Color.Rgb32 : Back.c32();
				if (defs[i]->IsValid() && defs[k]->IsValid())
				{
					stops[0].c32 = iColour;
					stops[1].c32 = kColour;
				}
				else if (defs[i]->IsValid())
				{
					stops[0].c32 = stops[1].c32 = iColour;
				}
				else
				{
					stops[0].c32 = stops[1].c32 = kColour;
				}
				
				// Create a brush
				GLinearBlendBrush br
				(
					*pts[i],
					*pts[k],
					2,
					stops
				);
				
				// Setup the clip
				GRect clip(	(int)MIN(pts[i]->x, pts[k]->x),
							(int)MIN(pts[i]->y, pts[k]->y),
							(int)MAX(pts[i]->x, pts[k]->x)-1,
							(int)MAX(pts[i]->y, pts[k]->y)-1);
				Corners->ClipRgn(&clip);
				
				// Draw the arc...
				GPath p;
				p.Circle(ctr, Px);
				if (defs[i]->IsValid() || defs[k]->IsValid())
					p.Fill(Corners, br);
				
				// Fill the background
				p.Empty();
				p.Ellipse(ctr, Px-x_px[i], Px-y_px[i]);
				if (DrawBackground)
				{
					GSolidBrush br(Back);
					p.Fill(Corners, br);					
				}
				else
				{
					GEraseBrush br;
					p.Fill(Corners, br);
				}
				
				Corners->ClipRgn(NULL);
			}
			
			#if 0
			static int count = 0;
			GString file;
			file.Printf("c:\\temp\\img-%i.bmp", ++count);
			GdcD->Save(file, Corners);
			#endif
		}
	}
	*/

	// Loop over the rectangles and draw everything
	// bool IsAlpha = Back.a() < 0xff;
	int Op = pDC->Op(GDC_ALPHA);

	for (unsigned i=0; i<r.Length(); i++)
	{
		GRect rc = r[i];
		if (HasRadius)
		{
			Px = (int)ceil(RadPx);
			Px2 = Px << 1;
			if (Px2 > rc.Y())
			{
				Px = rc.Y() / 2;
				Px2 = Px << 1;
			}			
			if (!Corners || Corners->Px2 != Px2)
			{
				Corners.Reset(new CornersImg((float)Px, BorderPx, defs, Back, DrawBackground));
			}
			
			// top left
			GRect r(0, 0, Px-1, Px-1);
			pDC->Blt(rc.x1, rc.y1, Corners, &r);
			
			// top right
			r.Set(Px, 0, Corners->X()-1, Px-1);
			pDC->Blt(rc.x2-Px+1, rc.y1, Corners, &r);
			
			// bottom left
			r.Set(0, Px, Px-1, Corners->Y()-1);
			pDC->Blt(rc.x1, rc.y2-Px+1, Corners, &r);
			
			// bottom right
			r.Set(Px, Px, Corners->X()-1, Corners->Y()-1);
			pDC->Blt(rc.x2-Px+1, rc.y2-Px+1, Corners, &r);
			
			#if 1
			pDC->Colour(Back);
			pDC->Rectangle(rc.x1+Px, rc.y1, rc.x2-Px, rc.y2);
			pDC->Rectangle(rc.x1, rc.y1+Px, rc.x1+Px-1, rc.y2-Px);
			pDC->Rectangle(rc.x2-Px+1, rc.y1+Px, rc.x2, rc.y2-Px);
			#else
			pDC->Colour(GColour(255, 0, 0, 0x80));
			pDC->Rectangle(rc.x1+Px, rc.y1, rc.x2-Px, rc.y2);
			pDC->Colour(GColour(0, 255, 0, 0x80));
			pDC->Rectangle(rc.x1, rc.y1+Px, rc.x1+Px-1, rc.y2-Px);
			pDC->Colour(GColour(0, 0, 255, 0x80));
			pDC->Rectangle(rc.x2-Px+1, rc.y1+Px, rc.x2, rc.y2-Px);
			#endif
		}
		else if (DrawBackground)
		{
			pDC->Colour(Back);
			pDC->Rectangle(&rc);
			
			/*
			if (Debug)
			{
				pDC->Colour(GColour(255, 0, 0));
				pDC->Box(&rc);
			}
			*/
		}

		GCss::BorderDef *b;
		if ((b = &Left)->IsValid())
		{
			pDC->Colour(b->Color.Rgb32, 32);
			DrawBorder db(pDC, *b);
			for (int i=0; i<b->Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x1 + i, rc.y1+Px, rc.x1+i, rc.y2-Px);
			}
		}
		if ((b = &Top)->IsValid())
		{
			pDC->Colour(b->Color.Rgb32, 32);
			DrawBorder db(pDC, *b);
			for (int i=0; i<b->Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x1+Px, rc.y1+i, rc.x2-Px, rc.y1+i);
			}
		}
		if ((b = &Right)->IsValid())
		{
			pDC->Colour(b->Color.Rgb32, 32);
			DrawBorder db(pDC, *b);
			for (int i=0; i<b->Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x2-i, rc.y1+Px, rc.x2-i, rc.y2-Px);
			}			
		}
		if ((b = &Bottom)->IsValid())
		{
			pDC->Colour(b->Color.Rgb32, 32);
			DrawBorder db(pDC, *b);
			for (int i=0; i<b->Value; i++)
			{
				pDC->LineStyle(db.LineStyle, db.LineReset);
				pDC->Line(rc.x1+Px, rc.y2-i, rc.x2-Px, rc.y2-i);
			}
		}
	}

	pDC->Op(Op);
}

static void FillRectWithImage(GSurface *pDC, GRect *r, GSurface *Image, GCss::RepeatType Repeat)
{
	int Px = 0, Py = 0;
	int Old = pDC->Op(GDC_ALPHA);

	if (!Image)
		return;

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

void GTag::OnPaint(GSurface *pDC, bool &InSelection, uint16 Depth)
{
	if (Depth >= MAX_RECURSION_DEPTH ||
		Display() == DispNone)
		return;

	int Px, Py;
	pDC->GetOrigin(Px, Py);

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
				GColour back = _Colour(false);
				PaintBorderAndBackground(pDC, back, &Px);
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
			COLOUR b = GetBack();
			if (b != GT_TRANSPARENT)
			{
				pDC->Colour(b, 32);
				pDC->Rectangle(Pos.x, Pos.y, Pos.x+Size.x, Pos.y+Size.y);
			}
			if (Image)
			{
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
				#if ENABLE_IMAGE_RESIZING
				if
				(
					!ImageResized &&
					(				
						Size.x != Image->X() ||
						Size.y != Image->Y()
					)
				)
				{
					ImageResized = true;

					GColourSpace Cs = Image->GetColourSpace();
					if (Cs == CsIndex8 &&
						Image->AlphaDC())
						Cs = System32BitColourSpace;
					
					GAutoPtr<GSurface> r(new GMemDC(Size.x, Size.y, Cs));
					if (r)
					{
						if (Cs == CsIndex8)
							r->Palette(new GPalette(Image->Palette()));
						ResampleDC(r, Image);
						Image = r;
					}
				}
				#endif

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
		default:
		{
			GColour fore = _Colour(true);
			GColour back = _Colour(false);

			if (Display() == DispBlock && Html->Environment)
			{
				GCss::ImageDef Img = BackgroundImage();
				if (Img.Img)
				{
					GRect Clip(0, 0, Size.x-1, Size.y-1);
					pDC->ClipRgn(&Clip);					
					FillRectWithImage(pDC, &Clip, Img.Img, BackgroundRepeat());					
					pDC->ClipRgn(NULL);

					back.Empty();
				}
			}

			PaintBorderAndBackground(pDC, back, NULL);

			GFont *f = GetFont();
			#if DEBUG_TEXT_AREA
			bool IsEditor = Html ? !Html->GetReadOnly() : false;
			#else
			bool IsEditor = false;
			#endif
			if (f && TextPos.Length())
			{
				// This is the non-display part of the font bounding box
				int LeadingPx = (int)(f->Leading() + 0.5);
				
				// This is the displayable part of the font
				int FontPx = f->GetHeight() - LeadingPx;
				
				// This is the pixel height we're aiming to fill
				int EffectiveLineHt = LineHeightCache >= 0 ? MAX(FontPx, LineHeightCache) : FontPx;
				
				// This gets added to the y coord of each peice of text
				int LineHtOff = ((EffectiveLineHt - FontPx + 1) >> 1) - LeadingPx;
								
				#define FontColour(InSelection) \
					f->Transparent(!InSelection && !IsEditor); \
					if (InSelection) \
						f->Colour(LC_FOCUS_SEL_FORE, LC_FOCUS_SEL_BACK); \
					else \
					{ \
						GColour bk(back.IsTransparent() ? GColour(LC_WORKSPACE, 24) : back);			\
						GColour fr(fore.IsTransparent() ? GColour(DefaultTextColour, 32) : fore);		\
						if (IsEditor)																\
							bk = bk.Mix(GColour(0, 0, 0), 0.05f);									\
						f->Colour(fr, bk);															\
					}

				if (Html->HasSelection() &&
					(Selection >= 0 || Cursor >= 0) &&
					Selection != Cursor)
				{
					ssize_t Min = -1;
					ssize_t Max = -1;

					ssize_t Base = GetTextStart();
					if (Cursor >= 0 && Selection >= 0)
					{
						Min = MIN(Cursor, Selection) + Base;
						Max = MAX(Cursor, Selection) + Base;
					}
					else if (InSelection)
					{
						Max = MAX(Cursor, Selection) + Base;
					}
					else
					{
						Min = MAX(Cursor, Selection) + Base;
					}

					GRect CursorPos;
					CursorPos.ZOff(-1, -1);

					for (unsigned i=0; i<TextPos.Length(); i++)
					{
						GFlowRect *Tr = TextPos[i];
						ssize_t Start = Tr->Text - Text();
						ssize_t Done = 0;
						int x = Tr->x1;

						if (Tr->Len == 0)
						{
							// Is this a selection edge point?
							if (!InSelection && Min == 0)
							{
								InSelection = !InSelection;
							}
							else if (InSelection && Max == 0)
							{
								InSelection = !InSelection;
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
							ssize_t c = Tr->Len - Done;

							FontColour(InSelection);

							// Is this a selection edge point?
							if (		!InSelection &&
										Min - Start >= Done &&
										Min - Start < Done + Tr->Len)
							{
								InSelection = !InSelection;
								c = Min - Start - Done;
							}
							else if (	InSelection &&
										Max - Start >= Done &&
										Max - Start <= Tr->Len)
							{
								InSelection = !InSelection;
								c = Max - Start - Done;
							}

							// Draw the text run
							GDisplayString ds(f, Tr->Text + Done, c);
							if (IsEditor)
							{
								GRect r(x, Tr->y1, x + ds.X() - 1, Tr->y2);
								ds.Draw(pDC, x, Tr->y1 + LineHtOff, &r);
							}
							else
							{
								ds.Draw(pDC, x, Tr->y1 + LineHtOff);
							}
							x += ds.X();
							Done += c;
							
							// Is this is end of the tag?
							if (Tr->Len == Done)
							{
								// Is it also a selection edge?
								if (		!InSelection &&
											Min - Start == Done)
								{
									InSelection = !InSelection;
								}
								else if (	InSelection &&
											Max - Start == Done)
								{
									InSelection = !InSelection;
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
					FontColour(InSelection);

					ssize_t Base = GetTextStart();
					for (unsigned i=0; i<TextPos.Length(); i++)
					{
						GFlowRect *Tr = TextPos[i];
						if (!Tr)
							break;
						ssize_t Pos = (Tr->Text - Text()) - Base;

						LgiAssert(Tr->y2 >= Tr->y1);
						GDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1 + LineHtOff, IsEditor ? Tr : NULL);

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
							ssize_t Off = Tr->Text == PreText() ? StrlenW(PreText()) : Cursor - Pos;
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
					FontColour(InSelection);

					for (unsigned i=0; i<TextPos.Length(); i++)
					{
						GFlowRect *Tr = TextPos[i];
						GDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1 + LineHtOff, IsEditor ? Tr : NULL);
					}
				}
			}
			break;
		}
	}
	
	#if DEBUG_TABLE_LAYOUT && 0
	if (IsTableCell(TagId))
	{
		GTag *Tbl = this;
		while (Tbl->TagId != TAG_TABLE && Tbl->Parent)
			Tbl = Tbl->Parent;
		if (Tbl && Tbl->TagId == TAG_TABLE && Tbl->Debug)
		{
			pDC->Colour(GColour(255, 0, 0));
			pDC->Box(0, 0, Size.x-1, Size.y-1);
		}
	}
	#endif

	for (unsigned i=0; i<Children.Length(); i++)
	{
		GTag *t = ToTag(Children[i]);
		pDC->SetOrigin(Px - t->Pos.x, Py - t->Pos.y);
		t->OnPaint(pDC, InSelection, Depth + 1);
		pDC->SetOrigin(Px, Py);
	}
	
	#if DEBUG_DRAW_TD
	if (TagId == TAG_TD)
	{
		GTag *Tbl = this;
		while (Tbl && Tbl->TagId != TAG_TABLE)
			Tbl = ToTag(Tbl->Parent);
		if (Tbl && Tbl->Debug)
		{
			int Ls = pDC->LineStyle(GSurface::LineDot);
			pDC->Colour(GColour::Blue);
			pDC->Box(0, 0, Size.x-1, Size.y-1);
			pDC->LineStyle(Ls);
		}
	}
	#endif
}

//////////////////////////////////////////////////////////////////////
GHtml::GHtml(int id, int x, int y, int cx, int cy, GDocumentEnv *e) :
	GDocView(e),
	ResObject(Res_Custom),
	GHtmlParser(NULL)
{
	View = this;
	d = new GHtmlPrivate;
	SetReadOnly(true);
	ViewWidth = -1;
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Cursor = 0;
	Selection = 0;
	DocumentUid = 0;

	_New();
}

GHtml::~GHtml()
{
	_Delete();
	DeleteObj(d);

	if (JobSem.Lock(_FL))
	{
		JobSem.Jobs.DeleteObjects();
		JobSem.Unlock();
	}
}

void GHtml::_New()
{
	d->StyleDirty = false;
	d->IsLoaded = false;
	d->Content.x = d->Content.y = 0;
	d->DeferredLoads = 0;
	Tag = 0;
	DocCharSet.Reset();

	IsHtml = true;
	
	#ifdef DefaultFont
	GFont *Def = new GFont;
	if (Def)
	{
		if (Def->CreateFromCss(DefaultFont))
			SetFont(Def, true);
		else
			DeleteObj(Def);
	}
	#endif
	
	FontCache = new GFontCache(this);	
	SetScrollBars(false, false);
}

void GHtml::_Delete()
{
	LgiAssert(!d->IsParsing);

	CssStore.Empty();
	CssHref.Empty();
	OpenTags.Length(0);
	Source.Reset();
	DeleteObj(Tag);
	DeleteObj(FontCache);
}

GFont *GHtml::DefFont()
{
	return GetFont();
}

void GHtml::OnAddStyle(const char *MimeType, const char *Styles)
{
	if (Styles)
	{
		const char *c = Styles;
		bool Status = CssStore.Parse(c);
		if (Status)
		{
			d->StyleDirty = true;
		}

		#if 0 // def _DEBUG
		bool LogCss = false;
		if (!Status)
		{
			char p[MAX_PATH];
			sprintf_s(p, sizeof(p), "c:\\temp\\css_parse_failure_%i.txt", LgiRand());
			GFile f;
			if (f.Open(p, O_WRITE))
			{
				f.SetSize(0);
				if (CssStore.Error)
					f.Print("Error: %s\n\n", CssStore.Error.Get());
				f.Write(Styles, strlen(Styles));
				f.Close();
			}
		}

		if (LogCss)
		{
			GStringPipe p;
			CssStore.Dump(p);
			GAutoString a(p.NewStr());
			GFile f;
			if (f.Open("C:\\temp\\css.txt", O_WRITE))
			{
				f.Write(a, strlen(a));
				f.Close();
			}
		}
		#endif
	}
}

void GHtml::ParseDocument(const char *Doc)
{
	if (!Tag)
	{
		Tag = new GTag(this, 0);
	}

	if (GetCss())
		GetCss()->DeleteProp(GCss::PropBackgroundColor);
	if (Tag)
	{
		Tag->TagId = ROOT;
		OpenTags.Length(0);

		if (IsHtml)
		{
			Parse(Tag, Doc);

			// Add body tag if not specified...
			GTag *Html = Tag->GetTagByName("html");
			GTag *Body = Tag->GetTagByName("body");

			if (!Html && !Body)
			{
				if ((Html = new GTag(this, 0)))
					Html->SetTag("html");
				if ((Body = new GTag(this, Html)))
					Body->SetTag("body");
				
				Html->Attach(Body);

				if (Tag->Text())
				{
					GTag *Content = new GTag(this, Body);
					if (Content)
					{
						Content->TagId = CONTENT;
						Content->Text(NewStrW(Tag->Text()));
					}
				}
				while (Tag->Children.Length())
				{
					GTag *t = ToTag(Tag->Children.First());
					Body->Attach(t, Body->Children.Length());
				}
				DeleteObj(Tag);
				
				Tag = Html;
			}
			else if (!Body)
			{
				if ((Body = new GTag(this, Html)))
					Body->SetTag("body");
					
				for (unsigned i=0; i<Html->Children.Length(); i++)
				{
					GTag *t = ToTag(Html->Children[i]);
					if (t->TagId != TAG_HEAD)
					{
						Body->Attach(t);
						i--;
					}
				}
				
				Html->Attach(Body);
			}
			
			if (Html && Body)
			{
				char16 *t = Tag->Text();
				if (t)
				{
					if (ValidStrW(t))
					{
						GTag *Content = new GTag(this, 0);
						if (Content)
						{
							Content->Text(NewStrW(Tag->Text()));
							Body->Attach(Content, 0);
						}
					}
					Tag->Text(0);
				}

				#if 0 // Enabling this breaks the test file 'gw2.html'.
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
							while ((c = t->Tags.First()))
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
						Environment->OnExecuteScript(this, (char*)OnLoad);
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
	if (Tag)
		Tag->ResetCaches();
	Invalidate();
}

bool GHtml::NameW(const char16 *s)
{
	GAutoPtr<char, true> utf(WideToUtf8(s));
	return Name(utf);
}

char16 *GHtml::NameW()
{
	GBase::Name(Source);
	return GBase::NameW();
}

bool GHtml::Name(const char *s)
{
	SetDocumentUid(GetDocumentUid()+1);

	_Delete();
	_New();

	IsHtml = false;

	// Detect HTML
	const char *c = s;
	while ((c = strchr(c, '<')))
	{
		char *t = 0;
		c = ParseName((char*) ++c, &t);
		if (t && GetTagInfo(t))
		{
			DeleteArray(t);
			IsHtml = true;
			break;
		}
		DeleteArray(t);
	}

	// Parse
	d->IsParsing = true;
	ParseDocument(s);
	d->IsParsing = false;

	if (Tag && d->StyleDirty)
	{
		d->StyleDirty = false;
		Tag->RestyleAll();
	}

	if (d->DeferredLoads == 0)
	{
		OnLoad();
	}

	Invalidate();	

	return true;
}

char *GHtml::Name()
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)='%30.30s'\n", _FL, this, Source, Source);
	#endif

	if (!Source && Tag)
	{
		GStringPipe s(1024);
		Tag->CreateSource(s);
		Source.Reset(s.NewStr());
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
			int InitDeferredLoads = d->DeferredLoads;
			
			if (JobSem.Lock(_FL))
			{
				for (unsigned i=0; i<JobSem.Jobs.Length(); i++)
				{
					GDocumentEnv::LoadJob *j = JobSem.Jobs[i];
					int MyUid = GetDocumentUid();
					
					if (j->UserUid == MyUid &&
						j->UserData != NULL)
					{
						Html1::GTag *r = static_cast<Html1::GTag*>(j->UserData);
						
						if (d->DeferredLoads > 0)
							d->DeferredLoads--;
						
						// Check the tag is still in our tree...
						if (Tag->HasChild(r))
						{
							// Process the returned data...
							if (j->pDC)
							{
								r->SetImage(j->Uri, j->pDC.Release());
								ViewWidth = 0;
								Update = true;
							}
							else if (r->TagId == TAG_LINK)
							{
								if (!CssHref.Find(j->Uri))
								{
									GStreamI *s = j->GetStream();
									if (s)
									{
										int Size = (int)s->GetSize();
										GAutoString Style(new char[Size+1]);
										ssize_t rd = s->Read(Style, Size);
										if (rd > 0)
										{
											Style[rd] = 0;
											CssHref.Add(j->Uri, true);
											OnAddStyle("text/css", Style);									
											ViewWidth = 0;
											Update = true;
										}
									}
								}
							}
						}
					}
					// else it's from another (historical) HTML control, ignore
				}
				JobSem.Jobs.DeleteObjects();
				JobSem.Unlock();
			}
			
			if (InitDeferredLoads > 0 && d->DeferredLoads <= 0)
			{
				LgiAssert(d->DeferredLoads == 0);
				d->DeferredLoads = 0;
				OnLoad();
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
			int LineY = GetFont()->GetHeight();

			if (f == GNotifyScrollBar_Create && VScroll && LineY > 0)
			{
				int y = Y();
				int p = MAX(y / LineY, 1);
				int fy = d->Content.y / LineY;
				VScroll->SetPage(p);
				VScroll->SetLimits(0, fy);
			}
			
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

GdcPt2 GHtml::Layout(bool ForceLayout)
{
	GRect Client = GetClient();
	if (Tag && (ViewWidth != Client.X() || ForceLayout))
	{
		GFlowRegion f(this, Client, false);

		// Flow text, width is different
		Tag->OnFlow(&f, 0);
		ViewWidth = Client.X();
		d->Content.x = f.MAX.x + 1;
		d->Content.y = f.MAX.y + 1;

		// Set up scroll box
		bool Sy = f.y2 > Y();
		int LineY = GetFont()->GetHeight();

		uint64 Now = LgiCurrentTime();
		if (Now - d->SetScrollTime > 100)
		{
			d->SetScrollTime = Now;
			SetScrollBars(false, Sy);
			
			if (Sy && VScroll && LineY > 0)
			{
				int y = Y();
				int p = MAX(y / LineY, 1);
				int fy = f.y2 / LineY;
				VScroll->SetPage(p);
				VScroll->SetLimits(0, fy);
			}
		}
		else
		{
			// LgiTrace("%s - Dropping SetScroll, loop detected: %i ms\n", GetClass(), (int)(Now - d->SetScrollTime));
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

		#if GHTML_USE_DOUBLE_BUFFER
		GRect Client = GetClient();
		if (!MemDC ||
			(MemDC->X() < Client.X() || MemDC->Y() < Client.Y()))
		{
			if (MemDC.Reset(new GMemDC))
			{
				int Sx = Client.X() + 10;
				int Sy = Client.Y() + 10;
				if (!MemDC->Create(Sx, Sy, System32BitColourSpace))
				{
					MemDC.Reset();
				}
			}
		}
		if (MemDC)
		{
			MemDC->ClipRgn(NULL);
			#if 0//def _DEBUG
			MemDC->Colour(GColour(255, 0, 255));
			MemDC->Rectangle();
			#endif
		}
		#endif

		GSurface *pDC = MemDC ? MemDC : ScreenDC;

		GColour cBack;
		if (GetCss())
		{
			GCss::ColorDef Bk = GetCss()->BackgroundColor();
			if (Bk.Type == GCss::ColorRgb)
				cBack = Bk;
		}
		if (!cBack.IsValid())
			cBack.Set(Enabled() ? LC_WORKSPACE : LC_MED, 24);		
		pDC->Colour(cBack);
		pDC->Rectangle();

		if (Tag)
		{
			Layout();

			if (VScroll)
			{
				int LineY = GetFont()->GetHeight();
				int Vs = (int)VScroll->Value();
				pDC->SetOrigin(0, Vs * LineY);
			}

			bool InSelection = false;
			Tag->OnPaint(pDC, InSelection, 0);
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
		Cursor = NULL;
		i = true;
	}
	if (Selection)
	{
		Selection->Selection = -1;
		Selection = NULL;
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
	if (t && t->Children.Length())
	{
		for (GTag *i = ToTag(t->Children.Last()); i; )
		{
			GTag *c = i->Children.Length() ? ToTag(i->Children.Last()) : NULL;
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
	for (GTag *p = t; p; p = ToTag(p->Parent))
	{
		// Does this tag have a parent?
		if (p->Parent)
		{
			// Prev?
			GTag *pp = ToTag(p->Parent);
			ssize_t Idx = pp->Children.IndexOf(p);
			GTag *Prev = Idx > 0 ? ToTag(pp->Children[Idx - 1]) : NULL;
			if (Prev)
			{
				GTag *Last = GetLastChild(Prev);
				return Last ? Last : Prev;
			}
			else
			{
				return ToTag(p->Parent);
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
	if (t->Children.Length() > 0)
	{
		return ToTag(t->Children.First());
	}
	else
	{
		// Walk up the parent chain
		for (GTag *p = t; p; p = ToTag(p->Parent))
		{
			// Does this tag have a next?
			if (p->Parent)
			{
				GTag *pp = ToTag(p->Parent);
				size_t Idx = pp->Children.IndexOf(p);
				
				GTag *Next = pp->Children.Length() > Idx + 1 ? ToTag(pp->Children[Idx + 1]) : NULL;
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
	for (GTag *t = Tag; t; t = ToTag(t->Parent))
	{
		n++;
	}
	return n;
}

bool GHtml::IsCursorFirst()
{
	if (!Cursor || !Selection)
		return false;
	return CompareTagPos(Cursor, Cursor->Cursor, Selection, Selection->Selection);
}

bool GHtml::CompareTagPos(GTag *a, ssize_t AIdx, GTag *b, ssize_t BIdx)
{
	// Returns true if the 'a' is before 'b' point.
	if (!a || !b)
		return false;

	if (a == b)
	{
		return AIdx < BIdx;
	}
	else
	{
		GArray<GTag*> ATree, BTree;
		for (GTag *t = a; t; t = ToTag(t->Parent))
			ATree.AddAt(0, t);
		for (GTag *t = b; t; t = ToTag(t->Parent))
			BTree.AddAt(0, t);

		ssize_t Depth = MIN(ATree.Length(), BTree.Length());
		for (int i=0; i<Depth; i++)
		{
			GTag *at = ATree[i];
			GTag *bt = BTree[i];
			if (at != bt)
			{
				LgiAssert(i > 0);
				GTag *p = ATree[i-1];
				LgiAssert(BTree[i-1] == p);
				ssize_t ai = p->Children.IndexOf(at);
				ssize_t bi = p->Children.IndexOf(bt);
				return ai < bi;
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
		SendNotify(GNotifyShowImagesChanged);

		if (GetLoadImages() && Tag)
		{
			Tag->LoadImages();
		}
	}
}

char *GHtml::GetSelection()
{
	char *s = 0;

	if (Cursor && Selection)
	{
		GMemQueue p;
		bool InSelection = false;
		Tag->CopyClipboard(p, InSelection);

		int Len = (int)p.GetSize();
		if (Len > 0)
		{
			char16 *t = (char16*)p.New(sizeof(char16));
			if (t)
			{
				size_t Len = StrlenW(t);
				for (int i=0; i<Len; i++)
				{
					if (t[i] == 0xa0) t[i] = ' ';
				}
				s = WideToUtf8(t);
				DeleteArray(t);
			}
		}
	}

	return s;
}

bool GHtml::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;
	if (!_stricmp(Name, "ShowImages"))
	{
		SetLoadImages(Value.CastInt32() != 0);
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
		
		GAutoWString w(Utf8ToWide(s));
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
	for (unsigned i=0; i<Tag->Children.Length(); i++)
	{
		GTag *c = ToTag(Tag->Children[i]);
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
		
	GHashTbl<const char*,char*> f(0, false);
	Form->CollectFormValues(f);
	bool Status = false;
	if (!_stricmp(Method, "post"))
	{
		GStringPipe p(256);
		bool First = true;

		// const char *Field;
		// for (char *Val = f.First(&Field); Val; Val = f.Next(&Field))
		for (auto v : f)
		{
			if (First)
				First = false;
			else
				p.Write("&", 1);
			
			FormEncode(p, v.key);
			p.Write("=", 1);
			FormEncode(p, v.value);
		}
		
		GAutoPtr<const char, true> Data(p.NewStr());
		Status = Environment->OnPostForm(this, Action, Data);
	}
	else if (!_stricmp(Method, "get"))
	{
		Status = Environment->OnNavigate(this, Action);
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

		d->FindText.Reset(Utf8ToWide(Params->Find));
		d->MatchCase = Params->MatchCase;
	}		

	if (!Cursor)
		Cursor = Tag;

	if (Cursor && d->FindText)
	{
		GArray<GTag*> Tags;
		BuildTagList(Tags, Tag);
		ssize_t Start = Tags.IndexOf(Cursor);
		for (unsigned i=1; i<Tags.Length(); i++)
		{
			ssize_t Idx = (Start + i) % Tags.Length();
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
				Dy = (int) (VScroll ? -VScroll->Value() : 0);
				Status = true;
				break;
			}
			case VK_END:
			{
				if (VScroll)
				{
					int64 Low, High;
					VScroll->Limits(Low, High);
					Dy = (int) ((High - Page) - VScroll->Value());
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
	return GetFont()->GetHeight() * (VScroll ? (int)VScroll->Value() : 0);
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

		GTagHit Hit;
		if (Tag)
		{
			Tag->GetTagByPos(Hit, m.x, m.y + Offset, 0, false, DEBUG_TAG_BY_POS);
			#if DEBUG_TAG_BY_POS
			Hit.Dump("MouseClick");
			#endif
		}
		
		if (m.Left() && !m.IsContextMenu())
		{
			if (m.Double())
			{
				d->WordSelectMode = true;

				if (Cursor)
				{
					// Extend the selection out to the current word's boundaries.
					Selection = Cursor;
					Selection->Selection = Cursor->Cursor;

					if (Cursor->Text())
					{
						ssize_t Base = Cursor->GetTextStart();
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
						ssize_t Base = Selection->GetTextStart();
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
					SendNotify(GNotifySelectionChanged);
				}
			}
			else if (Hit.NearestText)
			{
				d->WordSelectMode = false;
				UnSelectAll();

				Cursor = Hit.NearestText;
				Cursor->Cursor = Hit.Index;

				#if DEBUG_SELECTION
				LgiTrace("StartSelect Near='%20S' Idx=%i\n", Hit.NearestText->Text(), Hit.Index);
				#endif

				OnCursorChanged();
				SendNotify(GNotifySelectionChanged);
			}
			else
			{
				#if DEBUG_SELECTION
				LgiTrace("StartSelect no text hit %p, %p\n", Cursor, Selection);
				#endif
			}
		}

		if (Hit.NearestText && Hit.Near == 0)
		{
			TagProcessedClick = Hit.NearestText->OnMouseClick(m);
		}
		else if (Hit.Direct)
		{
			TagProcessedClick = Hit.Direct->OnMouseClick(m);
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
			GSubMenu RClick;

			enum ContextMenuCmds
			{
				IDM_DUMP = 100,
				IDM_COPY_SRC,
				IDM_VIEW_SRC,
				IDM_EXTERNAL,
				IDM_COPY,
				IDM_VIEW_IMAGES,
			};
			#define IDM_CHARSET_BASE	10000

			RClick.AppendItem					(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
			GMenuItem *Vs = RClick.AppendItem	(LgiLoadString(L_VIEW_SOURCE, "View Source"), IDM_VIEW_SRC, Source != 0);
			RClick.AppendItem					(LgiLoadString(L_COPY_SOURCE, "Copy Source"), IDM_COPY_SRC, Source != 0);
			GMenuItem *Load = RClick.AppendItem	(LgiLoadString(L_VIEW_IMAGES, "View External Images"), IDM_VIEW_IMAGES, true);
			if (Load) Load->Checked(GetLoadImages());
			RClick.AppendItem					(LgiLoadString(L_VIEW_IN_DEFAULT_BROWSER, "View in Default Browser"), IDM_EXTERNAL, Source != 0);
			GSubMenu *Cs = RClick.AppendSub		(LgiLoadString(L_CHANGE_CHARSET, "Change Charset"));
			if (Cs)
			{
				int n=0;
				for (GCharset *c = LgiGetCsList(); c->Charset; c++, n++)
				{
					Cs->AppendItem(c->Charset, IDM_CHARSET_BASE + n, c->IsAvailable());
				}
			}
			
			if (!GetReadOnly() || // Is editor
				#ifdef _DEBUG
				1
				#else
				0
				#endif
				)
			{
				RClick.AppendSeparator();
				RClick.AppendItem("Dump Layout", IDM_DUMP, Tag != 0);
			}

			if (Vs)
			{
				Vs->Checked(!IsHtml);
			}
			
			if (OnContextMenuCreate(Hit, RClick) &&
				GetMouse(m, true))
			{
				int Id = RClick.Float(this, m.x, m.y);
				switch (Id)
				{
					case IDM_COPY:
					{
						Copy();
						break;
					}
					case IDM_VIEW_SRC:
					{
						if (Vs)
						{
							DeleteObj(Tag);
							IsHtml = !IsHtml;
							ParseDocument(Source);
						}
						break;
					}
					case IDM_COPY_SRC:
					{
						if (Source)
						{
							GClipBoard c(this);
							const char *ViewCs = GetCharset();
							if (ViewCs)
							{
								GAutoWString w((char16*)LgiNewConvertCp(LGI_WideCharset, Source, ViewCs));
								if (w)
									c.TextW(w);
							}
							else c.Text(Source);
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
						if (!Source)
						{
							LgiTrace("%s:%i - No HTML source code.\n", _FL);
							break;
						}

						char Path[MAX_PATH];
						if (!LgiGetSystemPath(LSP_TEMP, Path, sizeof(Path)))
						{
							LgiTrace("%s:%i - Failed to get the system path.\n", _FL);
							break;
						}
						
						char f[32];
						sprintf_s(f, sizeof(f), "_%i.html", LgiRand(1000000));
						LgiMakePath(Path, sizeof(Path), Path, f);
						
						GFile F;
						if (!F.Open(Path, O_WRITE))
						{
							LgiTrace("%s:%i - Failed to open '%s' for writing.\n", _FL, Path);
							break;
						}

						GStringPipe Ex;
						bool Error = false;
						
						F.SetSize(0);

						GAutoWString SrcMem;
						const char *ViewCs = GetCharset();
						if (ViewCs)
							SrcMem.Reset((char16*)LgiNewConvertCp(LGI_WideCharset, Source, ViewCs));
						else
							SrcMem.Reset(Utf8ToWide(Source));						

						for (char16 *s=SrcMem; s && *s;)
						{
							char16 *cid = StristrW(s, L"cid:");
							while (cid && !strchr("\'\"", cid[-1]))
							{
								cid = StristrW(cid+1, L"cid:");
							}

							if (cid)
							{
								char16 Delim = cid[-1];
								char16 *e = StrchrW(cid, Delim);
								if (e)
								{
									*e = 0;
									if (StrchrW(cid, '\n'))
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
												j->Uri.Reset(WideToUtf8(cid));
												j->Env = Environment;
												j->Pref = GDocumentEnv::LoadJob::FmtFilename;
												j->UserUid = GetDocumentUid();

												GDocumentEnv::LoadType Result = Environment->GetContent(j);
												if (Result == GDocumentEnv::LoadImmediate)
												{
													if (j->Filename)
														strcpy_s(File, sizeof(File), j->Filename);
												}
												else if (Result == GDocumentEnv::LoadDeferred)
												{
													d->DeferredLoads++;
												}
												
												DeleteObj(j);
											}
										}
										
										*e = Delim;
										Ex.Push(s, cid - s);
										if (File[0])
										{
											char *d;
											while ((d = strchr(File, '\\')))
											{
												*d = '/';
											}
											
											Ex.Push(L"file:///");
											
											GAutoWString w(Utf8ToWide(File));
											Ex.Push(w);
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

						if (!Error)
						{
							int64 WideChars = Ex.GetSize() / sizeof(char16);
							GAutoWString w(Ex.NewStrW());
							bool Only8Bit = true;
							for (char16 *c = w; *c; c++)
							{
								if (*c > 0xff)
								{
									Only8Bit = false;
									break;
								}
							}
							
							if (Only8Bit)
							{
								GAutoString Final;
								
								// Convert to 8bit chars
								if (!Final.Reset(new char[(size_t)WideChars+1]))
									break;

								char *o = Final;
								for (char16 *i = w; *i; i++)
								{
									*o++ = (char)*i;
								}
								*o = 0;
								F.Write(Final, o - Final);
								F.Close();
							}
							else
							{
								// Write a byte order mark and then unicode...
								union {
									uint16 ByteOrderMark;
									uint8 Chars[2];
								};
								ByteOrderMark = 0xfeff;
								F.Write(&ByteOrderMark, sizeof(ByteOrderMark));
								F.Write(w, (int)WideChars * sizeof(char16));
								F.Close();
							}
							
							GAutoString Err;
							if (!LgiExecute(Path, NULL, NULL, &Err))
							{
								LgiMsg(	this,
										"Failed to open '%s'\n%s",
										LgiApp ? LgiApp->GBase::Name() : GetClass(),
										MB_OK,
										Path,
										Err.Get());
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
								Charset = c->Charset;
								OverideDocCharset = true;

								char *Src = Source.Release();
								_Delete();
								_New();
								Source.Reset(Src);
								ParseDocument(Source);

								Invalidate();

								SendNotify(GNotifyCharsetChanged);
							}								
						}
						else
						{
							OnContextMenuCommand(Hit, Id);
						}
						break;
					}
				}
			}
		}
	}
	else // Up Click
	{
		if (Selection &&
			Cursor &&
			Selection == Cursor &&
			Selection->Selection == Cursor->Cursor)
		{
			Selection->Selection = -1;
			Selection = 0;
			SendNotify(GNotifySelectionChanged);

			#if DEBUG_SELECTION
			LgiTrace("NoSelect on release\n");
			#endif
		}
	}
}

void GHtml::OnLoad()
{
	d->IsLoaded = true;
	SendNotify(GNotifyDocLoaded);
}

GTag *GHtml::GetTagByPos(int x, int y, ssize_t *Index, GdcPt2 *LocalCoords, bool DebugLog)
{
	GTag *Status = NULL;

	if (Tag)
	{
		if (DebugLog)
			LgiTrace("GetTagByPos starting...\n");

		GTagHit Hit;
		Tag->GetTagByPos(Hit, x, y, 0, DebugLog);

		if (DebugLog)
			LgiTrace("GetTagByPos Hit=%s, %i, %i...\n\n", Hit.Direct ? Hit.Direct->Tag.Get() : 0, Hit.Index, Hit.Near);
		
		Status = Hit.NearestText && Hit.Near == 0 ? Hit.NearestText : Hit.Direct;
		if (Hit.NearestText && Hit.Near < 30)
		{
			if (Index) *Index = Hit.Index;
			if (LocalCoords) *LocalCoords = Hit.LocalCoords;
		}
	}

	return Status;
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

LgiCursor GHtml::GetCursor(int x, int y)
{
	int Offset = ScrollY();
	ssize_t Index = -1;
	GdcPt2 LocalCoords;
	GTag *Tag = GetTagByPos(x, y + Offset, &Index, &LocalCoords);
	if (Tag)
	{
		GAutoString Uri;
		if (LocalCoords.x >= 0 &&
			LocalCoords.y >= 0 &&
			Tag->IsAnchor(&Uri))
		{
			GRect c = GetClient();
			c.Offset(-c.x1, -c.y1);
			if (c.Overlap(x, y) && ValidStr(Uri))
			{
				return LCUR_PointingHand;
			}
		}
	}
	
	return LCUR_Normal;
}

void GHtml::OnMouseMove(GMouse &m)
{
	if (!Tag)
		return;

	int Offset = ScrollY();
	GTagHit Hit;
	Tag->GetTagByPos(Hit, m.x, m.y + Offset, 0, false);
	if (!Hit.Direct && !Hit.NearestText)
		return;

	GAutoString Uri;
	GTag *HitTag = Hit.NearestText && Hit.Near == 0 ? Hit.NearestText : Hit.Direct;
	if (HitTag &&
		HitTag->TipId == 0 &&
		Hit.LocalCoords.x >= 0 &&
		Hit.LocalCoords.y >= 0 &&
		HitTag->IsAnchor(&Uri) &&
		Uri)
	{
		if (!Tip.GetParent())
		{
			Tip.Attach(this);
		}

		GRect r = HitTag->GetRect(false);
		r.Offset(0, -Offset);
		HitTag->TipId = Tip.NewTip(Uri, r);
		// LgiTrace("NewTip: %s @ %s, ID=%i\n", Uri.Get(), r.GetStr(), HitTag->TipId);
	}

	if (IsCapturing() &&
		Cursor &&
		Hit.NearestText)
	{
		if (!Selection)
		{
			Selection = Cursor;
			Selection->Selection = Cursor->Cursor;
			Cursor = Hit.NearestText;
			Cursor->Cursor = Hit.Index;
			OnCursorChanged();
			Invalidate();
			
			SendNotify(GNotifySelectionChanged);

			#if DEBUG_SELECTION
			LgiTrace("CreateSelection '%20S' %i\n", Hit.NearestText->Text(), Hit.Index);
			#endif
		}
		else if ((Cursor != Hit.NearestText) ||
				 (Cursor->Cursor != Hit.Index))
		{
			// Move the cursor to track the mouse
			if (Cursor)
			{
				Cursor->Cursor = -1;
			}

			Cursor = Hit.NearestText;
			Cursor->Cursor = Hit.Index;
			#if DEBUG_SELECTION
			LgiTrace("ExtendSelection '%20S' %i\n", Hit.NearestText->Text(), Hit.Index);
			#endif

			if (d->WordSelectMode && Cursor->Text())
			{
				ssize_t Base = Cursor->GetTextStart();
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
			SendNotify(GNotifySelectionChanged);
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
		if (t->Get("id", i) && _stricmp(i, id) == 0)
			return t;

		for (unsigned i=0; i<t->Children.Length(); i++)
		{
			GTag *c = ToTag(t->Children[i]);
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

bool GHtml::GetFormattedContent(const char *MimeType, GString &Out, GArray<GDocView::ContentMedia> *Media)
{
	if (!MimeType)
	{
		LgiAssert(!"No MIME type for getting formatted content");
		return false;
	}

	if (!_stricmp(MimeType, "text/html"))
	{
		// We can handle this type...
		GArray<GTag*> Imgs;
		if (Media)
		{
			// Find all the image tags...
			Tag->Find(TAG_IMG, Imgs);

			// Give them CID's if they don't already have them
			for (unsigned i=0; i<Imgs.Length(); i++)
			{
				GTag *Img = Imgs[i];
				if (!Img)
					continue;
				
				const char *Cid = NULL, *Src;
				if (Img->Get("src", Src) &&
					!Img->Get("cid", Cid))
				{
					char id[256];
					sprintf_s(id, sizeof(id), "%x.%x", (unsigned)LgiCurrentTime(), (unsigned)LgiRand());
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
							m.Id = Cid;
							m.Stream.Reset(f);
						}
					}
				}
			}
		}

		// Export the HTML, including the CID's from the first step
		Out = Name();
	}
	else if (!_stricmp(MimeType, "text/plain"))
	{
		// Convert DOM tree down to text instead...
		GStringPipe p(512);
		if (Tag)
		{
			GTag::TextConvertState State(&p);
			Tag->ConvertToText(State);
		}
		Out = p.NewGStr();
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

GHtmlElement *GHtml::CreateElement(GHtmlElement *Parent)
{
	return new GTag(this, Parent);
}

bool GHtml::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!_stricmp(Name, "supportLists")) // Type: Bool
		Value = false;
	else if (!_stricmp(Name, "vml")) // Type: Bool
		// Vector Markup Language
		Value = false;
	else if (!_stricmp(Name, "mso")) // Type: Bool
		// mso = Microsoft Office
		Value = false;
	else
		return false;

	return true;
}

bool GHtml::EvaluateCondition(const char *Cond)
{
	if (!Cond)
		return true;
	
	// This is a really bad attempt at writing an expression evaluator.
	// I could of course use the scripting language but that would pull
	// in a fairly large dependency on the HTML control. However user
	// apps that already have that could reimplement this virtual function
	// if they feel like it.
	GArray<char*> Str;
	for (const char *c = Cond; *c; )
	{
		if (IsAlpha(*c))
		{
			Str.Add(LgiTokStr(c));
		}
		else if (IsWhiteSpace(*c))
		{
			c++;
		}
		else
		{
			const char *e = c;
			while (*e && !IsWhiteSpace(*e) && !IsAlpha(*e))
				e++;
			Str.Add(NewStr(c, e - c));
			LgiAssert(e > c);
			if (e > c)
				c = e;
			else
				break;
		}
	}

	bool Result = true;
	bool Not = false;
	for (unsigned i=0; i<Str.Length(); i++)
	{
		char *s = Str[i];
		if (!_stricmp(s, "!"))
			Not = true;
		else
		{
			GVariant v;
			if (GetValue(s, v))
			{
				Result = v.CastInt32() != 0;
				if (Not) Result = !Result;
			}
			else
			{
				// If this fires: update GHtml::GetVariant with the variable.
				// LgiTrace("%s:%i - Unsupported variable '%s'\n", _FL, s);
			}
			Not = false;
		}
	}
	
	Str.DeleteArrays();
	
	return Result;
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
				VScroll->SendNotify();
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
		if (_stricmp(Class, "GHtml") == 0)
		{
			return new GHtml(-1, 0, 0, 100, 100, new GDefaultDocumentEnv);
		}

		return 0;
	}

} GHtml_Factory;

//////////////////////////////////////////////////////////////////////
struct BuildContext
{
	GHtmlTableLayout *Layout;
	GTag *Table;
	GTag *TBody;
	GTag *CurTr;
	GTag *CurTd;
	int cx, cy;
	
	BuildContext()
	{
		Layout = NULL;
		cx = cy = 0;

		Table = NULL;
		TBody = NULL;
		CurTr = NULL;
		CurTd = NULL;
	}

	bool Build(GTag *t, int Depth)
	{
		bool RetReattach = false;
		
		switch (t->TagId)
		{
			case TAG_TABLE:
			{
				if (!Table)
					Table = t;
				else
					return false;
				break;
			}
			case TAG_TBODY:
			{
				if (TBody)
					return false;
				TBody = t;
				break;
			}
			case TAG_TR:
			{
				CurTr = t;
				break;
			}
			case TAG_TD:
			{
				CurTd = t;
				if (t->Parent != CurTr)
				{
					if
					(
						!CurTr &&
						(Table || TBody)
					)
					{
						GTag *p = TBody ? TBody : Table;
						CurTr = new GTag(p->Html, p);
						if (CurTr)
						{
							CurTr->Tag.Reset(NewStr("tr"));
							CurTr->TagId = TAG_TR;
							

							ssize_t Idx = t->Parent->Children.IndexOf(t);
							t->Parent->Attach(CurTr, Idx);
						}
					}
					
					if (CurTr)
					{
						CurTr->Attach(t);
						RetReattach = true;
					}
					else
					{
						LgiAssert(0);
						return false;
					}
				}
				
				t->Cell->Pos.x = cx;
				t->Cell->Pos.y = cy;
				Layout->Set(t);
				break;
			}
			default:
			{
				if (CurTd == t->Parent)
					return false;
				break;
			}
		}

		for (unsigned n=0; n<t->Children.Length(); n++)
		{
			GTag *c = ToTag(t->Children[n]);
			bool Reattached = Build(c, Depth+1);
			if (Reattached)
				n--;
		}
		
		if (t->TagId == TAG_TR)
		{
			CurTr = NULL;
			cy++;
			cx = 0;
			Layout->s.y = cy;
		}
		if (t->TagId == TAG_TD)
		{
			CurTd = NULL;
			cx += t->Cell->Span.x;
			Layout->s.x = MAX(cx, Layout->s.x);
		}
		
		return RetReattach;
	}
};

GHtmlTableLayout::GHtmlTableLayout(GTag *table)
{
	Table = table;
	if (!Table)
		return;

	#if 0

	BuildContext Ctx;
	Ctx.Layout = this;
	Ctx.Build(table, 0);

	#else

	int y = 0;
	GTag *FakeRow = 0;
	GTag *FakeCell = 0;

	GTag *r;
	for (size_t i=0; i<Table->Children.Length(); i++)
	{
		r = ToTag(Table->Children[i]);
		if (r->Display() == GCss::DispNone)
			continue;
			
		if (r->TagId == TAG_TR)
		{
			FakeRow = 0;
			FakeCell = 0;
		}
		else if (r->TagId == TAG_TBODY)
		{
			ssize_t Index = Table->Children.IndexOf(r);
			for (size_t n=0; n<r->Children.Length(); n++)
			{
				GTag *t = ToTag(r->Children[n]);
				Table->Children.AddAt(++Index, t);
				t->Parent = Table;
				
				/*
				LgiTrace("Moving '%s'(%p) from TBODY(%p) into '%s'(%p)\n",
					t->Tag, t,
					r,
					t->Parent->Tag, t->Parent);
				*/
			}
			r->Children.Length(0);
		}
		else
		{
			if (!FakeRow)
			{
				if ((FakeRow = new GTag(Table->Html, 0)))
				{
					FakeRow->Tag.Reset(NewStr("tr"));
					FakeRow->TagId = TAG_TR;

					ssize_t Idx = Table->Children.IndexOf(r);
					Table->Attach(FakeRow, Idx);
				}
			}
			if (FakeRow)
			{
				if (!IsTableCell(r->TagId) && !FakeCell)
				{
					if ((FakeCell = new GTag(Table->Html, FakeRow)))
					{
						FakeCell->Tag.Reset(NewStr("td"));
						FakeCell->TagId = TAG_TD;
						if ((FakeCell->Cell = new GTag::TblCell))
						{
							FakeCell->Cell->Span.x = 1;
							FakeCell->Cell->Span.y = 1;
						}
					}
				}

				ssize_t Idx = Table->Children.IndexOf(r);
				r->Detach();

				if (IsTableCell(r->TagId))
				{
					FakeRow->Attach(r);
				}
				else
				{
					LgiAssert(FakeCell != NULL);
					FakeCell->Attach(r);
				}
				i = Idx - 1;
			}
		}
	}

	FakeCell = NULL;
	for (size_t n=0; n<Table->Children.Length(); n++)
	{
		GTag *r = ToTag(Table->Children[n]);
		if (r->TagId == TAG_TR)
		{
			int x = 0;
			for (size_t i=0; i<r->Children.Length(); i++)
			{
				GTag *cell = ToTag(r->Children[i]);
				if (!IsTableCell(cell->TagId))
				{
					if (!FakeCell)
					{
						// Make a fake TD cell
						FakeCell = new GTag(Table->Html, NULL);
						FakeCell->Tag.Reset(NewStr("td"));
						FakeCell->TagId = TAG_TD;
						if ((FakeCell->Cell = new GTag::TblCell))
						{
							FakeCell->Cell->Span.x = 1;
							FakeCell->Cell->Span.y = 1;
						}
						
						// Join the fake TD into the TR
						r->Children[i] = FakeCell;
						FakeCell->Parent = r;
					}
					else
					{
						// Not the first non-TD tag, so delete it from the TR. Only the
						// fake TD will remain in the TR.
						r->Children.DeleteAt(i--, true);
					}
					
					// Insert the tag into it as a child
					FakeCell->Children.Add(cell);
					cell->Parent = FakeCell;
					cell = FakeCell;
				}
				else
				{
					FakeCell = NULL;
				}
				
				if (IsTableCell(cell->TagId))
				{
					if (cell->Display() == GCss::DispNone)
						continue;
						
					while (Get(x, y))
					{
						x++;
					}

					cell->Cell->Pos.x = x;
					cell->Cell->Pos.y = y;
					Set(cell);
					x += cell->Cell->Span.x;
				}
			}

			y++;
			FakeCell = NULL;
		}
	}
	
	#endif
}

void GHtmlTableLayout::Dump()
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
			LgiTrace("%-10p", t);
		}
		LgiTrace("\n");

		for (x=0; x<Sx; x++)
		{
			GTag *t = Get(x, y);
			char s[256] = "";
			if (t)
				sprintf_s(s, sizeof(s), "%i,%i-%i,%i", t->Cell->Pos.x, t->Cell->Pos.y, t->Cell->Span.x, t->Cell->Span.y);
			LgiTrace("%-10s", s);
		}
		LgiTrace("\n");
	}

	LgiTrace("\n");
}

void GHtmlTableLayout::GetAll(List<GTag> &All)
{
	GHashTbl<void*, bool> Added;
	for (size_t y=0; y<c.Length(); y++)
	{
		CellArray &a = c[y];
		for (size_t x=0; x<a.Length(); x++)
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

void GHtmlTableLayout::GetSize(int &x, int &y)
{
	x = 0;
	y = (int)c.Length();

	for (size_t i=0; i<c.Length(); i++)
	{
		x = MAX(x, (int) c[i].Length());
	}
}

GTag *GHtmlTableLayout::Get(int x, int y)
{
	if (y >= (int) c.Length())
		return NULL;
	
	CellArray &a = c[y];
	if (x >= (int) a.Length())
		return NULL;
	
	return a[x];
}

bool GHtmlTableLayout::Set(GTag *t)
{
	if (!t)
		return false;

	for (int y=0; y<t->Cell->Span.y; y++)
	{
		for (int x=0; x<t->Cell->Span.x; x++)
		{
			// LgiAssert(!c[y][x]);
			c[t->Cell->Pos.y + y][t->Cell->Pos.x + x] = t;
		}
	}

	return true;
}

void GTagHit::Dump(const char *Desc)
{
	GArray<GTag*> d, n;
	GTag *t = Direct;
	unsigned i;
	for (i=0; i<3 && t; t = ToTag(t->Parent), i++)
	{
		d.AddAt(0, t);
	}
	t = NearestText;
	for (i=0; i<3 && t; t = ToTag(t->Parent), i++)
	{
		n.AddAt(0, t);
	}
	
	LgiTrace("Hit: %s Direct: ", Desc);
	for (i=0; i<d.Length(); i++)
		LgiTrace(">%s", d[i]->Tag ? d[i]->Tag.Get() : "CONTENT");
	LgiTrace(" Nearest: ");
	for (i=0; i<n.Length(); i++)
		LgiTrace(">%s", n[i]->Tag ? n[i]->Tag.Get() : "CONTENT");
	LgiTrace(" Local: %ix%i Index: %i Block: %s '%.10S'\n",
		LocalCoords.x, LocalCoords.y,
		Index,
		Block ? Block->GetStr() : NULL,
		Block ? Block->Text + Index : NULL);
}

////////////////////////////////////////////////////////////////////////
bool GCssStyle::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	if (!_stricmp(Name, "Display")) // Type: String
	{
		Value = Css->ToString(Css->Display());
		return Value.Str() != NULL;
	}
	else LgiAssert(!"Impl me.");
	
	return false;
}

bool GCssStyle::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	if (!_stricmp(Name, "display"))
	{
		const char *d = Value.Str();
		if (Css->ParseDisplayType(d))
		{
			GTag *t = dynamic_cast<GTag*>(Css);
			if (t)
			{
				t->Html->Layout(true);
				t->Html->Invalidate();
			}
			return true;
		}
	}
	else LgiAssert(!"Impl me.");
	
	return false;
}


