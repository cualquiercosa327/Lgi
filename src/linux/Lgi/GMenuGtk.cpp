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
#include "GToken.h"
#include "GDisplayString.h"

using namespace Gtk;
typedef ::GMenuItem LgiMenuItem;

///////////////////////////////////////////////////////////////////////////////////////////////
GSubMenu::GSubMenu(const char *name, bool Popup)
{
	Menu = NULL;
	Parent = NULL;
	Info = NULL;
	_ContextMenuId = NULL;
	
	if (name)
	{
		Info = GtkCast(Gtk::gtk_menu_new(), gtk_menu_shell, GtkMenuShell);
	}
}

GSubMenu::~GSubMenu()
{
	while (Items.Length())
	{
		LgiMenuItem *i = Items.First();
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

size_t GSubMenu::Length()
{
	return Items.Length();
}

LgiMenuItem *GSubMenu::ItemAt(int Id)
{
	return Items.ItemAt(Id);
}

LgiMenuItem *GSubMenu::AppendItem(const char *Str, int Id, bool Enabled, int Where, const char *Shortcut)
{
	LgiMenuItem *i = new LgiMenuItem(Menu, this, Str, Where < 0 ? Items.Length() : Where, Shortcut);
	if (i)
	{
		i->Id(Id);
		i->Enabled(Enabled);

		Items.Insert(i, Where);

		GtkWidget *item = GtkCast(i->Handle(), gtk_widget, GtkWidget);
		LgiAssert(item);
		if (item)
		{
			gtk_menu_shell_append(Info, item);
			gtk_widget_show(item);
		}

		return i;
	}

	return 0;
}

LgiMenuItem *GSubMenu::AppendSeparator(int Where)
{
	LgiMenuItem *i = new LgiMenuItem;
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
	LgiMenuItem *i = new LgiMenuItem(Menu, this, Str, Where < 0 ? Items.Length() : Where, NULL);
	if (i)
	{
		i->Id(-1);
		Items.Insert(i, Where);

		Gtk::GtkWidget *item = GtkCast(i->Handle(), gtk_widget, GtkWidget);
		LgiAssert(item);
		if (item)
		{
			if (Where < 0)
				Gtk::gtk_menu_shell_append(Info, item);
			else
				Gtk::gtk_menu_shell_insert(Info, item, Where);
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

void GSubMenu::ClearHandle()
{
	Info = NULL;
	for (LgiMenuItem *i = Items.First(); i; i = Items.Next())
	{
		i->ClearHandle();
	}
}

void GSubMenu::Empty()
{
	LgiMenuItem *i;
	while (i = Items.First())
	{
		RemoveItem(i);
		DeleteObj(i);
	}
}

bool GSubMenu::RemoveItem(int i)
{
	return RemoveItem(Items.ItemAt(i));
}

bool GSubMenu::RemoveItem(LgiMenuItem *Item)
{
	if (Item && Items.HasItem(Item))
	{
		return Item->Remove();
	}

	return false;
}

bool GSubMenu::IsContext(LgiMenuItem *Item)
{
	if (!_ContextMenuId)
	{
		LgiMenuItem *i = GetParent();
		GSubMenu *s = i ? i->GetParent() : NULL;
		if (s)
			// Walk up the chain of menus to find the top...
			return s->IsContext(Item);
		else
			// Ok we got to the top
			return false;
	}
	
	*_ContextMenuId = Item->Id();
	Gtk::gtk_main_quit();
	return true;
}

void GSubMenuDeactivate(Gtk::GtkMenuShell *widget, GSubMenu *Sub)
{
	Sub->OnDeactivate();
}

void GSubMenu::OnDeactivate()
{
	if (_ContextMenuId)
		*_ContextMenuId = 0;
	Gtk::gtk_main_quit();
}
                                                         
int GSubMenu::Float(GView *From, int x, int y, int Button)
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
	
	if (From->IsCapturing())
		From->Capture(false);

	// This signal handles the case where the user cancels the menu by clicking away from it.
	Gtk::g_signal_connect_data(GtkCast(Info, g_object, GObject), 
						"deactivate", 
						(Gtk::GCallback)GSubMenuDeactivate,
						this, NULL, (Gtk::GConnectFlags) 0);
	
	Gtk::gtk_widget_show_all(GtkCast(Info, gtk_widget, GtkWidget));

	int MenuId = 0;
	_ContextMenuId = &MenuId;

	GdcPt2 Pos(x, y);
	Gtk::gtk_menu_popup(GtkCast(Info, gtk_menu, GtkMenu),
						NULL, NULL, NULL, NULL,
						Button,
						Gtk::gtk_get_current_event_time());

	// In the case where there is no mouse button down, the popup menu can fail to 
	// show. If that happens and we enter the gtk_main loop then the application will
	// be a bad state. No GSubMenuDeactivate event will get called to exit the float
	// loop. There may be a better way to do this.
	Gtk::GdkScreen *screen = NULL;
	Gtk::gint mx, my;
	Gtk::GdkModifierType mask;
	Gtk::gdk_display_get_pointer(Gtk::gdk_display_get_default(),
								&screen,
								&mx,
								&my,
								&mask);
	if
	(
		Button == 0
		||
		(
			mask & (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK|GDK_BUTTON4_MASK|GDK_BUTTON5_MASK)
		)
	)
		Gtk::gtk_main();
	else
		LgiTrace("%s:%i - Popup loop avoided, no button down?\n", _FL);

	_ContextMenuId = NULL;
	return MenuId;
}

GSubMenu *GSubMenu::FindSubMenu(int Id)
{
	for (LgiMenuItem *i = Items.First(); i; i = Items.Next())
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

LgiMenuItem *GSubMenu::FindItem(int Id)
{
	for (LgiMenuItem *i = Items.First(); i; i = Items.Next())
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
		if (*in == '_')
		{
			*out++ = '_';
			*out++ = '_';
		}
		else if (*in != '&' || in[1] == '&')
		{
			*out++ = *in;
		}
		else
		{
			*out++ = '_';
		}
		
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

static void MenuItemCallback(LgiMenuItem *Item)
{
	if (!Item->Sub() && !Item->InSetCheck)
	{
		GSubMenu *Parent = Item->GetParent();
		
		if (!Parent || !Parent->IsContext(Item))
		{
			::GMenu *m = Item->GetMenu();
			if (m)
			{
				// Attached to a mean, so send an event to the window
				GViewI *w = m->WindowHandle();
				if (w)
					w->PostEvent(M_COMMAND, Item->Id());
				else
					LgiAssert(!"No window for menu to send to");
			}
			else
			{
				// Could be just a popup menu... in which case do nothing.				
			}
		}
	}
}

LgiMenuItem::GMenuItem()
{
	Info = GtkCast(Gtk::gtk_separator_menu_item_new(), gtk_menu_item, GtkMenuItem);
	Child = NULL;
	Menu = NULL;
	Parent = NULL;
	InSetCheck = false;
	
	Position = -1;
	
	_Flags = 0;	
	_Icon = -1;
	_Id = 0;
}

LgiMenuItem::GMenuItem(::GMenu *m, GSubMenu *p, const char *txt, int Pos, const char *shortcut)
{
	GAutoString Txt = MenuItemParse(txt);
	GBase::Name(txt);
	Info = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(Txt));
	
	Gtk::gulong ret = Gtk::g_signal_connect_data(Info,
												"activate",
												(Gtk::GCallback) MenuItemCallback,
												this,
												NULL,
												Gtk::G_CONNECT_SWAPPED);

	Child = NULL;
	Menu = m;
	Parent = p;
	InSetCheck = false;

	Position = Pos;

	_Flags = 0;	
	_Icon = -1;
	_Id = 0;

	ShortCut = shortcut;
	ScanForAccel();
}

LgiMenuItem::~GMenuItem()
{
	if (Info)
		Remove();
	DeleteObj(Child);
}

#if GtkVer(2, 24)
#include <gdk/gdkkeysyms-compat.h>
#endif

#ifndef GDK_F1
enum {
	GDK_F1 = 0xffbe,
	GDK_F2,
	GDK_F3,
	GDK_F4,
	GDK_F5,
	GDK_F6,
	GDK_F7,
	GDK_F8,
	GDK_F9,
	GDK_F10,
	GDK_F11,
	GDK_F12
};
#endif

Gtk::gint LgiKeyToGtkKey(int Key, const char *ShortCut)
{
	#ifdef GDK_a
	LgiAssert(GDK_a == 'a');
	LgiAssert(GDK_A == 'A');
	#endif
	
	/*
	if (Key >= 'a' && Key <= 'z')
		return Key;
	*/
	if (Key >= 'A' && Key <= 'Z')
		return Key;
	if (Key >= '0' && Key <= '9')
		return Key;
	
	switch (Key)
	{
		#ifdef GDK_Delete
		case VK_DELETE: return GDK_Delete;
		#endif
		#ifdef GDK_Insert
		case VK_INSERT: return GDK_Insert;
		#endif
		#ifdef GDK_Home
		case VK_HOME: return GDK_Home;
		#endif
		#ifdef GDK_End
		case VK_END: return GDK_End;
		#endif
		#ifdef GDK_Page_Up
		case VK_PAGEUP: return GDK_Page_Up;
		#endif
		#ifdef GDK_Page_Down
		case VK_PAGEDOWN: return GDK_Page_Down;
		#endif
		#ifdef GDK_BackSpace
		case VK_BACKSPACE: return GDK_BackSpace;
		#endif
		#ifdef GDK_Escape
		case VK_ESCAPE: return GDK_Escape;
		#else
		#warning "GDK_Escape not defined."
		#endif

		case VK_UP:
			return GDK_Up;
		case VK_DOWN:
			return GDK_Down;
		case VK_LEFT:
			return GDK_Left;
		case VK_RIGHT:
			return GDK_Right;

		case VK_F1:
			return GDK_F1;
		case VK_F2:
			return GDK_F2;
		case VK_F3:
			return GDK_F3;
		case VK_F4:
			return GDK_F4;
		case VK_F5:
			return GDK_F5;
		case VK_F6:
			return GDK_F6;
		case VK_F7:
			return GDK_F7;
		case VK_F8:
			return GDK_F8;
		case VK_F9:
			return GDK_F9;
		case VK_F10:
			return GDK_F10;
		case VK_F11:
			return GDK_F11;
		case VK_F12:
			return GDK_F12;
		case ' ':
			#ifdef GDK_space
			return GDK_space;
			#else
			return ' ';
			#endif
		default:
			LgiTrace("Unhandled menu accelerator: 0x%x (%s)\n", Key, ShortCut);
			break;
	}
	
	return 0;
}

bool LgiMenuItem::ScanForAccel()
{
	if (!Menu)
		return false;

	const char *Sc = ShortCut;
	if (!Sc)
	{
		char *n = GBase::Name();
		if (n)
		{
			char *Tab = strchr(n, '\t');
			if (Tab)
				Sc = Tab + 1;
		}
	}

	if (!Sc)
		return false;

	GToken Keys(Sc, "+-");
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
			else if (stricmp(k, "Left") == 0)
			{
				Key = VK_LEFT;
			}
			else if (stricmp(k, "Up") == 0)
			{
				Key = VK_UP;
			}
			else if (stricmp(k, "Right") == 0)
			{
				Key = VK_RIGHT;
			}
			else if (stricmp(k, "Down") == 0)
			{
				Key = VK_DOWN;
			}
			else if (!stricmp(k, "Esc") || !stricmp(k, "Escape"))
			{
				Key = VK_ESCAPE;
			}
			else if (stricmp(k, "Space") == 0)
			{
				Key = ' ';
			}
			else if (k[0] == 'F' && isdigit(k[1]))
			{
				int Idx = atoi(k+1);
				Key = VK_F1 + Idx - 1;
			}
			else if (isalpha(k[0]))
			{
				Key = toupper(k[0]);
			}
			else if (isdigit(k[0]))
			{
				Key = k[0];
			}
			else
			{
				printf("%s:%i - Unknown shortcut part '%s'\n", _FL, k);
			}
		}
		
		if (Key)
		{
			Gtk::gint GtkKey = LgiKeyToGtkKey(Key, Sc);
			if (GtkKey)
			{
				GtkWidget *w = GtkCast(Info, gtk_widget, GtkWidget);
				Gtk::GdkModifierType mod = (Gtk::GdkModifierType)
					(
						(TestFlag(Flags, LGI_EF_CTRL)  ? Gtk::GDK_CONTROL_MASK : 0) |
						(TestFlag(Flags, LGI_EF_SHIFT) ? Gtk::GDK_SHIFT_MASK : 0) |
						(TestFlag(Flags, LGI_EF_ALT)   ? Gtk::GDK_MOD1_MASK : 0)
					);

				const char *Signal = "activate";
	
				gtk_widget_add_accelerator(	w,
											Signal,
											Menu->AccelGrp,
											GtkKey,
											mod,
											Gtk::GTK_ACCEL_VISIBLE
										);
				gtk_widget_show_all(w);
			}
			else
			{
				printf("%s:%i - No gtk key for '%s'\n", _FL, Sc);
			}
			
			Menu->Accel.Insert( new GAccelerator(Flags, Key, Id()) );
		}
		else
		{
			printf("%s:%i - Accel scan failed, str='%s'\n", _FL, Sc);
			return false;
		}
	}

	return true;
}

GSubMenu *LgiMenuItem::GetParent()
{
	return Parent;
}

void LgiMenuItem::ClearHandle()
{
	Info = NULL;
	if (Child)
		Child->ClearHandle();
}

bool LgiMenuItem::Remove()
{
	if (!Parent)
	{
		return false;
	}

	if (Info)
	{
		LgiAssert(Info->item.bin.container.widget.object.parent_instance.g_type_instance.g_class);
		Gtk::GtkWidget *w = GtkCast(Info, gtk_widget, GtkWidget);
		if (Gtk::gtk_widget_get_parent(w))
		{		
			Gtk::GtkContainer *c = GtkCast(Parent->Info, gtk_container, GtkContainer);
			Gtk::gtk_container_remove(c, w);
		}
	}

	Parent->Items.Delete(this);
	Parent = NULL;
	return true;
}

void LgiMenuItem::Id(int i)
{
	_Id = i;
}

void LgiMenuItem::Separator(bool s)
{
	if (s)
	{
		_Id = -2;
	}
}

struct MenuItemIndex
{
	Gtk::GtkWidget *w;
	int Current;
	int Index;
	
	MenuItemIndex()
	{
		Index = -1;
		Current = 0;
		w = NULL;
	}
};

static void
FindMenuItemIndex(Gtk::GtkWidget *w, Gtk::gpointer data)
{
	MenuItemIndex *d = (MenuItemIndex*)data;

	printf("w=%p d->w=%p name=%s cur=%i d->Index=%i\n", w, d->w, G_OBJECT_TYPE_NAME(w), d->Current, d->Index);
	if (w == d->w)
		d->Index = d->Current;
	d->Current++;
}

int
gtk_container_get_child_index(GtkContainer *c, GtkWidget *child)
{
	MenuItemIndex Idx;
	if (c && child)
	{
		Idx.w = child;
		gtk_container_foreach(c, FindMenuItemIndex, &Idx);
	}
	return Idx.Index;
}

bool LgiMenuItem::Replace(Gtk::GtkWidget *newWid)
{
	if (!newWid || !Info)
	{
		LgiTrace("%s:%i - Error: New=%p Old=%p\n", newWid, Info);
		return false;
	}

	// Get widget
	GtkWidget *w = GTK_WIDGET(Info);
	
	// Is is attach already?
	if (gtk_widget_get_parent(w))
	{
		// Yes!
		GtkContainer *c = GTK_CONTAINER(Parent->Info);
		if (c)
		{			
			// Find index
			int PIdx = Parent->Items.IndexOf(this);
			LgiAssert(PIdx >= 0);
		
			// Remove old item
			gtk_container_remove(c, w);
		
			// Add new item
			gtk_menu_shell_insert(Parent->Info, newWid, PIdx);
			gtk_widget_show(newWid);
		}
		else LgiTrace("%s:%i - GTK_CONTAINER failed.\n", _FL);
	}
	else
	{
		// Delete it
		g_object_unref(Info);
	}
	
	Info = GTK_MENU_ITEM(newWid);
	return Info != NULL;
}

void LgiMenuItem::Icon(int i)
{
	_Icon = i;
	
	if (Info && GetMenu())
	{
		// Is the item a image menu item?
		if (!GTK_IS_IMAGE_MENU_ITEM(Info) && _Icon >= 0)
		{
			// Create a image menu item
			GAutoString Txt = MenuItemParse(Name());
			GtkWidget *w = gtk_image_menu_item_new_with_mnemonic(Txt);

			// Attach our signal
			gulong ret = g_signal_connect_data(	w,
												"activate",
												(GCallback) MenuItemCallback,
												this,
												NULL,
												G_CONNECT_SWAPPED);

			// Replace the existing menu item with this new one
			if (w)
				Replace(w);
			else
				LgiAssert(!"No new widget.");
		}
		
		if (_Icon >= 0 && GTK_IS_IMAGE_MENU_ITEM(Info))
		{
			GImageList *lst = GetMenu()->GetImageList();
			GtkImageMenuItem *imi = GTK_IMAGE_MENU_ITEM(Info);
			if (!lst)
			{
				LgiTrace("%s:%i - No image list to create icon with.\n", _FL);
			}
			else if (!imi)
			{
				LgiTrace("%s:%i - No image menu item to set icon.\n", _FL);
			}
			else
			{
				// Attempt to read the background colour from the theme settings...
				// Default back to LC_MED if we can't get it.
				GColour Back;				
				GdkColor colour;
				GtkStyle *style = gtk_rc_get_style(GTK_WIDGET(Info));
				if (gtk_style_lookup_color(style, "bg_color", &colour))
					Back.Rgb(colour.red>>8, colour.green>>8, colour.blue>>8);
				else
					Back.Set(LC_MED, 24);
    
   				// Create sub-image of the icon
				if (!IconImg.Reset(new GMemDC(lst->TileX(), lst->TileY(), System32BitColourSpace)))
				{
					LgiTrace("%s:%i - Couldn't create icon image.\n", _FL);
					return;
				}
				
				// Init to blank, then blt the pixels across...
				IconImg->Colour(Back);
				IconImg->Rectangle();
				GRect r(0, 0, lst->TileX()-1, lst->TileY()-1);
				r.Offset(lst->TileX() * _Icon, 0);
				IconImg->Op(GDC_ALPHA);
				IconImg->Blt(0, 0, lst, &r);
				
				// Get the sub-image of the icon
				GdkImage *img = IconImg->GetImage();
				if (!img)
				{
					LgiTrace("%s:%i - GetImage failed.\n", _FL);
					return;
				}
				
				// Create a new widget to wrap it...
				GtkWidget *img_wid = gtk_image_new_from_image(img, NULL);
				if (!img_wid)
				{
					LgiTrace("%s:%i - gtk_image_new_from_image failed.\n", _FL);
					return;
				}
				gtk_widget_show(img_wid);

				// Set the menu item's image as that icon widget
				gtk_image_menu_item_set_always_show_image(imi, true);
				gtk_image_menu_item_set_image(imi, img_wid);
				
				// printf("Setting '%s' to img %p (%i)\n", Name(), img_wid, _Icon);
				
				ScanForAccel();
			}
		}
	}
}

void LgiMenuItem::Checked(bool c)
{
	if (c)
		SetFlag(_Flags, ODS_CHECKED);
	else
		ClearFlag(_Flags, ODS_CHECKED);

	if (Info)
	{
		InSetCheck = true;
		
		// Is the item a checked menu item?
		if (!GTK_IS_CHECK_MENU_ITEM(Info) && c)
		{
			// Create a checkable menu item...
			GAutoString Txt = MenuItemParse(Name());
			GtkWidget *w = gtk_check_menu_item_new_with_mnemonic(Txt);

			// Attach our signal
			gulong ret = g_signal_connect_data(	w,
												"activate",
												(GCallback) MenuItemCallback,
												this,
												NULL,
												G_CONNECT_SWAPPED);

			// Replace the existing menu item with this new one
			if (w)
				Replace(w);
			else
				LgiAssert(!"No new widget.");
		}
		
		if (GTK_IS_CHECK_MENU_ITEM(Info))
		{
			// Now mark is checked
			GtkCheckMenuItem *chk = GTK_CHECK_MENU_ITEM(Info);
			if (chk)
			{
				Gtk::gtk_check_menu_item_set_active(chk, c);
			}
		}
		
		InSetCheck = false;
	}
}

bool LgiMenuItem::Name(const char *n)
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

void LgiMenuItem::Enabled(bool e)
{
	if (e)
		ClearFlag(_Flags, ODS_DISABLED);
	else
		SetFlag(_Flags, ODS_DISABLED);
	
	if (Info)
	{
		gtk_widget_set_sensitive(GtkCast(Info, gtk_widget, GtkWidget), e);
 	}
}

void LgiMenuItem::Focus(bool f)
{
}

void LgiMenuItem::Sub(GSubMenu *s)
{
	Child = s;
}

void LgiMenuItem::Visible(bool i)
{
}

int LgiMenuItem::Id()
{
	return _Id;
}

char *LgiMenuItem::Name()
{
	return GBase::Name();
}

bool LgiMenuItem::Separator()
{
	return _Id == -2;
}

bool LgiMenuItem::Checked()
{
	return TestFlag(_Flags, ODS_CHECKED);
}

bool LgiMenuItem::Enabled()
{
	return !TestFlag(_Flags, ODS_DISABLED);
}

bool LgiMenuItem::Visible()
{
	return true;
}

bool LgiMenuItem::Focus()
{
	return 0;
}

GSubMenu *LgiMenuItem::Sub()
{
	return Child;
}

int LgiMenuItem::Icon()
{
	return _Icon;
}

///////////////////////////////////////////////////////////////////////////////////////////////
GFont *::GMenu::_Font = 0;

class GMenuPrivate
{
public:
};

::GMenu::GMenu(const char *AppName) : GSubMenu("", false)
{
	Menu = this;
	d = NULL;
	AccelGrp = Gtk::gtk_accel_group_new();
	Info = GtkCast(Gtk::gtk_menu_bar_new(), gtk_menu_shell, GtkMenuShell);
}

::GMenu::~GMenu()
{
	Accel.DeleteObjects();
}

GFont *::GMenu::GetFont()
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

bool ::GMenu::Attach(GViewI *p)
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

	g_object_ref(Wnd->_Root);
	gtk_container_remove(wndcontainer, Wnd->_Root);

	gtk_container_add(wndcontainer, Wnd->_VBox);

	gtk_box_pack_start(vbox, menubar, false, false, 0);
	gtk_box_pack_end(vbox, Wnd->_Root, true, true, 0);
	g_object_unref(Wnd->_Root);

	gtk_widget_show_all(GtkCast(Wnd->Wnd, gtk_widget, GtkWidget));
	
	gtk_window_add_accel_group(Wnd->Wnd, AccelGrp);
	
	return true;
}

bool ::GMenu::Detach()
{
	bool Status = false;
	return Status;
}

bool ::GMenu::SetPrefAndAboutItems(int a, int b)
{
	return false;
}

bool ::GMenu::OnKey(GView *v, GKey &k)
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
			!dynamic_cast<LgiMenuItem*>(v) &&
			!dynamic_cast<GSubMenu*>(v))
		{
			bool Hide = false;
			
			for (LgiMenuItem *s=Items.First(); s; s=Items.Next())
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
							while (Amp && Amp[1] == '&')
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
			(TestFlag(Flags, LGI_EF_CTRL) ^ k.Ctrl() == 0) &&
			(TestFlag(Flags, LGI_EF_ALT) ^ k.Alt() == 0) &&
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
