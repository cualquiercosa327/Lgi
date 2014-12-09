#ifndef _IDE_PROJECT_H_
#define _IDE_PROJECT_H_

#include "GXmlTree.h"
#include "GDragAndDrop.h"
#include "GTree.h"
#include "GOptionsFile.h"
#include "GDebugger.h"

#define NODE_DROP_FORMAT			"Ide.ProjectNode"

#define OPT_Ftp						"ftp"
#define OPT_Www						"www"

enum ExeAction
{
	ExeRun,
	ExeDebug,
	ExeValgrind
};

class AppWnd;
class IdeProject;
class IdeCommon : public GTreeItem, public GXmlTag
{
	friend class IdeProject;

protected:
	IdeProject *Project;

public:
	IdeCommon(IdeProject *p);
	~IdeCommon();

	void OnOpen(GXmlTag *Src);	
	void CollectAllSubProjects(List<IdeProject> &c);
	void CollectAllSource(GArray<char*> &c, IdePlatform Platform);
	void SortChildren();
	void InsertTag(GXmlTag *t);
	void RemoveTag();
	virtual bool IsWeb() = 0;	
	virtual int GetPlatforms() = 0;
	IdeCommon *GetSubFolder(IdeProject *Project, char *Name, bool Create = false);
};

enum ProjSetting
{
	ProjNone,
	ProjMakefile,
	ProjExe,
	ProjArgs,
	ProjDefines,
	ProjCompiler,
	ProjIncludePaths,
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
	ProjMakefileRules
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

class GDebugContext : public GDebugEvents
{
	class GDebugContextPriv *d;
	
public:
	GList *Watch;
	GList *Locals;
	GList *CallStack;
	class GTextLog *ObjectDump;
	class GTextLog *MemoryDump;
	class GTextLog *DebuggerLog;
	class GTextLog *Registers;

	// Object
	GDebugContext(AppWnd *App, class IdeProject *Proj, const char *Exe, const char *Args);
	virtual ~GDebugContext();

	// Impl
	bool ParseFrameReference(const char *Frame, GAutoString &File, int &Line);
	bool SetFrame(int Frame);
	bool UpdateLocals();
	bool UpdateRegisters();
	bool DumpObject(const char *Var);
	
	// Ui events...
	bool OnCommand(int Cmd);
	void OnUserCommand(const char *Cmd);
	GMessage::Param OnEvent(GMessage *m);
	void OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex);
	void FormatMemoryDump(int WordSize, int Width, bool InHex);
	
	// Debugger events...
	int Write(const void *Ptr, int Size, int Flags = 0);
	void OnChildLoaded(bool Loaded);
	void OnRunState(bool Running);
	void OnFileLine(const char *File, int Line);
	void OnError(int Code, const char *Str);
	void OnCrash(int Code);
};

class IdeProject : public GXmlFactory, public IdeCommon
{
	friend class ProjectNode;
	class IdeProjectPrivate *d;

public:
	IdeProject(AppWnd *App);
	~IdeProject();

	bool IsWeb() { return false; }
	int GetPlatforms();

	char *GetFileName(); // Can be a relative path
	GAutoString GetFullPath(); // Always a complete path
	GAutoString GetBasePath(); // A non-relative path to the folder containing the project

	AppWnd *GetApp();
	const char *GetExecutable();
	const char *GetExeArgs();
	const char *GetIncludePaths();
	const char *GetPreDefinedValues();

	GXmlTag *Create(char *Tag);
	void Empty();
	GAutoString GetMakefile();
	bool GetExePath(char *Path, int Len);
	bool RelativePath(char *Out, char *In, bool Debug = false);
	void Build(bool All);
	void Clean();
	GDebugContext *Execute(ExeAction Act = ExeRun);
	char *FindFullPath(const char *File, class ProjectNode **Node = NULL);
	bool InProject(const char *FullPath, bool Open, class IdeDoc **Doc = 0);
	const char *GetFileComment();
	const char *GetFunctionComment();
	bool CreateMakefile(IdePlatform Platform);
	GAutoString GetTargetName(IdePlatform Platform);
	bool GetTargetFile(char *Buf, int BufSize);
	bool BuildIncludePaths(GArray<char*> &Paths, bool Recurse, IdePlatform Platform);
	void ShowFileProperties(const char *File);

	// Project heirarchy
	IdeProject *GetParentProject();
	bool GetChildProjects(List<IdeProject> &c);
	void SetParentProject(IdeProject *p);

	// File
	void CreateProject();
	bool OpenFile(char *FileName);
	bool SaveFile(char *FileName = 0);
	void SetClean();
	void SetDirty();
	bool Serialize();
	void ImportDsp(char *File);

	// Dependency calculation
	bool GetAllDependencies(GArray<char*> &Files, IdePlatform Platform);
	bool GetDependencies(const char *SourceFile, GArray<char*> &IncPaths, GArray<char*> &Files, IdePlatform Platform);
	
	// Settings
	IdeProjectSettings *GetSettings();

	// Impl
	char *GetText(int Col);
	int GetImage(int Flags);
	void OnMouseClick(GMouse &m);
};

class IdeTree : public GTree, public GDragDropTarget
{
	GTreeItem *Hit;

public:
	IdeTree();
	
	void OnAttach();
	void OnDragExit();
	int WillAccept(List<char> &Formats, GdcPt2 p, int KeyState);
	int OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState);
};

extern const char TagSettings[];

#endif
