/*hdr
**      FILE:           GMenuGtk.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           19/4/2013
**      DESCRIPTION:    Gtk menus
**
**      Copyright (C) 2013, Matthew Allen
**              fret@memecode.com
*/
#include <math.h>

#include "Lgi.h"
#include "GUtf8.h"
#include "GToken.h"

#if 1 // && defined __GTK_H__

///////////////////////////////////////////////////////////////////////////////////////////////
GSubMenu::GSubMenu(const char *name, bool Popup)
{
	Menu = NULL;
	Parent = NULL;
	Info = NULL;
	
	if (name)
	{
		Info = GtkCast(Gtk::gtk_menu_new(), gtk_menu_shell, GtkMenuShell);
	}
}

GSubMenu::~GSubMenu()
{
	while (Items.Length())
	{
		GMenuItem *i = Items.First();
		LgiAssert(i->Parent == this);
		DeleteObj(i);
	}
	
	if (Info)
	{
		// bool IsMenu = Menu == this;
		LgiAssert(Info->container.widget.object.parent_instance.g_type_instance.g_class);
		Gtk::GtkWidget *w = GtkCast(Info, gtk_widget, GtkWidget);
		// LgiTrace("%p::~GSubMenu w=%p name=%s IsMenu=%i\n", this, w, Name(), IsMenu);
		Gtk::gtk_widget_destroy(w);
		Info = NULL;
	}
}

int GSubMenu::Length()
{
	return Items.Length();
}

GMenuItem *GSubMenu::ItemAt(int Id)
{
	return Items.ItemAt(Id);
}

GMenuItem *GSubMenu::AppendItem(const char *Str, int Id, bool Enabled, int Where, const char *Shortcut)
{
	GMenuItem *i = new GMenuItem(Menu, this, Str, Where < 0 ? Items.Length() : Where, Shortcut);
	if (i)
	{
		i->Id(Id);
		i->Enabled(Enabled);
		i->ScanForAccel();

		Items.Insert(i, Where);

		Gtk::GtkWidget *item = GtkCast(i->Handle(), gtk_widget, GtkWidget);
		LgiAssert(item);
		if (item)
		{
			Gtk::gtk_menu_shell_append(Info, item);
			Gtk::gtk_widget_show(item);
		}

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

		Gtk::GtkWidget *item = GtkCast(i->Handle(), gtk_widget, GtkWidget);
		LgiAssert(item);
		if (item)
		{
			Gtk::gtk_menu_shell_append(Info, item);
			Gtk::gtk_widget_show(item);
		}
		
		return i;
	}

	return 0;
}

GSubMenu *GSubMenu::AppendSub(const char *Str, int Where)
{
	GBase::Name(Str);
	GMenuItem *i = new GMenuItem(Menu, this, Str, Where < 0 ? Items.Length() : Where, NULL);
	if (i)
	{
		i->Id(-1);
		Items.Insert(i, Where);

		Gtk::GtkWidget *item = GtkCast(i->Handle(), gtk_widget, GtkWidget);
		LgiAssert(item);
		if (item)
		{
			Gtk::gtk_menu_shell_append(Info, item);
			Gtk::gtk_widget_show(item);
		}

		i->Child = new GSubMenu(Str);
		if (i->Child)
		{
			i->Child->Parent = i;
			i->Child->Menu = Menu;
			i->Child->Window = Window;
			
			Gtk::GtkWidget *sub = GtkCast(i->Child->Handle(), gtk_widget, GtkWidget);
			LgiAssert(sub);
			if (i->Handle() && sub)
			{
				Gtk::gtk_menu_item_set_submenu(i->Handle(), sub);
				Gtk::gtk_widget_show(sub);
			}
			else
				LgiTrace("%s:%i Error: gtk_menu_item_set_submenu(%p,%p) failed\n", _FL, i->Handle(), sub);
		}
				
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
static GAutoString MenuItemParse(const char *s)
{
	char buf[256], *out = buf;
	const char *in = s;
	while (in && *in && out < buf + sizeof(buf) - 1)
	{
		if (*in != '&' || in[1] == '&')
			*out++ = *in;
		in++;
	}
	*out++ = 0;
	
	char *tab = strrchr(buf, '\t');
	if (tab)
	{
		*tab++ = 0;
	}
	
	return GAutoString(NewStr(buf));
}

static void MenuItemCallback(GMenuItem *Item)
{
	if (!Item->Sub())
	{
		GMenu *m = Item->GetMenu();
		if (m)
		{
			GViewI *w = m->WindowHandle();
			if (w)
			{
				w->PostEvent(M_COMMAND, Item->Id());
			}
			else LgiAssert(!"No window for menu to send to");
		}
		else LgiAssert(!"Menuitem not attached to menu");
	}
}

GMenuItem::GMenuItem()
{
	Info = GtkCast(Gtk::gtk_separator_menu_item_new(), gtk_menu_item, GtkMenuItem);
	Child = NULL;
	Menu = NULL;
	Parent = NULL;
	
	Position = -1;
	
	_Icon = -1;
	_Id = 0;
	_Enabled = true;
	_Check = false;
}

GMenuItem::GMenuItem(GMenu *m, GSubMenu *p, const char *txt, int Pos, const char *Shortcut)
{
	GAutoString Txt = MenuItemParse(txt);
	GBase::Name(txt);
	Info = GtkCast(Gtk::gtk_menu_item_new_with_label(Txt), gtk_menu_item, GtkMenuItem);
	Gtk::gulong ret = Gtk::g_signal_connect_data(Info,
												"activate",
												(Gtk::GCallback) MenuItemCallback,
												this,
												NULL,
												Gtk::G_CONNECT_SWAPPED);
	Child = NULL;
	Menu = m;
	Parent = p;

	Position = Pos;

	_Icon = -1;
	_Id = 0;
	_Enabled = true;
	_Check = false;

	ScanForAccel();
}

GMenuItem::~GMenuItem()
{
	if (Info)
		Remove();
	DeleteObj(Child);
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
							int Ascent = ceil(Font->Ascent());
							pDC->Colour(Font->Fore());
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
	
	GRect r(0, 0, pDC->X()-1, pDC->Y()-1);
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
	char *n = GBase::Name();
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
					else if (stricmp(k, "Del") == 0 ||
							 stricmp(k, "Delete") == 0)
					{
						Key = VK_DELETE;
					}
					else if (stricmp(k, "Ins") == 0 ||
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
	if (!Parent)
	{
		LgiAssert(!"No parent to remove menu item from");
		return false;
	}

	if (Info)
	{
		if (Child)
		{
			DeleteObj(Child);
		}

		LgiAssert(Info->item.bin.container.widget.object.parent_instance.g_type_instance.g_class);
		Gtk::GtkWidget *w = GtkCast(Info, gtk_widget, GtkWidget);
		// LgiTrace("%p::~GMenuItem w=%p name=%s\n", this, w, Name());
		Gtk::gtk_widget_destroy(w);
		Info = NULL;
	}

	Parent->Items.Delete(this);
	return true;
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

bool GMenuItem::Name(const char *n)
{
	bool Status = GBase::Name(n);	
	
	#if GtkVer(2, 16)
	LgiAssert(Info);
	GAutoString Txt = MenuItemParse(n);
	ScanForAccel();
	gtk_menu_item_set_label(Info, Txt);
	#else
	LgiTrace("Warning: can't set label after creation.");
	Status = false;
	#endif
	
	return Status;
}

void GMenuItem::Enabled(bool e)
{
	_Enabled = e;
	
	if (Info)
	{
		Gtk::gtk_widget_set_sensitive(GtkCast(Info, gtk_widget, GtkWidget), e);
 	}
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
	return GBase::Name();
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

class GMenuPrivate
{
public:
};

GMenu::GMenu() : GSubMenu("", false)
{
	Menu = this;
	d = NULL;
	Info = GtkCast(Gtk::gtk_menu_bar_new(), gtk_menu_shell, GtkMenuShell);
	int asd=0;
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
	if (!p)
	{
		LgiAssert(0);
		return false;
	}
		
	GWindow *Wnd = p->GetWindow();
	if (!Wnd)
	{
		LgiAssert(0);
		return false;
	}
		
	Window = Wnd;
	Gtk::GtkWidget *Root = NULL;
	if (Wnd->_VBox)
	{
		LgiAssert(!"Already has a menu");
		return false;
	}

	
	Gtk::GtkWidget *menubar = GtkCast(Info, gtk_widget, GtkWidget);

	Wnd->_VBox = Gtk::gtk_vbox_new(false, 0);

	Gtk::GtkBox *vbox = GtkCast(Wnd->_VBox, gtk_box, GtkBox);
	Gtk::GtkContainer *wndcontainer = GtkCast(Wnd->Wnd, gtk_container, GtkContainer);

	Gtk::g_object_ref(Wnd->_Root);
	Gtk::gtk_container_remove(wndcontainer, Wnd->_Root);

	Gtk::gtk_container_add(wndcontainer, Wnd->_VBox);

	Gtk::gtk_box_pack_start(vbox, menubar, false, false, 0);
	Gtk::gtk_box_pack_end(vbox, Wnd->_Root, true, true, 0);
	Gtk::g_object_unref(Wnd->_Root);

	Gtk::gtk_widget_show_all(GtkCast(Wnd->Wnd, gtk_widget, GtkWidget));
	
	return true;
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
						// s->Info->HideSub();
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
							// s->Info->ShowSub();
						}
						else
						{
							// s->Info->HideSub();
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
	
	if (k.vkey == VK_RSHIFT ||
		k.vkey == VK_LSHIFT ||
		k.vkey == VK_RCTRL ||
		k.vkey == VK_LCTRL ||
		k.vkey == VK_RALT ||
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

#endif