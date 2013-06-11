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
// Standard sub-class of any BControl

/*
template <class b>
class BSub : public b
{
	GView *View;

public:
	BSub(GView *v) : b(BRect(0, 0, 1, 1), "BSub")
	{
		View = v;
	}

	void AttachedToWindow()
	{
		if (View) View->OnCreate();
	}

	void DetachedFromWindow()
	{
		if (View) View->OnDestroy();
	}
	
	void Draw(BRect UpdateRect) {}
	void FrameMoved(BPoint Point) {}
	void FrameResized(float width, float height) {}
	void Pulse() {}
	void MessageReceived(BMessage *message) {}
	void MakeFocus(bool f = true) {}
	void KeyDown(const char *bytes, int32 numBytes) {}
	void KeyUp(const char *bytes, int32 numBytes) {}
	void MouseDown(BPoint point) {}
	void MouseMoved(BPoint point, uint32 transit, const BMessage *message) {}
	bool QuitRequested() {}
};
*/


///////////////////////////////////////////////////////////////////////////////////////////
extern rgb_color BeRGB(uchar r, uchar g, uchar b);

rgb_color BeRGB(uchar r, uchar g, uchar b)
{
	rgb_color c;

	c.red = r;
	c.green = g;
	c.blue = b;

	return c;
}

rgb_color BeRGB(COLOUR c)
{
	rgb_color b;

	b.red = R24(c);
	b.green = G24(c);
	b.blue = B24(c);

	return b;
}

/////////////////////////////////////////////////////////////////////////////////////////
/* FIXME?
#define LGI_WATCHER_MSG			0x80000000

class GMouseWatcher : public GThread
{
	int TimeOut;
	GView *Notify;

public:
	GMouseWatcher(int timeout, GView *notify)
	{
		TimeOut = timeout;
		Notify = notify;
		if (Notify)
		{
			Run();
		}
	}
	
	void Convert(GdcPt2 &Pt)
	{
		BPoint BPt(Pt.x, Pt.y);
		Notify->Handle()->ConvertFromScreen(&BPt);
		Pt.x = BPt.x;
		Pt.y = BPt.y;
	}
	
	int Main()
	{
		GMouse m;
		
		Notify->GetMouse(m);
		m.Flags |= LGI_WATCHER_MSG;
	
		while (m.Left() || m.Right())
		{
			LgiSleep(TimeOut);

			GMouse Ms;
			Notify->GetMouse(Ms);
		
			if (Ms.x != m.x ||
				Ms.y != m.y)
			{
				if (Notify->Handle()->LockLooper())
				{
					m.x = Ms.x;
					m.y = Ms.y;
					m.Flags |= LGI_WATCHER_MSG;
					Notify->OnMouseMove(m);

					Notify->Handle()->UnlockLooper();
				}
			}
		}
	
		return 0;
	}
};
*/

///////////////////////////////////////////////////////////////////////////////////////////
struct GDialogPriv
{
	bool IsModal;
	int ModalRet;
	sem_id ModalSem;
	
	GDialogPriv()
	{
		IsModal = true;
		ModalRet = 0;
		ModalSem = B_BAD_VALUE;
	}
};

GDialog::GDialog() : ResObject(Res_Dialog)
{
	d = new GDialogPriv;
	Name("Dialog");

	if (Wnd)
	{
		/*
		Wnd->SetFlags(B_NOT_RESIZABLE);
		Wnd->SetType(B_MODAL_WINDOW);
		Wnd->SetLook(B_TITLED_WINDOW_LOOK);
		*/
	}
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
	printf("GDialog::OnRequestClose(%i) modal=%i\n", OsClose, d->IsModal);
	if (d->IsModal)
		EndModal(0);
	else
		EndModeless();
		
	return false;
}

status_t WaitForDelete(sem_id blocker, BWindow *Wnd)
{
	status_t	result;
	thread_id	this_tid = find_thread(NULL);
	bool		Warned = false;

	// block until semaphore is deleted (modal is finished)
	do
	{
		if (Wnd)
		{
			if (!Warned && Wnd->IsLocked())
			{
				printf("%p Locked! Count=%i Thread=%i Req=%i\n",
					Wnd, Wnd->CountLocks(), Wnd->LockingThread(),
					Wnd->CountLockRequests());
				Warned = true;
			}
			
			// update the parent window periodically
			Wnd->UpdateIfNeeded();
		}
		
		result = acquire_sem_etc(blocker, 1, B_TIMEOUT, 1000000);
		printf("acquire_sem_etc = %i\n", result);
	}
	while (result != B_BAD_SEM_ID);
	
	return result;
}

int GDialog::DoModal(OsView ParentHnd)
{
	LgiTrace("DoModal() Me=%i Count=%i Thread=%i\n", LgiGetCurrentThread(), Wnd->CountLocks(), Wnd->LockingThread());

	d->IsModal = true;
	d->ModalRet = 0;
	d->ModalSem = create_sem(0, "ModalSem");
	
	if (!_Default)
	{
		GViewI *c = FindControl(IDOK);
		GButton *Def = c ? dynamic_cast<GButton*>(c) : 0;
		if (Def)
		{
			_Default = Def;
		}
		else
		{
			for (c=Children.First(); c; c=Children.Next())
			{
				_Default = dynamic_cast<GButton*>(c);
				if (_Default) break;
			}
		}
	}

	if (Wnd->LockLooper())
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
		Wnd->UnlockLooper();

		printf("starting WaitForDelete\n");

		thread_id	this_tid = find_thread(NULL);
		status_t	result;
		bool		Warned = false;
		BWindow		*parent = GetParent() ? GetParent()->WindowHandle() : NULL;
		printf("GDialog::DoModal parent=%p\n", parent);
	
		// block until semaphore is deleted (modal is finished)
		do
		{
			if (parent)
			{
				if (!Warned && parent->IsLocked())
				{
					printf("%p Locked! Count=%i Thread=%i Req=%i\n",
						parent, parent->CountLocks(), parent->LockingThread(),
						parent->CountLockRequests());
					Warned = true;
				}
				
				// update the parent window periodically
				parent->UpdateIfNeeded();
			}
			
			result = acquire_sem_etc(d->ModalSem, 1, B_TIMEOUT, 1000000);
			printf("acquire_sem_etc = %x\n", result);
		}
		while (result != B_BAD_SEM_ID);
	
		printf("...WaitForDelete done\n");
	}
	else
	{
		printf("%s:%i - DoModal Wnd->Lock() failed.\n", _FL);
	}	
	
	return d->ModalRet;
}

int GDialog::DoModeless()
{
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
		Wnd->Quit();
		Wnd = 0;
	}
	
	OnDestroy();
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
	if (Handle()->LockLooper())
	{
		Handle()->GetMouse(&Pt, &Buttons, FALSE);
		m.x = Pt.x;
		m.y = Pt.y;
		m.Flags = Buttons;

		if (Down)
		{
			int32 Clicks = 1;
			WindowHandle()->CurrentMessage()->FindInt32("clicks", &Clicks);
			m.Double(	(Clicks > 1) AND
						(abs(m.x-LastX) < DOUBLE_CLICK_THRESHOLD) AND
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
		Handle()->UnlockLooper();
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

//////////////////////////////////////////////////////////////////////////////////
#include "GBitmap.h"

GBitmap::GBitmap(int id, int x, int y, char *FileName, bool Async) :
	GControl(new BViewRedir(this)),
	ResObject(Res_Bitmap)
{
	SetId(id);
	GRect r(x, y, x + 1, y + 1);
	SetPos(r);
	pDC = 0;
	Sunken(true);

	int Opt = GdcD->SetOption(GDC_PROMOTE_ON_LOAD, GdcD->GetBits());
	pDC = LoadDC(FileName);
	GdcD->SetOption(GDC_PROMOTE_ON_LOAD, Opt);

	if (pDC)
	{
		r.Dimension(pDC->X()+4, pDC->Y()+4);
		SetPos(r);
	}
}

GBitmap::~GBitmap()
{
	DeleteObj(pDC);
}

GMessage::Result GBitmap::OnEvent(GMessage *Msg)
{
	return 0;
}

void GBitmap::OnPaint(GSurface *pScreen)
{
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);

	if (pDC)
	{
		pScreen->Blt(c.x1, c.y1, pDC);
	}
	else
	{
		pScreen->Colour(CBit(pScreen->GetBits(), Rgb24(255, 255, 255)));
		pScreen->Rectangle(&c);
	}
}

void GBitmap::OnMouseClick(GMouse &m)
{
	if (!m.Down() AND GetParent())
	{
		GDialog *p = dynamic_cast<GDialog*>(GetParent());
		p->OnNotify(this, 0);
	}
}

void GBitmap::SetDC(GSurface *pNewDC)
{
	DeleteObj(pDC);
	pDC = pNewDC;
	if (pDC)
	{
		GRect r = GetPos();
		r.Dimension(pDC->X() + 4, pDC->Y() + 4);
		SetPos(r);
		Invalidate();
	}
}

GSurface *GBitmap::GetSurface()
{
	return pDC;
}

///////////////////////////////////////////////////////////////////////////////////////////
GItemContainer::GItemContainer()
{
	Flags = 0;
	ImageList = 0;
}

GItemContainer::~GItemContainer()
{
	if (OwnList())
	{
		DeleteObj(ImageList);
	}
	else
	{
		ImageList = 0;
	}
}

bool GItemContainer::SetImageList(GImageList *list, bool Own)
{
	ImageList = list;
	OwnList(Own);
	AskImage(ImageList != NULL);
	return ImageList != NULL;
}
