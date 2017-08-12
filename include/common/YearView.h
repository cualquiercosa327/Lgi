

#ifndef __YEAR_VIEW_H
#define __YEAR_VIEW_H

#include "LDateTime.h"

/////////////////////////////////////////////////////////////////////////////////////
class YearView
{
	int Year;
	int Start[12], Len[12];
	int Cx, Cy;
	int Sx, Sy;

	int Update();

public:
	YearView(LDateTime *dt = 0);

	// Set
	void Set(LDateTime *dt);
	LDateTime &Get();
	void SetCursor(int x, int y);
	void SelectCell(int x, int y);
	
	// Get
	char *Title();
	char *Day(bool FromCursor = false);
	char *Date(bool FromCursor = false);
	int X();
	int Y();
	bool IsMonth();
	bool IsToday();
	bool IsSelected();
};

#endif
