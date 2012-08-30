#ifndef __GTEXTVIEW_H
#define __GTEXTVIEW_H

// Word wrap
#define TEXTED_WRAP_NONE			0	// no word wrap
#define TEXTED_WRAP_REFLOW			1	// dynamically wrap line to editor width

// Notify flags
#define GTVN_DOC_CHANGED			0x0001
#define GTVN_CURSOR_CHANGED			0x0002
#define GTVN_CODEPAGE_CHANGED		0x0004

// Util macros
#define IsWhiteSpace(c)				(strchr(GTextView::WhiteSpace, c) != 0)
#define IsDelimiter(c)				(strchr(GTextView::Delimiters, c) != 0)
/*
#define IsDigit(c)					((c) >= '0' AND (c) <= '9')
#define IsAlpha(c)					(((c) >= 'a' AND (c) <= 'z') || ((c) >= 'A' AND (c) <= 'Z'))
*/
#define IsText(c)					(IsDigit(c) || IsAlpha(c) || (c) == '_')
#define IsWordBoundry(c)			(strchr(GTextView::WhiteSpace, c) || strchr(GTextView::Delimiters, c))
#define UrlChar(c)					(strchr(GTextView::UrlDelim, (c)) || AlphaOrDigit((c)))

extern char16 *ConvertToCrLf(char16 *Text);

// Callback API for menu
class GTextViewMenu
{
public:
	virtual bool AppendItems(GSubMenu *Menu, int Base = 1000) = 0;
	virtual bool OnMenu(class GTextView *View, int Id) = 0;
};

// TextView class
class GTextView : public GLayout
{
public:
	// Static
	static char *WhiteSpace;
	static char *Delimiters;
	static char *UrlDelim;
	static bool AlphaOrDigit(char c);

	///////////////////////////////////////////////////////////////////////
	// Properties
	#define _TvMenuProp(Type, Name)						\
	protected:											\
		Type Name;										\
	public:												\
		virtual void Set##Name(Type i) { Name=i; }		\
		virtual Type Get##Name() { return Name; }

	_TvMenuProp(uint16, WrapAtCol)
	_TvMenuProp(bool, UrlDetect)
	_TvMenuProp(bool, ReadOnly)
	_TvMenuProp(uint8, WrapType)
	_TvMenuProp(uint8, TabSize)
	_TvMenuProp(bool, CrLf)
	_TvMenuProp(bool, AutoIndent)
	_TvMenuProp(COLOUR, BackColour)
	#undef _TvMenuProp

	///////////////////////////////////////////////////////////////////////
	// Object
	GTextView()
	{
		WrapAtCol = 0;
		UrlDetect = true;
		ReadOnly = false;
		WrapType = TEXTED_WRAP_REFLOW;
		CrLf = false;
		AutoIndent = true;
		BackColour = Rgb24(255, 255, 255);
	}

	///////////////////////////////////////////////////////////////////////
	// File
	virtual bool Open(char *Name, char *Cs = 0) { return false; }
	virtual bool Save(char *Name, char *Cs = 0) { return false; }

	///////////////////////////////////////////////////////////////////////
	// Find/Replace
	virtual bool DoFind() { return false; }
	virtual bool DoReplace() { return false; }

	///////////////////////////////////////////////////////////////////////
	// Callback API
	virtual GTextViewMenu *GetMenuCallback() { return 0; }
	virtual void SetMenuCallback(GTextViewMenu *c) {}
	
	///////////////////////////////////////////////////////////////////////
	// State / Selection
	
	// Set the cursor position, to select an area, move the cursor with Select=false
	// then set the other end of the region with Select=true.
	virtual void SetCursor(int i, bool Select, bool ForceFullUpdate = false) {}

	// Cursor=false means the other end of the selection if any. The cursor is alwasy
	// at one end of the selection.
	virtual int GetCursor(bool Cursor = true) { return 0; }

	// Selection access
	virtual bool HasSelection() { return false; }
	virtual void UnSelectAll() {}
	virtual void SelectWord(int From) {}
	virtual void SelectAll() {}
	virtual char *GetSelection() { return 0; }

	// Returns the character index at the x,y location
	virtual int IndexAt(int x, int y) { return 0; }

	// Index=-1 returns the x,y of the cursor, Index >=0 returns the specified x,y
	virtual void PositionAt(int &x, int &y, int Index = -1) { }

	virtual bool IsDirty() { return false; }
	virtual int GetLines() { return 0; }
	virtual void GetTextExtent(int &x, int &y) {}

	///////////////////////////////////////////////////////////////////////
	// Clipboard IO
	virtual bool Cut() { return false; }
	virtual bool Copy() { return false; }
	virtual bool Paste() { return false; }

	///////////////////////////////////////////////////////////////////////
	// Virtual events
	virtual void OnEscape(GKey &K) {}
	virtual void OnEnter(GKey &k) {}
	virtual void OnUrl(char *Url) {}
};

#endif
