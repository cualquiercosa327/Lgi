#if 0

#include <ctype.h>
#include <math.h>

#include "Lgi.h"
#include "qpopupmenu.h"
#include "qmenuprivate.h"

class QPopupMenuPrivate
{
public:
	QMenuItem *ItemClicked;
	QMenuItem *SubVisible;

	QPopupMenuPrivate()
	{
		ItemClicked = 0;
		SubVisible = 0;
	}
};

///////////////////////////////////////////////////////////////////////////
QPopupMenu::QPopupMenu() : QMenuData(0, 0)
{
	Popup = new QPopupMenuPrivate;
	isTabStop(false);
}

QPopupMenu::~QPopupMenu()
{
	DeleteObj(Popup);
}

QMenuItem *QPopupMenu::ItemClicked()
{
	return Popup->ItemClicked;
}

void QPopupMenu::ShowNextMenu(int Inc)
{
	QMenuData *Menu = Data->Parent->GetParent();
	if (Menu)
	{
		int Index = Menu->Data->Items.IndexOf(Data->Parent);
		QMenuItem *Item = Menu->Data->Items.ItemAt(Index + Inc);
		if (Item)
		{
			hide();
			Item->showPopup();
		}
	}
}

bool QPopupMenu::keyReleaseEvent(XlibEvent *e)
{
	char16 c = tolower(e->unicode());
	
	if (c == VK_RETURN OR c == ' ')
	{
		QMenuItem *i = QMenuItem::Cursor;
		if (i)
		{
			if (i->Item->Parent)
			{
				i->Item->Parent->OnMenuClick(i);
			}
			i->Activate();
		}
		
		return true;
	}
	
	return false;
}

bool QPopupMenu::keyPressEvent(XlibEvent *e)
{
	char16 c = tolower(e->unicode());
	if (c == VK_ESCAPE OR c == VK_LEFT)
	{
		hide();
		
		if (Data->Parent)
		{
			QPopupMenu *w = dynamic_cast<QPopupMenu*>(Data->Parent->GetParent());
			if (w)
			{
			}
			else if (c == VK_LEFT)
			{
				ShowNextMenu(-1);
			}
		}

		return true;
	}
	else if (c == VK_DOWN)
	{
		int Index = GetItems()->IndexOf(QMenuItem::Cursor);
		if (Index < GetItems()->Items() - 1)
		{
			QMenuItem *c = QMenuItem::Cursor;
			QMenuItem::Cursor = 0;
			if (c) c->update();
			while ((c = GetItems()->ItemAt(++Index)) AND c->Item->Separator);
			if (QMenuItem::Cursor = c) c->update();
		}
		return true;
	}
	else if (c == VK_UP)
	{
		int Index = GetItems()->IndexOf(QMenuItem::Cursor);
		if (Index < 0)
		{
			Index = GetItems()->Items();
		}
		QMenuItem *c = QMenuItem::Cursor;
		QMenuItem::Cursor = 0;
		if (c) c->update();
		while ((c = GetItems()->ItemAt(--Index)) AND c->Item->Separator);
		if (QMenuItem::Cursor = c) c->update();			
		return true;
	}
	else if (c == VK_RIGHT)
	{
		QMenuItem *i = QMenuItem::Cursor;
		if (i AND i->Item->Sub)
		{
			i->showPopup();
		}
		else
		{
			ShowNextMenu(1);
		}
		return true;
	}	
	else if (c >= 'a' AND c <= 'z')
	{
		for (QMenuItem *i=GetItems()->First(); i; i=GetItems()->Next())
		{
			char *s = i->getText();
			if (ValidStr(s))
			{
				char *Amp = strchr(s, '&');
				while (Amp AND Amp[1] == '&')
				{
					Amp = strchr(s+2, '&');
				}				
				if (Amp++)
				{
					if (c == tolower(*Amp))
					{
						if (i->Item->Parent)
						{
							i->Item->Parent->OnMenuClick(i);
						}
						i->Activate();
						break;
					}
				}
			}
		}
		return true;
	}
	
	return false;
}

int QPopupMenu::popup(QWidget *parent, int Sx, int Sy)
{
// printf("QPopupMenu::popup this=%p vis=%i\n", this, isVisible());
	if (!isVisible())
	{
		int Ly = SysFont->Y("A") + 4;
		int Col = (GdcD->Y() - 32) / Ly;
		int Cols = ceil((double)GetItems()->Items()/Col);
		int ColX = 0;
		int MaxX = 0, MaxY = 0;

		for (int c = 0; c < Cols; c++)
		{
			int x = 0;
			int y = 0;
			
			int n = 0;
			for (QMenuItem *i=GetItems()->ItemAt(c * Col); i AND n < Col; i=GetItems()->Next(), n++)
			{
				x = max(x, i->x()+4);
			}

			n = 0;
			for (QMenuItem *i=GetItems()->ItemAt(c * Col); i AND n < Col; i=GetItems()->Next(), n++)
			{
				int Ht = i->y();
				i->setGeometry(ColX + 2, 2 + y, x, Ht);
				OsPoint p(ColX + 2, 2 + y);

				if (!i->parentWidget())
				{
					i->reparent(this, p, true);
				}
				i->Item->Parent = this;

				y += Ht;
			}
			
			MaxX = max(ColX + x, MaxX);
			MaxY = max(y, MaxY);
			ColX += x;
		}

		MaxX += 4;
		MaxY += 4;

		int ScrX = GdcD->X();
		int ScrY = GdcD->Y();
		if (Sx + MaxX > ScrX)
		{
			Sx = ScrX - MaxX;
		}
		if (Sy + MaxY > ScrY)
		{
			Sy = ScrY - MaxY;
		}

		setGeometry(Sx, Sy, MaxX, MaxY);
	}
	else
	{
		printf("QPopupMenu::popup already shown!\n");
	}

	return QPopup::popup(parent, Sx, Sy);
}

void QPopupMenu::show(bool Raise)
{
	QPopup::show();

	QMenuItem *i = Data->Parent;
	if (i AND
		i->Item->Parent)
	{
		i->Item->Parent->OnSubMenuVisible(this, true);
	}
}

void QPopupMenu::hide()
{
	QPopup::hide();

	QMenuItem *i = Data->Parent;
	if (i AND
		i->Item->Parent)
	{
		i->Item->Parent->OnSubMenuVisible(this, false);
	}

	for (QMenuItem *i=GetItems()->First(); i; i=GetItems()->Next())
	{
		if (i->sub() AND
			i->sub()->isVisible())
		{
			i->sub()->hide();
		}
	}
}

void QPopupMenu::paintEvent(XlibEvent *e)
{
	GScreenDC Dc(this);
	GRect r(0, 0, width()-1, height()-1);
	LgiWideBorder(&Dc, r, RAISED);
	Dc.Colour(LC_MED, 24);
	Dc.Rectangle(&r);
}

void QPopupMenu::mousePressEvent(XlibEvent *e)
{
	QPopup::mousePressEvent(e);
}


void QPopupMenu::OnSubMenuVisible(QPopupMenu *Item, bool Visible)
{
	if (Visible)
	{
		if (Popup->SubVisible AND
			Popup->SubVisible->sub() != Item)
		{
			Popup->SubVisible->sub()->hide();
		}

		Popup->SubVisible = Item->Data->Parent;
	}
}

void QPopupMenu::OnMenuClick(QMenuItem *Item)
{
	// printf("QPopupMenu::OnMenuClick this=%p par=%p\n", this, Data->Parent ? Data->Parent->Item->Parent : 0);
	if (Data->Parent AND
		Data->Parent->Item->Parent)
	{
		Data->Parent->Item->Parent->OnMenuClick(Item);
	}

	Popup->ItemClicked = Item;
	hide();
}

void QPopupMenu::OnMenuEnter(QMenuItem *Item)
{
	if (Item->sub() AND
		!Item->sub()->isVisible())
	{
		Item->showPopup();
	}
}


#endif