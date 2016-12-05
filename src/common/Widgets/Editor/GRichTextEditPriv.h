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

#define DEBUG_LOG_CURSOR_COUNT		0

#define TEXT_LINK					"Link"

#define IsWordBreakChar(ch)			IsWhiteSpace(ch) // FIXME: Add asian character set support to this

//////////////////////////////////////////////////////////////////////
#define PtrCheckBreak(ptr)			if (!ptr) { LgiAssert(!"Invalid ptr"); break; }
#undef FixedToInt
#define FixedToInt(fixed)			((fixed)>>GDisplayString::FShift)
#undef IntToFixed
#define IntToFixed(val)				((val)<<GDisplayString::FShift)

#define CursorColour				GColour(0, 0, 0)

//////////////////////////////////////////////////////////////////////
class GRichEditElem : public GHtmlElement
{
	GString Style;
	GString Classes;

public:
	GRichEditElem(GHtmlElement *parent) : GHtmlElement(parent)
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
		
	}
};

struct GRichEditElemContext : public GCss::ElementCallback<GRichEditElem>
{
	/// Returns the element name
	const char *GetElement(GRichEditElem *obj)
	{
		return obj->Tag;
	}
	
	/// Returns the document unque element ID
	const char *GetAttr(GRichEditElem *obj, const char *Attr)
	{
		const char *a = NULL;
		obj->Get(Attr, a);
		return a;
	}
	
	/// Returns the class
	bool GetClasses(GArray<const char *> &Classes, GRichEditElem *obj)
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
	GRichEditElem *GetParent(GRichEditElem *obj)
	{
		return dynamic_cast<GRichEditElem*>(obj->Parent);
	}

	/// Returns an array of child objects
	GArray<GRichEditElem*> GetChildren(GRichEditElem *obj)
	{
		GArray<GRichEditElem*> a;
		for (unsigned i=0; i<obj->Children.Length(); i++)
			a.Add(dynamic_cast<GRichEditElem*>(obj->Children[i]));
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
	bool ShowTools;
	GRect Areas[GRichTextEdit::MaxArea];
	GVariant Values[GRichTextEdit::MaxArea];

	// Constructor
	GRichTextPriv(GRichTextEdit *view) :
		GHtmlParser(view),
		GFontCache(SysFont)
	{
		View = view;
		WordSelectMode = false;
		Dirty = false;
		ShowTools = true;
		for (unsigned i=0; i<CountOf(Areas); i++)
		{
			Areas[i].ZOff(-1, -1);
		}

		Values[GRichTextEdit::FontFamilyBtn] = "FontName";
		Values[GRichTextEdit::FontSizeBtn] = "14";

		Values[GRichTextEdit::BoldBtn] = true;
		Values[GRichTextEdit::ItalicBtn] = false;
		Values[GRichTextEdit::UnderlineBtn] = false;
		
		Values[GRichTextEdit::ForegroundColourBtn] = (int64)Rgb24To32(LC_TEXT);
		Values[GRichTextEdit::BackgroundColourBtn] = (int64)Rgb24To32(LC_WORKSPACE);

		Values[GRichTextEdit::MakeLinkBtn] = TEXT_LINK;

		Padding(GCss::Len(GCss::LenPx, 4));

		EmptyDoc();
	}
	
	~GRichTextPriv()
	{
		Empty();
	}
	
	bool Error(const char *file, int line, const char *fmt, ...)
	{
		va_list Arg;
		va_start(Arg, fmt);
		GString s;
		LgiPrintf(s, fmt, Arg);
		va_end(Arg);
		LgiTrace("%s:%i - Error: %s\n", file, line, s.Get());
		
		LgiAssert(0);
		return false;
	}

	struct Flow
	{
		GRichTextPriv *d;
		GSurface *pDC;	// Used for printing.

		int Left, Right;// Left and right margin positions as measured in px
						// from the left of the page (controls client area).
		int CurY;		// Current y position down the page in document co-ords
		bool Visible;	// true if the current block overlaps the visible page
						// If false, the implementation can take short cuts and
						// guess various dimensions.
	
		Flow(GRichTextPriv *priv)
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

	/// This is a run of text, all of the same style
	class StyleText : public GArray<char16>
	{
		GNamedStyle *Style; // owned by the CSS cache
	
	public:
		ColourPair Colours;
		GColour Fore, Back;
		
		StyleText(const char16 *t = NULL, int Chars = -1, GNamedStyle *style = NULL)
		{
			Style = NULL;
			if (style)
				SetStyle(style);
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
			/// [In] The initial offset.
			int Offset,
			/// [In] The x position hint
			int XPos
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

		BlockCursor(const BlockCursor &c);
		BlockCursor(Block *b, int off);
		~BlockCursor();
		
		BlockCursor &operator =(const BlockCursor &c);
		void Set(int off);
		void Set(Block *b, int off);
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
		GArray<StyleText*> Txt;
		GArray<TextLine*> Layout;
		GRect Margin, Border, Padding;
		GFont *Fnt;
		
		bool LayoutDirty;
		int Len; // chars in the whole block (sum of all Text lengths)
		GRect Pos; // position in document co-ordinates
		
		TextBlock();
		~TextBlock();

		bool IsValid();

		void Dump();
		GNamedStyle *GetStyle();		
		void SetStyle(GNamedStyle *s);
		int Length();
		bool ToHtml(GStream &s);
		bool GetPosFromIndex(GRect *CursorPos, GRect *LinePos, int Index);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		StyleText *GetTextAt(uint32 Offset);
		int DeleteAt(int BlkOffset, int Chars, GArray<char16> *DeletedText = NULL);
		int CopyAt(int Offset, int Chars, GArray<char16> *Text);
		bool AddText(int AtOffset, const char16 *Str, int Chars = -1, GNamedStyle *Style = NULL);
		int Seek(SeekType To, int Offset, int XPos);
	};
	
	GArray<Block*> Blocks;

	void EmptyDoc()
	{
		Block *Def = new TextBlock();
		if (Def)
		{			
			Blocks.Add(Def);
			Cursor.Reset(new BlockCursor(Def, 0));
		}
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
				if (c->Offset > 0)
				{
					GArray<char16> a;
					c->Blk->CopyAt(0, c->Offset, &a);
					
					int i = c->Offset;
					while (i > 0 && IsWordBreakChar(a[i-1]))
						i--;
					while (i > 0 && !IsWordBreakChar(a[i-1]))
						i--;

					c->Offset = i;
					Status = true;
				}
				else // Seek into previous block?
				{
				}
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
				if (c->Offset < c->Blk->Length())
				{
					GArray<char16> a;
					int RemainingCh = c->Blk->Length() - c->Offset;
					c->Blk->CopyAt(c->Offset, RemainingCh, &a);
					
					int i = 0;
					while (i < RemainingCh && !IsWordBreakChar(a[i]))
						i++;
					while (i < RemainingCh && IsWordBreakChar(a[i]))
						i++;

					c->Offset += i;
					Status = true;
				}
				else // Seek into next block?
				{
				}
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
	
	int HitTest(int x, int y, bool Click)
	{
		int CharPos = 0;
		HitTestResult r(x, y);

		if (Click)
		{
			int asd=0;
		}
		
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
			int Len = b->Length();

			if (Index >= CharPos &&
				Index <= CharPos + Len)
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
			Block *b = Blocks[i];
			b->OnLayout(f);
		}
		
		if (Cursor)
		{
			LgiAssert(Cursor->Blk != NULL);
			if (Cursor->Blk)
			{
				Cursor->Blk->GetPosFromIndex(&Cursor->Pos,
											 &Cursor->Line,
											 Cursor->Offset);
				
				// LgiTrace("%s:%i - Cursor->Pos=%s\n", _FL, Cursor->Pos.GetStr());
			}
		}
	}

	void OnStyleChange(GRichTextEdit::RectType t)
	{
	}

	void PaintBtn(GSurface *pDC, GRichTextEdit::RectType t)
	{
		GRect r = Areas[t];
		GVariant &v = Values[t];
		bool Down = v.Type == GV_BOOL && v.Value.Bool;

		LgiThinBorder(pDC, r, Down ? EdgeXpSunken : EdgeXpRaised);
		switch (v.Type)
		{
			case GV_STRING:
			{
				GDisplayString Ds(SysFont, v.Str());
				Ds.Draw(pDC, r.x1 + ((r.X()-Ds.X())>>1), r.y1 + ((r.Y()-Ds.Y())>>1), &r);
				break;
			}
			case GV_INT64:
			{
				pDC->Colour((uint32)v.Value.Int64, 32);
				pDC->Rectangle(&r);
				break;
			}
			case GV_BOOL:
			{
				const char *Label = NULL;
				switch (t)
				{
					case GRichTextEdit::BoldBtn: Label = "B"; break;
					case GRichTextEdit::ItalicBtn: Label = "I"; break;
					case GRichTextEdit::UnderlineBtn: Label = "U"; break;
				}
				if (!Label) break;
				GDisplayString Ds(SysFont, Label);
				Ds.Draw(pDC, r.x1 + ((r.X()-Ds.X())>>1) + Down, r.y1 + ((r.Y()-Ds.Y())>>1) + Down, &r);
				break;
			}
		}
	}

	void ClickBtn(GMouse &m, GRichTextEdit::RectType t)
	{
		switch (t)
		{
			case GRichTextEdit::FontFamilyBtn:
			{
				List<const char> Fonts;
				if (!GFontSystem::Inst()->EnumerateFonts(Fonts))
				{
					LgiTrace("%s:%i - EnumerateFonts failed.\n", _FL);
					break;
				}

				bool UseSub = (SysFont->GetHeight() * Fonts.Length()) > (GdcD->Y() * 0.8);

				GSubMenu s;
				GSubMenu *Cur = NULL;
				int Idx = 1;
				char Last = 0;
				for (const char *f = Fonts.First(); f; )
				{
					if (*f == '@')
					{
						Fonts.Delete(f);
						DeleteArray(f);
						f = Fonts.Current();
					}
					else if (UseSub)
					{
						if (*f != Last || Cur == NULL)
						{
							GString str;
							str.Printf("%c...", Last = *f);
							Cur = s.AppendSub(str);
						}
						if (Cur)
							Cur->AppendItem(f, Idx++);
						else
							break;
						f = Fonts.Next();
					}
					else
					{
						s.AppendItem(f, Idx++);
						f = Fonts.Next();
					}
				}

				GdcPt2 p(Areas[t].x1, Areas[t].y2 + 1);
				View->PointToScreen(p);
				int Result = s.Float(View, p.x, p.y, true);
				if (Result)
				{
					Values[t] = Fonts[Result-1];
					View->Invalidate(Areas+t);
					OnStyleChange(t);
				}
				break;
			}
			case GRichTextEdit::FontSizeBtn:
			{
				static const char *Sizes[] = { "6", "7", "8", "9", "10", "11", "12", "14", "16", "18", "20", "24",
												"28", "32", "40", "50", "60", "80", "100", "120", 0 };
				GSubMenu s;
				for (int Idx = 0; Sizes[Idx]; Idx++)
					s.AppendItem(Sizes[Idx], Idx+1);

				GdcPt2 p(Areas[t].x1, Areas[t].y2 + 1);
				View->PointToScreen(p);
				int Result = s.Float(View, p.x, p.y, true);
				if (Result)
				{
					Values[t] = Sizes[Result-1];
					View->Invalidate(Areas+t);
					OnStyleChange(t);
				}
				break;
			}
			case GRichTextEdit::BoldBtn:
			case GRichTextEdit::ItalicBtn:
			case GRichTextEdit::UnderlineBtn:
			{
				Values[t] = !Values[t].CastBool();
				View->Invalidate(Areas+t);
				OnStyleChange(t);
			}
			case GRichTextEdit::ForegroundColourBtn:
			case GRichTextEdit::BackgroundColourBtn:
			{
				GdcPt2 p(Areas[t].x1, Areas[t].y2 + 1);
				View->PointToScreen(p);
				new SelectColour(this, p, t);
				break;
			}
			case GRichTextEdit::MakeLinkBtn:
			{
				LgiAssert(!"Impl link dialog.");
				break;
			}
		}
	}
	
	void Paint(GSurface *pDC)
	{
		if (Areas[GRichTextEdit::ToolsArea].Valid())
		{
			// Draw tools area...
			GRect &t = Areas[GRichTextEdit::ToolsArea];
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(&t);

			GRect r = t;
			r.Size(2, 2);
			#define AllocPx(sz, border) \
				GRect(r.x1, r.y1, r.x1 + (int)(sz) - 1, r.y2); r.x1 += (int)(sz) + border

			Areas[GRichTextEdit::FontFamilyBtn] = AllocPx(100, 6);
			Areas[GRichTextEdit::FontSizeBtn] = AllocPx(40, 6);

			Areas[GRichTextEdit::BoldBtn] = AllocPx(r.Y(), 0);
			Areas[GRichTextEdit::ItalicBtn] = AllocPx(r.Y(), 0);
			Areas[GRichTextEdit::UnderlineBtn] = AllocPx(r.Y(), 6);

			Areas[GRichTextEdit::ForegroundColourBtn] = AllocPx(r.Y()*1.5, 0);
			Areas[GRichTextEdit::BackgroundColourBtn] = AllocPx(r.Y()*1.5, 6);

			GDisplayString Ds(SysFont, TEXT_LINK);
			Areas[GRichTextEdit::MakeLinkBtn] = AllocPx(Ds.X() + 12, 6);

			for (unsigned i = GRichTextEdit::FontFamilyBtn; i <= GRichTextEdit::MakeLinkBtn; i++)
			{
				PaintBtn(pDC, (GRichTextEdit::RectType) i);
			}
		}

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
		return new GRichEditElem(Parent);
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
				Tb->AddText(-1, &Buf[0], Used, Style);
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
				GRichEditElemContext StyleCtx;
				if (ctx.StyleStore.Match(Selectors, &StyleCtx, dynamic_cast<GRichEditElem*>(c)))
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
			//LgiTrace("%s%s IsBlock=%i CachedStyle=%p\n", Sp, c->Tag.Get(), IsBlock, CachedStyle);
			
			if ((IsBlock && ctx.LastChar != '\n') || c->TagId == TAG_BR)
			{
				if (!ctx.Tb)
					Blocks.Add(ctx.Tb = new TextBlock);
				if (ctx.Tb)
				{
					ctx.Tb->AddText(-1, L"\n", 1, CachedStyle);
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


#endif