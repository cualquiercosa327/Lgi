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
#include "LgiRes.h"

#ifdef _DEBUG
#define FEATURE_HILIGHT_ALL_MATCHES	1
#else
#define FEATURE_HILIGHT_ALL_MATCHES	0
#endif

#define DefaultCharset              "utf-8"
#define SubtractPtr(a, b)			((a) - (b))

#define GDCF_UTF8					-1
#define LUIS_DEBUG					0
#define POUR_DEBUG					0
#define PROFILE_POUR				0
#define PROFILE_PAINT				0
#define DRAW_LINE_BOXES				0
#define WRAP_POUR_TIMEOUT			90 // ms
#define PULSE_TIMEOUT				100 // ms
#define CURSOR_BLINK				1000 // ms

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
#if DRAW_LINE_BOXES
	#define PAINT_AFTER_LINE			GColour(240, 240, 240)
#else
	#define PAINT_AFTER_LINE			Back
#endif

#define CODEPAGE_BASE				100
#define CONVERT_CODEPAGE_BASE		200

#if !defined(WIN32) && !defined(toupper)
#define toupper(c)					(((c)>='a'&&(c)<='z') ? (c)-'a'+'A' : (c))
#endif

#define THREAD_CHECK() \
	if (!InThread()) \
	{ \
		LgiTrace("%s:%i - %s called out of thread.\n", _FL, __FUNCTION__); \
		return false; \
	}

static char SelectWordDelim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

//////////////////////////////////////////////////////////////////////
class GDocFindReplaceParams3 :
	public GDocFindReplaceParams,
	public LMutex
{
public:
	// Find/Replace History
	GAutoWString LastFind;
	GAutoWString LastReplace;
	bool MatchCase;
	bool MatchWord;
	bool SelectionOnly;
	bool SearchUpwards;
	
	GDocFindReplaceParams3()
	{
		MatchCase = false;
		MatchWord = false;
		SelectionOnly = false;
		SearchUpwards = false;
	}
};

class GTextView3Private : public GCss, public LMutex
{
public:
	GTextView3 *View;
	GRect rPadding;
	int PourX;
	bool LayoutDirty;
	ssize_t DirtyStart, DirtyLen;
	GColour UrlColour;
	bool CenterCursor;
	ssize_t WordSelectMode;
	GString Eol;
	GString LastError;

	// Find/Replace Params
	bool OwnFindReplaceParams;
	GDocFindReplaceParams3 *FindReplaceParams;

	// Map buffer
	ssize_t MapLen;
	char16 *MapBuf;

	// <RequiresLocking>
		// Thread safe Name(char*) impl
		GString SetName;
	// </RequiresLocking>

	GTextView3Private(GTextView3 *view) : LMutex("GTextView3Private")
	{
		View = view;
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
	
	void SetDirty(ssize_t Start, ssize_t Len = 0)
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
	ssize_t At;
	char16 *Text;

public:
	GTextView3Undo(	GTextView3 *view,
					char16 *t,
					ssize_t len,
					ssize_t at,
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
		size_t Len = StrlenW(Text);
		if (View->Text)
		{
			char16 *t = View->Text + At;
			for (size_t i=0; i<Len; i++)
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
				size_t Len = StrlenW(Text);
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

void GTextView3::GStyle::RefreshLayout(size_t Start, ssize_t Len)
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
	
	PourEnabled = true;
	PartialPour = false;
	BlinkTs = 0;
	LineY = 1;
	MaxX = 0;
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
	Blink = true;

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
	Underline = NULL;
	Bold = NULL;
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
			Font = Type.Create();
		else
			printf("%s:%i - failed to create font.\n", _FL);
	}

	if (Font)
	{
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
		Bold = new GFont;
		if (Bold)
		{
			*Bold = *Font;
			Bold->Bold(true);
			Bold->Create();
		}

		OnFontChange();
	}
	else
	{
		LgiTrace("%s:%i - Failed to create font, FontType=%p\n", _FL, FontType);
		Font = SysFont;
	}

	CursorPos.ZOff(1, LineY-1);
	CursorPos.Offset(d->rPadding.x1, d->rPadding.y1);

	GRect r;
	r.ZOff(cx-1, cy-1);
	r.Offset(x, y);
	SetPos(r);
	LgiResources::StyleElement(this);
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
	DeleteObj(Bold);
	// 'd' is owned by the GView::Css auto ptr
}

char16 *GTextView3::MapText(char16 *Str, ssize_t Len, bool RtlTrailingSpace)
{
	if (ObscurePassword /*|| ShowWhiteSpace*/ || RtlTrailingSpace)
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
			/*
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
			*/			

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

void GTextView3::SetCrLf(bool crlf)
{
	CrLf = crlf;
}

void GTextView3::SetTabSize(uint8 i)
{
	TabSize = limit(i, 2, 32);
	OnFontChange();
	OnPosChange();
	Invalidate();
}

void GTextView3::SetWrapType(LDocWrapType i)
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
		if (Font != SysFont)
			DeleteObj(Font);
		Font = f;
	}
	else if (!Font || Font == SysFont)
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
		if (Underline)
		{
			*Underline = *Font;
			Underline->Underline(true);
			Underline->Create();
			if (d->UrlColour.IsValid())
				Underline->Fore(d->UrlColour.c24());
		}

		if (!Bold)
			Bold = new GFont;
		if (Bold)
		{
			*Bold = *Font;
			Bold->Bold(true);
			Bold->Create();
		}
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

void GTextView3::LogLines()
{
	int Idx = 0;
	LgiTrace("DocSize: %i\n", (int)Size);
	for (auto i : Line)
	{
		LgiTrace("  [%i]=%i+%i %s\n", Idx, (int)i->Start, (int)i->Len, i->r.GetStr());
		Idx++;
	}
}

bool GTextView3::ValidateLines(bool CheckBox)
{
	size_t Pos = 0;
	char16 *c = Text;
	size_t Idx = 0;
	GTextLine *Prev = NULL;

	for (auto i : Line)
	{
		GTextLine *l = i;
		if (l->Start != Pos)
		{
			LogLines();
			LgiAssert(!"Incorrect start.");
			return false;
		}

		char16 *e = c;
		if (WrapType == TEXTED_WRAP_NONE)
		{
			while (*e && *e != '\n')
				e++;
		}
		else
		{
			char16 *end = Text + l->Start + l->Len;
			while (*e && *e != '\n' && e < end)
				e++;
		}
			
		ssize_t Len = e - c;
		if (l->Len != Len)
		{
			LogLines();
			LgiAssert(!"Incorrect length.");
			return false;
		}


		if (CheckBox &&
			Prev &&
			Prev->r.y2 != l->r.y1 - 1)
		{
			LogLines();
			LgiAssert(!"Lines not joined vertically");
		}

		if (*e)
		{
			if (*e == '\n')
				e++;
			else if (WrapType == TEXTED_WRAP_REFLOW)
				e++;
		}
		Pos = e - Text;
		c = e;
		Idx++;
		Prev = l;
	}

	if (WrapType == TEXTED_WRAP_NONE &&
		Pos != Size)
	{
		LogLines();
		LgiAssert(!"Last line != end of doc");
		return false;
	}

	return true;
}

// break array, break out of loop when we hit these chars
#define ExitLoop(c)	(	(c) == 0 ||								\
						(c) == '\n' ||							\
						(c) == ' ' ||							\
						(c) == '\t'								\
					)

// extra breaking opportunities
#define ExtraBreak(c) (	( (c) >= 0x3040 && (c) <= 0x30FF ) ||	\
						( (c) >= 0x3300 && (c) <= 0x9FAF )		\
					)

/*
Prerequisite:
The Line list must have either the objects with the correct Start/Len or be missing the lines altogether...
*/
void GTextView3::PourText(size_t Start, ssize_t Length /* == 0 means it's a delete */)
{
	#if PROFILE_POUR
	char _txt[256];
	sprintf_s(_txt, sizeof(_txt), "%p::PourText Lines=%i Sz=%i", this, (int)Line.Length(), (int)Size);
	GProfile Prof(_txt);
	#endif

	LgiAssert(InThread());

	GRect Client = GetClient();
	int Mx = Client.X() - d->rPadding.x1 - d->rPadding.x2;
	int Cy = 0;
	MaxX = 0;

	ssize_t Idx = -1;
	GTextLine *Cur = GetTextLine(Start, &Idx);
	// LgiTrace("Pour %i:%i Cur=%p Idx=%i\n", (int)Start, (int)Length, (int)Cur, (int)Idx);
	if (!Cur || !Cur->r.Valid())
	{
		// Find the last line that has a valid position...
		for (auto i = Idx >= 0 ? Line.begin(Idx) : Line.rbegin(); *i; i--, Idx--)
		{
			Cur = *i;
			if (Cur->r.Valid())
			{
				Cy = Cur->r.y1;
				if (Idx < 0)
					Idx = Line.IndexOf(Cur);
				break;
			}
		}
	}
	if (Cur && !Cur->r.Valid())
		Cur = NULL;
	if (Cur)
	{
		Cy = Cur->r.y1;
		Start = Cur->Start;
		Length = Size - Start;
		// LgiTrace("Reset start to %i:%i because Cur!=NULL\n", (int)Start, (int)Length);
	}
	else
	{
		Idx = 0;
		Start = 0;
		Length = Size;
	}
			
	if (!Text || !Font || Mx <= 0)
		return;

	// Tracking vars
	size_t e;
	//int LastX = 0;
	int WrapCol = GetWrapAtCol();

	GDisplayString Sp(Font, " ", 1);
	int WidthOfSpace = Sp.X();
	if (WidthOfSpace < 1)
	{
		printf("%s:%i - WidthOfSpace test failed.\n", _FL);
		return;
	}

	// Alright... lets pour!
	uint64 StartTs = LgiCurrentTime();
	if (WrapType == TEXTED_WRAP_NONE)
	{
		// Find the dimensions of each line that is missing a rect
		#if PROFILE_POUR
		Prof.Add("NoWrap: ExistingLines");
		#endif
		size_t Pos = 0;
		for (auto i = Line.begin(Idx); *i; i++)
		{
			GTextLine *l = *i;

			if (!l->r.Valid()) // If the layout is not valid...
			{
				GDisplayString ds(Font, Text + l->Start, l->Len);

				l->r.x1 = d->rPadding.x1;
				l->r.x2 = l->r.x1 + ds.X();

				MaxX = MAX(MaxX, l->r.X());
			}

			// Adjust the y position anyway... it's free.
			l->r.y1 = Cy;
			l->r.y2 = l->r.y1 + LineY - 1;
			Cy = l->r.y2 + 1;
			Pos = l->Start + l->Len;
			if (Text[Pos] == '\n')
				Pos++;
		}

		// Now if we are missing lines as well, create them and lay them out
		#if PROFILE_POUR
		Prof.Add("NoWrap: NewLines");
		#endif
		while (Pos < Size)
		{
			GTextLine *l = new GTextLine;
			l->Start = Pos;
			char16 *c = Text + Pos;
			char16 *e = c;
			while (*e && *e != '\n')
				e++;
			l->Len = e - c;

			l->r.x1 = d->rPadding.x1;
			l->r.y1 = Cy;
			l->r.y2 = l->r.y1 + LineY - 1;
			if (l->Len)
			{
				GDisplayString ds(Font, Text + l->Start, l->Len);
				l->r.x2 = l->r.x1 + ds.X();
			}
			else
			{
				l->r.x2 = l->r.x1;
			}

			Line.Insert(l);

			if (*e == '\n')
				e++;

			MaxX = MAX(MaxX, l->r.X());
			Cy = l->r.y2 + 1;
			Pos = e - Text;
		}

		PartialPour = false;
	}
	else // Wrap text
	{
		int DisplayStart = ScrollYLine();
		int DisplayLines = 	(Client.Y() + LineY - 1) / LineY;
		int DisplayEnd = DisplayStart + DisplayLines;
		
		// Pouring is split into 2 parts... 
		// 1) pouring to the end of the displayed text.
		// 2) pouring from there to the end of the document.
		//	potentially taking several goes to complete the full pour
		// This allows the document to display and edit faster..
		bool PourToDisplayEnd = Line.Length() < DisplayEnd;

		#if 0
		LgiTrace("Idx=%i, DisplayStart=%i, DisplayLines=%i, DisplayEnd=%i, PourToDisplayEnd=%i\n",
			Idx, DisplayStart, DisplayLines, DisplayEnd, PourToDisplayEnd);
		#endif

		if (Line.Length() > Idx)
		{
			for (auto i = Line.begin(Idx); *i; i++)
				delete *i;
			Line.Length(Idx);
			Cur = NULL;
		}

		int Cx = 0;
		size_t i;
		for (i=Start; i<Size; i = e)
		{
			// seek till next char of interest
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
				size_t OldE = e;
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
				ssize_t PrevExitChar = -1;
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
				if (PourToDisplayEnd)
				{
					if (Line.Length() > DisplayEnd)
					{
						// We have reached the end of the displayed area... so
						// exit out temporarily to display the layout to the user
						PartialPour = true;
						break;
					}
				}
				else
				{
					// Otherwise check if we are taking too long...
					if (Line.Length() % 20 == 0)
					{
						uint64 Now = LgiCurrentTime();
						if (Now - StartTs > WRAP_POUR_TIMEOUT)
						{
							PartialPour = true;
							// LgiTrace("Pour timeout...\n");
							break;
						}
					}
				}
					
				MaxX = MAX(MaxX, l->r.X());
				Cy += LineY;
					
				if (e < Size)
					e++;
			}
		}

		if (i >= Size)
			PartialPour = false;
		SendNotify(GNotifyCursorChanged);
	}

	#ifdef _DEBUG
	ValidateLines(true);
	#endif
	#if PROFILE_POUR
	Prof.Add("LastLine");
	#endif

	if (!PartialPour)
	{
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
			
				MaxX = MAX(MaxX, l->r.X());
				Cy += LineY;
			}
		}
	}

	bool ScrollYNeeded = Client.Y() < (Line.Length() * LineY);
	bool ScrollChange = ScrollYNeeded ^ (VScroll != NULL);
	d->LayoutDirty = WrapType != TEXTED_WRAP_NONE && ScrollChange;
	#if PROFILE_POUR
	static GString _s;
	_s.Printf("ScrollBars dirty=%i", d->LayoutDirty);
	Prof.Add(_s);
	#endif

	if (ScrollChange)
	{
		// printf("%s:%i - SetScrollBars(%i)\n", _FL, ScrollYNeeded);
		SetScrollBars(false, ScrollYNeeded);
	}
	UpdateScrollBars();
	
	#if 0 // def _DEBUG
	if (GetWindow())
	{
		static char s[256];
		sprintf_s(s, sizeof(s), "Pour: %.2f sec", (double)_PourTime / 1000);
		GetWindow()->PostEvent(M_TEXTVIEW_DEBUG_TEXT, (GMessage::Param)s);
	}
	#endif

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
	size_t Last = 0;
	int n = 0;

	// LgiTrace("StartStyle=%i,%i(%i) %s\n", (int)s->Start, (int)s->Len, (int)(s->Start+s->Len), s->Fore.GetStr());

	if (Style.Length() > 0)
	{
		// Optimize for last in the list
		GStyle *Last = Style.Last();
		if (s->Start >= Last->Start + Last->Len)
		{
			Style.Insert(s.Release());
			return true;
		}
	}

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

GTextView3::GStyle *GTextView3::GetNextStyle(ssize_t Where)
{
	GStyle *s = (Where >= 0) ? Style.First() : Style.Next();
	while (s)
	{
		// determin whether style is relevent..
		// styles in the selected region are ignored
		ssize_t Min = MIN(SelStart, SelEnd);
		ssize_t Max = MAX(SelStart, SelEnd);
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

	GUrl(GTextViewStyleOwners own) : GStyle(own)
	{
		Email = false;
	}

	bool OnMouseClick(GMouse *m)
	{
		if (View)
		{
			if ( (m && m->Left() && m->Double()) || (!m) )
			{
				char *Utf8 = WideToUtf8(View->NameW() + Start, Len);
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
				char *Url = WideToUtf8(View->NameW() + Start, Len);
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

	CURSOR_CHAR GetCursor()
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

GTextView3::GStyle *GTextView3::HitStyle(ssize_t i)
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

void GTextView3::PourStyle(size_t Start, ssize_t EditSize)
{
	#ifdef _DEBUG
	int64 StartTime = LgiCurrentTime();
	#endif

	LgiAssert(InThread());

	if (!Text || Size < 1)
		return;

	ssize_t Length = MAX(EditSize, 0);

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

				if (s->Overlap(Start, EditSize < 0 ? -EditSize : EditSize))
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
		GArray<GLinkInfo> Links;		
		LgiAssert(Start + Length <= Size);		
		if (LgiDetectLinks(Links, Text + Start, Length))
		{
			for (uint32 i=0; i<Links.Length(); i++)
			{
				GLinkInfo &Inf = Links[i];
				GUrl *Url;
                GAutoPtr<GTextView3::GStyle> a(Url = new GUrl(STYLE_NONE));
				if (Url)
				{
					Url->View = this;
					Url->Start = (int) (Inf.Start + Start);
					Url->Len = (int)Inf.Len;
					Url->Email = Inf.Email;
					Url->Font = Underline;
					Url->Fore = d->UrlColour;

					InsertStyle(a);
				}
			}
		}
	}

	#ifdef _DEBUG
	_StyleTime = LgiCurrentTime() - StartTime;
	#endif
}

bool GTextView3::Insert(size_t At, char16 *Data, ssize_t Len)
{
	GProfile Prof("GTextView3::Insert");
	Prof.HideResultsIfBelow(10);

	LgiAssert(InThread());
	
	if (!ReadOnly && Len > 0)
	{
		if (!Data)
			return false;

		// limit input to valid data
		At = MIN(Size, At);

		// make sure we have enough memory
		size_t NewAlloc = Size + Len + 1;
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
		
		Prof.Add("MemChk");

		if (Text)
		{
			// Insert the data

			// Move the section after the insert to make space...
			memmove(Text+(At+Len), Text+At, (Size-At) * sizeof(char16));

			Prof.Add("Undo");
			
			// Add the undo object...
			if (UndoOn)
				UndoQue += new GTextView3Undo(this, Data, Len, At, UndoInsert);

			Prof.Add("Cpy");

			// Copy new data in...
			memcpy(Text+At, Data, Len * sizeof(char16));
			Size += Len;
			Text[Size] = 0; // NULL terminate


			// Clear layout info for the new text
			ssize_t Idx = -1;
			GTextLine *Cur = NULL;
			
			if (Line.Length() == 0)
			{
				// Empty doc... set up the first line
				Line.Insert(Cur = new GTextLine);
				Idx = 0;
				Cur->Start = 0;
			}
			else
			{
				Cur = GetTextLine(At, &Idx);
			}

			if (Cur)
			{
				if (WrapType == TEXTED_WRAP_NONE)
				{
					// Clear layout for current line...
					Cur->r.ZOff(-1, -1);

					Prof.Add("NoWrap add lines");
					
					// Add any new lines that we need...
					char16 *e = Text + At + Len;
					char16 *c;
					for (c = Text + At; c < e; c++)
					{
						if (*c == '\n')
						{
							// Set the size of the current line...
							size_t Pos = c - Text;
							Cur->Len = Pos - Cur->Start;

							// Create a new line...
							Cur = new GTextLine();
							if (!Cur)
								return false;
							Cur->Start = Pos + 1;
							Line.Insert(Cur, ++Idx);
						}
					}

					Prof.Add("CalcLen");

					// Make sure the last Line's length is set..
					Cur->CalcLen(Text);

					Prof.Add("UpdatePos");
	
					// Now update all the positions of the following lines...
					for (auto i = Line.begin(++Idx); *i; i++)
						(*i)->Start += Len;
				}
				else
				{
					// Clear all lines to the end of the doc...
					for (auto i = Line.begin(Idx); *i; i++)
						delete *i;
					Line.Length(Idx);
				}
			}
			else
			{
				// If wrap is on then this can happen when an Insert happens before the 
				// OnPulse event has laid out the new text. Probably not a good thing in
				// non-wrap mode			
				if (WrapType == TEXTED_WRAP_NONE)
				{
					GTextLine *l = Line.Last();
					printf("%s:%i - Insert error: no cur, At=%i, Size=%i, Lines=%i, WrapType=%i\n",
						_FL, (int)At, (int)Size, (int)Line.Length(), (int)WrapType);
					if (l)
						printf("Last=%i, %i\n", (int)l->Start, (int)l->Len);
				}
			}

			#ifdef _DEBUG
			Prof.Add("Validate");
			ValidateLines();
			#endif

			Dirty = true;
			if (PourEnabled)
			{
				Prof.Add("PourText");
				PourText(At, Len);
				Prof.Add("PourStyle");
				PourStyle(At, Len);
			}
			SendNotify(GNotifyDocChanged);

			return true;
		}
	}

	return false;
}

bool GTextView3::Delete(size_t At, ssize_t Len)
{
	bool Status = false;

	LgiAssert(InThread());

	if (!ReadOnly)
	{
		// limit input
		At = MAX(At, 0);
		At = MIN(At, Size);
		Len = MIN(Size-At, Len);

		if (Len > 0)
		{
			int HasNewLine = 0;

			for (int i=0; i<Len; i++)
			{
				if (Text[At + i] == '\n')
					HasNewLine++;
			}

			// do delete
			if (UndoOn)
			{
				UndoQue += new GTextView3Undo(this, Text+At, Len, At, UndoDelete);
			}

			memmove(Text+At, Text+(At+Len), (Size-At-Len) * sizeof(char16));
			Size -= Len;
			Text[Size] = 0;

			if (WrapType == TEXTED_WRAP_NONE)
			{
				ssize_t Idx = -1;
				GTextLine *Cur = GetTextLine(At, &Idx);
				if (Cur)
				{
					Cur->r.ZOff(-1, -1);

					// Delete some lines...
					for (int i=0; i<HasNewLine; i++)
					{
						GTextLine *l = Line[Idx + 1];
						delete l;
						Line.DeleteAt(Idx + 1);
					}

					// Correct the current line's length
					Cur->CalcLen(Text);

					// Shift all further lines down...
					for (auto i = Line.begin(Idx + 1); *i; i++)
						(*i)->Start -= Len;
				}
			}
			else
			{
				ssize_t Index;
				GTextLine *Cur = GetTextLine(At, &Index);
				if (Cur)
				{
					for (auto i = Line.begin(Index); *i; i++)
						delete *i;
					Line.Length(Index);
				}
			}
			

			Dirty = true;
			Status = true;

			#ifdef _DEBUG
			ValidateLines();
			#endif
			if (PourEnabled)
			{
				PourText(At, -Len);
				PourStyle(At, -Len);
			}
			
			if (Cursor >= At && Cursor <= At + Len)
			{
				SetCaret(At, false, HasNewLine);
			}

			// Handle repainting in flowed mode, when the line starts change
			if (WrapType == TEXTED_WRAP_REFLOW)
			{
				ssize_t Index;
				GTextLine *Cur = GetTextLine(At, &Index);
				if (Cur)
				{
					GRect r = Cur->r;
					r.x2 = GetClient().x2;
					r.y2 = GetClient().y2;
					Invalidate(&r);
				}
			}

			SendNotify(GNotifyDocChanged);
			Status = true;
		}
	}

	return Status;
}

void GTextView3::DeleteSelection(char16 **Cut)
{
	if (SelStart >= 0)
	{
		ssize_t Min = MIN(SelStart, SelEnd);
		ssize_t Max = MAX(SelStart, SelEnd);

		if (Cut)
		{
			*Cut = NewStrW(Text + Min, Max - Min);
		}

		Delete(Min, Max - Min);
		SetCaret(Min, false, true);
	}
}

GTextView3::GTextLine *GTextView3::GetTextLine(ssize_t Offset, ssize_t *Index)
{
	int i = 0;

	for (GTextLine *l = Line.First(); l; l = Line.Next(), i++)
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

	return NULL;
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

GString GTextView3::operator[](int LineIdx)
{
	if (LineIdx <= 0 || LineIdx > GetLines())
		return GString();

	GTextLine *Ln = Line[LineIdx-1];
	if (!Ln)
		return GString();

	GString s(Text + Ln->Start, Ln->Len);
	return s; 
}

char *GTextView3::Name()
{
	UndoQue.Empty();
	DeleteArray(TextCache);
	TextCache = WideToUtf8(Text);

	return TextCache;
}

bool GTextView3::Name(const char *s)
{
	if (InThread())
	{
		UndoQue.Empty();
		DeleteArray(TextCache);
		DeleteArray(Text);
		Line.DeleteObjects();

		LgiAssert(LgiIsUtf8(s));
		Text = Utf8ToWide(s);
		if (!Text)
		{
			Text = new char16[1];
			if (Text) *Text = 0;
		}

		Size = Text ? StrlenW(Text) : 0;
		Alloc = Size + 1;
		Cursor = MIN(Cursor, Size);
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
		d->SetDirty(0, Size);
		PourText(0, Size);
		PourStyle(0, Size);
		UpdateScrollBars();
		Invalidate();
	}
	else if (d->Lock(_FL))
	{
		if (IsAttached())
		{
			d->SetName = s;
			PostEvent(M_TEXT_UPDATE_NAME);
		}
		else LgiAssert(!"Can't post event to detached/virtual window.");
		d->Unlock();
	}

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
	Cursor = MIN(Cursor, Size);
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
	Line.DeleteObjects();
	d->SetDirty(0, Size);
	PourText(0, Size);
	PourStyle(0, Size);
	UpdateScrollBars();
	Invalidate();

	return true;
}

GRange GTextView3::GetSelectionRange()
{
	GRange r;
	if (HasSelection())
	{
		r.Start = MIN(SelStart, SelEnd);
		ssize_t End = MAX(SelStart, SelEnd);
		r.Len = End - r.Start;
	}
	return r;
}

char *GTextView3::GetSelection()
{
	GRange s = GetSelectionRange();
	if (s.Len > 0)
	{
		return (char*)LgiNewConvertCp("utf-8", Text + s.Start, LGI_WideCharset, s.Len*sizeof(Text[0]) );
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

size_t GTextView3::GetLines()
{
	return Line.Length();
}

void GTextView3::GetTextExtent(int &x, int &y)
{
	PourText(0, Size);

	x = MaxX + d->rPadding.x1;
	y = (int)(Line.Length() * LineY);
}

bool GTextView3::GetLineColumnAtIndex(GdcPt2 &Pt, int Index)
{
	ssize_t FromIndex = 0;
	GTextLine *From = GetTextLine(Index < 0 ? Cursor : Index, &FromIndex);
	if (!From)
		return false;

	Pt.x = (int) (Cursor - From->Start);
	Pt.y = (int) FromIndex;
	return true;
}

ssize_t GTextView3::GetCaret(bool Cur)
{
	if (Cur)
	{
		return Cursor;
	}

	return 0;
}

ssize_t GTextView3::IndexAt(int x, int y)
{
	GTextLine *l = Line.ItemAt(y);
	if (l)
	{
		return l->Start + MIN(x, l->Len);
	}

	return 0;
}

void GTextView3::SetCaret(size_t i, bool Select, bool ForceFullUpdate)
{
    // int _Start = LgiCurrentTime();
	Blink = true;

	// Bound the new cursor position to the document
	if (i > Size) i = Size;

	// Store the old selection and cursor
	ssize_t s = SelStart, e = SelEnd, c = Cursor;
	
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

	ssize_t FromIndex = 0;
	GTextLine *From = GetTextLine(Cursor, &FromIndex);

	Cursor = i;

	// check the cursor is on the screen
	ssize_t ToIndex = 0;
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
				ssize_t i = ToIndex - (DisplayLines >> 1);
				VScroll->Value(MAX(0, i));
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
			
			ssize_t v = MIN(ToIndex - YOff + 1, Line.Length() - DisplayLines);
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
	else if
	(
		(
			SelStart != s
			||
			SelEnd != e
		)
	)
	{
		// Update just the selection bounds
		GRect Client = GetClient();
		size_t Start, End;
		if (SelStart >= 0 && s >= 0)
		{
			// Selection has changed, union the before and after regions
			Start = MIN(Cursor, c);
			End = MAX(Cursor, c);
		}
		else if (SelStart >= 0)
		{
			// Selection created...
			Start = MIN(SelStart, SelEnd);
			End = MAX(SelStart, SelEnd);
		}
		else if (s >= 0)
		{
			// Selection removed...
			Start = MIN(s, e);
			End = MAX(s, e);
		}
		else
		{
			LgiAssert(0);
			return;
		}

		GTextLine *SLine = GetTextLine(Start);
		GTextLine *ELine = GetTextLine(End);
		GRect u;
		if (SLine && ELine)
		{
			if (SLine->r.Valid())
			{
				u = DocToScreen(SLine->r);
			}
			else
				u.Set(0, 0, Client.X()-1, 1); // Start of visible page 
			
			GRect b(0, Client.Y()-1, Client.X()-1, Client.Y()-1);
			if (ELine->r.Valid())
			{
				b = DocToScreen(ELine->r);
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
			/*
			printf("%s,%i - Couldn't get SLine and ELine: %i->%p, %i->%p\n",
				_FL,
				(int)Start, SLine,
				(int)End, ELine);
			*/
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
		SendNotify(GNotifyCursorChanged);
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
		char *Txt8 = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), Txt16, LGI_WideCharset);

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
		ssize_t Min = MIN(SelStart, SelEnd);
		ssize_t Max = MAX(SelStart, SelEnd);

		char16 *Txt16 = NewStrW(Text+Min, Max-Min);
		#ifdef WIN32
		Txt16 = ConvertToCrLf(Txt16);
		#endif
		char *Txt8 = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), Txt16, LGI_WideCharset);

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
			Mem.Reset(Utf8ToWide(s));
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
	ssize_t Len = StrlenW(t);
	Insert(Cursor, t, Len);
	SetCaret(Cursor+Len, false, true); // Multiline
	
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
		int64 Bytes = f.GetSize();
		if (Bytes < 0 || Bytes & 0xffff000000000000LL)
		{
			LgiTrace("%s:%i - Invalid file size: " LGI_PrintfInt64 "\n", _FL, Bytes);
			return false;
		}
			
		SetCaret(0, false);
		
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
	GString TmpName;
	bool Status = false;
	
	d->LastError.Empty();

	if (f.Open(Name, O_WRITE))
	{
		if (f.SetSize(0) != 0)
		{
			// Can't resize file, fall back to renaming it and 
			// writing a new file...
			f.Close();
			TmpName = Name;
			TmpName += ".tmp";
			if (!FileDev->Move(Name, TmpName))
			{
				LgiTrace("%s:%i - Failed to move '%s'.\n", _FL, Name);
				return false;
			}

			if (!f.Open(Name, O_WRITE))
			{
				LgiTrace("%s:%i - Failed to open '%s' for writing.\n", _FL, Name);
				return false;
			}
		}

		if (Text)
		{			
			char *c8 = (char*)LgiNewConvertCp(CharSet ? CharSet : DefaultCharset, Text, LGI_WideCharset, Size * sizeof(char16));
			if (c8)
			{
				int Len = (int)strlen(c8);
				if (CrLf)
				{
					Status = true;

					int BufLen = 1 << 20;
					GAutoString Buf(new char[BufLen]);
					char *b = Buf;
					char *e = Buf + BufLen;
					char *c = c8;
					
					while (*c)
					{
					    if (b > e - 10)
					    {
					        ptrdiff_t Bytes = b - Buf;
					        if (f.Write(Buf, (int)Bytes) != Bytes)
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

			        ptrdiff_t Bytes = b - Buf;
			        if (f.Write(Buf, (int)Bytes) != Bytes)
			            Status = false;
				}
				else
				{
					Status = f.Write(c8, Len) == Len;
				}

				DeleteArray(c8);
			}

			Dirty = false;
		}
	}
	else
	{
		int Err = f.GetError();
		GAutoString sErr = LgiErrorCodeToString(Err);		
		d->LastError.Printf("Failed to open '%s' for writing: %i - %s\n", Name, Err, sErr.Get());
	}

	if (TmpName)
		FileDev->Delete(TmpName);

	return Status;
}

const char *GTextView3::GetLastError()
{
	return d->LastError;
}

void GTextView3::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		GRect Before = GetClient();

		int DisplayLines = Y() / LineY;
		ssize_t Lines = GetLines();
		// printf("SetLimits %i, %i\n", 0, (int)Lines);
		VScroll->SetLimits(0, Lines);
		if (VScroll)
		{
			VScroll->SetPage(DisplayLines);
			
			ssize_t Max = Lines - DisplayLines + 1;
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
		ssize_t Min = MIN(SelStart, SelEnd);
		ssize_t Max = MAX(SelStart, SelEnd);

		if (Min < Max)
		{
			UndoQue += new GTextView3Undo(this, Text + Min, Max-Min, Min, UndoChange);

			for (ssize_t i=Min; i<Max; i++)
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

			SendNotify(GNotifyDocChanged);
		}
	}
	
	return true;
}

ssize_t GTextView3::GetLine()
{
	ssize_t Idx = 0;
	GetTextLine(Cursor, &Idx);
	return Idx + 1;
}

void GTextView3::SetLine(int i)
{
	GTextLine *l = Line.ItemAt(i - 1);
	if (l)
	{
		d->CenterCursor = true;
		SetCaret(l->Start, false);
		d->CenterCursor = false;
	}
}

bool GTextView3::DoGoto()
{
	GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto line:"), "Text");
	if (Dlg.DoModal() == IDOK &&
		Dlg.GetStr())
	{
		SetLine(atoi(Dlg.GetStr()));
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
	bool Status = false;

	if (InThread())
	{
		if (d->FindReplaceParams->Lock(_FL))
		{
			if (d->FindReplaceParams->LastFind)
				Status = OnFind(d->FindReplaceParams->LastFind,
								d->FindReplaceParams->MatchWord,
								d->FindReplaceParams->MatchCase,
								d->FindReplaceParams->SelectionOnly,
								d->FindReplaceParams->SearchUpwards);
							
			d->FindReplaceParams->Unlock();
		}
	}
	else if (IsAttached())
	{
		Status = PostEvent(M_TEXTVIEW_FIND);
	}
	
	return Status;
}

bool
Text3_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	GTextView3 *v = (GTextView3*) User;

	if (v->d->FindReplaceParams &&
		v->d->FindReplaceParams->Lock(_FL))
	{
		v->d->FindReplaceParams->MatchWord = Dlg->MatchWord;
		v->d->FindReplaceParams->MatchCase = Dlg->MatchCase;
		v->d->FindReplaceParams->SelectionOnly = Dlg->SelectionOnly;
		v->d->FindReplaceParams->SearchUpwards = Dlg->SearchUpwards;
		v->d->FindReplaceParams->LastFind.Reset(Utf8ToWide(Dlg->Find));
		
		v->d->FindReplaceParams->Unlock();
	}

	return v->DoFindNext();
}

bool GTextView3::DoFind()
{
	GString u;

	if (HasSelection())
	{
		ssize_t Min = MIN(SelStart, SelEnd);
		ssize_t Max = MAX(SelStart, SelEnd);

		u = GString(Text + Min, Max - Min);
	}
	else
	{
		u = d->FindReplaceParams->LastFind.Get();
	}

	#ifdef BEOS

		GFindDlg *Dlg = new GFindDlg(this, u, Text3_FindCallback, this);
		if (Dlg)
			Dlg->DoModeless();

	#else

		GFindDlg Dlg(this, u, Text3_FindCallback, this);
		Dlg.DoModal();
		Focus(true);

	#endif

	return false;
}

bool GTextView3::DoReplace()
{
	bool SingleLineSelection = false;
	SingleLineSelection = HasSelection();
	if (SingleLineSelection)
	{
		GRange Sel = GetSelectionRange();
		for (ssize_t i = Sel.Start; i < Sel.End(); i++)
		{
			if (Text[i] == '\n')
			{
				SingleLineSelection = false;
				break;
			}
		}
	}

	char *LastFind8 = SingleLineSelection ? GetSelection() : WideToUtf8(d->FindReplaceParams->LastFind);
	char *LastReplace8 = WideToUtf8(d->FindReplaceParams->LastReplace);
	
	GReplaceDlg Dlg(this, LastFind8, LastReplace8);

	Dlg.MatchWord = d->FindReplaceParams->MatchWord;
	Dlg.MatchCase = d->FindReplaceParams->MatchCase;
	Dlg.SelectionOnly = HasSelection();

	int Action = Dlg.DoModal();

	DeleteArray(LastFind8);
	DeleteArray(LastReplace8);

	if (Action != IDCANCEL)
	{
		d->FindReplaceParams->LastFind.Reset(Utf8ToWide(Dlg.Find));
		d->FindReplaceParams->LastReplace.Reset(Utf8ToWide(Dlg.Replace));
		d->FindReplaceParams->MatchWord = Dlg.MatchWord;
		d->FindReplaceParams->MatchCase = Dlg.MatchCase;
		d->FindReplaceParams->SelectionOnly = Dlg.SelectionOnly;
	}

	switch (Action)
	{
		case IDC_FR_FIND:
		{
			OnFind(	d->FindReplaceParams->LastFind,
					d->FindReplaceParams->MatchWord,
					d->FindReplaceParams->MatchCase,
					d->FindReplaceParams->SelectionOnly,
					d->FindReplaceParams->SearchUpwards);
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
						d->FindReplaceParams->SelectionOnly,
						d->FindReplaceParams->SearchUpwards);
			break;
		}
	}

	return false;
}

void GTextView3::SelectWord(size_t From)
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

typedef int (*StringCompareFn)(const char16 *a, const char16 *b, ssize_t n);

ptrdiff_t GTextView3::MatchText(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly, bool SearchUpwards)
{
	if (!ValidStrW(Find))
		return -1;

	ssize_t FindLen = StrlenW(Find);
		
	// Setup range to search
	ssize_t Begin, End;
	if (SelectionOnly && HasSelection())
	{
		Begin = MIN(SelStart, SelEnd);
		End = MAX(SelStart, SelEnd);
	}
	else
	{
		Begin = 0;
		End = Size;
	}

	// Look through text...
	ssize_t i;
	bool Wrap = false;
	if (Cursor > End - FindLen)
	{
		Wrap = true;
		if (SearchUpwards)
			i = End - FindLen;
		else
			i = Begin;
	}
	else
	{
		i = Cursor;
	}
		
	if (i < Begin) i = Begin;
	if (i > End) i = End;
	
	StringCompareFn CmpFn = MatchCase ? StrncmpW : StrnicmpW;
	char16 FindCh = MatchCase ? Find[0] : toupper(Find[0]);

	for (; SearchUpwards ? i >= Begin : i <= End - FindLen; i += SearchUpwards ? -1 : 1)
	{
		if
		(
			(MatchCase ? Text[i] : toupper(Text[i]))
			==
			FindCh
		)
		{
			char16 *Possible = Text + i;

			if (CmpFn(Possible, Find, FindLen) == 0)
			{
				if (MatchWord)
				{
					// Check boundaries
							
					if (Possible > Text) // Check off the start
					{
						if (!IsWordBoundry(Possible[-1]))
							continue;
					}
					if (i + FindLen < Size) // Check off the end
					{
						if (!IsWordBoundry(Possible[FindLen]))
							continue;
					}
				}
						
				GRange r(Possible - Text, FindLen);
				if (!r.Overlap(Cursor))
					return r.Start;
			}
		}
				
		if (!Wrap && (i + 1 > End - FindLen))
		{
			Wrap = true;
			i = Begin;
			End = Cursor;
		}
	}
	
	return -1;
}

bool GTextView3::OnFind(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly, bool SearchUpwards)
{
	THREAD_CHECK();

	// Not sure what this is doing???
	if (HasSelection() &&
		SelEnd < SelStart)
	{
		Cursor = SelStart;
	}

	#if FEATURE_HILIGHT_ALL_MATCHES

	// Clear existing styles for matches
	for (GStyle *s = Style.First(); s; )
	{
		if (s->Owner == STYLE_FIND_MATCHES)
		{
			Style.Delete(s);
			DeleteObj(s);
			s = Style.Current();
		}
		else
		{
			s = Style.Next();
		}
	}

	ssize_t FindLen = StrlenW(Find);
	ssize_t FirstLoc = MatchText(Find, MatchWord, MatchCase, false, SearchUpwards), Loc;
	if (FirstLoc >= 0)
	{
		SetCaret(FirstLoc, false);
		SetCaret(FirstLoc + FindLen, true);
	}

	ssize_t Old = Cursor;
	if (!SearchUpwards)
		Cursor += FindLen;
	
	while ((Loc = MatchText(Find, MatchWord, MatchCase, false, false)) != FirstLoc)
	{
		GAutoPtr<GStyle> s(new GStyle(STYLE_FIND_MATCHES));
		s->Start = Loc;
		s->Len = FindLen;
		s->Fore.Set(LC_FOCUS_SEL_FORE, 24);
		s->Back = GColour(LC_FOCUS_SEL_BACK, 24).Mix(GColour(LC_WORKSPACE, 24));
		InsertStyle(s);

		Cursor = Loc + FindLen;
	}

	Cursor = Old;
	Invalidate();

	#else

	ssize_t Loc = MatchText(Find, MatchWord, MatchCase, SelectionOnly, SearchUpwards);
	if (Loc >= 0)
	{
		SetCaret(Loc, false);
		SetCaret(Loc + StrlenW(Find), true);
		return true;
	}

	#endif

	return false;
}

bool GTextView3::OnReplace(char16 *Find, char16 *Replace, bool All, bool MatchWord, bool MatchCase, bool SelectionOnly, bool SearchUpwards)
{
	THREAD_CHECK();

	if (ValidStrW(Find))
	{
		// int Max = -1;
		ssize_t FindLen = StrlenW(Find);
		ssize_t ReplaceLen = StrlenW(Replace);
		// size_t OldCursor = Cursor;
		ptrdiff_t First = -1;

		while (true)
		{
			ptrdiff_t Loc = MatchText(Find, MatchWord, MatchCase, SelectionOnly, SearchUpwards);
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
				ssize_t OldSelStart = SelStart;
				ssize_t OldSelEnd = SelEnd;
				
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
	}	
	
	return false;
}

ssize_t GTextView3::SeekLine(ssize_t Offset, GTextViewSeek Where)
{
	THREAD_CHECK();

	switch (Where)
	{
		case PrevLine:
		{
			for (; Offset > 0 && Text[Offset] != '\n'; Offset--)
				;
			
			if (Offset > 0)
				Offset--;
			
			for (; Offset > 0 && Text[Offset] != '\n'; Offset--)
				;

			if (Offset > 0)
				Offset++;
			break;
		}
		case NextLine:
		{
			for (; Offset < Size && Text[Offset] != '\n'; Offset++)
				;

			Offset++;
			break;
		}
		case StartLine:
		{
			for (; Offset > 0 && Text[Offset] != '\n'; Offset--)
				;

			if (Offset > 0)
				Offset++;
			break;
		}
 		case EndLine:
		{
			for (; Offset < Size && Text[Offset] != '\n'; Offset++)
				;
			break;
		}
		default:
		{
			LgiAssert(false);
			break;
		}
	}

	return Offset;
}

bool GTextView3::OnMultiLineTab(bool In)
{
	bool Status = false;
	ssize_t Min = MIN(SelStart, SelEnd);
	ssize_t Max = MAX(SelStart, SelEnd), i;

	Min = SeekLine(Min, StartLine);

	int Ls = 0;
	GArray<ssize_t> p;
	for (i=Min; i<Max && i<Size; i=SeekLine(i, NextLine))
	{
		p.Add(i);
		Ls++;
	}
	if (Max < i)
	{
		Max = SeekLine(Max, EndLine);
	}

	PourEnabled = false;

	ssize_t *Indexes = p.AddressOf(0);
	if (!Indexes)
		return false;

	for (i=Ls-1; i>=0; i--)
	{
		if (In)
		{
			// <-
			ssize_t n = Indexes[i], Space = 0;
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

			ssize_t Chs = n-Indexes[i];
			Delete(Indexes[i], Chs);
			Max -= Chs;
		}
		else
		{
			// ->
			ssize_t Len = Indexes[i];
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

	PourEnabled = true;
	PourText(Min, Max - Min);
	PourStyle(Min, Max - Min);
	
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

	if (!Processing)
	{
		Processing = true;
		GLayout::OnPosChange();

		GRect c = GetClient();
		bool ScrollYNeeded = c.Y() < (Line.Length() * LineY);
		bool ScrollChange = ScrollYNeeded ^ (VScroll != NULL);
		if (ScrollChange)
		{
			// printf("%s:%i - SetScrollBars(%i)\n", _FL, ScrollYNeeded);
			SetScrollBars(false, ScrollYNeeded);
		}
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

int GTextView3::OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;
	
	for (unsigned i=0; i<Data.Length(); i++)
	{
		GDragData &dd = Data[i];
		if (dd.Data.Length() <= 0)
			continue;
		
		if (dd.IsFormat("text/html") || dd.IsFormat("UniformResourceLocatorW"))
		{
			GVariant *Data = &dd.Data[0];
			LgiAssert(dd.Data.Length() == 1); // Impl multiple data entries if this asserts.
			if (Data->IsBinary())
			{
				OsChar *e = (OsChar*) ((char*)Data->Value.Binary.Data + Data->Value.Binary.Length);
				OsChar *s = (OsChar*) Data->Value.Binary.Data;
				int len = 0;
				while (s < e && s[len])
				{
					len++;
				}
				
				GAutoWString w
				(
					(char16*)LgiNewConvertCp
					(
						LGI_WideCharset,
						s,
						(
							sizeof(OsChar) == 1
							?
							"utf-8"
							:
							LGI_WideCharset
						),
						len * sizeof(*s)
					)
				);
				
				Insert(Cursor, w, len);
				Invalidate();
				return DROPEFFECT_COPY;
			}
		}
		else if (dd.IsFileDrop())
		{
			// We don't directly handle file drops... pass up to the parent
			for (GViewI *p = GetParent(); p; p = p->GetParent())
			{
				GDragDropTarget *t = p->DropTarget();
				if (t)
				{
					Status = t->OnDrop(Data, Pt, KeyState);
					break;
				}
			}
		}
	}

	return Status;
}

void GTextView3::OnCreate()
{
	SetWindow(this);
	DropTarget(true);

	SetPulse(PULSE_TIMEOUT);
}

void GTextView3::OnEscape(GKey &K)
{
}

bool GTextView3::OnMouseWheel(double l)
{
	if (VScroll)
	{
		int64 NewPos = VScroll->Value() + (int)l;
		NewPos = limit(NewPos, 0, GetLines());
		VScroll->Value(NewPos);
		Invalidate();
	}
	
	return true;
}

void GTextView3::OnFocus(bool f)
{
	Invalidate();
}

ssize_t GTextView3::HitText(int x, int y, bool Nearest)
{
	if (!Text)
		return 0;

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
			ssize_t Char = 0;
				
			GDisplayString Ds(Font, MapText(Text + l->Start, l->Len), l->Len, 0);
			Char = Ds.CharAt(At, Nearest ? LgiNearest : LgiTruncate);

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
		Y++;
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

	return 0;
}

void GTextView3::Undo()
{
	int Old = UndoQue.GetPos();
	UndoQue.Undo();
	if (Old && !UndoQue.GetPos())
	{
		Dirty = false;
		SendNotify(GNotifyDocChanged);
	}
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

	GStyle *s = HitStyle(HitText(m.x, m.y, true));
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
	switch (Id = RClick.Float(this, m))
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
				IndentSize = atoi(i.GetStr());
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
				SetTabSize(atoi(i.GetStr()));
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

			ssize_t Hit = HitText(m.x, m.y, true);
			if (Hit >= 0)
			{
				SetCaret(Hit, m.Shift());

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

	ssize_t Hit = HitText(m.x, m.y, true);
	if (IsCapturing())
	{
		if (d->WordSelectMode < 0)
		{
			SetCaret(Hit, m.Left());
		}
		else
		{
			ssize_t Min = Hit < d->WordSelectMode ? Hit : d->WordSelectMode;
			ssize_t Max = Hit > d->WordSelectMode ? Hit : d->WordSelectMode;

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
		for (size_t i=l->Start; i<Cursor; i++)
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
								size_t Min = MIN(SelStart, SelEnd), Max = MAX(SelStart, SelEnd);
								for (size_t i=Min; i<Max; i++)
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
						size_t Len = (l) ? l->Len : 0;
						
						if (l && k.c16 == VK_TAB && (!HardTabs || IndentSize != TabSize))
						{
							int x = GetColumn();							
							int Add = IndentSize - (x % IndentSize);
							
							if (HardTabs && ((x + Add) % TabSize) == 0)
							{
								int Rx = x;
								size_t Remove;
								for (Remove = Cursor; Text[Remove - 1] == ' ' && Rx % TabSize != 0; Remove--, Rx--);
								ssize_t Chars = (ssize_t)Cursor - Remove;
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
										size_t NewLen = (l) ? l->Len : 0;
										SetCaret(Cursor + Add, false, Len != NewLen - 1);
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
										SetCaret(Cursor, false, false);
									}
									else if (Text[Cursor-1] == ' ')
									{
										ssize_t Start = (ssize_t)Cursor - 1;
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
										SetCaret(Start + Use, false, false);
									}
								}
								
							}
							else if (In && Insert(Cursor, &In, 1))
							{
								l = GetTextLine(Cursor);
								size_t NewLen = (l) ? l->Len : 0;
								SetCaret(Cursor + 1, false, Len != NewLen - 1);
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
							ssize_t i;
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
								ssize_t Del = (ssize_t)Cursor - i;
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
							ssize_t Start = Cursor;
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
						SetCaret(MIN(SelStart, SelEnd), false);
					}
					else if (Cursor > 0)
					{
						ssize_t n = Cursor;

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

						SetCaret(n, k.Shift());
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
						SetCaret(MAX(SelStart, SelEnd), false);
					}
					else if (Cursor < Size)
					{
						ssize_t n = Cursor;

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

						SetCaret(n, k.Shift());
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
							ssize_t CharX = PrevLine.CharAt(ScreenX);

							SetCaret(Prev->Start + MIN(CharX, Prev->Len), k.Shift());
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
							ssize_t CharX = NextLine.CharAt(ScreenX);

							SetCaret(Next->Start + MIN(CharX, Next->Len), k.Shift());
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
						SetCaret(Size, k.Shift());
					}
					else
					{
						#ifdef MAC
						Jump_EndOfLine:
						#endif
						GTextLine *l = GetTextLine(Cursor);
						if (l)
						{
							SetCaret(l->Start + l->Len, k.Shift());
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
						SetCaret(0, k.Shift());
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
							ssize_t Whitespace = SubtractPtr(s, Line);

							if (l->Start + Whitespace == Cursor)
							{
								SetCaret(l->Start, k.Shift());
							}
							else
							{
								SetCaret(l->Start + Whitespace, k.Shift());
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
						ssize_t CurLine = Line.IndexOf(l);

						GTextLine *New = Line.ItemAt(MAX(CurLine - DisplayLines, 0));
						if (New)
						{
							SetCaret(New->Start + MIN(Cursor - l->Start, New->Len), k.Shift());
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
						ssize_t CurLine = Line.IndexOf(l);

						GTextLine *New = Line.ItemAt(MIN(CurLine + DisplayLines, GetLines()-1));
						if (New)
						{
							SetCaret(New->Start + MIN(Cursor - l->Start, New->Len), k.Shift());
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
		SetCaret(Cursor + StrlenW(InsertStr), false, true);
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

GRect GTextView3::DocToScreen(GRect r)
{
	r.Offset(0, d->rPadding.y1 - ScrollYPixel());
	return r;
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

	#if PROFILE_PAINT
	char s[256];
	sprintf_s(s, sizeof(s), "%p::OnPaint Lines=%i Sz=%i", this, (int)Line.Length(), (int)Size);
	GProfile Prof(s);
	#endif

		if (d->LayoutDirty)
		{
	#if PROFILE_PAINT
	Prof.Add("PourText");
	#endif
			PourText(d->DirtyStart, d->DirtyLen);
	#if PROFILE_PAINT
	Prof.Add("PourStyle");
	#endif
			PourStyle(d->DirtyStart, d->DirtyLen);
		}

	#if PROFILE_PAINT
	Prof.Add("Setup");
	#endif
		
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
		GColour Back
		(
			/*!ReadOnly &&*/ BkDef.Type == GCss::ColorRgb
			?
			Rgb32To24(BkDef.Rgb32)
			:
			Enabled() ? LC_WORKSPACE : LC_MED,
			24
		);

		// GColour Whitespace = Fore.Mix(Back, 0.85f);
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
			size_t SelMin = MIN(SelStart, SelEnd);
			size_t SelMax = MAX(SelStart, SelEnd);

			// font properties
			Font->Colour(Fore, Back);
			// Font->WhitespaceColour(Whitespace);
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
			size_t NextSelection = (SelStart != SelEnd) ? SelMin : -1; // offset where selection next changes
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

	#if PROFILE_PAINT
	Prof.Add("foreach Line loop");
	#endif
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
				//GRect OldTr = Tr;

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

				// How many chars on this line have we
				// processed so far:
				int Done = 0;
				bool LineHasSelection =	NextSelection >= l->Start &&
										NextSelection < l->Start + l->Len;

				// Fractional pixels we have moved so far:
				int MarginF = d->rPadding.x1 << GDisplayString::FShift;
				int FX = MarginF;
				int FY = Tr.y1 << GDisplayString::FShift;
				
				// loop through all sections of similar text on a line
				while (Done < l->Len)
				{
					// decide how big this block is
					int RtlTrailingSpace = 0;
					ssize_t Cur = l->Start + Done;
					ssize_t Block = l->Len - Done;
					
					// check for style change
					if (NextStyle &&
						NextStyle->End() <= l->Start)
						NextStyle = GetNextStyle();
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
						ssize_t StyleEnd = NextStyle->Start + NextStyle->Len;
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
					
					if (NextStyle &&							// There is a style
						(Cur < SelMin || Cur >= SelMax) &&		// && we're not drawing a selection block
						Cur >= NextStyle->Start &&				// && we're inside the styled area
						Cur < NextStyle->Start+NextStyle->Len)
					{
						GFont *Sf = NextStyle->Font ? NextStyle->Font : Font;
						if (Sf)
						{
							// draw styled text
							if (NextStyle->Fore.IsValid())
								Sf->Fore(NextStyle->Fore);
							if (NextStyle->Back.IsValid())
								Sf->Back(NextStyle->Back);
							else if (l->Back.IsValid())
								Sf->Back(l->Back);
							else
								Sf->Back(Back);								

							Sf->Transparent(false);

							LgiAssert(l->Start + Done >= 0);

							GDisplayString Ds(	Sf,
												MapText(Text + (l->Start + Done),
														Block,
														RtlTrailingSpace != 0),
												Block + RtlTrailingSpace);
							Ds.SetDrawOffsetF(FX - MarginF);
							Ds.ShowVisibleTab(ShowWhiteSpace);
							Ds.FDraw(pOut, FX, FY, 0, LineHasSelection);

							if (NextStyle->Decor == GStyle::DecorSquiggle)
							{
								pOut->Colour(NextStyle->DecorColour.c24(), 24);
								int x = FX >> GDisplayString::FShift;
								int End = x + Ds.X();
								while (x < End)
								{
									pOut->Set(x, Tr.y2-(x%2));
									x++;
								}
							}

							FX += Ds.FX();

							GColour fore = l->c.IsValid() ? l->c : Fore;
							GColour back = l->Back.IsValid() ? l->Back : Back;
							Sf->Colour(fore, back);
						}
						else LgiAssert(0);
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
						Ds.SetDrawOffsetF(FX - MarginF);
						Ds.ShowVisibleTab(ShowWhiteSpace);
						Ds.FDraw(pOut, FX, FY, 0, LineHasSelection);
						FX += Ds.FX();
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

					Done += Block + RtlTrailingSpace;
				
				} // end block loop

				Tr.x1 = FX >> GDisplayString::FShift;

				// eol processing
				ssize_t EndOfLine = l->Start+l->Len;
				if (EndOfLine >= SelMin && EndOfLine < SelMax)
				{
					// draw the '\n' at the end of the line as selected
					// GColour bk = Font->Back();
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

						ssize_t At = Cursor-l->Start;
						
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
								ScrollX = MAX(0, Cur.x1 - 40);
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

				#if DRAW_LINE_BOXES
				{
					uint Style = pDC->LineStyle(GSurface::LineAlternate);
					GColour Old = pDC->Colour(GColour::Red);
					pDC->Box(&OldTr);
					pDC->Colour(Old);
					pDC->LineStyle(Style);

					GString s;
					s.Printf("%i, %i", Line.IndexOf(l), l->Start);
					GDisplayString ds(SysFont, s);
					SysFont->Transparent(true);
					ds.Draw(pDC, OldTr.x2 + 2, OldTr.y1);
				}
				#endif

				#ifdef DOUBLE_BUFFER_PAINT
				// dump to screen
				pDC->Blt(d->rPadding.x1, y, pOut);
				#endif
				y += LineY;

			} // end of line loop

			// draw any space under the lines
			if (y <= r.y2)
			{
				pDC->Colour(Back);
				// pDC->Colour(GColour(255, 0, 255));
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
		case M_TEXT_UPDATE_NAME:
		{
			if (d->Lock(_FL))
			{
				Name(d->SetName);
				d->SetName.Empty();
				d->Unlock();
			}
			break;
		}
		case M_TEXTVIEW_FIND:
		{
			if (InThread())
				DoFindNext();
			else
				LgiTrace("%s:%i - Not in thread.\n", _FL);
			break;
		}
		case M_TEXTVIEW_REPLACE:
		{
			// DoReplace();
			break;
		}
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

int GTextView3::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_VSCROLL && VScroll)
	{
		if (Flags == GNotifyScrollBar_Create)
		{
			UpdateScrollBars();
		}
			
		Invalidate();
	}

	return 0;
}

void GTextView3::OnPulse()
{
	if (!ReadOnly)
	{
		uint64 Now = LgiCurrentTime();
		if (!BlinkTs)
			BlinkTs = Now;
		else if (Now - BlinkTs > CURSOR_BLINK)
		{
			Blink = !Blink;

			GRect p = CursorPos;
			p.Offset(-ScrollX, 0);
			Invalidate(&p);
			BlinkTs = Now;
		}
	}

	if (PartialPour)
		PourText(Size, 0);
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
