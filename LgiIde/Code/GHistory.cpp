#include "Lgi.h"
#include "GHistory.h"
#include "GList.h"

//////////////////////////////////////////////////////////////
// GHistoryItem
class GHistoryItem : public GListItem
{
	class GHistoryPopup *p;

public:
	GHistoryItem(GHistoryPopup *popup, char *s)
	{
		p = popup;
		SetText(s);
	}
	
	void OnMouseClick(GMouse &m);
};

//////////////////////////////////////////////////////////////
// GHistoryPopup
class GHistoryPopup : public GPopup
{
public:
	GList *Lst;
	char *Str;
	bool Ignore;

	GHistoryPopup() : GPopup(0)
	{
		Lst = 0;
		Str = 0;
		GRect r(0, 0, 300, 300);
		SetPos(r);

		Children.Insert(Lst = new GList(1, 1, 1, 100, 100));
		if (Lst)
		{
			Lst->ShowColumnHeader(false);
			Lst->Sunken(false);
			Lst->AddColumn("", 1000);
		}
	}
	
	~GHistoryPopup()
	{
		DeleteArray(Str);
	}
	
	void OnPaint(GSurface *pDC)
	{
		if (Lst)
		{
			Lst->Focus(true);
		}
		
		pDC->Colour(LC_BLACK, 24);
		pDC->Box();
	}
	
	void OnPosChange()
	{
		if (Lst)
		{
			GRect r = GetClient();
			r.Size(1, 1);
			Lst->SetPos(r);
		}
	}
};

void GHistoryItem::OnMouseClick(GMouse &m)
{
	if (!m.Down())
	{
		DeleteArray(p->Str);
		p->Str = NewStr(GetText(0));
		p->Visible(false);
	}
}

/////////////////////////////////////////////////////////////
// GHistory
class GHistoryPrivate
{
public:
	GHistoryPopup *Popup;
	int TargetId;
	
	GHistoryPrivate()
	{
		Popup = 0;
		TargetId = 0;
	}
};

GHistory::GHistory()
	: GDropDown(-1, 0, 0, 10, 10, 0),
	ResObject(Res_Custom)
{
	d = new GHistoryPrivate;
	SetPopup(d->Popup = new GHistoryPopup);
}

GHistory::~GHistory()
{
	DeleteObj(d);
	DeleteArrays();
}

int GHistory::GetTargetId()
{
	return d->TargetId;
}

void GHistory::SetTargetId(int id)
{
	d->TargetId = id;
}

void GHistory::Add(char *Str)
{
	if (Str)
	{
		bool Has = false;
		for (char *s=First(); s; s=Next())
		{
			if (strcmp(s, Str) == 0)
			{
				Has = true;
				break;
			}
		}
		if (!Has)
		{
			Insert(NewStr(Str));
			if (d->Popup && d->Popup->Lst)
			{
				d->Popup->Lst->Insert(new GHistoryItem(d->Popup, Str));
			}
		}
	}
}

void GHistory::Update()
{
	GList *Lst = d->Popup ? d->Popup->Lst : 0;
	if (Lst)
	{
		Lst->Empty();
		for (char *s=First(); s; s=Next())
		{
			GHistoryItem *i = new GHistoryItem(d->Popup, s);
			if (i) Lst->Insert(i);
		}
	}
}

void GHistory::OnPopupClose()
{
	if (!d->Popup->GetCancelled() &&
		d->TargetId &&
		d->Popup &&
		d->Popup->Str)
	{
		GetWindow()->SetCtrlName(d->TargetId, d->Popup->Str);
	}
}

////////////////////////////////////////////////////////////
// Factory
class GHistoryFactory : public GViewFactory
{
public:
	virtual GView *NewView
	(
		const char *Class,
		GRect *Pos,
		const char *Text
	)
	{
		if (stricmp(Class, "GHistory") == 0)
		{
			return new GHistory;
		}
		return 0;
	}

	
} HistoryFactory;
