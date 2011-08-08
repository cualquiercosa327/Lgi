/// \file
/// \author Matthew Allen
/// \brief A tab view

#ifndef _GTABVIEW_H_
#define _GTABVIEW_H_

// Tab control
class GTabPage;
typedef List<GTabPage> TabPageList;

/// A tab control that displays multiple pages of information in a small area.
class LgiClass GTabView :
	public GLayout,
	public ResObject
{
	friend class GTabPage;
	class GTabViewPrivate *d;
	int TabY();

public:
	/// Creates the tab view object
	GTabView(	int id = -1,
				int x = 0,
				int y = 0,
				int cx = 1000,
				int cy = 1000,
				const char *name = 0,
				int Init = 0);
	~GTabView();
	
	const char *GetClass() { return "GTabView"; }

	/// Gets the selected tab
	int64 Value();
	/// Sets the selected tab
	void Value(int64 i);
	
	bool GetPourChildren();
	void SetPourChildren(bool b);

	/// Append an existing tab
	bool Append(GTabPage *Page, int Where = -1);
	/// Append a new tab with the title 'name'
	GTabPage *Append(const char *name, int Where = -1);
	/// Delete a tab
	bool Delete(GTabPage *Page);

	///.Returns the tab at position 'i'
	GTabPage *TabAt(int i);
	/// Gets a pointer to the current tab
	GTabPage *GetCurrent();
	/// Gets the number of tabs
	int GetTabs();
	
	// Impl
	bool Attach(GViewI *parent);
	GMessage::Result OnEvent(GMessage *Msg);
	GViewI *FindControl(int Id);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	GRect &GetTabClient();

	#if defined WIN32
	GViewI *FindControl(HWND hCtrl);
	#endif

	void OnPosChange();
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	bool OnKey(GKey &k);
	void OnFocus(bool f);
	void OnCreate();
};

class LgiClass GTabPage :
	public GView,
	public GLgiRes,
	public ResObject
{
	friend class GTabView;

	// Vars
	GTabView *TabCtrl;
	GRect TabPos;

	// Methods
	void PaintTab(GSurface *pDC, bool Selected);
	bool Attach(GViewI *parent);

public:
	GTabPage(const char *name);
	~GTabPage();

	const char *GetClass() { return "GTabPage"; }

	char *Name();
	bool Name(const char *n);
	GTabView *GetTabControl() { return TabCtrl; }

	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	bool OnKey(GKey &k);
	void OnFocus(bool b);

	void Append(GViewI *Wnd);
	bool Remove(GViewI *Wnd);
	bool LoadFromResource(int Resource);
};

#endif
