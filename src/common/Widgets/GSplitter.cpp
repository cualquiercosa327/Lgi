#include "Lgi.h"

//////////////////////////////////////////////////////////////////////////////
#define SPLITER_BORDER			4
#define SPLITER_BAR_DEFAULT		6
#define SPLITER_INSET			2
#define SPLITER_MIN_X			(2*SPLITER_INSET)
#define SPLITER_MIN_Y			(2*SPLITER_INSET)

#define SPLITER_FULL_DRAG

class GSplitterPrivate
{
public:
	bool Vertical;
	bool Moving;
	int SplitPos;
	int SplitSet;
	bool OverBar;
	GRect	Bar;
	int Offset;
	bool SplitFollow;
	GRect	PosA;
	GRect	PosB;
	GView *ViewA;
	GView *ViewB;
	bool BorderA;
	bool BorderB;
	int BarSize;

	#ifdef WIN32
	HCURSOR OldCursor;
	HCURSOR Cursor;
	#endif
};

GSplitter::GSplitter()
{
	d = new GSplitterPrivate;
	d->PosA.ZOff(0, 0);
	d->PosB.ZOff(0, 0);
	d->ViewA = 0;
	d->ViewB = 0;
	d->BorderA = false;
	d->BorderB = false;
	d->SplitFollow = false;
	d->BarSize = SPLITER_BAR_DEFAULT;
	d->Vertical = true;
	d->SplitPos = 100;

	GRect r(0, 0, 2000, 1000);
	SetPos(r);
	Name("LGI_Spliter");
	d->SplitSet = d->SplitPos = -1;
	d->Moving = false;

	Border(true);
	Raised(true);

	SetPourLargest(true);
	_BorderSize = 1;

	#if WIN32NATIVE
	d->OverBar = false;
	d->Cursor = 0;
	d->OldCursor = 0;
	SetStyle(GetStyle() | WS_CLIPCHILDREN);
	#elif defined BEOS
	Handle()->SetViewColor(B_TRANSPARENT_COLOR);
	#endif

	IsVertical(true);
	Value(0);
}

GSplitter::~GSplitter()
{
	DeleteObj(d->ViewA);
	DeleteObj(d->ViewB);
	DeleteObj(d);
}

int GSplitter::BarSize() { return d->BarSize; }
void GSplitter::BarSize(int i) { d->BarSize = i; CalcRegions(); }
bool GSplitter::Border() { return TestFlag(WndFlags, GWF_BORDER); }
void GSplitter::Border(bool i) { (i) ? SetFlag(WndFlags, GWF_BORDER) : ClearFlag(WndFlags, GWF_BORDER); }
bool GSplitter::IsVertical() { return d->Vertical; }
bool GSplitter::DoesSplitFollow() { return d->SplitFollow; }
void GSplitter::DoesSplitFollow(bool i) { d->SplitFollow = i; }
GView *GSplitter::GetViewA() { return d->ViewA; }
GView *GSplitter::GetViewB() { return d->ViewB; }

void GSplitter::IsVertical(bool v)
{
	d->Vertical = v;
	#ifdef WIN32
	if (d->Cursor)
	{
		DeleteObject(d->Cursor);
		d->Cursor = 0;
	}
	#endif
}

void GSplitter::SetViewA(GView *a, bool Border)
{
	if (a != d->ViewA)
	{
		DeleteObj(d->ViewA);

		d->BorderA = Border;
		d->ViewA = a;
		if (a && IsAttached())
		{
			a->SetPos(d->PosA);
			a->Attach(this);
			CalcRegions();
			a->Visible(true);

			if (Visible())
			{
				Invalidate();
			}
		}
	}
}

void GSplitter::DetachViewA()
{
	if (d->ViewA)
	{
		if (d->ViewA->IsAttached())
		{
			d->ViewA->Detach();
		}
		d->ViewA = 0;
		
		CalcRegions();
		if (Visible())
		{
			Invalidate();
		}
	}
}

void GSplitter::SetViewB(GView *b, bool Border)
{
	if (b != d->ViewB)
	{
		DeleteObj(d->ViewB);

		d->BorderB = Border;
		d->ViewB = b;
		if (b && IsAttached())
		{
			b->SetPos(d->PosB);
			b->Attach(this);
			CalcRegions();
			b->Visible(true);

			if (Visible())
			{
				Invalidate();
			}
		}
	}
}

void GSplitter::DetachViewB()
{
	if (d->ViewB)
	{
		if (d->ViewB->IsAttached())
		{
			d->ViewB->Detach();
		}
		d->ViewB = 0;
		
		CalcRegions();
		if (Visible())
		{
			Invalidate();
		}
	}
}

int64 GSplitter::Value()
{
	return d->SplitPos;
}

void GSplitter::Value(int64 s)
{
	d->SplitSet = s;
	int Limit = ((d->Vertical) ? X() : Y()) - 18;
	int NewPos = (s > 0) ? max(s, 4) : max(4, Limit+s);
	if (NewPos != d->SplitPos)
	{
		d->SplitPos = NewPos;
		CalcRegions();
		if (Visible())
		{
			Invalidate();
		}
	}
}

GViewI *GSplitter::FindControl(OsView hCtrl)
{
	GViewI *c = 0;
	if (d->ViewA) c = d->ViewA->FindControl(hCtrl);
	if (c) return c;

	if (d->ViewB) c = d->ViewB->FindControl(hCtrl);
	return c;
}

void GSplitter::CalcRegions(bool Follow)
{
	GRect Rect = GetClient();
	
	d->PosA = Rect;
	if (Border())
	{
		d->PosA.Size(SPLITER_BORDER, SPLITER_BORDER);
	}

	d->PosB = d->PosA;

	if (d->SplitFollow && Follow)
	{
		d->SplitPos = max(1, ((d->Vertical) ? X()-10 : Y()-10) + d->SplitSet);
	}
	else if (Rect.Valid())
	{
		int Max = d->Vertical ? X()-10 : Y()-10;
		if (Max > 0)
		{
			d->SplitPos = max(1, d->SplitPos);
			d->SplitPos = min(Max, d->SplitPos);
		}
	}

	if (d->Vertical)
	{
		d->PosA.x2 = d->PosA.x1 + d->SplitPos;
		d->PosB.x1 = d->PosA.x2 + d->BarSize;
	}
	else
	{
		d->PosA.y2 = d->PosA.y1 + d->SplitPos;
		d->PosB.y1 = d->PosA.y2 + d->BarSize;
	}

	GRect r = d->PosA;
	if (d->BorderA)
	{
		r.Size(SPLITER_INSET, SPLITER_INSET);
	}

	if (d->ViewA)
	{
		if (d->ViewA->IsAttached())
		{
			if (r.Valid())
			{
				GRegion Rgn(r);
				d->ViewA->SetPos(r);
				d->ViewA->Pour(Rgn);
			}
			d->ViewA->Visible(r.Valid());
		}
		else
		{
			d->ViewA->SetPos(r);
		}
	}

	r = d->PosB;
	if (d->BorderB)
	{
		r.Size(SPLITER_INSET, SPLITER_INSET);
	}

	if (d->ViewB)
	{
		if (d->ViewB->IsAttached())
		{
			if (r.Valid())
			{
				GRegion Rgn(r);
				d->ViewB->SetPos(r);
				d->ViewB->Pour(Rgn);
			}
			d->ViewB->Visible(r.Valid());
		}
		else
		{
			d->ViewB->SetPos(r);
		}
	}

	#if 0
	if (d->ViewA && !stricmp(d->ViewA->GetClass(), "GTree"))
	{
		printf("Split::OnPosChange cli=%s a=%s b=%s\n",
			GetClient().GetStr(),
			d->PosA.GetStr(),
			d->PosB.GetStr());
	}
	#endif
	
	// printf("CalcPos Client=%s Value=%i A=%s B=%s\n", GetClient().GetStr(), (int)Value(), d->PosA.GetStr(), d->PosB.GetStr());
}

bool GSplitter::Pour(GRegion &r)
{
	bool s = GLayout::Pour(r);
	CalcRegions(true);
	return s;
}

void GSplitter::OnPosChange()
{
	CalcRegions(true);
}

#ifdef WIN32
void ClipDC(HDC hDC, RECT rc)
{
	HRGN hRgn = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
	SelectClipRgn(hDC, hRgn);
}
#endif

void GSplitter::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();

	pDC->Colour(LC_MED, 24);
	if (Border())
	{
		LgiFlatBorder(pDC, r, SPLITER_BORDER);
	}

	// bar
	pDC->Colour(LC_MED, 24);
	if (d->Vertical)
	{
		/* Old Win32
		pDC->Rectangle(PosA.x2, PosA.y1, PosB.x1, PosA.y2);
		pDC->Rectangle(PosA.x1, PosA.y2, PosA.x2, PosB.y1);
		*/
		pDC->Rectangle(d->PosA.x2+1, r.y1, d->PosB.x1-1, r.y2);
	}
	else
	{
		pDC->Rectangle(r.x1, d->PosA.y2+1, r.x2, d->PosB.y1-1);
	}

	// PosA region
	r = d->PosA;
	if (r.X() > SPLITER_MIN_X && r.Y() > SPLITER_MIN_Y)
	{
		if (d->BorderA)
		{
			LgiWideBorder(pDC, r, SUNKEN);
		}

		if (d->ViewA)
		{
			#ifdef WIN32
			if (!d->ViewA->Handle())
			{
				pDC->SetClient(&d->PosA);
				d->ViewA->OnPaint(pDC);
				pDC->SetClient(NULL);
			}
			else
			#endif
			{
				// View is real and will paint itself
			}
		}
		else
		{
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(&r);
		}
	}
	else
	{
		// too small
	}

	r = d->PosB;
	if (r.X() > SPLITER_MIN_X && r.Y() > SPLITER_MIN_Y)
	{
		if (d->BorderB)
		{
			LgiWideBorder(pDC, r, SUNKEN);
		}

		if (d->ViewB)
		{
			#ifdef WIN32
			if (!d->ViewB->Handle())
			{
				pDC->SetClient(&d->PosB);
				d->ViewB->OnPaint(pDC);
				pDC->SetClient(NULL);
			}
			else
			#endif
			{
				// View is real and will paint itself
			}
		}
		else
		{
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(&r);
		}
	}
	else
	{
		// too small
	}
}

void GSplitter::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (OverSplit(m.x, m.y))
		{
			Capture(true);

			if (d->Vertical)
			{
				d->Bar.x1 = d->PosA.x2;
				d->Bar.y1 = d->PosA.y1;
				d->Bar.x2 = d->PosB.x1;
				d->Bar.y2 = d->PosA.y2;
				d->Offset = m.x - d->Bar.x1;
			}
			else
			{
				d->Bar.x1 = d->PosA.x1;
				d->Bar.y1 = d->PosA.y2;
				d->Bar.x2 = d->PosA.x2;
				d->Bar.y2 = d->PosB.y1;
				d->Offset = m.y - d->Bar.y1;
			}
		}
	}
	else if (IsCapturing())
	{
		Capture(false);
	}
}

bool GSplitter::OverSplit(int x, int y)
{
	if (d->Vertical)
	{
		return (x > d->PosA.x2 && x < d->PosB.x1 && y > d->PosA.y1 && y < d->PosA.y2);
	}
	else
	{
		return (x > d->PosA.x1 && x < d->PosA.x2 && y > d->PosA.y2 && y < d->PosB.y1);
	}
}

void GSplitter::OnMouseExit(GMouse &m)
{
}

void GSplitter::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		if (d->Vertical)
		{
			d->Bar.Offset((m.x - d->Offset) - d->Bar.x1, 0);
		}
		else
		{
			d->Bar.Offset(0, (m.y - d->Offset) - d->Bar.y1);
		}

		int NewPos;
		if (d->Vertical)
		{
			NewPos = limit(d->Bar.x1-4, 4, X()-18);
		}
		else
		{
			NewPos = limit(d->Bar.y1, 4, Y()-18);
		}

		if (NewPos != d->SplitPos)
		{
			d->SplitPos = NewPos;
			CalcRegions();
			Invalidate((GRect*)0, true);
		}
	}
}

LgiCursor GSplitter::GetCursor(int x, int y)
{
	if (OverSplit(x, y))
		return (d->Vertical) ? LCUR_SizeHor : LCUR_SizeVer;
	
	return LCUR_Normal;
}

int GSplitter::OnHitTest(int x, int y)
{
	return -1;
}

bool GSplitter::Attach(GViewI *p)
{
	bool Status = GLayout::Attach(p);
	if (Status)
	{
		if (d->ViewA && !d->ViewA->IsAttached())
		{
			d->ViewA->SetPos(d->PosA);
			d->ViewA->Attach(this);
		}

		if (d->ViewB && !d->ViewB->IsAttached())
		{
			d->ViewB->SetPos(d->PosB);
			d->ViewB->Attach(this);
		}
	}

	return Status;
}

void GSplitter::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (!Attaching)
	{
		if (Wnd == d->ViewA)
		{
			d->ViewA = 0;
		}
		if (Wnd == d->ViewB)
		{
			d->ViewB = 0;
		}
	}
}
