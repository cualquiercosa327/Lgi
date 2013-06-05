#include "Lgi.h"
#include "IdeProject.h"
#include "GTableLayout.h"
#include "resdefs.h"
#include "GTextLabel.h"
#include "GEdit.h"

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
	GHashTbl<int, SettingInfo*> Map;
	IdeProjectSettings *Parent;
	GAutoString CurConfig;
	GArray<char*> Configs;
	
	IdeProjectSettingsPriv(IdeProjectSettings *parent)
	{
		Parent = parent;
		
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

	char *BuildPath(ProjSetting s, bool PlatformSpecific, int Config = -1)
	{
		SettingInfo *i = Map.Find(s);
		LgiAssert(i);
		char *Cfg = Config < 0 ? CurConfig : Configs[Config];
		sprintf_s(	PathBuf, sizeof(PathBuf),
					"%s.%s.%s.%s",
					i->Category,
					i->Name,
					PlatformSpecific ? sCurrentPlatform : sAllPlatforms,
					Cfg);
		return PathBuf;
	}
};

class GSettingDetail : public GLayout, public ResObject
{
	GTableLayout *Tbl;
	IdeProjectSettingsPriv *d;
	SettingInfo *Setting;
	bool PlatformSpecific;
	
	struct CtrlInfo
	{
		GText *Text;
		GEdit *Edit;
		
		CtrlInfo()
		{
			Text = 0;
			Edit = 0;
		}
	};
	
	GArray<CtrlInfo> Ctrls;

public:
	GSettingDetail() : ResObject(Res_Custom)
	{
		PlatformSpecific = false;
		d = NULL;
		Setting = NULL;
		AddView(Tbl = new GTableLayout);
	}
	
	void OnCreate()
	{
		AttachChildren();
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
	
	void SetPriv(IdeProjectSettingsPriv *priv)
	{
		d = priv;
	}
	
	void SetSetting(SettingInfo *setting, bool plat_spec)
	{
		if (Setting)
		{
			// Save previous values
			for (int i=0; i<Ctrls.Length(); i++)
			{
				char *Path = Ctrls[i].Text->Name();
				if (Ctrls[i].Edit)
				{
					char *Val = Ctrls[i].Edit->Name();
					if (Path && Val)
					{
						GXmlTag *t = d->Parent->GetTag(Path, true);
						if (t)
						{
							t->SetContent(Val);
						}
					}
				}
			}
		}	
	
		Tbl->Empty();
		Ctrls.Length(0);
		Setting = setting;
		PlatformSpecific = plat_spec;
		
		if (Setting)
		{
			GLayoutCell *c;
			
			for (int i=0; i<d->Configs.Length(); i++)
			{
				char *Path;
				int CellY = i * 2;
				c = Tbl->GetCell(0, CellY);
				c->Add(Ctrls[i].Text = new GText(50 + i, 0, 0, -1, -1, Path = d->BuildPath(Setting->Setting, PlatformSpecific, i)));
				c = Tbl->GetCell(0, CellY + 1);
				c->Add(Ctrls[i].Edit = new GEdit(100 + i, 0, 0, 60, 20));
				Ctrls[i].Edit->MultiLine(true);

				GXmlTag *t = d->Parent->GetTag(Path);
				if (t && t->Content)
				{
					Ctrls[i].Edit->Name(t->Content);
				}
			}
			
			Tbl->InvalidateLayout();
			Tbl->AttachChildren();
			Tbl->SetPos(GetClient());
		}
	}
	
	void OnPosChange()
	{
		Tbl->SetPos(GetClient());
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

class ProjectSettingsDlg;
class SettingItem : public GTreeItem
{
	ProjectSettingsDlg *Dlg;
	
public:
	SettingInfo *Setting;
	bool PlatformSpecific;

	SettingItem(SettingInfo *setting, bool plat_spec, ProjectSettingsDlg *dlg)
	{
		Setting = setting;
		Dlg = dlg;
		PlatformSpecific = plat_spec;
	}
	
	void Select(bool b);
};

class ProjectSettingsDlg : public GDialog
{
	IdeProjectSettingsPriv *d;
	GTree *Tree;
	GSettingDetail *Detail;

public:
	ProjectSettingsDlg(GViewI *parent, IdeProjectSettingsPriv *priv)
	{
		Tree = NULL;
		Detail = NULL;
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

					SettingItem *All = new SettingItem(i, false, this);
					All->SetText(sAllPlatforms);
					Item->Insert(All);

					SettingItem *Platform = new SettingItem(i, true, this);
					Platform->SetText(sCurrentPlatform);
					Item->Insert(Platform);
				}
			}
			
			if (GetViewById(IDC_DETAIL, Detail))
			{
				Detail->SetPriv(d);
			}
		}
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDOK:
				Detail->SetSetting(NULL, false);
				EndModal(1);
				break;
			case IDCANCEL:
				EndModal(0);
				break;
		}
		
		return GDialog::OnNotify(Ctrl, Flags);
	}
	
	void OnSelect(SettingItem *si)
	{
		Detail->SetSetting(si->Setting, si->PlatformSpecific);
	}
};

void SettingItem::Select(bool b)
{
	GTreeItem::Select(b);
	if (b)
		Dlg->OnSelect(this);
}

IdeProjectSettings::IdeProjectSettings() :
	GXmlTag(TagSettings)
{
	d = new IdeProjectSettingsPriv(this);
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
	for (SettingInfo *i = AllSettings; i->Setting; i++)
	{
		for (int Cfg = 0; Cfg < d->Configs.Length(); Cfg++)
		{
			char *p = d->BuildPath(i->Setting, false, Cfg);
			CreateTag(p);
			
			p = d->BuildPath(i->Setting, true, Cfg);
			CreateTag(p);
		}
	}
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
	GXmlTag *t = GetTag(path, true);
	if (t)
	{
		GXmlTag *c = t->GetTag(path, true);
		if (c)
			Status = c->Content;
	}
	
	return Status;
}

int IdeProjectSettings::GetInt(ProjSetting Setting, int Default)
{
	int Status = Default;
	char *path = d->BuildPath(Setting, true);
	GXmlTag *t = GetTag(path, true);
	if (t)
	{
		GXmlTag *c = t->GetTag(path, true);
		if (c && c->Content)
			Status = atoi(c->Content);
	}
	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, const char *Value)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, true);
	GXmlTag *t = GetTag(path, true);
	if (t)
	{
		GXmlTag *c = t->GetTag(path, true);
		if (c)
		{
			DeleteArray(c->Content);
			c->Content = NewStr(Value);
		}
	}
	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, int Value)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, true);
	GXmlTag *t = GetTag(path, true);
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
	}
	return Status;
}
