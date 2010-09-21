/**
	\file
	\author Matthew Allen <fret@memecode.com>
	\brief Simple single window wrapper around the GHtml2 control
 */
#ifndef _GBROWSER_H_
#define _GBROWSER_H_

class GBrowser : public GWindow
{
	class GBrowserPriv *d;

public:
	GBrowser(char *Title, char *Uri = 0);
	~GBrowser();

	bool SetUri(char *Uri = 0);
	void OnPosChange();
	int OnNotify(GViewI *c, int f);
	int OnEvent(GMessage *m);
};

#endif