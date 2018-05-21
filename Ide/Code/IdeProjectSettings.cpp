#include "Lgi.h"
#include "LgiIde.h"
#include "IdeProject.h"
#include "GTableLayout.h"
#include "resdefs.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GCombo.h"
#include "GButton.h"

const char TagSettings[] = "Settings";
// const char TagConfigs[] = "Configurations";

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

const char *sCompilers[] =
{
	#if defined WIN32
	"VisualStudio",
	"Cygwin",
	"MingW",
	"IAR",
	#elif defined MAC
	"XCode",
	#else
	"gcc",
	"cross",
	#endif
	NULL
};

const char *sBuildTypes[] =
{
	"Executable",
	"StaticLibrary",
	"DynamicLibrary",
	NULL
};

static const char **GetEnumValues(ProjSetting s)
{
	switch (s)
	{
		case ProjCompiler:
			return sCompilers;
		case ProjTargetType:
			return sBuildTypes;
		default:
			LgiAssert(!"Unknown enum type.");
			break;
	}
	
	return NULL;
}

#define SF_MULTILINE			0x01	// String setting is multiple lines, otherwise it's single line
#define SF_CROSSPLATFORM		0x02	// Just has a "all" platforms setting (no platform specific)
#define SF_PLATFORM_SPECIFC		0x04	// Just has a platform specific setting (no all)
#define SF_CONFIG_SPECIFIC		0x08	// Can have a different setting in different configs
#define SF_ENUM					0x10	// Integer is actually an enum index, not a straight number
#define SF_FILE_SELECT			0x20	// UI needs a file selection button

#define IDC_TEXT_BASE			100
#define IDC_EDIT_BASE			200
#define IDC_CHECKBOX_BASE		300
#define IDC_COMBO_BASE			400
#define IDC_BROWSE_FILE			500

struct SettingInfo
{
	struct BitFlags
	{
		uint32 MultiLine : 1;
		uint32 CrossPlatform : 1;
		uint32 PlatformSpecific : 1;
		uint32 ConfigSpecific : 1;
		uint32 Enum : 1;
		uint32 FileSelect : 1;
	};
	
	ProjSetting Setting;
	int Type;
	const char *Name;
	const char *Category;
	union {
		uint32 Flags;
		BitFlags Flag;
	};
};

SettingInfo AllSettings[] =
{
	{ProjMakefile,				GV_STRING,		"Makefile",			sGeneral,	SF_PLATFORM_SPECIFC|SF_FILE_SELECT},
	{ProjExe,					GV_STRING,		"Executable",		sGeneral,	SF_PLATFORM_SPECIFC|SF_CONFIG_SPECIFIC},
	{ProjArgs,					GV_STRING,		"Arguments",		sGeneral,	SF_CROSSPLATFORM|SF_CONFIG_SPECIFIC},
	{ProjDebugAdmin,			GV_BOOL,		"DebugAdmin",		sGeneral,	SF_CROSSPLATFORM},
	{ProjDefines,				GV_STRING,		"Defines",			sGeneral,	SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjCompiler,				GV_INT32,		"Compiler",			sGeneral,	SF_PLATFORM_SPECIFC|SF_ENUM},
	{ProjCCrossCompiler,		GV_STRING,		"CCrossCompiler",	sGeneral,	SF_PLATFORM_SPECIFC|SF_FILE_SELECT},
	{ProjCppCrossCompiler,		GV_STRING,		"CppCrossCompiler",	sGeneral,	SF_PLATFORM_SPECIFC|SF_FILE_SELECT},
	{ProjIncludePaths,			GV_STRING,		"IncludePaths",		sBuild,		SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjSystemIncludes,		GV_STRING,		"SystemIncludes",	sBuild,		SF_MULTILINE|SF_CONFIG_SPECIFIC|SF_PLATFORM_SPECIFC},
	{ProjLibraries,				GV_STRING,		"Libraries",		sBuild,		SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjLibraryPaths,			GV_STRING,		"LibraryPaths",		sBuild,		SF_MULTILINE|SF_CONFIG_SPECIFIC},
	{ProjTargetType,			GV_INT32,		"TargetType",		sBuild,		SF_CROSSPLATFORM|SF_ENUM},
	{ProjTargetName,			GV_STRING,		"TargetName",		sBuild,		SF_PLATFORM_SPECIFC|SF_CONFIG_SPECIFIC},
	{ProjApplicationIcon,		GV_STRING,		"ApplicationIcon",	sBuild,		SF_PLATFORM_SPECIFC|SF_FILE_SELECT},
	{ProjEditorTabSize,			GV_INT32,		"TabSize",			sEditor,	SF_CROSSPLATFORM},
	{ProjEditorIndentSize,		GV_INT32,		"IndentSize",		sEditor,	SF_CROSSPLATFORM},
	{ProjEditorShowWhiteSpace,	GV_BOOL,		"ShowWhiteSpace",	sEditor,	SF_CROSSPLATFORM},
	{ProjEditorUseHardTabs,		GV_BOOL,		"UseHardTabs",		sEditor,	SF_CROSSPLATFORM},
	{ProjCommentFile,			GV_STRING,		"CommentFile",		sEditor,	SF_MULTILINE|SF_CROSSPLATFORM},
	{ProjCommentFunction,		GV_STRING,		"CommentFunction",	sEditor,	SF_MULTILINE|SF_CROSSPLATFORM},
	{ProjMakefileRules,			GV_STRING,		"OtherMakefileRules", sAdvanced, SF_MULTILINE},
	{ProjPostBuildCommands,		GV_STRING,		"PostBuildCommands", sAdvanced, SF_MULTILINE},
	{ProjNone,					GV_NULL,		NULL,				NULL,		0},
};

static void ClearEmptyTags(GXmlTag *t)
{
	for (int i=0; i<t->Children.Length(); i++)
	{
		GXmlTag *c = t->Children[i];
		if (!c->GetContent() && !c->Children.Length())
		{
			c->RemoveTag();
			i--;
			DeleteObj(c);
		}
		else
		{
			ClearEmptyTags(c);
		}
	}
}

struct IdeProjectSettingsPriv
{
	char PathBuf[256];

public:
	IdeProject *Project;
	GHashTbl<int, SettingInfo*> Map;
	IdeProjectSettings *Parent;
	GXmlTag Active;
	GXmlTag Editing;
	GAutoString CurConfig;
	GArray<char*> Configs;
	
	GAutoString StrBuf; // Temporary storage for generated settings
	
	IdeProjectSettingsPriv(IdeProjectSettings *parent) :
		Active(TagSettings)
	{
		Parent = parent;
		Project = NULL;
		
		for (SettingInfo *i = AllSettings; i->Setting; i++)
		{
			Map.Add(i->Setting, i);
		}
		
		Configs.Add(NewStr(sDebug));
		Configs.Add(NewStr(sRelease));
		CurConfig.Reset(NewStr(sDebug));
	}
	
	~IdeProjectSettingsPriv()
	{
		Configs.DeleteArrays();
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
	
	int CurConfigIdx()
	{
		return FindConfig(CurConfig);
	}

	const char *PlatformToString(int Flags, IdePlatform Platform)
	{
		if (Flags & SF_PLATFORM_SPECIFC)
		{
			if (Platform >= 0 && Platform < PlatformMax)
			{
				return PlatformNames[Platform];
			}
			
			return sCurrentPlatform;
		}
		
		return sAllPlatforms;
	}

	char *BuildPath(ProjSetting s, int Flags, IdePlatform Platform, int Config = -1)
	{
		SettingInfo *i = Map.Find(s);
		LgiAssert(i);

		const char *PlatformStr = PlatformToString(Flags, Platform);
		int Ch = sprintf_s(	PathBuf, sizeof(PathBuf),
							"%s.%s.%s",
							i->Category,
							i->Name,
							PlatformStr);

		if (i->Flag.ConfigSpecific)
		{
			sprintf_s(	PathBuf+Ch, sizeof(PathBuf)-Ch,
						".%s",
						Config >= 0 ? Configs[Config] : CurConfig);
		}
		
		return PathBuf;
	}
};

class GSettingDetail : public GLayout, public ResObject
{
	GTableLayout *Tbl;
	IdeProjectSettingsPriv *d;
	SettingInfo *Setting;
	int Flags;
	
	struct CtrlInfo
	{
		GTextLabel *Text;
		GEdit *Edit;
		GCheckBox *Chk;
		GCombo *Cbo;
		
		CtrlInfo()
		{
			Text = NULL;
			Edit = NULL;
			Chk = NULL;
			Cbo = NULL;
		}
	};
	
	GArray<CtrlInfo> Ctrls;

public:
	GSettingDetail() : ResObject(Res_Custom)
	{
		Flags = 0;
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
		
		// Do label cell
		GLayoutCell *c = Tbl->GetCell(0, CellY);
		c->Add(Ctrls[i].Text = new GTextLabel(IDC_TEXT_BASE + i, 0, 0, -1, -1, Path = d->BuildPath(Setting->Setting, Flags, PlatformCurrent, Config)));
		
		// Do value cell
		c = Tbl->GetCell(0, CellY + 1);
		
		GXmlTag *t = d->Editing.GetChildTag(Path);
		if (Setting->Type == GV_STRING)
		{
			c->Add(Ctrls[i].Edit = new GEdit(IDC_EDIT_BASE + i, 0, 0, 60, 20));
			Ctrls[i].Edit->MultiLine(Setting->Flag.MultiLine);
			if (t && t->GetContent())
				Ctrls[i].Edit->Name(t->GetContent());
			
			if (Setting->Flag.FileSelect)
			{
				c = Tbl->GetCell(1, CellY + 1);
				if (c)
				{
					c->Add(new GButton(IDC_BROWSE_FILE + i, 0, 0, -1, -1, "..."));
				}
				else LgiTrace("%s:%i - No cell.\n", _FL);
			}
		}
		else if (Setting->Type == GV_INT32)
		{
			if (Setting->Flag.Enum)
			{
				// Enum setting
				c->Add(Ctrls[i].Cbo = new GCombo(IDC_COMBO_BASE + i, 0, 0, 60, 20));
				
				const char **Init = GetEnumValues(Setting->Setting);
				if (Init)
				{
					for (int n=0; Init[n]; n++)
					{
						Ctrls[i].Cbo->Insert(Init[n]);
						if (t && t->GetContent() && !stricmp(t->GetContent(), Init[n]))
							Ctrls[i].Cbo->Value(n);
					}
				}
			}
			else
			{
				// Straight integer
				c->Add(Ctrls[i].Edit = new GEdit(IDC_EDIT_BASE + i, 0, 0, 60, 20));
				if (t)
					Ctrls[i].Edit->Value(t->GetContent() ? atoi(t->GetContent()) : 0);
			}
		}
		else if (Setting->Type == GV_BOOL)
		{
			c->Add(Ctrls[i].Chk  = new GCheckBox(IDC_CHECKBOX_BASE + i, 0, 0, -1, -1, NULL));
			if (t && t->GetContent())
				Ctrls[i].Chk->Value(atoi(t->GetContent()));
		}
		else LgiAssert(!"Unknown type?");
	}
	
	void SetSetting(SettingInfo *setting, int flags)
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
				{
					LgiAssert(0);
					continue;
				}

				GXmlTag *t = d->Editing.GetChildTag(Path, true);
				if (!t)
				{
					LgiAssert(0);
					continue;
				}

				if (Ctrls[i].Edit)
				{
					char *Val = Ctrls[i].Edit->Name();
					// LgiTrace("Saving edit setting '%s': '%s'\n", Path, Val);
					t->SetContent(Val);
				}
				else if (Ctrls[i].Chk)
				{
					int64 Val = Ctrls[i].Chk->Value();
					// LgiTrace("Saving bool setting '%s': "LGI_PrintfInt64"\n", Path, Val);
					t->SetContent((int)Val);
				}
				else if (Ctrls[i].Cbo)
				{
					char *Val = Ctrls[i].Cbo->Name();
					// LgiTrace("Saving enum setting '%s': '%s'\n", Path, Val);
					t->SetContent(Val);
				}
				else LgiAssert(0);
			}
		}	
	
		Tbl->Empty();
		Ctrls.Length(0);
		Setting = setting;
		Flags = flags;
		
		if (Setting)
		{
			// GLayoutCell *c;
			
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
	int Flags;

	SettingItem(SettingInfo *setting, int flags, ProjectSettingsDlg *dlg)
	{
		Setting = setting;
		Dlg = dlg;
		Flags = flags;
	}
	
	void Select(bool b);
};

class ProjectSettingsDlg : public GDialog
{
	IdeProjectSettingsPriv *d;
	GTree *Tree;
	GSettingDetail *Detail;
	uint64 DefLockOut;

public:
	ProjectSettingsDlg(GViewI *parent, IdeProjectSettingsPriv *priv)
	{
		Tree = NULL;
		Detail = NULL;
		DefLockOut = 0;
		d = priv;
		SetParent(parent);
		if (LoadFromResource(IDD_PROJECT_SETTINGS))
		{
			MoveToCenter();
			
			GAutoString FullPath = d->Project->GetFullPath();
			if (FullPath)
			{
				SetCtrlName(IDC_PATH, FullPath);
			}
			
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
						SettingItem *All = new SettingItem(i, 0, this);
						All->SetText(sAllPlatforms);
						Item->Insert(All);
					}

					if (!i->Flag.CrossPlatform)
					{
						SettingItem *Platform = new SettingItem(i, SF_PLATFORM_SPECIFC, this);
						Platform->SetText(sCurrentPlatform);
						Item->Insert(Platform);
					}
				}
			}
			
			if (GetViewById(IDC_DETAIL, Detail))
			{
				Detail->SetPriv(d);
			}
			
			GView *Search;
			if (GetViewById(IDC_SEARCH, Search))
				Search->Focus(true);
		}
	}
	
	GTreeItem *FindItem(GTreeItem *i, const char *search)
	{
		char *s = i->GetText(0);
		if (s && search && stristr(s, search))
			return i;
		
		for (GTreeItem *c = i->GetChild(); c; c = c->GetNext())
		{
			GTreeItem *f = FindItem(c, search);
			if (f)
				return f;
		}
		return NULL;
	}
	
	void OnSearch(const char *s)
	{
		if (!Tree) return;
		GTreeItem *f = NULL;
		for (GTreeItem *i = Tree->GetChild(); i && !f; i = i->GetNext())
		{
			f = FindItem(i, s);
		}
		if (f)
		{
			f->Select(true);
			for (GTreeItem *i = f; i; i = i->GetParent())
				i->Expanded(true);
			Tree->Focus(true);
		}
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDC_BROWSE:
			{
				const char *s = GetCtrlName(IDC_PATH);
				if (s)
					LgiBrowseToFile(s);
				break;
			}
			case IDC_SEARCH:
			{
				if (Flags == VK_RETURN)
				{
					DefLockOut = LgiCurrentTime();
					OnSearch(GetCtrlName(IDC_SEARCH));
				}
				else
				{
					SetCtrlEnabled(IDC_CLEAR_SEARCH, ValidStr(Ctrl->Name()));
				}
				break;
			}
			case IDC_CLEAR_SEARCH:
			{
				SetCtrlName(IDC_SEARCH, NULL);
				OnSearch(NULL);
				break;
			}
			case IDOK:
			{
				if (LgiCurrentTime() - DefLockOut < 500)
					break;
				Detail->SetSetting(NULL, 0);
				EndModal(1);
				break;
			}
			case IDCANCEL:
			{
				if (LgiCurrentTime() - DefLockOut < 500)
					break;
				EndModal(0);
				break;
			}
			default:
			{
				int Id = Ctrl->GetId();
				if (Id >= IDC_BROWSE_FILE && Id < IDC_BROWSE_FILE+100)
				{
					int i = Id - IDC_BROWSE_FILE;
					GEdit *e;
					if (GetViewById(IDC_EDIT_BASE + i, e))
					{
						GFileSelect s;
						s.Parent(this);

						GFile::Path Path(d->Project->GetBasePath());
						GFile::Path Cur(e->Name());
						Path.Join(Cur.GetFull());
						Path--;
						if (Path.Exists())
							s.InitialDir(Path);

						if (s.Open())
						{
							const char *Base = GetCtrlName(IDC_PATH);
							GAutoString Rel;
							if (Base)
							{
								GFile::Path p = Base;
								Rel = LgiMakeRelativePath(--p, s.Name());
							}
							e->Name(Rel ? Rel : s.Name());
						}
					}
					//       else LgiTrace("%s:%i - Failed to find editbox: %i\n", _FL, i);
				}
				break;
			}
		}
		
		return GDialog::OnNotify(Ctrl, Flags);
	}
	
	void OnSelect(SettingItem *si)
	{
		Detail->SetSetting(si->Setting, si->Flags);
	}
};

void SettingItem::Select(bool b)
{
	GTreeItem::Select(b);
	if (b)
		Dlg->OnSelect(this);
}

IdeProjectSettings::IdeProjectSettings(IdeProject *Proj)
{
	d = new IdeProjectSettingsPriv(this);
	d->Project = Proj;
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
	char *p;
	for (SettingInfo *i = AllSettings; i->Setting; i++)
	{
		GVariant Default;
		GXmlTag *t = NULL;
		switch (i->Setting)
		{
			default:
				break;
			case ProjMakefile:
			{
				char s[64];
				sprintf_s(s, sizeof(s), "Makefile.%s", LgiGetOsName());
				strlwr(s + 1);
				Default = s;
				break;
			}
			case ProjTargetType:
			{
				Default = "Executable";
				break;
			}
			/* This won't work in the constructor because the IdeProject isn't initialized yet.
			case ProjTargetName:
			{
				if (d->Project)
				{
					char s[256];
					char *Fn = d->Project->GetFileName();
					char *Dir = Fn ? strrchr(Fn, DIR_CHAR) : NULL;
					if (Dir)
					{
						strsafecpy(s, ++Dir, sizeof(s));
						char *Dot = strrchr(s, '.');
						#ifdef WIN32
						strcpy(Dot, ".exe");
						#else
						if (Dot) *Dot = 0;
						#endif
						Default = s;
					}
				}
				break;
			}
			*/
			case ProjEditorTabSize:
			{
				Default = 4;
				break;
			}
			case ProjEditorIndentSize:
			{
				Default = 4;
				break;
			}
			case ProjEditorShowWhiteSpace:
			{
				Default = false;
				break;
			}
			case ProjEditorUseHardTabs:
			{
				Default = true;
				break;
			}
		}
		
		if (i->Flag.ConfigSpecific)
		{
			for (int Cfg = 0; Cfg < d->Configs.Length(); Cfg++)
			{
				if (!i->Flag.PlatformSpecific)
				{
					p = d->BuildPath(i->Setting, 0, PlatformCurrent, Cfg);
					d->Active.GetChildTag(p, true);
					if (t && !t->GetContent() && Default.Type != GV_NULL)
						t->SetContent(Default.Str());
				}
				
				if (!i->Flag.CrossPlatform)
				{
					p = d->BuildPath(i->Setting, SF_PLATFORM_SPECIFC, PlatformCurrent, Cfg);
					d->Active.GetChildTag(p, true);
					if (t && !t->GetContent() && Default.Type != GV_NULL)
						t->SetContent(Default.Str());
				}
			}
		}
		else
		{
			if (!i->Flag.PlatformSpecific)
			{
				p = d->BuildPath(i->Setting, 0, PlatformCurrent, -1);
				t = d->Active.GetChildTag(p, true);
				if (t && !t->GetContent() && Default.Type != GV_NULL)
					t->SetContent(Default.Str());
			}

			if (!i->Flag.CrossPlatform)
			{
				p = d->BuildPath(i->Setting, SF_PLATFORM_SPECIFC, PlatformCurrent, -1);
				t = d->Active.GetChildTag(p, true);
				if (t && !t->GetContent() && Default.Type != GV_NULL)
					t->SetContent(Default.Str());
			}
		}
	}
}

bool IdeProjectSettings::Edit(GViewI *parent)
{
	// Copy all the settings to the edit tag...
	d->Editing.Copy(d->Active, true);
	
	// Show the dialog...
	ProjectSettingsDlg Dlg(parent, d);
	bool Changed = Dlg.DoModal();
	if (Changed)
	{
		// User elected to save settings... so copy all the values
		// back over to the active settings...
		d->Active.Copy(d->Editing, true);
		d->Editing.Empty(true);
	}
	
	return Changed;
}

bool IdeProjectSettings::Serialize(GXmlTag *Parent, bool Write)
{
	if (!Parent)
	{
		LgiAssert(!"No parent tag?");
		return false;
	}

	GXmlTag *t = Parent->GetChildTag(TagSettings, true);
	if (!t)
	{
		LgiAssert(!"Can't find settings tags?");
		return false;
	}

	if (Write)
	{
		// d->Active -> Parent
		ClearEmptyTags(&d->Active);
		return t->Copy(d->Active, true);
	}
	else
	{
		// Parent -> d->Active
		bool Status = d->Active.Copy(*t, true);
		InitAllSettings();
		return Status;
	}
	
	return false;
}

const char *IdeProjectSettings::GetStr(ProjSetting Setting, const char *Default, IdePlatform Platform)
{
	SettingInfo *s = d->Map.Find(Setting);
	LgiAssert(s);
	GArray<char*> Strs;
	int Bytes = 0;
	if (!s->Flag.PlatformSpecific)
	{
		GXmlTag *t = d->Active.GetChildTag(d->BuildPath(Setting, 0, Platform));
		if (t && t->GetContent())
		{
			Strs.Add(t->GetContent());
			Bytes += strlen(t->GetContent()) + 1;
		}
	}
	if (!s->Flag.CrossPlatform)
	{
		GXmlTag *t = d->Active.GetChildTag(d->BuildPath(Setting, SF_PLATFORM_SPECIFC, Platform));
		if (t && t->GetContent())
		{
			Strs.Add(t->GetContent());
			Bytes += strlen(t->GetContent()) + 1;
		}
	}
	
	if (Strs.Length() == 0)
		return Default;
	
	if (Strs.Length() == 1)
		return Strs[0];

	if (!d->StrBuf.Reset(new char[Bytes]))
		return Default;

	// char *c = d->StrBuf;
	int ch = 0;
	for (int i=0; i<Strs.Length(); i++)
	{
		ch += sprintf_s(d->StrBuf + ch, Bytes - ch, "%s%s", Strs[i], i<Strs.Length()-1 ? "\n" : "");
	}
	
	return d->StrBuf;
}

int IdeProjectSettings::GetInt(ProjSetting Setting, int Default, IdePlatform Platform)
{
	int Status = Default;

	SettingInfo *s = d->Map.Find(Setting);
	LgiAssert(s);
	
	if (!s->Flag.PlatformSpecific)
	{
		GXmlTag *t = d->Active.GetChildTag(d->BuildPath(Setting, 0, Platform));
		if (t)
		{
			Status = t->GetContent() ? atoi(t->GetContent()) : 0;
		}
	}
	else if (!s->Flag.CrossPlatform)
	{
		GXmlTag *t = d->Active.GetChildTag(d->BuildPath(Setting, SF_PLATFORM_SPECIFC, Platform));
		if (t)
		{
			Status = t->GetContent() ? atoi(t->GetContent()) : 0;
		}
	}
	else LgiAssert(0);

	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, const char *Value, IdePlatform Platform)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, true, Platform);
	GXmlTag *t = d->Active.GetChildTag(path, true);
	if (t)
	{
		t->SetContent(Value);
	}
	else LgiTrace("%s:%i - Warning: missing setting tag '%s'\n", _FL, path);

	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, int Value, IdePlatform Platform)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, true, Platform);
	GXmlTag *t = d->Active.GetChildTag(path, true);
	if (t)
	{
		t->SetContent(Value);
	}
	else LgiTrace("%s:%i - Warning: missing setting tag '%s'\n", _FL, path);

	return Status;
}
