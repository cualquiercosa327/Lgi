#include "Lgi.h"
#include "GBrowser.h"
#include "GHtml2.h"
#include "INet.h"

enum {
	IDC_HTML = 100,
};

class GBrowserPriv
{
public:
	Html2::GHtml2 *Html;
};

GBrowser::GBrowser(char *Title, char *Uri)
{
	d = new GBrowserPriv;
	Name(Title?Title:"Browser");

	if (Attach(0))
	{
		AddView(d->Html = new Html2::GHtml2(IDC_HTML, 0, 0, 100, 100));
		AttachChildren();
		Pour();

		Visible(true);
	}
}

GBrowser::~GBrowser()
{
	DeleteObj(d);
}

bool GBrowser::SetUri(char *Uri)
{
	if (Uri)
	{
		GUri u(Uri);
		if (u.Protocol)
		{
			if (!stricmp(u.Protocol, "file"))
			{
				if (!u.Path)
					return false;

				GAutoString u(ReadTextFile(u.Path));
				if (u)
				{
					d->Html->Name(u);
				}
				else return false;
			}
			else if (!stricmp(u.Protocol, "http"))
			{
			}
			else return false;
		}
	}
	else
	{
		d->Html->Name("");
	}

	return true;
}
