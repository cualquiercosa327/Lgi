/*hdr
**      FILE:           LgiRes.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           12/3/2000
**      DESCRIPTION:    Xp resource interfaces
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "Lgi.h"
#include "GToken.h"
#include "LList.h"
#include "GTableLayout.h"
#if defined(LINUX) && !defined(LGI_SDL)
#include "LgiWinManGlue.h"
#endif
#include "GVariant.h"
#include "GDisplayString.h"
#include "LgiRes.h"

// If it is defined it will use the cross platform 
// "res" library distributed with the LGI library.

// #include "GXml.h"

#define DEBUG_RES_FILE						0
#define CastToGWnd(RObj)					((RObj != 0) ? dynamic_cast<GView*>(RObj) : 0)

class TagHash : public GHashTbl<char*,bool>, public ResReadCtx
{
	GToken Toks;

public:
	TagHash(const char *TagList) : Toks(TagList)
	{
		for (int i=0; i<Toks.Length(); i++)
			Add(Toks[i], true);
	}

	bool Check(const char *Tags)
	{
		bool Result = true;
		if (Tags)
		{
			GToken t(Tags);
			for (int i=0; i<t.Length(); i++)
			{
				char *Tag = t[i];
				if (*Tag == '!')
				{
					if (Find(Tag + 1))
						return false;
				}
				else if (!Find(Tag))
				{
					Result = false;
					break;
				}
			}
		}
		return Result;
	}

	bool Check(GXmlTag *t)
	{
		if (!t)
			return false;

		char *Tag = t->GetAttr("Tag");
		if (Tag)
			return Check(Tag);

		return true;
	}
};

const char *LgiStringRes::CodePage = 0;
GLanguage *LgiStringRes::CurLang = 0;

LgiStringRes::LgiStringRes(LgiResources *res)
{
	Res = res;
	Ref = 0;
	Id = 0;
	Str = 0;
	Tag = 0;
	// IsString = false;
}

LgiStringRes::~LgiStringRes()
{
	DeleteArray(Str);
	DeleteArray(Tag);
}

bool LgiStringRes::Read(GXmlTag *t, ResFileFormat Format)
{
	if (LgiStringRes::CurLang &&
		t &&
		stricmp(t->GetTag(), "string") == 0)
	{
		char *n = 0;
		if ((n = t->GetAttr("Cid")) ||
			(n = t->GetAttr("Id")) )
		{
			Id = atoi(n);
		}
		if ((n = t->GetAttr("Ref")))
		{
			Ref = atoi(n);
		}
		if ((n = t->GetAttr("Define")))
		{
			if (strcmp(n, "IDOK") == 0)
			{
				Id = IDOK;
			}
			else if (strcmp(n, "IDCANCEL") == 0)
			{
				Id = IDCANCEL;
			}
			else if (strcmp(n, "IDC_STATIC") == 0)
			{
				Id = -1;
			}
		}
		if ((n = t->GetAttr("Tag")))
		{
			Tag = NewStr(n);
		}

		const char *Cp = LgiStringRes::CodePage;
		char Name[256];
		strcpy_s(Name, sizeof(Name), LgiStringRes::CurLang->Id);
		n = 0;
		
		if ((n = t->GetAttr(Name)) &&
			strlen(n) > 0)
		{
			// Language string ok
			// Lang = LangFind(0, CurLang, 0);
		}
		else if (LgiStringRes::CurLang->OldId &&
				sprintf_s(Name, sizeof(Name), "Text(%i)", LgiStringRes::CurLang->OldId) &&
				(n = t->GetAttr(Name)) &&
				strlen(n) > 0)
		{
			// Old style language string ok
		}
		else if (!LgiStringRes::CurLang->IsEnglish())
		{
			// no string, try english
			n = t->GetAttr("en");
			GLanguage *Lang = GFindLang("en");
			if (Lang)
			{
				Cp = Lang->CodePage;
			}
		}

		if (n)
		{
			DeleteArray(Str);

			if (Cp)
			{
				Str = (char*)LgiNewConvertCp("utf-8", n, Cp);
			}
			else
			{
				Str = NewStr(n);
			}

			if (Str)
			{
				char *d = Str;
				for (char *s = Str; s && *s;)
				{
					if (*s == '\\')
					{
						if (*(s+1) == 'n')
						{
							*d++ = '\n';
						}
						else if (*(s+1) == 't')
						{
							*d++ = '\t';
						}
						s += 2;
					}
					else 
					{
						*d++ = *s++;
					}
				}
				*d++ = 0;
			}
		}

		if (Res)
		{
			for (int a=0; a<t->Attr.Length(); a++)
			{
				GXmlAttr *v = &t->Attr[a];
				char *Name = v->GetName();

				if (GFindLang(Name))
				{
					Res->AddLang(Name);
				}
				else if (	Name[0] == 'T' &&
							Name[1] == 'e' && 
							Name[2] == 'x' && 
							Name[3] == 't' &&
							Name[4] == '(')
				{
					int Old = atoi(Name + 5);
					if (Old)
					{
						GLanguage *OldLang = GFindOldLang(Old);
						if (OldLang)
						{
							LgiAssert(OldLang->OldId == Old);
							Res->AddLang(OldLang->Id);
						}
					}
				}
			}
		}

		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////
// Lgi resources
class LgiResourcesPrivate
{
public:
	bool Ok;
	ResFileFormat Format;
	GAutoString File;
	GHashTbl<int, LgiStringRes*> StrRef;
	GHashTbl<int, LgiStringRes*> Strings;
	GHashTbl<int, LgiStringRes*> DlgStrings;


	LgiResourcesPrivate() :
	    StrRef(0, true, -1, NULL),
	    Strings(0, true, -1, NULL),
	    DlgStrings(0, true, -1, NULL)
	{
		Ok = false;
		Format = Lr8File;
	}

	~LgiResourcesPrivate()
	{
		StrRef.DeleteObjects();
	}
};

GResourceContainer _ResourceOwner;
bool LgiResources::LoadStyles = false;

LgiResources::LgiResources(const char *FileName, bool Warn)
{
	d = new LgiResourcesPrivate;
	ScriptEngine = 0;

	// global pointer list
	_ResourceOwner.Add(this);

	GString File;
	char *FullPath = 0;

#if DEBUG_RES_FILE
LgiTrace("%s:%i - Filename='%s'\n", _FL, FileName);
#endif
	
	if (FileExists(FileName))
	{
		FullPath = NewStr(FileName);
	}
	else
	{
		if (FileName)
		{
			// We're given the file name, and we should find the path.
			const char *f = strrchr(FileName, DIR_CHAR);

#if DEBUG_RES_FILE
LgiTrace("%s:%i - f='%s'\n", _FL, f);
#endif

			File = f ? f + 1 : FileName;

#if DEBUG_RES_FILE
LgiTrace("%s:%i - File='%s'\n", _FL, File.Get());
#endif
		}
		else
		{
			// Need to look up the file associated by name with the current exe
			char Str[MAX_PATH];
			if (LgiGetExeFile(Str, sizeof(Str)))
			{
#if DEBUG_RES_FILE
LgiTrace("%s:%i - Str='%s'\n", _FL, Str);
#endif

				char *f = strrchr(Str, DIR_CHAR);
				if (f)
				{
					f++;
					
					#ifdef WINDOWS
					char *Period = strrchr(f, '.');
					if (Period)
					{
						*Period = 0;
					}
					#endif

					File = f;
				}

#if DEBUG_RES_FILE
LgiTrace("%s:%i - File='%s'\n", _FL, File);
#endif
			}
			else
			{
				LgiMsg(0, LgiLoadString(L_ERROR_RES_NO_EXE_PATH,
										"Fatal error: Couldn't get the path of the running\nexecutable. Can't find resource file."),
										"LgiResources::LgiResources");
				LgiTrace("%s:%i - Fatal error: Couldn't get the path of the running\nexecutable. Can't find resource file.", _FL);
				LgiExitApp();
			}
		}

		// Find the file..
		#ifdef MAC
		ssize_t DotApp = File.RFind(".app");
		if (DotApp >= 0)
			File.Length((int)DotApp);
		#endif

#if DEBUG_RES_FILE
LgiTrace("%s:%i - File='%s'\n", _FL, File.Get());
#endif

		GString BaseFile = File;
		GString AltFile = File.Replace(".");
		BaseFile += ".lr8";
		AltFile += ".lr8";

#if DEBUG_RES_FILE
LgiTrace("%s:%i - File='%s'\n", _FL, BaseFile.Get());
#endif

		FullPath = LgiFindFile(BaseFile);
		if (!FullPath)
			FullPath = LgiFindFile(AltFile);

#if DEBUG_RES_FILE
LgiTrace("%s:%i - FullPath='%s'\n", _FL, FullPath);
#endif
	}

	if (FullPath)
	{
		Load(FullPath);
		DeleteArray(FullPath);
	}
	else
	{
		char Msg[256];

		char Exe[256] = "(couldn't get exe path)";
		LgiGetExeFile(Exe, sizeof(Exe));

		// Prepare data
		sprintf_s(Msg, sizeof(Msg),
				LgiLoadString(L_ERROR_RES_NO_LR8_FILE,
								"Couldn't find the file '%s' required to run this application\n(Exe='%s')"),
				File.Get(),
				Exe);

		// Dialog
		printf("%s", Msg);
		if (Warn)
		{
			LgiMsg(0, Msg, "LgiResources::LgiResources");

			// Exit
			LgiExitApp();
		}
	}
}

LgiResources::~LgiResources()
{
	_ResourceOwner.Delete(this);
	LanguageNames.DeleteArrays();
	
	Dialogs.DeleteObjects();
	Menus.DeleteObjects();
	DeleteObj(d);
}

bool LgiResources::StyleElement(GViewI *v)
{
	if (!v) return false;
	if (!LoadStyles) return true;

	for (int i=0; i<_ResourceOwner.Length(); i++)
	{
		LgiResources *r = _ResourceOwner[i];
		if (r)
		{
			GCss::SelArray Selectors;
			GViewCssCb Ctx;
			r->CssStore.Match(Selectors, &Ctx, v);

			for (unsigned i=0; i<Selectors.Length(); i++)
			{
				const char *Defs = Selectors[i]->Style;
				GCss *Css = v->GetCss(true);
				if (Css && Defs)
				{
					Css->Parse(Defs, GCss::ParseRelaxed);
				}
			}
		}
	}
	
	return true;
}

ResFileFormat LgiResources::GetFormat()
{
	return d->Format;
}

bool LgiResources::IsOk()
{
	return d->Ok;
}

void LgiResources::AddLang(GLanguageId id)
{
	// search through id's...
	for (int i=0; i<Languages.Length(); i++)
	{
		if (stricmp(Languages[i], id) == 0)
			// already in the list
			return;
	}

	Languages.Add(id);
}

char *LgiResources::GetFileName()
{
	return d->File;
}

bool LgiResources::Load(char *FileName)
{
	if (!FileName)
	{
		LgiTrace("%s:%i - No filename.\n", _FL);
		LgiAssert(0);
		return false;
	}

	GFile F;
	if (!F.Open(FileName, O_READ))
	{
		LgiTrace("%s:%i - Couldn't open '%s'.\n", _FL, FileName);
		LgiAssert(0);
		return false;
	}

	d->File.Reset(NewStr(FileName));
	d->Format = Lr8File;
	char *Ext = LgiGetExtension(FileName);
	if (Ext && stricmp(Ext, "lr") == 0)
	{
		d->Format = CodepageFile;
	}
	else if (Ext && stricmp(Ext, "xml") == 0)
	{
		d->Format = XmlFile;
	}

	LgiStringRes::CurLang = LgiGetLanguageId();
	if (d->Format != CodepageFile)
	{
		LgiStringRes::CodePage = 0;
	}
	else if (LgiStringRes::CurLang)
	{
		LgiStringRes::CodePage = LgiStringRes::CurLang->CodePage;
	}

	GXmlTree x(0);
	GAutoPtr<GXmlTag> Root(new GXmlTag);
	if (!x.Read(Root, &F, 0))
	{
		LgiTrace("%s:%i - ParseXmlFile failed: %s\n", _FL, x.GetErrorMsg());
		// LgiAssert(0);
		return false;
	}

	for (GXmlTag *t = Root->Children.First(); t; )
	{
		if (t->IsTag("string-group"))
		{
			bool IsString = true;
			char *Name = 0;
			if ((Name = t->GetAttr("Name")))
			{
				IsString = stricmp("_Dialog Symbols_", Name) != 0;
			}

			for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
			{
				LgiStringRes *s = new LgiStringRes(this);
				if (s && s->Read(c, d->Format))
				{
					// This code saves the names of the languages if specified in the LR8 files.
					char *Def = c->GetAttr("define");
					if (Def && !stricmp(Def, "IDS_LANGUAGE"))
					{
						for (int i=0; i<c->Attr.Length(); i++)
						{
							GXmlAttr &a = c->Attr[i];
							LanguageNames.Add(a.GetName(), NewStr(a.GetValue()));
						}
					}

					// Save the string for later.
					d->StrRef.Add(s->Ref, s);
					if (s->Id != -1)
					{
					    if (IsString)
						    d->Strings.Add(s->Id, s);
					    else
						    d->DlgStrings.Add(s->Id, s);
					}
					d->Ok = true;
				}
				else
				{
					LgiTrace("%s:%i - string read failed\n", _FL);
					DeleteObj(s);
				}
			}
		}
		else if (t->IsTag("dialog"))
		{
			LgiDialogRes *n = new LgiDialogRes(this);
			if (n && n->Read(t, d->Format))
			{
				Dialogs.Insert(n);
				d->Ok = true;
				t->RemoveTag();
				t = 0;
			}
			else
			{
				LgiTrace("%s:%i - dialog read failed\n", _FL);
				DeleteObj(n);
			}
		}
		else if (t->IsTag("menu"))
		{
			LgiMenuRes *m = new LgiMenuRes(this);
			if (m && m->Read(t, d->Format))
			{
				Menus.Insert(m);
				d->Ok = true;
				t->RemoveTag();
				t = 0;
			}
			else
			{
				LgiTrace("%s:%i - menu read failed\n", _FL);
				DeleteObj(m);
			}
		}
		else if (t->IsTag("style"))
		{
			const char *c = t->GetContent();
			CssStore.Parse(c);
		}

		if (t)
			t = Root->Children.Next();
		else
			t = Root->Children.Current();
	}

	return true;
}

LgiStringRes *LgiResources::StrFromRef(int Ref)
{
	return d->StrRef.Find(Ref);
}

char *LgiResources::StringFromId(int Id)
{
	LgiStringRes *NotStr = 0;
	LgiStringRes *sr;

	if ((sr = d->Strings.Find(Id)))
		return sr->Str;

	if ((sr = d->DlgStrings.Find(Id)))
		return sr->Str;

	return NotStr ? NotStr->Str : 0;
}

char *LgiResources::StringFromRef(int Ref)
{
	LgiStringRes *s = d->StrRef.Find(Ref);
	return s ? s->Str : 0;
}

#include "GTextLabel.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GButton.h"
#include "GRadioGroup.h"
#include "GTabView.h"
#include "GCombo.h"
#include "GBitmap.h"
#include "GSlider.h"
#include "GScrollBar.h"
#include "GTree.h"

class GMissingCtrl : public GLayout, public ResObject
{
    GAutoString n;

public:
    GMissingCtrl(char *name) : ResObject(Res_Custom)
    {
        n.Reset(NewStr(name));
    }    

    void OnPaint(GSurface *pDC)
    {
        GRect c = GetClient();
        pDC->Colour(GColour(0xcc, 0xcc, 0xcc));
        pDC->Rectangle();
        pDC->Colour(GColour(0xff, 0, 0));
        pDC->Line(c.x1, c.y1, c.x2, c.y2);
        pDC->Line(c.x2, c.y1, c.x1, c.y2);
        GDisplayString ds(SysFont, n);
        SysFont->Transparent(true);
        SysFont->Fore(LC_TEXT);
        ds.Draw(pDC, 3, 0);
    }
};

ResObject *LgiResources::CreateObject(GXmlTag *t, ResObject *Parent)
{
	ResObject *Wnd = 0;
	if (t && t->GetTag())
	{
		char *Control = 0;
		
		if (stricmp(t->GetTag(), Res_StaticText) == 0)
		{
			Wnd = new GTextLabel(0, 0, 0, -1, -1, NULL);
		}
		else if (stricmp(t->GetTag(), Res_EditBox) == 0)
		{
			Wnd = new GEdit(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_CheckBox) == 0)
		{
			Wnd = new GCheckBox(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_Button) == 0)
		{
			Wnd = new GButton(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_Group) == 0)
		{
			Wnd = new GRadioGroup(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_RadioBox) == 0)
		{
			Wnd = new GRadioButton(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_TabView) == 0)
		{
			GTabView *Tv = new GTabView(0, 10, 10, 100, 100, "GTabView");
			Wnd = Tv;
			if (Tv)
			{
				Tv->SetPourLargest(false);
				Tv->SetPourChildren(false);
			}
		}
		else if (stricmp(t->GetTag(), Res_Tab) == 0)
		{
			Wnd = new GTabPage(0);
		}
		else if (stricmp(t->GetTag(), Res_ListView) == 0)
		{
			LList *w;
			Wnd = w = new LList(0, 0, 0, -1, -1, "");
			if (w)
			{
				w->Sunken(true);
			}
		}
		else if (stricmp(t->GetTag(), Res_Column) == 0)
		{
			if (Parent)
			{
				LList *Lst = dynamic_cast<LList*>(Parent);

				LgiAssert(Lst);

				if (Lst)
				{
					Wnd = Lst->AddColumn("");
				}
			}
		}
		else if (stricmp(t->GetTag(), Res_ComboBox) == 0)
		{
			Wnd = new GCombo(0, 0, 0, 100, 20, "");
		}
		else if (stricmp(t->GetTag(), Res_Bitmap) == 0)
		{
			Wnd = new GBitmap(0, 0, 0, 0);
		}
		else if (stricmp(t->GetTag(), Res_Progress) == 0)
		{
			Wnd = new GProgress(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_Slider) == 0)
		{
			Wnd = new GSlider(0, 0, 0, -1, -1, "", false);
		}
		else if (stricmp(t->GetTag(), Res_ScrollBar) == 0)
		{
			Wnd = new GScrollBar(0, 0, 0, 20, 100, "");
		}
		else if (stricmp(t->GetTag(), Res_Progress) == 0)
		{
			Wnd = new GProgress(0, 0, 0, 1, 1, "");
		}
		else if (stricmp(t->GetTag(), Res_TreeView) == 0)
		{
			Wnd = new GTree(0, 0, 0, 1, 1, "");
		}
		else if (stricmp(t->GetTag(), Res_ControlTree) == 0)
		{
			GView *v = GViewFactory::Create("GControlTree");
			if (!(Wnd = dynamic_cast<ResObject*>(v)))
			{
				DeleteObj(v);
			}
		}
		else if (stricmp(t->GetTag(), Res_Custom) == 0)
		{
			Control = t->GetAttr("ctrl");
			GView *v = GViewFactory::Create(Control);

			if (!v)
			    v = new GMissingCtrl(Control);

			if (v)
			{
				Wnd = dynamic_cast<ResObject*>(v);
				if (!Wnd)
				{
					// Not a "ResObject"
					LgiAssert(!"Not a ResObject");
					DeleteObj(v);
				}
			}
		}
		else if (stricmp(t->GetTag(), Res_Table) == 0)
		{
			Wnd = new GTableLayout;
		}
	
		if (!Wnd)
		{
			printf(LgiLoadString(L_ERROR_RES_CREATE_OBJECT_FAILED,
								"LgiResources::CreateObject(%s) failed. (Ctrl=%s)\n"),
					t->GetTag(),
					Control);
		}
	}
	
	LgiAssert(Wnd);
	return Wnd;
}

void LgiResources::Res_SetPos(ResObject *Obj, int x1, int y1, int x2, int y2)
{
	GItemColumn *Col = dynamic_cast<GItemColumn*>(Obj);
	if (Col)
	{
		Col->Width(x2-x1);
	}
	else
	{
		GView *w = CastToGWnd(Obj);
		if (w)
		{
			GRect n(x1, y1, x2, y2);
			w->SetPos(n);
		}
	}
}

void LgiResources::Res_SetPos(ResObject *Obj, char *s)
{
	if (Obj && s)
	{
		GToken T(s, ",");
		if (T.Length() == 4)
		{
			Res_SetPos(Obj, atoi(T[0]), atoi(T[1]), atoi(T[2]), atoi(T[3]));
		}
	}
}

bool LgiResources::Res_GetProperties(ResObject *Obj, GDom *Props)
{
	// this is a read-only system...
	return false;
}

struct ResObjectCallback : public GCss::ElementCallback<ResObject>
{
	GDom *Props;
	GVariant v;
	
	ResObjectCallback(GDom *props)
	{
		Props = props;
	}

	const char *GetElement(ResObject *obj)
	{
		return obj->GetObjectName();
	}
	
	const char *GetAttr(ResObject *obj, const char *Attr)
	{
		if (!Props->GetValue(Attr, v))
			v.Empty();		
		return v.Str();
	}
	
	bool GetClasses(GArray<const char *> &Classes, ResObject *obj)
	{
		if (Props->GetValue("class", v))
			Classes.Add(v.Str());
		return Classes.Length() > 0;
	}
	
	ResObject *GetParent(ResObject *obj)
	{
		LgiAssert(0);
		return NULL;
	}
	
	GArray<ResObject*> GetChildren(ResObject *obj)
	{
		GArray<ResObject*> a;
		return a;
	}
};

bool LgiResources::Res_SetProperties(ResObject *Obj, GDom *Props)
{
	GView *v = dynamic_cast<GView*>(Obj);
	if (!v || !Props)
		return false;

	GVariant i;
	if (Props->GetValue("enabled", i))
		v->Enabled(i.CastInt32());

	if (Props->GetValue("visible", i))
		v->Visible(i.CastInt32());

	if (Props->GetValue("style", i))
		v->SetCssStyle(i.Str());

	if (Props->GetValue("class", i))
	{
		ResObjectCallback Cb(Props);
		GCss::SelArray a;
		if (CssStore.Match(a, &Cb, Obj))
		{
			for (int i=0; i<a.Length(); i++)
			{
				GCss::Selector *s = a[i];
				if (s)
				{
					const char *style = s->Style;
					v->SetCssStyle(style);
				}
			}
		}
	}

	if (Props->GetValue("image", i))
		v->GetCss(true)->BackgroundImage(GCss::ImageDef(i.Str()));

	GEdit *e = dynamic_cast<GEdit*>(v);
	if (e)
	{
		if (Props->GetValue("pw", i))
			e->Password(i.CastInt32());

		if (Props->GetValue("multiline", i))
			e->MultiLine(i.CastInt32());
	}

	return true;
}

GRect LgiResources::Res_GetPos(ResObject *Obj)
{
	GView *w = CastToGWnd(Obj);
	if (w)
	{
		return w->GetPos();
	}

	return GRect(0, 0, 0, 0);
}

int LgiResources::Res_GetStrRef(ResObject *Obj)
{
	return 0;
}

bool LgiResources::Res_SetStrRef(ResObject *Obj, int Ref, ResReadCtx *Ctx)
{
	LgiStringRes *s = d->StrRef.Find(Ref);
	if (!s)
		return false;

	if (Ctx && !Ctx->Check(s->Tag))
		return false;

	GView *w = CastToGWnd(Obj);
	if (w)
	{
		w->SetId(s->Id);
		if (ValidStr(s->Str))
			w->Name(s->Str);
	}
	else if (Obj)
	{
		GItemColumn *Col = dynamic_cast<GItemColumn*>(Obj);
		if (Col)
		{
			Col->Name(s->Str);
		}
		else
		{
			GTabPage *Page = dynamic_cast<GTabPage*>(Obj);
			if (Page)
			{
				Page->Name(s->Str);
			}
		}
	}

	return true;
}

void LgiResources::Res_Attach(ResObject *Obj, ResObject *Parent)
{
	GView *o = CastToGWnd(Obj);
	GView *p = CastToGWnd(Parent);
	GTabPage *Tab = dynamic_cast<GTabPage*>(Parent);
	if (o)
	{
		if (Tab)
		{
			if (Tab)
			{
				GRect r = o->GetPos();
				r.Offset(-4, -24);
				o->SetPos(r);

				Tab->Append(o);
			}
		}
		else if (p)
		{
			if (!p->IsAttached() ||
				!o->Attach(p))
			{
				p->AddView(o);
				o->SetParent(p);
			}
		}
		else
		{
			LgiAssert(p);
		}
	}
}

bool LgiResources::Res_GetChildren(ResObject *Obj, List<ResObject> *l, bool Deep)
{
	GView *o = CastToGWnd(Obj);
	if (o && l)
	{
		GAutoPtr<GViewIterator> It(o->IterateViews());
		for (GViewI *w = It->First(); w; w = It->Next())
		{
			ResObject *n = dynamic_cast<ResObject*>(w);
			if (n)
			{
				l->Insert(n);
			}
		}
		return true;
	}
	return false;
}

void LgiResources::Res_Append(ResObject *Obj, ResObject *Parent)
{
	if (Obj && Parent)
	{
		GItemColumn *Col = dynamic_cast<GItemColumn*>(Obj);
		LList *Lst = dynamic_cast<LList*>(Parent);
		if (Lst && Col)
		{
			Lst->AddColumn(Col);
		}

		GTabView *Tab = dynamic_cast<GTabView*>(Obj);
		GTabPage *Page = dynamic_cast<GTabPage*>(Parent);
		if (Tab && Page)
		{
			Tab->Append(Page);
		}
	}
}

bool LgiResources::Res_GetItems(ResObject *Obj, List<ResObject> *l)
{
	if (Obj && l)
	{
		LList *Lst = dynamic_cast<LList*>(Obj);
		if (Lst)
		{
			for (int i=0; i<Lst->GetColumns(); i++)
			{
				l->Insert(Lst->ColumnAt(i));
			}
			return true;
		}

		GTabView *Tabs = dynamic_cast<GTabView*>(Obj);
		if (Tabs)
		{
			for (int i=0; i<Tabs->GetTabs(); i++)
			{
				l->Insert(Tabs->TabAt(i));
			}
			return true;
		}
	}
	return false;
}

GDom *LgiResources::Res_GetDom(ResObject *Obj)
{
	return dynamic_cast<GDom*>(Obj);
}

///////////////////////////////////////////////////////
LgiDialogRes::LgiDialogRes(LgiResources *res)
{
	Res = res;
	Dialog = 0;
	Str = 0;
}

LgiDialogRes::~LgiDialogRes()
{
	DeleteObj(Dialog);
}

bool LgiDialogRes::Read(GXmlTag *t, ResFileFormat Format)
{
	if ((Dialog = t))
	{
		char *n = 0;
		if ((n = Dialog->GetAttr("ref")))
		{
			int Ref = atoi(n);
			Str = Res->StrFromRef(Ref);
		}
		if ((n = Dialog->GetAttr("pos")))
		{
			GToken T(n, ",");
			if (T.Length() == 4)
			{
				Pos.x1 = atoi(T[0]);
				Pos.y1 = atoi(T[1]);
				Pos.x2 = atoi(T[2]);
				Pos.y2 = atoi(T[3]);
			}
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
LgiMenuRes::LgiMenuRes(LgiResources *res) : Strings(0, true, -1, NULL)
{
	Res = res;
	Tag = 0;
}

LgiMenuRes::~LgiMenuRes()
{
	Strings.DeleteObjects();
	DeleteObj(Tag);
}

bool LgiMenuRes::Read(GXmlTag *t, ResFileFormat Format)
{
	Tag = t;
	if (t && stricmp(t->GetTag(), "menu") == 0)
	{
		char *n;
		if ((n = t->GetAttr("name")))
		{
			GBase::Name(n);
		}

		for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
		{
			if (stricmp(c->GetTag(), "string-group") == 0)
			{
				for (GXmlTag *i = c->Children.First(); i; i = c->Children.Next())
				{
					LgiStringRes *s = new LgiStringRes(Res);
					if (s && s->Read(i, Format))
					{
					    LgiAssert(!Strings.Find(s->Ref)); // If this fires the string has a dupe ref
						Strings.Add(s->Ref, s);
					}
					else
					{
						DeleteObj(s);
					}
				}
				break;
			}
		}

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Dialog
bool GLgiRes::LoadFromResource(int Resource, GViewI *Parent, GRect *Pos, GAutoString *Name, char *TagList)
{
	LgiGetResObj();

	for (int i=0; i<_ResourceOwner.Length(); i++)
	{		
		if (_ResourceOwner[i]->LoadDialog(Resource, Parent, Pos, Name, 0, TagList))
			return true;
	}


	return false;
}

////////////////////////////////////////////////////////////////////////
// Get language
GLanguage *LgiGetLanguageId()
{
	// Check for command line override
	char Buf[64];
	if (LgiApp->GetOption("i", Buf))
	{
		GLanguage *i = GFindLang(Buf);
		if (i)
			return i;
	}

	// Check for config setting
	GXmlTag *LangConfig = LgiApp->GetConfig("Language");
	if (LangConfig)
	{
		char *Id;
		if ((Id = LangConfig->GetAttr("Id")) &&
			ValidStr(Id))
		{
			GLanguage *l = GFindLang(Id);
			if (l)
				return l;
		}
	}

	// Use a system setting
	#if defined WIN32

	int Lang = GetSystemDefaultLCID();

	TCHAR b[256];
	if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTLANGUAGE, b, CountOf(b)) > 0)
	{
		Lang = htoi(b);
	}

	int Primary = PRIMARYLANGID(Lang);
	int Sub = SUBLANGID(Lang);
	GLanguage *i, *English = 0;

	// Search for exact match
	for (i = GFindLang(NULL); i->Id; i++)
	{
		if (i->Win32Id == Lang)
			return i;
	}

	// Search for PRIMARYLANGID match
	for (i = GFindLang(NULL); i->Id; i++)
	{
		if (PRIMARYLANGID(i->Win32Id) == PRIMARYLANGID(Lang))
			return i;
	}

	#elif defined(LINUX) && !defined(LGI_SDL)

	GLibrary *WmLib = LgiApp->GetWindowManagerLib();
	if (WmLib)
	{
		Proc_LgiWmGetLanguage GetLanguage = (Proc_LgiWmGetLanguage) WmLib->GetAddress("LgiWmGetLanguage");
		
		char Lang[256];
		if (GetLanguage &&
			GetLanguage(Lang))
		{
			GLanguage *l = GFindLang(Lang);
			if (l)
				return l;
		}
	}

	return GFindLang("en");

	/*
	// Read in KDE language setting
	char Path[256];
	if (LgiGetSystemPath(LSP_HOME, Path, sizeof(Path)))
	{
		LgiMakePath(Path, sizeof(Path), Path, ".kde/share/config/kdeglobals");
		char *Txt = ReadTextFile(Path);

		if (Txt)
		{
			extern bool _GetIniField(char *Grp, char *Field, char *In, char *Out, int OutSize);
			char Lang[256] = "";
			GLanguage *Ret = 0;
			if (_GetIniField("Locale", "Language", Txt, Lang, sizeof(Lang)))
			{
				GToken Langs(Lang, ":,; \t");
				for (int i=0; i<Langs.Length(); i++)
				{
					if (Ret = GFindLang(Langs[i]))
					{
						break;
					}
				}
			}
			DeleteArray(Txt);
			if (Ret)
			{
				return Ret;
			}
		}
	}
	*/
	
	#endif

	// Return a default of English
	return GFindLang("en");
}

///////////////////////////////////////////////////////////////////////////////////////
// Load string
const char *LgiLoadString(int Res, const char *Default)
{
	char *s = 0;
	LgiResources *r = LgiGetResObj();
	if (r)
	{
		// use to test for non-resource strings
		// return "@@@@";
		s = r->StringFromId(Res);
	}
	
	if (s)
		return s;

	return Default;
}

bool LgiResources::LoadDialog(int Resource, GViewI *Parent, GRect *Pos, GAutoString *Name, GEventsI *Engine, char *TagList)
{
	bool Status = false;

	if (Resource)
	{
		ScriptEngine = Engine;
		TagHash Tags(TagList);

		for (LgiDialogRes *Dlg = Dialogs.First(); Dlg; Dlg = Dialogs.Next())
		{
			if (Dlg->Id() == ((int) Resource))
			{
				// found the dialog to load, set properties
				if (Dlg->Name())
				{
					if (Name)
						Name->Reset(NewStr(Dlg->Name()));
					else if (Parent)
						Parent->Name(Dlg->Name());
				}
				
				int x = Dlg->X();
				int y = Dlg->Y();
				x += LgiApp->GetMetric(LGI_MET_DECOR_X) - 4;
				y += LgiApp->GetMetric(LGI_MET_DECOR_Y) - 18;
				if (Pos)
					Pos->ZOff(x, y);
				else if (Parent && stricmp(Parent->GetClass(), "GTabPage"))
				{
					GRect r = Parent->GetPos();
					r.Dimension(x, y);
					Parent->SetPos(r);
				}

				// instantiate control list
				List<GXmlTag>::I it = Dlg->Dialog->Children.Start();
				for (GXmlTag *t = *it; t; t = *++it)
				{
					ResObject *Obj = CreateObject(t, 0);
					if (Obj)
					{
						if (Tags.Check(t))
						{
							if (Res_Read(Obj, t, Tags))
							{
								GView *w = dynamic_cast<GView*>(Obj);
								if (w)
									Parent->AddView(w);
							}
							else
							{
								LgiMsg(	NULL,
										LgiLoadString(	L_ERROR_RES_RESOURCE_READ_FAILED,
														"Resource read error, tag: %s"),
										"LgiResources::LoadDialog",
										MB_OK,
										t->GetTag());
								break;
							}
						}
					}
					else
					{
						LgiAssert(0);
					}
				}

				Status = true;
				break;
			}
		}
	}

	return Status;	
}

//////////////////////////////////////////////////////////////////////
// Menus
LgiStringRes *LgiMenuRes::GetString(GXmlTag *Tag)
{
	if (Tag)
	{
		char *n = Tag->GetAttr("ref");
		if (n)
		{
			int Ref = atoi(n);
			LgiStringRes *s = Strings.Find(Ref);
			if (s)
				return s;
		}
	}

	return 0;
}

bool GMenuLoader::Load(LgiMenuRes *MenuRes, GXmlTag *Tag, ResFileFormat Format, TagHash *TagList)
{
	bool Status = false;

	if (Tag && Tag->GetTag())
	{
		if (stricmp(Tag->GetTag(), "menu") == 0)
		{
			#if WINNATIVE
			if (!Info)
			{
				Info = ::CreateMenu();
			}
			#endif
		}

		#if WINNATIVE
		if (Info)
		#endif
		{
			Status = true;
			for (GXmlTag *t = Tag->Children.First(); t && Status; t = Tag->Children.Next())
			{
				if (t->IsTag("submenu"))
				{
					LgiStringRes *Str = MenuRes->GetString(t);
					if (Str && Str->Str)
					{
						bool Add = !TagList || TagList->Check(Str->Tag);
						GSubMenu *Sub = AppendSub(Str->Str);
						if (Sub)
						{
							GMenuItem *p = Sub->GetParent();
							if (p)
								p->Id(Str->Id);
							else
								LgiTrace("%s:%i - No Parent to set menu item ID.\n", _FL);
							
							Status = Sub->Load(MenuRes, t, Format, TagList);

							if (!Add)
							{
								// printf("Sub->GetParent()=%p this=%p\n", Sub->GetParent(), this);
								Sub->GetParent()->Remove();
								delete Sub->GetParent();
							}
						}
					}
					else
					{
						LgiAssert(0);
					}
				}
				else if (t->IsTag("menuitem"))
				{
					if (t->GetAsInt("sep") > 0)
					{
						AppendSeparator();
					}
					else
					{
						LgiStringRes *Str = MenuRes->GetString(t);
						if (Str && Str->Str)
						{
							if (!TagList || TagList->Check(Str->Tag))
							{
								int Enabled = t->GetAsInt("enabled");
								char *Shortcut = t->GetAttr("shortcut");
								Status = AppendItem(Str->Str, Str->Id, Enabled >= 0?Enabled:true, -1, Shortcut) != 0;
							}
							else Status = true;
						}
						else
						{
							LgiAssert(0);
						}
					}
				}
			}
		}
	}
	return Status;
}

bool GMenu::Load(GView *w, const char *Res, const char *TagList)
{
	bool Status = false;

	LgiResources *r = LgiGetResObj();
	if (r)
	{
		TagHash Tags(TagList);
		for (LgiMenuRes *m = r->Menus.First(); m; m = r->Menus.Next())
		{
			if (stricmp(m->Name(), Res) == 0)
			{
				#if WINNATIVE
				Status = GSubMenu::Load(m, m->Tag, r->GetFormat(), &Tags);
				#else
				Status = GMenuLoader::Load(m, m->Tag, r->GetFormat(), &Tags);
				#endif
				break;
			}
		}
	}

	return Status;
}

LgiResources *LgiGetResObj(bool Warn, const char *filename)
{
	// Look for existing file?
	if (filename)
	{
		for (int i=0; i<_ResourceOwner.Length(); i++)
		{
			LgiResources *r = _ResourceOwner[i];
			if (r && r->GetFileName())
			{
				if (!_stricmp(filename, r->GetFileName()))
					return r;
			}
		}

		// Load the new file...
		return new LgiResources(filename, Warn);
	}

	if (_ResourceOwner.Length())
		return _ResourceOwner[0];
	
	return new LgiResources(filename, Warn);
}
