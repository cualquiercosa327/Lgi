#include "Lgi.h"
#include "GSlider.h"
#include <COMMCTRL.H>

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	GControl(LGI_SLIDER),
	ResObject(Res_Slider)
{
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	Vertical = vert;
	Min = Max = 0;
	Val = 0;

	SetStyle(GetStyle() | WS_VISIBLE | WS_CHILD | WS_TABSTOP | ((Vertical) ? TBS_VERT : TBS_HORZ));
	if (SubClass && !SubClass->SubClass(TRACKBAR_CLASS))
	{
		LgiAssert(0);
	}

	// SetBackgroundFill(new GViewFill(LC_MED, 24));
}

GSlider::~GSlider()
{
}

int GSlider::SysOnNotify(int Code)
{
	return 0;
}

void GSlider::Value(int64 i)
{
	Val = i;
	if (Handle())
	{
		SendMessage(Handle(), TBM_SETPOS, TRUE, Val);
	}
}

int64 GSlider::Value()
{
	if (Handle())
	{
		Val = SendMessage(Handle(), TBM_GETPOS, 0, 0);
	}
	return Val;
}

void GSlider::GetLimits(int64 &min, int64 &max)
{
	if (Handle())
	{
		min = Min = SendMessage(Handle(), TBM_GETRANGEMIN, 0, 0);
		max = Max = SendMessage(Handle(), TBM_GETRANGEMAX, 0, 0);
	}
	else
	{
		min = Min;
		max = Max;
	}
}

void GSlider::SetLimits(int64 min, int64 max)
{
	Min = min;
	Max = max;
	if (Handle())
	{
		SendMessage(Handle(), TBM_SETRANGEMIN, false, (LPARAM)Min);
		SendMessage(Handle(), TBM_SETRANGEMAX, true, (LPARAM)Max);
	}
}

GMessage::Result GSlider::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_HSCROLL:
		case WM_VSCROLL:
		{
			SendNotify();
			break;
		}
		case WM_ERASEBKGND:
		{
			GViewFill *f = GetBackgroundFill();
			if (f)
			{
				HDC hDC = (HDC)MsgA(Msg);
				GScreenDC dc(hDC, Handle());
				f->Fill(&dc, &GetClient());
				return 1;
			}
			break;
		}
		case WM_PAINT:
		{
			GViewFill *f = GetBackgroundFill();
			if (f)
			{
				GScreenDC dc(Handle());
				f->Fill(&dc, &GetClient());
				
				RECT rc;
				SendMessage(Handle(), TBM_GETCHANNELRECT, 0, (LPARAM)&rc);
				GRect r = rc;
				LgiWideBorder(&dc, r, SUNKEN);

				SendMessage(Handle(), TBM_GETTHUMBRECT, 0, (LPARAM)&rc);
				GRect t = rc;
				LgiWideBorder(&dc, t, RAISED);
				dc.Colour(LC_MED, 24);
				dc.Rectangle(&t);

				if (GetFocus() == Handle())
				{
					RECT rc = GetClient();
					DrawFocusRect(dc.Handle(), &rc);
				}
				return 0;
			}
			break;
		}
	}

	GMessage::Result Status = GControl::OnEvent(Msg);

	switch (MsgCode(Msg))
	{
		case WM_CREATE:
		{
			SetLimits(Min, Max);
			Value(Val);
			break;
		}
	}

	return Status;
}

////////////////////////////////////////////////////////////////////////
class GSlider_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (stricmp(Class, "GSlider") == 0)
		{
			return new GSlider(-1, 0, 0, 100, 20, 0, 0);
		}

		return 0;
	}

} GHtml_Factory;
