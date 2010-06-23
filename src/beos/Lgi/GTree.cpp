//////////////////////////////////////////////////////////////////////////////
GTreeItem::GTreeItem()
{
	Str = 0;
	Tree = 0;
	Parent = 0;
}

GTreeItem::~GTreeItem()
{
	if (Tree)
	{
		Tree->Remove(this);
	}

	DeleteArray(Str);
}

GTreeItem *GTreeItem::GetChild()
{
	if (Tree)
	{
		BListItem *Bi = Tree->ItemUnderAt(this, TRUE, 0);
		GTreeItem *Ti = dynamic_cast<GTreeItem*>(Bi);
		return Ti;
	}
	return 0;
}

GTreeItem *GTreeItem::GetNext()
{
	if (Tree)
	{
		int MyIndex = Tree->IndexOf(this);
		BListItem *Super = Tree->Superitem(this);
		BListItem *Bi = 0;
		if (Super)
		{
			Bi = Tree->ItemUnderAt(Super, TRUE, MyIndex + 1);
		}
		else
		{
			Tree->ItemAt(MyIndex + 1);
		}
		GTreeItem *Ti = dynamic_cast<GTreeItem*>(Bi);
		return Ti;
	}
	return 0;
}

char *GTreeItem::GetText(int i)
{
	return Str;
}

bool GTreeItem::SetText(char *s, int i)
{
	DeleteArray(Str);
	Str = NewStr(s);
	return Str != 0;
}

int GTreeItem::GetImage(int Flags)
{
	return Sys_Image;
}

void GTreeItem::SetImage(int i)
{
	Sys_Image = i;
}

int GTreeCompareFunc(const BListItem *a, const BListItem *b)
{
	return ((GTreeItem*)a)->Index - ((GTreeItem*)b)->Index;
}

#define ITEM_HEIGHT 16.0

GTreeItem *GTreeItem::Insert(GTreeItem *Obj, int Pos)
{
	GTreeItem *NewObj = (Obj) ? Obj : NEW(GTreeItem);
	if (NewObj AND Tree)
	{
		bool Lock = Tree->LockLooper();
		Obj->Index = Tree->CountItemsUnder(this, true);
		Obj->Tree = Tree;
		Obj->Parent = this;
		Tree->AddUnder(Obj, this);
		Tree->SortItemsUnder(this, true, GTreeCompareFunc);
		if (Lock) Tree->UnlockLooper();
	}
	return NewObj;
}

void GTreeItem::Update()
{
	if (Parent)
	{
		if (Tree->LockLooper())
		{
			int32 i = Tree->IndexOf(this); // FullList
			BOutlineListView *t = dynamic_cast<BOutlineListView*>(Tree->GetBView());
			if (t)
			{
				BRect r = t->ItemFrame(i);
				t->Invalidate(r);
			}
			
			Tree->UnlockLooper();
		}
	}
}

bool GTreeItem::Selected()
{
	return IsSelected();
}

void GTreeItem::Selected(bool b)
{
	if (Tree)
	{
		Tree->BOutlineListView::Select(Tree->FullListIndexOf(this));
	}
}

bool GTreeItem::Expanded()
{
	return IsExpanded();
}

void GTreeItem::Expanded(bool b)
{
	SetExpanded(b);
}

void GTreeItem::DrawItem(BView *owner, BRect frame, bool complete)
{
	rgb_color color;
	if (IsSelected())
	{
		color.red = 192;
		color.green = 192;
		color.blue = 192;
	}
	else
	{
		color = owner->ViewColor();
	}

	owner->SetHighColor(color);
	owner->SetLowColor(color);
	owner->FillRect(frame);
	
	int Image = -1;
	int x = 4;
	if (Tree->ImageList)
	{
		Image = GetImage(Selected());
		x += 16;
	}

	char *Text = GetText(0);
	if (NOT Text)
	{
		Text = Str;
	}
	if (Text)
	{
		owner->MovePenTo(frame.left+x, frame.bottom-2);
		owner->SetHighColor(IsSelected() ? BeRGB(255, 255, 255) : BeRGB(0, 0, 0));
		owner->DrawString(Text);
	}

	if (Image >= 0)
	{
		int x = Image * 16;
		BBitmap *Bmp = Tree->ImageList->Handle();
		BRect S(x, 0, x+15, 15);
		BRect D(frame.left, frame.top, frame.left+15, frame.top+15);
		drawing_mode Mode = owner->DrawingMode();
		owner->SetDrawingMode(B_OP_OVER);
		owner->DrawBitmapAsync(Bmp, S, D);
		owner->SetDrawingMode(Mode);
	}
}

void GTree::OnItemSelect(GTreeItem *Item)
{
	if (Item)
	{
		Item->OnSelect();
	}
}

void GTree::OnItemExpand(GTreeItem *Item, bool Expand)
{
	if (Item)
	{
		Item->OnExpand(Expand);
	}
}

//////////////////////////////////////////////////////////////////////////////
GTree::GTree(int id, int x, int y, int cx, int cy, char *name) :
	GControl(this),
	BOutlineListView(	BRect(x, y, x+cx-14, y+cy-14),
						"List",
						B_SINGLE_SELECTION_LIST),
	ResObject(Resources, Res_TreeView)
{
	SetId(id);
	Pos.Set(x, y, x+cx, y+cy);
	if (name) GControl::Name(name);

	Lines = true;
	Buttons = true;
	LinesAtRoot = true;
	EditLabels = false;
	ScrollView = 0;
	SetViewColor(255, 255, 255);
}

GTree::~GTree()
{
	Lines = false;
}

bool GTree::SetPos(GRect &p, bool Repaint)
{
	ScrollView->MoveTo(p.x1, p.y1);
	ScrollView->ResizeTo(p.X()-1, p.Y()-1);
	ResizeTo(p.X()-16, p.Y()-16);
	Pos = p;
	return TRUE;
}

bool GTree::Attach(GViewI *parent)
{
	SetParent(parent);
	if (GetParent())
	{
		ScrollView = NEW(BScrollView(	"scroll_cities",
										this,
										B_FOLLOW_LEFT | B_FOLLOW_TOP,
										0,
										TRUE,
										TRUE,
										B_NO_BORDER));
		if (ScrollView)
		{
			GetParent()->GetBView()->AddChild(ScrollView);
			ScrollView->MoveTo(Pos.x1, Pos.y1);
			ScrollView->ResizeTo(Pos.X() + 3, Pos.Y() + 3);

			// standard white background where we don't draw
			ScrollView->SetViewColor(255, 255, 255);

			// set font size
			BFont f;
			GetBView()->GetFont(&f);
			f.SetSize(11.0);
			GetBView()->SetFont(&f);

			return TRUE;
		}
	}
	return FALSE;
}

void GTree::MouseDown(BPoint point)
{	
	MouseClickEvent(true);
	BOutlineListView::MouseDown(point);
	MouseClickEvent(false);
}

void GTree::SelectionChanged()
{
	int32 Index = FullListCurrentSelection();
	if (Index >= 0)
	{
		GTreeItem *Item = (GTreeItem*) FullListItemAt(Index);
		if (Item)
		{
			OnItemSelect(Item);
		}
	}
}

void GTree::OnMouseClick(GMouse &m)
{
	if (LockLooper())
	{
		int32 Index = FullListCurrentSelection();
		if (Index >= 0)
		{
			GTreeItem *Item = (GTreeItem*) FullListItemAt(Index);
			if (Item)
			{
				OnItemClick(Item, m);
			}
		}
		
		UnlockLooper();
	}
}

int GTree::OnEvent(BMessage *Msg)
{
	return GView::OnEvent(Msg);
}

GTreeItem *GTree::Insert(GTreeItem *Obj, int Pos)
{
	GTreeItem *NewObj = (Obj) ? Obj : NEW(GTreeItem);
	if (NewObj)
	{
		NewObj->Tree = this;
		NewObj->Parent = NULL;
		
		bool Lock = LockLooper();
		AddItem(NewObj, (Pos < 0) ? 100000000 : Pos);
		if (Lock) UnlockLooper();
	}
	return NewObj;
}

bool GTree::Remove(GTreeItem *Obj)
{
	if (Obj)
	{
		RemoveItem(Obj);
		return TRUE;
	}
	return FALSE;
}

void GTree::RemoveAll()
{
	MakeEmpty();
}

bool GTree::Delete(GTreeItem *Obj)
{
	bool Status = FALSE;
	if (Obj)
	{
		RemoveItem(Obj);
	}
	return Status;
}

bool GTree::Select(GTreeItem *Obj)
{
	bool Status = FALSE;
	if (Obj)
	{
		int32 Index = IndexOf(Obj);
		BListView::Select(Index);
	}
	return Status;
}

GTreeItem *GTree::Selection()
{
	GTreeItem *Item = 0;
	int Index = CurrentSelection(0);
	if (Index >= 0)
	{
		Item = (GTreeItem*) ItemAt(Index);
	}
	return Item;
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
		ZeroObj(m);
		m.Flags = Flags;
		Item->OnBeginDrag(m);
	}
}

