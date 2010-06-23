#include <stdlib.h>

#include "Lgi.h"
#include "LgiResEdit.h"
#include "resdefs.h"
#include "GCombo.h"
#include "LgiRes_Dialog.h"
#include "LgiRes_Menu.h"

////////////////////////////////////////////////////////////////////////////////////
// Search window
Search::Search(AppWnd *app)
{
	Text = 0;
	InLang = 0;
	NotInLang = 0;
	Ref = 0;
	CtrlId = 0;
	Define = 0;

	SetParent(App = app);
	if (LoadFromResource(IDD_SEARCH))
	{
		MoveToCenter();
		SetCtrlValue(IDC_IN_TEXT, true);

		List<GLanguage> l;
		
		List<Resource> Res;
		if (App->ListObjects(Res))
		{
			for (Resource *r = Res.First(); r; r = Res.Next())
			{
				ResStringGroup *g = r->IsStringGroup();
				if (g)
				{
					for (int i=0; i<g->GetLanguages(); i++)
					{
						GLanguage *Lang = g->GetLanguage(i);
						if (Lang)
						{
							bool Has = false;
							for (GLanguage *i=l.First(); i; i=l.Next())
							{
								if (i == Lang)
								{
									Has = true;
									break;
								}
							}
							if (!Has)
							{
								l.Insert(Lang);
							}
						}
					}
				}
			}
		}

		GCombo *c;
		if (GetViewById(IDC_LANG, c))
		{
			for (GLanguage *li = l.First(); li; li = l.Next())
			{
				c->Insert(li->Name);
			}
		}
		else
		{
			printf("%s:%i - No combo?\n", __FILE__, __LINE__);
		}
		
		if (GetViewById(IDC_NOT_LANG, c))
		{
			for (GLanguage *li = l.First(); li; li = l.Next())
			{
				c->Insert(li->Name);
			}
		}
		else
		{
			printf("%s:%i - No combo?\n", __FILE__, __LINE__);
		}

		OnCheck();
		
		GViewI *v = FindControl(IDC_TEXT);
		if (v) v->Focus(true);
	}
}

Search::~Search()
{
	DeleteArray(Text);
	DeleteArray(Define);
}

void Search::OnCheck()
{
	SetCtrlEnabled(IDC_TEXT, GetCtrlValue(IDC_IN_TEXT));
	SetCtrlEnabled(IDC_DEFINE, GetCtrlValue(IDC_IN_DEFINES));
	SetCtrlEnabled(IDC_LANG, GetCtrlValue(IDC_IN_LANG));
	SetCtrlEnabled(IDC_NOT_LANG, GetCtrlValue(IDC_NOT_IN_LANG));
	SetCtrlEnabled(IDC_REF, GetCtrlValue(IDC_IN_REF));
	SetCtrlEnabled(IDC_CTRL, GetCtrlValue(IDC_IN_CTRL));
}

int Search::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_IN_TEXT:
		case IDC_IN_DEFINES:
		case IDC_IN_LANG:
		case IDC_NOT_IN_LANG:
		case IDC_IN_REF:
		case IDC_IN_CTRL:
		{
			OnCheck();

			#define SetCtrlFocus(id) { GViewI *v = FindControl(id); if (v) v->Focus(true); }
			switch (c->GetId())
			{
				case IDC_IN_TEXT:
					SetCtrlFocus(IDC_TEXT);
					break;
				case IDC_IN_DEFINES:
					SetCtrlFocus(IDC_DEFINE);
					break;
				case IDC_IN_LANG:
					SetCtrlFocus(IDC_LANG);
					break;
				case IDC_NOT_IN_LANG:
					SetCtrlFocus(IDC_NOT_LANG);
					break;
				case IDC_IN_REF:
					SetCtrlFocus(IDC_REF);
					break;
				case IDC_IN_CTRL:
					SetCtrlFocus(IDC_CTRL);
					break;
			}
			break;
		}
		case IDOK:
		{
			if (GetCtrlValue(IDC_IN_TEXT))
			{
				char *t = GetCtrlName(IDC_TEXT);
				if (ValidStr(t))
					Text = NewStr(t);
			}

			if (GetCtrlValue(IDC_IN_DEFINES))
			{
				char *t = GetCtrlName(IDC_DEFINE);
				if (ValidStr(t))
					Define = NewStr(t);
			}
			
			if (GetCtrlValue(IDC_IN_LANG))
			{
				GLanguage *l = GFindLang(0, GetCtrlName(IDC_LANG));
				if (l) InLang = l->Id;
			}
			
			if (GetCtrlValue(IDC_NOT_IN_LANG))
			{
				GLanguage *l = GFindLang(0, GetCtrlName(IDC_NOT_LANG));
				if (l) NotInLang = l->Id;
			}
			
			if (GetCtrlValue(IDC_IN_REF) && GetCtrlName(IDC_REF))
			{
				Ref = GetCtrlValue(IDC_REF);
			}

			if (GetCtrlValue(IDC_IN_CTRL) && GetCtrlName(IDC_CTRL))
			{
				CtrlId = GetCtrlValue(IDC_CTRL);
			}
			// fall thru			
		}
		case IDCANCEL:
		{
			EndModal(c->GetId() == IDOK);
			break;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// Results window
class Result : public GListItem
{
public:
	AppWnd *App;
	ResStringGroup *Grp;
	ResMenuItem *Menu;
	ResDialog *Dialog;
	ResDialogCtrl *Ctrl;
	ResString *Str;
	GLanguageId Lang;
	
	Result(AppWnd *app, ResString *s, GLanguageId l = 0)
	{
		App = app;
		Grp = 0;
		Menu = 0;
		Dialog = 0;
		Ctrl = 0;
		Str = s;
		Lang = l;
	}
	
	char *GetText(int i)
	{
		switch (i)
		{
			case 0:
			{
				if (Menu) return "MenuItem";
				else if (Dialog) return "Control";
				return "String";
				break;
			}
			case 1:
			{
				return Str ? Str->GetDefine() : 0;
				break;
			}
			case 2:
			{
				if (Str)
				{
					return Str->Get(Lang);
				}
				break;
			}
			case 3:
			{
				return Lang;
				break;
			}
		}
		
		return 0;
	}	

	void OnMouseClick(GMouse &m)
	{
		if (App && m.Left() && m.Down())
		{
			App->GotoObject(Str, Grp, Dialog, Menu, Ctrl);
		}
	}
};

class ResultsPrivate
{
public:
	AppWnd *App;
	char *Text;
	char *Define;
	GLanguageId InLang;
	GLanguageId NotInLang;
	int RefId;
	int CtrlId;
	bool Searching;
	bool Running;
	GList *Lst;

	bool HasContent(char *s)
	{
		while (s && *s)
		{
			if (((*s & 0x80) != 0) || isalpha(*s))
			{
				return true;
			}
			*s++;
		}

		return false;
	}

	Result *Test(ResString *s)
	{
		Result *r = 0;
		
		if (s)
		{
			if (ValidStr(Text))
			{
				if (InLang)
				{
					char *Txt = s->Get(InLang);
					if (Txt && stristr(Txt, Text))
					{
						r = new Result(App, s, InLang);
					}
				}
				else
				{
					for (StrLang *t=s->Items.First(); t; t=s->Items.Next())
					{
						if (t->GetStr() && stristr(t->GetStr(), Text))
						{
							r = new Result(App, s, t->GetLang());
							break;
						}
					}
				}
			}
			if (ValidStr(Define) && s->GetDefine())
			{
				if (stristr(s->GetDefine(), Define))
				{
					r = new Result(App, s);
				}
			}
			if (!r && NotInLang)
			{
				if (s->Items.Length() > 0 &&
					!s->Get(NotInLang) &&
					HasContent(s->Get("en")))
				{
					r = new Result(App, s, NotInLang);
				}
			}
			if (!r && RefId)
			{
				if (s->GetRef() == RefId)
				{
					r = new Result(App, s);
				}
			}
			if (!r && CtrlId)
			{
				if (s->Id == CtrlId)
				{
					r = new Result(App, s);
				}
			}
		}
		return r;
	}
};

Results::Results(AppWnd *app, Search *params)
{
	d = new ResultsPrivate;
	d->App = app;
	d->Text = NewStr(params->Text);
	d->Define = NewStr(params->Define);
	d->InLang = params->InLang;
	d->NotInLang = params->NotInLang;
	d->RefId = params->Ref;
	d->CtrlId = params->CtrlId;
	d->Searching = false;
	d->Running = false;
	d->Lst = 0;
	
	GLgiRes r;
	GRect p;
	char n[256] = "";
	if (r.LoadFromResource(IDD_RESULTS, Children, &p, n))
	{
		SetPos(p);
		MoveToCenter();
		if (n[0]) Name(n);

		if (Attach(0))
		{
			AttachChildren();
			Visible(true);

			GetViewById(IDC_LIST, d->Lst);
			
			List<Resource> Res;
			d->App->ListObjects(Res);
			d->Searching = true;
			for (Resource *r=Res.First(); r && d->Searching; r=Res.Next())
			{
				if (r->IsStringGroup())
				{
					List<ResString>::I it = r->IsStringGroup()->GetStrs()->Start();
					for (ResString *s = *it; s && d->Searching; s = *++it)
					{
						Result *Res = d->Test(s);
						if (Res)
						{
							Res->Grp = r->IsStringGroup();
							d->Lst->Insert(Res);
						}
						
						LgiYield();
					}
				}
				else if (r->IsDialog())
				{
					List<ResDialogCtrl> Ctrls;
					r->IsDialog()->EnumCtrls(Ctrls);
					for (ResDialogCtrl *c = Ctrls.First(); c && d->Searching; c = Ctrls.Next())
					{
						Result *Res = d->Test(c->Str);
						if (Res)
						{
							Res->Ctrl = c;
							Res->Dialog = r->IsDialog();
							d->Lst->Insert(Res);
						}
						
						LgiYield();
					}
				}
				else if (r->IsMenu())
				{
					List<ResMenuItem> Items;
					r->IsMenu()->EnumItems(Items);
					for (ResMenuItem *c = Items.First(); c && d->Searching; c = Items.Next())
					{
						Result *Res = d->Test(&c->Str);
						if (Res)
						{
							Res->Menu = c;
							d->Lst->Insert(Res);
						}
						
						LgiYield();
					}
				}
			}
			d->Searching = false;
		}
	}
	
	d->Running = true;
}

Results::~Results()
{
	DeleteArray(d->Text);
	DeleteArray(d->Define);
	DeleteObj(d);
}

void Results::OnPosChange()
{
	GRect Client = GetClient();

	GViewI *v;
	if (GetViewById(IDC_LIST, v))
	{
		GRect r = v->GetPos();
		r.x2 = Client.x2 - 12;
		r.y2 = Client.y2 - 40;
		v->SetPos(r);

		v = FindControl(IDOK);
		if (v)
		{
			GRect p = v->GetPos();
			p.Offset(Client.x2 - p.X() - 12 - p.x1, r.y2 + 10 - p.y1);
			v->SetPos(p);
		}
	}
}

int Results::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDOK:
		{
			if (d->Searching)
			{
				d->Searching = false;
			}
			else if (d->Running)
			{
				Quit();
			}
			break;
		}
	}

	return 0;
}
