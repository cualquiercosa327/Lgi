/// \file
/// \author Matthew Allen
/// \brief A unicode text editor

#ifndef _RICH_TEXT_EDIT_H_
#define _RICH_TEXT_EDIT_H_

#include "GDocView.h"
#include "GUndo.h"
#include "GDragAndDrop.h"
#include "GCapabilities.h"

extern char Delimiters[];

/// Styled unicode text editor control.
class
#if defined(MAC)
	LgiClass
#endif
	GRichTextEdit :
	public GDocView,
	public ResObject,
	public GDragDropTarget,
	public GCapabilityTarget
{
	friend bool RichText_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User);

public:
	enum GTextViewSeek
	{
		PrevLine,
		NextLine,
		StartLine,
		EndLine
	};

protected:
	class GRichTextPriv *d;
	friend class GRichTextPriv;

	bool InternalIndexAt(int x, int y, int &Off, int &LineHint);

	// Overridables
	virtual void PourText(int Start, int Length);
	virtual void PourStyle(int Start, int Length);
	virtual void OnFontChange();
	virtual void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour);

public:
	// Construction
	GRichTextEdit(	int Id,
					int x = 0,
					int y = 0,
					int cx = 100,
					int cy = 100,
					GFontType *FontInfo = 0);
	~GRichTextEdit();

	const char *GetClass() { return "GRichTextEdit"; }

	// Data
	char *Name();
	bool Name(const char *s);
	char16 *NameW();
	bool NameW(const char16 *s);
	int64 Value();
	void Value(int64 i);
	const char *GetMimeType() { return "text/html"; }
	int GetSize();
	const char *GetCharset();
	void SetCharset(const char *s);

	int HitTest(int x, int y);
	bool DeleteSelection(char16 **Cut = 0);

	// Font
	GFont *GetFont();
	void SetFont(GFont *f, bool OwnIt = false);
	void SetFixedWidthFont(bool i);

	// Options
	void SetTabSize(uint8 i);
	void SetReadOnly(bool i);
	bool ShowStyleTools();
	void ShowStyleTools(bool b);

	enum RectType
	{
		ContentArea,
		ToolsArea,
		CapabilityArea,
		CapabilityBtn,

		FontFamilyBtn,
		FontSizeBtn,
		
		BoldBtn,
		ItalicBtn,
		UnderlineBtn,
		
		ForegroundColourBtn,
		BackgroundColourBtn,

		MakeLinkBtn,
		RemoveStyleBtn,
		
		MaxArea
	};
	GRect GetArea(RectType Type);

	/// Sets the wrapping on the control, use #TEXTED_WRAP_NONE or #TEXTED_WRAP_REFLOW
	void SetWrapType(uint8 i);
	
	// State / Selection
	void SetCursor(int i, bool Select, bool ForceFullUpdate = false);
	int IndexAt(int x, int y);
	bool IsDirty();
	void IsDirty(bool d);
	bool HasSelection();
	void UnSelectAll();
	void SelectWord(int From);
	void SelectAll();
	int GetCursor(bool Cursor = true);
	bool GetLineColumnAtIndex(GdcPt2 &Pt, int Index = -1);
	int GetLines();
	void GetTextExtent(int &x, int &y);
	char *GetSelection();

	// File IO
	bool Open(const char *Name, const char *Cs = 0);
	bool Save(const char *Name, const char *Cs = 0);

	// Clipboard IO
	bool Cut();
	bool Copy();
	bool Paste();

	// Undo/Redo
	void Undo();
	void Redo();
	bool GetUndoOn();
	void SetUndoOn(bool b);

	// Action UI
	virtual bool DoGoto();
	virtual bool DoCase(bool Upper);
	virtual bool DoFind();
	virtual bool DoFindNext();
	virtual bool DoReplace();

	// Action Processing	
	bool ClearDirty(bool Ask, char *FileName = 0);
	void UpdateScrollBars(bool Reset = false);
	int GetLine();
	void SetLine(int Line);
	GDocFindReplaceParams *CreateFindReplaceParams();
	void SetFindReplaceParams(GDocFindReplaceParams *Params);
	void OnAddStyle(const char *MimeType, const char *Styles);

	// Object Events
	bool OnFind(GFindReplaceCommon *Params);
	bool OnReplace(GFindReplaceCommon *Params);
	bool OnMultiLineTab(bool In);
	void OnSetHidden(int Hidden);
	void OnPosChange();
	void OnCreate();
	void OnEscape(GKey &K);
	bool OnMouseWheel(double Lines);

	// Capability target stuff
	bool NeedsCapability(const char *Name, const char *Param);
	void OnInstall(CapsHash *Caps, bool Status);
	void OnCloseInstaller();

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

	// Virtuals
	virtual bool Insert(int At, char16 *Data, int Len);
	virtual bool Delete(int At, int Len);
	virtual void OnEnter(GKey &k);
	virtual void OnUrl(char *Url);
	virtual void DoContextMenu(GMouse &m);
};

#endif
