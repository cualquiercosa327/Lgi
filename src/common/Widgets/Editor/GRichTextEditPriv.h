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
#include "LgiSpellCheck.h"

#define DEBUG_LOG_CURSOR_COUNT			0
#define DEBUG_OUTLINE_CUR_DISPLAY_STR	0
#define DEBUG_OUTLINE_CUR_STYLE_TEXT	0
#define DEBUG_OUTLINE_BLOCKS			0
#define DEBUG_NO_DOUBLE_BUF				0
#define DEBUG_COVERAGE_CHECK			0
#define DEBUG_NUMBERED_LAYOUTS			0
#if 0 // _DEBUG
#define LOG_FN							LgiTrace
#else
#define LOG_FN							d->Log->Print
#endif

#define TEXT_LINK						"Link"
#define TEXT_REMOVE_LINK				"X"
#define TEXT_REMOVE_STYLE				"Remove Style"
#define TEXT_CAP_BTN					"Ok"
#define TEXT_EMOJI						":)"
#define TEXT_HORZRULE					"HR"

#define RTE_CURSOR_BLINK_RATE			1000
#define RTE_PULSE_RATE					200

#define RICH_TEXT_RESIZED_JPEG_QUALITY	83 // out of 100, high = better quality

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

enum RteCommands
{
	// IDM_OPEN = 10,
	IDM_NEW = 2000,
	IDM_RTE_COPY,
	IDM_RTE_CUT,
	IDM_RTE_PASTE,
	IDM_RTE_UNDO,
	IDM_RTE_REDO,
	IDM_COPY_URL,
	IDM_AUTO_INDENT,
	IDM_UTF8,
	IDM_PASTE_NO_CONVERT,
	IDM_FIXED,
	IDM_SHOW_WHITE,
	IDM_HARD_TABS,
	IDM_INDENT_SIZE,
	IDM_TAB_SIZE,
	IDM_DUMP,
	IDM_RTL,
	IDM_COPY_ORIGINAL,
	IDM_COPY_CURRENT,
	IDM_DUMP_NODES,
	IDM_CLOCKWISE,
	IDM_ANTI_CLOCKWISE,
	IDM_X_FLIP,
	IDM_Y_FLIP,
	IDM_SCALE_IMAGE,
	CODEPAGE_BASE = 100,
	CONVERT_CODEPAGE_BASE = 200,
	SPELLING_BASE = 300
};

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
#include "GRange.h"

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

struct ButtonState
{
	uint8 IsMenu : 1;
	uint8 IsPress : 1;
	uint8 Pressed : 1;
	uint8 MouseOver : 1;
};

extern bool Utf16to32(GArray<uint32> &Out, const uint16 *In, int Len);

class GEmojiContext
{
	GAutoPtr<GSurface> EmojiImg;

public:
	GSurface *GetEmojiImage();
};

class GRichTextPriv :
	public GCss,
	public GHtmlParser,
	public GHtmlStaticInst,
	public GCssCache,
	public GFontCache,
	public GEmojiContext
{
	GStringPipe LogBuffer;

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
	GStream *Log;
	bool HtmlLinkAsCid;
	uint64 BlinkTs;

	// Spell check support
	GSpellCheck *SpellCheck;
	bool SpellDictionaryLoaded;
	GString SpellLang, SpellDict;

	// This is set when the user changes a style without a selection,
	// indicating that we should start a new run when new text is entered
	GArray<GRichTextEdit::RectType> StyleDirty;

	// Toolbar
	bool ShowTools;
	GRichTextEdit::RectType ClickedBtn, OverBtn;
	ButtonState BtnState[GRichTextEdit::MaxArea];
	GRect Areas[GRichTextEdit::MaxArea];
	GVariant Values[GRichTextEdit::MaxArea];

	// Scrolling
	int ScrollLinePx;
	int ScrollOffsetPx;
	bool ScrollChange;

	// Capabilities
	// GArray<CtrlCap> NeedsCap;

	// Debug stuff
	GArray<GRect> DebugRects;

	// Constructor
	GRichTextPriv(GRichTextEdit *view, GRichTextPriv **Ptr);
	~GRichTextPriv();
	
	bool Error(const char *file, int line, const char *fmt, ...);
	bool IsBusy(bool Stop = false);

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

		StyleText(const StyleText *St);
		StyleText(const uint32 *t = NULL, ssize_t Chars = -1, GNamedStyle *style = NULL);
		uint32 *At(ssize_t i);
		GNamedStyle *GetStyle();
		void SetStyle(GNamedStyle *s);
	};
	
	struct PaintContext
	{
		int Index;
		GSurface *pDC;
		SelectModeType Type;
		ColourPair Colours[2];
		BlockCursor *Cursor, *Select;

		// Cursor stuff
		int CurEndPoint;
		GArray<ssize_t> EndPoints;
		
		PaintContext()
		{
			Index = 0;
			pDC = NULL;
			Type = Unselected;
			Cursor = NULL;
			Select = NULL;
			CurEndPoint = 0;
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

		// This handles calculating the selection stuff for simple "one char" blocks
		// like images and HR. Call this at the start of the OnPaint.
		// \return TRUE if the content should be drawn selected.
		bool SelectBeforePaint(class GRichTextPriv::Block *b)
		{
			CurEndPoint = 0;

			if (b->Cursors > 0 && Select)
			{
				// Selection end point checks...
				if (Cursor && Cursor->Blk == b)
					EndPoints.Add(Cursor->Offset);
				if (Select && Select->Blk == b)
					EndPoints.Add(Select->Offset);
				
				// Sort the end points
				if (EndPoints.Length() > 1 &&
					EndPoints[0] > EndPoints[1])
				{
					ssize_t ep = EndPoints[0];
					EndPoints[0] = EndPoints[1];
					EndPoints[1] = ep;
				}
			}

			// Before selection end point
			if (CurEndPoint < EndPoints.Length() &&
				EndPoints[CurEndPoint] == 0)
			{
				Type = Type == Selected ? Unselected : Selected;
				CurEndPoint++;
			}

			return Type == Selected;
		}

		// Call this after the OnPaint
		// \return TRUE if the content after the block is selected.
		bool SelectAfterPaint(class GRichTextPriv::Block *b)
		{
			// After image selection end point
			if (CurEndPoint < EndPoints.Length() &&
				EndPoints[CurEndPoint] == 1)
			{
				Type = Type == Selected ? Unselected : Selected;
				CurEndPoint++;
			}

			return Type == Selected;
		}
	};

	struct HitTestResult
	{
		GdcPt2 In;
		Block *Blk;
		DisplayStr *Ds;
		ssize_t Idx;
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
	public:		
		GArray<DocChange*> Changes;

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
				DocChange *dc = Changes[i];
				if (!dc->Apply(Ctx, Forward))
					return false;
			}

			return true;
		}
	};

	GArray<Transaction*> UndoQue;
	ssize_t UndoPos;

	bool AddTrans(GAutoPtr<Transaction> &t);
	bool SetUndoPos(ssize_t Pos);

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
	class Block :
		public GEventSinkI,
		public GEventTargetI
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
		
		// Events
		bool PostEvent(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0)
		{
			bool r = d->View->PostEvent(M_BLOCK_MSG,
										(GMessage::Param)(Block*)this,
										(GMessage::Param)new GMessage(Cmd, a, b));
			#if defined(_DEBUG)
			if (!r)
				LgiTrace("%s:%i - Warning: PostEvent failed..\n", _FL);
			#endif
			return r;
		}

		GMessage::Result OnEvent(GMessage *Msg)
		{
			return 0;
		}

		/************************************************
		 * Get state methods, do not modify the block   *
		 ***********************************************/
			virtual const char *GetClass() { return "Block"; }
			virtual GRect GetPos() = 0;
			virtual ssize_t Length() = 0;
			virtual bool HitTest(HitTestResult &htr) = 0;
			virtual bool GetPosFromIndex(BlockCursor *Cursor) = 0;
			virtual bool OnLayout(Flow &f) = 0;
			virtual void OnPaint(PaintContext &Ctx) = 0;
			virtual bool ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media) = 0;
			virtual bool OffsetToLine(ssize_t Offset, int *ColX, GArray<int> *LineY) = 0;
			virtual int LineToOffset(int Line) = 0;
			virtual int GetLines() = 0;
			virtual ssize_t FindAt(ssize_t StartIdx, const uint32 *Str, GFindReplaceCommon *Params) = 0;
			virtual void SetSpellingErrors(GArray<GSpellCheck::SpellingError> &Errors) {}
			virtual void IncAllStyleRefs() {}
			virtual void Dump() {}
			virtual GNamedStyle *GetStyle(ssize_t At = -1) = 0;
			virtual int GetUid() const { return BlockUid; }
			virtual bool DoContext(GSubMenu &s, GdcPt2 Doc, ssize_t Offset, bool Spelling) { return false; }
			#ifdef _DEBUG
			virtual void DumpNodes(GTreeItem *Ti) = 0;
			#endif
			virtual bool IsValid() { return false; }
			virtual bool IsBusy(bool Stop = false) { return false; }
			virtual Block *Clone() = 0;
			virtual void OnComponentInstall(GString Name) {}

			// Copy some or all of the text out
			virtual ssize_t CopyAt(ssize_t Offset, ssize_t Chars, GArray<uint32> *Text) { return false; }

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
				ssize_t AtOffset,
				/// The text itself
				const uint32 *Str,
				/// [Optional] The number of characters
				ssize_t Chars = -1,
				/// [Optional] Style to give the text, NULL means "use the existing style"
				GNamedStyle *Style = NULL
			)	{ return false; }

			/// Delete some chars
			/// \returns the number of chars actually removed
			virtual ssize_t DeleteAt
			(
				Transaction *Trans,
				ssize_t Offset,
				ssize_t Chars,
				GArray<uint32> *DeletedText = NULL
			)	{ return false; }

			/// Changes the style of a range of characters
			virtual bool ChangeStyle
			(
				Transaction *Trans,
				ssize_t Offset,
				ssize_t Chars,
				GCss *Style,
				bool Add
			)	{ return false; }

			virtual bool DoCase
			(
				/// Current transaction
				Transaction *Trans,
				/// Start index of text to change
				ssize_t StartIdx,
				/// Number of chars to change
				ssize_t Chars,
				/// True if upper case is desired
				bool Upper
			)	{ return false; }

			// Split a block
			virtual Block *Split
			(
				/// Current transaction
				Transaction *Trans,
				/// The index to add at (-1 = the end)
				ssize_t AtOffset
			)	{ return NULL; }
			// Event called on dictionary load
			virtual bool OnDictionary(Transaction *Trans) { return false; }
	};

	struct BlockCursor
	{
		// The block the cursor is in.
		Block *Blk;

		// This is the character offset of the cursor relative to
		// the start of 'Blk'.
		ssize_t Offset;

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
		BlockCursor(Block *b, ssize_t off, int line);
		~BlockCursor();
		
		BlockCursor &operator =(const BlockCursor &c);
		void Set(ssize_t off);
		void Set(Block *b, ssize_t off, int line);
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
		ssize_t Chars;	// The number of UTF-32 characters. This can be different to
						// GDisplayString::Length() in the case that GDisplayString 
						// is using UTF-16 (i.e. Windows).
		int OffsetY;	// Offset of this string from the TextLine's box in the Y axis
		
		DisplayStr(StyleText *src, GFont *f, const uint32 *s, ssize_t l = -1, GSurface *pdc = NULL) :
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
		virtual GAutoPtr<DisplayStr> Clone(ssize_t Start, ssize_t Len = -1)
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
		
		virtual ssize_t PosToIndex(int x, bool Nearest)
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

		EmojiDisplayStr(StyleText *src, GSurface *img, GFont *f, const uint32 *s, ssize_t l = -1);
		GAutoPtr<DisplayStr> Clone(ssize_t Start, ssize_t Len = -1);
		void Paint(GSurface *pDC, int &FixX, int FixY, GColour &Back);
		double GetAscent();
		ssize_t PosToIndex(int XPos, bool Nearest);
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
		
		TextLine(int XOffsetPx, int WidthPx, int YOffsetPx);
		int Length();
		
		/// This runs after the layout line has been filled with display strings.
		/// It measures the line and works out the right offsets for each strings
		/// so that their baselines all match up correctly.
		void LayoutOffsets(int DefaultFontHt);
	};
	
	class TextBlock : public Block
	{
		GNamedStyle *Style;
		GArray<GSpellCheck::SpellingError> SpellingErrors;
		int PaintErrIdx, ClickErrIdx;
		GSpellCheck::SpellingError *SpErr;

		bool PreEdit(Transaction *Trans);
		void UpdateSpellingAndLinks(Transaction *Trans, GRange r);
		void DrawDisplayString(GSurface *pDC, DisplayStr *Ds, int &FixX, int FixY, GColour &Bk, int &Pos);
	
	public:
		GArray<StyleText*> Txt;
		GArray<TextLine*> Layout;
		GRect Margin, Border, Padding;
		GFont *Fnt;
		
		bool LayoutDirty;
		ssize_t Len; // chars in the whole block (sum of all Text lengths)
		GRect Pos; // position in document co-ordinates
		
		TextBlock(GRichTextPriv *priv);
		TextBlock(const TextBlock *Copy);
		~TextBlock();

		bool IsValid();

		// No state change methods
		const char *GetClass() { return "TextBlock"; }
		int GetLines();
		bool OffsetToLine(ssize_t Offset, int *ColX, GArray<int> *LineY);
		int LineToOffset(int Line);
		GRect GetPos() { return Pos; }
		void Dump();
		GNamedStyle *GetStyle(ssize_t At = -1);
		void SetStyle(GNamedStyle *s);
		ssize_t Length();
		bool ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		ssize_t GetTextAt(ssize_t Offset, GArray<StyleText*> &t);
		ssize_t CopyAt(ssize_t Offset, ssize_t Chars, GArray<uint32> *Text);
		bool Seek(SeekType To, BlockCursor &Cursor);
		ssize_t FindAt(ssize_t StartIdx, const uint32 *Str, GFindReplaceCommon *Params);
		void IncAllStyleRefs();
		void SetSpellingErrors(GArray<GSpellCheck::SpellingError> &Errors);
		bool DoContext(GSubMenu &s, GdcPt2 Doc, ssize_t Offset, bool Spelling);
		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif
		Block *Clone();
		bool IsEmptyLine(BlockCursor *Cursor);

		// Events
		GMessage::Result OnEvent(GMessage *Msg);

		// Transactional changes
		bool AddText(Transaction *Trans, ssize_t AtOffset, const uint32 *Str, ssize_t Chars = -1, GNamedStyle *Style = NULL);
		bool ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, GCss *Style, bool Add);
		ssize_t DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, GArray<uint32> *DeletedText = NULL);
		bool DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper);
		Block *Split(Transaction *Trans, ssize_t AtOffset);
		bool StripLast(Transaction *Trans, const char *Set = " \t\r\n"); // Strip trailing new line if present..
		bool OnDictionary(Transaction *Trans);
	};

	class HorzRuleBlock : public Block
	{
		GRect Pos;
		bool IsDeleted;

	public:
		HorzRuleBlock(GRichTextPriv *priv);
		HorzRuleBlock(const HorzRuleBlock *Copy);
		~HorzRuleBlock();

		bool IsValid();

		// No state change methods
		const char *GetClass() { return "HorzRuleBlock"; }
		int GetLines();
		bool OffsetToLine(ssize_t Offset, int *ColX, GArray<int> *LineY);
		int LineToOffset(int Line);
		GRect GetPos() { return Pos; }
		void Dump();
		GNamedStyle *GetStyle(ssize_t At = -1);
		void SetStyle(GNamedStyle *s);
		ssize_t Length();
		bool ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		ssize_t GetTextAt(ssize_t Offset, GArray<StyleText*> &t);
		ssize_t CopyAt(ssize_t Offset, ssize_t Chars, GArray<uint32> *Text);
		bool Seek(SeekType To, BlockCursor &Cursor);
		ssize_t FindAt(ssize_t StartIdx, const uint32 *Str, GFindReplaceCommon *Params);
		void IncAllStyleRefs();
		void SetSpellingErrors(GArray<GSpellCheck::SpellingError> &Errors);
		bool DoContext(GSubMenu &s, GdcPt2 Doc, ssize_t Offset, bool Spelling);
		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif
		Block *Clone();

		// Events
		GMessage::Result OnEvent(GMessage *Msg);

		// Transactional changes
		bool AddText(Transaction *Trans, ssize_t AtOffset, const uint32 *Str, ssize_t Chars = -1, GNamedStyle *Style = NULL);
		bool ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, GCss *Style, bool Add);
		ssize_t DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, GArray<uint32> *DeletedText = NULL);
		bool DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper);
		Block *Split(Transaction *Trans, ssize_t AtOffset);
	};

	class ImageBlock :
		public Block
	{
	public:
		struct ScaleInf
		{
			GdcPt2 Sz;
			GString MimeType;
			GAutoPtr<GStreamI> Compressed;
			int Percent;

			ScaleInf()
			{
				Sz.x = Sz.y = 0;
				Percent = 0;
			}
		};

		int ThreadHnd;

	protected:
		GNamedStyle *Style;
		int Scale;
		GRect SourceValid;
		GAutoString FileMimeType;

		GArray<ScaleInf> Scales;
		int ResizeIdx;
		int ThreadBusy;
		bool IsDeleted;
		void UpdateThreadBusy(const char *File, int Line, int Off);

		int GetThreadHandle();
		void UpdateDisplay(int y);
		void UpdateDisplayImg();
		

	public:
		GAutoPtr<GSurface> SourceImg, DisplayImg, SelectImg;
		GRect Margin, Border, Padding;
		GString Source;
		GdcPt2 Size;
		
		bool LayoutDirty;
		GRect Pos; // position in document co-ordinates
		GRect ImgPos;
		
		ImageBlock(GRichTextPriv *priv);
		ImageBlock(const ImageBlock *Copy);
		~ImageBlock();

		bool IsValid();
		bool IsBusy(bool Stop = false);
		bool Load(const char *Src = NULL);
		bool SetImage(GAutoPtr<GSurface> Img);

		// No state change methods
		int GetLines();
		bool OffsetToLine(ssize_t Offset, int *ColX, GArray<int> *LineY);
		int LineToOffset(int Line);
		GRect GetPos() { return Pos; }
		void Dump();
		GNamedStyle *GetStyle(ssize_t At = -1);
		void SetStyle(GNamedStyle *s);
		ssize_t Length();
		bool ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		ssize_t GetTextAt(ssize_t Offset, GArray<StyleText*> &t);
		ssize_t CopyAt(ssize_t Offset, ssize_t Chars, GArray<uint32> *Text);
		bool Seek(SeekType To, BlockCursor &Cursor);
		ssize_t FindAt(ssize_t StartIdx, const uint32 *Str, GFindReplaceCommon *Params);
		void IncAllStyleRefs();
		bool DoContext(GSubMenu &s, GdcPt2 Doc, ssize_t Offset, bool Spelling);
		#ifdef _DEBUG
		void DumpNodes(GTreeItem *Ti);
		#endif
		Block *Clone();
		void OnComponentInstall(GString Name);

		// Events
		GMessage::Result OnEvent(GMessage *Msg);

		// Transactional changes
		bool AddText(Transaction *Trans, ssize_t AtOffset, const uint32 *Str, ssize_t Chars = -1, GNamedStyle *Style = NULL);
		bool ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, GCss *Style, bool Add);
		ssize_t DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, GArray<uint32> *DeletedText = NULL);
		bool DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper);
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
	ssize_t IndexOfCursor(BlockCursor *c);
	ssize_t HitTest(int x, int y, int &LineHint, Block **Blk = NULL);
	bool CursorFromPos(int x, int y, GAutoPtr<BlockCursor> *Cursor, ssize_t *GlobalIdx);
	Block *GetBlockByIndex(ssize_t Index, ssize_t *Offset = NULL, int *BlockIdx = NULL, int *LineCount = NULL);
	bool Layout(GScrollBar *&ScrollY);
	void OnStyleChange(GRichTextEdit::RectType t);
	bool ChangeSelectionStyle(GCss *Style, bool Add);
	void PaintBtn(GSurface *pDC, GRichTextEdit::RectType t);
	bool MakeLink(TextBlock *tb, ssize_t Offset, ssize_t Len, GString Link);
	bool ClickBtn(GMouse &m, GRichTextEdit::RectType t);
	bool InsertHorzRule();
	void Paint(GSurface *pDC, GScrollBar *&ScrollY);
	GHtmlElement *CreateElement(GHtmlElement *Parent);
	GdcPt2 ScreenToDoc(int x, int y);
	GdcPt2 DocToScreen(int x, int y);
	bool Merge(Transaction *Trans, Block *a, Block *b);
	bool DeleteSelection(Transaction *t, char16 **Cut);
	GRichTextEdit::RectType PosToButton(GMouse &m);
	void OnComponentInstall(GString Name);

	struct CreateContext
	{
		TextBlock *Tb;
		ImageBlock *Ib;
		HorzRuleBlock *Hrb;
		GArray<uint32> Buf;
		char16 LastChar;
		GFontCache *FontCache;
		GCss::Store StyleStore;
		bool StartOfLine;
		
		CreateContext(GFontCache *fc)
		{
			Tb = NULL;
			Ib = NULL;
			Hrb = NULL;
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

	bool ToHtml(GArray<GDocView::ContentMedia> *Media = NULL);
	void DumpBlocks();
	bool FromHtml(GHtmlElement *e, CreateContext &ctx, GCss *ParentStyle = NULL, int Depth = 0);

	#ifdef _DEBUG
	void DumpNodes(GTree *Root);
	#endif
};

struct BlockCursorState
{
	bool Cursor;
	ssize_t Offset;
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

struct MultiBlockState : public GRichTextPriv::DocChange
{
	GRichTextPriv *Ctx;
	ssize_t Index; // Number of blocks before the edit
	ssize_t Length; // Of the other version currently in the Ctx stack
	GArray<GRichTextPriv::Block*> Blks;
	
	MultiBlockState(GRichTextPriv *ctx, ssize_t Start);
	bool Apply(GRichTextPriv *Ctx, bool Forward);

	bool Copy(ssize_t Idx);
	bool Cut(ssize_t Idx);
};

#ifdef _DEBUG
GTreeItem *PrintNode(GTreeItem *Parent, const char *Fmt, ...);
#endif

typedef GRichTextPriv::BlockCursor BlkCursor;
typedef GAutoPtr<GRichTextPriv::BlockCursor> AutoCursor;
typedef GAutoPtr<GRichTextPriv::Transaction> AutoTrans;

#endif