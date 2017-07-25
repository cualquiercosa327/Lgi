/*hdr
 **      FILE:           GWidgets.cpp
 **      AUTHOR:         Matthew Allen
 **      DATE:           30/12/2006
 **      DESCRIPTION:    Mac dialog components
 **
 **      Copyright (C) 2006 Matthew Allen
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
#define GreyBackground()

struct GDialogPriv
{
	bool IsModal;
	int ModalStatus;
};

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog()
: ResObject(Res_Dialog)
{
	d = new GDialogPriv;
	Name("Dialog");
	d->IsModal = false;
	d->ModalStatus = -1;
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
		EndModeless(0);
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

int GDialog::DoModal(OsView OverideParent)
{
	d->ModalStatus = 0;
	
	if (Wnd && Attach(0))
	{
		GWindow *Owner = GetParent() ? GetParent()->GetWindow() : 0;
		if (Owner)
		{
			GRect Pr = Owner->GetPos();
			GRect Mr = GetPos();
			Mr.Offset(	Pr.x1 + (Pr.X() - Mr.X()) / 2 - Mr.x1,
					  Pr.y1 + (Pr.Y() - Mr.Y()) / 2 - Mr.y1);
			SetPos(Mr);
			Owner->SetChildDialog(this);
		}
		
		d->IsModal = true;
		AttachChildren();
		Visible(true);
		
		// RunAppModalLoopForWindow(Wnd);
		
		if (Owner) Owner->SetChildDialog(0);
	}
	
	return d->ModalStatus;
}

void GDialog::EndModal(int Code)
{
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = Code;
		
		// QuitAppModalLoopForWindow(Wnd);
	}
	else
	{
		LgiAssert(0);
	}
}

int GDialog::DoModeless()
{
	d->IsModal = false;
	if (Attach(0))
	{
		AttachChildren();
		Visible(true);
	}
	return 0;
}

extern GButton *FindDefault(GView *w);

GMessage::Result GDialog::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CLOSE:
		{
			printf("M_CLOSE received... Fixme!\n");
			break;
		}
	}
	
	return GView::OnEvent(Msg);
}

void GDialog::EndModeless(int Code)
{
	GWindow::Quit(Code);
}

void GDialog::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
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
			size_t Len = e ? e-s : strlen(s);
			
			GDisplayString ds(SysFont, s, Len);
			Pt.x = max(Pt.x, ds.X());
			Pt.y += y;
		}
	}
	
	return Pt;
}

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :	ResObject(Res_Slider)
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
			n->OnNotify(this, (int)Val);
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
	return GView::OnEvent(Msg);
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
