#ifndef _IDE_PROJECT_H_
#define _IDE_PROJECT_H_

#include "GXmlTree.h"
#include "GDragAndDrop.h"
#include "GTree.h"

#define NODE_DROP_FORMAT			"Ide.ProjectNode"

#define TARGET_TYPE_EXE				0
#define TARGET_TYPE_SHARED_LIB		1
#define TARGET_TYPE_STATIC_LIB		2

#define OPT_Ftp						"ftp"
#define OPT_Www						"www"

enum ExeAction
{
	ExeRun,
	ExeDebug,
	ExeValgrind
};

class IdeCommon : public GTreeItem, public GXmlTag
{
	friend class IdeProject;

public:
	~IdeCommon();
	
	void CollectAllSubProjects(List<IdeProject> &c);
	void CollectAllSource(GArray<char*> &c);
	void SortChildren();
	void InsertTag(GXmlTag *t);
	void RemoveTag();
	virtual bool IsWeb() = 0;	
	IdeCommon *GetSubFolder(IdeProject *Project, char *Name, bool Create = false);
};

class IdeProject : public GXmlFactory, public IdeCommon
{
	friend class ProjectNode;
	class IdeProjectPrivate *d;

public:
	IdeProject(AppWnd *App);
	~IdeProject();

	bool IsWeb() { return false; }
	char *GetFileName();
	char *GetExecutable();
	char *GetExeArgs();
	char *GetIncludePaths();
	void CreateProject();
	bool OpenFile(char *FileName);
	bool SaveFile(char *FileName = 0);
	void SetClean();
	void SetDirty();
	char *GetText(int Col);
	int GetImage(int Flags);
	GXmlTag *Create(char *Tag);
	void Empty();
	void OnMouseClick(GMouse &m);
	AppWnd *GetApp();
	void ImportDsp(char *File);
	bool GetBasePath(char *Path);
	bool GetMakefile(char *Path, int Len);
	bool GetExePath(char *Path, int Len);
	bool RelativePath(char *Out, char *In);
	bool Serialize();
	void Build(bool All);
	void Clean();
	void Execute(ExeAction Act = ExeRun);
	IdeProject *GetParentProject();
	bool GetChildProjects(List<IdeProject> &c);
	void SetParentProject(IdeProject *p);
	char *FindFullPath(char *File);
	bool InProject(char *FullPath, bool Open, IdeDoc **Doc = 0);
	char *GetFileComment();
	char *GetFunctionComment();
	bool CreateMakefile();
	bool GetTargetName(char *Buf, int BufSize);
	bool GetTargetFile(char *Buf, int BufSize);
	bool BuildIncludePaths(List<char> &Paths, bool Recurse);
	class ProjectSettings *GetSettings();
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

#endif
