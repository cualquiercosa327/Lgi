/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A list control

#ifndef __GLIST2_H
#define __GLIST2_H

// Includes
#include "GMem.h"
#include "GArray.h"
#include "GPopup.h"
#include "GDisplayString.h"
#include "GCss.h"
#include "GItemContainer.h"

// Messages
#define WM_END_EDIT_LABEL			(WM_USER+0x556)

//////////////////////////////////////////////////////////////////////////////

/// View modes for the list control
enum GListMode
{
	GListDetails,
	GListColumns,
	GListSpacial,
};

class LgiClass GListItemPainter
{
public:
	// Overridable
	virtual void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c) = 0;
};

class LgiClass GListItemColumn : public GBase, public GItem, public GListItemPainter
{
	GListItem *_Item;
	int _Column;
	int _Value;

	void OnPaint(ItemPaintCtx &Ctx) {}

protected:
	List<GListItem> *GetAllItems();
	GListItemColumn *GetItemCol(GListItem *i, int Col);

public:
	GListItemColumn(GListItem *item, int col);

	// Other objects
	GListItem *GetItem() { return _Item; }
	GList *GetList();
	GItemContainer *GetContainer();

	// Props
	int GetColumn() { return _Column; }
	void SetColumn(int i) { _Column = i; }
	virtual int64 Value() { return _Value; }
	virtual void Value(int64 i);
};

/// GItem for populating a GList
class LgiClass GListItem : public GItem, public GListItemPainter
{
	friend class GList;
	friend class GListItemColumn;
	friend class GItemColumn;

	void OnEditLabelEnd();

protected:
	// Data
	class GListItemPrivate *d;
	GRect Pos;
	GList *Parent;

	// Methods
	bool GridLines();
	GDisplayString *GetDs(int Col, int FitTo = -1);
	void ClearDs(int Col);

public:
	// Application defined, defaults to 0
	union {
		void *_UserPtr;
		NativeInt _UserInt;
	};

	// Object
	GListItem();
	virtual ~GListItem();

	GItemContainer *GetContainer();
	/// Get the owning list
	GList *GetList() { return Parent; }
	/// Gets the GListItemColumn's.
	List<GListItemColumn> *GetItemCols();

	// Properties
	
	/// Set the text for a given column
	bool SetText(const char *s, int i=0);

	/// \brief Get the text for a given column.
	///
	/// Override this in your GListItem based class to
	/// return the text for a column. Otherwise call SetText to store the text in the
	/// control.
	/// 
	/// \returns A string
	char *GetText
	(
		/// The index of the column.
		int i
	);

	/// Get the icon index to display in the '0th' column. The image list is stored in
	/// the parent GList.
	int GetImage(int Flags = 0);
	/// Sets the icon index
	void SetImage(int i);
	/// Returns true if selected
	bool Select();
	/// Changes the selection start of the control.
	void Select(bool b);
	/// Gets the on screen position of the field at column 'col'
	GRect *GetPos(int Col = -1);
	/// True if the item is visible
	bool OnScreen() { return Pos.y1 < Pos.y2; }

	// Methods
	
	/// Update the text cache and display the updated data
	void Update();
	/// Moves the item on screen if not visible
	void ScrollTo();
	/// Sets the default selection of text when editing a label
	void SetEditLabelSelection(int SelStart, int SelEnd); // call before 'EditLabel'
	/// Opens an editbox over a column to let the user edit the value.
	GView *EditLabel(int Col = -1);
	
	// Events;
	void OnMouseClick(GMouse &m);
	void OnMeasure(GdcPt2 *Info);
	void OnPaint(GSurface *pDC) { LgiAssert(0); }
	void OnPaint(GItem::ItemPaintCtx &Ctx);
	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c);

	// Overridable
	virtual int Compare(GListItem *To, int Field) { return 0; }
	virtual void OnColumnNotify(int Col, int Data) { Update(); }
};

typedef int (*GListCompareFunc)(GListItem *a, GListItem *b, NativeInt Data);

class GListItems
{
protected:
	List<GListItem> Items;

public:
	template<class T>
	bool GetSelection(List<T> &n)
	{
		n.Empty();
		List<GListItem>::I It = Items.Start();
		for (GListItem *i=*It; i; i=*++It)
		{
			if (i->Select())
			{
				T *ptr = dynamic_cast<T*>(i);
				if (ptr)
					n.Insert(ptr);
			}
		}
		return n.Length() > 0;
	}

	template<class T>
	bool GetAll(List<T> &n)
	{
		n.Empty();
		List<GListItem>::I It = Items.Start();
		for (GListItem *i=*It; i; i=*++It)
		{
			T *ptr = dynamic_cast<T*>(i);
			if (ptr)
				n.Insert(ptr);
		}
		return n.Length() > 0;
	}

	List<GListItem>::I Start()
	{
		return Items.Start();
	}

	List<GListItem>::I End()
	{
		return Items.End();
	}
};

/// List widget
class LgiClass GList :
	public GItemContainer,
	public ResObject,
	public GListItems
{
	friend class GListItem;
	friend class GItemColumn;
	friend class GListItemColumn;

	#ifdef WIN32
	HCURSOR Cursor;
	#endif
	
protected:
	class GListPrivate *d;

	// Contents
	int Keyboard; // index of the item with keyboard focus

	// Flags
	bool EditLabels;
	bool GridLines;
	bool MultiItemSelect;

	// Double buffered
	GSurface *Buf;

	// Drawing locations
	GRect ItemsPos;
	GRect ScrollX, ScrollY;
	int FirstVisible;
	int LastVisible;
	int CompletelyVisible;

	// Misc
	bool GetUpdateRegion(GListItem *i, GRegion &r);
	GListItem *HitItem(int x, int y, int *Index = 0);
	GRect &GetClientRect();
	void Pour();
	void UpdateScrollBars();
	void KeyScroll(int iTo, int iFrom, bool SelectItems);
	void ClearDs(int Col);

public:
	/// Constructor
	GList
	(
		/// The control's ID
		int id,
		/// Left edge position
		int x,
		/// Top edge position
		int y,
		/// The width
		int cx,
		/// The height
		int cy,
		/// An unseen descriptor of the control
		const char *name = "List"
	);
	~GList();

	const char *GetClass() { return "GList"; }

	// Overridables
	
	/// Called when an item is clicked
	virtual void OnItemClick
	(
		/// The item clicked
		GListItem *Item,
		/// The mouse parameters for the click
		GMouse &m
	);
	/// Called when the user selects an item and starts to drag it
	virtual void OnItemBeginDrag
	(
		/// The item being dragged
		GListItem *Item,
		/// The mouse parameters at the time
		GMouse &m
	);
	/// Called when the user selects an item. If multiple items are selected
	/// in one hit this is only called for the first item. Use GetSelection to
	/// get the extent of the selected items.
	virtual void OnItemSelect
	(
		/// The item selected
		GArray<GListItem*> &Items
	);
	/// Called when a column is dragged somewhere
	virtual void OnColumnDrag
	(
		/// The column index
		int Col,
		/// The mouse parameters at the time
		GMouse &m
	) {}
	/// Called when the column is dropped to a new location
	/// /return true to have the columns reindexed for you
	virtual bool OnColumnReindex
	(
		/// The column dropped
		GItemColumn *Col,
		/// The old index
		int OldIndex,
		/// The new index
		int NewIndex
	)
	{
		return false;
	}
	
	// Events
	
	void OnPaint(GSurface *pDC);
	GMessage::Result OnEvent(GMessage *Msg);
	// int OnHitTest(int x, int y);
	LgiCursor GetCursor(int x, int y);
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPosChange();
	bool OnKey(GKey &k);
	bool OnMouseWheel(double Lines);
	void OnFocus(bool b);
	void OnPulse();

	// Properties
	
	/// Returns whether the user can edit labels
	bool AllowEditLabels() { return EditLabels; }
	/// Sets whether the user can edit labels
	void AllowEditLabels(bool b) { EditLabels = b; }
	/// Returns whether grid lines are drawn
	bool DrawGridLines() { return GridLines; }
	/// Sets whether grid lines are drawn
	void DrawGridLines(bool b) { GridLines = b; }
	/// Returns whether the user can select multiple items at the same time
	bool MultiSelect() { return MultiItemSelect; }
	/// Sets whether the user can select multiple items at the same time
	void MultiSelect(bool b) { MultiItemSelect = b; }

	// Methods
	
	/// Get the display mode.
	/// \sa GListMode
	GListMode GetMode();
	/// Set the display mode.
	/// \sa GListMode
	void SetMode(GListMode m);	

	/// Returns the index of the first selected item
	int64 Value();
	/// Selects the item at index 'i'
	void Value(int64 i);
	/// Selects 'obj'
	bool Select(GListItem *Obj);
	/// Gets the first selected object
	GListItem *GetSelected();
	/// Select all the item in the list
	void SelectAll();
	/// Scrolls the view to the first selected item if not in view
	void ScrollToSelection();
	/// Clears the text cache for all the items and repaints the screen.
	void UpdateAllItems();
	/// Gets the number of items.
	int Length() { return Items.Length(); }

	/// Returns true if the list is empty
	bool IsEmpty() { return Items.Length() == 0; }
	/// Deletes the current item
	bool Delete();
	/// Deletes the item at index 'Index'
	bool Delete(int Index);
	/// Deletes the item 'p'
	virtual bool Delete(GListItem *p);
	/// Remove the item 'Obj' but don't delete it
	virtual bool Remove(GListItem *Obj);
	/// Inserts the item 'p' at index 'Index'.
	bool Insert
	(
		/// The item to insert
		GListItem *p,
		/// The index to insert at or -1 for append
		int Index = -1,
		/// True if you want the list to update immediately. If you are inserting a lot of items quickly
		/// then you should pass false here and then update just once at the end of the insertions.
		bool Update = true
	);
	/// Insert a list of item
	virtual bool Insert
	(
		/// The items to insert
		List<GListItem> &l,
		/// The starting index to insert at
		int Index = -1,
		/// True if you want the list to update immediately. If you are inserting a lot of item list quickly
		/// then you should pass false here and then update just once at the end of the insertions.
		bool Update = true
	);
	/// Return true if the item 'Obj' is in the list
	bool HasItem(GListItem *Obj);
	/// Return the index of the item 'Obj' or -1 if not present
	int IndexOf(GListItem *Obj);
	/// Returns the item at index 'Index'
	GListItem *ItemAt(int Index);
	/// Sort the list
	void Sort
	(
		/// The comparision function. Should return a integer greater then > 0 if the first item item is greater in value.
		GListCompareFunc Compare,
		/// User defined 32-bit value passed through to the 'Compare' function
		NativeInt Data
	);

	/// Removes all items from list and delete the objects.
	virtual void Empty();
	/// Removes all references to externally owned items. Doesn't delete objects.
	virtual void RemoveAll();

	// Impl
	int GetContentSize(int ColumnIdx);
};

#endif
