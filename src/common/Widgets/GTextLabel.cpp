#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GTextLabel.h"
#include "GDisplayStringLayout.h"
#include "GVariant.h"
#include "GNotifications.h"
#include "LgiRes.h"

class GTextPrivate : public GDisplayStringLayout, public LMutex
{
	GText *Ctrl;

public:
	/// When GText::Name(W) is called out of thread, the string is put
	/// here first and then a message is posted over to the GUI thread
	/// to load it into the ctrl.
	GAutoString ThreadName;

	GTextPrivate(GText *ctrl) : LMutex("GTextPrivate")
	{
		Ctrl = ctrl;
	}

	bool PreLayout(int32 &Min, int32 &Max)
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
			GFont *f = Ctrl->GetFont() &&
						Ctrl->GetFont()->Handle() ?
						Ctrl->GetFont() :
						SysFont;
				
			DoLayout(f, Ctrl->GBase::Name(), Px);
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
	if (!_View || InThread())
	{
		if (!GView::Name(n))
			return false;
		d->Layout(X());
		Invalidate();
	}
	else if (d->Lock(_FL))
	{
		d->ThreadName.Reset(NewStr(n));
		d->Unlock();
		if (IsAttached())
			PostEvent(M_TEXT_UPDATE_NAME);
		else
		{
			#ifndef BEOS
			LgiAssert(!"Can't update name.");
			#endif
			if (!GView::Name(n))
				return false;
			return false;
		}
	}	
	else return false;
	
	return true;
}

bool GText::NameW(const char16 *n)
{
	if (InThread())
	{
		if (!GView::NameW(n))
			return false;
		d->Layout(X());
		Invalidate();
	}
	else if (d->Lock(_FL))
	{
		d->ThreadName.Reset(WideToUtf8(n));
		d->Unlock();
		if (IsAttached())
			PostEvent(M_TEXT_UPDATE_NAME);
		else
		{
			LgiAssert(!"Can't update name.");
			return false;
		}
	}	
	else return false;
	
	return true;
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

GMessage::Result GText::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_TEXT_UPDATE_NAME:
		{
			if (d->Lock(_FL))
			{
				GAutoString s = d->ThreadName;
				d->Unlock();
				
				if (s)
				{
					Name(s);
					SendNotify(GNotifyTableLayout_Refresh);
				}
			}
			break;
		}
	}
	
	return GView::OnEvent(Msg);
}

void GText::OnPaint(GSurface *pDC)
{
	// bool Status = false;

	GColour Fore, Back;
	Fore.Set(LC_TEXT, 24);
	Back.Set(LC_MED, 24);

	if (GetCss())
	{
		GCss::ColorDef Fill = GetCss()->Color();
		if (Fill.Type == GCss::ColorRgb)
			Fore.Set(Fill.Rgb32, 32);
		else if (Fill.Type == GCss::ColorTransparent)
			Fore.Empty();
			
		Fill = GetCss()->BackgroundColor();
		if (Fill.Type == GCss::ColorRgb)
			Back.Set(Fill.Rgb32, 32);
		else if (Fill.Type == GCss::ColorTransparent)
			Back.Empty();
	}
	
	// GFont *f = GetFont();
	if (d->Lock(_FL))
	{
		GRect c = GetClient();
		GdcPt2 pt(c.x1, c.y1);
		d->Paint(pDC, pt, c, Fore, Back, Enabled());
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
