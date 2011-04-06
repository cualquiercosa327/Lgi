/// \file
/// \author Matthew Allen <fret@memecode.com>
/// \brief A control layout manager for font sensitive GUI's.
#ifndef _GTABLE_LAYOUT_H_
#define _GTABLE_LAYOUT_H_

#define GTABLELAYOUT_LAYOUT_CHANGED			20

/// A layout cell, not currently implemened.
class GLayoutCell : public GDom
{
public:
	GLayoutCell() {}
	virtual ~GLayoutCell() {}
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
	const char *GetClass() { return "GTableLayout"; }
	GTableLayout();
	~GTableLayout();

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

	// Impl
	void OnFocus(bool b);
	void OnCreate();
	void OnPosChange();
	void OnPaint(GSurface *pDC);
	bool GetVariant(char *Name, GVariant &Value, char *Array = 0);
	bool SetVariant(char *Name, GVariant &Value, char *Array = 0);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
};

#endif