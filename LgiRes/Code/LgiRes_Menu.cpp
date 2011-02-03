/*
**	FILE:			LgiRes_Menu.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			6/3/2000
**	DESCRIPTION:	Menu Resource Editor
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_Menu.h"
#include "GClipBoard.h"

#define IDC_TREE				100

//////////////////////////////////////////////////////////////////
#define VAL_Separator			"Sep"
#define VAL_Enabled				"Enabled"
#define VAL_Shortcut			"Shortcut"

ResMenuItem::ResMenuItem(ResMenu *menu)
{
	Sep = false;
	Enabled = true;
	Menu = menu;
	_Str = 0;
}

ResMenuItem::~ResMenuItem()
{
	if (_Str)
		Menu->GetStringGroup()->DeleteStr(_Str);
}

ResString *ResMenuItem::GetStr()
{
	return _Str;
}

bool ResMenuItem::OnNew()
{
	_Str = Menu->GetStringGroup()->CreateStr();
	if (!_Str)
		return false;

	if (!_Str->GetDefine())
	{
		int Uid = Menu->App()->GetUniqueCtrlId();
		char Def[256];
		sprintf(Def, "IDM_MENU_%i", Uid);
		_Str->SetDefine(Def);
		_Str->SetId(Uid);
	}

	return true;
}

void ResMenuItem::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		ResString *Str = GetStr();
		if (Str)
		{
			GSubMenu RClick;
			bool PasteData = false;
			bool PasteTranslations = false;

			{
				GClipBoard c(Tree);
				char *Clip = c.Text();
				if (Clip)
				{
					PasteTranslations = strstr(Clip, TranslationStrMagic);

					char *p = Clip;
					while (*p && strchr(" \r\n\t", *p)) p++;
					PasteData = *p == '<';
					DeleteArray(Clip);
				}
			}

			RClick.AppendItem("Copy Text", IDM_COPY_TEXT, true);
			RClick.AppendItem("Paste Text", IDM_PASTE_TEXT, PasteTranslations);
			RClick.AppendSeparator();
			RClick.AppendItem("New Id", IDM_NEW_ID, true);

			if (Tree->GetMouse(m, true))
			{
				switch (RClick.Float(Tree, m.x, m.y))
				{
					case IDM_COPY_TEXT:
					{
						Str->CopyText();
						break;
					}
					case IDM_PASTE_TEXT:
					{
						Str->PasteText();
						break;
					}
					case IDM_NEW_ID:
					{
						Str->NewId();
						break;
					}
				}
			}
		}
	}
}

char *ResMenuItem::GetText(int i)
{
	if (Sep)
	{
		return "--------------";
	}

	ResString *Str = GetStr();
	char *s = Str ? Str->Get() : 0;
	return (s) ? s : (char*)"<null>";
}

bool ResMenuItem::GetFields(FieldTree &Fields)
{
	ResString *Str = GetStr();
	if (!Str)
		return false;

	Str->GetFields(Fields);
	Fields.Insert(this, DATA_BOOL, 300, VAL_Separator, "Separator");
	Fields.Insert(this, DATA_BOOL, 301, VAL_Enabled, "Enabled");
	Fields.Insert(this, DATA_STR, 301, VAL_Shortcut, "Shortcut");
	return true;
}

bool ResMenuItem::Serialize(FieldTree &Fields)
{
	ResString *Str = GetStr();
	if (!Str)
		return false;

	Str->Serialize(Fields);
	
	if (Fields.GetMode() == FieldTree::ObjToUi)
	{
		if (Str->GetRef() == 0)
			Str->SetRef(Str->GetGroup()->UniqueRef());
		
		if (ValidStr(Str->GetDefine()) && Str->GetId() == 0)
			Str->SetId(Str->GetGroup()->UniqueId(Str->GetDefine()));
	}

	Fields.Serialize(this, VAL_Separator, Sep);
	Fields.Serialize(this, VAL_Enabled, Enabled);
	Fields.Serialize(this, VAL_Shortcut, Short);

	return true;
}

bool ResMenuItem::Read(GXmlTag *t, ResMenuItem *Parent)
{
	bool Status = false;
	if (t)
	{
		bool SubMenu = stricmp(t->Tag, "submenu") == 0;
		bool MenuItem = stricmp(t->Tag, "menuitem") == 0;

		if (SubMenu OR MenuItem)
		{
			// Read item
			char *n = 0;
			Sep = (n = t->GetAttr("sep")) ? atoi(n) : false;
			Enabled = (n = t->GetAttr("enabled")) ? atoi(n) : true;
			Short.Reset(NewStr(t->GetAttr("shortcut")));

			if ((n = t->GetAttr("ref")) &&
				Menu &&
				Menu->Group)
			{
				int Ref = atoi(n);
				_Str = Menu->GetStringByRef(Ref);
				LgiAssert(_Str);
				if (!_Str)
				{
					if (!(_Str = Menu->GetStringGroup()->CreateStr()))
					{
						LgiAssert(!"Create str failed.");
					}
				}
			}

			// Attach item
			Status = MenuItem;
			if (Parent)
			{
				Parent->Insert(this);
			}
			else if (Menu)
			{
				Menu->Insert(this);
			}

			// Read sub items
			if (SubMenu)
			{
				for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
				{
					ResMenuItem *i = new ResMenuItem(Menu);
					if (i && i->Read(c, this))
					{
						Status = true;
					}
					else
					{
						LgiAssert(0);
						DeleteObj(i);
					}
				}
			}
		}
	}

	return Status;
}

bool ResMenuItem::Write(GXmlTag *t, int Tabs)
{
	bool SubMenu = GetChild() != 0;
	t->Tag = NewStr(SubMenu ? (char*) "submenu" : (char*) "menuitem");
	if (Sep)
	{
		t->SetAttr("Sep", 1);
	}
	else
	{
		if (!Enabled) t->SetAttr("Enabled", 0);
		t->SetAttr("Ref", _Str->GetRef());
		if (Short) t->SetAttr("Shortcut", Short);
		else t->DelAttr("Shortcut");
	}

	if (SubMenu)
	{
		for (ResMenuItem *i = dynamic_cast<ResMenuItem*>(GetChild()); i; i = dynamic_cast<ResMenuItem*>(i->GetNext()))
		{
			GXmlTag *c = new GXmlTag;
			if (c && i->Write(c, 0))
			{
				t->InsertTag(c);
			}
			else
			{
				DeleteObj(c);
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////
ResMenu::ResMenu(AppWnd *w, int type) :
	Resource(w, type),
	GTree(IDC_TREE, 0, 0, 200, 200, "Menu resource")
{
	Sunken(false);
	Group = new ResStringGroup(w);
}

ResMenu::~ResMenu()
{
	Empty();
	DeleteObj(Group);
}

void ResMenu::OnShowLanguages()
{
	if (Group)
	{
		Invalidate();
	}
}

void AddChildren(GTreeItem *i, List<ResMenuItem> &Items)
{
	for (GTreeItem *c = i->GetChild(); c; c = c->GetNext())
	{
		ResMenuItem *m = dynamic_cast<ResMenuItem*>(c);
		if (m)
		{
			Items.Insert(m);
		}
		AddChildren(c, Items);
	}
}

void AddChildren(GTree *i, List<ResMenuItem> &Items)
{
	for (GTreeItem *c = i->GetChild(); c; c = c->GetNext())
	{
		ResMenuItem *m = dynamic_cast<ResMenuItem*>(c);
		if (m)
		{
			Items.Insert(m);
		}
		AddChildren(c, Items);
	}
}

void ResMenu::EnumItems(List<ResMenuItem> &Items)
{
	AddChildren(this, Items);
}

void ResMenu::Create(GXmlTag *load)
{
	Name("IDM_MENU");

	if (load)
		Read(load, Lr8File);

	if (Resource::Item) Resource::Item->Update();
	Visible(true);
}

GView *ResMenu::CreateUI()
{
	return Ui = new ResMenuUi(this);
}

void ResMenu::OnItemClick(GTreeItem *Item, GMouse &m)
{
	GTree::OnItemClick(Item, m);
}

void ResMenu::OnItemBeginDrag(GTreeItem *Item, int Flags)
{
	GTree::OnItemBeginDrag(Item, Flags);
}

void ResMenu::OnItemExpand(GTreeItem *Item, bool Expand)
{
	GTree::OnItemExpand(Item, Expand);
}

void ResMenu::OnItemSelect(GTreeItem *Item)
{
	GTree::OnItemSelect(Item);

	if (AppWindow)
	{
		ResMenuItem *Item = dynamic_cast<ResMenuItem*>(Selection());
		AppWindow->OnObjSelect(Item);
	}
}

void ResMenu::OnRightClick(GSubMenu *RClick)
{
}

void ResMenu::OnCommand(int Cmd)
{
}

int ResMenu::OnCommand(int Cmd, int Event, OsView hWnd)
{
	switch (Cmd)
	{
		case IDM_NEW_SUB:
		{
			ResMenuItem *Item = dynamic_cast<ResMenuItem*>(Selection());
			GTreeItem *New = 0;

			ResMenuItem *NewItem = new ResMenuItem(this);
			if (NewItem && !NewItem->OnNew())
			{
				DeleteObj(NewItem);
				LgiMsg(GetTree(), "Unrecoverable error creating menu item (corrupt refs?)", AppName);
				return -1;
			}

			if (Item)
				New = Item->Insert(NewItem, 0);
			else
				New = Insert(NewItem, 0);

			if (New)
			{
				Select(New);
				AppWindow->SetDirty(true);
			}
			break;
		}
		case IDM_NEW_ITEM:
		{
			ResMenuItem *Item = dynamic_cast<ResMenuItem*>(Selection());
			if (Item)
			{
				int n = Item->IndexOf();
				if (n >= 0) n++;

				GTreeItem *Parent = Item->GetParent();
				GTreeItem *New = 0;
				ResMenuItem *NewItem = new ResMenuItem(this);
				if (NewItem && !NewItem->OnNew())
				{
					DeleteObj(NewItem);
					LgiMsg(GetTree(), "Unrecoverable error creating menu item (corrupt refs?)", AppName);
					return -1;
				}

				if (Parent)
				{
					New = Parent->Insert(NewItem, n);
				}
				else
				{
					New = Insert(NewItem, n);
				}

				if (New)
				{
					Select(New);
					AppWindow->SetDirty(true);
				}
			}
			break;
		}
		case IDM_DELETE_ITEM:
		{
			ResMenuItem *Item = dynamic_cast<ResMenuItem*>(Selection());
			if (Item)
			{
				Remove(Item);
				DeleteObj(Item);
				AppWindow->SetDirty(true);
			}
			break;
		}
		case IDM_MOVE_UP:
		{
			ResMenuItem *Item = dynamic_cast<ResMenuItem*>(Selection());
			if (Item)
			{
				GTreeNode *Parent = 0;
				if (Item->GetParent())
					Parent = Item->GetParent();
				else
					Parent = Item->GetTree();

				if (Parent)
				{
					int n = Item->IndexOf();
					if (n > 0)
					{
						Item->Remove();
						Parent->Insert(Item, n - 1);
						Item->Select(true);
						AppWindow->SetDirty(true);
					}
				}
			}
			break;
		}
		case IDM_MOVE_DOWN:
		{
			ResMenuItem *Item = dynamic_cast<ResMenuItem*>(Selection());
			if (Item)
			{
				GTreeNode *Parent = 0;
				if (Item->GetParent())
					Parent = Item->GetParent();
				else
					Parent = Item->GetTree();

				if (Parent)
				{
					int n = Item->IndexOf();
					if (n < Parent->GetItems() - 1)
					{
						Item->Remove();
						Parent->Insert(Item, n + 1);
						Item->Select(true);
						AppWindow->SetDirty(true);
					}
				}
			}
			break;
		}
		case IDM_NEW_LANG:
		{
			if (Group)
			{
				Group->OnCommand(IDM_NEW_LANG, 0, 0);
				Invalidate();
				AppWindow->SetDirty(true);
			}
			break;
		}
		case IDM_DELETE_LANG:
		{
			if (Group)
			{
				Group->OnCommand(IDM_DELETE_LANG, 0, 0);
				Invalidate();
				AppWindow->SetDirty(true);
			}
			break;
		}
	}

	return 0;
}

ResMenuItem *ResMenuItem::FindByRef(int Ref)
{
	for (GTreeItem *i = Items.First(); i; i = Items.Next())
	{
		ResMenuItem *m = dynamic_cast<ResMenuItem*>(i);
		if (m)
		{
			ResMenuItem *f = m->FindByRef(Ref);
			if (f)
				return f;
		}
	}

	return 0;
}

ResMenuItem *ResMenu::GetItemByRef(int Ref)
{
	for (GTreeItem *i = Items.First(); i; i = Items.Next())
	{
		ResMenuItem *m = dynamic_cast<ResMenuItem*>(i);
		if (m)
		{
			ResMenuItem *f = m->FindByRef(Ref);
			if (f)
				return f;
		}
	}

	return 0;
}

ResString *ResMenu::GetStringByRef(int Ref)
{
	return Group ? Group->FindRef(Ref) : 0;
}

bool ResMenu::Test(ErrorCollection *e)
{
	return Group->Test(e);
}

bool ResMenu::Read(GXmlTag *t, ResFileFormat Format)
{
	bool Status = true;

	char *p = 0;
	if (p = t->GetAttr("Name"))
	{
		Name(p);
	}

	for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
	{
		if (stricmp(c->Tag, "string-group") == 0)
		{
			if (Group)
			{
				if (!Group->Read(c, Format))
				{
					Status = false;
					LgiAssert(0);
				}
			}
			else
			{
				Status = false;
				LgiAssert(0);
			}
		}
		else if (!stricmp(c->Tag, "submenu") ||
				 !stricmp(c->Tag, "menuitem"))
		{
			ResMenuItem *i = new ResMenuItem(this);
			if (i && i->Read(c))
			{
			}
			else
			{
				Status = false;
				LgiAssert(0);
				DeleteObj(i);
			}
		}
		else
		{
			LgiAssert(!"Unexpected tag.");
			break;
		}
	}

	return Status;
}

bool ResMenu::Write(GXmlTag *t, ResFileFormat Format)
{
	bool Status = true;

	// Write start tag
	t->Tag = NewStr("menu");
	if (Name()) t->SetAttr("Name", Name());

	// Write group
	if (Group)
	{
		GXmlTag *g = new GXmlTag;
		if (g && Group->Write(g, Format))
		{
			t->InsertTag(g);
		}
		else
		{
			Status = false;
			DeleteObj(g);
		}
	}

	// Write submenus/items
	for (GTreeItem *i = Items.First(); i; i = Items.Next())
	{
		ResMenuItem *m = dynamic_cast<ResMenuItem*>(i);
		if (m)
		{
			GXmlTag *c = new GXmlTag;
			if (c && m->Write(c, 1))
			{
				t->InsertTag(c);
			}
			else
			{
				Status = false;
				DeleteObj(c);
			}
		}
	}

	return Status;
}

//////////////////////////////////////////////////////////////////////////
ResMenuUi::ResMenuUi(ResMenu *Res)
{
	Menu = Res;
	Tools = 0;
	Status = 0;
	StatusInfo = 0;
}

ResMenuUi::~ResMenuUi()
{
	if (Menu)
	{
		Menu->Ui = 0;
	}
}

void ResMenuUi::OnPaint(GSurface *pDC)
{
	GRegion Client(0, 0, X()-1, Y()-1);
	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		GRect r = w->GetPos();
		Client.Subtract(&r);
	}

	pDC->Colour(LC_MED, 24);
	for (GRect *r = Client.First(); r; r = Client.Next())
	{
		pDC->Rectangle(r);
	}
}

void ResMenuUi::Pour()
{
	GRegion Client(GetClient());
	GRegion Update;

	for (GViewI *v = Children.First(); v; v = Children.Next())
	{
		GRect OldPos = v->GetPos();
		Update.Union(&OldPos);

		if (v->Pour(Client))
		{
			if (!v->Visible())
			{
				v->Visible(true);
			}

			if (OldPos != v->GetPos())
			{
				// position has changed update...
				v->Invalidate();
			}

			Client.Subtract(&v->GetPos());
			Update.Subtract(&v->GetPos());
		}
		else
		{
			// make the view not visible
			v->Visible(false);
		}
	}

	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
}

void ResMenuUi::OnPosChange()
{
	Pour();
}

void ResMenuUi::OnCreate()
{
	Tools = new GToolBar;
	if (Tools)
	{
		char *FileName = LgiFindFile("_MenuIcons.gif");
		if (FileName && Tools->SetBitmap(FileName, 16, 16))
		{
			Tools->Attach(this);

			Tools->AppendButton("New child menu", IDM_NEW_SUB);
			Tools->AppendButton("New menu item", IDM_NEW_ITEM);
			Tools->AppendButton("Delete menu item", IDM_DELETE_ITEM);
			Tools->AppendButton("Move item up", IDM_MOVE_UP);
			Tools->AppendButton("Move item down", IDM_MOVE_DOWN);
			Tools->AppendSeparator();

			Tools->AppendButton("New language", IDM_NEW_LANG, TBT_PUSH, true, 6);
			Tools->AppendButton("Delete language", IDM_DELETE_LANG, TBT_PUSH, true, 7);
		}
		else
		{
			DeleteObj(Tools);
		}
		DeleteArray(FileName);
	}

	Status = new GStatusBar;
	if (Status)
	{
		Status->Attach(this);
		StatusInfo = Status->AppendPane("", 2);
	}

	ResFrame *Frame = new ResFrame(Menu);
	if (Frame)
	{
		Frame->Attach(this);
	}
}

int ResMenuUi::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_COMMAND:
		{
			Menu->OnCommand(MsgA(Msg)&0xffff, MsgA(Msg)>>16, (OsView) MsgB(Msg));
			break;
		}
		case M_DESCRIBE:
		{
			char *Text = (char*) MsgB(Msg);
			if (Text)
			{
				StatusInfo->Name(Text);
			}
			break;
		}
	}

	return GView::OnEvent(Msg);
}

int ResMenuUi::CurrentTool()
{
	return false;
}

void ResMenuUi::SelectTool(int i)
{
}
