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

	GDataGrid(int CtrlId);
	~GDataGrid();

	void OnItemSelect(GArray<GListItem*> &Items);
	void OnItemClick(GListItem *Item, GMouse &m);
	void OnCreate();
	int OnEvent(GMessage *Msg);
	int OnNotify(GViewI *c, int f);
	void OnMouseWheel(double Lines);
	void OnPaint(GSurface *pDC);

	void SetColFlag(int Col, GDataGridFlags Flags);
};

#endif