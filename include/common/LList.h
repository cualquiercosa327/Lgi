/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A list control

#ifndef __LList_H
#define __LList_H

// Includes
#include "GMem.h"
#include "GArray.h"
#include "GPopup.h"
#include "GDisplayString.h"
#include "GCss.h"
#include "GItemContainer.h"

class LList;
class LListItem;

// Messages
#define WM_END_EDIT_LABEL			(WM_USER+0x556)

//////////////////////////////////////////////////////////////////////////////

/// View modes for the list control
enum LListMode
{
	LListDetails,
	LListColumns,
	LListSpacial,
};

class LgiClass LListItemPainter
{
public:
	// Overridable
	virtual void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c) = 0;
};

class LgiClass LListItemColumn : public GBase, public GItem, public LListItemPainter
{
	LListItem *_Item;
	int _Column;
	int64 _Value;

	void OnPaint(ItemPaintCtx &Ctx) {}

protected:
	List<LListItem> *GetAllItems();
	LListItemColumn *GetItemCol(LListItem *i, int Col);

public:
	LListItemColumn(LListItem *item, int col);

	// Other objects
	LListItem *GetItem() { return _Item; }
	LList *GetList();
	GItemContainer *GetContainer();

	// Props
	int GetColumn() { return _Column; }
	void SetColumn(int i) { _Column = i; }
	virtual int64 Value() { return _Value; }
	virtual void Value(int64 i);
};

/// GItem for populating a LList
class LgiClass LListItem : public GItem, public LListItemPainter
{
	friend class LList;
	friend class LListItemColumn;
	friend class GItemColumn;

protected:
	// Data
	class LListItemPrivate *d;
	GRect Pos;
	LList *Parent;

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
	LListItem();
	virtual ~LListItem();

	GItemContainer *GetContainer();
	/// Get the owning list
	LList *GetList() { return Parent; }
	/// Gets the LListItemColumn's.
	List<LListItemColumn> *GetItemCols();

	// Properties
	
	/// Set the text for a given column
	bool SetText(const char *s, int i=0);

	/// \brief Get the text for a given column.
	///
	/// Override this in your LListItem based class to
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
	/// the parent LList.
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
	
	// Events;
	void OnMouseClick(GMouse &m);
	void OnMeasure(GdcPt2 *Info);
	void OnPaint(GSurface *pDC) { LgiAssert(0); }
	void OnPaint(GItem::ItemPaintCtx &Ctx);
	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c);

	// Overridable
	virtual int Compare(LListItem *To, int Field = 0) { return 0; }
	virtual void OnColumnNotify(int Col, int Data) { Update(); }
};

typedef int (*LListCompareFunc)(LListItem *a, LListItem *b, NativeInt Data);

class LListItems
{
protected:
	List<LListItem> Items;

public:
	template<class T>
	bool GetSelection(List<T> &n)
	{
		n.Empty();
		List<LListItem>::I It = Items.Start();
		for (LListItem *i=*It; i; i=*++It)
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
		List<LListItem>::I It = Items.Start();
		for (LListItem *i=*It; i; i=*++It)
		{
			T *ptr = dynamic_cast<T*>(i);
			if (ptr)
				n.Insert(ptr);
		}
		return n.Length() > 0;
	}

	List<LListItem>::I Start()
	{
		return Items.Start();
	}

	List<LListItem>::I End()
	{
		return Items.End();
	}

	template<typename T>
	bool Iterate(T *&Ptr)
	{
		if (Ptr)
			// Next
			Ptr = dynamic_cast<T*>(Items.Next());
		else
			// First
			Ptr = dynamic_cast<T*>(Items.First());

		return Ptr != NULL;
	}
};

/// List widget
class LgiClass LList :
	public GItemContainer,
	public ResObject,
	public LListItems
{
	friend class LListItem;
	friend class GItemColumn;
	friend class LListItemColumn;

	#ifdef WIN32
	HCURSOR Cursor;
	#endif
	
protected:
	class LListPrivate *d;

	// Contents
	int Keyboard; // index of the item with keyboard focus

	// Flags
	bool EditLabels;
	bool GridLines;

	// Double buffered
	GSurface *Buf;

	// Drawing locations
	GRect ItemsPos;
	GRect ScrollX, ScrollY;
	int FirstVisible;
	int LastVisible;
	int CompletelyVisible;

	// Misc
	bool GetUpdateRegion(LListItem *i, GRegion &r);
	LListItem *HitItem(int x, int y, int *Index = 0);
	GRect &GetClientRect();
	void PourAll();
	void UpdateScrollBars();
	void KeyScroll(int iTo, int iFrom, bool SelectItems);
	void ClearDs(int Col);

public:
	/// Constructor
	LList
	(
		/// The control's ID
		int id,
		/// Left edge position
		int x = 0,
		/// Top edge position
		int y = 0,
		/// The width
		int cx = 100,
		/// The height
		int cy = 100,
		/// An unseen descriptor of the control
		const char *name = NULL
	);
	~LList();

	const char *GetClass() { return "LList"; }

	// Overridables
	
	/// Called when an item is clicked
	virtual void OnItemClick
	(
		/// The item clicked
		LListItem *Item,
		/// The mouse parameters for the click
		GMouse &m
	);
	/// Called when the user selects an item and starts to drag it
	virtual void OnItemBeginDrag
	(
		/// The item being dragged
		LListItem *Item,
		/// The mouse parameters at the time
		GMouse &m
	);
	/// Called when the user selects an item. If multiple items are selected
	/// in one hit this is only called for the first item. Use GetSelection to
	/// get the extent of the selected items.
	virtual void OnItemSelect
	(
		/// The item selected
		GArray<LListItem*> &Items
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

	// Methods
	
	/// Get the display mode.
	/// \sa LListMode
	LListMode GetMode();
	/// Set the display mode.
	/// \sa LListMode
	void SetMode(LListMode m);	

	/// Returns the index of the first selected item
	int64 Value();
	/// Selects the item at index 'i'
	void Value(int64 i);
	/// Selects 'obj'
	bool Select(LListItem *Obj);
	/// Gets the first selected object
	LListItem *GetSelected();
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
	virtual bool Delete(LListItem *p);
	/// Remove the item 'Obj' but don't delete it
	virtual bool Remove(LListItem *Obj);
	/// Inserts the item 'p' at index 'Index'.
	bool Insert
	(
		/// The item to insert
		LListItem *p,
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
		List<LListItem> &l,
		/// The starting index to insert at
		int Index = -1,
		/// True if you want the list to update immediately. If you are inserting a lot of item list quickly
		/// then you should pass false here and then update just once at the end of the insertions.
		bool Update = true
	);
	/// Return true if the item 'Obj' is in the list
	bool HasItem(LListItem *Obj);
	/// Return the index of the item 'Obj' or -1 if not present
	int IndexOf(LListItem *Obj);
	/// Returns the item at index 'Index'
	LListItem *ItemAt(int Index);
	/// Sort the list
	void Sort
	(
		/// The comparision function. Should return a integer greater then > 0 if the first item item is greater in value.
		LListCompareFunc Compare,
		/// User defined 32-bit value passed through to the 'Compare' function
		NativeInt Data = 0
	);

	/// Removes all items from list and delete the objects.
	virtual void Empty();
	/// Removes all references to externally owned items. Doesn't delete objects.
	virtual void RemoveAll();

	// Impl
	int GetContentSize(int ColumnIdx);
};

#endif
