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

class GEditPrivate
{
public:
	bool FocusOnCreate;
	class _OsTextView *Edit;
	
	GEditPrivate()
	{
		FocusOnCreate = false;
	}
};

GEdit::GEdit(int id, int x, int y, int cx, int cy, char *name) :
	ResObject(Res_EditBox)
{
	_OsFontType Type;
	d = new GEditPrivate;
	d->Edit = new _OsTextView(this, &Type);

	GdcPt2 Size = SizeOfStr((name)?name:(char*)"A");
	if (cx < 0) cx = Size.x + 6;
	if (cy < 0) cy = Size.y + 4;

	if (d->Edit)
		d->Edit->SetMultiLine(false);
	SetId(id);
	Sunken(true);
	if (name) Name(name);

	GRect r(x, y, x+max(cx, 10), y+max(cy, 10));
	SetPos(r);
}

GEdit::~GEdit()
{
	DeleteObj(d->Edit);
	DeleteObj(d);
}

void GEdit::Select(int Start, int Len)
{
	if (d->Edit)
	{
		d->Edit->SetCursor(Start, false);
		d->Edit->SetCursor(Start + (Len > 0 ? Len : 0x7fffffff) - 1, true);
	}	
}

void GEdit::OnCreate()
{
	if (d->Edit)
	{
		d->Edit->SetParent(this);
		d->Edit->SetPos(GetClient());
		d->Edit->Attach(this);
		d->Edit->SetNotify(this);
		
		if (d->FocusOnCreate)
		{
			d->FocusOnCreate = false;
			d->Edit->Focus(true);
		}
		
		#ifdef XWIN
		// d->Edit->Handle()->wantKeys(Handle()->wantKeys());
		#endif
	}
}

int GEdit::OnNotify(GViewI *c, int f)
{
	if (c == d->Edit AND
		f == GTVN_DOC_CHANGED)
	{
		SendNotify();
	}

	return 0;
}

bool GEdit::Enabled()
{
	return d->Edit->Enabled();
}

void GEdit::Enabled(bool e)
{
	d->Edit->Enabled(e);
	d->Edit->SetReadOnly(!e);
}

void GEdit::Focus(bool f)
{
	if (d->Edit->IsAttached())
		d->Edit->Focus(f);
	else
		d->FocusOnCreate = f;
}

bool GEdit::Focus()
{
	return d->Edit->Focus();
}

bool GEdit::SetPos(GRect &p, bool Repaint)
{
	GView::SetPos(p, Repaint);

	if (d->Edit)
	{
		GRect c = GetClient();
		d->Edit->SetPos(c, Repaint);
	}
	else return false;
	
	return true;
}

char *GEdit::Name()
{
	return d->Edit ? d->Edit->Name() : 0;
}

bool GEdit::Name(char *n)
{
	return d->Edit ? d->Edit->Name(n) : false;
}

char16 *GEdit::NameW()
{
	return d->Edit ? d->Edit->NameW() : 0;
}

bool GEdit::NameW(char16 *n)
{
	return d->Edit ? d->Edit->NameW(n) : false;
}

bool GEdit::MultiLine()
{
	return d->Edit->GetMultiLine();
}

void GEdit::MultiLine(bool m)
{
	if (d->Edit) d->Edit->SetMultiLine(m);
}

void GEdit::Password(bool m)
{
	if (d->Edit) d->Edit->SetPassword(m);
}

int GEdit::GetCaret()
{
	return d->Edit ? d->Edit->GetCursor() : 0;
}

void GEdit::SetCaret(int i)
{
	if (d->Edit) d->Edit->SetCursor(i, false);
}

void GEdit::Value(int64 i)
{
	char Str[32];
	sprintf(Str, "%i", (int)i);
	Name(Str);
}

int64 GEdit::Value()
{
	char *n = Name();
	return (n) ? atoi(n) : 0;
}

bool GEdit::OnKey(GKey &k)
{
	return false;
}

int GEdit::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CUT:
		case M_COPY:
		case M_PASTE:
		{
			printf("d->edit=%p c/c/p msg\n", d->Edit);
			if (d->Edit)
				return d->Edit->OnEvent(Msg);
			break;
		}
	}
	
	return GView::OnEvent(Msg);
}

