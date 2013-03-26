#include <math.h>

#include "Lgi.h"
#include "GTree.h"
#include "GScrollBar.h"

#define TREE_BLOCK          16
#define DRAG_THRESHOLD		4
#define DRAG_SCROLL_EDGE	20
#define DRAG_SCROLL_X		8
#define DRAG_SCROLL_Y		1

#ifdef LINUX
#define TreeUpdateNow		false
#else
#define TreeUpdateNow		true
#endif

//////////////////////////////////////////////////////////////////////////////
// Private class definitions for binary compatibility
class GTreePrivate
{
public:
	// Private data
	int				LineFlags[4];
	bool			LayoutDirty;
	GdcPt2			Limit;
	GdcPt2			LastClick;
	GMemDC			*IconCache;
	bool			InPour;
	int64			DropSelectTime;
    int8            IconTextGap;

	// Pointers into items... be careful to clear when deleting items...
	GTreeItem		*LastHit;
	List<GTreeItem>	Selection;
	GTreeItem		*DropTarget;

	GTreePrivate()
	{
		DropSelectTime = 0;
		InPour = false;
		LastHit = 0;
		DropTarget = 0;
		IconCache = 0;
		LayoutDirty = true;
		IconTextGap = 0;
	}
	
	~GTreePrivate()
	{
		DeleteObj(IconCache);
	}
};

class GTreeItemPrivate
{
public:
	GDisplayString *Ds;
	GRect Pos;
	GRect Thumb;
	GRect Text;
	GRect Icon;
	bool Open;
	bool Selected;
	bool Visible;
	bool Last;
	int Depth;
	
	GTreeItemPrivate()
	{
		Ds = 0;
		Pos.ZOff(-1, -1);
		Open = false;
		Selected = false;
		Visible = false;
		Last = false;
		Depth = 0;
	}

	~GTreeItemPrivate()
	{
		DeleteObj(Ds);
	}
};

//////////////////////////////////////////////////////////////////////////////
GTreeNode::GTreeNode()
{
	Parent = 0;
	Tree = 0;
}

GTreeNode::~GTreeNode()
{
}

void GTreeNode::_Visible(bool v)
{
	for (GTreeItem *i=GetChild(); i; i=i->GetNext())
	{
		LgiAssert(i != this);
		i->OnVisible(v);
		i->_Visible(v);
	}
}

GTreeItem *GTreeNode::Insert(GTreeItem *Obj, int Idx)
{
	LgiAssert(Obj != this);

	if (Obj && Obj->Tree)
	{
		Obj->Remove();
	}
	
	GTreeItem *NewObj = (Obj) ? Obj : new GTreeItem;
	if (NewObj)
	{
		NewObj->Parent = Item();
		NewObj->_SetTreePtr(Tree);

		Items.Delete(NewObj);
		Items.Insert(NewObj, Idx);

		if (Tree)
		{
			Tree->d->LayoutDirty = true;
			if (Pos() && Pos()->Y() > 0)
			{
				Tree->_UpdateBelow(Pos()->y1);
			}
			else
			{
				Tree->Invalidate();
			}
		}
	}

	return NewObj;
}

void GTreeNode::Detach()
{
	if (Parent)
	{
		GTreeItem *It = Item();
		if (It)
		{
			LgiAssert(Parent->Items.HasItem(It));
			Parent->Items.Delete(It);
		}
		Parent = 0;
	}
	if (Tree)
	{
		Tree->d->LayoutDirty = true;
		Tree->Invalidate();
	}
	if (Item())
		Item()->_SetTreePtr(0);
}

void GTreeNode::Remove()
{
	int y = 0;
	if (Parent)
	{
		if (Item() && Item()->IsRoot())
		{
			GRect *p = Pos();
			GTreeItem *Prev = GetPrev();
			if (Prev)
			{
				y = Prev->d->Pos.y1;
			}
			else
			{
				y = p->y1;
			}
		}
		else
		{
			y = Parent->d->Pos.y1;
		}
	}

	GTree *t = Tree;

	if (Item())
		Item()->_Remove();

	if (t)
	{
		t->_UpdateBelow(y);
	}
}

bool GTreeNode::IsRoot()
{
	return Parent == 0 || (GTreeNode*)Parent == (GTreeNode*)Tree;
}

int GTreeNode::GetItems()
{
	return Items.Length();
}

int GTreeNode::IndexOf()
{
	if (Parent)
	{
		return Parent->Items.IndexOf(Item());
	}
	else if (Tree)
	{
		return Tree->Items.IndexOf(Item());
	}

	return -1;
}

GTreeItem *GTreeNode::GetChild()
{
	return Items.First();
}

GTreeItem *GTreeNode::GetPrev()
{
	List<GTreeItem> *l = (Parent) ? &Parent->Items : (Tree) ? &Tree->Items : 0;
	if (l)
	{
		int Index = l->IndexOf(Item());
		if (Index >= 0)
		{
			return l->ItemAt(Index-1);
		}
	}

	return 0;
}

GTreeItem *GTreeNode::GetNext()
{
	List<GTreeItem> *l = (Parent) ? &Parent->Items : (Tree) ? &Tree->Items : 0;
	if (l)
	{
		int Index = l->IndexOf(Item());
		if (Index >= 0)
		{
			return l->ItemAt(Index+1);
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
GTreeItem::GTreeItem()
{
	d = new GTreeItemPrivate;
	Str = 0;
	Sys_Image = -1;
}

GTreeItem::~GTreeItem()
{
	if (Tree)
	{
		if (Tree->d->DropTarget == this)
		{
			Tree->d->DropTarget = 0;
		}

		if (Tree->d->LastHit == this)
		{
			Tree->d->LastHit = 0;
		}
		Tree->Capture(false);
	}

	int y;
	GTree *t = 0;
	if (Parent && (GTreeNode*)Parent != (GTreeNode*)Tree)
	{
		t = Tree;
		y = Parent->d->Pos.y1;
	}
	else if ((GTreeNode*)this != (GTreeNode*)Tree)
	{
		t = Tree;
		GTreeItem *p = GetPrev();
		if (p)
		{
			y = p->d->Pos.y1;
		}
		else
		{
			y = d->Pos.y1;
		}
	}

	_Remove();
	Items.DeleteObjects();
	DeleteArray(Str);
	DeleteObj(d);

	if (t)
	{
		t->_UpdateBelow(y);
	}
}

GRect *GTreeItem::Pos()
{
	return &d->Pos;
}

GdcPt2 GTreeItem::_ScrollPos()
{
	GdcPt2 p;
	if (Tree) p = Tree->_ScrollPos();
	return p;
}

GRect *GTreeItem::_GetRect(GTreeItemRect Which)
{
	switch (Which)
	{
		case TreeItemPos: return &d->Pos;
		case TreeItemThumb: return &d->Thumb;
		case TreeItemText: return &d->Text;
		case TreeItemIcon: return &d->Icon;
	}
	
	return 0;
}

void GTreeItem::_RePour()
{
	if (Tree)
	{
		Tree->_Pour();
	}
}

void GTreeItem::ScrollTo()
{
	if (Tree && Tree->VScroll)
	{
		GRect c = Tree->GetClient();
		GRect p = d->Pos;
		int y = d->Pos.Y() ? d->Pos.Y() : 16;
		p.Offset(0, -Tree->VScroll->Value() * y);

		if (p.y1 < c.y1)
		{
			int Lines = (c.y1 - p.y1 + y - 1) / y;
			Tree->VScroll->Value(Tree->VScroll->Value() - Lines);
		}
		else if (p.y2 > c.y2)
		{
			int Lines = (p.y2 - c.y2 + y - 1) / y;
			Tree->VScroll->Value(Tree->VScroll->Value() + Lines);
		}
	}
}

void GTreeItem::_SetTreePtr(GTree *t)
{
	if (Tree && !t)
	{
		// Clearing tree pointer, must remove all references to this item that
		// the tree might still have.
		if (d->Selected)
		{
			Tree->d->Selection.Delete(this);
			d->Selected = false;
		}
		if (Tree->d->LastHit == this)
		{
			Tree->d->LastHit = 0;
		}
		if (Tree->d->DropTarget == this)
		{
			Tree->d->DropTarget = 0;
		}
	}
	Tree = t;
	for (GTreeItem *i=Items.First(); i; i=Items.Next())
	{
		i->_SetTreePtr(t);
	}
}

void GTreeItem::_Remove()
{
	if ((GTreeNode*)this != (GTreeNode*)Tree)
	{
		if (Parent)
		{
			LgiAssert(Parent->Items.HasItem(this));
			Parent->Items.Delete(this);
		}
		else if (Tree)
		{
			LgiAssert(Tree->Items.HasItem(this));
			Tree->Items.Delete(this);
		}

		if (Tree)
		{
			LgiAssert(Tree->d);
			Tree->d->LayoutDirty = true;
			Tree->Capture(false);
		}
	}

	Parent = 0;
	_SetTreePtr(0);
}

void GTreeItem::_PourText(GdcPt2 &Size)
{
	GDisplayString ds(SysFont, GetText());
	Size.x = ds.X() + 4;
	Size.y = 0;
}

void GTreeItem::_PaintText(GSurface *pDC, COLOUR Fore, COLOUR Back)
{
	char *Text = GetText();
	if (Text)
	{
		if (!d->Ds)
		{
			d->Ds = new GDisplayString(Tree->GetFont(), Text);
		}

		int Tab = SysFont->TabSize();
		SysFont->TabSize(0);
		SysFont->Transparent(false);
		SysFont->Colour(Fore, Back);
		
		d->Ds->Draw(pDC, d->Text.x1 + 2, d->Text.y1 + 1, &d->Text);
		
		SysFont->TabSize(Tab);
	}
	else
	{
		pDC->Colour(Back, 24);
		pDC->Rectangle(&d->Text);
	}
}

void GTreeItem::_Pour(GdcPt2 *Limit, int Depth, bool Visible)
{
	d->Visible = Visible;
	d->Depth = Depth;

	if (d->Visible)
	{
		GdcPt2 TextSize;
		_PourText(TextSize);
		GImageList *ImgLst = Tree->GetImageList();
		int IconX = (ImgLst && GetImage() >= 0) ? ImgLst->TileX() + Tree->d->IconTextGap : 0;
		int IconY = (ImgLst && GetImage() >= 0) ? ImgLst->TileY() : 0;
		int Height = max(TextSize.y, IconY);
		if (!Height)
		    Height = 16;

		if (!d->Ds)
		{
			d->Ds = new GDisplayString(Tree->GetFont(), GetText());
		}

		d->Pos.ZOff(	(TREE_BLOCK*d->Depth) +	// trunk
						TREE_BLOCK +			// node
						IconX +					// icon if present
						TextSize.x,				// text if present
						max(Height, d->Ds->Y())-1);
		d->Pos.Offset(0, Limit->y);

		Limit->x = max(Limit->x, d->Pos.x2 + 1);
		Limit->y = max(Limit->y, d->Pos.y2 + 1);
	}
	else
	{
		d->Pos.ZOff(-1, -1);
	}

	GTreeItem *n;
	for (GTreeItem *i=Items.First(); i; i=n)
	{
		n=Items.Next();
		i->d->Last = n == 0;
		i->_Pour(Limit, Depth+1, d->Open && d->Visible);
	}
}

char *GTreeItem::GetText(int i)
{
	return Str;
}

bool GTreeItem::SetText(const char *s, int i)
{
	char *New = NewStr(s);
	DeleteArray(Str);
	Str = New;

	if (Tree)
	{
		Update();
	}

	return true;
}

int GTreeItem::GetImage(int Flags)
{
	return Sys_Image;
}

void GTreeItem::SetImage(int i)
{
	Sys_Image = i;
}

void GTreeItem::Update()
{
	if (Tree)
	{
		GRect p = d->Pos;
		p.x2 = 1000;
		DeleteObj(d->Ds);
		Tree->_Update(&p, TreeUpdateNow);
	}
}

bool GTreeItem::Select()
{
	return d->Selected;
}

void GTreeItem::Select(bool b)
{
	if (d->Selected != b)
	{
		d->Selected = b;
		if (b)
		{
			GTreeItem *p = this;
			while ((p = p->GetParent()))
			{
				p->Expanded(true);
			}
		}

		Update();

		if (b && Tree)
		{
			Tree->_OnSelect(this);
			Tree->OnItemSelect(this);
		}
	}
}

bool GTreeItem::Expanded()
{
	return d->Open;
}

void GTreeItem::Expanded(bool b)
{
	if (d->Open != b)
	{
		d->Open = b;

		if (Items.Length() > 0)
		{
			if (Tree)
			{
				Tree->d->LayoutDirty = true;
				Tree->_UpdateBelow(d->Pos.y1);
			}
			OnExpand(b);
		}
	}
}

void GTreeItem::OnExpand(bool b)
{
	_Visible(b);
}

GTreeItem *GTreeItem::_HitTest(int x, int y)
{
	GTreeItem *Status = 0;

	if (d->Pos.Overlap(x, y) &&
		x > (d->Depth*TREE_BLOCK))
	{
		Status = this;
	}

	if (d->Open)
	{
		for (GTreeItem *i=Items.First(); i && !Status; i=Items.Next())
		{
			Status = i->_HitTest(x, y);
		}
	}

	return Status;
}

void GTreeItem::_MouseClick(GMouse &m)
{
	if (m.Down())
	{
		if ((Items.Length() > 0 &&
			d->Thumb.Overlap(m.x, m.y)) ||
			m.Double())
		{
			Expanded(!Expanded());
		}

		if (d->Text.Overlap(m.x, m.y) ||
			d->Icon.Overlap(m.x, m.y))
		{
			Select(true);

			if (Tree)
			{
				Tree->OnItemClick(this, m);
			}
		}
	}
}

void GTreeItem::OnPaint(ItemPaintCtx &Ctx)
{
	LgiAssert(Tree);

	// background up to text
	GSurface *&pDC = Ctx.pDC;
	pDC->Colour(LC_WORKSPACE, 24);
	pDC->Rectangle(0, d->Pos.y1, (d->Depth*TREE_BLOCK)+TREE_BLOCK, d->Pos.y2);

	// draw trunk
	int x = 0;
	COLOUR Lines = LC_MED;
	pDC->Colour(Lines, 24);
	for (int i=0; i<d->Depth; i++)
	{
		if (Tree->d->LineFlags[0] & (1 << i))
		{
			pDC->Line(x + 8, d->Pos.y1, x + 8, d->Pos.y2);
		}
		x += TREE_BLOCK;
	}

	// draw node
	int cy = d->Pos.y1 + (d->Pos.Y() >> 1);
	if (Items.Length() > 0)
	{
		// plus/minus symbol
		d->Thumb.ZOff(8, 8);
		d->Thumb.Offset(x + 4, cy - 4);

		pDC->Colour(LC_LOW, 24);
		pDC->Box(&d->Thumb);
		pDC->Colour(LC_WHITE, 24);
		pDC->Rectangle(d->Thumb.x1+1, d->Thumb.y1+1, d->Thumb.x2-1, d->Thumb.y2-1);
		pDC->Colour(LC_SHADOW, 24);
		pDC->Line(	d->Thumb.x1+2,
					d->Thumb.y1+4,
					d->Thumb.x1+6,
					d->Thumb.y1+4);

		pDC->Colour(Lines, 24);

		if (Parent || IndexOf() > 0)
		{
			// draw line to item above
			pDC->Line(x + 8, d->Pos.y1, x + 8, d->Thumb.y1-1);
		}

		// draw line to leaf beside
		pDC->Line(d->Thumb.x2+1, cy, x + (TREE_BLOCK-1), cy);

		if (!d->Last)
		{
			// draw line to item below
			pDC->Line(x + 8, d->Thumb.y2+1, x + 8, d->Pos.y2);
		}

		if (!d->Open)
		{
			// not open, so draw the cross bar making the '-' into a '+'
			pDC->Colour(LC_SHADOW, 24);
			pDC->Line(	d->Thumb.x1+4,
						d->Thumb.y1+2,
						d->Thumb.x1+4,
						d->Thumb.y1+6);
		}
	}
	else
	{
		// leaf node
		pDC->Colour(LC_MED, 24);
		if (d->Last)
		{
			pDC->Rectangle(x + 8, d->Pos.y1, x + 8, cy);
		}
		else
		{
			pDC->Rectangle(x + 8, d->Pos.y1, x + 8, d->Pos.y2);
		}

		pDC->Rectangle(x + 8, cy, x + (TREE_BLOCK-1), cy);
	}
	x += TREE_BLOCK;

	// draw icon
	int Image = GetImage(Select());
	GImageList *Lst = Tree->GetImageList();
	if (Image >= 0 && Lst)
	{
		d->Icon.ZOff(Lst->TileX() + Tree->d->IconTextGap - 1, d->Pos.Y() - 1);
		d->Icon.Offset(x, d->Pos.y1);

		pDC->Colour(LC_WORKSPACE, 24);

		if (Tree->d->IconCache)
		{
			// no flicker
			GRect From;

			From.ZOff(Lst->TileX()-1, Tree->d->IconCache->Y()-1);
			From.Offset(Lst->TileX()*Image, 0);

			pDC->Blt(d->Icon.x1, d->Icon.y1, Tree->d->IconCache, &From);

			pDC->Rectangle(d->Icon.x1 + Lst->TileX(), d->Icon.y1, d->Icon.x2, d->Icon.y2);
		}
		else
		{
			// flickers...
			int Pos = d->Icon.y1 + ((Lst->TileY()-d->Pos.Y()) >> 1);
			pDC->Rectangle(&d->Icon);
			Tree->GetImageList()->Draw(pDC, d->Icon.x1, Pos, Image);
		}

		x += d->Icon.X();
	}

	// text
	char *Text = GetText();
	if (Text)
	{
		GdcPt2 TextSize;
		_PourText(TextSize);

		d->Text.ZOff(TextSize.x, d->Pos.Y()-1);
		d->Text.Offset(x, d->Pos.y1);

		_PaintText(pDC, Ctx.Fore, Ctx.Back);
		d->Pos.x2 = d->Text.x2;
	}
	else
	{
		d->Text.ZOff(0, d->Pos.Y()-1);
		d->Text.Offset(x, d->Pos.y1);
	}

	// background after text
	pDC->Colour(LC_WORKSPACE, 24);
	pDC->Rectangle(d->Text.x2, d->Pos.y1, max(Tree->X(), Tree->d->Limit.x), d->Pos.y2);

	// children
	if (d->Open)
	{
		if (!d->Last)
		{
			Tree->d->LineFlags[0] |= 1 << d->Depth;
		}

		COLOUR SelFore = Tree->Focus() ? LC_FOCUS_SEL_FORE : LC_NON_FOCUS_SEL_FORE;
		COLOUR SelBack = Tree->Focus() ? LC_FOCUS_SEL_BACK : LC_NON_FOCUS_SEL_BACK;
		for (GTreeItem *i=Items.First(); i; i=Items.Next())
		{
			bool IsSelected = (Tree->d->DropTarget == i) || (Tree->d->DropTarget == 0 && i->Select());

			// Foreground
			GViewFill *Fill = i->GetForegroundFill();
			Ctx.Fore = Fill ? Fill->GetFlat().c24() : (IsSelected ? SelFore : LC_TEXT);

			// Background	
			Fill = i->GetBackgroundFill();
			Ctx.Back =	Fill ?
						Fill->GetFlat().c24() :
						(IsSelected ? SelBack : LC_WORKSPACE);

			i->OnPaint(Ctx);
		}

		Tree->d->LineFlags[0] &= ~(1 << d->Depth);
	}
}

//////////////////////////////////////////////////////////////////////////////
GTree::GTree(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_TreeView)
{
	d = new GTreePrivate;
	SetId(id);
	GRect e(x, y, x+cx, y+cy);
	SetPos(e);
	if (name) Name(name);
	else Name("LGI.GTree");
	Sunken(true);

	Tree = this;
	Lines = true;
	Buttons = true;
	LinesAtRoot = true;
	EditLabels = false;
	MultipleSelect = false;

	#if WIN32NATIVE
	SetStyle(GetStyle() | WS_CHILD | WS_VISIBLE | WS_TABSTOP);
	#endif
	SetTabStop(true);
}

GTree::~GTree()
{
	Empty();
	DeleteObj(d);
}

// Internal tree methods
void GTree::_Update(GRect *r, bool Now)
{
	if (r)
	{
		GRect u = *r;
		GdcPt2 s = _ScrollPos();
		GRect c = GetClient();
		u.Offset(c.x1-s.x, c.y1-s.y);
		Invalidate(&u, Now && !d->InPour);
	}
	else
	{
		Invalidate((GRect*)0, Now && !d->InPour);
	}
}

void GTree::_UpdateBelow(int y, bool Now)
{
	GdcPt2 s = _ScrollPos();
	GRect c = GetClient();
	GRect u(c.x1, y - s.y + c.y1, X()-1, Y()-1);
	Invalidate(&u, Now);
}

GdcPt2 GTree::_ScrollPos()
{
	/*
	int All = _Limit.y / TREE_BLOCK - 1;
	int Visible = Y() / TREE_BLOCK;
	*/
	GdcPt2 Status;

	Status.x = (HScroll) ? (int)HScroll->Value() : 0;
	Status.y = (VScroll) ? (int)VScroll->Value() * TREE_BLOCK : 0;

	return Status;
}

void GTree::_UpdateScrollBars()
{
	static bool Processing = false;
	if (!Processing)
	{
		Processing = true;

		GdcPt2 Old = _ScrollPos();
		
		GRect Client = GetClient();
		bool x = d->Limit.x > Client.X();
		bool y = d->Limit.y > Client.Y();
		SetScrollBars(x, y);
		Client = GetClient();

		// x scroll... in pixels
		if (HScroll)
		{
			HScroll->SetLimits(0, d->Limit.x-1);
			HScroll->SetPage(Client.X());

			int Max = d->Limit.x - Client.X();
			if (HScroll->Value() > Max)
			{
				HScroll->Value(Max+1);
			}
		}

		// y scroll... in items
		if (VScroll)
		{
			int All = d->Limit.y / TREE_BLOCK;
			int Visible = Client.Y() / TREE_BLOCK;

			VScroll->SetLimits(0, All - 1);
			VScroll->SetPage(Visible);

			/*
			int Max = All - Visible + 1;
			if (VScroll->Value() > Max)
			{
				VScroll->Value(Max);
			}
			*/
		}

		GdcPt2 New = _ScrollPos();
		if (Old.x != New.x ||
			Old.y != New.y)
		{
			Invalidate();
		}

		Processing = false;
	}
}

void GTree::_OnSelect(GTreeItem *Item)
{
	if (!MultipleSelect)
	{
		for (GTreeItem *i=d->Selection.First(); i; i=d->Selection.Next())
		{
			if (i != Item)
			{
				i->Select(false);
			}
		}

		d->Selection.Empty();
	}
	else
	{
		d->Selection.Delete(Item);
	}

	d->Selection.Insert(Item);
}

void GTree::_Pour()
{
	d->InPour = true;
	d->Limit.x = d->Limit.y = 0;

	GTreeItem *n;
	for (GTreeItem *i=Items.First(); i; i=n)
	{
		n = Items.Next();
		i->d->Last = n == 0;
		i->_Pour(&d->Limit, 0, true);
	}

	_UpdateScrollBars();
	d->LayoutDirty = false;
	d->InPour = false;
}

// External methods and events
void GTree::OnItemSelect(GTreeItem *Item)
{
	if (Item)
	{
		Item->OnSelect();
		SendNotify(GLIST_NOTIFY_SELECT);
	}
}

void GTree::OnItemExpand(GTreeItem *Item, bool Expand)
{
	if (Item)
	{
		Item->OnExpand(Expand);
	}
}

GTreeItem *GTree::GetAdjacent(GTreeItem *i, bool Down)
{
	if (i)
	{
		if (Down)
		{
			GTreeItem *n = i->GetChild();
			
			if (!n ||
				!n->d->Visible)
			{
				for (n = i; n; )
				{
					GTreeItem *p = n->GetParent();
					if (p)
					{
						int Index = n->IndexOf();
						if (Index < p->Items.Length()-1)
						{
							n = n->GetNext();
							break;
						}
						else
						{
							n = p;
						}
					}
					else
					{
						n = n->GetNext();
						break;
					}
				}
			}

			return n;
		}
		else
		{
			GTreeItem *p = i->GetParent() ? i->GetParent() : 0;
			int Index = i->IndexOf();
			if (p)
			{
				GTreeItem *n = p;
				if (Index > 0)
				{
					n = i->GetPrev();
					while (	n->GetChild() &&
							n->GetChild()->d->Visible)
					{
						n = n->Items.ItemAt(n->Items.Length()-1);
					}
				}

				if (n)
				{
					return n;
				}
			}
			else if (Index > 0)
			{
				p = i->GetTree()->ItemAt(Index - 1);
				while (p->GetChild() &&
						p->GetChild()->d->Visible)
				{
					if (p->Items.First())
					{
						p = p->Items.ItemAt(p->Items.Length()-1);
					}
					else break;
				}
				return p;
			}
		}
	}
	
	return 0;
}

bool GTree::OnKey(GKey &k)
{
	bool Status = false;
	
	GTreeItem *i = d->Selection.First();
	if (!i)
	{
		i = Items.First();
		if (i)
		{
			i->Select();
		}
	}

	if (k.Down())
	{
		switch (k.vkey)
		{
			case VK_PAGEUP:
			case VK_PAGEDOWN:
			{
				if (i && i->d->Pos.Y() > 0)
				{
					int Page = GetClient().Y() / i->d->Pos.Y();
					for (int j=0; j<Page; j++)
					{
						GTreeItem *n = GetAdjacent(i, k.c16 == VK_PAGEDOWN);
						if (n)
						{
							i = n;
						}
						else break;
					}
					if (i)
					{
						i->Select(true);
						i->ScrollTo();
					}
				}
				Status = true;
				break;
			}
			case VK_HOME:
			{
				GTreeItem *i;
				if ((i = Items.First()))
				{
					i->Select(true);
					i->ScrollTo();
				}
				Status = true;
				break;
			}
			case VK_END:
			{
				GTreeItem *n = i, *p = 0;
				while ((n = GetAdjacent(n, true)))
				{
					p = n;
				}
				if (p)
				{
					p->Select(true);
					p->ScrollTo();
				}
				Status = true;
				break;
			}
			case VK_LEFT:
			{
				if (i)
				{
					if (i->Items.First() && i->Expanded())
					{
						i->Expanded(false);
						break;
					}
					else
					{
						GTreeItem *p = i->GetParent();
						if (p)
						{
							p->Select(true);
							p->Expanded(false);
							_Pour();
							break;
						}
					}
				}
				// fall thru
			}
			case VK_UP:
			{
				GTreeItem *n = GetAdjacent(i, false);
				if (n)
				{
					n->Select(true);
					n->ScrollTo();
				}
				Status = true;
				break;
			}
			case VK_RIGHT:
			{
				if (i)
				{
					i->Expanded(true);
					if (d->LayoutDirty)
					{
						_Pour();
						break;
					}
				}
				// fall thru
			}
			case VK_DOWN:
			{
				GTreeItem *n = GetAdjacent(i, true);
				if (n)
				{
					n->Select(true);
					n->ScrollTo();
				}
				Status = true;
				break;
			}
			#ifdef VK_APPS
			case VK_APPS:
			{
				GTreeItem *s = Selection();
				if (s)
				{
					GRect *r = &s->d->Pos;
					if (r)
					{
						GdcPt2 Scroll = _ScrollPos();
						
						GMouse m;
						m.x = r->x1 + (r->X() >> 1) - Scroll.x;
						m.y = r->y1 + (r->Y() >> 1) - Scroll.y;
						m.Target = this;
						m.ViewCoords = true;
						m.Down(true);
						m.Right(true);
						
						s->OnMouseClick(m);
					}
				}
				break;
			}
			#endif
			default:
			{
				// LgiTrace("Key c16=%i, down=%i\n", k.c16, k.Down());
				break;
			}
		}
	}

	if (i && i != (GTreeItem*)this)
	{
		i->OnKey(k);
	}

	return Status;
}

GTreeItem *GTree::ItemAtPoint(int x, int y)
{
	GdcPt2 s = _ScrollPos();

	for (GTreeItem *i = Items.First(); i; i=Items.Next())
	{
		GTreeItem *Hit = i->_HitTest(s.x + x, s.y + y);
		if (Hit)
		{
			return Hit;
		}
	}

	return 0;
}

bool GTree::OnMouseWheel(double Lines)
{
	if (VScroll)
	{
		VScroll->Value(VScroll->Value() + (int)Lines);
	}
	
	return true;
}

void GTree::OnMouseClick(GMouse &m)
{
	GRect c = GetClient();

	m.x -= c.x1;
	m.y -= c.y1;

	if (m.Down())
	{
		Focus(true);
	}
	if (m.Down())
	{
		Capture(true);
		d->LastClick.x = m.x;
		d->LastClick.y = m.y;
	}
	else if (IsCapturing())
	{
		Capture(false);
	}

	d->LastHit = ItemAtPoint(m.x, m.y);
	if (d->LastHit)
	{
		GdcPt2 c = _ScrollPos();
		m.x += c.x;
		m.y += c.y;
		d->LastHit->_MouseClick(m);
	}
}

void GTree::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		if (abs(d->LastClick.x - m.x) > DRAG_THRESHOLD ||
			abs(d->LastClick.y - m.y) > DRAG_THRESHOLD)
		{
			OnItemBeginDrag(d->LastHit, m.Flags);
			Capture(false);
		}
	}
}

void GTree::OnPosChange()
{
	GLayout::OnPosChange();
	_UpdateScrollBars();
}

void GTree::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();

	d->IconTextGap = GetFont()->GetHeight() / 6;

	// icon cache
	if (GetImageList() &&
		!d->IconCache)
	{
		int Bits = GdcD->GetBits();
		int CacheHeight = max(SysFont->GetHeight(), GetImageList()->Y());
		
		d->IconCache = new GMemDC;
		if (d->IconCache &&
			d->IconCache->Create(GetImageList()->X(), CacheHeight, Bits))
		{
			if (Bits <= 8)
			{
				d->IconCache->Palette(new GPalette(GdcD->GetGlobalColour()->GetPalette()));
			}

			d->IconCache->Colour(LC_WORKSPACE, 24);
			d->IconCache->Rectangle();
			d->IconCache->Op(GDC_ALPHA);

			GetImageList()->Lock();
			int DrawY = (CacheHeight - GetImageList()->TileY()) >> 1;
			LgiAssert(DrawY >= 0);
			for (int i=0; i<GetImageList()->GetItems(); i++)
			{
				GetImageList()->Draw(d->IconCache, i * GetImageList()->TileX(), DrawY, i);
			}
			GetImageList()->Unlock();
			d->IconCache->Unlock();
		}
	}

	// scroll
	GdcPt2 s = _ScrollPos();
	int Ox, Oy;
	pDC->GetOrigin(Ox, Oy);
	pDC->SetOrigin(Ox + (s.x - r.x1), Oy + (s.y - r.y1));

	// selection colour
	GItem::ItemPaintCtx Ctx;
	Ctx.pDC = pDC;
	COLOUR SelFore = Focus() ? LC_FOCUS_SEL_FORE : LC_NON_FOCUS_SEL_FORE;
	COLOUR SelBack = Focus() ? LC_FOCUS_SEL_BACK : LC_NON_FOCUS_SEL_BACK;

	// layout items
	if (d->LayoutDirty)
	{
		_Pour();
	}

	// paint items
	ZeroObj(d->LineFlags);
	for (GTreeItem *i = Items.First(); i; i=Items.Next())
	{
		bool IsSelected = (d->DropTarget == i) || (d->DropTarget == 0 && i->Select());

		// Foreground
		GViewFill *Fill = i->GetForegroundFill();
		Ctx.Fore = Fill ? Fill->GetFlat().c24() : (IsSelected ? SelFore : LC_TEXT);

		// Background	
		Fill = i->GetBackgroundFill();
		Ctx.Back =	Fill ?
					Fill->GetFlat().c24() :
					(IsSelected ? SelBack : LC_WORKSPACE);

		i->OnPaint(Ctx);
	}

	pDC->SetOrigin(Ox, Oy);
	if (d->Limit.y-s.y < r.Y())
	{
		// paint after items
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(r.x1, r.y1 + d->Limit.y - s.y, r.x2, r.y2);
	}
}

int GTree::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_HSCROLL:
		case IDC_VSCROLL:
		{
			Invalidate();
			break;
		}
	}

	return GLayout::OnNotify(Ctrl, Flags);
}

GMessage::Result GTree::OnEvent(GMessage *Msg)
{
	return GLayout::OnEvent(Msg);
}

GTreeItem *GTree::Insert(GTreeItem *Obj, int Pos)
{
	GTreeItem *NewObj = GTreeNode::Insert(Obj, Pos);
	if (NewObj)
	{
		NewObj->_SetTreePtr(this);
	}

	return NewObj;
}

bool GTree::Remove(GTreeItem *Obj)
{
	if (Obj &&
		Obj->Tree == this)
	{
		Obj->Remove();
		return true;
	}
	return false;
}

void GTree::RemoveAll()
{
	for (GTreeItem *i=Items.First(); i; i=Items.First())
	{
		i->_Remove();
	}

	Invalidate();
}

void GTree::Empty()
{
	GTreeItem *i;
	while ((i = Items.First()))
	{
		Delete(i);
	}
}

bool GTree::Delete(GTreeItem *Obj)
{
	bool Status = false;
	
	if (Obj)
	{
		GTreeItem *i;
		while ((i = Obj->Items.First()))
		{
			Delete(i);
		}
		
		Obj->Remove();
		DeleteObj(Obj);
		Status = true;
	}
	
	return Status;
}

void GTree::OnPulse()
{
	if (d->DropTarget)
	{
		int64 p = LgiCurrentTime() - d->DropSelectTime;
		if (p >= 1000)
		{
			SetPulse();

			if (!d->DropTarget->Expanded() &&
				d->DropTarget->GetChild())
			{
				d->DropTarget->Expanded(true);
			}
		}
	}	

	if (InsideDragOp())
	{
		GMouse m;
		if (GetMouse(m))
		{
			GRect c = GetClient();
			if (VScroll)
			{
				if (m.y < DRAG_SCROLL_EDGE)
				{
					// Scroll up...
					VScroll->Value(VScroll->Value() - DRAG_SCROLL_Y);
				}
				else if (m.y > c.Y() - DRAG_SCROLL_EDGE)
				{
					// Scroll down...
					VScroll->Value(VScroll->Value() + DRAG_SCROLL_Y);
				}
			}

			if (HScroll)
			{
				if (m.x < DRAG_SCROLL_EDGE)
				{
					// Scroll left...
					HScroll->Value(HScroll->Value() - DRAG_SCROLL_X);
				}
				else if (m.x > c.X() - DRAG_SCROLL_EDGE)
				{
					// Scroll right...
					HScroll->Value(HScroll->Value() + DRAG_SCROLL_X);
				}
			}
		}
	}
}

void GTree::OnDragEnter()
{
	InsideDragOp(true);
	SetPulse(120);
}

void GTree::OnDragExit()
{
	InsideDragOp(false);
	SetPulse();
	SelectDropTarget(0);
}

void GTree::SelectDropTarget(GTreeItem *Item)
{
	if (Item != d->DropTarget)
	{
		bool Update = (d->DropTarget != 0) ^ (Item != 0);
		GTreeItem *Old = d->DropTarget;
		
		d->DropTarget = Item;
		if (Old)
		{
			Old->Update();
		}

		if (d->DropTarget)
		{
			d->DropTarget->Update();
			d->DropSelectTime = LgiCurrentTime();
		}

		if (Update)
		{
			OnFocus(true);
		}
	}
}

bool GTree::Select(GTreeItem *Obj)
{
	bool Status = false;
	if (Obj && IsAttached())
	{
		Obj->Select(true);
		Status = true;
	}
	else if (d->Selection.Length())
	{		
		d->Selection.Empty();
		OnItemSelect(0);
		Status = true;
	}
	return Status;
}

GTreeItem *GTree::Selection()
{
	return d->Selection.First();
}

void GTree::OnItemClick(GTreeItem *Item, GMouse &m)
{
	if (Item)
	{
		Item->OnMouseClick(m);
	}
}

void GTree::OnItemBeginDrag(GTreeItem *Item, int Flags)
{
	if (Item)
	{
		GMouse m;
		m.x = m.y = 0;
		m.Target = NULL;
		m.ViewCoords = false;
		m.Flags = Flags;
		Item->OnBeginDrag(m);
	}
}

void GTree::OnFocus(bool b)
{
	// errors during deletion of the control can cause 
	// this to be called after the destructor
	if (d)
	{
		for (GTreeItem *i=d->Selection.First(); i; i=d->Selection.Next())
		{
			i->Update();
		}
	}
}

static void GTreeItemUpdateAll(GTreeNode *n)
{
	for (GTreeItem *i=n->GetChild(); i; i=i->GetNext())
	{
		i->Update();
		GTreeItemUpdateAll(i);
	}
}

void GTree::UpdateAllItems()
{
	GTreeItemUpdateAll(this);
}
