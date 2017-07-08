#ifndef _IDE_PROJECT_H_
#define _IDE_PROJECT_H_

#include "GXmlTree.h"
#include "GDragAndDrop.h"
#include "GTree.h"
#include "GOptionsFile.h"
#include "GDebugger.h"
#include "GProgressDlg.h"

#define NODE_DROP_FORMAT			"Ide.ProjectNode"

#define OPT_Ftp						"ftp"
#define OPT_Www						"www"

enum ExeAction
{
	ExeRun,
	ExeDebug,
	ExeValgrind
};

enum AppCommands
{
	IDM_INSERT = 100,
	IDM_NEW_FOLDER,
	IDM_RENAME,
	IDM_DELETE,
	IDM_SETTINGS,
	IDM_INSERT_DEP,
	IDM_SORT_CHILDREN,
	IDM_PROPERTIES,
	IDM_BROWSE_FOLDER,
	IDM_OPEN_TERM,
	IDM_IMPORT_FOLDER,
	IDM_WEB_FOLDER,
	IDM_INSERT_FTP,
	IDM_BUILD_PROJECT,
	IDM_CLEAN_PROJECT,
	IDM_SHOW_IN_PROJECT
};

enum ProjectStatus
{
	OpenError,
	OpenOk,
	OpenCancel,
};

extern int PlatformCtrlId[];

class AddFilesProgress : public GDialog
{
	uint64 Ts;
	uint64 v;
	GView *Msg;

public:
	bool Cancel;

	AddFilesProgress(GViewI *par);

	int64 Value();
	void Value(int64 val);
	int OnNotify(GViewI *c, int f);
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

	IdeProject *GetProject() { return Project; }
	bool OnOpen(GProgressDlg &Prog, GXmlTag *Src);	
	void CollectAllSubProjects(List<IdeProject> &c);
	void CollectAllSource(GArray<GString> &c, IdePlatform Platform);
	void SortChildren();
	void InsertTag(GXmlTag *t);
	void RemoveTag();
	virtual bool IsWeb() = 0;	
	virtual int GetPlatforms() = 0;
	bool AddFiles(AddFilesProgress *Prog, const char *Path);
	IdeCommon *GetSubFolder(IdeProject *Project, char *Name, bool Create = false);
};

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

class WatchItem : public GTreeItem
{
	class IdeOutput *Out;
	GTreeItem *PlaceHolder;

public:
	WatchItem(IdeOutput *out, const char *Init = NULL);
	~WatchItem();
	
	bool SetText(const char *s, int i = 0);
	void OnExpand(bool b);
	bool SetValue(GVariant &v);
};

class GDebugContext : public GDebugEvents
{
	class GDebugContextPriv *d;
	
public:
	GTree *Watch;
	GList *Locals;
	GList *CallStack;
	GList *Threads;
	class GTextLog *ObjectDump;
	class GTextLog *MemoryDump;
	class GTextLog *DebuggerLog;
	class GTextLog *Registers;

	// Object
	GDebugContext(AppWnd *App, class IdeProject *Proj, const char *Exe, const char *Args, bool RunAsAdmin = false);
	virtual ~GDebugContext();

	// Impl
	bool ParseFrameReference(const char *Frame, GAutoString &File, int &Line);
	bool SetFrame(int Frame);
	bool UpdateLocals();
	bool UpdateWatches();
	bool UpdateRegisters();
	void UpdateCallStack();
	void UpdateThreads();
	bool SelectThread(int ThreadId);
	bool DumpObject(const char *Var, const char *Val);
	bool OnBreakPoint(GDebugger::BreakPoint &b, bool Add);
	
	// Ui events...
	bool OnCommand(int Cmd);
	void OnUserCommand(const char *Cmd);
	GMessage::Param OnEvent(GMessage *m);
	void OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex);
	void FormatMemoryDump(int WordSize, int Width, bool InHex);
	
	// Debugger events...
	int Write(const void *Ptr, int Size, int Flags = 0);
	void OnState(bool Debugging, bool Running);
	void OnFileLine(const char *File, int Line, bool CurrentIp);
	void OnError(int Code, const char *Str);
	void OnCrash(int Code);
};

class IdeProject : public GXmlFactory, public IdeCommon
{
	friend class ProjectNode;
	class IdeProjectPrivate *d;

	bool OnNode(const char *Path, class ProjectNode *Node, bool Add);

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
	bool RelativePath(char *Out, const char *In, bool Debug = false);
	void Build(bool All, bool Release);
	void StopBuild();
	void Clean(bool Release);
	GDebugContext *Execute(ExeAction Act = ExeRun);
	bool InProject(bool FuzzyMatch, const char *Path, bool Open, class IdeDoc **Doc = 0);
	const char *GetFileComment();
	const char *GetFunctionComment();
	bool CreateMakefile(IdePlatform Platform);
	GAutoString GetTargetName(IdePlatform Platform);
	bool GetTargetFile(char *Buf, int BufSize);
	bool BuildIncludePaths(GArray<GString> &Paths, bool Recurse, IdePlatform Platform);
	void ShowFileProperties(const char *File);
	bool GetExpanded(int Id);
	void SetExpanded(int Id, bool Exp);
	int AllocateId();
	
	// Nodes
	char *FindFullPath(const char *File, class ProjectNode **Node = NULL);
	bool GetAllNodes(GArray<ProjectNode*> &Nodes);

	// Project heirarchy
	IdeProject *GetParentProject();
	bool GetChildProjects(List<IdeProject> &c);
	void SetParentProject(IdeProject *p);

	// File
	void CreateProject();
	ProjectStatus OpenFile(char *FileName);
	bool SaveFile();
	bool SetClean();
	void SetDirty();
	void SetUserFileDirty();
	bool Serialize();
	void ImportDsp(char *File);

	// Dependency calculation
	bool GetAllDependencies(GArray<char*> &Files, IdePlatform Platform);
	bool GetDependencies(const char *SourceFile, GArray<GString> &IncPaths, GArray<char*> &Files, IdePlatform Platform);
	
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
	int OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState);
};

extern const char TagSettings[];

#endif
