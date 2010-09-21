#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GCheckBox.h"

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
GCheckBox::GCheckBox(int id, int x, int y, int cx, int cy, char *name, int InitState) :
	ResObject(Res_CheckBox)
{
	d = new GCheckBoxPrivate;
	Name(name);
	if (cx < 0 AND d->Txt) cx = d->Txt->X() + 26;
	if (cy < 0 AND d->Txt) cy = max(d->Txt->Y(), 16);

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

void GCheckBox::OnAttach()
{
}

int GCheckBox::OnEvent(GMessage *m)
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

bool GCheckBox::Name(char *n)
{
	bool Status = GView::Name(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

bool GCheckBox::NameW(char16 *n)
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
		if (!m.Down() AND
			r.Overlap(m.x, m.y) AND
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
	if (k.IsChar)
	{
		switch (k.vkey)
		{
			case ' ':
			case VK_RETURN:
			{
				if (!k.Down())
				{
					Value(!Value());
				}

				return true;
				break;
			}
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
	if (GApp::SkinEngine AND
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
		bool e = Enabled();
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

			if (Enabled())
			{
				d->Txt->Draw(pDC, t.x1 + 5, t.y1 - 1, &t);

				if (Focus() AND ValidStr(Name()))
				{
					GRect f(t.x1 + 3, t.y1, t.x1 + 7 + d->Txt->X(), t.y1 + d->Txt->Y());
					GRect c = GetClient();
					f.Bound(&c);
					
					pDC->Colour(LC_LOW, 24);
					pDC->Box(&f);
				}
			}
			else
			{
				f->Colour(LC_LIGHT, LC_MED);
				d->Txt->Draw(pDC, t.x1 + 6, t.y1, &t);
				
				f->Transparent(true);
				f->Colour(LC_LOW, LC_MED);
				d->Txt->Draw(pDC, t.x1 + 5, t.y1 - 1, &t);
			}
		}

		LgiWideBorder(pDC, d->ValuePos, SUNKEN);
		pDC->Colour(d->Over OR !e ? LC_MED : LC_WORKSPACE, 24);
		pDC->Rectangle(&d->ValuePos);
		pDC->Colour(e ? LC_TEXT : LC_LOW, 24);

		if (d->Three AND d->Val == 2)
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
	}
}

