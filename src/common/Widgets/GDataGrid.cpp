#include "Lgi.h"
#include "GDataGrid.h"
#include "GEdit.h"

#define IDC_EDIT		2020
#define CELL_EDGE		2

struct GDataGridPriv
{
	GDataGrid *This;
	int Col;
	class GDataGridEdit *e;
	GListItem *Cur;
	bool Dirty, PosDirty;
	GArray<GDataGrid::GDataGridFlags> Flags;

	GDataGridPriv(GDataGrid *t);
	void Save();
	void UpdatePos();
	void Create(int NewCol = -1);
	void MoveCell(int dx);
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
		if (!GEdit::OnKey(k) &&
			!k.IsChar &&
			k.Down())
		{
			switch (k.vkey)
			{
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
					}
					break;
				}
				case VK_PAGEUP:
				case VK_PAGEDOWN:
				case VK_UP:
				case VK_DOWN:
				{
					bool b = d->This->OnKey(k);

					if (d->e)
					{
						GRect rc = d->e->GetPos();
						rc.Size(-CELL_EDGE, -CELL_EDGE);
						d->This->Invalidate(&rc);
					}
					break;
				}
			}
		}

		return false;
	}
};

GDataGridPriv::GDataGridPriv(GDataGrid *t)
{
	Dirty = false;
	PosDirty = false;
	Cur = 0;
	This = t;
	Col = 0;
	e = 0;
}

void GDataGridPriv::Save()
{
	if (Dirty && Cur)
	{
		char *OldTxt = e->Name();
		Cur->SetText(OldTxt, Col);			
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
			rc.x2--;
			rc.y2--;
			e->SetPos(rc);
			e->Focus(true);
			PosDirty = false;
		}
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
	if (!Cur || i != Cur || (NewCol >= 0 && NewCol != Col))
	{
		Save();
		Cur = i;
		if (NewCol >= 0)
		{
			if (Flags[NewCol] & GDataGrid::GDG_READONLY)
			{
				// Pick a valid column
				for (int i=0; i<This->GetColumns(); i++)
				{
					if (NewCol != i && !(Flags[i] & GDataGrid::GDG_READONLY))
					{
						NewCol = i;
						break;
					}
				}
			}

			Col = NewCol;
		}

		char *CurText = i->GetText(Col);
		GRect *r = i->GetPos(Col);
		if (r)
		{
			GRect rc = r;
			rc.x2 -= 1;
			rc.y2 -= 1;
			if (!e)
			{
				// Create edit control with the correct info for the cell
				if (e = new GDataGridEdit(this, IDC_EDIT, rc.x1, rc.y1, rc.X(), rc.Y(), CurText))
				{
					#if 0 // def _DEBUG
					e->SetBackgroundFill(new GViewFill(Rgb24(255, 222, 222), 24));
					#endif
					e->Sunken(false);
					e->Attach(This);
					e->SetPos(rc);
				}
			}
			else
			{
				GRect Old = e->GetPos();
				Old.Size(-CELL_EDGE, -CELL_EDGE);
				This->Invalidate(&Old);

				e->SetPos(rc);
				e->Name(CurText);
			}

			e->Select(0);
			e->Focus(true);
		}			
	}
}

GDataGrid::GDataGrid(int CtrlId) : GList(CtrlId, 0, 0, 1000, 1000)
{
	DrawGridLines(true);
	MultiSelect(false);
	SetPourLargest(true);

	d = new GDataGridPriv(this);
}

GDataGrid::~GDataGrid()
{
	DeleteObj(d);
}

void GDataGrid::OnItemSelect(GArray<GListItem*> &Items)
{
	d->Create();
}

void GDataGrid::OnItemClick(GListItem *Item, GMouse &m)
{
	int ClickCol = ColumnAtX(m.x);
	if (ClickCol != d->Col)
	{
		d->Create(ClickCol);
	}
}

void GDataGrid::OnCreate()
{
	d->Create(0);
}

int GDataGrid::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
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

void GDataGrid::SetColFlag(int Col, GDataGridFlags Flags)
{
	d->Flags[Col] = Flags;
}

void GDataGrid::OnMouseWheel(double Lines)
{
	GList::OnMouseWheel(Lines);
	d->UpdatePos();
}

void GDataGrid::OnPaint(GSurface *pDC)
{
	GList::OnPaint(pDC);

	d->UpdatePos();
	if (d->e)
	{
		pDC->Colour(LC_BLACK, 24);
		GRect p = d->e->GetPos();
		p.Size(-CELL_EDGE, -CELL_EDGE);
		pDC->Box(&p);
	}
}
