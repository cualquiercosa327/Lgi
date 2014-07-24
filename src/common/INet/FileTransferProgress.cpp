/*
**	FILE:			FtpApp.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			7/9/99
**	DESCRIPTION:	i.Ftp application
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

// Debug defines

// Includes
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include "Lgi.h"
#include "FileTransferProgress.h"
#include "GSlider.h"
#include "GVariant.h"
#include "GDisplayString.h"

//////////////////////////////////////////////////////////////////
#define THROTTLE_TEXT_WIDTH		80
#define SLIDER_ID				100

int PipeSize[] = { 29491, 56 << 10, 64 << 10, 128 << 10, 256 << 10, 512 << 10, 1500 << 10, 8 << 20, 22 << 20, 0 };

class GPaneThrottle : public GStatusPane
{
	GSlider *Slider;
	GDom *App;
	int Pipe;

	void Set()
	{
		GRect r(0, 0, THROTTLE_TEXT_WIDTH, Y()-1);
		GView::Invalidate(&r);

		if (App)
		{
			/*
			App->SetDataRate(	(Value() == 100) ? -1 :
								(Value() * (PipeSize[Pipe]/8)) / 100 );
			*/

			GVariant v;
			App->SetValue(OPT_Throttle, v = Value());
			App->SetValue(OPT_PipeSize, v = Pipe);
		}
	}

public:
	GPaneThrottle(GDom *app);

	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPaint(GSurface *pDC);
	int64 Value();
	void OnMouseClick(GMouse &m);
};

GPaneThrottle::GPaneThrottle(GDom *app)
{
	Pipe = -1;
	App = app;
	Width = 80+THROTTLE_TEXT_WIDTH;
	Slider = new GSlider(SLIDER_ID, 0, 0, 100, 20, "Throttle", false);
}

int GPaneThrottle::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl && Ctrl->GetId() == SLIDER_ID)
	{
		Set();
	}

	return 0;
}

void GPaneThrottle::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);

	int x, y;
	pDC->GetOrigin(x, y);

	LgiThinBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(r.x1, r.y1, r.x1+THROTTLE_TEXT_WIDTH, r.y2);

	if (Slider)
	{
		GRect Sr = r;
		#ifdef WIN32
		Sr.Offset(GetPos().x1, GetPos().y1);
		#endif
		Sr.x1 += THROTTLE_TEXT_WIDTH+1;
		Slider->SetPos(Sr);

		if (!Slider->IsAttached())
		{
			#ifdef WIN32
			Slider->Attach(GetParent());
			#else
			Slider->Attach(this);
			#endif
			Slider->SetNotify(this);
			Slider->SetLimits(0, 100);
			Slider->Value(100);

			if (Pipe < 0)
			{
				GVariant v;
				if (App->GetValue(OPT_PipeSize, v))
					Pipe = v.CastInt32();
				if (Slider)
				{
					v = 100;
					if (App->GetValue(OPT_Throttle, v))
						Slider->Value(v.CastInt32());
				}
			}
			
			// DumpHnd(HIViewGetRoot(GetWindow()->WindowHandle()));
		}
		
		char Str[256];
		int64 v = Value();
		if (v < 100)
		{
			if (PipeSize[Pipe] < (1 << 20))
			{
				sprintf(Str, "%i%% of %i Kbps", (int)v, PipeSize[Pipe]>>10);
			}
			else
			{
				sprintf(Str, "%i%% of %.1f Mbps", (int)v, (double)(PipeSize[Pipe]>>20));
			}
		}
		else
		{
			strcpy_s(Str, sizeof(Str), "No limit");
		}
		
		SysFont->Colour(0, LC_MED);
		GDisplayString ds(SysFont, Str);
		ds.Draw(pDC, r.x1+2, r.y1);
	}
}

int64 GPaneThrottle::Value()
{
	return (Slider) ? Slider->Value() : 100;
}

void GPaneThrottle::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		GSubMenu *RClick = new GSubMenu;
		if (RClick)
		{
			for (int i=0; PipeSize[i]; i++)
			{
				char Str[256];
				if (PipeSize[i] < (1 << 20))
				{
					sprintf(Str, "%i Kbps", PipeSize[i]>>10);
				}
				else
				{
					sprintf(Str, "%.1f Mbps", (double)PipeSize[i] / 1024.0 / 1024.0);
				}
				GMenuItem *Item = RClick->AppendItem(Str, 100+i, true);
				if (Item && i == Pipe)
				{
					Item->Checked(true);
				}
			}

			if (GetMouse(m, true))
			{
				int Cmd = RClick->Float(this, m.x, m.y, m.Left());
				if (Cmd >= 100)
				{
					Pipe = Cmd - 100;
					Set();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define HISTORY_TEXT_WIDTH		70
#define MAX_SAMPLE				70
#define HISTORY_DIV				1000

class GPaneHistory : public GStatusPane
{
	GDom *App;
	int64 Cur;
	int64 Max;
	int64 Last;
	int64 PartialTime;
	int Bytes[MAX_SAMPLE];

	GSurface *pMemDC;

	void Push()
	{
		for (int i=MAX_SAMPLE-1; i>0; i--)
		{
			Bytes[i] = Bytes[i-1];
		}
		Bytes[0] = 0;
	}

	void Clear()
	{
		for (int i=0; i<MAX_SAMPLE; i++)
		{
			Bytes[i] = 0;
		}
	}

public:
	GPaneHistory(GDom *app);
	~GPaneHistory();

	void OnPaint(GSurface *pDC);
	void Value(int64 i);
};

GPaneHistory::GPaneHistory(GDom *app)
{
	Width = HISTORY_TEXT_WIDTH + MAX_SAMPLE + 2;
	App = app;
	pMemDC = 0;
	Cur = 0;
	Max = 0;
	Last = 0;
	Clear();
}

GPaneHistory::~GPaneHistory()
{
	DeleteObj(pMemDC);
}

void GPaneHistory::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);

	LgiThinBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(r.x1, r.y1, r.x1+HISTORY_TEXT_WIDTH, r.y2);

	char Str[256];
	sprintf(Str, "%.1f K/s", Max / 1024.0);
	SysFont->Colour(0, LC_MED);
	GDisplayString ds(SysFont, Str);
	ds.Draw(pDC, r.x1+2, r.y1);

	r.x1 += HISTORY_TEXT_WIDTH+1;

	if (!pMemDC ||
		(r.X() != pMemDC->X()))
	{
		DeleteObj(pMemDC);
		pMemDC = new GMemDC;
		if (pMemDC)
		{
			if (pMemDC->Create(r.X(), r.Y(), GdcD->GetBits()))
			{
				pMemDC->Colour(Rgb24(0, 0, 0), 24);
				pMemDC->Rectangle();
			}
		}
	}

	if (pMemDC)
	{
		pDC->Blt(r.x1, r.y1, pMemDC);
	}
	else
	{
		pDC->Colour(LC_LOW, 24);
		pDC->Rectangle(&r);
	}
}

void GPaneHistory::Value(int64 i)
{
	if (i <= 0)
	{
		Max = 0;
		Last = LgiCurrentTime();
		Cur = -i;
		PartialTime = 0;
		Clear();
	}
	else
	{
		int64 Now = LgiCurrentTime();
		int64 NewData = i - Cur;
		int64 Diff = Now - Last;

		if (Diff > 0)
		{
			double Sec = (double)Diff / 1000.0;
			double Rate = (double) NewData / Sec;
			
			// do any fractional second from Last -> next second
			if ((Last / 1000) != (Now / 1000))
			{
				int Ms = 1000 - (Last % 1000);
				double LastSec = (double)Ms / 1000.0;
				int Len = (int)(Rate * LastSec);

				Bytes[0] += Len;
				NewData -= Len;
				Diff -= Ms;
				
				Max = max(Bytes[0], Max);
				Push();
			}

			// do any whole seconds...
			while (Diff > 1000)
			{
				Bytes[0] = (int)Rate;
				NewData -= Bytes[0];
				Diff -= 1000;

				Max = max(Bytes[0], Max);
				Push();
			}

			// Any last partial second left to process.
			Bytes[0] += NewData;
			Max = max(Bytes[0], Max);
			Last = Now;
		}
		else
		{
			Bytes[0] += NewData;
		}
		Cur = i;

		// draw
		if (pMemDC && Max > 0)
		{
			pMemDC->Colour(Rgb24(0, 0, 0), 24);
			pMemDC->Rectangle();
			pMemDC->Colour(Rgb24(0, 255, 0), 24);

			int m=0;
			for (int x=0; x<pMemDC->X() && x<MAX_SAMPLE; x++)
			{
				m = max(Bytes[x], m);
				int Sample = (Bytes[x] * pMemDC->Y()) / Max;
				pMemDC->Line(x, pMemDC->Y()-Sample, x, pMemDC->Y());
			}

			if (m < Max)
			{
				Max = m;
			}
		}
	}

	GView::Invalidate();
}

//////////////////////////////////////////////////////////////////////////////////////
FileTransferProgress::FileTransferProgress(	GDom *App,
											GStatusBar *Status,
											bool Limit) : Timer(300)
{
	StartTime = StartPos = 0;
	ProgressPane = 0;

	// Download throttle
	StatusInfo[_STATUS_THROTTLE]	= (Limit) ? new GPaneThrottle(App) : 0;
	if (StatusInfo[_STATUS_THROTTLE]) Status->AppendPane(StatusInfo[_STATUS_THROTTLE]);
	
	// Historical graph
	StatusInfo[_STATUS_HISTORY]		= new GPaneHistory(App);
	if (StatusInfo[_STATUS_HISTORY]) Status->AppendPane(StatusInfo[_STATUS_HISTORY]);

	// current download position
	StatusInfo[_STATUS_POSITION]	= Status->AppendPane("", 120);
	if (StatusInfo[_STATUS_POSITION]) StatusInfo[_STATUS_POSITION]->Sunken(true);
	
	// progress meter
	StatusInfo[_STATUS_PROGRESS]	= ProgressPane = new GProgressStatusPane;
	if (StatusInfo[_STATUS_PROGRESS]) Status->AppendPane(ProgressPane);
	
	// download rate
	StatusInfo[_STATUS_RATE]		= Status->AppendPane("", 70);
	if (StatusInfo[_STATUS_RATE]) StatusInfo[_STATUS_RATE]->Sunken(true);
	
	// estimated time left
	StatusInfo[_STATUS_TIME_LEFT]	= Status->AppendPane("", 70);
	if (StatusInfo[_STATUS_TIME_LEFT]) StatusInfo[_STATUS_TIME_LEFT]->Sunken(true);
}

void FileTransferProgress::SetLimits(int64 l, int64 h)
{
	Progress::SetLimits(l, h);
	if (ProgressPane)
	{
		ProgressPane->SetLimits(l, h);
	}
}

void FileTransferProgress::Value(int64 v)
{
	bool Reset = v < Val;
	bool Start = Val == 0;
	int64 Now = LgiCurrentTime();
	if (Start)
	{
		StartTime = Now;
	}

	Progress::Value(v);
	
	if (Timer.DoNow() || Reset)
	{
		// Tell everyone about the new value
		if (ProgressPane) ProgressPane->Value(v);
		if (StatusInfo[_STATUS_HISTORY]) StatusInfo[_STATUS_HISTORY]->Value((Start) ? -v : v);
		
		if (Reset)
		{
			StatusInfo[_STATUS_POSITION]->Name("");
			StatusInfo[_STATUS_TIME_LEFT]->Name("");
			StatusInfo[_STATUS_RATE]->Name("");
		}
		else if (High > 0)
		{
			if (StatusInfo[_STATUS_POSITION])
			{
				char a[64], b[64], Str[128];
				LgiFormatSize(a, sizeof(a), Val);
				LgiFormatSize(b, sizeof(b), High);
				sprintf(Str, "%s of %s", a, b);
				StatusInfo[_STATUS_POSITION]->Name(Str);
			}

			double Rate = 0.0;
			double Seconds = ((double)Now-(double)StartTime)/1000;
			if (StatusInfo[_STATUS_RATE] && Seconds > 0.0)
			{
				char Str[256];
				Rate = ((double)(Val-StartPos))/Seconds;

				sprintf(Str, "%.2f K/s", Rate/1024.0);
				StatusInfo[_STATUS_RATE]->Name(Str);
			}

			if (StatusInfo[_STATUS_TIME_LEFT] && Rate > 0.0)
			{
				char Str[256];
				double Time = ((double) (High-StartPos) / Rate) - Seconds + 0.5;
				sprintf(Str, "%i:%2.2i:%2.2i", (int)(Time/3600), ((int)(Time/60))%60, ((int)Time)%60);
				StatusInfo[_STATUS_TIME_LEFT]->Name(Str);
			}
		}
		else
		{
			if (StatusInfo[_STATUS_POSITION])
			{
				char Str[256];
				sprintf(Str, LGI_PrintfInt64" K", Val>>10);
				StatusInfo[_STATUS_POSITION]->Name(Str);
			}
		}
	}
}

void FileTransferProgress::SetParameter(int Which, int What)
{
	switch (Which)
	{
		case PARM_START_VALUE:
		{
			Progress::Value(StartPos = What);
			if (StatusInfo[_STATUS_HISTORY]) StatusInfo[_STATUS_HISTORY]->Value(-Val);
		}
	}
}
