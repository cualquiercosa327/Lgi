#ifndef _IDE_PROJECT_H_
#define _IDE_PROJECT_H_

#include "GXmlTree.h"
#include "GDragAndDrop.h"
#include "GTree.h"
#include "GOptionsFile.h"
#include "GDebugger.h"
#include "GProgressDlg.h"
#include "LList.h"

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
	static const char *DefaultExt;
	GHashTbl<const char*, bool> Exts;

	AddFilesProgress(GViewI *par);

	int64 Value();
	void Value(int64 val);
	int OnNotify(GViewI *c, int f);
};

extern IdePlatform GetCurrentPlatform();

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
	bool OnOpen(GProgressDlg *Prog, GXmlTag *Src);	
	void CollectAllSubProjects(List<IdeProject> &c);
	void CollectAllSource(GArray<GString> &c, IdePlatform Platform);
	void SortChildren();
	void InsertTag(GXmlTag *t) override;
	void RemoveTag() override;
	virtual bool IsWeb() = 0;	
	virtual int GetPlatforms() = 0;
	bool AddFiles(AddFilesProgress *Prog, const char *Path);
	IdeCommon *GetSubFolder(IdeProject *Project, char *Name, bool Create = false);
};

class WatchItem : public GTreeItem
{
	class IdeOutput *Out;
	GTreeItem *PlaceHolder;

public:
	WatchItem(IdeOutput *out, const char *Init = NULL);
	~WatchItem();
	
	bool SetText(const char *s, int i = 0) override;
	void OnExpand(bool b) override;
	bool SetValue(GVariant &v);
};

#include "IdeProjectSettings.h"
#include "GDebugContext.h"

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
	GString GetExecutable(IdePlatform Platform);
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
	bool FixMissingFiles();
	bool FindDuplicateSymbols();
	bool InProject(bool FuzzyMatch, const char *Path, bool Open, class IdeDoc **Doc = 0);
	const char *GetFileComment();
	const char *GetFunctionComment();
	bool IsMakefileUpToDate();
	bool CreateMakefile(IdePlatform Platform, bool BuildAfterwards);
	GString GetTargetName(IdePlatform Platform);
	bool GetTargetFile(char *Buf, int BufSize);
	bool BuildIncludePaths(GArray<GString> &Paths, bool Recurse, bool IncludeSystem, IdePlatform Platform);
	void ShowFileProperties(const char *File);
	bool GetExpanded(int Id);
	void SetExpanded(int Id, bool Exp);
	int AllocateId();
	
	// Nodes
	char *FindFullPath(const char *File, class ProjectNode **Node = NULL);
	bool GetAllNodes(GArray<ProjectNode*> &Nodes);
	bool HasNode(ProjectNode *Node);

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
	bool Serialize(bool Write);
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
	
	void OnCreate();
	void OnDragExit();
	int WillAccept(List<char> &Formats, GdcPt2 p, int KeyState);
	int OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState);
};

extern const char TagSettings[];
extern void FixMissingFilesDlg(IdeProject *Proj);

#endif
