#ifndef __GTEXTVIEW4_H
#define __GTEXTVIEW4_H

#include "GDocView.h"
#include "GUndo.h"

// use CRLF as opposed to just LF
// internally it uses LF only... this is just to remember what to
// save out as.
#define TEXTED_USES_CR				0x00000001
#define DEFAULT_TAB_SIZE			4
#define WM_DEBUG_TEXT				(WM_USER+0x3421)

extern char Delimiters[];

enum GTextMove
{
	GUpLine,
	GDownLine,
	GLeftChar,
	GRightChar,
	GStartLine,
	GEndLine
};

class GTextView4 :
	public GDocView,
	public ResObject
{
protected:
	class GTextView4Private *d;

public:
	// Construction
	GTextView4(	int Id,
				int x,
				int y,
				int cx,
				int cy,
				GFontType *FontInfo = 0);
	~GTextView4();
	
	char *GetMimeType() { return "text/plain"; }

	// Data
	char *Name();
	bool Name(char *s);
	char16 *NameW();
	bool NameW(char16 *s);

	bool Insert(int At, char16 *Data, int Len);
	bool Delete(int At, int Len);
	int HitText(int x, int y);
	void DeleteSelection(char16 **Cut = 0);

	// Font
	GFont *GetFont();
	void SetFont(GFont *f, bool OwnIt = false);
	bool GetFixed();
	void SetFixed(bool f);

	// Options
	void SetTabSize(uint8 i);
	void SetBorder(int b);
	void SetReadOnly(bool i);
	void SetWrapType(uint8 i);
	
	// State / Selection
	void SetCursor(int i, bool Select, bool ForceFullUpdate = false);
	int IndexAt(int x, int y);
	bool IsDirty();
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

	// Actions
	bool ClearDirty(bool Ask, char *FileName = 0);
	void UpdateScrollBars(bool Reset = false);
	void GotoLine(int Line);
	bool DoGoto();
	bool DoFind();
	bool DoFindNext();
	bool DoReplace();
	bool DoCase(bool Upper);
	void Undo();
	void Redo();

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

	// Virtuals
	virtual void OnEnter(GKey &k);
	virtual void OnUrl(char *Url);
};

#endif
