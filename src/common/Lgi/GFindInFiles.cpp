#include "Lgi.h"
#include "GFindInFiles.h"
#include "GPopup.h"
#include "LList.h"
#include "GButton.h"
#include "GTableLayout.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GXmlTreeUi.h"
#include "GStringClass.h"

enum Ctrls
{
	IDC_STATIC = -1,
	IDC_TABLE = 100,
	IDC_SEARCH,
	IDC_SEARCH_HISTORY,
	IDC_WHERE,
	IDC_WHERE_BROWSE,
	IDC_WHERE_HISTORY,
	IDC_INC_SUB_FOLDERS,
	IDC_MATCH_CASE,
	IDC_FILE_TYPES,
	IDC_MATCH_WHOLE_WORD,
	IDC_TYPES_HISTORY
};

class GHistory;
class GHistoryPopup : public GPopup
{
	GHistory *History;
	LList *Lst;

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
	GString All;
	GString::Array Strs;
	
public:
	GHistory(int Id) :
		GDropDown(Id, 0, 0, 80, 20, 0),
		ResObject(Res_Custom)
	{
		SetPopup(new GHistoryPopup(this));
	}
	
	void Add(const char *s)
	{
		if (!s)
			return;
		for (int i=0; i<Strs.Length(); i++)
		{
			if (!_stricmp(s, Strs[i]))
				return;
		}
		Strs.New() = s;
	}
	
	bool OnLayout(GViewLayoutInfo &Inf)
	{
	    if (!Inf.Width.Max)
	    {
	        Inf.Width.Min =
	            Inf.Width.Max = (int) (SysFont->GetHeight() * 1.6);
	    }
	    else if (!Inf.Height.Max)
	    {
	        Inf.Height.Min =
                Inf.Height.Max = GButton::Overhead.y + SysFont->GetHeight();
	    }
	    else return false;

	    return true;
	}
	
	char *Name()
	{
		GString Sep("\t");
		All = Sep.Join(Strs);
		return All;
	}
	
	bool Name(const char *s)
	{
		All = s;
		Strs = All.SplitDelimit("\t");
		return true;
	}
};

GHistoryPopup::GHistoryPopup(GHistory *h) : GPopup(h)
{
	History = h;
	
	GRect r(0, 0, 300, 200);
	SetPos(r);
	r.Size(1, 1);
	AddView(Lst = new LList(100, r.x1, r.y1, r.X()-1, r.Y()-1));
	Lst->Sunken(false);
	Lst->ShowColumnHeader(false);
	
	for (int i=0; i<History->Strs.Length(); i++)
	{
		LListItem *it = new LListItem;
		it->SetText(History->Strs[i]);
		Lst->Insert(it);
	}
}

struct GFindInFilesPriv : public GXmlTreeUi
{
	GTableLayout *Tbl;
	GAutoString Search;
	GDom *Store;
	GHistory *SearchHistory, *WhereHistory, *TypesHistory;

	GFindInFilesPriv()
	{
		Tbl = NULL;
		Store = NULL;
		
		Map("fif_search", IDC_SEARCH);
		Map("fif_search_history", IDC_SEARCH_HISTORY);
		Map("fif_where", IDC_WHERE);
		Map("fif_where_history", IDC_WHERE_HISTORY);
		Map("fif_inc_sub", IDC_INC_SUB_FOLDERS);
		Map("fif_match_case", IDC_MATCH_CASE);
		Map("fif_match_word", IDC_MATCH_WHOLE_WORD);
		Map("fif_types", IDC_FILE_TYPES);
		Map("fif_types_history", IDC_TYPES_HISTORY);
	}
	
	~GFindInFilesPriv()
	{
	}
};
	
GFindInFiles::GFindInFiles(GViewI *Parent, GAutoString Search, GDom *Store)
{
	d = new GFindInFilesPriv;
	d->Search = Search;
	d->Store = Store;
	SetParent(Parent);
	GRect r(7, 7, 450, 343);
	SetPos(r);

	AddView(d->Tbl = new GTableLayout(IDC_TABLE));

	int y = 0;
	int cols = 3;
	GView *v;

	Name("Find In Files");
	
	GLayoutCell *c = d->Tbl->GetCell(0, y, true, cols);
	c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Find what:"));

	c = d->Tbl->GetCell(0, ++y, true, cols-1);
	c->Add(v = new GEdit(IDC_SEARCH, 0, 0, 60, 20));
	v->Focus(true);
	c = d->Tbl->GetCell(2, y);
	c->Add(d->SearchHistory = new GHistory(IDC_SEARCH_HISTORY));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Look in:"));

	c = d->Tbl->GetCell(0, ++y);
	c->Add(new GEdit(IDC_WHERE, 0, 0, 60, 20));
	c = d->Tbl->GetCell(1, y);
	c->Add(new GButton(IDC_WHERE_BROWSE, 0, 0, 20, 20, "..."));
	c = d->Tbl->GetCell(2, y);
	c->Add(d->WhereHistory = new GHistory(IDC_WHERE_HISTORY));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->PaddingTop(GCss::Len("0.5em"));
	c->Add(new GCheckBox(IDC_INC_SUB_FOLDERS, 0, 0, -1, -1, "Include sub-folders"));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->Add(new GCheckBox(IDC_MATCH_CASE, 0, 0, -1, -1, "Match case"));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->Add(new GCheckBox(IDC_MATCH_WHOLE_WORD, 0, 0, -1, -1, "Match whole word"));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->PaddingTop(GCss::Len("1em"));
	c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Look in these file types:"));

	c = d->Tbl->GetCell(0, ++y, true, cols-1);
	c->Add(new GEdit(IDC_FILE_TYPES, 0, 0, 60, 20));
	c = d->Tbl->GetCell(cols-1, y);
	c->Add(d->TypesHistory = new GHistory(IDC_TYPES_HISTORY));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->TextAlign(GCss::AlignRight);
	c->Add(new GButton(IDOK, 0, 0, 60, 20, "Search"));
	c->Add(new GButton(IDCANCEL, 0, 0, 60, 20, "Cancel"));

	d->Convert(d->Store, this, true);
	MoveToCenter();
}

GFindInFiles::~GFindInFiles()
{
	d->Convert(d->Store, this, false);
	DeleteObj(d);
}

int GFindInFiles::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_WHERE_BROWSE:
		{
			GFileSelect s;
			s.Parent(this);
			if (s.Open())
			{
				GFile::Path p = s.Name();
				if (p.IsFile())
					p--;
				SetCtrlName(IDC_WHERE, p.GetFull());
			}
			break;
		}
		case IDOK:
		{
			// Fall thru
			d->SearchHistory->Add(GetCtrlName(IDC_SEARCH));
			d->WhereHistory->Add(GetCtrlName(IDC_WHERE));
			d->TypesHistory->Add(GetCtrlName(IDC_FILE_TYPES));
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId() == IDOK);
			break;
		}
	}

	return GDialog::OnNotify(Ctrl, Flags);
}
