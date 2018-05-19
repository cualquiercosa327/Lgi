/*hdr
**      FILE:           GuiDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Dialog components
**
**      Copyright (C) 1998 Matthew Allen
**              fret@memecode.com
*/

#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSlider.h"
#include "GBitmap.h"
#include "GTableLayout.h"
#include "GDisplayString.h"

///////////////////////////////////////////////////////////////////////////////////////////
struct GDialogPriv
{
	int ModalStatus;
	bool IsModal;
	bool Resizable;
	GWindow *PrevWindow;
	
	GDialogPriv()
	{
		PrevWindow = NULL;
		IsModal = true;
		Resizable = true;
		ModalStatus = 0;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog()
	: ResObject(Res_Dialog)
{
	d = new GDialogPriv();
	Name("Dialog");
	_View = 0;
	_SetDynamic(false);
}

GDialog::~GDialog()
{
	DeleteObj(d);
}

bool GDialog::IsModal()
{
	return d->IsModal;
}

void GDialog::Quit(bool DontDelete)
{
	if (d->IsModal)
		EndModal(0);
	else
		GView::Quit(DontDelete);
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
}

bool GDialog::LoadFromResource(int Resource, char *TagList)
{
	GAutoString n;
	GRect p;

	bool Status = GLgiRes::LoadFromResource(Resource, this, &p, &n, TagList);
	if (Status)
	{
		Name(n);
		SetPos(p);
	}
	return Status;
}

bool GDialog::OnRequestClose(bool OsClose)
{
	if (d->IsModal)
	{
		EndModal(0);
		return false;
	}

	return true;
}

int GDialog::DoModal(OsView OverrideParent)
{
	d->ModalStatus = -1;
	d->IsModal = true;
	d->PrevWindow = LgiApp->AppWnd;	
	AttachChildren();
	if (d->PrevWindow)
		d->PrevWindow->PushWindow(this);
	else
		LgiApp->PushWindow(this);
	LgiApp->Run();
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = 0;
		d->PrevWindow->PopWindow();
	}
	return d->ModalStatus;
}

void GDialog::EndModal(int Code)
{
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = Code;
		LgiApp->Exit();
		d->PrevWindow->PopWindow();
	}
	else
	{
		LgiAssert(!"Not a modal dialog");
	}
}

int GDialog::DoModeless()
{
	d->IsModal = false;
	return 0;
}

void GDialog::EndModeless(int Code)
{
	Quit(Code);
}

GMessage::Result GDialog::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
	}

	return GView::OnEvent(Msg);
}


///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(OsView view) : GView(view)
{
	Pos.ZOff(10, 10);
}

GControl::~GControl()
{
}

GMessage::Result GControl::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
	}
	return 0;
}

GdcPt2 GControl::SizeOfStr(const char *Str)
{
	int y = SysFont->GetHeight();
	GdcPt2 Pt(0, 0);

	if (Str)
	{
		const char *e = 0;
		for (const char *s = Str; s && *s; s = e?e+1:0)
		{
			e = strchr(s, '\n');
			int Len = e ? (int)e-(int)s : strlen(s);

			GDisplayString ds(SysFont, s, Len);
			Pt.x = MAX(Pt.x, ds.X());
			Pt.y += y;
		}
	}

	return Pt;
}

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	ResObject(Res_Slider)
{
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	Vertical = vert;
	Min = Max = 0;
	Val = 0;
	SetTabStop(true);
}

GSlider::~GSlider()
{
}

void GSlider::Value(int64 i)
{
	if (i > Max) i = Max;
	if (i < Min) i = Min;
	
	if (i != Val)
	{
		Val = i;

		GViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
		{
			n->OnNotify(this, Val);
		}
		
		Invalidate();
	}
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
}

GMessage::Result GSlider::OnEvent(GMessage *Msg)
{
	return 0;
}

void GSlider::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
	
	GRect r = GetClient();
	int y = r.Y() >> 1;
	r.y1 = y - 2;
	r.y2 = r.y1 + 3;
	r.x1 += 3;
	r.x2 -= 3;
	LgiWideBorder(pDC, r, DefaultSunkenEdge);
	
	if (Min <= Max)
	{
		int x = Val * r.X() / (Max-Min);
		Thumb.ZOff(5, 9);
		Thumb.Offset(r.x1 + x - 3, y - 5);
		GRect b = Thumb;
		LgiWideBorder(pDC, b, DefaultRaisedEdge);
		pDC->Rectangle(&b);		
	}
}

void GSlider::OnMouseClick(GMouse &m)
{
	Capture(m.Down());
	if (Thumb.Overlap(m.x, m.y))
	{
		Tx = m.x - Thumb.x1;
		Ty = m.y - Thumb.y1;
	}
}

void GSlider::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		int Rx = X() - 6;
		if (Rx > 0 && Max >= Min)
		{
			int x = m.x - Tx;
			int v = x * (Max-Min) / Rx;
			Value(v);
		}
	}
}

/*
//////////////////////////////////////////////////////////////////////////////////
#if defined(_MT) || defined(LINUX)
#define LgiThreadBitmapLoad
#endif

#ifdef LgiThreadBitmapLoad
class GBitmapThread : public ::LThread
{
	GBitmap *Bmp;
	char *File;
	LThread **Owner;

public:
	GBitmapThread(GBitmap *bmp, char *file, LThread **owner) : LThread("GBitmapThread")
	{
		Bmp = bmp;
		File = NewStr(file);
		Owner = owner;
		if (Owner)
		{
			*Owner = this;
		}
		Run();
	}

	~GBitmapThread()
	{
		if (Owner)
		{
			*Owner = 0;
		}
		DeleteArray(File);
	}

	int Main()
	{
		if (Bmp)
		{
			GSurface *pDC = LoadDC(File);
			if (pDC)
			{
				Bmp->SetDC(pDC);
				GRect r = Bmp->GetPos();
				r.Dimension(pDC->X()+4, pDC->Y()+4);
				Bmp->SetPos(r);
				Bmp->Invalidate();
				
				GViewI *n = Bmp->GetNotify() ? Bmp->GetNotify() : Bmp->GetParent();
				if (n)
				{
					int Start = LgiCurrentTime();
					while (!n->Handle())
					{
						LgiSleep(100);
						if (LgiCurrentTime() - Start > 2000)
						{
							break;
						}
					}
					
					Bmp->SendNotify();
				}
			}
		}

		return 0;
	}
};
#endif

GBitmap::GBitmap(int id, int x, int y, char *FileName, bool Async) :
	ResObject(Res_Bitmap)
{
	pDC = 0;
	pThread = 0;

	SetId(id);
	GRect r;
	r.ZOff(16, 16);
	r.Offset(x, y);

	if (FileName)
	{
		#ifdef LgiThreadBitmapLoad
		if (!Async)
		{
		#endif
			pDC = LoadDC(FileName);
			if (pDC)
			{
				r.Dimension(pDC->X()+4, pDC->Y()+4);
			}
		#ifdef LgiThreadBitmapLoad
		}
		else
		{
			new GBitmapThread(this, FileName, &pThread);
		}
		#endif
	}

	SetPos(r);
}

GBitmap::~GBitmap()
{
	#ifdef _MT
	if (pThread)
	{
		pThread->Terminate();
		DeleteObj(pThread);
	}
	#endif
	DeleteObj(pDC);
}

void GBitmap::SetDC(GSurface *pNewDC)
{
	DeleteObj(pDC);
	pDC = pNewDC;
	
	if (pDC)
	{
		int Border = (Sunken() || Raised()) ? _BorderSize : 0;
		GRect r = GetPos();
		r.Dimension(pDC->X() + (Border<<1), pDC->Y() + (Border<<1));
		SetPos(r, true);
	}
	
	Invalidate();
}

GSurface *GBitmap::GetSurface()
{
	return pDC;
}

GMessage::Param GBitmap::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GBitmap::OnPaint(GSurface *pScreen)
{
	GRect a(0, 0, X()-1, Y()-1);
	if (pDC)
	{
		pScreen->Blt(a.x1, a.y1, pDC);

		pScreen->Colour(LC_MED, 24);
		if (pDC->X() < a.X())
		{
			pScreen->Rectangle(pDC->X(), 0, a.x2, pDC->Y());
		}
		if (pDC->Y() < a.Y())
		{
			pScreen->Rectangle(0, pDC->Y(), a.x2, a.y2);
		}
	}
	else
	{
		pScreen->Colour(LC_WHITE, 24);
		pScreen->Rectangle(&a);
	}
}

void GBitmap::OnMouseClick(GMouse &m)
{
	if (!m.Down() && GetParent())
	{
		GDialog *Dlg = dynamic_cast<GDialog*>(GetParent());
		if (Dlg) Dlg->OnNotify(this, 0);
	}
}
*/

///////////////////////////////////////////////////////////////////////////////////////////
/*
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
	AskImage(true);
	return ImageList != NULL;
}
*/
