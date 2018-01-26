#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GTextLabel.h"
#include "LStringLayout.h"
#include "GVariant.h"
#include "GNotifications.h"
#include "LgiRes.h"

class GTextPrivate : public LStringLayout, public LMutex
{
	GText *Ctrl;
	GFontCache Cache;

public:
	/// When GText::Name(W) is called out of thread, the string is put
	/// here first and then a message is posted over to the GUI thread
	/// to load it into the ctrl.
	GString ThreadName;

	GTextPrivate(GText *ctrl) : Cache(), LStringLayout(&Cache), LMutex("GTextPrivate")
	{
		Ctrl = ctrl;
		AmpersandToUnderline = true;
	}

	bool PreLayout(int32 &Min, int32 &Max)
	{
		if (Lock(_FL))
		{
			DoPreLayout(Min, Max);
			Unlock();
		}
		else return false;
		return true;
	}

	bool Layout(GFont *Base, int Px)
	{		
		if (Lock(_FL))
		{
			SetBaseFont(Base);
			DoLayout(Px);
			Unlock();
		}
		else return false;
		return true;
	}
};

GText::GText(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_StaticText)
{
	d = new GTextPrivate(this);
	Name(name);

	if (cx < 0) cx = d->GetMax().x >> GDisplayString::FShift;
	if (cy < 0) cy = d->GetMax().y;

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
}

GText::~GText()
{
	DeleteObj(d);
}

void GText::OnAttach()
{
	LgiResources::StyleElement(this);
	GView::OnAttach();
}

bool GText::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	if (p == ObjStyle)
	{
		const char *Style = Value.Str();
		if (Style)
		{
			GetCss(true)->Parse(Style, GCss::ParseRelaxed);
		}
		return true;
	}
	
	return false;
}

bool GText::GetWrap()
{
	return d->GetWrap();
}

void GText::SetWrap(bool b)
{
	d->SetWrap(b);
	d->Layout(GetFont(), X());
	Invalidate();
}

bool GText::Name(const char *n)
{
	if (!d->Lock(_FL))
		return false;

	if (InThread())
	{
		GView::Name(n);

		d->Empty();
		d->Add(n, GetCss());
		d->Layout(GetFont(), X());

		Invalidate();
		SendNotify(GNotifyTableLayout_Refresh);
	}
	else if (IsAttached())
	{
		d->ThreadName = n;
		PostEvent(M_TEXT_UPDATE_NAME);
	}
	else LgiAssert(!"Can't update name.");

	d->Unlock();

	return true;
}

bool GText::NameW(const char16 *n)
{
	if (!d->Lock(_FL))
		return false;

	if (InThread())
	{
		GView::NameW(n);

		d->Empty();
		d->Add(GView::Name(), GetCss());
		d->Layout(GetFont(), X());

		Invalidate();
		SendNotify(GNotifyTableLayout_Refresh);
	}
	else if (IsAttached())
	{
		d->ThreadName = n;
		PostEvent(M_TEXT_UPDATE_NAME);
	}
	else LgiAssert(!"Can't update name.");

	d->Unlock();

	return true;
}

void GText::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->Layout(GetFont(), X());
	Invalidate();
}

int64 GText::Value()
{
	char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void GText::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LGI_PrintfInt64, i);
	Name(Str);
}

void GText::OnStyleChange()
{
	if (d->Lock(_FL))
	{
		d->Empty();
		d->Add(GView::Name(), GetCss());
		d->Unlock();
	}
}

void GText::OnPosChange()
{
	// if (d->GetWrap())
	d->Layout(GetFont(), X());
}

bool GText::OnLayout(GViewLayoutInfo &Inf)
{
	if (!Inf.Width.Min)
	{
		d->PreLayout(Inf.Width.Min,
					 Inf.Width.Max);
	}
	else
	{
		d->Layout(GetFont(), Inf.Width.Max);
		Inf.Height.Min = d->GetMin().y;
		Inf.Height.Max = d->GetMax().y;
	}
	
	return true;
}

GMessage::Result GText::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_TEXT_UPDATE_NAME:
		{
			if (d->Lock(_FL))
			{
				GString s = d->ThreadName;
				d->ThreadName.Empty();
				d->Unlock();
				
				Name(s);
			}
			break;
		}
	}
	
	return GView::OnEvent(Msg);
}

int GText::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl == (GViewI*)this &&
		Flags == GNotify_Activate &&
		GetParent())
	{
		GAutoPtr<GViewIterator> c(GetParent()->IterateViews());
		if (c)
		{
			int Idx = c->IndexOf(this);
			GViewI *n = (*c)[++Idx];
			if (n)
				n->OnNotify(n, Flags);
		}
	}

	return 0;
}

void GText::OnPaint(GSurface *pDC)
{
	// bool Status = false;

	GColour Back;
	Back.Set(LC_MED, 24);
	if (GetCss())
	{
		GCss::ColorDef Fill = GetCss()->BackgroundColor();
		if (Fill.Type == GCss::ColorRgb)
			Back.Set(Fill.Rgb32, 32);
		else if (Fill.Type == GCss::ColorTransparent)
			Back.Empty();
	}

	if (d->Lock(_FL))
	{
		GRect c = GetClient();
		GdcPt2 pt(c.x1, c.y1);
		d->Paint(pDC, pt, Back, c, Enabled());
		d->Unlock();
	}
	else if (!Back.IsTransparent())
	{
		pDC->Colour(Back);
		pDC->Rectangle();
	}

	#if 0
	pDC->Colour(Rgb24(255, 0, 0), 24);
	pDC->Box(0, 0, X()-1, Y()-1);
	#endif	
}
