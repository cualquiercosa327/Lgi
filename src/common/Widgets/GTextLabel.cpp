#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GTextLabel.h"
#include "GDisplayStringLayout.h"

class GTextPrivate : public GDisplayStringLayout, public GMutex
{
	GText *Ctrl;

public:
	GTextPrivate(GText *ctrl) : GMutex("GTextPrivate")
	{
		Ctrl = ctrl;
	}

	bool PreLayout(int &Min, int &Max)
	{
		if (Lock(_FL))
		{
			DoPreLayout(Ctrl->GetFont(), Ctrl->GBase::Name(), Min, Max);
			Unlock();
		}
		else return false;
		return true;
	}

	bool Layout(int Px)
	{		
		if (Lock(_FL))
		{
			DoLayout(Ctrl->GetFont(), Ctrl->GBase::Name(), Px);
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

	if (cx < 0) cx = d->Max.x;
	if (cy < 0) cy = d->Max.y;

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
}

GText::~GText()
{
	DeleteObj(d);
}

bool GText::GetWrap()
{
	return d->Wrap;
}

void GText::SetWrap(bool b)
{
	d->Wrap = b;
	d->Layout(X());
	Invalidate();
}

bool GText::Name(const char *n)
{
	bool Status = GView::Name(n);
	d->Layout(X());
	Invalidate();
	return Status;
}

bool GText::NameW(const char16 *n)
{
	bool Status = GView::NameW(n);
	d->Layout(X());
	Invalidate();
	return Status;
}

void GText::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->Layout(X());
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

void GText::OnPosChange()
{
	if (d->Wrap)
	{
		d->Layout(X());
	}
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
		d->Layout(Inf.Width.Max);
		Inf.Height.Min = d->Min.y;
		Inf.Height.Max = d->Max.y;
	}
	
	return true;
}

void GText::OnPaint(GSurface *pDC)
{
	bool Status = false;

	GColour Fore, Back;
	if (GetCss())
	{
		GCss::ColorDef Fill = GetCss()->Color();
		if (Fill.Type == GCss::ColorRgb)
			Fore.Set(Fill.Rgb32, 32);
			
		Fill = GetCss()->BackgroundColor();
		if (Fill.Type == GCss::ColorRgb)
			Back.Set(Fill.Rgb32, 32);
	}
	
	if (!Fore.IsValid())
		Fore.Set(LC_TEXT, 24);
	if (!Back.IsValid())
		Back.Set(LC_MED, 24);
	
	GFont *f = GetFont();
	if (d->Lock(_FL))
	{
		GRect c = GetClient();
		d->Paint(pDC, f, c, Fore, Back, Enabled());
		d->Unlock();
	}
	else
	{
		pDC->Colour(Back);
		pDC->Rectangle();
	}

	#if 0
	pDC->Colour(Rgb24(255, 0, 0), 24);
	pDC->Box(0, 0, X()-1, Y()-1);
	#endif	
}
