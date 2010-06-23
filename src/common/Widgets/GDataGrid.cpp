#include "Lgi.h"
#include "GDataGrid.h"

GDataGrid::GDataGrid(int CtrlId) : GList(CtrlId, 0, 0, 1000, 1000)
{
	DrawGridLines(true);
}

GDataGrid::~GDataGrid()
{
}

