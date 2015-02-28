#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GTextView3.h"
#include "GInput.h"
#include "GScrollBar.h"
#ifdef WIN32
#include <imm.h>
#endif
#include "GClipBoard.h"
#include "GDisplayString.h"
#include "GViewPriv.h"
#include "GCssTools.h"

#define DefaultCharset              "utf-8" // historically LgiAnsiToLgiCp()
#define SubtractPtr(a, b)			((a) - (b))

#define GDCF_UTF8					-1
#define LUIS_DEBUG					0
#define POUR_DEBUG					0
#define PROFILE_POUR				0

/*
#ifdef BEOS
#define DOUBLE_BUFFER_PAINT
#endif
*/

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

#define PAINT_BORDER				Back
#define PAINT_AFTER_LINE			Back

#define CODEPAGE_BASE				100
#define CONVERT_CODEPAGE_BASE		200

#if !defined(WIN32) && !defined(toupper)
#define toupper(c)					(((c)>='a'&&(c)<='z') ? (c)-'a'+'A' : (c))
#endif

static char SelectWordDelim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

//////////////////////////////////////////////////////////////////////
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

class GTextView3Private : public GCss
{
public:
	GTextView3 *View;
	GRect rPadding;
	int PourX;
	bool LayoutDirty;
	int DirtyStart, DirtyLen;
	bool SimpleDelete;
	GColour UrlColour;
	bool CenterCursor;
	int WordSelectMode;

	// Find/Replace Params
	bool OwnFindReplaceParams;
	GDocFindReplaceParams3 *FindReplaceParams;

	// Map buffer
	int MapLen;
	char16 *MapBuf;

	GTextView3Private(GTextView3 *view)
	{
		View = view;
		SimpleDelete = false;
		WordSelectMode = -1;
		PourX = -1;
		DirtyStart = DirtyLen = 0;
		UrlColour.Rgb(0, 0, 255);
		
		uint32 c24 = 0;
		if (_lgi_read_colour_config("colour.LC_URL", &c24))
			UrlColour.c24(c24);

		CenterCursor = false;
		
		LayoutDirty = true;
		rPadding.ZOff(0, 0);
		MapBuf = 0;
		MapLen = 0;
		
		OwnFindReplaceParams = true;
		FindReplaceParams = new GDocFindReplaceParams3;
	}
	
	~GTextView3Private()
	{
		if (OwnFindReplaceParams)
		{
			DeleteObj(FindReplaceParams);
		}
		
		DeleteArray(MapBuf);
	}
	
	void SetDirty(int Start, int Len = 0)
	{
		LayoutDirty = true;
		DirtyStart = Start;
		DirtyLen = Len;
	}

	void OnChange(PropType Prop)
	{
		if (Prop == GCss::PropPadding ||
			Prop == GCss::PropPaddingLeft ||
			Prop == GCss::PropPaddingRight ||
			Prop == GCss::PropPaddingTop ||
			Prop == GCss::PropPaddingBottom)
		{
			GCssTools t(this, View->GetFont());
			rPadding.ZOff(0, 0);
			rPadding = t.ApplyPadding(rPadding);
		}
	}
};

//////////////////////////////////////////////////////////////////////
enum UndoType
{
	UndoDelete, UndoInsert, UndoChange
};

class GTextView3Undo : public GUndoEvent
{
	GTextView3 *View;
	UndoType Type;
	int At;
	char16 *Text;

public:
	GTextView3Undo(	GTextView3 *view,
					char16 *t,
					int len,
					int at,
					UndoType type)
	{
		View = view;
		Type = type;
		At = at;
		Text = NewStrW(t, len);
	}

	~GTextView3Undo()
	{
		DeleteArray(Text);
	}

	void OnChange()
	{
		int Len = StrlenW(Text);
		if (View->Text)
		{
			char16 *t = View->Text + At;
			for (int i=0; i<Len; i++)
			{
				char16 n = Text[i];
				Text[i] = t[i];
				t[i] = n;
			}
		}

		View->d->SetDirty(At, Len);
	}

	// GUndoEvent
    void ApplyChange()
	{
		View->UndoOn = false;

		switch (Type)
		{
			case UndoInsert:
			{
				int Len = StrlenW(Text);
				View->Insert(At, Text, Len);
				View->Cursor = At + Len;
				break;
			}
			case UndoDelete:
			{
				View->Delete(At, StrlenW(Text));
				View->Cursor = At;
				break;
			}
			case UndoChange:
			{
				OnChange();
				break;
			}
		}

		View->UndoOn = true;
		View->Invalidate();
	}

    void RemoveChange()
	{
		View->UndoOn = false;

		switch (Type)
		{
			case UndoInsert:
			{
				View->Delete(At, StrlenW(Text));
				break;
			}
			case UndoDelete:
			{
				View->Insert(At, Text, StrlenW(Text));
				break;
			}
			case UndoChange:
			{
				OnChange();				
				break;
			}
		}

		View->Cursor = At;
		View->UndoOn = true;
		View->Invalidate();
	}
};

void GTextView3::GStyle::RefreshLayout(int Start, int Len)
{
	View->PourText(Start, Len);
	View->PourStyle(Start, Len);
}

//////////////////////////////////////////////////////////////////////
GTextView3::GTextView3(	int Id,
						int x, int y, int cx, int cy,
						GFontType *FontType)
	: ResObject(Res_Custom)
{
	// init vars
	GView::d->Css.Reset(d = new GTextView3Private(this));
	
	LineY = 1;
	MaxX = 0;
	Blink = true;
	TextCache = 0;
	UndoOn = true;
	Font = 0;
	FixedWidthFont = false;
	FixedFont = 0;
	ShowWhiteSpace = false;
	ObscurePassword = false;
	TabSize = TAB_SIZE;
	IndentSize = TAB_SIZE;
	HardTabs = true;
	CanScrollX = false;

	// setup window
	SetId(Id);

	// default options
	Dirty = false;
	#if WINNATIVE
	CrLf = true;
	SetDlgCode(DLGC_WANTALLKEYS);
	#else
	CrLf = false;
	#endif
	Underline = 0;
	BackColour = LC_WORKSPACE;
	d->Padding(GCss::Len(GCss::LenPx, 2));

	#ifdef _DEBUG
	// debug times
	_PourTime = 0;
	_StyleTime = 0;
	_PaintTime = 0;
	#endif

	// Data
	Alloc = ALLOC_BLOCK;
	Text = new char16[Alloc];
	if (Text) *Text = 0;
	Cursor = 0;
	Size = 0;

	// Display
	SelStart = SelEnd = -1;
	DocOffset = 0;
	ScrollX = 0;

	if (FontType)
	{
		Font = FontType->Create();
	}
	else
	{
		GFontType Type;
		if (Type.GetSystemFont("Fixed"))
		{
			Font = Type.Create();
		}
		else printf("%s:%i - failed to create font.\n", __FILE__, __LINE__);
	}
	if (!Font)
	{
		LgiTrace("%s:%i - Failed to create font, FontType=%p\n", _FL, FontType);
		Font = SysFont;
	}
	if (Font)
	{
		// Font->PointSize(Font->PointSize() + 2);

		SetTabStop(true);

		Underline = new GFont;
		if (Underline)
		{
			*Underline = *Font;
			Underline->Underline(true);
			if (d->UrlColour.IsValid())
				Underline->Fore(d->UrlColour.c24());
			Underline->Create();
		}

		OnFontChange();
	}

	CursorPos.ZOff(1, LineY-1);
	CursorPos.Offset(d->rPadding.x1, d->rPadding.y1);

	GRect r;
	r.ZOff(cx-1, cy-1);
	r.Offset(x, y);
	SetPos(r);
}

GTextView3::~GTextView3()
{
	Line.DeleteObjects();
	Style.DeleteObjects();

	DeleteArray(TextCache);
	DeleteArray(Text);

	if (Font != SysFont) DeleteObj(Font);
	DeleteObj(FixedFont);
	DeleteObj(Underline);
	// 'd' is owned by the GView::Css auto ptr
}

char16 *GTextView3::MapText(char16 *Str, int Len, bool RtlTrailingSpace)
{
	if (ObscurePassword || ShowWhiteSpace || RtlTrailingSpace)
	{
		if (Len > d->MapLen)
		{
			DeleteArray(d->MapBuf);
			d->MapBuf = new char16[Len + RtlTrailingSpace];
			d->MapLen = Len;
		}

		if (d->MapBuf)
		{
			int n = 0;

			if (RtlTrailingSpace)
			{
				d->MapBuf[n++] = ' ';
				for (int i=0; i<Len; i++)
				{
					d->MapBuf[n++] = Str[i];
				}
			}
			else if (ObscurePassword)
			{
				for (int i=0; i<Len; i++)
				{
					d->MapBuf[n++] = '*';
				}
			}
			else if (ShowWhiteSpace)
			{
				for (int i=0; i<Len; i++)
				{
					if (Str[i] == ' ')
					{
						d->MapBuf[n++] = 0xb7;
					}
					else if (Str[i] == '\t')
					{
						d->MapBuf[n++] = 0x2192;
					}
					else
					{
						d->MapBuf[n++] = Str[i];
					}
				}
			}

			return d->MapBuf;
		}
	}

	return Str;
}

void GTextView3::SetFixedWidthFont(bool i)
{
	if (FixedWidthFont ^ i)
	{
		if (i)
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				GFont *f = FixedFont;
				FixedFont = Font;
				Font = f;

				if (!Font)
				{
					Font = Type.Create();
					if (Font)
					{
						Font->PointSize(FixedFont->PointSize());
					}
				}

				GDocView::SetFixedWidthFont(i);
			}
		}
		else if (FixedFont)
		{
			GFont *f = FixedFont;
			FixedFont = Font;
			Font = f;
			GDocView::SetFixedWidthFont(i);
		}

		OnFontChange();
		Invalidate();
	}
}

void GTextView3::SetReadOnly(bool i)
{
	GDocView::SetReadOnly(i);

	#if WINNATIVE
	SetDlgCode(i ? DLGC_WANTARROWS : DLGC_WANTALLKEYS);
	#endif
}

void GTextView3::SetTabSize(uint8 i)
{
	TabSize = limit(i, 2, 32);
	OnFontChange();
	OnPosChange();
	Invalidate();
}

void GTextView3::SetWrapType(uint8 i)
{
	GDocView::SetWrapType(i);
	CanScrollX = i != TEXTED_WRAP_REFLOW;

	OnPosChange();
	Invalidate();
}

GFont *GTextView3::GetFont()
{
	return Font;
}

void GTextView3::SetFont(GFont *f, bool OwnIt)
{
	if (!f)
		return;
	
	if (OwnIt)
	{
		DeleteObj(Font);
		Font = f;
	}
	else if (!Font)
	{
		Font = new GFont(*f);
	}
	else
	{
		*Font = *f;
	}

	if (Font)
	{
		if (!Underline)
			Underline = new GFont;
		*Underline = *Font;
		Underline->Underline(true);
		
		if (d->UrlColour.IsValid())
			Underline->Fore(d->UrlColour.c24());
	}

	OnFontChange();
}

void GTextView3::OnFontChange()
{
	if (Font)
	{
		// get line height
		// int OldLineY = LineY;
		if (!Font->Handle())
			Font->Create();
		LineY = Font->GetHeight();
		if (LineY < 1) LineY = 1;

		// get tab size
		char Spaces[32];
		memset(Spaces, 'A', TabSize);
		Spaces[TabSize] = 0;
		GDisplayString ds(Font, Spaces);
		Font->TabSize(ds.X());

		// repour doc
		d->SetDirty(0, Size);

		// validate blue underline font
		if (Underline)
		{
			*Underline = *Font;
			Underline->Underline(true);
			Underline->Create();
		}

		#if WINNATIVE // Set the IME font.
		HIMC hIMC = ImmGetContext(Handle());
		if (hIMC)
		{
			COMPOSITIONFORM Cf;						
			Cf.dwStyle = CFS_POINT;
			Cf.ptCurrentPos.x = CursorPos.x1;
			Cf.ptCurrentPos.y = CursorPos.y1;

			LOGFONT FontInfo;
			GetObject(Font->Handle(), sizeof(FontInfo), &FontInfo);
			ImmSetCompositionFont(hIMC, &FontInfo);

			ImmReleaseContext(Handle(), hIMC);
		}
		#endif

	}
}

void GTextView3::PourText(int Start, int Length /* == 0 means it's a delete */)
{
	#if PROFILE_POUR
	int StartTime = LgiCurrentTime();
	#endif

	GRect Client = GetClient();
	int Mx = Client.X() - d->rPadding.x1 - d->rPadding.x2;

	MaxX = 0;

	int Cx = 0, Cy = 0;
	bool SimplePour = d->SimpleDelete; // One line change
	int CurrentLine = -1;
	if (d->SimpleDelete || Start)
	{
		d->SimpleDelete = false;
		if (!SimplePour &&
			WrapType == TEXTED_WRAP_NONE &&
			Length > 0 &&
			Length < 100)
		{
			SimplePour = true;
			for (int i=0; i<Length; i++)
			{
				if (Text[Start + i] == '\n')
				{
					SimplePour = false;
					break;
				}
			}
		}
		
		// Get the line of the change
		GTextLine *Current = GetTextLine(Start, &CurrentLine);
		
		LgiAssert(Current != 0);
		LgiAssert(CurrentLine >= 0);
		
		if (!Current)
		{
			SimplePour = false;
		}
		else
		{		
			Start = Current->Start;
			Cy = Current->r.y1;
			
			if (SimplePour)
			{
				#if POUR_DEBUG
				printf("SimplePour Start=%i Length=%i CurrentLine=%i\n", Start, Length, CurrentLine);
				#endif
				Line.Delete(Current);
				DeleteObj(Current);
			}
			else
			{
				#if POUR_DEBUG
				printf("PartialPour Start=%i Length=%i\n", Start, Length);
				#endif
				bool Done = false;
				for (GTextLine *l=Line.Last(); l && !Done; l=Line.Last())
				{
					Done = l == Current;
					Line.Delete(l);
					DeleteObj(l);
				}
			}
		}
	}
	else
	{
		// Whole doc is dirty
		#if POUR_DEBUG
		printf("WholePour Start=%i Length=%i\n", Start, Length);
		#endif
		Start = 0;
		Line.DeleteObjects();
	}

	if (Text && Font && Mx > 0)
	{
		// break array, break out of loop when we hit these chars
		#define ExitLoop(c)	(	(c) == 0 ||								\
								(c) == '\n' ||							\
								(c) == ' ' ||							\
								(c) == '\t'								\
							)

		// extra breaking oportunities
		#define ExtraBreak(c) (	( (c) >= 0x3040 && (c) <= 0x30FF ) ||	\
								( (c) >= 0x3300 && (c) <= 0x9FAF )		\
							)

		// tracking vars
		int e;
		int LastX = 0;
		int LastChar = Start;
		int WrapCol = GetWrapAtCol();

		GDisplayString Sp(Font, " ", 1);
		int WidthOfSpace = Sp.X();
		if (WidthOfSpace < 1)
		{
			printf("%s:%i - WidthOfSpace test failed.\n", _FL);
			return;
		}

		// alright... lets pour!
		for (int i=Start; i<Size; i = e)
		{
			// seek till next char of interest
			// int Chars = 0;

			if (WrapType == TEXTED_WRAP_NONE)
			{
				// Seek line/doc end
				char16 *End;
				for (End = Text + i; *End && *End != '\n'; End++);
				e = (int)SubtractPtr(End, Text);
				GDisplayString ds(Font, Text + i, e - i);
				Cx = ds.X();

				// Alloc line
				GTextLine *l = new GTextLine;
				if (l)
				{
					l->Start = LastChar;
					l->r.x1 = d->rPadding.x1;
					l->Len = e - LastChar;
					l->r.x2 = l->r.x1 + Cx;
					LastChar = ++e;

					l->r.y1 = Cy;
					l->r.y2 = l->r.y1 + LineY - 1;

					Line.Insert(l, SimplePour ? CurrentLine : -1);

					MaxX = max(MaxX, l->r.X());
					LastX = Cx = 0;
					Cy += LineY;
				}
				else break;
				
				if (SimplePour)
				{
					for (l=Line[++CurrentLine]; l; l=Line.Next())
					{
						l->Start += Length;
					}
					break;
				}
			}
			else
			{
				e = i;
				int	Width = 0;

				// Find break point
				if (WrapCol)
				{
					// Wrap at column
					
					// Find the end of line
					while (true)
					{
						if (e >= Size ||
							Text[e] == '\n' ||
							(e-i) >= WrapCol)
						{
							break;
						}

						e++;
					}

					// Seek back some characters if we are mid word
					int OldE = e;
					if (e < Size &&
						Text[e] != '\n')
					{
						while (e > i)
						{
							if (ExitLoop(Text[e]) ||
								ExtraBreak(Text[e]))
							{
								break;
							}

							e--;
						}
					}

					if (e == i)
					{
						// No line break at all, so seek forward instead
						for (e=OldE; e < Size && Text[e] != '\n'; e++)
						{
							if (ExitLoop(Text[e]) ||
								ExtraBreak(Text[e]))
								break;
						}
					}

					// Calc the width
					GDisplayString ds(Font, Text + i, e - i);
					Width = ds.X();
				}
				else
				{
					// Wrap to edge of screen
					int PrevExitChar = -1;
					int PrevX = -1;

					while (true)
					{
						if (e >= Size ||
							ExitLoop(Text[e]) ||
							ExtraBreak(Text[e]))
						{
							GDisplayString ds(Font, Text + i, e - i);
							if (ds.X() + Cx > Mx)
							{
								if (PrevExitChar > 0)
								{
									e = PrevExitChar;
									Width = PrevX;
								}
								else
								{
									Width = ds.X();
								}
								break;
							}
							else if (e >= Size ||
									Text[e] == '\n')							
							{
								Width = ds.X();
								break;
							}
							
							PrevExitChar = e;
							PrevX = ds.X();
						}
						
						e++;
					}
				}

				// Create layout line
				GTextLine *l = new GTextLine;
				if (l)
				{
					l->Start = i;
					l->Len = e - i;
					l->r.x1 = d->rPadding.x1;
					l->r.x2 = l->r.x1 + Width - 1;

					l->r.y1 = Cy;
					l->r.y2 = l->r.y1 + LineY - 1;

					Line.Insert(l);
					
					MaxX = max(MaxX, l->r.X());
					Cy += LineY;
					
					if (e < Size)
						e++;
				}
			}
		}
	}

	GTextLine *Last = Line.Length() ? Line.Last() : 0;
	if (!Last ||
		Last->Start + Last->Len < Size)
	{
		GTextLine *l = new GTextLine;
		if (l)
		{
			l->Start = Size;
			l->Len = 0;
			l->r.x1 = l->r.x2 = d->rPadding.x1;
			l->r.y1 = Cy;
			l->r.y2 = l->r.y1 + LineY - 1;

			Line.Insert(l);
			
			MaxX = max(MaxX, l->r.X());
			Cy += LineY;
		}
	}

	bool ScrollYNeeded = Client.Y() < (Line.Length() * LineY);
	SetScrollBars(false, ScrollYNeeded);
	d->LayoutDirty = false;
	UpdateScrollBars();
	
	#if PROFILE_POUR
	int _PourTime = LgiCurrentTime() - StartTime;
	printf("TextPour: %i ms, %i lines\n", _PourTime, Line.Length());
	#endif
	#ifdef _DEBUG
	if (GetWindow())
	{
		static char s[256];
		sprintf_s(s, sizeof(s), "Pour: %.2f sec", (double)_PourTime / 1000);
		GetWindow()->PostEvent(M_TEXTVIEW_DEBUG_TEXT, (GMessage::Param)s);
	}
	#endif
//	printf("PourTime=%ims\n", _PourTime);

	#if POUR_DEBUG
	printf("Lines=%i\n", Line.Length());
	int Index = 0;
	for (GTextLine *l=Line.First(); l; l=Line.Next(), Index++)
	{
		printf("\t[%i] %i,%i (%s)\n", Index, l->Start, l->Len, l->r.Describe());
	}
	#endif
}

bool GTextView3::InsertStyle(GAutoPtr<GStyle> s)
{
	if (!s)
		return false;

	LgiAssert(s->Start >= 0);
	LgiAssert(s->Len > 0);
	int Last = 0;
	int n = 0;

	for (GStyle *i=Style.First(); i; i=Style.Next(), n++)
	{
		if (s->Overlap(i))
		{
			if (s->Owner > i->Owner)
			{
				// Fail the insert
				return false;
			}
			else
			{
				// Replace mode...
				Style.Delete(i);
				Style.Insert(s.Release(), n);
				return true;
			}
		}

		if (s->Start >= Last && s->Start < i->Start)
		{
			Style.Insert(s.Release(), n);
			return true;
		}

		Last = i->Start;
	}

	Style.Insert(s.Release());
	return true;
}

GTextView3::GStyle *GTextView3::GetNextStyle(int Where)
{
	GStyle *s = (Where >= 0) ? Style.First() : Style.Next();
	while (s)
	{
		// determin whether style is relevent..
		// styles in the selected region are ignored
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);
		if (SelStart >= 0 &&
			s->Start >= Min &&
			s->Start+s->Len < Max)
		{
			// style is completely inside selection: ignore
			s = Style.Next();
		}
		else if (Where >= 0 &&
			s->Start+s->Len < Where)
		{
			s = Style.Next();
		}
		else
		{
			return s;
		}
	}

	return 0;
}

class GUrl : public GTextView3::GStyle
{
public:
	bool Email;

	GUrl(int own) : GStyle(own)
	{
		Email = false;
	}

	bool OnMouseClick(GMouse *m)
	{
		if (View)
		{
			if ( (m && m->Left() && m->Double()) || (!m) )
			{
				char *Utf8 = LgiNewUtf16To8(View->NameW() + Start, Len * sizeof(char16));
				if (Utf8)
				{
					View->OnUrl(Utf8);
					DeleteArray(Utf8);
				}
				
				return true;
			}
		}

		return false;
	}

	bool OnMenu(GSubMenu *m)
	{
		if (m)
		{
			if (Email)
			{
				m->AppendItem(LgiLoadString(L_TEXTCTRL_EMAIL_TO, "New Email to..."), IDM_NEW, true);
			}
			else
			{
				m->AppendItem(LgiLoadString(L_TEXTCTRL_OPENURL, "Open URL"), IDM_OPEN, true);
			}

			m->AppendItem(LgiLoadString(L_TEXTCTRL_COPYLINK, "Copy link location"), IDM_COPY_URL, true);

			return true;
		}

		return false;
	}

	void OnMenuClick(int i)
	{
		switch (i)
		{
			case IDM_NEW:
			case IDM_OPEN:
			{
				OnMouseClick(0);
				break;
			}
			case IDM_COPY_URL:
			{
				char *Url = LgiNewUtf16To8(View->NameW() + Start, Len * sizeof(char16));
				if (Url)
				{
					GClipBoard Clip(View);
					Clip.Text(Url);
					DeleteArray(Url);
				}
				break;
			}
		}
	}

	TCHAR *GetCursor()
	{
		#ifdef WIN32
		GArray<int> Ver;
		int Os = LgiGetOs(&Ver);
		if ((Os == LGI_OS_WIN32 || Os == LGI_OS_WIN64) &&
			Ver[0] >= 5)
		{
			return MAKEINTRESOURCE(32649); // hand
		}
		else
		{
			return IDC_ARROW;
		}
		#endif
		return 0;
	}
};

GTextView3::GStyle *GTextView3::HitStyle(int i)
{
	for (GStyle *s=Style.First(); s; s=Style.Next())
	{
		if (i >= s->Start && i < s->Start+s->Len)
		{
			return s;
		}
	}

	return 0;	
}

void GTextView3::PourStyle(int Start, int EditSize)
{
	#ifdef _DEBUG
	int64 StartTime = LgiCurrentTime();
	#endif

	if (!Text || Size < 1)
		return;

	int Length = max(EditSize, 0);

	// Expand re-style are to word boundaries before and after the area of change
	while (Start > 0 && UrlChar(Text[Start-1]))
	{
		// Move the start back
		Start--;
		Length++;
	}
	while (Start + Length < Size && UrlChar(Text[Start+Length]))
	{
		// Move the end back
		Length++;
	}

	// Delete all the styles that we own inside the changed area
	for (GStyle *s = Style.First(); s; )
	{
		if (s->Owner == 0)
		{
			if (EditSize > 0)
			{
				if (s->Start > Start)
				{
					s->Start += EditSize;
				}

				if (s->Overlap(Start, abs(EditSize)))
				{
					Style.Delete();
					DeleteObj(s);
					s = Style.Current();
					continue;
				}
			}
			else
			{
				if (s->Overlap(Start, -EditSize))
				{
					Style.Delete();
					DeleteObj(s);
					s = Style.Current();
					continue;
				}
				
				if (s->Start > Start)
				{
					s->Start += EditSize;
				}
			}
		}
		
		s = Style.Next();
	}

	if (UrlDetect)
	{
		#if 1
		
		GArray<GLinkInfo> Links;
		if (LgiDetectLinks(Links, Text + Start, Length))
		{
			for (uint32 i=0; i<Links.Length(); i++)
			{
				GLinkInfo &Inf = Links[i];
				GUrl *Url;
                GAutoPtr<GTextView3::GStyle> a(Url = new GUrl(0));
				if (Url)
				{
					Url->View = this;
					Url->Start = (int) (Inf.Start + Start);
					Url->Len = Inf.Len;
					Url->Email = Inf.Email;
					Url->Font = Underline;
					Url->c = d->UrlColour;

					InsertStyle(a);
				}
			}
		}

		#else

		char16 Http[] = {'h', 't', 't', 'p', ':', '/', '/', 0 };
		char16 Https[] = {'h', 't', 't', 'p', 's', ':', '/', '/', 0};

		for (int i=0; i<Size; i++)
		{
			switch (Text[i])
			{
				case 'h':
				case 'H':
				{
					if (StrnicmpW(Text+i, Http, 6) == 0 ||
						StrnicmpW(Text+i, Https, 7) == 0)
					{
						// find end
						char16 *s = Text + i;
						char16 *e = s + 6;
						for ( ; (SubtractPtr(e, Text) < Size) && 
								UrlChar(*e); e++);
						
						while
						(
							e > s &&
							!
							(
								IsAlpha(e[-1]) ||
								IsDigit(e[-1]) ||
								e[-1] == '/'
							)
						)
							e--;

						GUrl *Url = new GUrl(0);
						if (Url)
						{
							Url->Email = false;
							Url->View = this;
							Url->Start = SubtractPtr(s, Text);
							Url->Len = SubtractPtr(e, s);
							Url->Font = Underline;
							Url->c = d->UrlColour;

							InsertStyle(Url);
						}
						i = SubtractPtr(e, Text);
					}
					break;
				}
				case '@':
				{
					// find start
					char16 *s = Text + (max(i, 1) - 1);
					
					for ( ; s > Text && EmailChar(*s); s--)
						;

					if (s < Text + i)
					{
						if (!EmailChar(*s))
							s++;

						bool FoundDot = false;
						char16 *Start = Text + i + 1;
						char16 *e = Start;
						for ( ; (SubtractPtr(e, Text) < Size) && 
								EmailChar(*e); e++)
						{
							if (*e == '.') FoundDot = true;
						}
						while (e > Start && e[-1] == '.') e--;

						if (FoundDot)
						{
							GUrl *Url = new GUrl(0);
							if (Url)
							{
								Url->Email = true;
								Url->View = this;
								Url->Start = SubtractPtr(s, Text);
								Url->Len = SubtractPtr(e, s);
								Url->Font = Underline;
								Url->c = d->UrlColour;

								InsertStyle(Url);
							}
							i = SubtractPtr(e, Text);
						}
					}
					break;
				}
			}
		}

		#endif
	}

	#ifdef _DEBUG
	_StyleTime = LgiCurrentTime() - StartTime;
	#endif
}

bool GTextView3::Insert(int At, char16 *Data, int Len)
{
	if (!ReadOnly && Len > 0)
	{
		// limit input to valid data
		At = min(Size, At);

		// make sure we have enough memory
		int NewAlloc = Size + Len + 1;
		NewAlloc += ALLOC_BLOCK - (NewAlloc % ALLOC_BLOCK);
		if (NewAlloc != Alloc)
		{
			char16 *NewText = new char16[NewAlloc];
			if (NewText)
			{
				if (Text)
				{
					// copy any existing data across
					memcpy(NewText, Text, (Size + 1) * sizeof(char16));
				}

				DeleteArray(Text);
				Text = NewText;
				Alloc = NewAlloc;
			}
			else
			{
				// memory allocation error
				return false;
			}
		}

		if (Text)
		{
			// insert the data
			// move the section after the insert
			memmove(Text+(At+Len), Text+At, (Size-At) * sizeof(char16));

			if (Data)
			{
				// copy new data in
				if (UndoOn)
				{
					UndoQue += new GTextView3Undo(this, Data, Len, At, UndoInsert);
				}

				memcpy(Text+At, Data, Len * sizeof(char16));
				Size += Len;
			}
			else
			{
				return false;
			}

			Text[Size] = 0;
			Dirty = true;
			PourText(At, Len);
			PourStyle(At, Len);
			SendNotify(GTVN_DOC_CHANGED);

			return true;
		}
	}

	return false;
}

bool GTextView3::Delete(int At, int Len)
{
	bool Status = false;

	if (!ReadOnly)
	{
		// limit input
		At = max(At, 0);
		At = min(At, Size);
		Len = min(Size-At, Len);

		if (Len > 0)
		{
			bool HasNewLine = false;
			for (int i=0; i<Len; i++)
			{
				if (Text[At + i] == '\n')
				{
					HasNewLine = true;
					break;
				}
			}

			int PrevLineStart = -1;
			int NextLineStart = -1;
			if (WrapType == TEXTED_WRAP_NONE)
			{
				d->SimpleDelete = !HasNewLine;
			}
			else
			{
				int Index;
				GTextLine *Cur = GetTextLine(At, &Index);
				if (Cur)
				{
					GTextLine *Prev = Line[Index-1];
					PrevLineStart = Prev ? Prev->Start : -1;
					GTextLine *Next = Line[Index+1];
					NextLineStart = Next ? Next->Start : -1;
				}
			}
			
			// do delete
			if (UndoOn)
			{
				UndoQue += new GTextView3Undo(this, Text+At, Len, At, UndoDelete);
			}

			memmove(Text+At, Text+(At+Len), (Size-At-Len) * sizeof(char16));
			Size -= Len;
			Text[Size] = 0;

			Dirty = true;
			Status = true;
			PourText(At, -Len);
			PourStyle(At, -Len);
			
			if (Cursor >= At && Cursor <= At + Len)
			{
				SetCursor(At, false, HasNewLine);
			}

			// Handle repainting in flowed mode, when the line starts change
			if (WrapType == TEXTED_WRAP_REFLOW)
			{
				int Index;
				GTextLine *Cur = GetTextLine(At, &Index);
				if (Cur)
				{
					GTextLine *Repaint = 0;
					GTextLine *Prev = Line[Index-1];
					if (Prev && PrevLineStart != Prev->Start)
					{
						// Paint previous line down
						Repaint = Prev;
					}
					else
					{
						GTextLine *Next = Line[Index+1];
						if (Next && NextLineStart != Next->Start)
						{
							// Paint next line down
							Repaint = Next;
						}
					}
					
					if (Repaint)
					{
						GRect r = Repaint->r;
						r.x2 = GetClient().x2;
						r.y2 = GetClient().y2;
						Invalidate(&r);
					}
				}
			}

			SendNotify(GTVN_DOC_CHANGED);
			Status = true;
		}
	}

	return Status;
}

void GTextView3::DeleteSelection(char16 **Cut)
{
	if (SelStart >= 0)
	{
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);

		if (Cut)
		{
			*Cut = NewStrW(Text + Min, Max - Min);
		}

		Delete(Min, Max - Min);
		SetCursor(Min, false, true);
	}
}

GTextView3::GTextLine *GTextView3::GetTextLine(int Offset, int *Index)
{
	int i = 0;
	for (GTextLine *l=Line.First(); l; l=Line.Next(), i++)
	{
		if (Offset >= l->Start && Offset <= l->Start+l->Len)
		{
			if (Index)
			{
				*Index = i;
			}

			return l;
		}
	}

	return 0;
}

int64 GTextView3::Value()
{
	char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void GTextView3::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LGI_PrintfInt64, i);
	Name(Str);
}

char *GTextView3::Name()
{
	UndoQue.Empty();
	DeleteArray(TextCache);
	TextCache = LgiNewUtf16To8(Text);

	return TextCache;
}

bool GTextView3::Name(const char *s)
{
	UndoQue.Empty();
	DeleteArray(TextCache);
	DeleteArray(Text);

	LgiAssert(LgiIsUtf8(s));
	Text = LgiNewUtf8To16(s);
	if (!Text)
	{
		Text = new char16[1];
		if (Text) *Text = 0;
	}

	Size = Text ? StrlenW(Text) : 0;
	Alloc = Size + 1;
	Cursor = min(Cursor, Size);
	if (Text)
	{
		// Remove '\r's
		char16 *o = Text;
		for (char16 *i=Text; *i; i++)
		{
			if (*i != '\r')
			{
				*o++ = *i;
			}
			else Size--;
		}
		*o++ = 0;
	}

	// update everything else
	PourText(0, Size);
	PourStyle(0, Size);
	UpdateScrollBars();
	Invalidate();

	return true;
}

char16 *GTextView3::NameW()
{
	return Text;
}

bool GTextView3::NameW(const char16 *s)
{
	DeleteArray(Text);
	Size = s ? StrlenW(s) : 0;
	Alloc = Size + 1;
	Text = new char16[Alloc];
	Cursor = min(Cursor, Size);
	if (Text)
	{
		memcpy(Text, s, Size * sizeof(char16));

		// remove LF's
		int In = 0, Out = 0;
		CrLf = false;
		for (; In<Size; In++)
		{
			if (Text[In] != '\r')
			{
				Text[Out++] = Text[In];
			}
			else
			{
				CrLf = true;
			}
		}
		Size = Out;
		Text[Size] = 0;

	}

	// update everything else
	PourText(0, Size);
	PourStyle(0, Size);
	UpdateScrollBars();
	Invalidate();

	return true;
}

char *GTextView3::GetSelection()
{
	if (HasSelection())
	{
		int Start = min(SelStart, SelEnd);
		int End = max(SelStart, SelEnd);
		return (char*)LgiNewConvertCp("utf-8", Text + Start, "utf-16", (End-Start)*sizeof(Text[0]) );
	}

	return 0;
}

bool GTextView3::HasSelection()
{
	return (SelStart >= 0) && (SelStart != SelEnd);
}

void GTextView3::SelectAll()
{
	SelStart = 0;
	SelEnd = Size;
	Invalidate();
}

void GTextView3::UnSelectAll()
{
	bool Update = HasSelection();

	SelStart = -1;
	SelEnd = -1;

	if (Update)
	{
		Invalidate();
	}
}

int GTextView3::GetLines()
{
	return Line.Length();
}

void GTextView3::GetTextExtent(int &x, int &y)
{
	PourText(0, Size);

	x = MaxX + d->rPadding.x1;
	y = Line.Length() * LineY;
}

void GTextView3::PositionAt(int &x, int &y, int Index)
{
	int FromIndex = 0;
	GTextLine *From = GetTextLine(Index < 0 ? Cursor : Index, &FromIndex);
	if (From)
	{
		x = Cursor - From->Start;
		y = FromIndex;
	}
}

int GTextView3::GetCursor(bool Cur)
{
	if (Cur)
	{
		return Cursor;
	}

	return 0;
}

int GTextView3::IndexAt(int x, int y)
{
	GTextLine *l = Line.ItemAt(y);
	if (l)
	{
		return l->Start + min(x, l->Len);
	}

	return 0;
}

void GTextView3::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
    // int _Start = LgiCurrentTime();
	Blink = true;

	// Bound the new cursor position to the document
	if (i < 0) i = 0;
	if (i > Size) i = Size;

	// Store the old selection and cursor
	int s = SelStart, e = SelEnd, c = Cursor;
	
	// If there is going to be a selected area
	if (Select && i != SelStart)
	{
		// Then set the start
		if (SelStart < 0)
		{
			// We are starting a new selection
			SelStart = Cursor;
		}

		// And end
		SelEnd = i;
	}
	else
	{
		// Clear the selection
		SelStart = SelEnd = -1;
	}

	int FromIndex = 0;
	GTextLine *From = GetTextLine(Cursor, &FromIndex);

	Cursor = i;

	// check the cursor is on the screen
	int ToIndex = 0;
	GTextLine *To = GetTextLine(Cursor, &ToIndex);
	if (VScroll && To)
	{
		GRect Client = GetClient();
		int DisplayLines = Client.Y() / LineY;

		if (ToIndex < VScroll->Value())
		{
			// Above the visible region...
			if (d->CenterCursor)
			{
				int i = ToIndex - (DisplayLines >> 1);
				VScroll->Value(max(0, i));
			}
			else
			{
				VScroll->Value(ToIndex);
			}
			ForceFullUpdate = true;
		}

		if (ToIndex >= VScroll->Value() + DisplayLines)
		{
			int YOff = d->CenterCursor ? DisplayLines >> 1 : DisplayLines;
			
			int v = min(ToIndex - YOff + 1, Line.Length() - DisplayLines);
			if (v != VScroll->Value())
			{
				// Below the visible region
				VScroll->Value(v);
				ForceFullUpdate = true;
			}
		}
	}

	// check whether we need to update the screen
	if (ForceFullUpdate ||
		!To ||
		!From)
	{
		// need full update
		Invalidate();
	}
	else if (	SelStart != s ||
				SelEnd != e)
	{
		// Update just the selection bounds
		GRect Client = GetClient();
		int Start, End;
		if (SelStart >= 0 && s >= 0)
		{
			// Selection has changed, union the before and after regions
			Start = min(Cursor, c);
			End = max(Cursor, c);
		}
		else if (SelStart >= 0)
		{
			// Selection created...
			Start = min(SelStart, SelEnd);
			End = max(SelStart, SelEnd);
		}
		else if (s >= 0)
		{
			// Selection removed...
			Start = min(s, e);
			End = max(s, e);
		}

		GTextLine *SLine = GetTextLine(Start);
		GTextLine *ELine = GetTextLine(End);
		GRect u;
		if (SLine && ELine)
		{
			if (SLine->r.Valid())
			{
				u = SLine->r;
				u.Offset(0, d->rPadding.y1-ScrollYPixel());
			}
			else
				u.Set(0, 0, Client.X()-1, 1); // Start of visible page 
			
			GRect b(0, Client.Y()-1, Client.X()-1, Client.Y()-1);
			if (ELine->r.Valid())
			{
				b = ELine->r;
				b.Offset(0, d->rPadding.y1-ScrollYPixel());
			}
			else
			{
				b.Set(0, Client.Y()-1, Client.X()-1, Client.Y()-1);
			}
			u.Union(&b);
			
			u.x1 = 0;
			u.x2 = X();
		}
		else
		{
			printf("%s,%i - Couldn't get SLine and ELine (%i, %i)\n", _FL, Start, End);
			u = Client;
		}

		Invalidate(&u);			
	}
	else if (Cursor != c)
	{
		// just the cursor has moved

		// update the line the cursor moved to
		GRect r = To->r;
		r.Offset(-ScrollX, d->rPadding.y1-DocOffset);
		r.x2 = X();
		Invalidate(&r);

		if (To != From)
		{
			// update the line the cursor came from,
			// if it's a different line from the "to"
			r = From->r;
			r.Offset(-ScrollX, d->rPadding.y1-DocOffset);
			r.x2 = X();
			Invalidate(&r);
		}
	}

	if (c != Cursor)
	{
		// Send off notify
		SendNotify(GTVN_CURSOR_CHANGED);
	}

//int _Time = LgiCurrentTime() - _Start;
//printf("Setcursor=%ims\n", _Time);
}

void GTextView3::SetBorder(int b)
{
}

bool GTextView3::Cut()
{
	bool Status = false;
	char16 *Txt16 = 0;
	
	DeleteSelection(&Txt16);
	if (Txt16)
	{
		#ifdef WIN32
		Txt16 = ConvertToCrLf(Txt16);
		#endif
		char *Txt8 = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), Txt16, "utf-16");

		GClipBoard Clip(this);

		Clip.Text(Txt8);
		Status = Clip.TextW(Txt16, false);
		
		DeleteArray(Txt8);
		DeleteArray(Txt16);
	}

	return Status;
}

bool GTextView3::Copy()
{
	bool Status = true;

	if (SelStart >= 0)
	{
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);

		char16 *Txt16 = NewStrW(Text+Min, Max-Min);
		#ifdef WIN32
		Txt16 = ConvertToCrLf(Txt16);
		#endif
		char *Txt8 = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), Txt16, "utf-16");

		GClipBoard Clip(this);
		
		Clip.Text(Txt8);
		Clip.TextW(Txt16, false);
		
		DeleteArray(Txt8);
		DeleteArray(Txt16);
	}
	else LgiTrace("%s:%i - No selection.\n", _FL);

	return Status;
}

bool GTextView3::Paste()
{
	GClipBoard Clip(this);

	GAutoWString Mem;
	char16 *t = Clip.TextW();
	if (!t) // ala Win9x
	{
		char *s = Clip.Text();
		if (s)
		{
			Mem.Reset(LgiNewUtf8To16(s));
			t = Mem;
		}
	}
	
	if (!t)
		return false;

	if (SelStart >= 0)
	{
		DeleteSelection();
	}

	// remove '\r's
	char16 *s = t, *d = t;
	for (; *s; s++)
	{
		if (*s != '\r')
		{
			*d++ = *s;
		}
	}
	*d++ = 0;

	// insert text
	int Len = StrlenW(t);
	Insert(Cursor, t, Len);
	SetCursor(Cursor+Len, false, true); // Multiline
	
	return true;
}

bool GTextView3::ClearDirty(bool Ask, char *FileName)
{
	if (Dirty)
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

bool GTextView3::Open(const char *Name, const char *CharSet)
{
	bool Status = false;
	GFile f;

	if (f.Open(Name, O_READ|O_SHARE))
	{
		DeleteArray(Text);
		size_t Bytes = (size_t)f.GetSize();
		SetCursor(0, false);
		
		char *c8 = new char[Bytes + 4];
		if (c8)
		{
			if (f.Read(c8, Bytes) == Bytes)
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
				
				// Convert to unicode first....
				if (Bytes == 0)
				{
					Text = new char16[1];
					if (Text) Text[0] = 0;
				}
				else
				{
					Text = (char16*)LgiNewConvertCp(LGI_WideCharset, DataStart, CharSet ? CharSet : DefaultCharset);
				}
				
				if (Text)
				{
					// Remove LF's
					char16 *In = Text, *Out = Text;
					CrLf = false;
					Size = 0;
					while (*In)
					{
						if (*In >= ' ' ||
							*In == '\t' ||
							*In == '\n')
						{
							*Out++ = *In;
							Size++;
						}
						else if (*In == '\r')
						{
							CrLf = true;
						}
						In++;
					}
					Size = (int) (Out - Text);
					*Out = 0;

					Alloc = Size + 1;
					Dirty = false;
					
					if (Text && Text[0] == 0xfeff) // unicode byte order mark
					{
						memmove(Text, Text+1, Size * sizeof(*Text));
						Size--;
					}

					PourText(0, Size);
					PourStyle(0, Size);
					UpdateScrollBars(true);
					
					Status = true;
				}
			}

			DeleteArray(c8);
		}
		else
		{
			Alloc = Size = 0;
		}

		Invalidate();
	}

	return Status;
}

bool GTextView3::Save(const char *Name, const char *CharSet)
{
	GFile f;
	if (f.Open(Name, O_WRITE))
	{
		f.SetSize(0);
		if (Text)
		{
			bool Status = false;
			
			char *c8 = (char*)LgiNewConvertCp(CharSet ? CharSet : DefaultCharset, Text, "utf-16", Size * sizeof(char16));
			if (c8)
			{
				int Len = (int)strlen(c8);
				if (CrLf)
				{
					Status = true;

					int BufLen = 1 << 20;
					GAutoPtr<char> Buf(new char[BufLen]);
					char *b = Buf;
					char *e = Buf + BufLen;
					char *c = c8;
					
					while (*c)
					{
					    if (b > e - 10)
					    {
					        int Bytes = b - Buf;
					        if (f.Write(Buf, Bytes) != Bytes)
					        {
					            Status = false;
					            break;
					        }
					        
					        b = Buf;
					    }
					    
					    if (*c == '\n')
					    {
					        *b++ = '\r';
					        *b++ = '\n';
					    }
					    else
					    {
					        *b++ = *c;
					    }
					    c++;
					}

			        int Bytes = b - Buf;
			        if (f.Write(Buf, Bytes) != Bytes)
			            Status = false;
				}
				else
				{
					Status = f.Write(c8, Len) == Len;
				}

				DeleteArray(c8);
			}

			Dirty = false;
			return Status;
		}
	}
	return false;
}

void GTextView3::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		GRect Before = GetClient();

		int DisplayLines = Y() / LineY;
		int Lines = GetLines();
		VScroll->SetLimits(0, Lines);
		if (VScroll)
		{
			VScroll->SetPage(DisplayLines);
			
			int Max = Lines - DisplayLines + 1;
			bool Inval = false;
			if (VScroll->Value() > Max)
			{

				VScroll->Value(Max);
				Inval = true;
			}

			if (Reset)
			{
				VScroll->Value(0);
				SelStart = SelEnd = -1;
			}

			GRect After = GetClient();

			if (Before != After && GetWrapType())
			{
				d->SetDirty(0, Size);
				Inval = true;
			}
			
			if (Inval)
			{
				Invalidate();
			}
		}
	}
}

bool GTextView3::DoCase(bool Upper)
{
	if (Text)
	{
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);

		if (Min < Max)
		{
			UndoQue += new GTextView3Undo(this, Text + Min, Max-Min, Min, UndoChange);

			for (int i=Min; i<Max; i++)
			{
				if (Upper)
				{
					if (Text[i] >= 'a' && Text[i] <= 'z')
					{
						Text[i] = Text[i] - 'a' + 'A';
					}
				}
				else
				{
					if (Text[i] >= 'A' && Text[i] <= 'Z')
					{
						Text[i] = Text[i] - 'A' + 'a';
					}
				}
			}

			Dirty = true;
			d->SetDirty(Min, 0);
			Invalidate();

			SendNotify(GTVN_DOC_CHANGED);
		}
	}
	
	return true;
}

int GTextView3::GetLine()
{
	int Idx = 0;
	GTextLine *t = GetTextLine(Cursor, &Idx);
	return Idx + 1;
}

void GTextView3::SetLine(int i)
{
	GTextLine *l = Line.ItemAt(i - 1);
	if (l)
	{
		d->CenterCursor = true;
		SetCursor(l->Start, false);
		d->CenterCursor = false;
	}
}

bool GTextView3::DoGoto()
{
	GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto line:"), "Text");
	if (Dlg.DoModal() == IDOK &&
		Dlg.Str)
	{
		SetLine(atoi(Dlg.Str));
	}

	return true;
}

GDocFindReplaceParams *GTextView3::CreateFindReplaceParams()
{
	return new GDocFindReplaceParams3;
}

void GTextView3::SetFindReplaceParams(GDocFindReplaceParams *Params)
{
	if (Params)
	{
		if (d->OwnFindReplaceParams)
		{
			DeleteObj(d->FindReplaceParams);
		}
		
		d->OwnFindReplaceParams = false;
		d->FindReplaceParams = (GDocFindReplaceParams3*) Params;
	}
}

bool GTextView3::DoFindNext()
{
	if (d->FindReplaceParams->LastFind)
	{
		return OnFind(	d->FindReplaceParams->LastFind,
						d->FindReplaceParams->MatchWord,
						d->FindReplaceParams->MatchCase,
						d->FindReplaceParams->SelectionOnly);
	}
	
	return false;
}

bool Text_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	GTextView3 *v = (GTextView3*) User;

	Dlg->MatchWord = v->d->FindReplaceParams->MatchWord;
	Dlg->MatchCase = v->d->FindReplaceParams->MatchCase;

	DeleteArray(v->d->FindReplaceParams->LastFind);
	v->d->FindReplaceParams->MatchWord = Dlg->MatchWord;
	v->d->FindReplaceParams->MatchCase = Dlg->MatchCase;

	v->d->FindReplaceParams->LastFind = LgiNewUtf8To16(Dlg->Find);

	if (v->HasSelection() &&
		v->SelEnd < v->SelStart)
	{
		v->Cursor = v->SelStart;
	}

	v->OnFind(	v->d->FindReplaceParams->LastFind,
				v->d->FindReplaceParams->MatchWord,
				v->d->FindReplaceParams->MatchCase,
				v->d->FindReplaceParams->SelectionOnly);

	return true;
}

bool GTextView3::DoFind()
{
	char *u = 0;
	if (HasSelection())
	{
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);

		u = LgiNewUtf16To8(Text + Min, (Max - Min) * sizeof(char16));
	}
	else
	{
		u = LgiNewUtf16To8(d->FindReplaceParams->LastFind);
	}

	GFindDlg Dlg(this, u, Text_FindCallback, this);
	Dlg.DoModal();
	DeleteArray(u);
	
	Focus(true);

	return false;
}

bool GTextView3::DoReplace()
{
	char *LastFind8 = HasSelection() ? GetSelection() : LgiNewUtf16To8(d->FindReplaceParams->LastFind);
	char *LastReplace8 = LgiNewUtf16To8(d->FindReplaceParams->LastReplace);
	
	GReplaceDlg Dlg(this, LastFind8, LastReplace8);

	Dlg.MatchWord = d->FindReplaceParams->MatchWord;
	Dlg.MatchCase = d->FindReplaceParams->MatchCase;
	Dlg.SelectionOnly = HasSelection();

	int Action = Dlg.DoModal();

	DeleteArray(LastFind8);
	DeleteArray(LastReplace8);

	if (Action != IDCANCEL)
	{
		DeleteArray(d->FindReplaceParams->LastFind);
		d->FindReplaceParams->LastFind = LgiNewUtf8To16(Dlg.Find);

		DeleteArray(d->FindReplaceParams->LastReplace);
		d->FindReplaceParams->LastReplace = LgiNewUtf8To16(Dlg.Replace);

		d->FindReplaceParams->MatchWord = Dlg.MatchWord;
		d->FindReplaceParams->MatchCase = Dlg.MatchCase;
		d->FindReplaceParams->SelectionOnly = Dlg.SelectionOnly;
		
		/*
		printf("DoReplace '%S'->'%S' %i,%i,%i\n",
			d->FindReplaceParams->LastFind,
			d->FindReplaceParams->LastReplace,
			d->FindReplaceParams->MatchWord,
			d->FindReplaceParams->MatchCase,
			d->FindReplaceParams->SelectionOnly);
		*/
	}

	switch (Action)
	{
		case IDC_FR_FIND:
		{
			OnFind(	d->FindReplaceParams->LastFind,
					d->FindReplaceParams->MatchWord,
					d->FindReplaceParams->MatchCase,
					d->FindReplaceParams->SelectionOnly);
			break;
		}
		case IDOK:
		case IDC_FR_REPLACE:
		{
			OnReplace(	d->FindReplaceParams->LastFind,
						d->FindReplaceParams->LastReplace,
						Action == IDOK,
						d->FindReplaceParams->MatchWord,
						d->FindReplaceParams->MatchCase,
						d->FindReplaceParams->SelectionOnly);
			break;
		}
	}

	return false;
}

void GTextView3::SelectWord(int From)
{
	for (SelStart = From; SelStart > 0; SelStart--)
	{
		if (strchr(SelectWordDelim, Text[SelStart]))
		{
			SelStart++;
			break;
		}
	}

	for (SelEnd = From; SelEnd < Size; SelEnd++)
	{
		if (strchr(SelectWordDelim, Text[SelEnd]))
		{
			break;
		}
	}

	Invalidate();
}

int GTextView3::MatchText(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	if (ValidStrW(Find))
	{
		int FindLen = StrlenW(Find);
		
		// Setup range to search
		int Begin, End;
		if (SelectionOnly && HasSelection())
		{
			Begin = min(SelStart, SelEnd);
			End = max(SelStart, SelEnd);
		}
		else
		{
			Begin = 0;
			End = Size;
		}

		// Look through text...
		int i;
		bool Wrap = false;
		if (Cursor > End - FindLen)
		{
			Wrap = true;
			i = Begin;
		}
		else
		{
			i = Cursor;
		}
		
		if (i < Begin) i = Begin;
		if (i > End) i = End;
		
		if (MatchCase)
		{
			for (; i<=End-FindLen; i++)
			{
				if (Text[i] == Find[0])
				{
					char16 *Possible = Text + i;;
					if (StrncmpW(Possible, Find, FindLen) == 0)
					{
						if (MatchWord)
						{
							// Check boundaries
							
							if (Possible > Text) // Check off the start
							{
								if (!IsWordBoundry(Possible[-1]))
								{
									continue;
								}
							}
							if (i + FindLen < Size) // Check off the end
							{
								if (!IsWordBoundry(Possible[FindLen]))
								{
									continue;
								}
							}
						}
						
						return SubtractPtr(Possible, Text);
						break;
					}
				}
				
				if (!Wrap && (i + 1 > End - FindLen))
				{
					Wrap = true;
					i = Begin;
					End = Cursor;
				}
			}
		}
		else
		{
			// printf("i=%i s=%i e=%i c=%i flen=%i sz=%i\n", i, Begin, End, Cursor, FindLen, Size);
			for (; i<=End-FindLen; i++)
			{
				if (toupper(Text[i]) == toupper(Find[0]))
				{
					char16 *Possible = Text + i;
					if (StrnicmpW(Possible, Find, FindLen) == 0)
					{
						if (MatchWord)
						{
							// Check boundaries
							
							if (Possible > Text) // Check off the start
							{
								if (!IsWordBoundry(Possible[-1]))
								{
									continue;
								}
							}
							if (i + FindLen < Size) // Check off the end
							{
								if (!IsWordBoundry(Possible[FindLen]))
								{
									continue;
								}
							}
						}
						
						return SubtractPtr(Possible, Text);
						break;
					}
				}
			
				if (!Wrap && (i + 1 > End - FindLen))
				{
					Wrap = true;
					i = Begin;
					End = Cursor;
				}
			}
		}
	}
	
	return -1;
}

bool GTextView3::OnFind(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	int Loc = MatchText(Find, MatchWord, MatchCase, SelectionOnly);
	if (Loc >= 0)
	{
		SetCursor(Loc, false);
		SetCursor(Loc + StrlenW(Find), true);
		return true;
	}

	return false;
}

bool GTextView3::OnReplace(char16 *Find, char16 *Replace, bool All, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	if (ValidStrW(Find))
	{
		// int Max = -1;
		int FindLen = StrlenW(Find);
		int ReplaceLen = StrlenW(Replace);
		int OldCursor = Cursor;
		int First = -1;

		while (true)
		{
			int Loc = MatchText(Find, MatchWord, MatchCase, SelectionOnly);
			if (First < 0)
			{
				First = Loc;
			}
			else if (Loc == First)
			{
				break;
			}
			
			if (Loc >= 0)
			{
				int OldSelStart = SelStart;
				int OldSelEnd = SelEnd;
				
				Delete(Loc, FindLen);
				Insert(Loc, Replace, ReplaceLen);
				
				SelStart = OldSelStart;
				SelEnd = OldSelEnd - FindLen + ReplaceLen;
				Cursor = Loc + ReplaceLen;
			}
			if (!All)
			{
				return Loc >= 0;
			}
			if (Loc < 0) break;
		}
		
		// SetCursor(OldCursor, false);
	}	
	
	return false;
}

int GTextView3::SeekLine(int i, GTextViewSeek Where)
{
	switch (Where)
	{
		case PrevLine:
		{
			for (; i > 0 && Text[i] != '\n'; i--);
			if (i > 0) i--;
			for (; i > 0 && Text[i] != '\n'; i--);
			if (i > 0) i++;
			break;
		}
		case NextLine:
		{
			for (; i < Size && Text[i] != '\n'; i++);
			i++;
			break;
		}
		case StartLine:
		{
			for (; i > 0 && Text[i] != '\n'; i--);
			if (i > 0) i++;
			break;
		}
 		case EndLine:
		{
			for (; i < Size && Text[i] != '\n'; i++);
			break;
		}
		default:
		{
			LgiAssert(false);
			break;
		}
	}

	return i;
}

bool GTextView3::OnMultiLineTab(bool In)
{
	bool Status = false;
	int Min = min(SelStart, SelEnd);
	int Max = max(SelStart, SelEnd);

	Min = SeekLine(Min, StartLine);

	GMemQueue p;
	int Ls = 0, i;
	for (i=Min; i<Max && i<Size; i=SeekLine(i, NextLine))
	{
		p.Write((uchar*)&i, sizeof(i));
		Ls++;
	}
	if (Max < i)
	{
		Max = SeekLine(Max, EndLine);
	}

	int *Indexes = new int[Ls];
	if (Indexes)
	{
		p.Read((uchar*)Indexes, Ls*sizeof(int));

		for (i=Ls-1; i>=0; i--)
		{
			if (In)
			{
				// <-
				int n = Indexes[i], Space = 0;
				for (; Space<IndentSize && n<Size; n++)
				{
					if (Text[n] == 9)
					{
						Space += IndentSize;
					}
					else if (Text[n] == ' ')
					{
						Space += 1;
					}
					else
					{
						break;
					}
				}

				int Chs = n-Indexes[i];
				Delete(Indexes[i], Chs);
				Max -= Chs;
			}
			else
			{
				// ->
				int Len = Indexes[i];
				for (; Text[Len] != '\n' && Len<Size; Len++);
				if (Len > Indexes[i])
				{
					if (HardTabs)
					{
						char16 Tab[] = {'\t', 0};
						Insert(Indexes[i], Tab, 1);
						Max++;
					}
					else
					{
						char16 *Sp = new char16[IndentSize];
						if (Sp)
						{
							for (int n=0; n<IndentSize; n++)
								Sp[n] = ' ';
							Insert(Indexes[i], Sp, IndentSize);
							Max += IndentSize;
						}
					}
				}
			}
		}

		SelStart = Min;
		SelEnd = Cursor = Max;

		DeleteArray(Indexes);
	}

	d->SetDirty(Min, Max-Min);
	Invalidate();
	Status = true;

	return Status;
}

void GTextView3::OnSetHidden(int Hidden)
{
}

void GTextView3::OnPosChange()
{
	static bool Processing = false;

	/*
	RECT rc;
	GetClientRect(Handle(), &rc);
	LgiTrace("GTextView3::OnPosChange par=%s iswin=%i isvis=%i rc=%i,%i,%i,%i par=%p\n",
		GetParent() ? GetParent()->Name() : "(none)",
		IsWindow(Handle()),
		IsWindowVisible(Handle()),
		rc.left, rc.top, rc.right, rc.bottom,
		::GetParent(Handle())
		);
	if (Handle())
	{
		GScreenDC scr(Handle());
		scr.Colour(GColour(255, 0, 0));
		scr.Line(0,0,X()-1, Y()-1);
	}
	*/

	if (!Processing)
	{
		Processing = true;
		GLayout::OnPosChange();

		GRect c = GetClient();
		bool ScrollYNeeded = c.Y() < (Line.Length() * LineY);
		SetScrollBars(false, ScrollYNeeded);
		UpdateScrollBars();

		if (GetWrapType() && d->PourX != X())
		{
			d->PourX = X();
			d->SetDirty(0, Size);
		}
		Processing = false;
	}
}

int GTextView3::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
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

int GTextView3::OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState)
{
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
			Insert(Cursor, s, len);
			Invalidate();
			return DROPEFFECT_COPY;
		}
	}

	return DROPEFFECT_NONE;
}

void GTextView3::OnCreate()
{
	SetWindow(this);
	DropTarget(true);

	if (Focus())
		SetPulse(1500);
}

void GTextView3::OnEscape(GKey &K)
{
}

bool GTextView3::OnMouseWheel(double l)
{
	if (VScroll)
	{
		int NewPos = (int)VScroll->Value() + (int) l;
		NewPos = limit(NewPos, 0, GetLines());
		VScroll->Value(NewPos);
		Invalidate();
	}
	
	return true;
}

void GTextView3::OnFocus(bool f)
{
	Invalidate();
	SetPulse(f ? 500 : -1);
}

int GTextView3::HitText(int x, int y)
{
	if (Text)
	{
		bool Down = y >= 0;
		int Y = (VScroll) ? (int)VScroll->Value() : 0;
		GTextLine *l = Line.ItemAt(Y);
		y += (l) ? l->r.y1 : 0;

		while (l)
		{
			if (l->r.Overlap(x, y))
			{
				// Over a line
				int At = x - l->r.x1;
				int Char = 0;
				
				GDisplayString Ds(Font, MapText(Text + l->Start, l->Len), l->Len, 0);
				Char = Ds.CharAt(At);

				return l->Start + Char;
			}
			else if (y >= l->r.y1 && y <= l->r.y2)
			{
				// Click horizontally before of after line
				if (x < l->r.x1)
				{
					return l->Start;
				}
				else if (x > l->r.x2)
				{
					return l->Start + l->Len;
				}
			}
			
			l = (Down) ? Line.Next() : Line.Prev();
		}

		// outside text area
		if (Down)
		{
			l = Line.Last();
			if (l)
			{
				if (y > l->r.y2)
				{
					// end of document
					return Size;
				}
			}
		}
	}

	return 0;
}

void GTextView3::Undo()
{
	UndoQue.Undo();
}

void GTextView3::Redo()
{
	UndoQue.Redo();
}

void GTextView3::DoContextMenu(GMouse &m)
{
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

	GStyle *s = HitStyle(HitText(m.x, m.y));
	if (s)
	{
		if (s->OnMenu(&RClick))
		{
			RClick.AppendSeparator();
		}
	}

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_CUT, "Cut"), IDM_CUT, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_PASTE, "Paste"), IDM_PASTE, ClipText != 0);
	RClick.AppendSeparator();

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_UNDO, "Undo"), IDM_UNDO, UndoQue.CanUndo());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_REDO, "Redo"), IDM_REDO, UndoQue.CanRedo());
	RClick.AppendSeparator();

	GMenuItem *i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_FIXED, "Fixed Width Font"), IDM_FIXED, true);
	if (i) i->Checked(GetFixedWidthFont());

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_AUTO_INDENT, "Auto Indent"), IDM_AUTO_INDENT, true);
	if (i) i->Checked(AutoIndent);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_SHOW_WHITESPACE, "Show Whitespace"), IDM_SHOW_WHITE, true);
	if (i) i->Checked(ShowWhiteSpace);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_HARD_TABS, "Hard Tabs"), IDM_HARD_TABS, true);
	if (i) i->Checked(HardTabs);
	
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_INDENT_SIZE, "Indent Size"), IDM_INDENT_SIZE, true);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_TAB_SIZE, "Tab Size"), IDM_TAB_SIZE, true);

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

				char *s = LgiNewUtf16To8(Text + l->Start, l->Len * sizeof(char16));
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
			SendNotify(GTVN_FIXED_WIDTH_CHANGED);
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
		default:
		{
			if (s)
			{
				s->OnMenuClick(Id);
			}

			if (Environment)
			{
				Environment->OnMenu(this, Id, 0);
			}
			break;
		}
	}
}

void GTextView3::OnMouseClick(GMouse &m)
{
	bool Processed = false;

	m.x += ScrollX;

	if (m.Down())
	{
		if (!m.IsContextMenu())
		{
			Focus(true);

			int Hit = HitText(m.x, m.y);
			if (Hit >= 0)
			{
				SetCursor(Hit, m.Shift());

				GStyle *s = HitStyle(Hit);
				if (s)
				{
					Processed = s->OnMouseClick(&m);
				}
			}

			if (!Processed && m.Double())
			{
				d->WordSelectMode = Cursor;
				SelectWord(Cursor);
			}
			else
			{
				d->WordSelectMode = -1;
			}
		}
		else
		{
			DoContextMenu(m);
			return;
		}
	}

	if (!Processed)
	{
		Capture(m.Down());
	}
}

int GTextView3::OnHitTest(int x, int y)
{
	#ifdef WIN32
	if (GetClient().Overlap(x, y))
	{
		return HTCLIENT;
	}
	#endif
	return GView::OnHitTest(x, y);
}

void GTextView3::OnMouseMove(GMouse &m)
{
	m.x += ScrollX;

	int Hit = HitText(m.x, m.y);
	if (IsCapturing())
	{
		if (d->WordSelectMode < 0)
		{
			SetCursor(Hit, m.Left());
		}
		else
		{
			int Min = Hit < d->WordSelectMode ? Hit : d->WordSelectMode;
			int Max = Hit > d->WordSelectMode ? Hit : d->WordSelectMode;

			for (SelStart = Min; SelStart > 0; SelStart--)
			{
				if (strchr(SelectWordDelim, Text[SelStart]))
				{
					SelStart++;
					break;
				}
			}

			for (SelEnd = Max; SelEnd < Size; SelEnd++)
			{
				if (strchr(SelectWordDelim, Text[SelEnd]))
				{
					break;
				}
			}

			Cursor = SelEnd;
			Invalidate();
		}
	}

	#ifdef WIN32
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		GStyle *s = HitStyle(Hit);
		TCHAR *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
	}
	#endif
}

int GTextView3::GetColumn()
{
	int x = 0;
	GTextLine *l = GetTextLine(Cursor);
	if (l)
	{
		for (int i=l->Start; i<Cursor; i++)
		{
			if (Text[i] == '\t')
			{
				x += TabSize - (x % TabSize);
			}
			else
			{
				x++;
			}
		}
	}
	return x;
}

int GTextView3::SpaceDepth(char16 *Start, char16 *End)
{
	int Depth = 0;
	while (Start < End)
	{
		if (*Start == '\t')
			Depth += IndentSize - (Depth % IndentSize);
		else if (*Start == ' ')
			Depth ++;
		else break;
		Start++;
	}
	return Depth;
}

bool GTextView3::OnKey(GKey &k)
{
	if (k.Down())
	{
		Blink = true;
	}

	// k.Trace("GTextView3::OnKey");

	if (k.IsContextMenu())
	{
		GMouse m;
		m.x = CursorPos.x1;
		m.y = CursorPos.y1 + (CursorPos.Y() >> 1);
		m.Target = this;
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
					if (k.Down())
					{
						// letter/number etc
						if (SelStart >= 0)
						{
							bool MultiLine = false;
							if (k.c16 == VK_TAB)
							{
								int Min = min(SelStart, SelEnd), Max = max(SelStart, SelEnd);
								for (int i=Min; i<Max; i++)
								{
									if (Text[i] == '\n')
									{
										MultiLine = true;
									}
								}
							}
							if (MultiLine)
							{
								if (OnMultiLineTab(k.Shift()))
								{
									return true;
								}
							}
							else
							{
								DeleteSelection();
							}
						}
						
						GTextLine *l = GetTextLine(Cursor);
						int Len = (l) ? l->Len : 0;
						
						if (l && k.c16 == VK_TAB && (!HardTabs || IndentSize != TabSize))
						{
							int x = GetColumn();							
							int Add = IndentSize - (x % IndentSize);
							
							if (HardTabs && ((x + Add) % TabSize) == 0)
							{
								int Rx = x;
								int Remove;
								for (Remove = Cursor; Text[Remove - 1] == ' ' && Rx % TabSize != 0; Remove--, Rx--);
								int Chars = Cursor - Remove;
								Delete(Remove, Chars);
								Insert(Remove, &k.c16, 1);
								Cursor = Remove + 1;
								
								Invalidate();
							}
							else
							{							
								char16 *Sp = new char16[Add];
								if (Sp)
								{
									for (int n=0; n<Add; n++) Sp[n] = ' ';
									if (Insert(Cursor, Sp, Add))
									{
										l = GetTextLine(Cursor);
										int NewLen = (l) ? l->Len : 0;
										SetCursor(Cursor + Add, false, Len != NewLen - 1);
									}
									DeleteArray(Sp);
								}
							}
						}
						else
						{
							char16 In = k.GetChar();

							if (In == '\t' &&
								k.Shift() &&
								Cursor > 0)
							{
								l = GetTextLine(Cursor);
								if (Cursor > l->Start)
								{
									if (Text[Cursor-1] == '\t')
									{
										Delete(Cursor - 1, 1);
										SetCursor(Cursor, false, false);
									}
									else if (Text[Cursor-1] == ' ')
									{
										int Start = Cursor - 1;
										while (Start >= l->Start && strchr(" \t", Text[Start-1]))
											Start--;
										int Depth = SpaceDepth(Text + Start, Text + Cursor);
										int NewDepth = Depth - (Depth % IndentSize);
										if (NewDepth == Depth && NewDepth > 0)
											NewDepth -= IndentSize;
										int Use = 0;
										while (SpaceDepth(Text + Start, Text + Start + Use + 1) < NewDepth)
											Use++;
										Delete(Start + Use, Cursor - Start - Use);
										SetCursor(Start + Use, false, false);
									}
								}
								
							}
							else if (In && Insert(Cursor, &In, 1))
							{
								l = GetTextLine(Cursor);
								int NewLen = (l) ? l->Len : 0;
								SetCursor(Cursor + 1, false, Len != NewLen - 1);
							}
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
				{
					OnEnter(k);
				}
				return true;
				break;
			}
			case VK_BACKSPACE:
			{
				if (GetReadOnly())
					break;

				if (k.Ctrl())
				{
				    // Ctrl+H
				    int asd=0;
				}
				else if (k.Down())
				{
					if (SelStart >= 0)
					{
						// delete selection
						DeleteSelection();
					}
					else
					{						
						char Del = Cursor > 0 ? Text[Cursor-1] : 0;
						
						if (Del == ' ' && (!HardTabs || IndentSize != TabSize))
						{
							// Delete soft tab
							int x = GetColumn();
							int Max = x % IndentSize;
							if (Max == 0) Max = IndentSize;
							int i;
							for (i=Cursor-1; i>=0; i--)
							{
								if (Max-- <= 0 || Text[i] != ' ')
								{
									i++;
									break;
								}
							}
							
							if (i < 0) i = 0;
							
							if (i < Cursor - 1)
							{
								int Del = Cursor - i;								
								Delete(i, Del);
								// SetCursor(i, false, false);
								// Invalidate();
								break;
							}
						}
						else if (Del == '\t' && HardTabs && IndentSize != TabSize)
						{
							int x = GetColumn();
							Delete(--Cursor, 1);
							
							for (int c=GetColumn(); c<x-IndentSize; c=GetColumn())
							{
								int Add = IndentSize + (c % IndentSize);
								if (Add)
								{
									char16 *s = new char16[Add];
									if (s)
									{
										for (int n=0; n<Add; n++) s[n] = ' ';
										Insert(Cursor, s, Add);
										Cursor += Add;
										DeleteArray(s);
									}
								}
								else break;
							}
							
							Invalidate();
							break;
						}


						if (Cursor > 0)
						{
							Delete(Cursor - 1, 1);
						}
					}
				}
				return true;
				break;
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
			{
				return !GetReadOnly();
			}
			case VK_BACKSPACE:
			{
				if (!GetReadOnly())
				{
					if (k.Alt())
					{
						if (k.Down())
						{
							if (k.Ctrl())
							{
								Redo();
							}
							else
							{
								Undo();
							}
						}
					}
					else if (k.Ctrl())
					{
						if (k.Down())
						{
							int Start = Cursor;
							while (IsWhiteSpace(Text[Cursor-1]) && Cursor > 0)
								Cursor--;

							while (!IsWhiteSpace(Text[Cursor-1]) && Cursor > 0)
								Cursor--;

							Delete(Cursor, Start - Cursor);
							Invalidate();
						}
					}

					return true;
				}
				break;
			}
			case VK_F3:
			{
				if (k.Down())
				{
					DoFindNext();
				}
				return true;
				break;
			}
			case VK_LEFT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (SelStart >= 0 &&
						!k.Shift())
					{
						SetCursor(min(SelStart, SelEnd), false);
					}
					else if (Cursor > 0)
					{
						int n = Cursor;

						#ifdef MAC
						if (k.System())
						{
							goto Jump_StartOfLine;
						}
						else
						#endif
						if (k.Ctrl())
						{
							// word move/select
							bool StartWhiteSpace = IsWhiteSpace(Text[n]);
							bool LeftWhiteSpace = n > 0 && IsWhiteSpace(Text[n-1]);

							if (!StartWhiteSpace ||
								Text[n] == '\n')
							{
								n--;
							}
							
							// Skip ws
							for (; n > 0 && strchr(" \t", Text[n]); n--)
								;
							
							if (Text[n] == '\n')
							{
								n--;
							}
							else if (!StartWhiteSpace || !LeftWhiteSpace)
							{
								if (IsDelimiter(Text[n]))
								{
									for (; n > 0 && IsDelimiter(Text[n]); n--);
								}
								else
								{
									for (; n > 0; n--)
									{
										//IsWordBoundry(Text[n])
										if (IsWhiteSpace(Text[n]) ||
											IsDelimiter(Text[n]))
										{
											break;
										}
									}
								}
							}
							if (n > 0) n++;
						}
						else
						{
							// single char
							n--;
						}

						SetCursor(n, k.Shift());
					}
				}
				return true;
				break;
			}
			case VK_RIGHT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (SelStart >= 0 &&
						!k.Shift())
					{
						SetCursor(max(SelStart, SelEnd), false);
					}
					else if (Cursor < Size)
					{
						int n = Cursor;

						#ifdef MAC
						if (k.System())
						{
							goto Jump_EndOfLine;
						}
						else
						#endif
						if (k.Ctrl())
						{
							// word move/select
							if (IsWhiteSpace(Text[n]))
							{
								for (; n<Size && IsWhiteSpace(Text[n]); n++);
							}
							else
							{
								if (IsDelimiter(Text[n]))
								{
									while (IsDelimiter(Text[n]))
									{
										n++;
									}
								}
								else
								{
									for (; n<Size; n++)
									{
										if (IsWhiteSpace(Text[n]) ||
											IsDelimiter(Text[n]))
										{
											break;
										}
									}
								}

								if (n < Size &&
									Text[n] != '\n')
								{
									if (IsWhiteSpace(Text[n]))
									{
										n++;
									}
								}
							}
						}
						else
						{
							// single char
							n++;
						}

						SetCursor(n, k.Shift());
					}
				}
				return true;
				break;
			}
			case VK_UP:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView3_PageUp;
					#endif
					
					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						GTextLine *Prev = Line.Prev();
						if (Prev)
						{
							GDisplayString CurLine(Font, Text + l->Start, Cursor-l->Start);
							int ScreenX = CurLine.X();

							GDisplayString PrevLine(Font, Text + Prev->Start, Prev->Len);
							int CharX = PrevLine.CharAt(ScreenX);

							SetCursor(Prev->Start + min(CharX, Prev->Len), k.Shift());
						}
					}
				}
				return true;
				break;
			}
			case VK_DOWN:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView3_PageDown;
					#endif

					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						GTextLine *Next = Line.Next();
						if (Next)
						{
							GDisplayString CurLine(Font, Text + l->Start, Cursor-l->Start);
							int ScreenX = CurLine.X();
							
							GDisplayString NextLine(Font, Text + Next->Start, Next->Len);
							int CharX = NextLine.CharAt(ScreenX);

							SetCursor(Next->Start + min(CharX, Next->Len), k.Shift());
						}
					}
				}
				return true;
				break;
			}
			case VK_END:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						SetCursor(Size, k.Shift());
					}
					else
					{
						#ifdef MAC
						Jump_EndOfLine:
						#endif
						GTextLine *l = GetTextLine(Cursor);
						if (l)
						{
							SetCursor(l->Start + l->Len, k.Shift());
						}
					}
				}
				return true;
				break;
			}
			case VK_HOME:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						SetCursor(0, k.Shift());
					}
					else
					{
						#ifdef MAC
						Jump_StartOfLine:
						#endif
						GTextLine *l = GetTextLine(Cursor);
						if (l)
						{
							char16 *Line = Text + l->Start;
							char16 *s;
							char16 SpTab[] = {' ', '\t', 0};
							for (s = Line; (SubtractPtr(s,Line) < l->Len) && StrchrW(SpTab, *s); s++);
							int Whitespace = SubtractPtr(s, Line);

							if (l->Start + Whitespace == Cursor)
							{
								SetCursor(l->Start, k.Shift());
							}
							else
							{
								SetCursor(l->Start + Whitespace, k.Shift());
							}
						}
					}
				}
				return true;
				break;
			}
			case VK_PAGEUP:
			{
				#ifdef MAC
				GTextView3_PageUp:
				#endif
				if (k.Down())
				{
					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						int DisplayLines = Y() / LineY;
						int CurLine = Line.IndexOf(l);

						GTextLine *New = Line.ItemAt(max(CurLine - DisplayLines, 0));
						if (New)
						{
							SetCursor(New->Start + min(Cursor - l->Start, New->Len), k.Shift());
						}
					}
				}
				return true;
				break;
			}
			case VK_PAGEDOWN:
			{
				#ifdef MAC
				GTextView3_PageDown:
				#endif
				if (k.Down())
				{
					GTextLine *l = GetTextLine(Cursor);
					if (l)
					{
						int DisplayLines = Y() / LineY;
						int CurLine = Line.IndexOf(l);

						GTextLine *New = Line.ItemAt(min(CurLine + DisplayLines, GetLines()-1));
						if (New)
						{
							SetCursor(New->Start + min(Cursor - l->Start, New->Len), k.Shift());
						}
					}
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
				if (!GetReadOnly())
				{
					if (k.Down())
					{
						if (SelStart >= 0)
						{
							if (k.Shift())
							{
								Cut();
							}
							else
							{
								DeleteSelection();
							}
						}
						else if (Cursor < Size &&
								Delete(Cursor, 1))
						{
							Invalidate();
						}
					}
					return true;
				}
				break;
			}
			default:
			{
				if (k.c16 == 17) break;

				if (k.Modifier() &&
					!k.Alt())
				{
					switch (k.GetChar())
					{
						case 0xbd: // Ctrl+'-'
						{
							if (k.Down() &&
								Font->PointSize() > 1)
							{
								Font->PointSize(Font->PointSize() - 1);
								OnFontChange();
								Invalidate();
							}
							break;
						}
						case 0xbb: // Ctrl+'+'
						{
							if (k.Down() &&
								Font->PointSize() < 100)
							{
								Font->PointSize(Font->PointSize() + 1);
								OnFontChange();
								Invalidate();
							}
							break;
						}
						case 'a':
						case 'A':
						{
							if (k.Down())
							{
								// select all
								SelStart = 0;
								SelEnd = Size;
								Invalidate();
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
							if (k.Down())
							{
								Copy();
							}
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
							{
								DoFind();
							}
							return true;
							break;
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

void GTextView3::OnEnter(GKey &k)
{
	// enter
	if (SelStart >= 0)
	{
		DeleteSelection();
	}

	char16 InsertStr[256] = {'\n', 0};

	GTextLine *CurLine = GetTextLine(Cursor);
	if (CurLine && AutoIndent)
	{
		int WsLen = 0;
		for (;	WsLen < CurLine->Len &&
				WsLen < (Cursor - CurLine->Start) &&
				strchr(" \t", Text[CurLine->Start + WsLen]); WsLen++);
		if (WsLen > 0)
		{
			memcpy(InsertStr+1, Text+CurLine->Start, WsLen * sizeof(char16));
			InsertStr[WsLen+1] = 0;
		}
	}

	if (Insert(Cursor, InsertStr, StrlenW(InsertStr)))
	{
		SetCursor(Cursor + StrlenW(InsertStr), false, true);
	}
}

int GTextView3::TextWidth(GFont *f, char16 *s, int Len, int x, int Origin)
{
	int w = x;
	int Size = f->TabSize();

	for (char16 *c = s; SubtractPtr(c, s) < Len; )
	{
		if (*c == 9)
		{			
			w = ((((w-Origin) + Size) / Size) * Size) + Origin;
			c++;
		}
		else
		{
			char16 *e;
			for (e = c; SubtractPtr(e, s) < Len && *e != 9; e++);
			
			GDisplayString ds(f, c, SubtractPtr(e, c));
			w += ds.X();
			c = e;
		}
	}

	return w - x;
}

int GTextView3::ScrollYLine()
{
	return (VScroll) ? (int)VScroll->Value() : 0;
}

int GTextView3::ScrollYPixel()
{
	return ScrollYLine() * LineY;
}

void GTextView3::OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour)
{
	pDC->Colour(colour);
	pDC->Rectangle(&r);
}

void GTextView3::OnPaint(GSurface *pDC)
{
	#if LGI_EXCEPTIONS
	try
	{
	#endif
		if (d->LayoutDirty)
		{
			PourText(d->DirtyStart, d->DirtyLen);
		}
		
		GRect r = GetClient();
		r.x2 += ScrollX;

		int Ox, Oy;
		pDC->GetOrigin(Ox, Oy);
		pDC->SetOrigin(Ox+ScrollX, Oy);
		
		#if 0
		// Coverage testing...
		pDC->Colour(Rgb24(255, 0, 255), 24);
		pDC->Rectangle();
		#endif

		GSurface *pOut = pDC;
		bool DrawSel = false;
		
		bool HasFocus = Focus();
		GColour SelectedText(HasFocus ? LC_FOCUS_SEL_FORE : LC_NON_FOCUS_SEL_FORE, 24);
		GColour SelectedBack(HasFocus ? LC_FOCUS_SEL_BACK : LC_NON_FOCUS_SEL_BACK, 24);

		GCss::ColorDef ForeDef, BkDef;
		if (GetCss())
		{
			ForeDef = GetCss()->Color();
			BkDef = GetCss()->BackgroundColor();
		}
		
		GColour Fore(ForeDef.Type ==  GCss::ColorRgb ? Rgb32To24(ForeDef.Rgb32) : LC_TEXT, 24);
		GColour Back(!ReadOnly ? (BkDef.Type == GCss::ColorRgb ? Rgb32To24(BkDef.Rgb32) : LC_WORKSPACE) : BackColour, 24);

		GColour Whitespace = Fore.Mix(Back, 0.85f);

		if (!Enabled())
		{
			Fore.Set(LC_LOW, 24);
			Back.Set(LC_MED, 24);
		}

		#ifdef DOUBLE_BUFFER_PAINT
		GMemDC *pMem = new GMemDC;
		pOut = pMem;
		#endif
		if (Text &&
			Font
			#ifdef DOUBLE_BUFFER_PAINT
			&&
			pMem &&
			pMem->Create(r.X()-d->rPadding.x1, LineY, GdcD->GetBits())
			#endif
			)
		{
			int SelMin = min(SelStart, SelEnd);
			int SelMax = max(SelStart, SelEnd);

			// font properties
			Font->Colour(Fore, Back);
			Font->WhitespaceColour(Whitespace);
			Font->Transparent(false);

			// draw margins
			pDC->Colour(PAINT_BORDER);
			// top margin
			pDC->Rectangle(0, 0, r.x2, d->rPadding.y1-1);
			// left margin
			{
				GRect LeftMargin(0, d->rPadding.y1, d->rPadding.x1-1, r.y2);
				OnPaintLeftMargin(pDC, LeftMargin, PAINT_BORDER);
			}
		
			// draw lines of text
			int k = ScrollYLine();
			GTextLine *l=Line.ItemAt(k);
			int Dy = (l) ? -l->r.y1 : 0;
			int NextSelection = (SelStart != SelEnd) ? SelMin : -1; // offset where selection next changes
			if (l &&
				SelStart >= 0 &&
				SelStart < l->Start &&
				SelEnd > l->Start)
			{
				// start of visible area is in selection
				// init to selection colour
				DrawSel = true;
				Font->Colour(SelectedText, SelectedBack);
				NextSelection = SelMax;
			}

			GStyle *NextStyle = GetNextStyle((l) ? l->Start : 0);

			DocOffset = (l) ? l->r.y1 : 0;

			// loop through all visible lines
			int y = d->rPadding.y1;
			for (; l && l->r.y1+Dy < r.Y(); l=Line.Next())
			{
				GRect Tr = l->r;
				#ifdef DOUBLE_BUFFER_PAINT
				Tr.Offset(-Tr.x1, -Tr.y1);
				#else
				Tr.Offset(0, y - Tr.y1);
				#endif

				// deal with selection change on beginning of line
				if (NextSelection == l->Start)
				{
					// selection change
					DrawSel = !DrawSel;
					NextSelection = (NextSelection == SelMin) ? SelMax : -1;
				}
				if (DrawSel)
				{
					Font->Colour(SelectedText, SelectedBack);
				}
				else
				{
					GColour fore = l->c.IsValid() ? l->c : Fore;
					GColour back = l->Back.IsValid() ? l->Back : Back;
					Font->Colour(fore, back);
				}

				// draw text
				int Done = 0;	// how many chars on this line have we 
								// processed so far
				int TextX = 0;	// pixels we have moved so far
				
				// loop through all sections of similar text on a line
				while (Done < l->Len)
				{
					// decide how big this block is
					int RtlTrailingSpace = 0;
					int Cur = l->Start + Done;
					int Block = l->Len - Done;
					
					// check for style change
					if (NextStyle)
					{
						// start
						if (l->Overlap(NextStyle->Start) &&
							NextStyle->Start > Cur &&
							NextStyle->Start - Cur < Block)
						{
							Block = NextStyle->Start - Cur;
						}

						// end
						int StyleEnd = NextStyle->Start + NextStyle->Len;
						if (l->Overlap(StyleEnd) &&
							StyleEnd > Cur &&
							StyleEnd - Cur < Block)
						{
							Block = StyleEnd - Cur;
						}
					}

					// check for next selection change
					// this may truncate the style
					if (NextSelection > Cur &&
						NextSelection - Cur < Block)
					{
						Block = NextSelection - Cur;
					}

					LgiAssert(Block != 0);	// sanity check
					
					int TabOri = Tr.x1 - d->rPadding.x1;

					if (NextStyle &&							// There is a style
						(Cur < SelMin || Cur >= SelMax) &&		// && we're not drawing a selection block
						Cur >= NextStyle->Start &&				// && we're inside the styled area
						Cur < NextStyle->Start+NextStyle->Len)
					{
						if (NextStyle->Font)
						{
							// draw styled text
							if (NextStyle->c.IsValid())
							{
								NextStyle->Font->Colour(NextStyle->c, l->Back.IsValid() ? l->Back : Back);
							}
							NextStyle->Font->Transparent(false);

							LgiAssert(l->Start + Done >= 0);

							GDisplayString Ds(	NextStyle->Font,
												MapText(Text + (l->Start + Done),
														Block,
														RtlTrailingSpace != 0),
												Block + RtlTrailingSpace);
							Ds.ShowVisibleTab(ShowWhiteSpace);
							Ds.SetTabOrigin(TabOri);
							TextX = Ds.X();
							Ds.Draw(pOut, Tr.x1, Tr.y1, 0);

							if (NextStyle->Decor == GStyle::DecorSquiggle)
							{
								pOut->Colour(NextStyle->DecorColour.c24(), 24);
								for (int i=0; i<TextX; i++)
									pOut->Set(Tr.x1+i, Tr.y2-(i%2));
							}

							GColour fore = l->c.IsValid() ? l->c : Fore;
							GColour back = l->Back.IsValid() ? l->Back : Back;
							NextStyle->Font->Colour(fore, back);
						}
					}
					else
					{
						// draw a block of normal text
						LgiAssert(l->Start + Done >= 0);
						
						GDisplayString Ds(	Font,
											MapText(Text + (l->Start + Done),
													Block,
													RtlTrailingSpace != 0),
											Block + RtlTrailingSpace);
						Ds.ShowVisibleTab(ShowWhiteSpace);
						Ds.SetTabOrigin(TabOri);
						TextX = Ds.X();

						Ds.Draw(pOut, Tr.x1, Tr.y1, 0);
					}

					if (NextStyle &&
						Cur+Block >= NextStyle->Start+NextStyle->Len)
					{
						// end of this styled block
						NextStyle = GetNextStyle();
					}

					if (NextSelection == Cur+Block)
					{
						// selection change
						DrawSel = !DrawSel;
						if (DrawSel)
						{
							Font->Colour(SelectedText, SelectedBack);
						}
						else
						{
							GColour fore = l->c.IsValid() ? l->c : Fore;
							GColour back = l->Back.IsValid() ? l->Back : Back;
							Font->Colour(fore, back);
						}
						NextSelection = (NextSelection == SelMin) ? SelMax : -1;
					}

					Tr.x1 += TextX;
					Done += Block + RtlTrailingSpace;
				
				} // end block loop

				// eol processing
				int EndOfLine = l->Start+l->Len;
				if (EndOfLine >= SelMin && EndOfLine < SelMax)
				{
					// draw the '\n' at the end of the line as selected
					GColour bk = Font->Back();
					pOut->Colour(Font->Back());
					pOut->Rectangle(Tr.x2, Tr.y1, Tr.x2+7, Tr.y2);
					Tr.x2 += 7;
				}
				else Tr.x2 = Tr.x1;

				// draw any space after text
				pOut->Colour(PAINT_AFTER_LINE);
				pOut->Rectangle(Tr.x2, Tr.y1, r.x2, Tr.y2);

				// cursor?
				if (HasFocus)
				{
					// draw the cursor if on this line
					if (Cursor >= l->Start && Cursor <= l->Start+l->Len)
					{
						CursorPos.ZOff(1, LineY-1);

						int At = Cursor-l->Start;
						
						GDisplayString Ds(Font, MapText(Text+l->Start, At), At);
						Ds.ShowVisibleTab(ShowWhiteSpace);
						int CursorX = Ds.X();
						CursorPos.Offset(d->rPadding.x1 + CursorX, Tr.y1);
						
						if (CanScrollX)
						{
							// Cursor on screen check
							GRect Scr = GetClient();
							Scr.Offset(ScrollX, 0);
							GRect Cur = CursorPos;
							if (Cur.x2 > Scr.x2 - 5) // right edge check
							{
								ScrollX = ScrollX + Cur.x2 - Scr.x2 + 40;
								Invalidate();
							}
							else if (Cur.x1 < Scr.x1 && ScrollX > 0)
							{
								ScrollX = max(0, Cur.x1 - 40);
								Invalidate();
							}
						}

						if (Blink)
						{
							GRect c = CursorPos;
							#ifdef DOUBLE_BUFFER_PAINT
							c.Offset(-d->rPadding.x1, -y);
							#endif

							pOut->Colour((!ReadOnly) ? Fore : GColour(192, 192, 192));
							pOut->Rectangle(&c);
						}

						#if WINNATIVE
						HIMC hIMC = ImmGetContext(Handle());
						if (hIMC)
						{
							COMPOSITIONFORM Cf;						
							Cf.dwStyle = CFS_POINT;
							Cf.ptCurrentPos.x = CursorPos.x1;
							Cf.ptCurrentPos.y = CursorPos.y1;
							ImmSetCompositionWindow(hIMC, &Cf);
							ImmReleaseContext(Handle(), hIMC);
						}
						#endif
					}
				}

				#ifdef DOUBLE_BUFFER_PAINT
				// dump to screen
				pDC->Blt(d->rPadding.x1, y, pOut);
				#endif
				y += LineY;

			} // end of line loop

			// draw any space under the lines
			if (y <= r.y2)
			{
				// printf("White %i, k=%i Lines=%i\n", r.y2 - y, k, Line.Length());

				pDC->Colour(Back);
				pDC->Rectangle(d->rPadding.x1, y, r.x2, r.y2);
			}

			#ifdef DOUBLE_BUFFER_PAINT
			DeleteObj(pMem);
			#endif
		}
		else
		{
			// default drawing: nothing
			pDC->Colour(Back);
			pDC->Rectangle(&r);
		}

		// _PaintTime = LgiCurrentTime() - StartTime;
		#ifdef PAINT_DEBUG
		if (GetNotify())
		{
			char s[256];
			sprintf_s(s, sizeof(s), "Pour:%i Style:%i Paint:%i ms", _PourTime, _StyleTime, _PaintTime);
			GMessage m = CreateMsg(DEBUG_TIMES_MSG, 0, (int)s);
			GetNotify()->OnEvent(&m);
		}
		#endif
		// printf("PaintTime: %ims\n", _PaintTime);
	
	#if LGI_EXCEPTIONS
	}
	catch (...)
	{
		LgiMsg(this, "GTextView3::OnPaint crashed.", "Lgi");
	}
	#endif
}

GMessage::Result GTextView3::OnEvent(GMessage *Msg)
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
			return Size;
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

						char16 *Utf = (char16*)LgiNewConvertCp("utf-16", Buf, LgiAnsiToLgiCp(), Size);
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

int GTextView3::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_VSCROLL && VScroll)
	{
		Invalidate();
	}

	return 0;
}

void GTextView3::OnPulse()
{
	if (!ReadOnly)
	{
		Blink = !Blink;

		GRect p = CursorPos;
		p.Offset(-ScrollX, 0);
		Invalidate(&p);
	}
}

void GTextView3::OnUrl(char *Url)
{
	if (Environment)
	{
		Environment->OnNavigate(this, Url);
	}
}

bool GTextView3::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = 32;
	Inf.Width.Max = -1;

	Inf.Height.Min = (Font ? Font->GetHeight() : 18) + 4;
	Inf.Height.Max = -1;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
class GTextView3_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "GTextView3") == 0)
		{
			return new GTextView3(-1, 0, 0, 2000, 2000);
		}

		return 0;
	}
} TextView3_Factory;
