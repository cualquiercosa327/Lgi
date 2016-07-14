// \file
// \author Matthew Allen
// \brief Native Win32 Combo Box

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GVariant.h"
#include "GCombo.h"

#define DEBUG_COMBOBOX	362

GRect GCombo::Pad(8, 4, 24, 4);

class GComboPrivate
{
public:
	bool SortItems;
	int SubType;
	GAutoString Buffer;
	GRect Pos;
	
	// Initialization data
	int Len;
	bool Init;
	int64 Value;
	GString Name;
	GArray<GString> Strs;

	GComboPrivate()
	{
		Len = 0;
		SortItems = 0;
		SubType = GV_NULL;
		Init = false;
		Value = 0;
	}
};

GCombo::GCombo(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_ComboBox)
{
	d = new GComboPrivate;
	if (ValidStr(name))
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
	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p SetValue("LGI_PrintfInt64")\n", _FL, this, _View, i);
	#endif

	if (Handle())
	{
		SendMessage(Handle(), CB_SETCURSEL, i, 0);
	}
	else if (i != d->Value)
	{
		// LgiTrace("GCombo::Value %i->%i\n", (int)d->Value, (int)i);
		d->Value = i;
	}
}

int64 GCombo::Value()
{
	if (Handle())
		d->Value = SendMessage(Handle(), CB_GETCURSEL, 0, 0);

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p GetValue("LGI_PrintfInt64")\n", _FL, this, _View, d->Value);
	#endif
	
	return d->Value;
}

bool GCombo::Name(const char *n)
{
	int Idx = IndexOf(n);

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p SetName(%s) Idx=%i\n", _FL, this, _View, n, Idx);
	#endif

	if (Idx < 0)
		return false;

	Value(Idx);
	return true;
}

char *GCombo::Name()
{
	if (d->Value >= 0 && d->Value < d->Strs.Length())
	{
		char *s = d->Strs[d->Value];
		LgiTrace("GCombo Name '%s'\n", s);
		return s;
	}
	
	LgiTrace("GCombo Name out of range %i %i\n", (int)d->Value, d->Strs.Length());
	return NULL;
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
	int64 Idx = Value();
	
	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p Delete() Idx="LGI_PrintfInt64"\n", _FL, this, _View, Idx);
	#endif

	return Delete(Idx);
}

bool GCombo::Delete(int i)
{
	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p Delete(%i)\n", _FL, this, _View, i);
	#endif

	if (Handle())
	{
		LRESULT r = SendMessage(Handle(), CB_DELETESTRING, i, 0);
		if (r == CB_ERR)
			return false;
		d->Len = r;
	}			

	if (i >= 0 && i < d->Strs.Length())
	{
		d->Strs.DeleteAt(i, true);
		return true;
	}
	return false;
}

bool GCombo::Delete(char *p)
{
	int Idx = IndexOf(p);

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p Delete(%s) Idx=%i\n", _FL, this, _View, p, Idx);
	#endif

	if (Idx < 0)
		return false;

	Value(Idx);
	return true;
}

bool GCombo::Insert(const char *p, int Index)
{
	if (!p)
		return false;

	if (_View)
	{
		GAutoWString n(LgiNewUtf8To16(p));
		if (!n)
			return false;

		if (Index < 0)
			Index = d->Len;

		LRESULT r = SendMessage(Handle(), CB_INSERTSTRING, Index, (LPARAM)n.Get());
		if (r == CB_ERR)
			return false;
			
		d->Len++;
	}

	d->Strs.AddAt(Index, p);

	return true;
}

int GCombo::Length()
{
	if (_View)
		d->Len = SendMessage(_View, CB_GETCOUNT, 0, 0);
	return d->Len;
}

char *GCombo::operator [](int i)
{
	/*
	if (_View)
	{
		LRESULT Len = SendMessage(_View, CB_GETLBTEXTLEN, 0, 0);
		if (Len != CB_ERR &&
			d->Buffer.Reset(new char[Len+1]))
		{
			LRESULT Ret = SendMessage(_View, CB_GETLBTEXT, i, (LPARAM)d->Buffer.Get());
			if (Ret != CB_ERR)
				return d->Buffer;
		}
		else return NULL;
	}
	*/

	return d->Strs[i];
}

int GCombo::IndexOf(const char *str)
{
	if (!ValidStr(str))
		return -1;
	
	for (unsigned i=0; i<d->Strs.Length(); i++)
	{
		if (!_stricmp(str, d->Strs[i]))
			return i;
	}
	
	return -1;
}

bool GCombo::SetPos(GRect &p, bool Repaint)
{
	if (p == Pos)
		return true;
	
	d->Pos = p;
	d->Pos.y2 = d->Pos.y1 + 200;
	bool b = GControl::SetPos(d->Pos, Repaint);
	if (b)
		Pos = p;
	
	return b;
}

int GCombo::SysOnNotify(int Msg, int Code)
{
	// LgiTrace("%s:%i - GCombo::SysOnNotify %i %i\n", _FL, Msg==WM_COMMAND, Code);
	
	if
	(
		Msg == WM_COMMAND
		&&
		Code == CBN_SELCHANGE
	)
	{
		uint64 Old = d->Value;
		if (Value() != Old)
		{
			SendNotify(d->Value);
		}
	}
	
	return 0;
}

GMessage::Result GCombo::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
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
		case WM_DESTROY:
		{
			Value();
			#if defined(DEBUG_COMBOBOX)
			if (DEBUG_COMBOBOX==GetId())
				LgiTrace("%s:%i %p.%p WM_DESTROY v="LGI_PrintfInt64")\n", _FL, this, _View, d->Value);
			#endif
			break;
		}
	}

	GMessage::Result Status = GControl::OnEvent(Msg);
	
	switch (MsgCode(Msg))
	{
		case WM_PAINT:
		{
			if (!d->Init)
			{
				d->Init = true;
				for (unsigned n=0; n<d->Strs.Length(); n++)
				{
					GAutoWString s(LgiNewUtf8To16(d->Strs[n]));
					SendMessage(Handle(), CB_INSERTSTRING, n, (LPARAM)s.Get());
				}

				SetFont(SysFont);
				
				if (d->Name)
					Name(d->Name);
				else
					Value(d->Value);
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

	d->Strs.Length(0);
}