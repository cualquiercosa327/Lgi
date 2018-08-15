/*
**	FILE:			GProgressStatusPane.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			13/12/1999
**	DESCRIPTION:	Progress meter for the status bar
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GProgressStatusPane.h"

////////////////////////////////////////////////////////////////////////////////
GProgressStatusPane::GProgressStatusPane() : DoEvery(200)
{
	SetWidth(100);
	Sunken(true);
	_BorderSize = 1;
}

GProgressStatusPane::~GProgressStatusPane()
{
}

void GProgressStatusPane::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();

	pDC->Colour(LC_MED, 24);
	pDC->Box(&r);
	r.Size(1, 1);
	pDC->Box(&r);
	r.Size(1, 1);

	double Pos = (High != Low) ? (double) Val / ((double) High - (double) Low) : 0;
	int x = (int) (r.X() * Pos);
	if (x < r.X())
	{
		pDC->Rectangle(r.x1+x, r.y1, r.x2, r.y2);
	}

	pDC->Colour(LC_FOCUS_SEL_BACK, 24);
	pDC->Rectangle(r.x1, r.y1, r.x1+x-1, r.y2);
}

int64 GProgressStatusPane::Value()
{
	return Progress::Value();
}

void GProgressStatusPane::Value(int64 v)
{
	Progress::Value(v);

	if (DoNow() || !v)
	{
		Invalidate();
		LgiYield();
	}
}

