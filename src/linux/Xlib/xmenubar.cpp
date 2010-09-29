#if 0

#include "Lgi.h"
#include "qmenubar.h"
#include "qmenuprivate.h"

QMenuItem *QMenuItem::Cursor = 0;

QMenuItem::QMenuItem(QMenuBar *Menu, QMenuData *Parent, GMenuItem *i)
{
	Item = new QMenuItemPrivate(Menu, Parent, this, i);
	isTabStop(false);
}

QMenuItem::~QMenuItem()
{
	if (Cursor == this)
	{
		Cursor = 0;
	}
	DeleteObj(Item);
}

bool QMenuItem::Checked()
{
	return Item->Checked;
}

void QMenuItem::Checked(bool i)
{
	Item->Checked = i;
}

int QMenuItem::x()
{
	return Item->x();
}

int QMenuItem::y()
{
	return Item->y();
}

int QMenuItem::cmd()
{
	return Item->Cmd;
}

void QMenuItem::setCmd(int i)
{
	Item->Cmd = i;
}

int QMenuItem::type()
{
	return Item->Separator;
}

void QMenuItem::setType(int i)
{
	Item->Separator = i;
}

QMenuData *QMenuItem::GetParent()
{
	return Item->Parent;
}

QPopupMenu *QMenuItem::sub()
{
	return Item->Sub;
}

void QMenuItem::setSub(QPopupMenu *m)
{
	Item->Sub = m;
}

void QMenuItem::paintEvent(XlibEvent *e)
{
	GScreenDC Dc(this);
	Item->OnPaint(&Dc);
}

void QMenuItem::hidePopup()
{
	if (Item->Sub AND Item->Sub->isVisible())
	{
		Item->Sub->hide();
	}
}

void QMenuItem::showPopup()
{
	if (Item->Sub)
	{
		OsPoint p;
		if (Item->IsOnSub())
		{
			p.set(width()-2, 0);
		}
		else
		{
			p.set(0, height());
		}
		p = mapToGlobal(p);
		Item->Sub->popup(this, p.x, p.y);
		Item->Sub->setFocus();
	}
}

void QMenuItem::mousePressEvent(XlibEvent *e)
{
	if (NOT Item->Separator AND
		isEnabled())
	{
		if (Item->Sub)
		{
			showPopup();
		}
		else
		{
			Item->Clicked = true;
			grabMouse();
			repaint();
		}
	}
}

void QMenuItem::mouseReleaseEvent(XlibEvent *e)
{
	if (isEnabled() AND
		Item->Clicked)
	{
		GRect r(0, 0, width(), height());
		
		if (Cursor == this AND
			NOT Item->Sub AND
			r.Overlap(e->x(), e->y()) )
		{
			if (Item->Parent)
			{
				Item->Parent->OnMenuClick(this);
			}
			if (Item->Menu)
			{
				Activate();
			}
			else
			{
				// printf("Error: Couldn't send Cmd - no menu.\n");
			}
		}

		ungrabMouse();
		Item->Clicked = false;
		update();
	}
}

void QMenuItem::Activate()
{
	if (Item)
	{
		GMessage m(M_COMMAND, Item->Cmd, 0);
		if (Item->Menu)
		{
			QApp()->postEvent(Item->Menu->parentWidget(), &m);
		}
	}
}

void QMenuItem::leaveEvent(XlibEvent *e)
{
	/*
	if (isEnabled())
	{
		Item->Over = false;
		update();
	}
	*/
}

void QMenuItem::enterEvent(XlibEvent *e)
{
	if (isEnabled())
	{
		QMenuItem *c = Cursor;
		Cursor = 0;
		if (c) c->update();
		Cursor = this;
		update();

		if (Item->Parent)
		{
			Item->Parent->OnMenuEnter(this);
		}
	}
}

////////////////////////////////////////////////////////////////////
QMenuData::QMenuData(QMenuBar *Menu, QMenuItem *Parent)
{
	Data = new QMenuDataPrivate(Menu, Parent);
}

QMenuData::~QMenuData()
{
	DeleteObj(Data);
}

QList<QMenuItem> *QMenuData::GetItems()
{
	return &Data->Items;
}

QWidget *QMenuData::GetWidget()
{
	return Data->Widget;
}

QMenuItem *QMenuData::insertItem(GMenuItem *item, int cmd, int where)
{
	QMenuItem *i = new QMenuItem(Data->Menu, this, item);
	if (i)
	{
		i->Item->Cmd = cmd;
		Data->insert(i, where);
	}

	return i;
}

QMenuItem *QMenuData::insertItem(GMenuItem *item, QPopupMenu *sub, int where)
{
	QMenuItem *i = 0;
	if (sub)
	{
		i = new QMenuItem(Data->Menu, this, item);
		if (i)
		{
			sub->Data->Menu = Data->Menu;
			sub->Data->Parent = i;

			i->Item->Sub = sub;
			Data->insert(i, where);
		}
	}

	return i;
}

QMenuItem *QMenuData::insertSeparator(int where)
{
	QMenuItem *i = new QMenuItem(Data->Menu, this, 0);
	if (i)
	{
		i->Item->Separator = true;
		Data->insert(i, where);
	}

	return i;
}

void QMenuData::deleteItem(QMenuItem *i)
{
	Data->Items.Delete(i);
}

//////////////////////////////////////////////////////////////////////
QMenuBar::QMenuBar() : QMenuData(this, 0)
{
	MenuBar = new QMenuBarPrivate;
	Data->Widget = this;
	setText("QMenuBar");
	isTabStop(false);
}

QMenuBar::~QMenuBar()
{
	DeleteObj(MenuBar);
}

void QMenuBar::OnSubMenuVisible(QPopupMenu *Item, bool Visible)
{
	if (Visible)
	{
		MenuBar->Open = Item;
	}
	else
	{
		MenuBar->Open = 0;
	}
}

void QMenuBar::OnMenuClick(QMenuItem *Item)
{
}

void QMenuBar::OnMenuEnter(QMenuItem *Item)
{
	if (Item AND
		Item->sub() AND
		MenuBar->Open)
	{
		if (MenuBar->Open != Item->sub())
		{
			MenuBar->Open->hide();
			Item->showPopup();
		}
	}
}

GRect &QMenuBar::Pour(GRect &p)
{
	static GRect Area;

	int x = 1;
	int y = 1;
	int Ly = SysFont->Y("A") + 4;
	
	for (QMenuItem *i=GetItems()->First(); i; i=GetItems()->Next())
	{
		int Tx = i->x();
		GRect r;
		if (x + Tx + 4 > p.X()-1)
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

		i->setGeometry(r.x1, r.y1, r.X(), r.Y());
	}

	Area.ZOff(p.X(), y + Ly);
	setGeometry(Area.x1, Area.y1, Area.X(), Area.Y());

	return Area;
}

void QMenuBar::paintEvent(XlibEvent *e)
{
	GScreenDC Dc(this);
	GRect p(0, 0, width()-1, height()-1);

	// draw edge
	LgiThinBorder(&Dc, p, RAISED);

	// draw bar
	Dc.Colour(LC_MED, 24);
	Dc.Rectangle(&p);
}

#endif 0