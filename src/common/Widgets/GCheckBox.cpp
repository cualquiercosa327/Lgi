#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GCheckBox.h"
#include "GDisplayString.h"

class GCheckBoxPrivate
{
public:
	int Val;
	bool Over;
	bool Three;
	GDisplayString *Txt;
	GRect ValuePos;

	GCheckBoxPrivate()
	{
		Val = 0;
		Over = false;
		Three = false;
		Txt = 0;
		ValuePos.ZOff(-1, -1);

	}

	~GCheckBoxPrivate()
	{
		DeleteObj(Txt);
	}

	void Layout(GFont *f, char *s)
	{
		DeleteObj(Txt);
		Txt = new GDisplayString(f, s);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
// Check box
static int PadXPx = 30;
#ifdef MAC
static int PadYPx = 8;
#else
static int PadYPx = 0;
#endif

GCheckBox::GCheckBox(int id, int x, int y, int cx, int cy, const char *name, int InitState) :
	ResObject(Res_CheckBox)
{
	d = new GCheckBoxPrivate;
    Name(name);
	if (cx < 0 && d->Txt) cx = d->Txt->X() + PadXPx;
	if (cy < 0 && d->Txt) cy = max(d->Txt->Y(), 16) + PadYPx;

	d->Val = InitState;
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
}

GCheckBox::~GCheckBox()
{
	DeleteObj(d);
}

bool GCheckBox::OnLayout(GViewLayoutInfo &Inf)
{
    if (!Inf.Width.Max)
    {
        Inf.Width.Min =
            Inf.Width.Max =
            (d->Txt ? d->Txt->X() : 0) + PadXPx;
    }
    else
    {
        Inf.Height.Min =
            Inf.Height.Max =
//                (d->Txt ? d->Txt->Y() : GetFont()->GetHeight()) + PadYPx;
                GetFont()->GetHeight() + PadYPx;
    }
    return true;    
}

void GCheckBox::OnAttach()
{
}

GMessage::Result GCheckBox::OnEvent(GMessage *m)
{
	return GView::OnEvent(m);
}

bool GCheckBox::ThreeState()
{
	return d->Three;
}

void GCheckBox::ThreeState(bool t)
{
	d->Three = t;
}

int64 GCheckBox::Value()
{
	return d->Val;
}

void GCheckBox::Value(int64 i)
{
	if (d->Val != i)
	{
		d->Val = i;
		Invalidate(&d->ValuePos);

		SendNotify(d->Val);
	}
}

bool GCheckBox::Name(const char *n)
{
	bool Status = GView::Name(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

bool GCheckBox::NameW(const char16 *n)
{
	bool Status = GView::NameW(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

void GCheckBox::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->Layout(GetFont(), GBase::Name());
	Invalidate();
}

void GCheckBox::OnMouseClick(GMouse &m)
{
	if (Enabled())
	{
		int Click = IsCapturing();

		Capture(d->Over = m.Down());
		if (m.Down())
		{
			Focus(true);
		}
		
		GRect r(0, 0, X()-1, Y()-1);
		if (!m.Down() &&
			r.Overlap(m.x, m.y) &&
			Click)
		{
			if (d->Three)
			{
				switch (d->Val)
				{
					case 0:
						Value(2); break;
					case 2:
						Value(1); break;
					default:
						Value(0); break;
				}
			}
			else
			{
				Value(!d->Val);
			}
		}
		else
		{
			Invalidate(&d->ValuePos);
		}
	}
}

void GCheckBox::OnMouseEnter(GMouse &m)
{
	if (IsCapturing())
	{
		d->Over = true;
		Invalidate(&d->ValuePos);
	}
}

void GCheckBox::OnMouseExit(GMouse &m)
{
	if (IsCapturing())
	{
		d->Over = false;
		Invalidate(&d->ValuePos);
	}
}

bool GCheckBox::OnKey(GKey &k)
{
    switch (k.vkey)
    {
        case ' ':
        {
            if (!k.Down())
            {
                Value(!Value());
            }

            return true;
        }
    }
	
	return false;
}

void GCheckBox::OnFocus(bool f)
{
	Invalidate();
}

void GCheckBox::OnPaint(GSurface *pDC)
{
    if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_CHECKBOX))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;
		State.Text = &d->Txt;
		d->ValuePos.Set(0, 0, 15, 15);
		GApp::SkinEngine->OnPaint_GCheckBox(this, &State);
	}
	else
	{
		bool en = Enabled();
		GFont *f = GetFont();

		GRect r(0, 0, X()-1, Y()-1);
		d->ValuePos.Set(0, 0, 12, 12);
		if (d->ValuePos.y2 < r.y2)
		{
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(0, d->ValuePos.y2+1, d->ValuePos.x2, r.y2);
		}

		if (d->Txt)
		{
			GRect t = r;
			t.x1 = d->ValuePos.x2 + 1;
            f->Colour(LC_TEXT, LC_MED);
			f->Transparent(false);

            GRect p;
            p.ZOff(d->Txt->X()-1, d->Txt->Y()-1);
            p.Offset(t.x1 + 10, t.y1 + ((t.Y() - d->Txt->Y()) >> 1));
			if (en)
			{
				d->Txt->Draw(pDC, p.x1, p.y1, &t);

				if (Focus() && ValidStr(Name()))
				{
                    GRect f = p;
                    f.Size(-2, -2);
					pDC->Colour(LC_LOW, 24);
					pDC->Box(&f);
				}
			}
			else
			{
				f->Colour(LC_LIGHT, LC_MED);
				d->Txt->Draw(pDC, p.x1 + 1, p.y1 + 1, &t);
				
				f->Transparent(true);
				f->Colour(LC_LOW, LC_MED);
				d->Txt->Draw(pDC, p.x1, p.y1, &t);
			}
		}

        #if defined MAC && !defined COCOA

			GColour Background(LC_MED, 24);
			if (GetCss())
			{
				GCss::ColorDef Bk = GetCss()->BackgroundColor();
				if (Bk.Type == GCss::ColorRgb)
					Background.Set(Bk.Rgb32, 32);
			}
			pDC->Colour(Background);
			pDC->Rectangle(&d->ValuePos);

			GRect c = GetClient();
			for (GViewI *v = this; v && !v->Handle(); v = v->GetParent())
			{
				GRect p = v->GetPos();
				c.Offset(p.x1, p.y1);
			}
			
			HIRect Bounds = c;
			HIThemeButtonDrawInfo Info;
			HIRect LabelRect;
	
			Info.version = 0;
			Info.state = d->Val ? kThemeStatePressed : (Enabled() ? kThemeStateActive : kThemeStateInactive);
			Info.kind = kThemeCheckBox;
			Info.value = d->Val ? kThemeButtonOn : kThemeButtonOff;
			Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;
	
			OSStatus e = HIThemeDrawButton(	  &Bounds,
											  &Info,
											  pDC->Handle(),
											  kHIThemeOrientationNormal,
											  &LabelRect);
	
			if (e) printf("%s:%i - HIThemeDrawButton failed %li\n", _FL, e);
		
		
        #else
    
			LgiWideBorder(pDC, d->ValuePos, DefaultSunkenEdge);
			pDC->Colour(d->Over || !en ? LC_MED : LC_WORKSPACE, 24);
			pDC->Rectangle(&d->ValuePos);
			pDC->Colour(en ? LC_TEXT : LC_LOW, 24);

			if (d->Three && d->Val == 2)
			{
				for (int y=d->ValuePos.y1; y<=d->ValuePos.y2; y++)
				{
					for (int x=d->ValuePos.x1; x<=d->ValuePos.x2; x++)
					{
						if ( (x&1) ^ (y&1) )
						{
							pDC->Set(x, y);
						}
					}
				}
			}
			else if (d->Val)
			{
				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y1+1, d->ValuePos.x2-1, d->ValuePos.y2-1);
				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y1+2, d->ValuePos.x2-2, d->ValuePos.y2-1);
				pDC->Line(d->ValuePos.x1+2, d->ValuePos.y1+1, d->ValuePos.x2-1, d->ValuePos.y2-2);

				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y2-1, d->ValuePos.x2-1, d->ValuePos.y1+1);
				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y2-2, d->ValuePos.x2-2, d->ValuePos.y1+1);
				pDC->Line(d->ValuePos.x1+2, d->ValuePos.y2-1, d->ValuePos.x2-1, d->ValuePos.y1+2);
			}
        
        #endif
	}
}

