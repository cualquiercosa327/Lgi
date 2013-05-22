#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GTextView2.h"
#include "GInput.h"

#ifdef GTEXTVIEW_RES
#include "resdefs.h"
#else
#define GTEXTVIEW_BASE				29000
#define GTEXTVIEW_EMAIL_TO			(GTEXTVIEW_BASE+1)
#define GTEXTVIEW_AUTO_INDENT		(GTEXTVIEW_BASE+2)
#define GTEXTVIEW_CODEPAGE			(GTEXTVIEW_BASE+3)
#define GTEXTVIEW_OPENURL			(GTEXTVIEW_BASE+4)
#define GTEXTVIEW_COPYLINK			(GTEXTVIEW_BASE+5)
#define GTEXTVIEW_CUT				(GTEXTVIEW_BASE+6)
#define GTEXTVIEW_COPY				(GTEXTVIEW_BASE+7)
#define GTEXTVIEW_PASTE				(GTEXTVIEW_BASE+8)
#define GTEXTVIEW_CONV_CODEPAGE		(GTEXTVIEW_BASE+9)
#endif

#ifdef BEOS
#define DOUBLE_BUFFER_PAINT
#endif

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

#define CODEPAGE_BASE				100
#define CONVERT_CODEPAGE_BASE		200

#ifndef WIN32
#define toupper(c)					(((c)>='a'AND(c)<='z') ? (c)-'a'+'A' : (c))
#endif

//////////////////////////////////////////////////////////////////////
char *GTextView2::LgiLoadString(int Id)
{
	// check resource for strings
	char *s = ::LgiLoadString(Id);
	if (s)
	{
		return s;
	}

	// setup some defaults
	switch (Id)
	{
		case GTEXTVIEW_EMAIL_TO:
			return "New Email to...";
		case GTEXTVIEW_AUTO_INDENT:
			return "Auto Indent";
		case GTEXTVIEW_CODEPAGE:
			return "Code page";
		case GTEXTVIEW_OPENURL:
			return "Open URL";
		case GTEXTVIEW_COPYLINK:
			return "Copy link location";
		case GTEXTVIEW_CUT:
			return "Cut";
		case GTEXTVIEW_COPY:
			return "Copy";
		case GTEXTVIEW_PASTE:
			return "Paste";
		case GTEXTVIEW_CONV_CODEPAGE:
			return "Convert to...";
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////
/*
GFile f;
if (f.Open("c:\\temp\\8bit.txt", O_WRITE))
{
	f.SetSize(0);
	char s[256];
	for (int i=0x80; i<0x100; )
	{
		sprintf(s, "%02.2X-%02.2X\t", i, i+0xf);
		uchar *c;
		int n = 0;
		for (c=(uchar*)(s+strlen(s)); n < 16; n++)
		{
			*c++ = i++;
			*c++ = ' ';
		}
		*c++ = '\r';
		*c++ = '\n';
		*c++ = 0;
		f.Write(s, strlen(s));
	}
}
*/

GTextView2::GTextView2(	int Id,
						int x, int y, int cx, int cy,
						GFontType *FontType)
{
	// init vars
	LineY = 1;
	MaxX = 0;
	LastFind = 0;
	LastReplace = 0;
	MatchCase = false;
	MatchWord = false;
	Blink = true;

	// setup window
	SetId(Id);
	GRect r;
	r.ZOff(cx-1, cy-1);
	r.Offset(x, y);
	SetPos(r);

	// default options
	WrapAtCol = 0;
	WrapType = TEXTED_WRAP_REFLOW;
	UrlDetect = true;
	AcceptEdits(true);
	Dirty = false;
	#ifdef WIN32
	CrLf = true;
	#else
	CrLf = false;
	#endif
	BlueUnderline = 0;
	AutoIndent = false;
	BackColour = LC_WORKSPACE;

	#ifdef _DEBUG
	// debug times
	_PourTime = 0;
	_StyleTime = 0;
	_PaintTime = 0;
	#endif

	// Data
	Alloc = ALLOC_BLOCK;
	Text = new char[Alloc];
	if (Text) *Text = 0;
	Cursor = 0;
	CharSize = 1; // or 2
	Size = 0;

	// Display
	Lines = 1;
	LeftMargin = 4;
	SelStart = SelEnd = -1;
	DocOffset = 0;

	if (FontType)
	{
		Font = FontType->CreateFont();
	}
	else
	{
		GFontType Type;
		Type.GetSystemFont("Fixed");
		Font = Type.CreateFont();
	}
	if (Font)
	{
		#if defined WIN32
		WndStyle |= WS_TABSTOP;
		#elif defined BEOS
		Handle()->SetFlags(Handle()->Flags() | B_NAVIGABLE);
		Handle()->SetViewColor(B_TRANSPARENT_COLOR);
		#endif

		BlueUnderline = new GFont;
		if (BlueUnderline)
		{
			*BlueUnderline = *Font;
			BlueUnderline->Underline(true);
			BlueUnderline->Fore(Rgb24(0, 0, 255));
			BlueUnderline->Create();
		}

		OnFontChange();
	}

	CursorPos.ZOff(1, LineY-1);
	CursorPos.Offset(LeftMargin, 0);
}

GTextView2::~GTextView2()
{
	Line.DeleteObjects();
	Style.DeleteObjects();

	DeleteArray(LastFind);
	DeleteArray(LastReplace);
	DeleteArray(Text);

	DeleteObj(Font);
	DeleteObj(BlueUnderline);
}

void GTextView2::SetWrapType(uint8 i)
{
	WrapType = i;
	OnPosChange();
	Invalidate();
}

void GTextView2::AcceptEdits(bool i)
{
	AcceptEdit = i;

	#if defined WIN32
	WndDlgCode = AcceptEdit ? DLGC_WANTALLKEYS : DLGC_WANTARROWS;
	#endif
}

GFont *GTextView2::GetFont()
{
	return Font;
}

void GTextView2::SetFont(GFont *f, bool OwnIt)
{
	if (f)
	{
		if (!Font)
		{
			Font = new GFont(*f);
		}
		else
		{
			*Font = *f;
		}

		if (Font)
		{
			*BlueUnderline = *Font;
			BlueUnderline->Underline(true);
			BlueUnderline->Fore(Rgb24(0, 0, 255));
		}

		OnFontChange();
	}
}

void GTextView2::OnFontChange()
{
	if (Font)
	{
		// get line height
		int OldLineY = LineY;
		Font->Size(0, &LineY, "A", 1);
		if (LineY < 1) LineY = 1;

		// get tab size
		char Spaces[TAB_SIZE+1];
		memset(Spaces, 'A', TAB_SIZE);
		Spaces[TAB_SIZE] = 0;
		Font->TabSize(Font->X(Spaces));

		// repour doc
		PourText();

		// validate blue underline font
		if (BlueUnderline)
		{
			BlueUnderline->X("a");
		}
	}
}

void GTextView2::PourText()
{
	#ifdef _DEBUG
	int StartTime = LgiCurrentTime();
	#endif

	GRect Client = GetClient();
	int Mx = Client.X() - LeftMargin;
	int y = 0;
	int Start = 0;
	
	MaxX = 0;

	// empty list
	Line.DeleteObjects();

	if (Text AND Font)
	{
		// break array, break out of loop when we hit these chars
		bool Break[256];
		ZeroObj(Break);
		Break[0] = true;
		Break['\n'] = true;
		Break[' '] = true;
		Break[9] = true;

		// tracking vars
		int e;
		int Cx = 0, Cy = 0;
		int LastX = 0;
		int LastChar = 0;
		int Fy = Font->Y("A");
		bool WrapIt = false;

		int WidthOfSpace;
		Font->Size(&WidthOfSpace, 0, " ", 1);
		if (WidthOfSpace < 1)
		{
			// no point continuing.. the font can't even get the
			// width of a space..
			return;
		}		
		Font->TabSize(WidthOfSpace * TAB_SIZE);

		// alright... lets pour!
		for (int i=Start; i<=Size; i = e)
		{
			// seek till next char of interest
			int Chars = 0;

			e=i;
			while (true)
			{
				uchar u = (uchar) Text[e];
				if (Break[u])
				{
					break;
				}
				
				e++;
				Chars++;
			}

			int w = 0;
			Font->Size(&w, 0, Text+i, Chars);
			Cx += w;

			if (Text[e] == 9)
			{
				// tab processing
				Cx = ((Cx + Font->TabSize()) / Font->TabSize()) * Font->TabSize();
				e++;
				continue;
			}
			else if (Text[e] == ' ')
			{
				// space processing
				Cx += WidthOfSpace;
				e++;

				if (WrapType == TEXTED_WRAP_REFLOW)
				{
					if (WrapAtCol > 0)
					{
						// wrap at the column specified
						if (i - LastChar < WrapAtCol)
						{
							LastX = Cx;
							continue;
						}

						WrapIt = true;
					}
					else
					{
						// wrap at the end of the page
						if (Cx < Mx)
						{
							LastX = Cx;
							continue;
						}

						WrapIt = true;
					}
				}
				else
				{
					continue;
				}
			}
			else if (Text[e] == '\n' OR Text[e] == 0)
			{
				if (WrapType == TEXTED_WRAP_REFLOW)
				{
					if (WrapAtCol > 0)
					{
						// wrap at the column specified
						WrapIt = (i - LastChar >= WrapAtCol);
					}
					else
					{
						// wrap at the end of the page
						WrapIt = (Cx >= Mx);
					}
				}
			}


			GTextLine *l = new GTextLine;
			if (l)
			{
				l->Start = LastChar;
				l->r.x1 = LeftMargin;

				if (WrapIt)
				{
					if (i > LastChar)
					{
						// wrap at last space
						l->Len = i - LastChar - 1;
						l->r.x2 = (int) (l->r.x1 + LastX - WidthOfSpace);
						LastChar = e = i;
					}
					else
					{
						// no space on this line,
						// wrap at char that is at the edge of page
						
						#ifdef _TRUCATE_TEXT
						// trucate line at end of available space
						l->Len = Font->CharAt(Mx, Text + LastChar, e - LastChar);
						l->r.x2 = l->r.x1 + Mx;
						#else
						// allow line to extend beyond the available space
						l->Len = e - LastChar;
						l->r.x2 = l->r.x1 + Cx;
						#endif

						LastChar = ++e;
					}

					WrapIt = false;
				}
				else
				{
					// end of line
					l->Len = e - LastChar;
					l->r.x2 = l->r.x1 + Cx;
					LastChar = ++e;
				}

				l->r.y1 = Cy;
				l->r.y2 = l->r.y1 + Fy - 1;

				Line.Insert(l);

				MaxX = max(MaxX, l->r.X());
				LastX = Cx = 0;
				Cy += Fy;
			}
		}

		// Update lines var
		Lines = Line.GetItems();
	}

	#ifdef _DEBUG
	_PourTime = LgiCurrentTime() - StartTime;
	#endif

	GRect c = GetClient();
	SetScrollBars(false, c.Y() < Lines * LineY);
}

void GTextView2::InsertStyle(GTextStyle *s)
{
	if (s)
	{
		int Last = 0;
		int n = 0;

		for (GTextStyle *i=Style.First(); i; i=Style.Next(), n++)
		{
			if (s->Start >= Last AND s->Start < i->Start)
			{
				Style.Insert(s, n);
				return;
			}

			Last = i->Start;
		}

		Style.Insert(s);
	}
}

GTextView2::GTextStyle *GTextView2::GetNextStyle(int Where)
{
	GTextStyle *s = (Where >= 0) ? Style.First() : Style.Next();
	while (s)
	{
		// determin whether style is relevent..
		// styles in the selected region are ignored
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);
		if (SelStart >= 0 AND
			s->Start >= Min AND
			s->Start+s->Len < Max)
		{
			// style is completely inside selection: ignore
			s = Style.Next();
		}
		else if (Where >= 0 AND
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

GTextView2::GTextStyle::GTextStyle(GTextView2 *Tv)
{
	View = Tv;
}

char *GTextView2::GTextStyle::LgiLoadString(int Id)
{
	return View->LgiLoadString(Id);
}

class GUrl : public GTextView2::GTextStyle
{
public:
	bool Email;

	GUrl(GTextView2 *Tv) : GTextStyle(Tv)
	{
	}

	bool OnMouseClick(GMouse *m)
	{
		if (View)
		{
			if ( (m AND m->Left() AND m->Double()) OR (!m) )
			{
				char *Url = NewStr(View->Name() + Start, Len);
				if (Url)
				{
					View->OnUrl(Url);
					DeleteObj(Url);
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
				m->AppendItem(LgiLoadString(GTEXTVIEW_EMAIL_TO), IDM_NEW, true);
			}
			else
			{
				m->AppendItem(LgiLoadString(GTEXTVIEW_OPENURL), IDM_OPEN, true);
			}

			m->AppendItem(LgiLoadString(GTEXTVIEW_COPYLINK), IDM_COPY_URL, true);

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
				char *Url = NewStr(View->Name() + Start, Len);
				if (Url)
				{
					GClipBoard Clip(View);
					Clip.Text(Url);
					DeleteObj(Url);
				}
				break;
			}
		}
	}

	char *GetCursor()
	{
		#ifdef WIN32
		int Ver, Rev;
		if (LgiGetOs(&Ver, &Rev) == LGI_OS_WINNT AND
			Ver >= 5)
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

GTextView2::GTextStyle *GTextView2::HitStyle(int i)
{
	for (GTextStyle *s=Style.First(); s; s=Style.Next())
	{
		if (i >= s->Start AND i < s->Start+s->Len)
		{
			return s;
		}
	}

	return 0;	
}

void GTextView2::PourStyle()
{
	#ifdef _DEBUG
	int StartTime = LgiCurrentTime();
	#endif

	// empty list
	for (GTextStyle *s=Style.First(); s; s=Style.First())
	{
		Style.Delete(s);
		DeleteObj(s);
	}

	if (UrlDetect)
	{
		char Map[256];

		ZeroObj(Map);
		Map['h'] = true;
		Map['@'] = true;

		for (int i=0; i<Size; i++)
		{
			if (Map[Text[i]])
			{
				switch (Text[i])
				{
					case 'h':
					{
						if (strnicmp(Text+i, "http://", 6) == 0)
						{
							// find end
							char *s = Text + i;
							char *e = s + 6;
							for ( ; (((int)e-(int)Text) < Size) AND 
									UrlChar(*e); e++);

							GUrl *Url = new GUrl(this);
							if (Url)
							{
								Url->Email = false;
								Url->Start = (int)s - (int)Text;
								Url->Len = (int)e - (int)s;
								Url->Font = BlueUnderline;
								Url->c = Rgb24(0, 0, 255);

								InsertStyle(Url);
							}
							i = (int)e-(int)Text;
						}
						break;
					}
					case '@':
					{
						// find start
						char *s = Text + (max(i, 1) - 1);
						for ( ; (s>=Text) AND 
								EmailChar(*s); s--);
						if (s < Text + i)
						{
							if (s>=Text) s++;
							bool FoundDot = false;
							char *e = Text + i + 1;
							for ( ; (((int)e-(int)Text) < Size) AND 
									UrlChar(*e); e++)
							{
								if (*e == '.') FoundDot = true;
							}

							if (FoundDot)
							{
								GUrl *Url = new GUrl(this);
								if (Url)
								{
									Url->Email = true;
									Url->Start = (int)s - (int)Text;
									Url->Len = (int)e - (int)s;
									Url->Font = BlueUnderline;
									Url->c = Rgb24(0, 0, 255);

									InsertStyle(Url);
								}
								i = (int)e-(int)Text;
							}
						}
						break;
					}
				}
			}
		}
	}

	#ifdef _DEBUG
	_StyleTime = LgiCurrentTime() - StartTime;
	#endif
}

bool GTextView2::Insert(int At, char *Data, int Len)
{
	if (AcceptEdit)
	{
		// limit input to valid data
		At = min(Size, At);

		// make sure we have enough memory
		int NewAlloc = Size + Len + 1;
		NewAlloc += ALLOC_BLOCK - (NewAlloc % ALLOC_BLOCK);
		if (NewAlloc != Alloc)
		{
			char *NewText = new char[NewAlloc];
			if (NewText)
			{
				if (Text)
				{
					// copy any existing data across
					memcpy(NewText, Text, Size);
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
			memmove(Text+(At+Len), Text+At, Size-At);

			if (Data)
			{
				// copy new data in
				memcpy(Text+At, Data, Len);
				Size += Len;
			}
			else
			{
				return false;
			}

			Text[Size] = 0;
			Dirty = true;
			PourText();
			PourStyle();

			GView *n = GetNotify()?GetNotify():GetParent();
			if (n)
			{
				n->OnNotify(this, GTVN_DOC_CHANGED);
			}

			return true;
		}
	}

	return false;
}

bool GTextView2::Delete(int At, int Len)
{
	bool Status = false;

	if (AcceptEdit)
	{
		// limit input
		At = max(At, 0);
		At = min(At, Size);
		Len = min(Size-At, Len);

		if (Len > 0)
		{
			// do delete
			memmove(Text+At, Text+(At+Len), Size-At-Len);
			Size -= Len;
			Text[Size] = 0;

			Dirty = true;
			Status = true;
			PourText();
			PourStyle();

			GView *n = GetNotify()?GetNotify():GetParent();
			if (n)
			{
				n->OnNotify(this, GTVN_DOC_CHANGED);
			}

			Status = true;
		}
	}

	return Status;
}

void GTextView2::DeleteSelection(char **Cut)
{
	if (SelStart >= 0)
	{
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);

		if (Cut)
		{
			*Cut = NewStr(Text + Min, Max - Min);
		}

		Delete(Min, Max - Min);
		SetCursor(Min, false, true);
	}
}

GTextView2::GTextLine *GTextView2::GetLine(int Offset, int *Index)
{
	int i = 0;
	for (GTextLine *l=Line.First(); l; l=Line.Next(), i++)
	{
		if (Offset >= l->Start AND Offset <= l->Start+l->Len)
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

char *GTextView2::Name()
{
	return Text;
}

bool GTextView2::Name(char *s)
{
	bool Status = false;

	DeleteArray(Text);
	Size = s ? strlen(s) : 0;
	Alloc = Size + 1;
	Text = new char[Alloc];
	Cursor = 0;
	if (Text)
	{
		memcpy(Text, s, Size);

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

		// update everything else
		PourText();
		PourStyle();
		UpdateScrollBars(true);
		Status = true;
	}

	Invalidate();
	return Status;
}

bool GTextView2::HasSelection()
{
	return (SelStart >= 0) AND (SelStart != SelEnd);
}

void GTextView2::SelectAll()
{
	SelStart = 0;
	SelEnd = Size;
	Invalidate();
}

void GTextView2::UnSelectAll()
{
	bool Update = HasSelection();

	SelStart = -1;
	SelEnd = -1;

	if (Update)
	{
		Invalidate();
	}
}

int GTextView2::GetLines()
{
	return Lines;
}

void GTextView2::GetTextExtent(int &x, int &y)
{
	x = MaxX + LeftMargin;
	y = Lines * LineY;
}

GdcPt2 GTextView2::GetCursor()
{
	GdcPt2 p(0, 0);
	int FromIndex = 0;
	GTextLine *From = GetLine(Cursor, &FromIndex);
	if (From)
	{
		p.x = Cursor - From->Start;
		p.y = FromIndex;
	}

	return p;
}

int GTextView2::IndexAt(int x, int y)
{
	GTextLine *l = Line.ItemAt(y);
	if (l)
	{
		return l->Start + min(x, l->Len);
	}

	return 0;
}

void GTextView2::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
	if (i >= 0 AND i<=Size)
	{
		int s = SelStart, e = SelEnd, c = Cursor;
		
		if (Select AND i != SelStart)
		{
			if (SelStart < 0)
			{
				SelStart = Cursor;
			}

			SelEnd = i;
		}
		else
		{
			SelStart = SelEnd = -1;
		}

		int FromIndex = 0;
		GTextLine *From = GetLine(Cursor, &FromIndex);

		Cursor = i;

		// check the cursor is on the screen
		int ToIndex = 0;
		GTextLine *To = GetLine(Cursor, &ToIndex);
		if (VScroll AND To)
		{
			if (ToIndex < VScroll->Value())
			{
				// above the visible region...
				VScroll->Value(ToIndex);
				ForceFullUpdate = true;
			}

			GRect Client = GetClient();
			int DisplayLines = Client.Y() / LineY;
			if (ToIndex >= VScroll->Value() + DisplayLines)
			{
				VScroll->Value(ToIndex - DisplayLines + 1);
				ForceFullUpdate = true;
			}
		}

		// check whether we need to update the screen
		if (ForceFullUpdate OR
			SelStart != s OR
			SelEnd != e OR
			!To OR
			!From)
		{
			// need full update
			Invalidate();
		}
		else if (Cursor != c)
		{
			// just the cursor has moved

			// update the line the cursor moved to
			GRect r = To->r;
			r.Offset(0, -DocOffset);
			r.x2 = X();
			Invalidate(&r);

			if (To != From)
			{
				// update the line the cursor came from,
				// if it's a different line from the "to"
				r = From->r;
				r.Offset(0, -DocOffset);
				r.x2 = X();
				Invalidate(&r);
			}
		}

		if (c != Cursor)
		{
			GView *n = GetNotify()?GetNotify():GetParent();
			if (n)
			{
				n->OnNotify(this, GTVN_CURSOR_CHANGED);
			}
		}
	}
}

void GTextView2::SetBorder(int b)
{
}

char *ConvertToCrLf(char *Text)
{
	if (Text)
	{
		// add '\r's
		int Lfs = 0;
		int Len = 0;
		char *s=Text;
		for (; *s; s++)
		{
			if (*s == '\n') Lfs++;
			Len++;
		}

		char *Temp = new char[Len+Lfs+1];
		if (Temp)
		{
			char *d=Temp;
			s = Text;
			for (; *s; s++)
			{
				if (*s == '\n')
				{
					*d++ = 0x0d;
					*d++ = 0x0a;
				}
				else if (*s == '\r')
				{
					// ignore
				}
				else
				{
					*d++ = *s;
				}
			}
			*d++ = 0;

			DeleteObj(Text);
			return Temp;
		}
	}

	return Text;
}

bool GTextView2::Cut()
{
	bool Status = false;
	char *t = 0;
	
	DeleteSelection(&t);
	GClipBoard Clip(this);
	if (t)
	{
		t = ConvertToCrLf(t);
		Status = Clip.Text(t);
		DeleteArray(t);
	}

	return Status;
}

bool GTextView2::Copy()
{
	bool Status = true;

	if (SelStart >= 0)
	{
		int Min = min(SelStart, SelEnd);
		int Max = max(SelStart, SelEnd);

		char *t = NewStr(Text+Min, Max-Min);
		char16 *Unicode = 0;
		t = ConvertToCrLf(t);
		Unicode = (char16*)LgiNewConvertCp("ucs-2", t, "utf-8");

		GClipBoard Clip(this);
		
		Status = Clip.Text(t);
		if (Unicode) Clip.TextW(Unicode, false);

		DeleteArray(t);
		DeleteArray(Unicode);
	}

	return Status;
}

bool GTextView2::Paste()
{
	GClipBoard Clip(this);

	char *t = 0;
	char16 *Unicode = Clip.TextW();
	if (Unicode)
	{
		int Len = StrlenW(Unicode);
		t = (char*)LgiNewConvertCp("utf-8", Unicode, "ucs-2");
	}

	if (!t)
	{
		t = Clip.Text();
	}

	if (t)
	{
		if (SelStart >= 0)
		{
			DeleteSelection();
		}

		// remove '\r's
		bool Multiline = false;
		char *s = t, *d = t;
		for (; *s; s++)
		{
			if (*s == '\n')
			{
				Multiline = true;
			}

			if (*s != '\r')
			{
				*d++ = *s;
			}
		}
		*d++ = 0;

		// insert text
		int Len = strlen(t);
		Insert(Cursor, t, Len);
		SetCursor(Cursor+Len, false, true); // Multiline

		// clean up
		DeleteArray(t);
	}
	return false;
}

bool GTextView2::ClearDirty(bool Ask, char *FileName)
{
	if (Dirty)
	{
		int Answer = (Ask) ? LgiMsg(this, "Do you want to save your changes to this document?", "Save", MB_YESNOCANCEL) : IDYES;
		if (Answer == IDYES)
		{
			GFileSelect Select;
			Select.Parent(this);
			if (!FileName AND
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

bool GTextView2::Open(char *Name)
{
	bool Status = false;
	GFile f;

	if (f.Open(Name, O_READ))
	{
		DeleteArray(Text);
		Size = f.GetSize();
		Alloc = Size + 1;
		Text = new char[Alloc];
		SetCursor(0, false);
		if (Text)
		{
			if (f.Read(Text, Size) == Size)
			{
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
				Dirty = false;

				PourText();
				PourStyle();
				UpdateScrollBars(true);

				Status = true;
			}
		}

		Invalidate();
	}

	return Status;
}

bool GTextView2::Save(char *Name)
{
	GFile f;
	if (f.Open(Name, O_WRITE))
	{
		f.SetSize(0);
		if (Text)
		{
			bool Status = false;
			
			if (CrLf)
			{
				int Start = 0;
				Status = true;
				for (int i=0; i<=Size AND Status; i++)
				{
					if (Text[i] == '\n' OR i >= Size)
					{
						Status = f.Write(Text + Start, i - Start) == (i - Start);
						if (i < Size)
						{
							Status = f.Write((char*)"\r\n", 2) == 2;
						}
						Start = i+1;
					}
				}
			}
			else
			{
				Status = f.Write(Text, Size) == Size;
			}

			Dirty = false;
			return Status;
		}
	}
	return false;
}

void GTextView2::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		GRect Before = GetClient();

		int DisplayLines = Y() / LineY;
		VScroll->SetLimits(0, Lines);
		VScroll->SetPage(DisplayLines);

		if (Reset)
		{
			VScroll->Value(0);
			SelStart = SelEnd = -1;
		}

		GRect After = GetClient();

		if (Before != After)
		{
			PourText();
			Invalidate();
		}
	}
}

bool GTextView2::DoFind()
{
	GFindDlg Dlg(this, LastFind);

	Dlg.MatchWord = MatchWord;
	Dlg.MatchCase = MatchCase;

	if (Dlg.DoModal() == IDOK)
	{
		DeleteArray(LastFind);
		LastFind = NewStr(Dlg.Find);
		MatchWord = Dlg.MatchWord;
		MatchCase = Dlg.MatchCase;

		OnFind(LastFind, MatchWord, MatchCase);
	}

	return false;
}

bool GTextView2::DoReplace()
{
	GReplaceDlg Dlg(this, LastFind, LastReplace);

	Dlg.MatchWord = MatchWord;
	Dlg.MatchCase = MatchCase;

	int Action = Dlg.DoModal();

	if (Action != IDCANCEL)
	{
		DeleteArray(LastFind);
		LastFind = NewStr(Dlg.Find);

		DeleteArray(LastReplace);
		LastReplace = NewStr(Dlg.Replace);

		MatchWord = Dlg.MatchWord;
		MatchCase = Dlg.MatchCase;
	}

	switch (Action)
	{
		case IDC_FR_FIND:
		{
			OnFind(LastFind, MatchWord, MatchCase);
			break;
		}
		case IDC_FR_REPLACE:
		{
			OnReplace(LastFind, LastReplace, false, MatchWord, MatchCase);
			break;
		}
		case IDOK:
		{
			OnReplace(LastFind, LastReplace, true, MatchWord, MatchCase);
			break;
		}
	}

	return false;
}

void GTextView2::SelectWord(int From)
{
	char Delim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

	for (SelStart = From; SelStart >= 0; SelStart--)
	{
		if (strchr(Delim, Text[SelStart]))
		{
			SelStart++;
			break;
		}
	}

	for (SelEnd = From; SelEnd < Size; SelEnd++)
	{
		if (strchr(Delim, Text[SelEnd]))
		{
			break;
		}
	}

	Invalidate();
}

bool GTextView2::OnFind(char *Find, bool MatchWord, bool MatchCase)
{
	if (ValidStr(Find))
	{
		int Len = strlen(Find);
		bool Found = false;
		int i=Cursor;

		if (MatchCase)
		{
			for (; i<Size-Len; i++)
			{
				if (Text[i] == Find[0])
				{
					Found = strncmp(Text+i, Find, Len) == 0;
					if (Found)
					{
						break;
					}
				}
			}
		}
		else
		{
			for (; i<Size-Len; i++)
			{
				if (toupper(Text[i]) == toupper(Find[0]))
				{
					Found = strnicmp(Text+i, Find, Len) == 0;
					if (Found)
					{
						break;
					}
				}
			}
		}

		if (Found)
		{
			SetCursor(i, false);
			SetCursor(i + Len, true);
			return true;
		}
	}


	return false;
}

bool GTextView2::OnReplace(char *Find, char *Replace, bool All, bool MatchWord, bool MatchCase)
{
	return false;
}

int GTextView2::SeekLine(int i, GTextViewSeek Where)
{
	switch (Where)
	{
		case PrevLine:
		{
			for (; i > 0 AND Text[i] != '\n'; i--);
			if (i > 0) i--;
			for (; i > 0 AND Text[i] != '\n'; i--);
			if (i > 0) i++;
			break;
		}
		case NextLine:
		{
			for (; i < Size AND Text[i] != '\n'; i++);
			i++;
			break;
		}
		case StartLine:
		{
			for (; i > 0 AND Text[i] != '\n'; i--);
			if (i > 0) i++;
			break;
		}
 		case EndLine:
		{
			for (; i < Size AND Text[i] != '\n'; i++);
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

bool GTextView2::OnMultiLineTab(bool In)
{
	bool Status = false;
	int Min = min(SelStart, SelEnd);
	int Max = max(SelStart, SelEnd);

	Min = SeekLine(Min, StartLine);

	GBytePipe p;
	int Ls = 0, i;
	for (i=Min; i<Max AND i<Size; i=SeekLine(i, NextLine))
	{
		p.Push((uchar*)&i, sizeof(i));
		Ls++;
	}
	if (Max < i)
	{
		Max = SeekLine(Max, EndLine);
	}

	int *Indexes = new int[Ls];
	if (Indexes)
	{
		p.Pop((uchar*)Indexes, Ls*sizeof(int));

		SelStart = Min;

		for (i=Ls-1; i>=0; i--)
		{
			if (In)
			{
				// <-
				int n = Indexes[i], Space = 0;
				for (; Space<TAB_SIZE AND n<Size; n++)
				{
					if (Text[n] == 9)
					{
						Space += TAB_SIZE;
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
				for (; Text[Len] != '\n' AND Len<Size; Len++);
				if (Len > Indexes[i])
				{
					Insert(Indexes[i], "\t", 1);
					Max++;
				}
			}
		}

		SelEnd = Cursor = Max;

		DeleteArray(Indexes);
	}

	PourText();
	Invalidate();
	Status = true;

	return Status;
}

void GTextView2::OnSetHidden(int Hidden)
{
}

void GTextView2::OnPosChange()
{
	GLayout::OnPosChange();
	
	PourText();
	UpdateScrollBars(false);
}

void GTextView2::OnCreate()
{
	SetPulse(500);
}

void GTextView2::OnEscape(GKey &K)
{
}

void GTextView2::OnMouseWheel(double l)
{
	if (VScroll)
	{
		int NewPos = VScroll->Value() + (int) l;
		NewPos = limit(NewPos, 0, Lines);
		VScroll->Value(NewPos);
		Invalidate();
	}
}

void GTextView2::OnFocus(bool f)
{
	Invalidate();
}

int GTextView2::HitText(int x, int y)
{
	bool Down = y >= 0;
	int Y = (VScroll) ? VScroll->Value() : 0;
	GTextLine *l = Line.ItemAt(Y);
	y += (l) ? l->r.y1 : 0;

	while (l)
	{
		if (l->r.Overlap(x, y))
		{
			// Click in a line
			return l->Start + Font->CharAt(x-l->r.x1, Text+l->Start, l->Len);
		}
		else if (y >= l->r.y1 AND y <= l->r.y2)
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

	return 0;
}

void GTextView2::OnMouseClick(GMouse &m)
{
	bool Processed = false;

	if (m.Down())
	{
		if (m.Left())
		{

			Focus(true);

			int Hit = HitText(m.x, m.y);
			if (Hit >= 0)
			{
				SetCursor(Hit, m.Shift());

				GTextStyle *s = HitStyle(Hit);
				if (s)
				{
					Processed = s->OnMouseClick(&m);
				}
			}

			if (!Processed AND m.Double())
			{
				SelectWord(Cursor);
			}
		}
		else if (m.Right())
		{
			GSubMenu *RClick = new GSubMenu;
			if (RClick)
			{
				char *ClipText = 0;
				{
					GClipBoard Clip(this);
					ClipText = Clip.Text();
				}

				GTextStyle *s = HitStyle(HitText(m.x, m.y));
				if (s)
				{
					if (s->OnMenu(RClick))
					{
						RClick->AppendSeparator();
					}
				}

				RClick->AppendItem(LgiLoadString(GTEXTVIEW_CUT), IDM_CUT, HasSelection());
				RClick->AppendItem(LgiLoadString(GTEXTVIEW_COPY), IDM_COPY, HasSelection());
				RClick->AppendItem(LgiLoadString(GTEXTVIEW_PASTE), IDM_PASTE, ClipText != 0);
				RClick->AppendSeparator();

				GMenuItem *Indent = RClick->AppendItem(LgiLoadString(GTEXTVIEW_AUTO_INDENT), IDM_AUTO_INDENT, true);
				if (Indent)
				{
					Indent->Checked(AutoIndent);
				}				

				if (GetMouse(m, true))
				{
					int Id = 0;
					switch (Id = RClick->Float(this, m.x, m.y))
					{
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
						case IDM_AUTO_INDENT:
						{
							AutoIndent = !AutoIndent;
							break;
						}
						default:
						{
							if (s)
							{
								s->OnMenuClick(Id);
							}
							break;
						}
					}
				}

				DeleteObj(RClick);
			}

			return;
		}
	}

	if (!Processed)
	{
		Capture(m.Down());
	}
}

int GTextView2::OnHitTest(int x, int y)
{
	#ifdef WIN32
	if (GetClient().Overlap(x, y))
	{
		return HTCLIENT;
	}
	#endif
	return GView::OnHitTest(x, y);
}

void GTextView2::OnMouseMove(GMouse &m)
{
	int Hit = HitText(m.x, m.y);

	if (IsCapturing())
	{
		SetCursor(Hit, m.Left());
	}

	#ifdef WIN32
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		GTextStyle *s = HitStyle(Hit);
		char *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
	}
	#endif
}

void GTextView2::OnKey(GKey &k)
{
	printf("GTextView2::OnKey down=%i ischar=%i c='%c'(%i)\n", k.Down(), k.IsChar, k.c, k.c);

	if (k.Down())
	{
		Blink = true;

		if (k.IsChar)
		{
			switch (k.c)
			{
				default:
				{
					// process single char input
					if (k.c >= ' ' OR
						k.c == 9)
					{
						// letter/number etc
						if (SelStart >= 0)
						{
							if (k.c == 9)
							{
								if (OnMultiLineTab(k.Shift()))
								{
									break;
								}
							}
							else
							{
								DeleteSelection();
							}
						}
						
						GTextLine *l = GetLine(Cursor);
						int Len = (l) ? l->Len : 0;
						if (Insert(Cursor, (char*) &k.c, 1))
						{
							l = GetLine(Cursor);
							int NewLen = (l) ? l->Len : 0;
							SetCursor(Cursor + 1, false, Len != NewLen - 1);
						}
					}
					break;
				}
				case VK_RETURN:
				{
					OnEnter(k);
					break;
				}
				case VK_BACK:
				{
					if (SelStart >= 0)
					{
						// delete selection
						DeleteSelection();
					}
					else
					{
						char Del = Text[Cursor-1];

						if (Cursor > 0 AND
							Delete(Cursor - 1, 1))
						{
							// normal backspace
							SetCursor(Cursor - 1, false, Del == '\n');
						}
					}
					break;
				}
			}
		}
		else // not a char
		{
			switch (k.c)
			{
				case VK_F3:
				{
					if (LastFind)
					{
						OnFind(LastFind, MatchWord, MatchCase);
					}
					break;
				}
				case VK_LEFT:
				{
					if (SelStart >= 0 AND
						!k.Shift())
					{
						SetCursor(min(SelStart, SelEnd), false);
					}
					else if (Cursor > 0)
					{
						int n = Cursor;

						if (k.Ctrl())
						{
							// word move/select
							bool StartWhiteSpace = IsWhiteSpace(Text[n]);
							bool LeftWhiteSpace = n > 0 AND IsWhiteSpace(Text[n-1]);

							if (!StartWhiteSpace OR
								Text[n] == '\n') n--;
							for (; n > 0 AND strchr(" \t", Text[n]); n--);
							if (Text[n] == '\n')
							{
								n--;
							}
							else if (!StartWhiteSpace OR !LeftWhiteSpace)
							{
								if (IsDelimiter(Text[n]))
								{
									for (; n > 0 AND IsDelimiter(Text[n]); n--);
								}
								else
								{
									for (; n > 0; n--)
									{
										//IsWordBoundry(Text[n])
										if (IsWhiteSpace(Text[n]) OR
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
					break;
				}
				case VK_RIGHT:
				{
					if (SelStart >= 0 AND
						!k.Shift())
					{
						SetCursor(max(SelStart, SelEnd), false);
					}
					else if (Cursor < Size)
					{
						int n = Cursor;

						if (k.Ctrl())
						{
							// word move/select
							if (IsWhiteSpace(Text[n]))
							{
								for (; n<Size AND IsWhiteSpace(Text[n]); n++);
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
										if (IsWhiteSpace(Text[n]) OR
											IsDelimiter(Text[n]))
										{
											break;
										}
										// IsAlpha(Text[n]) OR IsDigit(Text[n]));
									}
								}

								if (n < Size AND
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
					break;
				}
				case VK_UP:
				{
					int Old = Cursor;
					GTextLine *l = GetLine(Cursor);
					if (l)
					{
						GTextLine *Prev = Line.Prev();
						if (Prev)
						{
							int ScreenX = Font->X(Text + l->Start, Cursor-l->Start);
							int CharX = Font->CharAt(ScreenX, Text + Prev->Start, Prev->Len);

							SetCursor(Prev->Start + min(CharX, Prev->Len), k.Shift());
						}
					}
					break;
				}
				case VK_DOWN:
				{
					int Old = Cursor;
					GTextLine *l = GetLine(Cursor);
					if (l)
					{
						GTextLine *Next = Line.Next();
						if (Next)
						{
							int ScreenX = Font->X(Text + l->Start, Cursor-l->Start);
							int CharX = Font->CharAt(ScreenX, Text + Next->Start, Next->Len);

							SetCursor(Next->Start + min(CharX, Next->Len), k.Shift());
						}
					}
					break;
				}
				case VK_END:
				{
					if (k.Ctrl())
					{
						SetCursor(Size, k.Shift());
					}
					else
					{
						GTextLine *l = GetLine(Cursor);
						if (l)
						{
							SetCursor(l->Start + l->Len, k.Shift());
						}
					}
					break;
				}
				case VK_HOME:
				{
					if (k.Ctrl())
					{
						SetCursor(0, k.Shift());
					}
					else
					{
						GTextLine *l = GetLine(Cursor);
						if (l)
						{
							char *Line = Text + l->Start;
							char *s;
							for (s = Line; ((int)s-(int)Line) < l->Len AND strchr(" \t", *s); s++);
							int Whitespace = (int)s-(int)Line;

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
					break;
				}
				case VK_PRIOR:
				{
					GTextLine *l = GetLine(Cursor);
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
					break;
				}
				case VK_NEXT:
				{
					GTextLine *l = GetLine(Cursor);
					if (l)
					{
						int DisplayLines = Y() / LineY;
						int CurLine = Line.IndexOf(l);

						GTextLine *New = Line.ItemAt(min(CurLine + DisplayLines, Lines-1));
						if (New)
						{
							SetCursor(New->Start + min(Cursor - l->Start, New->Len), k.Shift());
						}
					}				
					break;
				}
				case VK_INSERT:
				{
					if (k.Ctrl())
					{
						Copy();
					}
					else if (k.Shift())
					{
						Paste();
					}
					break;
				}
				case VK_DELETE:
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
					else if (Cursor < Size AND
							Delete(Cursor, 1))
					{
						Invalidate();
					}
					break;
				}
				default:
				{
					if (k.Ctrl() AND !k.Alt())
					{
						switch (k.c)
						{
							case 'a':
							case 'A':
							{
								// select all
								SelStart = 0;
								SelEnd = Size;
								Invalidate();
								break;
							}
							case 'x':
							case 'X':
							{
								Cut();
								break;
							}
							case 'c':
							case 'C':
							{
								Copy();
								break;
							}
							case 'v':
							case 'V':
							{
								Paste();
								break;
							}
							case 'f':
							case 'F':
							{
								DoFind();
								break;
							}
							case 'g':
							case 'G':
							{
								GInput Dlg(this, "", "Goto line:", "Text");
								if (Dlg.DoModal() == IDOK AND
									Dlg.Str)
								{
									GTextLine *l = Line.ItemAt(atoi(Dlg.Str) - 1);
									if (l)
									{
										SetCursor(l->Start, false);
									}
								}
								break;
							}
							case 'h':
							case 'H':
							{
								DoReplace();
								break;
							}
							case VK_RETURN:
							{
								OnEnter(k);
								break;
							}
						}
					}
					break;
				}
			}
		}
	}
}

void GTextView2::OnEnter(GKey &k)
{
	// enter
	if (SelStart >= 0)
	{
		DeleteSelection();
	}

	char InsertStr[256] = "\n";

	GTextLine *CurLine = GetLine(Cursor);
	if (CurLine AND AutoIndent)
	{
		int WsLen = 0;
		for (;	WsLen < CurLine->Len AND
				WsLen < (Cursor - CurLine->Start) AND
				strchr(" \t", Text[CurLine->Start + WsLen]); WsLen++);
		if (WsLen > 0)
		{
			memcpy(InsertStr+1, Text+CurLine->Start, WsLen);
			InsertStr[WsLen+1] = 0;
		}
	}

	if (Insert(Cursor, InsertStr, strlen(InsertStr)))
	{
		SetCursor(Cursor + strlen(InsertStr), false, true);
	}
}

int GTextView2::TextWidth(GFont *f, char *s, int Len, int x, int Origin)
{
	int w = x;
	int Size = f->TabSize();

	for (char *c = s; ((int)c-(int)s) < Len; )
	{
		if (*c == 9)
		{			
			w = ((w + Size) / Size) * Size;
			c++;
		}
		else
		{
			char *e;
			for (e = c; ((int)e-(int)s) < Len AND *e != 9; e = LgiSeekUtf8(e, 1));

			int dx;
			f->Size(&dx, 0, c, ((int)e-(int)c), Origin);
			w += dx;
			c = e;
		}
	}

	return w - x;
}

void GTextView2::OnPaint(GSurface *pDC)
{
	#ifdef _DEBUG
	int StartTime = LgiCurrentTime();
	#endif

	GRect r(0, 0, X()-1, Y()-1);
	GSurface *pOut = pDC;
	bool DrawSel = false;
	COLOUR Fore = LC_TEXT;
	COLOUR Back = (AcceptEdit) ? LC_WORKSPACE  : BackColour;
	COLOUR Selected = LC_SELECTION;
	if (!Focus())
	{
		COLOUR Work = LC_WORKSPACE;
		Selected = Rgb24(	(R24(Work)+R24(Selected))/2,
							(G24(Work)+G24(Selected))/2,
							(B24(Work)+B24(Selected))/2);
	}

	if (!Enabled())
	{
		Fore = LC_LOW;
		Back = LC_MED;
	}

	#ifdef DOUBLE_BUFFER_PAINT
	GMemDC *pMem = new GMemDC;
	pOut = pMem;
	#endif
	if (Text AND
		Font
		#ifdef DOUBLE_BUFFER_PAINT
		AND
		pMem AND
		pMem->Create(r.X()-LeftMargin, LineY, GdcD->GetBits())
		#endif
		)
	{
		int y = 0;
		int x = LeftMargin;
		int SelMin = min(SelStart, SelEnd);
		int SelMax = max(SelStart, SelEnd);

		// font properties
		Font->Colour(Fore, Back);
		Font->Transparent(false);

		// draw left margin
		pDC->Colour(Back, 24);
		pDC->Rectangle(0, 0, x-1, r.y2);
	
		// draw lines of text
		int k = (VScroll) ? VScroll->Value() : 0;
		GTextLine *l=Line.ItemAt(k);
		int Dy = (l) ? -l->r.y1 : 0;
		int NextSelection = (SelStart != SelEnd) ? SelMin : -1; // offset where selection next changes
		if (l AND
			SelStart >= 0 AND
			SelStart < l->Start AND
			SelEnd > l->Start)
		{
			// start of visible area is in selection
			// init to selection colour
			DrawSel = true;
			Font->Colour(LC_SEL_TEXT, Selected);
			NextSelection = SelMax;
		}

		GTextStyle *NextStyle = GetNextStyle((l) ? l->Start : 0);

		DocOffset = (l) ? l->r.y1 : 0;

		// loop through all visible lines
		for (; l AND l->r.y1+Dy < r.Y(); l=Line.Next())
		{
			GRect Tr = l->r;
			#ifdef DOUBLE_BUFFER_PAINT
			Tr.Offset(-Tr.x1, -Tr.y1);
			#else
			Tr.Offset(0, Dy);
			#endif
			int TabOri = Tr.x1;

			// deal with selection change on beginning of line
			if (NextSelection == l->Start)
			{
				// selection change
				DrawSel = !DrawSel;
				if (DrawSel)
				{
					Font->Colour(LC_SEL_TEXT, Selected);
				}
				else
				{
					Font->Colour(Fore, Back);
				}
				NextSelection = (NextSelection == SelMin) ? SelMax : -1;
			}

			// draw text
			int Done = 0;	// how many chars on this line have we 
							// processed so far
			int TextX = 0;	// pixels we have moved so far
			
			// loop through all sections of similar text on a line
			while (Done < l->Len)
			{
				// decide how big this block is
				int Cur = l->Start + Done;
				int Block = l->Len - Done;
				
				// check for style change
				if (NextStyle)
				{
					// start
					if (l->Overlap(NextStyle->Start) AND
						NextStyle->Start > Cur AND
						NextStyle->Start - Cur < Block)
					{
						Block = NextStyle->Start - Cur;
					}

					// end
					int StyleEnd = NextStyle->Start + NextStyle->Len;
					if (l->Overlap(StyleEnd) AND
						StyleEnd > Cur AND
						StyleEnd - Cur < Block)
					{
						Block = StyleEnd - Cur;
					}
				}

				// check for next selection change
				// this may truncate the style
				if (l->Overlap(NextSelection) AND
					((NextSelection - Cur < Block) OR (Cur >= SelMin)))
				{
					Block = NextSelection - Cur;
				}

				LgiAssert(Block != 0);	// sanity check

				// Do code page mapping
				char *_temp = Text + (l->Start + Done); // MapChars(Text+(l->Start+Done), Block);
				LgiAssert(_temp); // code page mapping failed

				if (NextStyle AND							// There is a style
					Cur != SelMin AND						// AND we're not drawing a selection block
					Cur >= NextStyle->Start AND				// AND we're inside the styled area
					Cur < NextStyle->Start+NextStyle->Len)
				{
					if (NextStyle->Font)
					{
						// draw styled text
						NextStyle->Font->Colour(NextStyle->c, Back);
						NextStyle->Font->Transparent(false);

						TextX = TextWidth(NextStyle->Font, Text+(l->Start+Done), Block, Tr.x1 - LeftMargin, TabOri);

						NextStyle->Font->Text(	pOut,
												Tr.x1,
												Tr.y1,
												_temp,
												Block,
												&Tr,
												TabOri);
					}
				}
				else
				{
					// draw a block of normal text
					TextX = TextWidth(Font, Text+(l->Start+Done), Block, Tr.x1 - LeftMargin, TabOri);
					
					Font->Text(	pOut,
								Tr.x1,
								Tr.y1,
								_temp,
								Block,
								0,
								TabOri); // &Tr);
				}

				if (NextStyle AND
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
						Font->Colour(LC_SEL_TEXT, Selected);
					}
					else
					{
						Font->Colour(Fore, Back);
					}
					NextSelection = (NextSelection == SelMin) ? SelMax : -1;
				}

				Tr.x1 += TextX;
				Done += Block;
			
			} // end block loop

			// eol processing
			int EndOfLine = l->Start+l->Len;
			if (EndOfLine >= SelMin AND EndOfLine < SelMax)
			{
				// draw the '\n' at the end of the line as selected
				pOut->Colour(Font->Back(), 24);
				pOut->Rectangle(Tr.x2, Tr.y1, Tr.x2+7, Tr.y2);
				Tr.x2 += 7;
			}
			// draw any space after text
			pOut->Colour(Back, 24);
			pOut->Rectangle(Tr.x2, Tr.y1, r.x2, Tr.y2);

			// cursor?
			if (Focus())
			{
				// draw the cursor if on this line
				if (Cursor >= l->Start AND Cursor <= l->Start+l->Len)
				{
					CursorPos.ZOff(1, LineY-1);
					CursorPos.Offset(LeftMargin + TextWidth(Font, Text+l->Start, Cursor-l->Start, 0, TabOri), y);
					
					if (Blink)
					{
						GRect c = CursorPos;
						#ifdef DOUBLE_BUFFER_PAINT
						c.Offset(-LeftMargin, -y);
						#endif

						pOut->Colour((AcceptEdit) ? Fore : Rgb24(192, 192, 192), 24);
						pOut->Rectangle(&c);
					}
				}
			}

			#ifdef DOUBLE_BUFFER_PAINT
			// dump to screen
			pDC->Blt(LeftMargin, y, pOut);
			#endif
			y += LineY;

		} // end of line loop

		// draw any space under the lines
		if (y < r.y2)
		{
			pDC->Colour(Back, 24);
			pDC->Rectangle(x, y, r.x2, r.y2);
		}

		#ifdef DOUBLE_BUFFER_PAINT
		DeleteObj(pMem);
		#endif
	}
	else
	{
		// default drawing: nothing
		pDC->Colour(Back, 24);
		pDC->Rectangle(&r);
	}

	UpdateScrollBars();

	#ifdef _DEBUG
	_PaintTime = LgiCurrentTime() - StartTime;
	if (GetNotify())
	{
		char s[256];
		sprintf(s, "Pour:%i Style:%i Paint:%i ms", _PourTime, _StyleTime, _PaintTime);
		GMessage m = CreateMsg(DEBUG_TIMES_MSG, 0, (int)s);
		GetNotify()->OnEvent(&m);
	}
	#endif
}

int GTextView2::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		#ifdef BEOS
		case B_CUT:
		{
			Cut();
			break;
		}
		case B_COPY:
		{
			Copy();
			break;
		}
		case B_PASTE:
		{
			Paste();
			break;
		}
		#endif
	}

	return GLayout::OnEvent(Msg);
}

int GTextView2::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_VSCROLL AND VScroll)
	{
		Invalidate();
	}

	return 0;
}

void GTextView2::OnPulse()
{
	if (AcceptEdit)
	{
		Blink = !Blink;
		Invalidate(&CursorPos);
	}
}

void GTextView2::OnUrl(char *Url)
{
	if (Url)
	{
		if (strnicmp(Url, "mailto:", 7) == 0 OR
			(strchr(Url, '@') AND !strchr(Url, '/')))
		{
			// email
			char EmailApp[256];
			if (LgiGetAppForMimeType("application/email", EmailApp))
			{
				char *Arg = strstr(EmailApp, "%1");
				if (Arg)
				{
					// change '%1' into the email address in question
					char Post[256];
					strcpy(Post, Arg + 2);
					strcpy(Arg, Url);
					strcat(Arg, Post);
				}
				else
				{
					// no argument place holder... just pass as a normal arg
					strcat(EmailApp, " ");
					strcat(EmailApp, Url);
				}

				// find end of executable name
				char Delim = 0;
				char *File = 0;
				if (EmailApp[0]=='\"' OR EmailApp[0]=='\'')
				{
					Delim = EmailApp[0];
					File = EmailApp + 1;
				}
				else
				{
					Delim = ' ';
					File = EmailApp;
				}

				char *EndOfFile = strchr(File, Delim);
				if (EndOfFile)
				{
					*EndOfFile++ = 0;
					
					// run the damn thing
					LgiExecute(File, EndOfFile, ".");
				}
			}
		}
		else
		{
			// webpage
			LgiExecute(Url, "", "");
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
class GTextView2_Factory : public GViewFactory
{
	GView *NewView(char *Class, GRect *Pos, char *Text)
	{
		if (stricmp(Class, "GTextView2") == 0)
		{
			return new GTextView2(-1, 0, 0, 100, 100);
		}

		return 0;
	}

} TextView2_Factory;
