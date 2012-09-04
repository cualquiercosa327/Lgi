/*hdr
**      FILE:           GuiCommon.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           18/7/98
**      DESCRIPTION:    Common windows and controls
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GButton.h"

#define IDC_CALLBACK		100
#define IDC_EDIT			101

//////////////////////////////////////////////////////////////////////////////
GInput::GInput(GViewI *parent, const char *InitStr, const char *Msg, const char *Title, bool Password, GInputCallback callback, void *callbackparam)
{
	Callback = callback;
	CallbackParam = callbackparam;

	GText *Txt = new GText(-1, 5, 5, -1, -1, Msg);
	GDisplayString MsgDs(SysFont, ValidStr(InitStr)?InitStr:"A");
	int Dx = LgiApp->GetMetric(LGI_MET_DECOR_X) + 10;
	int Dy = LgiApp->GetMetric(LGI_MET_DECOR_Y);
	
	int ContextX = 200;
	ContextX = max(ContextX, MsgDs.X() + 40);
	ContextX = min(ContextX, (int)(GdcD->X() * 0.8));
	int EditX = ContextX;
	int CallbackX = callback ? GBUTTON_MIN_X + 20 : 0;
	ContextX = max(ContextX, Txt->X() + CallbackX);

	GRect r(0, 0, ContextX + CallbackX + Dx, 70 + Txt->Y() + Dy);

	SetParent(parent);
	Name(Title);
	SetPos(r);
	MoveToCenter();

	GRect c = GetClient();
	Children.Insert(Txt);
	Children.Insert(Edit = new GEdit(IDC_EDIT, 5, Txt->GetPos().y2 + 5, EditX - 1, MsgDs.Y()+7, InitStr));
	if (Edit)
	{
		Edit->Password(Password);
		Edit->Focus(true);
		if (Callback)
		{
			GRect e = Edit->GetPos();
			Children.Insert(new GButton(IDC_CALLBACK, c.X() - (CallbackX-5) - 6, e.y1, CallbackX-5, e.Y()-1, "..."));
		}
	}

	GButton *Ok = new GButton(IDOK, 0, 0, -1, -1, LgiLoadString(L_BTN_OK, "Ok"));
	GButton *Cancel = new GButton(IDCANCEL, 0, 0, -1, -1, LgiLoadString(L_BTN_CANCEL, "Cancel"));
	int BtnX = max(Ok->X(), Cancel->X());
	int BtnY = Edit->GetPos().y2 + 11;
	
	GRect p = Cancel->GetPos();
	p.x2 = p.x1 + BtnX - 1;
	p.Offset(c.X() - Cancel->X() - 5, BtnY);
	Cancel->SetPos(p);
	
	p = Ok->GetPos();
	p.x2 = p.x1 + BtnX - 1;
	p.Offset(Cancel->GetPos().x1 - p.X() - 5, BtnY);
	Ok->SetPos(p);

	Children.Insert(Ok);
	Children.Insert(Cancel);
	Ok->Default(true);
}

int GInput::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_CALLBACK:
		{
			if (Callback)
				Callback(this, Edit, CallbackParam);
			break;
		}
		case IDOK:
		{
			Str.Reset(NewStr(Edit->Name()));
			// fall thru
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId() == IDOK);
			break;
		}
	}

	return 0;
}

