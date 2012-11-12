/*hdr
**      FILE:           GList.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           14/2/2000
**      DESCRIPTION:    Lgi self-drawn listbox
**
**      Copyright (C) 2000 Matthew Allen
**              fret@memecode.com
*/

#include <math.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GList.h"
#include "GEdit.h"
#include "GScrollBar.h"

#define DRAG_NONE					0
#define SELECT_ITEMS				1
#define RESIZE_COLUMN				2
#define DRAG_COLUMN					3
#define CLICK_COLUMN				4
#define TOGGLE_ITEMS				5
#define CLICK_ITEM					6

// Number of pixels you have to move the mouse until a drag is initiated.
#define DRAG_THRESHOLD				4

// Switches for various profiling code..
#define GLIST_POUR_PROFILE			0
#define GLIST_ONPAINT_PROFILE		0

// Options
#define DOUBLE_BUFFER_PAINT			0

// Colours
#define DragColumnColour			LC_LOW

#if defined(WIN32)
#if !defined(WS_EX_LAYERED)
#define WS_EX_LAYERED				0x80000
#endif
#if !defined(LWA_ALPHA)
#define LWA_ALPHA					2
#endif
typedef BOOL (__stdcall *_SetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
#endif

#define ForAllItems(Var)			List<GListItem>::I it = Items.Start(); for (GListItem *Var = *it; it.In(); it++, Var = *it)
#define ForAllItemsReverse(Var)		Iterator<GListItem> ItemIter(&Items); for (GListItem *Var = ItemIter.Last(); Var; Var = ItemIter.Prev())
#define VisibleItems()				CompletelyVisible // (LastVisible - FirstVisible + 1)
#define MaxScroll()					max(Items.Length() - CompletelyVisible, 0)

////////////////////////////////////////////////////////////////////////////////////////////
#define DRAG_COL_ALPHA				0xc0
#define LINUX_TRANS_COL				0

class GDragColumn : public GWindow
{
	GList *List;
	GListColumn *Col;
	int Index;
	int Offset;
	GdcPt2 ListScrPos;
	
	#ifdef LINUX
	GSurface *Back;
	#endif

public:
	int GetOffset() { return Offset; }
	int GetIndex() { return Index; }
	GListColumn *GetColumn() { return Col; }

	GDragColumn(GList *list, int col);
	~GDragColumn();

	void OnPaint(GSurface *pScreen);
};

class GListPrivate
{
public:
	// Mode
	GListMode Mode;
	int Columns;
	int VisibleColumns;

	// This is a pointer to a flag, that gets set when the object
	// is deleted. Used to trap events deleting the window. If an
	// event handler deletes the current window we can't touch any
	// of the member variables anymore, so we need to know to quit/return
	// ASAP.
	bool *DeleteFlag;

	// If this is true the ctrl is selecting lots of things
	// and we only want to notify once.
	bool NoSelectEvent;

	// Drag'n'drop
	GDragColumn *DragCol;
	GdcPt2 DragStart;
	int DragMode;
	int DragData;
	
	// Kayboard search
	int KeyLast;
	char16 *KeyBuf;
	
	// Column click
	int ColClick;
	GMouse ColMouse;	

	// Editing label
	GView *Edit;
	
	// Class
	GListPrivate()
	{
		Edit = 0;
		DragCol = 0;
		DragMode = 0;
		DragData = 0;
		KeyBuf = 0;
		DeleteFlag = 0;
		Columns = 0;
		VisibleColumns = 0;
		ColClick = -1;
		Mode = GListDetails;
		NoSelectEvent = false;
	}
	
	~GListPrivate()
	{
		DeleteObj(Edit);
		if (DeleteFlag)
		{
			*DeleteFlag = true;
		}
		DeleteObj(DragCol);
		DeleteArray(KeyBuf);
	}
};

class GListColumnPrivate
{
public:
	GRect Pos;
	bool Down;
	bool Drag;

	GList *Parent;
	char *cName;
	GDisplayString *Txt;
	int cWidth;
	int cType;
	GSurface *cIcon;
	int cImage;
	int cMark;
	bool OwnIcon;
	bool CanResize;

	GListColumnPrivate(GList *parent)
	{
		Parent = parent;
		Txt = 0;
		cName = 0;
		cWidth = 0;
		cIcon = 0;
		cType = GIC_ASK_TEXT;
		cImage = -1;
		cMark = GLI_MARK_NONE;
		Down = false;
		OwnIcon = false;
		CanResize = true;
		Drag = false;
	}

	~GListColumnPrivate()
	{
		DeleteArray(cName);
		if (OwnIcon)
		{
			DeleteObj(cIcon);
		}
		DeleteObj(Txt);
	}
};

class GListItemPrivate
{
public:
	bool Selected;
	int ListItem_Image;
	List<GListItemColumn> Cols;
	GArray<char*> Str;
	GArray<GDisplayString*> Display;
	int SelectionStart, SelectionEnd;
	int16 LayoutColumn;

	GListItemPrivate()
	{
		SelectionStart = SelectionEnd = -1;
		Selected = 0;
		ListItem_Image = -1;
		LayoutColumn = -1;
	}

	~GListItemPrivate()
	{
		Cols.DeleteObjects();
		EmptyStrings();
		EmptyDisplay();
	}

	void EmptyStrings()
	{
		for (int i=0; i<Str.Length(); i++)
		{
			DeleteArray(Str[i]);
		}
		Str.Length(0);
	}

	void EmptyDisplay()
	{
		for (int i=0; i<Display.Length(); i++)
		{
			DeleteObj(Display[i]);
		}
		Display.Length(0);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
GDragColumn::GDragColumn(GList *list, int col)
{
	List = list;
	Index = col;
	Offset = 0;
	#ifdef LINUX
	Back = 0;
	#endif
	Col = List->ColumnAt(Index);
	if (Col)
	{
		Col->d->Down = false;
		Col->d->Drag = true;

		GRect r = Col->d->Pos;
		r.y1 = 0;
		r.y2 = List->Y()-1;
		List->Invalidate(&r, true);

		#if WIN32NATIVE
		
		GArray<int> Ver;
		bool Layered = LgiGetOs(&Ver) == LGI_OS_WINNT && Ver[0] >= 5;
		SetStyle(WS_POPUP);
		SetExStyle(GetExStyle() | WS_EX_TOOLWINDOW);
		if (Layered)
		{
			SetExStyle(GetExStyle() | WS_EX_LAYERED | WS_EX_TRANSPARENT);
		}
		
		#elif defined XWIN
		
		Back = new GMemDC;
		if (Back && Back->Create(List->X(), List->Y(), GdcD->GetBits()))
		{
			List->OnPaint(Back);
		}

		//XSetWindowAttributes a;
		//a.override_redirect = true;
		//a.save_under = false; // true;
		// XChangeWindowAttributes(Handle()->XDisplay(), Handle()->handle(), CWSaveUnder | CWOverrideRedirect, &a);
		
		#endif

		Attach(0);

		#if WIN32NATIVE
		
		if (Layered)
		{
			SetWindowLong(Handle(), GWL_EXSTYLE, GetWindowLong(Handle(), GWL_EXSTYLE) | WS_EX_LAYERED);

			GLibrary User32("User32");
			_SetLayeredWindowAttributes SetLayeredWindowAttributes = (_SetLayeredWindowAttributes)User32.GetAddress("SetLayeredWindowAttributes");
			if (SetLayeredWindowAttributes)
			{
				if (!SetLayeredWindowAttributes(Handle(), 0, DRAG_COL_ALPHA, LWA_ALPHA))
				{
					DWORD Err = GetLastError();
				}
			}
		}
		
		#endif

		GMouse m;
		List->GetMouse(m);
		Offset = m.x - r.x1;

		List->PointToScreen(ListScrPos);
		r.Offset(ListScrPos.x, ListScrPos.y);

		SetPos(r);
		Visible(true);
	}
}

GDragColumn::~GDragColumn()
{
	Visible(false);

	if (Col)
	{
		Col->d->Drag = false;
	}

	List->Invalidate();
}

#if LINUX_TRANS_COL
void GDragColumn::OnPosChange()
{
	Invalidate();
}
#endif

void GDragColumn::OnPaint(GSurface *pScreen)
{
	#if LINUX_TRANS_COL
	GSurface *Buf = new GMemDC(X(), Y(), GdcD->GetBits());
	GSurface *pDC = new GMemDC(X(), Y(), GdcD->GetBits());
	#else
	GSurface *pDC = pScreen;
	#endif
	
	pDC->SetOrigin(Col->d->Pos.x1, 0);
	if (Col) Col->d->Drag = false;
	List->OnPaint(pDC);
	if (Col) Col->d->Drag = true;
	pDC->SetOrigin(0, 0);
	
	#if LINUX_TRANS_COL
	if (Buf && pDC)
	{
		GRect p = GetPos();
		
		// Fill the buffer with the background
		Buf->Blt(ListScrPos.x - p.x1, 0, Back);

		// Draw painted column over the back with alpha
		Buf->Op(GDC_ALPHA);
		GApplicator *App = Buf->Applicator();
		if (App)
		{
			App->SetVar(GAPP_ALPHA_A, DRAG_COL_ALPHA);
		}
		Buf->Blt(0, 0, pDC);

		// Put result on the screen
		pScreen->Blt(0, 0, Buf);
	}
	DeleteObj(Buf);
	DeleteObj(pDC);
	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////
#define M_END_POPUP (M_USER+0x656)

class GItemEditBox : public GEdit
{
	GItemEdit *ItemEdit;

public:
	GItemEditBox(GItemEdit *i, int x, int y, char *s) : GEdit(100, 1, 1, x-3, y-3, s)
	{
		ItemEdit = i;
		Sunken(false);
		
		SetPos(GetPos());
	}

	void OnCreate()
	{
		GEdit::OnCreate();
		Focus(true);
	}

	void OnFocus(bool f)
	{
		printf("GItemEditBox::OnFocus(%i)\n", f);
		if (!f)
		{
			#if WIN32NATIVE
			HWND pop = GetLastActivePopup(Handle());
			HWND par = GetParent()->Handle();
			if (pop &&
				pop != Handle() &&
				par != pop &&
				IsWindow(pop) &&
				IsWindowVisible(pop))
			{
				return;
			}
			#endif

			GetParent()->Visible(false);
		}
		
		GEdit::OnFocus(f);
	}

	bool OnKey(GKey &k)
	{
		if (k.c16 == VK_RETURN ||
			k.c16 == VK_ESCAPE)
		{
			if (k.Down())
			{			
				ItemEdit->OnNotify(this, k.c16);
			}
			
			return true;
		}
		
		return GEdit::OnKey(k);
	}
};

class GItemEditPrivate
{
public:
	GItem *Item;
	GEdit *Edit;
	int Index;
	bool Esc;

	GItemEditPrivate()
	{
		Esc = false;
		Item = 0;
		Index = 0;
	}
};

GItemEdit::GItemEdit(GView *parent, GItem *item, int index, int SelStart, int SelEnd)
	: GPopup(parent)
{
	d = new GItemEditPrivate;
	d->Item = item;
	d->Index = index;

	_BorderSize = 0;
	Sunken(false);
	Raised(false);
	SetParent(parent);
	
	GdcPt2 p;
	GetParent()->PointToScreen(p);

	GRect r = d->Item->GetPos(d->Index);
	r.Offset(p.x-1, p.y);
	// r.x2 += 2;
	r.y2 += 2;
	SetPos(r);

	if (Attach(GetParent()))
	{
		d->Edit = new GItemEditBox(this, r.X(), r.Y(), d->Item->GetText(d->Index));
		if (d->Edit)
		{
			d->Edit->Attach(this);
			d->Edit->Focus(true);
			
			if (SelStart >= 0)
			{
				d->Edit->Select(SelStart, SelEnd-SelStart+1);
			}
		}

		Visible(true);
	}
}

GItemEdit::~GItemEdit()
{
	if (d->Item)
	{
		if (d->Edit && !d->Esc)
		{
			d->Item->SetText(d->Edit->Name(), d->Index);
			d->Item->Update();
		}

		d->Item->OnEditLabelEnd();
	}
	
	DeleteObj(d);
}

void GItemEdit::OnPaint(GSurface *pDC)
{
	#if 1

	// pDC->Colour(Rgb24(255, 0, 255), 24);
	pDC->Colour(LC_BLACK, 24);
	pDC->Rectangle();

	#else

	GRect r = GetClient();
	pDC->Colour(LC_BLACK, 24);
	pDC->Box(&r);
	r.Size(1, 1);

	#endif
}

int GItemEdit::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case 100:
		{
			if (f == VK_ESCAPE)
			{
				d->Esc = true;
			}

			if (f == VK_ESCAPE || f == VK_RETURN)
			{
				Visible(false);
			}

			break;
		}
	}

	return 0;
}

void GItemEdit::Visible(bool i)
{
	GPopup::Visible(i);
	if (!i)
	{
		PostEvent(M_END_POPUP);
	}
}

GMessage::Result GItemEdit::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_END_POPUP:
		{
			Quit();
			return 0;
		}
	}

	return GPopup::OnEvent(Msg);
}

////////////////////////////////////////////////////////////////////////////////////////////
// List column
GListColumn::GListColumn(GList *parent, const char *name, int width)
	: ResObject(Res_Column)
{
	d = new GListColumnPrivate(parent);
	d->cWidth = width;
	if (name)
		Name(name);
}

GListColumn::~GListColumn()
{
	if (d->Drag)
	{
		d->Parent->DragColumn(-1);
	}

	DeleteObj(d);
}

GList *GListColumn::GetList()
{
	return d->Parent;
}

void GListColumn::Image(int i)
{
	d->cImage = i;
}

int GListColumn::Image()
{
	return d->cImage;
}

bool GListColumn::Resizable()
{
	return d->CanResize;
}

void GListColumn::Resizable(bool i)
{
	d->CanResize = i;
}

void GListColumn::Name(const char *n)
{
	DeleteArray(d->cName);
	DeleteObj(d->Txt);
	d->cName = NewStr(n);
	d->Txt = new GDisplayString(SysFont, (char*)n);
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

char *GListColumn::Name()
{
	return d->cName;
}

int GListColumn::GetIndex()
{
	if (d->Parent)
	{
		return d->Parent->Columns.IndexOf(this);
	}

	return -1;
}

int GListColumn::GetContentSize()
{
	int Max = 0;

	if (d->Parent)
	{
		int Index = GetIndex();

		for (List<GListItem>::I Items = d->Parent->Items.Start(); Items.In(); Items++)
		{
			GListItem *i = *Items;
			GDisplayString *s = i->d->Display[Index];
			GDisplayString *Mem = 0;
			
			// If no cached string, create it for the list item
			if (!s || s->IsTruncated())
			{
				GFont *f = i->GetFont();
				if (!f) f = d->Parent->GetFont();
				if (!f) f = SysFont;

				char *Text = i->d->Str[Index] ? i->d->Str[Index] : i->GetText(Index);
				if (s && s->IsTruncated())
				{
					s = Mem = new GDisplayString(f, Text?Text:(char*)"");
				}
				else
				{
					s = i->d->Display[Index] = new GDisplayString(f, Text?Text:(char*)"");
				}
			}

			// Measure it
			if (s)
			{
				Max = max(Max, s->X());
			}
			
			DeleteObj(Mem);
		}
		
		// Measure the heading too
		GFont *f = d->Parent->GetFont();
		LgiAssert(f);
		if (f)
		{
			GDisplayString h(d->Parent->GetFont(), d->cName);
			int Hx = h.X() + (d->cMark ? 10 : 0);
			Max = max(Max, Hx);
		}
	}

	return Max;
}

void GListColumn::Width(int i)
{
	if (d->cWidth != i)
	{
		d->cWidth = i;
		
		// If we are attached to a list...
		if (d->Parent)
		{
			int MyIndex = GetIndex();

			// Clear all the cached strings for this column
			for (List<GListItem>::I it=d->Parent->Items.Start(); it.In(); it++)
			{
				DeleteObj((*it)->d->Display[MyIndex]);
			}

			if (d->Parent->IsAttached())
			{
				// Update the screen from this column across
				GRect Up = d->Parent->GetClient();
				Up.x1 = d->Pos.x1;
				d->Parent->Invalidate(&Up);
			}
		}

		// Notify listener
		GViewI *n = d->Parent->GetNotify() ? d->Parent->GetNotify() : d->Parent->GetParent();
		if (n)
		{
			n->OnNotify(d->Parent, GLIST_NOTIFY_COLS_SIZE);
		}
	}
}

int GListColumn::Width()
{
	return d->cWidth;
}

void GListColumn::Mark(int i)
{
	d->cMark = i;
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

int GListColumn::Mark()
{
	return d->cMark;
}

void GListColumn::Type(int i)
{
	d->cType = i;
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

int GListColumn::Type()
{
	return d->cType;
}

void GListColumn::Icon(GSurface *i, bool Own)
{
	if (d->OwnIcon)
	{
		DeleteObj(d->cIcon);
	}
	d->cIcon = i;
	d->OwnIcon = Own;

	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

GSurface *GListColumn::Icon()
{
	return d->cIcon;
}

int GListColumn::Value()
{
	return d->Down;
}

void GListColumn::OnPaint_Content(GSurface *pDC, GRect &r, bool FillBackground)
{
	if (!d->Drag)
	{
		int Off = d->Down ? 1 : 0;
		int Mx = r.x1 + 8, My = r.y1 + ((r.Y() - 8) / 2);
		if (d->cIcon)
		{
			if (FillBackground)
			{
				pDC->Colour(LC_MED, 24);
				pDC->Rectangle(&r);
			}

			int x = (r.X()-d->cIcon->X()) / 2;
			pDC->Blt(	r.x1 + x + Off,
						r.y1 + ((r.Y()-d->cIcon->Y())/2) + Off,
						d->cIcon);

			if (d->cMark)
			{
				Mx += x + d->cIcon->X() + 4;
			}
		}
		else if (d->cImage >= 0 && d->Parent)
		{
			if (FillBackground)
			{
				pDC->Colour(LC_MED, 24);
				pDC->Rectangle(&r);
			}
			
			if (d->Parent->ImageList)
			{
				GRect *b = d->Parent->ImageList->GetBounds();
				int x = r.x1;
				int y = r.y1;
				if (b)
				{
					b += d->cImage;
					x = r.x1 + ((r.X()-b->X()) / 2) - b->x1;
					y = r.y1 + ((r.Y()-b->Y()) / 2) - b->y1;
				}				
				
				d->Parent->ImageList->Draw(pDC,
											x + Off,
											y + Off,
											d->cImage, 0);
			}

			if (d->cMark)
			{
				Mx += d->Parent->ImageList->TileX() + 4;
			}
		}
		else if (ValidStr(d->cName) && d->Txt)
		{
			SysFont->Transparent(!FillBackground);
			SysFont->Colour(LC_TEXT, LC_MED);
			d->Txt->Draw(pDC, r.x1 + Off + 3, r.y1 + Off, &r);

			if (d->cMark)
			{
				Mx += d->Txt->X();
			}
		}
		else
		{
			if (FillBackground)
			{
				pDC->Colour(LC_MED, 24);
				pDC->Rectangle(&r);
			}
		}

		#define ARROW_SIZE	9
		pDC->Colour(LC_TEXT, 24);
		Mx += Off;
		My += Off - 1;

		switch (d->cMark)
		{
			case GLI_MARK_UP_ARROW:
			{
				pDC->Line(Mx + 2, My, Mx + 2, My + ARROW_SIZE - 1);
				pDC->Line(Mx, My + 2, Mx + 2, My);
				pDC->Line(Mx + 2, My, Mx + 4, My + 2);
				break;
			}
			case GLI_MARK_DOWN_ARROW:
			{
				pDC->Line(Mx + 2, My, Mx + 2, My + ARROW_SIZE - 1);
				pDC->Line(	Mx,
							My + ARROW_SIZE - 3,
							Mx + 2,
							My + ARROW_SIZE - 1);
				pDC->Line(	Mx + 2,
							My + ARROW_SIZE - 1,
							Mx + 4,
							My + ARROW_SIZE - 3);
				break;
			}
		}
	}
}

void ColumnPaint(void *UserData, GSurface *pDC, GRect &r, bool FillBackground)
{
	((GListColumn*)UserData)->OnPaint_Content(pDC, r, FillBackground);
}

void GListColumn::OnPaint(GSurface *pDC, GRect &Rgn)
{
	GRect r = Rgn;

	if (d->Drag)
	{
		pDC->Colour(DragColumnColour, 24);
		pDC->Rectangle(&r);
	}
	else
	{
		int Off = d->Down ? 1 : 0;

		#ifdef MAC

		GArray<GColourStop> Stops;
		GRect j(r.x1, r.y1, r.x2-1, r.y2-1); 
		if (d->cMark)
		{
			// pDC->Colour(Rgb24(0, 0x4e, 0xc1), 24);
			// pDC->Line(r.x1, r.y1, r.x2, r.y1);

			Stops[0].Pos = 0.0;
			Stops[0].Colour = Rgb32(0xd0, 0xe2, 0xf5);
			Stops[1].Pos = 2.0 / (r.Y() - 2);
			Stops[1].Colour = Rgb32(0x98, 0xc1, 0xe9);
			Stops[2].Pos = 0.5;
			Stops[2].Colour = Rgb32(0x86, 0xba, 0xe9);
			Stops[3].Pos = 0.51;
			Stops[3].Colour = Rgb32(0x68, 0xaf, 0xea);
			Stops[4].Pos = 1.0;
			Stops[4].Colour = Rgb32(0xbb, 0xfc, 0xff);
			
			LgiFillGradient(pDC, j, true, Stops);
			
			pDC->Colour(Rgb24(0x66, 0x93, 0xc0), 24);
			pDC->Line(r.x1, r.y2, r.x2, r.y2);
			pDC->Line(r.x2, r.y1, r.x2, r.y2);
		}
		else
		{
			// pDC->Colour(Rgb24(160, 160, 160), 24);
			// pDC->Line(r.x1, r.y1, r.x2, r.y1);
			
			Stops[0].Pos = 0.0;
			Stops[0].Colour = Rgb32(255, 255, 255);
			Stops[1].Pos = 0.5;
			Stops[1].Colour = Rgb32(241, 241, 241);
			Stops[2].Pos = 0.51;
			Stops[2].Colour = Rgb32(233, 233, 233);
			Stops[3].Pos = 1.0;
			Stops[3].Colour = Rgb32(255, 255, 255);
			
			LgiFillGradient(pDC, j, true, Stops);
			
			pDC->Colour(Rgb24(178, 178, 178), 24);
			pDC->Line(r.x1, r.y2, r.x2, r.y2);
			pDC->Line(r.x2, r.y1, r.x2, r.y2);
		}

		GRect n = r;
		n.Size(2, 2);
		OnPaint_Content(pDC, n, false);

		#else
		if (GApp::SkinEngine)
		{
			GSkinState State;
			
			State.pScreen = pDC;			
			State.Text = &d->Txt;
			State.Rect = Rgn;
			State.Value = Value();
			State.Enabled = GetList()->Enabled();

			GApp::SkinEngine->OnPaint_ListColumn(ColumnPaint, this, &State);
		}
		else
		{
			if (d->Down)
			{
				LgiThinBorder(pDC, r, SUNKEN);
				LgiFlatBorder(pDC, r, 1);
			}
			else
			{
				LgiWideBorder(pDC, r, RAISED);
			}
			
			OnPaint_Content(pDC, r, true);
		}	
		#endif	
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
GListItemColumn::GListItemColumn(GListItem *item, int col)
{
	_Column = col;
	_Item = item;
	_Value = 0;
	_Item->d->Cols.Insert(this);
}

GList *GListItemColumn::GetList()
{
	return _Item ? _Item->Parent : 0;
}

List<GListItem> *GListItemColumn::GetAllItems()
{
	return GetList() ? &GetList()->Items : 0;
}

void GListItemColumn::Value(int64 i)
{
	if (i != _Value)
	{
		_Value = i;
		_Item->Update();
		_Item->OnColumnNotify(_Column, _Value);
	}
}

GListItemColumn *GListItemColumn::GetItemCol(GListItem *i, int Col)
{
	if (i)
	{
		for (GListItemColumn *c=i->d->Cols.First(); c; c=i->d->Cols.Next())
		{
			if (c->_Column == Col)
			{
				return c;
			}
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////
// List item
GListItem::GListItem()
{
	d = new GListItemPrivate;
	Pos.ZOff(-1, -1);
	Parent = 0;
}

GListItem::~GListItem()
{
	if (Parent)
	{
		Parent->Remove(this);
	}

	DeleteObj(d);
}

void GListItem::OnEditLabelEnd()
{
	if (Parent)
	{
		Parent->d->Edit = 0;
	}
}

void GListItem::SetImage(int i)
{
	d->ListItem_Image = i;
}

int GListItem::GetImage(int Flags)
{
	return d->ListItem_Image;
}

List<GListItemColumn> *GListItem::GetItemCols()
{
	return &d->Cols;
}

/*
Calling this to store your data is optional. Just override the
"GetText" function to return your own data to avoid duplication
in memory.
*/
bool GListItem::SetText(const char *s, int i)
{
	if (i < 0)
		return false;
		
	// Delete any existing column
	DeleteArray((char*&)d->Str[i]);
	DeleteObj(d->Display[i]);

	// Add new string in
	d->Str[i] = NewStr(s);

	return true;
}

// User can override this if they want to use their own data
char *GListItem::GetText(int i)
{
	return d->Str[i];
}

bool GListItem::Select()
{
	return d->Selected;
}

void GListItem::SetEditLabelSelection(int SelStart, int SelEnd)
{
	d->SelectionStart = SelStart;
	d->SelectionEnd = SelEnd;
}

GView *GListItem::EditLabel(int Col)
{
	GetList()->Capture(false);

	if (Parent)
	{
		if (!Parent->d->Edit)
		{
			Parent->d->Edit = new GItemEdit(Parent, this, Col, d->SelectionStart, d->SelectionEnd);
			d->SelectionStart = d->SelectionEnd = -1;
		}

		return Parent->d->Edit;
	}

	return 0;	
}

GRect *GListItem::GetPos(int Col)
{
	static GRect r;

	r = Pos;

	if (Parent->GetMode() == GListDetails)
	{
		if (Col >= 0)
		{
			GListColumn *Column = 0;

			int Cx = Parent->GetImageList() ? 16 : 0;
			for (int c=0; c<Col; c++)
			{
				Column = Parent->ColumnAt(c);
				if (Column)
				{
					Cx += Column->Width();
				}
			}
			Column = Parent->ColumnAt(Col);

			if (Column)
			{
				r.x1 = Cx;
				r.x2 = Cx + Column->Width() - 1;
			}
		}
	}
	else
	{
		r.Offset(16, 0);
	}

	return &r;
}

void GListItem::Select(bool b)
{
	if (d->Selected != b)
	{
		d->Selected = b;
		Update();
		
		if (Parent &&
			d->Selected &&
			!Parent->d->NoSelectEvent)
		{
			GArray<GListItem*> Items;
			Items.Add(this);
			Parent->OnItemSelect(Items);
		}
	}
}

void GListItem::ScrollTo()
{
	if (Parent)
	{
		if (Parent->GetMode() == GListDetails && Parent->VScroll)
		{
			int n = Parent->Items.IndexOf(this);
			if (n < Parent->FirstVisible)
			{
				Parent->VScroll->Value(n);
				Parent->Invalidate(&Parent->ItemsPos);
			}
			else if (n >= Parent->LastVisible)
			{
				Parent->VScroll->Value(n - (Parent->LastVisible - Parent->FirstVisible) + 1);
				Parent->Invalidate(&Parent->ItemsPos);
			}
		}
		else if (Parent->GetMode() == GListColumns && Parent->HScroll)
		{
			int n = Parent->Items.IndexOf(this);
			if (n < Parent->FirstVisible)
			{
				Parent->HScroll->Value(d->LayoutColumn);
				Parent->Invalidate(&Parent->ItemsPos);
			}
			else if (n >= Parent->LastVisible)
			{
				int Range = Parent->HScroll->Page();
				
				Parent->HScroll->Value(d->LayoutColumn - Range);
				Parent->Invalidate(&Parent->ItemsPos);
			}
		}
	}
}

void GListItem::Update()
{
	if (Parent)
	{
		if (Parent->Lock(_FL))
		{
			d->EmptyDisplay();

			GMeasureInfo Info;
			OnMeasure(&Info);

			GRect r = Pos;
			if (r.Valid())
			{
				if (Info.y != r.Y())
				{
					Pos.y2 = Pos.y1 + Info.y - 1;
					Parent->Pour();

					r.y1 = min(r.y1, Pos.y1);
					r.y2 = Parent->ItemsPos.y2;
				}

				Parent->Invalidate(&r);
			}

			Parent->Unlock();
		}
	}
	else
	{
		d->EmptyDisplay();
	}
}

void GListItem::OnMeasure(GMeasureInfo *Info)
{
	if (Info)
	{
		if (Parent &&
			Parent->GetMode() == GListDetails)
		{
			Info->x = 1024;
		}
		else
		{
			GDisplayString *s = GetDs(0);
			Info->x = 22 + (s ? s->X() : 0);
		}
		
		Info->y = max(16, SysFont->GetHeight() + 2); // the default height
	}
}

bool GListItem::GridLines()
{
	return (Parent) ? Parent->GridLines : false;
}

void GListItem::OnMouseClick(GMouse &m)
{
	int Col = Parent->ColumnAtX(m.x);
	for (GListItemColumn *h = d->Cols.First(); h; h=d->Cols.Next())
	{
		if (Col == h->GetColumn())
		{
			h->OnMouseClick(m);
		}
	}
}

GDisplayString *GListItem::GetDs(int Col, int FitTo)
{
	if (!d->Display[Col])
	{
		GFont *f = GetFont();
		if (!f) f = Parent->GetFont();
		if (!f) f = SysFont;

		char *Text = d->Str[Col] ? d->Str[Col] : GetText(Col);
		LgiAssert((NativeInt)Text != 0xcdcdcdcd &&
				  (NativeInt)Text != 0xfdfdfdfd);
		d->Display[Col] = new GDisplayString(f, Text?Text:(char*)"");
		if (d->Display[Col] && FitTo > 0)
		{
			d->Display[Col]->TruncateWithDots(FitTo);
		}
	}
	return d->Display[Col];
}

void GListItem::OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GListColumn *c)
{
	GSurface *&pDC = Ctx.pDC;
	if (pDC && c)
	{
		GRect ng = Ctx; // non-grid area

		if (c->d->Drag)
		{
			pDC->Colour(DragColumnColour, 24);
			pDC->Rectangle(&ng);
		}
		else
		{
			COLOUR Background = Ctx.Back;
			GViewFill *ForeFill = GetForegroundFill();

			if (Parent->GetMode() == GListDetails &&
				c->Mark() &&
				!d->Selected)
			{
				Background = GdcMixColour(0, Background, (double)1/32);
			}

			if (GridLines())
			{
				ng.x2--;
				ng.y2--;
			}

			if (c->Type() == GIC_ASK_TEXT)
			{
				GDisplayString *Ds = GetDs(i, Ctx.X());
				if (Ds)
				{
					Ds->GetFont()->TabSize(0);
					Ds->GetFont()->Transparent(false);
					Ds->GetFont()->Colour(ForeFill ? ForeFill->GetFlat().c24() : Ctx.Fore, Background);
					Ds->Draw(pDC, Ctx.x1+1, Ctx.y1+1, &ng);
				}
				else
				{
					pDC->Colour(Background, 24);
					pDC->Rectangle(&ng);
				}
			}
			else
			{
				pDC->Colour(Background, 24);
				pDC->Rectangle(&ng);

				if (c->Type() == GIC_ASK_IMAGE &&
					Parent->ImageList)
				{
					int Img = GetImage();
					if (Img >= 0)
					{
					    int CenterY = Ctx.y1 + ((Ctx.Y() - Parent->ImageList->TileY()) >> 1);
					    LgiAssert(CenterY >= 0);
						Parent->ImageList->Draw(pDC, Ctx.x1+1, CenterY, Img, 0);
					}
				}
			}

			if (GridLines())
			{
				pDC->Colour(LC_LOW, 24);
				pDC->Line(Ctx.x1, Ctx.y2, Ctx.x2, Ctx.y2);
				pDC->Line(Ctx.x2, Ctx.y1, Ctx.x2, Ctx.y2);
			}
		}
	}
}

void GListItem::OnPaint(GItem::ItemPaintCtx &Ctx)
{
	if (!Parent)
	    return;

	int i = 0;
	int x = Ctx.x1;

	if (!Select())
	{
		GViewFill *Fill;
		if (Fill = GetForegroundFill())
			Ctx.Fore = Fill->GetFlat().c24();
		if (Fill = GetBackgroundFill())
			Ctx.Back = Fill->GetFlat().c24();
	}

	// Icon?
	if (Parent->IconCol)
	{
		GRect cr(x, Ctx.y1, x + Parent->IconCol->Width()-1, Ctx.y2);

		// draw icon
		OnPaintColumn(Ctx, -1, Parent->IconCol);
		x = cr.x2 + 1;
	}
	
	// draw columns
	GListItemColumn *h = d->Cols.First();
	GItem::ItemPaintCtx ColCtx = Ctx;
	for (GListColumn *c = Parent->Columns.First(); c; c = Parent->Columns.Next(), i++)
	{
		if (Parent->GetMode() == GListColumns)
			ColCtx.Set(x, Ctx.y1, Ctx.x2, Ctx.y2);
		else
			ColCtx.Set(x, Ctx.y1, x + c->Width()-1, Ctx.y2);
		
		OnPaintColumn(ColCtx, i, c);
		if (h && i == h->GetColumn())
		{
			h->OnPaintColumn(ColCtx, i, c);
			h = d->Cols.Next();
		}
		x = ColCtx.x2 + 1;
		
		if (Parent->GetMode() == GListColumns)
			break;
	}
	
	// after columns
	if (x <= Ctx.x2)
	{
		Ctx.pDC->Colour(Ctx.Back, 24);
		Ctx.pDC->Rectangle(x, Ctx.y1, Ctx.x2, Ctx.y2);
	}
}

//////////////////////////////////////////////////////////////////////////////
// List control
GList::GList(int id, int x, int y, int cx, int cy, const char *name)
	: ResObject(Res_ListView)
{
	d = new GListPrivate;
	SetId(id);
	Name(name);
	
	ItemsPos.ZOff(-1, -1);
	Buf = 0;
	GridLines = false;
	FirstVisible = -1;
	LastVisible = -1;
	ColumnHeaders = true;
	ColumnHeader.ZOff(-1, -1);
	EditLabels = false;
	MultiItemSelect = true;
	IconCol = 0;
	CompletelyVisible = 0;
	Keyboard = -1;
	Sunken(true);
	Name("GList");

	#if WIN32NATIVE
	SetStyle(GetStyle() | WS_TABSTOP);
	SetDlgCode(DLGC_WANTARROWS);
	Cursor = 0;
	#elif defined BEOS
	Handle()->SetViewColor(B_TRANSPARENT_COLOR);
	#endif
	SetTabStop(true);

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
}

GList::~GList()
{
	DeleteObj(Buf);
	DeleteObj(d->Edit);

	Empty();
	EmptyColumns();
	DeleteObj(d);
}

GListMode GList::GetMode()
{
	return d->Mode;
}

void GList::SetMode(GListMode m)
{
	if (d->Mode ^ m)
	{
		d->Mode = m;
		
		if (IsAttached())
		{
			Pour();
			Invalidate();
		}
	}
}

void GList::OnItemClick(GListItem *Item, GMouse &m)
{
	if (Item)
		Item->OnMouseClick(m);
}

void GList::OnItemBeginDrag(GListItem *Item, GMouse &m)
{
	if (Item)
		Item->OnBeginDrag(m);
}

void GList::OnItemSelect(GArray<GListItem*> &It)
{
	if (It.Length())
	{
		Keyboard = Items.IndexOf(It[0]);
		LgiAssert(Keyboard >= 0);
		
		GHashTbl<void*, bool> Sel;
		for (int n=0; n<It.Length(); n++)
		{
			It[n]->OnSelect();
			if (!MultiItemSelect)
				Sel.Add(It[n], true);
		}

		if (!MultiItemSelect)
		{
			// deselect all other items
			ForAllItems(i)
			{
				if (!Sel.Find(i))
				{
					if (i->d->Selected)
					{
					    /*
						i->d->Selected = false;
						i->Update();
						*/
						i->Select(false);
					}
				}
			}
		}
	}

	// Notify selection change
	SendNotify(GLIST_NOTIFY_SELECT);
}

GListColumn *GList::AddColumn(const char *Name, int Width, int Where)
{
	GListColumn *c = 0;

	if (Lock(_FL))
	{
		c = new GListColumn(this, Name, Width);
		if (c)
		{
			Columns.Insert(c, Where);
			UpdateAllItems();
			SendNotify(GLIST_NOTIFY_COLS_CHANGE);
		}
		Unlock();
	}

	return c;
}

bool GList::AddColumn(GListColumn *Col, int Where)
{
	bool Status = false;

	if (Col && Lock(_FL))
	{
		Status = Columns.Insert(Col, Where);
		if (Status)
		{
			UpdateAllItems();
			SendNotify(GLIST_NOTIFY_COLS_CHANGE);
		}

		Unlock();
	}

	return Status;
}

bool GList::DeleteColumn(GListColumn *Col)
{
	bool Status = false;

	if (Col &&
		Lock(_FL))
	{
		if (Columns.HasItem(Col))
		{
			Columns.Delete(Col);
			DeleteObj(Col);
			UpdateAllItems();

			SendNotify(GLIST_NOTIFY_COLS_CHANGE);
			Status = true;
		}

		Unlock();
	}

	return Status;
}

void GList::DragColumn(int Index)
{
	DeleteObj(d->DragCol);

	if (Index >= 0)
	{
		d->DragCol = new GDragColumn(this, Index);
		if (d->DragCol)
		{
			Capture(true);
			d->DragMode = DRAG_COLUMN;
		}
	}
}

int GList::ColumnAtX(int x, GListColumn **Col, int *Offset)
{
	GListColumn *Column = 0;
	if (!Col) Col = &Column;

	int Cx = GetImageList() ? 16 : 0;
	int c;
	for (c=0; *Col = Columns.ItemAt(c); c++)
	{
		if (x >= Cx && x < Cx + (*Col)->Width())
		{
			break;
		}
		Cx += (*Col)->Width();
	}

	if (*Col)
	{
		if (Offset)
		{
			*Offset = Cx;
		}

		return c;
	}

	return -1;
}

void GList::EmptyColumns()
{
	Columns.DeleteObjects();
	DeleteObj(IconCol);
	Invalidate(&ColumnHeader);
	SendNotify(GLIST_NOTIFY_COLS_CHANGE);
}

GMessage::Result GList::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		#ifdef WIN32
		case WM_VSCROLL:
		{
			if (VScroll)
				return VScroll->OnEvent(Msg);
			break;
		}
		#endif
	}

	return GLayout::OnEvent(Msg);
}

int GList::OnNotify(GViewI *Ctrl, int Flags)
{
	if
	(
		(Ctrl->GetId() == IDC_VSCROLL && VScroll)
		||
		(Ctrl->GetId() == IDC_HSCROLL && HScroll)
	)
	{
		Invalidate(&ItemsPos);
	}

	return GLayout::OnNotify(Ctrl, Flags);
}

GRect &GList::GetClientRect()
{
	static GRect r;

	r = GetPos();
	r.Offset(-r.x1, -r.y1);

	return r;
}

GListItem *GList::HitItem(int x, int y, int *Index)
{
	int n=0;
	ForAllItems(i)
	{
		// if (y >= i->Pos.y1 && y <= i->Pos.y2)
		if (i->Pos.Overlap(x, y))
		{
			if (Index) *Index = n;
			return i;
		}
		n++;
	}

	return NULL;
}

void GList::KeyScroll(int iTo, int iFrom, bool SelectItems)
{
	int Start = -1, End = -1, i = 0;
	{
		ForAllItems(n)
		{
			if (n->Select())
			{
				if (Start < 0)
				{
					Start = i;
				}
			}
			else if (Start >= 0 &&
					End < 0)
			{
				End = i - 1;
			}

			i++;
		}

		if (End < 0) End = i - 1;
	}

	iTo = limit(iTo, 0, Items.Length()-1);
	iFrom = limit(iFrom, 0, Items.Length()-1);
	GListItem *To = Items.ItemAt(iTo);
	GListItem *From = Items.ItemAt(iFrom);
	int Inc = (iTo < iFrom) ? -1 : 1;

	if (To && From && iTo != iFrom)
	{
		GListItem *Item = 0;
		if (SelectItems)
		{
			int OtherEnd = Keyboard == End ? Start : End;
			int Min = min(OtherEnd, iTo);
			int Max = max(OtherEnd, iTo);
			i = 0;

			d->NoSelectEvent = true;
			GArray<GListItem*> Sel;
			ForAllItems(n)
			{
				bool s = i>=Min && i<=Max;
				n->Select(s);
				if (s) Sel.Add(n);
				i++;
			}
			d->NoSelectEvent = false;

			OnItemSelect(Sel);
		}
		else
		{
			Select(To);
		}

		To->ScrollTo();

		Keyboard = iTo;
	}
}

void GList::OnMouseWheel(double Lines)
{
	if (VScroll)
	{
		int Old = VScroll->Value();
		VScroll->Value(Old + (int)Lines);
		if (Old != VScroll->Value())
		{
			Invalidate(&ItemsPos);
		}
	}
	
	if (HScroll)
	{
		int Old = HScroll->Value();
		HScroll->Value(Old + (int)(Lines / 3));
		if (Old != HScroll->Value())
		{
			Invalidate(&ItemsPos);
		}
	}
}

bool GList::OnKey(GKey &k)
{
	bool Status = false;
	
	GListItem *Item = GetSelected();
	if (Item)
	{
		Status = Item->OnKey(k);
	}

	#ifdef MAC
	#define Modifier System
	#else
	#define Modifier Ctrl
	#endif

	if (k.Down())
	{
		if (k.vkey != VK_UP && k.vkey != VK_DOWN && k.Modifier())
		{
			switch (k.c16)
			{
				case 'A':
				case 'a':
				{
					SelectAll();
					Status = true;
					break;
				}
			}
		}
		else
		{
			switch (k.vkey)
			{
				case VK_RETURN:
				{
					#ifdef WIN32
					if (!k.IsChar)
					#endif
					{
						SendNotify(GLIST_NOTIFY_RETURN);
					}
					break;
				}
				case VK_BACKSPACE:
				{
					SendNotify(GLIST_NOTIFY_BACKSPACE);
					break;
				}
				case VK_ESCAPE:
				{
					SendNotify(GLIST_NOTIFY_ESC_KEY);
					break;
				}
				case VK_DELETE:
				{
					SendNotify(GLIST_NOTIFY_DEL_KEY);
					break;					
				}
				case VK_UP:
				{
					// int i = Value();
					#ifdef MAC
					if (k.Ctrl())
						goto GList_PageUp;
					else if (k.System())
						goto GList_Home;
					#endif
					
					KeyScroll(Keyboard-1, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_DOWN:
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GList_PageDown;
					else if (k.System())
						goto GList_End;
					#endif

					KeyScroll(Keyboard+1, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_LEFT:
				{
					if (GetMode() == GListColumns)
					{
						GListItem *Hit = GetSelected();
						if (Hit)
						{
							GListItem *To = 0;
							int ToDist = 0x7fffffff;
							
							for (GListItem *i=Items[FirstVisible]; i && i->Pos.Valid(); i=Items.Next())
							{
								if (i->Pos.x2 < Hit->Pos.x1)
								{
									int Dx = i->Pos.x1 - Hit->Pos.x1;
									int Dy = i->Pos.y1 - Hit->Pos.y1;
									int IDist = Dx * Dx + Dy * Dy;
									if (!To || IDist < ToDist)
									{
										To = i;
										ToDist = IDist;
									}
								}
							}
							
							if (!To && HScroll)
							{
								if (Hit->d->LayoutColumn == HScroll->Value() + 1)
								{
									// Seek back to the start of the column before the 
									// first visible column
									for (List<GListItem>::I it = Items.Start(FirstVisible); it.In(); it--)
									{
										GListItem *i = *it;
										if (i->d->LayoutColumn < HScroll->Value())
										{
											it++;
											break;
										}
									}
									
									// Now find the entry at the right height
								}
							}

							if (To)
							{
								Select(0);
								To->Select(true);
								To->ScrollTo();
							}
						}
					}
					Status = true;
					break;
				}
				case VK_RIGHT:
				{
					if (GetMode() == GListColumns)
					{
						GListItem *Hit = GetSelected();
						if (Hit)
						{
							GListItem *To = 0;
							int ToDist = 0x7fffffff;
							
							for (GListItem *i=Items[FirstVisible]; i && i->Pos.Valid(); i=Items.Next())
							{
								if (i->Pos.x1 > Hit->Pos.x2)
								{
									int Dx = i->Pos.x1 - Hit->Pos.x1;
									int Dy = i->Pos.y1 - Hit->Pos.y1;
									int IDist = Dx * Dx + Dy * Dy;
									if (!To || IDist < ToDist)
									{
										To = i;
										ToDist = IDist;
									}
								}
							}
							
							if (To)
							{
								Select(0);
								To->Select(true);
								To->ScrollTo();
							}
						}
					}
					Status = true;
					break;
				}
				case VK_PAGEUP:
				{
					GList_PageUp:
					int Vis = VisibleItems();
					Vis = max(Vis, 0);

					KeyScroll(Keyboard-Vis, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_PAGEDOWN:
				{
					GList_PageDown:
					int Vis = VisibleItems();
					Vis = max(Vis, 0);
					KeyScroll(Keyboard+Vis, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_END:
				{
					GList_End:
					printf("End handler\n");
					KeyScroll(Items.Length()-1, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_HOME:
				{
					GList_Home:
					KeyScroll(0, Keyboard, k.Shift());
					Status = true;
					break;
				}
				#ifdef VK_APPS
				case VK_APPS:
				{
					GListItem *s = GetSelected();
					if (s)
					{
						GRect *r = &s->Pos;
						if (r)
						{
							GMouse m;
							GListItem *FirstVisible = ItemAt((VScroll) ? VScroll->Value() : 0);

							m.x = 32 + ItemsPos.x1;
							m.y = r->y1 + (r->Y() >> 1) - (FirstVisible ? FirstVisible->Pos.y1 : 0) + ItemsPos.y1;
							
							m.Target = this;
							m.ViewCoords = true;
							m.Down(true);
							m.Right(true);
							
							OnMouseClick(m);
						}
					}
					break;
				}
				#endif
				default:
				{
					if
					(
						!Status
						&&
						k.IsChar
						&&
						(
							IsDigit(k.c16)
							||
							IsAlpha(k.c16)
							||
							strchr("_.-", k.c16)
						)
					)
					{
						int Now = LgiCurrentTime();
						GStringPipe p;
						if (d->KeyBuf && Now < d->KeyLast + 1500)
						{
							p.Push(d->KeyBuf);
						}
						DeleteArray(d->KeyBuf);
						d->KeyLast = Now;
						p.Push(&k.c16, 1);
						d->KeyBuf = p.NewStrW();
						if (d->KeyBuf)
						{
							char *c8 = LgiNewUtf16To8(d->KeyBuf);
							if (c8)
							{
								int Col = 0;
								bool Ascend = true;
								for (GListColumn *c = Columns.First(); c; c = Columns.Next())
								{
									if (c->Mark())
									{
										Col = Columns.IndexOf(c);
										if (c->Mark() == GLI_MARK_UP_ARROW)
										{
											Ascend = false;
										}
									}
								}
								
								bool Selected = false;
								List<GListItem>::I It = Ascend ? Items.Start() : Items.End();
								for (GListItem *i = *It; It.In(); i = Ascend ? *++It : *--It)
								{
									if (!Selected)
									{
										char *t = i->GetText(Col);
										if (t && stricmp(t, c8) >= 0)
										{
											i->Select(true);
											i->ScrollTo();
											Selected = true;
										}
										else
										{
											i->Select(false);
										}
									}
									else
									{
										i->Select(false);
									}
								}
								
								DeleteArray(c8);
							}
						}						
						Status = true;
					}
					break;
				}
			}
		}
	}

	return Status;
}

int GList::HitColumn(int x, int y, GListColumn *&Resize, GListColumn *&Over)
{
	int Index = -1;

	Resize = 0;
	Over = 0;

	if (ColumnHeaders &&
		ColumnHeader.Overlap(x, y))
	{
		// Clicked on a column heading
		int cx = ColumnHeader.x1 + ((IconCol) ? IconCol->Width() : 0);
		int n = 0;
		for (GListColumn *c = Columns.First(); c; c = Columns.Next(), n++)
		{
			cx += c->Width();
			if (abs(x-cx) < 5)
			{
				if (c->Resizable())
				{
					Resize = c;
					Index = n;
					break;
				}
			}
			else if (c->d->Pos.Overlap(x, y))
			{
				Over = c;
				Index = n;
				break;
			}
		}
	}

	return Index;
}

int GList::OnHitTest(int x, int y)
{
	GListColumn *Resize, *Over;
	HitColumn(x, y, Resize, Over);
	if (Resize)
	{
		SetCursor(LCUR_SizeHor);
		return 1;
	}

	return -1;
}

void GList::OnMouseClick(GMouse &m)
{
	if (Lock(_FL))
	{
		if (m.Down())
		{
			Focus(true);

			d->DragMode = DRAG_NONE;
			d->DragStart.x = m.x;
			d->DragStart.y = m.y;

			if (ColumnHeaders &&
				ColumnHeader.Overlap(m.x, m.y))
			{
				// Clicked on a column heading
				GListColumn *Resize, *Over;
				int Index = HitColumn(m.x, m.y, Resize, Over);

				if (Resize)
				{
					if (m.Double())
					{
						Resize->Width(Resize->GetContentSize() + DEFAULT_COLUMN_SPACING);
					}
					else
					{
						d->DragMode = RESIZE_COLUMN;
						d->DragData = Columns.IndexOf(Resize);
						Capture(true);
					}
				}
				else
				{
					d->DragMode = CLICK_COLUMN;
					d->DragData = Columns.IndexOf(Over);
					if (Over)
					{
						Over->d->Down = true;
						Invalidate(&Over->d->Pos);
						Capture(true);
					}
				}
			}
			else if (ItemsPos.Overlap(m.x, m.y))
			{
				// Clicked in the items area
				bool HandlerHung = false;
				int ItemIndex = -1;
				GListItem *Item = HitItem(m.x, m.y, &ItemIndex);
				GViewI *Notify = Item ? (GetNotify()) ? GetNotify() : GetParent() : 0;
				d->DragData = ItemIndex;

				if (Item && Item->Select())
				{
					// Click on selected item
					if (m.Modifier())
					{
						Item->Select(false);
						OnItemClick(Item, m);
					}
					else
					{
						// Could be drag'n'drop operation
						// Or just a select
						int64 StartHandler = LgiCurrentTime();

						// this will get set if 'this' is deleted.
						bool DeleteFlag = false;

						// Setup the delete flag pointer
						d->DeleteFlag = &DeleteFlag;

						// Do the event... may delete 'this' object, or hang for a long time
						OnItemClick(Item, m);

						// If the object has been deleted... exit out of here NOW!
						if (DeleteFlag)
						{
							return;
						}

						// Shut down the delete flag pointer... it'll point to invalid stack soon.
						d->DeleteFlag = 0;

						// Check if the handler hung for a long time...
						HandlerHung = LgiCurrentTime() - StartHandler > 200;
						if (!HandlerHung && !m.Double())
						{
							// Start d'n'd watcher pulse...
							SetPulse(100);
							Capture(true);
							d->DragMode = CLICK_ITEM;
						}
						
						if (!IsCapturing())
						{
							// If capture failed then we reset the dragmode...
							d->DragMode = DRAG_NONE;
						}
					}
				}
				else
				{
					// Selection change
					if (m.Shift() && MultiSelect())
					{
						int n = 0;
						int a = min(ItemIndex, Keyboard);
						int b = max(ItemIndex, Keyboard);
						GArray<GListItem*> Sel;

						ForAllItems(i)
						{
							bool s = n >= a && n <= b;
							if (i->d->Selected ^ s)
							{
								i->d->Selected = s;
								i->Update();
							}
							if (s) Sel.Add(i);
							n++;
						}

						OnItemSelect(Sel);
					}
					else
					{
						bool PostSelect = false;
						bool SelectionChanged = false;
						
						ForAllItems(i)
						{
							if (Item == i) // clicked item
							{
								if (m.Modifier())
								{
									// Toggle selected state
									if (!i->Select())
									{
										Keyboard = Items.IndexOf(i);
									}

									i->Select(!i->Select());
								}
								else
								{
									// Select this after we have delselected everything else
									PostSelect = true;
								}
							}
							else if (!m.Modifier() || !MultiSelect())
							{
								if (i->Select())
								{
									i->Select(false);
									SelectionChanged = true;
								}
							}
						}

						if (PostSelect)
						{
							Item->Select(true);
							Keyboard = Items.IndexOf(Item);
						}

						if (!m.Modifier() && Items.First())
						{
							d->DragMode = SELECT_ITEMS;
							SetPulse(100);
							Capture(true);
						}

						if (SelectionChanged)
						{
							SendNotify(GLIST_NOTIFY_SELECT);
						}
					}

					OnItemClick(Item, m);
				}

				if (!HandlerHung)
				{
					if (m.IsContextMenu())
						SendNotify(GLIST_NOTIFY_CONTEXT_MENU);
					else if (m.Double())
						SendNotify(GLIST_NOTIFY_DBL_CLICK);
					else
						SendNotify(GLIST_NOTIFY_CLICK);
				}
			}
		}
		else // Up Click
		{
			switch (d->DragMode)
			{
				case CLICK_COLUMN:
				{
					GListColumn *c = Columns.ItemAt(d->DragData);
					if (c)
					{
						c->d->Down = false;
						Invalidate(&c->d->Pos);

						if (c->d->Pos.Overlap(m.x, m.y))
						{
							OnColumnClick(Columns.IndexOf(c), m);
						}
					}
					else
					{
						OnColumnClick(-1, m);
					}
					break;
				}
				case CLICK_ITEM:
				{
					// This code allows the user to change a larger selection
					// down to a single item, by clicking on that item. This
					// can't be done on the down click because the user may also
					// be clicking the selected items to drag them somewhere and
					// if we de-selected all but the clicked item on the down
					// click they would never be able to drag and drop more than
					// one item.
					//
					// However we also do not want this to select items after the
					// contents of the list box have changed since the down click
					GListItem *Item = Items.ItemAt(d->DragData);
					if (Item)
					{
						bool Change = false;
						
						GArray<GListItem*> s;
						ForAllItems(i)
						{
							bool Sel = Item == i;
							if (Sel ^ i->Select())
							{
								Change = true;
								i->Select(Sel);
								if (Sel)
								{
									s.Add(i);
								}
							}
						}
						
						if (Change)
							OnItemSelect(s);
					}
					break;
				}
				case DRAG_COLUMN:
				{
					// End column drag
					if (d->DragCol)
					{
						GRect DragPos = d->DragCol->GetPos();
						GdcPt2 p(DragPos.x1 + (DragPos.X()/2), 0);
						PointToView(p);

						int OldIndex = d->DragCol->GetIndex();
						int Best = 100000000, NewIndex = OldIndex, i=0, delta;
						GListColumn *Col = 0;
						for (Col = Columns.First(); Col; Col = Columns.Next(), i++)
						{
							delta = abs(p.x - Col->d->Pos.x1);
							if (delta < Best)
							{
								Best = delta;
								NewIndex = i - (i > OldIndex ? 1 : 0);
							}
						}
						delta = abs(p.x - Columns.Last()->d->Pos.x2);
						if (delta < Best)
						{
							NewIndex = i;
						}

						Col = d->DragCol->GetColumn();
						if (OldIndex != NewIndex &&
							OnColumnReindex(Col, OldIndex, NewIndex))
						{
							Columns.Delete(Col);
							Columns.Insert(Col, NewIndex);
							UpdateAllItems();
						}

						DeleteObj(d->DragCol);
					}

					Invalidate();
					break;
				}
			}

			GListItem *Item = HitItem(m.x, m.y);
			if (Item)
			{
				OnItemClick(Item, m);
			}

			if (IsCapturing())
			{
				Capture(false);
			}

			d->DragMode = DRAG_NONE;
		}

		Unlock();
	}
}

void GList::OnPulse()
{
	if (Lock(_FL))
	{
		if (IsCapturing())
		{
			GMouse m;
			GetMouse(m);

			if (m.y < 0 || m.y >= Y())
			{
				switch (d->DragMode)
				{
					case SELECT_ITEMS:
					{
						int OverIndex = 0;
						GListItem *Over = 0;

						if (m.y < 0)
						{
							int Space = -m.y;

							int n = FirstVisible - 1;
							for (GListItem *i = Items[n]; i; i=Items.Prev(), n--)
							{
								GMeasureInfo Info;
								i->OnMeasure(&Info);
								if (Space > Info.y)
								{
									Space -= Info.y;
								}
								else
								{
									OverIndex = n;
									Over = i;
									break;
								}
							}

							if (!Over)
							{
								Over = Items.First();
								OverIndex = 0;
							}
						}
						else if (m.y >= Y())
						{
							int Space = m.y - Y();
							int n = LastVisible + 1;
							for (GListItem *i = Items[n]; i; i=Items.Next(), n++)
							{
								GMeasureInfo Info;
								i->OnMeasure(&Info);
								if (Space > Info.y)
								{
									Space -= Info.y;
								}
								else
								{
									OverIndex = n;
									Over = i;
									break;
								}
							}

							if (!Over)
							{
								Over = Items.Last();
								OverIndex = Items.Length()-1;
							}
						}

						int Min = min(d->DragData, OverIndex);
						int Max = max(d->DragData, OverIndex);
						int n = Min;
						for (GListItem *i = Items[Min]; i && n <= Max; i=Items.Next(), n++)
						{
							i->Select(true);
						}

						if (Over)
						{
							Over->ScrollTo();
						}
						break;
					}
				}
			}
		}
		else
		{
			d->DragMode = DRAG_NONE;
			SetPulse();
		}

		Unlock();
	}
}

void GList::OnMouseMove(GMouse &m)
{
	if (Lock(_FL))
	{
		switch (d->DragMode)
		{
			case DRAG_COLUMN:
			{
				if (d->DragCol)
				{
					GdcPt2 p;
					PointToScreen(p);

					GRect r = d->DragCol->GetPos();
					r.Offset(-p.x, -p.y); // to view co-ord

					r.Offset(m.x - d->DragCol->GetOffset() - r.x1, 0);
					if (r.x1 < 0) r.Offset(-r.x1, 0);
					if (r.x2 > X()-1) r.Offset((X()-1)-r.x2, 0);

					r.Offset(p.x, p.y); // back to screen co-ord
					d->DragCol->SetPos(r, true);
					r = d->DragCol->GetPos();
				}
				break;
			}
			case RESIZE_COLUMN:
			{
				GListColumn *c = Columns.ItemAt(d->DragData);
				if (c)
				{
					int OldWidth = c->Width();
					int NewWidth = m.x - c->d->Pos.x1;

					c->Width(max(NewWidth, 4));
				}
				break;
			}
			case CLICK_COLUMN:
			{
				bool Update = false;
				GListColumn *c = Columns.ItemAt(d->DragData);
				if (c)
				{
					if (abs(m.x - d->DragStart.x) > DRAG_THRESHOLD ||
						abs(m.y - d->DragStart.y) > DRAG_THRESHOLD)
					{
						OnColumnDrag(d->DragData, m);
					}
					else
					{
						bool Over = c->d->Pos.Overlap(m.x, m.y);
						if (m.Down() && Over != c->d->Down)
						{
							c->d->Down = Over;
							Invalidate(&c->d->Pos);
						}
					}
				}
				break;
			}
			case SELECT_ITEMS:
			{
				int n=0;
				bool Selected = m.y < ItemsPos.y1;

				if (IsCapturing())
				{
					if (MultiItemSelect)
					{
						int Over = -1;
						
						GListItem *h = HitItem(m.x, m.y, &Over);

						/*
						if (m.y < ItemsPos.y1 && FirstVisible == 0)
						{
							Over = 0;
						}
						else
						{
							Iterator<GListItem> Iter(&Items);
							int n = FirstVisible;
							for (GListItem *k=Iter[n]; k && k->OnScreen(); k=Iter.Next())
							{
								if ((m.y >= k->Pos.y1) && (m.y <= k->Pos.y2))
								{
									Over = n;
									break;
								}
								n++;
							}
						}
						*/

						if (Over >= 0)
						{
							n = 0;

							int Start = min(Over, d->DragData);
							int End = max(Over, d->DragData);

							ForAllItems(i)
							{
								i->Select(n >= Start && n <= End);
								n++;
							}
						}
					}
					else
					{
						ForAllItems(i)
						{
							// i->Select(m.y >= i->Pos.y1 && m.y <= i->Pos.y2);
							i->Select(i->Pos.Overlap(m.x, m.y));
						}
					}
				}
				break;
			}
			case CLICK_ITEM:
			{
				GListItem *Cur = Items.ItemAt(d->DragData);
				if (Cur)
				{
					Cur->OnMouseMove(m);
					
					if (IsCapturing() &&
						abs(d->DragStart.x-m.x) > DRAG_THRESHOLD ||
						abs(d->DragStart.y-m.y) > DRAG_THRESHOLD)
					{
						OnItemBeginDrag(Cur, m);
						d->DragMode = DRAG_NONE;
						Capture(false);
					}
				}
				break;
			}
			default:
			{
				#ifndef WIN32
				GListColumn *Resize, *Over;
				HitColumn(m.x, m.y, Resize, Over);
				if (Resize)
				{
					SetCursor(LCUR_SizeHor);
				}
				#endif
				
				List<GListItem> s;
				if (GetSelection(s))
				{
					for (GListItem *c=s.First(); c; c=s.Next())
					{
						GMouse ms = m;
						ms.x -= c->Pos.x1;
						ms.y -= c->Pos.y1;
						c->OnMouseMove(ms);
					}
				}
				break;
			}
		}

		Unlock();
	}
}

int64 GList::Value()
{
	int n=0;
	ForAllItems(i)
	{
		if (i->Select())
		{
			return n;
		}
		n++;
	}
	return -1;
}

void GList::Value(int64 Index)
{
	int n=0;
	ForAllItems(i)
	{
		if (n == Index)
		{
			i->Select(true);
			Keyboard = n;
		}
		else
		{
			i->Select(false);
		}
		n++;
	}
}

void GList::SelectAll()
{
	if (Lock(_FL))
	{
		ForAllItems(i)
		{
			i->d->Selected = true;
		}

		Unlock();

		Invalidate();
	}
}

bool GList::Select(GListItem *Obj)
{
	bool Status = false;
	ForAllItems(i)
	{
		i->Select(Obj == i);
		if (Obj == i) Status = true;
	}
	return true;
}

GListItem *GList::GetSelected()
{
	GListItem *n = 0;

	if (Lock(_FL))
	{
		ForAllItems(i)
		{
			if (i->Select())
			{
				n = i;
				break;
			}
		}

		Unlock();
	}

	return n;
}

bool GList::GetUpdateRegion(GListItem *i, GRegion &r)
{
	r.Empty();

	if (d->Mode == GListDetails)
	{
		if (i->Pos.Valid())
		{
			GRect u = i->Pos;
			u.y2 = ItemsPos.y2;
			r.Union(&u);
			
			return true;
		}
	}
	else if (d->Mode == GListColumns)
	{
		if (i->Pos.Valid())
		{
			GRect u = i->Pos;
			u.y2 = ItemsPos.y2;
			r.Union(&u);
			
			u.x1 = u.x2 + 1;
			u.y1 = ItemsPos.y1;
			r.Union(&u);
			
			return true;
		}
	}

	return false;
}

bool GList::Insert(GListItem *i, int Index, bool Update)
{
	#if 1
	List<GListItem> l;
	l.Insert(i);
	return Insert(l, Index, Update);
	#else
	bool Status = false;
	if (i && Lock(_FL))
	{
		// Insert
		if (i->Parent != this)
		{
			bool First = Items.Length() == 0;
			
			i->Parent = this;
			i->Select(false);
			Items.Insert(i, Index);
			i->OnInsert();

			if (First)
			{
				Keyboard = 0;
				i->Select(true);
			}
			
			if (Update)
			{
				// Update screen
				Pour();
				GRegion Up;
				if (GetUpdateRegion(i, Up))
				{
					Up.y2 = Y();
					Invalidate(&Up);
				}

				// Notify
				SendNotify(GLIST_NOTIFY_INSERT);
			}
		}

		Status = true;

		Unlock();
	}
	return Status;
	#endif
}

bool GList::Insert(List<GListItem> &l, int Index, bool Update)
{
	bool Status = false;

	if (Lock(_FL))
	{
		bool First = Items.Length() == 0;
		
		// Insert list of items
		for (GListItem *i=l.First(); i; i=l.Next())
		{
			if (i->Parent != this)
			{
				i->Parent = this;
				i->Select(false);
				Items.Insert(i, Index);
				i->OnInsert();
				if (Index >= 0) Index++;
				
				if (First)
				{
					First = false;
					Keyboard = 0;
					i->Select(true);
				}
			}
		}

		if (Update)
		{
			// Update screen
			Pour();
			Invalidate();

			// Notify
			SendNotify(GLIST_NOTIFY_INSERT);
		}

		Status = true;

		Unlock();
	}

	return Status;
}

bool GList::Delete()
{
	return Delete(Items.Current());
}

bool GList::Delete(int Index)
{
	return Delete(Items.ItemAt(Index));
}

bool GList::Delete(GListItem *i)
{
	bool Status = false;
	if (Lock(_FL))
	{
		if (Remove(i))
		{
			// Delete
			DeleteObj(i);
			Status = true;
		}
		Unlock();
	}

	return Status;
}

bool GList::Remove(GListItem *i)
{
	bool Status = false;

	if (Lock(_FL))
	{
		if (i && i->GetList() == this)
		{
			GRegion Up;
			bool Visible = GetUpdateRegion(i, Up);
			bool Selected = i->Select();
			int Index = Items.IndexOf(i);
			int64 Pos = (VScroll) ? VScroll->Value() : 0;

			// Remove from list
			Items.Delete(i);
			i->OnRemove();
			i->Parent = 0;
			UpdateScrollBars();

			// Update screen
			if ((VScroll && VScroll->Value() != Pos) ||
				Index < FirstVisible)
			{
				Invalidate(&ItemsPos);
			}
			else if (Visible)
			{
				Up.y2 = ItemsPos.y2;
				Invalidate(&Up);
			}

			// Notify
			GViewI *Note = (GetNotify())?GetNotify():GetParent();
			if (Note)
			{
				if (Selected)
				{
					GArray<GListItem*> s;
					OnItemSelect(s);
				}

				Note->OnNotify(this, GLIST_NOTIFY_DELETE);
			}

			Status = true;
		}

		Unlock();
	}

	return Status;
}

bool GList::HasItem(GListItem *Obj)
{
	return Items.HasItem(Obj);
}

int GList::IndexOf(GListItem *Obj)
{
	return Items.IndexOf(Obj);
}

GListItem *GList::ItemAt(int Index)
{
	return Items.ItemAt(Index);
}

void GList::ScrollToSelection()
{
	if (VScroll)
	{
		int n=0;
		int Vis = VisibleItems();
		ForAllItems(i)
		{
			if (i->Select())
			{
				if (n < FirstVisible || n > LastVisible)
				{
					int k = n - (Vis/2);
					VScroll->Value(max(k, 0));
					Invalidate(&ItemsPos);
					break;
				}
			}
			n++;
		}
	}
}

void GList::Sort(GListCompareFunc Compare, NativeInt Data)
{
	if (Lock(_FL))
	{
		if (Compare)
		{
			Items.Sort(Compare, Data);
			Invalidate(&ItemsPos);
		}

		Unlock();
	}
}

void GList::Empty()
{
	if (Lock(_FL))
	{
		ForAllItems(i)
		{
			LgiAssert(i->Parent == this);
			i->Parent = 0;
			DeleteObj(i);
		}
		Items.Empty();
		
		FirstVisible = LastVisible = -1;
		d->DragMode = DRAG_NONE;

		if (VScroll)
		{
			VScroll->Value(0);
			VScroll->SetLimits(0, -1);
		}

		Invalidate();
		DeleteArray(d->KeyBuf);

		Unlock();
	}
}

void GList::RemoveAll()
{
	if (Lock(_FL))
	{
		if (Items.Length())
		{
			GArray<GListItem*> s;
			OnItemSelect(s);
		}

		for (GListItem *i = Items.First(); i; i = Items.Next())
		{
			i->OnRemove();
			i->Parent = 0;
		}
		Items.Empty();
		FirstVisible = LastVisible = -1;
		d->DragMode = DRAG_NONE;

		if (VScroll)
		{
			// these have to be in this order because
			// "SetLimits" can cause the VScroll object to
			// be deleted and becoming NULL
			VScroll->Value(0);
			VScroll->SetLimits(0, -1);
		}

		Invalidate();
		DeleteArray(d->KeyBuf);
		Unlock();
	}
}

void GList::OnPosChange()
{
	GLayout::OnPosChange();
}

void GList::UpdateScrollBars()
{
	static bool Processing = false;
	
	if (!Processing)
	{
		Processing = true;
		
		if (VScroll)
		{
			int Vis = VisibleItems();
			int Max = MaxScroll();

			if (VScroll->Value() > max(Max, 0))
			{
				VScroll->Value(Max);
			}

			VScroll->SetPage(Vis);
			VScroll->SetLimits(0, Items.Length() - 1);
		}
		
		if (HScroll)
		{
			HScroll->SetPage(d->VisibleColumns);
			HScroll->SetLimits(0, d->Columns - 1);
		}
		
		Processing = false;
	}
}

void GList::Pour()
{
	#if GLIST_POUR_PROFILE
	int Start = LgiCurrentTime();
	#endif

	// Layout all the elements
	GRect Client = GetClient();

	if (d->Mode == GListDetails)
	{
		if (ColumnHeaders)
		{
			ColumnHeader = Client;
			ColumnHeader.y2 = ColumnHeader.y1 + SysFont->GetHeight() + 4;
			ItemsPos = Client;
			ItemsPos.y1 = ColumnHeader.y2 + 1;
		}
		else
		{
			ItemsPos = Client;
		}
		
		int n = 0;
		int y = ItemsPos.y1;
		int Max = MaxScroll();
		FirstVisible = (VScroll) ? VScroll->Value() : 0;
		if (FirstVisible < 0) FirstVisible = 0;
		if (FirstVisible > Max) FirstVisible = Max;
		LastVisible = 0x7FFFFFFF;
		CompletelyVisible = 0;
		bool SomeHidden = false;

		if (Items.Length() == 4)
		{
			int asd=0;
		}

		ForAllItems(i)
		{
			if (n < FirstVisible || n > LastVisible)
			{
				i->Pos.Set(-1, -1, -2, -2);
				SomeHidden = true;
			}
			else
			{
				GMeasureInfo Info;
				
				i->OnMeasure(&Info);
				if (i->Pos.Valid() && Info.y != i->Pos.Y())
				{
					// This detects changes in item height and invalidates the items below this one.
					GRect in(0, y+Info.y, X()-1, Y()-1);
					Invalidate(&in);
				}

				i->Pos.Set(ItemsPos.x1, y, ItemsPos.x2, y+Info.y-1);
				y = y+Info.y;

				if (i->Pos.y2 > ItemsPos.y2)
				{
					LastVisible = n;
					SomeHidden = true;
				}
				else
				{
					CompletelyVisible++;
				}
			}

			n++;
		}

		if (LastVisible >= Items.Length())
		{
			LastVisible = Items.Length() - 1;
		}

		SetScrollBars(false, SomeHidden);
		UpdateScrollBars();
	}
	else if (d->Mode == GListColumns)
	{
		ColumnHeader.ZOff(-1, -1);
		ItemsPos = Client;
		FirstVisible = 0;

		int CurX = 0;
		int CurY = 0;
		int MaxX = 16;
		GArray<GListItem*> Col;
		d->Columns = 1;
		d->VisibleColumns = 0;
		int64 ScrollX = HScroll ? HScroll->Value() : 0;
		int64 OffsetY = HScroll ? 0 : GScrollBar::GetScrollSize();
		FirstVisible = -1;
		
		int n = 0;
		ForAllItems(i)
		{
			GMeasureInfo Info;
			i->OnMeasure(&Info);

			if (d->Columns <= ScrollX ||
				CurX > ItemsPos.X())
			{
				i->Pos.ZOff(-1, -1);
				i->d->LayoutColumn = d->Columns;
				
				if (ItemsPos.y1 + CurY + Info.y > ItemsPos.y2 - OffsetY)
				{
					CurY = 0;
					d->Columns++;
					
					if (d->Columns > ScrollX &&
						CurX < ItemsPos.X())
					{
						goto FlowItem;
					}
				}
			}
			else
			{
				FlowItem:
				if (ItemsPos.y1 + CurY + Info.y > ItemsPos.y2 - OffsetY)
				{
					// wrap to next column
					for (int n=0; n<Col.Length(); n++)
					{
						Col[n]->Pos.x2 = CurX + MaxX - 1;
					}
					Col.Length(0);
					
					CurX += MaxX;
					CurY = 0;
					d->Columns++;
					if (CurX < ItemsPos.X())
					{
						d->VisibleColumns++;
					}
				}
				
				if (FirstVisible < 0)
					FirstVisible = n;
				LastVisible = n;
				
				i->d->LayoutColumn = d->Columns;
				i->Pos.ZOff(Info.x-1, Info.y-1);
				i->Pos.Offset(ItemsPos.x1 + CurX, ItemsPos.y1 + CurY);
				Col[Col.Length()] = i;
				MaxX = max(MaxX, Info.x);
				
				CompletelyVisible++;
			}

			CurY += Info.y;
			n++;
		}
		
		d->VisibleColumns = max(1, d->VisibleColumns);

		// pour remaining items...
		for (n=0; n<Col.Length(); n++)
		{
			Col[n]->Pos.x2 = CurX + MaxX - 1;
		}
		Col.Length(0);
		if (CurX + MaxX < ItemsPos.X())
		{
			d->VisibleColumns++;
		}

		// printf("%u - ScrollX=%i VisCol=%i Cols=%i\n", (uint32)LgiCurrentTime(), ScrollX, d->VisibleColumns, d->Columns);
		SetScrollBars(d->VisibleColumns < d->Columns, false);
		UpdateScrollBars();
	}
	
	#if GLIST_POUR_PROFILE
	printf("GList::Pour() %i ms\n", LgiCurrentTime() - Start);
	#endif
}

void GList::OnPaint(GSurface *pDC)
{
	#if GLIST_ONPAINT_PROFILE
	int Start = LgiCurrentTime(), t1, t2, t3, t4, t5;
	#endif
	
	if (Lock(_FL))
	{
		COLOUR Back = Enabled() ? LC_WORKSPACE : LC_MED;
		Pour();
		
		#if GLIST_ONPAINT_PROFILE
		t1 = LgiCurrentTime();
		#endif

		// Check icon column status then draw
		if (AskImage() && !IconCol)
		{
			IconCol = new GListColumn(this, 0, 18);
			if (IconCol)
			{
				IconCol->Resizable(false);
				IconCol->Type(GIC_ASK_IMAGE);
			}
		}
		else if (!AskImage() && IconCol) DeleteObj(IconCol);

		// Draw column headings
		if (ColumnHeaders && ColumnHeader.Valid())
		{
			// Draw columns
			GRect cr = ColumnHeader;
			int cx = cr.x1;
			pDC->ClipRgn(&cr);

			if (IconCol)
			{
				cr.x1 = cx;
				cr.x2 = cr.x1 + IconCol->Width() - 1;
				IconCol->d->Pos = cr;
				IconCol->OnPaint(pDC, cr);
				cx += IconCol->Width();
			}

			// Draw other columns
			for (GListColumn *c = Columns.First(); c; c = Columns.Next())
			{
				cr.x1 = cx;
				cr.x2 = cr.x1 + c->Width() - 1;
				c->d->Pos = cr;
				c->OnPaint(pDC, cr);
				cx += c->Width();
			}

			// Draw ending peice
			cr.x1 = cx;
			cr.x2 = ColumnHeader.x2 + 2;

			if (cr.Valid())
			{
				#ifdef MAC
				GArray<GColourStop> Stops;
				GRect j(cr.x1, cr.y1, cr.x2-1, cr.y2-1); 
				// pDC->Colour(Rgb24(160, 160, 160), 24);
				// pDC->Line(r.x1, r.y1, r.x2, r.y1);
					
				Stops[0].Pos = 0.0;
				Stops[0].Colour = Rgb32(255, 255, 255);
				Stops[1].Pos = 0.5;
				Stops[1].Colour = Rgb32(241, 241, 241);
				Stops[2].Pos = 0.51;
				Stops[2].Colour = Rgb32(233, 233, 233);
				Stops[3].Pos = 1.0;
				Stops[3].Colour = Rgb32(255, 255, 255);
				
				LgiFillGradient(pDC, j, true, Stops);
				
				pDC->Colour(Rgb24(178, 178, 178), 24);
				pDC->Line(cr.x1, cr.y2, cr.x2, cr.y2);
				#else
				if (GApp::SkinEngine)
				{
					GSkinState State;
					State.pScreen = pDC;
					State.Rect = cr;
					State.Enabled = Enabled();
					GApp::SkinEngine->OnPaint_ListColumn(0, 0, &State);
				}
				else
				{
					LgiWideBorder(pDC, cr, RAISED);
					pDC->Colour(LC_MED, 24);
					pDC->Rectangle(&cr);
				}
				#endif
			}

			pDC->ClipRgn(0);
		}

		#if GLIST_ONPAINT_PROFILE
		t2 = LgiCurrentTime();
		#endif

		// Draw items
		if (!Buf)
		{
			Buf = new GMemDC;
		}

		GRect r = ItemsPos;
		int n = FirstVisible;
		int LastY = r.y1;
		GViewFill *Fill;
		COLOUR SelFore = Focus() ? LC_FOCUS_SEL_FORE : LC_NON_FOCUS_SEL_FORE;
		COLOUR SelBack = Focus() ? LC_FOCUS_SEL_BACK : (Enabled() ? LC_NON_FOCUS_SEL_BACK : LC_MED);
		int LastSelected = -1;

		GItem::ItemPaintCtx Ctx;
		Ctx.pDC = pDC;

		GRegion Rgn(ItemsPos);
		for (GListItem *i = Items.ItemAt(n); i; i = Items.Next(), n++)
		{
			if (i->Pos.Valid())
			{
				// Setup painting colours in the context
				if (LastSelected ^ (int)i->Select())
				{
					if (LastSelected = i->Select())
					{
						Ctx.Fore = SelFore;
						Ctx.Back = SelBack;
					}
					else
					{
						Ctx.Fore = LC_TEXT;
						Ctx.Back = Back;
					}
				}

				if (Fill = i->GetForegroundFill())
				{
					Ctx.Fore = Fill->GetFlat().c24();
					LastSelected = -1;
				}
				if (Fill = i->GetBackgroundFill())
				{
					Ctx.Fore = Fill->GetFlat().c24();
					LastSelected = -1;
				}


				// tell the item what colour to use
				#if DOUBLE_BUFFER_PAINT
				if (Buf->X() < i->Pos.X() ||
					Buf->Y() < i->Pos.Y())
				{
					Buf->Create(i->Pos.X(), i->Pos.Y(), GdcD->GetBits());
				}

				Ctx = i->Pos;
				Ctx.r.Offset(-Ctx.r.x1, -Ctx.r.y1);
				i->OnPaint(Ctx);
				pDC->Blt(i->Pos.x1, i->Pos.y1, Buf, &Ctx.r);
				#else
				(GRect&)Ctx = i->Pos;
				i->OnPaint(Ctx);
				#endif
				Rgn.Subtract(&i->Pos);

				LastY = i->Pos.y2 + 1;
			}
			else
			{
				break;
			}
		}
		
		pDC->Colour(Back, 24);
		for (GRect *w=Rgn.First(); w; w=Rgn.Next())
		{
			pDC->Rectangle(w);
		}

		Unlock();
	}

	#if GLIST_ONPAINT_PROFILE
	int64 End = LgiCurrentTime();
	printf("GList::OnPaint() pour=%i headers=%i items=%i\n", (int) (t1-Start), (int) (t2-t1), (int) (End-t2));
	#endif
}

void GList::OnFocus(bool b)
{
	GListItem *s = GetSelected();
	if (!s)
	{
		s = Items.First();
		if (s)
		{
			s->Select(true);
		}
	}

	for (GListItem *i = Items.ItemAt(FirstVisible); i; i = Items.Next())
	{
		if (i->Pos.Valid() &&
			i->d->Selected)
		{
			Invalidate(&i->Pos);
		}
	}

	GLayout::OnFocus(b);
	
	if (!b && IsCapturing())
	{
		Capture(false);
	}
}

void GList::UpdateAllItems()
{
    if (Lock(_FL))
    {
	    ForAllItems(i)
	    {
		    i->d->EmptyDisplay();
	    }
	    Unlock();
	}
	
	Invalidate();
}

void GList::OnColumnClick(int Col, GMouse &m)
{
	d->ColClick = Col;
	d->ColMouse = m;
	SendNotify(GLIST_NOTIFY_COLS_CLICK);
}

bool GList::GetColumnClickInfo(int &Col, GMouse &m)
{
	if (d->ColClick >= 0)
	{
		Col = d->ColClick;
		m = d->ColMouse;
		return true;
	}
	
	return false;	
}

void GList::ResizeColumnsToContent(int Border)
{
	if (Lock(_FL))
	{
		for (GListColumn *c=Columns.First(); c; c=Columns.Next())
		{
			if (c->Resizable())
			{
				c->Width(c->GetContentSize() + Border);
			}
		}
		Unlock();
	}

	Invalidate();
}
