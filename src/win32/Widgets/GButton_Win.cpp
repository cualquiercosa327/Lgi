// \file
// \author Matthew Allen
// \brief Native Win32 Button Class

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GButton.h"

// Size of extra pixels, beyond the size of the text itself.
GdcPt2 GButton::Overhead =
    GdcPt2(
        // Extra width needed
        #if defined(MAC) && !defined(LGI_SDL)
        28,
        #else
        16,
        #endif
        // Extra height needed
        6
    );

class GButtonPrivate
{
public:
	DWORD ButtonClassProc;
	bool Toggle;
	bool WantsDefault;

	GButtonPrivate()
	{
		Toggle = false;
		WantsDefault = false;
		ButtonClassProc = 0;
	}

	~GButtonPrivate()
	{
	}
};

GButton::GButton(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Button)
{
	d = new GButtonPrivate;
	Name(name);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	
	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = GWin32Class::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LgiAssert(!"No subclass?");
}

GButton::~GButton()
{
	DeleteObj(d);
}

bool GButton::GetIsToggle()
{
	return d->Toggle;
}

void GButton::SetIsToggle(bool toggle)
{
	d->Toggle = toggle;
}

void GButton::OnAttach()
{
	SetFont(SysFont);
}

bool GButton::Default()
{
	if (_View)
	{
		DWORD Style = (DWORD)GetWindowLong(_View, GWL_STYLE);
		return Style & BS_DEFPUSHBUTTON;
	}
	
	return d->WantsDefault;
}

void GButton::Default(bool b)
{
	d->WantsDefault = b;
	
	if (_View)
	{
		DWORD Style = (DWORD)GetWindowLong(_View, GWL_STYLE);
		if (b)
			Style |= BS_DEFPUSHBUTTON;
		else
			Style &= ~BS_DEFPUSHBUTTON;
		SetWindowLong(_View, GWL_STYLE, Style);
	}
}

bool GButton::Name(const char *n)
{
	return GView::Name(n);
}

bool GButton::NameW(const char16 *n)
{
	return GView::NameW(n);
}

void GButton::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
}

int GButton::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED)
	{
		OnClick();
	}
	
	return 0;
}

GMessage::Result GButton::OnEvent(GMessage *Msg)
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

	return GControl::OnEvent(Msg);
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
	switch (k.vkey)
	{
		case VK_ESCAPE:
		{
			if (GetId() != IDCANCEL)
			{
				break;
			}
			// else fall thru
		}
		case VK_RETURN:
		{
			if (!k.IsChar)
			{
				if (k.Down())
					OnClick();
				return true;
			}
			break;
		}
	}
	
	return false;
}

void GButton::OnFocus(bool f)
{
}

void GButton::OnPaint(GSurface *pDC)
{
}

int64 GButton::Value()
{
	return atoi(Name());
}

void GButton::Value(int64 i)
{
}

void GButton::OnClick()
{
	int Id = GetId();
	if (Id)
	{
		GViewI *n = GetNotify();
		GViewI *p = GetParent();
		GViewI *target = n ? n : p;
		if (target)
		{
			if (Handle())
			{
				target->PostEvent(M_CHANGE, (GMessage::Param)Id);
			}
			else if (InThread())
			{
				target->OnNotify(this, 0);
			}
			else
			{
				LgiAssert(!"Out of thread and no handle?");
			}
		}
	}
	else
		SendNotify();
}

