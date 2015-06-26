/*
**	FILE:			LgiRes.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			3/8/99
**	DESCRIPTION:	Lgi Resource Editor
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_Dialog.h"
#include "LgiRes_Menu.h"
#include "GAbout.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GProgressDlg.h"
#include "GTextView3.h"
#include "resdefs.h"
#include "GToken.h"
#include "GDataDlg.h"

char AppName[]				= "Lgi Resource Editor";
char HelpFile[]				= "Help.html";
char OptionsFileName[]		= "Options.r";
char TranslationStrMagic[] = "LgiRes.String";
#define VIEW_PULSE_RATE		100

#ifndef DIALOG_X
#define DIALOG_X					1.56
#define DIALOG_Y					1.85
#define CTRL_X						1.50
#define CTRL_Y						1.64
#endif

const char *TypeNames[] = {
    "",
	"Css",
	"Dialog",
	"String",
	"Menu",
	0};

//////////////////////////////////////////////////////////////////////////////
ResFileFormat GetFormat(char *File)
{
	ResFileFormat Format = Lr8File;
	char *Ext = LgiGetExtension(File);
	if (Ext)
	{
		if (stricmp(Ext, "lr") == 0) Format = CodepageFile;
		else if (stricmp(Ext, "xml") == 0) Format = XmlFile;
	}
	return Format;
}

char *EncodeXml(char *Str, int Len)
{
	char *Ret = 0;
	
	if (Str)
	{
		GStringPipe p;
		
		char *s = Str;
		for (char *e = Str; e && *e && (Len < 0 || ((e-Str) < Len)); )
		{
			switch (*e)
			{
				case '<':
				{
					p.Push(s, e-s);
					p.Push("&lt;");
					s = ++e;
					break;
				}
				case '>':
				{
					p.Push(s, e-s);
					p.Push("&gt;");
					s = ++e;
					break;
				}
				case '&':
				{
					p.Push(s, e-s);
					p.Push("&amp;");
					s = ++e;
					break;
				}
				case '\\':
				{
					if (e[1] == 'n')
					{
						// Newline
						p.Push(s, e-s);
						p.Push("\n");
						s = (e += 2);
						break;
					}
					
					// fall thru
				}
				case '\'':
				case '\"':
				case '/':
				{
					// Convert to entity
					p.Push(s, e-s);
					
					char b[32];
					sprintf(b, "&#%i;", *e);
					p.Push(b);

					s = ++e;
					break;
				}
				default:
				{
					// Regular character
					e++;
					break;
				}
			}
		}	
		p.Push(s);
		
		Ret = p.NewStr();
	}

	return Ret;
}

char *DecodeXml(char *Str, int Len)
{
	if (Str)
	{
		GStringPipe p;
		
		char *s = Str;
		for (char *e = Str; e && *e && (Len < 0 || ((e-Str) < Len)); )
		{
			switch (*e)
			{
				case '&':
				{
					// Store string up to here
					p.Push(s, e-s);

					e++;
					if (*e == '#')
					{
						// Numerical
						e++;
						if (*e == 'x' || *e == 'X')
						{
							// Hex
							e++;
							
							char16 c = htoi(e);
							char *c8 = LgiNewUtf16To8(&c, sizeof(char16));
							if (c8)
							{
								p.Push(c8);
								DeleteArray(c8);
							}
						}
						else if (isdigit(*e))
						{
							// Decimal
							char16 c = atoi(e);
							char *c8 = LgiNewUtf16To8(&c, sizeof(char16));
							if (c8)
							{
								p.Push(c8);
								DeleteArray(c8);
							}
						}
						else
						{
							LgiAssert(0);
						}

						while (*e && *e != ';') e++;
					}
					else if (isalpha(*e))
					{
						// named entity
						char *Name = e;
						while (*e && *e != ';') e++;
						int Len = e - Name;
						if (Len == 3 && strnicmp(Name, "amp", Len) == 0)
						{
							p.Push("&");
						}
						else if (Len == 2 && strnicmp(Name, "gt", Len) == 0)
						{
							p.Push(">");
						}
						else if (Len == 2 && strnicmp(Name, "lt", Len) == 0)
						{
							p.Push("<");
						}
						else
						{
							// Unsupported entity
							LgiAssert(0);
						}
					}
					else
					{
						LgiAssert(0);
						while (*e && *e != ';') e++;
					}
					
					s = ++e;
					break;
				}
				case '\n':
				{
					p.Push(s, e-s);
					p.Push("\\n");
					s = ++e;
					break;
				}
				default:
				{
					e++;
					break;
				}
			}
		}

		p.Push(s);
		
		return p.NewStr();	
	}
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
Resource::Resource(AppWnd *w, int t, bool enabled)
{
	AppWindow = w;
	ResType = t;
	Item = 0;
	SysObject = false;

	LgiAssert(AppWindow);
}

Resource::~Resource()
{
	if (Item)
	{
		Item->Obj = 0;
		DeleteObj(Item);
	}
}

bool Resource::IsSelected()
{
	return Item?Item->Select():false;
}

bool Resource::Attach(GViewI *Parent)
{
	GView *w = Wnd();
	if (w)
	{
		return w->Attach(Parent);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
ResFolder::ResFolder(AppWnd *w, int t, bool enabled) :
	Resource(w, t, enabled)
{
	Wnd()->Name("<untitled>");
	Wnd()->Enabled(enabled);
}

//////////////////////////////////////////////////////////////////////////////
ObjTreeItem::ObjTreeItem(Resource *Object)
{
	if ((Obj = Object))
	{
		Obj->Item = this;
		
		if (dynamic_cast<ResFolder*>(Object))
		    SetImage(ICON_FOLDER);
		else
		{
		    int t = Object->Type();
		    switch (t)
		    {
		        case TYPE_CSS: SetImage(ICON_CSS); break;
		        case TYPE_DIALOG: SetImage(ICON_DIALOG); break;
		        case TYPE_STRING: SetImage(ICON_STRING); break;
		        case TYPE_MENU: SetImage(ICON_MENU); break;
		    }
		}
	}
}

ObjTreeItem::~ObjTreeItem()
{
	if (Obj)
	{
		Obj->Item = 0;
		DeleteObj(Obj);
	}
}

char *ObjTreeItem::GetText(int i)
{
	if (Obj)
	{
		int Type = Obj->Type();
		if (Type > 0)
		{
			return Obj->Wnd()->Name();
		}
		else
		{
			return (char*)TypeNames[-Type];
		}
	}

	return (char*)"#NO_OBJ";
}

void ObjTreeItem::OnSelect()
{
	if (Obj)
	{
		Obj->App()->OnResourceSelect(Obj);
	}
}

void ObjTreeItem::OnMouseClick(GMouse &m)
{
	if (!Obj) return;
	if (m.IsContextMenu())
	{
		Tree->Select(this);
		GSubMenu RClick;

		if (Obj->Wnd()->Enabled())
		{
			if (Obj->Type() > 0)
			{
				// Resource
				RClick.AppendItem("Delete", IDM_DELETE, !Obj->SystemObject());
				RClick.AppendItem("Rename", IDM_RENAME, !Obj->SystemObject());
			}
			else
			{
				// Folder
				RClick.AppendItem("New", IDM_NEW, true);
				RClick.AppendSeparator();
				GSubMenu *Insert = RClick.AppendSub("Import from...");
				if (Insert)
				{
					Insert->AppendItem("Lgi File", IDM_IMPORT, true);
					Insert->AppendItem("Win32 Resource Script", IDM_IMPORT_WIN32, false);
				}
			}

			// Custom entries
			if (!Obj->SystemObject())
			{
				Obj->OnRightClick(&RClick);
			}
		}
		else
		{
			RClick.AppendItem("Not implemented", 0, false);
		}

		if (Tree->GetMouse(m, true))
		{
			int Cmd = 0;
			switch (Cmd = RClick.Float(Tree, m.x, m.y))
			{
				case IDM_NEW:
				{
					SerialiseContext Ctx;
					Obj->App()->NewObject(Ctx, 0, -Obj->Type());
					break;
				}
				case IDM_DELETE:
				{
					Obj->App()->SetDirty(true);
					Obj->App()->DelObject(Obj);
					break;
				}
				case IDM_RENAME:
				{
					GInput Dlg(Tree, GetText(), "Enter the name for the object", "Object Name");
					if (Dlg.DoModal())
					{
						Obj->Wnd()->Name(Dlg.Str);
						Update();
						Obj->App()->SetDirty(true);
					}
					break;
				}
				case IDM_IMPORT:
				{
					GFileSelect Select;
					Select.Parent(Obj->App());
					Select.Type("Text", "*.txt");
					if (Select.Open())
					{
						GFile F;
						if (F.Open(Select.Name(), O_READ))
						{
							SerialiseContext Ctx;
							Resource *Res = Obj->App()->NewObject(Ctx, 0, -Obj->Type());
							if (Res)
							{
								// TODO
								// Res->Read();
							}
						}
						else
						{
							LgiMsg(Obj->App(), "Couldn't open file for reading.");
						}
					}
					break;
				}
				case IDM_IMPORT_WIN32:
				{
					/*
					List<ResDialog> l;
					if (ImportWin32Dialogs(l, MainWnd))
					{
						for (ResDialog *r = l.First(); r; r = l.Next())
						{
							Obj->App()->InsertObject(TYPE_DIALOG, r);
						}
					}
					*/
					break;
				}
				default:
				{
					Obj->OnCommand(Cmd);
					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
FieldView::FieldView(AppWnd *app) : Fields(NextId, true)
{
	NextId = 100;
	App = app;
	Source = 0;
	Ignore = true;

	SetTabStop(true);
	#ifdef WIN32
	SetExStyle(GetExStyle() | WS_EX_CONTROLPARENT);
	#endif
}

FieldView::~FieldView()
{
}

void FieldView::Serialize(bool Write)
{
	if (!Source)
		return;

	Ignore = !Write;
	
	Fields.SetMode(Write ? FieldTree::UiToObj : FieldTree::ObjToUi);
	Fields.SetView(this);
	Source->Serialize(Fields);

	/*
	for (DataDlgField *f=Fields.First(); f; f=Fields.Next())
	{
		GViewI *v;
		if (GetViewById(f->GetCtrl(), v))
		{
			switch (f->GetType())
			{
				case DATA_STR:
				{
					if (Write) // Ctrl -> Options
					{
						char *s = v->Name();
						Options->Set(f->GetOption(), s);
					}
					else // Options -> Ctrl
					{
						char *s = 0;
						Options->Get(f->GetOption(), s);
						v->Name(s?s:(char*)"");
					}
					break;
				}
				case DATA_BOOL:
				case DATA_INT:
				{
					if (Write) // Ctrl -> Options
					{
						char *s = v->Name();
						if (s && (s = strchr(s, '\'')))
						{
							s++;

							char *e = strchr(s, '\'');
							int i = 0;
							if (e - s == 4)
							{
								memcpy(&i, s, 4);
								i = LgiSwap32(i);
								Options->Set(f->GetOption(), i);
							}
						}
						else
						{
							int i = v->Value();
							Options->Set(f->GetOption(), i);
						}
					}
					else // Options -> Ctrl
					{
						int i = 0;
						Options->Get(f->GetOption(), i);
						if (i != -1 && (i & 0xff000000) != 0)
						{
							char m[8];
							i = LgiSwap32(i);
							sprintf(m, "'%04.4s'", &i);
							v->Name(m);
						}
						else
						{
							v->Value(i);
						}
					}
					break;
				}
				case DATA_FLOAT:
				case DATA_PASSWORD:
				case DATA_STR_SYSTEM:
				default:
				{
					LgiAssert(0);
					break;
				}
			}
		}
		else LgiAssert(0);
	}
	*/

	Ignore = false;
}

class TextViewEdit : public GTextView3
{
public:
    bool Multiline;

	TextViewEdit(	int Id,
					int x,
					int y,
					int cx,
					int cy,
					GFontType *FontInfo = 0) :
		GTextView3(Id, x, y, cx, cy, FontInfo)
	{
	    Multiline = false;
		#ifdef WIN32
		SetDlgCode(DLGC_WANTARROWS | DLGC_WANTCHARS);
		#endif
	}

	bool OnKey(GKey &k)
	{
		if (!Multiline && (k.c16 == '\t' || k.c16 == VK_RETURN))
		{
			return false;
		}

		return GTextView3::OnKey(k);
	}
};

class Hr : public GView
{
public:
	Hr(int x1, int y, int x2)
	{
		GRect r(x1, y, x2, y+1);
		SetPos(r);
	}

	void OnPaint(GSurface *pDC)
	{
		GRect c = GetClient();
		LgiThinBorder(pDC, c, DefaultSunkenEdge);
	}
};

void FieldView::OnSelect(FieldSource *s)
{
	Ignore = true;

	if (Source)
	{
		// Clear fields
		Source->_FieldView = 0;
		Fields.Empty();

		// remove all children
		for (GViewI *c = Children.First(); c; c = Children.First())
		{
			c->Detach();
			DeleteObj(c);
		}
		
		Source = 0;
	}

	if (s)
	{
		// Add new fields
		Source = s;
		Source->_FieldView = Handle();

		if (Source->GetFields(Fields))
		{
			GFontType Sys;
			Sys.GetSystemFont("System");
			int Dy = SysFont->GetHeight() + 14;
			int y = 10;

			GArray<FieldTree::FieldArr*> a;
			Fields.GetAll(a);
			for (int i=0; i<a.Length(); i++)
			{
				FieldTree::FieldArr *b = a[i];
				for (int n=0; n<b->Length(); n++)
				{
					FieldTree::Field *c = (*b)[n];

					switch (c->Type)
					{
						case DATA_STR:
						case DATA_FLOAT:
						case DATA_INT:
						{
							AddView(new GText(-1, 10, y + 4, 50, 20, c->Label));
							TextViewEdit *Tv;
							AddView(Tv = new TextViewEdit(c->Id, 70, y, 100, c->Multiline ? SysFont->GetHeight() * 8 : Dy - 8, &Sys));
							if (Tv)
							{
							    if ((Tv->Multiline = c->Multiline))
							        y += Tv->Y() + 8 - Dy;
								Tv->SetWrapType(TEXTED_WRAP_NONE);
								Tv->Sunken(true);
							}
							break;
						}
						case DATA_BOOL:
						{
							AddView(new GCheckBox(c->Id, 70, y, 100, 20, c->Label));
							break;
						}
					}

					y += Dy;
				}

				if (i < a.Length() - 1)
				{
					AddView(new Hr(10, y, X()-1));
					y += 10;
				}
			}

			AttachChildren();
			OnPosChange();
		}

		Serialize(false);
		Ignore = false;
	}
}

void FieldView::OnPosChange()
{
	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		GText *e = dynamic_cast<GText*>(w);
		if (!e)
		{
			GRect r = w->GetPos();
			r.x2 = X()-10;
			w->SetPos(r);
		}
	}
}

GMessage::Result FieldView::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_OBJECT_CHANGED:
		{
			FieldSource *Src = (FieldSource*)MsgA(m);
			if (Src == Source)
			{
				Fields.SetMode(FieldTree::ObjToUi);
				Fields.SetView(this);
				Serialize(false);
			}
			else LgiAssert(0);
			break;
		}
	}

	return GLayout::OnEvent(m);
}

int FieldView::OnNotify(GViewI *Ctrl, int Flags)
{
	if (!Ignore)
	{
		GTextView3 *Tv = dynamic_cast<GTextView3*>(Ctrl);
		if (Tv && Flags == GTVN_CURSOR_CHANGED)
		{
			return 0;
		}

		GArray<FieldTree::FieldArr*> a;
		Fields.GetAll(a);
		for (int i=0; i<a.Length(); i++)
		{
			FieldTree::FieldArr *b = a[i];
			for (int n=0; n<b->Length(); n++)
			{
				FieldTree::Field *c = (*b)[n];
				if (c->Id == Ctrl->GetId())
				{
					// Write the value back to the objects
					Fields.SetMode(FieldTree::UiToObj);
					Fields.SetView(this);
					Source->Serialize(Fields);

					// Escape the loops
					i = a.Length();
					n = b->Length();
					break;
				}
			}
		}
	}

	return 0;
}

void FieldView::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

//////////////////////////////////////////////////////////////////////////////
ObjContainer::ObjContainer(AppWnd *w) :
	GTree(100, 0, 0, 100, 100, "LgiResObjTree")
{
	Window = w;
	Sunken(false);

	Insert(Style   = new ObjTreeItem( new ResFolder(Window, -TYPE_CSS)));
	Insert(Dialogs = new ObjTreeItem( new ResFolder(Window, -TYPE_DIALOG)));
	Insert(Strings = new ObjTreeItem( new ResFolder(Window, -TYPE_STRING)));
	Insert(Menus   = new ObjTreeItem( new ResFolder(Window, -TYPE_MENU)));

	const char *IconFile = "_icons.gif";
	GAutoString f(LgiFindFile(IconFile));
	if (f)
	{
		Images = LgiLoadImageList(f, 16, 16);
		if (Images)
			SetImageList(Images, false);
		else
			LgiTrace("%s:%i - failed to load '%s'\n", _FL, IconFile);
	}
}

ObjContainer::~ObjContainer()
{
	DeleteObj(Images);
}

bool ObjContainer::AppendChildren(ObjTreeItem *Res, List<Resource> &Lst)
{
	bool Status = true;
	if (Res)
	{
		GTreeItem *Item = Res->GetChild();
		while (Item)
		{
			ObjTreeItem *i = dynamic_cast<ObjTreeItem*>(Item);
			if (i) Lst.Insert(i->GetObj());
			else Status = false;
			Item = Item->GetNext();
		}
	}
	return Status;
}

bool ObjContainer::ListObjects(List<Resource> &Lst)
{
	bool Status = AppendChildren(Style, Lst);
	Status &= AppendChildren(Dialogs, Lst);
	Status &= AppendChildren(Strings, Lst);
	Status &= AppendChildren(Menus, Lst);
	return Status;
}

//////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
char *Icon = MAKEINTRESOURCE(IDI_ICON1);
#else
char *Icon = "icon.png";
#endif

AppWnd::AppWnd() :
	GDocApp<GOptionsFile>(AppName, Icon)
{
	LastRes = 0;
	Fields = 0;
	ViewMenu = 0;
	CurLang = -1;
	ShowLanguages.Add("en", true);

	if (_Create())
	{
		GVariant Langs;
		if (GetOptions()->GetValue(OPT_ShowLanguages, Langs))
		{
			ShowLanguages.Empty();
			GToken L(Langs.Str(), ",");
			for (int i=0; i<L.Length(); i++)
			{
				ShowLanguages.Add(L[i], true);
			}
		}
	}
	else printf("%s:%i - _Create failed.\n", _FL);
}

AppWnd::~AppWnd()
{
	OnResourceSelect(NULL);
    Objs->Empty();
	_Destroy();
}

void AppWnd::OnCreate()
{
	SetupUi();
}

void AppWnd::OnLanguagesChange(GLanguageId Lang, bool Add, bool Update)
{
	bool Change = false;

	if (Lang)
	{
		// Update the list....
		bool Has = false;
		for (int i=0; i<Languages.Length(); i++)
		{
			if (stricmp(Languages[i]->Id, Lang) == 0)
			{
				Has = true;

				if (!Add)
				{
					Languages.DeleteAt(i);
					Change = true;
				}
				break;
			}
		}

		if (Add && !Has)
		{
			Change = true;
			Languages.Add(GFindLang(Lang));
		}
	}

	// Update the menu...
	if (ViewMenu && (Change || Update))
	{
		// Remove existing language menu items
		while (ViewMenu->RemoveItem(2));

		// Add new ones
		int n = 0;
		for (int i=0; i<Languages.Length(); i++, n++)
		{
			GLanguage *Lang = Languages[i];
			if (Lang)
			{
				GMenuItem *Item = ViewMenu->AppendItem(Lang->Name, IDM_LANG_BASE + n, true);
				if (Item)
				{
					if (CurLang == i)
					{
						Item->Checked(true);
					}
				}
			}
		}
	}
}

bool AppWnd::ShowLang(GLanguageId Lang)
{
	return ShowLanguages.Find(Lang) != 0;
}

void AppWnd::ShowLang(GLanguageId Lang, bool Show)
{
	// Apply change
	if (Show)
	{
		OnLanguagesChange(Lang, true);
		ShowLanguages.Add(Lang, true);
	}
	else
	{
		ShowLanguages.Delete(Lang);
	}

	// Store the setting for next time
	GStringPipe p;
	const char *L;
	for (bool i = ShowLanguages.First(&L); i; i = ShowLanguages.Next(&L))
	{
		if (p.GetSize()) p.Push(",");
		p.Push(L);
	}
	char *Langs = p.NewStr();
	if (Langs)
	{
		GVariant v;
		GetOptions()->SetValue(OPT_ShowLanguages, v = Langs);
		DeleteArray(Langs);
	}

	// Update everything
	List<Resource> res;
	if (ListObjects(res))
	{
		for (Resource *r = res.First(); r; r = res.Next())
		{
			r->OnShowLanguages();
		}
	}
}

GLanguage *AppWnd::GetCurLang()
{
	if (CurLang >= 0 && CurLang < Languages.Length())
		return Languages[CurLang];

	return GFindLang("en");
}

void AppWnd::SetCurLang(GLanguage *L)
{
	for (int i=0; i<Languages.Length(); i++)
	{
		if (Languages[i]->Id == L->Id)
		{
			// Set new current
			CurLang = i;

			// Update everything
			List<Resource> res;
			if (ListObjects(res))
			{
				for (Resource *r = res.First(); r; r = res.Next())
				{
					r->OnShowLanguages();
				}
			}

			break;
		}
	}
}

GArray<GLanguage*> *AppWnd::GetLanguages()
{
	return &Languages;
}

class Test : public GView
{
	COLOUR c;

public:
	Test(COLOUR col, int x1, int y1, int x2, int y2)
	{
		c = col;
		GRect r(x1, y1, x2, y2);
		SetPos(r);
		_BorderSize = 1;
		Sunken(true);
	}
	
	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(c, 24);
		pDC->Rectangle();
	}
};

void AppWnd::SetupUi()
{
	if (_LoadMenu("IDM_MENU"))
	{
		if (_FileMenu)
		{
			int n = 6;
			_FileMenu->AppendSeparator(n++);
			_FileMenu->AppendItem("Import Win32 Script", IDM_IMPORT_WIN32, true, n++);
			_FileMenu->AppendItem("Import LgiRes Language", IDM_IMPORT_LANG, true, n++);
			_FileMenu->AppendItem("Compare To File...", IDM_COMPARE, true, n++);
			_FileMenu->AppendSeparator(n++);
			_FileMenu->AppendItem("Properties", IDM_PROPERTIES, true, n++);
		}


		ViewMenu = Menu->FindSubMenu(IDM_VIEW);
		LgiAssert(ViewMenu);
	}
	else printf("%s:%i - _LoadMenu failed.\n", _FL);

	Status = 0;
	StatusInfo[0] = StatusInfo[1] = 0;

	MainSplit = new GSplitter;
	if (MainSplit)
	{
		MainSplit->Attach(this);
		MainSplit->Value(240);

		SubSplit = new GSplitter;
		if (SubSplit)
		{
			SubSplit->Raised(false);
			
			MainSplit->SetViewA(SubSplit, false);
			SubSplit->Border(false);
			SubSplit->IsVertical(false);
			SubSplit->Value(200);

			SubSplit->SetViewA(Objs = new ObjContainer(this));
			if (Objs)
			{
				Objs->AskImage(true);
				Objs->AskText(true);
			}
			SubSplit->SetViewB(Fields = new FieldView(this));
		}
	}

	char Opt[256];
	if (LgiApp->GetOption("o", Opt))
	{
		LoadLgi(Opt);
	}

	DropTarget(true);
}

GMessage::Result AppWnd::OnEvent(GMessage *m)
{
	GMru::OnEvent(m);

	switch (MsgCode(m))
	{
		case M_CHANGE:
		{
			return OnNotify((GViewI*) MsgA(m), MsgB(m));
		}
		case M_DESCRIBE:
		{
			char *Text = (char*) MsgA(m);
			if (Text)
			{
				SetStatusText(Text, STATUS_NORMAL);
			}
			break;
		}
	}
	return GWindow::OnEvent(m);
}

#include "GToken.h"
void _CountGroup(ResStringGroup *Grp, int &Words, int &Multi)
{
	for (ResString *s = Grp->GetStrs()->First(); s; s=Grp->GetStrs()->Next())
	{
		if (s->Items.Length() > 1)
		{
			Multi++;

			char *e = s->Get("en");
			if (e)
			{
				GToken t(e, " ");
				Words += t.Length();
			}
		}
	}
}

int AppWnd::OnCommand(int Cmd, int Event, OsView Handle)
{
	SerialiseContext Ctx;
	switch (Cmd)
	{
		case IDM_SHOW_LANG:
		{
			ShowLanguagesDlg Dlg(this);
			break;
		}
		case IDM_NEW_DLG:
		{
			NewObject(Ctx, 0, TYPE_DIALOG);
			break;
		}
		case IDM_NEW_STRGRP:
		{
			NewObject(Ctx, 0, TYPE_STRING);
			break;
		}
		case IDM_NEW_MENU:
		{
			NewObject(Ctx, 0, TYPE_MENU);
			break;
		}
		case IDM_CLOSE:
		{
			Empty();
			break;
		}
		case IDM_IMPORT_WIN32:
		{
			LoadWin32();
			break;
		}
		case IDM_IMPORT_LANG:
		{
			ImportLang();
			break;
		}
		case IDM_COMPARE:
		{
			Compare();
			break;
		}
		case IDM_PROPERTIES:
		{
			List<Resource> l;
			if (Objs->ListObjects(l))
			{
				int Dialogs = 0;
				int Strings = 0;
				int Menus = 0;
				int Words = 0;
				int MultiLingual = 0;

				for (Resource *r = l.First(); r; r = l.Next())
				{
					switch (r->Type())
					{
						case TYPE_DIALOG:
						{
							Dialogs++;
							break;
						}
						case TYPE_STRING:
						{
							ResStringGroup *Grp = dynamic_cast<ResStringGroup*>(r);
							if (Grp)
							{
								Strings += Grp->GetStrs()->Length();
								_CountGroup(Grp, Words, MultiLingual);
							}
							break;
						}
						case TYPE_MENU:
						{
							Menus++;
							
							ResMenu *Menu = dynamic_cast<ResMenu*>(r);
							if (Menu)
							{
								if (Menu->Group)
								{
									Strings += Menu->Group->GetStrs()->Length();
									_CountGroup(Menu->Group, Words, MultiLingual);
								}
							}
							break;
						}
					}
				}

				LgiMsg(	this,
						"This file contains:\n"
						"\n"
						"   Dialogs: %i\n"
						"   Menus: %i\n"
						"   Strings: %i\n"
						"      Multi-lingual: %i\n"
						"         Words: %i",
						AppName,
						MB_OK,
						Dialogs,
						Menus,
						Strings,
						MultiLingual,
						Words);
			}
			break;
		}
		case IDM_EXIT:
		{
			LgiCloseApp();
			break;
		}
		case IDM_FIND:
		{
			Search s(this);
			if (s.DoModal())
			{
				new Results(this, &s);
			}
			break;
		}
		case IDM_NEXT:
		{
			LgiMsg(this, "Not implemented :(", AppName);
			break;
		}
		case IDM_TABLELAYOUT_TEST:
		{
			OpenTableLayoutTest(this);			
			break;
		}
		case IDM_HELP:
		{
			char ExeName[256];
			LgiGetExePath(ExeName, sizeof(ExeName));

			while (strchr(ExeName, DIR_CHAR) && strlen(ExeName) > 3)
			{
				char p[256];
				LgiMakePath(p, sizeof(p), ExeName, "index.html");
				if (!FileExists(p))
				{
					LgiMakePath(p, sizeof(p), ExeName, "help");
					LgiMakePath(p, sizeof(p), p, "index.html");
				}
				if (FileExists(p))
				{
					LgiExecute(HelpFile, NULL, ExeName);
					break;
				}
				
				LgiTrimDir(ExeName);
			}
			break;
		}
		case IDM_ABOUT:
		{
			GAbout Dlg(	this,
						AppName,
						APP_VER,
						"\nLgi Resource Editor.",
						"_about.gif",
						"http://www.memecode.com/lgires.php",
						"fret@memecode.com");
			break;
		}
		default:
		{
			int Idx = Cmd - IDM_LANG_BASE;
			if (Idx >= 0 && Idx < Languages.Length())
			{
				// Deselect the old lang
				GMenuItem *Item = ViewMenu ? ViewMenu->ItemAt(CurLang + 2) : 0;
				if (Item)
				{
					Item->Checked(false);
				}

				// Set the current
				CurLang = Idx;

				// Set the new lang's menu item
				Item = ViewMenu ? ViewMenu->ItemAt(CurLang + 2) : 0;
				if (Item)
				{
					Item->Checked(true);
				}

				// Update everything
				List<Resource> res;
				if (ListObjects(res))
				{
					for (Resource *r = res.First(); r; r = res.Next())
					{
						r->OnShowLanguages();
					}
				}
			}
			break;
		}
	}

	return GDocApp<GOptionsFile>::OnCommand(Cmd, Event, Handle);
}

int AppWnd::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		default:
		{
			break;
		}
	}
	return 0;
}

void AppWnd::FindStrings(List<ResString> &Strs, char *Define, int *CtrlId)
{
	if (Objs)
	{
		List<Resource> l;
		if (Objs->ListObjects(l))
		{
			for (Resource *r = l.First(); r; r = l.Next())
			{
				StringList *s = r->GetStrs();
				if (s)
				{
					for (ResString *Str = s->First(); Str; Str = s->Next())
					{
						if (Define && ValidStr(Str->GetDefine()))
						{
							if (strcmp(Define, Str->GetDefine()) == 0)
							{
								Strs.Insert(Str);
								continue;
							}
						}

						if (CtrlId)
						{
							if (*CtrlId == Str->GetId())
							{
								Strs.Insert(Str);
								continue;
							}
						}
					}
				}
			}
		}
	}
}

int AppWnd::GetUniqueCtrlId()
{
	int Max = 0;

	if (Objs)
	{
		List<Resource> l;
		if (Objs->ListObjects(l))
		{
			GHashTbl<int, int> t;
			for (Resource *r = l.First(); r; r = l.Next())
			{
				StringList *sl = r->GetStrs();
				if (sl)
				{
					for (ResString *s = sl->First(); s; s = sl->Next())
					{
						if (s->GetId() > 0 &&
						    !t.Find(s->GetId()))
						{
							t.Add(s->GetId(), s->GetId());
						}

						Max = max(s->GetId(), Max);
					}
				}
			}

			int i = 500;
			while (true)
			{
				if (t.Find(i))
				{
					i++;
				}
				else
				{
					return i;
				}
			}
		}
	}

	return Max + 1;
}

int AppWnd::GetUniqueStrRef(int	Start)
{
	if (!Objs)
		return -1;

	List<Resource> l;
	if (!Objs->ListObjects(l))
		return -1;

	GHashTbl<int, ResString*> Map;
	GArray<ResString*> Dupes;

	for (Resource *r = l.First(); r; r = l.Next())
	{
		ResStringGroup *Grp = r->GetStringGroup();
		if (Grp)
		{
			List<ResString>::I it = Grp->GetStrs()->Start();
			for (ResString *s = *it; s; s = *++it)
			{
			    if (s->GetRef())
			    {
				    ResString *Existing = Map.Find(s->GetRef());
				    if (Existing)
				    {
					    // These get their ref's reset to a unique value as a side
					    // effect of this function...
					    Dupes.Add(s);
				    }
				    else
				    {
					    Map.Add(s->GetRef(), s);
				    }
				}
				else
				{
				    int Idx = Grp->GetStrs()->IndexOf(s);
				    LgiAssert(!"No string ref?");
				}
			}			
		}
	}

	for (int i=Start; true; i++)
	{
		if (!Map.Find(i))
		{
			if (Dupes.Length())
			{
				ResString *s = Dupes[0];
				Dupes.DeleteAt(0);
				s->SetRef(i);

				SetDirty(true);
			}
			else
			{
				return i;
			}
		}
	}

	return -1;
}

ResString *AppWnd::GetStrFromRef(int Ref)
{
	ResString *Str = 0;

	if (Objs)
	{
		List<Resource> l;
		if (Objs->ListObjects(l))
		{
			for (Resource *r = l.First(); r && !Str; r = l.Next())
			{
				ResStringGroup *Grp = dynamic_cast<ResStringGroup*>(r);
				if (Grp)
				{
					if ((Str = Grp->FindRef(Ref)))
						break;
				}
			}
		}
	}

	return Str;
}

ResStringGroup *AppWnd::GetDialogSymbols()
{
	if (Objs)
	{
		List<Resource> l;
		if (Objs->ListObjects(l))
		{
			for (Resource *r = l.First(); r; r = l.Next())
			{
				ResStringGroup *Grp = dynamic_cast<ResStringGroup*>(r);
				if (Grp)
				{
					char *ObjName = Grp->Wnd()->Name();
					if (ObjName && stricmp(ObjName, StrDialogSymbols) == 0)
					{
						return Grp;
					}
				}
			}
		}
	}

	return NULL;
}

void AppWnd::OnReceiveFiles(GArray<char*> &Files)
{
	char *f = Files.Length() ? Files[0] : 0;
	if (f)
	{
		_OpenFile(f, false);
	}
}

void AppWnd::SetStatusText(char *Text, int Pane)
{
	if (Pane >= 0 && Pane < STATUS_MAX && StatusInfo[Pane])
	{
		StatusInfo[Pane]->Name(Text);
	}
}

Resource *AppWnd::NewObject(SerialiseContext ctx, GXmlTag *load, int Type, bool Select)
{
	Resource *r = 0;
	ObjTreeItem *Dir = 0;

	switch (Type)
	{
		case TYPE_CSS:
		{
			r = new ResCss(this);
			Dir = Objs->Style;
			break;
		}
		case TYPE_DIALOG:
		{
			r = new ResDialog(this);
			Dir = Objs->Dialogs;
			break;
		}
		case TYPE_STRING:
		{
			r = new ResStringGroup(this);
			Dir = Objs->Strings;
			break;
		}
		case TYPE_MENU:
		{
			r = new ResMenu(this);
			Dir = Objs->Menus;
  			break;
		}
	}

	if (r)
	{
		ObjTreeItem *Item = new ObjTreeItem(r);
		if (Item)
		{
			Dir->Insert(Item);
			Dir->Update();
			Dir->Expanded(true);
		
			if (Select)
			{
				Objs->Select(Item);
			}
		}

		r->Create(load, &ctx);

		if (Item)
		{
			Item->Update();
		}

		SetDirty(true);
	}

	return r;
}

bool AppWnd::InsertObject(int Type, Resource *r, bool Select)
{
	bool Status = false;
	if (r)
	{
		ObjTreeItem *Dir = 0;

		switch (Type)
		{
			case TYPE_CSS:
			{
				Dir = Objs->Style;
				break;
			}
			case TYPE_DIALOG:
			{
				Dir = Objs->Dialogs;
				break;
			}
			case TYPE_STRING:
			{
				Dir = Objs->Strings;
				break;
			}
			case TYPE_MENU:
			{
				Dir = Objs->Menus;
				break;
			}
		}

		if (Dir)
		{
			ObjTreeItem *Item = new ObjTreeItem(r);
			if (Item)
			{
				char *Name = Item->GetText();

				r->Item = Item;
				Dir->Insert(Item, (Name && Name[0] == '_') ? 0 : -1);
				Dir->Update();
				Dir->Expanded(true);
				
				if (Select)
				{
					Objs->Select(Item);
				}

				Status = true;
			}
		}
	}

	return Status;
}

void AppWnd::DelObject(Resource *r)
{
	OnResourceSelect(0);
	DeleteObj(r);
}

ObjTreeItem *GetTreeItem(GTreeItem *ti, Resource *r)
{
	for (GTreeItem *i=ti->GetChild(); i; i=i->GetNext())
	{
		ObjTreeItem *o = dynamic_cast<ObjTreeItem*>(i);
		if (o)
		{
			if (o->GetObj() == r) return o;
		}

		o = GetTreeItem(i, r);
		if (o) return o;
	}
	
	return 0;
}

ObjTreeItem *GetTreeItem(GTree *ti, Resource *r)
{
	for (GTreeItem *i=ti->GetChild(); i; i=i->GetNext())
	{
		ObjTreeItem *o = dynamic_cast<ObjTreeItem*>(i);
		if (o)
		{
			if (o->GetObj() == r) return o;
		}

		o = GetTreeItem(i, r);
		if (o) return o;
	}
	
	return 0;
}

void AppWnd::GotoObject(ResString *s,
						ResStringGroup *g,
						ResDialog *d,
						ResMenuItem *m,
						ResDialogCtrl *c)
{
	if (s)
	{
		Resource *Res = 0;
		if (g)
		{
			Res = g;
		}
		else if (d)
		{
			Res = d;
		}
		else if (m)
		{
			Res = m->GetMenu();
		}
		if (Res)
		{
			ObjTreeItem *ti = GetTreeItem(Objs, Res);
			if (ti)
			{
				ti->Select(true);
				
				if (g)
				{
					s->GetList()->Select(0);
					s->ScrollTo();
					LgiYield();
					s->Select(true);
				}
				else if (d)
				{
					LgiYield();
					d->SelectCtrl(c);
				}
				else if (m)
				{
					for (GTreeItem *i=m; i; i=i->GetParent())
					{
						i->Expanded(true);
					}
					m->Select(true);
					m->ScrollTo();
				}
			}
			else
			{
				printf("%s:%i - couldn't find resources tree item\n",
					__FILE__, __LINE__);
			}
		}
	}
}

bool AppWnd::ListObjects(List<Resource> &Lst)
{
	if (Objs)
	{
		return Objs->ListObjects(Lst);
	}

	return false;
}

void AppWnd::OnObjChange(FieldSource *r)
{
	if (Fields)
	{
		Fields->Serialize(false);
		SetDirty(true);
	}
}

void AppWnd::OnObjSelect(FieldSource *r)
{
	if (Fields)
	{
		Fields->OnSelect(r);
	}
}

void AppWnd::OnResourceSelect(Resource *r)
{
	if (LastRes)
	{
		OnObjSelect(NULL);
		MainSplit->SetViewB(0);
		LastRes = 0;
	}
	
	if (r)
	{
		GView *Wnd = r->CreateUI();
		if (Wnd)
		{
			if (SubSplit)
			{
				MainSplit->SetViewB(Wnd, false);
			}
			
			LgiYield();
			Pour();
			Wnd->Invalidate();
			LastRes = r;
		}
	}
}

char *TagName(GXmlTag *t)
{
	static char Buf[1024];
	GArray<GXmlTag*> Tags;
	for (; t; t = t->Parent)
	{
		Tags.AddAt(0, t);
	}
	Buf[0] = 0;
	for (int i=0; i<Tags.Length(); i++)
	{
		if (i) strcat(Buf, ".");
		strcat(Buf, Tags[i]->GetTag());
	}
	return Buf;
}

class ResCompare : public GWindow, public GLgiRes
{
	GList *Lst;

public:
	ResCompare(char *File1, char *File2)
	{
		Lst = 0;
		GRect p;
		GAutoString n;

		if (LoadFromResource(IDD_COMPARE, this, &p, &n))
		{
			SetPos(p);
			Name(n);
			MoveToCenter();
			GetViewById(IDC_DIFFS, Lst);

			if (Attach(0))
			{
				Visible(true);
				AttachChildren();

				GXmlTag *t1 = new GXmlTag;
				GXmlTag *t2 = new GXmlTag;
				if (t1 && File1)
				{
					GFile f;
					if (f.Open(File1, O_READ))
					{
						GXmlTree x(GXT_NO_ENTITIES);
						if (!x.Read(t1, &f, 0))
						{
							DeleteObj(t1);
						}
					}
					else
					{
						DeleteObj(t1);
					}
				}
				if (t2 && File2)
				{
					GFile f;
					if (f.Open(File2, O_READ))
					{
						GXmlTree x(GXT_NO_ENTITIES);
						if (!x.Read(t2, &f, 0))
						{
							DeleteObj(t2);
						}
					}
					else
					{
						DeleteObj(t2);
					}
				}

				if (Lst && t1 && t2)
				{
					Lst->Enabled(false);
					Compare(t1, t2);
					Lst->Enabled(true);
				}

				DeleteObj(t1);
				DeleteObj(t2);
			}
		}
	}

	void Compare(GXmlTag *t1, GXmlTag *t2)
	{
		char s[1024];

		if (stricmp(t1->GetTag(), t2->GetTag()) != 0)
		{
			sprintf(s, "Different Tag: '%s' <-> '%s'", t1->GetTag(), t2->GetTag());
			GListItem *i = new GListItem;
			if (i)
			{
				i->SetText(s);
				i->SetText(TagName(t1), 1);
				Lst->Insert(i);
			}
		}

		GHashTable a(0, false);		
		for (int i=0; i<t1->Attr.Length(); i++)
		{
			GXmlAttr *a1 = &t1->Attr[i];
			a.Add(a1->GetName(), a1);
		}

		for (int n=0; n<t2->Attr.Length(); n++)
		{
			GXmlAttr *a2 = &t2->Attr[n];
			GXmlAttr *a1 = (GXmlAttr*) a.Find(a2->GetName());
			if (a1)
			{
				if (strcmp(a1->GetValue(), a2->GetValue()) != 0)
				{
					sprintf(s, "Different Attr Value: '%s' <-> '%s'", a1->GetValue(), a2->GetValue());
					GListItem *i = new GListItem;
					if (i)
					{
						i->SetText(s);
						sprintf(s, "%s.%s", TagName(t1), a1->GetName());
						i->SetText(s, 1);
						Lst->Insert(i);
					}
				}

				a.Delete(a2->GetName());
			}
			else
			{
				sprintf(s, "[Right] Missing Attr: '%s' = '%s'", a2->GetName(), a2->GetValue());
				GListItem *i = new GListItem;
				if (i)
				{
					i->SetText(s);
					i->SetText(TagName(t1), 1);
					Lst->Insert(i);
				}
			}
		}

		char *Key;
		for (void *v = a.First(&Key); v; v = a.Next(&Key))
		{
			GXmlAttr *a1 = (GXmlAttr *)v;
			sprintf(s, "[Left] Missing Attr: '%s' = '%s'", a1->GetName(), a1->GetValue());
			GListItem *i = new GListItem;
			if (i)
			{
				i->SetText(s);
				i->SetText(TagName(t1), 1);
				Lst->Insert(i);
			}
		}

		if (t1->IsTag("string-group"))
		{
			GXmlTag *t;
			GArray<GXmlTag*> r1, r2;
			for (t = t1->Children.First(); t; t = t1->Children.Next())
			{
				char *Ref;
				if ((Ref = t->GetAttr("ref")))
				{
					int r = atoi(Ref);
					if (r)
					{
						r1[r] = t;
					}
				}
			}
			for (t = t2->Children.First(); t; t = t2->Children.Next())
			{
				char *Ref;
				if ((Ref = t->GetAttr("ref")))
				{
					int r = atoi(Ref);
					if (r)
					{
						r2[r] = t;
					}
				}
			}

			int Max = max(r1.Length(), r2.Length());
			for (int i = 0; i<Max; i++)
			{
				if (r1[i] && r2[i])
				{
					Compare(r1[i], r2[i]);
				}
				else if (r1[i])
				{
					sprintf(s, "[Right] Missing String: Ref=%s, Def=%s", r1[i]->GetAttr("ref"), r1[i]->GetAttr("Define"));
					GListItem *n = new GListItem;
					if (n)
					{
						n->SetText(s);
						n->SetText(TagName(r1[i]), 1);
						Lst->Insert(n);
					}
				}
				else if (r2[i])
				{
					sprintf(s, "[Left] Missing String: Ref=%s, Def=%s", r2[i]->GetAttr("ref"), r2[i]->GetAttr("Define"));
					GListItem *n = new GListItem;
					if (n)
					{
						n->SetText(s);
						n->SetText(TagName(r2[i]), 1);
						Lst->Insert(n);
					}
				}
			}
		}
		else
		{
			GXmlTag *c1 = t1->Children.First();
			GXmlTag *c2 = t2->Children.First();
			while (c1 && c2)
			{
				Compare(c1, c2);
				c1 = t1->Children.Next();
				c2 = t2->Children.Next();
			}
		}

		LgiYield();
	}

	void OnPosChange()
	{
		GRect c = GetClient();
		if (Lst)
		{
			c.Size(7, 7);
			Lst->SetPos(c);
		}
	}
};

void AppWnd::Compare()
{
	GFileSelect s;
	s.Parent(this);
	s.Type("Lgi Resource", "*.lr8");
	if (s.Open())
	{
		new ResCompare(GetCurFile(), s.Name());
	}
}

void AppWnd::ImportLang()
{
	// open dialog
	GFileSelect Select;

	Select.Parent(this);
	Select.Type("Lgi Resources", "*.lr;*.lr8;*.xml");

	if (Select.Open())
	{
		GFile F;
		if (F.Open(Select.Name(), O_READ))
		{
			SerialiseContext Ctx;
			Ctx.Format = GetFormat(Select.Name());

			// convert file to Xml objects
			GXmlTag *Root = new GXmlTag;
			if (Root)
			{
				GXmlTree Tree(GXT_NO_ENTITIES);
				if (Tree.Read(Root, &F, 0))
				{
					List<ResMenu> Menus;
					List<ResStringGroup> Groups;
					GXmlTag *t;

					for (t = Root->Children.First(); t; t = Root->Children.Next())
					{
						if (t->IsTag("menu"))
						{
							ResMenu *Menu = new ResMenu(this);
							if (Menu && Menu->Read(t, Ctx))
							{
								Menus.Insert(Menu);
							}
							else break;
						}
						else if (t->IsTag("string-group"))
						{
							ResStringGroup *g = new ResStringGroup(this);
							if (g && g->Read(t, Ctx))
							{
								Groups.Insert(g);
							}
							else break;
						}
					}
					
					Ctx.PostLoad(this);

					bool HasData = false;
					for (ResStringGroup *g=Groups.First(); g; g=Groups.Next())
					{
						g->SetLanguages();

						if (g->GetStrs()->Length() > 0 &&
							g->GetLanguages() > 0)
						{
							HasData = true;
						}
					}

					if (HasData)
					{
						List<GLanguage> Langs;
						for (ResStringGroup *g=Groups.First(); g; g=Groups.Next())
						{
							for (int i=0; i<g->GetLanguages(); i++)
							{
								GLanguage *Lang = g->GetLanguage(i);
								if (Lang)
								{
									bool Has = false;
									for (GLanguage *l=Langs.First(); l; l=Langs.Next())
									{
										if (stricmp((char*)l, (char*)Lang) == 0)
										{
											Has = true;
											break;
										}
									}
									if (!Has)
									{
										Langs.Insert(Lang);
									}
								}
							}
						}

						LangDlg Dlg(this, Langs);
						if (Dlg.DoModal() == IDOK &&
							Dlg.Lang)
						{
							GStringPipe Errors;
							
							int Matches = 0;
							int NotFound = 0;
							int Imported = 0;
							int Different = 0;
							
							for (ResStringGroup *g=Groups.First(); g; g=Groups.Next())
							{
								List<ResString>::I Strings = g->GetStrs()->Start();
								for (ResString *s=*Strings; s; s=*++Strings)
								{
									ResString *d = GetStrFromRef(s->GetRef());
									if (d)
									{
										Matches++;

										char *Str = s->Get(Dlg.Lang->Id);
										char *Dst = d->Get(Dlg.Lang->Id);
										if
										(
											(
												Str &&
												Dst &&
												strcmp(Dst, Str) != 0
											)
											||
											(
												(Str != 0) ^
												(Dst != 0)
											)
										)
										{
											Different++;
											d->Set(Str, Dlg.Lang->Id);
											Imported++;
										}
									}
									else
									{
										NotFound++;
										
										char e[256];
										sprintf(e, "String ref=%i (%s)\n", s->GetRef(), s->GetDefine());
										Errors.Push(e);
									}
								}
							}

							List<Resource> Lst;
							if (ListObjects(Lst))
							{
								for (ResMenu *m=Menus.First(); m; m=Menus.Next())
								{
									// find matching menu in our list
									ResMenu *Match = 0;
									for (Resource *r=Lst.First(); r; r=Lst.Next())
									{
										ResMenu *n = dynamic_cast<ResMenu*>(r);
										if (n && stricmp(n->Name(), m->Name()) == 0)
										{
											Match = n;
											break;
										}
									}

									if (Match)
									{
										// match strings
										List<ResString> *Src = m->GetStrs();
										List<ResString> *Dst = Match->GetStrs();
										
										for (ResString *s=Src->First(); s; s = Src->Next())
										{
											bool FoundRef = false;
											for (ResString *d=Dst->First(); d; d=Dst->Next())
											{
												if (s->GetRef() == d->GetRef())
												{
													FoundRef = true;

													char *Str = s->Get(Dlg.Lang->Id);
													if (Str)
													{
														char *Dst = d->Get(Dlg.Lang->Id);
														if (!Dst || strcmp(Dst, Str))
														{
															Different++;
														}

														d->Set(Str, Dlg.Lang->Id);
														Imported++;
													}
													break;
												}
											}
											if (!FoundRef)
											{
												NotFound++;

												char e[256];
												sprintf(e, "MenuString ref=%i (%s)\n", s->GetRef(), s->GetDefine());
												Errors.Push(e);
											}
										}

										Match->SetLanguages();
									}
								}

								for (Resource *r=Lst.First(); r; r=Lst.Next())
								{
									ResStringGroup *StrRes = dynamic_cast<ResStringGroup*>(r);
									if (StrRes)
									{
										StrRes->SetLanguages();
									}
								}
							}

							char *ErrorStr = Errors.NewStr();
							
							LgiMsg(	this,
									"Imported: %i\n"
									"Matched: %i\n"
									"Not matched: %i\n"
									"Different: %i\n"
									"Total: %i\n"
									"\n"
									"Import complete.\n"
									"\n%s",
									AppName,
									MB_OK,
									Imported,
									Matches,
									NotFound,
									Different,
									Matches + NotFound,
									ErrorStr?ErrorStr:(char*)"");
						}
					}
					else
					{
						LgiMsg(this, "No language information to import", AppName, MB_OK);
					}

					// Groups.DeleteObjects();
					// Menus.DeleteObjects();
				}
				else
				{
					LgiMsg(this, "Failed to parse XML from file.\nError: %s", AppName, MB_OK, Tree.GetErrorMsg());
				}

				DeleteObj(Root);
			}
		}
	}
}

void AppWnd::Empty()
{
	// Delete any existing objects
	List<Resource> l;
	if (ListObjects(l))
	{
		Resource *r;
		for (r = l.First(); r; )
		{
			if (r->SystemObject())
			{
				l.Delete(r);
				r = l.Current();
			}
			else
			{
				r=l.Next();
			}
		}

		for (r = l.First(); r; r=l.Next())
		{
			DelObject(r);
		}
	}
}

bool AppWnd::OpenFile(char *FileName, bool Ro)
{
	if (stristr(FileName, ".lr") ||
		stristr(FileName, ".lr8") ||
		stristr(FileName, ".xml"))
	{
		return LoadLgi(FileName);
	}
	else if (stristr(FileName, ".rc"))
	{
		return LoadWin32(FileName);
	}

	return false;
}

bool AppWnd::SaveFile(char *FileName)
{
	if (stristr(FileName, ".lr") ||
		stristr(FileName, ".lr8") ||
		stristr(FileName, ".xml"))
	{
		return SaveLgi(FileName);
	}
	else if (stristr(FileName, ".rc"))
	{
	}

	return false;
}

void AppWnd::GetFileTypes(GFileSelect *Dlg, bool Write)
{
	Dlg->Type("Lgi Resources", "*.lr;*.lr8;*.xml");
	if (!Write)
	{
		Dlg->Type("All Files", LGI_ALL_FILES);
	}
}

// Lgi load/save
bool AppWnd::TestLgi(bool Quite)
{
	bool Status = true;

	List<Resource> l;
	if (ListObjects(l))
	{
		ErrorCollection Errors;
		for (Resource *r = l.First(); r; r = l.Next())
		{
			Status &= r->Test(&Errors);
		}

		if (Errors.StrErr.Length() > 0)
		{
			GStringPipe Sample;
			for (int i=0; i<Errors.StrErr.Length(); i++)
			{
				ResString *s = Errors.StrErr[i].Str;
				Sample.Print("Ref=%i, %s: %s\n",
							s->GetRef(),
							s->GetDefine(),
							Errors.StrErr[i].Msg.Get());
			}
			char *Sam = Sample.NewStr();
			LgiMsg(this, "%i strings have errors.\n\n%s", AppName, MB_OK, Errors.StrErr.Length(), Sam);
			DeleteArray(Sam);
		}
		else if (!Quite)
		{
			LgiMsg(this, "Object are all ok.", AppName);
		}
	}

	return Status;
}

bool AppWnd::LoadLgi(char *FileName)
{
	bool Status = false;

	Empty();
	if (FileName)
	{
		ResFileFormat Format = GetFormat(FileName);

		GFile f;
		if (f.Open(FileName, O_READ))
		{
			GProgressDlg Progress(this);

			Progress.SetDescription("Initializing...");
			Progress.SetType("Tags");

			GXmlTag *Root = new GXmlTag;
			if (Root)
			{				
				// convert file to Xml objects
				GXmlTree Xml(0);
				Progress.SetDescription("Lexing...");
				if (Xml.Read(Root, &f, 0))
				{
					Progress.SetLimits(0, Root->Children.Length()-1);

					// convert Xml list into objects
					int i=0;
					DoEvery Timer(500);
					SerialiseContext Ctx;
					for (GXmlTag *t = Root->Children.First(); t; t = Root->Children.Next(), i++)
					{
						if (Timer.DoNow())
						{
							Progress.Value(Root->Children.IndexOf(t));
							LgiYield();
						}

						int RType = 0;
						if (t->IsTag("dialog"))
						{
							RType = TYPE_DIALOG;
						}
						else if (t->IsTag("string-group"))
						{
							RType = TYPE_STRING;
						}
						else if (t->IsTag("menu"))
						{
							RType = TYPE_MENU;
						}
						else if (t->IsTag("style"))
						{
						    RType = TYPE_CSS;
						}
						else
						{
							LgiAssert(!"Unexpected tag");
						}

						if (RType > 0)
						{
							NewObject(Ctx, t, RType, false);
						}
					}
					
					Ctx.PostLoad(this);

					SortDialogs();
					TestLgi();

					// Scan for languages and update the view lang menu
					Languages.Length(0);
					GHashTbl<const char*, GLanguage*> Langs;
					if (ViewMenu)
					{
						// Remove existing language menu items
						while (ViewMenu->RemoveItem(1));
						ViewMenu->AppendSeparator();

						// Enumerate all languages
						List<Resource> res;
						if (ListObjects(res))
						{
							for (Resource *r = res.First(); r; r = res.Next())
							{
								ResStringGroup *Sg = r->IsStringGroup();
								if (Sg)
								{
									for (int i=0; i<Sg->GetLanguages(); i++)
									{
										GLanguage *Lang = Sg->GetLanguage(i);
										if (Lang)
										{
											Langs.Add(Lang->Id, Lang);
										}
									}									
								}
							}
						}

						// Update languages array
						int n = 0;
						for (GLanguage *i = Langs.First(); i; i = Langs.Next(), n++)
						{
							Languages.Add(i);
							GMenuItem *Item = ViewMenu->AppendItem(i->Name, IDM_LANG_BASE + n, true);
							if (Item && i->IsEnglish())
							{
								Item->Checked(true);
								CurLang = n;
							}
						}

						if (Languages.Length() == 0)
						{
							ViewMenu->AppendItem("(none)", -1, false);
						}
					}

					Status = true;
				}
				else
				{
					LgiMsg(this, "Xml read failed: %s", AppName, MB_OK, Xml.GetErrorMsg());
				}

				DeleteObj(Root);
			}
		}
	}

	return Status;
}

void SerialiseContext::PostLoad(AppWnd *App)
{
	for (int i=0; i<FixId.Length(); i++)
	{
		ResString *s = FixId[i];
		int Id = App->GetUniqueCtrlId();
		s->SetId(Id);
		Log.Print("Repaired CtrlId of string ref %i to %i\n", s->GetRef(), Id);
	}
	
	GAutoString a(Log.NewStr());
	if (ValidStr(a))
	{
		LgiMsg(App, "%s", "Load Warnings", MB_OK, a.Get());
	}
}

int DialogNameCompare(ResDialog *a, ResDialog *b, NativeInt Data)
{
	char *A = (a)?a->Name():0;
	char *B = (b)?b->Name():0;
	if (A && B) return stricmp(A, B);
	return -1;
}

void AppWnd::SortDialogs()
{
	List<Resource> Lst;
	if (ListObjects(Lst))
	{
		List<ResDialog> Dlgs;
		for (Resource *r = Lst.First(); r; r = Lst.Next())
		{
			ResDialog *Dlg = dynamic_cast<ResDialog*>(r);
			if (Dlg)
			{
				Dlgs.Insert(Dlg);
				Dlg->Item->Remove();
			}
		}

		Dlgs.Sort(DialogNameCompare, 0);

		for (ResDialog *d = Dlgs.First(); d; d = Dlgs.Next())
		{
			Objs->Dialogs->Insert(d->Item);
		}
	}
}

class ResTreeNode
{
public:
	char *Str;
	ResTreeNode *a, *b;
	ResTreeNode(char *s)
	{
		a = b = 0;
		Str = s;
	}

	~ResTreeNode()
	{
		DeleteArray(Str);
		DeleteObj(a);
		DeleteObj(b);
	}

	void Enum(List<char> &l)
	{
		if (a)
		{
			a->Enum(l);
		}

		if (Str)
		{
			l.Insert(Str);
		}

		if (b)
		{
			b->Enum(l);
		}
	}

	bool Add(char *s)
	{
		int Comp = (Str && s) ? stricmp(Str, s) : -1;
		if (Comp == 0)
		{
			return false;
		}

		if (Comp < 0)
		{
			if (a)
			{
				return a->Add(s);
			}
			else
			{
				a = new ResTreeNode(s);
			}
		}

		if (Comp > 0)
		{
			if (b)
			{
				return b->Add(s);
			}
			else
			{
				b = new ResTreeNode(s);
			}
		}
		return true;
	}
};

class ResTree {

	ResTreeNode *Root;

public:
	ResTree()
	{
		Root = 0;
	}

	~ResTree()
	{
		DeleteObj(Root);
	}

	bool Add(char *s)
	{
		if (s)
		{
			if (!Root)
			{
				Root = new ResTreeNode(NewStr(s));
				return true;
			}
			else
			{
				return Root->Add(NewStr(s));
			}
		}

		return false;
	}

	void Enum(List<char> &l)
	{
		if (Root)
		{
			Root->Enum(l);
		}
	}
};

const char *HeaderStr = "// This file generated by LgiRes\r\n\r\n";
	
struct DefinePair
{
	char *Name;
	int Value;
};

int PairCmp(DefinePair *a, DefinePair *b)
{
	return a->Value - b->Value;
}

bool AppWnd::WriteDefines(GFile &Defs)
{
	bool Status = false;
	ResTree Tree;

	// Empty file
	Defs.Seek(0, SEEK_SET);
	Defs.SetSize(0);
	Defs.Write(HeaderStr, strlen(HeaderStr));

	// make a unique list of #define's
	List<Resource> Lst;
	if (ListObjects(Lst))
	{
		GHashTbl<char*,int> Def;
		GHashTable Ident;

		for (Resource *r = Lst.First(); r; r = Lst.Next())
		{
			List<ResString> *StrList = r->GetStrs();
			if (StrList)
			{
				Status = true;
				List<ResString>::I sl = StrList->Start();
				for (ResString *s = *sl; s; s = *++sl)
				{
					if (ValidStr(s->GetDefine()))
					{
						if (stricmp(s->GetDefine(), "IDOK") == 0)
						{
							s->SetId(IDOK);
						}
						else if (stricmp(s->GetDefine(), "IDCANCEL") == 0)
						{
							s->SetId(IDCANCEL);
						}
						else if (stricmp(s->GetDefine(), "IDC_STATIC") == 0)
						{
							s->SetId(-1);
						}
						else if (stricmp(s->GetDefine(), "-1") == 0)
						{
							s->SetDefine(0);
						}
						else
						{
							// Remove dupe ID's
							char IdStr[32];
							sprintf(IdStr, "%i", s->GetId());
							char *Define;
							if ((Define = (char*)Ident.Find(IdStr)))
							{
								if (strcmp(Define, s->GetDefine()))
								{
									List<ResString> n;
									FindStrings(n, s->GetDefine());
									int NewId = GetUniqueCtrlId();
									for (ResString *Ns = n.First(); Ns; Ns = n.Next())
									{
										Ns->SetId(NewId);
									}
								}
							}
							else
							{
								Ident.Add(IdStr, s->GetDefine());
							}

							// Make all define's the same
							int CtrlId;
							if ((CtrlId = Def.Find(s->GetDefine())))
							{
								// Already there...
								s->SetId(CtrlId);
							}
							else
							{
								// Add...
								LgiAssert(s->GetId());
								if (s->GetId())
									Def.Add(s->GetDefine(), s->GetId());
							}
						}
					}
				}
			}
		}

		// write the list out
		GArray<DefinePair> Pairs;
		char *s = 0;
		for (int i = Def.First(&s); i; i = Def.Next(&s))
		{
			if (ValidStr(s) &&
				stricmp(s, "IDOK") != 0 &&
				stricmp(s, "IDCANCEL") != 0 &&
				stricmp(s, "IDC_STATIC") != 0 &&
				stricmp(s, "-1") != 0)
			{
				DefinePair &p = Pairs.New();
				p.Name = s;
				p.Value = i;
			}
		}

		Pairs.Sort(PairCmp);

		for (int n=0; n<Pairs.Length(); n++)
		{
			DefinePair &p = Pairs[n];
			int Tabs = (43 - strlen(p.Name)) / 4;
			char Tab[32];
			ZeroObj(Tab);
			for (int n=0; n<Tabs; n++) Tab[n] = '\t';

			char s[4];
			memcpy(s, &p.Value, 4);
			#define IsPrintable(c) ((uint8)(c)>=' ' && (uint8)(c) <= 127)
			if (IsPrintable(s[0]) &&
				IsPrintable(s[1]) &&
				IsPrintable(s[2]) &&
				IsPrintable(s[3]))
			{
				#ifndef __BIG_ENDIAN__
				int32 i = LgiSwap32(p.Value);
				memcpy(s, &i, 4);
				#endif
				Defs.Print("#define %s%s'%04.4s'\r\n", p.Name, Tab, s);
			}
			else
				Defs.Print("#define %s%s%i\r\n", p.Name, Tab, p.Value);
		}
	}

	return Status;
}

bool AppWnd::SaveLgi(char *FileName)
{
	bool Status = false;

	if (!TestLgi())
	{
		if (LgiMsg(this, "Do you want to save the file with errors?", AppName, MB_YESNO) == IDNO)
		{
			return false;
		}
	}

	// Rename the existing file to 'xxxxxx.bak'
	if (FileExists(FileName))
	{
		char Bak[MAX_PATH];
		strcpy_s(Bak, sizeof(Bak), FileName);
		char *e = LgiGetExtension(Bak);
		if (e)
		{
			strcpy(e, "bak");
			if (FileExists(Bak))
				FileDev->Delete(Bak, false);

			FileDev->Move(FileName, Bak);
		}
	}

	ResString *s1 = GetStrFromRef(4);
	ResString *s2 = GetStrFromRef(184);

	// Save the file to xml
	if (FileName)
	{
		GFile f;
		GFile Defs;
		ResFileFormat Format = GetFormat(FileName);		
		
		char DefsName[256];
		strcpy(DefsName, FileName);
		LgiTrimDir(DefsName);
		strcat(DefsName, DIR_STR);
		strcat(DefsName, "resdefs.h");

		if (f.Open(FileName, O_WRITE) &&
			Defs.Open(DefsName, O_WRITE))
		{
			SerialiseContext Ctx;
			
			f.SetSize(0);
			Defs.SetSize(0);
			Defs.Print("// Generated by LgiRes\r\n\r\n");

			List<Resource> l;
			if (ListObjects(l))
			{
				// Remove all duplicate symbol Id's from the dialogs
				Resource *r;
				for (r = l.First(); r; r = l.Next())
				{
					ResDialog *Dlg = dynamic_cast<ResDialog*>(r);
					if (Dlg)
					{
						Dlg->CleanSymbols();
					}
				}

				// write defines
				WriteDefines(Defs);

				GXmlTag Root("resources");
				// Write all string lists out first so that when we load objects
				// back in again the strings will already be loaded and can
				// be referenced
				for (r = l.First(); r; r = l.Next())
				{
					if (r->Type() == TYPE_STRING)
					{
						GXmlTag *c = new GXmlTag;
						if (c && r->Write(c, Ctx))
						{
							Root.InsertTag(c);
						}
						else
						{
							LgiAssert(0);
							DeleteObj(c);
						}
					}
				}

				// now write the rest of the objects out
				for (r = l.First(); r; r = l.Next())
				{
					if (r->Type() != TYPE_STRING)
					{
						GXmlTag *c = new GXmlTag;
						if (c && r->Write(c, Ctx))
						{
							Root.InsertTag(c);
						}
						else
						{
							LgiAssert(0);
							DeleteObj(c);
						}
					}
				}

				// Set the offset type.
				//
				// Older versions of LgiRes stored the dialog's controls at a fixed
				// offset (3,17) from where they shouldv'e been. That was fixed, but
				// to differentiate between the 2 systems, we store a tag at the
				// root element.
				Root.SetAttr("Offset", 1);

				GXmlTree Tree(GXT_NO_ENTITIES);
				Status = Tree.Write(&Root, &f);
			}
		}
		else
		{
			LgiMsg(this,
					"Couldn't open these files for output:\n"
					"\t%s\n"
					"\t%s\n"
					"\n"
					"Maybe they are read only or locked by another application.",
					AppName,
					MB_OK,
					FileName,
					DefsName);
		}
	}

	return Status;
}

// Win32 load/save
#define ADJUST_CTRLS_X				2
#define ADJUST_CTRLS_Y				12

#define IMP_MODE_SEARCH				0
#define IMP_MODE_DIALOG				1
#define IMP_MODE_DLG_CTRLS			2
#define IMP_MODE_STRINGS			3
#define IMP_MODE_MENU				4

#include "GToken.h"

class ImportDefine
{
public:
	char *Name;
	char *Value;

	ImportDefine()
	{
		Name = Value = 0;
	}

	ImportDefine(char *Line)
	{
		Name = Value = 0;
		if (Line && *Line == '#')
		{
			Line++;
			if (strnicmp(Line, "define", 6) == 0)
			{
				Line += 6;
				Line = LgiSkipDelim(Line);
				char *Start = Line;
				const char *WhiteSpace = " \r\n\t";
				while (*Line && !strchr(WhiteSpace, *Line))
				{
					Line++;
				}

				Name = NewStr(Start, Line-Start);
				Line = LgiSkipDelim(Line);
				Start = Line;
				while (*Line && !strchr(WhiteSpace, *Line))
				{
					Line++;
				}
				if (Start != Line)
				{
					Value = NewStr(Start, Line-Start);
				}
			}
		}
	}

	~ImportDefine()
	{
		DeleteArray(Name);
		DeleteArray(Value);
	}
};

class DefineList : public List<ImportDefine>
{
	int NestLevel;

public:
	bool Defined;
	List<char> IncludeDirs;

	DefineList()
	{
		Defined = true;
		NestLevel = 0;
	}

	~DefineList()
	{
		for (ImportDefine *i = First(); i; i = Next())
		{
			DeleteObj(i);
		}

		for (char *c = IncludeDirs.First(); c; c = IncludeDirs.Next())
		{
			DeleteArray(c);
		}
	}
	
	void DefineSymbol(const char *Name, const char *Value = 0)
	{
		ImportDefine *Def = new ImportDefine;
		if (Def)
		{
			Def->Name = NewStr(Name);
			if (Value) Def->Value = NewStr(Value);
			Insert(Def);
		}
	}

	ImportDefine *GetDefine(char *Name)
	{
		if (Name)
		{
			for (ImportDefine *i = First(); i; i = Next())
			{
				if (i->Name && stricmp(i->Name, Name) == 0)
				{
					return i;
				}
			}
		}
		return NULL;
	}

	void ProcessLine(char *Line)
	{
		if (NestLevel > 16)
		{
			return;
		}

		if (Line && *Line == '#')
		{
			Line++;
			GToken T(Line);

			if (T.Length() > 0)
			{
				if (stricmp(T[0], "define") == 0) // #define
				{
					ImportDefine *Def = new ImportDefine(Line-1);
					if (Def)
					{
						if (Def->Name)
						{
							Insert(Def);
						}
						else
						{
							DeleteObj(Def);
						}
					}
				}
				else if (stricmp(T[0], "include") == 0) // #include
				{
					NestLevel++;

					GFile F;
					if (T.Length() > 1)
					{
						for (char *IncPath = IncludeDirs.First(); IncPath; IncPath = IncludeDirs.Next())
						{
							char FullPath[256];

							strcpy(FullPath, IncPath);
							if (FullPath[strlen(FullPath)-1] != DIR_CHAR)
							{
								strcat(FullPath, DIR_STR);
							}
							strcat(FullPath, T[1]);

							if (F.Open(FullPath, O_READ))
							{
								char Line[1024];
								while (!F.Eof())
								{
									F.ReadStr(Line, sizeof(Line));
									char *p = LgiSkipDelim(Line);
									if (*p == '#')
									{
										ProcessLine(p);
									}
								}

								break;
							}
						}
					}

					NestLevel--;
				}
				else if (stricmp(T[0], "if") == 0) // #if
				{
				}
				else if (stricmp(T[0], "ifdef") == 0) // #if
				{
					if (T.Length() > 1)
					{
						Defined = GetDefine(T[1]) != 0;
					}
				}
				else if (stricmp(T[0], "endif") == 0) // #endif
				{
					Defined = true;
				}
				else if (stricmp(T[0], "pragma") == 0)
				{
					ImportDefine *Def = new ImportDefine;
					if (Def)
					{
						char *Str = Line + 7;
						char *First = strchr(Str, '(');
						char *Second = (First) ? strchr(First+1, ')') : 0;
						if (First && Second)
						{
							Insert(Def);
							Def->Name = NewStr(Str, First-Str);
							First++;
							Def->Value = NewStr(First, Second-First);
						}
						else
						{
							DeleteObj(Def);
						}
					}
				}
				else if (stricmp(T[0], "undef") == 0)
				{
				}
			}
		}
		// else it's not for us anyway
	}
};

void TokLine(GArray<char*> &T, char *Line)
{
	if (Line)
	{
		// Exclude comments
		for (int k=0; Line[k]; k++)
		{
			if (Line[k] == '/' && Line[k+1] == '/')
			{
				Line[k] = 0;
				break;
			}
		}
		
		// Break into tokens
		for (const char *s = Line; s && *s; )
		{
			while (*s && strchr(" \t", *s)) s++;
			char *t = LgiTokStr(s);
			if (t)
				T.Add(t);
			else break;
		}
	}
}

bool AppWnd::LoadWin32(char *FileName)
{
	bool Status = false;
	GFileSelect Select;
	GHashTbl<const char*,bool> CtrlNames;
	CtrlNames.Add("LTEXT", true);
	CtrlNames.Add("EDITTEXT", true);
	CtrlNames.Add("COMBOBOX", true);
	CtrlNames.Add("SCROLLBAR", true);
	CtrlNames.Add("GROUPBOX", true);
	CtrlNames.Add("PUSHBUTTON", true);
	CtrlNames.Add("DEFPUSHBUTTON", true);
	CtrlNames.Add("CONTROL", true);
	CtrlNames.Add("ICON", true);
	CtrlNames.Add("LISTBOX", true);

	Empty();

	if (!FileName)
	{
		Select.Parent(this);
		Select.Type("Win32 Resource Script", "*.rc");
		if (Select.Open())
		{
			FileName = Select.Name();
		}
	}

	if (FileName)
	{
		GProgressDlg Progress(this);

		Progress.SetDescription("Initializing...");
		Progress.SetType("K");
		Progress.SetScale(1.0/1024.0);

		char *FileTxt = ReadTextFile(Select.Name());
		if (FileTxt)
		{
			GToken Lines(FileTxt, "\r\n");
			DeleteArray(FileTxt);

			DefineList Defines;
			ResStringGroup *String = new ResStringGroup(this);
			int Mode = IMP_MODE_SEARCH;

			// Language
			char *Language = 0;
			GLanguageId LanguageId = 0;

			// Dialogs
			List<ResDialog> DlgList;
			CtrlDlg *Dlg = 0;

			// Menus
			ResDialog *Dialog = 0;
			ResMenu *Menu = 0;
			List<ResMenu> Menus;
			ResMenuItem *MenuItem[32];
			int MenuLevel = 0;
			bool MenuNewLang = false;
			int MenuNextItem = 0;

			// Include defines
			char IncPath[256];
			strcpy(IncPath, Select.Name());
			LgiTrimDir(IncPath);
			Defines.IncludeDirs.Insert(NewStr(IncPath));

			Defines.DefineSymbol("_WIN32");
			Defines.DefineSymbol("IDC_STATIC", "-1");

			DoEvery Ticker(200);
			Progress.SetDescription("Reading resources...");
			Progress.SetLimits(0, Lines.Length()-1);

			if (String)
			{
				InsertObject(TYPE_STRING, String, false);
			}

			for (int CurLine = 0; CurLine < Lines.Length(); CurLine++)
			{
				if (Ticker.DoNow())
				{
					Progress.Value(CurLine);
					LgiYield();
				}
				
				// Skip white space
				char *Line = Lines[CurLine];
				char *p = LgiSkipDelim(Line);
				Defines.ProcessLine(Line);

				// Tokenize
				GArray<char*> T;
				TokLine(T, Line);

				// Process line
				if (Defines.Defined)
				{
					switch (Mode)
					{
						case IMP_MODE_SEARCH:
						{
							DeleteObj(Dialog);
							Dlg = 0;
							if (*p != '#')
							{
								if (T.Length() > 1 &&
									(stricmp(T[1], "DIALOG") == 0 ||
									 stricmp(T[1], "DIALOGEX") == 0))
								{
									Mode = IMP_MODE_DIALOG;
									Dialog = new ResDialog(this);
									if (Dialog)
									{
										Dialog->Create(NULL, NULL);
										Dialog->Name(T[0]);

										GAutoPtr<GViewIterator> It(Dialog->IterateViews());
										Dlg = dynamic_cast<CtrlDlg*>(It->First());
										if (Dlg)
										{
											int Pos[4] = {0, 0, 0, 0};
											int i = 0;
											for (; i<T.Length() && !LgiIsNumber(T[i]); i++)
												;
											for (int n=0; n<4; n++)
											{
												if (i+n<T.Length() && LgiIsNumber(T[i+n]))
												{
													Pos[n] = atoi(T[i+n]);
												}
												else
												{
													break;
												}
											}

											GRect r(Pos[0], Pos[1], Pos[0]+(Pos[2]*DIALOG_X), Pos[1]+(Pos[3]*DIALOG_Y));
											r.Offset(7, 7);
											Dlg->ResDialogCtrl::SetPos(r);
											Dlg->Str->SetDefine(T[0]);
											ImportDefine *Def = Defines.GetDefine(T[0]);
											if (Def)
											{
												Dlg->Str->SetId(atoi(Def->Value));
											}
										}
									}
									break;
								}

								if (T.Length() > 1 &&
									stricmp(T[1], "MENU") == 0)
								{
									ZeroObj(MenuItem);
									Mode = IMP_MODE_MENU;
									Menu = 0;

									// Check for preexisting menu in another language
									MenuNewLang = false;
									for (ResMenu *m = Menus.First(); m; m = Menus.Next())
									{
										if (stricmp(m->Name(), T[0]) == 0)
										{
											MenuNewLang = true;
											Menu = m;
											break;
										}
									}

									// If it doesn't preexist then create it
									if (!Menu)
									{
										Menu = new ResMenu(this);
										if (Menu)
										{
											Menus.Insert(Menu);
											Menu->Create(NULL, NULL);
											Menu->Name(T[0]);
										}
									}
									break;
								}

								if (T.Length() > 0 &&
									stricmp(T[0], "STRINGTABLE") == 0)
								{
									Mode = IMP_MODE_STRINGS;
									if (String)
									{
										String->Name("_Win32 Imports_");
									}
									break;
								}

								if (T.Length() > 2 &&
									stricmp(T[0], "LANGUAGE") == 0)
								{
									LanguageId = 0;
									DeleteArray(Language);
									char *Language = NewStr(T[1]);
									if (Language)
									{
										GLanguage *Info = GFindLang(0, Language);
										if (Info)
										{
											LanguageId = Info->Id;
											ResDialog::AddLanguage(Info->Id);
										}
									}
									break;
								}
							}
							break;
						}
						case IMP_MODE_DIALOG:
						{
							if (T.Length() > 0 && Dlg)
							{
								if (stricmp(T[0], "CAPTION") == 0)
								{
									char *Caption = T[1];
									if (Caption)
									{
										Dlg->Str->Set(Caption, LanguageId);
									}
								}
								else if (stricmp(T[0], "BEGIN") == 0)
								{
									Mode = IMP_MODE_DLG_CTRLS;
								}
							}
							break;
						}
						case IMP_MODE_DLG_CTRLS:
						{
							char *Type = T[0];
							if (!Type) break;

							if (stricmp(Type, "end") != 0)
							{
								// Add wrapped content to the token array.
								char *Next = Lines[CurLine+1];
								if (Next)
								{
									Next = LgiSkipDelim(Next);
									char *NextTok = LgiTokStr((const char*&)Next);
									if (NextTok)
									{
										if (stricmp(NextTok, "END") != 0 &&
											!CtrlNames.Find(NextTok))
										{
											TokLine(T, Lines[++CurLine]);
										}

										DeleteArray(NextTok);
									}
								}
							}
							
							// Process controls
							if (stricmp(Type, "LTEXT") == 0)
							{
								if (T.Length() >= 7)
								{
									CtrlText *Ctrl = new CtrlText(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->Set(T[1], LanguageId);
										Ctrl->Str->SetDefine(T[2]);
										ImportDefine *Def = Defines.GetDefine(T[2]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[5])*CTRL_X, atoi(T[6])*CTRL_Y);
										r.Offset(atoi(T[3])*CTRL_X+ADJUST_CTRLS_X, atoi(T[4])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl);
									}
								}
							}
							else if (stricmp(Type, "EDITTEXT") == 0)
							{
								if (T.Length() >= 7)
								{
									CtrlEditbox *Ctrl = new CtrlEditbox(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->SetDefine(T[1]);
										ImportDefine *Def = Defines.GetDefine(T[1]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[4])*CTRL_X, atoi(T[5])*CTRL_Y);
										r.Offset(atoi(T[2])*CTRL_X+ADJUST_CTRLS_X, atoi(T[3])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl);
									}
								}
							}
							else if (stricmp(Type, "COMBOBOX") == 0)
							{
								if (T.Length() >= 6)
								{
									CtrlComboBox *Ctrl = new CtrlComboBox(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->SetDefine(T[1]);
										ImportDefine *Def = Defines.GetDefine(T[1]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[4])*CTRL_X, atoi(T[5])*CTRL_Y);
										r.Offset(atoi(T[2])*CTRL_X+ADJUST_CTRLS_X, atoi(T[3])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl);
									}
								}
							}
							else if (stricmp(Type, "SCROLLBAR") == 0)
							{
								if (T.Length() == 6)
								{
									CtrlScrollBar *Ctrl = new CtrlScrollBar(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->SetDefine(T[1]);
										ImportDefine *Def = Defines.GetDefine(T[1]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[4])*CTRL_X, atoi(T[5])*CTRL_Y);
										r.Offset(atoi(T[2])*CTRL_X+ADJUST_CTRLS_X, atoi(T[3])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl);
									}
								}
							}
							else if (stricmp(Type, "GROUPBOX") == 0)
							{
								if (T.Length() >= 7)
								{
									CtrlGroup *Ctrl = new CtrlGroup(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->Set(T[1], LanguageId);
										Ctrl->Str->SetDefine(T[2]);
										ImportDefine *Def = Defines.GetDefine(T[2]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[5])*CTRL_X, atoi(T[6])*CTRL_Y);
										r.Offset(atoi(T[3])*CTRL_X+ADJUST_CTRLS_X, atoi(T[4])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl);
									}
								}
							}
							else if (stricmp(Type, "PUSHBUTTON") == 0 ||
									 stricmp(Type, "DEFPUSHBUTTON") == 0)
							{
								if (T.Length() >= 7)
								{
									CtrlButton *Ctrl = new CtrlButton(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->Set(T[1], LanguageId);
										Ctrl->Str->SetDefine(T[2]);
										ImportDefine *Def = Defines.GetDefine(T[2]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[5])*CTRL_X, atoi(T[6])*CTRL_Y);
										r.Offset(atoi(T[3])*CTRL_X+ADJUST_CTRLS_X, atoi(T[4])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl);
									}
								}
							}
							else if (stricmp(Type, "CONTROL") == 0)
							{
								if (T.Length() >= 7)
								{
									char *Caption = T[1];
									char *Id = T[2];
									char *Type = T[3];
									bool Checkbox = false;
									bool Radio = false;
									bool Done = false;

									// loop through styles
									int i;
									for (i=4; !Done && i<T.Length(); i++)
									{
										Done = true;
										if (stricmp(T[i], "BS_AUTOCHECKBOX") == 0)
										{
											Checkbox = true;
										}
										else if (stricmp(T[i], "BS_AUTORADIOBUTTON") == 0)
										{
											Radio = true;
										}

										if (i<T.Length()-1 && stricmp(T[i+1], "|") == 0)
										{
											// next token is an || operator
											i++;
											Done = false; // read through more styles
										}
									}

									GRect r(0, 0, 0, 0);
									if (i + 3 < T.Length() && Type)
									{
										ResDialogCtrl *Ctrl = 0;
										if (stricmp(Type, "Button") == 0)
										{
											if (Radio)
											{
												Ctrl = new CtrlRadio(Dialog, 0);
											}
											else if (Checkbox)
											{
												Ctrl = new CtrlCheckbox(Dialog, 0);
											}
										}
										else if (stricmp(Type, "SysListView32") == 0)
										{
											Ctrl = new CtrlList(Dialog, 0);
										}

										if (Ctrl)
										{
											r.ZOff(atoi(T[i+2])*CTRL_X, atoi(T[i+3])*CTRL_Y);
											r.Offset(atoi(T[i])*CTRL_X+ADJUST_CTRLS_X, atoi(T[i+1])*CTRL_Y+ADJUST_CTRLS_Y);
											Ctrl->SetPos(r);

											if (Caption) Ctrl->Str->Set(Caption, LanguageId);
											if (Id) Ctrl->Str->SetDefine(Id);
											ImportDefine *Def = Defines.GetDefine(Id);
											if (Def)
											{
												Ctrl->Str->SetId(atoi(Def->Value));
											}

											Dlg->AddView(Ctrl->View());
										}
									}
								}
							}
							else if (stricmp(Type, "ICON") == 0)
							{
								if (T.Length() >= 7)
								{
									CtrlBitmap *Ctrl = new CtrlBitmap(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->SetDefine(T[1]);
										ImportDefine *Def = Defines.GetDefine(T[1]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[5])*CTRL_X, atoi(T[6])*CTRL_Y);
										r.Offset(atoi(T[3])*CTRL_X+ADJUST_CTRLS_X, atoi(T[4])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl->View());
									}
								}
							}
							else if (stricmp(Type, "LISTBOX") == 0)
							{
								if (T.Length() >= 7)
								{
									CtrlList *Ctrl = new CtrlList(Dialog, 0);
									if (Ctrl)
									{
										Ctrl->Str->SetDefine(T[1]);
										ImportDefine *Def = Defines.GetDefine(T[1]);
										if (Def)
										{
											Ctrl->Str->SetId(atoi(Def->Value));
										}

										GRect r;
										r.ZOff(atoi(T[4])*CTRL_X, atoi(T[5])*CTRL_Y);
										r.Offset(atoi(T[2])*CTRL_X+ADJUST_CTRLS_X, atoi(T[3])*CTRL_Y+ADJUST_CTRLS_Y);
										Ctrl->ResDialogCtrl::SetPos(r);

										Dlg->AddView(Ctrl);
									}
								}
							}
							else if (stricmp(Type, "END") == 0)
							{
								// search for an existing dialog resource in
								// another language
								ResDialog *Match = 0;
								CtrlDlg *MatchObj = 0;
								for (ResDialog *d = DlgList.First(); d; d = DlgList.Next())
								{
									GAutoPtr<GViewIterator> It(d->IterateViews());
									GViewI *Wnd = It->First();
									if (Wnd)
									{
										CtrlDlg *Obj = dynamic_cast<CtrlDlg*>(Wnd);
										if (Obj)
										{
											if (Obj->Str->GetId() == Dlg->Str->GetId())
											{
												MatchObj = Obj;
												Match = d;
												break;
											}
										}
									}
								}

								if (Match)
								{
									// Merge the controls from "Dlg" to "MatchObj"
									List<ResDialogCtrl> Old;
									List<ResDialogCtrl> New;
									
									Dlg->ListChildren(New);
									MatchObj->ListChildren(Old);

									// add the language strings for the caption
									// without clobbering the languages already
									// present
									for (StrLang *s = Dlg->Str->Items.First();
										s; s = Dlg->Str->Items.Next())
									{
										if (!MatchObj->Str->Get(s->GetLang()))
										{
											MatchObj->Str->Set(s->GetStr(), s->GetLang());
										}
									}

									for (ResDialogCtrl *c = New.First(); c; c = New.Next())
									{
										ResDialogCtrl *MatchCtrl = 0;

										// try matching by Id
										{
											for (ResDialogCtrl *Mc = Old.First(); Mc; Mc = Old.Next())
											{
												if (Mc->Str->GetId() == c->Str->GetId() &&
													Mc->Str->GetId() > 0)
												{
													MatchCtrl = Mc;
													break;
												}
											}
										}

										// ok no Id match, match by location and type
										if (!MatchCtrl)
										{
											List<ResDialogCtrl> Overlapping;
											for (ResDialogCtrl *Mc = Old.First(); Mc; Mc = Old.Next())
											{
												GRect a = Mc->View()->GetPos();
												GRect b = c->View()->GetPos();
												GRect c = a;
												c.Bound(&b);
												if (c.Valid())
												{
													int Sa = a.X() * a.Y();
													int Sb = b.X() * b.Y();
													int Sc = c.X() * c.Y();
													int Total = Sa + Sb - Sc;
													double Amount = (double) Sc / (double) Total;
													if (Amount > 0.5)
													{
														// mostly similar in size
														Overlapping.Insert(Mc);
													}
												}
											}

											if (Overlapping.Length() == 1)
											{
												MatchCtrl = Overlapping.First();
											}
										}

										if (MatchCtrl)
										{
											// woohoo we are cool
											for (StrLang *s = c->Str->Items.First();
												s; s = c->Str->Items.Next())
											{
												MatchCtrl->Str->Set(s->GetStr(), s->GetLang());
											}
										}
									}

									// Delete the duplicate
									OnObjSelect(0);
									DeleteObj(Dialog);
								}
								else
								{
									// Insert the dialog
									InsertObject(TYPE_DIALOG, Dialog, false);
									DlgList.Insert(Dialog);
								}

								Dialog = 0;
								Dlg = 0;
								Status = true;
								Mode = IMP_MODE_SEARCH;
							}
							break;
						}
						case IMP_MODE_STRINGS:
						{
							if (stricmp(T[0], "BEGIN") == 0)
							{
							}
							else if (stricmp(T[0], "END") == 0)
							{
								Status = true;
								Mode = IMP_MODE_SEARCH;
							}
							else
							{
								if (T.Length() > 1)
								{
									ResString *Str = String->FindName(T[0]);
									if (!Str)
									{
										Str = String->CreateStr();
										if (Str)
										{
											ImportDefine *Def = Defines.GetDefine(T[0]);
											if (Def)
											{
												Str->SetId(atoi(Def->Value));
											}

											Str->SetDefine(T[0]);
											Str->UnDupelicate();
										}
									}

									if (Str)
									{
										// get the language
										GLanguage *Lang = GFindLang(Language);
										GLanguageId SLang = (Lang) ? Lang->Id : (char*)"en";
										StrLang *s = 0;

										// look for language present in string object
										for (s = Str->Items.First(); s && *s != SLang; s = Str->Items.Next());
										
										// if not present then add it
										if (!s)
										{
											s = new StrLang;
											if (s)
											{
												Str->Items.Insert(s);
												s->SetLang(SLang);
											}
										}

										// set the value
										if (s)
										{
											s->SetStr(T[1]);
										}
									}
								}
							}

							break;
						}
						case IMP_MODE_MENU:
						{
							if (T.Length() >= 1)
							{
								if (stricmp(T[0], "BEGIN") == 0)
								{
									MenuLevel++;
								}
								else if (stricmp(T[0], "END") == 0)
								{
									MenuLevel--;
									if (MenuLevel == 0)
									{
										Status = true;
										Mode = IMP_MODE_SEARCH;
										Menu->SetLanguages();

										if (!MenuNewLang)
										{
											InsertObject(TYPE_MENU, Menu, false);
										}

										Menu = 0;
									}
								}
								else
								{
									ResMenuItem *i = 0;
									char *Text = T[1];
									if (Text)
									{
										if (stricmp(T[0], "POPUP") == 0)
										{
											if (MenuNewLang)
											{
												GTreeItem *Ri = 0;
												if (MenuItem[MenuLevel])
												{
													Ri = MenuItem[MenuLevel]->GetNext();
												}
												else
												{
													if (MenuLevel == 1)
													{
														Ri = Menu->ItemAt(0);
													}
													else if (MenuItem[MenuLevel-1])
													{
														Ri = MenuItem[MenuLevel-1]->GetChild();
													}
												}

												if (Ri)
												{
													// Seek up to the next submenu
													while (!Ri->GetChild())
													{
														Ri = Ri->GetNext();
													}

													i = dynamic_cast<ResMenuItem*>(Ri);
													// char *si = i->Str.Get("en");
													if (i)
													{
														MenuItem[MenuLevel] = i;
													}
												}
											}
											else
											{
												MenuItem[MenuLevel] = i = new ResMenuItem(Menu);
											}

											if (i)
											{
												GLanguage *Lang = GFindLang(Language);
												i->GetStr()->Set(Text, (Lang) ? Lang->Id : (char*)"en");
											}

											MenuNextItem = 0;
										}
										else if (stricmp(T[0], "MENUITEM") == 0)
										{
											if (MenuNewLang)
											{
												if (MenuItem[MenuLevel-1] && T.Length() > 2)
												{
													ImportDefine *id = Defines.GetDefine(T[2]);
													if (id)
													{
														int Id = atoi(id->Value);

														int n = 0;
														for (GTreeItem *o = MenuItem[MenuLevel-1]->GetChild(); o; o = o->GetNext(), n++)
														{
															ResMenuItem *Res = dynamic_cast<ResMenuItem*>(o);
															if (Res && Res->GetStr()->GetId() == Id)
															{
																i = Res;
																break;
															}
														}
													}
												}

												MenuNextItem++;
											}
											else
											{
												i = new ResMenuItem(Menu);
											}

											if (i)
											{
												if (stricmp(Text, "SEPARATOR") == 0)
												{
													// Set separator
													i->Separator(true);
												}
												else
												{
													if (!MenuNewLang)
													{
														// Set Id
														i->GetStr()->SetDefine(T[2]);
														if (i->GetStr()->GetDefine())
														{
															ImportDefine *id = Defines.GetDefine(i->GetStr()->GetDefine());
															if (id)
															{
																i->GetStr()->SetId(atoi(id->Value));
																i->GetStr()->UnDupelicate();
															}
														}
													}

													// Set Text
													GLanguage *Lang = GFindLang(Language);
													i->GetStr()->Set(Text, (Lang) ? Lang->Id : (char*)"en");
												}
											}
										}
									}

									if (i && !MenuNewLang)
									{
										if (MenuLevel == 1)
										{
											Menu->Insert(i);
										}
										else if (MenuItem[MenuLevel-1])
										{
											MenuItem[MenuLevel-1]->Insert(i);
										}
									}
								}
							}

							break;
						}
					}
				}

				T.DeleteArrays();
			}

			DeleteObj(Dialog);

			if (String->Length() > 0)
			{
				String->SetLanguages();
			}
		}

		Invalidate();
	}

	return Status;
}

bool AppWnd::SaveWin32()
{
	return false;
}

/////////////////////////////////////////////////////////////////////////
ResFrame::ResFrame(Resource *child)
{
	Child = child;
	Name("ResFrame");
}

ResFrame::~ResFrame()
{
	if (Child)
	{
		Child->App()->OnObjSelect(NULL);
		Child->Wnd()->Detach();
	}
}

void ResFrame::OnFocus(bool b)
{
	Child->Wnd()->Invalidate();
}

bool ResFrame::Attach(GViewI *p)
{
	bool Status = GLayout::Attach(p);
	if (Status && Child)
	{
		Child->Attach(this);
		Child->Wnd()->Visible(true);
	}
	return Status;
}

bool ResFrame::Pour(GRegion &r)
{
	GRect *Best = FindLargest(r);
	if (Best)
	{
		SetPos(*Best);
		return true;
	}
	return false;
}

bool ResFrame::OnKey(GKey &k)
{
	bool Status = false;
	
	if (k.Down() && Child)
	{
		switch (k.c16)
		{
			case VK_DELETE:
			{
				if (k.Shift())
				{
					Child->Copy(true);
				}
				else
				{
					Child->Delete();
				}
				Status = true;
				break;
			}
			case 'x':
			case 'X':
			{
				if (k.Ctrl())
				{
					Child->Copy(true);
					Status = true;
				}
				break;
			}
			case 'c':
			case 'C':
			{
				if (k.Ctrl())
				{
					Child->Copy();
					Status = true;
				}
				break;
			}
			case VK_INSERT:
			{
				if (k.Ctrl())
				{
					Child->Copy();
				}
				else if (k.Shift())
				{
					Child->Paste();
				}
				Status = true;
				break;
			}
			case 'v':
			case 'V':
			{
				if (k.Ctrl())
				{
					Child->Paste();
					Status = true;
				}
				break;
			}
		}
	}

	return Child->Wnd()->OnKey(k) || Status;
}

void ResFrame::OnPaint(GSurface *pDC)
{
	// Draw nice frame
	GRect r(0, 0, X()-1, Y()-1);
	LgiThinBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(LC_MED, 24);
	LgiFlatBorder(pDC, r, 4);
	LgiWideBorder(pDC, r, DefaultSunkenEdge);

	// Set the child to the client area
	Child->Wnd()->SetPos(r);

	// Draw the dialog & controls
	GView::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
LgiFunc char *_LgiGenLangLookup();
#include "GAutoPtr.h"
#include "GVariant.h"
#include "GCss.h"
#include "GTableLayout.h"

class Foo : public GLayoutCell
{
public:
	Foo()
	{
		TextAlign(AlignLeft);
	}

	bool Add(GView *v) { return false; }
	bool Remove(GView *v) { return false; }
};

void TestFunc()
{
	/*
	Foo c;
	uint32 *p = (uint32*)&c;
	p++;
	p = (uint32*)(*p);
	void *method = (void*)p[2];

	for (int i=0; i<16; i++)
	{
		printf("[%i]=%p\n", i, p[i]);
	}

	c.OnChange(GCss::PropBackground);
	*/
}

int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, "LgiRes");
	if (a.IsOk())
	{
		if ((a.AppWnd = new AppWnd))
		{
			TestFunc();
			a.AppWnd->Visible(true);
			a.Run();
		}
	}

	return 0;
}

