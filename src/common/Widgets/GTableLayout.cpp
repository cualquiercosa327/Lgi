#include "Lgi.h"
#include "GTableLayout.h"
#include "GCssTools.h"
#include "GDisplayString.h"

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
#define DEBUG_LAYOUT		0
#define DEBUG_PROFILE		0
#define DEBUG_DRAW_CELLS	0

int GTableLayout::CellSpacing = 4;

const char *FlagToString(CellFlag f)
{
	switch (f)
	{
		case SizeUnknown: return "Unknown";
		case SizeFixed: return "Fixed";
		case SizeGrow: return "Grow";
		case SizeFill: return "Fill";
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

void DistributeUnusedSpace(	GArray<int> &Min,
							GArray<int> &Max,
							GArray<CellFlag> &Flags,
							int Total,
							int CellSpacing)
{
	// Now allocate unused space
	int Borders = Min.Length() - 1;
	int Sum = CountRange<int>(Min, 0, Borders) + (Borders * CellSpacing);
	if (Sum < Total)
	{
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
			}
		}
		Unders.Sort(Cmp);

        int UnknownSplit = 0;
		for (i=0; Avail>0 && i<Unders.Length(); i++)
		{
		    UnderInfo &u = Unders[i];
			if (u.Grow)
			{
			    Min[u.Col] += u.Grow;
			    Avail -= u.Grow;
			}
			else
			{
			    if (!UnknownSplit)
			        UnknownSplit = Avail / UnknownGrow;
			    if (!UnknownSplit)
			        UnknownSplit = Avail;
			        
			    Min[u.Col] += UnknownSplit;
			    Avail -= UnknownSplit;
			}
		}
	}
}

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
		GArray<int> Grow, Fill;
		int ExistingGrowSize = 0;
		int ExistingFillSize = 0;

		for (int i=Start; i<Start+Span; i++)
		{
		    switch (Flags[i])
		    {
				default:
					break;
		        case SizeGrow:
			    {
				    Grow.Add(i);
				    ExistingGrowSize += a[i];
				    break;
			    }
			    case SizeFill:
			    {
				    Fill.Add(i);
				    ExistingFillSize += a[i];
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
				if (a[Cell] && ExistingGrowSize)
				    Add = a[Cell] * AdditionalSize / ExistingGrowSize;
				else
				    Add = max(1, AdditionalSize / Grow.Length());
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
}

static uint32 NextChar(char *s)
{
	int Len = 0;
	while (s[Len] && Len < 6) Len++;
	return LgiUtf8To32((uint8*&)s, Len);
}

void PreLayoutTextCtrl(GView *v, int &Min, int &Max)
{
	int MyMin = 0;
	int MyMax = 0;

	char *t = v->Name();
	GFont *f = v->GetFont();
	if (t && f)
	{
		GDisplayString Sp(f, " ");
		int X = 0;

		char White[] = " \t\r\n";
		for (char *s = t; *s; )
		{
			while (*s && strchr(White, *s))
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
				if (e > s && LGI_BreakableChar(c))
					break;
				e = LgiSeekUtf8(e, 1);
			}

			GDisplayString d(f, s, e - s);
			MyMin = max(d.X(), MyMin);
			X += d.X() + (*e == ' ' ? Sp.X() : 0);
			MyMax = max(X, MyMax);

			s = *e && strchr(White, *e) ? e + 1 : e;
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
	if (t && f)
	{
		int y = f->GetHeight();

		char *end = t + strlen(t);
		for (char *s = t; s && *s; )
		{
			// Find the end of the line...
			int MaxLine = 0;
			while (s[MaxLine] && s[MaxLine] != '\n' && MaxLine < 1000)
				MaxLine++;
			if (!MaxLine)
			{
				if (s[MaxLine] == '\n')
				{
					s++;
					Ht += y;
					continue;
				}
				break;
			}

			// Create a string to measure...
			LgiAssert(s + MaxLine <= end);
			GDisplayString d(f, s, MaxLine);
			if (!(const OsChar*)d)
			{
				LgiAssert(0);
				break;
			}

			// Find break point
			int Ch = d.CharAt(Width);
			if (Ch < 0)
			{
				LgiAssert(0);
				break;
			}

			char *e = LgiSeekUtf8(s, Ch);
			while (	*e &&
					*e != '\n' &&
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
	GTableLayout *Table;
	GRect Cell;		// Cell position
	GRect Pos;		// Pixel position
	GRect Padding;	// Cell padding from CSS styles
	GArray<GView*> Children;
	GCss::DisplayType Disp;

	TableCell(GTableLayout *t, int Cx, int Cy);
	bool Add(GView *v);	
	bool Remove(GView *v);

	bool IsSpanned();
	bool GetVariant(const char *Name, GVariant &Value, char *Array);
	bool SetVariant(const char *Name, GVariant &Value, char *Array);
	/// Calculates the minimum and maximum widths this cell can occupy.
	void PreLayout(int &MinX, int &MaxX, CellFlag &Flag);
	/// Calculate the height of the cell based on the given width
	void Layout(int Width, int &MinY, int &MaxY, CellFlag &Flags);
	/// Called after the layout has been done to move the controls into place
	void PostLayout();
};

class GTableLayoutPrivate
{
	bool InLayout;

public:
	GArray<double> Rows, Cols;
	GArray<TableCell*> Cells;
	int CellSpacing;
	bool FirstLayout;
	GRect LayoutBounds;
	int LayoutMinX, LayoutMaxX;
	GTableLayout *Ctrl;

	// Object
	GTableLayoutPrivate(GTableLayout *ctrl);
	~GTableLayoutPrivate();
	
	// Utils
	TableCell *GetCellAt(int cx, int cy);
	void Empty(GRect *Range = NULL);
    bool CollectRadioButtons(GArray<GRadioButton*> &Btns);

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
	Cell.ZOff(0, 0);
	Cell.Offset(Cx, Cy);
	Padding.ZOff(0, 0);
	Disp = GCss::DispBlock;
}

bool TableCell::Add(GView *v)
{
	if (Children.HasItem(v))
		return false;
	Table->AddView(v);
	Children.Add(v);
	return true;
}

bool TableCell::Remove(GView *v)
{
	if (!Children.HasItem(v))
		return false;
	Table->DelView(v);
	Children.Delete(v, true);
	return true;
}

bool TableCell::IsSpanned()
{
	return Cell.X() > 1 || Cell.Y() > 1;
}

bool TableCell::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	return false;
}

bool TableCell::SetVariant(const char *Name, GVariant &Value, char *Array)
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
		TextAlign
		(
			Len
			(
				ConvertAlign(Value.Str(), true)
			)
		);
	}
	else if (stricmp(Name, "valign") == 0)
	{
		VerticalAlign
		(
			Len
			(
				ConvertAlign(Value.Str(), false)
			)
		);
	}
	else if (stricmp(Name, "class") == 0)
	{
		LgiResources *r = LgiGetResObj();
		if (r)
		{
			GCss::SelArray *a = r->CssStore.ClassMap.Find(Value.Str());
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
	}
	else return false;

	return true;
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
	if (Wid.Type != LenInherit)
	{
		Min = Max = Wid.ToPx(Table->X(), Table->GetFont());
		Flag = SizeFixed;
		if (Padding.x1 + Padding.x2 > Min)
		{
			// Remove padding as it's going to oversize the cell
			Padding.x1 = Padding.x2 = 0;
		}
	}
	else
	{
		for (int i=0; i<Children.Length(); i++)
		{
			GView *v = Children[i];
			if (!v)
				continue;
			
			const char *Cls = v->GetClass();
			GViewLayoutInfo Inf;
			GCss *Css = v->GetCss();
			GCss::Len Wid;
			if (Css)
				Wid = Css->Width();
				
			if (Wid.IsValid())
			{
				Min = Max = Wid.ToPx(Max, v->GetFont());
			}
			else if (v->OnLayout(Inf))
			{
				if (Inf.Width.Max < 0)
				{
					if (Flag < SizeFill)
						Flag = SizeFill;
				}
				else
				{
					Max = max(Max, Inf.Width.Max);
				}

				if (Inf.Width.Min)
				{
					Min = max(Min, Inf.Width.Min);
				}
			}
			else if (Izza(GText))
			{
				PreLayoutTextCtrl(v, Min, Max);
				if (Max > Min)
				{
					if (Flag < SizeGrow)
						Flag = SizeGrow;
				}
				else
				{
					if (Flag < SizeFixed)
						Flag = SizeFixed;
				}
			}
			else if (Izza(GCheckBox) ||
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
				GCombo *Cbo = Izza(GCombo);
				GFont *f = Cbo->GetFont();
				int min_x = -1, max_x = 0;
				char *t;
				for (int i=0; (t = (*Cbo)[i]); i++)
				{
					GDisplayString ds(f, t);
					min_x = min_x < 0 ? ds.X() : min(min_x, ds.X());
					max_x = max(ds.X() + 4, max_x);
				}				
				
				Min = max(Min, min_x + 32);
				Max = max(Max, max_x + 32);
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
					Tbl->d->LayoutHorizontal(Table->GetClient(), &Min, &Max, &Flag);
					// LgiTrace("    TblHorz = %i, %i, %s\n", Min, Max, FlagToString(Flag));
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
		Pos.y2 = MinY = MaxY = Ht.ToPx(Table->Y(), Table->GetFont()) - 1;
		if (Flags < SizeFixed)
			Flags = SizeFixed;
		return;
	}
	
	int BtnX = 0;
	int BtnRows = -1;
			
	Width -= Padding.x1 + Padding.x2;
	LgiAssert(Width >= 0);
	
	for (int i=0; i<Children.Length(); i++)
	{
		GView *v = Children[i];
		if (!v)
			continue;

		const char *Cls = v->GetClass();
		GTableLayout *Tbl;
		GRadioGroup *Grp;
		GCss *Css = v->GetCss();
		GCss::Len Ht;
		if (Css)
			Ht = Css->Height();

		if (i)
			Pos.y2 += GTableLayout::CellSpacing;

		GViewLayoutInfo Inf;
		Inf.Width.Min = Inf.Width.Max = Width;

		if (Ht.IsValid())
		{
			MinY = MaxY = Ht.ToPx(MaxY, v->GetFont());
		}
		else if (v->OnLayout(Inf))
		{
			// Supports layout info
			if (Inf.Height.Max < 0)
				Flags = SizeFill;
			else
				Pos.y2 += Inf.Height.Max - 1;
		}
		else if (Izza(GText))
		{
			GText *Txt = dynamic_cast<GText*>(v);
			if (Txt && Txt->GetWrap() == false)
				Txt->SetWrap(true);

			int y = LayoutTextCtrl(v, 0, Pos.X());
			Pos.y2 += y;
			
			GRect r = v->GetPos();
			r.y2 = r.y1 + y - 1;
			v->SetPos(r);
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
				Pos.y2 += y + GTableLayout::CellSpacing;
			}
			else
			{
				// Don't wrap
				BtnX += v->X() + GTableLayout::CellSpacing;
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
		else if (Izza(GCheckBox) ||
				 Izza(GRadioButton))
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
			MaxY = max(MaxY, 1000);
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
			Tbl->d->LayoutVertical(r, &MinY, &MaxY, &Flags);
			Pos.y2 += MinY;
			
			// LgiTrace("    TblVert = %i, %i, %s\n", MinY, MaxY, FlagToString(Flags));
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

	for (int i=0; i<Children.Length(); i++)
	{
		GView *v = Children[i];
		if (!v)
			continue;

		if (Disp == DispNone)
		{
			v->Visible(false);
			continue;
		}
			
		GTableLayout *Tbl = Izza(GTableLayout);
		GRect r = v->GetPos();
		r.Offset(Pos.x1 - r.x1 + Cx, Pos.y1 - r.y1 + Cy);

		GViewLayoutInfo Inf;
		Inf.Width.Max = Pos.X() - Padding.x1 - Padding.x2;
		Inf.Height.Max = Pos.Y() - Padding.y1 - Padding.y2;

		if (Tbl)
		{
			r.Dimension(Pos.X(), Pos.Y());
			// LgiTrace("    TblPost %s\n", r.GetStr());
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
		else if (v->OnLayout(Inf))
		{
			if (Inf.Height.Max < 0)
				r.y2 = Pos.y2;
			else
				r.y2 = r.y1 + Inf.Height.Max - 1;
		}

		if (!Izza(GButton) && !Tbl)
		{
			if (Inf.Width.Max < 0)
				Inf.Width.Max = Pos.X() - Padding.x1 - Padding.x2;
				
			r.x2 = r.x1 + Inf.Width.Max - 1;
		}

		New[i] = r;
		MaxY = max(MaxY, r.y2 - Pos.y1);
		Cx += r.X() + GTableLayout::CellSpacing;
		if (Cx >= Pos.X())
		{
			int Wid = Cx - GTableLayout::CellSpacing;
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
			Cy = MaxY + GTableLayout::CellSpacing;
		}
	}

	if (Disp == DispNone)
	{
		return;
	}

	int n;
	int Wid = Cx - GTableLayout::CellSpacing;
	int OffsetX = 0;
	if (TextAlign().Type == AlignCenter)
	{
		OffsetX = (Pos.X() - Wid) / 2;
	}
	else if (TextAlign().Type == AlignRight)
	{
		OffsetX = Pos.X() - Wid;
	}
	for (n=RowStart; n<Children.Length(); n++)
	{
		New[n].Offset(OffsetX, 0);
	}

	int OffsetY = 0;
	if (VerticalAlign().Type == VerticalMiddle)
	{
		OffsetY = (Pos.Y() - MaxY) / 2;
	}
	else if (VerticalAlign().Type == VerticalBottom)
	{
		OffsetY = Pos.Y() - MaxY;
	}
	for (n=0; n<Children.Length(); n++)
	{
		GView *v = Children[n];
		if (!v)
			break;

		New[n].Offset(0, OffsetY);
		v->SetPos(New[n]);		
		v->Visible(true);
	}
}

////////////////////////////////////////////////////////////////////////////
GTableLayoutPrivate::GTableLayoutPrivate(GTableLayout *ctrl)
{
	InLayout = false;
	Ctrl = ctrl;
	FirstLayout = true;
	CellSpacing = GTableLayout::CellSpacing;
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
				c->Children.DeleteObjects();
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

	FirstLayout = true;
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
	ColFlags.Length(Cols.Length());
	RowFlags.Length(Rows.Length());
		
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
					c->Cell.X() == 1 &&
					c->Cell.Y() == 1)
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
	Dbg.Print("Layout Id=%i, Size=%i,%i\n", Ctrl->GetId(), Client.X(), Client.Y());

	for (i=0; i<Cols.Length(); i++)
	{
		Dbg.Print("\tColBeforeSpan[%i]: min=%i max=%i (%s)\n", i, MinCol[i], MaxCol[i], FlagToString(ColFlags[i]));
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
					(c->Cell.X() > 1 || c->Cell.Y() > 1))
				{
					int Min = 0, Max = 0;
					CellFlag Flag = SizeUnknown;

					if (c->Width().IsValid())
					{
						GCss::Len l = c->Width();
						Min = l.ToPx(Client.X(), Ctrl->GetFont());
					}
					else
					{
						c->PreLayout(Min, Max, Flag);
					}

					if (Max > Client.X())
						Max = Client.X();
					if (Flag)
					{
						for (i=c->Cell.x1; i<=c->Cell.x2; i++)
						{
							if (ColFlags[i] == Flag)
								break;
							if (!ColFlags[i])
								ColFlags[i] = Flag;
						}
					}

					DistributeSize(MinCol, ColFlags, c->Cell.x1, c->Cell.X(), Min, CellSpacing);
					DistributeSize(MaxCol, ColFlags, c->Cell.x1, c->Cell.X(), Flag >= SizeGrow ? Client.X() : Max, CellSpacing);
				}

				Cx += c->Cell.X();
			}
			else Cx++;
		}
	}

	LayoutMinX = 0;
	for (i=0; i<Cols.Length(); i++)
	{
		LayoutMinX += MinCol[i] + GTableLayout::CellSpacing;
		LayoutMaxX += MaxCol[i] + GTableLayout::CellSpacing;
	}

	#if DEBUG_LAYOUT
	for (i=0; i<Cols.Length(); i++)
	{
		Dbg.Print("\tColBefore[%i]: min=%i max=%i (%s)\n", i, MinCol[i], MaxCol[i], FlagToString(ColFlags[i]));
	}
	#endif

	// Now allocate unused space
	DistributeUnusedSpace(MinCol, MaxCol, ColFlags, Client.X(), CellSpacing);

	#if DEBUG_LAYOUT
	for (i=0; i<Cols.Length(); i++)
	{
		Dbg.Print("\tColAfter[%i]: min=%i max=%i (%s)\n", i, MinCol[i], MaxCol[i], FlagToString(ColFlags[i]));
	}
	#endif
	
	// Collect together our sizes
	if (MinX)
	{
		int x = CountRange<int>(MinCol, 0, MinCol.Length()-1);
		*MinX = max(*MinX, x);
	}
	if (MaxX)
	{
		int x = CountRange<int>(MaxCol, 0, MinCol.Length()-1);
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

	// printf("\t\t\trows=%i row[0]=%i\n", MinRow.Length(), MinRow[0]);

	// Do row height layout for single cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Single cell
				if (c->Cell.x1 == Cx && c->Cell.y1 == Cy && !c->IsSpanned())
				{
					int x = CountRange<int>(MinCol, c->Cell.x1, c->Cell.x2) +
							((c->Cell.X() - 1) * CellSpacing);
					c->Layout(x, MinRow[Cy], MaxRow[Cy], RowFlags[Cy]);
					
					// printf("\t\t\trow layout %i,%i = %i,%i\n", c->Cell.x1, c->Cell.y1, MinRow[Cy], MaxRow[Cy]);
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
				if (c->Cell.x1 == Cx && c->Cell.y1 == Cy && c->IsSpanned())
				{
					int x = CountRange<int>(MinCol, c->Cell.x1, c->Cell.x2) +
							((c->Cell.X() - 1) * CellSpacing);
					int MaxY = MaxRow[Cy];

					c->Layout(x, MinRow[Cy], MaxY, RowFlags[Cy]);

					// This code stops the max being set on spanned cells.
					GArray<int> Growable;
					for (int i=0; i<c->Cell.Y(); i++)
					{
						if (MaxRow[Cy+i] > MinRow[Cy+i])
							Growable.Add(Cy+i);
					}
					if (Growable.Length())
					{
						int Amt = MaxY / Growable.Length();
						for (int i=0; i<Growable.Length(); i++)
						{
							int Idx = Growable[i];
							MaxRow[Idx] = max(Amt, MaxRow[Idx]);
						}
					}
					else
					{
						MaxRow[c->Cell.y2] = max(MaxY, MaxRow[c->Cell.y2]);
					}
				}

				Cx += c->Cell.X();
			}
			else
				Cx++;
		}
	}

	#if DEBUG_LAYOUT
	for (i=0; i<Rows.Length(); i++)
	{
		Dbg.Print("\tRowBefore[%i]: min=%i max=%i (%s)\n", i, MinRow[i], MaxRow[i], FlagToString(RowFlags[i]));
	}
	#endif

	// Allocate remaining vertical space
	DistributeUnusedSpace(MinRow, MaxRow, RowFlags, Client.Y(), CellSpacing);
	
	#if DEBUG_LAYOUT
	for (i=0; i<Rows.Length(); i++)
	{
		Dbg.Print("\tRowAfter[%i]: min=%i max=%i (%s)\n", i, MinRow[i], MaxRow[i], FlagToString(RowFlags[i]));
	}
	for (i=0; i<Cols.Length(); i++)
	{
		Dbg.Print("\tFinalCol[%i]=%i\n", i, MinCol[i]);
	}
	GAutoString DbgStr(Dbg.NewStr());
	LgiTrace("%s", DbgStr.Get());
	#endif

	// Collect together our sizes
	if (MinY)
	{
		int y = CountRange<int>(MinRow, 0, MinRow.Length()-1);
		*MinY = max(*MinY, y);
	}
	if (MaxY)
	{
		int y = CountRange<int>(MaxRow, 0, MinRow.Length()-1);
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
	int Px = 0, Py = 0, Cx, Cy, i;

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
			else
				Cx++;
		}

		Py += MinRow[Cy] + CellSpacing;
		Px = 0;
	}

	LayoutBounds.ZOff(Px-1, Py-1);
}

void GTableLayoutPrivate::Layout(GRect &Client)
{
    if (InLayout)
    {    
        LgiAssert(!"In layout, no recursion should happen.");
        return;
    }    
        
    InLayout = true;

	#if DEBUG_PROFILE
	int64 Start = LgiCurrentTime();
	#endif

	LayoutHorizontal(Client);
	LayoutVertical(Client);
	LayoutPost(Client);

	Ctrl->SendNotify(GTABLELAYOUT_LAYOUT_CHANGED);
	#if DEBUG_PROFILE
	LgiTrace("GTableLayout::Layout = %i ms\n", (int)(LgiCurrentTime()-Start));
	#endif
	
	InLayout = false;
}

GTableLayout::GTableLayout(int id) : ResObject(Res_Table)
{
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
	if (GetCss())
	{
		GCssTools t(GetCss(), GetFont());
		r = t.ApplyPadding(r);
	}
	d->Layout(r);
}

GRect GTableLayout::GetUsedArea()
{
	if (d->FirstLayout)
	{
		d->FirstLayout = false;
		OnPosChange();
	}

	GRect r;
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
	d->FirstLayout = true;
	Invalidate();
}

void GTableLayout::OnPaint(GSurface *pDC)
{
	if (d->FirstLayout)
	{
		d->FirstLayout = false;
		OnPosChange();
	}

	GCss::ColorDef fill;
	if (GetCss() &&
		(fill = GetCss()->BackgroundColor()).Type == GCss::ColorRgb)
		pDC->Colour(fill.Rgb32, 32);
	else
		pDC->Colour(LC_MED, 24);
	pDC->Rectangle();

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

int GTableLayout::OnNotify(GViewI *c, int f)
{
    if (f == GTABLELAYOUT_REFRESH)
    {
        OnPosChange();
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
            Btns[v]->Value(true);
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
