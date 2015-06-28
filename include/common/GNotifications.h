#ifndef _GNOTIFICATIONS_H_
#define _GNOTIFICATIONS_H_

enum GNotifyType
{
	GNotifyNull,
	GNotifyValueChanged,

	// GDocView
	GNotifyDocChanged,
	GNotifyDocLoaded,
	GNotifyCursorChanged,
	GNotifySelectionChanged,
	GNotifyCharsetChanged,
	GNotifyFixedWidthChanged,
	GNotifyShowImagesChanged,

	// GTableLayout
	GNotifyTableLayout_LayoutChanged,
	GNotifyTableLayout_Refresh,

	// GZoomView
	GNotifyViewport_Changed,

	// GTabPage
	GNotifyTabPage_ButtonClick,

	// GItemContainer notification flags
	/// Item inserted
	/// \sa GList, GView::OnNotify
	GNotifyItem_Insert,
	/// Item deleted
	/// \sa GList, GView::OnNotify
	GNotifyItem_Delete,
	/// Item selected
	/// \sa GList, GView::OnNotify
	GNotifyItem_Select,
	/// Item clicked
	/// \sa GList, GView::OnNotify
	GNotifyItem_Click,
	/// Item double clicked
	/// \sa GList, GView::OnNotify
	GNotifyItem_DoubleClick,
	/// Item changed
	/// \sa GList, GView::OnNotify
	GNotifyItem_Change,
	/// Column changed
	/// \sa GList, GView::OnNotify
	GNotifyItem_ColumnsChanged,
	/// Column sized
	/// \sa GList, GView::OnNotify
	GNotifyItem_ColumnsResized,
	/// Column clicks
	/// \sa GList, GView::OnNotify
	GNotifyItem_ColumnClicked,
	/// Backspace pressed
	/// \sa GList, GView::OnNotify
	GNotifyItem_Backspace,
	/// Return/Enter pressed
	/// \sa GList, GView::OnNotify
	GNotifyItem_ReturnKey,
	/// Delete pressed
	/// \sa GList, GView::OnNotify
	GNotifyItem_DeleteKey,
	/// Escape pressed
	/// \sa GList, GView::OnNotify
	GNotifyItem_EscapeKey,
	/// Items dropped on the control
	GNotifyItem_ItemsDropped,
	/// Sent when the control requests a context menu 
	/// outside of the existing items, i.e. in the blank
	/// space below the items.
	GNotifyItem_ContextMenu,
	
	
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