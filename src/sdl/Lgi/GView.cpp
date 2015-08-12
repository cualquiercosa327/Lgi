
/*hdr
**      FILE:           GView.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           12/8/2015
**      DESCRIPTION:    SDL GView Implementation
**
**      Copyright (C) 2015, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <time.h>


#include "Lgi.h"
#include "Base64.h"
#include "GDragAndDrop.h"
#include "GDropFiles.h"
#include "GViewPriv.h"
#include "GCss.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
GViewPrivate::GViewPrivate()
{
	Font = 0;
	FontOwn = false;
	CtrlId = -1;
	DropTarget = NULL;
	DropSource = NULL;
	Parent = 0;
	ParentI = 0;
	Notify = 0;
	IsThemed = true;
}

GViewPrivate::~GViewPrivate()
{
	if (FontOwn)
	{
		DeleteObj(Font);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Stuff
#include "zmouse.h"

int MouseRollMsg = 0;

#ifdef __GNUC__
#define MSH_WHEELMODULE_CLASS	"MouseZ"
#define MSH_WHEELMODULE_TITLE	"Magellan MSWHEEL"
#define MSH_SCROLL_LINES		"MSH_SCROLL_LINES_MSG" 
#endif

int _lgi_mouse_wheel_lines()
{
	OSVERSIONINFO Info;
	ZeroObj(Info);
	Info.dwOSVersionInfoSize = sizeof(Info);
	if (GetVersionEx(&Info) &&
		Info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
		Info.dwMajorVersion == 4 &&
		Info.dwMinorVersion == 0)
	{
       HWND hdlMSHWheel=NULL;
       UINT msgMSHWheelGetScrollLines=NULL;
       UINT uiMsh_WheelScrollLines;

       msgMSHWheelGetScrollLines = 
               RegisterWindowMessage(MSH_SCROLL_LINES);
       hdlMSHWheel = FindWindow(MSH_WHEELMODULE_CLASS, 
                                MSH_WHEELMODULE_TITLE);
       if (hdlMSHWheel && msgMSHWheelGetScrollLines)
       {
			return SendMessage(hdlMSHWheel, msgMSHWheelGetScrollLines, 0, 0);
       }
	}
	else
	{
		UINT nScrollLines;
		if (SystemParametersInfo(	SPI_GETWHEELSCROLLLINES, 
									0, 
									(PVOID) &nScrollLines, 
									0))
		{
			return nScrollLines;
		}
	}

	return 3;
}

#define SetKeyFlag(v, k, f)		if (GetKeyState(k)&0xFF00) { v |= f; }

int _lgi_get_key_flags()
{
	int Flags = 0;
	
	if (LgiGetOs() == LGI_OS_WIN9X)
	{
		SetKeyFlag(Flags, VK_MENU, LGI_EF_ALT);

		SetKeyFlag(Flags, VK_SHIFT, LGI_EF_SHIFT);

		SetKeyFlag(Flags, VK_CONTROL, LGI_EF_CTRL);
	}
	else // is NT/2K/XP
	{
		SetKeyFlag(Flags, VK_LMENU, LGI_EF_LALT);
		SetKeyFlag(Flags, VK_RMENU, LGI_EF_RALT);

		SetKeyFlag(Flags, VK_LSHIFT, LGI_EF_LSHIFT);
		SetKeyFlag(Flags, VK_RSHIFT, LGI_EF_RSHIFT);

		SetKeyFlag(Flags, VK_LCONTROL, LGI_EF_LCTRL);
		SetKeyFlag(Flags, VK_RCONTROL, LGI_EF_RCTRL);
	}

	if (GetKeyState(VK_CAPITAL))
		SetFlag(Flags, LGI_EF_CAPS_LOCK);

	return Flags;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int GetInputACP()
{
	char Str[16];
	LCID Lcid = (NativeInt)GetKeyboardLayout(LgiGetCurrentThread()) & 0xffff;
	GetLocaleInfo(Lcid, LOCALE_IDEFAULTANSICODEPAGE , Str, sizeof(Str));
	return atoi(Str);
}

GKey::GKey(int v, int flags)
{
	const char *Cp = 0;

	vkey = v;
	c16 = 0;

	#if OLD_WM_CHAR_MODE
	
	c16 = vkey;
	
	#else

	typedef int (WINAPI *p_ToUnicode)(UINT, UINT, PBYTE, LPWSTR, int, UINT);

	static bool First = true;
	static p_ToUnicode ToUnicode = 0;

	if (First)
	{
		ToUnicode = (p_ToUnicode) GetProcAddress(LoadLibrary("User32.dll"), "ToUnicode");
		First = false;
	}

	if (ToUnicode)
	{
		BYTE state[256];
		GetKeyboardState(state);
		char16 w[4];
		int r = ToUnicode(vkey, flags & 0x7f, state, w, CountOf(w), 0);
		if (r == 1)
		{
			c16 = w[0];
		}
	}

	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GViewI *GWindowFromHandle(OsView Wnd)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
void GView::_Delete()
{
	GViewI *c;
	#ifdef _DEBUG
	// Sanity check..
	// GArray<GViewI*> HasView;
	for (c = Children.First(); c; c = Children.Next())
	{
		// LgiAssert(!HasView.HasItem(c));
		// HasView.Add(c);
		LgiAssert(((GViewI*)c->GetParent()) == this || c->GetParent() == 0);
	}
	#endif

	// Delete myself out of my parent's list
	if (d->Parent)
	{
		d->Parent->OnChildrenChanged(this, false);
		d->Parent->DelView(this);
		d->Parent = 0;
		d->ParentI = 0;
	}

	// Delete all children
	while (c = Children.First())
	{
		// If it has no parent, remove the pointer from the child list
		if (c->GetParent() == 0)
			Children.Delete(c);

		// Delete the child view
		DeleteObj(c);
	}

	// Delete the OS representation of myself
	
	// NULL my handles and flags
	_View = 0;
	WndFlags = 0;

	// Remove static references to myself
	if (_Over == this) _Over = 0;
	if (_Capturing == this) _Capturing = 0;

	if (LgiApp && LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	GWindow *Wnd = GetWindow();
	if (Wnd)
	{
		Wnd->SetFocus(this, GWindow::ViewDelete);
	}

	// this should only exist in an ex-GWindow, due to the way
	// C++ deletes objects it needs to be here.
	DeleteObj(_Lock);
}

void GView::Quit(bool DontDelete)
{
	if (_View)
	{
		if (!DontDelete)
		{
			WndFlags |= GWF_QUIT_WND;
		}

		// DestroyWindow(_View);
	}
}

bool GView::IsAttached()
{
	return _View;
}

bool GView::Attach(GViewI *p)
{
	bool Status = false;

	SetParent(p);
	GView *Parent = d->GetParent();
	if (Parent && !_Window)
		_Window = Parent->_Window;

	if (0)
	{
		// Real window with HWND
		bool Enab = Enabled();

		LgiAssert(!Parent || Parent->Handle() != 0);

		if (_View)
		{
			Status = (_View != 0);
		}

		OnAttach();

	}
	else
	{
		// Virtual window (no HWND)
		Status = true;
	}

	if (Status && d->Parent)
	{
		if (!d->Parent->HasView(this))
		{
			d->Parent->AddView(this);
		}
		d->Parent->OnChildrenChanged(this, true);
	}

	return Status;
}

bool GView::Detach()
{
	bool Status = false;
	if (d->Parent)
	{
		Visible(false);
		d->Parent->DelView(this);
		d->Parent->OnChildrenChanged(this, false);
		d->Parent = 0;
		d->ParentI = 0;
		Status = true;
		WndFlags &= ~GWF_FOCUS;

		if (_Capturing == this)
		{
			if (_View)
				ReleaseCapture();
			_Capturing = 0;
		}
		if (_View)
		{
			WndFlags &= ~GWF_QUIT_WND;
			// Status = DestroyWindow(_View);
			LgiAssert(Status);
		}
	}
	return Status;
}

GRect &GView::GetClient(bool InClientSpace)
{
	static GRect Client;

	Client = Pos;
	if (InClientSpace)
		Client.Offset(-Client.x1, -Client.y1);

	return Client;
}

LgiCursor GView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

bool LgiToWindowsCursor(LgiCursor Cursor)
{
	char *Set = 0;
	switch (Cursor)
	{
		case LCUR_UpArrow:
			Set = IDC_UPARROW;
			break;
		case LCUR_Cross:
			Set = IDC_CROSS;
			break;
		case LCUR_Wait:
			Set = IDC_WAIT;
			break;
		case LCUR_Ibeam:
			Set = IDC_IBEAM;
			break;
		case LCUR_SizeVer:
			Set = IDC_SIZENS;
			break;
		case LCUR_SizeHor:
			Set = IDC_SIZEWE;
			break;
		case LCUR_SizeBDiag:
			Set = IDC_SIZENESW;
			break;
		case LCUR_SizeFDiag:
			Set = IDC_SIZENWSE;
			break;
		case LCUR_SizeAll:
			Set = IDC_SIZEALL;
			break;
		case LCUR_PointingHand:
		{
			GArray<int> Ver;
			int Os = LgiGetOs(&Ver);
			if
			(
				(
					Os == LGI_OS_WIN32
					||
					Os == LGI_OS_WIN64
				)
				&&
				Ver[0] >= 5)
			{
				#ifndef IDC_HAND
				#define IDC_HAND MAKEINTRESOURCE(32649)
				#endif
				Set = IDC_HAND;
			}
			// else not supported
			break;
		}
		case LCUR_Forbidden:
			Set = IDC_NO;
			break;

		// Not impl
		case LCUR_SplitV:
			break;
		case LCUR_SplitH:
			break;
		case LCUR_Blank:
			break;
	}

	SetCursor(LoadCursor(0, MAKEINTRESOURCE(Set?Set:IDC_ARROW)));

	return true;
}

void GView::PointToScreen(GdcPt2 &p)
{
	POINT pt = {p.x, p.y};
	GViewI *t = this;
	while (	t &&
			t->GetParent() &&
			!t->Handle())
	{
		pt.x += t->GetPos().x1;
		pt.y += t->GetPos().y1;
		t = t->GetParent();
	}
	// ClientToScreen(t->Handle(), &pt);
	p.x = pt.x;
	p.y = pt.y;
}

void GView::PointToView(GdcPt2 &p)
{
	POINT pt = {p.x, p.y};
	GViewI *t = this;
	while (	t &&
			t->GetParent() &&
			!t->Handle())
	{
		pt.x -= t->GetPos().x1;
		pt.y -= t->GetPos().y1;
		t = t->GetParent();
	}
	// ScreenToClient(t->Handle(), &pt);
	p.x = pt.x;
	p.y = pt.y;
}

bool GView::GetMouse(GMouse &m, bool ScreenCoords)
{
	// position
	POINT p;
	GetCursorPos(&p);
	if (!ScreenCoords)
	{
		// ScreenToClient(_View, &p);
	}
	m.x = p.x;
	m.y = p.y;
	m.Target = this;

	// buttons
	m.Flags =	((GetAsyncKeyState(VK_LBUTTON)&0x8000) ? LGI_EF_LEFT : 0) |
				((GetAsyncKeyState(VK_MBUTTON)&0x8000) ? LGI_EF_MIDDLE : 0) |
				((GetAsyncKeyState(VK_RBUTTON)&0x8000) ? LGI_EF_RIGHT : 0) |
				((GetAsyncKeyState(VK_CONTROL)&0x8000) ? LGI_EF_CTRL : 0) |
				((GetAsyncKeyState(VK_MENU)&0x8000) ? LGI_EF_ALT : 0) |
				((GetAsyncKeyState(VK_SHIFT)&0x8000) ? LGI_EF_SHIFT : 0);

	if (m.Flags & (LGI_EF_LEFT | LGI_EF_MIDDLE | LGI_EF_RIGHT))
	{
		m.Flags |= LGI_EF_DOWN;
	}

	return true;
}

bool GView::SetPos(GRect &p, bool Repaint)
{
	bool Status = true;
	GRect OldPos = Pos;

	if (Pos != p)
	{
		Pos = p;
		if (_View)
		{
		}
		else if (GetParent())
		{
			OnPosChange();
		}
		
		if (Repaint)
		{
			Invalidate();
		}
	}

	return Status;
}

bool GView::Invalidate(GRect *r, bool Repaint, bool Frame)
{
	if (_View)
	{
		bool Status = false;

		if (Frame)
		{
		}
		else
		{
			if (r)
			{
				// Status = InvalidateRect(_View, &((RECT)*r), false);
			}
			else
			{
				RECT c = GetClient();
				// Status = InvalidateRect(_View, &c, false);
			}
		}

		if (Repaint)
		{
			// UpdateWindow(_View);
		}

		return Status;
	}
	else
	{
		GRect Up;
		GViewI *p = this;

		if (r)
		{
			Up = *r;
		}
		else
		{
			Up.Set(0, 0, Pos.X()-1, Pos.Y()-1);
		}

		if (dynamic_cast<GWindow*>(this))
			return true;

		while (p && !p->Handle())
		{
			GViewI *Par = p->GetParent();
			GView *VPar = Par?Par->GetGView():0;
			GRect w = p->GetPos();
			GRect c = p->GetClient(false);
			if (Frame && p == this)
				Up.Offset(w.x1, w.y1);
			else				
				Up.Offset(w.x1 + c.x1, w.y1 + c.y1);
			p = Par;
		}

		if (p && p->Handle())
		{
			return p->Invalidate(&Up, Repaint);
		}
	}

	return false;
}

void GView::SetPulse(int Length)
{
	if (_View)
	{
		if (Length > 0)
		{
		}
		else
		{
		}
	}
}

void GView::DrawThemeBorder(GSurface *pDC, GRect &r)
{
	LgiWideBorder(pDC, r, DefaultSunkenEdge);
}

bool IsKeyChar(GKey &k, int vk)
{
	if (k.Ctrl() || k.Alt() || k.System())
		return false;

	switch (vk)
	{
		case VK_BACK:
		case VK_TAB:
		case VK_RETURN:
		case VK_SPACE:

		case 0xba: // ;
		case 0xbb: // =
		case 0xbc: // ,
		case 0xbd: // -
		case 0xbe: // .
		case 0xbf: // /

		case 0xc0: // `

		case 0xdb: // [
		case 0xdc: // |
		case 0xdd: // ]
		case 0xde: // '
			return true;
	}

	if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE)
		return true;

	if (vk >= '0' && vk <= '9')
		return true;

	if (vk >= 'A' && vk <= 'Z')
		return true;

	return false;
}

#define KEY_FLAGS		(~(MK_LBUTTON | MK_MBUTTON | MK_RBUTTON))

GMessage::Result GView::OnEvent(GMessage *Msg)
{
	return 0;
}

GViewI *GView::FindControl(OsView hCtrl)
{
	if (_View == hCtrl)
	{
		return this;
	}

	for (List<GViewI>::I i = Children.Start(); i.In(); i++)
	{
		GViewI *Ctrl = (*i)->FindControl(hCtrl);
		if (Ctrl)
		{
			return Ctrl;
		}
	}
	return 0;
}

GView *&GView::PopupChild()
{
	return d->Popup;
}

bool GView::_Mouse(GMouse &m, bool Move)
{
	return false;
}

void GView::_Focus(bool f)
{
}
