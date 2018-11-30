/*	Instantiation:

	GAutoPtr<GFont> f(new LEmojiFont());
	if (f && f->Create())
		GFontSystem::Inst()->AddFont(f);
*/
#ifndef _EMOJI_FONT_H_
#define _EMOJI_FONT_H_

#include "GFont.h"

class LEmojiFont : public GFont
{
	struct LEmojiFontPriv *priv;

	void _Measure(int &x, int &y, OsChar *Str, int Len) override;
	int _CharAt(int x, OsChar *Str, int Len, LgiPxToIndexType Type) override;
	void _Draw(GSurface *pDC, int x, int y, OsChar *Str, int Len, GRect *r, GColour &fore) override;

public:
	LEmojiFont();
	~LEmojiFont();

	bool Create(const char *Face = NULL, GCss::Len Size = GCss::LenInherit, GSurface *pSurface = NULL) override;
	int GetHeight() override;
};

#endif