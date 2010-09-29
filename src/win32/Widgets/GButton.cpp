// \file
// \author Matthew Allen
// \brief Native Win32 Button Class

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GButton.h"

class GButtonPrivate
{
public:
	DWORD ButtonClassProc;

	GButtonPrivate()
	{
		ButtonClassProc = 0;
	}

	~GButtonPrivate()
	{
	}
};

GButton::GButton(int id, int x, int y, int cx, int cy, char *name) :
	ResObject(Res_Button)
{
	d = new GButtonPrivate;
	Name(name);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	
	#ifdef SKIN_MAGIC
	SetClass("BUTTON");
	#else
	SetClass(ClassName);
	d->Class->SubClass("BUTTON");
	#endif
}

GButton::~GButton()
{
	DeleteObj(d);
}

void GButton::OnAttach()
{
	#ifdef SKIN_MAGIC

	d->ButtonClassProc = GetWindowLong(Handle(), GWL_WNDPROC);
	SetWindowLong(Handle(), GWL_WNDPROC, (DWORD)GWin32Class::Redir);
	SetWindowLong(Handle(), GWL_USERDATA, (DWORD)(GViewI*)this);

	#endif

	SetFont(SysFont);
}

bool GButton::Default()
{
	return false;
}

void GButton::Default(bool b)
{
}

bool GButton::Name(char *n)
{
	return GView::Name(n);
}

bool GButton::NameW(char16 *n)
{
	return GView::NameW(n);
}

void GButton::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
}

int GButton::OnEvent(GMessage *Msg)
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

void GButton::OnMouseClick(GMouse &m)
{
}

void GButton::OnMouseEnter(GMouse &m)
{
}

void GButton::OnMouseExit(GMouse &m)
{
}

bool GButton::OnKey(GKey &k)
{
	return false;
}

void GButton::OnFocus(bool f)
{
}

void GButton::OnPaint(GSurface *pDC)
{
}

int GButton::Value()
{
	return atoi(Name());
}

void GButton::Value(int i)
{
}

