#ifndef _GGRAPH_H_
#define _GGRAPH_H_

#include "GDb.h"

class GGraph : public GLayout
{
	struct GGraphPriv *d;

public:
	GGraph(int Id, int XAxix = -1, int YAxis = -1);
	~GGraph();

	bool SetDataSource(GDbRecordset *Rs);
	void OnPaint(GSurface *pDC);
};

#endif