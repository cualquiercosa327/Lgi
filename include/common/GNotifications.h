#ifndef _GNOTIFICATIONS_H_
#define _GNOTIFICATIONS_H_

enum GNotifyType
{
	GNotifyNull,

	// GItemContainer notification flags
	/// Item inserted
	/// \sa LList, GView::OnNotify
	GNotifyItem_Insert = 0x100,
	/// Item deleted
	/// \sa LList, GView::OnNotify
	GNotifyItem_Delete,
	/// Item selected
	/// \sa LList, GView::OnNotify
	GNotifyItem_Select,
	/// Item clicked
	/// \sa LList, GView::OnNotify
	GNotifyItem_Click,
	/// Item double clicked
	/// \sa LList, GView::OnNotify
	GNotifyItem_DoubleClick,
	/// Item changed
	/// \sa LList, GView::OnNotify
	GNotifyItem_Change,
	/// Column changed
	/// \sa LList, GView::OnNotify
	GNotifyItem_ColumnsChanged,
	/// Column sized
	/// \sa LList, GView::OnNotify
	GNotifyItem_ColumnsResized,
	/// Column clicks
	/// \sa LList, GView::OnNotify
	GNotifyItem_ColumnClicked,
	/// Items dropped on the control
	GNotifyItem_ItemsDropped,
	/// Sent when the control requests a context menu 
	/// outside of the existing items, i.e. in the blank
	/// space below the items.
	GNotifyItem_ContextMenu,
	/// Blank space clicked
	GNotifyContainer_Click,

	// Generic value changed
	GNotifyValueChanged,

	// Control notifications
	GNotify_Activate,					// User has used say the keyboard to activate control:
										//		Check box -> toggle value
										//		Radio button -> select
										//		Button -> push or toggle
										//		Text label -> activate "next" control
										//		Most others -> take focus
										// etc

	// GDocView
	GNotifyDocChanged,
	GNotifyDocLoaded,
	GNotifyCursorChanged,
	GNotifySelectionChanged,
	GNotifyCharsetChanged,
	GNotifyFixedWidthChanged,
	GNotifyShowImagesChanged,

	// GTableLayout
	GNotifyTableLayout_LayoutChanged,	// Sent by GTableLayout to notify it's layout changed
	GNotifyTableLayout_Refresh,			// Sent by child views of GTableLayout to cause it to
										// update the layout

	// GZoomView
	GNotifyViewport_Changed,

	// GTabPage
	GNotifyTabPage_ButtonClick,
	
	// GScrollBar
	GNotifyScrollBar_Create,
	GNotifyScrollBar_Destroy,

	/// Return/Enter pressed
	/// \sa LList, GView::OnNotify
	GNotify_ReturnKey = VK_RETURN,
	/// Backspace pressed
	/// \sa LList, GView::OnNotify
	GNotify_BackspaceKey = VK_BACKSPACE,
	/// Delete pressed
	/// \sa LList, GView::OnNotify
	GNotify_DeleteKey = VK_DELETE,
	/// Escape pressed
	/// \sa LList, GView::OnNotify
	GNotify_EscapeKey = VK_ESCAPE,
	
	
	/// User app notification IDs should start with this value:
	GNotifyUserApp = 0x10000
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