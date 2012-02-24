/// \file
/// \author Matthew Allen
#include <stdio.h>

#include "Lgi.h"
#include "GPopup.h"
#include "MonthView.h"
#include "GToken.h"
#include "GList.h"
#include "GEdit.h"
#include "GDateTimeCtrls.h"

/// This class is the list of times in a popup, it is used by GTimePopup
class GTimeDrop : public GPopup
{
	friend class GTimePopup;
	GTimePopup *Popup;
	GList *Times;
	bool Ignore;

public:
	GTimeDrop(GTimePopup *p);
	~GTimeDrop();

	void OnCreate();
	void OnPaint(GSurface *pDC);
	void SetTime(GDateTime *t);
	int OnNotify(GViewI *c, int f);
};

GTimePopup::GTimePopup() : ResObject(Res_Custom), GDropDown(-1, 0, 0, 10, 10, 0)
{
	DateSrc = 0;
	SetPopup(Drop = new GTimeDrop(this));
}

bool GTimePopup::OnLayout(GViewLayoutInfo &Inf)
{
    if (!Inf.Width.Max)
    {
        Inf.Width.Min = Inf.Width.Max = 20;
    }
    else if (!Inf.Height.Max)
    {
        Inf.Height.Min = Inf.Height.Max = SysFont->GetHeight() + 6;
    }
    else return false;
    
    return true;
}

void GTimePopup::SetDate(char *d)
{
	GViewI *n = GetNotify();
	if (n AND d)
	{
		GDateTime New;
		New.SetNow();
		char *Old = n->Name();
		if (ValidStr(Old))
		{
			New.Set(Old);
		}
		else if (DateSrc)
		{
			Old = DateSrc->Name();
			if (ValidStr(Old))
			{
				New.Set(Old);
			}
		}
		New.SetTime(d);
			
		char Buf[256];
		New.Get(Buf);

		n->Name(Buf);

		GViewI *Nn = n->GetNotify() ? n->GetNotify() : n->GetParent();
		if (Nn)
		{
			Nn->OnNotify(n, 0);
		}
	}
}

void GTimePopup::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Drop)
		{
			// Set it's time from our current value
			GViewI *n = GetNotify();
			if (n)
			{
				GDateTime New;
				char *Old = n->Name();
				if (Old AND
					New.Set(Old))
				{
					Drop->SetTime(&New);
				}
			}
		}
	}

	GDropDown::OnMouseClick(m);
}

GTimeDrop::GTimeDrop(GTimePopup *p) : GPopup(p)
{
	SetParent(p);
	Popup = p;
	Ignore = true;
	
	Children.Insert(Times = new GList(100, 1, 1, 50, 50));
	if (Times)
	{
		Times->Sunken(false);
		Times->ShowColumnHeader(false);
		Times->AddColumn("");
		Times->MultiSelect(false);

		// Build the list of times...
		GDateTime Dt;
		for (int t=0; t<24; t++)
		{
			int Time;
			
			// Be sensitive to the 24-hr setting...
			if (Dt.GetFormat() & GDTF_24HOUR)
			{
				Time = t;
			}
			else
			{
				Time = t % 12 > 0 ? t % 12 : 12;
			}

			char s[256];
			
			GListItem *i = new GListItem;
			if (i)
			{
				if (Dt.GetFormat() & GDTF_24HOUR)
				{
					sprintf(s, "%i:00", Time);
				}
				else
				{
					sprintf(s, "%i:00 %c", Time, t < 12 ? 'a' : 'p');
				}
				i->SetText(s);
				Times->Insert(i);
			}
			i = new GListItem;
			if (i)
			{
				if (Dt.GetFormat() & GDTF_24HOUR)
				{
					sprintf(s, "%i:30", Time);
				}
				else
				{
					sprintf(s, "%i:30 %c", Time, t < 12 ? 'a' : 'p');
				}
				i->SetText(s);
				Times->Insert(i);
			}
		}
	}

	GDateTime Now;
	Now.SetNow();
	SetTime(&Now);

	GRect r(0, 0, 100, 10*16 + 1);
	SetPos(r);
}

GTimeDrop::~GTimeDrop()
{
	Popup->Drop = 0;
	Popup->Invalidate();
}

void GTimeDrop::OnCreate()
{
	Times->Attach(this);
}

void GTimeDrop::OnPaint(GSurface *pDC)
{
	// 1px black border
	GRect r = GetClient();
	r.Offset(-r.x1, -r.y1);
	pDC->Colour(LC_TEXT, 24);
	pDC->Box(&r);
	r.Size(1, 1);

	// Client
	if (Times)
	{
		Ignore = true;
		Times->SetPos(r);
		Times->Focus(true);
		Ignore = false;
	}
}

int GTimeDrop::OnNotify(GViewI *c, int f)
{
	if (c->GetId() == 100 AND !Ignore)
	{
		if (f == GLIST_NOTIFY_CLICK OR f == GLIST_NOTIFY_RETURN)
		{
			GListItem *Sel = Times->GetSelected();
			if (Sel)
			{
				char *t = Sel->GetText(0);
				if (t)
				{
					Popup->SetDate(t);
					Visible(false);
				}
			}
		}
		else if (f == GLIST_NOTIFY_ESC_KEY)
		{
			Visible(false);
		}
	}

	return 0;
}

void GTimeDrop::SetTime(GDateTime *t)
{
	if (t AND Times)
	{
		List<GListItem>::I All = Times->Start();
		for (GListItem *i=*All; i; i=*++All)
		{
			char *s = i->GetText(0);
			if (s)
			{
				GDateTime p;
				p.SetTime(s);

				if (p.Hours() == t->Hours())
				{
					if (	(p.Minutes() < 30 AND t->Minutes() < 30) OR
							(p.Minutes() >= 30 AND t->Minutes() >= 30) )
					{
						Ignore = true;
						i->Select(true);
						i->ScrollTo();
						Ignore = false;
						break;
					}
				}
			}
		}
	}
}

class GTimePopupFactory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (Class AND
			stricmp(Class, "GTimePopup") == 0)
		{
			return new GTimePopup;
		}

		return 0;
	}
} TimePopupFactory;
