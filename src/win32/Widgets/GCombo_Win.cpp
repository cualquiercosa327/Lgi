// \file
// \author Matthew Allen
// \brief Native Win32 Combo Box

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GVariant.h"
#include "GCombo.h"

GRect GCombo::Pad(8, 4, 24, 4);

class GComboPrivate
{
public:
	bool SortItems;
	int SubType;
	GAutoString Buffer;
	GRect Pos;
	
	// Initialization data
	bool Init;
	int InitVal;
	List<char> InitStrs;

	GComboPrivate()
	{
		SortItems = 0;
		SubType = GV_NULL;
		Init = false;
		InitVal = 0;
	}

	~GComboPrivate()
	{
		InitStrs.DeleteArrays();
	}
};

GCombo::GCombo(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_ComboBox)
{
	d = new GComboPrivate;
	Name(name);
	
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | CBS_DROPDOWNLIST);

	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = GWin32Class::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("COMBOBOX");
	else
		LgiAssert(!"No subclass?");
}

GCombo::~GCombo()
{
	DeleteObj(d);
}

void GCombo::OnAttach()
{
}

bool GCombo::Sort()
{
	return d->SortItems;
}

void GCombo::Sort(bool s)
{
	d->SortItems = s;
}

int GCombo::Sub()
{
	return d->SubType;
}

void GCombo::Sub(int Type)
{
	d->SubType = Type;
}

void GCombo::Value(int64 i)
{
	if (Handle())
	{
		SendMessage(Handle(), CB_SETCURSEL, i, 0);
	}
	else
	{
		d->InitVal = i;
	}
}

int64 GCombo::Value()
{
	if (Handle())
	{
		return SendMessage(Handle(), CB_GETCURSEL, 0, 0);
	}
	else
	{
		return d->InitVal;
	}
}

bool GCombo::Name(const char *n)
{
	return false;
}

char *GCombo::Name()
{
	return GView::Name();
}

GSubMenu *GCombo::GetMenu()
{
	LgiAssert(!"Impl me.");
	return 0;
}

void GCombo::SetMenu(GSubMenu *m)
{
	LgiAssert(!"Impl me.");
}

void GCombo::DoMenu()
{
	LgiAssert(!"Impl me.");
}

bool GCombo::Delete()
{
	return Delete(Value());
}

bool GCombo::Delete(int i)
{
	if (Handle())
	{
		return SendMessage(Handle(), CB_DELETESTRING, i, 0);
	}
	else
	{
		char *s = d->InitStrs[i];
		if (s)
		{
			d->InitStrs.Delete();
			DeleteArray(s);
			return true;
		}
	}

	return false;
}

bool GCombo::Delete(char *p)
{
	if (Handle())
	{
		int Index = SendMessage(Handle(), CB_FINDSTRINGEXACT, 0, (long)p);
		if (Index != CB_ERR)
		{
			return Delete(Index);
		}
	}

	return false;
}

bool GCombo::Insert(const char *p, int Index)
{
	bool Status = false;

	if (p)
	{
		if (Handle())
		{
			char *n = LgiToNativeCp(p);
			if (n)
			{
				if (Index < 0)
				{
					Index = Length();
				}

				Status = SendMessage(Handle(), CB_INSERTSTRING, Index, (long)n) == Index;
				DeleteArray(n);
			}
		}
		else
		{
			d->InitStrs.Insert(NewStr(p), Index);
			Status = true;
		}
	}

	return Status;
}

int GCombo::Length()
{
	return SendMessage(Handle(), CB_GETCOUNT, 0, 0);
}

char *GCombo::operator [](int i)
{
	if (_View)
	{
		LRESULT Len = SendMessage(_View, CB_GETLBTEXTLEN, 0, 0);
		if (Len != CB_ERR &&
			d->Buffer.Reset(new char[Len+1]))
		{
			LRESULT Ret = SendMessage(_View, CB_GETLBTEXT, i, 0);
			if (Ret != CB_ERR)
				return d->Buffer;
		}
		else return NULL;
	}

	return d->InitStrs[i];
}

GRect &GCombo::GetPos()
{
	return d->Pos;
}

bool GCombo::SetPos(GRect &p, bool Repaint)
{
	d->Pos = p;
	p.y2 = p.y1 + 200;
	return GControl::SetPos(p, Repaint);
}

int GCombo::SysOnNotify(int Msg, int Code)
{
	// LgiTrace("%s:%i - GCombo::SysOnNotify %i %i\n", _FL, Msg==WM_COMMAND, Code);
	
	if (Msg == WM_COMMAND &&
		Code == CBN_SELCHANGE)
	{
		SendNotify(Value());
	}
	
	return 0;
}

GMessage::Result GCombo::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		/*
		case WM_GETDLGCODE:
		{
			return CallWindowProc((WNDPROC)d->ComboClassProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg)) |
				DLGC_WANTTAB;
		}
		*/
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

	// int Status = CallWindowProc((WNDPROC)d->ComboClassProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg));
	GMessage::Result Status = GControl::OnEvent(Msg);
	
	switch (MsgCode(Msg))
	{
		case WM_PAINT:
		{
			if (!d->Init)
			{
				d->Init = true;
				int n = 0;
				for (char *s=d->InitStrs.First(); s; s=d->InitStrs.Next())
				{
					SendMessage(Handle(), CB_INSERTSTRING, n++, (long)s);
				}
				d->InitStrs.DeleteArrays();
				SetFont(SysFont);
				Value(d->InitVal);
			}
			break;
		}
	}

	return Status;
}

void GCombo::Empty()
{
	if (_View)
		SendMessage(_View, CB_RESETCONTENT, 0, 0);
	else
		d->InitStrs.DeleteArrays();
}