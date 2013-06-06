#include "Lgi.h"
#include "IdeProject.h"
#include "GTableLayout.h"
#include "resdefs.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GCheckBox.h"

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

#define SF_MULTILINE			0x1 // String setting is multiple lines, otherwise it's single line
#define SF_CROSSPLATFORM		0x2	// Just has a "all" platforms setting (no platform specific)
#define SF_PLATFORM_SPECIFC		0x4 // Just has a platform specific setting (no all)
#define SF_CONFIG_SPECIFIC		0x8 // Can have a different setting in different config

#define IDC_TEXT_BASE			100
#define IDC_EDIT_BASE			200
#define IDC_CHECKBOX_BASE		300

struct SettingInfo
{
	ProjSetting Setting;
	int Type;
	const char *Name;
	const char *Category;
	union {
		uint32 Flags;
		struct BitFlags
		{
			uint32 MultiLine : 1;
			uint32 CrossPlatform : 1;
			uint32 PlatformSpecific : 1;
			uint32 ConfigSpecific : 1;
		} Flag;
	};
};

SettingInfo AllSettings[] =
{
	{ProjMakefile,				GV_STRING,		"Makefile",			sGeneral,	SF_PLATFORM_SPECIFC},
	{ProjExe,					GV_STRING,		"Executable",		sGeneral,	SF_PLATFORM_SPECIFC|SF_CONFIG_SPECIFIC},
	{ProjArgs,					GV_STRING,		"Arguments",		sGeneral,	SF_CROSSPLATFORM|SF_CONFIG_SPECIFIC},
	{ProjDefines,				GV_STRING,		"Defines",			sGeneral,	SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjCompiler,				GV_INT32,		"Compiler",			sGeneral,	SF_PLATFORM_SPECIFC},
	{ProjIncludePaths,			GV_STRING,		"IncludePaths",		sBuild,		SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjLibraries,				GV_STRING,		"Libraries",		sBuild,		SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjLibraryPaths,			GV_STRING,		"LibraryPaths",		sBuild,		SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjTargetType,			GV_INT32,		"TargetType",		sBuild,		SF_CROSSPLATFORM},
	{ProjTargetName,			GV_STRING,		"TargetName",		sBuild,		SF_PLATFORM_SPECIFC|SF_CONFIG_SPECIFIC},
	{ProjEditorTabSize,			GV_INT32,		"TabSize",			sEditor,	SF_CROSSPLATFORM},
	{ProjEditorIndentSize,		GV_INT32,		"IndentSize",		sEditor,	SF_CROSSPLATFORM},
	{ProjEditorShowWhiteSpace,	GV_BOOL,		"ShowWhiteSpace",	sEditor,	SF_CROSSPLATFORM},
	{ProjEditorUseHardTabs,		GV_BOOL,		"UseHardTabs",		sEditor,	SF_CROSSPLATFORM},
	{ProjCommentFile,			GV_STRING,		"CommentFile",		sEditor,	SF_MULTILINE|SF_CROSSPLATFORM},
	{ProjCommentFunction,		GV_STRING,		"CommentFunction",	sEditor,	SF_MULTILINE|SF_CROSSPLATFORM},
	{ProjMakefileRules,			GV_STRING,		"OtherMakefileRules", sAdvanced, SF_MULTILINE},
	{ProjNone,					GV_NULL,		NULL,				NULL,		0},
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
		int Ch = sprintf_s(	PathBuf, sizeof(PathBuf),
							"%s.%s.%s",
							i->Category,
							i->Name,
							PlatformSpecific ? sCurrentPlatform : sAllPlatforms);

		if (Config >= 0)
		{
			sprintf_s(	PathBuf+Ch, sizeof(PathBuf)-Ch,
						".%s",
						Configs[Config]);
		}
		
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
		GCheckBox *Chk;
		
		CtrlInfo()
		{
			Text = NULL;
			Edit = NULL;
			Chk = NULL;
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
	
	void AddLine(int i, int Config)
	{
		char *Path;
		int CellY = i * 2;
		GLayoutCell *c = Tbl->GetCell(0, CellY);
		c->Add(Ctrls[i].Text = new GText(IDC_TEXT_BASE + i, 0, 0, -1, -1, Path = d->BuildPath(Setting->Setting, PlatformSpecific, Config)));
		c = Tbl->GetCell(0, CellY + 1);
		
		GXmlTag *t = d->Parent->GetTag(Path);
		if (Setting->Type == GV_STRING)
		{
			c->Add(Ctrls[i].Edit = new GEdit(IDC_EDIT_BASE + i, 0, 0, 60, 20));
			Ctrls[i].Edit->MultiLine(Setting->Flag.MultiLine);
			if (t && t->Content)
				Ctrls[i].Edit->Name(t->Content);
		}
		else if (Setting->Type == GV_INT32)
		{
			c->Add(Ctrls[i].Edit = new GEdit(IDC_EDIT_BASE + i, 0, 0, 60, 20));
			Ctrls[i].Edit->MultiLine(Setting->Flag.MultiLine);
			if (t)
				Ctrls[i].Edit->Value(t->Content ? atoi(t->Content) : 0);
		}
		else if (Setting->Type == GV_BOOL)
		{
			c->Add(Ctrls[i].Chk  = new GCheckBox(IDC_CHECKBOX_BASE + i, 0, 0, -1, -1, NULL));
			if (t && t->Content)
				Ctrls[i].Chk->Value(atoi(t->Content));
		}
		else LgiAssert(!"Unknown type?");
	}
	
	void SetSetting(SettingInfo *setting, bool plat_spec)
	{
		if (Setting)
		{
			// Save previous values
			for (int i=0; i<Ctrls.Length(); i++)
			{
				if (!Ctrls[i].Text)
					continue;
				
				char *Path = Ctrls[i].Text->Name();
				if (!Path)
					continue;

				GXmlTag *t = d->Parent->GetTag(Path, true);
				if (!t)
					continue;

				if (Ctrls[i].Edit)
				{
					char *Val = Ctrls[i].Edit->Name();
					t->SetContent(Val);
				}
				else if (Ctrls[i].Chk)
				{
					int64 Val = Ctrls[i].Chk->Value();
					t->SetContent((int)Val);
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
			
			if (Setting->Flag.ConfigSpecific)
			{
				for (int i=0; i<d->Configs.Length(); i++)
				{
					AddLine(i, i);
				}
			}
			else
			{
				AddLine(0, -1);
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

					if (!i->Flag.PlatformSpecific)
					{
						SettingItem *All = new SettingItem(i, false, this);
						All->SetText(sAllPlatforms);
						Item->Insert(All);
					}

					if (!i->Flag.CrossPlatform)
					{
						SettingItem *Platform = new SettingItem(i, true, this);
						Platform->SetText(sCurrentPlatform);
						Item->Insert(Platform);
					}
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
		if (i->Flag.ConfigSpecific)
		{
			for (int Cfg = 0; Cfg < d->Configs.Length(); Cfg++)
			{
				char *p;
				
				if (!i->Flag.PlatformSpecific)
				{
					p = d->BuildPath(i->Setting, false, Cfg);
					CreateTag(p);
				}
				
				if (!i->Flag.CrossPlatform)
				{
					p = d->BuildPath(i->Setting, true, Cfg);
					CreateTag(p);
				}
			}
		}
		else
		{
			char *p = d->BuildPath(i->Setting, false, -1);
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
