#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GTextView4.h"
#include "GInput.h"
#include "GScrollBar.h"
#ifdef WIN32
#include <imm.h>
#endif
#include "GClipBoard.h"
#include "GDisplayString.h"
#include "GViewPriv.h"
#include "GCssTools.h"
#include "GFontCache.h"

#include "GHtmlCommon.h"
#include "GHtmlParser.h"

#undef FixedToInt
#define FixedToInt(fixed)			((fixed)>>GDisplayString::FShift)
#undef IntToFixed
#define IntToFixed(val)				((val)<<GDisplayString::FShift)
#define DefaultCharset              "utf-8"
#define PtrCheckBreak(ptr)			if (!ptr) { LgiAssert(!"Invalid ptr"); break; }
#define CursorColour				GColour(0, 0, 0)

#define GDCF_UTF8					-1
#define LUIS_DEBUG					0
#define POUR_DEBUG					0
#define PROFILE_POUR				0

#define ALLOC_BLOCK					64
#define IDC_VS						1000

#ifndef IDM_OPEN
#define IDM_OPEN					1
#endif
#ifndef IDM_NEW
#define	IDM_NEW						2
#endif
#ifndef IDM_COPY
#define IDM_COPY					3
#endif
#ifndef IDM_CUT
#define IDM_CUT						4
#endif
#ifndef IDM_PASTE
#define IDM_PASTE					5
#endif
#define IDM_COPY_URL				6
#define IDM_AUTO_INDENT				7
#define IDM_UTF8					8
#define IDM_PASTE_NO_CONVERT		9
#ifndef IDM_UNDO
#define IDM_UNDO					10
#endif
#ifndef IDM_REDO
#define IDM_REDO					11
#endif
#define IDM_FIXED					12
#define IDM_SHOW_WHITE				13
#define IDM_HARD_TABS				14
#define IDM_INDENT_SIZE				15
#define IDM_TAB_SIZE				16
#define IDM_DUMP					17
#define IDM_RTL						18
#define IDM_COPY_ORIGINAL			19

#define PAINT_BORDER				Back
#define PAINT_AFTER_LINE			Back

#define CODEPAGE_BASE				100
#define CONVERT_CODEPAGE_BASE		200

#if !defined(WIN32) && !defined(toupper)
#define toupper(c)					(((c)>='a'&&(c)<='z') ? (c)-'a'+'A' : (c))
#endif

static char SelectWordDelim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

//////////////////////////////////////////////////////////////////////
class GText4Elem : public GHtmlElement
{
	GString Style;
	GString Classes;

public:
	GText4Elem(GHtmlElement *parent) : GHtmlElement(parent)
	{
	}

	bool Get(const char *attr, const char *&val)
	{
		if (!attr)
			return false;
		if (!_stricmp(attr, "style") && Style)
		{
			val = Style;
			return true;
		}
		else if (!_stricmp(attr, "class") && Classes)
		{
			val = Classes;
			return true;
		}
		return false;
	}
	
	void Set(const char *attr, const char *val)
	{
		if (!attr)
			return;
		if (!_stricmp(attr, "style"))
			Style = val;
		else if (!_stricmp(attr, "class"))
			Classes = val;		
	}
	
	void SetStyle()
	{
		int asd = 0;
	}
};

struct GText4ElemContext : public GCss::ElementCallback<GText4Elem>
{
	/// Returns the element name
	const char *GetElement(GText4Elem *obj)
	{
		return obj->Tag;
	}
	
	/// Returns the document unque element ID
	const char *GetAttr(GText4Elem *obj, const char *Attr)
	{
		const char *a = NULL;
		obj->Get(Attr, a);
		return a;
	}
	
	/// Returns the class
	bool GetClasses(GArray<const char *> &Classes, GText4Elem *obj)
	{
		const char *c;
		if (!obj->Get("class", c))
			return false;
		
		GString cls = c;
		GString::Array classes = cls.Split(" ");
		for (unsigned i=0; i<classes.Length(); i++)
			Classes.Add(NewStr(classes[i]));
		return true;
	}

	/// Returns the parent object
	GText4Elem *GetParent(GText4Elem *obj)
	{
		return dynamic_cast<GText4Elem*>(obj->Parent);
	}

	/// Returns an array of child objects
	GArray<GText4Elem*> GetChildren(GText4Elem *obj)
	{
		GArray<GText4Elem*> a;
		for (unsigned i=0; i<obj->Children.Length(); i++)
			a.Add(dynamic_cast<GText4Elem*>(obj->Children[i]));
		return a;
	}
};

class GDocFindReplaceParams3 : public GDocFindReplaceParams
{
public:
	// Find/Replace History
	char16 *LastFind;
	char16 *LastReplace;
	bool MatchCase;
	bool MatchWord;
	bool SelectionOnly;
	
	GDocFindReplaceParams3()
	{
		LastFind = 0;
		LastReplace = 0;
		MatchCase = false;
		MatchWord = false;
		SelectionOnly = false;
	}

	~GDocFindReplaceParams3()
	{
		DeleteArray(LastFind);
		DeleteArray(LastReplace);
	}
};

struct GNamedStyle : public GCss
{
	GString Name;
};

class GCssCache
{
	int Idx;
	GArray<GNamedStyle*> Styles;

public:
	GCssCache()
	{
		Idx = 1;
	}
	
	~GCssCache()
	{
		Styles.DeleteObjects();
	}

	bool OutputStyles(GStream &s, int TabDepth)
	{
		char Tabs[64];
		memset(Tabs, '\t', TabDepth);
		Tabs[TabDepth] = 0;
		
		for (unsigned i=0; i<Styles.Length(); i++)
		{
			GNamedStyle *ns = Styles[i];
			if (ns)
			{
				s.Print("%s.%s {\n", Tabs, ns->Name.Get());
				
				GAutoString a = ns->ToString();
				GString all = a.Get();
				GString::Array lines = all.Split("\n");
				for (unsigned n=0; n<lines.Length(); n++)
				{
					s.Print("%s%s\n", Tabs, lines[n].Get());
				}
				
				s.Print("%s}\n\n", Tabs);
			}
		}
		
		return true;
	}

	GNamedStyle *AddStyleToCache(GAutoPtr<GCss> &s)
	{
		if (!s)
			return NULL;

		// Look through existing styles for a match...			
		for (unsigned i=0; i<Styles.Length(); i++)
		{
			GNamedStyle *ns = Styles[i];
			if (*ns == *s)
				return ns;
		}
		
		// Not found... create new...
		GNamedStyle *ns = new GNamedStyle;
		if (ns)
		{
			ns->Name.Printf("style%i", Idx++);
			*(GCss*)ns = *s.Get();
			Styles.Add(ns);
		}
		
		return ns;
	}
};

class GTv4Priv :
	public GCss,
	public GHtmlParser,
	public GHtmlStaticInst,
	public GCssCache,
	public GFontCache
{
public:
	GTextView4 *View;
	GString OriginalText;
	GAutoWString WideNameCache;
	GAutoString UtfNameCache;
	GAutoPtr<GFont> Font;
	bool WordSelectMode;

	enum SelectModeType
	{
		Unselected = 0,
		Selected = 1,
	};

	enum SeekType
	{
		SkUnknown,
		
		SkLineStart,
		SkLineEnd,		
		SkDocStart,
		SkDocEnd,

		// Horizontal navigation		
		SkLeftChar,
		SkLeftWord,
		SkRightChar,
		SkRightWord,
		
		// Vertical navigation
		SkUpPage,
		SkUpLine,
		SkCurrentLine,		
		SkDownLine,
		SkDownPage,
	};

	struct DisplayStr;
	struct BlockCursor;
	class Block;

	bool Error(const char *file, int line, const char *fmt, ...)
	{
		LgiAssert(0);
		return false;
	}

	struct Flow
	{
		GTv4Priv *d;
		GSurface *pDC;	// Used for printing.

		int Left, Right;// Left and right margin positions as measured in px
						// from the left of the page (controls client area).
		int CurY;		// Current y position down the page in document co-ords
		bool Visible;	// true if the current block overlaps the visible page
						// If false, the implementation can take short cuts and
						// guess various dimensions.
	
		Flow(GTv4Priv *priv)
		{
			d = priv;
			pDC = NULL;
			Left = 0;
			Right = 1000;
			CurY = 0;
			Visible = true;
		}
		
		int X()
		{
			return Right - Left + 1;
		}
		
		GString Describe()
		{
			GString s;
			s.Printf("Left=%i Right=%i CurY=%i", Left, Right, CurY);
			return s;
		}
	};

	struct ColourPair
	{
		GColour Fore, Back;
		
		void Empty()
		{
			Fore.Empty();
			Back.Empty();
		}
	};

	class Text : public GArray<char16>
	{
		GNamedStyle *Style; // owned by the CSS cache
	
	public:
		ColourPair Colours;
		GColour Fore, Back;
		
		Text(const char16 *t = NULL, int Chars = -1)
		{
			Style = NULL;
			if (t)
				Add((char16*)t, Chars >= 0 ? Chars : StrlenW(t));
		}
		
		GNamedStyle *GetStyle()
		{
			return Style;
		}
				
		void SetStyle(GNamedStyle *s)
		{
			if (Style != s)
			{
				Style = s;
				Colours.Empty();
				
				if (Style)
				{			
					GCss::ColorDef c = Style->Color();
					if (c.Type == GCss::ColorRgb)
						Colours.Fore.Set(c.Rgb32, 32);
					c = Style->BackgroundColor();
					if (c.Type == GCss::ColorRgb)
						Colours.Back.Set(c.Rgb32, 32);
				}				
			}
		}
	};
	
	struct PaintContext
	{
		GSurface *pDC;
		SelectModeType Type;
		ColourPair Colours[2];
		BlockCursor *Cursor, *Select;
		
		PaintContext()
		{
			pDC = NULL;
			Type = Unselected;
			Cursor = NULL;
			Select = NULL;
		}
		
		GColour &Fore()
		{
			return Colours[Type].Fore;
		}

		GColour &Back()
		{
			return Colours[Type].Back;
		}

		void DrawBox(GRect &r, GRect &Edge, GColour &c)
		{
			if (Edge.x1 > 0 ||
				Edge.x2 > 0 ||
				Edge.y1 > 0 ||
				Edge.y2 > 0)
			{
				pDC->Colour(c);
				if (Edge.x1)
				{
					pDC->Rectangle(r.x1, r.y1, r.x1 + Edge.x1 - 1, r.y2);
					r.x1 += Edge.x1;
				}
				if (Edge.y1)
				{
					pDC->Rectangle(r.x1, r.y1, r.x2, r.y1 + Edge.y1 - 1);
					r.y1 += Edge.y1;
				}
				if (Edge.y2)
				{
					pDC->Rectangle(r.x1, r.y2 - Edge.y2 + 1, r.x2, r.y2);
					r.y2 -= Edge.y2;
				}
				if (Edge.x2)
				{
					pDC->Rectangle(r.x2 - Edge.x2 + 1, r.y1, r.x2, r.y2);
					r.x2 -= Edge.x2;
				}
			}
		}
	};

	struct HitTestResult
	{
		GdcPt2 In;
		Block *Blk;
		DisplayStr *Ds;
		int Idx;
		bool Near;
		
		HitTestResult(int x, int y)
		{
			In.x = x;
			In.y = y;
			Blk = NULL;
			Ds = NULL;
			Idx = -1;
			Near = false;
		}
	};

	class Block // is like a DIV in HTML, it's as wide as the page and
				// always starts and ends on a whole line.
	{
	public:
		/// This is the number of cursors current referencing this Block.
		int8 Cursors;
		
		Block()
		{
			Cursors = 0;
		}
		
		virtual ~Block()
		{
			// We must have removed cursors by the time we are deleted
			// otherwise there will be a hanging pointer in the cursor
			// object.
			LgiAssert(Cursors == 0);
		}
		
		virtual int Length() = 0;
		virtual bool HitTest(HitTestResult &htr) = 0;
		virtual bool GetPosFromIndex(GRect *CursorPos, GRect *LinePos, int Index) = 0;
		virtual bool OnLayout(Flow &f) = 0;
		virtual void OnPaint(PaintContext &Ctx) = 0;
		virtual bool ToHtml(GStream &s) = 0;
		
		/// This method moves a cursor index.
		/// \returns the new cursor index or -1 on error.
		virtual int Seek
		(
			/// [In] true if the next line is needed, false for the previous line
			SeekType To,
			/// [In] The inital offset.
			int Offset,
			/// [In] The x position hint
			int XPos
		) = 0;

		virtual void Dump() {}
		virtual GNamedStyle *GetStyle() = 0;
	};

	struct BlockCursor
	{
		// The block the cursor is in.
		Block *Blk;

		// This is the character offset of the cursor relative to
		// the start of 'Blk'.
		int Offset;
		
		// This is the position on the screen in doc coords.
		GRect Pos;
		
		// This is the position line that the cursor is on. This is
		// used to calculate the bounds for screen updates.
		GRect Line;

		BlockCursor(const BlockCursor &c)
		{
			Blk = NULL;
			*this = c;
		}
		
		BlockCursor(Block *b, int off)
		{
			Blk = NULL;
			Offset = -1;
			Pos.ZOff(-1, -1);
			Line.ZOff(-1, -1);

			if (b)
				Set(b, off);
		}
		
		~BlockCursor()
		{
			Set(NULL, 0);
		}
		
		BlockCursor &operator =(const BlockCursor &c)
		{
			if (Blk)
			{
				LgiAssert(Blk->Cursors > 0);
				Blk->Cursors--;
			}
			Blk = c.Blk;
			if (Blk)
			{
				LgiAssert(Blk->Cursors < 0x7f);
				Blk->Cursors++;
			}
			Offset = c.Offset;
			Pos = c.Pos;
			Line = c.Line;
			
			LgiAssert(Offset >= 0);
			
			return *this;
		}
		
		void Set(Block *b, int off)
		{
			if (Blk)
			{
				LgiAssert(Blk->Cursors > 0);
				Blk->Cursors--;
				Blk = NULL;
			}
			if (b)
			{
				Blk = b;
				LgiAssert(Blk->Cursors < 0x7f);
				Blk->Cursors++;
			}
			Offset = off;
			LgiAssert(Offset >= 0);
		}
	};
	
	GAutoPtr<BlockCursor> Cursor, Selection;

	/// This is part or all of a Text run
	struct DisplayStr : public GDisplayString
	{
		Text *Src;
		int OffsetY; // Offset of this string from the TextLine's box in the Y axis
		
		DisplayStr(Text *src, GFont *f, const char16 *s, int l = -1, GSurface *pdc = NULL) :
			GDisplayString(f, s, l, pdc)
		{
			Src = src;
			OffsetY = 0;
		}
	};

	/// This structure is a layout of a full line of text. Made up of one or more
	/// display string objects.
	struct TextLine
	{
		/// This is a position relative to the parent Block
		GRect PosOff;

		/// The array of display strings
		GArray<DisplayStr*> Strs;

		/// Is '1' for lines that have a new line character at the end.
		uint8 NewLine;
		
		TextLine(int XOffsetPx, int WidthPx, int YOffsetPx)
		{
			NewLine = 0;
			PosOff.ZOff(WidthPx-1, 0);
			PosOff.Offset(XOffsetPx, YOffsetPx);
		}

		int Length()
		{
			int Len = NewLine;
			for (unsigned i=0; i<Strs.Length(); i++)
				Len += Strs[i]->Length();
			return Len;
		}
		
		/// This runs after the layout line has been filled with display strings.
		/// It measures the line and works out the right offsets for each strings
		/// so that their baselines all match up correctly.
		void LayoutOffsets(int DefaultFontHt)
		{
			double BaseLine = 0.0;
			int HtPx = 0;
			
			for (unsigned i=0; i<Strs.Length(); i++)
			{
				DisplayStr *ds = Strs[i];
				GFont *f = ds->GetFont();
				double FontBase = f->Ascent();
				
				BaseLine = max(BaseLine, FontBase);
				HtPx = max(HtPx, ds->Y());
			}
			
			if (Strs.Length() == 0)
				HtPx = DefaultFontHt;
			else
				LgiAssert(HtPx > 0);
			
			for (unsigned i=0; i<Strs.Length(); i++)
			{
				DisplayStr *ds = Strs[i];
				GFont *f = ds->GetFont();
				double FontBase = f->Ascent();
				ds->OffsetY = (int)(BaseLine - FontBase);
				LgiAssert(ds->OffsetY >= 0);
			}
			
			PosOff.y2 = PosOff.y1 + HtPx - 1;
		}
	};
	
	class TextBlock : public Block
	{
		GNamedStyle *Style;

	public:
		GArray<Text*> Txt;
		GArray<TextLine*> Layout;
		GRect Margin, Border, Padding;
		GFont *Fnt;
		
		bool LayoutDirty;
		int Len; // chars in the whole block (sum of all Text lengths)
		GRect Pos; // position in document co-ordinates
		
		TextBlock()
		{
			LayoutDirty = false;
			Len = 0;
			Pos.ZOff(-1, -1);
			Style = NULL;
			Fnt = NULL;
			
			Margin.ZOff(0, 0);
			Border.ZOff(0, 0);
			Padding.ZOff(0, 0);
		}
		
		~TextBlock()
		{
			Txt.DeleteObjects();
		}

		void Dump()
		{
			LgiTrace("    margin=%s, border=%s, padding=%s\n",
				Margin.GetStr(),
				Border.GetStr(),
				Padding.GetStr());
			for (unsigned i=0; i<Txt.Length(); i++)
			{
				Text *t = Txt[i];
				GString s(t->Length() ? &t->First() : NULL);
				s = s.Strip();
				
				LgiTrace("    %p: style=%p/%s, txt(%i)=%s\n",
					t,
					t->GetStyle(),
					t->GetStyle() ? t->GetStyle()->Name.Get() : NULL,
					t->Length(),
					s.Get());
			}
		}
		
		GNamedStyle *GetStyle()
		{
			return Style;
		}
		
		void SetStyle(GNamedStyle *s)
		{
			if ((Style = s))
			{
				LgiAssert(Fnt != NULL);

				Margin.x1 = Style->MarginLeft().ToPx(Pos.X(), Fnt);
				Margin.y1 = Style->MarginTop().ToPx(Pos.Y(), Fnt);
				Margin.x2 = Style->MarginRight().ToPx(Pos.X(), Fnt);
				Margin.y2 = Style->MarginBottom().ToPx(Pos.Y(), Fnt);

				Border.x1 = Style->BorderLeft().ToPx(Pos.X(), Fnt);
				Border.y1 = Style->BorderTop().ToPx(Pos.Y(), Fnt);
				Border.x2 = Style->BorderRight().ToPx(Pos.X(), Fnt);
				Border.y2 = Style->BorderBottom().ToPx(Pos.Y(), Fnt);

				Padding.x1 = Style->PaddingLeft().ToPx(Pos.X(), Fnt);
				Padding.y1 = Style->PaddingTop().ToPx(Pos.Y(), Fnt);
				Padding.x2 = Style->PaddingRight().ToPx(Pos.X(), Fnt);
				Padding.y2 = Style->PaddingBottom().ToPx(Pos.Y(), Fnt);
			}
		}

		int Length()
		{
			return Len;
		}

		bool ToHtml(GStream &s)
		{
			s.Print("<p>");
			for (unsigned i=0; i<Txt.Length(); i++)
			{
				Text *t = Txt[i];
				GNamedStyle *style = t->GetStyle();
				if (style)
				{
					s.Print("<span class='%s'>%.*S</span>", style->Name.Get(), t->Length(), &t[0]);
				}
				else
				{
					s.Print("%.*S", t->Length(), &t[0]);
				}
			}
			s.Print("</p>\n");
			return true;
		}		

		bool GetPosFromIndex(GRect *CursorPos, GRect *LinePos, int Index)
		{
			if (!CursorPos || !LinePos)
			{
				LgiAssert(0);
				return false;
			}
		
			int CharPos = 0;
			for (unsigned i=0; i<Layout.Length(); i++)
			{
				TextLine *tl = Layout[i];
				PtrCheckBreak(tl);

				GRect r = tl->PosOff;
				r.Offset(Pos.x1, Pos.y1);
				
				int FixX = 0;
				for (unsigned n=0; n<tl->Strs.Length(); n++)
				{
					DisplayStr *ds = tl->Strs[n];
					int dsChars = ds->Length();
					
					if (Index >= CharPos &&
						Index <= CharPos + dsChars)
					{
						int CharOffset = Index - CharPos;
						if (CharOffset == 0)
						{
							// First char
							CursorPos->x1 = r.x1 + IntToFixed(FixX);
						}
						else if (CharOffset == dsChars)
						{
							// Last char
							CursorPos->x1 = r.x1 + IntToFixed(FixX + ds->FX());
						}
						else
						{
							// In the middle somewhere...
							GDisplayString Tmp(ds->GetFont(), *ds, CharOffset);
							CursorPos->x1 = r.x1 + IntToFixed(FixX + Tmp.FX());
						}

						CursorPos->y1 = r.y1 + ds->OffsetY;
						CursorPos->y2 = CursorPos->y1 + ds->Y() - 1;
						CursorPos->x2 = CursorPos->x1 + 1;

						*LinePos = r;
						return true;
					}					
					
					FixX += ds->FX();
					CharPos += ds->Length();
				}
				
				if (tl->Strs.Length() == 0 && Index == CharPos)
				{
					// Cursor at the start of empty line.
					CursorPos->x1 = r.x1;
					CursorPos->x2 = CursorPos->x1 + 1;
					CursorPos->y1 = r.y1;
					CursorPos->y2 = r.y2;
					*LinePos = r;
					return true;
				}
				
				CharPos += tl->NewLine;
			}
			
			return false;
		}
		
		bool HitTest(HitTestResult &htr)
		{
			if (!Pos.Overlap(htr.In.x, htr.In.y))
				return false;

			int CharPos = 0;
			for (unsigned i=0; i<Layout.Length(); i++)
			{
				TextLine *tl = Layout[i];
				PtrCheckBreak(tl);

				GRect r = tl->PosOff;
				r.Offset(Pos.x1, Pos.y1);
				bool Over = r.Overlap(htr.In.x, htr.In.y);
				bool OnThisLine =	htr.In.y >= r.y1 &&
									htr.In.y <= r.y2;
				if (OnThisLine && htr.In.x < r.x1)
				{
					htr.Near = true;
					htr.Idx = CharPos;
					return true;
				}
				
				int FixX = 0;
				int InputX = IntToFixed(htr.In.x - Pos.x1);
				for (unsigned n=0; n<tl->Strs.Length(); n++)
				{
					DisplayStr *ds = tl->Strs[n];
					int dsFixX = ds->FX();
					
					if (Over &&
						InputX >= FixX &&
						InputX < FixX + dsFixX)
					{
						int OffFix = InputX - FixX;
						int OffPx = FixedToInt(OffFix);
						int OffChar = ds->CharAt(OffPx);
						
						htr.Blk = this;
						htr.Ds = ds;
						htr.Idx = CharPos + OffChar;
						return true;
					}
					
					FixX += ds->FX();

					CharPos += ds->Length();
				}

				if (OnThisLine && htr.In.x > r.x2)
				{
					htr.Near = true;
					htr.Idx = CharPos;
					return true;
				}
				
				CharPos += tl->NewLine;
			}

			return false;
		}
		
		void OnPaint(PaintContext &Ctx)
		{
			int CharPos = 0;
			int EndPoints = 0;
			int EndPoint[2] = {-1, -1};
			int CurEndPoint = 0;

			if (Cursors > 0 && Ctx.Select)
			{
				// Selection end point checks...
				if (Ctx.Cursor && Ctx.Cursor->Blk == this)
					EndPoint[EndPoints++] = Ctx.Cursor->Offset;
				if (Ctx.Select && Ctx.Select->Blk == this)
					EndPoint[EndPoints++] = Ctx.Select->Offset;
				
				// Sort the end points
				if (EndPoints > 1 &&
					EndPoint[0] > EndPoint[1])
				{
					int ep = EndPoint[0];
					EndPoint[0] = EndPoint[1];
					EndPoint[1] = ep;
				}
			}
			
			// Paint margins, borders and padding...
			GRect r = Pos;
			r.x1 -= Margin.x1;
			r.y1 -= Margin.y1;
			r.x2 -= Margin.x2;
			r.y2 -= Margin.y2;
			GCss::ColorDef BorderStyle;
			if (Style)
				BorderStyle = Style->BorderLeft().Color;
			GColour BorderCol(222, 222, 222);
			if (BorderStyle.Type == GCss::ColorRgb)
				BorderCol.Set(BorderStyle.Rgb32, 32);

			Ctx.DrawBox(r, Margin, Ctx.Colours[Unselected].Back);
			Ctx.DrawBox(r, Border, BorderCol);
			Ctx.DrawBox(r, Padding, Ctx.Colours[Unselected].Back);
			
			for (unsigned i=0; i<Layout.Length(); i++)
			{
				TextLine *Line = Layout[i];

				GRect LinePos = Line->PosOff;
				LinePos.Offset(Pos.x1, Pos.y1);
				if (Line->PosOff.X() < Pos.X())
				{
					Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
					Ctx.pDC->Rectangle(LinePos.x2, LinePos.y1, Pos.x2, LinePos.y2);
				}

				int FixX = IntToFixed(LinePos.x1);
				int CurY = LinePos.y1;
				GFont *Fnt = NULL;

				for (unsigned n=0; n<Line->Strs.Length(); n++)
				{
					DisplayStr *Ds = Line->Strs[n];
					GFont *f = Ds->GetFont();
					ColourPair &Cols = Ds->Src->Colours;
					if (f != Fnt)
					{
						f->Transparent(false);
						Fnt = f;
					}

					// If the current text part doesn't cover the full line height we have to
					// fill in the rest here...
					if (Ds->Y() < Line->PosOff.Y())
					{
						Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
						int CurX = FixedToInt(FixX);
						if (Ds->OffsetY > 0)
							Ctx.pDC->Rectangle(CurX, CurY, CurX+Ds->X(), CurY+Ds->OffsetY-1);

						int DsY2 = Ds->OffsetY + Ds->Y();
						if (DsY2 < Pos.Y())
							Ctx.pDC->Rectangle(CurX, CurY+DsY2, CurX+Ds->X(), Pos.y2);
					}

					// Check for selection changes...
					int FixY = IntToFixed(CurY + Ds->OffsetY);
					if (CurEndPoint < EndPoints &&
						EndPoint[CurEndPoint] >= CharPos &&
						EndPoint[CurEndPoint] <= CharPos + Ds->Length())
					{
						// Process string into parts based on the selection boundaries
						const char16 *s = *(GDisplayString*)Ds;
						int Ch = EndPoint[CurEndPoint] - CharPos;
						GDisplayString ds1(f, s, Ch);
						
						// First part...
						f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
									Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
						ds1.FDraw(Ctx.pDC, FixX, FixY);
						FixX += ds1.FX();
						Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
						CurEndPoint++;
						
						// Is there 3 parts?
						//
						// This happens when the selection starts and end in the one string.
						//
						// The alternative is that it starts or ends in the strings but the other
						// end point is in a different string. In which case there is only 2 strings
						// to draw.
						if (CurEndPoint < EndPoints &&
							EndPoint[CurEndPoint] >= CharPos &&
							EndPoint[CurEndPoint] < CharPos + Ds->Length())
						{
							// Yes..
							int Ch2 = EndPoint[CurEndPoint] - CharPos;

							// Part 2
							GDisplayString ds2(f, s + Ch, Ch2 - Ch);
							f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
										Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
							ds2.FDraw(Ctx.pDC, FixX, FixY);
							FixX += ds2.FX();
							Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
							CurEndPoint++;

							// Part 3
							GDisplayString ds3(f, s + Ch2);
							f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
										Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
							ds3.FDraw(Ctx.pDC, FixX, FixY);
							FixX += ds3.FX();
						}
						else
						{
							// No... draw 2nd part
							GDisplayString ds2(f, s + Ch);
							f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
										Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
							ds2.FDraw(Ctx.pDC, FixX, FixY);
							FixX += ds2.FX();
						}
					}
					else
					{
						// No selection changes... draw the whole string
						f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
									Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
						
						Ds->FDraw(Ctx.pDC, FixX, FixY);
						FixX += Ds->FX();
					}
					
					CharPos += Ds->Length();
				}
				
				CharPos += Line->NewLine;
			}

			if (Ctx.Cursor &&
				Ctx.Cursor->Blk == this)
			{
				Ctx.pDC->Colour(CursorColour);
				Ctx.pDC->Rectangle(&Ctx.Cursor->Pos);
			}
			#if 0 // def _DEBUG
			if (Ctx.Select &&
				Ctx.Select->Blk == this)
			{
				Ctx.pDC->Colour(GColour(255, 0, 0));
				Ctx.pDC->Rectangle(&Ctx.Select->Pos);
			}
			#endif
		}
		
		bool OnLayout(Flow &flow)
		{
			if (Pos.X() == flow.X() && !LayoutDirty)
			{
				flow.CurY = Pos.y2 + 1;
				return true;
			}

			LayoutDirty = false;
			Layout.DeleteObjects();
			
			/*
			GString Str = flow.Describe();
			LgiTrace("%p::OnLayout Flow: %s Style: %s, %s, %s, %s\n",
				this,
				Str.Get(),
				Style?Style->Name.Get():0,
				Margin.GetStr(),
				Border.GetStr(),
				Padding.GetStr());
			*/
			
			flow.Left += Margin.x1;
			flow.Right -= Margin.x2;
			flow.CurY += Margin.y1;
			
			Pos.x1 = flow.Left;
			Pos.y1 = flow.CurY;
			Pos.x2 = flow.Right;
			Pos.y2 = flow.CurY-1; // Start with a 0px height.
			
			flow.Left += Border.x1 + Padding.x1;
			flow.Right -= Border.x2 + Padding.x2;
			flow.CurY += Border.y1 + Padding.y1;

			/*
			Str = flow.Describe();
			LgiTrace("    Pos: %s Flow: %s\n", Pos.GetStr(), Str.Get());
			*/
			
			int FixX = 0; // Current x offset (fixed point) on the current line
			GAutoPtr<TextLine> CurLine(new TextLine(flow.Left - Pos.x1, flow.X(), flow.CurY - Pos.y1));
			if (!CurLine)
				return flow.d->Error(_FL, "alloc failed.");

			for (unsigned i=0; i<Txt.Length(); i++)
			{
				Text *t = Txt[i];
				
				// Get the font for 't'
				GFont *f = flow.d->GetFont(t->GetStyle());
				if (!f)
					return flow.d->Error(_FL, "font creation failed.");
				
				char16 *sStart = &(*t)[0];
				char16 *sEnd = sStart + t->Length();
				for (unsigned Off = 0; Off < t->Length(); )
				{					
					// How much of 't' is on the same line?
					char16 *s = sStart + Off;
					if (*s == '\n')
					{
						// New line handling...
						Off++;
						CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX);
						FixX = 0;
						CurLine->LayoutOffsets(f->GetHeight());
						Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
						CurLine->NewLine = 1;
						
						// LgiTrace("        [%i] = %s\n", Layout.Length(), CurLine->PosOff.GetStr());
						
						Layout.Add(CurLine.Release());
						CurLine.Reset(new TextLine(flow.Left - Pos.x1, flow.X(), Pos.Y()));
						continue;
					}

					char16 *e = s;
					while (*e != '\n' && e < sEnd)
						e++;
					
					// Add 't' to current line
					int Chars = min(1024, e - s);
					GAutoPtr<DisplayStr> Ds(new DisplayStr(t, f, s, Chars, flow.pDC));
					if (!Ds)
						return flow.d->Error(_FL, "display str creation failed.");

					if (FixedToInt(FixX) + Ds->X() > Pos.X())
					{
						// Wrap the string onto the line...
						int AvailablePx = Pos.X() - FixedToInt(FixX);
						int FitChars = Ds->CharAt(AvailablePx);
						if (FitChars < 0)
							flow.d->Error(_FL, "CharAt(%i) failed.", AvailablePx);
						else
						{
							// Wind back to the last break opportunity
							int ch;
							for (ch = FitChars-1; ch > 0; ch--)
							{
								if ((*t)[ch] == ' ') // FIXME: use better breaking ops
									break;
							}
							if (ch > FitChars >> 2)
								Chars = FitChars;
							
							// Create a new display string of the right size...
							if (!Ds.Reset(new DisplayStr(t, f, s, Chars, flow.pDC)))
								return flow.d->Error(_FL, "failed to create wrapped display str.");
							
							FixX = Ds->FX();
						}
					}
					else
					{
						FixX += Ds->FX();
					}
					
					if (!Ds)
						break;
					
					CurLine->PosOff.x2 = FixedToInt(FixX);
					CurLine->Strs.Add(Ds.Release());
					Off += Chars;
				}
			}
			
			if (CurLine && CurLine->Strs.Length() > 0)
			{
				CurLine->LayoutOffsets(CurLine->Strs.Last()->GetFont()->GetHeight());
				Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
				Layout.Add(CurLine.Release());
			}
			
			flow.CurY = Pos.y2 + 1 + Margin.y2 + Border.y2 + Padding.y2;
			flow.Left -= Margin.x1 + Border.x1 + Padding.x1;
			flow.Right += Margin.x2 + Border.x2 + Padding.x2;
			
			/*
			Str = flow.Describe();
			LgiTrace("    Pos: %s Flow: %s\n", Pos.GetStr(), Str.Get());
			*/

			return true;
		}
		
		Text *GetTextAt(uint32 Offset)
		{
			Text **t = &Txt[0];
			Text **e = t + Txt.Length();
			while (Offset > 0 && t < e)
			{
				if (Offset < (*t)->Length())
					return *t;
				Offset -= (*t)->Length();
			}
			return NULL;
		}
		
		bool AddText(GNamedStyle *Style, const char16 *Str, int Chars = -1)
		{
			if (!Str)
				return false;
			if (Chars < 0)
				Chars = StrlenW(Str);
			bool StyleDiff = (Style != NULL) ^ ((Txt.Length() ? Txt.Last()->GetStyle() : NULL) != NULL);
			Text *t = !StyleDiff && Txt.Length() ? Txt.Last() : NULL;
			if (t)
			{
				t->Add((char16*)Str, Chars);
				Len += Chars;
			}
			else if ((t = new Text(Str, Chars)))
			{
				Len += t->Length();
				Txt.Add(t);
			}
			else return false;

			t->SetStyle(Style);
			return true;
		}

		int Seek(SeekType To, int Offset, int XPos)
		{
			int XOffset = XPos - Pos.x1;
			int CharPos = 0;
			GArray<int> LineOffset;
			GArray<int> LineLen;
			int CurLine = -1;
			
			for (unsigned i=0; i<Layout.Length(); i++)
			{
				TextLine *Line = Layout[i];
				PtrCheckBreak(Line);
				int Len = Line->Length();				
				
				LineOffset[i] = CharPos;
				LineLen[i] = Len;
				
				if (Offset >= CharPos &&
					Offset <= CharPos + Len) // - Line->NewLine
				{
					CurLine = i;
				}				
				
				CharPos += Len;
			}
			
			if (CurLine < 0)
			{
				LgiAssert(!"Index not in layout lines.");
				return -1;
			}
				
			TextLine *Line = NULL;
			switch (To)
			{
				case SkLineStart:
				{
					return LineOffset[CurLine];
				}
				case SkLineEnd:
				{
					return	LineOffset[CurLine] +
							LineLen[CurLine] -
							Layout[CurLine]->NewLine;
				}
				case SkUpLine:
				{
					// Get previous line...
					if (CurLine == 0)
						return -1;
					Line = Layout[--CurLine];
					if (!Line)
						return -1;
					break;
				}				
				case SkDownLine:
				{
					// Get next line...
					if (CurLine >= (int)Layout.Length() - 1)
						return -1;
					Line = Layout[++CurLine];
					if (!Line)
						return -1;
					break;
				}
				default:
				{
					return false;
					break;
				}
			}
			
			if (Line)
			{
				// Work out where the cursor should be based on the 'XOffset'
				if (Line->Strs.Length() > 0)
				{
					int FixX = 0;
					int CharOffset = 0;
					for (unsigned i=0; i<Line->Strs.Length(); i++)
					{
						DisplayStr *Ds = Line->Strs[i];
						PtrCheckBreak(Ds);
						
						if (XOffset >= FixedToInt(FixX) &&
							XOffset <= FixedToInt(FixX + Ds->FX()))
						{
							// This is the matching string...
							int Px = XOffset - FixedToInt(FixX);
							int Char = Ds->CharAt(Px);
							if (Char >= 0)
							{
								return	LineOffset[CurLine] +	// Character offset of line
										CharOffset +			// Character offset of current string
										Char;					// Offset into current string for 'XOffset'
							}
						}
						
						FixX += Ds->FX();
						CharOffset += Ds->Length();
					}
					
					// Cursor is nearest the end of the string...?
					return LineOffset[CurLine] + Line->Length() - 1;
				}
				else if (Line->NewLine)
				{
					return LineOffset[CurLine];
				}
			}
			
			return -1;
		}
	};
	
	GArray<Block*> Blocks;

	GTv4Priv(GTextView4 *view) :
		GHtmlParser(view),
		GFontCache(SysFont)
	{
		View = view;
		WordSelectMode = false;
	}
	
	~GTv4Priv()
	{
		Empty();
	}
	
	void Empty()
	{
		// Delete cursors first to avoid hanging references
		Cursor.Reset();
		Selection.Reset();
		
		// Clear the block list..
		Blocks.DeleteObjects();
	}

	bool Seek(BlockCursor *In, SeekType Dir, bool Select)
	{
		if (!In || !In->Blk || Blocks.Length() == 0)
			return false;
		
		GAutoPtr<BlockCursor> c(new BlockCursor(*In));
		if (!c)
			return false;

		bool Status = false;
		switch (Dir)
		{
			case SkLineEnd:
			case SkLineStart:
			case SkUpLine:
			case SkDownLine:
			{
				int Off = c->Blk->Seek(Dir, c->Offset, c->Pos.x1);
				if (Off >= 0)
				{
					// Got the next line in the current block.
					c->Offset = Off;
					Status = true;
				}
				else if (Dir == SkUpLine || Dir == SkDownLine)
				{
					// No more lines in the current block...
					// Move to the next block.
					bool Up = Dir == SkUpLine;
					int CurIdx = Blocks.IndexOf(c->Blk);
					int NewIdx = CurIdx + (Up ? -1 : 1);
					if (NewIdx >= 0 && (unsigned)NewIdx < Blocks.Length())
					{
						Block *b = Blocks[NewIdx];
						if (!b)
							return false;
						
						c->Blk = b;
						c->Offset = Up ? b->Length() : 0;
						LgiAssert(c->Offset >= 0);
						Status = true;							
					}
				}
				break;
			}
			case SkDocStart:
			{
				c->Blk = Blocks[0];
				c->Offset = 0;
				Status = true;
				break;
			}
			case SkDocEnd:
			{
				c->Blk = Blocks.Last();
				c->Offset = c->Blk->Length();
				LgiAssert(c->Offset >= 0);
				Status = true;
				break;
			}
			case SkLeftChar:
			{
				if (c->Offset > 0)
				{
					c->Offset--;
					Status = true;
				}
				else // Seek to previous block
				{
					int Idx = Blocks.IndexOf(c->Blk);
					if (Idx > 0)
					{
						c->Blk = Blocks[--Idx];
						if (c->Blk)
						{
							c->Offset = 0;
							Status = true;
						}
					}
				}
				break;
			}
			case SkLeftWord:
			{
				/*
				bool StartWhiteSpace = IsWhiteSpace(Text[n]);
				bool LeftWhiteSpace = n > 0 && IsWhiteSpace(Text[n-1]);

				if (!StartWhiteSpace ||
					Text[n] == '\n')
				{
					n--;
				}
				
				// Skip ws
				for (; n > 0 && strchr(" \t", Text[n]); n--)
					;
				
				if (Text[n] == '\n')
				{
					n--;
				}
				else if (!StartWhiteSpace || !LeftWhiteSpace)
				{
					if (IsDelimiter(Text[n]))
					{
						for (; n > 0 && IsDelimiter(Text[n]); n--);
					}
					else
					{
						for (; n > 0; n--)
						{
							//IsWordBoundry(Text[n])
							if (IsWhiteSpace(Text[n]) ||
								IsDelimiter(Text[n]))
							{
								break;
							}
						}
					}
				}
				if (n > 0) n++;
				*/
				break;
			}
			case SkUpPage:
			{
				break;
			}
			case SkRightChar:
			{
				if (c->Offset < c->Blk->Length())
				{
					c->Offset++;
					Status = true;
				}
				else // Seek to next block
				{
					int Idx = Blocks.IndexOf(c->Blk);
					if (Idx < (int)Blocks.Length() - 1)
					{
						c->Blk = Blocks[++Idx];
						if (c->Blk)
						{
							c->Offset = 0;
							Status = true;
						}
					}
				}
				break;
			}
			case SkRightWord:
			{
				/*
				if (IsWhiteSpace(Text[n]))
				{
					for (; n<Size && IsWhiteSpace(Text[n]); n++);
				}
				else
				{
					if (IsDelimiter(Text[n]))
					{
						while (IsDelimiter(Text[n]))
						{
							n++;
						}
					}
					else
					{
						for (; n<Size; n++)
						{
							if (IsWhiteSpace(Text[n]) ||
								IsDelimiter(Text[n]))
							{
								break;
							}
						}
					}

					if (n < Size &&
						Text[n] != '\n')
					{
						if (IsWhiteSpace(Text[n]))
						{
							n++;
						}
					}
				}
				*/
				break;
			}
			case SkDownPage:
			{
				break;
			}
			default:
			{
				LgiAssert(!"Unknown seek type.");
				return false;
			}
		}
		
		if (Status)
		{
			c->Blk->GetPosFromIndex(&c->Pos, &c->Line, c->Offset);
			SetCursor(c, Select);
		}
		
		return Status;
	}
	
	bool CursorFirst()
	{
		if (!Cursor || !Selection)
			return true;
		
		int CIdx = Blocks.IndexOf(Cursor->Blk);
		int SIdx = Blocks.IndexOf(Selection->Blk);
		if (CIdx != SIdx)
			return CIdx < SIdx;
		
		return Cursor->Offset < Selection->Offset;
	}
	
	bool SetCursor(GAutoPtr<BlockCursor> c, bool Select = false)
	{
		GRect InvalidRc(0, 0, -1, -1);

		if (!c || !c->Blk)
		{
			LgiAssert(0);
			return false;
		}

		if (Select && !Selection)
		{
			// Selection starting... save cursor as selection end point
			if (Cursor)
				InvalidRc = Cursor->Line;
			Selection = Cursor;
		}
		else if (!Select && Selection)
		{
			// Selection ending... invalidate selection region and delete 
			// selection end point
			GRect r = SelectionRect();
			View->Invalidate(&r);
			Selection.Reset();

			// LgiTrace("Ending selection delete(sel) Idx=%i\n", i);
		}
		else if (Select && Cursor)
		{
			// Changing selection...
			InvalidRc = Cursor->Line;

			// LgiTrace("Changing selection region: %i\n", i);
		}

		if (Cursor && !Select)
		{
			// Just moving cursor
			View->Invalidate(&Cursor->Pos);
		}

		if (!Cursor)
			Cursor.Reset(new BlockCursor(*c));
		else
			*Cursor = *c;
		Cursor->Blk->GetPosFromIndex(&Cursor->Pos, &Cursor->Line, Cursor->Offset);
		if (Select)
			InvalidRc.Union(&Cursor->Line);
		else
			View->Invalidate(&Cursor->Pos);
		
		if (InvalidRc.Valid())
		{
			// Update the screen
			View->Invalidate(&InvalidRc);
		}

		return true;
	}

	GRect SelectionRect()
	{
		GRect SelRc;
		if (Cursor)
		{
			SelRc = Cursor->Line;
			if (Selection)
				SelRc.Union(&Selection->Line);
		}
		else if (Selection)
		{
			SelRc = Selection->Line;
		}
		return SelRc;
	}

	int IndexOfCursor(BlockCursor *c)
	{
		if (!c)
			return -1;

		int CharPos = 0;
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Block *b = Blocks[i];
			if (c->Blk == b)
				return CharPos + c->Offset;			
			CharPos += b->Length();
		}
		
		LgiAssert(0);
		return -1;
	}
	
	int HitTest(int x, int y)
	{
		int CharPos = 0;
		HitTestResult r(x, y);
		
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Block *b = Blocks[i];
			if (b->HitTest(r))
				return CharPos + r.Idx;
			
			CharPos += b->Length();
		}
		
		return -1;
	}

	Block *GetBlockByIndex(int Index, int *Offset = NULL)
	{
		int CharPos = 0;
		
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Block *b = Blocks[i];
			if (Index >= CharPos &&
				Index < CharPos + b->Length())
			{
				if (Offset)
					*Offset = Index - CharPos;
				return b;
			}
			
			CharPos += b->Length();
		}
		
		return NULL;
	}
	
	void Layout(GRect &Client)
	{
		Flow f(this);
		
		f.Left = Client.x1;
		f.Right = Client.x2;
		f.CurY = Client.y1;
		
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Blocks[i]->OnLayout(f);
		}
		
		if (Cursor)
		{
			LgiAssert(Cursor->Blk != NULL);
			if (Cursor->Blk)
				Cursor->Blk->GetPosFromIndex(&Cursor->Pos,
											 &Cursor->Line,
											 Cursor->Offset);
		}
	}
	
	void Paint(GSurface *pDC)
	{
		PaintContext Ctx;
		
		Ctx.pDC = pDC;
		Ctx.Cursor = Cursor;
		Ctx.Select = Selection;
		Ctx.Colours[Unselected].Fore.Set(LC_TEXT, 24);
		Ctx.Colours[Unselected].Back.Set(LC_WORKSPACE, 24);		
		
		if (View->Focus())
		{
			Ctx.Colours[Selected].Fore.Set(LC_FOCUS_SEL_FORE, 24);
			Ctx.Colours[Selected].Back.Set(LC_FOCUS_SEL_BACK, 24);
		}
		else
		{
			Ctx.Colours[Selected].Fore.Set(LC_NON_FOCUS_SEL_FORE, 24);
			Ctx.Colours[Selected].Back.Set(LC_NON_FOCUS_SEL_BACK, 24);
		}
		
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Block *b = Blocks[i];
			if (b)
				b->OnPaint(Ctx);
		}
	}
	
	GHtmlElement *CreateElement(GHtmlElement *Parent)
	{
		return new GText4Elem(Parent);
	}
	
	struct CreateContext
	{
		TextBlock *Tb;
		GArray<char16> Buf;
		char16 LastChar;
		GFontCache *FontCache;
		GCss::Store StyleStore;
		
		CreateContext(GFontCache *fc)
		{
			Tb = NULL;
			LastChar = '\n';
			FontCache = fc;
		}
		
		void AddText(GNamedStyle *Style, char16 *Str)
		{
			if (!Str || !Tb)
				return;
			
			int Used = 0;
			char16 *s = Str;
			char16 *e = s + StrlenW(s);
			while (s < e)
			{
				if (*s == '\r')
				{
					s++;
					continue;
				}
				
				if (IsWhiteSpace(*s))
				{
					Buf[Used++] = ' ';
					while (s < e && IsWhiteSpace(*s))
						s++;
				}
				else
				{
					Buf[Used++] = *s++;
					while (s < e && !IsWhiteSpace(*s))
					{
						Buf[Used++] = *s++;
					}
				}
			}
			
			if (Used > 0)
			{
				Tb->AddText(Style, &Buf[0], Used);
				LastChar = Buf[Used-1];
			}
		}
	};
	
	GAutoPtr<CreateContext> CreationCtx;

	bool ToHtml()		
	{
		GStringPipe p(256);
		
		p.Print("<html>\n"
				"<head>\n"
				"\t<style>\n");		
		OutputStyles(p, 1);		
		p.Print("\t</style>\n"
				"</head>\n"
				"<body>\n");
		
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Blocks[i]->ToHtml(p);
		}
		
		p.Print("</body>\n");
		return UtfNameCache.Reset(p.NewStr());
	}
	
	void DumpBlocks()
	{
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Block *b = Blocks[i];
			LgiTrace("%p: style=%p/%s {\n",
				b,
				b->GetStyle(),
				b->GetStyle() ? b->GetStyle()->Name.Get() : NULL);
			b->Dump();
			LgiTrace("}\n");
		}
	}
	
	bool FromHtml(GHtmlElement *e, CreateContext &ctx, GCss *ParentStyle = NULL, int Depth = 0)
	{
		char Sp[256];
		int SpLen = Depth << 1;
		memset(Sp, ' ', SpLen);
		Sp[SpLen] = 0;
		
		for (unsigned i = 0; i < e->Children.Length(); i++)
		{
			GHtmlElement *c = e->Children[i];
			GAutoPtr<GCss> Style;
			if (ParentStyle)
				Style.Reset(new GCss(*ParentStyle));
			
			// Check to see if the element is block level and end the previous
			// paragraph if so.
			c->Info = c->Tag ? GHtmlStatic::Inst->GetTagInfo(c->Tag) : NULL;
			bool IsBlock =	c->Info != NULL && c->Info->Block();

			switch (c->TagId)
			{
				case TAG_STYLE:
				{
					char16 *Style = e->GetText();
					if (Style)
						LgiAssert(!"Impl me.");
					continue;
					break;
				}
				case TAG_B:
				{
					if (Style.Reset(new GCss))
						Style->FontWeight(GCss::FontWeightBold);
					break;
				}
				/*
				case TAG_IMG:
				{
					ctx.Tb = NULL;
					
					const char *Src = NULL;
					if (e->Get("src", Src))
					{
						ImageBlock *Ib = new ImageBlock;
						if (Ib)
						{
							Ib->Src = Src;
							Blocks.Add(Ib);
						}
					}
					break;
				}
				*/
				default:
				{
					break;
				}
			}
			
			const char *Css, *Class;
			if (c->Get("style", Css))
			{
				if (!Style)
					Style.Reset(new GCss);
				if (Style)
					Style->Parse(Css, ParseRelaxed);
			}
			if (c->Get("class", Class))
			{
				GCss::SelArray Selectors;
				GText4ElemContext StyleCtx;
				if (ctx.StyleStore.Match(Selectors, &StyleCtx, dynamic_cast<GText4Elem*>(c)))
				{
					for (unsigned n=0; n<Selectors.Length(); n++)
					{
						GCss::Selector *sel = Selectors[n];
						if (sel)
						{
							const char *s = sel->Style;
							if (s)
							{
								if (!Style)
									Style.Reset(new GCss);
								if (Style)
								{
									LgiTrace("class style: %s\n", s);
									Style->Parse(s);
								}
							}
						}
					}
				}
			}

			GNamedStyle *CachedStyle = AddStyleToCache(Style);
			LgiTrace("%s%s IsBlock=%i CachedStyle=%p\n", Sp, c->Tag.Get(), IsBlock, CachedStyle);

			if ((IsBlock && ctx.LastChar != '\n') || c->TagId == TAG_BR)
			{
				if (!ctx.Tb)
					Blocks.Add(ctx.Tb = new TextBlock);
				if (ctx.Tb)
				{
					ctx.Tb->AddText(CachedStyle, L"\n");
					ctx.LastChar = '\n';
				}
			}

			bool EndStyleChange = false;
			if (IsBlock && ctx.Tb != NULL)
			{
				if (CachedStyle != ctx.Tb->GetStyle())
				{
					// Start a new block because the styles are different...
					EndStyleChange = true;
					Blocks.Add(ctx.Tb = new TextBlock);
					
					if (CachedStyle)
					{
						ctx.Tb->Fnt = ctx.FontCache->GetFont(CachedStyle);
						ctx.Tb->SetStyle(CachedStyle);
					}
				}
			}

			if (c->GetText())
			{
				if (!ctx.Tb)
					Blocks.Add(ctx.Tb = new TextBlock);
				ctx.AddText(CachedStyle, c->GetText());
			}
			
			if (!FromHtml(c, ctx, Style, Depth + 1))
				return false;
			
			if (EndStyleChange)
				ctx.Tb = NULL;
		}
		
		return true;
	}
};

//////////////////////////////////////////////////////////////////////
enum UndoType
{
	UndoDelete, UndoInsert, UndoChange
};

class GTextView4Undo : public GUndoEvent
{
	GTextView4 *View;
	UndoType Type;

public:
	GTextView4Undo(	GTextView4 *view,
					char16 *t,
					int len,
					int at,
					UndoType type)
	{
		View = view;
		Type = type;
	}

	~GTextView4Undo()
	{
	}

	void OnChange()
	{
	}

	// GUndoEvent
    void ApplyChange()
	{
	}

    void RemoveChange()
	{
	}
};

//////////////////////////////////////////////////////////////////////
GTextView4::GTextView4(	int Id,
						int x, int y, int cx, int cy,
						GFontType *FontType)
	: ResObject(Res_Custom)
{
	// init vars
	GView::d->Css.Reset(d = new GTv4Priv(this));
	
	// setup window
	SetId(Id);

	// default options
	#if WINNATIVE
	CrLf = true;
	SetDlgCode(DLGC_WANTALLKEYS);
	#else
	CrLf = false;
	#endif
	BackColour = LC_WORKSPACE;
	d->Padding(GCss::Len(GCss::LenPx, 4));
	d->BackgroundColor(GCss::ColorDef(GCss::ColorRgb, Rgb24To32(LC_WORKSPACE)));
	SetFont(SysFont);

	#if 0 // def _DEBUG
	Name("<html>\n"
		"<body>\n"
		"	This is some <b style='font-size: 20pt; color: green;'>bold text</b> to test with.<br>\n"
		"   A second line of text for testing.\n"
		"</body>\n"
		"</html>\n");
	#endif
}

GTextView4::~GTextView4()
{
	// 'd' is owned by the GView CSS autoptr.
}

bool GTextView4::IsDirty()
{
	return false;
}

void GTextView4::IsDirty(bool d)
{
}

char16 *GTextView4::MapText(char16 *Str, int Len, bool RtlTrailingSpace)
{
	if (ObscurePassword || ShowWhiteSpace || RtlTrailingSpace)
	{
	}

	return Str;
}

void GTextView4::SetFixedWidthFont(bool i)
{
	if (FixedWidthFont ^ i)
	{
		if (i)
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				GDocView::SetFixedWidthFont(i);
			}
		}

		OnFontChange();
		Invalidate();
	}
}

void GTextView4::SetReadOnly(bool i)
{
	GDocView::SetReadOnly(i);

	#if WINNATIVE
	SetDlgCode(i ? DLGC_WANTARROWS : DLGC_WANTALLKEYS);
	#endif
}

void GTextView4::SetTabSize(uint8 i)
{
	TabSize = limit(i, 2, 32);
	OnFontChange();
	OnPosChange();
	Invalidate();
}

void GTextView4::SetWrapType(uint8 i)
{
	GDocView::SetWrapType(i);

	OnPosChange();
	Invalidate();
}

GFont *GTextView4::GetFont()
{
	return d->Font;
}

void GTextView4::SetFont(GFont *f, bool OwnIt)
{
	if (!f)
		return;
	
	if (OwnIt)
	{
		d->Font.Reset(f);
	}
	else if (d->Font.Reset(new GFont))
	{		
		*d->Font = *f;
		d->Font->Create(NULL, 0, 0);
	}
	
	OnFontChange();
}

void GTextView4::OnFontChange()
{
}

void GTextView4::PourText(int Start, int Length /* == 0 means it's a delete */)
{
}

void GTextView4::PourStyle(int Start, int EditSize)
{
}

bool GTextView4::Insert(int At, char16 *Data, int Len)
{
	return false;
}

bool GTextView4::Delete(int At, int Len)
{
	return false;
}

void GTextView4::DeleteSelection(char16 **Cut)
{
}

int64 GTextView4::Value()
{
	char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void GTextView4::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LGI_PrintfInt64, i);
	Name(Str);
}

char *GTextView4::Name()
{
	d->ToHtml();
	return d->UtfNameCache;
}

static GHtmlElement *FindElement(GHtmlElement *e, HtmlTag TagId)
{
	if (e->TagId == TagId)
		return e;
		
	for (unsigned i = 0; i < e->Children.Length(); i++)
	{
		GHtmlElement *c = FindElement(e->Children[i], TagId);
		if (c)
			return c;
	}
	
	return NULL;
}

void GTextView4::OnAddStyle(const char *MimeType, const char *Styles)
{
	if (d->CreationCtx)
	{
		d->CreationCtx->StyleStore.Parse(Styles);
	}
}

bool GTextView4::Name(const char *s)
{
	d->Empty();
	d->OriginalText = s;
	
	GHtmlElement Root(NULL);

	if (!d->CreationCtx.Reset(new GTv4Priv::CreateContext(d)))
		return false;

	if (!d->GHtmlParser::Parse(&Root, s))
		return d->Error(_FL, "Failed to parse HTML.");

	GHtmlElement *Body = FindElement(&Root, TAG_BODY);
	if (!Body)
		Body = &Root;

	bool Status = d->FromHtml(Body, *d->CreationCtx);
	if (Status)
		SetCursor(0, false);
	
	d->DumpBlocks();
	
	return Status;
}

char16 *GTextView4::NameW()
{
	d->WideNameCache.Reset(LgiNewUtf8To16(Name()));
	return d->WideNameCache;
}

bool GTextView4::NameW(const char16 *s)
{
	GAutoString a(LgiNewUtf16To8(s));
	return Name(a);
}

char *GTextView4::GetSelection()
{
	if (HasSelection())
	{
	}

	return 0;
}

bool GTextView4::HasSelection()
{
	return d->Selection.Get() != NULL;
}

void GTextView4::SelectAll()
{
	Invalidate();
}

void GTextView4::UnSelectAll()
{
	bool Update = HasSelection();

	if (Update)
	{
		Invalidate();
	}
}

int GTextView4::GetLines()
{
	return 0;
}

void GTextView4::GetTextExtent(int &x, int &y)
{
}

void GTextView4::PositionAt(int &x, int &y, int Index)
{
}

int GTextView4::GetCursor(bool Cur)
{
	if (!d->Cursor)
		return -1;
		
	int CharPos = 0;
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		GTv4Priv::Block *b = d->Blocks[i];
		if (d->Cursor->Blk == b)
			return CharPos + d->Cursor->Offset;
		CharPos += b->Length();
	}
	
	LgiAssert(!"Cursor block not found.");
	return -1;
}

int GTextView4::IndexAt(int x, int y)
{
	return d->HitTest(x, y);
}

void GTextView4::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
	int Offset = -1;
	GTv4Priv::Block *Blk = d->GetBlockByIndex(i, &Offset);
	if (Blk)
	{
		GAutoPtr<GTv4Priv::BlockCursor> c(new GTv4Priv::BlockCursor(Blk, Offset));
		if (c)
		{
			c->Blk->GetPosFromIndex(&c->Pos, &c->Line, Offset);
			d->SetCursor(c, Select);
		}
	}
}

void GTextView4::SetBorder(int b)
{
}

bool GTextView4::Cut()
{
	return false;
}

bool GTextView4::Copy()
{
	bool Status = true;

	return Status;
}

bool GTextView4::Paste()
{
	GClipBoard Clip(this);
	
	return false;
}

bool GTextView4::ClearDirty(bool Ask, char *FileName)
{
	if (1 /*dirty*/)
	{
		int Answer = (Ask) ? LgiMsg(this,
									LgiLoadString(L_TEXTCTRL_ASK_SAVE, "Do you want to save your changes to this document?"),
									LgiLoadString(L_TEXTCTRL_SAVE, "Save"),
									MB_YESNOCANCEL) : IDYES;
		if (Answer == IDYES)
		{
			GFileSelect Select;
			Select.Parent(this);
			if (!FileName &&
				Select.Save())
			{
				FileName = Select.Name();
			}

			Save(FileName);
		}
		else if (Answer == IDCANCEL)
		{
			return false;
		}
	}

	return true;
}

bool GTextView4::Open(const char *Name, const char *CharSet)
{
	bool Status = false;
	GFile f;

	if (f.Open(Name, O_READ|O_SHARE))
	{
		size_t Bytes = (size_t)f.GetSize();
		SetCursor(0, false);
		
		char *c8 = new char[Bytes + 4];
		if (c8)
		{
			if (f.Read(c8, Bytes) == Bytes)
			{
				char *DataStart = c8;

				c8[Bytes] = 0;
				c8[Bytes+1] = 0;
				c8[Bytes+2] = 0;
				c8[Bytes+3] = 0;
				
				if ((uchar)c8[0] == 0xff && (uchar)c8[1] == 0xfe)
				{
					// utf-16
					if (!CharSet)
					{
						CharSet = "utf-16";
						DataStart += 2;
					}
				}
				
			}

			DeleteArray(c8);
		}
		else
		{
		}

		Invalidate();
	}

	return Status;
}

bool GTextView4::Save(const char *Name, const char *CharSet)
{
	GFile f;
	if (f.Open(Name, O_WRITE))
	{
		f.SetSize(0);
	}
	return false;
}

void GTextView4::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		GRect Before = GetClient();

	}
}

bool GTextView4::DoCase(bool Upper)
{
	return true;
}

int GTextView4::GetLine()
{
	int Idx = 0;
	return Idx + 1;
}

void GTextView4::SetLine(int i)
{
}

bool GTextView4::DoGoto()
{
	GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto line:"), "Text");
	if (Dlg.DoModal() == IDOK &&
		Dlg.Str)
	{
		SetLine(atoi(Dlg.Str));
	}

	return true;
}

GDocFindReplaceParams *GTextView4::CreateFindReplaceParams()
{
	return new GDocFindReplaceParams3;
}

void GTextView4::SetFindReplaceParams(GDocFindReplaceParams *Params)
{
	if (Params)
	{
	}
}

bool GTextView4::DoFindNext()
{
	return false;
}

bool
Text4_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	return true;
}

bool GTextView4::DoFind()
{
	char *u = 0;
	if (HasSelection())
	{
	}
	else
	{
	}

	GFindDlg Dlg(this, u, Text4_FindCallback, this);
	Dlg.DoModal();
	DeleteArray(u);
	
	Focus(true);

	return false;
}

bool GTextView4::DoReplace()
{
	return false;
}

void GTextView4::SelectWord(int From)
{
	Invalidate();
}

bool GTextView4::OnFind(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	return false;
}

bool GTextView4::OnReplace(char16 *Find, char16 *Replace, bool All, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	if (ValidStrW(Find))
	{
	}	
	
	return false;
}

bool GTextView4::OnMultiLineTab(bool In)
{
	return false;
}

void GTextView4::OnSetHidden(int Hidden)
{
}

void GTextView4::OnPosChange()
{
	static bool Processing = false;

	if (!Processing)
	{
		Processing = true;
		GLayout::OnPosChange();

		GRect c = GetClient();
		Processing = false;
	}
}

int GTextView4::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	for (char *s = Formats.First(); s; )
	{
		if (!_stricmp(s, "text/uri-list") ||
			!_stricmp(s, "text/html") ||
			!_stricmp(s, "UniformResourceLocatorW"))
		{
			s = Formats.Next();
		}
		else
		{
			// LgiTrace("Ignoring format '%s'\n", s);
			Formats.Delete(s);
			DeleteArray(s);
			s = Formats.Current();
		}
	}

	return Formats.Length() ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

int GTextView4::OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState)
{
	/* FIXME
	if (!_stricmp(Format, "text/uri-list") ||
		!_stricmp(Format, "text/html") ||
		!_stricmp(Format, "UniformResourceLocatorW"))
	{
		if (Data->IsBinary())
		{
			char16 *e = (char16*) ((char*)Data->Value.Binary.Data + Data->Value.Binary.Length);
			char16 *s = (char16*)Data->Value.Binary.Data;
			int len = 0;
			while (s < e && s[len])
			{
				len++;
			}
			// Insert(Cursor, s, len);
			Invalidate();
			return DROPEFFECT_COPY;
		}
	}
	*/

	return DROPEFFECT_NONE;
}

void GTextView4::OnCreate()
{
	SetWindow(this);
	DropTarget(true);

	if (Focus())
		SetPulse(1500);
}

void GTextView4::OnEscape(GKey &K)
{
}

bool GTextView4::OnMouseWheel(double l)
{
	if (VScroll)
	{
		int NewPos = (int)VScroll->Value() + (int) l;
		NewPos = limit(NewPos, 0, GetLines());
		VScroll->Value(NewPos);
		Invalidate();
	}
	
	return true;
}

void GTextView4::OnFocus(bool f)
{
	Invalidate();
	SetPulse(f ? 500 : -1);
}

int GTextView4::HitText(int x, int y)
{
	return d->HitTest(x, y);
}

void GTextView4::Undo()
{
}

void GTextView4::Redo()
{
}

void GTextView4::DoContextMenu(GMouse &m)
{
	GMenuItem *i;
	GSubMenu RClick;
	GAutoString ClipText;
	{
		GClipBoard Clip(this);
		ClipText.Reset(NewStr(Clip.Text()));
	}

	#if LUIS_DEBUG
	RClick.AppendItem("Dump Layout", IDM_DUMP, true);
	RClick.AppendSeparator();
	#endif

	/*
	GStyle *s = HitStyle(HitText(m.x, m.y));
	if (s)
	{
		if (s->OnMenu(&RClick))
		{
			RClick.AppendSeparator();
		}
	}
	*/

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_CUT, "Cut"), IDM_CUT, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_PASTE, "Paste"), IDM_PASTE, ClipText != 0);
	RClick.AppendSeparator();

	#if 0
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_UNDO, "Undo"), IDM_UNDO, false /* UndoQue.CanUndo() */);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_REDO, "Redo"), IDM_REDO, false /* UndoQue.CanRedo() */);
	RClick.AppendSeparator();

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_FIXED, "Fixed Width Font"), IDM_FIXED, true);
	if (i) i->Checked(GetFixedWidthFont());
	#endif

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_AUTO_INDENT, "Auto Indent"), IDM_AUTO_INDENT, true);
	if (i) i->Checked(AutoIndent);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_SHOW_WHITESPACE, "Show Whitespace"), IDM_SHOW_WHITE, true);
	if (i) i->Checked(ShowWhiteSpace);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_HARD_TABS, "Hard Tabs"), IDM_HARD_TABS, true);
	if (i) i->Checked(HardTabs);
	
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_INDENT_SIZE, "Indent Size"), IDM_INDENT_SIZE, true);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_TAB_SIZE, "Tab Size"), IDM_TAB_SIZE, true);
	RClick.AppendItem("Copy Original", IDM_COPY_ORIGINAL, d->OriginalText.Get() != NULL);

	if (Environment)
		Environment->AppendItems(&RClick);

	int Id = 0;
	m.ToScreen();
	switch (Id = RClick.Float(this, m.x, m.y))
	{
		#if LUIS_DEBUG
		case IDM_DUMP:
		{
			int n=0;
			for (GTextLine *l=Line.First(); l; l=Line.Next(), n++)
			{
				LgiTrace("[%i] %i,%i (%s)\n", n, l->Start, l->Len, l->r.Describe());

				char *s = LgiNewUtf16To8(Text + l->Start, l->Len * sizeof(char16));
				if (s)
				{
					LgiTrace("%s\n", s);
					DeleteArray(s);
				}
			}
			break;
		}
		#endif
		case IDM_FIXED:
		{
			SetFixedWidthFont(!GetFixedWidthFont());							
			SendNotify(GNotifyFixedWidthChanged);
			break;
		}
		case IDM_CUT:
		{
			Cut();
			break;
		}
		case IDM_COPY:
		{
			Copy();
			break;
		}
		case IDM_PASTE:
		{
			Paste();
			break;
		}
		case IDM_UNDO:
		{
			Undo();
			break;
		}
		case IDM_REDO:
		{
			Redo();
			break;
		}
		case IDM_AUTO_INDENT:
		{
			AutoIndent = !AutoIndent;
			break;
		}
		case IDM_SHOW_WHITE:
		{
			ShowWhiteSpace = !ShowWhiteSpace;
			Invalidate();
			break;
		}
		case IDM_HARD_TABS:
		{
			HardTabs = !HardTabs;
			break;
		}
		case IDM_INDENT_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", IndentSize);
			GInput i(this, s, "Indent Size:", "Text");
			if (i.DoModal())
			{
				IndentSize = atoi(i.Str);
			}
			break;
		}
		case IDM_TAB_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", TabSize);
			GInput i(this, s, "Tab Size:", "Text");
			if (i.DoModal())
			{
				SetTabSize(atoi(i.Str));
			}
			break;
		}
		case IDM_COPY_ORIGINAL:
		{
			GClipBoard c(this);
			c.Text(d->OriginalText);
			break;
		}
		default:
		{
			/*
			if (s)
			{
				s->OnMenuClick(Id);
			}
			*/

			if (Environment)
			{
				Environment->OnMenu(this, Id, 0);
			}
			break;
		}
	}
}

void GTextView4::OnMouseClick(GMouse &m)
{
	bool Processed = false;

	if (m.Down())
	{
		if (m.IsContextMenu())
		{
			DoContextMenu(m);
			return;
		}
		else
		{
			Focus(true);

			int Hit = HitText(m.x, m.y);
			d->WordSelectMode = !Processed && m.Double();

			if (Hit >= 0)
			{
				SetCursor(Hit, m.Shift());
				if (d->WordSelectMode)
					SelectWord(Hit);
			}
		}
	}

	if (!Processed)
	{
		Capture(m.Down());
	}
}

int GTextView4::OnHitTest(int x, int y)
{
	#ifdef WIN32
	if (GetClient().Overlap(x, y))
	{
		return HTCLIENT;
	}
	#endif
	return GView::OnHitTest(x, y);
}

void GTextView4::OnMouseMove(GMouse &m)
{
	int Hit = d->HitTest(m.x, m.y);
	if (IsCapturing())
	{
		if (!d->WordSelectMode)
		{
			SetCursor(Hit, m.Left());
		}
		else
		{
			/*
			int Min = Hit < d->WordSelectMode ? Hit : d->WordSelectMode;
			int Max = Hit > d->WordSelectMode ? Hit : d->WordSelectMode;

			for (SelStart = Min; SelStart > 0; SelStart--)
			{
				if (strchr(SelectWordDelim, Text[SelStart]))
				{
					SelStart++;
					break;
				}
			}

			for (SelEnd = Max; SelEnd < Size; SelEnd++)
			{
				if (strchr(SelectWordDelim, Text[SelEnd]))
				{
					break;
				}
			}

			Cursor = SelEnd;
			Invalidate();
			*/
		}
	}

	#ifdef WIN32
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		/*
		GStyle *s = HitStyle(Hit);
		TCHAR *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
		*/
	}
	#endif
}

bool GTextView4::OnKey(GKey &k)
{
	if (k.Down())
	{
		// Blink = true;
	}

	// k.Trace("GTextView4::OnKey");

	if (k.IsContextMenu())
	{
		GMouse m;
		/*
		m.x = CursorPos.x1;
		m.y = CursorPos.y1 + (CursorPos.Y() >> 1);
		m.Target = this;
		*/
		DoContextMenu(m);
	}
	else if (k.IsChar)
	{
		switch (k.c16)
		{
			default:
			{
				// process single char input
				if
				(
					!GetReadOnly()
					&&
					(
						(k.c16 >= ' ' || k.c16 == VK_TAB)
						&&
						k.c16 != 127
					)
				)
				{
					if (k.Down())
					{
						// letter/number etc
						/*
						if (SelStart >= 0)
						{
							bool MultiLine = false;
							if (k.c16 == VK_TAB)
							{
								int Min = min(SelStart, SelEnd), Max = max(SelStart, SelEnd);
								for (int i=Min; i<Max; i++)
								{
									if (Text[i] == '\n')
									{
										MultiLine = true;
									}
								}
							}
							if (MultiLine)
							{
								if (OnMultiLineTab(k.Shift()))
								{
									return true;
								}
							}
							else
							{
								DeleteSelection();
							}
						}
						
						GTextLine *l = GetTextLine(Cursor);
						int Len = (l) ? l->Len : 0;
						
						if (l && k.c16 == VK_TAB && (!HardTabs || IndentSize != TabSize))
						{
							int x = GetColumn();							
							int Add = IndentSize - (x % IndentSize);
							
							if (HardTabs && ((x + Add) % TabSize) == 0)
							{
								int Rx = x;
								int Remove;
								for (Remove = Cursor; Text[Remove - 1] == ' ' && Rx % TabSize != 0; Remove--, Rx--);
								int Chars = Cursor - Remove;
								Delete(Remove, Chars);
								Insert(Remove, &k.c16, 1);
								Cursor = Remove + 1;
								
								Invalidate();
							}
							else
							{							
								char16 *Sp = new char16[Add];
								if (Sp)
								{
									for (int n=0; n<Add; n++) Sp[n] = ' ';
									if (Insert(Cursor, Sp, Add))
									{
										l = GetTextLine(Cursor);
										int NewLen = (l) ? l->Len : 0;
										SetCursor(Cursor + Add, false, Len != NewLen - 1);
									}
									DeleteArray(Sp);
								}
							}
						}
						else
						{
							char16 In = k.GetChar();

							if (In == '\t' &&
								k.Shift() &&
								Cursor > 0)
							{
								l = GetTextLine(Cursor);
								if (Cursor > l->Start)
								{
									if (Text[Cursor-1] == '\t')
									{
										Delete(Cursor - 1, 1);
										SetCursor(Cursor, false, false);
									}
									else if (Text[Cursor-1] == ' ')
									{
										int Start = Cursor - 1;
										while (Start >= l->Start && strchr(" \t", Text[Start-1]))
											Start--;
										int Depth = SpaceDepth(Text + Start, Text + Cursor);
										int NewDepth = Depth - (Depth % IndentSize);
										if (NewDepth == Depth && NewDepth > 0)
											NewDepth -= IndentSize;
										int Use = 0;
										while (SpaceDepth(Text + Start, Text + Start + Use + 1) < NewDepth)
											Use++;
										Delete(Start + Use, Cursor - Start - Use);
										SetCursor(Start + Use, false, false);
									}
								}
								
							}
							else if (In && Insert(Cursor, &In, 1))
							{
								l = GetTextLine(Cursor);
								int NewLen = (l) ? l->Len : 0;
								SetCursor(Cursor + 1, false, Len != NewLen - 1);
							}
						}
						*/
					}
					return true;
				}
				break;
			}
			case VK_RETURN:
			{
				if (GetReadOnly())
					break;

				if (k.Down() && k.IsChar)
				{
					OnEnter(k);
				}
				return true;
				break;
			}
			case VK_BACKSPACE:
			{
				if (GetReadOnly())
					break;

				if (k.Ctrl())
				{
				    // Ctrl+H
				    int asd=0;
				}
				else if (k.Down())
				{
					/*
					if (SelStart >= 0)
					{
						// delete selection
						DeleteSelection();
					}
					else
					{						
						char Del = Cursor > 0 ? Text[Cursor-1] : 0;
						
						if (Del == ' ' && (!HardTabs || IndentSize != TabSize))
						{
							// Delete soft tab
							int x = GetColumn();
							int Max = x % IndentSize;
							if (Max == 0) Max = IndentSize;
							int i;
							for (i=Cursor-1; i>=0; i--)
							{
								if (Max-- <= 0 || Text[i] != ' ')
								{
									i++;
									break;
								}
							}
							
							if (i < 0) i = 0;
							
							if (i < Cursor - 1)
							{
								int Del = Cursor - i;								
								Delete(i, Del);
								// SetCursor(i, false, false);
								// Invalidate();
								break;
							}
						}
						else if (Del == '\t' && HardTabs && IndentSize != TabSize)
						{
							int x = GetColumn();
							Delete(--Cursor, 1);
							
							for (int c=GetColumn(); c<x-IndentSize; c=GetColumn())
							{
								int Add = IndentSize + (c % IndentSize);
								if (Add)
								{
									char16 *s = new char16[Add];
									if (s)
									{
										for (int n=0; n<Add; n++) s[n] = ' ';
										Insert(Cursor, s, Add);
										Cursor += Add;
										DeleteArray(s);
									}
								}
								else break;
							}
							
							Invalidate();
							break;
						}


						if (Cursor > 0)
						{
							Delete(Cursor - 1, 1);
						}
					}
					*/
				}
				return true;
				break;
			}
		}
	}
	else // not a char
	{
		switch (k.vkey)
		{
			case VK_TAB:
				return true;
			case VK_RETURN:
			{
				return !GetReadOnly();
			}
			case VK_BACKSPACE:
			{
				if (!GetReadOnly())
				{
					if (k.Alt())
					{
						if (k.Down())
						{
							if (k.Ctrl())
							{
								Redo();
							}
							else
							{
								Undo();
							}
						}
					}
					else if (k.Ctrl())
					{
						if (k.Down())
						{
							/*
							int Start = Cursor;
							while (IsWhiteSpace(Text[Cursor-1]) && Cursor > 0)
								Cursor--;

							while (!IsWhiteSpace(Text[Cursor-1]) && Cursor > 0)
								Cursor--;

							Delete(Cursor, Start - Cursor);
							Invalidate();
							*/
						}
					}

					return true;
				}
				break;
			}
			case VK_F3:
			{
				if (k.Down())
				{
					DoFindNext();
				}
				return true;
				break;
			}
			case VK_LEFT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);
						d->SetCursor(d->CursorFirst() ? d->Cursor : d->Selection);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_StartOfLine;
						else
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GTv4Priv::SkLeftWord : GTv4Priv::SkLeftChar,
								k.Shift());
					}
				}
				return true;
				break;
			}
			case VK_RIGHT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);
						d->SetCursor(d->CursorFirst() ? d->Selection : d->Cursor);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_EndOfLine;
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GTv4Priv::SkRightWord : GTv4Priv::SkRightChar,
								k.Shift());
					}
				}
				return true;
				break;
			}
			case VK_UP:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageUp;
					#endif

					d->Seek(d->Cursor,
							GTv4Priv::SkUpLine,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_DOWN:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageDown;
					#endif

					d->Seek(d->Cursor,
							GTv4Priv::SkDownLine,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_END:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_EndOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GTv4Priv::SkDocEnd : GTv4Priv::SkLineEnd,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_HOME:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_StartOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GTv4Priv::SkDocStart : GTv4Priv::SkLineStart,
							k.Shift());

					/*
					char16 *Line = Text + l->Start;
					char16 *s;
					char16 SpTab[] = {' ', '\t', 0};
					for (s = Line; (SubtractPtr(s,Line) < l->Len) && StrchrW(SpTab, *s); s++);
					int Whitespace = SubtractPtr(s, Line);

					if (l->Start + Whitespace == Cursor)
					{
						SetCursor(l->Start, k.Shift());
					}
					else
					{
						SetCursor(l->Start + Whitespace, k.Shift());
					}
					*/
				}
				return true;
				break;
			}
			case VK_PAGEUP:
			{
				#ifdef MAC
				GTextView4_PageUp:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GTv4Priv::SkUpPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_PAGEDOWN:
			{
				#ifdef MAC
				GTextView4_PageDown:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GTv4Priv::SkDownPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_INSERT:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						Copy();
					}
					else if (k.Shift())
					{
						if (!GetReadOnly())
						{
							Paste();
						}
					}
				}
				return true;
				break;
			}
			case VK_DELETE:
			{
				if (!GetReadOnly())
				{
					if (k.Down())
					{
						/*
						if (SelStart >= 0)
						{
							if (k.Shift())
							{
								Cut();
							}
							else
							{
								DeleteSelection();
							}
						}
						else if (Cursor < Size &&
								Delete(Cursor, 1))
						{
							Invalidate();
						}
						*/
					}
					return true;
				}
				break;
			}
			default:
			{
				if (k.c16 == 17) break;

				if (k.Modifier() &&
					!k.Alt())
				{
					switch (k.GetChar())
					{
						case 0xbd: // Ctrl+'-'
						{
							/*
							if (k.Down() &&
								Font->PointSize() > 1)
							{
								Font->PointSize(Font->PointSize() - 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 0xbb: // Ctrl+'+'
						{
							/*
							if (k.Down() &&
								Font->PointSize() < 100)
							{
								Font->PointSize(Font->PointSize() + 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 'a':
						case 'A':
						{
							if (k.Down())
							{
								// select all
								/*
								SelStart = 0;
								SelEnd = Size;
								Invalidate();
								*/
							}
							return true;
							break;
						}
						case 'y':
						case 'Y':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Redo();
								}
								return true;
							}
							break;
						}
						case 'z':
						case 'Z':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									if (k.Shift())
									{
										Redo();
									}
									else
									{
										Undo();
									}
								}
								return true;
							}
							break;
						}
						case 'x':
						case 'X':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Cut();
								}
								return true;
							}
							break;
						}
						case 'c':
						case 'C':
						{
							if (k.Shift())
								return false;
							
							if (k.Down())
								Copy();
							return true;
							break;
						}
						case 'v':
						case 'V':
						{
							if (!k.Shift() &&
								!GetReadOnly())
							{
								if (k.Down())
								{
									Paste();
								}
								return true;
							}
							break;
						}
						case 'f':
						{
							if (k.Down())
							{
								DoFind();
							}
							return true;
							break;
						}
						case 'g':
						case 'G':
						{
							if (k.Down())
							{
								DoGoto();
							}
							return true;
							break;
						}
						case 'h':
						case 'H':
						{
							if (k.Down())
							{
								DoReplace();
							}
							return true;
							break;
						}
						case 'u':
						case 'U':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									DoCase(k.Shift());
								}
								return true;
							}
							break;
						}
						case VK_RETURN:
						{
							if (!GetReadOnly() && !k.Shift())
							{
								if (k.Down())
								{
									OnEnter(k);
								}
								return true;
							}
							break;
						}
					}
				}
				break;
			}
		}
	}
	
	return false;
}

void GTextView4::OnEnter(GKey &k)
{
	// enter
	/*
	if (SelStart >= 0)
	{
		DeleteSelection();
	}

	char16 InsertStr[256] = {'\n', 0};

	GTextLine *CurLine = GetTextLine(Cursor);
	if (CurLine && AutoIndent)
	{
		int WsLen = 0;
		for (;	WsLen < CurLine->Len &&
				WsLen < (Cursor - CurLine->Start) &&
				strchr(" \t", Text[CurLine->Start + WsLen]); WsLen++);
		if (WsLen > 0)
		{
			memcpy(InsertStr+1, Text+CurLine->Start, WsLen * sizeof(char16));
			InsertStr[WsLen+1] = 0;
		}
	}

	if (Insert(Cursor, InsertStr, StrlenW(InsertStr)))
	{
		SetCursor(Cursor + StrlenW(InsertStr), false, true);
	}
	*/
}

void GTextView4::OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour)
{
	pDC->Colour(colour);
	pDC->Rectangle(&r);

}

void GTextView4::OnPaint(GSurface *pDC)
{
	#if 1
	pDC->Colour(GColour(255, 222, 255));
	pDC->Rectangle();
	#endif
	
	GRect r = GetClient();
	GCssTools ct(d, d->Font);
	r = ct.PaintBorderAndPadding(pDC, r);

	d->Layout(r);
	d->Paint(pDC);
}

GMessage::Result GTextView4::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CUT:
		{
			Cut();
			break;
		}
		case M_COPY:
		{
			printf("M_COPY received.\n");
			Copy();
			break;
		}
		case M_PASTE:
		{
			Paste();
			break;
		}
		#if defined WIN32
		case WM_GETTEXTLENGTH:
		{
			return 0 /*Size*/;
		}
		case WM_GETTEXT:
		{
			int Chars = (int)MsgA(Msg);
			char *Out = (char*)MsgB(Msg);
			if (Out)
			{
				char *In = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), NameW(), LGI_WideCharset, Chars);
				if (In)
				{
					int Len = (int)strlen(In);
					memcpy(Out, In, Len);
					DeleteArray(In);
					return Len;
				}
			}
			return 0;
		}

		/* This is broken... the IME returns garbage in the buffer. :(
		case WM_IME_COMPOSITION:
		{
			if (Msg->b & GCS_RESULTSTR) 
			{
				HIMC hIMC = ImmGetContext(Handle());
				if (hIMC)
				{
					int Size = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
					char *Buf = new char[Size];
					if (Buf)
					{
						ImmGetCompositionString(hIMC, GCS_RESULTSTR, Buf, Size);

						char16 *Utf = (char16*)LgiNewConvertCp(LGI_WideCharset, Buf, LgiAnsiToLgiCp(), Size);
						if (Utf)
						{
							Insert(Cursor, Utf, StrlenW(Utf));
							DeleteArray(Utf);
						}

						DeleteArray(Buf);
					}

					ImmReleaseContext(Handle(), hIMC);
				}
				return 0;
			}
			break;
		}
		*/
		#endif
	}

	return GLayout::OnEvent(Msg);
}

int GTextView4::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_VSCROLL && VScroll)
	{
		Invalidate();
	}

	return 0;
}

void GTextView4::OnPulse()
{
	if (!ReadOnly)
	{
		/*
		Blink = !Blink;

		GRect p = CursorPos;
		p.Offset(-ScrollX, 0);
		Invalidate(&p);
		*/
	}
}

void GTextView4::OnUrl(char *Url)
{
	if (Environment)
	{
		Environment->OnNavigate(this, Url);
	}
}

bool GTextView4::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = 32;
	Inf.Width.Max = -1;

	// Inf.Height.Min = (Font ? Font->GetHeight() : 18) + 4;
	Inf.Height.Max = -1;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
class GTextView4_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "GTextView4") == 0)
		{
			return new GTextView4(-1, 0, 0, 2000, 2000);
		}

		return 0;
	}
} TextView4_Factory;
