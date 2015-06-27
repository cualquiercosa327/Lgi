#ifndef _GNOTIFICATIONS_H_
#define _GNOTIFICATIONS_H_

enum GNotifyType
{
	GNotifyNull,
	GNotifyValueChanged,

	// GDocView
	GTVN_DOC_CHANGED,
	GTVN_CURSOR_CHANGED,
	GTVN_SELECTION_CHANGED,
	GTVN_CODEPAGE_CHANGED,
	GTVN_FIXED_WIDTH_CHANGED,
	GTVN_SHOW_IMGS_CHANGED,
	GTVN_DOC_LOADED,

	// GTableLayout
	GTABLELAYOUT_LAYOUT_CHANGED,
	GTABLELAYOUT_REFRESH,

	// GZoomView
	NotifyViewportChanged,

	// GTabPage
	TabPage_BtnNone,
	TabPage_BtnClick,

	// GItemContainer notification flags
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
	
	
	/// User app notification IDs should start with this value:
	GNotifyUserApp = 1000
	/*
	e.g.
	enum MyAppNotifications
	{
		NotifyStart = GNotifyUserApp,
		NotifyEnd,
		NotifyThreadDone,
	};
	*/
};

#endif