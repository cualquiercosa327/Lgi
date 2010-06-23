
#ifndef __XFont_h
#define __XFont_h

#define Status int
#include "Xft.h"
#undef Status
#include "LgiOsDefs.h"

class XFont
{
	class XFontPrivate *Data;
	friend class XFontMetrics;

	void GetScale(double &x, double &y);

public:
	XFont();
	~XFont();

	XftFont *GetTtf();
	XFont &operator =(XFont &f);

	void SetPainter(OsPainter p);
	void SetFamily(char *face);
	void SetPointSize(int height);
	void SetBold(bool bold);
	void SetItalic(bool italic);
	void SetUnderline(bool underline);

	int GetAscent();
	int GetDescent();
	char *GetFamily();
	int GetPointSize();
	bool GetBold();
	bool GetItalic();
	bool GetUnderline();
};

class XFontMetrics
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

extern bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize);

#endif
