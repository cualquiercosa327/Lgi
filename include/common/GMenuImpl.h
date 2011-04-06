#ifndef _GMENU_IMPL_H_
#define _GMENU_IMPL_H_

class GSubMenu;
class GMenuItem;
class SubMenuImpl;
class MenuImpl;

#include "GPopup.h"

class MenuClickImpl
{
	friend class MenuItemImpl;

protected:
	GMenuItem *Clicked;

public:
	MenuClickImpl()
	{
		Clicked = 0;
	}
	
	virtual ~MenuClickImpl()
	{
	}
	
	GMenuItem *ItemClicked()
	{
		return Clicked;
	}
	
	virtual GView *View() { return 0; }
	virtual SubMenuImpl *IsSub() { return 0; }
	virtual MenuImpl *IsMenu() { return 0; }
};

class SubMenuImpl : public GPopup, public MenuClickImpl
{
	class SubMenuImplPrivate *d;

public:
	SubMenuImpl(GSubMenu *Sub);
	~SubMenuImpl();
	
	const char *GetClass() { return "SubMenuImpl"; }
	bool Visible() { return GPopup::Visible(); }
	void Visible(bool b);
	void Layout(int x, int y);
	void OnPaint(GSurface *pDC);
	bool OnKey(GKey &k);
	
	GView *View() { return this; }
	SubMenuImpl *IsSub() { return this; }
	GSubMenu *GetSub();
};

class MenuImpl : public GView, public MenuClickImpl
{
	class MenuImplPrivate *d;

public:
	MenuImpl(GMenu *Sub);
	~MenuImpl();
	
	const char *GetClass() { return "MenuImpl"; }
	bool Pour(GRegion &r);
    void OnPaint(GSurface *pDC);
	bool HasSubOpen();
    
	GView *View() { return this; }
	MenuImpl *IsMenu() { return this; }
};

class MenuItemImpl : public GView
{
	class MenuItemImplPrivate *d;

public:
	MenuItemImpl(GMenuItem *Item);
	~MenuItemImpl();
	
	const char *GetClass() { return "MenuItemImpl"; }
	GMenuItem *Item();
	void ShowSub();
	void HideSub(bool SetClick = false);
	void Activate();
	bool IsOnSubMenu();
	void OnPaint(GSurface *pDC);
    void OnMouseClick(GMouse &m);
	void OnMouseEnter(GMouse &m);
	void OnMouseExit(GMouse &m);
};

#endif
