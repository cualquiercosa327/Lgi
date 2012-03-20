///////////////////////////////////////////////////////////////////////
// Multi-language library: LgiRes
#include "Res.h"
#include "GContainers.h"

class LgiResources;
class LgiClass LgiStringRes
{
	LgiResources *Res;

public:
	static const char *CodePage;
	static GLanguage *CurLang;

	int Ref;
	int Id;
	char *Str;
	char *Tag;
	// bool IsString;

	LgiStringRes(LgiResources *res);
	~LgiStringRes();
	bool Read(GXmlTag *Tag, ResFileFormat Format);
};

class LgiClass LgiDialogRes
{
	LgiResources *Res;

public:
	GXmlTag *Dialog;
	LgiStringRes *Str;
	GRect Pos;

	LgiDialogRes(LgiResources *res);
	~LgiDialogRes();

	bool Read(GXmlTag *Tag, ResFileFormat Format);
	char *Name() { return (Str) ? Str->Str : 0; }
	int Id() { return (Str) ? Str->Id : 0; }
	int X() { return Pos.X(); }
	int Y() { return Pos.Y(); }
};

class LgiClass LgiMenuRes : public GBase
{
	LgiResources *Res;
	GHashTbl<int, LgiStringRes*> Strings;

public:
	GXmlTag *Tag;

	LgiMenuRes(LgiResources *res);
	~LgiMenuRes();

	bool Read(GXmlTag *Tag, ResFileFormat Format);
	LgiStringRes *GetString(GXmlTag *Tag);
};

class LgiClass LgiResources : public ResFactory
{
	friend class GLgiRes;
	friend class GMenu;
	friend class LgiStringRes;

	class LgiResourcesPrivate *d;
	List<LgiDialogRes> Dialogs;
	List<LgiMenuRes> Menus;
	GEventsI *ScriptEngine;

	// NULL terminated list of languages
	// available in the loaded file.
	GLanguageId *Languages; 
	void AddLang(GLanguageId id);

public:
	GHashTbl<const char*, char*> LanguageNames;

	LgiResources(const char *FileName = 0, bool Warn = false);
	virtual ~LgiResources();

	// Instantiate resources
	bool LoadDialog(int Resource,
					GViewI *Parent,
					GRect *Pos = 0,
					GAutoString *Name = 0,
					GEventsI *ScriptEngine = 0,
					char *Tags = 0);
	LgiStringRes *StrFromRef(int Ref);
	char *StringFromRef(int Ref);
	char *StringFromId(int Ref);
	bool IsOk();
	ResFileFormat GetFormat();

	// Api
	bool Load(char *FileName);
	char *GetFileName();
	GLanguageId *GetLanguages() { return Languages; }
	List<LgiDialogRes>::I GetDialogs() { return Dialogs.Start(); }

	// Factory
	ResObject *CreateObject(GXmlTag *Tag, ResObject *Parent);
	void Res_SetPos(ResObject *Obj, int x1, int y1, int x2, int y2);
	void Res_SetPos(ResObject *Obj, char *s);
	GRect Res_GetPos(ResObject *Obj);
	int Res_GetStrRef(ResObject *Obj);
	bool Res_SetStrRef(ResObject *Obj, int Ref, ResReadCtx *Ctx);
	void Res_Attach(ResObject *Obj, ResObject *Parent);
	bool Res_GetChildren(ResObject *Obj, List<ResObject> *l, bool Deep);
	void Res_Append(ResObject *Obj, ResObject *Parent);
	bool Res_GetItems(ResObject *Obj, List<ResObject> *l);
	bool Res_GetProperties(ResObject *Obj, GDom *Props);
	bool Res_SetProperties(ResObject *Obj, GDom *Props);
	GDom* Res_GetDom(ResObject *Obj);
};

class GResourceContainer : public GArray<LgiResources*>
{
public:
	~GResourceContainer()
	{
		DeleteObjects();
	}
};

LgiExtern GResourceContainer _ResourceOwner;
LgiExtern LgiResources *LgiGetResObj(bool Warn = false, const char *filename = 0);
