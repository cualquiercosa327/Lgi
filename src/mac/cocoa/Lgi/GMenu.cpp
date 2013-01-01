/*hdr
**      FILE:           GuiMenu.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           18/7/98
**      DESCRIPTION:    Gui menu system
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <math.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GToken.h"
#include "GUtf8.h"

#define DEBUG_INFO		0

///////////////////////////////////////////////////////////////////////////////////////////////
GSubMenu::GSubMenu(const char *name, bool Popup)
{
	Menu = 0;
	Parent = 0;
	Info = 0;
}

GSubMenu::~GSubMenu()
{
	Items.DeleteObjects();
}

void GSubMenu::OnAttach(bool Attach)
{
	for (GMenuItem *i = Items.First(); i; i = Items.Next())
	{
		i->OnAttach(Attach);
	}
	
	if (Attach AND
		this != Menu AND
		Parent AND
		Parent->Parent)
	{
		#if 0
		GSubMenu *k = Parent->Parent;

		if (Parent->Info == 0)
		{
			Parent->Info = k->Items.IndexOf(Parent) + 1;
			char *Str = Parent->Name();
			CFStringRef s = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)Str, strlen(Str), kCFStringEncodingUTF8, false);
			OSStatus e = InsertMenuItemTextWithCFString(Parent->Parent->Info, s, Parent->Info - 1, 0, 0);
			CFRelease(s);

			if (e) printf("%s:%i - Error: AppendMenuItemTextWithCFString(%p)=%i\n", __FILE__, __LINE__, Parent->Parent->Info, Parent->Info);
			else
			{
				e = SetMenuItemHierarchicalMenu(Parent->Parent->Info, Parent->Info, Info);	
				if (e) printf("%s:%i - Error: SetMenuItemHierarchicalMenu(%p, %i, %p) = %i\n", __FILE__, __LINE__, Parent->Parent->Info, Parent->Info, Info, e);
			}
		}
		#endif
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
	GMenuItem *i = new GMenuItem(Menu, this, Where, Shortcut);
	if (i)
	{
		if (Info)
		{
			Items.Insert(i, Where);
			
			i->Name(Str);
			i->Parent = this;
			i->Menu = Menu;

			Str = i->Name();
			
			i->Id(Id);
			i->Enabled(Enabled);
			i->ScanForAccel();
			
			return i;
		}
		else
		{
			printf("%s:%i - No menu to attach item to.\n", __FILE__, __LINE__);
			DeleteObj(i);
		}
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

		if (Info)
		{
		}
		else
		{
			printf("%s:%i - No menu to attach item to.\n", __FILE__, __LINE__);
		}
		
		return i;
	}

	return 0;
}

GSubMenu *GSubMenu::AppendSub(const char *Str, int Where)
{
	GMenuItem *i = new GMenuItem;
	if (i && Str)
	{
		i->Name(Str);
		i->Parent = this;
		i->Menu = Menu;
		i->Id(-1);

		Items.Insert(i, Where);

		if (Info)
		{
			i->Child = new GSubMenu(Str);
			if (i->Child)
			{
				i->Child->Parent = i;
				i->Child->Menu = Menu;
				i->Child->Window = Window;
			}
		}
		else
		{
			printf("%s:%i - No menu to attach item to.\n", __FILE__, __LINE__);
		}

		return i->Child;
	}

	return 0;
}

void GSubMenu::Empty()
{
	while (Items.First())
	{
		RemoveItem(Items.First());
	}
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
	return -1;
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
	GAutoString Shortcut;
};

GMenuItem::GMenuItem()
{
	d = new GMenuItemPrivate();
	Menu = 0;
	Info = 0;
	Child = 0;
	Parent = 0;
	_Icon = -1;
	_Id = 0;
	_Check = false;
}

GMenuItem::GMenuItem(GMenu *m, GSubMenu *p, int Pos, const char *Shortcut)
{
	d = new GMenuItemPrivate();
	Menu = m;
	Parent = p;
	Info = 0;
	Child = 0;
	_Icon = -1;
	_Id = 0;
	_Check = false;
	d->Shortcut.Reset(NewStr(Shortcut));
}

GMenuItem::~GMenuItem()
{
	DeleteObj(Child);
	DeleteObj(d);
}

void GMenuItem::OnAttach(bool Attach)
{
	if (Attach)
	{
		if (_Icon >= 0)
		{
			Icon(_Icon);
		}
		
		if (Sub())
		{
			Sub()->OnAttach(Attach);
		}
	}
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

		while (i AND *i)
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
			int Mx, Tx;
			GDisplayString ds(Font, Str, (int)Tab-(int)Str);
			Mx = ds.X();
			GDisplayString ds2(Font, Tab + 1);
			Tx = ds2.X();
			Size.x = IconX + 32 + Mx + Tx;
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
		GFont *Font = Menu AND Menu->GetFont() ? Menu->GetFont() : SysFont;
		bool Underline = false;
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
							int Ascent = (int)ceil(Font->Ascent());
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
	
	#if defined(WIN32) || defined(MAC)
	GRect r(0, 0, pDC->X()-1, pDC->Y()-1);
	#else
	GRect r = Info->GetClient();
	#endif

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
		pDC->Rectangle();

		// Draw the text on top
		GFont *Font = Menu AND Menu->GetFont() ? Menu->GetFont() : SysFont;
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
	if (!d->Shortcut)
		return false;

	GToken Keys(d->Shortcut, "+-");
	if (Keys.Length() <= 0)
		return false;

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
			int F[] =
			{
				VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6,
				VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12
			};
			int idx = atoi(k + 1);
			if (idx >= 1 AND idx <= 12)
			{
				Key = F[idx-1];
			}
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
	
	if (Key == ' ')
	{
		Menu->Accel.Insert( new GAccelerator(Flags, Key, Id()) );
	}
	else if (Key)
	{
		/*
		OSStatus e;
		int ModMask =	(TestFlag(Flags, LGI_EF_CTRL) ? 0 : kMenuNoCommandModifier) |
						(TestFlag(Flags, LGI_EF_ALT) ? kMenuOptionModifier : 0) |
						(TestFlag(Flags, LGI_EF_SHIFT) ? kMenuShiftModifier : 0);

		e = SetMenuItemModifiers(Parent->Info, Info, ModMask);

		if (e) printf("%s:%i - SetMenuItemModifiers() failed with %i\n",
						__FILE__, __LINE__, (int)e);
		
		switch (Key)
		{
			#define Map(k, g) \
				case k: \
					SetMenuItemKeyGlyph(Parent->Info, Info, g); \
					break
					
			Map(VK_F1, kMenuF1Glyph);
			Map(VK_F2, kMenuF2Glyph);
			Map(VK_F3, kMenuF3Glyph);
			Map(VK_F4, kMenuF4Glyph);
			Map(VK_F5, kMenuF5Glyph);
			Map(VK_F6, kMenuF6Glyph);
			Map(VK_F7, kMenuF7Glyph);
			Map(VK_F8, kMenuF8Glyph);
			Map(VK_F9, kMenuF9Glyph);
			Map(VK_F10, kMenuF10Glyph);
			Map(VK_F11, kMenuF11Glyph);
			Map(VK_F12, kMenuF12Glyph);
			Map(' ', kMenuSpaceGlyph);
			Map(VK_DELETE, kMenuDeleteLeftGlyph); // kMenuDeleteRightGlyph
			Map(VK_BACKSPACE, kMenuDeleteLeftGlyph);
			Map(VK_UP, kMenuUpArrowGlyph);
			Map(VK_DOWN, kMenuDownArrowGlyph);
			Map(VK_LEFT, kMenuLeftArrowGlyph);
			Map(VK_RIGHT, kMenuRightArrowGlyph);
			default:
			{					
				e = SetMenuItemCommandKey(	Parent->Info,
											Info,
											false,
											Key);
				if (e) printf("%s:%i - SetMenuItemCommandKey(%i/%c) failed with %i\n",
								__FILE__, __LINE__, Key, Key, (int)e);
				break;
			}
		}
		*/
	}

	return true;
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

	if (Parent)
	{
	}
}

void GMenuItem::Checked(bool c)
{
	_Check = c;
	if (Parent)
	{
	}
}

bool GMenuItem::Name(const char *n)
{
	char *Tmp = NewStr(n);
	if (Tmp)
	{
		char *in = Tmp, *out = Tmp;
		while (*in)
		{
			if (*in != '&')
				*out++ = *in;
			in++;
		}
		*out++ = 0;
	}

	bool Status = GBase::Name(Tmp);
	if (Status && Parent)
	{
	}

	DeleteArray(Tmp);

	return Status;
}

void GMenuItem::Enabled(bool e)
{
	if (Parent)
	{
	}
}

void GMenuItem::Focus(bool f)
{
}

void GMenuItem::Sub(GSubMenu *s)
{
	Child = s;
}

void releaseData(void *info, const void *data, size_t size)
{
	uchar *i = (uchar*)info;
	DeleteArray(i);
}

void GMenuItem::Icon(int i)
{
	_Icon = i;
	
	if (Parent AND Parent->Info AND Info)
	{
		GImageList *Lst = Menu ? Menu->GetImageList() : Parent->GetImageList();
		if (!Lst)
			return;

		int Bpp = Lst->GetBits() / 8;
		int Off = _Icon * Lst->TileX() * Bpp;
		int Line = Lst->X() * Bpp;
		uchar *Base = (*Lst)[0];
		if (!Base)
			return;

		int TempSize = Lst->TileX() * Lst->TileY() * Bpp;
		uchar *Temp = new uchar[TempSize];
		if (!Temp)
			return;
		
		uchar *d = Temp;
		for (int y=0; y<Lst->TileY(); y++)
		{
			uchar *s = Base + Off + (Line * y);
			uchar *e = s + (Lst->TileX() * Bpp);
			while (s < e)
			{
				if (memcmp(Base, s, Bpp) == 0)
					memset(d, 0, Bpp);
				else
					memcpy(d, s, Bpp);
				d += Bpp;
				s += Bpp;
			}
		}
		
	}
	else
	{
		printf("Can't set icon.\n");
	}
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
	if (Parent)
	{
	}

	return true;
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
				#ifndef MAC
				_Font->CodePage(SysFont->CodePage());
				#endif
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
		
		if (Info)
		{
			OnAttach(true);
			Status = true;
		}
		else
		{
			printf("%s:%i - No menu\n", __FILE__, __LINE__);
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
				Window->OnCommand(a->GetId(), 0, 0);
				return true;
			}
		}
		
		if (k.Alt() AND
			!dynamic_cast<GMenuItem*>(v) AND
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

							if (Amp)
							{
								char Accel = tolower(Amp[1]);
								char Press = tolower(k.c16);
								if (Accel == Press)
								{
									Hide = true;
								}
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
	int Press = (uint) k.c16;
	
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
