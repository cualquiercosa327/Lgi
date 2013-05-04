#include "Lgi.h"
#include "GDataGrid.h"
#include "GEdit.h"
#include "GCombo.h"

enum Controls
{
	IDC_DELETE = 2000,
	IDC_EDIT,
};
#define M_DELETE_LATER		(M_USER+2000)
#define CELL_EDGE			2

struct GDataGridPriv
{
	GDataGrid *This;
	int Col;
	GView *e;
	GView *DeleteLater;
	GListItem *Cur;
	bool Dirty, PosDirty;
	GArray<GDataGrid::GDataGridFlags> Flags;
	GArray<GVariant> ColumnArgs;
	GListItem *NewRecord;
	GDataGrid::ItemFactory Factory;
	void *UserData;
	GDataGrid::ItemArray Dropped;
	GDataGrid::IndexArray Deleted;
	char *SrcFmt;
	char *AcceptFmt;

	GDataGridPriv(GDataGrid *t);
	void Save();
	void UpdatePos();
	void Create(int NewCol = -1);
	void MoveCell(int dx);
	void Invalidate();

	GListItem *NewItem()
	{
		if (Factory)
			return Factory(UserData);
		
		return new GListItem;
	}
};

class GDataGridEdit : public GEdit
{
	GDataGridPriv *d;

public:
	GDataGridEdit(GDataGridPriv *data, int id, int x, int y, int cx, int cy, char *txt) :
		GEdit(id, x, y, cx, cy, txt)
	{
		d = data;
	}

	bool OnKey(GKey &k)
	{
		if (!GEdit::OnKey(k) && k.Down())
		{
			if (k.IsChar)
			{
			}
			else
			{
				switch (k.vkey)
				{
					case VK_RETURN:
					{
						GListItem *s = d->This->GetSelected();
						int Idx = s ? d->This->IndexOf(s) : -1;
						d->Save();
						if (Idx >= 0)
						{
							s->Select(false);
							if ((s = d->This->ItemAt(Idx + 1)))
								s->Select(true);

						}
						break;
					}
					case VK_LEFT:
					{
						int Start, Len;
						if (GetSelection(Start, Len))
						{
							break;
						}

						if (GetCaret() ==  0)
						{
							d->MoveCell(-1);
							return true;
						}
						break;
					}
					case VK_RIGHT:
					{
						int Start, Len;
						if (GetSelection(Start, Len))
						{
							break;
						}

						char16 *Txt = NameW();
						Len = Txt ? StrlenW(Txt) : 0;
						int Cursor = GetCaret();
						if (Cursor >= Len)
						{
							d->MoveCell(1);
							return true;
						}
						break;
					}
					case VK_PAGEUP:
					case VK_PAGEDOWN:
					case VK_UP:
					case VK_DOWN:
					{
						bool b = d->This->OnKey(k);
						d->Invalidate();
						return b;
						break;
					}
				}
			}
		}

		return false;
	}
};

class GDataGridCombo : public GCombo
{
	GDataGridPriv *d;

public:
	GDataGridCombo(GDataGridPriv *priv, int id, GRect &rc) : GCombo(id, rc.x1, rc.y1, rc.X(), rc.Y(), 0)
	{
		d = priv;
	}
	
	bool OnKey(GKey &k)
	{
		if (!k.IsChar && k.Down())
		{
			if (k.c16 == VK_LEFT)
			{
				d->MoveCell(-1);
			}
			else if (k.c16 == VK_RIGHT)
			{
				d->MoveCell(1);
			}
		}
		
		return GCombo::OnKey(k);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
GDataGridPriv::GDataGridPriv(GDataGrid *t)
{
	Factory = 0;
	UserData = 0;
	SrcFmt = AcceptFmt = 0;
	DeleteLater = NULL;

	NewRecord = 0;
	Dirty = false;
	PosDirty = false;
	Cur = 0;
	This = t;
	Col = 0;
	e = 0;
}

void GDataGridPriv::Save()
{
	if (Dirty && Cur && e)
	{
		if (Flags[Col] & GDataGrid::GDG_INTEGER)
		{
			int64 OldVal = e->Value();
			if (OldVal < 0 && Cur == NewRecord)
				return;

			char t[64];
			sprintf(t, LGI_PrintfInt64, OldVal);
			Cur->SetText(t, Col);
		}
		else
		{
			char *OldTxt = e->Name();
			if (!OldTxt && Cur == NewRecord)
				return;

			Cur->SetText(OldTxt, Col);
		}

		if (Cur == NewRecord)
		{
			This->Insert(NewRecord = NewItem());
		}
	}
}

void GDataGridPriv::UpdatePos()
{
	if (e && Cur)
	{
		GRect *r = Cur->GetPos(Col);
		if (r)
		{
			GRect rc = *r;

			if (Flags[Col] & GDataGrid::GDG_INTEGER)
			{
				rc.Offset(2, 0);
			}
			else
			{
				rc.x2--;
				rc.y2--;
			}

			e->SetPos(rc);
			e->Focus(true);
			
			
			PosDirty = false;
		}
	}
}

void GDataGridPriv::Invalidate()
{
	if (e)
	{
		GRect p = e->GetPos();
		p.Size(-CELL_EDGE, -CELL_EDGE);
		This->Invalidate(&p);
	}
}

void GDataGridPriv::MoveCell(int Dx)
{
	for (int i=Col+Dx; i>=0 && i<This->GetColumns(); i+=Dx)
	{
		if (!(Flags[i] & GDataGrid::GDG_READONLY))
		{
			Create(i);
			break;
		}
	}
}

void GDataGridPriv::Create(int NewCol)
{
	if (!This->IsAttached())
		return;

	GListItem *i = This->GetSelected();
	if (!i)
		return;

	if (!Cur || i != Cur || (NewCol >= 0 && NewCol != Col))
	{
		int OldCtrl = Flags[Col] & GDataGrid::GDG_INTEGER;

		Save();
		Cur = i;

		if (NewCol >= 0)
			Col = NewCol;

		if (Flags[Col] & GDataGrid::GDG_READONLY)
		{
			// Pick a valid column
			int NewCol = -1;
			for (int i=0; i<This->GetColumns(); i++)
			{
				if (Col != i && !(Flags[i] & GDataGrid::GDG_READONLY))
				{
					NewCol = i;
					break;
				}
			}

			if (NewCol < 0)
				return;

			Col = NewCol;
		}

		int NewCtrl = Flags[Col] & GDataGrid::GDG_INTEGER;

		char *CurText = i->GetText(Col);
		GRect *r = i->GetPos(Col);
		if (r)
		{
			GDataGridEdit *Edit = 0;
			GCombo *Combo = 0;
			GRect rc = r;
			
			if (!e || (NewCtrl ^ OldCtrl))
			{
				if (e)
				{
					e->Detach();
					Invalidate();
					DeleteLater = e;
					e = NULL;					
					This->PostEvent(M_DELETE_LATER);
				}

				if (Flags[Col] & GDataGrid::GDG_INTEGER)
				{
                    e = Combo = new GDataGridCombo(this, IDC_EDIT, rc);
					if (e)
					{
						GVariant &e = ColumnArgs[Col];
						if (e.Type == GV_LIST)
						{
							for (GVariant *v = e.Value.Lst->First(); v; v = e.Value.Lst->Next())
							{
								char *s = v->Str();
								Combo->Insert(s);
							}
						}

						if (CurText)
							Combo->Name(CurText);
						else
							Combo->Value(-1);
					}
				}
				else
				{
					rc.x2 -= 1;
					rc.y2 -= 1;

					// Create edit control with the correct info for the cell
                    e = Edit = new GDataGridEdit(this, IDC_EDIT, rc.x1, rc.y1, rc.X(), rc.Y(), CurText);
					if (e)
					{
						e->Sunken(false);
					}
				}

				e->SetPos(rc);
				e->Attach(This);
				Invalidate();
			}
			else
			{
				Invalidate();
				e->SetPos(rc);
				e->Name(CurText);
				Invalidate();
			}

			if (Edit)
				Edit->Select(0);
			e->Focus(true);
		}			
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GDataGrid::GDataGrid(int CtrlId, ItemFactory Func, void *userdata) :
	GList(CtrlId, 0, 0, 1000, 1000)
{
	_ObjName = Res_Custom;

	DrawGridLines(true);
	SetPourLargest(true);

	d = new GDataGridPriv(this);
	SetFactory(Func, userdata);
}

GDataGrid::~GDataGrid()
{
	DeleteObj(d);
}

bool GDataGrid::Remove(GListItem *Obj)
{
	if (Obj == d->Cur)
	{
		DeleteObj(d->e);
		d->Cur = 0;
	}
	return GList::Remove(Obj);
}

void GDataGrid::Empty()
{
	d->NewRecord = 0;
	d->Cur = 0;
	DeleteObj(d->e);
	
	GList::Empty();
}

void GDataGrid::OnItemSelect(GArray<GListItem*> &Items)
{
	if (Items.Length() == 1)
	{
		d->Create();
	}
	else if (d->e)
	{
		d->Cur = 0;
		d->DeleteLater = d->e;
		d->e = NULL;
		PostEvent(M_DELETE_LATER);
	}

	GList::OnItemSelect(Items);
}

void GDataGrid::OnItemClick(GListItem *Item, GMouse &m)
{
	if (m.IsContextMenu())
	{
		GSubMenu s;
		s.AppendItem("Delete", IDC_DELETE);
		m.ToScreen();
		switch (s.Float(this, m.x, m.y, m.Left()))
		{
			case IDC_DELETE:
			{
				List<GListItem> Sel;
				GetSelection(Sel);
				d->Deleted.Length(0);
				for (GListItem *i=Sel.First(); i; i=Sel.Next())
				{
					d->Deleted.Add(IndexOf(i));
				}
				SendNotify(GLIST_NOTIFY_DELETE);
				d->Deleted.Length(0);

				Sel.Delete(d->NewRecord);
				Sel.DeleteObjects();
				break;
			}
		}
	}
	else
	{
		int ClickCol = ColumnAtX(m.x);
		if (ClickCol != d->Col)
		{
			d->Create(ClickCol);
		}
	}
}

void GDataGrid::OnCreate()
{
	d->Create(0);
	SetWindow(this);
}

GMessage::Result GDataGrid::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_DELETE_LATER:
		{
			DeleteObj(d->DeleteLater);
			break;
		}
	}

	return GList::OnEvent(Msg);
}

int GDataGrid::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_EDIT:
		{
			d->Dirty = true;
			break;
		}
		case IDC_VSCROLL:
		{
			d->PosDirty = true;
			break;
		}
	}

	return GList::OnNotify(c, f);
}

void GDataGrid::SetColFlag(int Col, GDataGridFlags Flags, GVariant *Arg)
{
	d->Flags[Col] = Flags;
	if (Arg)
		d->ColumnArgs[Col] = *Arg;
}

bool GDataGrid::OnMouseWheel(double Lines)
{
	GList::OnMouseWheel(Lines);
	d->UpdatePos();
	return true;
}

void GDataGrid::OnPaint(GSurface *pDC)
{
	GList::OnPaint(pDC);

	d->UpdatePos();
	if (d->e && !(d->Flags[d->Col] & GDG_INTEGER))
	{
		pDC->Colour(LC_BLACK, 24);
		GRect p = d->e->GetPos();
		p.Size(-CELL_EDGE, -CELL_EDGE);
		pDC->Box(&p);
	}
}

bool GDataGrid::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = 16;
	for (int i=0; i<GetColumns(); i++)
	{
		Inf.Width.Min += ColumnAt(i)->Width();
	}
	Inf.Width.Max = Inf.Width.Min + 20;
	
	Inf.Height.Min = 50;
	Inf.Height.Max = 2000;
	
	return true;
}

bool GDataGrid::CanAddRecord()
{
	return d->NewRecord;
}

void GDataGrid::CanAddRecord(bool b)
{
	if (b)
	{
		if (!d->NewRecord)
			Insert(d->NewRecord = d->NewItem());
	}
	else
	{
		DeleteObj(d->NewRecord);
	}
}

GListItem *GDataGrid::NewItem()
{
	return d->NewItem();
}

void GDataGrid::SetFactory(ItemFactory Func, void *userdata)
{
	d->Factory = Func;
	d->UserData = userdata;
}

void GDataGrid::SetDndFormats(char *SrcFmt, char *AcceptFmt)
{
	d->SrcFmt = SrcFmt;
	d->AcceptFmt = AcceptFmt;
}

int GDataGrid::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	for (char *f=Formats.First(); f; f=Formats.Next())
	{
		if (d->AcceptFmt && !stricmp(f, d->AcceptFmt))
		{
			return DROPEFFECT_COPY;
		}
	}

	return DROPEFFECT_NONE;
}

GDataGrid::ItemArray *GDataGrid::GetDroppedItems()
{
	return &d->Dropped;
}

GDataGrid::IndexArray *GDataGrid::GetDeletedItems()
{
	return &d->Deleted;
}

int GDataGrid::OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState)
{
	if (d->AcceptFmt &&
		!stricmp(Format, d->AcceptFmt) &&
		Data->Type == GV_BINARY)
	{
		GListItem **Item = (GListItem**)Data->Value.Binary.Data;
		int Items = Data->Value.Binary.Length / sizeof(GListItem*);
		d->Dropped.Length(0);
		for (int i=0; i<Items; i++)
		{
			d->Dropped.Add(Item[i]);
		}
		SendNotify(GLIST_NOTIFY_ITEMS_DROPPED);
		return DROPEFFECT_COPY;
	}

	return DROPEFFECT_NONE;
}

void GDataGrid::OnItemBeginDrag(GListItem *Item, GMouse &m)
{
	Drag(this, DROPEFFECT_COPY);
}

bool GDataGrid::GetFormats(List<char> &Formats)
{
	if (!d->SrcFmt)
		return false;

	Formats.Insert(NewStr(d->SrcFmt));
	return true;
}

bool GDataGrid::GetData(GVariant *Data, char *Format)
{
	List<GListItem> s;
	if (GetSelection(s))
	{
		GArray<GListItem*> a;
		for (GListItem *i=s.First(); i; i=s.Next())
		{
			a.Add(i);
		}
		Data->SetBinary(sizeof(GListItem*)*a.Length(), &a[0]);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
class GDataGridFactory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (Class AND
			stricmp(Class, "GDataGrid") == 0)
		{
			return new GDataGrid(0);
		}

		return 0;
	}
};

static GDataGridFactory _Factory;
