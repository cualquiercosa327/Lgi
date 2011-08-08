#include "Lgi.h"
#include "GProgress.h"
#include <COMMCTRL.H>

///////////////////////////////////////////////////////////////////////////////////////////
GProgress::GProgress(int id, int x, int y, int cx, int cy, const char *name) :
	GControl(LGI_PROGRESS),
	ResObject(Res_Progress)
{
	Shift = 0;
	SetClassW32(LGI_PROGRESS);
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	if (name) GControl::Name(name);

	SetStyle(GetStyle() | WS_CHILD | WS_TABSTOP | WS_VISIBLE | PBS_SMOOTH);
	if (SubClass)
	{
		SubClass->SubClass(PROGRESS_CLASS);
	}
}

GProgress::~GProgress()
{
}

void GProgress::SetLimits(int64 l, int64 h)
{
	Low = l;
	High = h;
	Shift = 0;

	if (h > 0x7fffffff)
	{
		int64 i = h;
		while (i > 0x7fffffff)
		{
			i >>= 1;
			Shift++;
		}
	}

	if (Handle())
	{
		PostMessage(Handle(), PBM_SETRANGE32, Low>>Shift, High>>Shift);
	}
}

int64 GProgress::Value()
{
	return Val;
}

void GProgress::Value(int64 v)
{
	Val = v;
	if (Handle())
	{
		// DWORD low = SendMessage(Handle(), PBM_GETRANGE, true, 0);
		// DWORD high = SendMessage(Handle(), PBM_GETRANGE, false, 0);

		PostMessage(Handle(), PBM_SETPOS, (WPARAM) (Val>>Shift), 0);
	}
}

GMessage::Result GProgress::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_CREATE:
		{
			PostMessage(Handle(), PBM_SETRANGE32, Low>>Shift, High>>Shift);
			PostMessage(Handle(), PBM_SETPOS, (WPARAM) Val>>Shift, 0);

			GViewFill *f;
			if (f = GetForegroundFill())
			{
				GColour c = f->GetFlat();
				PostMessage(_View, PBM_SETBARCOLOR, 0, c.c24());
			}
			if (f = GetBackgroundFill())
			{
				GColour c = f->GetFlat();
				PostMessage(_View, PBM_SETBKCOLOR, 0, c.c24());
			}
			break;
		}
	}

	return GControl::OnEvent(Msg);
}

bool GProgress::SetForegroundFill(GViewFill *Fill)
{
	if (_View AND Fill)
	{
		GColour c32 = Fill->GetFlat();
		SendMessage(_View, PBM_SETBARCOLOR, 0, c32.c24());
	}
	else
	{
		SendMessage(_View, PBM_SETBARCOLOR, 0, CLR_DEFAULT);		 
	}

	return GView::SetForegroundFill(Fill);
}

bool GProgress::SetBackgroundFill(GViewFill *Fill)
{
	if (_View AND Fill)
	{
		GColour c = Fill->GetFlat();
		SendMessage(_View, PBM_SETBKCOLOR, 0, c.c24());
	}
	else
	{
		SendMessage(_View, PBM_SETBKCOLOR, 0, CLR_DEFAULT);		 
	}

	return GView::SetForegroundFill(Fill);
}
