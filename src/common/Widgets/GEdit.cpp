// \file
// \author Matthew Allen (fret@memecode.com)
#include "Lgi.h"
#include "GEdit.h"
#include "GClipBoard.h"
#include "GDisplayString.h"

///////////////////////////////////////////////////////////////////////////////////////////
// Edit
#include "GTextView3.h"

class _OsFontType : public GFontType
{
public:
	GFont *Create(int Param = 0)
	{
		return SysFont;
	}
};

static _OsFontType SysFontType;


class GEditPrivate
{
public:
	bool FocusOnCreate;
	bool Multiline;
	bool Password;
	
	GEditPrivate()
	{
		FocusOnCreate = false;
	}
};

GEdit::GEdit(int id, int x, int y, int cx, int cy, const char *name) :
	#if WIN32NATIVE
	ResObject(Res_EditBox)
	#else
	GTextView3(id, x, y, cx, cy, &SysFontType)
	#endif
{
	#if !WIN32NATIVE
	_ObjName = Res_EditBox;
	SetUrlDetect(false);
	SetWrapType(TEXTED_WRAP_NONE);
	#endif
	_OsFontType Type;
	d = new GEditPrivate;

	GDisplayString Ds(SysFont, (char*)(name?name:"A"));
	if (cx < 0) cx = Ds.X() + 6;
	if (cy < 0) cy = Ds.Y() + 4;

	d->Multiline = false;
	d->Password = false;

	Sunken(true);
	if (name) Name(name);

	GRect r(x, y, x+max(cx, 10), y+max(cy, 10));
	SetPos(r);
}

GEdit::~GEdit()
{
	DeleteObj(d);
}

void GEdit::SetEmptyText(const char *EmptyText)
{
	LgiAssert(!"Impl me.");
}

void GEdit::SendNotify(int Data)
{
	if (Data == GTVN_DOC_CHANGED)
		return GTextView3::SendNotify(0);
	else if (Data == VK_RETURN)
		return GTextView3::SendNotify(Data);
}

bool GEdit::GetSelection(int &Start, int &Len)
{
	int Sel = GetCursor(false);
	Start = GetCursor();
	if (Sel < Start)
		Len = Start - Sel + 1;
	else
		Len = Sel - Start + 1;
	return true;
}

void GEdit::Select(int Start, int Len)
{
	SetCursor(Start, false);
	SetCursor(Start + (Len > 0 ? Len : 0x7fffffff) - 1, true);
}

int GEdit::GetCaret()
{
	return GetCursor();
}

void GEdit::SetCaret(int i)
{
	SetCursor(i, false);
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
	return (n) ? atoi(n) : 0;
}

bool GEdit::MultiLine()
{
	return d->Multiline;
}

void GEdit::MultiLine(bool m)
{
	d->Multiline = m;
}

bool GEdit::Password()
{
	return d->Password;
}

void GEdit::Password(bool m)
{
	SetObscurePassword(d->Password = m);
}

bool GEdit::OnKey(GKey &k)
{
	if
	(
		!d->Multiline &&
		(
			#ifdef VK_TAB
			k.vkey == VK_TAB ||
			#endif
			k.c16 == '\t' ||
			k.c16 == '\r'
		)
	)
	{
		if (k.c16 == '\r')
		{
			GTextView3::OnKey(k);
		}
		
		return false;
	}

	bool Status = GTextView3::OnKey(k);
	return Status;
}

void GEdit::OnEnter(GKey &k)
{
	if (d->Multiline)
	{
		GTextView3::OnEnter(k);
	}
	else
	{
		SendNotify(VK_RETURN);
	}
}

bool GEdit::Paste()
{
	GClipBoard Clip(this);

	GAutoWString Mem;
	char16 *t = Clip.TextW();
	if (!t) // ala Win9x
	{
		char *s = Clip.Text();
		if (s)
		{
			Mem.Reset(LgiNewUtf8To16(s));
			t = Mem;
		}
	}

	if (!t)
		return false;

	if (SelStart >= 0)
	{
		DeleteSelection();
	}

	// remove '\r's
	char16 *in = t, *out = t;
	for (; *in; in++)
	{
		if (*in == '\n')
		{
			if (d->Multiline)
				*out++ = *in;
		}
		else if (*in != '\r')
		{
			*out++ = *in;
		}
	}
	*out++ = 0;

	// insert text
	int Len = StrlenW(t);
	Insert(Cursor, t, Len);
	SetCursor(Cursor+Len, false, true); // Multiline
	
	return true;
}

