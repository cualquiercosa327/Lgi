/// \file
/// \author Matthew Allen <fret@memecode.com>
/// \brief A control layout manager for font sensitive GUI's.
#ifndef _GTABLE_LAYOUT_H_
#define _GTABLE_LAYOUT_H_

#include "GCss.h"
#include "GNotifications.h"

/// A layout cell, not currently implemened.
class GLayoutCell : public GDom, public GCss
{
public:
	GLayoutCell() {}
	virtual ~GLayoutCell() {}
	
	virtual class GTableLayout *GetTable() = 0;
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
	/// The system default cell spacing for all tables. Individual tables can have their own
	/// cell spacing by setting the CSS property 'border-spacing'.
    static int CellSpacing;

	GTableLayout(int id = -1);
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
	GMessage::Result OnEvent(GMessage *m);
};


/// This is just a light-weight layout system for doing things manually.
class GLayoutRect : public GRect
{
	int Spacing;

public:
	GLayoutRect(GViewI *c, int spacing = GTableLayout::CellSpacing)
	{
		Spacing = spacing;
		((GRect&)*this) = c->GetClient();
		Size(Spacing, Spacing);
	}

	// Allocate object on left edge
	// ------------------------------------
		GLayoutRect Left(GRect &rc, int Px)
		{
			rc = *this;
			rc.x2 = rc.x1 + Px - 1;
			x1 = rc.x2 + Spacing;
			return *this;
		}

		GLayoutRect Left(GViewI *v, int Px)
		{
			GRect r;
			Left(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}

	// Allocate on right edge...
	// ------------------------------------
		GLayoutRect Right(GRect &rc, int Px)
		{
			rc = *this;
			rc.x1 = rc.x2 - Px + 1;
			x2 = rc.x1 - Spacing;
			return *this;
		}

		GLayoutRect Right(GViewI *v, int Px)
		{
			GRect r;
			Right(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}

	// Allocate on top edge...
	// ------------------------------------
		GLayoutRect Top(GRect &rc, int Px)
		{
			rc = *this;
			rc.y2 = rc.y1 + Px - 1;
			y1 = rc.y2 + Spacing;
			return *this;
		}

		GLayoutRect Top(GViewI *v, int Px)
		{
			GRect r;
			Top(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}

	// Allocate on top edge...
	// ------------------------------------
		GLayoutRect Bottom(GRect &rc, int Px)
		{
			rc = *this;
			rc.y2 = rc.y1 + Px - 1;
			y1 = rc.y2 + Spacing;
			return *this;
		}

		GLayoutRect Bottom(GViewI *v, int Px)
		{
			GRect r;
			Bottom(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}
	
	// Allocate all remaining space.
	// ------------------------------------
	void Remaining(GRect &rc)
	{
		rc = *this;
		ZOff(-1, -1);
	}

	void Remaining(GViewI *v)
	{
		if (v)
			v->SetPos(*this);
		ZOff(-1, -1);
	}

};

#endif