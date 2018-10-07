// \file
// \author Matthew Allen
// \brief Native Win32 Radio Group and Button

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GRadioGroup.h"
#include "GDisplayString.h"
#include "GNotifications.h"

#define RADIO_GRID  2
static int PadXPx = 30;
#ifdef MAC
static int PadYPx = 6;
#else
static int PadYPx = 4;
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// Radio group
class GRadioGroupPrivate
{
public:
	static int NextId;
	int64 InitVal;
	int MaxLayoutWidth;
    LHashTbl<PtrKey<void*>,GViewLayoutInfo*> Info;

	GRadioGroupPrivate()
	{
		InitVal = 0;
		MaxLayoutWidth = 0;
	}

	~GRadioGroupPrivate()
	{
        Info.DeleteObjects();
	}
};

int GRadioGroupPrivate::NextId = 10000;

GRadioGroup::GRadioGroup(int id, int x, int y, int cx, int cy, const char *name, int Init)
	: ResObject(Res_Group)
{
	d = new GRadioGroupPrivate;
	d->InitVal = Init;

	Name(name);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_GROUPBOX | WS_GROUP | WS_CLIPCHILDREN);

	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = GWin32Class::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LgiAssert(!"No subclass?");
}

GRadioGroup::~GRadioGroup()
{
	DeleteObj(d);
}

void GRadioGroup::OnAttach()
{
}

GMessage::Result GRadioGroup::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_ERASEBKGND:
		{
			GScreenDC Dc((HDC)Msg->A(), _View);
			Dc.Colour(LC_MED, 24);
			Dc.Rectangle();
			break;
		}
   		case WM_DESTROY:
		{
			d->InitVal = Value();
			break;
		}
	}

	return GControl::OnEvent(Msg);
}

bool GRadioGroup::Name(const char *n)
{
	return GView::Name(n);
}

bool GRadioGroup::NameW(const char16 *n)
{
	return GView::NameW(n);
}

void GRadioGroup::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt);
}

void GRadioGroup::OnCreate()
{
	SetFont(SysFont);
	AttachChildren();
	Value(d->InitVal);
}

int64 GRadioGroup::Value()
{
	if (Handle())
	{
		int n = 0;
		for (GViewI *c = Children.First(); c; c = Children.Next())
		{
			GRadioButton *Check = dynamic_cast<GRadioButton*>(c);
			if (Check)
			{
				if (Check->Value())
					return n;
				n++;
			}
		}

		return -1;
	}
	else
	{
		return d->InitVal;
	}
}

void GRadioGroup::Value(int64 Which)
{
	if (Handle())
	{
		int n = 0;
		for (GViewI *c = Children.First(); c; c = Children.Next())
		{
			GRadioButton *Check = dynamic_cast<GRadioButton*>(c);
			if (Check)
			{
				Check->Value(n == Which);
				n++;
			}
		}
	}
	else
	{
		d->InitVal = Which;
	}
}

int GRadioGroup::OnNotify(GViewI *Ctrl, int Flags)
{
	GViewI *n = GetNotify() ? GetNotify() : GetParent();
	if (n)
	{
		if (dynamic_cast<GRadioButton*>(Ctrl))
		{
			return n->OnNotify(this, Flags);
		}
		else
		{
			return n->OnNotify(Ctrl, Flags);
		}
	}
	return 0;
}

void GRadioGroup::OnPaint(GSurface *pDC)
{
}

GRadioButton *GRadioGroup::Append(int x, int y, const char *name)
{
	GRadioButton *But = new GRadioButton(d->NextId++, x, y, -1, -1, name);
	if (But)
	{
		Children.Insert(But);
	}

	return But;
}

bool GRadioGroup::OnLayout(GViewLayoutInfo &Inf)
{
    GViewIterator *it = IterateViews();
    const int BORDER_PX = 2;

    GDisplayString Txt(GetFont(), Name());
    if (!Inf.Width.Max)
    {
        // Work out the width...
		int TextPx = Txt.X();
		int MinPx = (RADIO_GRID + BORDER_PX) * 2;
		
        d->Info.DeleteObjects();
        Inf.Width.Min = 16 + TextPx;
        Inf.Width.Max = RADIO_GRID + BORDER_PX * 2;
	    for (GViewI *w = it->First(); w; w = it->Next())
	    {
	        GAutoPtr<GViewLayoutInfo> c(new GViewLayoutInfo);
            if (w->OnLayout(*c))
            {
                // Layout enabled control
                Inf.Width.Min = max(Inf.Width.Min, c->Width.Min + MinPx);
                Inf.Width.Max += c->Width.Max + RADIO_GRID;
                d->Info.Add(w, c.Release());
            }
            else
            {
                // Non layout enabled control
                Inf.Width.Min = max(Inf.Width.Min, w->X() + (RADIO_GRID << 1));
                Inf.Width.Max += w->X() + RADIO_GRID;
            }
	    }
	    
		if (Inf.Width.Max < Inf.Width.Min)
			Inf.Width.Max = Inf.Width.Min;
		d->MaxLayoutWidth = Inf.Width.Max;
    }
    else
    {
        // Working out the height, and positioning the controls
        Inf.Height.Min = Txt.Y();
        
        bool Horiz = d->MaxLayoutWidth <= Inf.Width.Max;
        int Cx = BORDER_PX + RADIO_GRID, Cy = Txt.Y();
        int LastY = 0;
	    for (GViewI *w = it->First(); w; w = it->Next())
	    {
	        GViewLayoutInfo *c = d->Info.Find(w);
            if (c)
            {
                if (w->OnLayout(*c))
                {
                    GRect r(Cx, Cy, Cx + c->Width.Max - 1, Cy + c->Height.Max - 1);
                    w->SetPos(r);

                    if (Horiz)
                        // Horizontal layout
                        Cx += r.X() + RADIO_GRID;
                    else
                        // Vertical layout
                        Cy += r.Y() + RADIO_GRID;

                    LastY = max(LastY, r.y2);
                }
                else LgiAssert(!"This shouldn't fail.");
            }
            else
            {
                // Non layout control... just use existing size
                GRect r = w->GetPos();
                r.Offset(Cx - r.x1, Cy - r.y1);
                w->SetPos(r);

                if (Horiz)
                    // Horizontal layout
                    Cx += r.X() + RADIO_GRID;
                else
                    // Vertical layout
                    Cy += r.Y() + RADIO_GRID;

                LastY = max(LastY, r.y2);
            }
	    }
	    
	    Inf.Height.Min = Inf.Height.Max = LastY + RADIO_GRID * 2 + BORDER_PX;
    }
    
    DeleteObj(it);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Radio button
class GRadioButtonPrivate
{
public:
	DWORD ParentProc;
	int64 InitVal;

	GRadioButtonPrivate()
	{
		ParentProc = 0;
		InitVal = 0;
	}

	~GRadioButtonPrivate()
	{
	}
};

GRadioButton::GRadioButton(int id, int x, int y, int cx, int cy, const char *name)
	: ResObject(Res_RadioBox)
{
	d = new GRadioButtonPrivate;

	Name(name);

	GDisplayString t(SysFont, name);
	if (cx < 0) cx = t.X() + 26;
	if (cy < 0) cy = t.Y() + 4;

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_AUTORADIOBUTTON);

	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = GWin32Class::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LgiAssert(!"No subclass?");
}

GRadioButton::~GRadioButton()
{
	DeleteObj(d);
}

void GRadioButton::OnAttach()
{
	SetFont(SysFont);
	Value(d->InitVal);
}

int GRadioButton::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED &&
		GetParent())
	{
		Value(true);
	}
	
	return 0;
}

bool GRadioButton::Name(const char *n)
{
	return GView::Name(n);
}

bool GRadioButton::NameW(const char16 *n)
{
	return GView::NameW(n);
}

int64 GRadioButton::Value()
{
	if (Handle())
		return SendMessage(Handle(), BM_GETCHECK, 0, 0);

	return d->InitVal;
}

void GRadioButton::Value(int64 i)
{
	if (Handle())
	{
		if (i)
		{
			// Turn off any other radio buttons in the current group
			GAutoPtr<GViewIterator> it(GetParent()->IterateViews());
			if (it)
			{
				for (GViewI *c = it->First(); c; c = it->Next())
				{
					GRadioButton *b = dynamic_cast<GRadioButton*>(c);
					if (b &&
						b != this &&
						b-Value())
					{
						b->Value(false);
					}
				}
			}
		}

		// Change the value of this button
		SendMessage(Handle(), BM_SETCHECK, i, 0);

		if (i)
		{
			// If gaining selection, tell the parent
			SendNotify(true);
		}
	}
	else
	{
		d->InitVal = i;
	}
}

bool GRadioButton::OnLayout(GViewLayoutInfo &Inf)
{
	GDisplayString Txt(GetFont(), Name());
    if (!Inf.Width.Max)
    {
        Inf.Width.Min =
            Inf.Width.Max =
            Txt.X() + PadXPx;
    }
    else
    {
		int y = Txt.Y() + PadYPx;
        Inf.Height.Min = max(Inf.Height.Min, y);
        Inf.Height.Max = max(Inf.Height.Max, y);
    }
	
    return true;    
}

int GRadioButton::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl == (GViewI*)this && Flags == GNotify_Activate)
	{
		Value(true);
	}

	return 0;
}

bool GRadioButton::OnKey(GKey &k)
{
	bool Status = false;
	int Move = 0;

	switch (k.vkey)
	{
		case VK_UP:
		case VK_LEFT:
		{
			if (k.Down())
			{
				Move = -1;
			}
			Status = true;
			break;
		}
		case VK_RIGHT:
		case VK_DOWN:
		{
			if (k.Down())
			{
				Move = 1;
			}
			Status = true;
			break;
		}
        case ' ':
        {
            if (k.Down())
            {
                Value(1);
            }
            return true;
        }
	}

	if (Move)
	{
		List<GRadioButton> Btns;
		GViewIterator *L = GetParent()->IterateViews();
		if (L)
		{
			for (GViewI *c=L->First(); c; c=L->Next())
			{
				GRadioButton *b = dynamic_cast<GRadioButton*>(c);
				if (b) Btns.Insert(b);
			}
			if (Btns.Length() > 1)
			{
				auto Index = Btns.IndexOf(this);
				if (Index >= 0)
				{
					GRadioButton *n = Btns[(Index + Move + Btns.Length()) % Btns.Length()];
					if (n)
					{
						n->Focus(true);
					}
				}
			}
			DeleteObj(L);
		}
	}

	return Status;
}

