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

#include "GHtmlCommon.h"
#include "GHtmlParser.h"

#define FixedToInt(fixed)			((fixed)>>GDisplayString::FShift)
#define IntToFixed(val)				((val)<<GDisplayString::FShift)
#define DefaultCharset              "utf-8"
#define SubtractPtr(a, b)			((a) - (b))

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

public:
	GText4Elem(GHtmlElement *parent) : GHtmlElement(parent)
	{
	}

	bool Get(const char *attr, const char *&val)
	{
		if (attr && !stricmp(attr, "style") && Style)
		{
			val = Style;
			return true;
		}
		return false;
	}
	
	void Set(const char *attr, const char *val)
	{
		if (attr && !stricmp(attr, "style"))
			Style = val;
	}
	
	void SetStyle()
	{
		int asd = 0;
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
		}
		
		return ns;
	}
};

class GFontCache
{
	GArray<GFont*> Fonts;
	
public:
	GFontCache()
	{
	}
	
	~GFontCache()
	{
		Fonts.DeleteObjects();
	}
	
	GFont *AddFont(	const char *Face,
					int PtSize,
					GCss::FontWeightType Weight)
	{
		// Matching existing fonts...
		for (unsigned i=0; i<Fonts.Length(); i++)
		{
			GFont *f = Fonts[i];
			if
			(
				!stricmp(f->Face(), Face) &&
				f->PointSize() == PtSize &&
				f->Bold() == (Weight == GCss::FontWeightBold)
			)
				return f;
		}
		
		// No matching font... create a new one
		GFont *f = new GFont;
		if (f)
		{
			f->Bold(Weight == GCss::FontWeightBold);
			if (!f->Create(Face, PtSize))
			{
				LgiAssert(0);
				DeleteObj(f);
				return NULL;
			}
			
			Fonts.Add(f);
		}
		
		return f;
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
	GAutoPtr<GFont> Font;

	bool Error(const char *file, int line, const char *fmt, ...)
	{
		LgiAssert(0);
		return false;
	}

	GFont *GetFont(GCss *Style)
	{
		GFont *Default = View->GetFont();
		if (!Style)
			return Default;
		
		// Look through all the existing fonts for a match:
		GCss::StringsDef Face = Style->FontFamily();
		GCss::Len Size = Style->FontSize();
		GCss::FontWeightType Weight = Style->FontWeight();
		
		return AddFont(	Face.Length() ? Face[0] : Default->Face(),
						Size.Type == GCss::LenPt ? (int)Size.Value : Default->PointSize(),
						Weight);
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
		GCss *Style; // owned by the CSS cache
	
	public:
		ColourPair Colours;
		GColour Fore, Back;
		
		Text(const char16 *t = NULL, int Chars = -1)
		{
			Style = NULL;
			if (t)
				Add((char16*)t, Chars >= 0 ? Chars : StrlenW(t));
		}
		
		GCss *GetStyle()
		{
			return Style;
		}
				
		void SetStyle(GCss *s)
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
	
	enum SelectModeType
	{
		Unselected = 0,
		Selected = 1,
	};

	struct PaintContext
	{
		GSurface *pDC;
		SelectModeType Type;
		ColourPair Colours[2];
		
		PaintContext()
		{
			pDC = NULL;
			Type = Unselected;
		}
		
		GColour &Fore()
		{
			return Colours[Type].Fore;
		}

		GColour &Back()
		{
			return Colours[Type].Back;
		}
	};

	class Block // is like a DIV in HTML, it's as wide as the page and
				// always starts and ends on a whole line.
	{
	public:
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
		virtual bool OnLayout(Flow &f) = 0;
		virtual void OnPaint(PaintContext &Ctx) = 0;
		virtual bool OnKey(GKey &k) = 0;
	};

	struct BlockCursor
	{
		Block *Blk;
		int Offset;
		GRect Pos;
		
		BlockCursor(Block *b, int off)
		{
			Blk = NULL;
			Offset = -1;
			Pos.ZOff(-1, -1);

			if (b)
				Set(b, off);
		}
		
		~BlockCursor()
		{
			Set(NULL, 0);
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
		
		TextLine(GRect &BlockPos)
		{
			PosOff.ZOff(BlockPos.X()-1, 0);
			PosOff.Offset(0, BlockPos.Y());
		}
		
		/// This runs after the layout line has been filled with display strings.
		/// It measures the line and works out the right offsets for each strings
		/// so that their baselines all match up correctly.
		void LayoutOffsets()
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
	
	struct TextBlock : public Block
	{
		GArray<Text*> Txt;
		GArray<TextLine*> Layout;
		
		bool LayoutDirty;
		int Len; // chars in the whole block (sum of all Text lengths)
		GRect Pos; // position in document co-ordinates
		
		TextBlock()
		{
			LayoutDirty = false;
			Len = 0;
			Pos.ZOff(-1, -1);
		}
		
		~TextBlock()
		{
			Txt.DeleteObjects();
		}

		int Length()
		{
			return Len;
		}

		void OnPaint(PaintContext &Ctx)
		{
			for (unsigned i=0; i<Layout.Length(); i++)
			{
				TextLine *Line = Layout[i];

				GRect LinePos = Line->PosOff;
				LinePos.Offset(Pos.x1, Pos.y1);
				if (Line->PosOff.X() < Pos.X())
				{
					Ctx.pDC->Colour(Ctx.Back());
					Ctx.pDC->Rectangle(LinePos.x2, LinePos.y1, Pos.x2, LinePos.y2);
				}

				int FixX = IntToFixed(LinePos.x1);
				int CurY = LinePos.y1;
				GFont *Fnt = NULL;

				for (unsigned n=0; n<Line->Strs.Length(); n++)
				{
					DisplayStr *Ds = Line->Strs[n];
					GFont *f = Ds->GetFont();
					if (f != Fnt)
					{
						f->Transparent(false);
						Fnt = f;
					}

					ColourPair &Cols = Ds->Src->Colours;
					f->Colour(	Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
								Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
					
					Ds->FDraw(Ctx.pDC, FixX, IntToFixed(CurY + Ds->OffsetY));
					
					// If the current text part doesn't cover the full line height we have to
					// fill in the rest here...
					int CurX = FixedToInt(FixX);
					if (Ds->OffsetY > 0)
					{
						Ctx.pDC->Colour(Ctx.Back());
						Ctx.pDC->Rectangle(CurX, CurY, CurX+Ds->X(), CurY+Ds->OffsetY-1);
					}
					int DsY2 = Ds->OffsetY + Ds->Y();
					if (DsY2 < Pos.Y())
					{
						Ctx.pDC->Colour(Ctx.Back());
						Ctx.pDC->Rectangle(CurX, CurY+DsY2, CurX+Ds->X(), Pos.y2);
					}
					
					FixX += Ds->FX();
				}
			}
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
			
			Pos.x1 = flow.Left;
			Pos.y1 = flow.CurY;
			Pos.x2 = flow.Right;
			Pos.y2 = flow.CurY-1; // Start with a 0px height.
			
			int FixX = 0; // Current x offset (fixed point) on the current line
			GAutoPtr<TextLine> CurLine(new TextLine(Pos));
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
						CurLine->PosOff.x2 = FixedToInt(FixX);
						FixX = 0;
						CurLine->LayoutOffsets();
						Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
						Layout.Add(CurLine.Release());
						
						CurLine.Reset(new TextLine(Pos));
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
					
					if (!Ds)
						break;
					
					CurLine->PosOff.x2 = FixedToInt(FixX);
					CurLine->Strs.Add(Ds.Release());
					Off += Chars;
				}
			}
			
			if (CurLine)
			{
				CurLine->LayoutOffsets();
				Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
				Layout.Add(CurLine.Release());
			}
			
			flow.CurY = Pos.y2 + 1;
			
			return true;
		}
		
		bool OnKey(GKey &k)
		{
			return false;
		}
		
		Text *GetTextAt(int Offset)
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
		
		bool AddText(GNamedStyle *Style, char16 *Str, int Chars = -1)
		{
			if (!Str)
				return false;
			if (Chars < 0)
				Chars = StrlenW(Str);
			bool StyleDiff = (Style != NULL) ^ ((Txt.Length() ? Txt.Last()->GetStyle() : NULL) != NULL);
			Text *t = !StyleDiff && Txt.Length() ? Txt.Last() : NULL;
			if (t)
			{
				t->Add(Str, Chars);
				Len += Chars;
			}
			else if (t = new Text(Str, Chars))
			{
				Len += t->Length();
				Txt.Add(t);
			}
			else return false;

			t->SetStyle(Style);
			return true;
		}
	};
	
	struct ImageBlock : public Block
	{
		GString Src;
		GAutoPtr<GSurface> Img;
		
		int Length()
		{
			return 2; // one 'image' and a virtual new line
		}

		bool OnLayout(Flow &f)
		{
			return true;
		}
		
		bool OnKey(GKey &k)
		{
			return false;
		}
		
		void OnPaint(PaintContext &Ctx)
		{
		}
	};
	
	GArray<Block*> Blocks;

	GTv4Priv(GTextView4 *view) : GHtmlParser(view)
	{
		View = view;
	}
	
	~GTv4Priv()
	{
		Empty();
	}
	
	void Empty()
	{
		Blocks.DeleteObjects();
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
	}
	
	void Paint(GSurface *pDC)
	{
		PaintContext Ctx;
		Ctx.pDC = pDC;
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
		
		CreateContext()
		{
			Tb = NULL;
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
			
			Tb->AddText(Style, &Buf[0], Used);
		}
	};

	bool CreateFromHtml(GHtmlElement *e, CreateContext &ctx)
	{
		for (unsigned i = 0; i < e->Children.Length(); i++)
		{
			GHtmlElement *c = e->Children[i];
			GAutoPtr<GCss> Style;

			switch (c->TagId)
			{
				case TAG_B:
				{
					if (Style.Reset(new GCss))
						Style->FontWeight(GCss::FontWeightBold);
					break;
				}
				case TAG_BR:
				{
					if (!ctx.Tb)
						Blocks.Add(ctx.Tb = new TextBlock);
					if (ctx.Tb)
						ctx.Tb->AddText(NULL, L"\n");
					break;
				}
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
				default:
				{
					break;
				}
			}
			
			const char *Css;
			if (c->Get("style", Css))
			{
				if (!Style)
					Style.Reset(new GCss);
				if (Style)
					Style->Parse(Css);
			}

			if (c->GetText())
			{
				if (!ctx.Tb)
					Blocks.Add(ctx.Tb = new TextBlock);
				ctx.AddText(AddStyleToCache(Style), c->GetText());
			}
			
			if (!CreateFromHtml(c, ctx))
				return false;
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

	#ifdef _DEBUG
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
	return NULL;
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

bool GTextView4::Name(const char *s)
{
	GHtmlElement Root(NULL);
	if (!d->GHtmlParser::Parse(&Root, s))
		return d->Error(_FL, "Failed to parse HTML.");
	
	GHtmlElement *Body = FindElement(&Root, TAG_BODY);
	if (!Body)
		Body = &Root;

	GTv4Priv::CreateContext Ctx;
	return d->CreateFromHtml(Body, Ctx);
}

char16 *GTextView4::NameW()
{
	return NULL;
}

bool GTextView4::NameW(const char16 *s)
{
	return false;
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
	return false;
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
	return 0;
}

int GTextView4::IndexAt(int x, int y)
{
	return 0;
}

void GTextView4::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
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

static bool
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

int GTextView4::OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState)
{
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
	return 0;
}

void GTextView4::Undo()
{
}

void GTextView4::Redo()
{
}

void GTextView4::DoContextMenu(GMouse &m)
{
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

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_UNDO, "Undo"), IDM_UNDO, false /* UndoQue.CanUndo() */);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_REDO, "Redo"), IDM_REDO, false /* UndoQue.CanRedo() */);
	RClick.AppendSeparator();

	GMenuItem *i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_FIXED, "Fixed Width Font"), IDM_FIXED, true);
	if (i) i->Checked(GetFixedWidthFont());

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_AUTO_INDENT, "Auto Indent"), IDM_AUTO_INDENT, true);
	if (i) i->Checked(AutoIndent);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_SHOW_WHITESPACE, "Show Whitespace"), IDM_SHOW_WHITE, true);
	if (i) i->Checked(ShowWhiteSpace);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_HARD_TABS, "Hard Tabs"), IDM_HARD_TABS, true);
	if (i) i->Checked(HardTabs);
	
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_INDENT_SIZE, "Indent Size"), IDM_INDENT_SIZE, true);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_TAB_SIZE, "Tab Size"), IDM_TAB_SIZE, true);

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

	/*
	m.x += ScrollX;

	if (m.Down())
	{
		if (!m.IsContextMenu())
		{
			Focus(true);

			int Hit = HitText(m.x, m.y);
			if (Hit >= 0)
			{
				SetCursor(Hit, m.Shift());

				GStyle *s = HitStyle(Hit);
				if (s)
				{
					Processed = s->OnMouseClick(&m);
				}
			}

			if (!Processed && m.Double())
			{
				d->WordSelectMode = Cursor;
				SelectWord(Cursor);
			}
			else
			{
				d->WordSelectMode = -1;
			}
		}
		else
		{
			DoContextMenu(m);
			return;
		}
	}
	*/

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
	/*
	m.x += ScrollX;

	int Hit = HitText(m.x, m.y);
	if (IsCapturing())
	{
		if (d->WordSelectMode < 0)
		{
			SetCursor(Hit, m.Left());
		}
		else
		{
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
		}
	}

	#ifdef WIN32
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		GStyle *s = HitStyle(Hit);
		TCHAR *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
	}
	#endif
	*/
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
					/*
					if (SelStart >= 0 &&
						!k.Shift())
					{
						SetCursor(min(SelStart, SelEnd), false);
					}
					else if (Cursor > 0)
					{
						int n = Cursor;

						#ifdef MAC
						if (k.System())
						{
							goto Jump_StartOfLine;
						}
						else
						#endif
						if (k.Ctrl())
						{
							// word move/select
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
						}
						else
						{
							// single char
							n--;
						}

						SetCursor(n, k.Shift());
					}
					*/
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
					/*
					if (SelStart >= 0 &&
						!k.Shift())
					{
						SetCursor(max(SelStart, SelEnd), false);
					}
					else if (Cursor < Size)
					{
						int n = Cursor;

						#ifdef MAC
						if (k.System())
						{
							goto Jump_EndOfLine;
						}
						else
						#endif
						if (k.Ctrl())
						{
							// word move/select
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
						}
						else
						{
							// single char
							n++;
						}

						SetCursor(n, k.Shift());
					}
					*/
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
					/*
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageUp;
					#endif
					
					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						GTextLine *Prev = Line.Prev();
						if (Prev)
						{
							GDisplayString CurLine(Font, Text + l->Start, Cursor-l->Start);
							int ScreenX = CurLine.X();

							GDisplayString PrevLine(Font, Text + Prev->Start, Prev->Len);
							int CharX = PrevLine.CharAt(ScreenX);

							SetCursor(Prev->Start + min(CharX, Prev->Len), k.Shift());
						}
					}
					*/
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
					/*
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageDown;
					#endif

					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						GTextLine *Next = Line.Next();
						if (Next)
						{
							GDisplayString CurLine(Font, Text + l->Start, Cursor-l->Start);
							int ScreenX = CurLine.X();
							
							GDisplayString NextLine(Font, Text + Next->Start, Next->Len);
							int CharX = NextLine.CharAt(ScreenX);

							SetCursor(Next->Start + min(CharX, Next->Len), k.Shift());
						}
					}
					*/
				}
				return true;
				break;
			}
			case VK_END:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						// SetCursor(Size, k.Shift());
					}
					else
					{
						/*
						#ifdef MAC
						Jump_EndOfLine:
						#endif
						GTextLine *l = GetTextLine(Cursor);
						if (l)
						{
							SetCursor(l->Start + l->Len, k.Shift());
						}
						*/
					}
				}
				return true;
				break;
			}
			case VK_HOME:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						SetCursor(0, k.Shift());
					}
					else
					{
						/*
						#ifdef MAC
						Jump_StartOfLine:
						#endif
						GTextLine *l = GetTextLine(Cursor);
						if (l)
						{
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
						}
						*/
					}
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
					/*
					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						int DisplayLines = Y() / LineY;
						int CurLine = Line.IndexOf(l);

						GTextLine *New = Line.ItemAt(max(CurLine - DisplayLines, 0));
						if (New)
						{
							SetCursor(New->Start + min(Cursor - l->Start, New->Len), k.Shift());
						}
					}
					*/
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
					/*
					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						int DisplayLines = Y() / LineY;
						int CurLine = Line.IndexOf(l);

						GTextLine *New = Line.ItemAt(min(CurLine + DisplayLines, GetLines()-1));
						if (New)
						{
							SetCursor(New->Start + min(Cursor - l->Start, New->Len), k.Shift());
						}
					}
					*/
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
