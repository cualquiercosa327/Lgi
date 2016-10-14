/// \file
/// \author Matthew Allen
#ifndef _GFILTER_UI_H_
#define _GFILTER_UI_H_

#include "GTree.h"

class GFilterViewPrivate;

enum GFilterNode
{
	LNODE_NULL,
	LNODE_AND,
	LNODE_OR,
	LNODE_COND,
	LNODE_NEW
};

enum GFilterMenu
{
	FMENU_FIELD,
	FMENU_OP,
	FMENU_VALUE
};

class GFilterView;
class GFilterItem;

typedef int (*FilterUi_Menu)(GFilterView *View,
							 GFilterItem *Item,
							 GFilterMenu Menu,
							 GRect &r,
							 GArray<char*> *GetList,
							 void *UserData);

class GFilterItem : public GTreeItem, public GDragDropSource
{
	class GFilterItemPrivate *d;

protected:
	void _PourText(GdcPt2 &Size);
	void _PaintText(GItem::ItemPaintCtx &Ctx);
	void ShowControls(bool s);

public:
	GFilterItem(GFilterViewPrivate *Data, GFilterNode Node = LNODE_NEW);
	~GFilterItem();

	bool GetNot();
	char *GetField();
	int GetOp();
	char *GetValue();

	void SetNot(bool b);
	void SetField(const char *s);
	void SetOp(int i);
	void SetValue(char *s);

	GFilterNode GetNode();
	void SetNode(GFilterNode n);
	bool OnKey(GKey &k);
	void OnMouseClick(GMouse &m);
	void OnExpand(bool b);
	void OptionsMenu();

	// Dnd
	bool OnBeginDrag(GMouse &m);
	bool GetFormats(List<char> &Formats);
	bool GetData(GArray<GDragData> &Data);
};

/// Filter user interface using a tree structure.
class GFilterView : public GLayout
{
	class GFilterViewPrivate *d;

public:
	GFilterView(FilterUi_Menu Callback = 0, void *UserData = 0);
	~GFilterView();

	void SetDefault();
	bool ShowLegend();
	void ShowLegend(bool b);
	void Empty();
	GFilterItem *Create(GFilterNode Node = LNODE_NEW);
	GTreeNode *GetRootNode();

	void OnPaint(GSurface *pDC);
	void OnPosChange();
	void OnCreate();
};

#endif