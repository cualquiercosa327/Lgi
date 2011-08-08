/*
**	FILE:			GLog.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			28/6/99
**	DESCRIPTION:	Reality logging system
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <time.h>

#include "Lgi.h"
#include "GLog.h"
#include "GScrollBar.h"

////////////////////////////////////////////////////////////////////
RLogEntry::RLogEntry(const char *t, const char *desc, int Len, COLOUR Col)
{
	c = Col;
	if (desc)
	{
		Desc = NewStr(desc);
	}
	else
	{
		char TimeStr[40];
		time_t Time = time(NULL);
		tm *ptm = localtime(&Time);
		strftime(TimeStr, sizeof(TimeStr)-1, "%d/%m/%Y %H:%M:%S", ptm);

		Desc = NewStr(TimeStr);
	}

	Text = NewStr(t, Len);
}

RLogEntry::~RLogEntry()
{
	DeleteArray(Desc);
	DeleteArray(Text);
}

////////////////////////////////////////////////////////////////////
RLogView::RLogView(GLog *log)
{
	Log = log;
	HasBorder = true;
	IsTopDown = true;
	SplitPos = 150;
	Sunken(true);
	SetPourLargest(true);
	
	if (VScroll)
	{
		VScroll->SetLimits(0, -1);
	}

	
	#ifdef BEOS
	Handle()->SetViewColor(B_TRANSPARENT_COLOR);
	#endif
}

int RLogView::GetScreenItems()
{
	GRect Client = GetClient();
	int y = SysFont->GetHeight();
	return Client.Y() / y;
}

int RLogView::GetTotalItems()
{
	return (Log) ? Log->Entries.Length() : 0;
}

void RLogView::OnPosChange()
{
	GLayout::OnPosChange();
	UpdateScrollBar();
}

int RLogView::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl == (GViewI*)this)
	{
		UpdateScrollBar();
		if (VScroll)
		{
			if (TopDown())
			{
				int n = GetTotalItems();
				int Screen = GetScreenItems();
				if (n > Screen)
				{
					VScroll->Value(n-Screen);
				}
			}
		}

		// show new data
		Invalidate();
		#ifdef WIN32
		UpdateWindow(Handle());
		#endif
	}
	else if (Ctrl->GetId() == IDC_VSCROLL OR
			 Ctrl->GetId() == IDC_HSCROLL)
	{
		Invalidate();
	}

	return 0;
}

void RLogView::UpdateScrollBar()
{
	int Screen = GetScreenItems();
	int Total = GetTotalItems();

	SetScrollBars(false, Screen < Total);
	if (VScroll AND Log)
	{
		VScroll->SetLimits(0, Total-1);
		VScroll->SetPage(Screen);
		
		int Pos = VScroll->Value();
		if (TopDown())
		{
			if (Pos > (Total - Screen))
			{
				VScroll->Value(Total - Screen + 1);
			}
		}
	}
}

void RLogView::OnPaint(GSurface *pDC)
{
	GRect r(GetClient());
	r.Offset(-r.x1, -r.y1);

	pDC->Colour(LC_MED, 24);

	if (Log AND r.Valid())
	{
		SysFont->Back(LC_WORKSPACE);
		SysFont->Transparent(false);

		int y = SysFont->GetHeight();
		int Screen = GetScreenItems();
		int Index = max((VScroll) ? (int)VScroll->Value() : 0, 0);
		int n = GetTotalItems() - 1;
		RLogEntry *e = 0;
		
		if (TopDown())
		{
			e = Log->Entries.ItemAt(max(Index, 0));
		}
		else
		{
			e = Log->Entries.ItemAt(min(Screen+Index-1, n));
		}

		while (e)
		{
			GRect t;

			SysFont->Fore(e->c);

			if (TopDown())
			{
				if (r.y1 + y >= r.y2)
				{
					break;
				}

				t.Set(r.x1, r.y1, r.x1 + SplitPos, r.y1+y-1);
			}
			else
			{
				if (r.y2 - y + 1 < r.y1)
				{
					break;
				}

				t.Set(r.x1, r.y2-y+1, r.x1 + SplitPos, r.y2);
			}

			GDisplayString ds(SysFont, e->Desc);
			ds.Draw(pDC, t.x1, t.y1, &t);
			t.x1 = t.x2+1;
			t.x2 = r.x2;
			if (ValidStr(e->Text))
			{
				GDisplayString ds(SysFont, e->Text);
				ds.Draw(pDC, t.x1, t.y1, &t);
			}
			else
			{
				pDC->Colour(LC_WORKSPACE, 24);
				pDC->Rectangle(&t);
			}

			if (TopDown())
			{
				r.y1 += y;
				e = Log->Entries.Next();
			}
			else
			{
				r.y2 -= y;
				e = Log->Entries.Prev();
			}
		}
	}

	if (r.Valid())
	{
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(&r);
	}
}

void RLogView::OnNcPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	if (r.Valid())
	{
		pDC->Colour(LC_MED, 24);
		if (Sunken() OR Raised())
		{
			LgiWideBorder(pDC, r, Sunken() ? SUNKEN : RAISED);
		}
	}
}

void RLogView::OnNcCalcClient(long &x1, long &y1, long &x2, long &y2)
{
	int d = -2;

	d += (Sunken() OR Raised()) ? 2 : 0;

	x1 += d;
	y1 += d;
	x2 -= d;
	y2 -= d;

}

GMessage::Result RLogView::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		#ifdef WIN32
		case WM_NCPAINT:
		{
			HDC hDC = GetWindowDC(Handle());
			if (hDC)
			{
				GScreenDC DeviceContext(hDC, Handle());
				OnNcPaint(&DeviceContext);
			}

			return DefWindowProc(Handle(), Msg->Msg, Msg->a, Msg->b);
		}
		case WM_NCCALCSIZE:
		{
			RECT *rc = ((NCCALCSIZE_PARAMS*) Msg->b)->rgrc;
			OnNcCalcClient(	rc->left,
							rc->top, 
							rc->right,
							rc->bottom);

			// this is required to draw the scroll bar
			return DefWindowProc(Handle(), Msg->Msg, Msg->a, Msg->b);
		}
		#endif
	}

	return GLayout::OnEvent(Msg);
}

////////////////////////////////////////////////////////////////////
GLog::GLog(char *File)
{
	LogView = 0;
	FileName = (File) ? NewStr(File) : 0;
	if (FileName)
	{
		FileDev->Delete(FileName);
	}
}

GLog::~GLog()
{
	if (LogView)
	{
		LogView->Log = 0;
	}

	DeleteArray(FileName);
	Entries.DeleteObjects();
}

void GLog::SetView(RLogView *View)
{
	LogView = View;
}

void GLog::Write(COLOUR c, const char *Buffer, int Len, char *Desc)
{
	if (Buffer)
	{
		RLogEntry *Entry = 0;
		Entries.Insert(Entry = new RLogEntry(Buffer, Desc, Len, c));
		if (Entry AND FileName)
		{
			GFile F;
			if (F.Open(FileName, O_WRITE))
			{
				F.Seek(F.GetSize(), SEEK_SET);
				if (Entry->Desc)
				{
					F.Write(Entry->Desc, strlen(Entry->Desc));
					F.Write((char*)"   ", 3);
				}
				F.Write((char*)Buffer, strlen(Buffer));
				F.Write((char*)"\r\n", 2);
				F.Close();
			}
		}

		if (LogView AND LogView->Visible())
		{
			LogView->OnNotify(LogView, 0);
		}
	}
}

void GLog::Print(COLOUR c, const char *Str, ...)
{
	if (Str)
	{
		char Buffer[1025];

		va_list Arg;
		va_start(Arg, Str);
		vsprintf(Buffer, Str, Arg);
		va_end(Arg);
		
		Write(c, Buffer);
	}
}

