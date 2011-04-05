

#ifndef __MONTH_VIEW_H
#define __MONTH_VIEW_H

#include "GDateTime.h"

/////////////////////////////////////////////////////////////////////////////////////
extern const char *ShortDayNames[7];
extern const char *FullDayNames[7];
extern const char *ShortMonthNames[12];
extern const char *FullMonthNames[12];

class MonthView
{
	static char Buf[256];

	GDateTime Cursor;
	GDateTime First;	// of month
	GDateTime Start;	// of visible
	GDateTime Cell;

	int Sx, Sy;
	int MonthX, MonthY;

public:
	MonthView(GDateTime *dt = 0);

	// Set
	void Set(GDateTime *dt);
	GDateTime &Get();
	void SetCursor(int x, int y);
	void GetCursor(int &x, int &y);
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
