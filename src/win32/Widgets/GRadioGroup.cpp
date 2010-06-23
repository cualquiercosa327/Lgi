// \file
// \author Matthew Allen
// \brief Native Win32 Radio Group and Button

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GRadioGroup.h"

///////////////////////////////////////////////////////////////////////////////////////////
// Radio group
class GRadioGroupPrivate
{
public:
	static int NextId;
	int InitVal;
	DWORD ParentProc;

	GRadioGroupPrivate()
	{
		InitVal = 0;
		ParentProc = 0;
	}

	~GRadioGroupPrivate()
	{
	}
};

int GRadioGroupPrivate::NextId = 10000;

GRadioGroup::GRadioGroup(int id, int x, int y, int cx, int cy, char *name, int Init)
	: ResObject(Res_Group)
{
	d = NEW(GRadioGroupPrivate);
	d->InitVal = Init;

	Name(name);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetClass("BUTTON");
	SetStyle(GetStyle() | BS_GROUPBOX | WS_GROUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
}

GRadioGroup::~GRadioGroup()
{
	DeleteObj(d);
}

void GRadioGroup::OnAttach()
{
	#ifdef SKIN_MAGIC

	d->ParentProc = GetWindowLong(Handle(), GWL_WNDPROC);
	SetWindowLong(Handle(), GWL_WNDPROC, (DWORD)GWin32Class::Redir);
	SetWindowLong(Handle(), GWL_USERDATA, (DWORD)(GViewI*)this);

	#endif

	SetFont(SysFont);
	
	for (int i=0; i<Children.GetItems(); i++)
	{
		GView *c = Children[i];
		if (c)
		{
			Children.Delete(c);

			GRect r = c->GetPos();
			r.Offset(GetPos().x1, GetPos().y1);
			c->SetPos(r);

			c->Attach(GetParent());
			GetParent()->Children.Delete(c);
			Children.Insert(c, i);
			c->SetParent(this);
		}
	}

	Value(d->InitVal);
}

int GRadioGroup::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_DESTROY:
		{
			d->InitVal = Value();
			break;
		}
	}

	return CallWindowProc((WNDPROC)d->ParentProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg));
}

bool GRadioGroup::Name(char *n)
{
	return GView::Name(n);
}

bool GRadioGroup::NameW(char16 *n)
{
	return GView::NameW(n);
}

void GRadioGroup::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt);
}

void GRadioGroup::OnCreate()
{
}

int GRadioGroup::Value()
{
	if (Handle())
	{
		int n = 0;
		for (GView *c = Children.First(); c; c = Children.Next())
		{
			GRadioButton *Check = dynamic_cast<GRadioButton*>(c);
			if (Check)
			{
				if (Check->Value())
				{
					return n;
				}
				n++;
			}
		}

		return -1;
	}
	else
	{
		return d->InitVal;
	}
}

void GRadioGroup::Value(int Which)
{
	if (Handle())
	{
		int n = 0;
		for (GView *c = Children.First(); c; c = Children.Next())
		{
			GRadioButton *Check = dynamic_cast<GRadioButton*>(c);
			if (Check)
			{
				Check->Value(n == Which);
				n++;
			}
		}
	}
	else
	{
		d->InitVal = Which;
	}
}

int GRadioGroup::OnNotify(GViewI *Ctrl, int Flags)
{
	GView *n = GetNotify() ? GetNotify() : GetParent();
	if (n)
	{
		if (dynamic_cast<GRadioButton*>(Ctrl))
		{
			return n->OnNotify(this, Flags);
		}
		else
		{
			return n->OnNotify(Ctrl, Flags);
		}
	}
	return 0;
}

void GRadioGroup::OnPaint(GSurface *pDC)
{
}

GRadioButton *GRadioGroup::Append(int x, int y, char *name)
{
	GRadioButton *But = NEW(GRadioButton(d->NextId++, x, y, -1, -1, name));
	if (But)
	{
		Children.Insert(But);
	}

	return But;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Radio button
class GRadioButtonPrivate
{
public:
	DWORD ParentProc;
	int InitVal;

	GRadioButtonPrivate()
	{
		ParentProc = 0;
		InitVal = 0;
	}

	~GRadioButtonPrivate()
	{
	}
};

GRadioButton::GRadioButton(int id, int x, int y, int cx, int cy, char *name)
	: ResObject(Res_RadioBox)
{
	d = NEW(GRadioButtonPrivate);

	Name(name);

	GDisplayString t(SysFont, name);
	if (cx < 0) cx = t.X() + 26;
	if (cy < 0) cy = t.Y() + 4;

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetClass("BUTTON");
	SetStyle(GetStyle() | BS_AUTORADIOBUTTON);
}

GRadioButton::~GRadioButton()
{
	DeleteObj(d);
}

void GRadioButton::OnAttach()
{
	#ifdef SKIN_MAGIC

	d->ParentProc = GetWindowLong(Handle(), GWL_WNDPROC);
	SetWindowLong(Handle(), GWL_WNDPROC, (DWORD)GWin32Class::Redir);
	SetWindowLong(Handle(), GWL_USERDATA, (DWORD)(GViewI*)this);

	#endif

	SetFont(SysFont);
	Value(d->InitVal);
}

int GRadioButton::OnEvent(GMessage *Msg)
{
	return CallWindowProc((WNDPROC)d->ParentProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg));
}

bool GRadioButton::Name(char *n)
{
	return GView::Name(n);
}

bool GRadioButton::NameW(char16 *n)
{
	return GView::NameW(n);
}

void GRadioButton::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
}

int GRadioButton::Value()
{
	if (Handle())
	{
		return SendMessage(Handle(), BM_GETCHECK, 0, 0);
	}

	return d->InitVal;
}

void GRadioButton::Value(int i)
{
	if (Handle())
	{
		SendMessage(Handle(), BM_SETCHECK, i, 0);
	}
	else
	{
		d->InitVal = i;
	}
}

void GRadioButton::OnMouseClick(GMouse &m)
{
}

void GRadioButton::OnMouseEnter(GMouse &m)
{
}

void GRadioButton::OnMouseExit(GMouse &m)
{
}

bool GRadioButton::OnKey(GKey &k)
{
	return false;
}

void GRadioButton::OnFocus(bool f)
{
}

void GRadioButton::OnPaint(GSurface *pDC)
{
}

