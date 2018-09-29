#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GButton.h"
#include "GDisplayString.h"
#include "GTableLayout.h"
#include "LgiRes.h"
#include "LStringLayout.h"

#define DOWN_MOUSE		0x1
#define DOWN_KEY		0x2

// Size of extra pixels, beyond the size of the text itself.
GdcPt2 GButton::Overhead =
    GdcPt2(
        // Extra width needed
        #if defined(MAC) && !defined(LGI_SDL)
        28,
        #else
        16,
        #endif
        // Extra height needed
        6
    );

class GButtonPrivate : public LStringLayout
{
	GFontCache Cache;
	
public:
	int Pressed;
	bool KeyDown;
	bool Over;
	bool WantsDefault;
	bool Toggle;
	
	GRect TxtSz;
	GSurface *Image;
	bool OwnImage;
	
	GButtonPrivate() : Cache(SysFont), LStringLayout(&Cache)
	{
		AmpersandToUnderline = true;
		Pressed = 0;
		KeyDown = false;
		Toggle = false;
		Over = 0;
		// Txt = 0;
		WantsDefault = false;
		Image = NULL;
		OwnImage = false;
		SetWrap(false);
	}

	~GButtonPrivate()
	{
		// DeleteObj(Txt);
		if (OwnImage)
			DeleteObj(Image);
	}

	void Layout(GFont *f, char *s)
	{
		Empty();

		GCss c;
		c.FontWeight(GCss::FontWeightBold);
		
		Add(s, &c);

		int32 MinX, MaxX;
		DoPreLayout(MinX, MaxX);
		DoLayout(MaxX);
		TxtSz = GetBounds();
	}
};

GButton::GButton(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Button)
{
	d = new GButtonPrivate;
	Name(name);
	
	GRect r(x,
			y,
			x + (cx <= 0 ? d->TxtSz.X() + Overhead.x : cx) - 1,
			y + (cy <= 0 ? d->TxtSz.Y() + Overhead.y : cy) - 1);
	LgiAssert(r.Valid());
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	#ifndef MAC
	SetFont(SysBold, false);
	#endif
	
	if (LgiApp->SkinEngine)
		SetFont(LgiApp->SkinEngine->GetDefaultFont(_ObjName));
}

GButton::~GButton()
{
	if (GetWindow() &&
		GetWindow()->_Default == this)
	{
		GetWindow()->_Default = 0;
	}

	DeleteObj(d);
}

int GButton::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl == (GViewI*)this && Flags == GNotify_Activate)
	{
		OnClick();
	}

	return 0;
}

bool GButton::Default()
{
	if (GetWindow())
	{
		return GetWindow()->_Default == this;
	}
	else LgiTrace("%s:%i - No window.\n", _FL);
	
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

bool GButton::GetIsToggle()
{
	return d->Toggle;
}

void GButton::SetIsToggle(bool toggle)
{
	d->Toggle = toggle;
}

GSurface *GButton::GetImage()
{
	return d->Image;
}

bool GButton::SetImage(const char *FileName)
{
	if (d->OwnImage)
		DeleteObj(d->Image);
	d->Image = LoadDC(FileName);
	Invalidate();
	return d->OwnImage = d->Image != NULL;
}

bool GButton::SetImage(GSurface *Img, bool OwnIt)
{
	if (d->OwnImage)
		DeleteObj(d->Image);
	d->Image = Img;
	d->OwnImage = d->Image != NULL && OwnIt;
	Invalidate();
	return d->OwnImage;
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
	LgiAssert(Fnt && Fnt->Handle());


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
	if (!Enabled())
		return;

	if (d->Toggle)
	{
		if (m.Down())
		{
			Value(!Value());
			OnClick();
		}
	}
	else
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

				if (!m.Down() &&
					d->Pressed == 0)
				{
					// This may delete ourself, so do it last.
					OnClick();
				}
			}
		}
	}
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
		k.IsChar ||
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
					OnClick();
				}
			}

			return true;
			break;
		}
	}
	
	return false;
}

void GButton::OnClick()
{
	int Id = GetId();
	if (Id)
	{
		GViewI *n = GetNotify();
		GViewI *p = GetParent();
		GViewI *target = n ? n : p;
		if (target)
		{
			if (Handle())
			{
				target->PostEvent(M_CHANGE, (GMessage::Param)Id);
			}
			else if (InThread())
			{
				target->OnNotify(this, 0);
			}
			else
			{
				LgiAssert(!"Out of thread and no handle?");
			}
		}
	}
	else
		SendNotify();
}

void GButton::OnFocus(bool f)
{
	Invalidate();
}

void GButton::OnPaint(GSurface *pDC)
{
	#if defined(MAC) && !defined(COCOA) && !defined(LGI_SDL)

	GColour NoPaintColour(LC_MED, 24);
	if (GetCss())
	{
		GCss::ColorDef NoPaint = GetCss()->NoPaintColor();
		if (NoPaint.Type == GCss::ColorRgb)
			NoPaintColour.Set(NoPaint.Rgb32, 32);
		else if (NoPaint.Type == GCss::ColorTransparent)
			NoPaintColour.Empty();
	}
	if (!NoPaintColour.IsTransparent())
	{
		pDC->Colour(NoPaintColour);
		pDC->Rectangle();
	}
	
	GRect rc = GetClient();
	rc.x1 += 2;
	rc.y2 -= 1;
	rc.x2 -= 1;
	HIRect Bounds = rc;
	HIThemeButtonDrawInfo Info;
	HIRect LabelRect;

	Info.version = 0;
	Info.state = d->Pressed ? kThemeStatePressed : (Enabled() ? kThemeStateActive : kThemeStateInactive);
	Info.kind = kThemePushButton;
	Info.value = Default() ? kThemeButtonOn : kThemeButtonOff;
	Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;

	OSStatus e = HIThemeDrawButton(	  &Bounds,
									  &Info,
									  pDC->Handle(),
									  kHIThemeOrientationNormal,
									  &LabelRect);

	if (e) printf("%s:%i - HIThemeDrawButton failed %li\n", _FL, e);
	else
	{
		GdcPt2 pt;
		GRect r = GetClient();
		pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
		pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
		d->Paint(pDC, pt, GColour(), r, Enabled());
	}
	
	#else

	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_BUTTON))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;

		/*
		char *Nl = strchr(Name(), '\n');
		if (Nl)
		{
			GString n = Name();
			GString::Array a = n.Split("\n");
			State.aText = new GArray<GDisplayString*>;
			if (State.aText)
			{
				for (unsigned i=0; i<a.Length(); i++)
				{
					State.aText->Add(new GDisplayString(GetFont(), a[i]));
				}
			}
		}
		else
		{
			State.ptrText = &d->Txt;
		}
		*/
		
		State.Image = d->Image;
		GApp::SkinEngine->OnPaint_GButton(this, &State);
		
		GdcPt2 pt;
		GRect r = GetClient();
		pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
		pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
		d->Paint(pDC, pt, GColour(), r, Enabled());
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
		LgiWideBorder(pDC, r, d->Pressed ? DefaultSunkenEdge : DefaultRaisedEdge);

		GdcPt2 pt;
		pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
		pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
		d->Paint(pDC, pt, GColour(Back,24), r, Enabled());

		/*
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
		*/
	}
	
	#endif
}

int64 GButton::Value()
{
	return d->Pressed != 0;
}

void GButton::Value(int64 i)
{
	d->Pressed = (int)i;
	Invalidate();
}

void GButton::OnCreate()
{
}

void GButton::OnAttach()
{
	LgiResources::StyleElement(this);
	if (d->WantsDefault)
	{
		d->WantsDefault = false;
		if (GetWindow())
		{
			GetWindow()->_Default = this;
		}
	}
}

void GButton::SetPreferredSize(int x, int y)
{
	GRect r = GetPos();

	int Ix = d->Image ? d->Image->X() : 0;
	int Iy = d->Image ? d->Image->Y() : 0;
	int Cx = d->TxtSz.X() + Ix + (d->TxtSz.X() && d->Image ? GTableLayout::CellSpacing : 0);
	int Cy = MAX(d->TxtSz.Y(), Iy);
	
	r.Dimension((x > 0 ? x : Cx + Overhead.x) - 1,
				(y > 0 ? y : Cy + Overhead.y) - 1);

	SetPos(r);
}
