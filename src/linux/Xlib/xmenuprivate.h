#if 0

#define ITEM_SEP

class QMenuDataPrivate
{
public:
	QMenuBar *Menu;
	QMenuItem *Parent;
	QWidget *Widget;
	QList<QMenuItem> Items;

	QMenuDataPrivate(QMenuBar *m, QMenuItem *p)
	{
		Menu = m;
		Parent = p;
		Widget = 0;
	}

	~QMenuDataPrivate()
	{
		Items.DeleteObjects();
	}

	void insert(QMenuItem *i, int where)
	{
		Items.Insert(i, where);

		if (Widget)
		{
			OsPoint p(0, 0);
			i->reparent(Widget, p, true);
		}
	}
};



class QMenuItemPrivate
{
public:
	int Sx, Sy;
	GMenuItem *Source;
	
	QMenuBar *Menu;
	QMenuData *Parent;
	QMenuItem *Item;
	int Cmd;
	QPopupMenu *Sub;
	bool Separator;
	bool Clicked;
	bool Checked;

	QMenuItemPrivate(QMenuBar *menu, QMenuData *parent, QMenuItem *i, GMenuItem *src)
	{
		Menu = menu;
		Parent = parent;
		Item = i;
		Source = src;
		Cmd = -1;
		Separator = false;
		Sub = 0;
		Clicked = false;
		Checked = false;

		Sx = Sy = 0;		
	}

	~QMenuItemPrivate()
	{
		Parent->Data->Items.Delete(Item);
	}

	bool IsOnSub()
	{
		return dynamic_cast<QPopupMenu*>(Parent) != 0;
	}

	void Measure()
	{
		if (NOT Sx OR NOT Sy)
		{
			if (Source)
			{
				GdcPt2 s;
				Source->_Measure(s);
				Sx = s.x;
				Sy = s.y;
			}
			else
			{
				Sx = 20;
				Sy = 8;
			}
		}
	}
		
	int x()
	{
		Measure();
		return Sx;
	}
	
	int y()
	{
		Measure();
		return Sy;
	}

	/*
	void PaintText(GSurface *pDC, GRect &r, int x, int y, char *s)
	{
		char *Tab = strchr(s, '\t');
		if (Tab)
		{
			SysFont->Text(pDC, r.x1 + x, r.y1 + y, s, (int)Tab-(int)s);

			int Tx = SysFont->X(Tab+1);
			SysFont->Text(pDC, r.x1 + r.X() + x - Tx - 7, r.y1 + y, Tab+1);
		}
		else
		{
			SysFont->Text(pDC, r.x1 + x, r.y1 + y, s);
		}
	}
	*/

	void OnPaint(GSurface *pDC)
	{
		GRect r(0, 0, Item->width()-1, Item->height()-1);

		if (Separator)
		{
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(&r);

			int Cy = r.Y()/2;
			pDC->Colour(LC_LOW, 24);
			pDC->Line(r.x1, Cy, r.x2, Cy);
			pDC->Colour(LC_LIGHT, 24);
			pDC->Line(r.x1, Cy+1, r.x2, Cy+1);
		}
		else
		{
			if (Source)
			{
				Source->_Paint(pDC, (Item->Cursor == Item  ? ODS_SELECTED : 0) |
									(Item->isEnabled() ? 0 : ODS_DISABLED) |
									(Checked ? ODS_CHECKED : 0));
			}
			
			/*
			int x = 4;
			int y = 0;
			COLOUR Back = Over ? LC_HIGH : LC_MED;

			if (Clicked AND
				Over)
			{
				LgiThinBorder(pDC, r, SUNKEN);
				x++;
				y++;
			}
			else
			{
				pDC->Colour(Back, 24);
				pDC->Box(&r);
				r.Size(1, 1);
			}

			pDC->Colour(Back, 24);
			pDC->Rectangle(&r);

			if (Item->getText())
			{
				char t[256], *Out=t;
				for (char *In=Item->getText(); *In; In++)
				{
					if (*In == '&')
					{
						if (In[1] == '&')
						{
							*Out++ = *In++;
						}
					}
					else
					{
						*Out++ = *In;
					}
				}
				*Out++ = 0;

				SysFont->Transparent(true);
				if (Item->isEnabled())
				{
					SysFont->Colour(LC_BLACK, LC_MED);
					PaintText(pDC, r, r.x1 + x, r.y1 + y, t);
				}
				else
				{
					SysFont->Colour(LC_LIGHT, LC_MED);
					PaintText(pDC, r, r.x1 + x + 1, r.y1 + y + 1, t);

					SysFont->Colour(LC_LOW, LC_MED);
					PaintText(pDC, r, r.x1 + x, r.y1 + y, t);
				}
			}

			if (Sub AND IsOnSub())
			{
				int x = r.x2 - 7;
				int y = (r.Y()-7) / 2;

				pDC->Colour(LC_TEXT, 24);
				for (int i=0; i<4; i++)
				{
					pDC->Line(x+i, y+i, x+i, y+7-i);
				}
			}
			*/
		}
	}
};

class QMenuBarPrivate
{
public:
	QPopupMenu *Open;

	QMenuBarPrivate()
	{
		Open = 0;
	}
};


#endif