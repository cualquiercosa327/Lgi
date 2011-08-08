/*
**	FILE:			GLog.h
**	AUTHOR:			Matthew Allen
**	DATE:			28/6/99
**	DESCRIPTION:	Reality logging system
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#ifndef __RLOG_H
#define __RLOG_H

class RLogEntry;
class RLogView;
class GLog;

class RLogEntry
{
friend class RLogView;
friend class GLog;

	char *Desc;
	char *Text;
	COLOUR c;

public:
	RLogEntry(const char *t, const char *desc = 0, int Len = -1, COLOUR c = 0);
	~RLogEntry();
};

class RLogView : public GLayout
{
friend class RLogEntry;
friend class GLog;

protected:
	GLog *Log;
	bool HasBorder;
	bool IsTopDown;
	int SplitPos;

	void UpdateScrollBar();
	int GetScreenItems();
	int GetTotalItems();

public:
	RLogView(GLog *log);

	void OnPaint(GSurface *pDC);
	void OnNcPaint(GSurface *pDC);
	void OnNcCalcClient(long &x1, long &y1, long & x2, long &y2);
	GMessage::Result OnEvent(GMessage *Msg);
	void OnPosChange();
	int OnNotify(GViewI *Ctrl, int Flags);

	bool Border() { return HasBorder; }
	void Border(bool i) { HasBorder = i; }
	bool TopDown() { return IsTopDown; }
	void TopDown(bool i) { IsTopDown = i; }
	int Split() { return SplitPos; }
	void Split(int i) { SplitPos = i; }
};

class GLog
{
friend class RLogEntry;
friend class RLogView;

	char *FileName;
	RLogView *LogView;
	List<RLogEntry> Entries;

public:
	GLog(char *File = 0);
	~GLog();

	void SetView(RLogView *View);
	void Print(COLOUR c, const char *Str, ...);
	void Write(COLOUR c, const char *Str, int Len = -1, char *Desc = 0);
};

#endif
