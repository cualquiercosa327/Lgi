#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GRadioGroup.h"
#include "GCheckBox.h"
#include "GDisplayString.h"

///////////////////////////////////////////////////////////////////////////////////////////
// Radio group
class GRadioGroupPrivate
{
public:
	static int NextId;
	int Val;
	int MaxLayoutWidth;
	GDisplayString *Txt;
    GHashTbl<void*,GViewLayoutInfo*> Info;

	GRadioGroupPrivate()
	{
		Txt = 0;
		MaxLayoutWidth = 0;
	}

	~GRadioGroupPrivate()
	{
		DeleteObj(Txt);
        Info.DeleteObjects();
	}

	void Layout(GFont *f, char *s)
	{
		DeleteObj(Txt);
		Txt = new GDisplayString(f, s);
	}
};

int GRadioGroupPrivate::NextId = 10000;

GRadioGroup::GRadioGroup(int id, int x, int y, int cx, int cy, const char *name, int Init)
	: ResObject(Res_Group)
{
	d = new GRadioGroupPrivate;
	Name(name);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	d->Val = Init;
}

GRadioGroup::~GRadioGroup()
{
	DeleteObj(d);
}

#define RADIO_GRID  7

bool GRadioGroup::OnLayout(GViewLayoutInfo &Inf)
{
    GViewIterator *it = IterateViews();

    if (!Inf.Width.Max)
    {
        // Work out the width...
        d->Info.DeleteObjects();
        Inf.Width.Min = 16 + (d->Txt ? d->Txt->X() : 16);
        
        Inf.Width.Max = RADIO_GRID;
	    for (GViewI *w = it->First(); w; w = it->Next())
	    {
	        GAutoPtr<GViewLayoutInfo> c(new GViewLayoutInfo);
            if (w->OnLayout(*c))
            {
                // Layout enabled control
                Inf.Width.Min = max(Inf.Width.Min, c->Width.Min + (RADIO_GRID << 1));
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
	    
	    d->MaxLayoutWidth = Inf.Width.Max;
    }
    else
    {
        // Working out the height, and positioning the controls
        Inf.Height.Min = d->Txt ? d->Txt->Y() : 16;
        
        bool Horiz = d->MaxLayoutWidth <= Inf.Width.Max;
        int Cx = RADIO_GRID, Cy = GetFont()->GetHeight();
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
	    
	    Inf.Height.Min = Inf.Height.Max = LastY + RADIO_GRID;
    }
    
    DeleteObj(it);
    return true;
}

void GRadioGroup::OnAttach()
{
}

GMessage::Result GRadioGroup::OnEvent(GMessage *m)
{
	return GView::OnEvent(m);
}

bool GRadioGroup::Name(const char *n)
{
	bool Status = GView::Name(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

bool GRadioGroup::NameW(const char16 *n)
{
	bool Status = GView::NameW(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

void GRadioGroup::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt);
	d->Layout(GetFont(), GBase::Name());
	Invalidate();
}

void GRadioGroup::OnCreate()
{
	AttachChildren();
	Value(d->Val);
}

int64 GRadioGroup::Value()
{
	int i=0;
	for (	GViewI *w = Children.First();
			w;
			w = Children.Next())
	{
		GRadioButton *But = dynamic_cast<GRadioButton*>(w);
		if (But)
		{
			if (But->Value())
			{
				d->Val = i;
				break;
			}
			i++;
		}
	}

	return d->Val;
}

void GRadioGroup::Value(int64 Which)
{
	d->Val = Which;

	int i=0;
	for (	GViewI *w = Children.First();
			w;
			w = Children.Next())
	{
		GRadioButton *But = dynamic_cast<GRadioButton*>(w);
		if (But)
		{
			if (i == Which)
			{
				But->Value(true);
				break;
			}
			i++;
		}
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
	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_GROUP))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.MouseOver = false;
		State.Text = &d->Txt;
		GApp::SkinEngine->OnPaint_GRadioGroup(this, &State);
	}
	else
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();

		int y = d->Txt ? d->Txt->Y() : 12;
		GRect b(0, y/2, X()-1, Y()-1);
		LgiWideBorder(pDC, b, CHISEL);

		if (d->Txt)
		{
			GRect t;
			t.ZOff(d->Txt->X(), d->Txt->Y());
			t.Offset(6, 0);
			SysFont->Colour(LC_TEXT, LC_MED);
			SysFont->Transparent(false);
			d->Txt->Draw(pDC, t.x1, t.y1, &t);
		}
	}
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

///////////////////////////////////////////////////////////////////////////////////////////
// Radio button
class GRadioButtonPrivate
{
public:
	bool Val;
	bool Over;
	GDisplayString *Txt;

	GRadioButtonPrivate()
	{
		Txt = 0;
	}

	~GRadioButtonPrivate()
	{
		DeleteObj(Txt);
	}

	void Layout(GFont *f, char *s)
	{
		DeleteObj(Txt);
		Txt = new GDisplayString(f, s);
	}
};

GRadioButton::GRadioButton(int id, int x, int y, int cx, int cy, const char *name)
	: ResObject(Res_RadioBox)
{
	d = new GRadioButtonPrivate;
	Name(name);
	if (cx < 0 && d->Txt) cx = d->Txt->X() + 26;
	if (cy < 0 && d->Txt) cy = d->Txt->Y() + 4;

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	d->Val = false;
	d->Over = false;
	SetTabStop(true);

	#if WIN32NATIVE
	SetDlgCode(GetDlgCode() | DLGC_WANTARROWS);
	#endif
}

GRadioButton::~GRadioButton()
{
	DeleteObj(d);
}

void GRadioButton::OnAttach()
{
}

GMessage::Result GRadioButton::OnEvent(GMessage *m)
{
	return GView::OnEvent(m);
}

bool GRadioButton::Name(const char *n)
{
	bool Status = GView::Name(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

bool GRadioButton::NameW(const char16 *n)
{
	bool Status = GView::NameW(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

void GRadioButton::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->Layout(GetFont(), GBase::Name());
	Invalidate();
}

bool GRadioButton::OnLayout(GViewLayoutInfo &Inf)
{
    if (!Inf.Width.Max)
    {
        Inf.Width.Min =
            Inf.Width.Max =
            d->Txt ? 20 + d->Txt->X() : 30;
    }
    else
    {
		int y = d->Txt ? d->Txt->Y() : SysFont->GetHeight();
        Inf.Height.Min = max(Inf.Height.Min, y);
        Inf.Height.Max = max(Inf.Height.Max, y);
    }
	
    return true;    
}

int64 GRadioButton::Value()
{
	return d->Val;
}

void GRadioButton::Value(int64 i)
{
	if (d->Val != (bool)i)
	{
		if (i)
		{
			// remove the value from the currenly selected radio value
			GViewI *p = GetParent();
			if (p)
			{
				GViewIterator *l = p->IterateViews();
				if (l)
				{
					for (GViewI *c=l->First(); c; c=l->Next())
					{
						if (c != this)
						{
							GRadioButton *b = dynamic_cast<GRadioButton*>(c);
							if (b && b->d->Val)
							{
								b->d->Val = false;
								b->Invalidate();
							}
						}
					}
					DeleteObj(l);
				}
			}
		}

		d->Val = i;
		Invalidate();

		if (i)
		{
			SendNotify(0);
		}
	}
}

void GRadioButton::OnMouseClick(GMouse &m)
{
	if (Enabled())
	{
		bool WasCapturing = IsCapturing();

		if (m.Down()) Focus(true);
		Capture(m.Down());
		d->Over = m.Down();
	
		GRect r(0, 0, X()-1, Y()-1);
		if (!m.Down() &&
			r.Overlap(m.x, m.y) &&
			WasCapturing)
		{
			Value(true);
		}
		else
		{
			Invalidate();
		}
	}
}

void GRadioButton::OnMouseEnter(GMouse &m)
{
	if (Enabled() && IsCapturing())
	{
		d->Over = true;
		Invalidate();
	}
}

void GRadioButton::OnMouseExit(GMouse &m)
{
	if (Enabled() && IsCapturing())
	{
		d->Over = false;
		Invalidate();
	}
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
				int Index = Btns.IndexOf(this);
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

void GRadioButton::OnFocus(bool f)
{
	Invalidate();
}

void GRadioButton::OnPaint(GSurface *pDC)
{
	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_RADIO))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;
		State.Text = &d->Txt;
		GApp::SkinEngine->OnPaint_GRadioButton(this, &State);
	}
	else
	{
		GRect r(0, 0, X()-1, Y()-1);
		GRect c(0, 0, 12, 12);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();

		bool e = Enabled();
		if (d->Txt)
		{
			GRect t = r;
			t.x1 = c.x2 + 1;
			
			int Off = e ? 0 : 1;
			SysFont->Colour(e ? LC_TEXT : LC_LIGHT, LC_MED);
			SysFont->Transparent(false);
			d->Txt->Draw(pDC, t.x1 + 5 + Off, t.y1 + Off, &t);

			if (!e)
			{
				SysFont->Transparent(true);
				SysFont->Colour(LC_LOW, LC_MED);
				d->Txt->Draw(pDC, t.x1 + 5, t.y1, &t);
			}
		}

		// Draw border
		pDC->Colour(LC_LOW, 24);
		pDC->Line(c.x1+1, c.y1+9, c.x1+1, c.y1+10);
		pDC->Line(c.x1, c.y1+4, c.x1, c.y1+8);
		pDC->Line(c.x1+1, c.y1+2, c.x1+1, c.y1+3);
		pDC->Line(c.x1+2, c.y1+1, c.x1+3, c.y1+1);
		pDC->Line(c.x1+4, c.y1, c.x1+8, c.y1);
		pDC->Line(c.x1+9, c.y1+1, c.x1+10, c.y1+1);

		pDC->Colour(LC_SHADOW, 24);
		pDC->Set(c.x1+2, c.y1+9);
		pDC->Line(c.x1+1, c.y1+4, c.x1+1, c.y1+8);
		pDC->Line(c.x1+2, c.y1+2, c.x1+2, c.y1+3);
		pDC->Set(c.x1+3, c.y1+2);
		pDC->Line(c.x1+4, c.y1+1, c.x1+8, c.y1+1);
		pDC->Set(c.x1+9, c.y1+2);

		pDC->Colour(LC_LIGHT, 24);
		pDC->Line(c.x1+11, c.y1+2, c.x1+11, c.y1+3);
		pDC->Line(c.x1+12, c.y1+4, c.x1+12, c.y1+8);
		pDC->Line(c.x1+11, c.y1+9, c.x1+11, c.y1+10);
		pDC->Line(c.x1+9, c.y1+11, c.x1+10, c.y1+11);
		pDC->Line(c.x1+4, c.y1+12, c.x1+8, c.y1+12);
		pDC->Line(c.x1+2, c.y1+11, c.x1+3, c.y1+11);

		/// Draw center
		pDC->Colour(d->Over || !e ? LC_MED : LC_WORKSPACE, 24);
		pDC->Rectangle(c.x1+2, c.y1+4, c.x1+10, c.y1+8);
		pDC->Box(c.x1+3, c.y1+3, c.x1+9, c.y1+9);
		pDC->Box(c.x1+4, c.y1+2, c.x1+8, c.y1+10);

		// Draw value
		if (d->Val)
		{
			pDC->Colour(e ? LC_TEXT : LC_LOW, 24);
			pDC->Rectangle(c.x1+4, c.y1+5, c.x1+8, c.y1+7);
			pDC->Rectangle(c.x1+5, c.y1+4, c.x1+7, c.y1+8);
		}
	}
}

