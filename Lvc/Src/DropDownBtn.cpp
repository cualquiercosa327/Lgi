#include "Lgi.h"
#include "Lvc.h"

struct DropDownBtnPriv
{
	DropDownBtn *View;
	int EditId;

	DropDownBtnPriv(DropDownBtn *view)
	{
		View = view;
		EditId = -1;
	}

	GViewI *GetEdit()
	{
		GViewI *e = NULL;
		if (View && View->GetParent())
			View->GetParent()->GetViewById(EditId, e);
		LgiAssert(e != 0);
		return e;
	}
};

class DropLst : public GPopup
{
public:
	DropDownBtnPriv *d;
	LList *Lst;

	DropLst(DropDownBtnPriv *priv, GView *owner) : GPopup(owner)
	{
		d = priv;

		int x = 300;
		GRect r(0, 0, x, 200);
		SetPos(r);
		AddView(Lst = new LList(10));
		Lst->Sunken(false);
		Lst->ShowColumnHeader(false);
		Lst->AddColumn("", x);
	}

	void OnPosChange()
	{
		GRect c = GetClient();
		c.Size(1, 1);
		if (Lst)
			Lst->SetPos(c);
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(GColour::Black);
		pDC->Box();
	}

	int OnNotify(GViewI *c, int flag)
	{
		if (c->GetId() == 10 &&
			flag == GNotifyItem_Click)
		{
			LListItem *i = Lst->GetSelected();
			GViewI *e = d->GetEdit();
			if (e && i)
			{
				e->Name(i->GetText(0));
			}

			Visible(false);
		}

		return 0;
	}
};

DropDownBtn::DropDownBtn() :
	GDropDown(-1, 0, 0, 100, 24, NULL),
	ResObject(Res_Custom)
{
	d = new DropDownBtnPriv(this);
	d->EditId = -1;
	SetPopup(Pu = new DropLst(d, this));
}

DropDownBtn::~DropDownBtn()
{
	DeleteObj(d);
}

GString::Array DropDownBtn::GetList()
{
	GString::Array a;
	if (Pu && Pu->Lst)
	{
		for (LListItem *i = NULL; Pu->Lst->Iterate(i); )
		{
			a.New() = i->GetText(0);
		}
	}
	return a;
}

bool DropDownBtn::OnLayout(GViewLayoutInfo &Inf)
{
	if (Inf.Width.Min)
	{
		// Vertical layout
		Inf.Height.Min = Inf.Height.Max = 23;
	}
	else
	{
		// Horizontal layout
		Inf.Width.Min = Inf.Width.Max = 23;
	}
	return true;
}

bool DropDownBtn::SetList(int EditCtrl, GString::Array a)
{
	if (!Pu || !Pu->Lst)
		return false;
	Pu->Lst->Empty();
	d->EditId = EditCtrl;
	for (GString *s = NULL; a.Iterate(s); )
	{
		LListItem *i = new LListItem;
		i->SetText(*s);
		Pu->Lst->Insert(i);
	}

	return true;
}

class DropDownBtnFactory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (!stricmp(Class, "DropDownBtn"))
			return new DropDownBtn;
		return NULL;
	}

}	DropDownBtnFactoryInst;