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

#include "GXml.h"
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

public:
	ResString Str;

	ResMenuItem(ResMenu *menu);

	bool Separator() { return Sep; }
	void Separator(bool i) { Sep = i; }
	bool Enable() { return Enabled; }
	void Enable(bool i) { Enabled = i; }
	char *Shortcut() { return Short; }
	void Shortcut(char *s) { Short.Reset(NewStr(s)); }

	char *GetText(int i=0);
	ResMenu *GetMenu() { return Menu; }
	bool GetFields(FieldTree &Fields);
	bool Serialize(FieldTree &Fields);

	bool Read(GXmlTag *t, ResMenuItem *Parent = 0);
	bool Write(GXmlTag *t, int Tabs);

	void OnMouseClick(GMouse &m);
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

	void Create(GXmlTag *load);
	GView *Wnd() { return dynamic_cast<GView*>(this); }
	void SetLanguages() { if (Group) Group->SetLanguages(); }
	List<ResString> *GetStrs() { return (Group)?Group->GetStrs():0; }
	ResStringGroup *GetGroup() { return Group; }
	ResString *GetStringByRef(int Ref);
	void EnumItems(List<ResMenuItem> &Items);
	ResMenu *IsMenu() { return this; }
	void OnShowLanguages();

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
	bool Read(GXmlTag *t, ResFileFormat Format);
	bool Write(GXmlTag *t, ResFileFormat Format);
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
	void Pour();
	void OnPosChange();
	void OnCreate();
	int OnEvent(GMessage *Msg);

	int CurrentTool();
	void SelectTool(int i);
};

#endif
