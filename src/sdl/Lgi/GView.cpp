
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
	FontOwnType = GV_FontPtr;
	CtrlId = -1;
	DropTarget = NULL;
	DropSource = NULL;
	Parent = 0;
	ParentI = 0;
	Notify = 0;
	IsThemed = true;
	PulseId = NULL;
}

GViewPrivate::~GViewPrivate()
{
	if (FontOwnType == GV_FontOwned)
	{
		DeleteObj(Font);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Stuff

int MouseRollMsg = 0;


#define SetKeyFlag(v, k, f)		if (GetKeyState(k)&0xFF00) { v |= f; }


GKey::GKey(int v, int flags)
{
	const char *Cp = 0;

	vkey = v;
	c16 = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GViewI *GWindowFromHandle(OsView Wnd)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
void GView::_Delete()
{
	SetPulse();
	
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
	while ((c = Children.First()))
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
			LgiAssert(!"Release capture");
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
	Client.Offset(-Client.x1, -Client.y1);

	return Client;
}

LgiCursor GView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

bool LgiToWindowsCursor(LgiCursor Cursor)
{
	return false;
}

void GView::PointToScreen(GdcPt2 &p)
{
}

void GView::PointToView(GdcPt2 &p)
{
}

bool GView::GetMouse(GMouse &m, bool ScreenCoords)
{
	return false;
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

	while (p && !dynamic_cast<GWindow*>(p))
	{
		GViewI *Par = p->GetParent();
		if (Par == p)
		{
			LgiAssert(!"Window can't be parent of itself.");
			break;
		}
		
		GView *VPar = Par ? Par->GetGView() : 0;
		GRect w = p->GetPos();
		GRect c = p->GetClient(false);
		
		if (Frame && p == this)
			Up.Offset(w.x1, w.y1);
		else				
			Up.Offset(w.x1 + c.x1, w.y1 + c.y1);
		
		p = Par;
	}

	return LgiApp->InvalidateRect(Up);
}

Uint32 SDL_PulseCallback(Uint32 interval, GView *v)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = M_PULSE;
	e.user.data1 = v;
	SDL_PushEvent(&e);
	return v->d->PulseLength;
}

void GView::SetPulse(int Length)
{
	if (d->PulseId > 0)
	{
		SDL_RemoveTimer(d->PulseId);
		d->PulseId = 0;
	}
	
	if (Length > 0)
	{
		d->PulseLength = Length;
		d->PulseId = SDL_AddTimer(Length, (SDL_NewTimerCallback)SDL_PulseCallback, this);
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
	if
	(
		GetWindow()
		&&
		!GetWindow()->HandleViewMouse(this, m)
	)
	{
		return false;
	}

	if (Move)
	{
		GViewI *o = WindowFromPoint(m.x, m.y, true);
		if (_Over != o)
		{
			if (_Over)
			{
				// LgiTrace("From Over %s\n", _Over->GetClass());
				_Over->OnMouseExit(lgi_adjust_click(m, _Over));
			}
			_Over = o;
			if (_Over)
			{
				_Over->OnMouseEnter(lgi_adjust_click(m, _Over));
				// LgiTrace("To Over %s\n", _Over->GetClass());
			}
		}
	}

	GViewI *cap = _Capturing;
	if (_Capturing)
	{
		if (Move)
		{
			GMouse Local = lgi_adjust_click(m, _Capturing);
			// LgiToGtkCursor(_Capturing, _Capturing->GetCursor(Local.x, Local.y));
			_Capturing->OnMouseMove(Local); // This can set _Capturing to NULL
		}
		else
		{
			_Capturing->OnMouseClick(lgi_adjust_click(m, _Capturing));
		}
	}
	else
	{
		GView *Target = dynamic_cast<GView*>(_Over ? _Over : this);

		GRect Client = Target->GView::GetClient(false);
		
		m = lgi_adjust_click(m, Target);
		if (!Client.Valid() || Client.Overlap(m.x, m.y))
		{
			// LgiToGtkCursor(Target, Target->GetCursor(m.x, m.y));

			if (Move)
			{
				Target->OnMouseMove(m);
			}
			else
			{
				#if 0
				if (!Move)
				{
					char Msg[256];
					sprintf(Msg, "_Mouse Target %s", Target->GetClass());
					m.Trace(Msg);
				}
				#endif
				Target->OnMouseClick(m);
			}
		}
		else if (!Move)
		{
			printf("%s:%i - Click outside %s %s %i,%i\n", _FL, Target->GetClass(), Client.GetStr(), m.x, m.y);
		}
	}
	
	return true;
}

void GView::_Focus(bool f)
{
}
