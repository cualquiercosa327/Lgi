#ifndef _GDATAGRID_H_
#define _GDATAGRID_H_

#include "GList.h"
#include "GDragAndDrop.h"

class GDataGrid : public GList, public GDragDropSource, public GDragDropTarget
{
	struct GDataGridPriv *d;

public:
	enum GDataGridFlags
	{
		GDG_READONLY	= 0x1,
		GDG_INTEGER		= 0x2,
	};

	typedef GListItem *(*ItemFactory)(void *userdata);
	typedef GArray<GListItem*> ItemArray;
	typedef GArray<int> IndexArray;

	GDataGrid(int CtrlId, ItemFactory Func = 0, void *userdata = 0);
	~GDataGrid();

	// Methods
	bool CanAddRecord();
	void CanAddRecord(bool b);
	void SetFactory(ItemFactory Func = 0, void *userdata = 0);
	GListItem *NewItem();
	void SetColFlag(int Col, GDataGridFlags Flags, GVariant *Arg = 0);
	IndexArray *GetDeletedItems();

	// Impl
	void OnItemSelect(GArray<GListItem*> &Items);
	void OnItemClick(GListItem *Item, GMouse &m);
	void OnCreate();
	int OnEvent(GMessage *Msg);
	int OnNotify(GViewI *c, int f);
	void OnMouseWheel(double Lines);
	void OnPaint(GSurface *pDC);
	bool OnLayout(GViewLayoutInfo &Inf);
	bool Remove(GListItem *Obj);
	void Empty();
	
	// D'n'd
	void SetDndFormats(char *SrcFmt, char *AcceptFmt);
	void OnItemBeginDrag(GListItem *Item, GMouse &m);
	bool GetData(GVariant *Data, char *Format);
	bool GetFormats(List<char> &Formats);
	int WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState);
	int OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState);
	ItemArray *GetDroppedItems();
};

#endif