// \file
// \author Matthew Allen (fret@memecode.com)
// \brief Native Win32 Edit Control

#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GEdit.h"

///////////////////////////////////////////////////////////////////////////////////////////
class GEditPrivate
{
public:
	bool IgnoreNotify;
	DWORD ParentProc;

	GEditPrivate()
	{
		IgnoreNotify = true;
		ParentProc = 0;
	}

	~GEditPrivate()
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
GEdit::GEdit(int id, int x, int y, int cx, int cy, const char *name) :
	GControl(LGI_EDITBOX),
	ResObject(Res_EditBox)
{
	d = new GEditPrivate;
	GdcPt2 Size = SizeOfStr((name)?name:(char*)"A");
	if (cx < 0) cx = Size.x + 6;
	if (cy < 0) cy = Size.y + 4;

	SetId(id);
	Sunken(true);
	if (name) Name(name);
	GRect r(x, y, x+max(cx, 10), y+max(cy, 10));
	SetPos(r);
	SetStyle(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP);
	SetFont(SysFont);
}

GEdit::~GEdit()
{
	DeleteObj(d);
}

void GEdit::Select(int Start, int Len)
{
	if (_View)
	{
		SendMessage(_View, EM_SETSEL, Start, Start+Len-1);
	}
}

bool GEdit::GetSelection(int &Start, int &Len)
{
	if (!_View)
		return false;

	DWORD s = 0, e = 0;
	SendMessage(_View, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
	if (s == e)
		return false;

	Start = s;
	Len = e - s;
	return true;
}

bool GEdit::MultiLine()
{
	return TestFlag(GetStyle(), ES_MULTILINE);
}

void GEdit::MultiLine(bool m)
{
	int Flags = ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN;
	if (m)
		SetStyle(GetStyle() | Flags);
	else
		SetStyle(GetStyle() & ~Flags);
}

bool GEdit::Password()
{
	return TestFlag(GetStyle(), ES_PASSWORD);
}

void GEdit::Password(bool m)
{
	if (m)
		SetStyle(GetStyle() | ES_PASSWORD);
	else
		SetStyle(GetStyle() & ~ES_PASSWORD);
}

char *GEdit::Name()
{
	if (Handle())
	{
		GBase::Name(0);

		char *s = GControl::Name();
		if (s)
		{
			char *i = s;
			char *o = s;
			while (*i)
			{
				if (*i != '\r')
				{
					*o++ = *i;
				}
				i++;
			}
			*o = 0;

		}

		return s;
	}
	
	return GBase::Name();
}

bool GEdit::Name(const char *n)
{
	bool Old = d->IgnoreNotify;
	d->IgnoreNotify = true;

	char *Mem = 0;

	if (n && strchr(n, '\n'))
	{
		GStringPipe p(256);
		for (char *s = (char*)n; s && *s; )
		{
			char *nl = strchr(s, '\n');
			if (nl)
			{
				p.Write(s, nl-s);
				p.Write("\r\n", 2);
				s = nl + 1;
			}
			else
			{
				p.Write(s, strlen(s));
				break;
			}
		}

		n = Mem = p.NewStr();
	}

	bool Status = GControl::Name(n);

	DeleteArray(Mem);

	d->IgnoreNotify = Old;
	return Status;
}

char16 *GEdit::NameW()
{
	return GControl::NameW();
}

bool GEdit::NameW(const char16 *s)
{
	bool Old = d->IgnoreNotify;
	d->IgnoreNotify = true;
	bool Status = GControl::NameW(s);
	d->IgnoreNotify = Old;
	return Status;
}

int GEdit::SysOnNotify(int Code)
{
	if (!d->IgnoreNotify)
	{
		int Len = SendMessage(Handle(), WM_GETTEXTLENGTH, 0, 0);
		char *Str = new char[Len+1];
		if (Str)
		{
			SendMessage(Handle(), WM_GETTEXT, (WPARAM) (Len+1), (LPARAM) Str);
			GBase::Name(Str);
			DeleteArray(Str);
		}

		if (_View)
		{
			SendNotify(0);
		}
	}

	return 0;
}

void GEdit::Value(int64 i)
{
	char Str[32];
	sprintf(Str, LGI_PrintfInt64, i);
	Name(Str);
}

int64 GEdit::Value()
{
	char *n = Name();
	return (n) ? atoi64(n) : 0;
}

#define EDIT_PROCESSING 1

GMessage::Result GEdit::OnEvent(GMessage *Msg)
{
	#if EDIT_PROCESSING
	switch (Msg->Msg)
	{
		case WM_DESTROY:
		{
			Name();
			_View = 0;
			break;
		}
		case WM_SETTEXT:
		{
			if (d->IgnoreNotify)
			{
				// return true;
			}
			break;
		}
		case WM_GETDLGCODE:
		{
			int Code = 0;

			Code = DLGC_WANTALLKEYS;
			return Code;
		}
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		{
			if (!MultiLine() &&
				(Msg->a == VK_UP || Msg->a == VK_DOWN))
			{
				GView::OnEvent(Msg);
				return 1;
			}

			if (SysOnKey(this, Msg))
			{
				return 0;
			}
			break;
		}
		case WM_KEYDOWN:
		{
			if (!MultiLine() &&
				(Msg->a == VK_UP || Msg->a == VK_DOWN))
			{
				GView::OnEvent(Msg);
				return 1;				
			}

			if (Msg->a == VK_TAB && MultiLine())
			{
				// this causes the multi-line Edit control to
				// insert tabs into the text and not move
				// to the next field.
				return 0;
			}

			if (tolower(Msg->a) == 'u')
			{
				if (GetKeyState(VK_CONTROL) & 0xFF00)
				{
					DWORD Start = 0, End = 0;
					SendMessage(_View, EM_GETSEL, (WPARAM) &Start, (LPARAM) &End);
					int Chars = End - Start;

					char16 *n = NameW();
					if (n)
					{
						if (GetKeyState(VK_SHIFT) & 0xFF00)
						{
							// upper case selection
							for (int i=Start; i<Start+End; i++)
							{
								n[i] = toupper(n[i]);
							}
						}
						else
						{
							// lower case selection
							for (int i=Start; i<Start+End; i++)
							{
								n[i] = tolower(n[i]);
							}
						}

						NameW(n);
						SendMessage(_View, EM_SETSEL, Start, End);
					}
				}
			}

			if (SysOnKey(this, Msg))
			{
				return 0;
			}
			break;
		}
		case WM_CHAR:
		{
			if ((Msg->a == VK_TAB || Msg->a == VK_RETURN) &&
				!MultiLine())
			{
				// single line edit controls make "click"
				// when they get a TAB char... this
				// avoids that.
				return 0;
			}
			break;
		}
	}
	#endif

	int Status = 	
	GControl::OnEvent(Msg);

	#if EDIT_PROCESSING
	switch (MsgCode(Msg))
	{
		case WM_CREATE:
		{
			d->IgnoreNotify = false;
			break;
		}
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
		case WM_MOUSEMOVE:
		{
			GView::OnEvent(Msg);
			break;
		}
	}
	#endif

	return Status;
}

bool GEdit::OnKey(GKey &k)
{
	if (!k.IsChar &&
		k.vkey == VK_RETURN)
	{
		if (k.Down())
		{
			SendNotify(k.c16);
		}

		if (MultiLine())
		{
			return true;
		}
	}

	return false;
}

int GEdit::GetCaret()
{
	DWORD Start, End = 0;
	if (_View)
        SendMessage(_View, EM_GETSEL, (GMessage::Param)&Start, (GMessage::Param)&End);
	return End;
}

void GEdit::SetCaret(int Pos)
{
	if (_View)
		SendMessage(_View, EM_SETSEL, Pos, Pos);
}
