/*hdr
**      FILE:           GuiDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Dialog components
**
**      Copyright (C) 1998 Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Lgi.h"
#include "GButton.h"
#include "GTableLayout.h"

#define TICKS_PER_SECOND					1000000
#define DOUBLE_CLICK_TIMEOUT				(TICKS_PER_SECOND/3)

#define DefaultViewColour()					SetViewColor(B_TRANSPARENT_COLOR); SetFlags(Flags() & ~B_FULL_UPDATE_ON_RESIZE)

///////////////////////////////////////////////////////////////////////////////////////////
struct GDialogPriv
{
	GDialog *Dlg;
	bool IsModal;
	int ModalRet;
	sem_id ModalSem;
	
	GDialogPriv(GDialog *dlg)
	{
		Dlg = dlg;
		IsModal = true;
		ModalRet = 0;
		ModalSem = B_BAD_VALUE;
	}
	
	void SetDefaultBtn()
	{
		if (!Dlg->_Default)
		{
			GViewI *c = Dlg->FindControl(IDOK);
			GButton *Def = c ? dynamic_cast<GButton*>(c) : 0;
			if (Def)
			{
				Dlg->_Default = Def;
			}
			else
			{
				for (c=Dlg->Children.First(); c; c=Dlg->Children.Next())
				{
					Dlg->_Default = dynamic_cast<GButton*>(c);
					if (Dlg->_Default)
						break;
				}
			}
		}
	}
};

GDialog::GDialog() : ResObject(Res_Dialog)
{
	d = new GDialogPriv(this);
	Name("Dialog");
}

GDialog::~GDialog()
{
	SetParent(0);
	DeleteObj(d);
}

void GDialog::Quit(bool DontDelete)
{
	if (d->IsModal)
	{
		EndModal(0);
	}
	else
	{
		EndModeless();
	}	
}

void GDialog::OnPosChange()
{
    if (Children.Length() == 1)
    {
        List<GViewI>::I it = Children.Start();
        GTableLayout *t = dynamic_cast<GTableLayout*>((GViewI*)it.First());
        if (t)
        {
            GRect r = GetClient();
            r.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
            t->SetPos(r);
        }
    }
    
    GWindow::OnPosChange();
}

bool GDialog::LoadFromResource(int Resource, char *Param)
{
	GAutoString n;
	GRect p;
	bool Status = GLgiRes::LoadFromResource(Resource, this, &p, &n);
	if (Status)
	{
		Name(n);
		SetPos(p, true);
	}
	return Status;
}

bool GDialog::OnRequestClose(bool OsClose)
{
	if (d->IsModal)
		EndModal(0);
	else
		EndModeless();
		
	return false;
}

int GDialog::DoModal(OsView ParentHnd)
{
	d->IsModal = true;
	d->ModalRet = 0;
	d->ModalSem = create_sem(0, "ModalSem");
	
	if (GetParent())
		GetParent()->Capture(false);
	
	d->SetDefaultBtn();

	GLocker Locker(Wnd, _FL);
	if (Locker.Lock())
	{
		// Setup the BWindow
		Wnd->ResizeTo(Pos.X(), Pos.Y());
		Wnd->MoveTo(Pos.x1, Pos.y1);
		Wnd->SetTitle(GBase::Name());

		// Add BView here to move the "OnCreate" call to the correct place
		BRect r = Wnd->Bounds();
		Handle()->MoveTo(0, 0);
		Handle()->ResizeTo(r.Width(), r.Height());
		Wnd->AddChild(Handle());
		AttachChildren();

		Wnd->Show();
		Locker.Unlock();

		thread_id	this_tid = find_thread(NULL);
		status_t	result;
		bool		Warned = false;
		BWindow		*parent = GetParent() ? GetParent()->WindowHandle() : NULL;
		int			Locks = parent ? parent->CountLocks() : 0;
		bool		ForceParentToUnlock = false;

		if (ForceParentToUnlock)
		{
			for (int c=0; c<Locks; c++)
				parent->UnlockLooper();
		}
	
		// block until semaphore is deleted (modal is finished)
		do
		{
			if (parent)
			{
				/*
				if (!Warned && parent->IsLocked())
				{
					printf("%p Locked! Count=%i Thread=%i Req=%i\n",
						parent, parent->CountLocks(), parent->LockingThread(),
						parent->CountLockRequests());
					Warned = true;
				}
				*/
				
				// update the parent window periodically
				parent->UpdateIfNeeded();
			}
			
			result = acquire_sem_etc(d->ModalSem, 1, B_TIMEOUT, 50 * 1000); // 50 ms
		}
		while (result != B_BAD_SEM_ID);
	
		if (ForceParentToUnlock)
		{
			for (int c=0; c<Locks; c++)
				parent->LockLooper();
		}
	}
	else
	{
		printf("%s:%i - DoModal Wnd->Lock() failed.\n", _FL);
	}	
	
	return d->ModalRet;
}

int GDialog::DoModeless()
{
	d->IsModal = false;
	
	d->SetDefaultBtn();

	if (Wnd->Lock())
	{
		// Setup the BWindow
		Wnd->ResizeTo(Pos.X(), Pos.Y());
		Wnd->MoveTo(Pos.x1, Pos.y1);
		Wnd->SetTitle(GBase::Name());

		// Add BView here to move the "OnCreate" call to the correct place
		BRect r = Wnd->Bounds();
		Handle()->MoveTo(0, 0);
		Handle()->ResizeTo(r.Width(), r.Height());
		Wnd->AddChild(Handle());
		AttachChildren();

		// Open the window..
		Wnd->Show();
		Wnd->Unlock();
		
		return true;
	}
	
	return false;
}

GMessage::Result GDialog::OnEvent(GMessage *Msg)
{
	return GWindow::OnEvent(Msg);
}

bool GDialog::IsModal()
{
	return d->IsModal;
}


void GDialog::EndModal(int Code)
{
	d->ModalRet = Code;
	delete_sem(d->ModalSem); // causes DoModal to unblock
}

void GDialog::EndModeless(int Code)
{
	if (Wnd->Lock())
	{
		Handle()->RemoveSelf();
		
		Wnd->Hide();
		OnDestroy();
		
		// This will exit this thread...
		OsWindow w = Wnd;
		Wnd = NULL;
		delete this;
		
		if (w->Lock())
		{
			w->Quit();
		}
	}
	
	LgiTrace("GDialog::EndModeless Lock failed..\n");
}

///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(BView *view) : GView(view)
{
	Pos.ZOff(10, 10);
	SetId(0);
	Sys_LastClick = 0;
}

GControl::~GControl()
{
}

GMessage::Result GControl::OnEvent(GMessage *Msg)
{
	return 0;
}

void GControl::MouseClickEvent(bool Down)
{
	static int LastX = -1, LastY = -1;

	GMouse m;
	BPoint Pt;
	uint32 Buttons;
	
	GLocker Locker(Handle(), _FL);
	if (Locker.Lock())
	{
		Handle()->GetMouse(&Pt, &Buttons, FALSE);
		m.x = Pt.x;
		m.y = Pt.y;
		m.Flags = Buttons;

		if (Down)
		{
			int32 Clicks = 1;
			WindowHandle()->CurrentMessage()->FindInt32("clicks", &Clicks);
			m.Double(	(Clicks > 1) &&
						(abs(m.x-LastX) < DOUBLE_CLICK_THRESHOLD) &&
						(abs(m.y-LastY) < DOUBLE_CLICK_THRESHOLD));
			
			LastX = m.x;
			LastY = m.y;
		}
		else
		{
			m.Double(false);
		}
		
		m.Down(Down);
		OnMouseClick(m);
	}
}

//////////////////////////////////////////////////////////////////////////////////
// Slider control
#include "GSlider.h"

GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	GControl(new BViewRedir(this)),
	ResObject(Res_Slider)
{
	SetId(id);
	
	if (vert)
	{
		GRect r(x, y, x+13, y+cy);
		SetPos(r);
	}
	else
	{
		GRect r(x, y, x+cx, y+13);
		SetPos(r);
	}
	
	Name(name);
	Vertical = vert;
	Min = 0;
	Max = 1;
	Val = 0;
	
	Thumb.ZOff(-1, -1);
}

GSlider::~GSlider()
{
}

void GSlider::Value(int64 i)
{
	Val = i;
	Invalidate();
}

int64 GSlider::Value()
{
	return Val;
}

void GSlider::GetLimits(int64 &min, int64 &max)
{
	min = Min;
	max = Max;
}

void GSlider::SetLimits(int64 min, int64 max)
{
	Min = min;
	Max = max;
	if (Val < Min) Val = Min;
	if (Val > Max) Val = Max;
}

GMessage::Result GSlider::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

#define SLIDER_BAR_SIZE			2
#define SLIDER_THUMB_WIDTH		10
#define SLIDER_THUMB_HEIGHT		6

void GSlider::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);

	// fill area
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);

	// draw tray
	int Mid = (Vertical) ? r.X()/2 : r.Y()/2;
	GRect Bar;
	if (Vertical)
	{
		Bar.ZOff(SLIDER_BAR_SIZE, Y()-1);
		Bar.Offset(Mid-(Bar.X()/2), 0);
	}
	else
	{
		Bar.ZOff(X()-1, SLIDER_BAR_SIZE);
		Bar.Offset(0, Mid-(Bar.Y()/2));
	}
	
	LgiThinBorder(pDC, Bar, SUNKEN);
	pDC->Colour(0);
	pDC->Line(Bar.x1, Bar.y1, Bar.x2, Bar.y2);
	
	// draw thumb
	int Length = ((Vertical) ? Y() : X()) - SLIDER_THUMB_HEIGHT - 1;
	int Pos = (Max != Min) ? Val * Length / (Max - Min) : 0;
	if (Vertical)
	{
		Thumb.ZOff(SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT);
		Thumb.Offset(Mid - (Thumb.X()/2), Pos);
	}
	else
	{
		Thumb.ZOff(SLIDER_THUMB_HEIGHT, SLIDER_THUMB_WIDTH);
		Thumb.Offset(Pos, Mid - (Thumb.Y()/2));
	}
	
	GRect t = Thumb;
	LgiWideBorder(pDC, t, RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&t);
}

void GSlider::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Thumb.Overlap(m.x, m.y))
		{
			Tx = m.x - Thumb.x1;
			Ty = m.y - Thumb.y1;
			Capture(true);
		}
	}
	else if (IsCapturing())
	{
		Capture(false);
	}
}

void GSlider::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		int Length = ((Vertical) ? Y() : X()) - SLIDER_THUMB_HEIGHT - 1;
		int NewPos = (Vertical) ? m.y - Ty : m.x - Tx;
		if (NewPos < 0) NewPos = 0;
		if (NewPos > Length) NewPos = Length;
		int NewVal = (Min != Max) ? (NewPos*(Max-Min)) / Length : 0;
		if (NewVal != Val)
		{
			Val = NewVal;
			Invalidate();
			
			if (GetParent())
			{
				GetParent()->OnNotify(this, Val);
			}
		}
	}
}

