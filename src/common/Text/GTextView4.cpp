#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GTextView4.h"
#include "GInput.h"

#if defined WIN32
#define NativeUnicode				"ucs-2"
#elif defined LINUX
#define NativeUnicode				"utf-32"
#elif defined BEOS
#define NativeUnicode				"utf-8"
#endif

#define PROF_PAINT					0
#define BACKGROUND_COLOUR			LC_MED
// #define BACKGROUND_COLOUR			LC_WORKSPACE

#define SubtractPtr(a, b)			(	(((int)(a))-((int)(b))) / sizeof(char16)	)

char Tabs[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

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

//////////////////////////////////////////////////////////////////////
class GBlock;
class GCursor;

enum GTag
{
	TAG_CONTENT,
	TAG_P,
};

class GFlow
{
public:
	int MinX, MaxX;
	int CurX, CurY;
	
	GFlow()
	{
		MinX = MaxX = CurX = 0;
		CurY = 0;
	}
	
	void EnterMargin(GRect &r)
	{
		MinX += r.x1;
		MaxX -= r.x2;
		CurX += r.x1;
		CurY += r.y1;
	}

	void ExitMargin(GRect &r)
	{
		MinX -= r.x1;
		MaxX += r.x2;
		CurX -= r.x1;
		CurY += r.y2;
	}
	
	int X()
	{
		return MaxX - MinX + 1;
	}
};

class GCursor
{
public:
	GBlock *Block;
	int Offset;
	GRegion Pos;

	GCursor();
	~GCursor();
	
	void Attach(GBlock *b, int off);
	void Detach();
};

class GTextView4Private
{
public:
	GTextView4 *View;
	GBlock *Root;
	GCursor *Cursor;
	GCursor *Select;

	GTextView4Private(GTextView4 *view);
	~GTextView4Private();
};

class GRun : public GDisplayString
{
public:
	int x, y;
	int Offset;

	GRun(GFont *f, char *s, int len = -1, int tabOrigin = 0) :
		GDisplayString(f, s, len, tabOrigin)
	{
		x = y = 0;
		Offset = 0;
	}
	
	~GRun()
	{
	}
};

class GBlock
{
	// Raw text and Display strings cache of text
	char					*Text;
	int						TextLen;

public:
	////////////////////////////////////////////////////
	// Data
	
	// Owning object
	GTextView4Private		*View;
	
	// Block type
	GTag					Tag;

	// Display strings cache of text
	GArray<GRun*>			Str;

	// On screen bounds + padding
	GRect					Border;
	GRect					Padding;
	GRect					Pos; // relitive to parent
	
	// Heirarchy
	GBlock					*Parent;
	GArray<GBlock*>			c;

	// Array of cursors in this block
	GArray<GCursor*>		Cursors;

	////////////////////////////////////////////////////
	// Methods
	GBlock(GTextView4Private *view)
	{
		Tag = TAG_CONTENT;
		View = view;
		Text = 0;
		Parent = 0;
		Border.ZOff(0, 0);
		Padding.ZOff(0, 0);
	}
	
	~GBlock()
	{
		while (Cursors.Length())
		{
			Cursors[0]->Detach();
		}
		
		Empty();
	}
	
	GFont *GetFont()
	{
		return View->View->GetFont();
	}
	
	char *GetText()
	{
		return Text;
	}
	
	void SetText(char *t, int len)
	{
		DeleteArray(Text);
		Text = NewStr(t, len);
		TextLen = len >= 0 ? len : Text ? strlen(Text) : 0;
	}
	
	void OnPaint(GSurface *pDC, GRegion *Unpainted)
	{
		GFont *f = GetFont();
		if (f)
		{
			// Draw the my text
			f->Transparent(false);
			f->Colour(LC_TEXT, LC_WORKSPACE);

			for (int i=0; i<Str.Length(); i++)
			{
				GRun *r = Str[i];
				if (r)
				{
					/*
					printf("Draw '%.20S' at %i,%i (%i,%i)\n",
						(OsChar*)*r,
						Pos.x1 + r->x, Pos.y1 + r->y,
						r->x, r->y);
					*/
					
					GRect t;
					t.ZOff(r->X()-1, r->Y()-1);
					t.Offset(Pos.x1 + r->x, Pos.y1 + r->y);
					Unpainted->Subtract(&t);

					r->Draw(pDC, Pos.x1 + r->x, Pos.y1 + r->y);
				}
			}
			
			// Draw my children
			for (int i=0; i<c.Length(); i++)
			{
				c[i]->OnPaint(pDC, Unpainted);
			}
		}
	}
	
	void OnFlow(GFlow *Flow, int Depth)
	{
		GFont *f = GetFont();
		if (f)
		{
			Flow->EnterMargin(Border);
			Flow->EnterMargin(Padding);
			
			if (Flow->X() != Pos.X())
			{
				Str.DeleteObjects();
			}

			Pos.x1 = Flow->MinX;
			Pos.x2 = Flow->MaxX;
			Pos.y1 = Flow->CurY;
			Pos.y2 = Flow->CurY;
		
			if (Text)
			{
				if (NOT Str.Length())
				{
					char *End = Text + TextLen;
					
					for (char *s = Text; s AND *s; )
					{
						int Len = (int)End - (int)s;
						GRun *r = NEW(GRun(f, s, min(300, Len)));
						if (r)
						{
							// Store the display string
							Str[Str.Length()] = r;
							r->x = Flow->CurX - Pos.x1;
							r->y = Flow->CurY - Pos.y1;
							r->Offset = (int)s - (int)Text;

							// See if it fits in the remaining layout space
							int RemainingX = Flow->MaxX - Flow->CurX;
							
							if (r->X() > RemainingX)
							{
								// Get the cutoff point
								int Char = r->CharAt(RemainingX);
								
								// Seek back to the last break point, usually whitespace
								int StartChar = Char;
								OsChar *Start = (OsChar*)*r;
								OsChar *o;
								for (o = Start + Char;
									o > Start AND *o != ' ' AND *o != '\t';
									PrevOsChar(o))
								{
									Char--;
								}
								
								if (Char == 0)
									Char = StartChar;
								
								// Changes the display to fit the available space
								r->Length(Char);
								
								// Move to the next line in the flow space
								Flow->CurY += r->Y();
								Flow->CurX = Flow->MinX;
								
								// Move the pointer on
								for (int i=0; i<Char+1; i++)
									LgiNextUtf8(s);
							}
							else
							{
								// Move the flow point
								Flow->CurX += r->X();
								
								// Move the pointer on
								int Char = LgiCharLen((OsChar*)*r, NativeUnicode, r->Length());
								for (int i=0; i<Char; i++)
									LgiNextUtf8(s);
							}
						}
						else break;
					}
				}
				else
				{
					GRun *r = Str[Str.Length()-1];
					if (r)
					{
						Flow->CurY = Pos.y1 + r->y;
					}
				}
			}

			// Flow children
			for (int i=0; i<c.Length(); i++)
			{
				c[i]->OnFlow(Flow, Depth+1);
			}
			
			// Post flow
			if (Tag == TAG_P)
			{
				Flow->CurY += f->GetHeight();
				Flow->CurX = Flow->MinX;
			}
			Pos.y2 = Flow->CurY;

			Flow->ExitMargin(Padding);
			Flow->ExitMargin(Border);
		}
		else
		{
			// Error
			Pos.ZOff(-1, -1);
		}
	}
	
	void Empty()
	{
		DeleteArray(Text);		
		Str.DeleteObjects();
		c.DeleteObjects();
	}
	
	GBlock *NewChild()
	{
		GBlock *b = NEW(GBlock(View));
		if (b)
		{
			c[c.Length()] = b;
		}
		
		return b;
	}
};

GCursor::GCursor()
{
	Block = 0;
	Offset = 0;
	Pos.ZOff(-1, -1);
}

GCursor::~GCursor()
{
	Detach();	
}

void GCursor::Attach(GBlock *b, int off)
{
	if (NOT Block AND
		b AND
		NOT b->Cursors.HasItem(this))
	{
		Block = b;
		Block->Cursors[Block->Cursors.Length()] = this;
		Offset = off;
	}
}

void GCursor::Detach()
{
	if (Block)
	{
		Block->Cursors.Delete(this);
		Block = 0;
	}
}

//////////////////////////////////////////////////////////////////////
GTextView4Private::GTextView4Private(GTextView4 *view)
{
	View = view;
	Root = NEW(GBlock(this));
	Cursor = NEW(GCursor);
	if (Root)
	{
		Root->Padding.Set(4, 4, 4, 4);
		if (Cursor)
		{
			Cursor->Attach(Root, 0);
		}
	}
}

GTextView4Private::~GTextView4Private()
{
	DeleteObj(Root);
}

//////////////////////////////////////////////////////////////////////
GTextView4::GTextView4(	int Id,
						int x, int y, int cx, int cy,
						GFontType *FontType)
	: ResObject(Res_Custom)
{
	// init vars
	d = NEW(GTextView4Private(this));

	// setup window
	SetId(Id);
	SetPourLargest(true);

	GRect r;
	r.ZOff(cx-1, cy-1);
	r.Offset(x, y);
	SetPos(r);
}

GTextView4::~GTextView4()
{
	DeleteObj(d);
}

bool GTextView4::GetFixed()
{
	return false;
}

void GTextView4::SetFixed(bool i)
{
}

void GTextView4::SetReadOnly(bool i)
{
	GDocView::SetReadOnly(i);

	#if defined WIN32
	SetDlgCode(i ? DLGC_WANTARROWS : DLGC_WANTALLKEYS);
	#endif
}

void GTextView4::SetTabSize(uint8 i)
{
	TabSize = limit(i, 2, 32);
}

void GTextView4::SetWrapType(uint8 i)
{
	GDocView::SetWrapType(i);
	OnPosChange();
	Invalidate();
}

GFont *GTextView4::GetFont()
{
	return GLayout::GetFont();
}

void GTextView4::SetFont(GFont *f, bool OwnIt)
{
	GLayout::SetFont(f, OwnIt);
}

bool GTextView4::Insert(int At, char16 *Data, int Len)
{
	if (NOT ReadOnly)
	{
	}

	return false;
}

bool GTextView4::Delete(int At, int Len)
{
	bool Status = false;

	if (NOT ReadOnly)
	{
	}

	return Status;
}

void GTextView4::DeleteSelection(char16 **Cut)
{
}

char *GTextView4::Name()
{
	return 0;
}

bool GTextView4::Name(char *s)
{
	bool Status = ValidStr(s);
	
	if (d->Root)
	{
		d->Root->Empty();
		
		while (s AND *s)
		{
			// Find the end of paragraph
			char *e = strchr(s, '\n');
			if (NOT e) e = s + strlen(s);
			int Len = (int)e-(int)s;

			// Insert block for paragraph
			GBlock *b = d->Root->NewChild();
			if (NOT b)
			{
				Status = false;
				break;
			}
			
			b->Tag = TAG_P;
			b->SetText(s, Len);
			if (NOT b->GetText())
			{
				Status = false;
				break;
			}
			
			s = *e ? e + 1 : e;
		}
	}
	
	return Status;
}

char16 *GTextView4::NameW()
{
	return 0;
}

bool GTextView4::NameW(char16 *s)
{
	return 0;
}

char *GTextView4::GetSelection()
{
	return 0;
}

bool GTextView4::HasSelection()
{
	return 0;
}

void GTextView4::SelectAll()
{
}

void GTextView4::UnSelectAll()
{
}

int GTextView4::GetLines()
{
	return 0;
}

void GTextView4::GetTextExtent(int &x, int &y)
{
}

void GTextView4::PositionAt(int &x, int &y, int Index)
{
}

int GTextView4::GetCursor(bool Cur)
{
	return 0;
}

int GTextView4::IndexAt(int x, int y)
{
	return 0;
}

void GTextView4::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
}

void GTextView4::SetBorder(int b)
{
}

bool GTextView4::Cut()
{
	bool Status = false;

	return Status;
}

bool GTextView4::Copy()
{
	bool Status = true;

	return Status;
}

bool GTextView4::Paste()
{
	return false;
}

bool GTextView4::ClearDirty(bool Ask, char *FileName)
{
	return false;
}

bool GTextView4::Open(char *Name, char *CharSet)
{
	bool Status = false;

	return Status;
}

bool GTextView4::Save(char *Name, char *CharSet)
{
	return false;
}

void GTextView4::UpdateScrollBars(bool Reset)
{
}

bool GTextView4::DoCase(bool Upper)
{
	return false;
}

void GTextView4::GotoLine(int i)
{
}

bool GTextView4::DoGoto()
{
	return false;
}

bool GTextView4::DoFindNext()
{
	return false;
}

bool GTextView4::DoFind()
{
	return false;
}

bool GTextView4::DoReplace()
{
	return false;
}

void GTextView4::SelectWord(int From)
{
}

bool GTextView4::OnFind(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	return false;
}

bool GTextView4::OnReplace(char16 *Find, char16 *Replace, bool All, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	return false;
}

bool GTextView4::OnMultiLineTab(bool In)
{
	bool Status = false;

	return Status;
}

void GTextView4::OnSetHidden(int Hidden)
{
}

void GTextView4::OnPosChange()
{
}

void GTextView4::OnCreate()
{
}

void GTextView4::OnEscape(GKey &K)
{
}

void GTextView4::OnMouseWheel(double l)
{
}

void GTextView4::OnFocus(bool f)
{
	Invalidate();
	
	SetPulse(f ? 500 : -1);
}

int GTextView4::HitText(int x, int y)
{
	return 0;
}

void GTextView4::Undo()
{
}

void GTextView4::Redo()
{
}

void GTextView4::OnMouseClick(GMouse &m)
{
}

int GTextView4::OnHitTest(int x, int y)
{
	return GView::OnHitTest(x, y);
}

void GTextView4::OnMouseMove(GMouse &m)
{
}

bool GTextView4::OnKey(GKey &k)
{
	return false;
}

void GTextView4::OnEnter(GKey &k)
{
}

void GTextView4::OnPaint(GSurface *pDC)
{
	#if 0
	pDC->Colour(Rgb24(255, 230, 255), 24);
	pDC->Rectangle();
	#endif

	if (d->Root)
	{
		#if PROF_PAINT
		int Start = LgiCurrentTime();
		#endif
		
		GFlow Flow;
		Flow.MinX = 0;
		Flow.MaxX = X()-1;
		Flow.CurY = 0;		
		d->Root->OnFlow(&Flow, 0);
		
		#if PROF_PAINT
		int Paint = LgiCurrentTime();
		#endif
		
		GRegion Unpainted;
		Unpainted.Union(&GetClient());
		d->Root->OnPaint(pDC, &Unpainted);
		pDC->Colour(BACKGROUND_COLOUR, 24);
		for (int i=0; i<Unpainted.Length(); i++)
		{
			pDC->Rectangle(Unpainted[i]);
		}

		#if PROF_PAINT
		printf("Flow=%ims Paint=%ims\n", Paint - Start, LgiCurrentTime() - Paint);
		#endif
	}
	else
	{
		pDC->Colour(BACKGROUND_COLOUR, 24);
		pDC->Rectangle();
	}
}

int GTextView4::OnEvent(GMessage *Msg)
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
	}

	return GLayout::OnEvent(Msg);
}

int GTextView4::OnNotify(GViewI *Ctrl, int Flags)
{
	return 0;
}

void GTextView4::OnPulse()
{
}

void GTextView4::OnUrl(char *Url)
{
}

bool GTextView4::IsDirty()
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////
class GTextView4_Factory : public GViewFactory
{
	GView *NewView(char *Class, GRect *Pos, char *Text)
	{
		if (stricmp(Class, "GTextView4") == 0)
		{
			return NEW(GTextView4(-1, 0, 0, 100, 100));
		}

		return 0;
	}

} TextView4_Factory;
