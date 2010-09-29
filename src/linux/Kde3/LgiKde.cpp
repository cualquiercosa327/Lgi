#include <string.h>

#include <qstring.h>
#include <kapplication.h>
#include <kservice.h>
#include <kuserprofile.h>
#include <kglobalsettings.h>
#include <kglobal.h>
#include <klocale.h>

#include "LgiWinManGlue.h"

QCString InstName("LgiKde3");
KInstance Inst(InstName);

extern "C"
{
	bool LgiWmInit(void *Reserved);
	bool LgiWmMimeToApps(char *Mime, GArray<GAppInfo*> &Apps, int Limit);
	bool LgiWmGetSysFont(char *Type, char *Font, int FontLen, int &PtSize);
	bool LgiWmGetLanguage(char *Lang);
	bool LgiWmGetColour(int Which, WmColour *Colour);
	bool LgiWmGetPath(int Which, char *Buf, int BufLen);
}

char *QStrToUtf8(QString s)
{
	char *n = 0;
	
	QCString utf = s.utf8();
	int Len = utf.length();
	n = new char[Len + 1];
	if (n)
	{
		memcpy(n, (const char*)utf, Len);
		n[Len] = 0;
	}
	
	return n;
}

bool LgiWmInit(void *Reserved)
{
	printf("Lgi Kde3 add-on loaded.\n");

	return true;
}

bool LgiWmMimeToApps(char *Mime, GArray<GAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	KServiceTypeProfile::OfferList offers = KServiceTypeProfile::offers(Mime, "Application");
	KServiceTypeProfile::OfferList::ConstIterator it;
	for (it=offers.begin(); Apps.Length()<Limit AND it!=offers.end(); it++)
	{
		KService::Ptr service = (*it).service();
		GAppInfo *App = new GAppInfo;
		if (App)
		{
			Apps.Add(App);
			App->Path = QStrToUtf8(service->exec());
			App->Name = QStrToUtf8(service->name());
			if (App->Icon = QStrToUtf8(service->icon()))
			{
				if (NOT strchr(App->Icon, '.'))
				{
					DeleteArray(App->Icon);
				}
			}
			
			Status = true;
		}
	}		

	return Status;
}

bool LgiWmGetSysFont(char *Type, char *Font, int FontLen, int &PtSize)
{
	bool Status = false;
	
	if (Type AND Font)
	{
		QFont f;
		
		if (strcasecmp(Type, "font") == 0)
		{
			f = KGlobalSettings::generalFont();
		}
		else if (strcasecmp(Type, "menuFont") == 0)
		{
			f = KGlobalSettings::menuFont();
		}
		else if (strcasecmp(Type, "toolBarFont") == 0)
		{
			f = KGlobalSettings::toolBarFont();
		}
		else if (strcasecmp(Type, "fixed") == 0)
		{
			f = KGlobalSettings::fixedFont();
		}
		else
		{
			printf("%s:%i - '%s' not a valid font type.\n", __FILE__, __LINE__, Type);
			return false;
		}
		
		char *Family = QStrToUtf8(f.family());
		if (Family)
		{
			strncpy(Font, Family, FontLen-1);
			Font[FontLen-1] = 0;
			
			PtSize = f.pointSize();
			Status = true;
			
			DeleteArray(Family);
		}
		else printf("%s:%i - QStrToUtf8(f.family()) failed.\n", __FILE__, __LINE__);
	}
	
	return Status;
}

bool LgiWmGetLanguage(char *Lang)
{
	bool Status = false;
	
	if (Lang)
	{
		char *l = QStrToUtf8(KGlobal::locale()->language());
		if (l)
		{
			strcpy(Lang, l);
			Status = true;
		}
	}
	
	return Status;
}

bool LgiWmGetColour(int Which, WmColour *Colour)
{
	if (NOT Colour)
		return false;

	QColor c;

	switch (Which)
	{
		case 7: // LC_MED
		{
			c = KGlobalSettings::buttonBackground();
			break;
		}
		case 10: // LC_DIALOG
		{
			c = KGlobalSettings::baseColor();
			break;
		}
		case 12: // LC_TEXT
		{
			c = KGlobalSettings::textColor();
			break;
		}
		case 13: // LC_SELECTION
		{
			c = KGlobalSettings::highlightColor();
			break;
		}
		case 14: // LC_SEL_TEXT
		{
			c = KGlobalSettings::highlightedTextColor();
			break;
		}
		case 15: // LC_ACTIVE_TITLE
		{
			c = KGlobalSettings::activeTitleColor();
			break;
		}
		case 16: // LC_ACTIVE_TITLE_TEXT
		{
			c = KGlobalSettings::activeTextColor();
			break;
		}
		case 17: // LC_INACTIVE_TITLE
		{
			c = KGlobalSettings::inactiveTitleColor();
			break;
		}
		case 18: // LC_INACTIVE_TITLE_TEXT
		{
			c = KGlobalSettings::inactiveTextColor();
			break;
		}

		case 0: // LC_BLACK
		case 1: // LC_DKGREY
		case 2: // LC_MIDGREY
		case 3: // LC_LTGREY
		case 4: // LC_WHITE
		case 5: // LC_SHADOW
		case 6: // LC_LOW
		case 8: // LC_HIGH
		case 9: // LC_LIGHT
		case 11: // LC_WORKSPACE
		case 19: // LC_MENU_BACKGROUND
		case 20: // LC_MENU_TEXT
		default:
		{
			return false;
		}
	}
	
	Colour->r = c.red();
	Colour->g = c.green();
	Colour->b = c.blue();
	Colour->range = Colour->a = 255;
	
	// printf("Colour[%i] = %i,%i,%i\n", Which, Colour->r, Colour->g, Colour->b);

	return true;
}

bool LgiWmGetPath(int Which, char *Buf, int BufLen)
{
	if (NOT Buf OR BufLen < 2)
		return false;
	
	QString p;
	switch (Which)
	{
		case LSP_DESKTOP:
		{
			p = KGlobalSettings::desktopPath();
			break;
		}
		case LSP_TRASH:
		{
			p = KGlobalSettings::trashPath();
			break;
		}
		case LSP_LOCAL_APP_DATA:
		{
			p = KGlobalSettings::documentPath();
			break;
		}
		default:
		{
			return false;
		}
	}
	
	char *u = QStrToUtf8(p);
	if (u)
	{	
		strncpy(Buf, u, BufLen - 1);
		Buf[BufLen] = 0;
		
		char *e = Buf + strlen(Buf);
		if (e > Buf AND e[-1] == '/')
			e[-1] = 0;
		
		return true;
	}
	
	return false;
}
