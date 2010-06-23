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

///////////////////////////////////////////////////////////////////////////////////////////
#define GreyBackground()

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog()
	: ResObject(Res_Dialog)
{
	Name("Dialog");
	IsModal = false;
	_SetDynamic(false);
}

GDialog::~GDialog()
{
}

void GDialog::OnPosChange()
{
}

bool GDialog::LoadFromResource(int Resource)
{
	char n[256];
	GRect p;

	bool Status = GLgiRes::LoadFromResource(Resource, Children, &p, n);
	if (Status)
	{
		Name(n);
		SetPos(p);
	}
	return Status;
}

bool GDialog::OnRequestClose(bool OsClose)
{
	if (IsModal)
	{
		EndModal(0);
		return false;
	}

	return true;
}

int GDialog::DoModal(OsView OverideParent)
{
	ModalStatus = 0;

	if (Wnd && Attach(0))
	{
		GWindow *Owner = GetParent() ? GetParent()->GetWindow() : 0;
		if (Owner) Owner->SetChildDialog(this);
		
		IsModal = true;
		AttachChildren();
		Visible(true);
		
		RunAppModalLoopForWindow(Wnd);

		if (Owner) Owner->SetChildDialog(0);
	}

	return ModalStatus;
}

void GDialog::EndModal(int Code)
{
	if (IsModal)
	{
		IsModal = false;
		ModalStatus = Code;
		
		QuitAppModalLoopForWindow(Wnd);
	}
	else
	{
		LgiAssert(0);
	}
}

int GDialog::DoModeless()
{
	IsModal = false;
	if (Attach(0))
	{
		AttachChildren();
		Visible(true);
	}
	return 0;
}

extern GButton *FindDefault(GView *w);

int GDialog::OnEvent(GMessage *Msg)
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
	Quit(Code);
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

int GControl::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
	}
	return 0;
}

GdcPt2 GControl::SizeOfStr(char *Str)
{
	int y = SysFont->GetHeight();
	GdcPt2 Pt(0, 0);

	if (Str)
	{
		char *e = 0;
		for (char *s = Str; s AND *s; s = e?e+1:0)
		{
			e = strchr(s, '\n');
			int Len = e ? (int)e-(int)s : strlen(s);

			GDisplayString ds(SysFont, s, Len);
			Pt.x = max(Pt.x, ds.X());
			Pt.y += y;
		}
	}

	return Pt;
}

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, char *name, bool vert) :	ResObject(Res_Slider)
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

int GSlider::OnEvent(GMessage *Msg)
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
	LgiWideBorder(pDC, r, SUNKEN);
	
	if (Min <= Max)
	{
		int x = Val * r.X() / (Max-Min);
		Thumb.ZOff(5, 9);
		Thumb.Offset(r.x1 + x - 3, y - 5);
		GRect b = Thumb;
		LgiWideBorder(pDC, b, RAISED);
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
		if (Rx > 0 AND Max >= Min)
		{
			int x = m.x - Tx;
			int v = x * (Max-Min) / Rx;
			Value(v);
		}
	}
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
	AskImage(true);
	return ImageList != NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////
ThemeButtonDrawUPP LgiLabelUPP = 0;

void LgiLabelProc(	const Rect *r,
					ThemeButtonKind kind,
					const ThemeButtonDrawInfo *info,
					UInt32 UserData,
					SInt16 depth,
					Boolean ColourDev)
{
	GLabelData *d = (GLabelData*)UserData;
	if (d)
	{
		char *Str = d->Ctrl->Name();
		if (Str)
		{
			CFStringRef s = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)Str, strlen(Str), kCFStringEncodingUTF8, false);
			if (d->Ctrl->Enabled())
				d->pDC->Colour(LC_TEXT, 24);
			else
				d->pDC->Colour(LC_LOW, 24);

			Rect rc = *r;
			rc.top += d->r.y1;
			// rc.right -= 20;

			DrawThemeTextBox(	s,
								kThemeSmallSystemFont,
								info->state,
								false,
								&rc,
								d->Justification,
								d->pDC->Handle());
			CFRelease(s);
		}
	}
}
