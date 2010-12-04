/// \file
/// \author Matthew Allen
/// \brief A unicode text editor

#ifndef __GTEXTVIEW3_H
#define __GTEXTVIEW3_H

#include "GDocView.h"
#include "GUndo.h"
#include "GDragAndDrop.h"

// use CRLF as opposed to just LF
// internally it uses LF only... this is just to remember what to
// save out as.
#define TEXTED_USES_CR				0x00000001
#define TAB_SIZE					4
#define DEBUG_TIMES_MSG				8000 // a=0 b=(char*)Str

#define M_TEXTVIEW_DEBUG_TEXT		(M_USER+0x3421)

extern char Delimiters[];

class GTextView3;

/// Unicode text editor control.
class
#ifdef MAC
LgiClass
#endif
	GTextView3 :
	public GDocView,
	public ResObject,
	public GDragDropTarget
{
	friend class GUrl;
	friend class GTextView3Undo;
	friend bool Text_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User);

public:
	class GStyle
	{
		friend class GUrl;

	protected:
		void RefreshLayout(int Start, int Len);

	public:
		enum StyleDecor
		{
			DecorNone,
			DecorSquiggle,
		};

		/// The view the style is for
		GTextView3 *View;
		/// When you write several bits of code to do styling assign them
		/// different owner id's so that they can manage the lifespan of their
		/// own styles. GTextView3::PourStyle is owner '0', anything else it
		/// will leave alone.
		int Owner;
		/// The start index into the text buffer of the region to style.
		int Start;
		/// The length of the styled region
		int Len;
		/// The font to draw the styled text in
		GFont *Font;
		/// The colour to draw with (24 bit)
		COLOUR c;
		/// Optional extra decor not supported by the fonts
		StyleDecor Decor;
		/// Colour for the optional decor.
		COLOUR DecorColour;

		/// Application base data
		char *Data;

		GStyle(int owner)
		{
			Owner = owner;
			View = 0;
			c = 0;
			Font = 0;
			Start = -1;
			Len = 0;
			Decor = DecorNone;
			DecorColour = 0;
			Data = 0;
		}

		virtual bool OnMouseClick(GMouse *m) { return false; }
		virtual bool OnMenu(GSubMenu *m) { return false; }
		virtual void OnMenuClick(int i) {}
		virtual TCHAR *GetCursor() { return 0; }

		/// Returns true if this style overlaps the position of 's'
		bool Overlap(GStyle *s)
		{
			return Overlap(s->Start, s->Len);
		}

		/// Returns true if this style overlaps the position of 's'
		bool Overlap(int sStart, int sLen)
		{
			if (sStart + sLen < Start ||
				sStart >= Start + Len)
				return false;

			return true;
		}
	};

	friend class GTextView3::GStyle;

protected:
	// Internal classes
	enum GTextViewSeek
	{
		PrevLine,
		NextLine,
		StartLine,
 		EndLine
	};

	class GTextLine
	{
	public:
		int Start;		// Start offset
		int Len;		// length of text
		GRect r;		// Screen location
		COLOUR Col;		// Colour of line

		GTextLine()
		{
			Col = 0x80000000;
		}
		virtual ~GTextLine() {}
		bool Overlap(int i)
		{
			return i>=Start AND i<=Start+Len;
		}
	};
	
	class GTextView3Private *d;
	friend class GTextView3Private;

	// Options
	bool Dirty;
	bool CanScrollX;

	// Display
	GFont *Font;
	GFont *FixedFont;
	GFont *Underline;	// URL display
	int LineY;
	int SelStart, SelEnd;
	int DocOffset;
	int MaxX;
	bool Blink;
	int ScrollX;
	GRect CursorPos;

	List<GTextLine> Line;
	List<GStyle> Style;		// sorted in 'Start' order

	// For ::Name(...)
	char *TextCache;

	// Data
	char16 *Text;
	int Cursor;
	int Size;
	int Alloc;

	// Undo stuff
	bool UndoOn;
	GUndo UndoQue;

	// private methods
	GTextLine *GetLine(int Offset, int *Index = 0);
	int SeekLine(int Start, GTextViewSeek Where);
	int TextWidth(GFont *f, char16 *s, int Len, int x, int Origin);
	int ScrollYLine();
	int ScrollYPixel();
	int MatchText(char16 *Text, bool MatchWord, bool MatchCase, bool SelectionOnly);
	
	// styles
	bool InsertStyle(GStyle *s);
	GStyle *GetNextStyle(int Where = -1);
	GStyle *HitStyle(int i);
	int GetColumn();

	// Overridables
	virtual void PourText(int Start, int Length);
	virtual void PourStyle(int Start, int Length);
	virtual void OnFontChange();
	virtual char16 *MapText(char16 *Str, int Len, bool RtlTrailingSpace = false);

	#ifdef _DEBUG
	// debug
	int _PourTime;
	int _StyleTime;
	int _PaintTime;
	#endif

public:
	// Construction
	GTextView3(	int Id,
				int x,
				int y,
				int cx,
				int cy,
				GFontType *FontInfo = 0);
	~GTextView3();

	char *GetClass() { return "GTextView3"; }

	// Data
	char *Name();
	bool Name(char *s);
	char16 *NameW();
	bool NameW(char16 *s);
	int64 Value();
	void Value(int64 i);
	char *GetMimeType() { return "text/plain"; }
	int GetSize() { return Size; }

	int HitText(int x, int y);
	void DeleteSelection(char16 **Cut = 0);

	// Font
	GFont *GetFont();
	void SetFont(GFont *f, bool OwnIt = false);
	void SetFixedWidthFont(bool i);

	// Options
	void SetTabSize(uint8 i);
	void SetBorder(int b);
	void SetReadOnly(bool i);

	/// Sets the wrapping on the control, use #TEXTED_WRAP_NONE or #TEXTED_WRAP_REFLOW
	void SetWrapType(uint8 i);
	
	// Margin
	GRect &GetMargin();
	void SetMargin(GRect &r);

	// State / Selection
	void SetCursor(int i, bool Select, bool ForceFullUpdate = false);
	int IndexAt(int x, int y);
	bool IsDirty() { return Dirty; }
	void IsDirty(bool d) { Dirty = d; }
	bool HasSelection();
	void UnSelectAll();
	void SelectWord(int From);
	void SelectAll();
	int GetCursor(bool Cursor = true);
	void PositionAt(int &x, int &y, int Index = -1);
	int GetLines();
	void GetTextExtent(int &x, int &y);
	char *GetSelection();

	// File IO
	bool Open(char *Name, char *Cs = 0);
	bool Save(char *Name, char *Cs = 0);

	// Clipboard IO
	bool Cut();
	bool Copy();
	bool Paste();

	// Undo/Redo
	void Undo();
	void Redo();
	bool GetUndoOn() { return UndoOn; }
	void SetUndoOn(bool b) { UndoOn = b; }

	// Action UI
	virtual bool DoGoto();
	virtual bool DoCase(bool Upper);
	virtual bool DoFind();
	virtual bool DoFindNext();
	virtual bool DoReplace();

	// Action Processing	
	bool ClearDirty(bool Ask, char *FileName = 0);
	void UpdateScrollBars(bool Reset = false);
	void GotoLine(int Line);
	GDocFindReplaceParams *CreateFindReplaceParams();
	void SetFindReplaceParams(GDocFindReplaceParams *Params);

	// Object Events
	bool OnFind(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly);
	bool OnReplace(char16 *Find, char16 *Replace, bool All, bool MatchWord, bool MatchCase, bool SelectionOnly);
	bool OnMultiLineTab(bool In);
	void OnSetHidden(int Hidden);
	void OnPosChange();
	void OnCreate();
	void OnEscape(GKey &K);
	void OnMouseWheel(double Lines);

	// Window Events
	void OnFocus(bool f);
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	bool OnKey(GKey &k);
	void OnPaint(GSurface *pDC);
	int OnEvent(GMessage *Msg);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPulse();
	int OnHitTest(int x, int y);
	bool OnLayout(GViewLayoutInfo &Inf);
	int WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState);
	int OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState);

	// Virtuals
	virtual bool Insert(int At, char16 *Data, int Len);
	virtual bool Delete(int At, int Len);
	virtual void OnEnter(GKey &k);
	virtual void OnUrl(char *Url);
};

#endif
