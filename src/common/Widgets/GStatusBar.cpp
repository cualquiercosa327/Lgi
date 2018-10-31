#include "Lgi.h"
#include "GDisplayString.h"
#include "LgiRes.h"

//////////////////////////////////////////////////////////////////////////////////////
GStatusBar::GStatusBar()
{
	Name("LGI_StatusBar");
	Raised(true);
	_BorderSize = 1;
	LgiResources::StyleElement(this);
}

GStatusBar::~GStatusBar()
{
}

bool GStatusBar::Pour(GRegion &r)
{
	GRect *Best = FindLargestEdge(r, GV_EDGE_BOTTOM);
	if (Best)
	{
		GRect Take = *Best;
		if (Take.Y() > 21)
		{
			Take.y1 = MAX((Take.y2 - 21), Take.y1);
			SetPos(Take);
			return true;
		}
	}
	return false;
}

void GStatusBar::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

void GStatusBar::OnPosChange()
{
	int x = X() - 5;
	auto i = Children.Length() - 1;
	auto c = GetClient();
	c.y2 -= 6;

	for (	GViewI *w = Children.Last();
			w;
			w = Children.Prev(), i--)
	{
		GStatusPane *Pane = dynamic_cast<GStatusPane*>(w);
		if (Pane)
		{
			#ifndef WIN32
			if (!Pane->IsAttached())
				Pane->Attach(this);
			#endif
			
			if (i)
			{
				// not first
				x -= Pane->Width; 

				GRect r;
				r.ZOff(Pane->Width, c.Y()-1);
				r.Offset(x, 2);
				Pane->SetPos(r);
				x -= STATUSBAR_SEPARATOR;
			}
			else
			{
				// first
				GRect r;
				r.ZOff(x-2, c.Y()-1);
				r.Offset(2, 2);
				Pane->SetPos(r);
				x = 0;
			}
		}
	}
}

GStatusPane *GStatusBar::AppendPane(const char *Text, int Width)
{
	GStatusPane *Pane = 0;
	if (Text)
	{
		Pane = new GStatusPane;
		if (Pane)
		{
			AddView(Pane);
			Pane->Name(Text);
			Pane->Width = Width;
		}
	}
	return Pane;
}

bool GStatusBar::AppendPane(GStatusPane *Pane)
{
	if (!Pane)
		return false;

	return AddView(Pane);
}

// Pane
GStatusPane::GStatusPane()
{
	SetParent(0);
	Flags = 0;
	Width = 32;
	GRect r(0, 0, Width-1, 20);
	SetPos(r);
	pDC = 0;
	_BorderSize = 1;
}

GStatusPane::~GStatusPane()
{
	DeleteObj(pDC)
}

bool GStatusPane::Name(const char *n)
{
	bool Status = false;

	if (Lock(_FL))
	{
		char *l = Name();
		if (!n ||
			!l ||
			strcmp(l, n) != 0)
		{
			Status = GBase::Name(n);
			GRect p(0, 0, X()-1, Y()-1);
			p.Size(1, 1);
			Invalidate(&p);
		}
		Unlock();
	}

	return Status;
}

void GStatusPane::OnPaint(GSurface *pDC)
{
	GAutoString t;
	if (Lock(_FL))
	{
		t.Reset(NewStr(Name()));
		Unlock();
	}

	GRect r = GetClient();
	if (ValidStr(t))
	{
		int TabSize = SysFont->TabSize();

		SysFont->TabSize(0);
		SysFont->Colour(LC_TEXT, LC_MED);
		SysFont->Transparent(false);

		GDisplayString ds(SysFont, t);
		ds.Draw(pDC, 1, (r.Y()-ds.Y())/2, &r);
		
		SysFont->TabSize(TabSize);
	}
	else
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
	}
}

int GStatusPane::GetWidth()
{
	return Width;
}

void GStatusPane::SetWidth(int x)
{
	Width = x;
	if (GetParent())
	{
		GStatusBar *Bar = dynamic_cast<GStatusBar*>(GetParent());
		if (Bar)
			Bar->OnPosChange();
	}
}

bool GStatusPane::Sunken()
{
	return TestFlag(Flags, GSP_SUNKEN);
}

void GStatusPane::Sunken(bool s)
{
	bool Old = Sunken();
	if (s) SetFlag(Flags, GSP_SUNKEN);
	else ClearFlag(Flags, GSP_SUNKEN);
	if (Old != Sunken() && GetParent())
	{
		GetParent()->Invalidate();
	}
}

GSurface *GStatusPane::Bitmap()
{
	return pDC;
}

void GStatusPane::Bitmap(GSurface *pNewDC)
{
	DeleteObj(pDC);
	if (pNewDC)
	{
		pDC = new GMemDC;
		if (pDC)
		{
			if (pDC->Create(pNewDC->X(), pNewDC->Y(), pNewDC->GetColourSpace()))
			{
				pDC->Blt(0, 0, pNewDC);
			}
			else
			{
				DeleteObj(pDC);
			}
		}
	}

	if (GetParent())
	{
		GetParent()->Invalidate();
	}
}

