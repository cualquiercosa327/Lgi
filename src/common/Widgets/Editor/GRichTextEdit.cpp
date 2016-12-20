#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GInput.h"
#include "GScrollBar.h"
#ifdef WIN32
#include <imm.h>
#endif
#include "GClipBoard.h"
#include "GDisplayString.h"
#include "GViewPriv.h"
#include "GCssTools.h"
#include "GFontCache.h"
#include "GUnicode.h"

#include "GHtmlCommon.h"
#include "GHtmlParser.h"
#include "LgiRes.h"

#define DefaultCharset              "utf-8"

#define GDCF_UTF8					-1
#define LUIS_DEBUG					0
#define POUR_DEBUG					0
#define PROFILE_POUR				0

#define ALLOC_BLOCK					64
#define IDC_VS						1000

#ifndef IDM_OPEN
#define IDM_OPEN					1
#endif
#ifndef IDM_NEW
#define	IDM_NEW						2
#endif
#ifndef IDM_COPY
#define IDM_COPY					3
#endif
#ifndef IDM_CUT
#define IDM_CUT						4
#endif
#ifndef IDM_PASTE
#define IDM_PASTE					5
#endif
#define IDM_COPY_URL				6
#define IDM_AUTO_INDENT				7
#define IDM_UTF8					8
#define IDM_PASTE_NO_CONVERT		9
#ifndef IDM_UNDO
#define IDM_UNDO					10
#endif
#ifndef IDM_REDO
#define IDM_REDO					11
#endif
#define IDM_FIXED					12
#define IDM_SHOW_WHITE				13
#define IDM_HARD_TABS				14
#define IDM_INDENT_SIZE				15
#define IDM_TAB_SIZE				16
#define IDM_DUMP					17
#define IDM_RTL						18
#define IDM_COPY_ORIGINAL			19

#define PAINT_BORDER				Back
#define PAINT_AFTER_LINE			Back

#define CODEPAGE_BASE				100
#define CONVERT_CODEPAGE_BASE		200

#if !defined(WIN32) && !defined(toupper)
#define toupper(c)					(((c)>='a'&&(c)<='z') ? (c)-'a'+'A' : (c))
#endif

static char SelectWordDelim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

#include "GRichTextEditPriv.h"

typedef GRichTextPriv::BlockCursor BlkCursor;
typedef GAutoPtr<GRichTextPriv::BlockCursor> AutoCursor;

//////////////////////////////////////////////////////////////////////
enum UndoType
{
	UndoDelete, UndoInsert, UndoChange
};

class GRichEditUndo : public GUndoEvent
{
	GRichTextEdit *View;
	UndoType Type;

public:
	GRichEditUndo(	GRichTextEdit *view,
					char16 *t,
					int len,
					int at,
					UndoType type)
	{
		View = view;
		Type = type;
	}

	~GRichEditUndo()
	{
	}

	void OnChange()
	{
	}

	// GUndoEvent
    void ApplyChange()
	{
	}

    void RemoveChange()
	{
	}
};

//////////////////////////////////////////////////////////////////////
GRichTextEdit::GRichTextEdit(	int Id,
								int x, int y, int cx, int cy,
								GFontType *FontType)
	: ResObject(Res_Custom)
{
	// init vars
	GView::d->Css.Reset(new GRichTextPriv(this, d));

	// setup window
	SetId(Id);
	SetTabStop(true);

	// default options
	#if WINNATIVE
	CrLf = true;
	SetDlgCode(DLGC_WANTALLKEYS);
	#else
	CrLf = false;
	#endif
	d->Padding(GCss::Len(GCss::LenPx, 4));
	
	#if 0
	d->BackgroundColor(GCss::ColorDef(GColour::Green));
	#else
	d->BackgroundColor(GCss::ColorDef(GCss::ColorRgb, Rgb24To32(LC_WORKSPACE)));
	#endif
	
	SetFont(SysFont);

	#if 0 // def _DEBUG
	Name("<html>\n"
		"<body>\n"
		"	This is some <b style='font-size: 20pt; color: green;'>bold text</b> to test with.<br>\n"
		"   A second line of text for testing.\n"
		"</body>\n"
		"</html>\n");
	#endif

	NeedsCapability("Alpha", "This control is still in alpha.");
}

GRichTextEdit::~GRichTextEdit()
{
	// 'd' is owned by the GView CSS autoptr.
}

bool GRichTextEdit::NeedsCapability(const char *Name, const char *Param)
{
	d->NeedsCap.New().Set(Name, Param);
	Invalidate();
	return true;
}

void GRichTextEdit::OnInstall(CapsHash *Caps, bool Status)
{
	OnCloseInstaller();
}

void GRichTextEdit::OnCloseInstaller()
{
	d->NeedsCap.Length(0);
	Invalidate();
}

bool GRichTextEdit::IsDirty()
{
	return d->Dirty;
}

void GRichTextEdit::IsDirty(bool dirty)
{
	if (d->Dirty ^ dirty)
	{
		d->Dirty = dirty;
	}
}

void GRichTextEdit::SetFixedWidthFont(bool i)
{
	if (FixedWidthFont ^ i)
	{
		if (i)
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				GDocView::SetFixedWidthFont(i);
			}
		}

		OnFontChange();
		Invalidate();
	}
}

void GRichTextEdit::SetReadOnly(bool i)
{
	GDocView::SetReadOnly(i);

	#if WINNATIVE
	SetDlgCode(i ? DLGC_WANTARROWS : DLGC_WANTALLKEYS);
	#endif
}

GRect GRichTextEdit::GetArea(RectType Type)
{
	return	Type >= ContentArea &&
			Type <= MaxArea
			?
			d->Areas[Type]
			:
			GRect(0, 0, -1, -1);
}

bool GRichTextEdit::ShowStyleTools()
{
	return d->ShowTools;
}

void GRichTextEdit::ShowStyleTools(bool b)
{
	if (d->ShowTools ^ b)
	{
		d->ShowTools = b;
		Invalidate();
	}
}

void GRichTextEdit::SetTabSize(uint8 i)
{
	TabSize = limit(i, 2, 32);
	OnFontChange();
	OnPosChange();
	Invalidate();
}

void GRichTextEdit::SetWrapType(uint8 i)
{
	GDocView::SetWrapType(i);

	OnPosChange();
	Invalidate();
}

GFont *GRichTextEdit::GetFont()
{
	return d->Font;
}

void GRichTextEdit::SetFont(GFont *f, bool OwnIt)
{
	if (!f)
		return;
	
	if (OwnIt)
	{
		d->Font.Reset(f);
	}
	else if (d->Font.Reset(new GFont))
	{		
		*d->Font = *f;
		d->Font->Create(NULL, 0, 0);
	}
	
	OnFontChange();
}

void GRichTextEdit::OnFontChange()
{
}

void GRichTextEdit::PourText(int Start, int Length /* == 0 means it's a delete */)
{
}

void GRichTextEdit::PourStyle(int Start, int EditSize)
{
}

bool GRichTextEdit::Insert(int At, char16 *Data, int Len)
{
	return false;
}

bool GRichTextEdit::Delete(int At, int Len)
{
	return false;
}

bool GRichTextEdit::DeleteSelection(char16 **Cut)
{
	if (!d->Cursor || !d->Selection)
		return false;

	GArray<char16> DeletedText;
	GArray<char16> *DelTxt = Cut ? &DeletedText : NULL;

	bool Cf = d->CursorFirst();
	GRichTextPriv::BlockCursor *Start = Cf ? d->Cursor : d->Selection;
	GRichTextPriv::BlockCursor *End = Cf ? d->Selection : d->Cursor;
	if (Start->Blk == End->Blk)
	{
		// In the same block... just delete the text
		int Len = End->Offset - Start->Offset;
		Start->Blk->DeleteAt(Start->Offset, Len, DelTxt);
	}
	else
	{
		// Multi-block delete...

		// 1) Delete all the content to the end of the first block
		int StartLen = Start->Blk->Length();
		if (Start->Offset < StartLen)
			Start->Blk->DeleteAt(Start->Offset, StartLen - Start->Offset, DelTxt);

		// 2) Delete any blocks between 'Start' and 'End'
		int i = d->Blocks.IndexOf(Start->Blk);
		if (i >= 0)
		{
			for (++i; d->Blocks[i] != End->Blk && i < (int)d->Blocks.Length(); )
			{
				GRichTextPriv::Block *b = d->Blocks[i];
				b->CopyAt(0, -1, DelTxt);
				d->Blocks.DeleteAt(i, true);
				DeleteObj(b);
			}
		}
		else
		{
			LgiAssert(0);
			return false;
		}

		// 3) Delete any text up to the Cursor in the 'End' block
		End->Blk->DeleteAt(0, End->Offset, DelTxt);

		// Try and merge the start and end blocks
		d->Merge(Start->Blk, End->Blk);
	}

	// Set the cursor and update the screen
	d->Cursor->Set(Start->Blk, Start->Offset, Start->LineHint);
	d->Selection.Reset();
	Invalidate();

	if (Cut)
	{
		DelTxt->Add(0);
		*Cut = DelTxt->Release();
	}

	return true;
}

int64 GRichTextEdit::Value()
{
	char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void GRichTextEdit::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LGI_PrintfInt64, i);
	Name(Str);
}

char *GRichTextEdit::Name()
{
	d->ToHtml();
	return d->UtfNameCache;
}

const char *GRichTextEdit::GetCharset()
{
	return d->Charset;
}

void GRichTextEdit::SetCharset(const char *s)
{
	d->Charset = s;
}

static GHtmlElement *FindElement(GHtmlElement *e, HtmlTag TagId)
{
	if (e->TagId == TagId)
		return e;
		
	for (unsigned i = 0; i < e->Children.Length(); i++)
	{
		GHtmlElement *c = FindElement(e->Children[i], TagId);
		if (c)
			return c;
	}
	
	return NULL;
}

void GRichTextEdit::OnAddStyle(const char *MimeType, const char *Styles)
{
	if (d->CreationCtx)
	{
		d->CreationCtx->StyleStore.Parse(Styles);
	}
}

bool GRichTextEdit::Name(const char *s)
{
	d->Empty();
	d->OriginalText = s;
	
	GHtmlElement Root(NULL);

	if (!d->CreationCtx.Reset(new GRichTextPriv::CreateContext(d)))
		return false;

	if (!d->GHtmlParser::Parse(&Root, s))
		return d->Error(_FL, "Failed to parse HTML.");

	GHtmlElement *Body = FindElement(&Root, TAG_BODY);
	if (!Body)
		Body = &Root;

	bool Status = d->FromHtml(Body, *d->CreationCtx);
	if (!d->Blocks.Length())
	{
		d->EmptyDoc();
	}
	else
	{
		// Clear out any zero length blocks.
		for (unsigned i=0; i<d->Blocks.Length(); i++)
		{
			GRichTextPriv::Block *b = d->Blocks[i];
			if (b->Length() == 0)
			{
				d->Blocks.DeleteAt(i--, true);
				DeleteObj(b);
			}
		}
	}
	
	if (Status)
		SetCursor(0, false);
	
	// d->DumpBlocks();
	
	return Status;
}

char16 *GRichTextEdit::NameW()
{
	d->WideNameCache.Reset(Utf8ToWide(Name()));
	return d->WideNameCache;
}

bool GRichTextEdit::NameW(const char16 *s)
{
	GAutoString a(WideToUtf8(s));
	return Name(a);
}

char *GRichTextEdit::GetSelection()
{
	if (!HasSelection())
		return NULL;

	GArray<char16> Text;
	if (!d->GetSelection(Text))
		return NULL;
	
	return WideToUtf8(&Text[0]);
}

bool GRichTextEdit::HasSelection()
{
	return d->Selection.Get() != NULL;
}

void GRichTextEdit::SelectAll()
{
	AutoCursor Start(new BlkCursor(d->Blocks.First(), 0, 0));
	d->SetCursor(Start);

	GRichTextPriv::Block *Last = d->Blocks.Length() ? d->Blocks.Last() : NULL;
	if (Last)
	{
		AutoCursor End(new BlkCursor(Last, Last->Length(), Last->GetLines()-1));
		d->SetCursor(End, true);
	}
	else d->Selection.Reset();

	Invalidate();
}

void GRichTextEdit::UnSelectAll()
{
	bool Update = HasSelection();

	if (Update)
	{
		d->Selection.Reset();
		Invalidate();
	}
}

int GRichTextEdit::GetLines()
{
	uint32 Count = 0;
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		Count += b->GetLines();
	}
	return Count;
}

int GRichTextEdit::GetLine()
{
	if (!d->Cursor)
		return -1;

	int Idx = d->Blocks.IndexOf(d->Cursor->Blk);
	if (Idx < 0)
	{
		LgiAssert(0);
		return -1;
	}

	int Count = 0;
	
	// Count lines in blocks before the cursor...
	for (int i=0; i<Idx; i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		Count += b->GetLines();
	}

	// Add the lines in the cursor's block...
	if (d->Cursor->LineHint)
	{
		Count += d->Cursor->LineHint;
	}
	else
	{
		GArray<int> BlockLine;
		if (d->Cursor->Blk->OffsetToLine(d->Cursor->Offset, NULL, &BlockLine))
			Count += BlockLine.First();
		else
		{
			// Hmmm...
			LgiAssert(!"Can't find block line.");
			return -1;
		}
	}


	return Count;
}

void GRichTextEdit::SetLine(int i)
{
	int Count = 0;
	
	// Count lines in blocks before the cursor...
	for (int i=0; i<(int)d->Blocks.Length(); i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		int Lines = b->GetLines();
		if (i >= Count && i < Count + Lines)
		{
			int BlockLine = i - Count;
			int Offset = b->LineToOffset(BlockLine);
			if (Offset >= 0)
			{
				AutoCursor c(new BlkCursor(b, Offset, BlockLine));
				d->SetCursor(c);
				break;
			}
		}		
		Count += Lines;
	}
}

void GRichTextEdit::GetTextExtent(int &x, int &y)
{
	x = d->DocumentExtent.x;
	y = d->DocumentExtent.y;
}

bool GRichTextEdit::GetLineColumnAtIndex(GdcPt2 &Pt, int Index)
{
	int Offset = -1, BlockLines = -1;
	GRichTextPriv::Block *b = d->GetBlockByIndex(Index, &Offset, NULL, &BlockLines);
	if (!b)
		return false;

	int Cols;
	GArray<int> Lines;
	if (b->OffsetToLine(Offset, &Cols, &Lines))
		return false;
	
	Pt.x = Cols;
	Pt.y = BlockLines + Lines.First();
	return true;
}

int GRichTextEdit::GetCursor(bool Cur)
{
	if (!d->Cursor)
		return -1;
		
	int CharPos = 0;
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		if (d->Cursor->Blk == b)
			return CharPos + d->Cursor->Offset;
		CharPos += b->Length();
	}
	
	LgiAssert(!"Cursor block not found.");
	return -1;
}

bool GRichTextEdit::InternalIndexAt(int x, int y, int &Off, int &LineHint)
{
	GdcPt2 Doc = d->ScreenToDoc(x, y);
	Off = d->HitTest(Doc.x, Doc.y, LineHint);
	return Off >= 0;
}

int GRichTextEdit::IndexAt(int x, int y)
{
	int Idx, Line;
	if (!InternalIndexAt(x, y, Idx, Line))
		return -1;
	return Idx;
}

void GRichTextEdit::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
	int Offset = -1;
	GRichTextPriv::Block *Blk = d->GetBlockByIndex(i, &Offset);
	if (Blk)
	{
		AutoCursor c(new BlkCursor(Blk, Offset, -1));
		if (c)
			d->SetCursor(c, Select);
	}
}

bool GRichTextEdit::Cut()
{
	if (!HasSelection())
		return false;

	char16 *Txt = NULL;
	if (!DeleteSelection(&Txt))
		return false;

	bool Status = true;
	if (Txt)
	{
		GClipBoard Cb(this);
		Status = Cb.TextW(Txt);
		DeleteArray(Txt);
	}

	SendNotify(GNotifyDocChanged);

	return Status;
}

bool GRichTextEdit::Copy()
{
	if (!HasSelection())
		return false;

	GArray<char16> Text;
	if (!d->GetSelection(Text))
		return false;

	// Put on the clipboard
	GClipBoard Cb(this);
	return Cb.TextW(&Text[0]);
}

bool GRichTextEdit::Paste()
{
	GClipBoard Cb(this);
	GAutoWString Text(Cb.TextW());
	if (!Text)
		return false;
	
	if (!d->Cursor ||
		!d->Cursor->Blk)
	{
		LgiAssert(0);
		return false;
	}

	if (HasSelection())
		DeleteSelection();

	int Len = Strlen(Text.Get());
	if (!d->Cursor->Blk->AddText(d->Cursor->Offset, Text, Len))
	{
		LgiAssert(0);
		SendNotify(GNotifyDocChanged);
		return false;
	}

	d->Cursor->Offset += Len;
	Invalidate();
	SendNotify(GNotifyDocChanged);

	return true;
}

bool GRichTextEdit::ClearDirty(bool Ask, char *FileName)
{
	if (1 /*dirty*/)
	{
		int Answer = (Ask) ? LgiMsg(this,
									LgiLoadString(L_TEXTCTRL_ASK_SAVE, "Do you want to save your changes to this document?"),
									LgiLoadString(L_TEXTCTRL_SAVE, "Save"),
									MB_YESNOCANCEL) : IDYES;
		if (Answer == IDYES)
		{
			GFileSelect Select;
			Select.Parent(this);
			if (!FileName &&
				Select.Save())
			{
				FileName = Select.Name();
			}

			Save(FileName);
		}
		else if (Answer == IDCANCEL)
		{
			return false;
		}
	}

	return true;
}

bool GRichTextEdit::Open(const char *Name, const char *CharSet)
{
	bool Status = false;
	GFile f;

	if (f.Open(Name, O_READ|O_SHARE))
	{
		size_t Bytes = (size_t)f.GetSize();
		SetCursor(0, false);
		
		char *c8 = new char[Bytes + 4];
		if (c8)
		{
			if (f.Read(c8, (int)Bytes) == Bytes)
			{
				char *DataStart = c8;

				c8[Bytes] = 0;
				c8[Bytes+1] = 0;
				c8[Bytes+2] = 0;
				c8[Bytes+3] = 0;
				
				if ((uchar)c8[0] == 0xff && (uchar)c8[1] == 0xfe)
				{
					// utf-16
					if (!CharSet)
					{
						CharSet = "utf-16";
						DataStart += 2;
					}
				}
				
			}

			DeleteArray(c8);
		}
		else
		{
		}

		Invalidate();
	}

	return Status;
}

bool GRichTextEdit::Save(const char *FileName, const char *CharSet)
{
	GFile f;
	if (!FileName || !f.Open(FileName, O_WRITE))
		return false;

	f.SetSize(0);
	char *Nm = Name();
	if (!Nm)
		return false;

	int Len = strlen(Nm);
	return f.Write(Nm, Len) == Len;
}

void GRichTextEdit::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		//GRect Before = GetClient();

	}
}

bool GRichTextEdit::DoCase(bool Upper)
{
	return true;
}

bool GRichTextEdit::DoGoto()
{
	GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto line:"), "Text");
	if (Dlg.DoModal() == IDOK &&
		Dlg.Str)
	{
		SetLine(atoi(Dlg.Str));
	}

	return true;
}

GDocFindReplaceParams *GRichTextEdit::CreateFindReplaceParams()
{
	return new GDocFindReplaceParams3;
}

void GRichTextEdit::SetFindReplaceParams(GDocFindReplaceParams *Params)
{
	if (Params)
	{
	}
}

bool GRichTextEdit::DoFindNext()
{
	return false;
}

bool
RichText_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	return ((GRichTextEdit*)User)->OnFind(Dlg);
}

////////////////////////////////////////////////////////////////////////////////// FIND
bool GRichTextEdit::DoFind()
{
	GArray<char16> Sel;
	if (HasSelection())
		d->GetSelection(Sel);
	GAutoString u(Sel.Length() ? WideToUtf8(&Sel.First()) : NULL);
	GFindDlg Dlg(this, u, RichText_FindCallback, this);
	Dlg.DoModal();	
	Focus(true);
	return false;
}

bool GRichTextEdit::OnFind(GFindReplaceCommon *Params)
{
	if (!Params || !d->Cursor)
	{
		LgiAssert(0);
		return false;
	}
	
	GAutoWString w(Utf8ToWide(Params->Find));
	int Idx = d->Blocks.IndexOf(d->Cursor->Blk);
	if (Idx < 0)
	{
		LgiAssert(0);
		return false;
	}

	for (unsigned n = 0; n < d->Blocks.Length(); n++)
	{
		int i = Idx + n;
		GRichTextPriv::Block *b = d->Blocks[i % d->Blocks.Length()];
		int At = n ? 0 : d->Cursor->Offset;
		int Result = b->FindAt(At, w, Params);
		if (Result >= At)
		{
			int Len = Strlen(w.Get());
			AutoCursor Sel(new BlkCursor(b, Result, -1));
			d->SetCursor(Sel, false);

			AutoCursor Cur(new BlkCursor(b, Result + Len, -1));
			return d->SetCursor(Cur, true);
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////// REPLACE
bool GRichTextEdit::DoReplace()
{
	return false;
}

bool GRichTextEdit::OnReplace(GFindReplaceCommon *Params)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////////////
void GRichTextEdit::SelectWord(int From)
{
	int Start, End, BlockIdx;
	GRichTextPriv::Block *b = d->GetBlockByIndex(From, &Start, &BlockIdx);
	if (!b)
		return;

	GArray<char16> Txt;
	if (!b->CopyAt(0, b->Length(), &Txt))
		return;

	End = Start;
	while (Start > 0 &&
			!IsWordBreakChar(Txt[Start-1]))
		Start--;
	
	while
	(
		End < b->Length()
		&&
		(
			End == Txt.Length()
			||
			!IsWordBreakChar(Txt[End])
		)
	)
		End++;

	AutoCursor c(new BlkCursor(b, Start, -1));
	d->SetCursor(c);
	c.Reset(new BlkCursor(b, End, -1));
	d->SetCursor(c, true);
}

bool GRichTextEdit::OnMultiLineTab(bool In)
{
	return false;
}

void GRichTextEdit::OnSetHidden(int Hidden)
{
}

void GRichTextEdit::OnPosChange()
{
	static bool Processing = false;

	if (!Processing)
	{
		Processing = true;
		GLayout::OnPosChange();

		// GRect c = GetClient();
		Processing = false;
	}
}

int GRichTextEdit::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	for (char *s = Formats.First(); s; )
	{
		if (!_stricmp(s, "text/uri-list") ||
			!_stricmp(s, "text/html") ||
			!_stricmp(s, "UniformResourceLocatorW"))
		{
			s = Formats.Next();
		}
		else
		{
			// LgiTrace("Ignoring format '%s'\n", s);
			Formats.Delete(s);
			DeleteArray(s);
			s = Formats.Current();
		}
	}

	return Formats.Length() ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

int GRichTextEdit::OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState)
{
	/* FIXME
	if (!_stricmp(Format, "text/uri-list") ||
		!_stricmp(Format, "text/html") ||
		!_stricmp(Format, "UniformResourceLocatorW"))
	{
		if (Data->IsBinary())
		{
			char16 *e = (char16*) ((char*)Data->Value.Binary.Data + Data->Value.Binary.Length);
			char16 *s = (char16*)Data->Value.Binary.Data;
			int len = 0;
			while (s < e && s[len])
			{
				len++;
			}
			// Insert(Cursor, s, len);
			Invalidate();
			return DROPEFFECT_COPY;
		}
	}
	*/

	return DROPEFFECT_NONE;
}

void GRichTextEdit::OnCreate()
{
	SetWindow(this);
	DropTarget(true);

	if (Focus())
		SetPulse(1500);
}

void GRichTextEdit::OnEscape(GKey &K)
{
}

bool GRichTextEdit::OnMouseWheel(double l)
{
	if (VScroll)
	{
		VScroll->Value(VScroll->Value() + (int64)l);
		Invalidate();
	}
	
	return true;
}

void GRichTextEdit::OnFocus(bool f)
{
	Invalidate();
	SetPulse(f ? 500 : -1);
}

int GRichTextEdit::HitTest(int x, int y)
{
	int Line = -1;
	return d->HitTest(x, y, Line);
}

void GRichTextEdit::Undo()
{
}

void GRichTextEdit::Redo()
{
}

void GRichTextEdit::DoContextMenu(GMouse &m)
{
	GMenuItem *i;
	GSubMenu RClick;
	GAutoString ClipText;
	{
		GClipBoard Clip(this);
		ClipText.Reset(NewStr(Clip.Text()));
	}

	#if LUIS_DEBUG
	RClick.AppendItem("Dump Layout", IDM_DUMP, true);
	RClick.AppendSeparator();
	#endif

	/*
	GStyle *s = HitStyle(HitText(m.x, m.y));
	if (s)
	{
		if (s->OnMenu(&RClick))
		{
			RClick.AppendSeparator();
		}
	}
	*/

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_CUT, "Cut"), IDM_CUT, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_PASTE, "Paste"), IDM_PASTE, ClipText != 0);
	RClick.AppendSeparator();

	#if 0
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_UNDO, "Undo"), IDM_UNDO, false /* UndoQue.CanUndo() */);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_REDO, "Redo"), IDM_REDO, false /* UndoQue.CanRedo() */);
	RClick.AppendSeparator();

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_FIXED, "Fixed Width Font"), IDM_FIXED, true);
	if (i) i->Checked(GetFixedWidthFont());
	#endif

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_AUTO_INDENT, "Auto Indent"), IDM_AUTO_INDENT, true);
	if (i) i->Checked(AutoIndent);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_SHOW_WHITESPACE, "Show Whitespace"), IDM_SHOW_WHITE, true);
	if (i) i->Checked(ShowWhiteSpace);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_HARD_TABS, "Hard Tabs"), IDM_HARD_TABS, true);
	if (i) i->Checked(HardTabs);
	
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_INDENT_SIZE, "Indent Size"), IDM_INDENT_SIZE, true);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_TAB_SIZE, "Tab Size"), IDM_TAB_SIZE, true);
	RClick.AppendItem("Copy Original", IDM_COPY_ORIGINAL, d->OriginalText.Get() != NULL);

	if (Environment)
		Environment->AppendItems(&RClick);

	int Id = 0;
	m.ToScreen();
	switch (Id = RClick.Float(this, m.x, m.y))
	{
		#if LUIS_DEBUG
		case IDM_DUMP:
		{
			int n=0;
			for (GTextLine *l=Line.First(); l; l=Line.Next(), n++)
			{
				LgiTrace("[%i] %i,%i (%s)\n", n, l->Start, l->Len, l->r.Describe());

				char *s = WideToUtf8(Text + l->Start, l->Len);
				if (s)
				{
					LgiTrace("%s\n", s);
					DeleteArray(s);
				}
			}
			break;
		}
		#endif
		case IDM_FIXED:
		{
			SetFixedWidthFont(!GetFixedWidthFont());							
			SendNotify(GNotifyFixedWidthChanged);
			break;
		}
		case IDM_CUT:
		{
			Cut();
			break;
		}
		case IDM_COPY:
		{
			Copy();
			break;
		}
		case IDM_PASTE:
		{
			Paste();
			break;
		}
		case IDM_UNDO:
		{
			Undo();
			break;
		}
		case IDM_REDO:
		{
			Redo();
			break;
		}
		case IDM_AUTO_INDENT:
		{
			AutoIndent = !AutoIndent;
			break;
		}
		case IDM_SHOW_WHITE:
		{
			ShowWhiteSpace = !ShowWhiteSpace;
			Invalidate();
			break;
		}
		case IDM_HARD_TABS:
		{
			HardTabs = !HardTabs;
			break;
		}
		case IDM_INDENT_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", IndentSize);
			GInput i(this, s, "Indent Size:", "Text");
			if (i.DoModal())
			{
				IndentSize = atoi(i.Str);
			}
			break;
		}
		case IDM_TAB_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", TabSize);
			GInput i(this, s, "Tab Size:", "Text");
			if (i.DoModal())
			{
				SetTabSize(atoi(i.Str));
			}
			break;
		}
		case IDM_COPY_ORIGINAL:
		{
			GClipBoard c(this);
			c.Text(d->OriginalText);
			break;
		}
		default:
		{
			/*
			if (s)
			{
				s->OnMenuClick(Id);
			}
			*/

			if (Environment)
			{
				Environment->OnMenu(this, Id, 0);
			}
			break;
		}
	}
}

void GRichTextEdit::OnMouseClick(GMouse &m)
{
	bool Processed = false;

	if (m.Down())
	{
		Focus(true);
		
		if (m.IsContextMenu())
		{
			DoContextMenu(m);
			return;
		}
		else
		{
			Focus(true);

			if (d->Areas[ToolsArea].Overlap(m.x, m.y) ||
				d->Areas[CapabilityArea].Overlap(m.x, m.y))
			{
				for (unsigned i=CapabilityBtn; i<MaxArea; i++)
				{
					if (d->Areas[i].Valid() &&
						d->Areas[i].Overlap(m.x, m.y))
					{
						Processed |= d->ClickBtn(m, (RectType)i);
					}
				}
				return;
			}
			else
			{
				d->WordSelectMode = !Processed && m.Double();

				AutoCursor c(new BlkCursor(NULL, 0, 0));
				GdcPt2 Doc = d->ScreenToDoc(m.x, m.y);
				int Idx = -1;
				if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx))
				{
					d->SetCursor(c, m.Shift());
					if (d->WordSelectMode)
						SelectWord(Idx);
				}
			}
		}
	}

	if (!Processed)
	{
		Capture(m.Down());
	}
}

int GRichTextEdit::OnHitTest(int x, int y)
{
	#ifdef WIN32
	if (GetClient().Overlap(x, y))
	{
		return HTCLIENT;
	}
	#endif
	return GView::OnHitTest(x, y);
}

void GRichTextEdit::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		AutoCursor c;
		GdcPt2 Doc = d->ScreenToDoc(m.x, m.y);
		int Idx = -1;
		if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx))
		{
			d->SetCursor(c, m.Left());

			if (d->WordSelectMode && d->Selection)
			{
				// Extend the selection to include the whole word
				if (!d->CursorFirst())
				{
					// Extend towards the end of the doc...
					GArray<char16> Txt;
					GRichTextPriv::Block *b = d->Selection->Blk;
					if (b->CopyAt(0, b->Length(), &Txt))
					{
						int Off = d->Cursor->Offset;
						while (Off < (int)Txt.Length() &&
							!IsWordBreakChar(Txt[Off]))
							Off++;
						if (Off != d->Cursor->Offset)
						{
							AutoCursor c(new BlkCursor(b, Off, -1));
							d->SetCursor(c, true);
						}
					}
				}
				else
				{
					// Extend towards the start of the doc...
					GArray<char16> Txt;
					GRichTextPriv::Block *b = d->Selection->Blk;
					if (b->CopyAt(0, b->Length(), &Txt))
					{
						int Off = d->Cursor->Offset;
						while (Off > 0 &&
							!IsWordBreakChar(Txt[Off-1]))
							Off--;
						if (Off != d->Cursor->Offset)
						{
							AutoCursor c(new BlkCursor(b, Off, -1));
							d->SetCursor(c, true);
						}
					}
				}
			}
		}
	}

	#ifdef WIN32
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		/*
		GStyle *s = HitStyle(Hit);
		TCHAR *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
		*/
	}
	#endif
}

bool GRichTextEdit::OnKey(GKey &k)
{
	if (k.Down() &&
		d->Cursor)
		d->Cursor->Blink = true;

	// k.Trace("GRichTextEdit::OnKey");
	if (k.IsContextMenu())
	{
		GMouse m;
		DoContextMenu(m);
	}
	else if (k.IsChar)
	{
		switch (k.c16)
		{
			default:
			{
				// process single char input
				if
				(
					!GetReadOnly()
					&&
					(
						(k.c16 >= ' ' || k.c16 == VK_TAB)
						&&
						k.c16 != 127
					)
				)
				{
					if (k.Down() &&
						d->Cursor &&
						d->Cursor->Blk)
					{
						// letter/number etc
						GRichTextPriv::Block *b = d->Cursor->Blk;
						if (b->AddText(d->Cursor->Offset, &k.c16, 1))
						{
							d->Cursor->Set(d->Cursor->Offset + 1);
							Invalidate();
							SendNotify(GNotifyDocChanged);
						}
					}
					return true;
				}
				break;
			}
			case VK_RETURN:
			{
				if (GetReadOnly())
					break;

				if (k.Down() && k.IsChar)
					OnEnter(k);

				return true;
			}
			case VK_BACKSPACE:
			{
				if (GetReadOnly())
					break;

				bool Changed = false;
				if (k.Ctrl())
				{
				    // Ctrl+H
				}
				else if (k.Down())
				{
					if (HasSelection())
					{
						DeleteSelection();
					}
					else if (d->Cursor &&
							 d->Cursor->Blk)
					{
						if (d->Cursor->Offset > 0)
						{
							Changed = d->Cursor->Blk->DeleteAt(d->Cursor->Offset-1, 1) > 0;
							if (Changed)
								d->Cursor->Set(d->Cursor->Offset - 1);
						}
						else
						{
							GRichTextPriv::Block *Prev = d->Prev(d->Cursor->Blk);
							if (Prev)
							{
								// Try and merge the two blocks...
								int Len = Prev->Length();
								d->Merge(Prev, d->Cursor->Blk);

								AutoCursor c(new BlkCursor(Prev, Len, -1));
								d->SetCursor(c);
							}
							else // at the start of the doc...
							{
								// Don't send the doc changed...
								return true;
							}
						}
					}
				}

				if (Changed)
				{
					Invalidate();
					SendNotify(GNotifyDocChanged);
				}
				return true;
			}
		}
	}
	else // not a char
	{
		switch (k.vkey)
		{
			case VK_TAB:
				return true;
			case VK_RETURN:
				return !GetReadOnly();
			case VK_BACKSPACE:
			{
				if (!GetReadOnly())
				{
					if (k.Alt())
					{
						if (k.Down())
						{
							if (k.Ctrl())
								Redo();
							else
								Undo();
						}
					}
					else if (k.Ctrl())
					{
						if (k.Down())
						{
							// Implement delete by word
							LgiAssert(!"Impl backspace by word");
						}
					}

					return true;
				}
				break;
			}
			case VK_F3:
			{
				if (k.Down())
					DoFindNext();
				return true;
			}
			case VK_LEFT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);
						
						AutoCursor c(new BlkCursor(d->CursorFirst() ? *d->Cursor : *d->Selection));
						d->SetCursor(c);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_StartOfLine;
						else
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GRichTextPriv::SkLeftWord : GRichTextPriv::SkLeftChar,
								k.Shift());
					}
				}
				return true;
			}
			case VK_RIGHT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);

						AutoCursor c(new BlkCursor(d->CursorFirst() ? *d->Selection : *d->Cursor));
						d->SetCursor(c);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_EndOfLine;
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GRichTextPriv::SkRightWord : GRichTextPriv::SkRightChar,
								k.Shift());
					}
				}
				return true;
			}
			case VK_UP:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageUp;
					#endif

					d->Seek(d->Cursor,
							GRichTextPriv::SkUpLine,
							k.Shift());
				}
				return true;
			}
			case VK_DOWN:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageDown;
					#endif

					d->Seek(d->Cursor,
							GRichTextPriv::SkDownLine,
							k.Shift());
				}
				return true;
			}
			case VK_END:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_EndOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GRichTextPriv::SkDocEnd : GRichTextPriv::SkLineEnd,
							k.Shift());
				}
				return true;
			}
			case VK_HOME:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_StartOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GRichTextPriv::SkDocStart : GRichTextPriv::SkLineStart,
							k.Shift());
				}
				return true;
			}
			case VK_PAGEUP:
			{
				#ifdef MAC
				GTextView4_PageUp:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GRichTextPriv::SkUpPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_PAGEDOWN:
			{
				#ifdef MAC
				GTextView4_PageDown:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GRichTextPriv::SkDownPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_INSERT:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						Copy();
					}
					else if (k.Shift())
					{
						if (!GetReadOnly())
						{
							Paste();
						}
					}
				}
				return true;
				break;
			}
			case VK_DELETE:
			{
				if (GetReadOnly())
					break;

				if (!k.Down())
					return true;

				bool Changed = false;
				GRichTextPriv::Block *b;
				if (HasSelection())
				{
					if (k.Shift())
						Changed |= Cut();
					else
						Changed |= DeleteSelection();
				}
				else if (d->Cursor &&
						(b = d->Cursor->Blk))
				{
					if (d->Cursor->Offset >= b->Length())
					{
						// Cursor is at the end of this block, pull the styles
						// from the next block into this one.
						GRichTextPriv::Block *next = d->Next(b);
						if (!next)
						{
							// No next block, therefor nothing to delete
							break;
						}

						// Try and merge the blocks
						if (d->Merge(b, next))
							Changed = true;
						else // move the cursor to the next block							
							d->Cursor.Reset(new GRichTextPriv::BlockCursor(b, 0, 0));
					}

					if (!Changed && b->DeleteAt(d->Cursor->Offset, 1))
					{
						if (b->Length() == 0)
						{
							GRichTextPriv::Block *n = d->Next(b);
							if (n)
							{
								d->Blocks.Delete(b, true);
								d->Cursor.Reset(new GRichTextPriv::BlockCursor(n, 0, 0));
							}
						}

						Changed = true;
					}
				}
						
				if (Changed)
				{
					Invalidate();
					SendNotify(GNotifyDocChanged);
				}
				return true;
			}
			default:
			{
				if (k.c16 == 17)
					break;

				if (k.c16 == ' ' &&
					k.Ctrl() &&
					k.Alt() &&
					d->Cursor &&
					d->Cursor->Blk)
				{
					if (k.Down())
					{
						// letter/number etc
						GRichTextPriv::Block *b = d->Cursor->Blk;
						char16 Nbsp[] = {0xa0};
						if (b->AddText(d->Cursor->Offset, Nbsp, 1))
						{
							d->Cursor->Set(d->Cursor->Offset + 1);
							Invalidate();
							SendNotify(GNotifyDocChanged);
						}
					}
					break;
				}

				if (k.Modifier() &&
					!k.Alt())
				{
					switch (k.GetChar())
					{
						case 0xbd: // Ctrl+'-'
						{
							/*
							if (k.Down() &&
								Font->PointSize() > 1)
							{
								Font->PointSize(Font->PointSize() - 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 0xbb: // Ctrl+'+'
						{
							/*
							if (k.Down() &&
								Font->PointSize() < 100)
							{
								Font->PointSize(Font->PointSize() + 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 'a':
						case 'A':
						{
							if (k.Down())
							{
								// select all
								SelectAll();
							}
							return true;
							break;
						}
						case 'y':
						case 'Y':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Redo();
								}
								return true;
							}
							break;
						}
						case 'z':
						case 'Z':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									if (k.Shift())
									{
										Redo();
									}
									else
									{
										Undo();
									}
								}
								return true;
							}
							break;
						}
						case 'x':
						case 'X':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Cut();
								}
								return true;
							}
							break;
						}
						case 'c':
						case 'C':
						{
							if (k.Shift())
								return false;
							
							if (k.Down())
								Copy();
							return true;
							break;
						}
						case 'v':
						case 'V':
						{
							if (!k.Shift() &&
								!GetReadOnly())
							{
								if (k.Down())
								{
									Paste();
								}
								return true;
							}
							break;
						}
						case 'f':
						{
							if (k.Down())
								DoFind();
							return true;
						}
						case 'g':
						case 'G':
						{
							if (k.Down())
							{
								DoGoto();
							}
							return true;
							break;
						}
						case 'h':
						case 'H':
						{
							if (k.Down())
							{
								DoReplace();
							}
							return true;
							break;
						}
						case 'u':
						case 'U':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									DoCase(k.Shift());
								}
								return true;
							}
							break;
						}
						case VK_RETURN:
						{
							if (!GetReadOnly() && !k.Shift())
							{
								if (k.Down())
								{
									OnEnter(k);
								}
								return true;
							}
							break;
						}
					}
				}
				break;
			}
		}
	}
	
	return false;
}

void GRichTextEdit::OnEnter(GKey &k)
{
	// enter
	if (HasSelection())
		DeleteSelection();

	if (d->Cursor &&
		d->Cursor->Blk)
	{
		GRichTextPriv::Block *b = d->Cursor->Blk;
		if (b->AddText(d->Cursor->Offset, L"\n", 1))
		{
			d->Cursor->Set(d->Cursor->Offset + 1);
			Invalidate();
		}
	}

	SendNotify(GNotifyDocChanged);
}

void GRichTextEdit::OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour)
{
	pDC->Colour(colour);
	pDC->Rectangle(&r);
}

void GRichTextEdit::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();

	#if 0
	pDC->Colour(GColour(255, 0, 255));
	pDC->Rectangle();
	#endif

	int FontY = GetFont()->GetHeight();

	GCssTools ct(d, d->Font);
	r = ct.PaintBorder(pDC, r);

	bool HasSpace = r.Y() > (FontY * 3);
	if (d->NeedsCap.Length() > 0 && HasSpace)
	{
		d->Areas[CapabilityArea] = r;
		d->Areas[CapabilityArea].y2 = d->Areas[CapabilityArea].y1 + 4 + ((FontY + 4) * d->NeedsCap.Length());
		r.y1 = d->Areas[CapabilityArea].y2 + 1;

		d->Areas[CapabilityBtn] = d->Areas[CapabilityArea];
		d->Areas[CapabilityBtn].Size(2, 2);
		d->Areas[CapabilityBtn].x1 = d->Areas[CapabilityBtn].x2 - 30;
	}
	else
	{
		d->Areas[CapabilityArea].ZOff(-1, -1);
		d->Areas[CapabilityBtn].ZOff(-1, -1);
	}

	if (d->ShowTools && HasSpace)
	{
		d->Areas[ToolsArea] = r;
		d->Areas[ToolsArea].y2 = d->Areas[ToolsArea].y1 + (FontY + 8) - 1;
		r.y1 = d->Areas[ToolsArea].y2 + 1;
	}
	else
	{
		d->Areas[ToolsArea].ZOff(-1, -1);
	}

	d->Areas[ContentArea] = r;

	if (d->Layout(VScroll))
		d->Paint(pDC, VScroll);
	// else the scroll bars changed, wait for re-paint
}

GMessage::Result GRichTextEdit::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CUT:
		{
			Cut();
			break;
		}
		case M_COPY:
		{
			Copy();
			break;
		}
		case M_PASTE:
		{
			Paste();
			break;
		}
		#if defined WIN32
		case WM_GETTEXTLENGTH:
		{
			return 0 /*Size*/;
		}
		case WM_GETTEXT:
		{
			int Chars = (int)MsgA(Msg);
			char *Out = (char*)MsgB(Msg);
			if (Out)
			{
				char *In = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), NameW(), LGI_WideCharset, Chars);
				if (In)
				{
					int Len = (int)strlen(In);
					memcpy(Out, In, Len);
					DeleteArray(In);
					return Len;
				}
			}
			return 0;
		}

		/* This is broken... the IME returns garbage in the buffer. :(
		case WM_IME_COMPOSITION:
		{
			if (Msg->b & GCS_RESULTSTR) 
			{
				HIMC hIMC = ImmGetContext(Handle());
				if (hIMC)
				{
					int Size = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
					char *Buf = new char[Size];
					if (Buf)
					{
						ImmGetCompositionString(hIMC, GCS_RESULTSTR, Buf, Size);

						char16 *Utf = (char16*)LgiNewConvertCp(LGI_WideCharset, Buf, LgiAnsiToLgiCp(), Size);
						if (Utf)
						{
							Insert(Cursor, Utf, StrlenW(Utf));
							DeleteArray(Utf);
						}

						DeleteArray(Buf);
					}

					ImmReleaseContext(Handle(), hIMC);
				}
				return 0;
			}
			break;
		}
		*/
		#endif
	}

	return GLayout::OnEvent(Msg);
}

int GRichTextEdit::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_VSCROLL && VScroll)
	{
		Invalidate(d->Areas + ContentArea);
	}

	return 0;
}

void GRichTextEdit::OnPulse()
{
	if (!ReadOnly && d->Cursor)
	{
		d->Cursor->Blink = !d->Cursor->Blink;
		d->InvalidateDoc(&d->Cursor->Pos);
	}
}

void GRichTextEdit::OnUrl(char *Url)
{
	if (Environment)
	{
		Environment->OnNavigate(this, Url);
	}
}

bool GRichTextEdit::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = 32;
	Inf.Width.Max = -1;

	// Inf.Height.Min = (Font ? Font->GetHeight() : 18) + 4;
	Inf.Height.Max = -1;

	return true;
}

#if _DEBUG
void GRichTextEdit::DumpNodes(GTree *Root)
{
	d->DumpNodes(Root);
}
#endif


///////////////////////////////////////////////////////////////////////////////
SelectColour::SelectColour(GRichTextPriv *priv, GdcPt2 p, GRichTextEdit::RectType t) : GPopup(priv->View)
{
	d = priv;
	Type = t;

	int Px = 16;
	int PxSp = Px + 2;
	int x = 6;
	int y = 6;

	// Do grey ramp
	for (int i=0; i<8; i++)
	{
		Entry &en = e.New();
		int Grey = i * 255 / 7;
		en.r.ZOff(Px-1, Px-1);
		en.r.Offset(x + (i * PxSp), y);
		en.c.Rgb(Grey, Grey, Grey);
	}

	// Do colours
	y += PxSp + 4;
	int SatRange = 255 - 64;
	int SatStart = 255 - 32;
	int HueStep = 360 / 8;
	for (int sat=0; sat<8; sat++)
	{
		for (int hue=0; hue<8; hue++)
		{
			GColour c;
			c.SetHLS(hue * HueStep, SatStart - ((sat * SatRange) / 7), 255);
			c.ToRGB();
			LgiTrace("c=%s\n", c.GetStr());

			Entry &en = e.New();
			en.r.ZOff(Px-1, Px-1);
			en.r.Offset(x + (hue * PxSp), y);
			en.c = c;
		}

		y += PxSp;
	}

	GRect r(0, 0, 12 + (8 * PxSp) - 1, y + 6 - 1);
	r.Offset(p.x, p.y);
	SetPos(r);

	Visible(true);
}

void SelectColour::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
	for (unsigned i=0; i<e.Length(); i++)
	{
		pDC->Colour(e[i].c);
		pDC->Rectangle(&e[i].r);
	}
}

void SelectColour::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		for (unsigned i=0; i<e.Length(); i++)
		{
			if (e[i].r.Overlap(m.x, m.y))
			{
				d->Values[Type] = (int64)e[i].c.c32();
				d->View->Invalidate(d->Areas + Type);
				d->OnStyleChange(Type);
				Visible(false);
				break;
			}
		}
	}
}

void SelectColour::Visible(bool i)
{
	GPopup::Visible(i);
	if (!i)
		delete this;
}

///////////////////////////////////////////////////////////////////////////////
class GRichTextEdit_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "GRichTextEdit") == 0)
		{
			return new GRichTextEdit(-1, 0, 0, 2000, 2000);
		}

		return 0;
	}
} RichTextEdit_Factory;
