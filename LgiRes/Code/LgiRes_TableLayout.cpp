#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_Dialog.h"
#include "GButton.h"
#include "GVariant.h"
#include "GToken.h"
#include "GTableLayout.h"

#define DRAW_CELL_INDEX			0
#define DRAW_TABLE_SIZE			0

/////////////////////////////////////////////////////////////////////
struct Pair { int Pos, Size; };
void CalcCell(GArray<Pair> &p, GArray<double> &s, int Total)
{
	int CurI = 0;
	for (int i=0; i<s.Length(); i++)
	{
		p[i].Size = s[i] * Total;
		p[i].Pos = CurI;
		CurI += p[i].Size;
	}
	p[s.Length()].Pos = Total;
	p[s.Length()].Size = 0;
}

/////////////////////////////////////////////////////////////////////
// Table layout
template<class T>
T SumElements(GArray<T> &a, int start, int end)
{
	T Sum = 0;
	for (int i=start; i<=end; i++)
	{
		Sum += a[i];
	}
	return Sum;
}

void MakeSumUnity(GArray<double> &a)
{
	double s = SumElements<double>(a, 0, a.Length()-1);
	double Diff = s - 1.0;
	if (Diff < 0) Diff *= -1.0;
	if (Diff > 0.001)
	{
		for (int i=0; i<a.Length(); i++)
		{
			a[i] = a[i] / s;
		}
	}
}

enum TableAlign
{
	AlignMin = 0,
	AlignCenter = 1,
	AlignMax = 2
};

char *TableAlignNames[] =
{
	"Min",
	"Center",
	"Max"
};

class TableCell : public GDom
{
public:
	CtrlTable *Table;
	GRect Cell;	// Cell location
	GRect Pos;	// Pixels
	bool Selected;
	GArray<ResDialogCtrl*> Ctrls;
	TableAlign AlignX;
	TableAlign AlignY;

	TableCell(CtrlTable *table, int cx, int cy)
	{
		AlignX = AlignY = AlignMin;
		Table = table;
		Selected = false;
		Cell.ZOff(0, 0);
		Cell.Offset(cx, cy);
		Pos.ZOff(-1, -1);
	}

	~TableCell()
	{
		Ctrls.DeleteObjects();
	}

	void MoveCtrls()
	{
		for (int i=0; i<Ctrls.Length(); i++)
		{
			ResDialogCtrl *c = Ctrls[i];
			if (c)
			{
				GRect r = c->View()->GetPos();

				if (r.X() > Pos.X())
				{
					r.x2 = r.x1 + Pos.X() - 1;
				}
				if (r.Y() > Pos.Y())
				{
					r.y2 = r.y1 + Pos.Y() - 1;
				}
				if (r.x2 > Pos.x2)
				{
					r.Offset(Pos.x2 - r.x2, 0);
				}
				if (r.y2 > Pos.y2)
				{
					r.Offset(0, Pos.y2 - r.y2);
				}

				c->SetPos(r);
			}
		}
	}

	void SetPos(GRect &p)
	{
		int Dx = p.x1 - Pos.x1;
		int Dy = p.y1 - Pos.y1;
		for (int i=0; i<Ctrls.Length(); i++)
		{
			ResDialogCtrl *c = Ctrls[i];
			if (c)
			{
				GRect r = c->View()->GetPos();
				r.Offset(Dx, Dy);
				c->SetPos(r);
			}
		}
		Pos = p;
		MoveCtrls();
	}

	bool GetVariant(const char *Name, GVariant &Value, char *Array)
	{
		if (stricmp(Name, "span") == 0)
		{
			Value = Cell.Describe();
		}
		else if (stricmp(Name, "children") == 0)
		{
			List<GVariant> c;
			for (int i=0; i<Ctrls.Length(); i++)
			{
				c.Insert(new GVariant((ResObject*)Ctrls[i]));
			}
			Value.SetList(&c);
			c.DeleteObjects();
		}
		else if (stricmp(Name, "align") == 0)
		{
			Value = TableAlignNames[AlignX];
		}
		else if (stricmp(Name, "valign") == 0)
		{
			Value = TableAlignNames[AlignY];
		}
		else return false;

		return true;
	}

	bool SetVariant(const char *Name, GVariant &Value, char *Array)
	{
		if (stricmp(Name, "span") == 0)
		{
			GRect r;
			if (r.SetStr(Value.Str()))
			{
				Cell = r;
			}
			else return false;
		}
		else if (stricmp(Name, "children") == 0)
		{
			for (GVariant *v = Value.Value.Lst->First(); v; v = Value.Value.Lst->Next())
			{
				LgiAssert(v->Type == GV_VOID_PTR);
				ResDialogCtrl *Obj = dynamic_cast<ResDialogCtrl*>((ResObject*) v->Value.Ptr);
				if (Obj)
				{
					Table->SetAttachCell(this);
					GRect r = Obj->View()->GetPos();
					Table->AttachCtrl(Obj, &r);
					Table->SetAttachCell(0);
				}
			}
		}
		else if (stricmp(Name, "align") == 0)
		{
			if (Value.Str())
			{
				for (int i=0; i<CountOf(TableAlignNames); i++)
				{
					if (stricmp(TableAlignNames[i], Value.Str()) == 0)
					{
						AlignX = (TableAlign)i;
						break;
					}
				}
			}
		}
		else if (stricmp(Name, "valign") == 0)
		{
			if (Value.Str())
			{
				for (int i=0; i<CountOf(TableAlignNames); i++)
				{
					if (stricmp(TableAlignNames[i], Value.Str()) == 0)
					{
						AlignY = (TableAlign)i;
						break;
					}
				}
			}
		}
		else
		{
			return false;
		}

		return true;
	}
	
	void OnMouseClick(GMouse &m)
	{
		#define IDM_ALIGN_X_MIN				100
		#define IDM_ALIGN_X_CTR				101
		#define IDM_ALIGN_X_MAX				102
		#define IDM_ALIGN_Y_MIN				103
		#define IDM_ALIGN_Y_CTR				104
		#define IDM_ALIGN_Y_MAX				105
		#define IDM_UNMERGE					106
		#define IDM_FIX_TABLE				107
		#define IDM_INSERT_ROW				108

		if (m.Down() AND m.Right())
		{
			GSubMenu *RClick = new GSubMenu;
			if (RClick)
			{
				GSubMenu *s = RClick->AppendSub("Horizontal Align");
				if (s)
				{
					s->AppendItem("Left", IDM_ALIGN_X_MIN, AlignX != AlignMin);
					s->AppendItem("Center", IDM_ALIGN_X_CTR, AlignX != AlignCenter);
					s->AppendItem("Right", IDM_ALIGN_X_MAX, AlignX != AlignMax);
				}
				s = RClick->AppendSub("Vertical Align");
				if (s)
				{
					s->AppendItem("Top", IDM_ALIGN_Y_MIN, AlignY != AlignMin);
					s->AppendItem("Center", IDM_ALIGN_Y_CTR, AlignY != AlignCenter);
					s->AppendItem("Bottom", IDM_ALIGN_Y_MAX, AlignY != AlignMax);
				}
				RClick->AppendItem("Unmerge", IDM_UNMERGE, Cell.X() > 1 || Cell.Y() > 1);
				RClick->AppendItem("Fix Missing Cells", IDM_FIX_TABLE, true);
				RClick->AppendItem("Insert Row", IDM_INSERT_ROW, true);

				m.ToScreen();
				switch (RClick->Float(Table, m.x, m.y))
				{
					case IDM_ALIGN_X_MIN:
						AlignX = AlignMin;
						break;
					case IDM_ALIGN_X_CTR:
						AlignX = AlignCenter;
						break;
					case IDM_ALIGN_X_MAX:
						AlignX = AlignMax;
						break;
					case IDM_ALIGN_Y_MIN:
						AlignY = AlignMin;
						break;
					case IDM_ALIGN_Y_CTR:
						AlignY = AlignCenter;
						break;
					case IDM_ALIGN_Y_MAX:
						AlignY = AlignMax;
						break;
					case IDM_UNMERGE:
						Table->UnMerge(this);
						break;
					case IDM_FIX_TABLE:
						Table->Fix();
						break;
					case IDM_INSERT_ROW:
						Table->InsertRow(Cell.y1);
						break;
				}

				DeleteObj(RClick);
			}
		}
	}
};

class CellJoin : public GRect
{
public:
	TableCell *a, *b;
};

class CtrlTablePrivate
{
public:
	// The cell container
	CtrlTable *Table;
	int CellX, CellY;
	GArray<TableCell*> Cells;
	TableCell *AttachTo;

	// Column + Row sizes
	GArray<double> ColSize;
	GArray<double> RowSize;

	// Goobers for editing the cell layout
	GRect AddX, AddY;
	GArray<GRect> DelCol;
	GArray<GRect> DelRow;
	GArray<GRect> SizeRow;
	GArray<GRect> SizeCol;
	GArray<CellJoin> JoinBtns;

	int DragRowSize;
	int DragColSize;

	// Methods
	TableCell *GetCellAt(int cx, int cy)
	{
		for (int i=0; i<Cells.Length(); i++)
		{
			if (Cells[i]->Cell.Overlap(cx, cy))
			{
				return Cells[i];
			}
		}

		return 0;
	}

	CtrlTablePrivate(CtrlTable *t)
	{
		Table = t;
		CellX = CellY = 2;
		AttachTo = 0;
		Cells[0] = new TableCell(t, 0, 0);
		Cells[1] = new TableCell(t, 1, 0);
		Cells[2] = new TableCell(t, 0, 1);
		Cells[3] = new TableCell(t, 1, 1);
		ColSize[0] = 0.5;
		ColSize[1] = 0.5;
		RowSize[0] = 0.5;
		RowSize[1] = 0.5;
		DragRowSize = DragColSize = -1;
	}

	~CtrlTablePrivate()
	{
		Cells.DeleteObjects();
	}

	void Layout(GRect c)
	{
		#define ADD_BORDER		10
		#define CELL_SPACING	1

		int x, y;
		int BtnSize = ADD_BORDER * 2 / 3;
		AddX.ZOff(BtnSize, BtnSize);
		AddY.ZOff(BtnSize, BtnSize);
		int BtnX = c.X() - AddX.X() - 2;
		int BtnY = c.Y() - AddY.Y() - 2;
		AddX.Offset(BtnX, 0);
		AddY.Offset(0, BtnY);
		c.x2 = AddX.x2 - ADD_BORDER;
		c.y2 = AddY.y2 - ADD_BORDER;
		DelCol.Length(0);
		DelRow.Length(0);
		SizeRow.Length(0);
		SizeCol.Length(0);
		JoinBtns.Length(0);

		if (c.Valid())
		{
			int AvailX = c.X();
			int AvailY = c.Y();

			GArray<Pair> XPair, YPair;
			CalcCell(XPair, ColSize, AvailX);
			CalcCell(YPair, RowSize, AvailY);
			XPair[CellX].Pos = AvailX;

			for (x=0; x<CellX; x++)
			{
				DelCol[x].ZOff(BtnSize, BtnSize);
				DelCol[x].Offset(XPair[x].Pos + (XPair[x].Size / 2), BtnY);

				if (x < CellX - 1)
				{
					SizeCol[x].ZOff(BtnSize+4, BtnSize-2);
					SizeCol[x].Offset(XPair[x+1].Pos - (SizeCol[x].X() / 2) - 1, BtnY);
				}
			}

			for (y=0; y<CellY; y++)
			{
				DelRow[y].ZOff(BtnSize, BtnSize);
				DelRow[y].Offset(BtnX, YPair[y].Pos + (YPair[y].Size / 2));

				if (y < CellY - 1)
				{
					SizeRow[y].ZOff(BtnSize-2, BtnSize+4);
					SizeRow[y].Offset(BtnX, YPair[y+1].Pos - (SizeRow[y].Y() / 2) - 1);
				}
			}

			for (y=0; y<CellY; y++)
			{
				int Py = YPair[y].Pos;

				for (x=0; x<CellX;)
				{
					int Px = XPair[x].Pos;

					TableCell *Cell = GetCellAt(x, y);
					if (Cell)
					{
						if (Cell->Cell.x1 == x AND
							Cell->Cell.y1 == y)
						{
							GRect Pos = Cell->Pos;
							
							// Set cells pixel position, with spanning
							int Ex = XPair[Cell->Cell.x2 + 1].Pos;
							int Ey = YPair[Cell->Cell.y2 + 1].Pos;
							Pos.ZOff(Ex - Px - 1 - CELL_SPACING, Ey - Py - 1 - CELL_SPACING);

							// Offset the cell into place
							Pos.Offset(Px, Py);

							Cell->SetPos(Pos);

							if (Cell->Selected)
							{
								// Add cell joining goobers...
								TableCell *c = GetCellAt(x + Cell->Cell.X(), y);
								if (c AND c->Selected)
								{
									CellJoin *j = &JoinBtns[JoinBtns.Length()];
									j->a = Cell;
									j->b = c;
									j->ZOff(BtnSize, BtnSize);
									j->Offset(Cell->Pos.x2 - (j->X()>>1), Cell->Pos.y1 + ((Cell->Pos.Y()-j->Y()) >> 1));
								}

								c = GetCellAt(x, y + Cell->Cell.Y());
								if (c AND c->Selected)
								{
									CellJoin *j = &JoinBtns[JoinBtns.Length()];
									j->a = Cell;
									j->b = c;
									j->ZOff(BtnSize, BtnSize);
									j->Offset(Cell->Pos.x1 + ((Cell->Pos.X()-j->X()) >> 1), Cell->Pos.y2 - (j->Y()>>1));
								}
							}
						}

						LgiAssert(Cell->Cell.X());
						x += Cell->Cell.X();

						Px += Cell->Pos.X() + CELL_SPACING;
					}
					else break;
				}
			}
		}
	}

	void DrawBtn(GSurface *pDC, GRect &r, char Btn)
	{
		#define OFF			1

		pDC->Rectangle(&r);

		int cx = r.X() >> 1;
		int cy = r.Y() >> 1;

		COLOUR Old = pDC->Colour();
		pDC->Colour(LC_WHITE, 24);
		switch (Btn)
		{
			case '|':
			{
				pDC->Line(r.x1 + cx, r.y1 + OFF, r.x1 + cx, r.y2 - OFF);
				pDC->Line(r.x1 + cx - 1, r.y1 + OFF + 1, r.x1 + cx + 1, r.y1 + OFF + 1);
				pDC->Line(r.x1 + cx - 1, r.y2 - OFF - 1, r.x1 + cx + 1, r.y2 - OFF - 1);
				break;
			}
			case '_':
			{
				pDC->Line(r.x1 + OFF, r.y1 + cy, r.x2 - OFF, r.y1 + cy);
				pDC->Line(r.x1 + OFF + 1, r.y1 + cy - 1, r.x1 + OFF + 1, r.y1 + cy + 1);
				pDC->Line(r.x2 - OFF - 1, r.y1 + cy - 1, r.x2 - OFF - 1, r.y1 + cy + 1);
				break;
			}
			case '+':
			{
				pDC->Line(r.x1 + cx, r.y1 + OFF, r.x1 + cx, r.y2 - OFF);
				pDC->Line(r.x1 + OFF, r.y1 + cy, r.x2 - OFF, r.y1 + cy);
				break;
			}
			case '-':
			{
				pDC->Line(r.x1 + OFF, r.y1 + cy, r.x2 - OFF, r.y1 + cy);
				break;
			}
		}

		pDC->Colour(Old);
	}

	bool DeleteCol(int x)
	{
		bool Status = false;

		// Delete column 'x'
		for (int y=0; y<CellY; )
		{
			TableCell *c = GetCellAt(x, y);
			if (c)
			{
				y = c->Cell.y2 + 1;
				if (c->Cell.X() == 1)
				{
					Cells.Delete(c);
					DeleteObj(c);
				}
				else
				{
					c->Cell.x2--;
				}
				Status = true;
			}
			else break;
		}
		CellX--;

		double Last = ColSize[x];
		ColSize.DeleteAt(x, true);
		for (int i=0; i<ColSize.Length(); i++)
		{
			ColSize[i] = ColSize[i] / (1.0 - Last);
		}

		// Move down all the columns after 'x'
		for (x++; x<DelCol.Length(); x++)
		{
			for (int y=0; y<CellY; y++)
			{
				TableCell *c = GetCellAt(x, y);
				if (c)
				{
					if (c->Cell.x1 == x AND
						c->Cell.y1 == y)
					{
						c->Cell.Offset(-1, 0);
					}
				}
			}
		}

		return Status;
	}

	bool DeleteRow(int y)
	{
		bool Status = false;
		int x;

		// Delete row 'y'
		for (x=0; x<CellX; )
		{
			TableCell *c = GetCellAt(x, y);
			if (c)
			{
				x = c->Cell.x2 + 1;

				if (c->Cell.Y() == 1)
				{
					Cells.Delete(c);
					DeleteObj(c);
				}
				else
				{
					c->Cell.y2--;
				}
				Status = true;
			}
			else break;
		}
		CellY--;

		double Last = RowSize[y];
		RowSize.DeleteAt(y, true);
		for (int i=0; i<RowSize.Length(); i++)
		{
			RowSize[i] = RowSize[i] / (1.0 - Last);
		}

		// Move down all the rows after 'y'
		for (y++; y<DelRow.Length(); y++)
		{
			for (int x=0; x<CellX; x++)
			{
				TableCell *c = GetCellAt(x, y);
				if (c)
				{
					if (c->Cell.x1 == x AND
						c->Cell.y1 == y)
					{
						c->Cell.Offset(0, -1);
					}
				}
			}
		}

		return Status;
	}

	TableCell *GetCell(ResDialogCtrl *Ctrl)
	{
		for (int i=0; i<Cells.Length(); i++)
		{
			if (Cells[i]->Ctrls.HasItem(Ctrl))
			{
				return Cells[i];
			}
		}

		return 0;
	}
};

CtrlTable::CtrlTable(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Table, load)
{
	d = new CtrlTablePrivate(this);
	AcceptChildren = true;
}

CtrlTable::~CtrlTable()
{
	DeleteObj(d);
}

void CtrlTable::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	LgiTrace("Tbl Ref=%i\n", Str->GetRef());
	for (int i=0; i<d->Cells.Length(); i++)
	{
		TableCell *c = d->Cells[i];
		for (int n=0; n<c->Ctrls.Length(); n++)
		{
			LgiTrace("	Ref=%i\n", c->Ctrls[n]->Str->GetRef());
			c->Ctrls[n]->EnumCtrls(Ctrls);
		}
	}
}

bool CtrlTable::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (stricmp(Name, "cols") == 0)
	{
		GStringPipe p;
		for (int i=0; i<d->ColSize.Length(); i++)
		{
			if (i) p.Push(",");
			p.Print("%.3f", d->ColSize[i]);
		}
		char *s = p.NewStr();
		if (s)
		{
			Value = s;
			DeleteArray(s);
		}
	}
	else if (stricmp(Name, "rows") == 0)
	{
		GStringPipe p;
		for (int i=0; i<d->RowSize.Length(); i++)
		{
			if (i) p.Push(",");
			p.Print("%.3f", d->RowSize[i]);
		}
		char *s = p.NewStr();
		if (s)
		{
			Value = s;
			DeleteArray(s);
		}
	}
	else if (stricmp(Name, "Cell") == 0)
	{
		if (Array)
		{
			GToken t(Array, ",");
			if (t.Length() == 2)
			{
				Value = d->GetCellAt(atoi(t[0]), atoi(t[1]));
			}
			else return false;
		}
		else return false;
	}
	else return false;

	return true;
}

bool CtrlTable::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (stricmp(Name, "cols") == 0)
	{
		d->Cells.DeleteObjects();

		GToken t(Value.Str(), ",");
		d->ColSize.Length(0);
		for (int i=0; i<t.Length(); i++)
		{
			d->ColSize.Add(atof(t[i]));
		}

		MakeSumUnity(d->ColSize);

		d->CellX = d->ColSize.Length();
	}
	else if (stricmp(Name, "rows") == 0)
	{
		d->Cells.DeleteObjects();

		GToken t(Value.Str(), ",");
		d->RowSize.Length(0);
		for (int i=0; i<t.Length(); i++)
		{
			d->RowSize.Add(atof(t[i]));
		}

		MakeSumUnity(d->RowSize);

		d->CellY = d->RowSize.Length();
	}
	else if (stricmp(Name, "Cell") == 0)
	{
		if (Array)
		{
			GToken t(Array, ",");
			if (t.Length() == 2)
			{
				int Cx = atoi(t[0]);
				int Cy = atoi(t[1]);
				TableCell *c = new TableCell(this, Cx, Cy);
				if (c)
				{
					d->Cells.Add(c);
					GDom **Ptr = (GDom**)Value.Value.Ptr;
					if (Ptr)
					{
						*Ptr = c;
					}
				}
			}
			else return false;
		}
		else return false;
	}
	else return false;

	return true;
}

GRect *CtrlTable::GetPasteArea()
{
	for (int i=0; i<d->Cells.Length(); i++)
	{
		if (d->Cells[i]->Selected)
		{
			return &d->Cells[i]->Pos;
		}
	}

	return 0;
}

GRect *CtrlTable::GetChildArea(ResDialogCtrl *Ctrl)
{
	TableCell *c = d->GetCell(Ctrl);
	if (c)
	{
		return &c->Pos;
	}
	return 0;
}

void CtrlTable::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (!Attaching)
	{
		ResDialogCtrl *Rc = dynamic_cast<ResDialogCtrl*>(Wnd);
		if (Rc)
		{
			TableCell *c = d->GetCell(Rc);
			if (c)
			{
				c->Ctrls.Delete(Rc);
			}
		}
	}
}

void CtrlTable::SetAttachCell(TableCell *c)
{
	d->AttachTo = c;
}

bool CtrlTable::AttachCtrl(ResDialogCtrl *Ctrl, GRect *r)
{
	if (d->AttachTo)
	{
		Layout();

		if (!d->AttachTo->Ctrls.HasItem(Ctrl))
		{
			d->AttachTo->Ctrls.Add(Ctrl);
		}

	
		GRect b = d->AttachTo->Pos;
		if (r->X() > b.X())
		{
			r->x1 = b.x1;
			r->x2 = b.x2;
		}
		else if (r->x2 > b.x2)
		{
			r->Offset(b.x2 - r->x2, 0);
		}
		if (r->Y() > b.Y())
		{
			r->y1 = b.y1;
			r->y2 = b.y2;
		}
		else if (r->y2 > b.y2)
		{
			r->Offset(b.y2 - r->y2, 0);
		}

		Ctrl->SetPos(*r);
	}
	else
	{
		for (int i=0; i<d->Cells.Length(); i++)
		{
			TableCell *c = d->Cells[i];
			if (c)
			{
				if (c->Pos.Overlap(r->x1, r->y1))
				{
					if (!c->Ctrls.HasItem(Ctrl))
					{
						c->Ctrls.Add(Ctrl);
					}
					Ctrl->SetPos(*r);
					c->MoveCtrls();
					*r = Ctrl->View()->GetPos();
					break;
				}
			}
		}
	}

	return ResDialogCtrl::AttachCtrl(Ctrl, r);
}

void CtrlTable::Layout()
{
	d->Layout(GetClient());
}

void CtrlTable::OnPaint(GSurface *pDC)
{
	int i;
	COLOUR Blue = Rgb24(0, 30, 222);
	Client.Set(0, 0, X()-1, Y()-1);

	d->Layout(GetClient());
	pDC->Colour(Blue, 24);
	for (i=0; i<d->Cells.Length(); i++)
	{
		TableCell *c = d->Cells[i];
		if (c)
		{
			pDC->Box(&c->Pos);
			if (c->Selected)
			{
				int Op = pDC->Op(GDC_ALPHA);
				pDC->Applicator()->SetVar(GAPP_ALPHA_A, 0x20);
				pDC->Rectangle(c->Pos.x1 + 1, c->Pos.y1 + 1, c->Pos.x2 - 1, c->Pos.y2 - 1);
				pDC->Op(Op);
			}

			#if DRAW_CELL_INDEX
			SysFont->Colour(Blue, 24);
			SysFont->Transparent(true);
			char s[256];
			sprintf(s, "%i,%i-%i,%i", c->Cell.x1, c->Cell.y1, c->Cell.X(), c->Cell.Y());
			SysFont->Text(pDC, c->Pos.x1 + 3, c->Pos.y1 + 1, s);
			#endif
		}
	}
	d->DrawBtn(pDC, d->AddX, '+');
	d->DrawBtn(pDC, d->AddY, '+');
	for (i=0; i<d->DelCol.Length(); i++)
	{
		d->DrawBtn(pDC, d->DelCol[i], '-');
	}
	for (i=0; i<d->DelRow.Length(); i++)
	{
		d->DrawBtn(pDC, d->DelRow[i], '-');
	}
	for (i=0; i<d->SizeCol.Length(); i++)
	{
		d->DrawBtn(pDC, d->SizeCol[i], '_');
	}
	for (i=0; i<d->SizeRow.Length(); i++)
	{
		d->DrawBtn(pDC, d->SizeRow[i], '|');
	}
	for (i=0; i<d->JoinBtns.Length(); i++)
	{
		d->DrawBtn(pDC, d->JoinBtns[i], '+');
	}

	#if DRAW_TABLE_SIZE
	SysFont->Colour(Blue, 24);
	SysFont->Transparent(true);
	char s[256];
	sprintf(s, "Cells: %i,%i", d->CellX, d->CellY);
	SysFont->Text(pDC, 3, 24, s);
	#endif

	ResDialogCtrl::OnPaint(pDC);
}

void CtrlTable::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		GArray<Pair> p;

		if (d->DragRowSize >= 0)
		{
			// Adjust RowSize[d->DragRowSize] and RowSize[d->DragRowSize+1] to
			// center on the mouse y location
			int AvailY = GetClient().Y();
			double Total = d->RowSize[d->DragRowSize] + d->RowSize[d->DragRowSize+1];
			CalcCell(p, d->RowSize, AvailY);
			int y = m.y - p[d->DragRowSize].Pos;
			double fy = (double) y / AvailY;
			d->RowSize[d->DragRowSize] = fy;
			d->RowSize[d->DragRowSize+1] = Total - fy;
			Layout();
			Invalidate();
			return;
		}
		else if (d->DragColSize >= 0)
		{
			// Adjust ColSize[d->DragColSize] and ColSize[d->DragColSize+1] to
			// center on the mouse x location
			int AvailX = GetClient().X();
			double Total = d->ColSize[d->DragColSize] + d->ColSize[d->DragColSize+1];
			CalcCell(p, d->ColSize, AvailX);
			int x = m.x - p[d->DragColSize].Pos;
			double fx = (double) x / AvailX;
			d->ColSize[d->DragColSize] = fx;
			d->ColSize[d->DragColSize+1] = Total - fx;
			Layout();
			Invalidate();
			return;
		}
	}

	ResDialogCtrl::OnMouseMove(m);
}

void CtrlTable::Fix()
{
	// Fix missing cells
	for (int y=0; y<d->CellY; y++)
	{
		for (int x=0; x<d->CellX; )
		{
			TableCell *c = d->GetCellAt(x, y);
			if (c)
			{
				x += c->Cell.X();
			}
			else
			{
				c = new TableCell(this, x, y);
				if (c)
				{
					d->Cells.Add(c);
					x++;
				}
				else break;
			}
		}
	}

	// Fix rows/cols size
	#define absf(i) ((i) < 0.0 ? -(i) : (i))
	double Sum = 0.0;
	int n;
	for (n=0; n<d->RowSize.Length(); n++)
	{
		Sum += d->RowSize[n];
	}
	if (absf(Sum - 1.0) > 0.001)
	{
		for (n=0; n<d->RowSize.Length(); n++)
		{
			double k = d->RowSize[n];
			k /= Sum;
			d->RowSize[n] = k;
		}
	}
	Sum = 0.0;
	for (n=0; n<d->RowSize.Length(); n++)
	{
		Sum += d->RowSize[n];
	}
	if (absf(Sum - 1.0) > 0.001)
	{
		for (n=0; n<d->RowSize.Length(); n++)
		{
			double k = d->RowSize[n];
			k /= Sum;
			d->RowSize[n] = k;
		}
	}

	Invalidate();
}

void CtrlTable::InsertRow(int y)
{
	// Shift existing cells down
	int i;
	for (i=0; i<d->Cells.Length(); i++)
	{
		TableCell *c = d->Cells[i];
		if (c)
		{
			if (c->Cell.y1 >= y)
			{
				c->Cell.Offset(0, 1);
			}
		}
	}

	// Add new row of 1x1 cells
	for (i=0; i<d->CellX; i++)
	{
		d->Cells.Add(new TableCell(this, i, y));
	}
	d->CellY++;

	// Update rows
	double Last = d->RowSize[d->RowSize.Length()-1];
	d->RowSize.Add(Last);
	for (i=0; i<d->RowSize.Length(); i++)
	{
		d->RowSize[i] = d->RowSize[i] / (1.0 + Last);
	}

	// Refresh the screen
	Invalidate();
}

void CtrlTable::UnMerge(TableCell *Cell)
{
	if (Cell AND (Cell->Cell.X() > 1 || Cell->Cell.Y() > 1))
	{
		for (int y=Cell->Cell.x1; y<=Cell->Cell.y2; y++)
		{
			for (int x=Cell->Cell.x1; x<=Cell->Cell.x2; x++)
			{
				if (x > Cell->Cell.x1 ||
					y > Cell->Cell.y1)
				{
					TableCell *n = new TableCell(this, x, y);
					if (n)
					{
						d->Cells.Add(n);
					}
				}
			}
		}

		Cell->Cell.x2 = Cell->Cell.x1;
		Cell->Cell.y2 = Cell->Cell.y1;

		Invalidate();
	}
}

void CtrlTable::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (m.Left())
		{
			if (d->AddX.Overlap(m.x, m.y))
			{
				int i;
				for (i=0; i<d->CellY; i++)
				{
					d->Cells.Add(new TableCell(this, d->CellX, i));
				}
				d->CellX++;

				double Last = d->ColSize[d->ColSize.Length()-1];
				d->ColSize.Add(Last);
				for (i=0; i<d->ColSize.Length(); i++)
				{
					d->ColSize[i] = d->ColSize[i] / (1.0 + Last);
				}

				Invalidate();
				return;
			}
			else if (d->AddY.Overlap(m.x, m.y))
			{
				int i;
				for (i=0; i<d->CellX; i++)
				{
					d->Cells.Add(new TableCell(this, i, d->CellY));
				}
				d->CellY++;

				double Total = 0;
				double Last = d->RowSize[d->RowSize.Length()-1];
				d->RowSize.Add(Last);
				for (i=0; i<d->RowSize.Length(); i++)
				{
					d->RowSize[i] = d->RowSize[i] / (1.0 + Last);
					Total += d->RowSize[i];
				}

				Invalidate();
				return;
			}
			else
			{
				bool Dirty = false;
				bool EatClick = true;
				int i;
				TableCell *Over = 0;

				// Look at cell joins
				for (i=0; i<d->JoinBtns.Length(); i++)
				{
					CellJoin *j = &d->JoinBtns[i];
					if (j->Overlap(m.x, m.y))
					{
						// Do a cell merge
						GRect u = j->a->Cell;
						u.Union(&j->b->Cell);

						d->Cells.Delete(j->a);
						for (int y=u.y1; y<=u.y2; y++)
						{
							for (int x=u.x1; x<=u.x2; x++)
							{
								TableCell *c = d->GetCellAt(x, y);
								if (c)
								{
									d->Cells.Delete(c);
									DeleteObj(c);
								}
							}
						}
						j->a->Cell = u;
						d->Cells.Add(j->a);
						Dirty = true;
					}
				}

				if (!Dirty)
				{
					// Select a cell?
					for (i=0; i<d->Cells.Length(); i++)
					{
						TableCell *c = d->Cells[i];
						if (c->Pos.Overlap(m.x, m.y))
						{
							Over = c;
							if (!c->Selected || m.Ctrl())
							{
								Dirty = true;
								EatClick = false;
								c->Selected = !c->Selected;
							}
						}
						else if (!m.Ctrl())
						{
							if (c->Selected)
							{
								Dirty = true;
								EatClick = false;
								c->Selected = false;
							}
						}
					}
				}

				// Delete column goobers
				for (int x=0; x<d->DelCol.Length(); x++)
				{
					if (d->DelCol[x].Overlap(m.x, m.y))
					{
						if (Dirty = d->DeleteCol(x))
						{
							break;
						}
					}
				}
				// Delete row goobers
				for (int y=0; y<d->DelRow.Length(); y++)
				{
					if (d->DelRow[y].Overlap(m.x, m.y))
					{
						if (Dirty = d->DeleteRow(y))
						{
							break;
						}
					}
				}

				// Size row goobs
				if (!Dirty)
				{
					for (int i=0; i<d->SizeRow.Length(); i++)
					{
						if (Dirty = d->SizeRow[i].Overlap(m.x, m.y))
						{
							d->DragRowSize = i;
							Capture(true);
							break;
						}
					}
				}

				// Size col goobs
				if (!Dirty)
				{
					for (int i=0; i<d->SizeCol.Length(); i++)
					{
						if (Dirty = d->SizeCol[i].Overlap(m.x, m.y))
						{
							d->DragColSize = i;
							Capture(true);
							break;
						}
					}
				}

				if (Dirty)
				{
					Invalidate();
					if (EatClick)
						return;
				}
			}
		}
		else if (m.Right())
		{
			for (int i=0; i<d->Cells.Length(); i++)
			{
				TableCell *c = d->Cells[i];
				if (c->Pos.Overlap(m.x, m.y))
				{
					c->OnMouseClick(m);
					return;
				}
			}
		}
	}
	else
	{
		Capture(false);
		d->DragRowSize = -1;
		d->DragColSize = -1;
	}

	ResDialogCtrl::OnMouseClick(m);
}

////////////////////////////////////////////////////////////////////////////////////////////////
enum {
    M_FINISHED = M_USER + 1000,
};

class TableLayoutTest : public GDialog
{
	GTableLayout *Tbl;
	GView *Msg;
	GTree *Tree;
	class DlgContainer *View;
	GAutoPtr<GThread> Worker;
	GAutoString Base;
	
public:
	TableLayoutTest(GViewI *par);
	~TableLayoutTest();
	
	void OnDialog(LgiDialogRes *Dlg);
	int OnNotify(GViewI *Ctrl, int Flags);
    GMessage::Param OnEvent(GMessage *m);
};

class DlgItem : public GTreeItem
{
    TableLayoutTest *Wnd;
	LgiDialogRes *Dlg;
	
public:
	DlgItem(TableLayoutTest *w, LgiDialogRes *dlg)
	{
	    Wnd = w;
		Dlg = dlg;
	}
	
	char *GetText(int i)
	{
		return Dlg->Str->Str;
	}
	
	void OnSelect()
	{
	    Wnd->OnDialog(Dlg);
	}
};

static bool HasTableLayout(GXmlTag *t)
{
	if (t->IsTag("TableLayout"))
		return true;
	for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
	{
		if (HasTableLayout(c))
			return true;
	}
	return false;
}

class Lr8Item : public GTreeItem
{
    TableLayoutTest *Wnd;
	GAutoString File;
	GAutoPtr<LgiResources> Res;

public:
	Lr8Item(TableLayoutTest *w, GAutoPtr<LgiResources> res, char *file)
	{
	    Wnd = w;
		Res = res;
		File.Reset(NewStr(file));

		List<LgiDialogRes>::I d = Res->GetDialogs();
		for (LgiDialogRes *dlg = *d; dlg; dlg = *++d)
		{
			if (dlg->Str && HasTableLayout(dlg->Dialog))
			{
				Insert(new DlgItem(Wnd, dlg));
			}
		}
		
		if (stristr(File, "Scribe-Branches\\v2.00"))
		{
		    Expanded(true);
		}
	}
	
	char *GetText(int i)
	{
		return File ? File : "#error";
	}
};

class Lr8Search : public GThread
{
    TableLayoutTest *Wnd;
	char *Base;
	GTree *Tree;
	
public:
	Lr8Search(TableLayoutTest *w, char *base, GTree *tree) : GThread("Lr8Search")
	{
	    Wnd = w;
		Base = base;
		Tree = tree;
		Run();
	}
	
	~Lr8Search()
	{
		while (!IsExited())
		{
			LgiSleep(1);
		}
	}
	
	int Main()
	{
		GArray<const char*> Ext;
		GArray<char*> Files;
		Ext.Add("*.lr8");
		if (LgiRecursiveFileSearch(Base, &Ext, &Files))
		{
			for (int i=0; i<Files.Length(); i++)
			{
            	GAutoPtr<LgiResources> Res;
    			if (Res.Reset(new LgiResources(Files[i])))
    			{
				    List<LgiDialogRes>::I d = Res->GetDialogs();
				    bool HasTl = false;
				    for (LgiDialogRes *dlg = *d; dlg; dlg = *++d)
				    {
					    if (dlg->Str && HasTableLayout(dlg->Dialog))
					    {
					        HasTl = true;
					        break;
					    }
				    }
				    if (HasTl)
				    {
			            GAutoString r = LgiMakeRelativePath(Base, Files[i]);
			            Tree->Insert(new Lr8Item(Wnd, Res, r));
				    }
				}
			}
		}
		
		Files.DeleteArrays();
		Wnd->PostEvent(M_FINISHED);
		return 0;
	}
};

class DlgContainer : public GLayout
{
    LgiDialogRes *Dlg;
    GRect Size;

public:
    DlgContainer(int id)
    {
        Dlg = 0;
        Sunken(true);
        SetId(id);
        
        Size.Set(0, 0, 100, 100);
        SetPos(Size);
    }
    
    void OnPaint(GSurface *pDC)
    {
        pDC->Colour(LC_WORKSPACE, 24);
        pDC->Rectangle();
    }
    
    void OnDialog(LgiDialogRes *d)
    {
        while (Children.First())
            delete Children.First();
    
        if (Dlg = d)
        {
            if (Dlg->GetRes()->LoadDialog(Dlg->Str->Id, this, &Size))
            {
                GRect r = GetPos();
                r.Dimension(Size.X(), Size.Y());
                SetPos(r);
                AttachChildren();
                
                SendNotify(GTABLELAYOUT_REFRESH);
            }
        }        
    }
};

TableLayoutTest::TableLayoutTest(GViewI *par)
{
	GRect r(0, 0, 1000, 800);
	SetPos(r);
	SetParent(par);
	
	AddView(Tbl = new GTableLayout);
	GLayoutCell *c;

	c = Tbl->GetCell(0, 0, true, 2);
	c->Add(Msg = new GText(100, 0, 0, -1, -1, "Searching for files..."));

	c = Tbl->GetCell(0, 1);
	c->Add(Tree = new GTree(101, 0, 0, 100, 100));
	c = Tbl->GetCell(1, 1);
	c->Add(View = new DlgContainer(102));

	c = Tbl->GetCell(0, 2, true, 2);
	c->SetAlignX(GLayoutCell::AlignMax);
	c->Add(new GButton(IDOK, 0, 0, -1, -1, "Close"));

	char e[MAX_PATH];
	LgiGetSystemPath(LSP_APP_INSTALL, e, sizeof(e));
	LgiMakePath(e, sizeof(e), e, "../../..");
	Base.Reset(NewStr(e));
	
	Worker.Reset(new Lr8Search(this, Base, Tree));
}

TableLayoutTest::~TableLayoutTest()
{
	Worker.Reset();
}

void TableLayoutTest::OnDialog(LgiDialogRes *Dlg)
{
    if (View)
        View->OnDialog(Dlg);
}

int TableLayoutTest::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
			EndModal(1);
			break;
	}
	
	return GDialog::OnNotify(Ctrl, Flags);
}

GMessage::Param TableLayoutTest::OnEvent(GMessage *m)
{
    if (MsgCode(m) == M_FINISHED)
    {
        Tree->Invalidate();
        Msg->Name("Finished.");
        Worker.Reset();
    }
    return GDialog::OnEvent(m);
}

void OpenTableLayoutTest(GViewI *p)
{
	TableLayoutTest Dlg(p);
	Dlg.DoModal();
}
