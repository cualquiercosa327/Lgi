
#ifndef __XFont_h
#define __XFont_h

#define Status int
#include "Xft.h"
#undef Status
#include "LgiLinux.h"
#include "xpainter.h"

class XFont : public XObject
{
	class XFontPrivate *Data;
	friend class XFontMetrics;

	void GetScale(double &x, double &y);

public:
	XFont();
	~XFont();

	Font GetFont();
	XFontStruct *GetStruct();
	XftFont *GetTtf();
	XFont &operator =(XFont &f);

	void SetPainter(XPainter *p);
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

#endif
