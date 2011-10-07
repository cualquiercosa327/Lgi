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

    struct GGraphPair
    {
	    GVariant x, y;
	    void *UserData;
    };
    
	GGraph(int Id, int XAxix = -1, int YAxis = -1);
	~GGraph();

	// Api
	bool SetDataSource(GDbRecordset *Rs, int XAxis = -1, int YAxis = -1);
	bool AddPair(char *x, char *y, void *UserData = 0);
	void SetStyle(Style s);
	Style GetStyle();
	GArray<GGraphPair*> *GetSelection();

    // Impl
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);	
};

#endif