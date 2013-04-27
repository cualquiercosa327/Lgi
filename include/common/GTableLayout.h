/// \file
/// \author Matthew Allen <fret@memecode.com>
/// \brief A control layout manager for font sensitive GUI's.
#ifndef _GTABLE_LAYOUT_H_
#define _GTABLE_LAYOUT_H_

#include "GCss.h"

#define GTABLELAYOUT_LAYOUT_CHANGED			20
#define GTABLELAYOUT_REFRESH                21

/// A layout cell, not currently implemened.
class GLayoutCell : public GDom, public GCss
{
public:
	GLayoutCell() {}
	virtual ~GLayoutCell() {}
	
	virtual bool Add(GView *v) = 0;
	virtual bool Remove(GView *v) = 0;
};

/// A table layout control. This uses techniques similar to HTML table layout to set the position
/// and size of the child controls. Child controls exist in a particular cell of the table. Cells
/// can span multiple rows and columns. The only way to create/edit these at the moment is via the lr8
/// resource file and LgiRes.
class LgiClass GTableLayout :
	public GLayout,
	public ResObject,
	public GDom
{
	friend class TableCell;
	class GTableLayoutPrivate *d;

public:
    static int CellSpacing;

	GTableLayout();
	~GTableLayout();

	const char *GetClass() { return "GTableLayout"; }

	/// Return the number of cells across (columns).
	int CellX();
	/// Returns the number of cell high (rows).
	int CellY();
	/// Returns the cell at a given location.
	GLayoutCell *CellAt(int x, int y);
	/// Returns area being used by cells
	GRect GetUsedArea();
	/// Invalidates the layout, causing the control to relay all the children
	void InvalidateLayout();

    /// Create a cell
    GLayoutCell *GetCell(int x, int y, bool create = true, int colspan = 1, int rowspan = 1);
    /// Clear all cells;
    void Empty(GRect *Range = NULL);

	// Impl
	void OnFocus(bool b);
	void OnCreate();
	void OnPosChange();
	void OnPaint(GSurface *pDC);
	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0);
	bool SetVariant(const char *Name, GVariant &Value, char *Array = 0);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	int64 Value();
	void Value(int64 v);
	int OnNotify(GViewI *c, int f);
};


/// This is just a light-weight layout system for doing things manually.
class GLayoutRect : public GRect
{
public:
	GLayoutRect(GViewI *c)
	{
		((GRect&)*this) = c->GetClient();
		Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
	}
	
	GLayoutRect Left(GViewI *v, int Px)
	{
		GLayoutRect r = *this;
		r.x2 = r.x1 + Px - 1;
		x1 = r.x2 + GTableLayout::CellSpacing;
		if (v)
			v->SetPos(r);
		return r;
	}

	GLayoutRect Right(GViewI *v, int Px)
	{
		GLayoutRect r = *this;
		r.x1 = r.x2 - Px + 1;
		x2 = r.x1 - GTableLayout::CellSpacing;
		if (v)
			v->SetPos(r);
		return r;
	}

	GLayoutRect Top(GViewI *v, int Px)
	{
		GLayoutRect r = *this;
		r.y2 = r.y1 + Px - 1;
		y1 = r.y2 + GTableLayout::CellSpacing;
		if (v)
			v->SetPos(r);
		return r;
	}

	GLayoutRect Bottom(GViewI *v, int Px)
	{
		GLayoutRect r = *this;
		r.y2 = r.y1 + Px - 1;
		y1 = r.y2 + GTableLayout::CellSpacing;
		if (v)
			v->SetPos(r);
		return r;
	}
	
	void Remaining(GViewI *v)
	{
		if (v)
			v->SetPos(*this);
		ZOff(-1, -1);
	}
};

#endif