#ifndef __GTEXTVIEW2_H
#define __GTEXTVIEW2_H

#include "GDocView.h"

// Word wrap
#define TEXTED_WRAP_NONE			0	// no word wrap
#define TEXTED_WRAP_REFLOW			1	// dynamically wrap line to editor width

// Code pages
#define TVCP_US_ASCII				0
#define TVCP_ISO_8859_2				1
#define TVCP_WIN_1250				2
#define TVCP_MAX					3

// use CRLF as opposed to just LF
// internally it uses LF only... this is just to remember what to
// save out as.
#define TEXTED_USES_CR				0x00000001
#define TAB_SIZE					4
#define DEBUG_TIMES_MSG				8000 // a=0 b=(char*)Str

extern char Delimiters[];

class GTextView2 : public GDocView
{
	friend class GTextStyle;
	friend class GUrl;

protected:
	// Internal classes
	enum GTextViewSeek
	{
		PrevLine,
		NextLine,
		StartLine,
 		EndLine
	};

	class GTextLine {
	public:
		int Start;			// Start offset
		int Len;			// length of text
		GRect r;			// Screen location

		virtual ~GTextLine() {}
		bool Overlap(int i)
		{
			return i>=Start AND i<=Start+Len;
		}
	};

	class GTextStyle
	{
	public:
		// data
		class GTextView2 *View;
		int Start;
		int Len;
		GFont *Font;
		COLOUR c;

		// attach
		char *Data;

		GTextStyle(class GTextView2 *Tv);

		virtual bool OnMouseClick(GMouse *m) { return false; }
		virtual bool OnMenu(GSubMenu *m) { return false; }
		virtual void OnMenuClick(int i) {}
		virtual char *GetCursor() { return 0; }
	};

	// Options
	int WrapAtCol;
	int WrapType;
	bool UrlDetect;
	bool AcceptEdit;
	bool CrLf;
	bool Dirty;
	bool AutoIndent;
	COLOUR BackColour;

	// Display
	GFont *Font;
	GFont *BlueUnderline;	// URL display
	int LineY;
	int Lines;
	int LeftMargin;
	int SelStart, SelEnd;
	int DocOffset;
	int MaxX;
	bool Blink;
	GRect CursorPos;

	List<GTextLine> Line;
	List<GTextStyle> Style;		// sorted in 'Start' order

	// History
	char *LastFind;
	char *LastReplace;
	bool MatchCase;
	bool MatchWord;

	// Data
	char *Text;
	int Cursor;
	int CharSize;
	int Size;
	int Alloc;

	// private methods
	bool Insert(int At, char *Data, int Len);
	bool Delete(int At, int Len);
	int HitText(int x, int y);
	GTextLine *GetLine(int Offset, int *Index = 0);
	void DeleteSelection(char **Cut = 0);
	int SeekLine(int Start, GTextViewSeek Where);
	int TextWidth(GFont *f, char *s, int Len, int x, int Origin);
	
	// styles
	void InsertStyle(GTextStyle *s);
	GTextStyle *GetNextStyle(int Where = -1);
	GTextStyle *HitStyle(int i);

	// Overridables
	virtual void PourText();
	virtual void PourStyle();
	virtual void OnFontChange();

	#ifdef _DEBUG
	// debug
	int _PourTime;
	int _StyleTime;
	int _PaintTime;
	#endif

public:
	// Construction
	GTextView2(	int Id,
				int x,
				int y,
				int cx,
				int cy,
				GFontType *FontInfo = 0);
	~GTextView2();

	// Data
	char *Name();
	bool Name(const char *s);

	// Font
	GFont *GetFont();
	void SetFont(GFont *f, bool OwnIt = false);

	// Options
	void SetBorder(int b);
	void AcceptEdits(bool i);
	void SetWrapType(uint8 i);

	// State / Selection
	void SetCursor(int i, bool Select, bool ForceFullUpdate = false);
	int IndexAt(int x, int y);
	bool IsDirty() { return Dirty; }
	bool HasSelection();
	void UnSelectAll();
	void SelectWord(int From);
	void SelectAll();
	GdcPt2 GetCursor();
	int GetLines();
	void GetTextExtent(int &x, int &y);

	// File IO
	bool Open(char *Name);
	bool Save(char *Name);

	// Clipboard IO
	bool Cut();
	bool Copy();
	bool Paste();

	// Actions
	bool ClearDirty(bool Ask, char *FileName = 0);
	void UpdateScrollBars(bool Reset = false);
	bool DoFind();
	bool DoReplace();

	// Object Events
	bool OnFind(char *Find, bool MatchWord, bool MatchCase);
	bool OnReplace(char *Find, char *Replace, bool All, bool MatchWord, bool MatchCase);
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

	// Virtuals
	virtual void OnEnter(GKey &k);
	virtual void OnUrl(char *Url);
};

#endif
