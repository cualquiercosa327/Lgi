#include "Lgi.h"
#include "GTableLayout.h"
#include "GToken.h"
#include "GVariant.h"
#include "GTextLabel.h"
#include "GButton.h"
#include "GEdit.h"
#include "GCombo.h"
#include "GList.h"
#include "GTree.h"
#include "GCheckBox.h"
#include "GRadioGroup.h"
#include "GBitmap.h"
#include "GTabView.h"
#include "GUtf8.h"

#define Izza(c)				dynamic_cast<c*>(v)
#define CELL_SPACING		4
#define DEBUG_LAYOUT		0
#define DEBUG_PROFILE		0
#define DEBUG_DRAW_CELLS	0

#ifdef MAC
#define BUTTON_OVERHEAD_X	28
#else
#define BUTTON_OVERHEAD_X	16
#endif

enum CellFlag
{
	SizeUnknown,
	SizeFixed,
	SizeGrow,
};

template <class T>
T CountRange(GArray<T> &a, int Start, int End)
{
	T c = 0;
	for (int i=Start; i<=End; i++)
	{
		c += a[i];
	}
	return c;
}

struct UnderInfo
{
	int Col;		// index of column
	int Grow;		// in px
};

int Cmp(UnderInfo *a, UnderInfo *b)
{
	return b->Grow - a->Grow;
}

void DistributeUnusedSpace(	GArray<int> &Min,
							GArray<int> &Max,
							GArray<CellFlag> &Flags,
							int Total,
							int CellSpacing)
{
	// Now allocate unused space
	int Sum = CountRange<int>(Min, 0, Min.Length()-1) + ((Min.Length() - 1) * CellSpacing);
	if (Sum < Total)
	{
		int i, Avail = Total - Sum;

		// Do growable ones first
		GArray<UnderInfo> Small, Large;
		for (i=0; i<Min.Length(); i++)
		{
			CellFlag f = Flags[i];
			if (Max[i] > Min[i])
			{
				UnderInfo u;
				u.Col = i;
				u.Grow = Max[i] - Min[i];
				if (u.Grow < Avail >> 1)
				{
					Small.Add(u);
				}
				else
				{
					Large.Add(u);
				}
			}
		}
		Small.Sort(Cmp);
		Large.Sort(Cmp);

		for (i=0; Avail>0 AND i<Small.Length(); i++)
		{
			int Col = Small[i].Col;
			Min[Col] += Small[i].Grow;
			Avail -= Small[i].Grow;
		}

		if (Large.Length())
		{
			int Part = Avail / Large.Length();
			for (i=0; Avail>0 AND i<Large.Length(); i++)
			{
				int Col = Large[i].Col;
				Min[Col] += Part;
				Avail -= Part;
			}
		}
	}
}

static uint32 NextChar(char *s)
{
	int Len = 0;
	while (s[Len] AND Len < 6) Len++;
	return LgiUtf8To32((uint8*&)s, Len);
}

void PreLayoutTextCtrl(GView *v, int &Min, int &Max)
{
	int MyMin = 0;
	int MyMax = 0;

	char *t = v->Name();
	GFont *f = v->GetFont();
	if (t AND f)
	{
		GDisplayString Sp(f, " ");
		int X = 0;

		char White[] = " \t\r\n";
		for (char *s = t; *s; )
		{
			while (*s AND strchr(White, *s))
			{
				if (*s == '\n')
				{
					X = 0;
				}
				s++;
			}
			char *e = s;
			while (*e)
			{
				uint32 c = NextChar(e);
				if (c == 0)
					break;
				if (e > s AND LGI_BreakableChar(c))
					break;
				e = LgiSeekUtf8(e, 1);
			}

			GDisplayString d(f, s, e - s);
			MyMin = max(d.X(), MyMin);
			X += d.X() + (*e == ' ' ? Sp.X() : 0);
			MyMax = max(X, MyMax);

			s = *e AND strchr(White, *e) ? e + 1 : e;
		}
	}

	Min = max(Min, MyMin);
	Max = max(Max, MyMax);
}

int LayoutTextCtrl(GView *v, int Offset, int Width)
{
	int Ht = 0;

	char *t = v->Name();
	GFont *f = v->GetFont();
	if (t AND f)
	{
		int y = f->GetHeight();

		char *e = t + strlen(t);
		for (char *s = t; s AND *s; )
		{
			GDisplayString d(f, s, min(1000, e - s));
			if (!(OsChar*)d)
			{
				LgiAssert(0);
				break;
			}

			int Ch = d.CharAt(Width);
			if (Ch < 0)
			{
				LgiAssert(0);
				break;
			}

			e = LgiSeekUtf8(s, Ch);
			while (	*e AND
					*e != '\n' AND
					e > s)
			{
				uint32 c = NextChar(e);

				if (LGI_BreakableChar(c))
					break;

				e = LgiSeekUtf8(e, -1, t);
			}

			s = *e ? LgiSeekUtf8(e, 1) : e;
			Ht += y;
		}
	}

	return Ht;
}

enum CellAlign
{
	AlignMin,
	AlignCenter,
	AlignMax,
};

CellAlign ConvertAlign(char *s)
{
	if (s)
	{
		if (stricmp(s, "Center") == 0)
			return AlignCenter;
		if (stricmp(s, "Max") == 0)
			return AlignMax;
	}

	return AlignMin;
}

class TableCell;

class GTableLayoutPrivate
{
public:
	GArray<double> Rows, Cols;
	GArray<TableCell*> Cells;
	int CellSpacing;
	bool FirstLayout;
	GRect LayoutBounds;
	int LayoutMinX, LayoutMaxX;
	GTableLayout *Ctrl;

	GTableLayoutPrivate(GTableLayout *ctrl);
	~GTableLayoutPrivate();
	TableCell *GetCellAt(int cx, int cy);
	void Layout(GRect &Client);
};

class TableCell : public GLayoutCell
{
public:
	GTableLayout *Table;
	GRect Cell;		// Cell position
	GRect Pos;		// Pixel position
	GArray<GView*> Children;
	CellAlign AlignX, AlignY;

	TableCell(GTableLayout *t, int Cx, int Cy)
	{
		AlignX = AlignY = AlignMin;
		Table = t;
		Cell.ZOff(0, 0);
		Cell.Offset(Cx, Cy);
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		return false;
	}

	bool SetVariant(char *Name, GVariant &Value, char *Array)
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
				if (v->Type == GV_VOID_PTR)
				{
					ResObject *o = (ResObject*)v->Value.Ptr;
					if (o)
					{
						GView *gv = dynamic_cast<GView*>(o);
						if (gv)
						{
							Children.Add(gv);
							Table->Children.Insert(gv);
							gv->SetParent(Table);

							GText *t = dynamic_cast<GText*>(gv);
							if (t)
							{
								t->SetWrap(true);
							}
						}
					}
				}
			}
		}
		else if (stricmp(Name, "align") == 0)
		{
			AlignX = ConvertAlign(Value.Str());
		}
		else if (stricmp(Name, "valign") == 0)
		{
			AlignY = ConvertAlign(Value.Str());
		}
		else return false;

		return true;
	}

	/// Calculates the minimum and maximum widths this cell can occupy.
	void PreLayout(int &Min, int &Max, CellFlag &Flag)
	{
		for (int i=0; i<Children.Length(); i++)
		{
			GView *v = Children[i];
			if (Izza(GText))
			{
				PreLayoutTextCtrl(v, Min, Max);
				if (Max > Min)
				{
					Flag = SizeGrow;
				}
			}
			else if (Izza(GCheckBox) OR
					 Izza(GRadioButton))
			{
				int cmin = 0, cmax = 0;
				PreLayoutTextCtrl(v, cmin, cmax);
				Min = max(Min, cmin + 16);
				Max = max(Max, cmax + 16);
			}
			else if (Izza(GButton))
			{
				GDisplayString ds(v->GetFont(), v->Name());
				int x = max(v->X(), ds.X() + BUTTON_OVERHEAD_X);
				if (x > v->X())
				{
					GRect r = v->GetPos();
					r.x2 = r.x1 + x - 1;
					v->SetPos(r);
				}

				Min = max(Min, x);
				Max = max(Max, x);
				Flag = SizeFixed;
				// Max += x + 10;
				// Flag = SizeGrow;
			}
			else if (Izza(GEdit))
			{
				Min = max(Min, 40);
				Max = max(Max, 1000);
			}
			else if (Izza(GCombo))
			{
				GCombo *Cbo = Izza(GCombo);
				GFont *f = Cbo->GetFont();
				int x = 0;
				char *t;
				for (int i=0; t = (*Cbo)[i]; i++)
				{
					GDisplayString ds(f, t);
					x = max(ds.X(), x);
				}				
				
				Min = max(Min, 40);
				Max = max(Max, x + 32);
				Flag = SizeGrow;
			}
			else if (Izza(GBitmap))
			{
				GBitmap *b = Izza(GBitmap);
				GSurface *Dc = b->GetSurface();
				if (Dc)
				{
					Min = max(Min, Dc->X() + 4);
					Max = max(Max, Dc->X() + 4);
				}
				else
				{
					Min = max(Min, 3000);
					Max = max(Max, 3000);
				}
			}
			else if (Izza(GList))
			{
				GList *Lst = Izza(GList);
				int m = 0;
				for (int i=0; i<Lst->GetColumns(); i++)
				{
					m += Lst->ColumnAt(i)->Width();
				}
				m = max(m, 40);
				Min = max(Min, 60);
				Max = max(Max, (m * 2) + 20);
				Flag = SizeGrow;
			}
			else if (Izza(GTree) OR
					 Izza(GTabView))
			{
				Min = max(Min, 40);
				Max = max(Max, 3000);
				Flag = SizeGrow;
			}
			else
			{
				GTableLayout *Tbl = Izza(GTableLayout);
				if (Tbl)
				{
					GRect r(0, 0, 10000, 10000);
					Tbl->d->Layout(r);
					Min = max(Min, Tbl->d->LayoutMinX);
					Max = max(Max, Tbl->d->LayoutMaxX);
				}
				else
				{
					Min = max(Min, v->X());
					Max = max(Max, v->X());
				}
			}
		}
	}

	/// Calculate the height of the cell based on the given width
	void Layout(int Width, int &Min, int &Max)
	{
		Pos.ZOff(Width-1, 0);
		for (int i=0; i<Children.Length(); i++)
		{
			GView *v = Children[i];
			if (v)
			{
				GTableLayout *Tbl;
				GRadioGroup *Grp;

				if (i)
				{
					Pos.y2 += CELL_SPACING;
				}

				if (Izza(GText))
				{
					int y = LayoutTextCtrl(v, 0, Pos.X());
					Pos.y2 += y;
					
					GRect r = v->GetPos();
					r.y2 = r.y1 + y - 1;
					v->SetPos(r);
				}
				else if (Izza(GEdit) OR
						 Izza(GButton) OR
						 Izza(GCombo))
				{
					int y = v->GetFont()->GetHeight() + 8;
					
					GRect r = v->GetPos();
					r.y2 = r.y1 + y - 1;
					v->SetPos(r);

					Pos.y2 += y;

					if (Izza(GEdit) AND
						Izza(GEdit)->MultiLine())
					{
						Max = max(Max, 1000);
					}
				}
				else if (Izza(GCheckBox) OR
						 Izza(GRadioButton))
				{
					int y = v->GetFont()->GetHeight() + 2;
					
					GRect r = v->GetPos();
					r.y2 = r.y1 + y - 1;
					v->SetPos(r);

					Pos.y2 += y;
				}
				else if (Izza(GList) OR
						 Izza(GTree) OR
						 Izza(GTabView))
				{
					Pos.y2 += v->GetFont()->GetHeight() + 8;
					Max = max(Max, 1000);
				}
				else if (Izza(GBitmap))
				{
					GBitmap *b = Izza(GBitmap);
					GSurface *Dc = b->GetSurface();
					if (Dc)
					{
						Max = max(Max, Dc->Y() + 4);
					}
					else
					{
						Max = max(Max, 1000);
					}
				}
				else if (Tbl = Izza(GTableLayout))
				{
					int Ht = min(v->Y(), Tbl->d->LayoutBounds.Y());

					GRect r = v->GetPos();
					r.x2 = r.x1 + min(Width, Tbl->d->LayoutMaxX) - 1;
					r.y2 = r.y1 + Ht - 1;
					v->SetPos(r);

					Pos.y2 += Ht;
				}
				else if (Grp = Izza(GRadioGroup))
				{
					GArray<GViewI*> Btns;
					int MaxY = 0;
					GRect Bounds(0, 0, -1, -1);
					bool HasOther = false;

					GAutoPtr<GViewIterator> It(Grp->IterateViews());
					for (GViewI *i=It->First(); i; i=It->Next())
					{
						GRadioButton *b = dynamic_cast<GRadioButton*>(i);
						GCheckBox *c = dynamic_cast<GCheckBox*>(i);
						GViewI *Ctrl = b ? (GViewI*)b : (GViewI*)c;
						if (Ctrl)
						{
							Btns.Add(Ctrl);
							MaxY = max(MaxY, Ctrl->GetPos().Y());
							if (Bounds.Valid())
								Bounds.Union(&Ctrl->GetPos());
							else
								Bounds = Ctrl->GetPos();
						}
						else HasOther = true;
					}

					if (!HasOther)
					{
						bool IsVert = !(Bounds.Y() < (MaxY + 5));
						GFont *f = Grp->GetFont();
						int Ht = (IsVert) ? (f->GetHeight() * (Btns.Length() + 1)) + 8 : (f->GetHeight() * 2) + 8;
						GRect r = Grp->GetPos();
						r.y2 = r.y1 + Ht - 1;
						Grp->SetPos(r);
						Pos.y2 += r.Y();

						int x = 7;
						int y = f->GetHeight() + 2;
						for (int i=0; i<Btns.Length(); i++)
						{
							GFont *BtnFont = Btns[i]->GetFont();
							char *Name = Btns[i]->Name();
							GDisplayString ds(BtnFont, Name);
							int Wid = ds.X();

							r = Btns[i]->GetPos();
							r.Offset(x - r.x1, y - r.y1);
							r.x2 = r.x1 + 24 + Wid - 1;
							Btns[i]->SetPos(r);
							if (IsVert)
							{
								y += r.Y();
							}
							else
							{
								x += r.X() + 5;
							}
						}
					}
					else
					{
						Pos.y2 += v->Y();
					}
				}
				else
				{
					Pos.y2 += v->Y();
				}
			}
		}

		Min = max(Min, Pos.Y());
		Max = max(Max, Pos.Y());
	}

	/// Called after the layout has been done to move the controls into place
	void PostLayout()
	{
		int Cx = 0;
		int Cy = 0;
		int MaxY = 0;
		int RowStart = 0;
		GArray<GRect> New;

		for (int i=0; i<Children.Length(); i++)
		{
			GView *v = Children[i];
			if (!v)
				continue;

			GTableLayout *Tbl = Izza(GTableLayout);

			GRect r = v->GetPos();
			r.Offset(Pos.x1 - r.x1 + Cx, Pos.y1 - r.y1 + Cy);

			if (Izza(GList) OR
				Izza(GTree) OR
				Izza(GTabView) OR
				(Izza(GEdit) AND Izza(GEdit)->MultiLine()))
			{
				r.y2 = Pos.y2;
			}
			if (!Izza(GButton) AND !Tbl)
			{
				r.x2 = r.x1 + Pos.X() - 1;
			}

			New[i] = r;
			MaxY = max(MaxY, r.y2);
			Cx += r.X() + CELL_SPACING;
			if (Cx >= Pos.X())
			{
				int Wid = Cx - CELL_SPACING;
				int OffsetX = 0;
				if (AlignX == AlignCenter)
				{
					OffsetX = (Pos.X() - Wid) / 2;
				}
				else if (AlignX == AlignMax)
				{
					OffsetX = Pos.X() - Wid;
				}

				for (int n=RowStart; n<=i; n++)
				{
					New[n].Offset(OffsetX, 0);
				}

				RowStart = i + 1;
				Cx = 0;
				Cy = MaxY + CELL_SPACING;
			}
		}

		int n;
		int Wid = Cx - CELL_SPACING;
		int OffsetX = 0;
		if (AlignX == AlignCenter)
		{
			OffsetX = (Pos.X() - Wid) / 2;
		}
		else if (AlignX == AlignMax)
		{
			OffsetX = Pos.X() - Wid;
		}
		for (n=RowStart; n<Children.Length(); n++)
		{
			New[n].Offset(OffsetX, 0);
		}

		int OffsetY = 0;
		if (AlignY == AlignCenter)
		{
			OffsetY = (Pos.Y() - (MaxY - Pos.y1)) / 2;
		}
		else if (AlignY == AlignMax)
		{
			OffsetY = Pos.Y() - (MaxY - Pos.y1);
		}
		for (n=0; n<Children.Length(); n++)
		{
			GView *v = Children[n];
			if (!v)
				break;

			New[n].Offset(0, OffsetY);
			v->SetPos(New[n]);
		}
	}
};

void DistributeSize(GArray<int> &a, GArray<CellFlag> &Flags, int Start, int Span, int Size, int Border)
{
	// Calculate the current size of the cells
	int Cur = -Border;
	for (int i=0; i<Span; i++)
	{
		Cur += a[Start+i] + Border;
	}

	// Is there more to allocate?
	if (Cur < Size)
	{
		// Get list of growable cells
		GArray<int> Pref;
		for (int i=Start; i<Start+Span; i++)
		{
			if (Flags[i] != SizeFixed)
			{
				Pref.Add(i);
			}
		}

		if (Pref.Length())
		{
			// Distribute size amongst the cells
			int Needs = Size - Cur;
			int Part = Needs / Pref.Length();
			for (int i=0; i<Pref.Length(); i++)
			{
				int Cell = Pref[i];
				int Add = i < Pref.Length() - 1 ? Part : Needs;
				a[Cell] = a[Cell] + Add;
				Needs -= Add;
			}
		}
		else
		{
			// Distribute size amongst the cells
			int Needs = Size - Cur;
			int Part = Needs / Span;
			for (int k=0; k<Span; k++)
			{
				int Add = k < Span - 1 ? Part : Needs;
				a[Start+k] = a[Start+k] + Add;
				Needs -= Add;
			}
		}
	}
}

GTableLayoutPrivate::GTableLayoutPrivate(GTableLayout *ctrl)
{
	Ctrl = ctrl;
	FirstLayout = false;
	CellSpacing = CELL_SPACING;
	LayoutBounds.ZOff(-1, -1);
	LayoutMinX = LayoutMaxX = 0;
}

GTableLayoutPrivate::~GTableLayoutPrivate()
{
	Cells.DeleteObjects();
}

TableCell *GTableLayoutPrivate::GetCellAt(int cx, int cy)
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

void GTableLayoutPrivate::Layout(GRect &Client)
{
	#if DEBUG_PROFILE
	int64 Start = LgiCurrentTime();
	#endif

	int Px = 0, Py = 0, Cx, Cy, i;
	GArray<int> MinCol, MaxCol;
	GArray<CellFlag> ColFlags, RowFlags;

	ColFlags.Length(Cols.Length());
	RowFlags.Length(Rows.Length());

	// Do pre-layout to determin minimum and maximum column sizes
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Non-spanned cells
				if (c->Cell.x1 == Cx AND
					c->Cell.y1 == Cy AND
					c->Cell.X() == 1 AND
					c->Cell.Y() == 1)
				{
					CellFlag Flag = SizeUnknown;
					c->PreLayout(MinCol[Cx], MaxCol[Cx], Flag);
					if (!ColFlags[Cx])
					{
						ColFlags[Cx] = Flag;
					}
				}

				Cx += c->Cell.X();
			}
			else break;
		}
	}

	// Pre-layout for spanned cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				if (c->Cell.x1 == Cx AND
					c->Cell.y1 == Cy AND
					(c->Cell.X() > 1 OR c->Cell.Y() > 1))
				{
					int Min = 0, Max = 0;
					CellFlag Flag = SizeUnknown;
					c->PreLayout(Min, Max, Flag);
					if (Flag)
					{
						for (i=c->Cell.x1; i<=c->Cell.x2; i++)
						{
							if (!ColFlags[i])
							{
								ColFlags[i] = Flag;
							}
						}
					}

					DistributeSize(MinCol, ColFlags, c->Cell.x1, c->Cell.X(), Min, CellSpacing);
					DistributeSize(MaxCol, ColFlags, c->Cell.x1, c->Cell.X(), Max, CellSpacing);
				}

				Cx += c->Cell.X();
			}
			else break;
		}
	}

	LayoutMinX = 0;
	for (i=0; i<Cols.Length(); i++)
	{
		LayoutMinX += MinCol[i] + CELL_SPACING;
		LayoutMaxX += MaxCol[i] + CELL_SPACING;
	}

	#if DEBUG_LAYOUT
	LgiTrace("Layout %i,%i\n", Client.X(), Client.Y());

	for (i=0; i<Cols.Length(); i++)
	{
		LgiTrace("\tColBefore[%i]: min=%i max=%i (f=%x)\n", i, MinCol[i], MaxCol[i], ColFlags[i]);
	}
	#endif

	// Now allocate unused space
	DistributeUnusedSpace(MinCol, MaxCol, ColFlags, Client.X(), CellSpacing);

	#if DEBUG_LAYOUT
	for (i=0; i<Cols.Length(); i++)
	{
		LgiTrace("\tColAfter[%i]: min=%i max=%i (f=%x)\n", i, MinCol[i], MaxCol[i], ColFlags[i]);
	}
	#endif

	// Do row height layout
	GArray<int> MinRow, MaxRow;
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Single cell
				if (c->Cell.x1 == Cx AND
					c->Cell.y1 == Cy)
				{
					int x = CountRange<int>(MinCol, c->Cell.x1, c->Cell.x2) +
							((c->Cell.X() - 1) * CellSpacing);

					c->Layout(x, MinRow[Cy], MaxRow[Cy]);
				}

				Cx += c->Cell.X();
			}
			else break;
		}
	}

	#if DEBUG_LAYOUT
	for (i=0; i<Rows.Length(); i++)
	{
		LgiTrace("\tRowBefore[%i]: min=%i max=%i\n", i, MinRow[i], MaxRow[i]);
	}
	#endif

	// Allocate remaining vertical space
	DistributeUnusedSpace(MinRow, MaxRow, RowFlags, Client.Y(), CellSpacing);
	
	#if DEBUG_LAYOUT
	for (i=0; i<Rows.Length(); i++)
	{
		LgiTrace("\tRowAfter[%i]: min=%i max=%i\n", i, MinRow[i], MaxRow[i]);
	}
	for (i=0; i<Cols.Length(); i++)
	{
		LgiTrace("\tFinalCol[%i]=%i\n", i, MinCol[i]);
	}
	#endif

	// Move cells into their final positions
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		int MaxY = 0;
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				if (c->Cell.x1 == Cx AND
					c->Cell.y1 == Cy)
				{
					int y = CountRange<int>(MinRow, c->Cell.y1, c->Cell.y2) +
							((c->Cell.Y() - 1) * CellSpacing);

					c->Pos.y2 = c->Pos.y1 + y - 1;
					c->Pos.Offset(Client.x1 + Px, Client.y1 + Py);
					c->PostLayout();

					MaxY = max(MaxY, c->Pos.y2);
				}

				Px += c->Pos.X() + CellSpacing;
				Cx += c->Cell.X();
			}
			else break;
		}

		Py += MinRow[Cy] + CellSpacing;
		Px = 0;
	}

	LayoutBounds.ZOff(Px-1, Py-1);

	#if DEBUG_PROFILE
	LgiTrace("GTableLayout::Layout = %i ms\n", (int)(LgiCurrentTime()-Start));
	#endif

	Ctrl->SendNotify(GTABLELAYOUT_LAYOUT_CHANGED);
}

GTableLayout::GTableLayout() : ResObject(Res_Table)
{
	d = new GTableLayoutPrivate(this);
	SetPourLargest(true);
	Name("GTableLayout");
}

GTableLayout::~GTableLayout()
{
	DeleteObj(d);
}

void GTableLayout::OnFocus(bool b)
{
	if (b)
	{
		GViewI *v = GetNextTabStop(this, false);
		if (v)
			v->Focus(true);
	}
}

void GTableLayout::OnCreate()
{
	AttachChildren();
}

int GTableLayout::CellX()
{
	return d->Cols.Length();
}

int GTableLayout::CellY()
{
	return d->Rows.Length();
}

GLayoutCell *GTableLayout::CellAt(int x, int y)
{
	return d->GetCellAt(x, y);
}

void GTableLayout::OnPosChange()
{
	GRect r = GetClient();
	r.Size(CELL_SPACING, CELL_SPACING);
	d->Layout(r);
}

GRect GTableLayout::GetUsedArea()
{
	if (!d->FirstLayout)
	{
		d->FirstLayout = true;
		OnPosChange();
	}

	TableCell *c = d->GetCellAt(d->Cols.Length()-1, d->Rows.Length()-1);
	GRect r(0, 0, c ? c->Pos.x2 : 0, c ? c->Pos.y2 : 0);
	return r;
}

void GTableLayout::InvalidateLayout()
{
	d->FirstLayout = false;
	Invalidate();
}

void GTableLayout::OnPaint(GSurface *pDC)
{
	if (!d->FirstLayout)
	{
		d->FirstLayout = true;
		OnPosChange();
	}

	GViewFill *fill = GetBackgroundFill();
	if (fill)
	{
		fill->Fill(pDC);
	}
	else
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
	}

	#if DEBUG_DRAW_CELLS
	pDC->Colour(Rgb24(255, 0, 0), 24);
	pDC->Box();
	#endif

	#if DEBUG_DRAW_CELLS
	for (int i=0; i<d->Cells.Length(); i++)
	{
		GRect r = d->Cells[i]->Pos;
		r.Size(-2, -2);
		pDC->Colour(Rgb24(192, 192, 222), 24);
		pDC->Box(&r);
		pDC->Line(r.x1, r.y1, r.x2, r.y2);
		pDC->Line(r.x2, r.y1, r.x1, r.y2);
	}
	#endif
}

bool GTableLayout::GetVariant(char *Name, GVariant &Value, char *Array)
{
	return false;
}

bool ConvertNumbers(GArray<double> &a, char *s)
{
	GToken t(s, ",");
	for (int i=0; i<t.Length(); i++)
	{
		a.Add(atof(t[i]));
	}
	return a.Length() > 0;
}

bool GTableLayout::SetVariant(char *Name, GVariant &Value, char *Array)
{
	if (stricmp(Name, "cols") == 0)
	{
		return ConvertNumbers(d->Cols, Value.Str());
	}
	else if (stricmp(Name, "rows") == 0)
	{
		return ConvertNumbers(d->Rows, Value.Str());
	}
	else if (stricmp(Name, "cell") == 0)
	{
		if (Array)
		{
			GToken t(Array, ",");
			if (t.Length() == 2)
			{
				int cx = atoi(t[0]);
				int cy = atoi(t[1]);
				TableCell *c = new TableCell(this, cx, cy);
				if (c)
				{
					d->Cells.Add(c);
					if (Value.Type == GV_VOID_PTR)
					{
						GDom **d = (GDom**)Value.Value.Ptr;
						if (d)
						{
							*d = c;
						}
					}
				}
				else return false;
			}
			else return false;
		}
		else return false;
	}
	else return false;

	return true;
}

void GTableLayout::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (!Attaching)
	{
		for (int i=0; i<d->Cells.Length(); i++)
		{
			TableCell *c = d->Cells[i];
			for (int n=0; n<c->Children.Length(); n++)
			{
				if (c->Children[n] == Wnd)
				{
					c->Children.DeleteAt(n);
					return;
				}
			}
		}

		LgiAssert(0);
	}
}
