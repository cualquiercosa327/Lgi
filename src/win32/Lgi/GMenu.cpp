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
#include <stdlib.h>
#include <ctype.h>

#include "Lgi.h"
#include "GToken.h"

///////////////////////////////////////////////////////////////////////////////////////////////
class GMenuPrivate
{
public:
	COLOUR RootMenuBack;
	
	GMenuPrivate()
	{
		GArray<int> Ver;
		int Os = LgiGetOs(&Ver);
		if
		(
			Os == LGI_OS_WINNT
			&&
			(
				Ver[0] > 5
				||
				(Ver[0] == 5 && Ver[1] > 0)
			)
		)
		{
			#ifndef SPI_GETFLATMENU
			#define SPI_GETFLATMENU 0x1022
			#endif

			BOOL Flat = true;
			SystemParametersInfo(SPI_GETFLATMENU, 0, &Flat, 0);
			if (Flat)
				RootMenuBack = GetSysColor(COLOR_MENUBAR);
			else
				RootMenuBack = GetSysColor(COLOR_MENU);
		}
		else
		{
			RootMenuBack = GetSysColor(COLOR_MENU);
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
GSubMenu::GSubMenu(const char *name, bool Popup)
{
	if (Popup)
	{
		Info = CreatePopupMenu();
	}
	else
	{
		Info = CreateMenu();
	}

	Menu = 0;
	Parent = 0;
	TrackHandle = 0;
	Window = 0;
}

GSubMenu::~GSubMenu()
{
	if (TrackHandle)
	{
		// At least in Win98, if you call InvalidateRect(...) on a window
		// multiple times just after you use TracePopupMenu(...) on that
		// window the invalid region isn't updated.
		//
		// This behaviour is not by design, it is a bug. However this
		// update forces the window to repaint thus working around the
		// bug in windows.
		UpdateWindow(TrackHandle);
		TrackHandle = 0;
	}

	if (Info)
	{
		DestroyMenu(Info);
		Info = 0;
	}

	if (Parent)
	{
		Parent->Child = 0;
	}
	
	GMenuItem *i;
	while (i = Items.First())
	{
		LgiAssert(i->Parent == this);
		DeleteObj(i);
	}
}

GMenuItem *GSubMenu::AppendItem(const char *Str, int Id, bool Enabled, int Where, const char *Shortcut)
{
	GMenuItem *Item = new GMenuItem(Menu, this, Items.Length(), Shortcut);
	if (Item)
	{
		Item->Name(Str);
		Item->Id(Id);
		Item->Enabled(Enabled);
		Item->Parent = this;

		Items.Insert(Item, Where);
		Item->ScanForAccel();

		Item->Insert(Items.IndexOf(Item));
	}

	if (Menu)
	{
		Menu->OnChange();
	}

	return Item;
}

GMenuItem *GSubMenu::AppendSeparator(int Where)
{
	GMenuItem *Item = new GMenuItem(Menu, this, Items.Length());
	if (Item)
	{
		Item->Separator(true);
		Item->Parent = this;
		Items.Insert(Item, Where);

		Item->Insert(Items.IndexOf(Item));
	}

	if (Menu)
	{
		Menu->OnChange();
	}

	return Item;
}

GSubMenu *GSubMenu::AppendSub(const char *Str, int Where)
{
	GMenuItem *Item = new GMenuItem(Menu, this, Items.Length());
	GSubMenu *Sub = new GSubMenu;

	if (Item && Sub)
	{
		Item->Parent = this;
		Item->Name(Str);
		Item->Sub(Sub);
		Items.Insert(Item, Where);

		Item->Insert(Items.IndexOf(Item));
	}
	else
	{
		DeleteObj(Item);
		DeleteObj(Sub);
	}

	if (Menu)
	{
		Menu->OnChange();
	}

	return Sub;
}

void GSubMenu::Empty()
{
	while (RemoveItem(0));
}

bool GSubMenu::RemoveItem(int i)
{
	bool Status = false;
	GMenuItem *Item = Items[i];
	if (Item)
	{
		Status = Item->Remove();
		DeleteObj(Item);
	}
	return Status;
}

bool GSubMenu::RemoveItem(GMenuItem *Item)
{
	if (Item && Items.HasItem(Item))
	{
		return Item->Remove();
	}

	return false;
}

int GSubMenu::Float(GView *From, int x, int y, bool Left)
{
	int Cmd = 0;

	if (From && Info)
	{
		GViewI *Wnd = From;
		while (Wnd && !Wnd->Handle())
		{
			Wnd = Wnd->GetParent();
		}

		if (Wnd && Wnd->Handle())
		{
			Cmd = TrackPopupMenu(	Info,
									TPM_LEFTALIGN |
									TPM_TOPALIGN |
									TPM_RETURNCMD |
									((Left)?TPM_LEFTBUTTON:TPM_RIGHTBUTTON),
									x,
									y,
									0,
									Wnd->Handle(),
									NULL);
		}
		else
		{
			LgiTrace("%s:%i - No handle to track popup menu.\n", __FILE__, __LINE__);
		}
	}

	return Cmd;
}

int GSubMenu::Length()
{
	return Items.Length();
}

GMenuItem *GSubMenu::ItemAt(int Id)
{
	return Items.ItemAt(Id);
}

GSubMenu *GSubMenu::FindSubMenu(int Id)
{
	for (GMenuItem *i = Items.First(); i; i = Items.Next())
	{
		GSubMenu *Sub = i->Sub();

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
class GMenuItemPrivate
{
public:
	bool StartUnderline;		// Underline the first display string
	bool HasAccel;				// The last display string should be right aligned
	List<GDisplayString> Strs;	// Draw each alternate display string with underline
								// except the last in the case of HasAccel==true.
	GAutoString Shortcut;

	GMenuItemPrivate()
	{
		HasAccel = false;
	}

	~GMenuItemPrivate()
	{
		Strs.DeleteObjects();
	}
};

GMenuItem::GMenuItem()
{
	d = new GMenuItemPrivate;
	Menu = 0;
	Parent = 0;
	Child = 0;
	Position = 0;
	_Icon = -1;

	ZeroObj(Info);
	Info.cbSize = sizeof(Info);

	#ifdef LGI_OWNER_DRAW_MENUS
	Info.fMask = MIIM_DATA;
	Info.fType = MFT_OWNERDRAW;

	#if _MSC_VER >= 1400
	Info.dwItemData = (ULONG_PTR)this;
	#else
	Info.dwItemData = (DWORD)this;
	#endif

	#endif

	Enabled(true);
}

GMenuItem::GMenuItem(GMenu *m, GSubMenu *p, int Pos, const char *Shortcut)
{
	d = new GMenuItemPrivate;
	d->Shortcut.Reset(NewStr(Shortcut));
	Position = Pos;
	Menu = m;
	Parent = p;
	Child = 0;
	_Icon = -1;

	ZeroObj(Info);
	Info.cbSize = sizeof(Info);

	#ifdef LGI_OWNER_DRAW_MENUS
	Info.fMask = MIIM_DATA;
	Info.fType = MFT_OWNERDRAW;
	#if _MSC_VER >= 1400
	Info.dwItemData = (ULONG_PTR)this;
	#else
	Info.dwItemData = (DWORD)this;
	#endif
	#endif

	Name("<error>");
	Id(0);
	Enabled(true);
}

GMenuItem::~GMenuItem()
{
	if (Parent)
	{
		if (Parent->Handle())
		{
			int Index = Parent->Items.IndexOf(this);
			RemoveMenu(Parent->Handle(), Index, MF_BYPOSITION);
		}
		Parent->Items.Delete(this);
	}
	DeleteObj(Child);
	DeleteObj(d);
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
	if (Separator())
	{
		Size.x = 8;
		Size.y = 8;
	}
	else
	{
		GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
		bool BaseMenu = Parent == Menu; // true if attached to a windows menu
										// else is a submenu
		int Ht = Font->GetHeight();
		Size.x = BaseMenu ? 0 : 20;

		for (GDisplayString *s = d->Strs.First(); s; s = d->Strs.Next())
		{
			Size.x += s->X();
		}

		if (d->Shortcut ||
			strchr(Name(), '\t'))
		{
			Size.x += 32;
		}

		if (!BaseMenu)
		{
			// leave room for child pointer
			Size.x += Child ? 8 : 0;
		}

		Size.y = BaseMenu ? GetSystemMetrics(SM_CYMENU)+1 : Ht+2;
		if (Size.y < 16) Size.y = 16;
	}
}

void GMenuItem::_PaintText(GSurface *pDC, int x, int y, int Width)
{
	bool Underline = d->StartUnderline;
	GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
	for (int i=0; i < (d->Strs.Length() - (d->HasAccel ? 1 : 0)); i++)
	{
		GDisplayString *s = d->Strs[i];
		if (!s) break;

		s->Draw(pDC, x, y);
		if (Underline)
		{
			int UnderX = s->X();
			pDC->Colour(Font->Fore());
			pDC->Line(x, y+s->Y()-1, x+max(s->X()-2, 1), y+s->Y()-1);
		}

		Underline = !Underline;
		x += s->X();
	}

	if (d->HasAccel)
	{
		GDisplayString *s = d->Strs.Last();
		if (s)
		{
			s->Draw(pDC, Width - s->X() - 8, y);
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
		// paint a separator
		int Cy = pDC->Y() / 2;

		pDC->Colour(LC_MENU_BACKGROUND, 24);
		pDC->Rectangle();

		pDC->Colour(LC_LOW, 24);
		pDC->Line(0, Cy-1, pDC->X()-1, Cy-1);
		
		pDC->Colour(LC_LIGHT, 24);
		pDC->Line(0, Cy, pDC->X()-1, Cy);
	}
	else
	{
		// paint a text menu item
		COLOUR Fore = Selected ? LC_FOCUS_SEL_FORE : LC_MENU_TEXT;
		COLOUR Back = BaseMenu ? Menu->d->RootMenuBack : Selected ? LC_FOCUS_SEL_BACK : LC_MENU_BACKGROUND;
		int x = IconX;
		GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
		int y = (pDC->Y() - Font->GetHeight()) >> 1;

		// paint the background
		if (BaseMenu)
		{
			// for a menu, sunken on selected
			GRect rgn = r;
			if (Selected)
			{
				LgiThinBorder(pDC, rgn, SUNKEN);
				Fore = LC_MENU_TEXT;
				x++;
				y++;
			}
			
			// always dialog colour
			pDC->Colour(Back, 24);
			pDC->Rectangle(&rgn);
		}
		else
		{
			// for a submenu
			pDC->Colour(Back, 24);
			pDC->Rectangle();
		}

		// draw the text on top
		Font->Transparent(true);
		if (Disabled)
		{
			// disabled text
			if (!Selected)
			{
				Font->Colour(LC_LIGHT, 0);
				_PaintText(pDC, x+1, y, r.X());
			}
			// else selected... don't draw the hilight

			// "greyed" text...
			Font->Colour(LC_LOW, 0);
			_PaintText(pDC, x, y-1, r.X()-1);
		}
		else
		{
			// normal coloured text
			Font->Colour(Fore, 0);
			_PaintText(pDC, x, y-1, r.X());
		}

		GImageList *ImgLst = (Menu && Menu->GetImageList()) ? Menu->GetImageList() : Parent ? Parent->GetImageList() : 0;

		// draw icon/check mark
		if (Checked && IconX > 0)
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
		else if (ImgLst &&
				_Icon >= 0)
		{
			// it's an icon!
			ImgLst->Draw(pDC, 0, 0, _Icon);
		}
	}
}

bool GMenuItem::ScanForAccel()
{
	char *Accel = 0;

	if (d->Shortcut)
	{
		Accel = d->Shortcut;
	}
	else
	{
		char *n = GBase::Name();
		if (n)
		{
			char *Tab = strchr(n, '\t');
			if (Tab)
			{
				Accel = Tab + 1;
			}
		}
	}

	if (Accel)
	{
		GToken Keys(Accel, "-+");
		if (Keys.Length() > 0)
		{
			int Flags = 0;
			uchar Key = 0;
			
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
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Ins") == 0 ||
						 stricmp(k, "Insert") == 0)
				{
					Key = VK_INSERT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Home") == 0)
				{
					Key = VK_HOME;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "End") == 0)
				{
					Key = VK_END;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "PageUp") == 0 ||
						 stricmp(k, "Page Up") == 0 ||
						 stricmp(k, "Page-Up") == 0)
				{
					Key = VK_PRIOR;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "PageDown") == 0 ||
						 stricmp(k, "Page Down") == 0 ||
						 stricmp(k, "Page-Down") == 0)
				{
					Key = VK_NEXT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Backspace") == 0)
				{
					Key = VK_BACK;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Up") == 0)
				{
					Key = VK_UP;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Down") == 0)
				{
					Key = VK_DOWN;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Left") == 0)
				{
					Key = VK_LEFT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Right") == 0)
				{
					Key = VK_RIGHT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Esc") == 0)
				{
					Key = VK_ESCAPE;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Space") == 0)
				{
					Key = ' ';
				}
				else if (k[0] == 'F' && IsDigit(k[1]))
				{
					Key = VK_F1 + atoi(k+1) - 1;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (IsAlpha(k[0]))
				{
					Key = toupper(k[0]);
				}
				else if (IsDigit(k[0]))
				{
					Key = k[0];
				}
				else if (strchr(",./\\[]`;\'", k[0]))
				{
					if (Flags & LGI_EF_CTRL)
					{
						switch (k[0])
						{
							case ';': Key = 186; break;
							case '=': Key = 187; break;
							case ',': Key = 188; break;
							case '_': Key = 189; break;
							case '.': Key = 190; break;
							case '/': Key = 191; break;
							case '`': Key = 192; break;
							case '[': Key = 219; break;
							case '\\': Key = 220; break;
							case ']': Key = 221; break;
							case '\'': Key = 222; break;
							default: LgiAssert(!"Unknown key."); break;
						}
					}
					else
					{
						Key = k[0];
					}
				}
			}
			
			if (Key && Menu)
			{
				Menu->Accel.Insert( new GAccelerator(Flags, Key, Id()) );
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
	bool Status = false;
	if (Parent)
	{
		int Index = Parent->Items.IndexOf(this);
		Parent->Items.Delete(this);
		Status = RemoveMenu(Parent->Handle(), Index, MF_BYPOSITION);

		int n=0;
		for (GMenuItem *i=Parent->Items.First(); i; i=Parent->Items.Next())
		{
			i->Position = n++;
		}
	}
	
	return Status;
}

bool GMenuItem::Update()
{
	bool Status = FALSE;

	if (Parent && Parent->Handle())
	{
		Status = SetMenuItemInfo(Parent->Handle(), Position, true, &Info);
	}

	return Status;
}

void GMenuItem::Icon(int i)
{
	_Icon = i;
}

int GMenuItem::Icon()
{
	return _Icon;
}

void GMenuItem::Id(int i)
{
	Info.wID = i;
	Info.fMask |= MIIM_ID;
	Update();
}

void GMenuItem::Separator(bool s)
{
	if (s)
	{
		Info.fType |= MFT_SEPARATOR;
	}
	else
	{
		Info.fType &= ~MFT_SEPARATOR;
	}

	Info.fMask |= MIIM_TYPE;
	Update();
}

void GMenuItem::Checked(bool c)
{
	if (c)
	{
		Info.fState |= MFS_CHECKED;
	}
	else
	{
		Info.fState &= ~MFS_CHECKED;
	}
	
	Info.fMask |= MIIM_STATE;
	Update();
}

bool GMenuItem::Name(const char *Txt)
{
	bool Status = GBase::Name(Txt);
	if (Status)
	{
		char *n = NewStr(Txt);
		if (n)
		{
			// Set OS menu structure
			Info.dwTypeData = GBase::Name();
			Info.cch = strlen(GBase::Name());
			Info.fType |= MFT_STRING;
			Info.fMask |= MIIM_TYPE | MIIM_DATA;

			// Build up our display strings, 
			d->Strs.DeleteObjects();
			GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
			d->StartUnderline = false;

			char *Tab = strrchr(n, '\t');
			if (Tab)
			{
				*Tab = 0;
			}

			char *Amp = 0, *a = n;
			while (a = strchr(a, '&'))
			{
				if (a[1] != '&')
				{
					Amp = a;
					break;
				}

				a++;
			}

			if (Amp)
			{
				// Before amp
				d->Strs.Insert(new GDisplayString(Font, n, Amp - n ));

				// Amp'd letter
				char *e = LgiSeekUtf8(++Amp, 1);
				d->Strs.Insert(new GDisplayString(Font, Amp, e - Amp ));

				// After Amp
				if (*e)
				{
					d->Strs.Insert(new GDisplayString(Font, e));
				}
			}
			else
			{
				d->Strs.Insert(new GDisplayString(Font, n));
			}

			if (d->HasAccel = (Tab != 0))
			{
				d->Strs.Insert(new GDisplayString(Font, Tab + 1));
				*Tab = '\t';
			}
			else if (d->HasAccel = (d->Shortcut.Get() != 0))
			{
				d->Strs.Insert(new GDisplayString(Font, d->Shortcut));
			}

			// Tell the OS
			Update();
			
			DeleteArray(n);
		}
	}
	return Status;
}

void GMenuItem::Enabled(bool e)
{
	Info.fState &= ~(MFS_ENABLED|MFS_DISABLED|MFS_GRAYED);

	if (e)
	{
	}
	else
	{
		Info.fState |= MFS_DISABLED|MFS_GRAYED;
	}

	Info.fMask |= MIIM_STATE;
	Update();
}

void GMenuItem::Focus(bool f)
{
	if (f)
	{
		Info.fState |= MFS_HILITE;
	}
	else
	{
		Info.fState &= ~MFS_HILITE;
	}

	Info.fMask |= MIIM_STATE;
	Update();
}

void GMenuItem::Sub(GSubMenu *s)
{
	Child = s;
	if (Child)
	{
		Info.hSubMenu = Child->Handle();
		s->Menu = Menu;
		s->Parent = this;
	}
	else
	{
		Info.hSubMenu = 0;
	}
	Info.fMask |= MIIM_SUBMENU;
	Update();
}

bool GMenuItem::Insert(int Pos)
{
	bool Status = false;

	if (Parent)
	{
		Position = Pos;
		Status = InsertMenuItem(Parent->Handle(),
								Position,
								true,
								&Info);
	}

	return Status;
}

void GMenuItem::Visible(bool i)
{
}

int GMenuItem::Id() { return Info.wID; }
char *GMenuItem::Name() { return (!(Info.fType & MFT_SEPARATOR)) ? Info.dwTypeData : 0; }
bool GMenuItem::Separator() { return (Info.fType & MFT_SEPARATOR) != 0; }
bool GMenuItem::Checked() { return (Info.fState & MF_CHECKED) != 0; }
bool GMenuItem::Enabled()
{
	return (Info.fState & MFS_DISABLED) == 0;
}
bool GMenuItem::Visible() { return true; }
bool GMenuItem::Focus() { return (Info.fState & MFS_HILITE) != 0; }
GSubMenu *GMenuItem::Sub() { return Child; }

///////////////////////////////////////////////////////////////////////////////////////////////
int _GMenuFontRef = 0;
GFont *GMenu::_Font = 0;

GMenu::GMenu() : GSubMenu("", false)
{
	d = new GMenuPrivate;
	Menu = this;
	Window = NULL;
	_GMenuFontRef++;
}

GMenu::~GMenu()
{
	Accel.DeleteObjects();
	if (--_GMenuFontRef == 0)
	{
		DeleteObj(_Font);
	}
	DeleteObj(d);
}

void GMenu::OnChange()
{
	if (Info && Window)
	{
		if (::GetMenu(Window->Handle()) != Info)
		{
			SetMenu(Window->Handle(), Info);
		}
	}
}

GFont *GMenu::GetFont()
{
	if (!_Font)
	{
		GFontType Type;
		if (Type.GetSystemFont("Menu"))
		{
			_Font = Type.Create();
		}
	}

	return _Font ? _Font : SysFont;
}

bool GMenu::Attach(GViewI *p)
{
	Window = p;

	return true;
}

bool GMenu::Detach()
{
	bool Status = FALSE;

	if (Window)
	{
		HMENU hWndMenu = ::GetMenu(Window->Handle());
		if (hWndMenu == Info)
		{
			Status = SetMenu(Window->Handle(), NULL);
			if (Status)
			{
				Window = NULL;
			}
		}
	}

	return Status;
}

bool GMenu::OnKey(GView *v, GKey &k)
{
	if (k.Down() && k.vkey != 17)
	{
		for (GAccelerator *a = Accel.First(); a; a = Accel.Next())
		{
			if (a->Match(k))
			{
				GMenuItem *i = FindItem(a->GetId());
				if (!i || i->Enabled())
				{
					Window->OnCommand(a->GetId(), 0, 0);
					return false;
				}
			}
		}
	}

	return true;
}

int GMenu::_OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case WM_MENUCHAR:
		{
			short Key = MsgA(Msg) & 0xffff;
			HMENU View = (HMENU)MsgB(Msg);
			MENUITEMINFO Info;

			ZeroObj(Info);
			Info.cbSize = sizeof(Info);
			Info.fMask = MIIM_DATA;
			if (GetMenuItemInfo(View, 0, true, &Info))
			{
				GMenuItem *Item = (GMenuItem*)Info.dwItemData;
				if (Item)
				{
					GSubMenu *Menu = Item->Parent;
					if (Menu)
					{
						int Index=0;
						for (GMenuItem *i=Menu->Items.First(); i; i=Menu->Items.Next(), Index++)
						{
							char *n = i->Name();
							if (n)
							{
								char *Amp = strchr(n, '&');
								if (Amp &&
									toupper(Amp[1]) == toupper(Key))
								{
									return (MNC_EXECUTE << 16) | Index;
								}
							}
						}
					}
				}
			}			
			break;
		}
		case WM_MEASUREITEM:
		{
			LPMEASUREITEMSTRUCT Item = (LPMEASUREITEMSTRUCT)Msg->b;
			if (Item)
			{
				GdcPt2 Size;

				((GMenuItem*)Item->itemData)->_Measure(Size);
				Item->itemWidth = Size.x;
				Item->itemHeight = Size.y;
				return true;
			}
			break;
		}
		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT Item = (LPDRAWITEMSTRUCT)Msg->b;
			if (Item &&
				Item->CtlType == ODT_MENU)
			{
				GRect r = Item->rcItem;

				GScreenDC Dc(Item->hDC, Item->hwndItem);
				
				// Get the original origin. We need to do this because when
				// "animated menus" are on the offset starts at -3,-3 and throws
				// the menu items off. This is a bug in windows, but watcha
				// gonna do?
				int x, y;
				Dc.GetOrigin(x, y);
				
				// Clip and offset so that the menu item draws in client co-ords.
				Dc.SetOrigin(-r.x1+x, -r.y1+y);
				Dc.SetSize(r.X(), r.Y());

				// Paint the item...
				((GMenuItem*)Item->itemData)->_Paint(&Dc, Item->itemState);

				// Set the origin back the original value.
				Dc.SetOrigin(x, y);
				return true;
			}
			break;
		}
	}

	return 0;
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
	if (!k.IsChar &&
		tolower(k.vkey) == tolower(Key))
	{
		if
		(
			(TestFlag(Flags, LGI_EF_CTRL) ^ k.Ctrl() == 0) &&
			(TestFlag(Flags, LGI_EF_ALT) ^ k.Alt() == 0) &&
			(TestFlag(Flags, LGI_EF_SHIFT) ^ k.Shift() == 0) &&
			(!TestFlag(Flags, LGI_EF_IS_CHAR) || k.IsChar) &&
			(!TestFlag(Flags, LGI_EF_IS_NOT_CHAR) || !k.IsChar)
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
