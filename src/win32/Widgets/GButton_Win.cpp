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

static int IsWin7 = -1;

class GButtonPrivate
{
public:
	DWORD ButtonClassProc;
	bool Toggle;
	bool WantsDefault;
	int64 Value;

	GButtonPrivate()
	{
		Toggle = false;
		WantsDefault = false;
		ButtonClassProc = 0;
		Value = 0;
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
	
	if ((cx < 0 || cy < 0))
	{
		GDisplayString ds(SysFont, ValidStr(name)?name:"1");
		if (cx < 0) cx = ds.X() + Overhead.x;
		if (cy < 0) cy = ds.Y() + Overhead.y;
	}
	
	GRect r(x, y, x+cx-1, y+cy-1);
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
	GWindow *w = GetWindow();
	if (w && w->GetDefault() == (GViewI*)this)
	{
		w->SetDefault(NULL);
	}
	
	DeleteObj(d);
}

bool GButton::GetIsToggle()
{
	return d->Toggle;
}

void GButton::SetIsToggle(bool toggle)
{
	d->Toggle = toggle;
	
	int Tog = BS_PUSHLIKE | BS_AUTOCHECKBOX;
	if (_View)
	{
		DWORD Style = (DWORD)GetWindowLong(_View, GWL_STYLE);
		if (toggle)
			SetWindowLong(_View, GWL_STYLE, Style | Tog);
		else
			SetWindowLong(_View, GWL_STYLE, Style & ~Tog);
	}
	else
	{
		if (toggle)
			SetStyle(GetStyle() | Tog);
		else
			SetStyle(GetStyle() & ~Tog);
	}
}

void GButton::OnAttach()
{
	SetFont(SysFont);
	if (d->WantsDefault && GetWindow())
	{
		GetWindow()->SetDefault(this);
	}
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
		case WM_CREATE:
		{
			if (d->Toggle)
				Value(d->Value);
			break;
		}
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
		case WM_PAINT:
		{
			if (!GetCss())
				break; // Just let the system draw the control

			if (IsWin7 < 0)
			{
				GArray<int> Ver;
				int Os = LgiGetOs(&Ver);
				if (Os == LGI_OS_WIN32 ||
					Os == LGI_OS_WIN64)
					IsWin7 = Ver[0] == 6 && Ver[1] == 1;
				else
					IsWin7 = false;
			}
			if (!IsWin7)
				break;

			// This is the effective background of the parent window:
			GCss::ColorDef bk = GetCss()->NoPaintColor();
		
			// If it's not the default...
			if (!bk.IsValid())
				break;

			// Then create a screen device context for painting
			GScreenDC dc(this);
			
			// Get the control to draw itself into a bitmap
			GRect c = GetPos();
			
			// Create a HBITMAP in the same size as the control 
			// and the same bit depth as the screen
			GMemDC m(c.X(), c.Y(), GdcD->GetColourSpace());
			// Create a HDC for the bitmap
			HDC hdc = m.StartDC();					
			// Ask the control to draw itself into the memory bitmap
			SendMessage(_View, WM_PRINT, (WPARAM)hdc, PRF_ERASEBKGND|PRF_CLIENT);
			// End the HDC
			m.EndDC();

			// Draw correct background
			m.Colour(bk.Rgb32, 32);
			// The outside 1px border (unfilled rect)
			m.Box(0, 0, c.X()-1, c.Y()-1);
			// The 4 pixels at the corners
			m.Set(1, 1);
			m.Set(c.X()-2, 1);
			m.Set(1, c.Y()-2);
			m.Set(c.X()-2, c.Y()-2);

			// Now stick it on the screen
			dc.Blt(0, 0, &m);

			// Skip over calling the parent class' callback procedure.
			return true;
			break;
		}
	}

	GMessage::Result r = GControl::OnEvent(Msg);
	
	return r;
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
	if (!_View)
	{
		// Fall back drawing code
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
		pDC->Colour(GColour::Red);
		GRect c = GetClient();
		pDC->Line(0, 0, c.X()-1, c.Y()-1); 
		pDC->Line(c.X()-1, 0, 0, c.Y()-1); 
	}
}

int64 GButton::Value()
{
	if (d->Toggle)
	{
		if (_View)
			d->Value = SendMessage(Handle(), BM_GETCHECK, 0, 0);
		return d->Value;
	}
	else
	{
		return atoi(Name());
	}
}

void GButton::Value(int64 i)
{
	if (d->Toggle)
	{
		if (_View)
			SendMessage(_View, BM_SETCHECK, i ? BST_CHECKED : BST_UNCHECKED, 0);
		d->Value = i;
	}
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

