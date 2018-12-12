/*hdr
**      FILE:           LList.cpp
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
#include "LList.h"
#include "GScrollBar.h"
#include "GDisplayString.h"
#include "LgiRes.h"

// Debug defines
#define DEBUG_EDIT_LABEL				1

// Number of pixels you have to move the mouse until a drag is initiated.
#define DRAG_THRESHOLD					4

// Switches for various profiling code..
#define LList_POUR_PROFILE				0
#define LList_ONPAINT_PROFILE			0

// Options
#define DOUBLE_BUFFER_PAINT				0
#define DOUBLE_BUFFER_COLUMN_DRAWING	0

#define ForAllItems(Var)				for (auto Var : Items)
#define ForAllItemsReverse(Var)			Iterator<LListItem> ItemIter(&Items); for (LListItem *Var = ItemIter.Last(); Var; Var = ItemIter.Prev())
#define VisibleItems()					CompletelyVisible // (LastVisible - FirstVisible + 1)
#define MaxScroll()						MAX((int)Items.Length() - CompletelyVisible, 0)

class LListPrivate
{
public:
	// Mode
	LListMode Mode;
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
	GdcPt2 DragStart;
	int DragData;
	
	// Kayboard search
	uint64 KeyLast;
	char16 *KeyBuf;
	
	// Class
	LListPrivate()
	{
		DragData = 0;
		KeyBuf = 0;
		DeleteFlag = 0;
		Columns = 0;
		VisibleColumns = 0;
		Mode = LListDetails;
		NoSelectEvent = false;
	}
	
	~LListPrivate()
	{
		if (DeleteFlag)
			*DeleteFlag = true;
		DeleteArray(KeyBuf);
	}
};

class LListItemPrivate
{
public:
	bool Selected;
	int ListItem_Image;
	List<LListItemColumn> Cols;
	GArray<char*> Str;
	GArray<GDisplayString*> Display;
	int16 LayoutColumn;

	LListItemPrivate()
	{
		Selected = 0;
		ListItem_Image = -1;
		LayoutColumn = -1;
	}

	~LListItemPrivate()
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

////////////////////////////////////////////////////////////////////////////////////////////
LListItemColumn::LListItemColumn(LListItem *item, int col)
{
	_Column = col;
	_Item = item;
	_Value = 0;
	_Item->d->Cols.Insert(this);
}

LList *LListItemColumn::GetList()
{
	return _Item ? _Item->Parent : 0;
}

GItemContainer *LListItemColumn::GetContainer()
{
	return GetList();
}

List<LListItem> *LListItemColumn::GetAllItems()
{
	return GetList() ? &GetList()->Items : 0;
}

void LListItemColumn::Value(int64 i)
{
	if (i != _Value)
	{
		_Value = i;
		_Item->OnColumnNotify(_Column, _Value);
	}
}

LListItemColumn *LListItemColumn::GetItemCol(LListItem *i, int Col)
{
	if (i)
	{
		for (LListItemColumn *c=i->d->Cols.First(); c; c=i->d->Cols.Next())
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
LListItem::LListItem()
{
	d = new LListItemPrivate;
	Pos.ZOff(-1, -1);
	Parent = 0;
}

LListItem::~LListItem()
{
	if (Parent)
	{
		Parent->Remove(this);
	}

	DeleteObj(d);
}

void LListItem::SetImage(int i)
{
	d->ListItem_Image = i;
}

int LListItem::GetImage(int Flags)
{
	return d->ListItem_Image;
}

GItemContainer *LListItem::GetContainer()
{
	return Parent;
}

List<LListItemColumn> *LListItem::GetItemCols()
{
	return &d->Cols;
}

/*
Calling this to store your data is optional. Just override the
"GetText" function to return your own data to avoid duplication
in memory.
*/
bool LListItem::SetText(const char *s, int i)
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
char *LListItem::GetText(int i)
{
	return d->Str[i];
}

bool LListItem::Select()
{
	return d->Selected;
}

GRect *LListItem::GetPos(int Col)
{
	static GRect r;

	r = Pos;

	if (Parent->GetMode() == LListDetails)
	{
		if (Col >= 0)
		{
			GItemColumn *Column = 0;

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

void LListItem::Select(bool b)
{
	if (d->Selected != b)
	{
		d->Selected = b;
		Update();
		
		if (Parent &&
			d->Selected &&
			!Parent->d->NoSelectEvent)
		{
			GArray<LListItem*> Items;
			Items.Add(this);
			Parent->OnItemSelect(Items);
		}
	}
}

void LListItem::ScrollTo()
{
	if (Parent)
	{
		if (Parent->GetMode() == LListDetails && Parent->VScroll)
		{
			ssize_t n = Parent->Items.IndexOf(this);
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
		else if (Parent->GetMode() == LListColumns && Parent->HScroll)
		{
			ssize_t n = Parent->Items.IndexOf(this);
			if (n < Parent->FirstVisible)
			{
				Parent->HScroll->Value(d->LayoutColumn);
				Parent->Invalidate(&Parent->ItemsPos);
			}
			else if (n >= Parent->LastVisible)
			{
				ssize_t Range = Parent->HScroll->Page();
				
				Parent->HScroll->Value(d->LayoutColumn - Range);
				Parent->Invalidate(&Parent->ItemsPos);
			}
		}
	}
}

void LListItem::Update()
{
	if (Parent)
	{
		if (Parent->Lock(_FL))
		{
			d->EmptyDisplay();

			GdcPt2 Info;
			OnMeasure(&Info);

			GRect r = Pos;
			if (r.Valid())
			{
				if (Info.y != r.Y())
				{
					Pos.y2 = Pos.y1 + Info.y - 1;
					Parent->PourAll();

					r.y1 = MIN(r.y1, Pos.y1);
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

void LListItem::OnMeasure(GdcPt2 *Info)
{
	if (Info)
	{
		if (Parent &&
			Parent->GetMode() == LListDetails)
		{
			Info->x = 1024;
		}
		else
		{
			GDisplayString *s = GetDs(0);
			Info->x = 22 + (s ? s->X() : 0);
		}
		
		GFont *f = Parent ? Parent->GetFont() : SysFont;
		Info->y = MAX(16, f->GetHeight() + 2); // the default height
	}
}

bool LListItem::GridLines()
{
	return (Parent) ? Parent->GridLines : false;
}

void LListItem::OnMouseClick(GMouse &m)
{
	int Col = Parent->ColumnAtX(m.x);
	for (LListItemColumn *h = d->Cols.First(); h; h=d->Cols.Next())
	{
		if (Col == h->GetColumn())
		{
			h->OnMouseClick(m);
		}
	}
}

GDisplayString *LListItem::GetDs(int Col, int FitTo)
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

void LListItem::ClearDs(int Col)
{
	if (Col >= 0)
	{
		DeleteObj(d->Display[Col]);
	}
	else
	{
		d->Display.DeleteObjects();
	}
}

void LListItem::OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c)
{
	GSurface *&pDC = Ctx.pDC;
	if (pDC && c)
	{
		GRect ng = Ctx; // non-grid area

		if (c->InDrag())
		{
			pDC->Colour(DragColumnColour, 24);
			pDC->Rectangle(&ng);
		}
		else
		{
			GColour Background = Ctx.Back;

			if (Parent->GetMode() == LListDetails &&
				c->Mark() &&
				!d->Selected)
			{
				Background = GdcMixColour(GColour(0, 24), Background, (double)1/32);
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
					Ds->GetFont()->Colour(Ctx.Fore, Background);
					
					switch (Ctx.Align.Type)
					{
						case GCss::AlignCenter:
							Ds->Draw(pDC, ng.x1+((ng.X()-Ds->X())/2), ng.y1+1, &ng);
							break;
						case GCss::AlignRight:
							Ds->Draw(pDC, ng.x2-Ds->X()-1, ng.y1+1, &ng);
							break;
						default: // Left or inherit
							Ds->Draw(pDC, ng.x1+1, ng.y1+1, &ng);
							break;
					}
				}
				else
				{
					pDC->Colour(Background);
					pDC->Rectangle(&ng);
				}
			}
			else
			{
				pDC->Colour(Background);
				pDC->Rectangle(&ng);

				if (c->Type() == GIC_ASK_IMAGE &&
					Parent->GetImageList())
				{
					int Img = GetImage();
					if (Img >= 0)
					{
					    int CenterY = Ctx.y1 + ((Ctx.Y() - Parent->GetImageList()->TileY()) >> 1);
					    LgiAssert(CenterY >= 0);
					    
						Parent->GetImageList()->Draw(pDC, Ctx.x1+1, CenterY, Img, Background);
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

void LListItem::OnPaint(GItem::ItemPaintCtx &Ctx)
{
	if (!Parent)
	    return;

	int x = Ctx.x1;

	if (GetCss())
	{
		GCss::ColorDef Fill = GetCss()->Color();
		if (Fill.Type == GCss::ColorRgb)
			Ctx.Fore.Set(Fill.Rgb32, 32);
		
		if (!Select())
		{
			Fill = GetCss()->BackgroundColor();
			if (Fill.Type == GCss::ColorRgb)
				Ctx.Back.Set(Fill.Rgb32, 32);
		}
	}

	// Icon?
	if (Parent->IconCol)
	{
		GItem::ItemPaintCtx IcoCtx = Ctx;
		IcoCtx.Set(x, Ctx.y1, x + Parent->IconCol->Width()-1, Ctx.y2);

		// draw icon
		OnPaintColumn(IcoCtx, -1, Parent->IconCol);
		x = IcoCtx.x2 + 1;
	}
	
	// draw columns
	LListItemColumn *h = d->Cols.First();
	GItem::ItemPaintCtx ColCtx = Ctx;
	
	for (int i=0; i<Parent->Columns.Length(); i++)
	{
		GItemColumn *c = Parent->Columns[i];
		if (Parent->GetMode() == LListColumns)
			ColCtx.Set(x, Ctx.y1, Ctx.x2, Ctx.y2);
		else
			ColCtx.Set(x, Ctx.y1, x + c->Width()-1, Ctx.y2);
		ColCtx.Align = c->TextAlign();
		
		OnPaintColumn(ColCtx, i, c);
		if (h && i == h->GetColumn())
		{
			h->OnPaintColumn(ColCtx, i, c);
			h = d->Cols.Next();
		}
		x = ColCtx.x2 + 1;
		
		if (Parent->GetMode() == LListColumns)
			break;
	}
	
	// after columns
	if (x <= Ctx.x2)
	{
		Ctx.pDC->Colour(Ctx.Back);
		Ctx.pDC->Rectangle(x, Ctx.y1, Ctx.x2, Ctx.y2);
	}
}

//////////////////////////////////////////////////////////////////////////////
// List control
LList::LList(int id, int x, int y, int cx, int cy, const char *name)
	: ResObject(Res_ListView)
{
	d = new LListPrivate;
	SetId(id);
	Name(name);
	
	ItemsPos.ZOff(-1, -1);
	Buf = 0;
	GridLines = false;
	FirstVisible = -1;
	LastVisible = -1;
	EditLabels = false;
	MultiSelect(true);
	CompletelyVisible = 0;
	Keyboard = -1;
	Sunken(true);
	Name("LList");

	#if WINNATIVE
	SetStyle(GetStyle() | WS_TABSTOP);
	SetDlgCode(DLGC_WANTARROWS);
	Cursor = 0;
	#elif defined BEOS
	Handle()->SetViewColor(B_TRANSPARENT_COLOR);
	#endif
	SetTabStop(true);

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	LgiResources::StyleElement(this);
}

LList::~LList()
{
	DeleteObj(Buf);
	Empty();
	EmptyColumns();
	DeleteObj(d);
}

LListMode LList::GetMode()
{
	return d->Mode;
}

void LList::SetMode(LListMode m)
{
	if (d->Mode ^ m)
	{
		d->Mode = m;
		
		if (IsAttached())
		{
			PourAll();
			Invalidate();
		}
	}
}

void LList::OnItemClick(LListItem *Item, GMouse &m)
{
	if (Item)
		Item->OnMouseClick(m);
}

void LList::OnItemBeginDrag(LListItem *Item, GMouse &m)
{
	if (Item)
		Item->OnBeginDrag(m);
}

void LList::OnItemSelect(GArray<LListItem*> &It)
{
	if (It.Length())
	{
		Keyboard = (int)Items.IndexOf(It[0]);
		LgiAssert(Keyboard >= 0);
		
		LHashTbl<PtrKey<LListItem*>, bool> Sel;
		for (int n=0; n<It.Length(); n++)
		{
			It[n]->OnSelect();
			if (!MultiSelect())
				Sel.Add(It[n], true);
		}

		if (!MultiSelect())
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
	SendNotify(GNotifyItem_Select);
}

bool GItemContainer::DeleteColumn(GItemColumn *Col)
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

			SendNotify(GNotifyItem_ColumnsChanged);
			Status = true;
		}

		Unlock();
	}

	return Status;
}

GMessage::Result LList::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
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

int LList::OnNotify(GViewI *Ctrl, int Flags)
{
	if
	(
		(Ctrl->GetId() == IDC_VSCROLL && VScroll)
		||
		(Ctrl->GetId() == IDC_HSCROLL && HScroll)
	)
	{
		if (Flags == GNotifyScrollBar_Create)
			UpdateScrollBars();

		Invalidate(&ItemsPos);
	}

	return GLayout::OnNotify(Ctrl, Flags);
}

GRect &LList::GetClientRect()
{
	static GRect r;

	r = GetPos();
	r.Offset(-r.x1, -r.y1);

	return r;
}

LListItem *LList::HitItem(int x, int y, int *Index)
{
	int n=0;
	ForAllItems(i)
	{
		if
		(
			(
				// Is list mode we consider the item to have infinite width.
				// This helps with multi-selection when the cursor falls outside
				// the window's bounds but is still receiving mouse move messages
				// because of mouse capture.
				d->Mode == LListDetails
				&&
				y >= i->Pos.y1
				&&
				y <= i->Pos.y2
			)
			||
			(
				i->Pos.Overlap(x, y)
			)
		)
		{
			if (Index) *Index = n;
			return i;
		}
		n++;
	}

	return NULL;
}

void LList::ClearDs(int Col)
{
	ForAllItems(i)
	{
		i->ClearDs(Col);
	}
}

void LList::KeyScroll(int iTo, int iFrom, bool SelectItems)
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

	iTo = limit(iTo, 0, (int)Items.Length()-1);
	iFrom = limit(iFrom, 0, (int)Items.Length()-1);
	LListItem *To = Items.ItemAt(iTo);
	LListItem *From = Items.ItemAt(iFrom);
	// int Inc = (iTo < iFrom) ? -1 : 1;

	if (To && From && iTo != iFrom)
	{
		// LListItem *Item = 0;
		if (SelectItems)
		{
			int OtherEnd = Keyboard == End ? Start : End;
			int Min = MIN(OtherEnd, iTo);
			int Max = MAX(OtherEnd, iTo);
			i = 0;

			d->NoSelectEvent = true;
			GArray<LListItem*> Sel;
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

bool LList::OnMouseWheel(double Lines)
{
	if (VScroll)
	{
		int64 Old = VScroll->Value();
		VScroll->Value(Old + (int)Lines);
		if (Old != VScroll->Value())
		{
			Invalidate(&ItemsPos);
		}
	}
	
	if (HScroll)
	{
		int64 Old = HScroll->Value();
		HScroll->Value(Old + (int)(Lines / 3));
		if (Old != HScroll->Value())
		{
			Invalidate(&ItemsPos);
		}
	}
	
	return true;
}

bool LList::OnKey(GKey &k)
{
	bool Status = false;
	
	LListItem *Item = GetSelected();
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
					#if WINNATIVE
					if (!k.IsChar)
					#endif
					{
						SendNotify(GNotify_ReturnKey);
					}
					break;
				}
				case VK_BACKSPACE:
				{
					SendNotify(GNotify_BackspaceKey);
					break;
				}
				case VK_ESCAPE:
				{
					SendNotify(GNotify_EscapeKey);
					break;
				}
				case VK_DELETE:
				{
					SendNotify(GNotify_DeleteKey);
					break;					
				}
				case VK_UP:
				{
					// int i = Value();
					#ifdef MAC
					if (k.Ctrl())
						goto LList_PageUp;
					else if (k.System())
						goto LList_Home;
					#endif
					
					KeyScroll(Keyboard-1, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_DOWN:
				{
					#ifdef MAC
					if (k.Ctrl())
						goto LList_PageDown;
					else if (k.System())
						goto LList_End;
					#endif

					KeyScroll(Keyboard+1, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_LEFT:
				{
					if (GetMode() == LListColumns)
					{
						LListItem *Hit = GetSelected();
						if (Hit)
						{
							LListItem *To = 0;
							int ToDist = 0x7fffffff;
							
							for (LListItem *i=Items[FirstVisible]; i && i->Pos.Valid(); i=Items.Next())
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
									for (List<LListItem>::I it = Items.begin(FirstVisible); it.In(); it--)
									{
										LListItem *i = *it;
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
					if (GetMode() == LListColumns)
					{
						LListItem *Hit = GetSelected();
						if (Hit)
						{
							LListItem *To = 0;
							int ToDist = 0x7fffffff;
							
							for (LListItem *i=Items[FirstVisible]; i && i->Pos.Valid(); i=Items.Next())
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
					#ifdef MAC
					LList_PageUp:
					#endif
					int Vis = VisibleItems();
					Vis = MAX(Vis, 0);

					KeyScroll(Keyboard-Vis, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_PAGEDOWN:
				{
					#ifdef MAC
					LList_PageDown:
					#endif
					int Vis = VisibleItems();
					Vis = MAX(Vis, 0);
					KeyScroll(Keyboard+Vis, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_END:
				{
					#ifdef MAC
					LList_End:
					#endif
					printf("End handler\n");
					KeyScroll((int)Items.Length()-1, Keyboard, k.Shift());
					Status = true;
					break;
				}
				case VK_HOME:
				{
					#ifdef MAC
					LList_Home:
					#endif
					KeyScroll(0, Keyboard, k.Shift());
					Status = true;
					break;
				}
				#ifdef VK_APPS
				case VK_APPS:
				{
					LListItem *s = GetSelected();
					if (s)
					{
						GRect *r = &s->Pos;
						if (r)
						{
							GMouse m;
							LListItem *FirstVisible = ItemAt((VScroll) ? (int)VScroll->Value() : 0);

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
						uint64 Now = LgiCurrentTime();
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
							char *c8 = WideToUtf8(d->KeyBuf);
							if (c8)
							{
								int Col = 0;
								bool Ascend = true;
								for (int i=0; i<Columns.Length(); i++)
								{
									GItemColumn *c = Columns[i];
									if (c->Mark())
									{
										Col = i;
										if (c->Mark() == GLI_MARK_UP_ARROW)
										{
											Ascend = false;
										}
									}
								}
								
								bool Selected = false;
								List<LListItem>::I It = Ascend ? Items.begin() : Items.rbegin();
								for (LListItem *i = *It; It.In(); i = Ascend ? *++It : *--It)
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

LgiCursor LList::GetCursor(int x, int y)
{
	GItemColumn *Resize, *Over;
	HitColumn(x, y, Resize, Over);
	if (Resize)
		return LCUR_SizeHor;
	
	return LCUR_Normal;
}

void LList::OnMouseClick(GMouse &m)
{
	if (Lock(_FL))
	{
		if (m.Down())
		{
			Focus(true);

			DragMode = DRAG_NONE;
			d->DragStart.x = m.x;
			d->DragStart.y = m.y;

			if (ColumnHeaders &&
				ColumnHeader.Overlap(m.x, m.y))
			{
				// Clicked on a column heading
				GItemColumn *Resize, *Over;
				int Index = HitColumn(m.x, m.y, Resize, Over);

				if (Resize)
				{
					if (m.Double())
					{
						if (m.Modifier())
						{
							ResizeColumnsToContent();
						}
						else
						{
							ColSizes Sizes;
							GetColumnSizes(Sizes);
							
							int AvailablePx = GetClient().X() - 5;
							if (VScroll)
								AvailablePx -= VScroll->X();

							int ExpandPx = AvailablePx - (Sizes.FixedPx + Sizes.ResizePx);
							if (ExpandPx > 0)
							{
								int MaxPx = Resize->GetContentSize() + DEFAULT_COLUMN_SPACING;
								int AddPx = MIN(ExpandPx, MaxPx - Resize->Width());
								if (AddPx > 0)
								{
									Resize->Width(Resize->Width() + AddPx);
									ClearDs(Index);
									Invalidate();
								}
							}
						}
					}
					else
					{
						DragMode = RESIZE_COLUMN;
						d->DragData = (int)Columns.IndexOf(Resize);
						Capture(true);
					}
				}
				else
				{
					DragMode = CLICK_COLUMN;
					d->DragData = (int)Columns.IndexOf(Over);
					if (Over)
					{
						Over->Value(true);
						GRect r = Over->GetPos();
						Invalidate(&r);
						Capture(true);
					}
				}
			}
			else if (ItemsPos.Overlap(m.x, m.y))
			{
				// Clicked in the items area
				bool HandlerHung = false;
				int ItemIndex = -1;
				LListItem *Item = HitItem(m.x, m.y, &ItemIndex);
				// GViewI *Notify = Item ? (GetNotify()) ? GetNotify() : GetParent() : 0;
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
						uint64 Now = LgiCurrentTime();
						HandlerHung = Now - StartHandler > 200;
						if (!HandlerHung && !m.Double() && !m.IsContextMenu())
						{
							// Start d'n'd watcher pulse...
							SetPulse(100);
							Capture(true);
							DragMode = CLICK_ITEM;
						}
						
						if (!IsCapturing())
						{
							// If capture failed then we reset the dragmode...
							DragMode = DRAG_NONE;
						}
					}
				}
				else
				{
					// Selection change
					if (m.Shift() && MultiSelect())
					{
						int n = 0;
						int a = MIN(ItemIndex, Keyboard);
						int b = MAX(ItemIndex, Keyboard);
						GArray<LListItem*> Sel;

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
										Keyboard = (int)Items.IndexOf(i);
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
							Keyboard = (int)Items.IndexOf(Item);
						}

						if (!m.Modifier() && Items.First() && !m.IsContextMenu())
						{
							DragMode = SELECT_ITEMS;
							SetPulse(100);
							Capture(true);
						}

						if (SelectionChanged)
						{
							SendNotify(GNotifyItem_Select);
						}
					}

					OnItemClick(Item, m);
				}

				if (!HandlerHung)
				{
					if (m.IsContextMenu())
						SendNotify(GNotifyItem_ContextMenu);
					else if (m.Double())
						SendNotify(GNotifyItem_DoubleClick);
					else if (Item)
						SendNotify(GNotifyItem_Click);
					else
						SendNotify(GNotifyContainer_Click);
				}
			}
		}
		else // Up Click
		{
			switch (DragMode)
			{
				case CLICK_COLUMN:
				{
					if (d->DragData < 0)
						break;
					
					GItemColumn *c = Columns[d->DragData];
					if (c)
					{
						c->Value(false);
						
						GRect cpos = c->GetPos();
						Invalidate(&cpos);

						if (cpos.Overlap(m.x, m.y))
						{
							OnColumnClick((int)Columns.IndexOf(c), m);
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
					LListItem *Item = Items.ItemAt(d->DragData);
					if (Item)
					{
						bool Change = false;
						
						GArray<LListItem*> s;
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
					if (DragCol)
					{
						GRect DragPos = DragCol->GetPos();
						GdcPt2 p(DragPos.x1 + (DragPos.X()/2), 0);
						PointToView(p);

						int OldIndex = DragCol->GetIndex();
						int Best = 100000000, NewIndex = OldIndex, i=0, delta;
						for (i=0; i<Columns.Length(); i++)
						{
							GItemColumn *Col = Columns[i];
							delta = abs(p.x - Col->GetPos().x1);
							if (delta < Best)
							{
								Best = delta;
								NewIndex = i - (i > OldIndex ? 1 : 0);
							}
						}
						delta = abs(p.x - Columns.Last()->GetPos().x2);
						if (delta < Best)
						{
							NewIndex = i;
						}

						GItemColumn *Col = DragCol->GetColumn();
						if (OldIndex != NewIndex &&
							OnColumnReindex(Col, OldIndex, NewIndex))
						{
							Columns.SetFixedLength(false);
							Columns.Delete(Col);
							Columns.AddAt(NewIndex, Col);
							Columns.SetFixedLength(true);

							UpdateAllItems();
						}

						DragCol->Quit();
						DragCol = NULL;
					}

					Invalidate();
					break;
				}
			}

			LListItem *Item = HitItem(m.x, m.y);
			if (Item)
			{
				OnItemClick(Item, m);
			}

			if (IsCapturing())
			{
				Capture(false);
			}

			DragMode = DRAG_NONE;
		}

		Unlock();
	}
}

void LList::OnPulse()
{
	if (Lock(_FL))
	{
		if (IsCapturing())
		{
			GMouse m;
			GetMouse(m);

			if (m.y < 0 || m.y >= Y())
			{
				switch (DragMode)
				{
					case SELECT_ITEMS:
					{
						int OverIndex = 0;
						LListItem *Over = 0;

						if (m.y < 0)
						{
							int Space = -m.y;

							int n = FirstVisible - 1;
							for (LListItem *i = Items[n]; i; i=Items.Prev(), n--)
							{
								GdcPt2 Info;
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
							for (LListItem *i = Items[n]; i; i=Items.Next(), n++)
							{
								GdcPt2 Info;
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
								OverIndex = (int)Items.Length()-1;
							}
						}

						int Min = MIN(d->DragData, OverIndex);
						int Max = MAX(d->DragData, OverIndex);
						int n = Min;
						for (LListItem *i = Items[Min]; i && n <= Max; i=Items.Next(), n++)
						{
							if (!i->Select())
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
			DragMode = DRAG_NONE;
			SetPulse();
		}

		Unlock();
	}
}

void LList::OnMouseMove(GMouse &m)
{
	if (Lock(_FL))
	{
		switch (DragMode)
		{
			case DRAG_COLUMN:
			{
				if (DragCol)
				{
					GdcPt2 p;
					PointToScreen(p);

					GRect r = DragCol->GetPos();
					r.Offset(-p.x, -p.y); // to view co-ord

					r.Offset(m.x - DragCol->GetOffset() - r.x1, 0);
					if (r.x1 < 0) r.Offset(-r.x1, 0);
					if (r.x2 > X()-1) r.Offset((X()-1)-r.x2, 0);

					r.Offset(p.x, p.y); // back to screen co-ord
					DragCol->SetPos(r, true);
					r = DragCol->GetPos();
				}
				break;
			}
			case RESIZE_COLUMN:
			{
				GItemColumn *c = Columns[d->DragData];
				if (c)
				{
					// int OldWidth = c->Width();
					int NewWidth = m.x - c->GetPos().x1;

					c->Width(MAX(NewWidth, 4));
					ClearDs(d->DragData);
					Invalidate();
				}
				break;
			}
			case CLICK_COLUMN:
			{
				if (d->DragData < 0 || d->DragData >= Columns.Length())
					break;
					
				GItemColumn *c = Columns[d->DragData];
				if (c)
				{
					if (abs(m.x - d->DragStart.x) > DRAG_THRESHOLD ||
						abs(m.y - d->DragStart.y) > DRAG_THRESHOLD)
					{
						OnColumnDrag(d->DragData, m);
					}
					else
					{
						bool Over = c->GetPos().Overlap(m.x, m.y);
						if (m.Down() && Over != c->Value())
						{
							c->Value(Over);
							GRect r = c->GetPos();
							Invalidate(&r);
						}
					}
				}
				break;
			}
			case SELECT_ITEMS:
			{
				int n=0;
				// bool Selected = m.y < ItemsPos.y1;

				if (IsCapturing())
				{
					if (MultiSelect())
					{
						int Over = -1;
						
						/*
						LListItem *h = HitItem(m.x, m.y, &Over);
						if (m.y < ItemsPos.y1 && FirstVisible == 0)
						{
							Over = 0;
						}
						else
						{
							Iterator<LListItem> Iter(&Items);
							int n = FirstVisible;
							for (LListItem *k=Iter[n]; k && k->OnScreen(); k=Iter.Next())
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

							int Start = MIN(Over, d->DragData);
							int End = MAX(Over, d->DragData);

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
				LListItem *Cur = Items.ItemAt(d->DragData);
				if (Cur)
				{
					Cur->OnMouseMove(m);
					
					if (IsCapturing() &&
						(abs(d->DragStart.x-m.x) > DRAG_THRESHOLD ||
						abs(d->DragStart.y-m.y) > DRAG_THRESHOLD))
					{
						Capture(false);
						OnItemBeginDrag(Cur, m);
						DragMode = DRAG_NONE;
					}
				}
				break;
			}
			default:
			{
				List<LListItem> s;
				if (GetSelection(s))
				{
					for (LListItem *c=s.First(); c; c=s.Next())
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

int64 LList::Value()
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

void LList::Value(int64 Index)
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

void LList::SelectAll()
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

bool LList::Select(LListItem *Obj)
{
	bool Status = false;
	ForAllItems(i)
	{
		i->Select(Obj == i);
		if (Obj == i) Status = true;
	}
	return true;
}

LListItem *LList::GetSelected()
{
	LListItem *n = 0;

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

bool LList::GetUpdateRegion(LListItem *i, GRegion &r)
{
	r.Empty();

	if (d->Mode == LListDetails)
	{
		if (i->Pos.Valid())
		{
			GRect u = i->Pos;
			u.y2 = ItemsPos.y2;
			r.Union(&u);
			
			return true;
		}
	}
	else if (d->Mode == LListColumns)
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

bool LList::Insert(LListItem *i, int Index, bool Update)
{
	List<LListItem> l;
	l.Insert(i);
	return Insert(l, Index, Update);
}

bool LList::Insert(List<LListItem> &l, int Index, bool Update)
{
	bool Status = false;

	if (Lock(_FL))
	{
		bool First = Items.Length() == 0;
		
		// Insert list of items
		for (LListItem *i=l.First(); i; i=l.Next())
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

		Status = true;

		Unlock();

		if (Update)
		{
			// Update screen
			PourAll();
			Invalidate();

			// Notify
			SendNotify(GNotifyItem_Insert);
		}
	}

	return Status;
}

bool LList::Delete()
{
	return Delete(Items.Current());
}

bool LList::Delete(int Index)
{
	return Delete(Items.ItemAt(Index));
}

bool LList::Delete(LListItem *i)
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

bool LList::Remove(LListItem *i)
{
	bool Status = false;

	if (Lock(_FL))
	{
		if (i && i->GetList() == this)
		{
			GRegion Up;
			bool Visible = GetUpdateRegion(i, Up);
			bool Selected = i->Select();
			int Index = (int)Items.IndexOf(i);
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
					GArray<LListItem*> s;
					OnItemSelect(s);
				}

				Note->OnNotify(this, GNotifyItem_Delete);
			}

			Status = true;
		}

		Unlock();
	}

	return Status;
}

bool LList::HasItem(LListItem *Obj)
{
	return Items.HasItem(Obj);
}

int LList::IndexOf(LListItem *Obj)
{
	return (int)Items.IndexOf(Obj);
}

LListItem *LList::ItemAt(size_t Index)
{
	return Items.ItemAt(Index);
}

void LList::ScrollToSelection()
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
					VScroll->Value(MAX(k, 0));
					Invalidate(&ItemsPos);
					break;
				}
			}
			n++;
		}
	}
}

/*
int ListStringCompare(LListItem *a, LListItem *b, NativeInt data)
{
	char *ATxt = (a)->GetText(data);
	char *BTxt = (b)->GetText(data);
	if (ATxt && BTxt)
		return stricmp(ATxt, BTxt);
	return 0;
}

void LList::Sort(LListCompareFunc Compare, NativeInt Data)
{
	if (Lock(_FL))
	{
		LListItem *Kb = Items[Keyboard];
		Items.Sort(Compare ? Compare : ListStringCompare, Data);
		Keyboard = Kb ? Items.IndexOf(Kb) : -1;
		Invalidate(&ItemsPos);

		Unlock();
	}
}
*/

void LList::Empty()
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
		DragMode = DRAG_NONE;

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

void LList::RemoveAll()
{
	if (Lock(_FL))
	{
		if (Items.Length())
		{
			GArray<LListItem*> s;
			OnItemSelect(s);
		}

		for (LListItem *i = Items.First(); i; i = Items.Next())
		{
			i->OnRemove();
			i->Parent = 0;
		}
		Items.Empty();
		FirstVisible = LastVisible = -1;
		DragMode = DRAG_NONE;

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

void LList::OnPosChange()
{
	GLayout::OnPosChange();
}

void LList::UpdateScrollBars()
{
	static bool Processing = false;
	
	if (!Processing)
	{
		Processing = true;
		
		if (VScroll)
		{
			int Vis = VisibleItems();
			int Max = MaxScroll();

			if (VScroll->Value() > MAX(Max, 0))
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

void LList::PourAll()
{
	#if LList_POUR_PROFILE
	int Start = LgiCurrentTime();
	#endif

	// Layout all the elements
	GRect Client = GetClient();
	GFont *Font = GetFont();

	if (d->Mode == LListDetails)
	{
		if (ColumnHeaders)
		{
			ColumnHeader = Client;
			ColumnHeader.y2 = ColumnHeader.y1 + Font->GetHeight() + 4;
			ItemsPos = Client;
			ItemsPos.y1 = ColumnHeader.y2 + 1;
		}
		else
		{
			ItemsPos = Client;
			ColumnHeader.ZOff(-1, -1);
		}
		
		int n = 0;
		int y = ItemsPos.y1;
		int Max = MaxScroll();
		FirstVisible = (VScroll) ? (int)VScroll->Value() : 0;
		if (FirstVisible > Max) FirstVisible = Max;
		LastVisible = 0x7FFFFFFF;
		CompletelyVisible = 0;
		bool SomeHidden = false;

		ForAllItems(i)
		{
			if (n < FirstVisible || n > LastVisible)
			{
				i->Pos.Set(-1, -1, -2, -2);
				SomeHidden = true;
			}
			else
			{
				GdcPt2 Info;
				
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
			LastVisible = (int)Items.Length() - 1;
		}

		SetScrollBars(false, SomeHidden);
		UpdateScrollBars();
	}
	else if (d->Mode == LListColumns)
	{
		ColumnHeader.ZOff(-1, -1);
		ItemsPos = Client;
		FirstVisible = 0;

		int CurX = 0;
		int CurY = 0;
		int MaxX = 16;
		GArray<LListItem*> Col;
		d->Columns = 1;
		d->VisibleColumns = 0;
		int64 ScrollX = HScroll ? HScroll->Value() : 0;
		int64 OffsetY = HScroll ? 0 : GScrollBar::GetScrollSize();
		FirstVisible = -1;
		
		int n = 0;
		ForAllItems(i)
		{
			GdcPt2 Info;
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
				MaxX = MAX(MaxX, Info.x);
				
				CompletelyVisible++;
			}

			CurY += Info.y;
			n++;
		}
		
		d->VisibleColumns = MAX(1, d->VisibleColumns);

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
	
	#if LList_POUR_PROFILE
	printf("LList::Pour() %i ms\n", LgiCurrentTime() - Start);
	#endif
}

void LList::OnPaint(GSurface *pDC)
{
	#if LList_ONPAINT_PROFILE
	int Start = LgiCurrentTime(), t1, t2, t3, t4, t5;
	#endif
	
	if (Lock(_FL))
	{
		COLOUR Back = Enabled() ? LC_WORKSPACE : LC_MED;
		PourAll();
		
		#if LList_ONPAINT_PROFILE
		t1 = LgiCurrentTime();
		#endif

		// Check icon column status then draw
		if (AskImage() && !IconCol)
		{
			IconCol.Reset(new GItemColumn(this, 0, 18));
			if (IconCol)
			{
				IconCol->Resizable(false);
				IconCol->Type(GIC_ASK_IMAGE);
			}
		}
		else if (!AskImage())
			IconCol.Reset();

		PaintColumnHeadings(pDC);

		#if LList_ONPAINT_PROFILE
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
		GCss::ColorDef Fill;
		COLOUR SelFore = Focus() ? LC_FOCUS_SEL_FORE : LC_NON_FOCUS_SEL_FORE;
		COLOUR SelBack = Focus() ? LC_FOCUS_SEL_BACK : (Enabled() ? LC_NON_FOCUS_SEL_BACK : LC_MED);
		int LastSelected = -1;

		GItem::ItemPaintCtx Ctx;
		Ctx.pDC = pDC;

		GRegion Rgn(ItemsPos);
		for (LListItem *i = Items.ItemAt(n); i; i = Items.Next(), n++)
		{
			if (i->Pos.Valid())
			{
				// Setup painting colours in the context
				if (LastSelected ^ (int)i->Select())
				{
					if ((LastSelected = i->Select()))
					{
						Ctx.Fore.Set(SelFore, 24);
						Ctx.Back.Set(SelBack, 24);
					}
					else
					{
						Ctx.Fore.Set(LC_TEXT, 24);
						Ctx.Back.Set(Back, 24);
					}
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

	#if LList_ONPAINT_PROFILE
	int64 End = LgiCurrentTime();
	printf("LList::OnPaint() pour=%i headers=%i items=%i\n", (int) (t1-Start), (int) (t2-t1), (int) (End-t2));
	#endif
}

void LList::OnFocus(bool b)
{
	LListItem *s = GetSelected();
	if (!s)
	{
		s = Items.First();
		if (s)
		{
			s->Select(true);
		}
	}

	for (LListItem *i = Items.ItemAt(FirstVisible); i; i = Items.Next())
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

void LList::UpdateAllItems()
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

int LList::GetContentSize(int Index)
{
	int Max = 0;

	for (List<LListItem>::I It = Items.begin(); It.In(); It++)
	{
		LListItem *i = *It;
		GDisplayString *s = i->d->Display[Index];
		GDisplayString *Mem = 0;
		
		// If no cached string, create it for the list item
		if (!s || s->IsTruncated())
		{
			GFont *f = i->GetFont();
			if (!f) f = GetFont();
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
			Max = MAX(Max, s->X());
		}
		
		DeleteObj(Mem);
	}
	
	// Measure the heading too
	GItemColumn *Col = Columns[Index];
	GFont *f = GetFont();
	LgiAssert(f != 0);
	if (f)
	{
		GDisplayString h(f, Col->Name());
		int Hx = h.X() + (Col->Mark() ? 10 : 0);
		Max = MAX(Max, Hx);
	}

	return Max;
}
