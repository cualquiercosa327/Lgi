/*hdr
**      FILE:           ToolTabBar.h
**      AUTHOR:         Matthew Allen
**      DATE:           22/3/2000
**      DESCRIPTION:    Toolbar of tabs
**
**      Copyright (C) 1997-1998 Matthew Allen
**              fret@memecode.com
*/

#ifndef __TOOL_TAB_BAR_H
#define __TOOL_TAB_BAR_H

class GToolTab : public GToolButton
{
	friend class GToolTabBar;
	List<GView> Attached;

public:
	GToolTab();
	~GToolTab();

	void OnPaint(GSurface *pDC);
	virtual bool AttachControls(class GToolTabBar *Parent) { return false; }
};

class GToolTabBar : public GToolBar
{
	friend class GToolTab;

	GRect Client;
	GRect Tab;
	GToolTab *Current;
	bool FitToArea;
	bool Border;

	void _PaintTab(GSurface *pDC, GToolTab *Tab);

public:
	GToolTabBar();
	~GToolTabBar();

	bool IsFitToArea() { return FitToArea; }
	void IsFitToArea(bool b) { FitToArea = b; }
	bool HasBorder() { return Border; }
	void HasBorder(bool b) { Border = b; }

	bool Pour(GRegion &r);

	void OnButtonClick(GToolButton *Btn);
	void OnChange(GToolButton *Btn);
	void OnPaint(GSurface *pDC);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnCreate();
};

#endif
