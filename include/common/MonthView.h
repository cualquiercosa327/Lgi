

#ifndef __MONTH_VIEW_H
#define __MONTH_VIEW_H

#include "LDateTime.h"

/////////////////////////////////////////////////////////////////////////////////////
extern const char *ShortDayNames[7];
extern const char *FullDayNames[7];
extern const char *ShortMonthNames[12];
extern const char *FullMonthNames[12];

class MonthView
{
	static char Buf[256];

protected:
	LDateTime Cursor;
	LDateTime First;	// of month
	LDateTime Start;	// of visible
	LDateTime Cell;

	int Sx, Sy;
	int MonthX, MonthY;

public:
	MonthView(LDateTime *dt = 0);

	// Set
	void Set(LDateTime *dt);
	LDateTime &Get();
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
