/*
**	FILE:			LgiRes_Menu.h
**	AUTHOR:			Matthew Allen
**	DATE:			6/3/2000
**	DESCRIPTION:	Menu Resource Editor
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#ifndef __LGIRES_MENU_H
#define __LGIRES_MENU_H

#include "Res.h"
#include "LgiRes_String.h"

class ResMenu;
class ResMenuUi;

class ResMenuItem : public GTreeItem, public FieldSource
{
	ResMenu *Menu;
	bool Sep;
	bool Enabled;
	GAutoString Short;
	ResString *_Str;

public:
	ResString *GetStr();

	ResMenuItem(ResMenu *menu);
	~ResMenuItem();

	bool Separator() { return Sep; }
	void Separator(bool i) { Sep = i; }
	bool Enable() { return Enabled; }
	void Enable(bool i) { Enabled = i; }
	char *Shortcut() { return Short; }
	void Shortcut(char *s) { Short.Reset(NewStr(s)); }

	char *GetText(int i=0);
	ResMenu *GetMenu() { return Menu; }
	ResMenuItem *FindByRef(int Ref);
	bool GetFields(FieldTree &Fields);
	bool Serialize(FieldTree &Fields);

	bool Read(GXmlTag *t, ResMenuItem *Parent = 0);
	bool Write(GXmlTag *t, int Tabs);

	void OnMouseClick(GMouse &m);
	bool OnNew();
};

class ResMenu : public Resource, public GTree
{
	friend class ResMenuItem;
	friend class AppWnd;
	friend class ResMenuUi;

protected:
	ResMenuUi *Ui;
	ResStringGroup *Group;

public:
	ResMenu(AppWnd *w, int type = TYPE_MENU);
	~ResMenu();

	void Create(GXmlTag *load, SerialiseContext *Ctx);
	GView *Wnd() { return dynamic_cast<GView*>(this); }
	void SetLanguages() { if (Group) Group->SetLanguages(); }
	List<ResString> *GetStrs() { return (Group)?Group->GetStrs():0; }
	ResString *GetStringByRef(int Ref);
	ResMenuItem *GetItemByRef(int Ref);
	void EnumItems(List<ResMenuItem> &Items);
	ResMenu *IsMenu() { return this; }
	void OnShowLanguages();
	ResStringGroup *GetStringGroup() { return Group; }

	// Tree
	void OnItemClick(GTreeItem *Item, GMouse &m);
	void OnItemBeginDrag(GTreeItem *Item, int Flags);
	void OnItemExpand(GTreeItem *Item, bool Expand);
	void OnItemSelect(GTreeItem *Item);

	// Resource
	GView *CreateUI();
	void OnRightClick(GSubMenu *RClick);
	void OnCommand(int Cmd);
	int OnCommand(int Cmd, int Event, OsView hWnd);

	// Serialize
	bool Test(ErrorCollection *e);
	bool Read(GXmlTag *t, SerialiseContext &Ctx);
	bool Write(GXmlTag *t, SerialiseContext &Ctx);
};

class ResMenuUi : public GLayout
{
	GToolBar *Tools;
	ResMenu *Menu;
	GStatusBar *Status;
	GStatusPane *StatusInfo;

public:
	ResMenuUi(ResMenu *Res);
	~ResMenuUi();

	void OnPaint(GSurface *pDC);
	void PourAll();
	void OnPosChange();
	void OnCreate();
	GMessage::Result OnEvent(GMessage *Msg);

	int CurrentTool();
	void SelectTool(int i);
};

#endif
