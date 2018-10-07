
#ifndef __XFontMetrics_h
#define __XFontMetrics_h

#include "xfont.h"

class XFontMetrics : public XObject
{
	class XFontMetricsPrivate *Data;

public:
	XFontMetrics(XFont *f);
	~XFontMetrics();

	int width(uchar i);
	int width(char *str, int len = -1);
	int width(char16 *str, int len = -1);
	int height();
	int ascent();
	int descent();
	
	uchar *GetCoverage(uchar *Map, int Max);
};

#endif
