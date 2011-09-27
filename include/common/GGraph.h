#ifndef _GGRAPH_H_
#define _GGRAPH_H_

#include "GDb.h"

class GGraph : public GLayout
{
	struct GGraphPriv *d;

public:
    enum Style
    {
        LineGraph,
        PointGraph,
    };
    
	GGraph(int Id, int XAxix = -1, int YAxis = -1);
	~GGraph();

	// Api
	bool SetDataSource(GDbRecordset *Rs);
	void SetStyle(Style s);
	Style GetStyle();

    // Impl
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);	
};

#endif