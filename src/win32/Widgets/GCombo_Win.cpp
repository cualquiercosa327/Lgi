// \file
// \author Matthew Allen
// \brief Native Win32 Combo Box

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GVariant.h"
#include "GCombo.h"

#define DEBUG_COMBOBOX	1365

GRect GCombo::Pad(8, 4, 24, 4);

class GComboPrivate
{
public:
	bool SortItems;
	int SubType;
	GAutoString Buffer;
	GRect Pos;
	
	// Initialization data
	size_t Len;
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
	SetStyle(GetStyle() | CBS_DROPDOWNLIST | CBS_DISABLENOSCROLL);

	#if 0
	GString Cls = WC_COMBOBOX;
	SetClassW32(Cls);
	#else
	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = GWin32Class::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("COMBOBOX");
	else
		LgiAssert(!"No subclass?");
	#endif
}

GCombo::~GCombo()
{
	DeleteObj(d);
}

void GCombo::OnAttach()
{
	/*
	if (!d->Init)
	{
		d->Init = true;
		for (unsigned n=0; n<d->Strs.Length(); n++)
		{
			GAutoWString s(Utf8ToWide(d->Strs[n]));
			SendMessage(Handle(), CB_INSERTSTRING, n, (LPARAM) (s ? s.Get() : L"(NULL)"));
		}

		SetFont(SysFont);
		
		if (d->Name)
			Name(d->Name);
		else
			Value(d->Value);
	}
	*/
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
		LgiTrace("GCombo::Value(" LPrintfInt64 ") this=%p, hnd=%p strs=%i\n",
			i, this, _View, d->Strs.Length());
	#endif

	d->Value = i;
	
	if (Handle())
	{
		if (d->Strs.Length() == 0 || i < 0)
			i = 0;
		if (i >= (ssize_t)d->Strs.Length())
			i = d->Strs.Length() - 1;

		SendMessage(Handle(), CB_SETCURSEL, i, 0);
	}
}

int64 GCombo::Value()
{
	if (Handle())
	{
		LRESULT r = SendMessage(Handle(), CB_GETCURSEL, 0, 0);
		if (r != CB_ERR)
		{
			d->Value = r;
			if (d->Strs.Length() == 0 || d->Value < 0)
				d->Value = 0;
			else if (d->Value >= (ssize_t)d->Strs.Length())
				d->Value = d->Strs.Length() - 1;
		}
	}

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("GCombo::Value()=" LPrintfInt64 " this=%p, hnd=%p strs=%i\n",
			d->Value, this, _View, d->Strs.Length());
	#endif
	
	return d->Value;
}

bool GCombo::Name(const char *n)
{
	int Idx = IndexOf(n);

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("GCombo::Name(%s) this=%p, hnd=%p idx=%i strs=%i\n",
			n, this, _View, Idx, d->Strs.Length());
	#endif

	if (Idx < 0)
	{
		LgiTrace("%s:%i - Can't find '%s' to set combo index.\n", _FL, n);
		return false;
	}

	Value(Idx);
	return true;
}

char *GCombo::Name()
{
	if (d->Value >= 0 &&
		d->Value < (ssize_t)d->Strs.Length())
	{
		char *s = d->Strs[d->Value];

		#if defined(DEBUG_COMBOBOX)
		if (DEBUG_COMBOBOX==GetId())
			LgiTrace("GCombo::Name()=" LPrintfInt64 "=%s this=%p, hnd=%p strs=%i\n",
				d->Value, s, this, _View, d->Strs.Length());
		#endif

		return s;
	}

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("GCombo::Name() " LPrintfInt64 "=out of range this=%p, hnd=%p strs=%i\n",
			d->Value, this, _View, d->Strs.Length());
	#endif
	
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
		LgiTrace("%s:%i %p.%p Delete() Idx=" LPrintfInt64 "\n", _FL, this, _View, Idx);
	#endif

	return Delete(Idx);
}

bool GCombo::Delete(size_t i)
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

	if (_View && d->Init)
	{
		GAutoWString n(Utf8ToWide(p));
		if (!n)
			return false;

		if (Index < 0)
			Index = (int)d->Len;

		LRESULT r = SendMessage(Handle(), CB_INSERTSTRING, Index, (LPARAM)n.Get());
		if (r == CB_ERR)
			return false;
			
		d->Len++;
	}

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("GCombo::Insert(%s, %i) this=%p, hnd=%p strs=%i\n",
			p, Index, this, _View, d->Strs.Length());
	#endif
	d->Strs.AddAt(Index, p);

	return true;
}

size_t GCombo::Length()
{
	if (_View && d->Init)
		d->Len = SendMessage(_View, CB_GETCOUNT, 0, 0);
	
	return d->Strs.Length();
}

char *GCombo::operator [](int i)
{
	if (i >= 0 && i < d->Strs.Length())
		return d->Strs[i];
	
	LgiAssert(!"Out of range combo string access.");
	return NULL;
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
	
	if (Msg == WM_COMMAND)
	{
		switch (Code)
		{
			case CBN_SELCHANGE:
			{
				uint64 Old = d->Value;
				if (Value() != Old)
					SendNotify((int)d->Value);
				break;
			}
			case CBN_DROPDOWN:
			{
				Focus(true);
				break;
			}
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
		case WM_CREATE:
		{
			SetFont(SysFont);
			break;
		}
		case WM_DESTROY:
		{
			Value();
			#if defined(DEBUG_COMBOBOX)
			if (DEBUG_COMBOBOX==GetId())
				LgiTrace("%s:%i %p.%p WM_DESTROY v=" LPrintfInt64 ")\n", _FL, this, _View, d->Value);
			#endif
			break;
		}
		#if 1
		case WM_MOUSEWHEEL:
		{
			// Force the mouse wheel to do something useful
			LRESULT c = SendMessage(_View, CB_GETCURSEL, 0, 0);
			LRESULT items = SendMessage(_View, CB_GETCOUNT, 0, 0);
			int16 delta = (int16) (Msg->A() >> 16);
			int new_cur = (int) (c - (delta / WHEEL_DELTA));
			SendMessage(_View, CB_SETCURSEL, limit(new_cur, 0, items-1), 0); 
			break;
		}
		#endif
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
					GAutoWString s(Utf8ToWide(d->Strs[n]));
					SendMessage(Handle(), CB_INSERTSTRING, n, (LPARAM) (s ? s.Get() : L"(NULL)"));
				}

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

bool GCombo::OnKey(GKey &k)
{
	switch (k.vkey)
	{
		case VK_UP:
		case VK_DOWN:
			return true;
	}
	
	return false;
}
