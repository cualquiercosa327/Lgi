#include "Lgi.h"
#include "IdeProject.h"
#include "resdefs.h"

struct SettingInfo
{
	ProjSetting Setting;
	int Type;
	const char *Name;
	const char *Category;
};

const char TagSettings[] = "Settings";
const char TagConfigs[] = "Configurations";

const char sGeneral[] = "General";
const char sBuild[] = "Build";
const char sEditor[] = "Editor";
const char sAdvanced[] = "Advanced";
const char sDebug[] = "Debug";
const char sRelease[] = "Release";
const char sAllPlatforms[] = "All";
const char sCurrentPlatform[] =
	#if defined WIN32
	"Windows"
	#elif defined LINUX
	"Linux"
	#elif defined MAC
	"Mac"
	#elif defined BEOS
	"Haiku"
	#else
	#error "Not impl"
	#endif
	;


SettingInfo AllSettings[] =
{
	{ProjMakefile,				GV_STRING,		"Makefile",			sGeneral},
	{ProjExe,					GV_STRING,		"Executable",		sGeneral},
	{ProjArgs,					GV_STRING,		"Arguments",		sGeneral},
	{ProjDefines,				GV_STRING,		"Defines",			sGeneral},
	{ProjCompiler,				GV_INT32,		"Compiler",			sGeneral},
	{ProjIncludePaths,			GV_STRING,		"IncludePaths",		sBuild},
	{ProjLibraries,				GV_STRING,		"Libraries",		sBuild},
	{ProjLibraryPaths,			GV_STRING,		"LibraryPaths",		sBuild},
	{ProjTargetType,			GV_INT32,		"TargetType",		sBuild},
	{ProjTargetName,			GV_STRING,		"TargetName",		sBuild},
	{ProjEditorTabSize,			GV_INT32,		"TabSize",			sEditor},
	{ProjEditorIndentSize,		GV_INT32,		"IndentSize",		sEditor},
	{ProjEditorShowWhiteSpace,	GV_BOOL,		"ShowWhiteSpace",	sEditor},
	{ProjEditorUseHardTabs,		GV_BOOL,		"UseHardTabs",		sEditor},
	{ProjCommentFile,			GV_STRING,		"CommentFile",		sEditor},
	{ProjCommentFunction,		GV_STRING,		"CommentFunction",	sEditor},
	{ProjMakefileRules,			GV_STRING,		"OtherMakefileRules", sAdvanced},
	{ProjNone,					GV_NULL,		NULL,				NULL},
};

struct IdeProjectSettingsPriv
{
	char PathBuf[256];

public:
	GOptionsFile *Opts;
	GHashTbl<int, SettingInfo*> Map;

	GAutoString CurConfig;
	GArray<char*> Configs;
	
	IdeProjectSettingsPriv()
	{
		Opts = NULL;
		for (SettingInfo *i = AllSettings; i->Setting; i++)
		{
			Map.Add(i->Setting, i);
		}
		
		Configs.Add(NewStr(sDebug));
		Configs.Add(NewStr(sRelease));
		CurConfig.Reset(NewStr(sDebug));
	}

	int FindConfig(const char *Cfg)
	{
		for (int i=0; i<Configs.Length(); i++)
		{
			if (!stricmp(Configs[i], Cfg))
				return i;
		}
		return -1;
	}

	char *BuildPath(ProjSetting s, bool PlatformSpecific)
	{
		SettingInfo *i = Map.Find(s);
		LgiAssert(i);
		sprintf_s(	PathBuf, sizeof(PathBuf),
					"%s.%s.%s.%s",
					i->Category,
					i->Name,
					PlatformSpecific ? sCurrentPlatform : sAllPlatforms,
					CurConfig.Get());
		return PathBuf;
	}
};

class GSettingDetail : public GLayout, public ResObject
{
public:
	GSettingDetail() : ResObject(Res_Custom)
	{
		Sunken(true);
	}
	
	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
	}
	
	bool OnLayout(GViewLayoutInfo &Inf)
	{
		Inf.Width.Min = -1;
		Inf.Width.Max = -1;
		Inf.Height.Min = -1;
		Inf.Height.Max = -1;
		return true;
	}
};

class GSettingDetailFactory : public GViewFactory
{
	GView *NewView
	(
		const char *Class,
		GRect *Pos,
		const char *Text
	)
	{
		if (!stricmp(Class, "GSettingDetail"))
			return new GSettingDetail;
		return NULL;
	}
} SettingDetailFactory;

class ProjectSettingsDlg : public GDialog
{
	IdeProjectSettingsPriv *d;
	GTree *Tree;

public:
	ProjectSettingsDlg(GViewI *parent, IdeProjectSettingsPriv *priv)
	{
		Tree = NULL;
		d = priv;
		SetParent(parent);
		if (LoadFromResource(IDD_PROJECT_SETTINGS2))
		{
			MoveToCenter();
			
			if (GetViewById(IDC_SETTINGS, Tree))
			{
				const char *Section = NULL;
				GTreeItem *SectionItem = NULL;
				for (SettingInfo *i = AllSettings; i->Setting; i++)
				{
					if (!SectionItem || (Section && stricmp(i->Category, Section)))
					{
						Section = i->Category;
						SectionItem = new GTreeItem();
						SectionItem->SetText(i->Category);
						Tree->Insert(SectionItem);
					}
					
					GTreeItem *Item = new GTreeItem();
					Item->SetText(i->Name);
					SectionItem->Insert(Item);
					SectionItem->Expanded(true);

					GTreeItem *All = new GTreeItem();
					All->SetText(sAllPlatforms);
					Item->Insert(All);

					GTreeItem *Platform = new GTreeItem();
					Platform->SetText(sCurrentPlatform);
					Item->Insert(Platform);
				}
			}
		}
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDOK:
				EndModal(1);
				break;
			case IDCANCEL:
				EndModal(0);
				break;
		}
		
		return GDialog::OnNotify(Ctrl, Flags);
	}
};

IdeProjectSettings::IdeProjectSettings(GOptionsFile *Opts)
{
	d = new IdeProjectSettingsPriv;
	d->Opts = Opts;
	InitAllSettings();
}

IdeProjectSettings::~IdeProjectSettings()
{
	DeleteObj(d);
}

const char *IdeProjectSettings::GetCurrentConfig()
{
	return d->CurConfig;
}

bool IdeProjectSettings::SetCurrentConfig(const char *Config)
{
	int i = d->FindConfig(Config);
	if (i < 0)
		return false;
	
	d->CurConfig.Reset(NewStr(d->Configs[i]));
	return true;
}

bool IdeProjectSettings::AddConfig(const char *Config)
{
	int i = d->FindConfig(Config);
	if (i >= 0)
		return true;

	d->Configs.Add(NewStr(Config));
	return true;
}

bool IdeProjectSettings::DeleteConfig(const char *Config)
{
	int i = d->FindConfig(Config);
	if (i < 0)
		return false;
	
	delete [] d->Configs[i];
	d->Configs.DeleteAt(i);
	return true;
}

void IdeProjectSettings::InitAllSettings(bool ClearCurrent)
{
}

void IdeProjectSettings::Edit(GViewI *parent)
{
	ProjectSettingsDlg Dlg(parent, d);
	if (Dlg.DoModal())
	{
		
	}
}

const char *IdeProjectSettings::GetStr(ProjSetting Setting, const char *Default)
{
	const char *Status = Default;
	char *path = d->BuildPath(Setting, true);
	GXmlTag *t = d->Opts->LockTag(path, _FL);
	if (t)
	{
		GXmlTag *c = t->GetTag(path, true);
		if (c)
			Status = c->Content;
		d->Opts->Unlock();
	}
	
	return Status;
}

int IdeProjectSettings::GetInt(ProjSetting Setting, int Default)
{
	int Status = Default;
	char *path = d->BuildPath(Setting, true);
	GXmlTag *t = d->Opts->LockTag(path, _FL);
	if (t)
	{
		GXmlTag *c = t->GetTag(path, true);
		if (c && c->Content)
			Status = atoi(c->Content);
		d->Opts->Unlock();
	}
	
	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, const char *Value)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, true);
	GXmlTag *t = d->Opts->LockTag(path, _FL);
	if (t)
	{
		GXmlTag *c = t->GetTag(path, true);
		if (c)
		{
			DeleteArray(c->Content);
			c->Content = NewStr(Value);
		}
		
		d->Opts->Unlock();
	}
	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, int Value)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, true);
	GXmlTag *t = d->Opts->LockTag(path, _FL);
	if (t)
	{
		GXmlTag *c = t->GetTag(path, true);
		if (c)
		{
			DeleteArray(c->Content);
			char s[32];
			sprintf_s(s, sizeof(s), "%i", Value);
			c->Content = NewStr(s);
		}
		
		d->Opts->Unlock();
	}
	return Status;
}
