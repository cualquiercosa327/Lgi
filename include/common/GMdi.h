#ifndef _GMDI_H_
#define _GMDI_H_

class GMdiChild : public GLayout
{
	friend class GMdiParent;
	
	class GMdiChildPrivate *d;

public:
	GMdiChild();
	~GMdiChild();
	
	void OnPaint(GSurface *pDC);	
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	void Raise();
	void Lower();
	GRect &GetClient(bool InClientSpace = true);
	bool Pour();
	bool Attach(GViewI *p);
	int OnEvent(GMessage *m);
	char *Name();
	bool Name(char *n);
};

class GMdiParent : public GLayout
{
	class GMdiParentPrivate *d;

	GMdiChild *IsChild(GView *v);

public:
	GMdiParent();
	~GMdiParent();
	
	void OnPaint(GSurface *pDC);
	bool Attach(GViewI *p);
	bool OnViewMouse(GView *View, GMouse &m);
	bool OnViewKey(GView *View, GKey &Key);
	GRect NewPos();
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
};

#endif
