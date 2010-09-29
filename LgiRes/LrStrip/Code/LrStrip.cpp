#include "Lgi.h"
#include "resdefs.h"
#include "GXmlTree.h"
#include "GList.h"
#include "GListItemCheckBox.h"
#include "GToken.h"

char AppName[] = "LrStrip";

int Cmp(GLanguage *a, GLanguage *b, int d)
{
	return stricmp(a->Name, b->Name);
}

class LangInc : public GListItem
{
	GLanguage *Lang;
	GListItemCheckBox *Inc;

public:
	LangInc(GLanguage *l)
	{
		Lang = l;
		SetText(l->Name, 0);
		SetText(l->Id, 1);
		Inc = new GListItemCheckBox(this, 2);
	}

	bool Value() { return Inc->Value(); }
};

class App : public GDialog
{
	GXmlTag *Tree;
	GList *Inc;

public:
	GHashTable Langs;

	App()
	{
		Tree = 0;
		Inc = 0;
		if (LoadFromResource(IDD_APP))
		{
			MoveToCenter();
			GetViewById(IDC_LANG, Inc);
		}
	}

	~App()
	{
		DeleteObj(Tree);
	}

	void CollectLangs(GXmlTag *t)
	{
		if (t->Tag AND stricmp(t->Tag, "String") == 0)
		{
			for (GXmlAttr *a = t->Attr.First(); a; a = t->Attr.Next())
			{
				GLanguage *l = GFindLang(a->Name);
				if (l)
				{
					if (NOT Langs.Find(l->Id))
					{
						Langs.Add(l->Id, l);
					}
				}
			}
		}

		for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
		{
			CollectLangs(c);
		}
	}

	bool Load(char *File)
	{
		bool Status = false;

		Langs.Empty();
		DeleteObj(Tree);
		Tree = new GXmlTag;
		if (Tree)
		{
			if (File)
			{
				GFile f;
				if (f.Open(File, O_READ))
				{
					GXmlTree t(GXT_NO_ENTITIES);
					if (t.Read(Tree, &f, 0))
					{
						CollectLangs(Tree);

						List<GLanguage> Lst;
						char *Key = 0;
						for (void *p = Langs.First(&Key); p; p = Langs.Next(&Key))
						{
							Lst.Insert((GLanguage*)p);
						}
						Lst.Sort(Cmp, 0);

						for (GLanguage *l = Lst.First(); l; l = Lst.Next())
						{
							Inc->Insert(new LangInc(l));
						}

						Status = true;
					}
					else
					{
						LgiMsg(this, "Couldn't parse XML.", AppName);
					}
				}
				else
				{
					LgiMsg(this, "Couldn't open '%s'\n", AppName, MB_OK, File);
				}
			}
			else
			{
				LgiMsg(this, "No file.", AppName);
			}
		}

		return Status;
	}

	void RemoveLangs(GXmlTag *t)
	{
		if (t->Tag AND stricmp(t->Tag, "String") == 0)
		{
			for (GXmlAttr *a = t->Attr.First(); a; )
			{
				GLanguage *l = GFindLang(a->Name);
				if (l)
				{
					if (NOT Langs.Find(l->Id))
					{
						t->Attr.Delete(a);
						DeleteObj(a);
						a = t->Attr.Current();
						continue;
					}
				}

				a = t->Attr.Next();
			}
		}

		for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
		{
			RemoveLangs(c);
		}
	}

	bool Save(char *File)
	{
		bool Status = false;

		if (Tree AND Inc)
		{
			if (Langs.Length())
			{
				// scan tree, removing the languages we don't want
				RemoveLangs(Tree);

				// write the remaining tree out to a file...
				GFile f;
				if (f.Open(File, O_WRITE))
				{
					f.SetSize(0);

					GXmlTree t(GXT_NO_ENTITIES | GXT_NO_PRETTY_WHITESPACE);
					if (t.Write(Tree, &f))
					{
						Status = true;
					}
					else
					{
						LgiMsg(this, "XML output failed.", AppName);
					}
				}
				else
				{
					LgiMsg(this, "Couldn't open '%s'.", AppName, MB_OK, File);
				}
			}
			else
			{
				LgiMsg(this, "No languages selected.", AppName);
			}
		}

		return Status;
	}

	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDC_CMD_LINE:
			{
				LgiMsg(	this,
						"These are the command line options you can use with LrStrip. If you specify these\n"
						"then no window will be shown and the application will exit after it's completed\n"
						"it's work.\n"
						"\n"
						"Required options:\n"
						"\n"
						"    -in \'<FileName>\'\n"
						"    -out \'<FileName>\'\n"
						"    -langs \'<LangId>[, <LangId>]\'\n"
						"\n"
						"Where 'LangId' is a standard language code (like 'en' for english).",
						AppName);
				break;
			}
			case IDC_LOAD_FILE:
			{
				GFileSelect s;
				s.Parent(this);
				s.Type("Lgi Resource", "*.lr*");
				if (s.Open())
				{
					Load(s.Name());
				}
				break;
			}
			case IDOK:
			{
				GFileSelect s;
				s.Parent(this);
				s.Type("Lgi Resource", "*.lr*");
				if (s.Save())
				{
					// put together the list of langs
					Langs.Empty();
					for (LangInc *i = (LangInc*)Inc->First(); i; i = (LangInc*)Inc->Next())
					{
						if (i->Value())
						{
							Langs.Add(i->GetText(1), i);
						}
					}

					// overwrite check
					if (FileExists(s.Name()))
					{
						if (LgiMsg(this, "Overwrite '%s'?", AppName, MB_YESNO, s.Name()) == IDNO)
						{
							return 0;
						}
					}

					// save the file...
					if (Save(s.Name()))
					{
						LgiMsg(this, "Striped resource file written ok.", AppName);
					}
				}
				break;
			}
			case IDCANCEL:
			{
				EndModal(c->GetId());
				break;
			}
		}

		return 0;
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	GApp *a = new GApp("application/lrstrip", AppArgs);
	if (a)
	{
		{
			App Wnd;

			char In[256];
			char Out[256];
			char Langs[256];
			if (LgiApp->GetOption("In", In) AND
				LgiApp->GetOption("Out", Out) AND
				LgiApp->GetOption("Langs", Langs))
			{
				if (Wnd.Load(In))
				{
					if (stricmp(Langs, "all"))
					{
						Wnd.Langs.Empty();

						GToken l(Langs, " ,;");
						for (int i=0; i<l.Length(); i++)
						{
							Wnd.Langs.Add(l[i]);
						}
					}

					Wnd.Save(Out);
				}

				Wnd.Children.DeleteObjects();
			}
			else
			{
				Wnd.DoModal();
			}
		}

		DeleteObj(a);
	}

	return 0;
}