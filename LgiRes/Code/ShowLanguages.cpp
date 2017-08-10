#include "Lgi.h"
#include "LgiResEdit.h"
#include "LList.h"
#include "GButton.h"
#include "LListItemCheckBox.h"

class ShowLanguagesDlgPriv
{
public:
	AppWnd *App;
	LList *Lst;
};

class Lang : public LListItem
{
	GLanguage *L;
	int Index;
	LListItemCheckBox *Val;

public:
	Lang(AppWnd *App, GLanguage *l, int i)
	{
		L = l;
		Index = i;
		SetText(l->Id, 1);
		SetText(l->Name, 2);
		Val = new LListItemCheckBox(this, 0, App->ShowLang(L->Id));
	}

	GLanguage *GetLang()
	{
		return L;
	}

	int GetVal()
	{
		return Val->Value();
	}
};

int Cmp(LListItem *a, LListItem *b, NativeInt d)
{
	return stricmp(a->GetText(2), b->GetText(2));
}

ShowLanguagesDlg::ShowLanguagesDlg(AppWnd *app)
{
	d = new ShowLanguagesDlgPriv;
	SetParent(d->App = app);

	GRect r(0, 0, 300, 500);
	SetPos(r);
	MoveToCenter();
	Name("Show Languages");
	r = GetClient();
	Children.Insert(d->Lst = new LList(100, 10, 10, r.X() - 20, r.Y() - 50));
	Children.Insert(new GButton(IDOK, r.X() - 140, r.Y() - 30, 60, 20, "Ok"));
	Children.Insert(new GButton(IDCANCEL, r.X() - 70, r.Y() - 30, 60, 20, "Cancel"));

	if (d->Lst)
	{
		d->Lst->AddColumn("Show", 40);
		d->Lst->AddColumn("Id", 40);
		d->Lst->AddColumn("Language", 180);

		for (int i=0; i<d->App->GetLanguages()->Length(); i++)
		{
			GLanguage *L = (*d->App->GetLanguages())[i];
			if (L)
			{
				d->Lst->Insert(new Lang(d->App, L, i));
			}
		}

		d->Lst->Sort(Cmp, 0);
		d->Lst->Select(*d->Lst->Start());
	}
	
	DoModal();
}

ShowLanguagesDlg::~ShowLanguagesDlg()
{
	DeleteObj(d);
}

int ShowLanguagesDlg::OnNotify(GViewI *n, int f)
{
	switch (n->GetId())
	{
		case IDOK:
		{
			if (d->Lst)
			{
				List<Lang> All;
				d->Lst->GetAll(All);
				for (Lang *L = All.First(); L; L = All.Next())
				{
					d->App->ShowLang(L->GetLang()->Id, L->GetVal());
				}

				d->App->OnLanguagesChange(0, 0, true);
			}
		}
		case IDCANCEL:
		{
			EndModal(n->GetId() == IDOK);
			break;
		}
	}

	return false;
}

