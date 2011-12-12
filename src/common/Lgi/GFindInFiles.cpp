#include "Lgi.h"
#include "GFindInFiles.h"
#include "GPopup.h"
#include "GList.h"
#include "resdefs.h"

class GHistory;
class GHistoryPopup : public GPopup
{
	GHistory *History;
	GList *Lst;

public:
	GHistoryPopup(GHistory *h);
	
	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(GColour(0, 0, 0));
		pDC->Rectangle();
	}
};

class GHistory : public GDropDown, public ResObject
{
	friend class GHistoryPopup;
	GArray<GAutoString> Strs;
	
public:
	GHistory() :
		GDropDown(-1, 0, 0, 100, 20, 0),
		ResObject(Res_Custom)
	{
		SetPopup(new GHistoryPopup(this));
	}
};

GHistoryPopup::GHistoryPopup(GHistory *h) : GPopup(h)
{
	History = h;
	
	GRect r(0, 0, 300, 200);
	SetPos(r);
	r.Size(1, 1);
	AddView(Lst = new GList(100, r.x1, r.y1, r.X()-1, r.Y()-1));
	Lst->Sunken(false);
	Lst->ShowColumnHeader(false);
	
	for (int i=0; i<History->Strs.Length(); i++)
	{
		GListItem *it = new GListItem;
		it->SetText(History->Strs[i]);
		Lst->Insert(it);
	}
}

class GHistoryFactory : public GViewFactory
{
public:
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (!stricmp(Class, "GHistory"))
		{
			return new GHistory;
		}
		return 0;
	}
} HistoryFactory;

struct GFindInFilesPriv
{
	GFindInFilesPriv()
	{
	}
	
	~GFindInFilesPriv()
	{
	}
};
	
GFindInFiles::GFindInFiles(GViewI *Parent)
{
	d = new GFindInFilesPriv;
	SetParent(Parent);
	
	if (LoadFromResource(IDD_FIND_IN_FILES))
	{
		MoveToCenter();
	}
}

GFindInFiles::~GFindInFiles()
{
	DeleteObj(d);
}

int GFindInFiles::OnNotify(GViewI *Ctrl, int Flags)
{
	return GDialog::OnNotify(Ctrl, Flags);
}
