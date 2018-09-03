/// \file
/// \author Matthew Allen
/// \brief A unicode text editor

#ifndef __GTEXTVIEW3_H
#define __GTEXTVIEW3_H

#include "GDocView.h"
#include "GUndo.h"
#include "GDragAndDrop.h"
#include "GCss.h"
#include "LUnrolledList.h"

// use CRLF as opposed to just LF
// internally it uses LF only... this is just to remember what to
// save out as.
#define TEXTED_USES_CR				0x00000001
#define TAB_SIZE					4
#define DEBUG_TIMES_MSG				8000 // a=0 b=(char*)Str

enum GTextView3Messages
{
	M_TEXTVIEW_DEBUG_TEXT = M_USER + 0x3421,
	M_TEXTVIEW_FIND,
	M_TEXTVIEW_REPLACE,
	M_TEXT_POUR_CONTINUE,
};

extern char Delimiters[];

class GTextView3;

enum GTextViewStyleOwners
{
	STYLE_NONE,
	STYLE_IDE,
	STYLE_SPELLING,
	STYLE_FIND_MATCHES,
	STYLE_ADDRESS,
	STYLE_URL,
};

/// Unicode text editor control.
class LgiClass
	GTextView3 :
	public GDocView,
	public ResObject,
	public GDragDropTarget
{
	friend class GTextView3Undo;
	friend bool Text3_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User);

public:
	class GStyle
	{
	protected:
		void RefreshLayout(size_t Start, ssize_t Len);

	public:
		/// The view the style is for
		GTextView3 *View;
		/// When you write several bits of code to do styling assign them
		/// different owner id's so that they can manage the lifespan of their
		/// own styles. GTextView3::PourStyle is owner '0', anything else it
		/// will leave alone.
		GTextViewStyleOwners Owner;
		/// The start index into the text buffer of the region to style.
		ssize_t Start;
		/// The length of the styled region
		ssize_t Len;
		/// The font to draw the styled text in
		GFont *Font;
		/// The colour to draw with. If transparent, then the default 
		/// line colour is used.
		GColour Fore, Back;
		/// Cursor
		LgiCursor Cursor;		
		/// Optional extra decor not supported by the fonts
		GCss::TextDecorType Decor;
		/// Colour for the optional decor.
		GColour DecorColour;

		/// Application base data
		GVariant Data;

		GStyle(GTextViewStyleOwners owner = STYLE_NONE)
		{
			Owner = owner;
			View = NULL;
			Font = NULL;
			Empty();
			Cursor = LCUR_Normal;
			Decor = GCss::TextDecorNone;
		}

		GStyle(const GStyle &s)
		{
			Owner = s.Owner;
			View = s.View;
			Font = s.Font;
			Start = s.Start;
			Len = s.Len;
			Decor = s.Decor;
			DecorColour = s.DecorColour;
			Fore = s.Fore;
			Back = s.Back;
			Data = s.Data;
		}
		
		GStyle &Construct(GTextView3 *view, GTextViewStyleOwners owner)
		{
			View = view;
			Owner = owner;
			Font = NULL;
			Empty();
			Cursor = LCUR_Normal;
			Decor = GCss::TextDecorNone;
			return *this;
		}

		void Empty()
		{			
			Start = -1;
			Len = 0;
		}

		bool Valid()
		{
			return Start >= 0 && Len > 0;
		}

		/*
		virtual ~GStyle() {}

		virtual bool OnMouseClick(GMouse *m) { return false; }
		virtual bool OnMenu(GSubMenu *m) { return false; }
		virtual void OnMenuClick(int i) {}
		virtual CURSOR_CHAR GetCursor()  { return 0; }
		*/

		size_t End() const { return Start + Len; }

		/// \returns true if style is the same
		bool operator ==(const GStyle &s)
		{
			return	Owner == s.Owner &&
					Start == s.Start &&
					Len == s.Len &&
					Fore == s.Fore &&
					Back == s.Back &&
					Decor == s.Decor;
		}

		/// Returns true if this style overlaps the position of 's'
		bool Overlap(GStyle &s)
		{
			return Overlap(s.Start, s.Len);
		}

		/// Returns true if this style overlaps the position of 's'
		bool Overlap(ssize_t sStart, ssize_t sLen)
		{
			if (sStart + sLen - 1 < Start ||
				sStart >= Start + Len)
				return false;

			return true;
		}

		void Union(const GStyle &s)
		{
			if (Start < 0)
			{
				Start = s.Start;
				Len = s.Len;
			}
			else
			{
				Start = MIN(Start, s.Start);
				Len = MAX(End(), s.End()) - Start;
			}
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
		ssize_t Start;	// Start offset
		ssize_t Len;	// length of text
		GRect r;		// Screen location
		GColour c;		// Colour of line... transparent = default colour
		GColour Back;	// Background colour or transparent

		GTextLine()
		{
			Start = -1;
			Len = 0;
			r.ZOff(-1, -1);
		}
		virtual ~GTextLine() {}
		bool Overlap(ssize_t i)
		{
			return i>=Start && i<=Start+Len;
		}

		size_t CalcLen(char16 *Text)
		{
			char16 *c = Text + Start, *e = c;
			while (*e && *e != '\n')
				e++;
			return Len = e - c;
		}
	};
	
	class GTextView3Private *d;
	friend class GTextView3Private;

	// Options
	bool Dirty;
	bool CanScrollX;
	bool PourEnabled;

	// Display
	GFont *Font;
	GFont *Bold;		// Bold variant of 'Font'
	GFont *Underline;	// Underline variant of 'Font'

	GFont *FixedFont;
	int LineY;
	ssize_t SelStart, SelEnd;
	int DocOffset;
	int MaxX;
	bool Blink;
	uint64 BlinkTs;
	int ScrollX;
	GRect CursorPos;

	/// true if the text pour process is still ongoing
	bool PartialPour;

	List<GTextLine> Line;
	LUnrolledList<GStyle> Style;		// sorted in 'Start' order
	typedef LUnrolledList<GStyle>::Iter StyleIter;

	// For ::Name(...)
	char *TextCache;

	// Data
	char16 *Text;
	ssize_t Cursor;
	ssize_t Size;
	ssize_t Alloc;

	// Undo stuff
	bool UndoOn;
	GUndo UndoQue;

	// private methods
	GTextLine *GetTextLine(ssize_t Offset, ssize_t *Index = 0);
	ssize_t SeekLine(ssize_t Offset, GTextViewSeek Where);
	int TextWidth(GFont *f, char16 *s, int Len, int x, int Origin);
	int ScrollYLine();
	int ScrollYPixel();
	GRect DocToScreen(GRect r);
	ptrdiff_t MatchText(char16 *Text, bool MatchWord, bool MatchCase, bool SelectionOnly, bool SearchUpwards);
	
	// styles
	bool InsertStyle(GAutoPtr<GStyle> s);
	GStyle *GetNextStyle(StyleIter &it, ssize_t Where = -1);
	GStyle *HitStyle(ssize_t i);
	int GetColumn();
	int SpaceDepth(char16 *Start, char16 *End);

	// Overridables
	virtual void PourText(size_t Start, ssize_t Length);
	virtual void PourStyle(size_t Start, ssize_t Length);
	virtual void OnFontChange();
	virtual void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour);
	virtual char16 *MapText(char16 *Str, ssize_t Len, bool RtlTrailingSpace = false);

	#ifdef _DEBUG
	// debug
	uint64 _PourTime;
	uint64 _StyleTime;
	uint64 _PaintTime;
	#endif

	void LogLines();
	bool ValidateLines(bool CheckBox = false);

public:
	// Construction
	GTextView3(	int Id,
				int x,
				int y,
				int cx,
				int cy,
				GFontType *FontInfo = 0);
	~GTextView3();

	const char *GetClass() { return "GTextView3"; }

	// Data
	char *Name();
	bool Name(const char *s);
	char16 *NameW();
	bool NameW(const char16 *s);
	int64 Value();
	void Value(int64 i);
	const char *GetMimeType() { return "text/plain"; }
	size_t GetSize() { return Size; }
	GString operator[](int LineIdx);

	ssize_t HitText(int x, int y, bool Nearest);
	void DeleteSelection(char16 **Cut = 0);

	// Font
	GFont *GetFont();
	GFont *GetBold();
	void SetFont(GFont *f, bool OwnIt = false);
	void SetFixedWidthFont(bool i);

	// Options
	void SetTabSize(uint8 i);
	void SetBorder(int b);
	void SetReadOnly(bool i);
	void SetCrLf(bool crlf);

	/// Sets the wrapping on the control, use #TEXTED_WRAP_NONE or #TEXTED_WRAP_REFLOW
	void SetWrapType(LDocWrapType i);
	
	// State / Selection
	ssize_t GetCaret(bool Cursor = true);
	virtual void SetCaret(size_t i, bool Select, bool ForceFullUpdate = false);
	ssize_t IndexAt(int x, int y);
	bool IsDirty() { return Dirty; }
	void IsDirty(bool d) { Dirty = d; }
	bool HasSelection();
	void UnSelectAll();
	void SelectWord(size_t From);
	void SelectAll();
	bool GetLineColumnAtIndex(GdcPt2 &Pt, int Index = -1);
	size_t GetLines();
	void GetTextExtent(int &x, int &y);
	char *GetSelection();
	GRange GetSelectionRange();

	// File IO
	bool Open(const char *Name, const char *Cs = 0);
	bool Save(const char *Name, const char *Cs = 0);
	const char *GetLastError();

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
	ssize_t GetLine();
	void SetLine(int Line);
	GDocFindReplaceParams *CreateFindReplaceParams();
	void SetFindReplaceParams(GDocFindReplaceParams *Params);

	// Object Events
	bool OnFind(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly, bool SearchUpwards);
	bool OnReplace(char16 *Find, char16 *Replace, bool All, bool MatchWord, bool MatchCase, bool SelectionOnly, bool SearchUpwards);
	bool OnMultiLineTab(bool In);
	void OnSetHidden(int Hidden);
	void OnPosChange();
	void OnCreate();
	void OnEscape(GKey &K);
	bool OnMouseWheel(double Lines);

	// Window Events
	void OnFocus(bool f);
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	bool OnKey(GKey &k);
	void OnPaint(GSurface *pDC);
	GMessage::Result OnEvent(GMessage *Msg);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPulse();
	int OnHitTest(int x, int y);
	bool OnLayout(GViewLayoutInfo &Inf);
	int WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState);
	int OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState);
	LgiCursor GetCursor(int x, int y);

	// Virtuals
	virtual bool Insert(size_t At, char16 *Data, ssize_t Len);
	virtual bool Delete(size_t At, ssize_t Len);
	virtual void OnEnter(GKey &k);
	virtual void OnUrl(char *Url);
	virtual void DoContextMenu(GMouse &m);
	virtual bool OnStyleClick(GStyle *style, GMouse *m);
	virtual bool OnStyleMenu(GStyle *style, GSubMenu *m);
	virtual void OnStyleMenuClick(GStyle *style, int i);
};

#endif
