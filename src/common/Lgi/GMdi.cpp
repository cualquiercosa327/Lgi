#include "Lgi.h"
#include "GMdi.h"
#include <stdio.h>
#include "GDisplayString.h"

#define DEBUG_MDI			0

enum GMdiDrag
{
	DragNone	= 0,
	DragMove	= 1 << 1,
	DragLeft	= 1 << 2,
	DragTop		= 1 << 3,
	DragRight	= 1 << 4,
	DragBottom	= 1 << 5,
	DragClose	= 1 << 6,
	DragSystem	= 1 << 7
};

class GMdiChildPrivate
{
public:
	GMdiChild *Child;

	GRect Title;
	#if MDI_TAB_STYLE
	GRect Tab;
	int Order;
	#else
	GRect Close;
	GRect System;
	#endif
	
	int Fy;	
	GMdiDrag Drag;	
	int Ox, Oy;
	bool CloseDown;
	bool CloseOnUp;
	
	GMdiChildPrivate(GMdiChild *c)
	{
		CloseOnUp = false;
		Child = c;
		CloseDown = false;
		Fy = SysFont->GetHeight() + 1;
		Title.ZOff(-1, -1);
		
		#if MDI_TAB_STYLE
		Tab.ZOff(-1, -1);
		Order = -1;
		#else
		Close.ZOff(-1, -1);
		System.ZOff(-1, -1);
		#endif
	}

	#if !MDI_TAB_STYLE
	GMdiDrag HitTest(int x, int y)
	{
		GMdiDrag Hit = DragNone;
		
		if (Child->WindowFromPoint(x, y) == Child)
		{
			if (!Child->GetClient().Overlap(x, y))
			{
				if (System.Overlap(x, y))
				{
					Hit = DragSystem;
				}
				else if (Close.Overlap(x, y))
				{
					Hit = DragClose;
				}
				else if (Title.Overlap(x, y))
				{
					Hit = DragMove;
				}
				else
				{
					GRect c = Child->GLayout::GetClient();
					int Corner = 24;
					if (x < c.x1 + Corner)
					{
						(int&)Hit |= DragLeft;
					}
					if (x > c.x2 - Corner)
					{
						(int&)Hit |= DragRight;
					}
					if (y < c.y1 + Corner)
					{
						(int&)Hit |= DragTop;
					}
					if (y > c.y2 - Corner)
					{
						(int&)Hit |= DragBottom;
					}
				}
			}
		}

		return Hit;
	}
	#endif
};

class GMdiParentPrivate
{
public:
	bool InOnPosChange;
	GRect Tabs;
	GRect Content;
	::GArray<GMdiChild*> Children;
	
	GMdiParentPrivate()
	{
		InOnPosChange = false;
		Tabs.ZOff(-1, -1);
		Content.ZOff(-1, -1);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
GMdiChild::GMdiChild()
{
	d = new GMdiChildPrivate(this);
	GdcPt2 m(100, d->Fy + 8);
	SetMinimumSize(m);

	#if WINNATIVE
	SetStyle(GetStyle() | WS_CLIPSIBLINGS);
	#endif
}

GMdiChild::~GMdiChild()
{
	Detach();
	DeleteObj(d);
}

bool GMdiChild::Attach(GViewI *p)
{
	#if MDI_TAB_STYLE
	GMdiParent *par = dynamic_cast<GMdiParent*>(p);
	if (!par)
		return false;	

	LgiAssert(!par->d->Children.HasItem(this));
	par->d->Children.Add(this);
	d->Order = par->GetNextOrder();
	#endif

	bool s = GLayout::Attach(p);
	if (s)
		AttachChildren();
	return s;
}

bool GMdiChild::Detach()
{
	#if MDI_TAB_STYLE
	GMdiParent *par = dynamic_cast<GMdiParent*>(GetParent());
	if (par)
	{
		LgiAssert(par->d->Children.HasItem(this));
		par->d->Children.Delete(this);
	}
	#endif

	return GLayout::Detach();
}

char *GMdiChild::Name()
{
	return GView::Name();
}

bool GMdiChild::Name(const char *n)
{
	bool s = GView::Name(n);
	
	#if MDI_TAB_STYLE
	if (GetParent())
		GetParent()->Invalidate();
	#else
	Invalidate((GRect*)0, false, true);
	#endif
	
	return s;
}

bool GMdiChild::Pour()
{
	GRect c = GetClient();
	#if !MDI_TAB_STYLE
	c.Size(2, 2);
	#endif

	GRegion Client(c);
	GRegion Update;

	#if DEBUG_MDI
	LgiTrace("    GMdiChild::Pour() '%s', cli=%s, vis=%i, children=%i\n",
		Name(), c.GetStr(), Visible(), Children.Length());
	#endif

	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		if (Visible())
		{
			GRect OldPos = w->GetPos();
			Update.Union(&OldPos);

			if (w->Pour(Client))
			{
				GRect p = w->GetPos();
				
				if (!w->IsAttached())
					w->Attach(this);

				if (!w->Visible())
					w->Visible(true);

				Client.Subtract(&w->GetPos());
				Update.Subtract(&w->GetPos());
			}
			else
			{
				// non-pourable
			}
		}
		else
		{
			w->Visible(false);
		}

		#if DEBUG_MDI
		LgiTrace("      %s, vis=%i\n", w->GetClass(), w->Visible());
		#endif
	}

	return true;
}

void GMdiChild::OnTitleClick(GMouse &m)
{
	if (!m.IsContextMenu() && m.Down())
	{
		Raise();
	}
}

#if MDI_TAB_STYLE
int GMdiChild::GetOrder()
{
	return d->Order;
}
#else
GRect &GMdiChild::GetClient(bool ClientSpace)
{
	static GRect r;
	
	r = GLayout::GetClient(ClientSpace);
	r.Size(3, 3);
	r.y1 += d->Fy + 2;
	r.Size(1, 1);
	
	return r;
}

void GMdiChild::OnPaint(GSurface *pDC)
{
	GRect p = GLayout::GetClient();

	// Border
	LgiWideBorder(pDC, p, RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Box(&p);
	p.Size(1, 1);

	// Title bar
	char *n = Name();
	if (!n) n = "";
	d->Title.ZOff(p.X()-1, d->Fy + 1);
	d->Title.Offset(p.x1, p.y1);
	d->System = d->Title;
	d->System.x2 = d->System.x1 + d->System.Y() - 1;

	// Title text
	bool Top = false;
	if (GetParent())
	{
		GViewIterator *it = GetParent()->IterateViews();
		Top = it->Last() == (GViewI*)this;
		DeleteObj(it);
	}
	
	SysFont->Colour(Top ? LC_ACTIVE_TITLE_TEXT : LC_INACTIVE_TITLE_TEXT, Top ? LC_ACTIVE_TITLE : LC_INACTIVE_TITLE);
	SysFont->Transparent(false);
	GDisplayString ds(SysFont, n);
	ds.Draw(pDC, d->System.x2 + 1, d->Title.y1 + 1, &d->Title);
	
	// System button
	GRect r = d->System;
	r.Size(2, 1);
	pDC->Colour(LC_BLACK, 24);
	pDC->Box(&r);
	r.Size(1, 1);
	pDC->Colour(LC_WHITE, 24);
	pDC->Rectangle(&r);
	pDC->Colour(LC_BLACK, 24);
	for (int k=r.y1 + 1; k < r.y2 - 1; k += 2)
	{
		pDC->Line(r.x1 + 1, k, r.x2 - 1, k);
	}

	// Close button
	d->Close = d->Title;
	d->Close.x1 = d->Close.x2 - d->Close.Y() + 1;
	d->Close.Size(2, 2);
	r = d->Close;
	LgiWideBorder(pDC, r, d->CloseDown ? SUNKEN : RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);
	int Cx = d->Close.x1 + (d->Close.X() >> 1) - 1 + d->CloseDown;
	int Cy = d->Close.y1 + (d->Close.Y() >> 1) - 1 + d->CloseDown;
	int s = 3;
	pDC->Colour(Rgb24(108,108,108), 24);
	pDC->Line(Cx-s, Cy-s+1, Cx+s-1, Cy+s);
	pDC->Line(Cx-s+1, Cy-s, Cx+s, Cy+s-1);
	pDC->Line(Cx-s, Cy+s-1, Cx+s-1, Cy-s);
	pDC->Line(Cx-s+1, Cy+s, Cx+s, Cy-s+1);
	pDC->Colour(LC_BLACK, 24);
	pDC->Line(Cx-s, Cy-s, Cx+s, Cy+s);
	pDC->Line(Cx-s, Cy+s, Cx+s, Cy-s);

	// Client
	GRect Client = GetClient();
	Client.Size(-1, -1);
	pDC->Colour(LC_MED, 24);
	pDC->Box(&Client);
	
	if (!Children.First())
	{
		Client.Size(1, 1);
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(&Client);
	}
	
	Pour();
}

void GMdiChild::OnMouseClick(GMouse &m)
{
	if (m.Left())
	{
		Raise();

		if (m.Down())
		{
			GRect c = GLayout::GetClient();
			d->Drag = DragNone;
			d->Ox = d->Oy = -1;
			
			d->Drag = d->HitTest(m.x, m.y);
			if (d->Drag)
			{
				Capture(true);
			}

			if (d->Drag == DragSystem)
			{
				if (m.Double())
				{
					d->CloseOnUp = true;
				}
				else
				{
					/*
					GdcPt2 p(d->System.x1, d->System.y2+1);
					PointToScreen(p);
					GSubMenu *Sub = new GSubMenu;
					if (Sub)
					{
						Sub->AppendItem("Close", 1, true);
						Close = Sub->Float(this, p.x, p.y, true) == 1;
						DeleteObj(Sub);
					}
					*/
				}
			}
			else if (d->Drag == DragClose)
			{
				d->CloseDown = true;
				Invalidate(&d->Close);
			}
			else if (d->Drag == DragMove)
			{
				d->Ox = m.x - c.x1;
				d->Oy = m.y - c.y1;
			}
			else
			{
				if (d->Drag & DragLeft)
				{
					d->Ox = m.x - c.x1;
				}
				if (d->Drag & DragRight)
				{
					d->Ox = m.x - c.x2;
				}
				if (d->Drag & DragTop)
				{
					d->Oy = m.y - c.y1;
				}
				if (d->Drag & DragBottom)
				{
					d->Oy = m.y - c.y2;
				}
			}
		}
		else
		{
			if (IsCapturing())
			{
				Capture(false);
			}

			if (d->CloseOnUp)
			{
				d->Drag = DragNone;
				if (OnRequestClose(false))
				{
					Quit();
					return;
				}
			}
			else if (d->Drag == DragClose)
			{
				if (d->Close.Overlap(m.x, m.y))
				{
					d->Drag = DragNone;
					d->CloseDown = false;
					Invalidate(&d->Close);
					
					if (OnRequestClose(false))
					{
						Quit();
						return;
					}
				}
				else printf("%s:%i - Outside close btn.\n", _FL);
			}
			else
			{
				d->Drag = DragNone;
			}
		}
	}
}

LgiCursor GMdiChild::GetCursor(int x, int y)
{
	GMdiDrag Hit = d->HitTest(x, y);
	if (Hit & DragLeft)
	{
		if (Hit & DragTop)
		{
			return LCUR_SizeFDiag;
		}
		else if (Hit & DragBottom)
		{
			return LCUR_SizeBDiag;
		}
		else
		{
			return LCUR_SizeHor;
		}
	}
	else if (Hit & DragRight)
	{
		if (Hit & DragTop)
		{
			return LCUR_SizeBDiag;
		}
		else if (Hit & DragBottom)
		{
			return LCUR_SizeFDiag;
		}
		else
		{
			return LCUR_SizeHor;
		}
	}
	else if ((Hit & DragTop) || (Hit & DragBottom))
	{
		return LCUR_SizeVer;
	}
	
	return LCUR_Normal;
}

void GMdiChild::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		GRect p = GetPos();
		GdcPt2 Min = GetMinimumSize();

		if (d->Drag == DragClose)
		{
			bool Over = d->Close.Overlap(m.x, m.y);
			if (Over ^ d->CloseDown)
			{
				d->CloseDown = Over;
				Invalidate(&d->Close);
			}
		}
		else
		{
			// GetMouse(m, true);
			GdcPt2 n(m.x, m.y);
			n.x += GetPos().x1;
			n.y += GetPos().y1;
			// GetParent()->PointToView(n);
				
			if (d->Drag == DragMove)
			{
				p.Offset(n.x - d->Ox - p.x1, n.y - d->Oy - p.y1);
				if (p.x1 < 0) p.Offset(-p.x1, 0);
				if (p.y1 < 0) p.Offset(0, -p.y1);
			}
			else
			{
				if (d->Drag & DragLeft)
				{
					p.x1 = min(p.x2 - Min.x, n.x - d->Ox);
					if (p.x1 < 0) p.x1 = 0;
				}
				if (d->Drag & DragRight)
				{
					p.x2 = max(p.x1 + Min.x, n.x - d->Ox);
				}
				if (d->Drag & DragTop)
				{
					p.y1 = min(p.y2 - Min.y, n.y - d->Oy);
					if (p.y1 < 0) p.y1 = 0;
				}
				if (d->Drag & DragBottom)
				{
					p.y2 = max(p.y1 + Min.y, n.y - d->Oy);
				}
			}
		}

		SetPos(p);
	}
}
#endif

#if defined __GTK_H__
using namespace Gtk;
#endif

void GMdiChild::Raise()
{
	GMdiParent *p = dynamic_cast<GMdiParent*>(GetParent());
	if (p)
	{
		#if MDI_TAB_STYLE
		
		#else

		#if defined __GTK_H__
		GtkWidget *wid = Handle();
		GdkWindow *wnd = wid ? GDK_WINDOW(wid->window) : NULL;
		if (wnd)
			gdk_window_raise(wnd);
		else
			LgiAssert(0);
		#elif WINNATIVE
		SetWindowPos(Handle(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		#else
		#error "Impl me."
		#endif

		#endif

		if (p->d->Children.Last() != this)
		{
			p->d->Children.Delete(this);
			p->d->Children.Add(this);
			p->d->Tabs.ZOff(-1, -1);
			p->OnPosChange();
		}
		
		Focus(true);

		#if DEBUG_MDI
		LgiTrace("GMdiChild::Raise() '%s'\n", Name());
		#endif
	}
}

void GMdiChild::Lower()
{
	GMdiParent *p = dynamic_cast<GMdiParent*>(GetParent());
	if (p)
	{
		#if MDI_TAB_STYLE


		#else

		#if defined __GTK_H__
		GtkWidget *wid = Handle();
		GdkWindow *wnd = wid ? GDK_WINDOW(wid->window) : NULL;
		if (wnd)
			gdk_window_lower(wnd);
		else
			LgiAssert(0);
		#elif WINNATIVE
		SetWindowPos(Handle(), HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		#else
		#error "Impl me."
		#endif

		#endif

		if (p->d->Children[0] != this)
		{
			p->d->Children.Delete(this);
			p->d->Children.AddAt(0, this);
			p->d->Tabs.ZOff(-1, -1);
			p->OnPosChange();
		}
		
		#if DEBUG_MDI
		LgiTrace("GMdiChild::Lower() '%s'\n", Name());
		#endif
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
GMdiParent::GMdiParent()
{
	d = new GMdiParentPrivate;
	SetPourLargest(true);
}

GMdiParent::~GMdiParent()
{
	if (GetWindow())
	{
		GetWindow()->UnregisterHook(this);
	}
	
	DeleteObj(d);
}

#if MDI_TAB_STYLE
int ViewCmp(GMdiChild **a, GMdiChild **b)
{
	return (*a)->GetOrder() - (*b)->GetOrder();
}
#endif

void GMdiParent::OnPaint(GSurface *pDC)
{
	#if MDI_TAB_STYLE
	if (d->Children.Length() == 0)
	#endif
	{
		pDC->Colour(LC_LOW, 24);
		pDC->Rectangle();
		return;
	}
	
	#if MDI_TAB_STYLE
	// Draw tabs...
	GFont *Fnt = GetFont();
	
	GColour Back(LC_LOW, 24), Black(0, 0, 0);
	int Cx = 5;
	pDC->Colour(Back);
	pDC->Rectangle(d->Tabs.x1, d->Tabs.y1, d->Tabs.x1 + Cx - 1, d->Tabs.y2 - 1);
	pDC->Colour(Black);
	pDC->Line(d->Tabs.x1, d->Tabs.y2, d->Tabs.x2, d->Tabs.y2);
	
	int MarginPx = 10;
	
	::GArray<GMdiChild*> Views;
	GMdiChild *Last = dynamic_cast<GMdiChild*>(d->Children.Last());
	GetChildren(Views);
	
	for (int Idx=0; Idx<Views.Length(); Idx++)
	{
		GMdiChild *c = Views[Idx];
		bool Active = c == Last;
		
		GDisplayString ds(GetFont(), c->Name());
		c->d->Tab.ZOff(ds.X()+1, d->Tabs.y2-1);
		c->d->Tab.Offset(d->Tabs.x1 + Cx, d->Tabs.y1);
		c->d->Tab.x2 += MarginPx * 2;
		
		c->Visible(Active);
		GColour Bk(Active ? LC_WORKSPACE : LC_MED, 24);
		GColour Edge(LC_BLACK, 24);
		GColour Txt(LC_TEXT, 24);
		
		GRect r = c->d->Tab;
		if (Active)
			r.y2 += 1;

		int OffX = 0, OffY = 0;
		for (; OffY < r.Y(); OffX++, OffY += 2)
		{
			pDC->Colour(Edge);
			pDC->VLine(r.x1 + OffX, r.y2 - OffY - 1, r.y2 - OffY);
			pDC->Colour(Back);
			pDC->VLine(r.x1 + OffX, r.y1, r.y2 - OffY - 2);
			if (OffY > 0)
			{
				pDC->Colour(Bk);
				pDC->VLine(r.x1 + OffX, r.y2 - OffY + 1, r.y2);
			}
		}
		pDC->Colour(Edge);
		pDC->HLine(r.x1 + OffX - 1, r.x2, r.y1);
		pDC->VLine(r.x2, r.y1, r.y2);

		r.Size(1, 1);
		r.y2 += 1;

		Fnt->Fore(Txt);
		Fnt->Back(Bk);
		Fnt->Transparent(false);
		GRect txt = r;
		txt.x1 += OffX - 1;
		ds.Draw(pDC, r.x1 + MarginPx + 2, r.y1, &txt);
		
		Cx += c->d->Tab.X();
	}

	pDC->Colour(Back);
	pDC->Rectangle(d->Tabs.x1 + Cx, d->Tabs.y1, d->Tabs.x2, d->Tabs.y2 - 1);
	#endif
}

bool GMdiParent::Attach(GViewI *p)
{
	bool s = GLayout::Attach(p);
	if (s)
	{
		GetWindow()->RegisterHook(this, GKeyAndMouseEvents);
		OnPosChange();
	}
	return s;
}

GMdiChild *GMdiParent::IsChild(GViewI *View)
{
	for (GViewI *v=View; v; v=v->GetParent())
	{
		if (v->GetParent() == this)
		{
			GMdiChild *c = dynamic_cast<GMdiChild*>(v);
			if (c)
			{
				return c;
			}
			LgiAssert(0);
			break;
		}
	}
	
	return 0;
}

bool GMdiParent::OnViewMouse(GView *View, GMouse &m)
{
	if (m.Down())
	{
		GMdiChild *v = IsChild(View);
		GMdiChild *l = IsChild(Children.Last());
		if (v != NULL && v != l)
		{
			v->Raise();
		}
	}
	
	return true;
}

bool GMdiParent::OnViewKey(GView *View, GKey &Key)
{
	if (Key.Down() && Key.Ctrl() && Key.c16 == '\t')
	{
		GMdiChild *Child = IsChild(View);
		#if DEBUG_MDI
		LgiTrace("Child=%p %s view=%s\n", Child, Child ? Child->Name() : 0, View->GetClass());
		#endif
		if (Child)
		{
			GView *v;

			if (Key.Shift())
			{
				v = dynamic_cast<GView*>(Children.First());
			}
			else
			{
				v = dynamic_cast<GView*>(Children.Last());
			}

			GMdiChild *c = dynamic_cast<GMdiChild*>(v);
			if (c)
			{
				int Idx = Children.IndexOf((GViewI*)v);
				#if DEBUG_MDI
				LgiTrace("roll = %i of %i\n", Idx, Children.Length());
				#endif
				
				if (Key.Shift())
				{
					c->Raise();
				}
				else
				{
					c->Lower();
				}

				GMdiChild *Top = dynamic_cast<GMdiChild*>(Children.Last());
				if (Top)
					Top->Focus(true);

				return true;
			}
		}
	}

	return false;
}

#if MDI_TAB_STYLE
void GMdiParent::OnMouseClick(GMouse &m)
{
	::GArray<GMdiChild*> Views;
	if (GetChildren(Views))
	{
		for (int i=0; i<Views.Length(); i++)
		{
			GMdiChild *c = Views[i];
			if (c->d->Tab.Overlap(m.x, m.y))
			{
				c->OnTitleClick(m);
				break;
			}
		}
	}	
}

void GMdiParent::OnPosChange()
{
	if (!IsAttached())
		return;

	GFont *Fnt = GetFont();
	GRect Client = GetClient();
	GRect Tabs, Content;
		
	d->InOnPosChange = true;
	
	Tabs = Client;
	Tabs.y2 = Tabs.y1 + Fnt->GetHeight() + 1;
	Content = Client;
	Content.y1 = Tabs.y2 + 1;
	
	if (Tabs != d->Tabs ||
		Content != d->Content)
	{
		d->Tabs = Tabs;
		d->Content = Content;
		
		GMdiChild *Last = d->Children.Length() ? d->Children.Last() : NULL;

		#if DEBUG_MDI
		LgiTrace("OnPosChange: Tabs=%s, Content=%s, Children=%i, Last=%s\n",
			d->Tabs.GetStr(), d->Content.GetStr(),
			Children.Length(),
			Last ? Last->Name() : 0);
		#endif

		for (int Idx=0; Idx<d->Children.Length(); Idx++)
		{
			GMdiChild *c = d->Children[Idx];

			if (c == Last)
			{
				#if DEBUG_MDI
				LgiTrace("  [%p/%s], vis=%i, attached=%i\n",
					c, c->Name(), c == Last, c->IsAttached());
				#endif
				
				if (!c->IsAttached())
					c->GLayout::Attach(this);

				c->SetPos(d->Content);
				c->Visible(true);
				c->Pour();
			}
			else
			{
				#if DEBUG_MDI
				LgiTrace("  [%p/%s], vis=%i, attached=%i\n",
					c, c->Name(), c == Last, c->IsAttached());
				#endif
				#ifdef BEOS
				if (c->IsAttached())
				{
					#if DEBUG_MDI
					LgiTrace("    detaching %s\n", c->GetClass());
					#endif
					c->GLayout::Detach();
					c->SetParent(this);
				}
				#else
				c->Visible(false);
				#endif
			}
		}

		Invalidate();
	}

	d->InOnPosChange = false;
}
#endif

GRect GMdiParent::NewPos()
{
	GRect Status(0, 0, X()*0.75, Y()*0.75);

	int Block = 5;
	for (int y=0; y<Y()>>Block; y++)
	{
		for (int x=0; x<X()>>Block; x++)
		{
			bool Has = false;
			for (GViewI *c = Children.First(); c; c = Children.Next())
			{
				GRect p = c->GetPos();
				if (p.x1 >> Block == x &&
					p.y1 >> Block == y)
				{
					Has = true;
					break;
				}
			}
			
			if (!Has)
			{
				Status.Offset(x << Block, y << Block);
				return Status;
			}
		}
	}
	
	return Status;
}

bool GMdiParent::Detach()
{
	d->InOnPosChange = true;
	return GLayout::Detach();
}

void GMdiParent::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	#if MDI_TAB_STYLE
	if (!d->InOnPosChange /*&& Attaching*/)
	{
		d->Tabs.ZOff(-1, -1);
		OnPosChange();
		Invalidate();
	}
	#else
	for (GViewI *v=Children.First(); v; )
	{
		GMdiChild *c = dynamic_cast<GMdiChild*>(v);
		v = Children.Next();
		if (c)
		{
			c->Invalidate(&c->d->Title);
			
			if (!v)
			{
				GViewI *n = c->Children.First();
				if (n)
				{
					n->Focus(true);
				}
			}
		}
	}
	#endif
}

bool GMdiParent::GetChildren(::GArray<GMdiChild*> &Views)
{
	Views = d->Children;
	#if MDI_TAB_STYLE
	Views.Sort(ViewCmp);
	#endif
	return Views.Length() > 0;
}

GViewI *GMdiParent::GetTop()
{
	return d->Children.Length() > 0 ? d->Children.Last() : NULL;
}

#if MDI_TAB_STYLE
int GMdiParent::GetNextOrder()
{
	int m = 0;
	for (int i=0; i<d->Children.Length(); i++)
	{
		m = max(m, d->Children[i]->GetOrder());
	}
	return m + 1;
}
#endif
