// \file
// \author Matthew Allen
// \brief Native Win32 Combo Box

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GVariant.h"
#include "GCombo.h"

static char *ClassName = "LgiCombo";

class GComboPrivate
{
public:
	bool SortItems;
	int SubType;
	DWORD ComboClassProc;
	
	// Initialization data
	bool Init;
	int InitVal;
	List<char> InitStrs;

	GComboPrivate()
	{
		SortItems = 0;
		SubType = GV_NULL;
		ComboClassProc = 0;
		Init = false;
		InitVal = 0;
	}

	~GComboPrivate()
	{
		InitStrs.DeleteArrays();
	}
};

GCombo::GCombo(int id, int x, int y, int cx, int cy, char *name) :
	ResObject(Res_ComboBox)
{
	d = new GComboPrivate;
	Name(name);
	GRect r(x, y, x+cx, y+200);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetClass("COMBOBOX");
	SetStyle(GetStyle() | CBS_DROPDOWNLIST);
}

GCombo::~GCombo()
{
	DeleteObj(d);
}

void GCombo::OnAttach()
{
	#ifdef SKIN_MAGIC

	d->ComboClassProc = GetWindowLong(Handle(), GWL_WNDPROC);
	SetWindowLong(Handle(), GWL_WNDPROC, (DWORD)GWin32Class::Redir);
	SetWindowLong(Handle(), GWL_USERDATA, (DWORD)(GViewI*)this);

	#endif
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

void GCombo::Value(int i)
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

int GCombo::Value()
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

bool GCombo::Name(char *n)
{
	return false;
}

char *GCombo::Name()
{
	return GView::Name();
}

GSubMenu *GCombo::GetMenu()
{
	return 0;
}

void GCombo::SetMenu(GSubMenu *m)
{
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

bool GCombo::Insert(char *p, int Index)
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
					Index = GetItems();
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

int GCombo::GetItems()
{
	return SendMessage(Handle(), CB_GETCOUNT, 0, 0);
}

char *GCombo::operator [](int i)
{
	return 0;
}

void GCombo::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
}

int GCombo::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_GETDLGCODE:
		{
			return CallWindowProc((WNDPROC)d->ComboClassProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg)) |
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
	}

	int Status = CallWindowProc((WNDPROC)d->ComboClassProc, Handle(), MsgCode(Msg), MsgA(Msg), MsgB(Msg));
	
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

void GCombo::OnMouseClick(GMouse &m)
{
}

bool GCombo::OnKey(GKey &k)
{
	return false;
}

void GCombo::OnFocus(bool f)
{
}

void GCombo::OnPaint(GSurface *pDC)
{
}

bool GCombo::SetPos(GRect &p, bool Repaint)
{
	p.y2 = p.y1 + 200;
	return GView::SetPos(p, Repaint);
}
