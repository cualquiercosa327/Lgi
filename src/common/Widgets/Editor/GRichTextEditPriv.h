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
#include "Emoji.h"

#define DEBUG_LOG_CURSOR_COUNT			0
#define DEBUG_OUTLINE_CUR_DISPLAY_STR	0
#define DEBUG_OUTLINE_CUR_STYLE_TEXT	0
#define DEBUG_OUTLINE_BLOCKS			0
#define DEBUG_NO_DOUBLE_BUF				0
#define DEBUG_COVERAGE_CHECK			0
#define DEBUG_NUMBERED_LAYOUTS			0

#define TEXT_LINK						"Link"
#define TEXT_REMOVE_LINK				"X"
#define TEXT_REMOVE_STYLE				"Remove Style"
#define TEXT_CAP_BTN					"Ok"
#define TEXT_EMOJI						":)"

#define NoTransaction					NULL
#define IsWordBreakChar(ch)				\
	( \
		( \
			(ch) == ' ' || (ch) == '\t' || (ch) == '\r' || (ch) == '\n' \
		) \
		|| \
		( \
			EmojiToIconIndex(&(ch), 1) >= 0 \
		) \
	)

//////////////////////////////////////////////////////////////////////
#define PtrCheckBreak(ptr)				if (!ptr) { LgiAssert(!"Invalid ptr"); break; }
#undef FixedToInt
#define FixedToInt(fixed)				((fixed)>>GDisplayString::FShift)
#undef IntToFixed
#define IntToFixed(val)					((val)<<GDisplayString::FShift)

#define CursorColour					GColour::Black
#define TextColour						GColour::Black

/*
int Utf16Strlen(const uint16 *s, int &len)
{
	int c = 0;

	if (!s)
	{
		len = 0;
		return 0;
	}

	if (len < 0)
	{
		const uint16 *start = s;
		while (*s)
		{
			if ((*s & 0xfc00) == 0xD800)
			{
				s++;
				if (!*s)
					break;
				if ((*s & 0xfc00) == 0xDC00)
					s++;

				c++;
			}
			else c++;
		}
		len = s - start;
	}
	else
	{
		const uint16 *e = s + len;
		while (s < e)
		{
			if ((*s & 0xfc00) == 0xD800)
			{
				s++;
				if (s >= e)
					break;
				if ((*s & 0xfc00) == 0xDC00)
					s++;

				c++;
			}
			else c++;
		}
	}

	return c;
}
*/

//////////////////////////////////////////////////////////////////////
struct Range
{
	int Start;
	int Len;

	Range(int s, int l)
	{
		Start = s;
		Len = l;
	}

	Range Overlap(const Range &r)
	{
		Range o(0, 0);
		if (r.Start >= End())
			return o;
		if (r.End() <= Start)
			return o;

		int e = min(End(), r.End());
		o.Start = max(r.Start, Start);
		o.Len = e - o.Start;
		return o; 
	}

	int End() const
	{
		return Start + Len;
	}
};

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
	int RefCount;
	GString Name;
};

class GCssCache
{
	int Idx;
	GArray<GNamedStyle*> Styles;
	GString Prefix;

public:
	GCssCache();
	~GCssCache();

	void SetPrefix(GString s) { Prefix = s; }
	uint32 GetStyles();
	void ZeroRefCounts();
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
	
	const char *GetClass() { return "SelectColour"; }

	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void Visible(bool i);
};

class EmojiMenu : public GPopup
{
	GRichTextPriv *d;

	struct Emoji
	{
		GRect Src, Dst;
		uint32 u;
	};
	struct Pane
	{
		GRect Btn;
		GArray<Emoji> e;
	};
	GArray<Pane> Panes;
	static int Cur;

public:
	EmojiMenu(GRichTextPriv *priv, GdcPt2 p);

	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void Visible(bool i);
	bool InsertEmoji(uint32 Ch);
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

extern bool Utf16to32(GArray<uint32> &Out, const uint16 *In, int Len);

class GRichTextPriv :
	public GCss,
	public GHtmlParser,
	public GHtmlStaticInst,
	public GCssCache,
	public GFontCache
{
	GAutoPtr<GSurface> EmojiImg;

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
	GHtmlStaticInst Inst;
	int NextUid;

	// This is set when the user changes a style without a selection,
	// indicating that we should start a new run when new text is entered
	GArray<GRichTextEdit::RectType> StyleDirty;

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
	class StyleText : public GArray<uint32>
	{
		GNamedStyle *Style; // owned by the CSS cache
	
	public:
		ColourPair Colours;
		HtmlTag Element;
		GString Param;
		bool Emoji;

		StyleText(const StyleText *St)
		{
			Emoji = St->Emoji;
			Style = NULL;
			Element = St->Element;
			Param = St->Param;
			if (St->Style)
				SetStyle(St->Style);
			Add((uint32*)&St->ItemAt(0), St->Length());
		}
		
		StyleText(const uint32 *t = NULL, int Chars = -1, GNamedStyle *style = NULL)
		{
			Emoji = false;
			Style = NULL;
			Element = CONTENT;
			if (style)
				SetStyle(style);
			if (t)
			{
				if (Chars < 0)
					Chars = Strlen(t);
				Add((uint32*)t, Chars);
			}
		}

		uint32 *At(int i)
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

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Undo structures...
	struct DocChange
	{
		virtual ~DocChange() {}
		virtual bool Apply(GRichTextPriv *Ctx, bool Forward) = 0;
	};

	class Transaction
	{
		GArray<DocChange*> Changes;

	public:		
		~Transaction()
		{
			Changes.DeleteObjects();
		}

		void Add(DocChange *Dc)
		{
			Changes.Add(Dc);
		}

		bool Apply(GRichTextPriv *Ctx, bool Forward)
		{
			for (unsigned i=0; i<Changes.Length(); i++)
			{
				if (!Changes[i]->Apply(Ctx, Forward))
					return false;
			}

			return true;
		}
	};

	GArray<Transaction*> UndoQue;
	int UndoPos;

	bool AddTrans(GAutoPtr<Transaction> &t);
	bool SetUndoPos(int Pos);

	template<typename T>
	bool GetBlockByUid(T *&Ptr, int Uid, int *Idx = NULL)
	{
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Block *b = Blocks[i];
			if (b->GetUid() == Uid)
			{
				if (Idx) *Idx = i;
				return (Ptr = dynamic_cast<T*>(b)) != NULL;
			}
		}

		if (Idx) *Idx = -1;
		return false;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// A Block is like a DIV in HTML, it's as wide as the page and
	// always starts and ends on a whole line.
	class Block 
	{
	protected:
		int BlockUid;
		GRichTextPriv *d;

	public:
		/// This is the number of cursors current referencing this Block.
		int8 Cursors;
		
		Block(GRichTextPriv *priv)
		{
			d = priv;
			BlockUid = d->NextUid++;
			Cursors = 0;
		}

		Block(const Block *blk)
		{
			d = blk->d;
			BlockUid = blk->GetUid();
			Cursors = 0;
		}
		
		virtual ~Block()
		{
			// We must have removed cursors by the time we are deleted
			// otherwise there will be a hanging pointer in the cursor
			// object.
			LgiAssert(Cursors == 0);
		}
		
		/************************************************
		 * Get state methods, do not modify the block   *
		 ***********************************************/
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
			virtual int FindAt(int StartIdx, const uint32 *Str, GFindReplaceCommon *Params) = 0;
			virtual void IncAllStyleRefs() {}
			virtual void Dump() {}
			virtual GNamedStyle *GetStyle(int At = -1) = 0;
			virtual int GetUid() const { return BlockUid; }
			#ifdef _DEBUG
			virtual void DumpNodes(GTreeItem *Ti) = 0;
			#endif

			// Copy some or all of the text out
			virtual int CopyAt(int Offset, int Chars, GArray<uint32> *Text) { return false; }

			/// This method moves a cursor index.
			/// \returns the new cursor index or -1 on error.
			virtual bool Seek
			(
				/// [In] true if the next line is needed, false for the previous line
				SeekType To,
				/// [In/Out] The starting cursor.
				BlockCursor &Cursor
			) = 0;

		/************************************************
		 * Change state methods, require a transaction  *
		 ***********************************************/
			// Add some text at a given position
			virtual bool AddText
			(
				/// Current transaction
				Transaction *Trans,
				/// The index to add at (-1 = the end)
				int AtOffset,
				/// The text itself
				const uint32 *Str,
				/// [Optional] The number of characters
				int Chars = -1,
				/// [Optional] Style to give the text, NULL means "use the existing style"
				GNamedStyle *Style = NULL
			)	{ return false; }

			/// Delete some chars
			/// \returns the number of chars actually removed
			virtual int DeleteAt
			(
				Transaction *Trans,
				int Offset,
				int Chars,
				GArray<uint32> *DeletedText = NULL
			)	{ return false; }

			/// Changes the style of a range of characters
			virtual bool ChangeStyle
			(
				Transaction *Trans,
				int Offset,
				int Chars,
				GCss *Style,
				bool Add
			)	{ return false; }

			virtual bool DoCase
			(
				/// Current transaction
				Transaction *Trans,
				/// Start index of text to change
				int StartIdx,
				/// Number of chars to change
				int Chars,
				/// True if upper case is desired
				bool Upper
			)	{ return false; }
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
		bool operator ==(const BlockCursor &c)
		{
			return Blk == c.Blk &&
				Offset == c.Offset;
		}

		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif
	};
	
	GAutoPtr<BlockCursor> Cursor, Selection;

	/// This is part or all of a Text run
	struct DisplayStr : public GDisplayString
	{
		StyleText *Src;
		int Chars;		// The number of UTF-32 characters. This can be different to 
						// GDisplayString::Length() in the case that GDisplayString 
						// is using UTF-16 (i.e. Windows).
		int OffsetY;	// Offset of this string from the TextLine's box in the Y axis
		
		DisplayStr(StyleText *src, GFont *f, const uint32 *s, int l = -1, GSurface *pdc = NULL) :
			GDisplayString(f,
				#ifndef WINDOWS
				(char16*)
				#endif
				s, l, pdc)
		{
			Src = src;
			OffsetY = 0;
			#if defined(_MSC_VER)
			Chars = l < 0 ? Strlen(s) : l;
			#else
			Chars = len;
			#endif
		}
		
		template<typename T>
		T *Utf16Seek(T *s, int i)
		{
			T *e = s + i;
			while (s < e)
			{
				uint16 n = *s & 0xfc00;
				if (n == 0xd800)
				{
					s++;
					if (s >= e)
						break;

					n = *s & 0xfc00;
					if (n != 0xdc00)
					{
						LgiAssert(!"Unexpected surrogate");
						continue;
					}
					// else skip over the 2nd surrogate
				}

				s++;
			}
			
			return s;
		}
		
		// Make a sub-string of this display string
		virtual GAutoPtr<DisplayStr> Clone(int Start, int Len = -1)
		{
			GAutoPtr<DisplayStr> c;
			if (len > 0 && Len != 0)
			{
				const char16 *Str = *this;
				if (Len < 0)
					Len = len - Start;
				if (Start >= 0 &&
					Start < (int)len &&
					Start + Len <= (int)len)
				{
					#if defined(_MSC_VER)
					LgiAssert(Str != NULL);
					const char16 *s = Utf16Seek(Str, Start);
					const char16 *e = Utf16Seek(s, Len);
					GArray<uint32> Tmp;
					if (Utf16to32(Tmp, (const uint16*)s, e - s))
						c.Reset(new DisplayStr(Src, GetFont(), &Tmp[0], Tmp.Length(), pDC));
					#else
					c.Reset(new DisplayStr(Src, GetFont(), (uint32*)Str + Start, Len, pDC));
					#endif
				}
			}		
			return c;
		}

		virtual void Paint(GSurface *pDC, int &FixX, int FixY, GColour &Back)
		{
			FDraw(pDC, FixX, FixY);
			FixX += FX();
		}

		virtual double GetAscent()
		{
			return Font->Ascent();
		}
		
		virtual int PosToIndex(int x, bool Nearest)
		{
			return CharAt(x);
		}
	};
	
	struct EmojiDisplayStr : public DisplayStr
	{
		GArray<GRect> SrcRect;
		GSurface *Img;
		#if defined(_MSC_VER)
		GArray<uint32> Utf32;
		#endif

		EmojiDisplayStr(StyleText *src, GSurface *img, GFont *f, const uint32 *s, int l = -1) :
			DisplayStr(src, NULL, s, l)
		{
			Img = img;
			#if defined(_MSC_VER)
			Utf16to32(Utf32, (const uint16*) StrCache.Get(), len);
			uint32 *u = &Utf32[0];
			#else
			uint32 *u = (uint32*)StrCache.Get();
			#endif

			for (int i=0; i<Chars; i++)
			{
				int Idx = EmojiToIconIndex(u + i, Chars - i);
				LgiAssert(Idx >= 0);
				if (Idx >= 0)
				{
					int x = Idx % EMOJI_GROUP_X;
					int y = Idx / EMOJI_GROUP_X;
					GRect &rc = SrcRect[i];
					rc.ZOff(EMOJI_CELL_SIZE-1, EMOJI_CELL_SIZE-1);
					rc.Offset(x * EMOJI_CELL_SIZE, y * EMOJI_CELL_SIZE);
				}
			}

			x = SrcRect.Length() * EMOJI_CELL_SIZE;
			y = EMOJI_CELL_SIZE;
			xf = IntToFixed(x);
			yf = IntToFixed(y);
		}

		GAutoPtr<DisplayStr> Clone(int Start, int Len = -1)
		{
			if (Len < 0)
				Len = Chars - Start;
			#if defined(_MSC_VER)
			LgiAssert(	Start >= 0 &&
						Start < (int)Utf32.Length() &&
						Start + Len <= (int)Utf32.Length());
			#endif
			GAutoPtr<DisplayStr> s(new EmojiDisplayStr(Src, Img, NULL,
				#if defined(_MSC_VER)
				&Utf32[Start]
				#else
				(uint32*)(const char16*)(*this)
				#endif
				, Len));
			return s;
		}

		void Paint(GSurface *pDC, int &FixX, int FixY, GColour &Back)
		{
			GRect f(0, 0, x-1, y-1);
			f.Offset(FixedToInt(FixX), FixedToInt(FixY));
			pDC->Colour(Back);
			pDC->Rectangle(&f);

			int Op = pDC->Op(GDC_ALPHA);
			for (unsigned i=0; i<SrcRect.Length(); i++)
			{
				pDC->Blt(f.x1, f.y1, Img, &SrcRect[i]);
				f.x1 += EMOJI_CELL_SIZE;
				FixX += IntToFixed(EMOJI_CELL_SIZE);
			}
			pDC->Op(Op);
		}

		double GetAscent()
		{
			return EMOJI_CELL_SIZE * 0.8;
		}

		int PosToIndex(int XPos, bool Nearest)
		{
			if (XPos >= (int)x)
				return Chars;
			if (XPos <= 0)
				return 0;
			return (XPos + (Nearest ? EMOJI_CELL_SIZE >> 1 : 0)) / EMOJI_CELL_SIZE;
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
				Len += Strs[i]->Chars;
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
				double Ascent = ds->GetAscent();
				BaseLine = max(BaseLine, Ascent);
				HtPx = max(HtPx, ds->Y());
			}
			
			if (Strs.Length() == 0)
				HtPx = DefaultFontHt;
			else
				LgiAssert(HtPx > 0);
			
			for (unsigned i=0; i<Strs.Length(); i++)
			{
				DisplayStr *ds = Strs[i];
				double Ascent = ds->GetAscent();
				if (Ascent > 0.0)
					ds->OffsetY = (int)(BaseLine - Ascent);
				LgiAssert(ds->OffsetY >= 0);
				HtPx = max(HtPx, ds->OffsetY+ds->Y());
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
		TextBlock(const TextBlock *Copy);
		~TextBlock();

		bool IsValid();

		// No state change methods
		int GetLines();
		bool OffsetToLine(int Offset, int *ColX, GArray<int> *LineY);
		int LineToOffset(int Line);
		GRect GetPos() { return Pos; }
		void Dump();
		GNamedStyle *GetStyle(int At = -1);
		void SetStyle(GNamedStyle *s);
		int Length();
		bool ToHtml(GStream &s);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		int GetTextAt(uint32 Offset, GArray<StyleText*> &t);
		int CopyAt(int Offset, int Chars, GArray<uint32> *Text);
		bool Seek(SeekType To, BlockCursor &Cursor);
		int FindAt(int StartIdx, const uint32 *Str, GFindReplaceCommon *Params);
		void IncAllStyleRefs();
		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif

		// Transactional changes
		bool AddText(Transaction *Trans, int AtOffset, const uint32 *Str, int Chars = -1, GNamedStyle *Style = NULL);
		bool ChangeStyle(Transaction *Trans, int Offset, int Chars, GCss *Style, bool Add);
		int DeleteAt(Transaction *Trans, int BlkOffset, int Chars, GArray<uint32> *DeletedText = NULL);
		bool DoCase(Transaction *Trans, int StartIdx, int Chars, bool Upper);
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
	bool Merge(Block *a, Block *b);
	GSurface *GetEmojiImage();
	bool DeleteSelection(Transaction *t, char16 **Cut);

	struct CreateContext
	{
		TextBlock *Tb;
		GArray<uint32> Buf;
		char16 LastChar;
		GFontCache *FontCache;
		GCss::Store StyleStore;
		bool StartOfLine;
		
		CreateContext(GFontCache *fc)
		{
			Tb = NULL;
			LastChar = '\n';
			FontCache = fc;
			StartOfLine = true;
		}
		
		bool AddText(GNamedStyle *Style, char16 *Str)
		{
			if (!Str || !Tb)
				return false;
			
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
			
			bool Status = false;
			if (Used > 0)
			{
				Status = Tb->AddText(NoTransaction, -1, &Buf[0], Used, Style);
				LastChar = Buf[Used-1];
			}
			return Status;
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

struct BlockCursorState
{
	bool Cursor;
	int Offset;
	int LineHint;
	int BlockUid;

	BlockCursorState(bool cursor, GRichTextPriv::BlockCursor *c);
	bool Apply(GRichTextPriv *Ctx, bool Forward);
};

struct CompleteTextBlockState : public GRichTextPriv::DocChange
{
	int Uid;
	GAutoPtr<BlockCursorState> Cur, Sel;
	GAutoPtr<GRichTextPriv::TextBlock> Blk;

	CompleteTextBlockState(GRichTextPriv *Ctx, GRichTextPriv::TextBlock *Tb);
	bool Apply(GRichTextPriv *Ctx, bool Forward);
};

#ifdef _DEBUG
GTreeItem *PrintNode(GTreeItem *Parent, const char *Fmt, ...);
#endif


#endif