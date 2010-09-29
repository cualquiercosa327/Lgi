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
	int InitState;
	DWORD ButtonClassProc;

	GCheckBoxPrivate()
	{
		InitState = 0;
		ButtonClassProc = 0;
	}

	~GCheckBoxPrivate()
	{
	}
};

GCheckBox::GCheckBox(int id, int x, int y, int cx, int cy, char *name, int initstate) :
	ResObject(Res_CheckBox)
{
	d = new GCheckBoxPrivate;
	d->InitState = initstate;

	Name(name);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_AUTOCHECKBOX);
	SetClass("BUTTON");
}

GCheckBox::~GCheckBox()
{
	DeleteObj(d);
}

void GCheckBox::OnAttach()
{
	#ifdef SKIN_MAGIC

	d->ButtonClassProc = GetWindowLong(Handle(), GWL_WNDPROC);
	SetWindowLong(Handle(), GWL_WNDPROC, (DWORD)GWin32Class::Redir);
	SetWindowLong(Handle(), GWL_USERDATA, (DWORD)(GViewI*)this);

	#endif

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

bool GCheckBox::Name(char *n)
{
	return GView::Name(n);
}

bool GCheckBox::NameW(char16 *n)
{
	return GView::NameW(n);
}

void GCheckBox::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
}

int GCheckBox::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_GETDLGCODE:
		{
			return CallWindowProc((WNDPROC)d->ButtonClassProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg)) |
				DLGC_WANTTAB;
		}
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

	return CallWindowProc((WNDPROC)d->ButtonClassProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg));
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

int GCheckBox::Value()
{
	if (Handle())
	{
		return SendMessage(Handle(), BM_GETCHECK, 0, 0);
	}

	return d->InitState;
}

void GCheckBox::Value(int i)
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

