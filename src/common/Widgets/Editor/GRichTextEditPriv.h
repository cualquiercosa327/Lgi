/* Rich text design notes:

- The document is an array of Blocks (Blocks have no hierarchy)
- Blocks have a length in characters. New lines are considered as one '\n' char.
- Currently the main type of block is the TextBlock
	- TextBlock contains:
		- array of StyleText:
			This is the source text. Each run of text has a style associated with it.
			This forms the input to the layout algorithm and is what the user is 
			editing.
		- array of TextLine:
			Contains all the info needed to render one line of text. Essentially
			the output of the layout engine.
			Contains an array of DisplayStr objects. i.e. Characters in the exact
			same style as each other.
			It will regularly be deleted and re-flowed from the StyleText objects.
	- For a plaint text document the entire thing is contained by the one TextBlock.
- There will be an Image block down the track, where the image is treated as one character object.

*/
#ifndef _RICH_TEXT_EDIT_PRIV_H_
#define _RICH_TEXT_EDIT_PRIV_H_

#include "GHtmlCommon.h"
#include "GHtmlParser.h"
#include "GFontCache.h"
#include "GDisplayString.h"
#include "GColourSpace.h"
#include "GPopup.h"

#define DEBUG_LOG_CURSOR_COUNT			0
#define DEBUG_OUTLINE_CUR_DISPLAY_STR	0
#define DEBUG_OUTLINE_CUR_STYLE_TEXT	0
#define DEBUG_OUTLINE_BLOCKS			0
#define DEBUG_NO_DOUBLE_BUF				0
#define DEBUG_COVERAGE_CHECK			0
#define DEBUG_NUMBERED_LAYOUTS			0

#define TEXT_LINK						"Link"
#define TEXT_REMOVE_STYLE				"Remove Style"
#define TEXT_CAP_BTN					"Ok"

#define IsWordBreakChar(ch)				IsWhiteSpace(ch) // FIXME: Add asian character set support to this

//////////////////////////////////////////////////////////////////////
#define PtrCheckBreak(ptr)				if (!ptr) { LgiAssert(!"Invalid ptr"); break; }
#undef FixedToInt
#define FixedToInt(fixed)				((fixed)>>GDisplayString::FShift)
#undef IntToFixed
#define IntToFixed(val)					((val)<<GDisplayString::FShift)

#define CursorColour					GColour::Black
#define TextColour						GColour::Black

//////////////////////////////////////////////////////////////////////
class GRichEditElem : public GHtmlElement
{
	GHashTbl<const char*, GString> Attr;

public:
	GRichEditElem(GHtmlElement *parent) : GHtmlElement(parent)
	{
	}

	bool Get(const char *attr, const char *&val)
	{
		if (!attr)
			return false;

		GString s = Attr.Find(attr);
		if (!s)
			return false;
		
		val = s;
		return true;
	}
	
	void Set(const char *attr, const char *val)
	{
		if (!attr)
			return;
		Attr.Add(attr, GString(val));
	}
	
	void SetStyle()
	{
		
	}
};

struct GRichEditElemContext : public GCss::ElementCallback<GRichEditElem>
{
	/// Returns the element name
	const char *GetElement(GRichEditElem *obj);
	/// Returns the document unque element ID
	const char *GetAttr(GRichEditElem *obj, const char *Attr);
	/// Returns the class
	bool GetClasses(GArray<const char *> &Classes, GRichEditElem *obj);
	/// Returns the parent object
	GRichEditElem *GetParent(GRichEditElem *obj);
	/// Returns an array of child objects
	GArray<GRichEditElem*> GetChildren(GRichEditElem *obj);
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
	GCssCache();
	~GCssCache();

	bool OutputStyles(GStream &s, int TabDepth);
	GNamedStyle *AddStyleToCache(GAutoPtr<GCss> &s);
};

class GRichTextPriv;
class SelectColour : public GPopup
{
	GRichTextPriv *d;
	GRichTextEdit::RectType Type;

	struct Entry
	{
		GRect r;
		GColour c;
	};
	GArray<Entry> e;

public:
	SelectColour(GRichTextPriv *priv, GdcPt2 p, GRichTextEdit::RectType t);

	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void Visible(bool i);
};

struct CtrlCap
{
	GString Name, Param;

	void Set(const char *name, const char *param)
	{
		Name = name;
		Param = param;
	}
};

class GRichTextPriv :
	public GCss,
	public GHtmlParser,
	public GHtmlStaticInst,
	public GCssCache,
	public GFontCache
{
public:
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

	GRichTextEdit *View;
	GString OriginalText;
	GAutoWString WideNameCache;
	GAutoString UtfNameCache;
	GAutoPtr<GFont> Font;
	bool WordSelectMode;
	bool Dirty;
	GdcPt2 DocumentExtent; // Px
	GString Charset;

	// Toolbar
	bool ShowTools;
	GRect Areas[GRichTextEdit::MaxArea];
	GVariant Values[GRichTextEdit::MaxArea];

	// Scrolling
	int ScrollLinePx;
	int ScrollOffsetPx;
	bool ScrollChange;

	// Capabilities
	GArray<CtrlCap> NeedsCap;

	// Debug stuff
	GArray<GRect> DebugRects;

	// Constructor
	GRichTextPriv(GRichTextEdit *view, GRichTextPriv *&Ptr);	
	~GRichTextPriv();
	
	bool Error(const char *file, int line, const char *fmt, ...);

	struct Flow
	{
		GRichTextPriv *d;
		GSurface *pDC;	// Used for printing.

		int Left, Right;// Left and right margin positions as measured in px
						// from the left of the page (controls client area).
		int Top;
		int CurY;		// Current y position down the page in document co-ords
		bool Visible;	// true if the current block overlaps the visible page
						// If false, the implementation can take short cuts and
						// guess various dimensions.
	
		Flow(GRichTextPriv *priv)
		{
			d = priv;
			pDC = NULL;
			Left = 0;
			Top = 0;
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

	/// This is a run of text, all of the same style
	class StyleText : public GArray<char16>
	{
		GNamedStyle *Style; // owned by the CSS cache
	
	public:
		ColourPair Colours;
		HtmlTag Element;
		GString Param;
		
		StyleText(const char16 *t = NULL, int Chars = -1, GNamedStyle *style = NULL)
		{
			Style = NULL;
			Element = CONTENT;
			if (style)
				SetStyle(style);
			if (t)
				Add((char16*)t, Chars >= 0 ? Chars : StrlenW(t));
		}

		char16 *At(int i)
		{
			if (i >= 0 && i < (int)Length())
				return &(*this)[i];
			LgiAssert(0);
			return NULL;
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
		int Index;
		GSurface *pDC;
		SelectModeType Type;
		ColourPair Colours[2];
		BlockCursor *Cursor, *Select;
		
		PaintContext()
		{
			Index = 0;
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
		int LineHint;
		bool Near;
		
		HitTestResult(int x, int y)
		{
			In.x = x;
			In.y = y;
			Blk = NULL;
			Ds = NULL;
			Idx = -1;
			LineHint = -1;
			Near = false;
		}
	};

	class Block // is like a DIV in HTML, it's as wide as the page and
				// always starts and ends on a whole line.
	{
	protected:
		GRichTextPriv *d;

	public:
		/// This is the number of cursors current referencing this Block.
		int8 Cursors;
		
		Block(GRichTextPriv *priv)
		{
			d = priv;
			Cursors = 0;
		}
		
		virtual ~Block()
		{
			// We must have removed cursors by the time we are deleted
			// otherwise there will be a hanging pointer in the cursor
			// object.
			LgiAssert(Cursors == 0);
		}
		
		virtual GRect GetPos() = 0;
		virtual int Length() = 0;
		virtual bool HitTest(HitTestResult &htr) = 0;
		virtual bool GetPosFromIndex(BlockCursor *Cursor) = 0;
		virtual bool OnLayout(Flow &f) = 0;
		virtual void OnPaint(PaintContext &Ctx) = 0;
		virtual bool ToHtml(GStream &s) = 0;
		virtual bool OffsetToLine(int Offset, int *ColX, GArray<int> *LineY) = 0;
		virtual int LineToOffset(int Line) = 0;
		virtual int GetLines() = 0;
		virtual int FindAt(int StartIdx, const char16 *Str, GFindReplaceCommon *Params) = 0;
		
		/// This method moves a cursor index.
		/// \returns the new cursor index or -1 on error.
		virtual bool Seek
		(
			/// [In] true if the next line is needed, false for the previous line
			SeekType To,
			/// [In/Out] The starting cursor.
			BlockCursor &Cursor
		) = 0;

		// Add some text at a given position
		virtual bool AddText
		(
			/// The index to add at (-1 = the end)
			int AtOffset,
			/// The text itself
			const char16 *Str,
			/// [Optional] The number of characters
			int Chars = -1,
			/// [Optional] Style to give the text, NULL means "use the existing style"
			GNamedStyle *Style = NULL
		)	{ return false; }

		/// Delete some chars
		/// \returns the number of chars actually removed
		virtual int DeleteAt(int Offset, int Chars, GArray<char16> *DeletedText = NULL) { return false; }
		// Copy some or all of the text out
		virtual int CopyAt(int Offset, int Chars, GArray<char16> *Text) { return false; }

		/// Changes the style of a range of characters
		virtual bool ChangeStyle(int Offset, int Chars, GCss *Style, bool Add) = 0;

		virtual void Dump() {}
		virtual GNamedStyle *GetStyle() = 0;

		#ifdef _DEBUG
		virtual void DumpNodes(GTreeItem *Ti) = 0;
		#endif
	};

	struct BlockCursor
	{
		// The block the cursor is in.
		Block *Blk;

		// This is the character offset of the cursor relative to
		// the start of 'Blk'.
		int Offset;

		// In wrapped text, a given offset can either be at the end
		// of one line or the start of the next line. This tells the
		// text block which line the cursor is actually on.
		int LineHint;
		
		// This is the position on the screen in doc coords.
		GRect Pos;
		
		// This is the position line that the cursor is on. This is
		// used to calculate the bounds for screen updates.
		GRect Line;

		// Cursor is currently blinking on
		bool Blink;

		BlockCursor(const BlockCursor &c);
		BlockCursor(Block *b, int off, int line);
		~BlockCursor();
		
		BlockCursor &operator =(const BlockCursor &c);
		void Set(int off);
		void Set(Block *b, int off, int line);

		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif
	};
	
	GAutoPtr<BlockCursor> Cursor, Selection;

	/// This is part or all of a Text run
	struct DisplayStr : public GDisplayString
	{
		StyleText *Src;
		int OffsetY; // Offset of this string from the TextLine's box in the Y axis
		
		DisplayStr(StyleText *src, GFont *f, const char16 *s, int l = -1, GSurface *pdc = NULL) :
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
			PosOff.ZOff(0, 0);
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

		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif
	};
	
	class TextBlock : public Block
	{
		GNamedStyle *Style;

	public:
		GArray<StyleText*> Txt;
		GArray<TextLine*> Layout;
		GRect Margin, Border, Padding;
		GFont *Fnt;
		
		bool LayoutDirty;
		int Len; // chars in the whole block (sum of all Text lengths)
		GRect Pos; // position in document co-ordinates
		
		TextBlock(GRichTextPriv *priv);
		~TextBlock();

		bool IsValid();

		int GetLines();
		bool OffsetToLine(int Offset, int *ColX, GArray<int> *LineY);
		int LineToOffset(int Line);
		GRect GetPos() { return Pos; }
		void Dump();
		GNamedStyle *GetStyle();		
		void SetStyle(GNamedStyle *s);
		int Length();
		bool ToHtml(GStream &s);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		StyleText *GetTextAt(uint32 Offset);
		int DeleteAt(int BlkOffset, int Chars, GArray<char16> *DeletedText = NULL);
		int CopyAt(int Offset, int Chars, GArray<char16> *Text);
		bool AddText(int AtOffset, const char16 *Str, int Chars = -1, GNamedStyle *Style = NULL);
		bool ChangeStyle(int Offset, int Chars, GCss *Style, bool Add);
		bool Seek(SeekType To, BlockCursor &Cursor);
		int FindAt(int StartIdx, const char16 *Str, GFindReplaceCommon *Params);

		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif
	};
	
	GArray<Block*> Blocks;
	Block *Next(Block *b);
	Block *Prev(Block *b);

	void InvalidateDoc(GRect *r);
	void ScrollTo(GRect r);
	void UpdateStyleUI();

	void EmptyDoc();	
	void Empty();
	bool Seek(BlockCursor *In, SeekType Dir, bool Select);
	bool CursorFirst();
	bool SetCursor(GAutoPtr<BlockCursor> c, bool Select = false);
	GRect SelectionRect();
	bool GetSelection(GArray<char16> &Text);
	int IndexOfCursor(BlockCursor *c);
	int HitTest(int x, int y, int &LineHint);
	bool CursorFromPos(int x, int y, GAutoPtr<BlockCursor> *Cursor, int *GlobalIdx);
	Block *GetBlockByIndex(int Index, int *Offset = NULL, int *BlockIdx = NULL, int *LineCount = NULL);
	bool Layout(GScrollBar *&ScrollY);
	void OnStyleChange(GRichTextEdit::RectType t);
	bool ChangeSelectionStyle(GCss *Style, bool Add);
	void PaintBtn(GSurface *pDC, GRichTextEdit::RectType t);
	bool ClickBtn(GMouse &m, GRichTextEdit::RectType t);
	void Paint(GSurface *pDC, GScrollBar *&ScrollY);
	GHtmlElement *CreateElement(GHtmlElement *Parent);
	GdcPt2 ScreenToDoc(int x, int y);
	GdcPt2 DocToScreen(int x, int y);

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
				Tb->AddText(-1, &Buf[0], Used, Style);
				LastChar = Buf[Used-1];
			}
		}
	};
	
	GAutoPtr<CreateContext> CreationCtx;

	bool ToHtml();
	void DumpBlocks();
	bool FromHtml(GHtmlElement *e, CreateContext &ctx, GCss *ParentStyle = NULL, int Depth = 0);

	#ifdef _DEBUG
	void DumpNodes(GTree *Root);
	#endif
};

#ifdef _DEBUG
GTreeItem *PrintNode(GTreeItem *Parent, const char *Fmt, ...);
#endif


#endif