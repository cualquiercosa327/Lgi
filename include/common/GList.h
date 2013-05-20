/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A list control

#ifndef __GLIST2_H
#define __GLIST2_H

// Includes
#include "GPopup.h"
#include "GArray.h"

// GList notification flags

/// Item inserted
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_INSERT			0
/// Item deleted
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_DELETE			1
/// Item selected
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_SELECT			2
/// Item clicked
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_CLICK			3
/// Item double clicked
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_DBL_CLICK		4
/// Item changed
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_CHANGE			5
/// Column changed
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_COLS_CHANGE	6
/// Column sized
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_COLS_SIZE		7
/// Column clicks
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_COLS_CLICK		8
/// Backspace pressed
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_BACKSPACE		9
/// Return/Enter pressed
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_RETURN			13
/// Delete pressed
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_DEL_KEY		14
/// Escape pressed
/// \sa GList, GView::OnNotify
#define GLIST_NOTIFY_ESC_KEY		15
/// Items dropped on the control
#define GLIST_NOTIFY_ITEMS_DROPPED	16
/// Sent when the control requests a context menu 
/// outside of the existing items, i.e. in the blank
/// space below the items.
#define GLIST_NOTIFY_CONTEXT_MENU	17

// Messages
#define WM_END_EDIT_LABEL			(WM_USER+0x556)

// Constants
#define DEFAULT_COLUMN_SPACING		12

//////////////////////////////////////////////////////////////////////////////

/// View modes for the list control
enum GListMode
{
	GListDetails,
	GListColumns,
	GListSpacial,
};

/// Class for measuring the size of a GItem based object.
class LgiClass GMeasureInfo
{
public:
	/// The width (generally not used)
	int x;
	/// The height
	int y;
};

/// Base class for items in widget containers
class LgiClass GItem : virtual public GEventsI
{
    GAutoPtr<GViewFill> _Fore, _Back;
    
public:
	/// Painting context
	struct ItemPaintCtx : public GRect
	{
		/// The surface to draw on
		GSurface *pDC;

		/// Current foreground colour (24bit)
		COLOUR Fore;
		/// Current background colour (24bit)
		COLOUR Back;
	};

    GItem &operator =(GItem &i)
    {
		if (i._Fore.Get())
			_Fore.Reset(new GViewFill(*i._Fore));
		else
			_Fore.Reset();

		if (i._Back.Get())
			_Back.Reset(new GViewFill(*i._Back));
		else
			_Back.Reset();

        return *this;
    }

	// Events
	
	/// Called when the item is selected
	virtual void OnSelect() {}
	/// Called when the item is clicked
	virtual void OnMouseClick(GMouse &m) {}
	/// Called when the item needs painting
	virtual void OnPaint(ItemPaintCtx &Ctx) = 0;
	/// Called when the item is dragged
	virtual bool OnBeginDrag(GMouse &m) { return false; }
	/// Called when the owning container needs to know the size of the item
	virtual void OnMeasure(GMeasureInfo *Info) {}
	/// Called when the item is inserted into a new container
	virtual void OnInsert() {}
	/// Called when the item is removed from it's container
	virtual void OnRemove() {}

	// Methods

	/// Call to tell the container that the data displayed by the item has changed
	virtual void Update() {}
	/// Moves the item onscreen
	virtual void ScrollTo() {}
	/// Shows a editable label above the item allowing the user to change the value associated with the column 'Col'
	virtual GView *EditLabel(int Col = -1) { return 0; }
	/// Event called when the edit label ends
	virtual void OnEditLabelEnd() {}

	// Data

	/// True if the item is selected
	virtual bool Select() { return false; }
	/// Select/Deselect the item
	virtual void Select(bool b) {}
	/// Gets the text associated with the column 'Col'
	virtual char *GetText(int Col=0) { return 0; }
	/// Sets the text associated with the column 'Col'
	virtual bool SetText(const char *s, int Col=0) { return false; }
	/// Gets the icon index
	virtual int GetImage(int Flags = 0) { return -1; }
	/// Sets the icon index
	virtual void SetImage(int Col) {}
	/// Gets the position
	virtual GRect *GetPos(int Col = -1) { return 0; }
	/// Gets the font for the item
	virtual GFont *GetFont() { return 0; }
    /// Gets the foreground (font colour)
	virtual GViewFill *GetForegroundFill() { return _Fore; }
	/// Sets the foreground
	virtual void SetForegroundFill(GViewFill *Fill) { _Fore.Reset(Fill); }
	/// Gets the background fill setting
	virtual GViewFill *GetBackgroundFill() { return _Back; }
	/// Sets the background fill setting.
	virtual void SetBackgroundFill(GViewFill *Fill) { _Back.Reset(Fill); }
	
	/// Reads / writes list item to XML
	virtual bool XmlIo(class GXmlTag *Tag, bool Write) { return false; }

	bool OnScriptEvent(GViewI *Ctrl) { return false; }
	GMessage::Result OnEvent(GMessage *Msg) { return 0; }
	void OnMouseEnter(GMouse &m) {}
	void OnMouseExit(GMouse &m) {}
	void OnMouseMove(GMouse &m) {}
	bool OnMouseWheel(double Lines) { return false; }
	bool OnKey(GKey &k) { return false; }
	void OnAttach() {}
	void OnCreate() {}
	void OnDestroy() {}
	void OnFocus(bool f) {}
	void OnPulse() {}
	void OnPosChange() {}
	bool OnRequestClose(bool OsShuttingDown) { return false; }
	int OnHitTest(int x, int y) { return 0; }
	void OnChildrenChanged(GViewI *Wnd, bool Attaching) {}
	int OnNotify(GViewI *Ctrl, int Flags) { return 0; }
	int OnCommand(int Cmd, int Event, OsView Wnd) { return 0; }
	void OnPaint(GSurface *pDC) { LgiAssert(0); }
};

/// The popup label for GItem's
class GItemEdit : public GPopup
{
	class GItemEditPrivate *d;

public:
	GItemEdit(GView *parent, GItem *item, int index, int selstart, int selend);
	~GItemEdit();
	
	void OnPaint(GSurface *pDC);
	int OnNotify(GViewI *v, int f);
	void Visible(bool i);
	GMessage::Result OnEvent(GMessage *Msg);
};

/// No marking
/// \sa GListColumn::Mark
#define GLI_MARK_NONE				0
/// Up arrow mark
/// \sa GListColumn::Mark
#define GLI_MARK_UP_ARROW			1
/// Down arrow mark
/// \sa GListColumn::Mark
#define GLI_MARK_DOWN_ARROW			2

/// List view column
class LgiClass GListColumn
	: public ResObject
{
	class GListColumnPrivate *d;
	friend class GDragColumn;
	friend class GListItem;
	friend class GList;

public:
	GListColumn(GList *parent, const char *name, int width);
	virtual ~GListColumn();

	// properties
	
	/// Sets the text
	void Name(const char *n);
	/// Gets the text
	char *Name();
	/// Sets the width
	void Width(int i);
	/// Gets the width
	int Width();
	/// Sets the type of content in the header. Use one of #GIC_ASK_TEXT, #GIC_OWNER_DRAW, #GIC_ASK_IMAGE.
	///
	/// Is this used??
	void Type(int i);
	/// Gets the type of content.
	int Type();
	/// Sets the marking, one of #GLI_MARK_NONE, #GLI_MARK_UP_ARROW or #GLI_MARK_DOWN_ARROW
	void Mark(int i);
	/// Gets the marking, one of #GLI_MARK_NONE, #GLI_MARK_UP_ARROW or #GLI_MARK_DOWN_ARROW
	int Mark();
	/// Sets the icon to display
	void Icon(GSurface *i, bool Own = true);
	/// Gets the icon displayed
	GSurface *Icon();
	/// True if clicked
	int Value();
	
	/// Sets the index into the parent containers GImageList
	void Image(int i);
	/// Gets the index into the parent containers GImageList
	int Image();
	/// true if resizable
	bool Resizable();
	/// Sets whether the user can resize the column
	void Resizable(bool i);

	/// Returns the index of the column if attached to a list
	int GetIndex();
	/// Returns the size of the content in this column
	int GetContentSize();
	/// Returns the list
	GList *GetList();

	/// Paint the column header.
	void OnPaint(GSurface *pDC, GRect &r);

	/// Draws the just the icon, text and mark.
	void OnPaint_Content(GSurface *pDC, GRect &r, bool FillBackground); 
};

class LgiClass GListItemPainter
{
public:
	// Overridable
	virtual void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GListColumn *c) = 0;
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
	friend class GListColumn;

	void OnEditLabelEnd();

protected:
	// Data
	class GListItemPrivate *d;
	GRect Pos;
	GList *Parent;

	// Methods
	bool GridLines();
	GDisplayString *GetDs(int Col, int FitTo = -1);

public:
	// Application defined, defaults to 0
	union {
		void *_UserPtr;
		NativeInt _UserInt;
	};

	// Object
	GListItem();
	virtual ~GListItem();

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
	void OnMeasure(GMeasureInfo *Info);
	void OnPaint(GSurface *pDC) { LgiAssert(0); }
	void OnPaint(GItem::ItemPaintCtx &Ctx);
	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GListColumn *c);

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
	public GLayout,
	public GItemContainer,
	public ResObject,
	public GListItems
{
	friend class GListItem;
	friend class GListColumn;
	friend class GListItemColumn;

	#ifdef WIN32
	HCURSOR Cursor;
	#endif
	
protected:
	class GListPrivate *d;

	// Contents
	List<GListColumn> Columns;
	int Keyboard; // index of the item with keyboard focus

	// Flags
	bool ColumnHeaders;
	bool EditLabels;
	bool GridLines;
	bool MultiItemSelect;

	// Double buffered
	GSurface *Buf;

	// Drawing locations
	GRect ItemsPos;
	GRect ColumnHeader;
	GRect ScrollX, ScrollY;
	int FirstVisible;
	int LastVisible;
	int CompletelyVisible;
	GListColumn *IconCol;

	// Misc
	bool GetUpdateRegion(GListItem *i, GRegion &r);
	GListItem *HitItem(int x, int y, int *Index = 0);
	GRect &GetClientRect();
	void Pour();
	void UpdateScrollBars();
	void KeyScroll(int iTo, int iFrom, bool SelectItems);
	int HitColumn(int x, int y, GListColumn *&Resize, GListColumn *&Over);

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
	/// Called when a column is clicked
	virtual void OnColumnClick
	(
		/// The index of the column
		int Col,
		/// The mouse parameters at the time
		GMouse &m
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
		GListColumn *Col,
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

	// Columns
	
	/// Adds a column to the list
	GListColumn *AddColumn
	(
		/// The text for the column or NULL for no text
		const char *Name,
		/// The width of the column
		int Width = 50,
		/// The index to insert at, or -1 to append to the end
		int Where = -1
	);
	/// Adds a preexisting column to the control
	bool AddColumn
	(
		/// The column object. The object once added is owned by the GList
		GListColumn *Col,
		/// The location to insert or -1 to append
		int Where = -1
	);
	/// Deletes a column from the GList
	bool DeleteColumn(GListColumn *Col);
	/// Deletes all the columns of the GList
	void EmptyColumns();
	/// Returns the column at index 'Index'
	GListColumn *ColumnAt(int Index) { return Columns.ItemAt(Index); }
	/// Returns the column at horizontal offset 'x', or -1 if none matches.
	int ColumnAtX(int X, GListColumn **Col = 0, int *Offset = 0);
	/// Returns the number of columns
	int GetColumns() { return Columns.Length(); }
	/// Starts a column d'n'd operation with the column at index 'Index'
	/// \sa OnColumnReindex is called when the user drops the column
	void DragColumn(int Index);
	/// Returns the last column click info
	bool GetColumnClickInfo(int &Col, GMouse &m);

	// Properties
	
	/// Returns whether display of column headers is switched on
	bool ShowColumnHeader() { return ColumnHeaders; }
	/// Turns on display of column headers
	void ShowColumnHeader(bool Show) { ColumnHeaders = Show; }
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
	/// Resizes all the columns to their content, allowing a little extra space for visual effect
	void ResizeColumnsToContent(int Border = DEFAULT_COLUMN_SPACING);
};

#endif
