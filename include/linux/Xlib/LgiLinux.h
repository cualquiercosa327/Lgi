

#ifndef __LgiLinux_h
#define __LgiLinux_h

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "GMem.h"
#include "GToken.h"
#include "GRect.h"

#undef Status
#undef Success
#undef None
#undef Above
#undef Below

#define XStatus		int
#define XSuccess	0
#define XAbove		0
#define XBelow		1
#define XNone		0L

extern "C" uint64 LgiCurrentTime();
extern bool _GetKdePaths(GToken &t, char *Type);
extern bool _GetIniField(char *Grp, char *Field, char *In, char *Out, int OutSize);
extern bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize);

XChar2b *ConvertToX(char16 *s, int len = -1);
extern char *XErr(int i);
extern char *XMessage(int i);

class XObject
{
public:
	virtual ~XObject() {}

	static Display *XDisplay();
	static class XApplication *XApp();
};

class OsPoint
{
public:
	int x, y;

	OsPoint()
	{
		x = y = 0;
	}

	OsPoint(int X, int Y)
	{
		x = X;
		y = Y;
	}

	void set(int sx, int sy) { x = sx; y = sy; }
};

class XCursor : public XObject
{
public:
	OsPoint &pos();
};

class XEventSink : public XObject
{
public:
	virtual void OnEvent(XEvent *Event) = 0;
};

enum ViewBackground
{
	NoBackground
};

#include "xlist.h"

#endif
