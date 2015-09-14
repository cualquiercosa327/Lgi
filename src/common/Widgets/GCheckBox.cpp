#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GCheckBox.h"
#include "GDisplayString.h"
#include "GDisplayStringLayout.h"

static int PadXPx = 24;
#ifdef MAC
static int PadYPx = 8;
#else
static int PadYPx = 0;
#endif
static int MinYSize = 16;

class GCheckBoxPrivate : public GMutex, public GDisplayStringLayout
{
	GCheckBox *Ctrl;
	
public:
	int Val;
	bool Over;
	bool Three;
	GRect ValuePos;

	GCheckBoxPrivate(GCheckBox *ctrl) : GMutex("GCheckBoxPrivate")
	{
		Ctrl = ctrl;
		Val = 0;
		Over = false;
		Three = false;
		Wrap = true;
		ValuePos.ZOff(-1, -1);
	}

	bool PreLayout(int &Min, int &Max)
	{
		if (Lock(_FL))
		{
			GFont *f = Ctrl->GetFont();
			char *s = Ctrl->GBase::Name();
			DoPreLayout(f, s, Min, Max);
			Unlock();
		}
		else return false;
		return true;
	}

	bool Layout(int Px)
	{		
		if (Lock(_FL))
		{
			GFont *f = Ctrl->GetFont();
			char *s = Ctrl->GBase::Name();
			DoLayout(f, s, Px);
			Unlock();
			if (Min.y < MinYSize)
				Min.y = MinYSize;
			if (Max.y < MinYSize)
				Max.y = MinYSize;
		}
		else return false;
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
// Check box
GCheckBox::GCheckBox(int id, int x, int y, int cx, int cy, const char *name, int InitState) :
	ResObject(Res_CheckBox)
{
	d = new GCheckBoxPrivate(this);
    Name(name);
	if (cx < 0) cx = d->Max.x + PadXPx;
	if (cy < 0) cy = max(d->Max.y, MinYSize) + PadYPx;

	d->Val = InitState;
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	LgiResources::StyleElement(this);
}

GCheckBox::~GCheckBox()
{
	DeleteObj(d);
}

bool GCheckBox::OnLayout(GViewLayoutInfo &Inf)
{
	if (!Inf.Width.Max)
	{
		d->PreLayout(Inf.Width.Min, Inf.Width.Max);
		Inf.Width.Min += PadXPx;
		Inf.Width.Max += PadXPx;
	}
	else
	{
		d->Layout(Inf.Width.Max);
		Inf.Height.Min = d->Min.y + PadYPx;
		Inf.Height.Max = d->Max.y + PadYPx;
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
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = GView::Name(n);
		d->Unlock();
	}
	d->Layout(X());
	return Status;
}

bool GCheckBox::NameW(const char16 *n)
{
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = GView::NameW(n);
		d->Unlock();
	}
	d->Layout(X());
	return Status;
}

void GCheckBox::SetFont(GFont *Fnt, bool OwnIt)
{
	if (d->Lock(_FL))
	{
		GView::SetFont(Fnt, OwnIt);
		d->Unlock();
	}
	d->Layout(X());
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

void GCheckBox::OnPosChange()
{
	d->Layout(X());
}

void GCheckBox::OnPaint(GSurface *pDC)
{
	#if 0
	pDC->Colour(GColour(255, 0, 255));
	pDC->Rectangle();
	#endif
	
    if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_CHECKBOX))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;
		State.aText = &d->Strs;
		d->ValuePos.Set(0, 0, 15, 15);
		GApp::SkinEngine->OnPaint_GCheckBox(this, &State);
	}
	else
	{
		bool en = Enabled();
		GFont *f = GetFont();

		GRect r = GetClient();
		
		#if defined MAC && !defined COCOA
		d->ValuePos.Set(0, 0, PadXPx, MinYSize);
		#else
		d->ValuePos.Set(0, 0, 12, 12);
		#endif
		
		if (d->ValuePos.y2 < r.y2)
		{
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(0, d->ValuePos.y2+1, d->ValuePos.x2, r.y2);
		}

		GRect t = r;
		t.x1 = d->ValuePos.x2 + 1;
        GColour cFore(LC_TEXT, 24);
        GColour cBack(LC_MED, 24);

		if (d->Lock(_FL))
		{
			d->Paint(pDC, GetFont(), t, cFore, cBack, en);
			d->Unlock();
		}

        #if defined MAC && !defined COCOA && !defined(LGI_SDL)

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

