#ifndef _GMDI_H_
#define _GMDI_H_

#define MDI_TAB_STYLE	1

class GMdiChild : public GLayout
{
	friend class GMdiParent;
	
	class GMdiChildPrivate *d;

public:
	GMdiChild();
	~GMdiChild();

	const char *GetClass() { return "GMdiChild"; }

	#if MDI_TAB_STYLE
	int GetOrder();
	#else
	void OnPaint(GSurface *pDC);	
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	LgiCursor GetCursor(int x, int y);
	GRect &GetClient(bool InClientSpace = true);
	#endif

	bool Attach(GViewI *p);
	bool Detach();

	bool Pour();
	char *Name();
	bool Name(const char *n);
	
	virtual void Raise();
	virtual void Lower();
	virtual void OnTitleClick(GMouse &m);
};

class GMdiParent : public GLayout
{
	friend class GMdiChild;
	class GMdiParentPrivate *d;

	#if MDI_TAB_STYLE
	int GetNextOrder();
	#endif
	GMdiChild *IsChild(GViewI *v);

public:
	GMdiParent();
	~GMdiParent();
	
	const char *GetClass() { return "GMdiParent"; }

	void OnPaint(GSurface *pDC);
	bool Attach(GViewI *p);
	bool OnViewMouse(GView *View, GMouse &m);
	bool OnViewKey(GView *View, GKey &Key);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	GRect NewPos();
	bool GetChildren(GArray<GMdiChild*> &Views);
	
	#if MDI_TAB_STYLE
	void OnPosChange();
	void OnMouseClick(GMouse &m);
	bool Detach();
	#endif
};

#endif
