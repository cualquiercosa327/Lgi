#ifndef _GDATAGRID_H_
#define _GDATAGRID_H_

#include "GList.h"

class GDataGrid : public GList
{
	struct GDataGridPriv *d;

public:
	enum GDataGridFlags
	{
		GDG_READONLY = 0x1,
	};

	typedef GListItem *(*ItemFactory)(void *userdata);

	GDataGrid(int CtrlId, ItemFactory Func = 0, void *userdata = 0);
	~GDataGrid();

	bool CanAddRecord();
	void CanAddRecord(bool b);
	void SetFactory(ItemFactory Func = 0, void *userdata = 0);

	void OnItemSelect(GArray<GListItem*> &Items);
	void OnItemClick(GListItem *Item, GMouse &m);
	void OnCreate();
	int OnEvent(GMessage *Msg);
	int OnNotify(GViewI *c, int f);
	void OnMouseWheel(double Lines);
	void OnPaint(GSurface *pDC);
	bool OnLayout(GViewLayoutInfo &Inf);
	void SetColFlag(int Col, GDataGridFlags Flags);
	GListItem *NewItem();
};

#endif