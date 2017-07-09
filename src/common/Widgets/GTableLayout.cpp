#include "Lgi.h"
#include "GTableLayout.h"
#include "GCssTools.h"
#include "GDisplayString.h"
#include "LgiRes.h"

enum CellFlag
{
    // A null value when the call flag is not known yet
	SizeUnknown,
	// The cell contains fixed size objects
	SizeFixed,
	// The cell contains objects that have variable size
	SizeGrow,
	// The cell contains objects that will use all available space
	SizeFill,
};

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
#include "GScrollBar.h"
#include "GCss.h"

#define Izza(c)				dynamic_cast<c*>(v)
// #define DEBUG_LAYOUT		539
#define DEBUG_PROFILE		0
#define DEBUG_DRAW_CELLS	0

int GTableLayout::CellSpacing = 4;

const char *FlagToString(CellFlag f)
{
	switch (f)
	{
		case SizeUnknown: return "Unknown";
		case SizeFixed:   return "Fixed";
		case SizeGrow:    return "Grow";
		case SizeFill:    return "Fill";
	}
	return "error";
}

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
    int Priority;
	int Col;		// index of column
	int Grow;		// in px
};

int Cmp(UnderInfo *a, UnderInfo *b)
{
    if (a->Priority != b->Priority)
        return a->Priority - b->Priority;
	return b->Grow - a->Grow;
}

static void
DistributeUnusedSpace(	GArray<int> &Min,
						GArray<int> &Max,
						GArray<CellFlag> &Flags,
						int Total,
						int CellSpacing,
						GStream *Debug = NULL)
{
	// Now allocate unused space
	int Borders = Min.Length() - 1;
	int Sum = CountRange<int>(Min, 0, Borders) + (Borders * CellSpacing);
	if (Sum >= Total)
		return;

	int i, Avail = Total - Sum;

	// Do growable ones first
	GArray<UnderInfo> Unders;
	int UnknownGrow = 0;
	for (i=0; i<Min.Length(); i++)
	{
		CellFlag f = Flags[i];
		if (/* f == SizeGrow ||*/ f == SizeFill)
		{
			UnderInfo &u = Unders.New();
			u.Col = i;
			u.Grow = 0;
			u.Priority = f == SizeGrow ? 2 : 3;
			UnknownGrow++;

			/*			
			if (Debug)
				Debug->Print("\t\tAdding[%i] fill, pri=%i\n", i, u.Priority);
			*/
		}
		else if (Max[i] > Min[i])
		{
			UnderInfo &u = Unders.New();
			u.Col = i;
			u.Grow = Max[i] - Min[i];
			if (u.Grow > Avail)
			{
			    u.Grow = 0;
			    u.Priority = 2;
			    UnknownGrow++;
			}
			else
			{
			    u.Priority = u.Grow < Avail >> 1 ? 0 : 1;
			}

			/*
			if (Debug)
				Debug->Print("\t\tAdding[%i] grow, pri=%i\n", i, u.Priority);
			*/
		}
	}
	
	Unders.Sort(Cmp);

    int UnknownSplit = 0;
	for (i=0; Avail>0 && i<Unders.Length(); i++)
	{
	    UnderInfo &u = Unders[i];
		if (u.Grow)
		{
			/*
			if (Debug)
				Debug->Print("\t\tGrow[%i] %i += %i\n", i, Min[u.Col], u.Grow);
			*/

		    Min[u.Col] += u.Grow;
		    Avail -= u.Grow;
		}
		else
		{
		    if (!UnknownSplit)
		        UnknownSplit = Avail / UnknownGrow;
		    if (!UnknownSplit)
		        UnknownSplit = Avail;
		    
		    /*    
			if (Debug)
				Debug->Print("\t\tFill[%i] %i += %i\n", i, Min[u.Col], UnknownSplit);
			*/

		    Min[u.Col] += UnknownSplit;
		    Avail -= UnknownSplit;
		}
	}
}

static void
DistributeSize(	GArray<int> &a,
				GArray<CellFlag> &Flags,
				int Start, int Span,
				int Size,
				int Border,
				GStream *Debug = NULL)
{
	// Calculate the current size of the cells
	int Cur = -Border;
	for (int i=0; i<Span; i++)
	{
		Cur += a[Start+i] + Border;
	}

	// Is there more to allocate?
	if (Cur >= Size)
		return;

	// Get list of growable cells
	GArray<int> Grow, Fill, Fixed, Unknown;
	int ExistingGrowPx = 0;
	int ExistingFillPx = 0;
	int UnknownPx = 0;

	for (int i=Start; i<Start+Span; i++)
	{
	    switch (Flags[i])
	    {
			default:
				break;
			case SizeUnknown:
			{
				Unknown.Add(i);
				UnknownPx += a[i];
				break;
			}
			case SizeFixed:
			{
				Fixed.Add(i);
				break;
			}
	        case SizeGrow:
		    {
			    Grow.Add(i);
			    ExistingGrowPx += a[i];
			    break;
		    }
		    case SizeFill:
		    {
			    Fill.Add(i);
			    ExistingFillPx += a[i];
		        break;
		    }
		}
	}

	int AdditionalSize = Size - Cur;
	if (Fill.Length())
	{
		// Distribute size amongst the growable cells
		int PerFillSize = AdditionalSize / Fill.Length();
		if (PerFillSize <= 0)
		    PerFillSize = 1;
		    
		for (int i=0; i<Fill.Length(); i++)
		{
			int Cell = Fill[i];
			int Add = i == Fill.Length() - 1 ? AdditionalSize : PerFillSize;
			a[Cell] = a[Cell] + Add;
			AdditionalSize -= Add;
		}
	}
	else if (Grow.Length())
	{
		// Distribute size amongst the growable cells
		int AdditionalSize = Size - Cur;
		for (int i=0; i<Grow.Length(); i++)
		{
			int Cell = Grow[i];
			int Add;
			if (a[Cell] && ExistingGrowPx)
			    Add = a[Cell] * AdditionalSize / ExistingGrowPx;
			else
			    Add = max(1, AdditionalSize / Grow.Length());
			a[Cell] = a[Cell] + Add;
		}
	}
	else if (Fixed.Length() > 0 && Unknown.Length() > 0)
	{
		// Distribute size amongst the unknown cells
		int AdditionalSize = Size - Cur;
		for (int i=0; i<Unknown.Length(); i++)
		{
			int Cell = Unknown[i];
			int Add;
			if (a[Cell] && UnknownPx)
			    Add = a[Cell] * AdditionalSize / UnknownPx;
			else
			    Add = max(1, AdditionalSize / Unknown.Length());
			a[Cell] = a[Cell] + Add;
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

GCss::LengthType ConvertAlign(char *s, bool x_axis)
{
	if (s)
	{
		if (stricmp(s, "Center") == 0)
			return x_axis ? GCss::AlignCenter : GCss::VerticalMiddle;
		if (stricmp(s, "Max") == 0)
			return x_axis ? GCss::AlignRight : GCss::VerticalBottom;
	}

	return x_axis ? GCss::AlignLeft : GCss::VerticalTop;
}

class TableCell : public GLayoutCell
{
public:
	struct Child
	{
		GViewLayoutInfo Inf;
		GView *View;
		bool IsLayout;
		bool Debug;
	};

	GTableLayout *Table;
	GRect Cell;		// Cell position
	GRect Pos;		// Pixel position
	GRect Padding;	// Cell padding from CSS styles
	GArray<Child> Children;
	GCss::DisplayType Disp;
	GString ClassName;
	bool Debug;

	TableCell(GTableLayout *t, int Cx, int Cy);
	GTableLayout *GetTable() { return Table; }
	bool Add(GView *v);	
	bool Remove(GView *v);
	bool RemoveAll();
	Child *HasView(GView *v);

	bool IsSpanned();
	bool GetVariant(const char *Name, GVariant &Value, char *Array);
	bool SetVariant(const char *Name, GVariant &Value, char *Array);
	int MaxCellWidth();

	/// Calculates the minimum and maximum widths this cell can occupy.
	void PreLayout(int &MinX, int &MaxX, CellFlag &Flag);
	/// Calculate the height of the cell based on the given width
	void Layout(int Width, int &MinY, int &MaxY, CellFlag &Flags);
	/// Called after the layout has been done to move the controls into place
	void PostLayout();
	void OnPaint(GSurface *pDC);
};

class GTableLayoutPrivate
{
	friend class TableCell;

	bool InLayout;
	bool DebugLayout;

public:
	int PrevWidth;
	GArray<double> Rows, Cols;
	GArray<TableCell*> Cells;
	int BorderSpacing;
	GRect LayoutBounds;
	int LayoutMinX, LayoutMaxX;
	GTableLayout *Ctrl;

	// Object
	GTableLayoutPrivate(GTableLayout *ctrl);
	~GTableLayoutPrivate();
	
	// Utils
	bool IsInLayout() { return InLayout; }
	TableCell *GetCellAt(int cx, int cy);
	void Empty(GRect *Range = NULL);
    bool CollectRadioButtons(GArray<GRadioButton*> &Btns);
    void InitBorderSpacing();

	// Layout temporary values
	GStringPipe Dbg;
	GArray<int> MinCol, MaxCol;
	GArray<int> MinRow, MaxRow;
	GArray<CellFlag> ColFlags, RowFlags;

	// Layout staged methods, call in order to complete the layout
	void LayoutHorizontal(GRect &Client, int *MinX = NULL, int *MaxX = NULL, CellFlag *Flag = NULL);
	void LayoutVertical(GRect &Client, int *MinY = NULL, int *MaxY = NULL, CellFlag *Flag = NULL);
	void LayoutPost(GRect &Client);
	
	// This does the whole layout, basically calling all the stages for you
	void Layout(GRect &Client);
};

///////////////////////////////////////////////////////////////////////////////////////////
TableCell::TableCell(GTableLayout *t, int Cx, int Cy)
{
	TextAlign(AlignLeft);
	VerticalAlign(VerticalTop);
	Table = t;
	Debug = false;
	Cell.ZOff(0, 0);
	Cell.Offset(Cx, Cy);
	Padding.ZOff(0, 0);
	Disp = GCss::DispBlock;
}

TableCell::Child *TableCell::HasView(GView *v)
{
	unsigned Len = Children.Length();
	if (!Len)
		return NULL;

	Child *s = &Children[0];
	Child *e = s + Len;
	while (s < e)
	{
		if (s->View == v)
			return s;
		s++;
	}
	return NULL;
}

bool TableCell::Add(GView *v)
{
	if (HasView(v))
		return false;

	Table->AddView(v);
	Children.New().View = v;
	return true;
}

bool TableCell::Remove(GView *v)
{
	Child *c = HasView(v);
	if (!c)
		return false;
		
	Table->DelView(v);

	int Idx = (int) (c - &Children.First());
	Children.DeleteAt(Idx, true);
	return true;
}

bool TableCell::RemoveAll()
{
	Child *c = &Children[0];
	for (unsigned i=0; i<Children.Length(); i++)
	{
		DeleteObj(c[i].View);
	}
	return Children.Length(0);
}

bool TableCell::IsSpanned()
{
	return Cell.X() > 1 || Cell.Y() > 1;
}

bool TableCell::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty Fld = LgiStringToDomProp(Name);
	switch (Fld)
	{
		case ContainerChildren: // Type: GView[]
		{
			if (!Value.SetList())
				return false;

			for (unsigned i=0; i<Children.Length(); i++)
			{
				Child &c = Children[i];
				GVariant *v = new GVariant;
				if (v)
				{
					v->Type = GV_GVIEW;
					v->Value.View = c.View;
					Value.Value.Lst->Insert(v);
				}
			}
			break;
		}
		case ContainerSpan: // Type: GRect
		{
			Value = Cell.GetStr();
			break;
		}
		case ContainerAlign: // Type: String
		{
			GStringPipe p(128);
			if (TextAlign().ToString(p))
				Value.OwnStr(p.NewStr());
			else
				return false;
			break;
		}
		case ContainerVAlign: // Type: String
		{
			GStringPipe p(128);
			if (VerticalAlign().ToString(p))
				Value.OwnStr(ToString());
			else
				return false;
			break;
		}
		case ObjClass: // Type: String
		{
			Value = ClassName;
			break;
		}
		case ObjStyle: // Type: String
		{
			Value.OwnStr(ToString());
			break;
		}
		case ObjDebug:
		{
			Value = Debug;
			break;
		}
		default:
			return false;
	}

	return true;
}

bool TableCell::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty Fld = LgiStringToDomProp(Name);
	switch (Fld)
	{
		case ContainerChildren: // Type: GView[]
		{
			if (Value.Type != GV_LIST)
			{
				LgiAssert(!"Should be a list.");
				return false;
			}
			
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
							Children.New().View = gv;
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
			break;
		}
		case ContainerSpan:
		{
			GRect r;
			if (r.SetStr(Value.Str()))
				Cell = r;
			else
				return false;
			break;
		}
		case ContainerAlign:
		{
			TextAlign
			(
				Len
				(
					ConvertAlign(Value.Str(), true)
				)
			);
			break;
		}
		case ContainerVAlign:
		{
			VerticalAlign
			(
				Len
				(
					ConvertAlign(Value.Str(), false)
				)
			);
			break;
		}
		case ObjClass:
		{
			ClassName = Value.Str();
			
			LgiResources *r = LgiGetResObj();
			if (r)
			{	
				GCss::SelArray *a = r->CssStore.ClassMap.Find(ClassName);
				if (a)
				{
					for (int i=0; i<a->Length(); i++)
					{
						GCss::Selector *s = (*a)[i];
						
						// This is not exactly a smart matching algorithm.
						if (s && s->Parts.Length() == 1)
						{
							const char *style = s->Style;
							Parse(style, ParseRelaxed);
						}
					}
				}
			}
			break;
		}
		case ObjStyle:
		{
			const char *style = Value.Str();
			if (style)
				Parse(style, ParseRelaxed);
			break;
		}
		case ObjDebug:
		{
			Debug = Value.CastInt32() != 0;
			break;
		}
		default:
			return false;
	}

	return true;
}

int TableCell::MaxCellWidth()
{
	// Table size minus padding
	GCssTools t(Table->GetCss(), Table->GetFont());
	GRect cli = Table->GetClient();
	cli = t.ApplyPadding(cli);
	
	// Work out any borders on spanned cells...
	int BorderPx = Cell.X() > 1 ? (Cell.X() - 1) * Table->d->BorderSpacing : 0;
	
	// Return client - padding - border size.
	return cli.X() - BorderPx;
}

/// Calculates the minimum and maximum widths this cell can occupy.
void TableCell::PreLayout(int &MinX, int &MaxX, CellFlag &Flag)
{
	int MaxBtnX = 0;
	int TotalBtnX = 0;
	int Min = 0, Max = 0;

	// Calculate CSS padding
	#define CalcCssPadding(Prop, Axis, Edge) \
	{ \
		Len l = Prop(); \
		if (l.Type == GCss::LenInherit) l = GCss::Padding(); \
		if (l.Type) \
			Padding.Edge = l.ToPx(Table->Axis(), Table->GetFont()); \
		else \
			Padding.Edge = 0; \
	}
	
	Disp = Display();
	if (Disp == DispNone)
		return;
	
	CalcCssPadding(PaddingLeft, X, x1)
	CalcCssPadding(PaddingRight, X, x2)
	CalcCssPadding(PaddingTop, Y, y1)
	CalcCssPadding(PaddingBottom, Y, y2)

	Len Wid = Width();
	if (Wid.IsValid())
	{
		int Tx = Table->X();
		
		if (Wid.Type == GCss::LenAuto)
			Flag = SizeFill;
		else
		{
			Max = Wid.ToPx(Tx, Table->GetFont()) - Padding.x1 - Padding.x2;
			
			if (!Wid.IsDynamic())
			{
				Min = Max;
				Flag = SizeFixed;

				if (Padding.x1 + Padding.x2 > Min)
				{
					// Remove padding as it's going to oversize the cell
					Padding.x1 = Padding.x2 = 0;
				}
			}
			else Flag = SizeGrow;
		}
	}

	if (!Wid.IsValid() || Wid.IsDynamic())
	{
		Child *c = &Children[0];
		for (int i=0; i<Children.Length(); i++, c++)
		{
			GView *v = c->View;
			if (!v)
				continue;

			ZeroObj(c->Inf);
			
			// const char *Cls = v->GetClass();
			GCss *Css = v->GetCss();
			GCss::Len ChildWid;
			if (Css)
				ChildWid = Css->Width();
				
			if (ChildWid.IsValid())
			{
				int MaxPx = MaxCellWidth();
				int Px = ChildWid.ToPx(MaxPx, v->GetFont());
				Min = max(Min, Px);
				Max = max(Max, Px);
				
				GRect r = v->GetPos();
				r.x2 = r.x1 + Px - 1;
				v->SetPos(r);
			}
			else if (v->OnLayout(c->Inf))
			{
				if (c->Inf.Width.Max < 0)
				{
					if (Flag < SizeFill)
						Flag = SizeFill;
				}
				else	
				{
					Max = max(Max, c->Inf.Width.Max);
				}

				if (c->Inf.Width.Min)
				{
					Min = max(Min, c->Inf.Width.Min);
					if (c->Inf.Width.Max > c->Inf.Width.Min &&
						Flag == SizeUnknown)
						Flag = SizeGrow;
				}
			}
			else if (Izza(GButton))
			{
				GDisplayString ds(v->GetFont(), v->Name());
				int x = max(v->X(), ds.X() + GButton::Overhead.x);
				if (x > v->X())
				{
					// Resize the button to show all the text on it...
					GRect r = v->GetPos();
					r.x2 = r.x1 + x - 1;
					v->SetPos(r);
				}

				MaxBtnX = max(MaxBtnX, x);
				TotalBtnX = TotalBtnX ? TotalBtnX + GTableLayout::CellSpacing + x : x;
				
				if (Flag < SizeFixed)
					Flag = SizeFixed;
			}
			else if (Izza(GEdit) ||
					 Izza(GScrollBar))
			{
				Min = max(Min, 40);
				Flag = SizeFill;
			}
			else if (Izza(GCombo))
			{
				int PadX = GCombo::Pad.x1 + GCombo::Pad.x2;
				GCombo *Cbo = Izza(GCombo);
				GFont *f = Cbo->GetFont();
				int min_x = -1, max_x = 0;
				char *t;
				for (int i=0; i < Cbo->Length() && (t = (*Cbo)[i]); i++)
				{
					GDisplayString ds(f, t);
					min_x = min_x < 0 ? ds.X() : min(min_x, ds.X());
					max_x = max(ds.X() + 4, max_x);
				}				
				
				Min = max(Min, min_x + PadX);
				Max = max(Max, max_x + PadX);
				if (Flag < SizeGrow)
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
					Min = max(Min, 16);
					Max = max(Max, 16);
					if (Flag < SizeFill)
						Flag = SizeFill;
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
				Min = max(Min, 40);
				Flag = SizeFill;
			}
			else if (Izza(GTree) ||
					 Izza(GTabView))
			{
				Min = max(Min, 40);
				Flag = SizeFill;
			}
			else
			{
				GTableLayout *Tbl = Izza(GTableLayout);
				if (Tbl)
				{
					Tbl->d->InitBorderSpacing();
					Tbl->d->LayoutHorizontal(Table->GetClient(), &Min, &Max, &Flag);
					
					if (Wid.IsDynamic())
					{
						if (Min > Max)
							Min = Max;
					}
				}
				else
				{
					Min = max(Min, v->X());
					Max = max(Max, v->X());
				}
			}
		}
	}

	if (MaxBtnX)
	{
		Min = max(Min, MaxBtnX);
		Max = max(Max, TotalBtnX);
	}

	Len MaxWid = MaxWidth();
	if (MaxWid.IsValid())
	{
		int Tx = Table->X();
		int Px = MaxWid.ToPx(Tx, Table->GetFont()) - Padding.x1 - Padding.x2;
		if (Min > Px)
			Min = Px;
		if (Max > Px)
			Max = Px;
	}

	MinX = max(MinX, Min + Padding.x1 + Padding.x2);
	MaxX = max(MaxX, Max + Padding.x1 + Padding.x2);

}

/// Calculate the height of the cell based on the given width
void TableCell::Layout(int Width, int &MinY, int &MaxY, CellFlag &Flags)
{
	Pos.ZOff(Width-1, 0);
	if (Disp == DispNone)
		return;
	
	Len Ht = Height();
	if (Ht.Type != LenInherit)
	{
		if (Ht.IsDynamic())
		{
			MaxY = Ht.ToPx(Table->Y(), Table->GetFont());
			if (Flags < SizeGrow)
				Flags = SizeGrow;
		}
		else
		{
			MinY = MaxY = Ht.ToPx(Table->Y(), Table->GetFont());
			Pos.y2 = MinY - 1;
			if (Flags < SizeFixed)
				Flags = SizeFixed;
			return;
		}
	}
	
	int BtnX = 0;
	int BtnRows = -1;
	
	GCss::Len CssWidth = GCss::Width();
	Width -= Padding.x1 + Padding.x2;
	LgiAssert(Width >= 0);
	
	Child *c = &Children[0];
	for (int i=0; i<Children.Length(); i++, c++)
	{
		GView *v = c->View;
		if (!v)
			continue;

		if (CssWidth.Type != LenInherit)
		{
			// In the case where the CSS width is set, we need to default the
			// OnLayout info with valid widths otherwise the OnLayout calls will
			// not fill out the height, but initialize the width instead.
			c->Inf.Width.Min = Width;
			c->Inf.Width.Max = Width;
		}
		
		GTableLayout *Tbl = NULL;
		// GRadioGroup *Grp = NULL;

		GCss *Css = v->GetCss();
		GCss::Len Ht;
		if (Css)
			Ht = Css->Height();

		if (!Izza(GButton) && i)
			Pos.y2 += Table->d->BorderSpacing;

		if (c->Inf.Width.Min > Width)
			c->Inf.Width.Min = Width;
		if (c->Inf.Width.Max > Width)
			c->Inf.Width.Max = Width;

		if (Ht.IsValid())
		{
			int CtrlHeight = Ht.ToPx(Table->Y(), v->GetFont());
			if (MaxY < CtrlHeight)
				MaxY = CtrlHeight;
			if (!Ht.IsDynamic() && MinY < CtrlHeight)
				MinY = CtrlHeight;
			
			GRect r = v->GetPos();
			r.y2 = r.y1 + CtrlHeight - 1;
			v->SetPos(r);
			
			Pos.y2 = max(Pos.y2, r.Y()-1);
		}
		else if (v->OnLayout(c->Inf))
		{
			// Supports layout info
			GRect r = v->GetPos();

			if (c->Inf.Height.Max < 0)
				Flags = SizeFill;
			else
			{
				Pos.y2 += c->Inf.Height.Max - 1;
				r.y2 = r.y1 + c->Inf.Height.Max - 1;
			}

			int Px = min(Width, c->Inf.Width.Max);
			r.x2 = r.x1 + Px - 1;
			v->SetPos(r);

			c->IsLayout = true;
		}
		else if (Izza(GScrollBar))
		{
			Pos.y2 += 15;
		}
		else if (Izza(GButton))
		{
			int y = v->GetFont()->GetHeight() + GButton::Overhead.y;
			if (BtnRows < 0)
			{
				// Setup first row
				BtnRows = 1;
				Pos.y2 += y;
			}
			
			if (BtnX + v->X() > Width)
			{
				// Wrap
				BtnX = v->X();
				BtnRows++;
				Pos.y2 += y + Table->d->BorderSpacing;
			}
			else
			{
				// Don't wrap
				BtnX += v->X() + Table->d->BorderSpacing;
			}
			
			// Set button height..
			GRect r = v->GetPos();
			r.y2 = r.y1 + y - 1;
			v->SetPos(r);
		}
		else if (Izza(GEdit) || Izza(GCombo))
		{
			GFont *f = v->GetFont();
			int y = (f ? f : SysFont)->GetHeight() + 8;
			
			GRect r = v->GetPos();
			r.y2 = r.y1 + y - 1;
			v->SetPos(r);

			Pos.y2 += y;

			if (Izza(GEdit) &&
				Izza(GEdit)->MultiLine())
			{
				MaxY = max(MaxY, 1000);
			}
		}
		else if (Izza(GRadioButton))
		{
			int y = v->GetFont()->GetHeight() + 2;
			
			GRect r = v->GetPos();
			r.y2 = r.y1 + y - 1;
			v->SetPos(r);

			Pos.y2 += y;
		}
		else if (Izza(GList) ||
				 Izza(GTree) ||
				 Izza(GTabView))
		{
			Pos.y2 += v->GetFont()->GetHeight() + 8;
			// MaxY = max(MaxY, 1000);
			Flags = SizeFill;
		}
		else if (Izza(GBitmap))
		{
			GBitmap *b = Izza(GBitmap);
			GSurface *Dc = b->GetSurface();
			if (Dc)
			{
				MaxY = max(MaxY, Dc->Y() + 4);
			}
			else
			{
				MaxY = max(MaxY, 1000);
			}
		}
		else if ((Tbl = Izza(GTableLayout)))
		{
			GRect r;
			r.ZOff(Width-1, Table->Y()-1);
			Tbl->d->InitBorderSpacing();
			Tbl->d->LayoutVertical(r, &MinY, &MaxY, &Flags);
			Pos.y2 += MinY;
			
			c->Inf.Height.Min = MinY;
			c->Inf.Height.Max = MaxY;
		}
		else
		{
			// Doesn't support layout info
			Pos.y2 += v->Y();
		}
	}
	
	MinY = max(MinY, Pos.Y() + Padding.y1 + Padding.y2);
	MaxY = max(MaxY, Pos.Y() + Padding.y1 + Padding.y2);
}

/// Called after the layout has been done to move the controls into place
void TableCell::PostLayout()
{
	int Cx = Padding.x1;
	int Cy = Padding.y1;
	int MaxY = Padding.y1;
	int RowStart = 0;
	GArray<GRect> New;
	int WidthPx = Pos.X() - Padding.x1 - Padding.x2;
	int HeightPx = Pos.Y() - Padding.y1 - Padding.y2;

	Child *c = &Children[0];
	for (int i=0; i<Children.Length(); i++, c++)
	{
		GView *v = c->View;
		if (!v)
			continue;
			
		if (Disp == DispNone)
		{
			v->Visible(false);
			continue;
		}
		
		GTableLayout *Tbl = Izza(GTableLayout);
		GRect r = v->GetPos();

		if (i > 0 && Cx + r.X() > Pos.X())
		{
			// Do wrapping
			int Wid = Cx - Table->d->BorderSpacing;
			int OffsetX = 0;
			if (TextAlign().Type == AlignCenter)
			{
				OffsetX = (Pos.X() - Wid) / 2;
			}
			else if (TextAlign().Type == AlignRight)
			{
				OffsetX = Pos.X() - Wid;
			}

			for (int n=RowStart; n<=i; n++)
			{
				New[n].Offset(OffsetX, 0);
			}

			RowStart = i + 1;
			Cx = Padding.x1;
			Cy = MaxY + Table->d->BorderSpacing;
		}

		r.Offset(Pos.x1 - r.x1 + Cx, Pos.y1 - r.y1 + Cy);

		if (c->Inf.Width.Max >= WidthPx)
			c->Inf.Width.Max = WidthPx;
		if (c->Inf.Height.Max >= HeightPx)
			c->Inf.Height.Max = HeightPx;

		if (r.Y() > HeightPx)
			r.y2 = r.y1 + HeightPx - 1;

		if (Tbl)
		{
			int HeightPx = c->Inf.Height.Min < c->Inf.Height.Max ? c->Inf.Height.Min : Pos.Y();
			r.Dimension(Pos.X(), HeightPx);
		}
		else if
		(
			Izza(GList) ||
			Izza(GTree) ||
			Izza(GTabView) ||
			(Izza(GEdit) && Izza(GEdit)->MultiLine())
		)
		{
			r.y2 = Pos.y2;
		}
		else if (c->IsLayout)
		{
			if (c->Inf.Height.Max < 0)
				r.y2 = Pos.y2;
			else
				r.y2 = r.y1 + c->Inf.Height.Max - 1;
		}

		if (!Izza(GButton) && !Tbl)
		{
			if (c->Inf.Width.Max <= 0 ||
				c->Inf.Width.Max >= WidthPx)
				r.x2 = r.x1 + WidthPx - 1;
			else if (c->Inf.Width.Max)
				r.x2 = r.x1 + c->Inf.Width.Max - 1;
		}

		New[i] = r;
		MaxY = max(MaxY, r.y2 - Pos.y1);
		Cx += r.X() + Table->d->BorderSpacing;
	}

	if (Disp == DispNone)
	{
		return;
	}
	
	int n;
	int Wid = Cx - Table->d->BorderSpacing;
	int OffsetX = 0;
	if (TextAlign().Type == AlignCenter)
	{
		OffsetX = (Pos.X() - Wid) / 2;
	}
	else if (TextAlign().Type == AlignRight)
	{
		OffsetX = Pos.X() - Wid;
	}
	if (OffsetX)
	{
		for (n=RowStart; n<Children.Length(); n++)
		{
			New[n].Offset(OffsetX, 0);
		}
	}

	int OffsetY = 0;
	GCss::Len VAlign = VerticalAlign();
	if (VAlign.Type == VerticalMiddle)
	{
		int Py = Pos.Y();
		OffsetY = (Py - MaxY) / 2;
	}
	else if (VAlign.Type == VerticalBottom)
	{
		int Py = Pos.Y();
		OffsetY = Py - MaxY;
	}

	#if DEBUG_LAYOUT
	if (Table->d->DebugLayout)
	{
		Table->d->Dbg.Print("\tCell[%i,%i]=%s\n", Cell.x1, Cell.y1, Pos.GetStr());
	}
	#endif
	for (n=0; n<Children.Length(); n++)
	{
		GView *v = Children[n].View;
		if (!v)
			break;

		New[n].Offset(0, OffsetY);

		#if DEBUG_LAYOUT
		if (Table->d->DebugLayout)
		{
			Table->d->Dbg.Print("\t\tView[%i]=%s Offy=%i\n", n, New[n].GetStr(), OffsetY);
		}
		#endif

		v->SetPos(New[n]);		
		v->Visible(true);
	}
}

void TableCell::OnPaint(GSurface *pDC)
{
	GCssTools t(this, Table->GetFont());
	GRect r = Pos;	
	t.PaintBorder(pDC, r);
}

////////////////////////////////////////////////////////////////////////////
GTableLayoutPrivate::GTableLayoutPrivate(GTableLayout *ctrl)
{
	PrevWidth = -1;
	InLayout = false;
	DebugLayout = false;
	Ctrl = ctrl;
	BorderSpacing = GTableLayout::CellSpacing;
	LayoutBounds.ZOff(-1, -1);
	LayoutMinX = LayoutMaxX = 0;
}

GTableLayoutPrivate::~GTableLayoutPrivate()
{
	Empty();
}

bool GTableLayoutPrivate::CollectRadioButtons(GArray<GRadioButton*> &Btns)
{
    GAutoPtr<GViewIterator> it(Ctrl->IterateViews());
    for (GViewI *i = it->First(); i; i = it->Next())
    {
        GRadioButton *b = dynamic_cast<GRadioButton*>(i);
        if (b) Btns.Add(b);
    }
    return Btns.Length() > 0;
}

void GTableLayoutPrivate::Empty(GRect *Range)
{
	if (Range)
	{
		// Clear a range of cells..
		for (int i=0; i<Cells.Length(); i++)
		{
			TableCell *c = Cells[i];
			if (Range->Overlap(&c->Cell))
			{
				c->RemoveAll();
				Cells.DeleteAt(i--, true);
				DeleteObj(c);
			}
		}
	}
	else
	{
		// Clear all the cells
		GViewI *c;
		GAutoPtr<GViewIterator> it(Ctrl->IterateViews());
		while ((c = it->First()))
		{
			DeleteObj(c);
		}
		
		Cells.DeleteObjects();
		Rows.Length(0);
		Cols.Length(0);
	}

	PrevWidth = -1;
	LayoutBounds.ZOff(-1, -1);
	LayoutMinX = LayoutMaxX = 0;
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

void GTableLayoutPrivate::LayoutHorizontal(GRect &Client, int *MinX, int *MaxX, CellFlag *Flag)
{
	// This only gets called when you nest GTableLayout controls. It's 
	// responsible for doing pre layout stuff for an entire control of cells.
	int Cx, Cy, i;

	// Zero everything to start with
	MinCol.Length(0);
	MaxCol.Length(0);
	MinRow.Length(0);
	MaxRow.Length(0);
	ColFlags.Length(0);
	RowFlags.Length(0);

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		int asd=0;
	}
	#endif
		
	// Do pre-layout to determine minimum and maximum column widths
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Non-spanned cells
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.X() == 1)
				{
					c->PreLayout(MinCol[Cx], MaxCol[Cx], ColFlags[Cx]);
				}

				Cx += c->Cell.X();
			}
			else
				Cx++;
		}
	}

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		Dbg.Print("Layout Id=%i, Size=%i,%i\n", Ctrl->GetId(), Client.X(), Client.Y());
		for (i=0; i<Cols.Length(); i++)
		{
			Dbg.Print(	"\tLayoutHorizontal.AfterSingle[%i]: min=%i max=%i (%s)\n",
						i,
						MinCol[i], MaxCol[i],
						FlagToString(ColFlags[i]));
		}
	}
	#endif

	// Pre-layout column width for spanned cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.X() > 1)
				{
					int Min = 0, Max = 0;
					CellFlag Flag = SizeUnknown;
					
					if (DebugLayout)
					{
						int asd=0;
					}

					if (c->Width().IsValid())
					{
						GCss::Len l = c->Width();
						if (l.Type == GCss::LenAuto)
						{
							for (int i=c->Cell.x1; i<=c->Cell.x2; i++)
							{
								ColFlags[i] = SizeFill;
							}
						}
						else
						{
							int Px = l.ToPx(Client.X(), Ctrl->GetFont());;
							if (l.IsDynamic())
							{
								c->PreLayout(Min, Max, Flag);
							}
							else
							{
								Min = Max = Px;
							}
						}
					}
					else
					{
						c->PreLayout(Min, Max, Flag);
					}

					if (Max > Client.X())
						Max = Client.X();
					if (Flag)
					{
						bool HasFlag = false;
						bool HasUnknown = false;
						for (i=c->Cell.x1; i<=c->Cell.x2; i++)
						{
							if (ColFlags[i] == Flag)
							{
								HasFlag = true;
							}
							else if (ColFlags[i] == SizeUnknown)
							{
								HasUnknown = true;
							}
						}
						
						if (!HasFlag)
						{
							for (i=c->Cell.x1; i<=c->Cell.x2; i++)
							{
								if (HasUnknown)
								{
									if (ColFlags[i] == SizeUnknown)
										ColFlags[i] = Flag;
								}
								else
								{
									if (ColFlags[i] < Flag)
										ColFlags[i] = Flag;
								}
							}
						}
					}
					
					DistributeSize(MinCol, ColFlags, c->Cell.x1, c->Cell.X(), Min, BorderSpacing);
					
					// This is the total size of all the px currently allocated
					int AllPx = CountRange(MinCol, 0, Cols.Length()-1) + ((Cols.Length() - 1) * BorderSpacing);
					
					// This is the minimum size of this cell's cols
					int MyPx = CountRange(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1) * BorderSpacing);
					
					// This is the total remaining px we could add...
					int Remaining = Client.X() - AllPx;
					if (Remaining > 0)
					{
						// Limit the max size of this cell to the existing + remaining px
						Max = min(Max, MyPx + Remaining);
						
						// Distribute the max px across the cell's columns.
						DistributeSize(MaxCol, ColFlags, c->Cell.x1, c->Cell.X(), Max, BorderSpacing);
					}
				}

				Cx += c->Cell.X();
			}
			else Cx++;
		}
	}

	LayoutMinX = -BorderSpacing;
	LayoutMaxX = -BorderSpacing;
	for (i=0; i<Cols.Length(); i++)
	{
		LayoutMinX += MinCol[i] + BorderSpacing;
		LayoutMaxX += MaxCol[i] + BorderSpacing;
	}

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Cols.Length(); i++)
		{
			Dbg.Print(	"\tLayoutHorizontal.AfterSpanned[%i]: min=%i max=%i (%s)\n",
						i,
						MinCol[i], MaxCol[i],
						FlagToString(ColFlags[i]));
		}
	}
	#endif

	// Now allocate unused space
	DistributeUnusedSpace(MinCol, MaxCol, ColFlags, Client.X(), BorderSpacing, DebugLayout?&Dbg:NULL);

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Cols.Length(); i++)
		{
			Dbg.Print(	"\tLayoutHorizontal.AfterDist[%i]: min=%i max=%i (%s)\n",
						i,
						MinCol[i], MaxCol[i],
						FlagToString(ColFlags[i]));
		}
	}
	#endif
	
	// Collect together our sizes
	int Spacing = BorderSpacing * (MinCol.Length() - 1);
	if (MinX)
	{
		int x = CountRange<int>(MinCol, 0, MinCol.Length()-1) + Spacing;
		*MinX = max(*MinX, x);
	}
	if (MaxX)
	{
		int x = CountRange<int>(MaxCol, 0, MinCol.Length()-1) + Spacing;
		*MaxX = max(*MaxX, x);
	}
	if (Flag)
	{	
		for (i=0; i<ColFlags.Length(); i++)
		{
			*Flag = max(*Flag, ColFlags[i]);
		}
	}
}

void GTableLayoutPrivate::LayoutVertical(GRect &Client, int *MinY, int *MaxY, CellFlag *Flag)
{
	int Cx, Cy, i;

	if (DebugLayout)
	{
		int asd=0;
	}
	
	// Do row height layout for single cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Single cell
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.Y() == 1)
				{
					int x = CountRange(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1) * BorderSpacing);
					int &Min = MinRow[Cy];
					int &Max = MaxRow[Cy];
					CellFlag &Flags = RowFlags[Cy];
					c->Layout(x, Min, Max, Flags);
				}

				Cx += c->Cell.X();
			}
			else
				Cx++;
		}
	}

	// Row height for spanned cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Spanned cell
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.Y() > 1)
				{
					int WidthPx      = CountRange(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1)   * BorderSpacing);					
					int InitMinY     = CountRange(MinRow, c->Cell.y1, c->Cell.y2) + ((c->Cell.Y() - 1)   * BorderSpacing);
					int InitMaxY     = CountRange(MaxRow, c->Cell.y1, c->Cell.y2) + ((c->Cell.Y() - 1)   * BorderSpacing);
					//int AllocY       = CountRange(MinRow, 0,     Rows.Length()-1) + ((Rows.Length() - 1) * BorderSpacing);
					int MinY         = InitMinY;
					int MaxY         = InitMaxY;
					// int RemainingY   = Client.Y() - AllocY;
					CellFlag RowFlag = SizeUnknown;

					c->Layout(WidthPx, MinY, MaxY, RowFlag);

					// This code stops the max being set on spanned cells.
					GArray<int> AddTo;
					for (int y=c->Cell.y1; y<=c->Cell.y2; y++)
					{
						if
						(
							RowFlags[y] != SizeFixed
							&&
							(
								RowFlags[y] != SizeGrow
								||
								MaxRow[y] > MinRow[y]
							)
						)
							AddTo.Add(y);
					}
					if (AddTo.Length() == 0)
					{
						for (int y=c->Cell.y1; y<=c->Cell.y2; y++)
						{
							if (!AddTo.HasItem(y))
							{
								if (RowFlags[y] != SizeFixed)
									AddTo.Add(y);
							}
						}
					}

					if (AddTo.Length())
					{
						if (MinY > InitMinY)
						{
							// Allocate any extra min px somewhere..
							int Amt = (MinY - InitMinY) / AddTo.Length();
							for (int i=0; i<AddTo.Length(); i++)
							{
								int Idx = AddTo[i];
								MinRow[Idx] += Amt;
							}
						}
						
						if (MaxY > InitMaxY)
						{
							// Allocate any extra max px somewhere..
							int Amt = (MaxY - InitMaxY) / AddTo.Length();
							for (int i=0; i<AddTo.Length(); i++)
							{
								int Idx = AddTo[i];
								MaxRow[Idx] += Amt;
							}
						}						
						
						if (RowFlag > SizeUnknown)
						{
							// Apply the size flag somewhere...
							for (int y=c->Cell.y2; y>=c->Cell.y1; y++)
							{
								if (RowFlags[y] == SizeUnknown)
								{
									RowFlags[y] = SizeFill;
									break;
								}
							}
						}
					}
					else
					{
						// Last chance... stuff extra px in last cell...
						MaxRow[c->Cell.y2] = max(MaxY, MaxRow[c->Cell.y2]);
					}
				}

				Cx += c->Cell.X();
			}
			else Cx++;
		}
	}

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Rows.Length(); i++)
		{
			Dbg.Print("\tLayoutVertical.AfterSpanned[%i]: min=%i max=%i (%s)\n", i, MinRow[i], MaxRow[i], FlagToString(RowFlags[i]));
		}
	}
	#endif

	// Allocate remaining vertical space
	DistributeUnusedSpace(MinRow, MaxRow, RowFlags, Client.Y(), BorderSpacing);
	
	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Rows.Length(); i++)
		{
			Dbg.Print("\tLayoutVertical.AfterDist[%i]: min=%i max=%i (%s)\n", i, MinRow[i], MaxRow[i], FlagToString(RowFlags[i]));
		}
		for (i=0; i<Cols.Length(); i++)
		{
			Dbg.Print("\tLayoutVertical.Final[%i]=%i\n", i, MinCol[i]);
		}
		GAutoString DbgStr(Dbg.NewStr());
		LgiTrace("%s", DbgStr.Get());
	}
	#endif

	// Collect together our sizes
	if (MinY)
	{
		int y = CountRange(MinRow, 0, MinRow.Length()-1) + ((MinRow.Length()-1) * BorderSpacing);
		*MinY = max(*MinY, y);
	}
	if (MaxY)
	{
		int y = CountRange(MaxRow, 0, MinRow.Length()-1) + ((MaxRow.Length()-1) * BorderSpacing);
		*MaxY = max(*MaxY, y);
	}
	if (Flag)
	{	
		for (i=0; i<RowFlags.Length(); i++)
		{
			*Flag = max(*Flag, RowFlags[i]);
		}
	}
}

void GTableLayoutPrivate::LayoutPost(GRect &Client)
{
	int Px = 0, Py = 0, Cx, Cy;
	GFont *Fnt = Ctrl->GetFont();

	// Move cells into their final positions
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		int MaxY = 0;
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy)
				{
					GCss::PositionType PosType = c->Position();
					int x = CountRange<int>(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1) * BorderSpacing);
					int y = CountRange<int>(MinRow, c->Cell.y1, c->Cell.y2) + ((c->Cell.Y() - 1) * BorderSpacing);

					// Set the height of the cell
					c->Pos.x2 = c->Pos.x1 + x - 1;
					c->Pos.y2 = c->Pos.y1 + y - 1;
					
					if (PosType == GCss::PosAbsolute)
					{
						// Hmm this is a bit of a hack... we'll see
						GCss::Len Left = c->Left();
						GCss::Len Top = c->Top();
						
						int LeftPx = Left.IsValid() ? Left.ToPx(Client.X(), Fnt) : Px;
						int TopPx = Top.IsValid() ? Top.ToPx(Client.Y(), Fnt) : Py;
						
						c->Pos.Offset(Client.x1 + LeftPx, Client.y1 + TopPx);
					}
					else
					{
						c->Pos.Offset(Client.x1 + Px, Client.y1 + Py);
					}
					c->PostLayout();

					MaxY = max(MaxY, c->Pos.y2);
				}

				Px = c->Pos.x2 + BorderSpacing - Client.x1 + 1;
				Cx += c->Cell.X();
			}
			else
				Cx++;
		}

		Py += MinRow[Cy] + BorderSpacing;
		Px = 0;
	}

	LayoutBounds.ZOff(Px-1, Py-1);
}

void GTableLayoutPrivate::InitBorderSpacing()
{
	BorderSpacing = GTableLayout::CellSpacing;
	if (Ctrl->GetCss())
	{
		GCss::Len bs = Ctrl->GetCss()->BorderSpacing();
		if (bs.Type != GCss::LenInherit)
			BorderSpacing = bs.ToPx(Ctrl->X(), Ctrl->GetFont());
	}
}

void GTableLayoutPrivate::Layout(GRect &Client)
{
    if (InLayout)
    {    
        LgiAssert(!"In layout, no recursion should happen.");
        return;
    }

    InLayout = true;
	InitBorderSpacing();
    
    #if DEBUG_LAYOUT
    int CtrlId = Ctrl->GetId();
    DebugLayout = CtrlId == DEBUG_LAYOUT;
    if (DebugLayout)
    {
		int asd=0;
    }
    #endif

	#if DEBUG_PROFILE
	int64 Start = LgiCurrentTime();
	#endif

	LayoutHorizontal(Client);
	LayoutVertical(Client);
	LayoutPost(Client);

	Ctrl->SendNotify(GNotifyTableLayout_LayoutChanged);
	#if DEBUG_PROFILE
	LgiTrace("GTableLayout::Layout = %i ms\n", (int)(LgiCurrentTime()-Start));
	#endif
	
	InLayout = false;
}

GTableLayout::GTableLayout(int id) : ResObject(Res_Table)
{
	LgiResources::StyleElement(this);
	d = new GTableLayoutPrivate(this);
	SetPourLargest(true);
	Name("GTableLayout");
	SetId(id);
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
	if (r.X() != d->PrevWidth)
	{
		d->PrevWidth = r.X();
		if (d->PrevWidth > 0)
		{		
			if (GetCss())
			{
				GCssTools t(GetCss(), GetFont());
				r = t.ApplyPadding(r);
			}
			
			d->Layout(r);
		}
	}
}

GRect GTableLayout::GetUsedArea()
{
	if (d->PrevWidth != GetClient().X())
	{
		OnPosChange();
	}

	GRect r(0, 0, -1, -1);
    for (int i=0; i<d->Cells.Length(); i++)
    {
        TableCell *c = d->Cells[i];
        if (i)
            r.Union(&c->Pos);
        else
            r = c->Pos;
    }
    
	return r;
}

void GTableLayout::InvalidateLayout()
{
	d->PrevWidth = -1;
	if (d->IsInLayout())
	{
		PostEvent(	M_CHANGE,
					(GMessage::Param) GetId(),
					(GMessage::Param) GNotifyTableLayout_LayoutChanged);
	}
	else
	{
		OnPosChange();
		Invalidate();
	}
}

GMessage::Result GTableLayout::OnEvent(GMessage *m)
{
	switch (m->Msg())
	{
		case M_TABLE_LAYOUT:
		{
			OnPosChange();
			Invalidate();
			return 0;
		}
	}
	
	return GLayout::OnEvent(m);
}

void GTableLayout::OnPaint(GSurface *pDC)
{
	GRect Client = GetClient();
	if (d->PrevWidth != Client.X())
	{
		#ifdef LGI_SDL
		OnPosChange();
		#else
		PostEvent(M_TABLE_LAYOUT);
		#endif
		return;
	}

	GColour Back;
	Back.Set(LC_MED, 24);
	if (GetCss())
	{		
		GCss::ColorDef fill = GetCss()->BackgroundColor();
		if (fill.Type == GCss::ColorRgb)
			Back.Set(fill.Rgb32, 32);
		else if (fill.Type == GCss::ColorTransparent)
			Back.Empty();
	}
	if (!Back.IsTransparent())
	{
		pDC->Colour(Back);
		pDC->Rectangle();
	}

	for (int i=0; i<d->Cells.Length(); i++)
	{
		TableCell *c = d->Cells[i];
		c->OnPaint(pDC);
	}

	#if 0 // DEBUG_DRAW_CELLS
	pDC->Colour(Rgb24(255, 0, 0), 24);
	pDC->Box();
	#endif

	#if DEBUG_DRAW_CELLS
	#if defined(DEBUG_LAYOUT)
	if (GetId() == DEBUG_LAYOUT)
	#endif
	{
		pDC->LineStyle(GSurface::LineDot);
		for (int i=0; i<d->Cells.Length(); i++)
		{
			TableCell *c = d->Cells[i];
			GRect r = c->Pos;
			pDC->Colour(c->Debug ? Rgb24(255, 222, 0) : Rgb24(192, 192, 222), 24);
			pDC->Box(&r);
			pDC->Line(r.x1, r.y1, r.x2, r.y2);
			pDC->Line(r.x2, r.y1, r.x1, r.y2);
		}
		pDC->LineStyle(GSurface::LineSolid);
	}
	#endif
}

bool GTableLayout::GetVariant(const char *Name, GVariant &Value, char *Array)
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

bool GTableLayout::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (stricmp(Name, "cols") == 0)
	{
		return ConvertNumbers(d->Cols, Value.Str());
	}
	else if (stricmp(Name, "rows") == 0)
	{
		return ConvertNumbers(d->Rows, Value.Str());
	}
	else if (!stricmp(Name, "style"))
	{
		const char *Defs = Value.Str();
		if (Defs)
			GetCss(true)->Parse(Defs, GCss::ParseRelaxed);
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
				if (c->Children[n].View == Wnd)
				{
					c->Children.DeleteAt(n);
					return;
				}
			}
		}

		LgiAssert(0);
	}
}

int GTableLayout::OnNotify(GViewI *c, int f)
{
    if (f == GNotifyTableLayout_Refresh)
    {
		d->PrevWidth = -1;
        OnPosChange();
        Invalidate();
        return 0;
    }

    return GLayout::OnNotify(c, f);
}

int64 GTableLayout::Value()
{
    GArray<GRadioButton*> Btns;
    if (d->CollectRadioButtons(Btns))
    {
        for (int i=0; i<Btns.Length(); i++)
        {
            if (Btns[i]->Value())
                return i;
        }
    }
    return -1;
}

void GTableLayout::Value(int64 v)
{
    GArray<GRadioButton*> Btns;
    if (d->CollectRadioButtons(Btns))
    {
        if (v >= 0 && v < Btns.Length())
            Btns[(int)v]->Value(true);
    }
}

void GTableLayout::Empty(GRect *Range)
{
	d->Empty(Range);
}

GLayoutCell *GTableLayout::GetCell(int cx, int cy, bool create, int colspan, int rowspan)
{
	TableCell *c = d->GetCellAt(cx, cy);
	if (!c && create)
    {
        c = new TableCell(this, cx, cy);
        if (c)
        {
            if (colspan > 1)
                c->Cell.x2 += colspan - 1;
            if (rowspan > 1)
                c->Cell.y2 += rowspan - 1;
            d->Cells.Add(c);
            
            while (d->Cols.Length() <= c->Cell.x2)
                d->Cols.Add(1);
            while (d->Rows.Length() <= c->Cell.y2)
                d->Rows.Add(1);
        }
    }
    
    return c;
}
