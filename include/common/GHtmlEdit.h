#ifndef _RICH_EDIT_H_
#define _RICH_EDIT_H_

#include "GHtml2.h"

class GHtmlEdit : public GDocView
{
	class GHtmlEditPriv *d;
	friend class HtmlEdit;
	void SetIgnorePulse(bool b);

public:
	GHtmlEdit();
	~GHtmlEdit();

	const char *GetMimeType() { return "text/html"; }
	void OnPaint(GSurface *pDC);
	void OnCreate();
	void OnPosChange();
	void OnPulse();
	int OnNotify(GViewI * c, int f);

	char *Name();
	bool Name(char *s);
	bool Sunken();
	void Sunken(bool i);
};

#endif