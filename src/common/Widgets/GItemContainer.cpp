#include "Lgi.h"
#include "GItemContainer.h"
#include "GDisplayString.h"
#include "GSkinEngine.h"
#include "GScrollBar.h"

// Colours
#if defined(WIN32)
#if !defined(WS_EX_LAYERED)
#define WS_EX_LAYERED					0x80000
#endif
#if !defined(LWA_ALPHA)
#define LWA_ALPHA						2
#endif
typedef BOOL (__stdcall *_SetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
#endif


class GItemColumnPrivate
{
public:
	GRect Pos;
	bool Down;
	bool Drag;

	GItemContainer *Parent;
	char *cName;
	GDisplayString *Txt;
	int cWidth;
	int cType;
	GSurface *cIcon;
	int cImage;
	int cMark;
	bool OwnIcon;
	bool CanResize;

	GItemColumnPrivate(GItemContainer *parent)
	{
		Parent = parent;
		Txt = 0;
		cName = 0;
		cWidth = 0;
		cIcon = 0;
		cType = GIC_ASK_TEXT;
		cImage = -1;
		cMark = GLI_MARK_NONE;
		Down = false;
		OwnIcon = false;
		CanResize = true;
		Drag = false;
	}

	~GItemColumnPrivate()
	{
		DeleteArray(cName);
		if (OwnIcon)
		{
			DeleteObj(cIcon);
		}
		DeleteObj(Txt);
	}
};

//////////////////////////////////////////////////////////////////////////////////////
GItemContainer::GItemContainer()
{
	Flags = 0;
	DragMode = 0;
	DragCol = NULL;
	ColClick = -1;
	ColumnHeaders = true;
	ColumnHeader.ZOff(-1, -1);
	Columns.SetFixedLength(true);
}

GItemContainer::~GItemContainer()
{
	DeleteObj(DragCol);
}

void GItemContainer::PaintColumnHeadings(GSurface *pDC)
{
	// Draw column headings
	if (ColumnHeaders && ColumnHeader.Valid())
	{
		GSurface *ColDC = pDC;
		GRect cr = ColumnHeader;
		int cx = cr.x1;

		#if DOUBLE_BUFFER_COLUMN_DRAWING
		GMemDC Bmp;
		if (!pDC->SupportsAlphaCompositing())
		{
			if (Bmp.Create(ColumnHeader.X(), ColumnHeader.Y(), 32))
			{
				ColDC = &Bmp;
				Bmp.Op(GDC_ALPHA);
			}
			else
			{
				ColDC = pDC;
			}

			printf("GList::OnPaint ***START*** %s %i\n",
				GColourSpaceToString(ColDC->GetColourSpace()), ColDC->SupportsAlphaCompositing());
		}
		else
		#else
		{
			ColDC = pDC;
			pDC->ClipRgn(&cr);
		}
		#endif
		
		// Draw columns
		if (IconCol)
		{
			cr.x1 = cx;
			cr.x2 = cr.x1 + IconCol->Width() - 1;
			IconCol->SetPos(cr);
			IconCol->OnPaint(ColDC, cr);
			cx += IconCol->Width();
		}

		// Draw other columns
		for (int i=0; i<Columns.Length(); i++)
		{
			GItemColumn *c = Columns[i];
			if (c)
			{
				cr.x1 = cx;
				cr.x2 = cr.x1 + c->Width() - 1;
				c->SetPos(cr);
				c->OnPaint(ColDC, cr);
				cx += c->Width();
			}
			else LgiAssert(0);
		}

		// Draw ending peice
		cr.x1 = cx;
		cr.x2 = ColumnHeader.x2 + 2;

		if (cr.Valid())
		{
			#ifdef MAC
			GArray<GColourStop> Stops;
			GRect j(cr.x1, cr.y1, cr.x2-1, cr.y2-1); 
			// ColDC->Colour(Rgb24(160, 160, 160), 24);
			// ColDC->Line(r.x1, r.y1, r.x2, r.y1);
				
			Stops[0].Pos = 0.0;
			Stops[0].Colour = Rgb32(255, 255, 255);
			Stops[1].Pos = 0.5;
			Stops[1].Colour = Rgb32(241, 241, 241);
			Stops[2].Pos = 0.51;
			Stops[2].Colour = Rgb32(233, 233, 233);
			Stops[3].Pos = 1.0;
			Stops[3].Colour = Rgb32(255, 255, 255);
			
			LgiFillGradient(ColDC, j, true, Stops);
			
			ColDC->Colour(Rgb24(178, 178, 178), 24);
			ColDC->Line(cr.x1, cr.y2, cr.x2, cr.y2);
			#else
			if (GApp::SkinEngine)
			{
				GSkinState State;
				State.pScreen = ColDC;
				State.Rect = cr;
				State.Enabled = Enabled();
				GApp::SkinEngine->OnPaint_ListColumn(0, 0, &State);
			}
			else
			{
				LgiWideBorder(ColDC, cr, DefaultRaisedEdge);
				ColDC->Colour(LC_MED, 24);
				ColDC->Rectangle(&cr);
			}
			#endif
		}

		#if DOUBLE_BUFFER_COLUMN_DRAWING
		printf("GList::OnPaint ***END*** %s %i\n",
			GColourSpaceToString(ColDC->GetColourSpace()), ColDC->SupportsAlphaCompositing());

		if (!pDC->SupportsAlphaCompositing())
		{
			pDC->Blt(ColumnHeader.x1, ColumnHeader.y1, &Bmp);
		}
		else
		#endif
		{
			pDC->ClipRgn(0);
		}
	}
}

GItemColumn *GItemContainer::AddColumn(const char *Name, int Width, int Where)
{
	GItemColumn *c = 0;

	if (Lock(_FL))
	{
		c = new GItemColumn(this, Name, Width);
		if (c)
		{
			Columns.SetFixedLength(false);
			Columns.AddAt(Where, c);
			Columns.SetFixedLength(true);

			UpdateAllItems();
			SendNotify(GNotifyItem_ColumnsChanged);
		}
		Unlock();
	}

	return c;
}

bool GItemContainer::AddColumn(GItemColumn *Col, int Where)
{
	bool Status = false;

	if (Col && Lock(_FL))
	{
		Columns.SetFixedLength(false);
		Status = Columns.AddAt(Where, Col);
		Columns.SetFixedLength(true);

		if (Status)
		{
			UpdateAllItems();
			SendNotify(GNotifyItem_ColumnsChanged);
		}

		Unlock();
	}

	return Status;
}

void GItemContainer::DragColumn(int Index)
{
	DeleteObj(DragCol);

	if (Index >= 0)
	{
		DragCol = new GDragColumn(this, Index);
		if (DragCol)
		{
			Capture(true);
			DragMode = DRAG_COLUMN;
		}
	}
}

int GItemContainer::ColumnAtX(int x, GItemColumn **Col, int *Offset)
{
	GItemColumn *Column = 0;
	if (!Col) Col = &Column;

	int Cx = GetImageList() ? 16 : 0;
	int c;
	for (c=0; c<Columns.Length(); c++)
	{
		*Col = Columns[c];
		if (x >= Cx && x < Cx + (*Col)->Width())
		{
			if (Offset)
				*Offset = Cx;
			return c;
		}
		Cx += (*Col)->Width();
	}

	return -1;
}

void GItemContainer::EmptyColumns()
{
	Columns.DeleteObjects();
	Invalidate(&ColumnHeader);
	SendNotify(GNotifyItem_ColumnsChanged);
}

int GItemContainer::HitColumn(int x, int y, GItemColumn *&Resize, GItemColumn *&Over)
{
	int Index = -1;

	Resize = 0;
	Over = 0;

	if (ColumnHeaders &&
		ColumnHeader.Overlap(x, y))
	{
		// Clicked on a column heading
		int cx = ColumnHeader.x1 + ((IconCol) ? IconCol->Width() : 0);
		
		for (int n = 0; n < Columns.Length(); n++)
		{
			GItemColumn *c = Columns[n];
			cx += c->Width();
			if (abs(x-cx) < 5)
			{
				if (c->Resizable())
				{
					Resize = c;
					Index = n;
					break;
				}
			}
			else if (c->d->Pos.Overlap(x, y))
			{
				Over = c;
				Index = n;
				break;
			}
		}
	}

	return Index;
}

#ifdef BEOS
int ColInfoCmp(GItemContainer::ColInfo *a, GItemContainer::ColInfo *b)
#else
DeclGArrayCompare(ColInfoCmp, GItemContainer::ColInfo, void)
#endif
{
	int AGrowPx = a->GrowPx();
	int BGrowPx = b->GrowPx();
	return AGrowPx - BGrowPx;
}

void GItemContainer::GetColumnSizes(ColSizes &cs)
{
	// Read in the current sizes
	cs.FixedPx = 0;
	cs.ResizePx = 0;
	for (int i=0; i<Columns.Length(); i++)
	{
		GItemColumn *c = Columns[i];
		if (c->Resizable())
		{
			ColInfo &Inf = cs.Info.New();
			Inf.Col = c;
			Inf.Idx = i;
			Inf.ContentPx = c->GetContentSize();
			Inf.WidthPx = c->Width();

			cs.ResizePx += Inf.WidthPx;
		}
		else
		{
			cs.FixedPx += c->Width();
		}
	}
}

void GItemContainer::ResizeColumnsToContent(int Border)
{
	if (Lock(_FL))
	{
		// Read in the current sizes
		ColSizes Sizes;
		GetColumnSizes(Sizes);
		
		// Allocate space
		int AvailablePx = GetClient().X() - 5;
		if (VScroll)
			AvailablePx -= VScroll->X();

		int ExpandPx = AvailablePx - (Sizes.FixedPx + Sizes.ResizePx);
		if (ExpandPx > 0)
		{
			#ifdef BEOS
			Sizes.Info.Sort(ColInfoCmp);
			#else
			Sizes.Info.Sort(ColInfoCmp, (void*)NULL);
			#endif
			for (int i=0; i<Sizes.Info.Length(); i++)
			{
				ColInfo &Inf = Sizes.Info[i];
				if (Inf.Col)
				{
					int AddPx = min(Inf.GrowPx() + Border, ExpandPx);
					if (AddPx > 0)
					{
						Inf.Col->Width(Inf.WidthPx + AddPx);
						ClearDs(Inf.Idx);
						ExpandPx -= AddPx;
					}
				}
			}
		}

		Unlock();
	}

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////////
GDragColumn::GDragColumn(GItemContainer *list, int col)
{
	List = list;
	Index = col;
	Offset = 0;
	#ifdef LINUX
	Back = 0;
	#endif
	Col = List->ColumnAt(Index);
	if (Col)
	{
		Col->d->Down = false;
		Col->d->Drag = true;

		GRect r = Col->d->Pos;
		r.y1 = 0;
		r.y2 = List->Y()-1;
		List->Invalidate(&r, true);

		#if WINNATIVE
		
		GArray<int> Ver;
		bool Layered = (
							LgiGetOs(&Ver) == LGI_OS_WIN32 ||
							LgiGetOs(&Ver) == LGI_OS_WIN64
						) &&
						Ver[0] >= 5;
		SetStyle(WS_POPUP);
		SetExStyle(GetExStyle() | WS_EX_TOOLWINDOW);
		if (Layered)
		{
			SetExStyle(GetExStyle() | WS_EX_LAYERED | WS_EX_TRANSPARENT);
		}
		
		#elif defined XWIN
		
		Back = new GMemDC;
		if (Back && Back->Create(List->X(), List->Y(), GdcD->GetBits()))
		{
			List->OnPaint(Back);
		}

		//XSetWindowAttributes a;
		//a.override_redirect = true;
		//a.save_under = false; // true;
		// XChangeWindowAttributes(Handle()->XDisplay(), Handle()->handle(), CWSaveUnder | CWOverrideRedirect, &a);
		
		#endif

		Attach(0);

		#if WINNATIVE
		
		if (Layered)
		{
			SetWindowLong(Handle(), GWL_EXSTYLE, GetWindowLong(Handle(), GWL_EXSTYLE) | WS_EX_LAYERED);

			GLibrary User32("User32");
			_SetLayeredWindowAttributes SetLayeredWindowAttributes = (_SetLayeredWindowAttributes)User32.GetAddress("SetLayeredWindowAttributes");
			if (SetLayeredWindowAttributes)
			{
				if (!SetLayeredWindowAttributes(Handle(), 0, DRAG_COL_ALPHA, LWA_ALPHA))
				{
					DWORD Err = GetLastError();
				}
			}
		}
		
		#endif

		GMouse m;
		List->GetMouse(m);
		Offset = m.x - r.x1;

		List->PointToScreen(ListScrPos);
		r.Offset(ListScrPos.x, ListScrPos.y);

		SetPos(r);
		Visible(true);
	}
}

GDragColumn::~GDragColumn()
{
	Visible(false);

	if (Col)
	{
		Col->d->Drag = false;
	}

	List->Invalidate();
}

#if LINUX_TRANS_COL
void GDragColumn::OnPosChange()
{
	Invalidate();
}
#endif

void GDragColumn::OnPaint(GSurface *pScreen)
{
	#if LINUX_TRANS_COL
	GSurface *Buf = new GMemDC(X(), Y(), GdcD->GetBits());
	GSurface *pDC = new GMemDC(X(), Y(), GdcD->GetBits());
	#else
	GSurface *pDC = pScreen;
	#endif
	
	pDC->SetOrigin(Col->d->Pos.x1, 0);
	if (Col) Col->d->Drag = false;
	List->OnPaint(pDC);
	if (Col) Col->d->Drag = true;
	pDC->SetOrigin(0, 0);
	
	#if LINUX_TRANS_COL
	if (Buf && pDC)
	{
		GRect p = GetPos();
		
		// Fill the buffer with the background
		Buf->Blt(ListScrPos.x - p.x1, 0, Back);

		// Draw painted column over the back with alpha
		Buf->Op(GDC_ALPHA);
		GApplicator *App = Buf->Applicator();
		if (App)
		{
			App->SetVar(GAPP_ALPHA_A, DRAG_COL_ALPHA);
		}
		Buf->Blt(0, 0, pDC);

		// Put result on the screen
		pScreen->Blt(0, 0, Buf);
	}
	DeleteObj(Buf);
	DeleteObj(pDC);
	#endif
}

///////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// List column
GItemColumn::GItemColumn(GItemContainer *parent, const char *name, int width)
	: ResObject(Res_Column)
{
	d = new GItemColumnPrivate(parent);
	d->cWidth = width;
	if (name)
		Name(name);
}

GItemColumn::~GItemColumn()
{
	if (d->Drag)
	{
		d->Parent->DragColumn(-1);
	}

	DeleteObj(d);
}

GItemContainer *GItemColumn::GetList()
{
	return d->Parent;
}

void GItemColumn::Image(int i)
{
	d->cImage = i;
}

int GItemColumn::Image()
{
	return d->cImage;
}

bool GItemColumn::Resizable()
{
	return d->CanResize;
}

void GItemColumn::Resizable(bool i)
{
	d->CanResize = i;
}

bool GItemColumn::InDrag()
{
	return d->Drag;
}

GRect GItemColumn::GetPos()
{
	return d->Pos;
}

void GItemColumn::SetPos(GRect &r)
{
	d->Pos = r;
}

void GItemColumn::Name(const char *n)
{
	DeleteArray(d->cName);
	DeleteObj(d->Txt);
	d->cName = NewStr(n);
	
	GFont *f =	d->Parent &&
				d->Parent->GetFont() &&
				d->Parent->GetFont()->Handle() ?
				d->Parent->GetFont() :
				SysFont;
	
	d->Txt = new GDisplayString(f, (char*)n);
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

char *GItemColumn::Name()
{
	return d->cName;
}

int GItemColumn::GetIndex()
{
	if (d->Parent)
	{
		return d->Parent->Columns.IndexOf(this);
	}

	return -1;
}

int GItemColumn::GetContentSize()
{
	return d->Parent->GetContentSize(GetIndex());
}

void GItemColumn::Width(int i)
{
	if (d->cWidth != i)
	{
		d->cWidth = i;
		
		// If we are attached to a list...
		if (d->Parent)
		{
			/* FIXME
			 int MyIndex = GetIndex();
			// Clear all the cached strings for this column
			for (List<GListItem>::I it=d->Parent->Items.Start(); it.In(); it++)
			{
				DeleteObj((*it)->d->Display[MyIndex]);
			}

			if (d->Parent->IsAttached())
			{
				// Update the screen from this column across
				GRect Up = d->Parent->GetClient();
				Up.x1 = d->Pos.x1;
				d->Parent->Invalidate(&Up);
			}
			*/
		}

		// Notify listener
		GViewI *n = d->Parent->GetNotify() ? d->Parent->GetNotify() : d->Parent->GetParent();
		if (n)
		{
			n->OnNotify(d->Parent, GNotifyItem_ColumnsResized);
		}
	}
}

int GItemColumn::Width()
{
	return d->cWidth;
}

void GItemColumn::Mark(int i)
{
	d->cMark = i;
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

int GItemColumn::Mark()
{
	return d->cMark;
}

void GItemColumn::Type(int i)
{
	d->cType = i;
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

int GItemColumn::Type()
{
	return d->cType;
}

void GItemColumn::Icon(GSurface *i, bool Own)
{
	if (d->OwnIcon)
	{
		DeleteObj(d->cIcon);
	}
	d->cIcon = i;
	d->OwnIcon = Own;

	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

GSurface *GItemColumn::Icon()
{
	return d->cIcon;
}

bool GItemColumn::Value()
{
	return d->Down;
}

void GItemColumn::Value(bool i)
{
	d->Down = i;
}

void GItemColumn::OnPaint_Content(GSurface *pDC, GRect &r, bool FillBackground)
{
	if (!d->Drag)
	{
		int Off = d->Down ? 1 : 0;
		int Mx = r.x1 + 8, My = r.y1 + ((r.Y() - 8) / 2);
		if (d->cIcon)
		{
			if (FillBackground)
			{
				pDC->Colour(LC_MED, 24);
				pDC->Rectangle(&r);
			}

			int x = (r.X()-d->cIcon->X()) / 2;
			
			pDC->Blt(	r.x1 + x + Off,
						r.y1 + ((r.Y()-d->cIcon->Y())/2) + Off,
						d->cIcon);

			if (d->cMark)
			{
				Mx += x + d->cIcon->X() + 4;
			}
		}
		else if (d->cImage >= 0 && d->Parent)
		{
			GColour Background(LC_MED, 24);
			if (FillBackground)
			{
				pDC->Colour(Background);
				pDC->Rectangle(&r);
			}
			
			if (d->Parent->GetImageList())
			{
				GRect *b = d->Parent->GetImageList()->GetBounds();
				int x = r.x1;
				int y = r.y1;
				if (b)
				{
					b += d->cImage;
					x = r.x1 + ((r.X()-b->X()) / 2) - b->x1;
					y = r.y1 + ((r.Y()-b->Y()) / 2) - b->y1;
				}				
				
				d->Parent->GetImageList()->Draw(pDC,
												x + Off,
												y + Off,
												d->cImage,
												Background);
			}

			if (d->cMark)
			{
				Mx += d->Parent->GetImageList()->TileX() + 4;
			}
		}
		else if (ValidStr(d->cName) && d->Txt)
		{
			GFont *f = d->Parent ? d->Parent->GetFont() : SysFont;
			f->Transparent(!FillBackground);
			f->Colour(LC_TEXT, LC_MED);
			d->Txt->Draw(pDC, r.x1 + Off + 3, r.y1 + Off, &r);

			if (d->cMark)
			{
				Mx += d->Txt->X();
			}
		}
		else
		{
			if (FillBackground)
			{
				pDC->Colour(LC_MED, 24);
				pDC->Rectangle(&r);
			}
		}

		#define ARROW_SIZE	9
		pDC->Colour(LC_TEXT, 24);
		Mx += Off;
		My += Off - 1;

		switch (d->cMark)
		{
			case GLI_MARK_UP_ARROW:
			{
				pDC->Line(Mx + 2, My, Mx + 2, My + ARROW_SIZE - 1);
				pDC->Line(Mx, My + 2, Mx + 2, My);
				pDC->Line(Mx + 2, My, Mx + 4, My + 2);
				break;
			}
			case GLI_MARK_DOWN_ARROW:
			{
				pDC->Line(Mx + 2, My, Mx + 2, My + ARROW_SIZE - 1);
				pDC->Line(	Mx,
							My + ARROW_SIZE - 3,
							Mx + 2,
							My + ARROW_SIZE - 1);
				pDC->Line(	Mx + 2,
							My + ARROW_SIZE - 1,
							Mx + 4,
							My + ARROW_SIZE - 3);
				break;
			}
		}
	}
}

void ColumnPaint(void *UserData, GSurface *pDC, GRect &r, bool FillBackground)
{
	((GItemColumn*)UserData)->OnPaint_Content(pDC, r, FillBackground);
}

void GItemColumn::OnPaint(GSurface *pDC, GRect &Rgn)
{
	GRect r = Rgn;

	if (d->Drag)
	{
		pDC->Colour(DragColumnColour, 24);
		pDC->Rectangle(&r);
	}
	else
	{
		#ifdef MAC

		GArray<GColourStop> Stops;
		GRect j(r.x1, r.y1, r.x2-1, r.y2-1); 
		if (d->cMark)
		{
			// pDC->Colour(Rgb24(0, 0x4e, 0xc1), 24);
			// pDC->Line(r.x1, r.y1, r.x2, r.y1);

			Stops[0].Pos = 0.0;
			Stops[0].Colour = Rgb32(0xd0, 0xe2, 0xf5);
			Stops[1].Pos = 2.0 / (r.Y() - 2);
			Stops[1].Colour = Rgb32(0x98, 0xc1, 0xe9);
			Stops[2].Pos = 0.5;
			Stops[2].Colour = Rgb32(0x86, 0xba, 0xe9);
			Stops[3].Pos = 0.51;
			Stops[3].Colour = Rgb32(0x68, 0xaf, 0xea);
			Stops[4].Pos = 1.0;
			Stops[4].Colour = Rgb32(0xbb, 0xfc, 0xff);
			
			LgiFillGradient(pDC, j, true, Stops);
			
			pDC->Colour(Rgb24(0x66, 0x93, 0xc0), 24);
			pDC->Line(r.x1, r.y2, r.x2, r.y2);
			pDC->Line(r.x2, r.y1, r.x2, r.y2);
		}
		else
		{
			// pDC->Colour(Rgb24(160, 160, 160), 24);
			// pDC->Line(r.x1, r.y1, r.x2, r.y1);
			
			Stops[0].Pos = 0.0;
			Stops[0].Colour = Rgb32(255, 255, 255);
			Stops[1].Pos = 0.5;
			Stops[1].Colour = Rgb32(241, 241, 241);
			Stops[2].Pos = 0.51;
			Stops[2].Colour = Rgb32(233, 233, 233);
			Stops[3].Pos = 1.0;
			Stops[3].Colour = Rgb32(255, 255, 255);
			
			LgiFillGradient(pDC, j, true, Stops);
			
			pDC->Colour(Rgb24(178, 178, 178), 24);
			pDC->Line(r.x1, r.y2, r.x2, r.y2);
			pDC->Line(r.x2, r.y1, r.x2, r.y2);
		}

		GRect n = r;
		n.Size(2, 2);
		OnPaint_Content(pDC, n, false);

		#else
		if (GApp::SkinEngine)
		{
			GSkinState State;
			
			State.pScreen = pDC;			
			State.ptrText = &d->Txt;
			State.Rect = Rgn;
			State.Value = Value();
			State.Enabled = GetList()->Enabled();

			GApp::SkinEngine->OnPaint_ListColumn(ColumnPaint, this, &State);
		}
		else
		{
			if (d->Down)
			{
				LgiThinBorder(pDC, r, DefaultSunkenEdge);
				LgiFlatBorder(pDC, r, 1);
			}
			else
			{
				LgiWideBorder(pDC, r, DefaultRaisedEdge);
			}
			
			OnPaint_Content(pDC, r, true);
		}	
		#endif	
	}
}

