/*hdr
**      FILE:           GProgressDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           11/11/98
**      DESCRIPTION:    Progress stuff
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <time.h>
#include "Lgi.h"
#include "GProgressDlg.h"
#include "GTextLabel.h"
#include "GButton.h"

/////////////////////////////////////////////////////////////////////////////////////////
ProgressList::ProgressList()
{
	InUse = false;
}

bool ProgressList::Lock()
{
	while (InUse)
	{
		LgiSleep(20);
	}

	InUse = true;

	return true;
}

void ProgressList::Unlock()
{
	InUse = false;
}

/////////////////////////////////////////////////////////////////////////////////////////
Progress::Progress() : GSemaphore("ProgressObj")
{
	Description = 0;
	Start = 0;
	Val = Low = 0;
	High = 0;
	Type = 0;
	Scale = 1.0;
	Canceled = false;
	UserData = 0;
}

Progress::Progress(char *desc, int64 l, int64 h, char *type, double scale)
{
	Description = NewStr(desc);
	Start = 0;
	Val = Low = l;
	High = h;
	Type = type;
	Scale = scale;
}

Progress::~Progress()
{
	DeleteArray(Description);
}

void Progress::SetLimits(int64 l, int64 h)
{
	Low = l;
	High = h;
}

void Progress::GetLimits(int64 *l, int64 *h)
{
	if (l) *l = Low;
	if (h) *h = High;
}

void Progress::SetDescription(const char *d)
{
	char *Desc = NewStr(d);
	DeleteArray(Description);
	Description = Desc;
}

Progress &Progress::operator =(Progress &p)
{
	SetDescription(p.GetDescription());

	int64 h, l;
	p.GetLimits(&h, &l);
	SetLimits(h, l);

	SetScale(p.GetScale());
	SetType(p.GetType());
	Value(p.Value());

	return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////
#define IDC_DESCRIPTION			100
#define IDC_VALUE				101
#define IDC_RATE				102
#define IDC_PROGRESS			103
#define IDC_BUTTON				104

#define PANE_X					260
#define PANE_Y					85

// #define ALT_SCALE				30000

GProgressPane::GProgressPane()
{
	GRect r(0, 0, PANE_X-1, PANE_Y-1);
	SetPos(r);
	Name("Progress");
	Canceled = false;
	Wait = false;
	Ref = 0;

	Children.Insert(Desc	= new GText(IDC_DESCRIPTION, 6, 6, PANE_X - 14, 14, ""));
	Children.Insert(ValText	= new GText(IDC_VALUE, 6, 22, (PANE_X - 20) / 2, 14, ""));
	Children.Insert(Rate	= new GText(IDC_RATE, PANE_X / 2, 22, (PANE_X - 20) / 2, 14, ""));
	Children.Insert(Bar		= new GProgress(IDC_PROGRESS, 6, 41, PANE_X - 14, 10, "Progress"));
	Children.Insert(But		= new GButton(IDC_BUTTON, (PANE_X - 100) / 2, 59, 120, 18, "Request Abort"));
}

GProgressPane::~GProgressPane()
{
}

void GProgressPane::SetLimits(int64 l, int64 h)
{
	Progress::SetLimits(l, h);

	if (Bar)
	{
		#ifdef ALT_SCALE
		Bar->SetLimits(0, ALT_SCALE);
		#else
		Bar->SetLimits(l, h);
		#endif
	}
}

void GProgressPane::Value(int64 v)
{
	char Str[256];

	Progress::Value(v);
	if (Start == 0)
	{
		// initialize the clock
		Start = LgiCurrentTime();
	}
	else if (Rate)
	{
		// calc rate
		uint64 Now = LgiCurrentTime();
		double Secs = ((double)(int64)(Now - Start)) / 1000.0;
		double PerSec;
		
		if (Secs != 0.0)
		{
			PerSec = ((double) Val - Low) / Secs;
		}
		else
		{
			PerSec = 0;
		}
		
		sprintf(Str, "@ %.2f %s / sec", PerSec * Scale, (Type) ? Type : "");
		Rate->Name(Str);
	}

	if (ValText)
	{
		if (Scale == 1.0)
		{
			sprintf(Str, "%.0f of %.0f %s",
				Val * Scale,
				(High - Low) * Scale,
				(Type) ? Type : "");
		}
		else
		{
			sprintf(Str, "%.1f of %.1f %s",
				(double)Val * Scale,
				(double)(High - Low) * Scale,
				(Type) ? Type : "");
		}
		
		ValText->Name(Str);
	}

	if (Bar)
	{
		if (High != Low)
		{
			#ifdef ALT_SCALE
			double Raw = ((double) v - Low) / ((double) High - Low);
			Bar->Value((int)(Raw * ALT_SCALE));
			#else
			Bar->Value(v);
			#endif

			Bar->Invalidate();
		}
		else
		{
			Bar->Value(0);
		}
	}
}

void GProgressPane::OnCreate()
{
	AttachChildren();
}

int GProgressPane::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_BUTTON:
		{
			Cancel(true);

			if (GetParent() AND
				!Wait)
			{
				delete GetParent();
			}

			if (But)
			{
				But->Name("Waiting...");
			}
			break;
		}
	}

	return 0;
}

void GProgressPane::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	LgiThinBorder(pDC, r, RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);
}

bool GProgressPane::Pour(GRegion &r)
{
	GRect *Best = FindLargest(r);
	if (Best)
	{
		GRect r, p = GetPos();
		r.ZOff(PANE_X, PANE_Y);
		r.Offset(Best->x1-p.x1, Best->y1-p.y1);
		SetPos(r, true);
		return true;
	}
	return false;
}

GFont *GProgressPane::GetFont()
{
	// GdcBeTtf *Fnt = SysFont;
	// return (Fnt) ? Fnt->Handle() : 0;
	return 0;
}

char *GProgressPane::GetDescription()
{
	return Progress::GetDescription();
}

void GProgressPane::SetDescription(const char *d)
{
	Progress::SetDescription(d);
	if (Desc)
	{
		Desc->Name(d);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// #ifdef WIN32
// #define DefX		((GetSystemMetrics(SM_CXDLGFRAME) * 2))
// #define DefY		((GetSystemMetrics(SM_CYDLGFRAME) * 2) + GetSystemMetrics(SM_CYCAPTION))
// #else
#define DefX		LgiApp->GetMetric(LGI_MET_DECOR_X)
#define DefY		LgiApp->GetMetric(LGI_MET_DECOR_Y)
// #endif

GProgressDlg::GProgressDlg(GView *parent, bool wait)
{
	Wait = wait;
	SetParent(parent);

	GRect r(0,
			0,
			LgiApp->GetMetric(LGI_MET_DECOR_X) + PANE_X,
			LgiApp->GetMetric(LGI_MET_DECOR_Y) + PANE_Y * 2);
	r.Offset(400, 400);
	SetPos(r);
	MoveToCenter();

	Name("Progress");
	#ifdef BEOS
	WindowHandle()->SetFeel(B_NORMAL_WINDOW_FEEL);
	#endif

	DoModeless();
}

GProgressDlg::~GProgressDlg()
{
	EndModeless(true);
}

void GProgressDlg::OnCreate()
{
	if (Progri.Length() == 0)
		Push();
}

int GProgressDlg::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		#ifdef WIN32
		case WM_CLOSE:
		{
			return 0;
		}
		#endif
	}

	return GDialog::OnEvent(Msg);
}

GProgressPane *GProgressDlg::ItemAt(int i)
{
	return Progri.ItemAt(i);
}

GProgressPane *GProgressDlg::Push()
{
	GProgressPane *Pane = new GProgressPane;
	if (Pane)
	{
		// Attach the new pane..
		Pane->Wait = Wait;
		Pane->Attach(this);
		Progri.Insert(Pane);

		// Resize the window to fit the panels
		GRect r, c = GetPos();
		r.ZOff(DefX, DefY);
		r.x2 += PANE_X - 1;
		r.y2 += (PANE_Y * Progri.Length()) - 1;
		r.Offset(c.x1, c.y1);
		SetPos(r);

		// Layout all the panes...
		int y = 0;
		for (int i=0; i<Progri.Length(); i++)
		{
			GProgressPane *p = Progri[i];
			if (p)
			{
				GRect r(0, y, p->X()-1, y + p->Y()-1);
				p->SetPos(r);
				p->Visible(true);
				y = r.y2 + 1;
			}
		}
	}

	return Pane;
}

void GProgressDlg::Pop(GProgressPane *p)
{
	GProgressPane *Pane = (p) ? p : Progri.Last();
	if (Pane)
	{
		Pane->Detach();
		Progri.Delete(Pane);
		GView::Invalidate();

		DeleteObj(Pane);

		int y = 0;
		for (GProgressPane *p = Progri.First(); p; p = Progri.Next())
		{
			GRect r(0, y, PANE_X, y + PANE_Y);
			p->SetPos(r);
			y = r.y2 + 1;
		}

		// resize the window to fit the panels
		GRect r, c = GetPos();
		r.ZOff(DefX, DefY);
		r.x2 += PANE_X;
		r.y2 += PANE_Y * Progri.Length();
		r.Offset(c.x1, c.y1);
		SetPos(r, true);
	}
}

char *GProgressDlg::GetDescription()
{
	GProgressPane *Pane = Progri.First();
	return (Pane) ? Pane->GetDescription() : 0;
}

void GProgressDlg::SetDescription(const char *d)
{
	GProgressPane *Pane = Progri.First();
	if (Pane)
	{
		Pane->SetDescription(d);
	}
}

void GProgressDlg::GetLimits(int64 *l, int64 *h)
{
	GProgressPane *Pane = Progri.First();
	if (Pane) Pane->GetLimits(l, h);
	else
	{
		if (l) *l = 0;
		if (h) *h = 0;
	}
}

void GProgressDlg::SetLimits(int64 l, int64 h)
{
	GProgressPane *Pane = Progri.First();
	if (Pane) Pane->SetLimits(l, h);
}

int64 GProgressDlg::Value()
{
	GProgressPane *Pane = Progri.First();
	return (Pane) ? Pane->Value() : 0;
}

void GProgressDlg::Value(int64 v)
{
	GProgressPane *Pane = Progri.First();
	if (Pane) Pane->Value(v);
}

double GProgressDlg::GetScale()
{
	GProgressPane *Pane = Progri.First();
	return (Pane) ? Pane->GetScale() : 0;
}

void GProgressDlg::SetScale(double s)
{
	GProgressPane *Pane = Progri.First();
	if (Pane) Pane->Progress::SetScale(s);
}

const char *GProgressDlg::GetType()
{
	GProgressPane *Pane = Progri.First();
	return (Pane) ? Pane->GetType() : 0;
}

void GProgressDlg::SetType(const char *t)
{
	GProgressPane *Pane = Progri.First();
	if (Pane) Pane->SetType(t);
}

bool GProgressDlg::Cancel()
{
	for (GProgressPane *p=Progri.First(); p; p=Progri.Next())
	{
		if (p->Cancel()) return true;
	}
	return false;
}

void GProgressDlg::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

// Sync this window with the contents of a list.
void GProgressDlg::OnSync(ProgressList *Prg)
{
	if (Prg AND Prg->Lock())
	{
		GProgressPane *Pane = Progri[1];

		// Loop through all the up to date panes
		for (Progress *p = Prg->First(); p; p = Prg->Next())
		{
			bool NewPane = false;
			if (Pane)
			{
				if (Pane->Ref != p)
				{
					Pop(Pane);
					NewPane = true;
				}
				else
				{
					// pane already exist for this progress
					// so just update the value
					if (Pane->Value() != p->Value())
					{
						Pane->Value(p->Value());
					}

					if (Pane->Cancel() != p->Cancel())
					{
						p->Cancel(Pane->Cancel());
					}
				}
			}
			else
			{
				NewPane = true;
			}

			if (NewPane)
			{
				// new pane
				Pane = Push();
				if (Pane)
				{
					Pane->Ref = p;
					Pane->SetDescription(p->GetDescription());

					int64 h, l;
					p->GetLimits(&h, &l);
					Pane->SetLimits(h, l);

					Pane->Progress::SetScale(p->GetScale());
					Pane->SetType(p->GetType());
					Pane->Value(p->Value());

				}
			}

			Pane = Progri.Next();
		}

		// too many panes, delete some
		while (Progri.Length() > Prg->Length() + 1)
		{
			Pop();
		}

		Prg->Unlock();
	}
}


