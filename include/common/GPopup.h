/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A popup window

#ifndef __GPOPUP_H
#define __GPOPUP_H

/// A popup window: closes when the user clicks off-window.
class LgiClass GPopup :
	public GView
{
	friend class _QPopup;
	friend class GWindow;
	friend class GDropDown;
	friend class GMouseHook;
	friend class GMouseHookPrivate;

    #ifdef __GTK_H__
    Gtk::GtkWidget *Wnd;
    #endif

    #if !WINNATIVE
    static GArray<GPopup*> CurrentPopups;
    #endif

protected:
	class GPopupPrivate *d;
	bool Cancelled;
	GView *Owner;
	int64 Start;

public:
	GPopup(GView *owner);
	~GPopup();

	/// Sets whether the popup should take the focus when it's shown.
	/// The default is 'true'
	void TakeFocus(bool Take);
	const char *GetClass() { return "GPopup"; }
	bool GetCancelled() { return Cancelled; }
	bool Attach(GViewI *p);
	void Visible(bool i);
	bool Visible();
	GMessage::Result OnEvent(GMessage *Msg);
	
	#if defined(MAC)
	bool SetPos(GRect &p, bool Repaint = false);
	GRect &GetPos();
	#endif
};

/// Drop down menu, UI widget for opening a popup.
class LgiClass GDropDown : public GLayout
{
	friend class GPopup;
	
	GPopup *Popup;

public:
	GDropDown(int Id, int x, int y, int cx, int cy, GPopup *popup);
	~GDropDown();

	// Properties
	bool IsOpen();
	void SetPopup(GPopup *popup);
	GPopup *GetPopup();

	// Window events
	void OnFocus(bool f);
	void OnPaint(GSurface *pDC);
	bool OnKey(GKey &k);
	void OnMouseClick(GMouse &m);
	int OnNotify(GViewI *c, int f);

	// Override
	virtual void Activate();
	virtual void OnPopupClose() {}
};

/// Mouse hook grabs mouse events from the OS
class GPopup;
class GMouseHook
{
	class GMouseHookPrivate *d;
	
public:
	GMouseHook();
	~GMouseHook();

	void RegisterPopup(GPopup *p);
	void UnregisterPopup(GPopup *p);
	bool OnViewKey(GView *v, GKey &k);
	void TrackClick(GView *v);

	#ifdef WIN32
	static LRESULT CALLBACK MouseProc(int Code, WPARAM a, LPARAM b);
	#endif
};




#endif
