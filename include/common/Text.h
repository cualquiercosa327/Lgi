/*hdr
**	FILE:		Text.h
**	AUTHOR:		Matthew Allen
**	DATE:		27/6/97
**	DESCRIPTION:	Generic text document handler
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

#ifndef __TEXT_H
#define __TEXT_H

#include "Lgi.h"

// word wrap
#define TEXTED_WRAP_NONE			0	// no word wrap
#define TEXTED_WRAP_NONREFLOW		1	// insert LF when you hit the end
#define TEXTED_WRAP_REFLOW			2	// dynamically wrap line to editor width

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

#define IsWhite(c)					(strchr(WhiteSpace, c) != 0)
#define IsDelimiter(c)				(strchr(Delimiters, c) != 0)
#define IsDigit(c)					((c) >= '0' AND (c) <= '9')
#define IsAlpha(c)					(((c) >= 'a' AND (c) <= 'z') OR ((c) >= 'A' AND (c) <= 'Z'))
#define IsText(c)					(IsDigit(c) OR IsAlpha(c) OR (c) == '_')

extern char Delimiters[];
extern int StrLen(char *c);
extern int RevStrLen(char *c);

// Document
class Document {
protected:
	char *File;
	GFile F;

	bool Open(char *FileName, int Attrib);

public:
	Document();
	virtual ~Document();

	virtual bool Load(char *File) { return FALSE; }
	virtual bool Save(char *File) { return FALSE; }
	char *GetName() { return File; }
};

// Text
class TextDocument;

class GCursor {

	friend class TextDocument;

	TextDocument *Parent;
	int x, y;			// location of cursor
	int Length;			// size of the current line
	int Offset;			// offset to the start of the line 

	void GoUpLine();
	void GoDownLine();

public:
	GCursor();
	~GCursor();

	int X() { return x; }
	int Y() { return y; }
	int LineLength() { return Length; }
	int GetOffset() { return Offset; }
	operator char*();
	char *GetStr();

	bool AtEndOfLine();
	bool AtBeginningOfLine();

	int CursorX(int TabSize);
	void SetX(int X);
	void SetY(int Y);
	void MoveX(int Dx);
	void MoveY(int Dy);
	
	bool operator ==(GCursor &c)
	{
		return	(x == c.x) AND
				(y == c.y) AND
				(Parent == c.Parent);
	}

	bool operator !=(GCursor &c)
	{
		return	(x != c.x) OR
				(y != c.y) OR
				(Parent != c.Parent);
	}

	bool operator <(GCursor &c)
	{
		return	(y < c.y) OR
				((y == c.y) AND (x < c.x));
	}

	bool operator <=(GCursor &c)
	{
		return	(y < c.y) OR
				((y == c.y) AND (x <= c.x));
	}

	bool operator >(GCursor &c)
	{
		return	(y > c.y) OR
				((y == c.y) AND (x > c.x));
	}

	bool operator >=(GCursor &c)
	{
		return	(y > c.y) OR
				((y == c.y) AND (x >= c.x));
	}
	
	int operator -(GCursor &c)
	{
		return (Offset + x) - (c.Offset + c.x);
	}
};

class TextLock {

	friend class TextDocument;

	int StartLine;
	int Lines;
	char **Line;
	ushort **LineW;

public:
	TextLock();
	~TextLock();

	int Start() { return StartLine; }
	char *operator [](int i);
	ushort *GetLineW(int i);
};

class UserAction
{
public:
	char *Text;
	int x, y;
	bool Insert;

	UserAction();
	~UserAction();
};

#define TEXT_BLOCK			0x4000
#define TEXT_MASK			0x3FFF

class TextDocument : public Document {
public:
	friend GCursor;

	int Flags;
	int LockCount;
	bool Dirty;
	bool CrLf;
	bool Editable;

	// Undo stuff
	int IgnoreUndo;
	int UndoPos;
	List<UserAction> Queue;
	void TruncateQueue();
	void ApplyAction(UserAction *a, GCursor &c, bool Reverse = false);

	// Data
	int Lines;		// Total lines of text
	int Length;		// Bytes used by doc
	int Alloc;		// Allocated memory
	char *Data;		// Pointer to start of doc

	// Methods
	bool SetLength(int Len);
	int CountLines(char *c, int Len = -1);
	char *FindLine(int i);

public:
	TextDocument();
	virtual ~TextDocument();

	bool UseCrLf() { return CrLf; }
	void UseCrLf(bool Use) { CrLf = Use; }
	bool AcceptEdits() { return Editable; }
	void AcceptEdits(bool i) { Editable = i; }

	// size
	int GetLines() { return Lines; }
	int GetLength() { return Length; }

	// serialization
	bool Load(char *File);
	bool Save(char *File);
	bool IsDirty() { return Dirty; }
	bool Import(char *s, int size = -1);
	bool Export(char *&s, int &size);

	// data access
	bool Lock(TextLock *Lock, int StartLine, int Lines, int CodePage = TVCP_US_ASCII);
	void UnLock(TextLock *Lock);
	bool MoveLock(TextLock *Lock, int Dy);

	// cursor support
	bool CursorCreate(GCursor *c, int X, int Y);

	// data io
	bool Insert(GCursor *At, char *Text, int Len = -1);
	bool Delete(GCursor *From, int Len, char *Buffer = NULL);

	// undo
	void Undo(GCursor &c);
	void Redo(GCursor &c);
	void ClearUndoQueue();
	bool UndoAvailable(bool Redo = false);
};

#define TVF_SELECTION				0x00000001
#define TVF_DIRTY_CURSOR			0x00000010
#define TVF_DIRTY_TO_EOL			0x00000020
#define TVF_DIRTY_TO_EOP			0x00000040
#define TVF_DIRTY_SELECTION			0x00000080
#define TVF_DIRTY_ALL				0x00000100
#define TVF_DIRTY_MASK				(TVF_DIRTY_CURSOR | TVF_DIRTY_TO_EOL | TVF_DIRTY_TO_EOP | TVF_DIRTY_SELECTION | TVF_DIRTY_ALL)
#define TVF_SHIFT					0x00000200
#define TVF_GOTO_START				0x00000400	// of selection
#define TVF_GOTO_END				0x00000800	// of selection
#define TVF_EAT_MOVE				0x00001000	// don't move cursor

class TextView {
protected:
	// Misc
	int Flags;
	bool IsDirty();
	virtual void Dirty(int Type);
	void Clean();
	ushort *GetCodePageMap(int Page = -1);

	char *StatusMsg;
	void SetStatus(char *Msg);

	// Clipboard
	char *ClipData;

	virtual bool ClipText(char *Str, int Len = -1);
	virtual char *ClipText();
	virtual bool Cut();
	virtual bool Copy();
	virtual bool Paste();
	
	// Data storage
	TextDocument Doc;
	bool OnInsertText(char *Text, int Len = -1);
	bool OnDeleteText(GCursor *c, int Len, bool Clip);

	// Current cursor location
	int HiddenLines;
	int DisplayLines;
	GCursor User;
	virtual bool UpdateHiddenCheck();
	virtual bool OnMoveCursor(int Dx, int Dy = 0, bool NoSelect = FALSE);
	virtual void OnSetHidden(int Hidden);
	virtual void OnSetCursor(int X, int Y);
	virtual bool OnMultiLineTab(bool In) { return FALSE; } // return TRUE if processed

	// Selection
	GCursor Start, End;
	void OnStartSelection(GCursor *c);
	void OnEndSelection();
	void OnDeleteSelection(bool Clip);

	// Events
	virtual void AfterInsertText(char *c, int len) {}
	virtual void AfterDeleteText() {}

	// Properties
	int WrapType;
	int CodePage;

public:
	TextView();
	virtual ~TextView();

	virtual bool Open(char *Name);
	virtual bool Save(char *Name);
	virtual int ProcessKey(GKey &K);
	virtual int Paint() { return FALSE; }
	virtual int GetWrapType() { return WrapType; }
	virtual void SetWrapType(int i) { WrapType = i; }
	virtual int GetCodePage() { return CodePage; }
	virtual void SetCodePage(int i) { CodePage = i; }

	virtual bool ClearDirty(bool Ask);
	virtual void OnSave();
	virtual void OnGotoLine();
	virtual void OnEscape(GKey &K);
	virtual void OnEnter(GKey &K);
};

#endif
