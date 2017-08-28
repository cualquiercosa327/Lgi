#ifndef _IDE_PROJECT_SETTINGS_H_
#define _IDE_PROJECT_SETTINGS_H_

enum ProjSetting
{
	ProjNone,
	ProjMakefile,
	ProjExe,
	ProjArgs,
	ProjDebugAdmin,
	ProjDefines,
	ProjCompiler,
	ProjCrossCompiler,
	ProjIncludePaths,
	ProjSystemIncludes,
	ProjLibraries,
	ProjLibraryPaths,
	ProjTargetType,
	ProjTargetName,
	ProjEditorTabSize,
	ProjEditorIndentSize,
	ProjEditorShowWhiteSpace,
	ProjEditorUseHardTabs,
	ProjCommentFile,
	ProjCommentFunction,
	ProjMakefileRules,
	ProjApplicationIcon
};

class IdeProjectSettings
{
	struct IdeProjectSettingsPriv *d;

public:
	IdeProjectSettings(IdeProject *Proj);
	~IdeProjectSettings();

	void InitAllSettings(bool ClearCurrent = false);

	// Configuration
	const char *GetCurrentConfig();
	bool SetCurrentConfig(const char *Config);
	bool AddConfig(const char *Config);
	bool DeleteConfig(const char *Config);
	
	// UI
	bool Edit(GViewI *parent);

	// Serialization
	bool Serialize(GXmlTag *Parent, bool Write);

	// Accessors
	const char *GetStr(ProjSetting Setting, const char *Default = NULL, IdePlatform Platform = PlatformCurrent);
	int GetInt(ProjSetting Setting, int Default = NULL, IdePlatform Platform = PlatformCurrent);
	bool Set(ProjSetting Setting, const char *Value, IdePlatform Platform = PlatformCurrent);
	bool Set(ProjSetting Setting, int Value, IdePlatform Platform = PlatformCurrent);
};

#endif