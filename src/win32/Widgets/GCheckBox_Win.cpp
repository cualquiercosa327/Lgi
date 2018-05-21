// \file
// \author Matthew Allen
// \brief Native Win32 Check Box Class

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GCheckBox.h"

class GCheckBoxPrivate
{
public:
	int64 InitState;

	GCheckBoxPrivate()
	{
		InitState = 0;
	}

	~GCheckBoxPrivate()
	{
	}
};

GCheckBox::GCheckBox(int id, int x, int y, int cx, int cy, const char *name, int initstate) :
	ResObject(Res_CheckBox)
{
	d = new GCheckBoxPrivate;
	d->InitState = initstate;

	Name(name);
	
	if (cx < 0 || cy < 0)
	{
		GDisplayString ds(SysFont, name);
		if (cx < 0)
			cx = 18 + ds.X();
		if (cy < 0)
			cy = ds.Y();
	}
	
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_AUTOCHECKBOX);

	#if 1
	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = GWin32Class::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LgiAssert(!"No subclass?");
	#else
	SetClassW32("BUTTON");
	#endif
}

GCheckBox::~GCheckBox()
{
	DeleteObj(d);
}

void GCheckBox::OnAttach()
{
	SetFont(SysFont);
	Value(d->InitState);
}

bool GCheckBox::ThreeState()
{
	return TestFlag(GetStyle(), BS_3STATE);
}

void GCheckBox::ThreeState(bool t)
{
	if (t)
	{
		SetStyle(GetStyle() | BS_AUTO3STATE);
	}
	else
	{
		SetStyle(GetStyle() & ~BS_AUTO3STATE);
	}
}

bool GCheckBox::Name(const char *n)
{
	return GView::Name(n);
}

bool GCheckBox::NameW(const char16 *n)
{
	return GView::NameW(n);
}

void GCheckBox::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
}

int GCheckBox::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED)
	{
		SendNotify(Value());
	}
	
	return 0;
}

int GCheckBox::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl == (GViewI*)this && Flags == GNotify_Activate)
	{
		Value(!Value());
	}

	return 0;
}

GMessage::Result GCheckBox::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			if (SysOnKey(this, Msg))
			{
				return 0;
			}
			break;
		}
	}

	return GControl::OnEvent(Msg);
}

void GCheckBox::OnMouseClick(GMouse &m)
{
}

void GCheckBox::OnMouseEnter(GMouse &m)
{
}

void GCheckBox::OnMouseExit(GMouse &m)
{
}

bool GCheckBox::OnKey(GKey &k)
{
	return false;
}

void GCheckBox::OnFocus(bool f)
{
}

void GCheckBox::OnPaint(GSurface *pDC)
{
}

int64 GCheckBox::Value()
{
	if (Handle())
	{
		return SendMessage(Handle(), BM_GETCHECK, 0, 0);
	}

	return d->InitState;
}

void GCheckBox::Value(int64 i)
{
	if (Handle())
	{
		SendMessage(Handle(), BM_SETCHECK, i, 0);
	}
	else
	{
		d->InitState = i;
	}
}

bool GCheckBox::OnLayout(GViewLayoutInfo &Inf)
{
	if (Inf.Width.Max)
	{
		int y = GetFont()->GetHeight();
		Inf.Height.Min = y;
		Inf.Height.Max = y;
	}
	else
	{
		GDisplayString s(GetFont(), Name());
		int x = s.X() + 32;
		Inf.Width.Min = x;
		Inf.Width.Max = x;
	}

	return true;
}

void GCheckBox::OnPosChange()
{
}
