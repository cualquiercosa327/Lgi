#include <math.h>
#include "Lgi.h"
#include "GMenu.h"

#define MENU_IMPL_DEBUG				0

/////////////////////////////////////////////////////////////////////////
// Locals
static MenuItemImpl *Over = 0;

/////////////////////////////////////////////////////////////////////////
// Submenu class
class SubMenuImplPrivate
{
public:
	GSubMenu *Sub;
	GMenuItem *Clicked;
	
	SubMenuImplPrivate()
	{
		Sub = 0;
		Clicked = 0;
	}
	
	void Move(int Inc)
	{
		GSubMenu *p = Sub->GetParent() ? Sub->GetParent()->GetParent() : 0;
		if (p)
		{
			int Index = p->Items.IndexOf(Sub->GetParent());
			GMenuItem *i = p->Items[Index + Inc];
			if (i)
			{
				i->Info->ShowSub();
			}
		}		
	}
	
	void Select(int Inc)
	{
		MenuItemImpl *NewOver = 0;
		if (Over &&
			Over->Item())
		{
			GSubMenu *s = Over->Item()->GetParent();
			if (s == Sub)
			{
				int Index = s->Items.IndexOf(Over->Item());
				int NewIdx = Index + Inc;
				while (NewIdx >= 0 && NewIdx < s->Items.Length())
				{
					GMenuItem *Sel = s->Items[NewIdx];
					if (Sel)
					{
						if (Sel->Separator() || !Sel->Enabled())
						{
							Inc += Inc < 0 ? -1 : 1;
							NewIdx = Index + Inc;
						}
						else
						{
							NewOver = Sel->Info;
							break;
						}
					}
					else break;
				}
			}
			else
			{
				GMenuItem *i = Sub->Items.First();
				if (i)
				{
					NewOver = i->Info;
				}
			}
		}
		else
		{
			GMenuItem *i = Sub->Items.First();
			if (i)
			{
				NewOver = i->Info;
			}
		}
		
		if (NewOver)
		{
			if (Over)
			{
				Over->Invalidate();
			}
			Over = NewOver;
			if (Over)
			{
				Over->Invalidate();
			}
		}
	}
};

SubMenuImpl::SubMenuImpl(GSubMenu *Sub) : GPopup(0)
{
	d = new SubMenuImplPrivate;
	d->Sub = Sub;
	GView::Visible(false);
}

SubMenuImpl::~SubMenuImpl()
{
	DeleteObj(d);
}

GSubMenu *SubMenuImpl::GetSub()
{
	return d->Sub;
}

bool SubMenuImpl::OnKey(GKey &k)
{
	bool Status = false;
	
	#if MENU_IMPL_DEBUG
	printf("IsChar=%i c16=%i\n", k.IsChar, k.c16);
	#endif
	
	if (k.IsChar || k.Alt())
	{
		Status = true;
		switch (k.c16)
		{
			case ' ':
			case VK_RETURN:
			{
				if (k.Down() && Over)
				{
					Over->Activate();
				}
				break;
			}
			default:
			{
				int c = tolower(k.c16);
				if (c >= 'a' && c <= 'z')
				{
					if (k.Down())
					{
						for (GMenuItem *i = d->Sub->Items.First(); i; i = d->Sub->Items.Next())
						{
							char *n = i->Name();
							if (n)
							{
								char *a = n;
								while (a = strchr(a, '&'))
								{
									if (a[1] != '&')
									{
										break;
									}
									else
									{
										a += 2;
									}
								}
								if (a++)
								{
									if (c == tolower(*a))
									{
										i->Info->Activate();
										break;
									}
								}
							}
						}
					}
				}
				else
				{				
					Status = false;
				}
				break;
			}
		}
	}
	else
	{
		Status = true;
		switch (k.c16)
		{
			case ' ':
			{
				if (k.Down() && Over)
				{
					Over->Activate();
				}
				break;
			}
			case VK_UP:
			{
				if (k.Down())
				{
					d->Select(-1);
				}
				break;
			}
			case VK_DOWN:
			{
				if (k.Down())
				{
					d->Select(1);
				}
				break;
			}
			case VK_ESCAPE:
			{
				if (k.Down())
				{
					Visible(false);
				}
				break;
			}
			case VK_LEFT:
			{
				if (k.Down())
				{
					d->Move(-1);
				}
				break;
			}
			case VK_RIGHT:
			{
				if (k.Down())
				{
					if (Over && Over->Item()->Sub())
					{
						Over->ShowSub();
					}
					else
					{
						d->Move(1);
					}
				}
				break;
			}
			default:
			{
				Status = false;
			}
		}
	}
	
	return Status;
}

void SubMenuImpl::OnPaint(GSurface *pDC)
{
	GRect c = GetClient();
    LgiWideBorder(pDC, c, EdgeXpRaised);
    pDC->Colour(LC_MED, 24);
    pDC->Rectangle(&c);
}

void SubMenuImpl::Layout(int Bx, int By)
{
	int Ly = SysFont->GetHeight() + 4;
	int Col = (GdcD->Y() - 32) / Ly;
	int Cols = ceil((double)d->Sub->Items.Length()/Col);
	int ColX = 0;
	int MaxX = 0, MaxY = 0;

	for (int c = 0; c < Cols; c++)
	{
		int x = 0;
		int y = 0;
		
		int n = 0;
		for (GMenuItem *i=d->Sub->Items[c * Col]; i && n < Col; i=d->Sub->Items.Next(), n++)
		{
			GdcPt2 Size;
			i->_Measure(Size);
			x = max(x, Size.x+4);
		}

		n = 0;
		for (GMenuItem *i=d->Sub->Items[c * Col]; i && n < Col; i=d->Sub->Items.Next(), n++)
		{
			GdcPt2 Size;
			i->_Measure(Size);

			int Ht = Size.y;
			
			GRect r;
			r.ZOff(x-1, Ht-1);
			r.Offset(ColX + 2, 2 + y);
			
			if (!Children.HasItem(i->Info))
			{
				Children.Insert(i->Info);
				i->Info->SetParent(this);
			}

			i->Info->SetPos(r);
			y += Ht;
		}
		
		MaxX = max(ColX + x, MaxX);
		MaxY = max(y, MaxY);
		ColX += x;
		
	}

	MaxX += 4;
	MaxY += 4;

	GRect p = GetPos();
	p.Offset(Bx - p.x1, By - p.y1);
	
	int ScrX = GdcD->X();
	int ScrY = GdcD->Y();
	if (p.x1 + MaxX > ScrX)
	{
		p.x1 = ScrX - MaxX;
	}
	if (p.y1 + MaxY > ScrY)
	{
		p.y1 = ScrY - MaxY;
	}
	p.Dimension(MaxX, MaxY);

	SetPos(p);
}

void SubMenuImpl::Visible(bool b)
{
	if (b)
	{
		Layout(GetPos().x1, GetPos().y1);

		if (Over)
		{
			Over->Invalidate();
		}
		Over = d->Sub->Items[0] ? d->Sub->Items[0]->Info : 0;

		GSubMenu *Parent = d->Sub->Parent ? d->Sub->Parent->Parent : 0;
		if (Parent)
		{
			for (GMenuItem *i = Parent->Items.First(); i; i = Parent->Items.Next())
			{
				if (i->Child &&
					i->Child->Info != this)
				{
					GView *v = i->Child->Info->View();
					if (v)
					{
						if (v->Visible())
						{
							v->Visible(false);
						}
					}
				}
			}
		}
	}
	else
	{
		GSubMenu *s = d->Sub;
		if (s)
		{
			for (GMenuItem *i = s->Items.First(); i; i = s->Items.Next())
			{
				if (i->Child &&
					i->Child->Info != this)
				{
					GView *v = i->Child->Info->View();
					if (v)
					{
						v->Visible(false);
					}
				}
			}
		}
	}
	
	GPopup::Visible(b);
}

/////////////////////////////////////////////////////////////////////////
class MenuImplPrivate
{
public:
	GMenu *Menu;
	
	MenuImplPrivate()
	{
		Menu = 0;
	}
};

MenuImpl::MenuImpl(GMenu *Menu)
{
	d = new MenuImplPrivate;
	d->Menu = Menu;
	Name("MenuImpl");

	GRect r(0, 0, 1000, SysFont->GetHeight() + 4);
	SetPos(r);
}

MenuImpl::~MenuImpl()
{
	if (_View)
	{
	}

	DeleteObj(d);
}

bool MenuImpl::HasSubOpen()
{
	for (::GMenuItem *i = d->Menu->Items.First(); i; i = d->Menu->Items.Next())
	{
		GSubMenu *s = i->Sub();
		if (s &&
		    s->Info &&
			s->Info->View() &&
			s->Info->View()->Visible())
		{
			return true;
		}
	}
	
	return false;
}

void MenuImpl::OnPaint(GSurface *pDC)
{
	GRect c = GetClient();
	
	LgiThinBorder(pDC, c, EdgeXpRaised);
    pDC->Colour(LC_MED, 24);
    pDC->Rectangle(&c);
}

bool MenuImpl::Pour(GRegion &r)
{
	GRect *p = FindLargest(r);
	if (!p)
		return false;

	int x = 1;
	int y = 1;
	int Ly = SysFont->GetHeight() + 4;
	
	for (::GMenuItem *i=d->Menu->Items.First(); i; i=d->Menu->Items.Next())
	{
		GdcPt2 Size;
		i->_Measure(Size);
		int Tx = Size.x;
		
		GRect r;
		if (x + Tx + 4 > p->X()-1)
		{
			// next line
			x = 1;
			y += Ly;
			r.Set(x, y, x + Tx + 3, y + Ly - 1);
			x += Tx + 4;
		}
		else
		{
			// next right
			r.Set(x, y, x + Tx + 3, y + Ly - 1);
			x += Tx + 4;
		}

		if (i->Info)
		{
			if (!Children.HasItem(i->Info))
			{
				Children.Insert(i->Info);
				i->Info->SetParent(this);
			}
			i->Info->SetPos(r);
		}
		else
		{
			printf("%s:%i - No view to layout.\n", __FILE__, __LINE__);
		}
	}

	GRect Area;
	Area.ZOff(p->X(), y + Ly);
	SetPos(Area);
	
	return true;
}

/////////////////////////////////////////////////////////////////////////
class MenuItemImplPrivate
{
public:
	::GMenuItem *Item;
	bool Clicked;
	
	MenuItemImplPrivate()
	{
		Item = 0;
		Clicked = false;
	}
};

MenuItemImpl::MenuItemImpl(::GMenuItem *Item)
{
	d = new MenuItemImplPrivate;
	d->Item = Item;

	// _View->_SetWndPtr(0);
	// DeleteObj(_View);
}

MenuItemImpl::~MenuItemImpl()
{
	if (Over == this)
	{
		Over = 0;
	}
	DeleteObj(d);
}

::GMenuItem *MenuItemImpl::Item()
{
	return d->Item;
}

void MenuItemImpl::OnPaint(GSurface *pDC)
{
	int Flags = (d->Item->Enabled() ? 0 : ODS_DISABLED) |
				(Over == this		? ODS_SELECTED : 0) | 
				(d->Item->Checked() ? ODS_CHECKED : 0);
	
	d->Item->_Paint(pDC, Flags);
}

bool MenuItemImpl::IsOnSubMenu()
{
	typedef ::GMenu LgiMenu;
	return !dynamic_cast<LgiMenu*>(d->Item->GetParent());
}

void MenuItemImpl::Activate()
{
	HideSub(true);

	::GMenu *Menu = d->Item->Menu;
	if (Menu && Menu->Info)
	{
		GWindow *w = Menu->Info->View()->GetWindow();
		if (w)
		{
			LgiPostEvent(w->Handle(), M_COMMAND, d->Item->Id(), 0);
		}
		else printf("%s:%i - No Window.\n", _FL);
	}
	else printf("%s:%i - No menu info.\n", _FL);
}

void MenuItemImpl::HideSub(bool SetClick)
{
	for (GSubMenu *	s = d->Item->GetParent();
					s;
					s = s->GetParent() ? s->GetParent()->GetParent() : 0)
	{
		if (SetClick)
		{
			if (s->Info)
			{
				s->Info->Clicked = d->Item;
			}
			else
			{
				printf("%s:%i - no info\n", __FILE__, __LINE__);
			}
		}
		
		if (s->GetMenu() == s)
		{
			break;
		}
		else
		{
			s->Info->View()->Visible(false);
		}
	}
}

void MenuItemImpl::ShowSub()
{
	if (d->Item->Child)
	{
		// Open the submenu
		GdcPt2 p(X(), 0);

		if (!IsOnSubMenu())
		{
			p.x = 0;
			p.y = Y() + 1;
		}

		PointToScreen(p);

		GView *v = d->Item->Child->Info ? d->Item->Child->Info->View() : 0;
		if (v)
		{
			d->Item->Child->Info->IsSub()->Layout(p.x, p.y);
			if (v->Attach(this))
			{
				#if MENU_IMPL_DEBUG
				printf("%s:%i - Showing submenu @ %i,%i (hnd=%p, vis=%i).\n",
					__FILE__, __LINE__, p.x, p.y,
					v->Handle(),
					v->Visible());
				#endif

				v->Visible(true);
			}
			else printf("%s:%i - Attach submenu failed.\n", _FL);
		}
		else printf("%s:%i - No child view.\n", _FL);
	}
	else printf("%s:%i - No child menu, huh?\n", _FL);
}

void MenuItemImpl::OnMouseClick(GMouse &m)
{
	// m.Trace("MenuItemImpl::OnMouseClick");
	if (m.Down())
	{
		if (m.Left())
		{
			if (d->Item->Child)
			{
				#if MENU_IMPL_DEBUG
				printf("%s:%i - Showing sub...\n", __FILE__, __LINE__);
				#endif
				
				ShowSub();
			}
			else
			{
				#if MENU_IMPL_DEBUG
				printf("%s:%i - Menu item start capture...\n", __FILE__, __LINE__);
				#endif
				
				if (!Capture(true))
				{
					printf("%s:%i - Capture failed.\n", __FILE__, __LINE__);
				}
			}
		}
	}
	else
	{
		if (d->Item->Child)
		{
		}
		else if (IsCapturing())
		{
			Capture(false);
			
			GRect c = GetClient();
			if (c.Overlap(m.x, m.y))
			{
				Activate();
			}
			else printf("%s:%i - Up click off item: %i,%i not on %s\n", _FL, m.x, m.y, c.Describe());
		}
		else printf("%s:%i - Menu item up click, but not capturing.\n", _FL);
	}
}

void MenuItemImpl::OnMouseEnter(GMouse &m)
{
	if (d->Item->Enabled() && !d->Item->Separator())
	{
		if (Over)
		{
			Over->Invalidate();
		}
		Over = this;
		if (Over)
		{
			Over->Invalidate();
			
			bool HasOpen = false;
			if (d->Item &&
				d->Item->GetParent() &&
				d->Item->GetParent()->Info)
			{
				GSubMenu *Sub = d->Item->GetParent();
				MenuImpl *Impl = Sub->Info->IsMenu();
				if (Impl)
				{
					HasOpen = Impl->HasSubOpen();
				}
			}
			
			if ((IsOnSubMenu() || HasOpen) && d->Item->Child)
			{
				ShowSub();
			}
		}
	}
}

void MenuItemImpl::OnMouseExit(GMouse &m)
{
	/*
	if (Over)
	{
		Over->Invalidate();
	}
	Over = 0;
	if (Over)
	{
		Over->Invalidate();
	}
	*/
}

