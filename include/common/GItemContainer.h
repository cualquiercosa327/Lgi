#ifndef _ITEM_CONTAINER_H_
#define _ITEM_CONTAINER_H_

#include "GPopup.h"

class GItemContainer;
#define DragColumnColour				LC_LOW

// GItemContainer notification flags
enum GItemContainerNotify
{
	/// Item inserted
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_INSERT,
	/// Item deleted
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_DELETE,
	/// Item selected
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_SELECT,
	/// Item clicked
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_CLICK,
	/// Item double clicked
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_DBL_CLICK,
	/// Item changed
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_CHANGE,
	/// Column changed
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_COLS_CHANGE,
	/// Column sized
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_COLS_SIZE,
	/// Column clicks
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_COLS_CLICK,
	/// Backspace pressed
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_BACKSPACE,
	/// Return/Enter pressed
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_RETURN,
	/// Delete pressed
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_DEL_KEY,
	/// Escape pressed
	/// \sa GList, GView::OnNotify
	GITEM_NOTIFY_ESC_KEY,
	/// Items dropped on the control
	GITEM_NOTIFY_ITEMS_DROPPED,
	/// Sent when the control requests a context menu 
	/// outside of the existing items, i.e. in the blank
	/// space below the items.
	GITEM_NOTIFY_CONTEXT_MENU,
};

/// Base class for items in widget containers
class LgiClass GItem : virtual public GEventsI
{
    GAutoPtr<GCss> Css;
    
public:
	/// Painting context
	struct ItemPaintCtx : public GRect
	{
		/// The surface to draw on
		GSurface *pDC;

		/// Current foreground colour (24bit)
		GColour Fore;
		/// Current background colour (24bit)
		GColour Back;
		
		/// Number of columns
		int Columns;
		/// Width of each column
		int *ColPx;
	};

    GItem &operator =(GItem &i)
    {
		if (i.GetCss())
		{
			GCss *c = GetCss(true);
			if (c)
			{
				*c = *i.GetCss();
			}
		}
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
	virtual void OnMeasure(GdcPt2 *Info) {}
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

	// Style access
	GCss *GetCss(bool Create = false)
	{
		if (!Css && Create) Css.Reset(new GCss);
		return Css;
	}
	
	bool SetCssStyle(const char *CssStyle)
	{
		if (!Css && ValidStr(CssStyle)) Css.Reset(new GCss);
		if (!Css) return false;
		return Css->Parse(CssStyle, GCss::ParseRelaxed);		
	}
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
/// \sa GItemColumn::Mark
#define GLI_MARK_NONE				0
/// Up arrow mark
/// \sa GItemColumn::Mark
#define GLI_MARK_UP_ARROW			1
/// Down arrow mark
/// \sa GItemColumn::Mark
#define GLI_MARK_DOWN_ARROW			2

/// Item container column
class LgiClass GItemColumn
	: public ResObject
{
	class GItemColumnPrivate *d;
	friend class GDragColumn;
	friend class GListItem;
	friend class GItemContainer;

public:
	GItemColumn(GItemContainer *parent, const char *name, int width);
	virtual ~GItemColumn();

	// properties
	bool InDrag();
	GRect GetPos();
	void SetPos(GRect &r);
	
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
	bool Value();
	/// Set clicked
	void Value(bool i);
	
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
	GItemContainer *GetList();

	/// Paint the column header.
	void OnPaint(GSurface *pDC, GRect &r);

	/// Draws the just the icon, text and mark.
	void OnPaint_Content(GSurface *pDC, GRect &r, bool FillBackground); 
};

/// Client draws the content.
#define GIC_OWNER_DRAW				0x01
/// Column header is text.
#define GIC_ASK_TEXT				0x02
/// Column header is an image.
#define GIC_ASK_IMAGE				0x04
/// Not used.
#define GIC_OWN_LIST				0x08
/// Drag is over the control
#define GIC_IN_DRAG_OP				0x10

#define DRAG_NONE					0
#define SELECT_ITEMS				1
#define RESIZE_COLUMN				2
#define DRAG_COLUMN					3
#define CLICK_COLUMN				4
#define TOGGLE_ITEMS				5
#define CLICK_ITEM					6
#define DEFAULT_COLUMN_SPACING		12

class LgiClass GItemContainer :
	public GLayout,
	public GImageListOwner 
{
	friend class GItemColumn;

public:
	struct ColInfo
	{
		int Idx, ContentPx, WidthPx;
		GItemColumn *Col;
		int GrowPx() { return WidthPx < ContentPx ? ContentPx - WidthPx : 0; }
	};
	
	struct ColSizes
	{
		GArray<ColInfo> Info;
		int FixedPx;
		int ResizePx;
	};
	
protected:
	int Flags;
	int DragMode;
	bool ColumnHeaders;
	GRect ColumnHeader;
	int ColClick;
	GMouse ColMouse;	

	GArray<GItemColumn*> Columns;
	GAutoPtr<GItemColumn> IconCol;
	class GDragColumn *DragCol;

	/// Returns size information for columns
	void GetColumnSizes(ColSizes &cs);
	
	/// Paints all the column headings
	void PaintColumnHeadings(GSurface *pDC);
	
	/// This clears all the display string cache for a given column
	virtual void ClearDs(int Col) = 0;

public:
	GItemContainer();
	virtual ~GItemContainer();

	// Props
	bool OwnerDraw() { return TestFlag(Flags, GIC_OWNER_DRAW); }
	void OwnerDraw(bool b) { if (b) SetFlag(Flags, GIC_OWNER_DRAW); else ClearFlag(Flags, GIC_OWNER_DRAW); }
	bool AskText() { return TestFlag(Flags, GIC_ASK_TEXT); }
	void AskText(bool b) { if (b) SetFlag(Flags, GIC_ASK_TEXT); else ClearFlag(Flags, GIC_ASK_TEXT); }
	bool AskImage() { return TestFlag(Flags, GIC_ASK_IMAGE); }
	void AskImage(bool b) { if (b) SetFlag(Flags, GIC_ASK_IMAGE); else ClearFlag(Flags, GIC_ASK_IMAGE); }
	bool InsideDragOp() { return TestFlag(Flags, GIC_IN_DRAG_OP); }
	void InsideDragOp(bool b) { if (b) SetFlag(Flags, GIC_IN_DRAG_OP); else ClearFlag(Flags, GIC_IN_DRAG_OP); }

	/// Returns whether display of column headers is switched on
	bool ShowColumnHeader() { return ColumnHeaders; }

	/// Turns on display of column headers
	void ShowColumnHeader(bool Show) { ColumnHeaders = Show; }
		
	/// Adds a column to the list
	GItemColumn *AddColumn
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
		GItemColumn *Col,
		/// The location to insert or -1 to append
		int Where = -1
	);
	
	/// Deletes a column from the GList
	bool DeleteColumn(GItemColumn *Col);
	
	/// Deletes all the columns of the GList
	void EmptyColumns();
	
	/// Returns the column at index 'Index'
	GItemColumn *ColumnAt(unsigned i) { return i < Columns.Length() ? Columns[i] : NULL; }
	
	/// Returns the column at horizontal offset 'x', or -1 if none matches.
	int ColumnAtX(int X, GItemColumn **Col = 0, int *Offset = 0);
	
	/// Returns the number of columns
	int GetColumns() { return Columns.Length(); }
	
	/// Starts a column d'n'd operation with the column at index 'Index'
	/// \sa OnColumnReindex is called when the user drops the column
	void DragColumn(int Index);
	
	/// Returns the last column click info
	bool GetColumnClickInfo(int &Col, GMouse &m);

	int HitColumn(int x, int y, GItemColumn *&Resize, GItemColumn *&Over);

	/// Called when a column is clicked
	virtual void OnColumnClick
	(
		/// The index of the column
		int Col,
		/// The mouse parameters at the time
		GMouse &m
	);

	virtual void UpdateAllItems() = 0;
	virtual int GetContentSize(int ColumnIdx) = 0;

	/// Resizes all the columns to their content, allowing a little extra space for visual effect
	virtual void ResizeColumnsToContent(int Border = DEFAULT_COLUMN_SPACING);
};

////////////////////////////////////////////////////////////////////////////////////////////
#define DRAG_COL_ALPHA					0xc0
#define LINUX_TRANS_COL					0

class GDragColumn : public GWindow
{
	GItemContainer *List;
	GItemColumn *Col;
	int Index;
	int Offset;
	GdcPt2 ListScrPos;
	
	#ifdef LINUX
	GSurface *Back;
	#endif

public:
	int GetOffset() { return Offset; }
	int GetIndex() { return Index; }
	GItemColumn *GetColumn() { return Col; }

	GDragColumn(GItemContainer *list, int col);
	~GDragColumn();

	void OnPaint(GSurface *pScreen);
};


#endif