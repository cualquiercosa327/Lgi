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
#include "Lgi.h"
#include "GString.h"

///////////////////////////////////////////////////////////////////////////////////////////////
extern char *NewStrLessAnd(char *s);
char *NewStrLessAnd(char *s)
{
	char *ns = NewStr(s);
	if (ns)
	{
		char *In = ns;
		char *Out = ns;
		while (*In)
		{
			if (*In != '&')
			{
				*Out++ = *In;
			}
			In++;
		}
		*Out = 0;
	}
	return ns;
}

///////////////////////////////////////////////////////////////////////////////////////////////
class LgiMenuItem : public BMenuItem
{
public:
	GMenuItem *i;

	LgiMenuItem(GMenuItem *Item, const char *label, BMessage *message, char shortcut = 0, uint32 modifiers = 0) :
		BMenuItem(label, message, shortcut, modifiers)
	{
		i = Item;
	}
	
	LgiMenuItem(GMenuItem *Item, BMenu *menu, BMessage *message = NULL) :
		BMenuItem(menu, message)
	{
		i = Item;
	}
		
	LgiMenuItem(GMenuItem *Item, BMessage *data) :
		BMenuItem(data)
	{
		i = Item;
	}

	void DrawContent();
	status_t Invoke(BMessage *message = NULL);
};

void LgiMenuItem::DrawContent()
{
	BMenuItem::DrawContent();
	
	if (i)
	{
		GScreenDC DC(Menu());
		BPoint p = ContentLocation();
		DC.SetOrigin(-p.x, -p.y);
		i->_Paint(&DC, 0);
		DC.SetOrigin(0, 0);
	}
}

status_t LgiMenuItem::Invoke(BMessage *message)
{
	GMenu *m = i->Menu;
	GViewI *p = (m) ? m->WindowHandle() : 0;
	BView *v = (p) ? p->Handle() : 0;
	if (v)
	{
		SetTarget(v);
		// printf("LgiMenuItem::Invoke Handler=%p, Looper=%p Menu=%p GView=%p BView=%p\n", h, l, m, p, v);
	}

	BMenuItem::Invoke(message);
}

///////////////////////////////////////////////////////////////////////////////////////////////
GSubMenu::GSubMenu(char *name, bool Popup)
{
	if (name)
	{
		char *Temp = NewStrLessAnd(name);
		if (Temp)
		{
			Info = new BMenu(Temp);
			DeleteArray(Temp);
		}
	}

	Parent = 0;
	Menu = 0;
	Window = 0;
}

GSubMenu::~GSubMenu()
{
	DeleteObj(Info);
}

GMenuItem *GSubMenu::AppendItem(char *Str, int Id, bool Enabled, int Where)
{
	GMenuItem *i = new GMenuItem;
	if (Info AND i)
	{
		Items.Insert(i, Where);

		i->Menu = Menu;
		i->Parent = this;
		i->Position = Items.IndexOf(i);
		i->Name(Str);
		i->Id(Id);
		i->Enabled(Enabled);
		if (Window AND Window->Handle())
		{
			i->Info->SetTarget(Window->Handle());
		}

		Info->AddItem(i->Info, i->Position);
	}

	return i;
}

GMenuItem *GSubMenu::AppendSeparator(int Where)
{
	GMenuItem *i = new GMenuItem(new BSeparatorItem);
	if (Info AND i)
	{
		Items.Insert(i, Where);

		i->Menu = Menu;
		i->Parent = this;
		i->Position = Items.IndexOf(i);

		Info->AddItem(i->Info, i->Position);
	}
	
	return i;
}

GSubMenu *GSubMenu::AppendSub(char *Str, int Where)
{
	GSubMenu *Sub = 0;
	GMenuItem *i = new GMenuItem(Sub = new GSubMenu(Str));
	if (Info AND i)
	{
		Items.Insert(i, Where);

		i->Name(Str);
		i->Menu = Menu;
		i->Parent = this;
		i->Position = Items.IndexOf(i);

		Sub->Menu = Menu;
		Sub->Parent = i;
		Sub->Window = Window;

		Info->AddItem(i->Info, i->Position);
	}

	return Sub;
}

void GSubMenu::Empty()
{
	GMenuItem *i = 0;
	while (i = Items.First())
	{
		if (Info)
		{
			Info->RemoveItem(i->Info);
		}
		DeleteObj(i);
	}
}

GMenuItem *GSubMenu::ItemAt(int i)
{
	return Items.ItemAt(i);
}

bool GSubMenu::RemoveItem(int i)
{
	bool Status = false;
	GMenuItem *Item = Items.ItemAt(i);
	if (Item)
	{
		Items.Delete(Item);
		Status = true;
		if (Info) Status = Info->RemoveItem(Item->Info);
	}
	return Status;
}

bool GSubMenu::RemoveItem(GMenuItem *Item)
{
	bool Status = false;
	if (Item AND Items.HasItem(Item))
	{
		Items.Delete(Item);
		Status = true;
		if (Info) Status = Info->RemoveItem(Item->Info);
	}
	return Status;
}

void GSubMenu::_CopyMenu(BMenu *To, GSubMenu *From)
{
	if (To AND From)
	{
		GMenuItem *i = 0;
		for (int n=0; i=From->ItemAt(n); n++)
		{
			if (i->Separator())
			{
				// space
				To->AddSeparatorItem();
			}
			else if (i->Sub())
			{
				// submenu
				GSubMenu *Sub = new GSubMenu(i->Name());
				GMenuItem *Item = new GMenuItem(Sub);
				if (Item)
				{
					if (From->GetImageList())
					{
						Sub->SetImageList(From->GetImageList(), false);
					}
					
					To->AddItem(Item->Info);
					_CopyMenu(Sub->Info, i->Sub());
				}
			}
			else
			{
				// normal item
				GMenuItem *Item = new GMenuItem;
				if (Item)
				{
					Item->Id(i->Id());
					Item->Name(i->Name());
					if (i->Icon() >= 0)
					{
						Item->Icon(i->Icon());
					}
					Item->Parent = From;
					
					To->AddItem(Item->Info);
				}
			}
		}
	}
}

int GSubMenu::Float(GView *Parent, int x, int y, bool Left)
{
	if (Info)
	{
		BPopUpMenu *Popup = new BPopUpMenu("PopUpMenu");
		if (Popup)
		{
			_CopyMenu(Popup, this);
		
			BPoint Pt(x, y);
			BMenuItem *Item = Popup->Go(Pt);
			if (Item)
			{
				#undef Message
				BMessage *Msg = Item->Message();
				int32 i;
				if (Msg AND Msg->FindInt32("Cmd", &i) == B_OK)
				{
					return i;
				}
			}
		}
	}
	
	return -1;
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
	for (int n=0; n<Items.Length(); n++)
	{
		GMenuItem *i = Items.ItemAt(n);
		if (i)
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
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
class GMenuItemPrivate
{
public:
	BMessage *Msg;

	GMenuItemPrivate()
	{
		Msg = 0;
	}
	
	~GMenuItemPrivate()
	{
	}
};

GMenuItem::GMenuItem()
{
	d = new GMenuItemPrivate;
	Menu = 0;
	Parent = 0;
	Child = 0;
	Position = -1;
	_Icon = -1;
	
	Info = new LgiMenuItem(this, "<error>", d->Msg = new BMessage(M_COMMAND));
}

GMenuItem::GMenuItem(BMenuItem *item)
{
	d = new GMenuItemPrivate;
	Menu = 0;
	Parent = 0;
	Child = 0;
	Position = -1;
	_Icon = -1;
	
	Info = item;
}

GMenuItem::GMenuItem(GSubMenu *p)
{
	d = new GMenuItemPrivate;
	Menu = 0;
	Parent = 0;
	Child = p;
	Position = -1;
	_Icon = -1;

	if (Child)
	{
		Info = new LgiMenuItem(this, Child->Info);
		if (Info)
		{
			Child->Parent = this;
			Child->Menu = Menu;
		}
	}
	else
	{
		Info = 0;
	}
}

GMenuItem::~GMenuItem()
{
	if (Parent)
	{
		Parent->Items.Delete(this);
	}
	DeleteObj(d);
}

void GMenuItem::_Measure(GdcPt2 &Size)
{
}

void GMenuItem::_Paint(GSurface *pDC, int Flags)
{
	if (_Icon >= 0)
	{
		if (Parent AND
			Parent->GetImageList())
		{
			Parent->GetImageList()->Draw(pDC, -16, 0, _Icon);
		}
	}
}

void GMenuItem::_PaintText(GSurface *pDC, int x, int y, int Width)
{
}

void GMenuItem::Icon(int i)
{
	_Icon = i;
}

int GMenuItem::Icon()
{
	return _Icon;
}

GSubMenu *GMenuItem::Create()
{
	return 0;
}

bool GMenuItem::Remove()
{
	if (Parent)
	{
		Parent->Items.Delete(this);
	}
	
	DeleteObj(Info);
	
	return true;
}

GSubMenu *GMenuItem::GetParent()
{
	return Parent;
}

void GMenuItem::Id(int i)
{
	if (d->Msg)
	{
		d->Msg->AddInt32("Cmd", i);
	}
}

int GMenuItem::Id()
{
	if (d->Msg)
	{
		int32 i;
		if (d->Msg->FindInt32("Cmd", &i) == B_OK)
		{
			return i;
		}
	}
	return 0;
}

void GMenuItem::Separator(bool s)
{
}

bool GMenuItem::Separator()
{
	return dynamic_cast<BSeparatorItem*>(Info) != 0;
}

void GMenuItem::Checked(bool c)
{
	if (Info) Info->SetMarked(c);
}

bool GMenuItem::Checked()
{
	return (Info) ? Info->IsMarked() : false;
}

bool GMenuItem::Name(char *n)
{
	bool Status = false;
	char *p = (n) ? NewStrLessAnd(n) : NewStr("");
	if (p)
	{
		char *Tab = strchr(p, '\t');
		if (Tab)
		{
			*Tab = 0;
			Tab++;
			
			if (strnicmp(Tab, "Ctrl+", 5) == 0)
			{
				if (Info) Info->SetShortcut(Tab[5], 0);
			}
			else if (stricmp(Tab, "Del") == 0)
			{
				// SetShortcut('', 0);
			}
		}

		Status = GObject::Name(p);
		if (Info) Info->SetLabel(p);
		DeleteArray(p);
	}

	return Status;
}

char *GMenuItem::Name()
{
	return (char*) ((Info) ? Info->Label() : 0);
}

void GMenuItem::Enabled(bool e)
{
	if (Info) Info->SetEnabled(e);
}

bool GMenuItem::Enabled()
{
	return (Info) ? Info->IsEnabled() : false;
}

void GMenuItem::Focus(bool f)
{
	// if (Item) Item->Highlight(f);
}

bool GMenuItem::Focus()
{
	return false;
}

void GMenuItem::Visible(bool v)
{
}

bool GMenuItem::Visible()
{
	return true;
}

void GMenuItem::Sub(GSubMenu *s)
{
	DeleteObj(Child);
	Child = s;
	if (Child AND Info)
	{
		// fixme.... reallocate the item to have a submenu
	}
}

GSubMenu *GMenuItem::Sub()
{
	return Child;
}

///////////////////////////////////////////////////////////////////////////////////////////////
GFont *GMenu::_Font = 0;

class GMenuPrivate
{
public:
	BMenuBar *Bar;

	GMenuPrivate()
	{
		Bar = 0;
	}
	
	~GMenuPrivate()
	{
		DeleteObj(Bar);
	}	
};

GMenu::GMenu()
{
	d = new GMenuPrivate;
	Window = NULL;
	Menu = this;
	Info = d->Bar = new BMenuBar(BRect(0, 0, 1, 1), "LGI::GMenu");
}

GMenu::~GMenu()
{
	Detach();
	Info = 0;
	DeleteObj(d);
}

GRect GMenu::GetPos()
{
	GRect r(d->Bar->Frame());
	return r;
}

GFont *GMenu::GetFont()
{
	if (NOT _Font)
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
	bool Status = false;
	Window = p;
	if (Window)
	{
		if (Window->GetWindow())
		{
			Window->GetWindow()->Menu = this;
		}
		Window->Handle()->AddChild(d->Bar);
		Status = true;
	}
	return Status;
}

bool GMenu::Detach()
{
	bool Status = FALSE;

	if (Window)
	{
		if (Window->GetWindow())
		{
			Window->GetWindow()->Menu = 0;
		}
		Window->Handle()->RemoveChild(d->Bar);
		Window = 0;
	}

	return Status;
}

bool GMenu::OnKey(GView *v, GKey &k)
{
	return true;
}

/*
GMenuItem *GMenu::AppendItem(char *Str, int Id, bool Enabled, int Where)
{
	GMenuItem *Item = new GMenuItem();
	if (Item)
	{
		if (Window AND Window->_View)
		{
			Item->Info->SetTarget(Window->_View);
		}

		Item->Menu = this;
		Item->Parent = this;
		Items.Insert(Item, Where);
		Item->Position = Items.IndexOf(Item);
		AddItem(Item->Info, Item->Position);
		
		char *s = NewStrLessAnd(Str);
		if (s)
		{
			Item->Name(s);
			DeleteArray(s);
		}

		Item->Id(Id);
		Item->Enabled(Enabled);
	}
	return Item;
}

GMenuItem *GMenu::AppendSeparator(int Where)
{
	GMenuItem *Item = new GMenuItem(new BSeparatorItem);
	if (Item)
	{
		Item->Menu = this;
		Item->Parent = this;
		Items.Insert(Item, Where);
		Item->Position = Items.IndexOf(Item);
		AddItem(Item->Info, Item->Position);
	}
	return NULL;
}

GSubMenu *GMenu::AppendSub(char *Str, int Where)
{
	GSubMenu *Sub = 0;
	GMenuItem *Item = new GMenuItem(Sub = new GSubMenu(Str));
	if (Item)
	{
		Item->Menu = this;
		Item->Parent = this;
		Items.Insert(Item, Where);
		Item->Position = Items.IndexOf(Item);

		Sub->Window = Window;
		Sub->Parent = Item;

		AddItem(Item->Info, Item->Position);
	}

	return Sub;
}

void GMenu::Empty()
{
	GMenuItem *i = 0;
	while (i = Items.First())
	{
		BMenu::RemoveItem(i->Info);
		DeleteObj(i);
	}
}

bool GMenu::RemoveItem(int i)
{
	bool Status = false;
	GMenuItem *Item = Items.ItemAt(i);
	if (Item)
	{
		Status = BMenu::RemoveItem(Item->Info);
	}
	return Status;
}

bool GMenu::RemoveItem(GMenuItem *Item)
{
	bool Status = false;
	if (Item AND Items.HasItem(Item))
	{
		Status = BMenu::RemoveItem(Item->Info);
	}
	return Status;
}

GMenuItem *GMenu::FindItem(int Id)
{
	for (int n=0; n<Items.GetItems(); n++)
	{
		GMenuItem *i = Items.ItemAt(n);
		if (i)
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
	}

	return 0;
}
*/

///////////////////////////////////////////////////////////////////////////////////////////
GCommand::GCommand()
{
	Flags = GWF_ENABLED | GWF_VISIBLE;
	Id = 0;
	ToolButton = 0;
	MenuItem = 0;
	Accelerator = 0;
	TipHelp = 0;
}

GCommand::~GCommand()
{
	DeleteArray(Accelerator);
	DeleteArray(TipHelp);
}

bool GCommand::Enabled()
{
	if (ToolButton)
	{
		return ToolButton->Enabled();
	}
	else if (MenuItem)
	{
		return MenuItem->Enabled();
	}
	
	return false;
}

void GCommand::Enabled(bool e)
{
	if (Enabled() != e)
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
}

bool GCommand::Value()
{
	bool v = FALSE;
	if (ToolButton)
	{
		v |= ToolButton->Value();
	}
	if (MenuItem)
	{
		v |= MenuItem->Checked();
	}
	return v;
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
}
