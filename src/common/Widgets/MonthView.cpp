#include <stdio.h>
#include "Gdc2.h"
#include "MonthView.h"

const char *ShortDayNames[7];
const char *FullDayNames[7];
const char *ShortMonthNames[12];
const char *FullMonthNames[12];
char MonthView::Buf[256];

MonthView::MonthView(GDateTime *dt)
{
	MonthX = MonthY = 0;
	Sx = Sy = 0;
	Set(dt);
}

void MonthView::Set(GDateTime *dt)
{
	if (dt)
	{
		Cursor = *dt;
		First = Cursor;
		First.Day(1);
		Start = First;
		Start.AddDays(-Start.DayOfWeek());

		int DayOfWeek = First.DayOfWeek();
		MonthX = 7;
		MonthY = (Cursor.DaysInMonth() + DayOfWeek - 1) / 7 + 1;
	}
}

GDateTime &MonthView::Get()
{
	return Cursor;
}

void MonthView::SetCursor(int x, int y)
{
	// Wrap x
	if (x < 0)
	{
		y--;
		x = 6;
	}
	else if (x > 6)
	{
		y++;
		x = 0;
	}

	GDateTime n = Start;
	n.AddDays( (y * MonthX) + x );
	Set(&n);
}

void MonthView::GetCursor(int &x, int &y)
{
	x = Cursor.DayOfWeek();
	
	GDateTime t = Cursor;
	t.Day(1);
	int BaseOffset = t.DayOfWeek();
	y = (Cursor.Day() - 1 + BaseOffset) / 7;
}

void MonthView::SelectCell(int x, int y)
{
	Sx = limit(x, 0, MonthX);
	Sy = limit(y, 0, MonthY);
	Cell = Start;
	Cell.AddDays( (Sy * MonthX) + Sx );
}

char *MonthView::Title()
{
	sprintf(Buf, "%s %i", FullMonthNames[Cursor.Month()-1], Cursor.Year());
	return Buf;
}

char *MonthView::Day(bool FromCursor)
{
	sprintf(Buf, "%i", FromCursor ? Cursor.Day() : Cell.Day());
	return Buf;
}

char *MonthView::Date(bool FromCursor)
{
	if (FromCursor)
	{
		Cursor.GetDate(Buf);
	}
	else
	{
		Cell.GetDate(Buf);
	}
	return Buf;
}

int MonthView::X()
{
	return MonthX;
}

int MonthView::Y()
{
	return MonthY;
}

bool MonthView::IsMonth()
{
	return Cell.Month() == First.Month();
}

bool MonthView::IsToday()
{
	GDateTime Now;
	Now.SetNow();
	return	Now.Day() == Cell.Day() &&
			Now.Month() == Cell.Month() &&
			Now.Year() == Cell.Year();
}

bool MonthView::IsSelected()
{
	return	Cell.Day() == Cursor.Day() &&
			Cell.Month() == Cursor.Month() &&
			Cell.Year() == Cursor.Year();
}

