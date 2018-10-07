#ifndef __LgiLinux_h
#define __LgiLinux_h

#include <stdio.h>
#include "GMem.h"
#include "GToken.h"
#include "GRect.h"

extern "C" uint64 LgiCurrentTime();
extern bool _GetKdePaths(GToken &t, char *Type);
extern bool _GetIniField(char *Grp, char *Field, char *In, char *Out, int OutSize);
extern bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize);

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

#endif
