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
#include "GTableLayout.h"

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
Progress::Progress() : GMutex("ProgressObj")
{
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
	Description.Reset(NewStr(desc));
	Start = 0;
	Val = Low = l;
	High = h;
	Type = type;
	Scale = scale;
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
    if (d != Description)
    	Description.Reset(NewStr(d));
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
#define IDC_TABLE				105

#if 1
#define PANE_X					300
#define PANE_Y					100
#else
#define PANE_X					260
#define PANE_Y					85
#endif

GProgressPane::GProgressPane()
{
	GRect r(0, 0, PANE_X-1, PANE_Y-1);
	SetPos(r);
	Name("Progress");
	Canceled = false;
	Wait = false;
	Ref = 0;

	if (AddView(t = new GTableLayout(IDC_TABLE)))
	{
		OnPosChange();
		
		int Row = 0;
		GLayoutCell *c = t->GetCell(0, Row++, true, 2, 1);
		c->Add(Desc = new GText(IDC_DESCRIPTION, 0, 0, -1, -1, "##"));

		c = t->GetCell(0, Row);
		c->Add(ValText = new GText(IDC_VALUE, 0, 0, -1, -1, "##"));
		c = t->GetCell(1, Row++);
		c->Add(Rate = new GText(IDC_RATE, 0, 0, -1, -1, "##"));

		c = t->GetCell(0, Row++, true, 2, 1);
		c->Add(Bar = new GProgress(IDC_PROGRESS, 0, 0, PANE_X - 14, 10, "Progress"));

		c = t->GetCell(0, Row++, true, 2, 1);
		c->TextAlign(GCss::Len(GCss::AlignCenter));
		c->Add(But = new GButton(IDC_BUTTON, 0, 0, -1, -1, "Request Abort"));
	}
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
	bool Update = false;

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
		
		sprintf_s(Str, sizeof(Str), "@ %.2f %s / sec", PerSec * Scale, (Type) ? Type : "");
		Update |= Rate->Name(Str);
	}

	if (ValText)
	{
		if (Scale == 1.0)
		{
			sprintf_s(Str, sizeof(Str), "%.0f of %.0f %s",
				Val * Scale,
				(High - Low) * Scale,
				(Type) ? Type : "");
		}
		else
		{
			sprintf_s(Str, sizeof(Str), "%.1f of %.1f %s",
				(double)Val * Scale,
				(double)(High - Low) * Scale,
				(Type) ? Type : "");
		}
		
		Update |= ValText->Name(Str);
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
	
	if (Update && ValText)
		ValText->SendNotify(GTABLELAYOUT_REFRESH);
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

			if (GetParent() &&
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
	GRect r = GetClient();
	LgiThinBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);
}

void GProgressPane::OnPosChange()
{
	if (t)
	{
		GRect cr = GetClient();
		cr.Size(4, 4);
		
		printf("tpos=%s\n", cr.GetStr());
		t->SetPos(cr);
	}
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
	Resize();
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

bool GProgressDlg::OnRequestClose(bool OsClose)
{
	if (Wait)
	{
		for (int i=0; i<Progri.Length(); i++)
		{
			GProgressPane *pp = Progri[i];
			if (pp)
			{
				pp->Cancel(true);
			}
		}
		
		return false;
	}
	
	return GDialog::OnRequestClose(OsClose);
}

void GProgressDlg::Resize()
{
	GRect r, c = GetPos();
	int DecorX = LgiApp->GetMetric(LGI_MET_DECOR_X);
	int DecorY = LgiApp->GetMetric(LGI_MET_DECOR_Y);

	int Items = max(1, Progri.Length());
	int Width = DecorX + PANE_X;
	int Height = DecorY + (PANE_Y * Items);

	r.ZOff(Width - 1, Height - 1);
	r.Offset(c.x1, c.y1);
	SetPos(r);

	// Layout all the panes...
	int y = 0;
	for (int i=0; i<Progri.Length(); i++)
	{
		GProgressPane *p = Progri[i];
		if (p)
		{
			GRect r(0, y, PANE_X - 1, y + PANE_Y - 1);
			p->SetPos(r);
			p->Visible(true);
			y = r.y2 + 1;
		}
	}
}

void GProgressDlg::OnCreate()
{
	if (Progri.Length() == 0)
		Push();
}

void GProgressDlg::OnPosChange()
{
	GRect c = GetClient();
	
	// Layout all the panes...
	int y = 0;
	for (int i=0; i<Progri.Length(); i++)
	{
		GProgressPane *p = Progri[i];
		if (p)
		{
			GRect r = p->GetPos();
			r.Offset(0, y-r.y1);
			r.x2 = c.x2;
			
			p->SetPos(r);
			p->Visible(true);
			
			y = r.y2 + 1;
		}
	}
}

GMessage::Result GProgressDlg::OnEvent(GMessage *Msg)
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
		Resize();
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
		Resize();
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
	if (Prg && Prg->Lock())
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


