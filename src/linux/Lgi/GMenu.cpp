/*hdr
**      FILE:           GuiMenu.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           18/7/98
**      DESCRIPTION:    Gui menu system
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "Lgi.h"
#include "GToken.h"

///////////////////////////////////////////////////////////////////////////////////////////////
GSubMenu::GSubMenu(char *name, bool Popup)
{
	Menu = 0;
	Parent = 0;
	Info = 0;

	#if defined __GTK_H__
	if (Popup)
		Info = new SubMenuImpl(this);
	#endif
}

GSubMenu::~GSubMenu()
{
	Items.DeleteObjects();
	DeleteObj(Info);
}

int GSubMenu::Length()
{
	return Items.Length();
}

GMenuItem *GSubMenu::ItemAt(int Id)
{
	return Items.ItemAt(Id);
}

GMenuItem *GSubMenu::AppendItem(char *Str, int Id, bool Enabled, int Where, char *Shortcut)
{
	GMenuItem *i = new GMenuItem;
	if (i)
	{
		i->Parent = this;
		i->Menu = Menu;
		i->Id(Id);
		i->Name(Str);
		i->Enabled(Enabled);
		i->ScanForAccel();

		Items.Insert(i, Where);

		return i;
	}

	return 0;
}

GMenuItem *GSubMenu::AppendSeparator(int Where)
{
	GMenuItem *i = new GMenuItem;
	if (i)
	{
		i->Parent = this;
		i->Menu = Menu;
		i->Id(-2);

		Items.Insert(i, Where);

		return i;
	}

	return 0;
}

GSubMenu *GSubMenu::AppendSub(char *Str, int Where)
{
	GMenuItem *i = new GMenuItem;
	if (i)
	{
		i->Parent = this;
		i->Menu = Menu;
		i->Id(-1);

		Items.Insert(i, Where);

		i->Child = new GSubMenu(Str);
		if (i->Child)
		{
			i->Child->Parent = i;
			i->Child->Menu = Menu;
			i->Child->Window = Window;
		}

		i->Name(Str);
		
		return i->Child;
	}

	return 0;
}

void GSubMenu::Empty()
{
	Items.DeleteObjects();
}

bool GSubMenu::RemoveItem(int i)
{
	return RemoveItem(Items.ItemAt(i));
}

bool GSubMenu::RemoveItem(GMenuItem *Item)
{
	if (Item AND Items.HasItem(Item))
	{
		return Item->Remove();
	}

	return false;
}

bool GSubMenu::OnKey(GKey &k)
{
	return false;
}

#if 0
bool IsOverMenu(XEvent *e)
{
	if (e->xany.type == ButtonPress OR
		e->xany.type == ButtonRelease)
	{
		QWidget *q = QWidget::Find(e->xany.window);
		if (q)
		{
			QPopup *m = 0;
			for (; q; q = q->parentWidget())
			{
				if (m = dynamic_cast<QPopup*>(q))
				{
					break;
				}
			}
			
			if (m)
			{
				GRect gr = m->geometry();
				
				return gr.Overlap
					(
						e->xbutton.x_root,
						e->xbutton.y_root
					);
			}
		}
	}
	
	return false;
}
#endif

int GSubMenu::Float(GView *From, int x, int y, bool Left)
{
	static int Depth = 0;

	while (From && !From->Handle())
	{
		From = dynamic_cast<GView*>(From->GetParent());
	}		
	
	if (!From || !From->Handle())
	{
		printf("%s:%i - No menu handle\n", _FL);
		return -1;
	}

	SubMenuImpl *m = Info->IsSub();
	if (!m)
	{
		printf("%s,%i - Not a popupmenu.\n", __FILE__, __LINE__);
	}
	else
	{
		From->Capture(false);

		// Simple depth protection to prevent recursive calling
		bool First = true;
		Depth++;
		if (Depth == 1)
		{
			// Show the menu
			GRect r = m->GetPos();
			r.Offset(x - r.x1, y - r.y1);
			m->SetPos(r);
			m->SetParent(From);			
			m->Visible(true);
			
			LgiTrace("Starting float loop...\n");
			if (m->Handle())
			{
				// Wait for the menu to hide itself
				while (m->Visible())
				{
					LgiYield();
					LgiSleep(1);
				}
			}
			LgiTrace("Finished float loop...\n");
		}
		else printf("%s:%i - Depth was %i!?!\n", _FL, Depth);
		Depth--;

		GMenuItem *i = m->ItemClicked();
		if (i)
		{
			return i->Id();
		}
	}

	return 0;
}

GSubMenu *GSubMenu::FindSubMenu(int Id)
{
	for (GMenuItem *i = Items.First(); i; i = Items.Next())
	{
		GSubMenu *Sub = i->Sub();

		// printf("Find(%i) '%s' %i sub=%p\n", Id, i->Name(), i->Id(), Sub);
		if (i->Id() == Id)
		{
			return Sub;
		}
		else if (Sub)
		{
			GSubMenu *m = Sub->FindSubMenu(Id);
			if (m)
			{
				return m;
			}
		}
	}

	return 0;
}

GMenuItem *GSubMenu::FindItem(int Id)
{
	for (GMenuItem *i = Items.First(); i; i = Items.Next())
	{
		GSubMenu *Sub = i->Sub();

		if (i->Id() == Id)
		{
			return i;
		}
		else if (Sub)
		{
			i = Sub->FindItem(Id);
			if (i)
			{
				return i;
			}
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
GMenuItem::GMenuItem()
{
	Info = new MenuItemImpl(this);
	Child = 0;
	_Icon = -1;
	_Id = 0;
	_Enabled = true;
	_Check = false;
}

GMenuItem::GMenuItem(GMenu *m, GSubMenu *p, int Pos, char *Shortcut)
{
	Info = 0;
	Child = 0;
	_Icon = -1;
}

GMenuItem::~GMenuItem()
{
	DeleteObj(Child);
	DeleteObj(Info);
}

// the following 3 functions paint the menus according the to
// windows standard. but also allow for correct drawing of menuitem
// icons. some implementations of windows force the program back
// to the 8-bit palette when specifying the icon graphic, thus removing
// control over the colours displayed. these functions remove that
// limitation and also provide the application the ability to override
// the default painting behaviour if desired.
void GMenuItem::_Measure(GdcPt2 &Size)
{
	GFont *Font = Menu AND Menu->GetFont() ? Menu->GetFont() : SysFont;
	bool BaseMenu = Parent == Menu; // true if attached to a windows menu
									// else is a submenu
	int Ht = Font->GetHeight();
	// int IconX = BaseMenu ? ((24-Ht)/2)-Ht : 20;
	int IconX = BaseMenu ? 2 : 16;

	if (Separator())
	{
		Size.x = 8;
		Size.y = 8;
	}
	else
	{
		// remove '&' chars for string measurement
		char Str[256];
		char *n = Name(), *i = n, *o = Str;

		while (i && *i)
		{
			if (*i == '&')
			{
				if (i[1] == '&')
				{
					*o++ = *i++;
				}
			}
			else
			{
				*o++ = *i;
			}

			i++;
		}
		*o++ = 0;
		
		// check for accelerators
		char *Tab = strchr(Str, '\t');
		
		if (Tab)
		{
			// string with accel
			GDisplayString Name(Font, Str, (int)Tab-(int)Str);
			GDisplayString ShCut(Font, Tab+1);
			Size.x = IconX + 32 + Name.X() + ShCut.X();

		}
		else
		{
			// normal string
			GDisplayString ds(Font, Str);
			Size.x = IconX + ds.X() + 4;
		}

		if (!BaseMenu)
		{
			// leave room for child pointer
			Size.x += Child ? 8 : 0;
		}

		Size.y = max(IconX, Ht+2);
	}
}

#define Time(a, b) ((double)(b - a) / 1000)

void GMenuItem::_PaintText(GSurface *pDC, int x, int y, int Width)
{
	char *n = Name();
	if (n)
	{
		GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
		bool Underline = false;
		int CharY = Font->GetHeight();

		char *e = 0;
		for (char *s=n; s AND *s; s = *e ? e : 0)
		{
			switch (*s)
			{
				case '&':
				{
					if (s[1] == '&')
					{
						e = s + 2;
						GDisplayString d(Font, "&");
						d.Draw(pDC, x, y, 0);
						x += d.X();
					}
					else
					{
						Underline = true;
						e = s + 1;
					}
					break;
				}
				case '\t':
				{
					GDisplayString ds(Font, e + 1);
					x = Width - ds.X() - 8;
					e = s + 1;
					break;
				}
				default:
				{
					if (Underline)
					{
						LgiNextUtf8(e);
					}
					else
					{
						for (e = s; *e; e++)
						{
							if (*e == '\t') break;
							if (*e == '&') break;
						}
					}

					int Len = e - s;
					if (Len > 0)
					{
						// paint text till that point
						GDisplayString d(Font, s, Len);
						d.Draw(pDC, x, y, 0);

						if (Underline)
						{
							GDisplayString ds(Font, s, 1); 
							int UnderX = ds.X();
							int Ascent = ceil(Font->GetAscent());
							pDC->Colour(Font->Fore(), 24);
							pDC->Line(x, y+Ascent+1, x+max(UnderX-2, 1), y+Ascent+1);
							Underline = false;
						}

						x += d.X();
					}
					break;
				}
			}
		}
	}

}

void GMenuItem::_Paint(GSurface *pDC, int Flags)
{
	bool BaseMenu = Parent == Menu;
	int IconX = BaseMenu ? 5 : 20;
	bool Selected = TestFlag(Flags, ODS_SELECTED);
	bool Disabled = TestFlag(Flags, ODS_DISABLED);
	bool Checked = TestFlag(Flags, ODS_CHECKED);
	
	#ifdef WIN32
	GRect r(0, 0, pDC->X()-1, pDC->Y()-1);
	#else
	GRect r = Info->GetClient();
	#endif
	char *Text = Name();

	if (Separator())
	{
		// Paint a separator
		int Cy = r.Y() / 2;

		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();

		pDC->Colour(LC_LOW, 24);
		pDC->Line(0, Cy-1, pDC->X()-1, Cy-1);
		
		pDC->Colour(LC_LIGHT, 24);
		pDC->Line(0, Cy, pDC->X()-1, Cy);
	}
	else
	{
		// Paint a text menu item
		COLOUR Fore = LC_TEXT; // Selected ? LC_SEL_TEXT : LC_TEXT;
		COLOUR Back = Selected ? LC_HIGH : LC_MED; // Selected ? LC_SELECTION : LC_MED;
		int x = IconX;
		int y = 1;

		// For a submenu
		pDC->Colour(Back, 24);
		pDC->Rectangle(&r);

		// Draw the text on top
		GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
		Font->Transparent(true);
		if (Disabled)
		{
			// Disabled text
			if (!Selected)
			{
				Font->Colour(LC_LIGHT, 0);
				_PaintText(pDC, x+1, y+1, r.X());
			}
			// Else selected... don't draw the hilight

			// "greyed" text...
			Font->Colour(LC_LOW, 0);
			_PaintText(pDC, x, y, r.X());
		}
		else
		{
			// Normal coloured text
			Font->Colour(Fore, 0);
			_PaintText(pDC, x, y, r.X());
		}

		GImageList *ImgLst = (Menu AND Menu->GetImageList()) ? Menu->GetImageList() : Parent ? Parent->GetImageList() : 0;

		// Draw icon/check mark
		if (Checked AND IconX > 0)
		{
			// it's a check!
			int x = 4;
			int y = 6;
			
			pDC->Colour(Fore, 24);
			pDC->Line(x, y, x+2, y+2);
			pDC->Line(x+2, y+2, x+6, y-2);
			y++;
			pDC->Line(x, y, x+2, y+2);
			pDC->Line(x+2, y+2, x+6, y-2);
			y++;
			pDC->Line(x, y, x+2, y+2);
			pDC->Line(x+2, y+2, x+6, y-2);
		}
		else if (ImgLst AND
				_Icon >= 0)
		{
			// it's an icon!
			ImgLst->Draw(pDC, 0, 0, _Icon);
		}

		// Sub menu arrow
		if (Child AND !dynamic_cast<GMenu*>(Parent))
		{
			pDC->Colour(LC_TEXT, 24);

			int x = r.x2 - 4;
			int y = r.y1 + (r.Y()/2);
			for (int i=0; i<4; i++)
			{
				pDC->Line(x, y-i, x, y+i);
				x--;
			}
		}
	}
}

bool GMenuItem::ScanForAccel()
{
	char *n = GObject::Name();
	if (n AND Menu)
	{
		char *Tab = strchr(n, '\t');
		if (Tab)
		{
			GToken Keys(++Tab, "+-");
			if (Keys.Length() > 0)
			{
				int Flags = 0;
				char16 Key = 0;
				
				for (int i=0; i<Keys.Length(); i++)
				{
					char *k = Keys[i];
					if (stricmp(k, "Ctrl") == 0)
					{
						Flags |= LGI_EF_CTRL;
					}
					else if (stricmp(k, "Alt") == 0)
					{
						Flags |= LGI_EF_ALT;
					}
					else if (stricmp(k, "Shift") == 0)
					{
						Flags |= LGI_EF_SHIFT;
					}
					else if (stricmp(k, "Del") == 0 OR
							 stricmp(k, "Delete") == 0)
					{
						Key = VK_DELETE;
					}
					else if (stricmp(k, "Ins") == 0 OR
							 stricmp(k, "Insert") == 0)
					{
						Key = VK_INSERT;
					}
					else if (stricmp(k, "Home") == 0)
					{
						Key = VK_HOME;
					}
					else if (stricmp(k, "End") == 0)
					{
						Key = VK_END;
					}
					else if (stricmp(k, "PageUp") == 0)
					{
						Key = VK_PAGEUP;
					}
					else if (stricmp(k, "PageDown") == 0)
					{
						Key = VK_PAGEDOWN;
					}
					else if (stricmp(k, "Backspace") == 0)
					{
						Key = VK_BACKSPACE;
					}
					else if (stricmp(k, "Space") == 0)
					{
						Key = ' ';
					}
					else if (k[0] == 'F' AND isdigit(k[1]))
					{
						Key = VK_F1 + atoi(k+1) - 1;
					}
					else if (isalpha(k[0]))
					{
						Key = toupper(k[0]);
					}
					else if (isdigit(k[0]))
					{
						Key = k[0];
					}
				}
				
				if (Key)
				{
					// printf("\tFlags=%x Key=%c(%i)\n", Flags, Key, Key);
					Menu->Accel.Insert( new GAccelerator(Flags, Key, Id()) );
				}
				else
				{
					printf("Accel scan failed, str='%s'\n", Tab);
				}
			}
		}
	}

	return false;
}

GSubMenu *GMenuItem::GetParent()
{
	return Parent;
}

bool GMenuItem::Remove()
{
	if (Parent)
	{
		Parent->Items.Delete(this);
		return true;
	}

	return false;
}

void GMenuItem::Id(int i)
{
	_Id = i;
}

void GMenuItem::Separator(bool s)
{
	if (s)
	{
		_Id = -2;
	}
}

void GMenuItem::Checked(bool c)
{
	_Check = c;
}

bool GMenuItem::Name(char *n)
{
	return GObject::Name(n);
}

void GMenuItem::Enabled(bool e)
{
	_Enabled = e;
}

void GMenuItem::Focus(bool f)
{
}

void GMenuItem::Sub(GSubMenu *s)
{
	Child = s;
}

void GMenuItem::Icon(int i)
{
	_Icon = i;
}

void GMenuItem::Visible(bool i)
{
}

int GMenuItem::Id()
{
	return _Id;
}

char *GMenuItem::Name()
{
	return GObject::Name();
}

bool GMenuItem::Separator()
{
	return _Id == -2;
}

bool GMenuItem::Checked()
{
	return _Check;
}

bool GMenuItem::Enabled()
{
	return _Enabled;
}

bool GMenuItem::Visible()
{
	return true;
}

bool GMenuItem::Focus()
{
	return 0;
}

GSubMenu *GMenuItem::Sub()
{
	return Child;
}

int GMenuItem::Icon()
{
	return _Icon;
}

///////////////////////////////////////////////////////////////////////////////////////////////
GFont *GMenu::_Font = 0;

GMenu::GMenu() : GSubMenu("", false)
{
	Menu = this;
}

GMenu::~GMenu()
{
	Accel.DeleteObjects();
}

GFont *GMenu::GetFont()
{
	if (!_Font)
	{
		GFontType Type;
		if (Type.GetSystemFont("Menu"))
		{
			_Font = Type.Create();
			if (_Font)
			{
				// _Font->CodePage(SysFont->CodePage());
			}
			else
			{
				printf("GMenu::GetFont Couldn't create menu font.\n");
			}
		}
		else
		{
			printf("GMenu::GetFont Couldn't get menu typeface.\n");
		}

		if (!_Font)
		{
			_Font = new GFont;
			if (_Font)
			{
				*_Font = *SysFont;
			}
		}
	}

	return _Font ? _Font : SysFont;
}

bool GMenu::Attach(GViewI *p)
{
	bool Status = false;

	GWindow *w = dynamic_cast<GWindow*>(p);
	if (w)
	{
		Window = p;
		Info = new MenuImpl(this);
		if (Info AND
			Info->View())
		{
			Status = Info->View()->Attach(p);
		}
	}

	return Status;
}

bool GMenu::Detach()
{
	bool Status = false;
	return Status;
}

bool GMenu::OnKey(GView *v, GKey &k)
{
	if (k.Down())
	{
		for (GAccelerator *a = Accel.First(); a; a = Accel.Next())
		{
			if (a->Match(k))
			{
				// printf("Matched accel\n");
				Window->OnCommand(a->GetId(), 0, 0);
				return true;
			}
		}
		
		if (k.Alt() &&
			!dynamic_cast<GMenuItem*>(v) &&
			!dynamic_cast<GSubMenu*>(v))
		{
			bool Hide = false;
			
			for (GMenuItem *s=Items.First(); s; s=Items.Next())
			{
				if (!s->Separator())
				{
					if (Hide)
					{
						s->Info->HideSub();
					}
					else
					{
						char *n = s->Name();
						if (ValidStr(n))
						{
							char *Amp = strchr(n, '&');
							while (Amp AND Amp[1] == '&')
							{
								Amp = strchr(Amp + 2, '&');
							}

							char16 Accel = tolower(Amp[1]);
							char16 Press = tolower(k.c16);
							if (Accel == Press)
							{
								Hide = true;
							}
						}
						
						if (Hide)
						{
							s->Info->ShowSub();
						}
						else
						{
							s->Info->HideSub();
						}
					}
				}
			}
			
			if (Hide)
			{
				return true;
			}
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////
GAccelerator::GAccelerator(int flags, int key, int id)
{
	Flags = flags;
	Key = key;
	Id = id;
}

bool GAccelerator::Match(GKey &k)
{
	int Press = (uint) k.vkey;
	
	if (k.vkey == VK_RSHIFT OR
		k.vkey == VK_LSHIFT OR
		k.vkey == VK_RCTRL OR
		k.vkey == VK_LCTRL OR
		k.vkey == VK_RALT OR
		k.vkey == VK_RALT)
	{
		return false;
	}
	
	#if 0
	printf("GAccelerator::Match %i(%c)%s%s%s = %i(%c)%s%s%s\n",
		Press,
		Press>=' '?Press:'.',
		k.Ctrl()?" ctrl":"",
		k.Alt()?" alt":"",
		k.Shift()?" shift":"",
		Key,
		Key>=' '?Key:'.',
		TestFlag(Flags, LGI_EF_CTRL)?" ctrl":"",
		TestFlag(Flags, LGI_EF_ALT)?" alt":"",
		TestFlag(Flags, LGI_EF_SHIFT)?" shift":""		
		);
	#endif

	if (toupper(Press) == (uint)Key)
	{
		if
		(
			(TestFlag(Flags, LGI_EF_CTRL) ^ k.Ctrl() == 0) AND
			(TestFlag(Flags, LGI_EF_ALT) ^ k.Alt() == 0) AND
			(TestFlag(Flags, LGI_EF_SHIFT) ^ k.Shift() == 0)
		)				
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////
GCommand::GCommand()
{
	Flags = GWF_VISIBLE;
	Id = 0;
	ToolButton = 0;
	MenuItem = 0;
	Accelerator = 0;
	TipHelp = 0;
	PrevValue = false;
}

GCommand::~GCommand()
{
	DeleteArray(Accelerator);
	DeleteArray(TipHelp);
}

bool GCommand::Enabled()
{
	if (ToolButton)
		return ToolButton->Enabled();
	if (MenuItem)
		return MenuItem->Enabled();
	return false;
}

void GCommand::Enabled(bool e)
{
	if (ToolButton)
	{
		ToolButton->Enabled(e);
	}
	if (MenuItem)
	{
		MenuItem->Enabled(e);
	}
}

bool GCommand::Value()
{
	bool HasChanged = false;

	if (ToolButton)
	{
		HasChanged |= (ToolButton->Value() != 0) ^ PrevValue;
	}
	if (MenuItem)
	{
		HasChanged |= (MenuItem->Checked() != 0) ^ PrevValue;
	}

	if (HasChanged)
	{
		Value(!PrevValue);
	}

	return PrevValue;
}

void GCommand::Value(bool v)
{
	if (ToolButton)
	{
		ToolButton->Value(v);
	}
	if (MenuItem)
	{
		MenuItem->Checked(v);
	}
	PrevValue = v;
}
