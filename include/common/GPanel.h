/*
**	FILE:			GPanel.h
**	AUTHOR:			Matthew Allen
**	DATE:			29/8/99
**	DESCRIPTION:	Scribe Mail Object and UI
**
**	Copyright (C) 1998, Matthew Allen
**		fret@memecode.com
*/

#ifndef __GPANEL_H
#define __GPANEL_H

#include "GDisplayString.h"

class LgiClass GPanel : public GLayout
{
protected:
	bool IsOpen;
	int Align;
	int ClosedSize;
	int OpenSize;
	GRect ThumbPos;
	GDisplayString *Ds;

	void RePour();
	void SetChildrenVisibility(bool i);
	virtual int CalcWidth();

public:
	GPanel(const char *name, int size, bool open = true);
	~GPanel();

	bool Open();
	virtual void Open(bool i);
	int GetClosedSize();
	void SetClosedSize(int i);
	int GetOpenSize();
	void SetOpenSize(int i);
	int Alignment(); // GV_EDGE_TOP | GV_EDGE_RIGHT | GV_EDGE_BOTTOM | GV_EDGE_LEFT
	void Alignment(int i);

	bool Attach(GViewI *Wnd);
	bool Pour(GRegion &r);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
};

#endif
