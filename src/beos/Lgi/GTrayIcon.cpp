#include "Lgi.h"

////////////////////////////////////////////////////////////////////////////
int GTrayIcon::Id = 1;

GTrayIcon::GTrayIcon(int icons)
{
	Val = 0;
	Msg = 0;
	Notify = 0;
	MyId = 0;
	Icons = max(icons, 1);
}

GTrayIcon::~GTrayIcon()
{
	Visible(FALSE);
}

int GTrayIcon::OnEvent(GMessage *Msg)
{
	return 0;
}

void GTrayIcon::OnMouseClick(GMouse &m)
{
}

bool GTrayIcon::LoadIcon(char *Str, int Index)
{
	return FALSE;
}

bool GTrayIcon::Visible()
{
	return 0;
}

void GTrayIcon::Visible(bool v)
{
	if (Visible() != v)
	{
		if (v)
		{
		}
		else
		{
		}
	}
}

int GTrayIcon::Value()
{
	return Val;
}

void GTrayIcon::Value(int v)
{
	Val = v;
}

