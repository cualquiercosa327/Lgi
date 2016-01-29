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
	GRect TabPos;
	bool First;

	bool SetPos(GRect &r, bool Repaint = false);
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m) {}
	void OnMouseMove(GMouse &m) {}
	void OnMouseEnter(GMouse &m) {}
	void OnMouseExit(GMouse &m) {}

public:
	GToolTab();
	~GToolTab();

	const char *GetClass() { return "GToolTab"; }

	/// Override this event to attach controls to the current view.
	virtual void OnSelect() {}
};

class GToolTabBar : public GToolBar
{
	friend class GToolTab;

	GRect Client;
	GRect Tab;
	GToolTab *Current;
	bool FitToArea;
	bool Border;
	bool InOnChangeEvent;

	void _PaintTab(GSurface *pDC, GToolTab *Tab);

public:
	GToolTabBar(int Id = -1);
	~GToolTabBar();

	const char *GetClass() { return "GToolTabBar"; }

	int64 Value();
	void Value(int64 i);

	bool IsOk();

	bool IsFitToArea() { return FitToArea; }
	void IsFitToArea(bool b) { FitToArea = b; }
	bool HasBorder() { return Border; }
	void HasBorder(bool b) { Border = b; }

	bool Pour(GRegion &r);

	void OnButtonClick(GToolButton *Btn);
	void OnChange(GToolButton *Btn);
	void OnPaint(GSurface *pDC);
	void OnCreate();
	void OnMouseClick(GMouse &m);
	int OnNotify(GViewI *c, int f);
};

#endif
