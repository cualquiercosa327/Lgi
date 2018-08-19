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

	bool PourAll();
	char *Name();
	bool Name(const char *n);
	
	virtual void Raise();
	virtual void Lower();
	virtual void OnTitleClick(GMouse &m);
	virtual void OnButtonClick(GMouse &m);
	virtual void OnPaintButton(GSurface *pDC, GRect &rc);
};

class GMdiParent : public GLayout
{
	friend class GMdiChild;
	class GMdiParentPrivate *d;

	#if MDI_TAB_STYLE
	int GetNextOrder();
	#endif
	GMdiChild *IsChild(GViewI *v);
	::GArray<GMdiChild*> &PrivChildren();

public:
	GMdiParent();
	~GMdiParent();
	
	const char *GetClass() { return "GMdiParent"; }

	bool HasButton();
	void HasButton(bool b);

	void OnPaint(GSurface *pDC);
	bool Attach(GViewI *p);
	bool OnViewMouse(GView *View, GMouse &m);
	bool OnViewKey(GView *View, GKey &Key);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	GRect NewPos();
	GViewI *GetTop();

	template<typename T>
	bool GetChildren(::GArray<T*> &Views)
	{
		for (auto c : PrivChildren())
		{
			T *t = dynamic_cast<T*>(c);
			if (t)
				Views.Add(t);
		}
		#if MDI_TAB_STYLE
		Views.Sort
		(
			[]
			(T **a, T **b)
			{
				return (*a)->GetOrder() - (*b)->GetOrder();
			}
		);
		#endif
		return Views.Length() > 0;
	}

	
	#if MDI_TAB_STYLE
	void OnPosChange();
	void OnMouseClick(GMouse &m);
	bool Detach();
	#endif
};

#endif
