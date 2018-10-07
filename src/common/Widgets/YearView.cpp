#include "Lgi.h"
#include "YearView.h"

/////////////////////////////////////////////////////////////////////////////////////
YearView::YearView(LDateTime *dt)
{
	Sx = 0;
	Sy = 12;
	Cx = Cy = 0;
	Set(dt);
}

void YearView::Set(LDateTime *dt)
{
	if (dt)
	{
		Year = dt->Year();
	}
	else
	{
		LDateTime n;
		n.SetNow();
		Year = n.Year();
	}

	int Day[12], Min = 7;
	for (int i=0; i<12; i++)
	{
		LDateTime t;
		t.Day(1);
		t.Month(i+1);
		t.Year(Year);
		Day[i] = t.DayOfWeek();
		Len[i] = t.DaysInMonth();
		Min = MIN(Day[i], Min);
	}
	Sx = 0;
	for (int n=0; n<12; n++)
	{
		Start[n] = Day[n] - Min;
		Sx = MAX(Sx, Start[n] + Len[n]);
	}
}

LDateTime &YearView::Get()
{
	static LDateTime t;
	int d = Cx - Start[Cy];
	t.Month(Cy + 1);
	t.Year(Year);
	if (d >= 0)
	{
		if (d < t.DaysInMonth())
		{
			t.Day(d + 1);
		}
		else
		{
			t.Day(t.DaysInMonth());
			t.AddDays(d - t.DaysInMonth() + 1);
		}
	}
	else
	{
		t.Day(1);
		t.AddDays(d);
	}
	return t;
}

void YearView::SetCursor(int x, int y)
{
	Cx = limit(x, 0, Sx-1);
	Cy = limit(y, 0, Sy-1);
}

void YearView::SelectCell(int x, int y)
{
}

char *YearView::Title()
{
	return 0;
}

char *YearView::Day(bool FromCursor)
{
	return 0;
}

char *YearView::Date(bool FromCursor)
{
	return 0;
}

int YearView::X()
{
	return Sx;
}

int YearView::Y()
{
	return Sy;
}

bool YearView::IsMonth()
{
	return (Cx >= Start[Cy] && Cx < Start[Cy] + Len[Cy]);
}

bool YearView::IsToday()
{
	return false;
}

bool YearView::IsSelected()
{
	return false;
}

