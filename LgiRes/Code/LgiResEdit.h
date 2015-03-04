/*hdr
**      FILE:           LgiRes.h
**      AUTHOR:         Matthew Allen
**      DATE:           22/10/00
**      DESCRIPTION:    Resource Editor App Header
**
*/

#include "Lgi.h"
#include "INet.h"
#include "GDocApp.h"
#include "GProperties.h"
#include "GVariant.h"
#include "GDataDlg.h"
#include "GOptionsFile.h"

#include "resource.h"
#include "GTree.h"

////////////////////////////////////////////////////////////////////////////////////////////
// Defines

// version
#define APP_VER						"3.6.0"

// window messages
#define	IDM_UNDO					201
#define	IDM_REDO					202
#define	IDM_CUT						203
#define	IDM_COPY					204
#define	IDM_PASTE					205

#define	IDM_DELETE					300
#define	IDM_RENAME					301
#define	IDM_SETTINGS				302
#define	IDM_IMPORT					303
#define	IDM_IMPORT_WIN32			304
#define	IDM_EXPORT					305
#define	IDM_EXPORT_WIN32			306
#define	IDM_NEW_LANG				307
#define	IDM_DELETE_LANG				308
#define	IDM_IMPORT_LANG				309
#define	IDM_PROPERTIES				310
#define IDM_NEW_SUB					311
#define IDM_NEW_ITEM				312
#define IDM_DELETE_ITEM				313
#define IDM_MOVE_LEFT				314
#define IDM_MOVE_RIGHT				315
#define IDM_SET_LANG				316
#define IDM_TAB_ORDER				317
#define	IDM_DUMP					318
#define IDM_COPY_TEXT				319
#define IDM_PASTE_TEXT				320
#define IDM_NEW_ID					321
#define IDM_COMPARE					322
#define IDM_MOVE_UP					324
#define IDM_MOVE_DOWN				325
#define IDM_REF_EQ_ID				326
#define IDM_ID_EQ_REF				327

#define	IDM_NEW_DLG					400
#define	IDM_NEW_STRGRP				401

#define IDM_LANG_BASE				2000

#define	IDC_CHAT_MSG				1000

#define	STATUS_NORMAL				0
#define	STATUS_INFO					1
#define	STATUS_MAX					2

#define VAL_Ref						"Ref"
#define VAL_Id						"Id"
#define VAL_Define					"Define"
#define VAL_Tag						"Tag"
#define VAL_Text					"Text"
#define VAL_CellClass				"CellClass"
#define VAL_Text					"Text"
#define VAL_Pos						"Pos"
#define VAL_x1						"x1"
#define VAL_y1						"y1"
#define VAL_x2						"x2"
#define VAL_y2						"y2"
#define VAL_Visible					"Visible"
#define VAL_Enabled					"Enabled"
#define VAL_Class                   "Class"
#define VAL_Style                   "Style"

// Misc
#define	MainWnd						((AppWnd*)GApp::ObjInstance()->AppWnd)

// App
enum ObjectTypes {
    TYPE_CSS = 1,
    TYPE_DIALOG,
    TYPE_STRING,
    TYPE_MENU
};

enum IconTypes {
    ICON_FOLDER,
    ICON_IMAGE,
    ICON_ICON,
    ICON_CURSOR,
    ICON_DIALOG,
    ICON_STRING,
    ICON_MENU,
    ICON_DISABLED,
    ICON_CSS
};

#define OPT_ShowLanguages			"ShowLang"

#define StrDialogSymbols			"_Dialog Symbols_"
extern char TranslationStrMagic[];
extern char AppName[];

////////////////////////////////////////////////////////////////////////////////////////////
// Functions
extern char *EncodeXml(char *s, int l = -1);
extern char *DecodeXml(char *s, int l = -1);

////////////////////////////////////////////////////////////////////////////////////////////
// Classes
class AppWnd;
class ObjTreeItem;
class ResString;
typedef List<ResString> StringList;
class ResMenu;
class ResDialog;
class ResStringGroup;
class ResMenuItem;

struct ErrorInfo
{
	ResString *Str;
	GAutoString Msg;
};

class ErrorCollection
{
public:
	GArray<ErrorInfo> StrErr;
};

struct SerialiseContext
{
	ResFileFormat Format;
	GStringPipe Log;
	GArray<ResString*> FixId;
	
	SerialiseContext() : Log(512)
	{
		Format = Lr8File;
	}
	
	void PostLoad(AppWnd *App);
};

class Resource
{
	friend class AppWnd;
	friend class ObjTreeItem;

protected:
	int ResType;
	AppWnd *AppWindow;
	ObjTreeItem *Item;
	bool SysObject;

public:
	Resource(AppWnd *w, int t = 0, bool enabled = true);
	virtual ~Resource();

	AppWnd *App() { return AppWindow; }
	bool SystemObject() { return SysObject; }
	void SystemObject(bool i) { SysObject = i; }
	bool IsSelected();

	virtual GView *Wnd() { return NULL; }
	virtual bool Attach(GViewI *Parent);
	virtual int Type() { return ResType; }
	virtual void Type(int i) { ResType = i; }
	virtual void Create(GXmlTag *load, SerialiseContext *ctx) = 0; // called when users creates
	virtual ResStringGroup *GetStringGroup() { return 0; }
	
	// Sub classes
	virtual ResStringGroup *IsStringGroup() { return 0; }
	virtual ResDialog *IsDialog() { return 0; }
	virtual ResMenu *IsMenu() { return 0; }
	
	// Serialization
	virtual bool Test(ErrorCollection *e) = 0;
	virtual bool Read(GXmlTag *t, SerialiseContext &Ctx) = 0;
	virtual bool Write(GXmlTag *t, SerialiseContext &Ctx) = 0;
	virtual StringList *GetStrs() { return NULL; }

	// Clipboard
	virtual void Delete() {}
	virtual void Copy(bool Delete = false) {}
	virtual void Paste() {}

	// UI
	virtual GView *CreateUI() { return 0; }
	virtual void OnRightClick(GSubMenu *RClick) {}
	virtual void OnCommand(int Cmd) {}
	virtual void OnShowLanguages() {}
};

class ResFolder : public Resource, public GView
{
public:
	ResFolder(AppWnd *w, int t, bool enabled = true);
	GView *Wnd() { return dynamic_cast<GView*>(this); }

	void Create(GXmlTag *load, SerialiseContext *ctx) { LgiAssert(0); }
	bool Test(ErrorCollection *e) { return false; }
	bool Read(GXmlTag *t, SerialiseContext &Ctx) { return false; }
	bool Write(GXmlTag *t, SerialiseContext &Ctx) { return false; }
};

class ResFrame : public GLayout
{
	Resource *Child;

public:
	ResFrame(Resource *child);
	~ResFrame();

	bool Pour(GRegion &r);
	bool OnKey(GKey &k);
	void OnPaint(GSurface *pDC);
	bool Attach(GViewI *p);
	void OnFocus(bool b);
};

class ObjTreeItem : public GTreeItem
{
	friend class Resource;

	Resource *Obj;

public:
	ObjTreeItem(Resource *Object);
	~ObjTreeItem();

	Resource *GetObj() { return Obj; }
	char *GetText(int i=0);
	void OnSelect();
	void OnMouseClick(GMouse &m);
};

class ObjContainer : public GTree
{
	friend class AppWnd;

	ObjTreeItem *Style;
	ObjTreeItem *Dialogs;
	ObjTreeItem *Strings;
	ObjTreeItem *Menus;

	GImageList *Images;
	AppWnd *Window;

	bool AppendChildren(ObjTreeItem *Item, List<Resource> &Lst);

public:
	ObjContainer(AppWnd *w);
	~ObjContainer();

	bool ListObjects(List<Resource> &Lst);
};

class FieldTree
{
public:
	enum FieldMode
	{
		None,
		UiToObj,
		ObjToUi,
		ObjToStore,
		StoreToObj,
	};

	struct Field
	{
		// Global
		FieldTree *Tree;
		GAutoString Label;
		GAutoString Name;
		int Type;
		int Id;
		bool Multiline;
		void *Token;

		Field(FieldTree *tree)
		{
			Tree = tree;
			Type = 0;
			Id = 0;
			Token = 0;
			Multiline = false;
		}
	};

	typedef GArray<Field*> FieldArr;

protected:
	int &NextId;
	FieldMode Mode;
	GViewI *View;
	GDom *Store;
	bool Deep;

	GHashTbl<void*, FieldArr*> f;

	FieldArr *Get(void *Token, bool Create = false)
	{
		FieldArr *a = f.Find(Token);
		if (!a)
		{
			if (Create)
				f.Add(Token, a = new FieldArr);
			else
				LgiAssert(0);
		}
		return a;
	}

	Field *GetField(void *Token, const char *FieldName)
	{
		if (!Token || !FieldName) return 0;

		FieldArr *a = Get(Token);
		if (!a) return 0;

		for (int i=0; i<a->Length(); i++)
		{
			if (!stricmp((*a)[i]->Name, FieldName))
				return (*a)[i];
		}

		return 0;
	}

public:
	FieldTree(int &next, bool deep) : NextId(next)
	{
		Mode = None;
		View = 0;
		Store = 0;
		Deep = deep;
	}

	~FieldTree()
	{
		Empty();
	}

	FieldMode GetMode()
	{
		return Mode;
	}
	
	bool GetDeep()
	{
		return Deep;
	}

	void SetMode(FieldMode m)
	{
		Mode = m;
	}

	void SetView(GViewI *v)
	{
		View = v;
	}

	void SetStore(GDom *p)
	{
		Store = p;
	}

	void Empty()
	{
		for (FieldArr *a = f.First(); a; a = f.Next())
		{
			a->DeleteObjects();
		}
		f.DeleteObjects();
		View = 0;
	}

	void Insert(void *Token, int Type, int Reserved, const char *Name, const char *Label, int Idx = -1, bool Multiline = false)
	{
		FieldArr *a = Get(Token, true);
		if (!a) return;

		Field *n = new Field(this);
		if (n)
		{
			n->Token = Token;
			n->Label.Reset(NewStr(Label));
			n->Name.Reset(NewStr(Name));
			n->Id = NextId++;
			n->Type = Type;
			n->Multiline = Multiline;
			a->Add(n);
		}
	}

	void Serialize(void *Token, const char *FieldName, int &i)
	{
		Field *f = GetField(Token, FieldName);
		if (!f) return;
		GVariant v;

		switch (Mode)
		{
			case ObjToUi:
				View->SetCtrlValue(f->Id, i);
				break;
			case UiToObj:
				i = View->GetCtrlValue(f->Id);
				break;
			case StoreToObj:
				if (Store->GetValue(FieldName, v))
					i = v.CastInt32();
				break;
			case ObjToStore:
				Store->SetValue(FieldName, v = i);
				break;
			default:
				LgiAssert(0);
		}
	}

	void Serialize(void *Token, const char *FieldName, bool &b, int Default = -1)
	{
		Field *f = GetField(Token, FieldName);
		if (!f) return;
		GVariant i;

		switch (Mode)
		{
			case ObjToUi:
				View->SetCtrlValue(f->Id, b);
				break;
			case UiToObj:
				b = View->GetCtrlValue(f->Id);
				break;
			case StoreToObj:
			{
				if (Store->GetValue(FieldName, i))
				{
					b = i.CastInt32() != 0;
				}
				break;
			}
			case ObjToStore:
				if (Default < 0 || (bool)Default != b)
					Store->SetValue(FieldName, i = b);
				break;
			default:
				LgiAssert(0);
		}
	}

	void Serialize(void *Token, const char *FieldName, char *&s)
	{
		Field *f = GetField(Token, FieldName);
		if (!f) return;
		GVariant v;

		switch (Mode)
		{
			case ObjToUi:
				View->SetCtrlName(f->Id, s);
				break;
			case UiToObj:
			{
				DeleteArray(s);
				char *t = View->GetCtrlName(f->Id);
				if (ValidStr(t))
					s = NewStr(t);
				break;
			}
			case StoreToObj:
			{
				DeleteArray(s);
				if (Store->GetValue(FieldName, v))
					s = v.ReleaseStr();
				break;
			}
			case ObjToStore:
				Store->SetValue(FieldName, v = s);
				break;
			default:
				LgiAssert(0);
		}
	}

	void Serialize(void *Token, const char *FieldName, GAutoString &s)
	{
		Field *f = GetField(Token, FieldName);
		if (!f) return;
		GVariant v;

		switch (Mode)
		{
			case ObjToUi:
				View->SetCtrlName(f->Id, s);
				break;
			case UiToObj:
			{
				char *t = View->GetCtrlName(f->Id);
				s.Reset(ValidStr(t) ? NewStr(t) : 0);
				break;
			}
			case StoreToObj:
			{
				s.Reset(Store->GetValue(FieldName, v) ? v.ReleaseStr() : 0);
				break;
			}
			case ObjToStore:
				Store->SetValue(FieldName, v = s);
				break;
			default:
				LgiAssert(0);
		}
	}

	void Serialize(void *Token, const char *FieldName, GRect &r)
	{
		Field *f = GetField(Token, FieldName);
		if (!f) return;
		GVariant v;

		switch (Mode)
		{
			case ObjToUi:
				View->SetCtrlName(f->Id, r.GetStr());
				break;
			case UiToObj:
			{
				r.SetStr(View->GetCtrlName(f->Id));
				break;
			}
			case StoreToObj:
			{
				if (Store->GetValue(FieldName, v))
				{
					r.SetStr(v.Str());
				}
				break;
			}
			case ObjToStore:
			{
				Store->SetValue(FieldName, v = r.GetStr());
				break;
			}
			default:
				LgiAssert(0);
		}
	}

	static int FieldArrCmp(FieldArr **a, FieldArr **b)
	{
		return (**a)[0]->Id - (**b)[0]->Id;
	}

	void GetAll(GArray<FieldArr*> &Fields)
	{
		for (FieldArr *a = f.First(0); a; a = f.Next(0))
		{
			Fields.Add(a);
		}
		Fields.Sort(FieldArrCmp);
	}
};

#define M_OBJECT_CHANGED (M_USER + 4567)
class FieldSource
{
	friend class FieldView;
	OsView _FieldView;

public:
	FieldSource()
	{
		_FieldView = 0;
	}
	
	virtual ~FieldSource() {}

	virtual bool GetFields(FieldTree &Fields) = 0;
	virtual bool Serialize(FieldTree &Fields) = 0;
	
	void OnFieldChange()
	{
		if (_FieldView)
			LgiPostEvent(_FieldView, M_OBJECT_CHANGED, (GMessage::Param)this);
	}
};

#include "LgiRes_Dialog.h"

class FieldView : public GLayout
{
protected:
	FieldSource *Source;
	FieldTree Fields;
	int NextId;
	bool Ignore;
	AppWnd *App;

public:
	FieldView(AppWnd *app);
	~FieldView();

	void Serialize(bool Write);

	void OnSelect(FieldSource *s);
	GMessage::Result OnEvent(GMessage *m);
	void OnPaint(GSurface *pDC);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPosChange();
};

#include "LgiRes_String.h"

class AppWnd : public GDocApp<GOptionsFile>
{
protected:
	// UI
	GSplitter		*MainSplit;
	GSplitter		*SubSplit;

	GSubMenu		*Edit;
	GSubMenu		*Help;
	GSubMenu		*ViewMenu;

	GStatusBar		*Status;
	GStatusPane		*StatusInfo[STATUS_MAX];

	// App
	ObjContainer	*Objs;
	Resource		*LastRes;
	FieldView		*Fields;
	
	// Languages
	int				CurLang;
	GArray<GLanguage*> Languages;
	GHashTbl<const char*, bool> ShowLanguages;

	void SetupUi();
	void SortDialogs();
	void GetFileTypes(GFileSelect *Dlg, bool Write);

public:
	AppWnd();
	~AppWnd();

	// Languages
	bool ShowLang(GLanguageId Lang);
	void ShowLang(GLanguageId Lang, bool Show);
	GLanguage *GetCurLang();
	void SetCurLang(GLanguage *L);
	GArray<GLanguage*> *GetLanguages();
	void OnLanguagesChange(GLanguageId Lang, bool Add, bool Update = false);

	// ---------------------------------------------------------------------
	// Application
	Resource *NewObject(SerialiseContext ctx, GXmlTag *load, int Type, bool Select = true);
	bool InsertObject(int Type, Resource *r, bool Select = true);
	void DelObject(Resource *r);
	bool ListObjects(List<Resource> &Lst);
	int GetUniqueStrRef(int Start = 1);
	int GetUniqueCtrlId();
	void FindStrings(List<ResString> &Strs, char *Define = 0, int *CtrlId = 0);
	ResString *GetStrFromRef(int Ref);
	ResStringGroup *GetDialogSymbols();

	void Empty();
	void OnObjChange(FieldSource *r);
	void OnObjSelect(FieldSource *r);
	void OnResourceSelect(Resource *r);
	void GotoObject(class ResString *s,
					ResStringGroup *g,
					ResDialog *d,
					ResMenuItem *m,
					ResDialogCtrl *c);

	// ---------------------------------------------------------------------
	// Methods
	void SetStatusText(char *Text, int Pane = 0);
	bool TestLgi(bool Quite = true);
	bool LoadLgi(char *FileName = 0);
	bool SaveLgi(char *FileName = 0);
	bool LoadWin32(char *FileName = 0);
	bool SaveWin32();
	void ImportLang();
	void Compare();
	bool WriteDefines(GFile &Defs);

	bool OpenFile(char *FileName, bool Ro);
	bool SaveFile(char *FileName);

	// ---------------------------------------------------------------------
	// Window
	int OnNotify(GViewI *Ctrl, int Flags);
	GMessage::Result OnEvent(GMessage *m);
	int OnCommand(int Cmd, int Event, OsView Handle);
	void OnReceiveFiles(GArray<char*> &Files);
	void OnCreate();
};

class Search : public GDialog
{
	AppWnd *App;

	void OnCheck();

public:
	char *Text;
	char *Define;
	GLanguageId InLang;
	GLanguageId NotInLang;
	int Ref;
	int CtrlId;

	Search(AppWnd *app);
	~Search();

	int OnNotify(GViewI *c, int f);
};

class Results : public GWindow
{
	class ResultsPrivate *d;

public:
	Results(AppWnd *app, Search *search);
	~Results();

	void OnPosChange();
	int OnNotify(GViewI *v, int f);

};

class ShowLanguagesDlg : public GDialog
{
	class ShowLanguagesDlgPriv *d;

public:
	ShowLanguagesDlg(AppWnd *app);
	~ShowLanguagesDlg();

	int OnNotify(GViewI *n, int f);
};

class ResCss : public Resource, public GLayout
{
    friend class ResCssUi;
    
protected:
	class ResCssUi *Ui;
	GAutoString Style;

public:
	ResCss(AppWnd *w, int type = TYPE_CSS);
	~ResCss();

	void Create(GXmlTag *Load, SerialiseContext *Ctx);
	GView *Wnd() { return dynamic_cast<GView*>(this); }
	void OnShowLanguages();

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

extern void OpenTableLayoutTest(GViewI *p);