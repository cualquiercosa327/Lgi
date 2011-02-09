// \file
// \author Matthew Allen (fret@memecode.com)
#include "Lgi.h"
#include "GEdit.h"

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

/*
class _OsTextView : public GTextView3
{
	GEdit *Edit;
	bool Multiline;
	bool Password;

	bool SetScrollBars(bool x, bool y)
	{
		if (Multiline)
		{
			return GTextView3::SetScrollBars(x, y);
		}

		return false;
	}

public:
	_OsTextView(GEdit *e, GFontType *type) :
		GTextView3(-1, 0, 0, 100, 100, type)
	{
		Edit = e;
		Multiline = false;
		Password = false;
		SetUrlDetect(false);
		SetWrapType(TEXTED_WRAP_NONE);

		GRect m = GetMargin();
		m.y1 = 0;
		SetMargin(m);
		Sunken(false);
		Raised(false);
		_BorderSize = 0;
	}

	bool GetMultiLine()
	{
		return Multiline;
	}
	
	void SetMultiLine(bool m)
	{
		Multiline = m;
		CanScrollX = !m;
	}

	bool DoGoto()
	{
		return Multiline ? GTextView3::DoGoto() : false;
	}

	bool DoFind()
	{
		return Multiline ? GTextView3::DoFind() : false;
	}
	
	bool DoFindNext()
	{
		return Multiline ? GTextView3::DoFindNext() : false;
	}

	bool DoReplace()
	{
		return Multiline ? GTextView3::DoReplace() : false;
	}

	void SetPassword(bool p)
	{
		Password = p;
		SetObscurePassword(p);
	}

	bool OnKey(GKey &k)
	{
		if (Edit->OnKey(k))
			return true;

		if (!Multiline &&
			(k.vkey == VK_TAB || k.c16 == '\t' || k.c16 == '\r'))
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

	void OnEnter(GKey &k)
	{
		if (Multiline)
		{
			GTextView3::OnEnter(k);
		}
		else
		{
			Edit->SendNotify(VK_RETURN);
		}
	}
};
*/

class GEditPrivate
{
public:
	bool FocusOnCreate;
	bool Multiline;
	bool Password;
	// class _OsTextView *Edit;
	
	GEditPrivate()
	{
		FocusOnCreate = false;
	}
};

GEdit::GEdit(int id, int x, int y, int cx, int cy, char *name) :
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

	GDisplayString Ds(SysFont, (name)?name:(char*)"A");
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

void GEdit::SendNotify(int Data)
{
	printf("GEdit::SendNotify(%i)\n", Data);
	if (Data == GTVN_DOC_CHANGED)
		return GTextView3::SendNotify(0);
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
	if (!d->Multiline &&
		(k.vkey == VK_TAB || k.c16 == '\t' || k.c16 == '\r'))
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
