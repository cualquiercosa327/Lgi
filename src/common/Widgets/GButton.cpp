#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GButton.h"

#define DOWN_MOUSE		0x1
#define DOWN_KEY		0x2

// Size of extra pixels, beyond the size of the text itself.
GdcPt2 GButton::Overhead =
    GdcPt2(
        // Extra width needed
        #ifdef MAC
        28,
        #else
        16,
        #endif
        // Extra height needed
        6
    );

class GButtonPrivate
{
public:
	int Pressed;
	bool KeyDown;
	bool Over;
	bool WantsDefault;
	GDisplayString *Txt;
	
	#ifdef MAC
	ThemeButtonDrawInfo Cur;
	#endif

	GButtonPrivate()
	{
		Pressed = 0;
		KeyDown = false;
		Over = 0;
		Txt = 0;
		WantsDefault = false;
		
		#ifdef MAC
		Cur.state = kThemeStateInactive;
		Cur.value = kThemeButtonOff;
		Cur.adornment = kThemeAdornmentNone;
		#endif
	}

	~GButtonPrivate()
	{
		DeleteObj(Txt);
	}

	void Layout(GFont *f, char *s)
	{
		DeleteObj(Txt);
		Txt = new GDisplayString(f, s);
	}
};

GButton::GButton(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Button)
{
	d = new GButtonPrivate;
	Name(name);
	
	GRect r(x,
			y,
			x + (cx < 0 ? d->Txt->X() + Overhead.x : cx),
			y + (cy < 0 ? d->Txt->Y() + Overhead.y : cy)
			);
	LgiAssert(r.Valid());
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	
	if (LgiApp->SkinEngine)
	{
		SetFont(LgiApp->SkinEngine->GetDefaultFont(_ObjName));
	}
}

GButton::~GButton()
{
	if (GetWindow() AND
		GetWindow()->_Default == this)
	{
		GetWindow()->_Default = 0;
	}

	DeleteObj(d);
}

bool GButton::Default()
{
	if (GetWindow())
	{
		return GetWindow()->_Default == this;
	}
	else
	{
		printf("%s:%i - No window.\n", __FILE__, __LINE__);
	}
	
	return false;
}

void GButton::Default(bool b)
{
	if (GetWindow())
	{
		GetWindow()->_Default = b ? this : 0;
		if (IsAttached())
		{
			Invalidate();
		}
	}
	else
	{
		d->WantsDefault = b;
	}
}

bool GButton::Name(const char *n)
{
	bool Status = GView::Name(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

bool GButton::NameW(const char16 *n)
{
	bool Status = GView::NameW(n);
	d->Layout(GetFont(), GBase::Name());
	return Status;
}

void GButton::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->Layout(GetFont(), GBase::Name());
	Invalidate();
}

GMessage::Result GButton::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GButton::OnMouseClick(GMouse &m)
{
	if (Enabled())
	{
		bool Click = IsCapturing();
		
		Capture(m.Down());

		if (Click ^ m.Down())
		{
			if (d->Over)
			{
				if (m.Down())
				{
					d->Pressed++;
					Focus(true);
				}
				else
				{
					d->Pressed--;
				}

				Invalidate();

				// LgiTrace("Mouse=%i Pressed=%x\n", m.Down(), d->Pressed);
				
				if (!m.Down() AND
					d->Pressed == 0)
				{
					// This may delete ourself, so do it last.
					SendNotify();
				}
			}
		}
	}
	else printf("%s:%i - Not enabled.\n", _FL);
}

void GButton::OnMouseEnter(GMouse &m)
{
	d->Over = true;
	if (IsCapturing())
	{
		Value(d->Pressed + 1);
	}
	else if (Enabled())
	{
		if (!LgiApp->SkinEngine)
			Invalidate();
	}
}

void GButton::OnMouseExit(GMouse &m)
{
	d->Over = false;
	if (IsCapturing())
	{
		Value(d->Pressed - 1);
	}
	else if (Enabled())
	{
		if (!LgiApp->SkinEngine)
			Invalidate();
	}
}

bool GButton::OnKey(GKey &k)
{
	if (
		#ifdef WIN32
		k.IsChar OR
		#endif
		!Enabled())
	{
		return false;
	}
	
	switch (k.vkey)
	{
		case VK_ESCAPE:
		{
			if (GetId() != IDCANCEL)
			{
				break;
			}
			// else fall thru
		}
		case ' ':
		case VK_RETURN:
		#ifdef LINUX
		case VK_KP_ENTER:
		#endif
		{
			if (d->KeyDown ^ k.Down())
			{
				d->KeyDown = k.Down();
				if (k.Down())
				{
					d->Pressed++;
				}
				else
				{
					d->Pressed--;
				}

				Invalidate();

				if (!k.Down() && d->Pressed == 0)
				{
					SendNotify(0);
				}
			}
			// else printf("No change.\n");

			return true;
			break;
		}
	}
	
	return false;
}

void GButton::OnFocus(bool f)
{
	Invalidate();
}

void GButton::OnPaint(GSurface *pDC)
{
	#ifdef MAC

	pDC->Colour(LC_MED, 24);	
	pDC->Rectangle();

	GRect c = GetClient();
	for (GViewI *v = this; v && !v->Handle(); v = v->GetParent())
	{
		GRect p = v->GetPos();
		c.Offset(p.x1, p.y1);
	}
	
	Rect Bounds = { c.y1, c.x1+2, c.y2-1, c.x2-1 };
	ThemeButtonDrawInfo Info;
	Info.state = d->Pressed ? kThemeStatePressed : (Enabled() ? kThemeStateActive : kThemeStateInactive);
	Info.value = Default() ? kThemeButtonOn : kThemeButtonOff;
	Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;

	GLabelData User;
	User.Ctrl = this;
	User.pDC = pDC;
	User.r.y1 = 2;
	User.Justification = teJustCenter;
	OSStatus e = DrawThemeButton(&Bounds,
								kThemePushButton,
								&Info,
								&d->Cur,
								0,
								LgiLabelUPP ? LgiLabelUPP : LgiLabelUPP = NewThemeButtonDrawUPP(LgiLabelProc),
								(UInt32)&User);
	
	d->Cur = Info;
	
	#else

	if (GApp::SkinEngine AND
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_BUTTON))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;
		State.Text = &d->Txt;
		GApp::SkinEngine->OnPaint_GButton(this, &State);
	}
	else
	{
		COLOUR Back = d->Over ? LC_HIGH : LC_MED;
		GRect r(0, 0, X()-1, Y()-1);
		if (Default())
		{
			pDC->Colour(LC_BLACK, 24);
			pDC->Box(&r);
			r.Size(1, 1);
		}
		LgiWideBorder(pDC, r, d->Pressed ? SUNKEN : RAISED);

		if (d->Txt)
		{
			int x = d->Txt->X(), y = d->Txt->Y();
			int Tx = r.x1 + ((r.X()-x)/2) + (d->Pressed != 0);
			int Ty = r.y1 + ((r.Y()-y)/2) + (d->Pressed != 0);

			GFont *f = GetFont();
			f->Transparent(false);
			if (Enabled())
			{
				f->Colour(LC_TEXT, Back);
				d->Txt->Draw(pDC, Tx, Ty, &r);
			}
			else
			{
				f->Colour(LC_LIGHT, Back);
				d->Txt->Draw(pDC, Tx+1, Ty+1, &r);

				f->Transparent(true);
				f->Colour(LC_LOW, Back);
				d->Txt->Draw(pDC, Tx, Ty, &r);
			}

			if (Focus())
			{
				pDC->Colour(LC_LOW, 24);
				pDC->Box(Tx-2, Ty, Tx+x+2, Ty+y);
			}
		}
		else
		{
			pDC->Colour(Back, 24);
			pDC->Rectangle(&r);
		}
	}
	
	#endif
}

int64 GButton::Value()
{
	return d->Pressed != 0;
}

void GButton::Value(int64 i)
{
	d->Pressed = i;
	Invalidate();
}

void GButton::OnAttach()
{
	if (d->WantsDefault)
	{
		d->WantsDefault = false;
		if (GetWindow())
		{
			GetWindow()->_Default = this;
		}
	}
}

void GButton::SetPreferredSize()
{
	GRect r = GetPos();
	r.x2 = r.x1 + d->Txt->X() + Overhead.x - 1;
	r.y2 = r.y1 + d->Txt->Y() + Overhead.y - 1;
	SetPos(r);
}
