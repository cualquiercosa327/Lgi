#ifndef _PROJECT_NODE_H_
#define _PROJECT_NODE_H_

#include "FtpThread.h"

enum NodeType
{
	NodeNone,
	NodeDir,
	NodeSrc,
	NodeHeader,
	NodeDependancy,
	NodeResources,
	NodeGraphic,
	NodeWeb,
	NodeText,
};

#define ForAllProjectNodes(Var) \
	for (ProjectNode *Var=dynamic_cast<ProjectNode*>(GetChild()); \
		Var; \
		Var=dynamic_cast<ProjectNode*>(Var->GetNext()))


extern int NodeSort(GTreeItem *a, GTreeItem *b, NativeInt d);
extern int XmlSort(GXmlTag *a, GXmlTag *b, NativeInt d);

class ProjectNode : public IdeCommon, public GDragDropSource, public FtpCallback, public NodeSource
{
	NodeType Type;
	int Platforms;
	char *File;
	char *LocalCache;
	char *Name;
	IdeProject *Dep;
	bool IgnoreExpand;
	int64 ChildCount;
	GString Label;

	void OpenLocalCache(IdeDoc *&Doc);
	void OnCmdComplete(FtpCmd *Cmd);
	int64 CountNodes();

public:
	ProjectNode(IdeProject *p);
	~ProjectNode();

	// Tree hierarchy
	ProjectNode *ChildNode();
	ProjectNode *NextNode();

	// Actions
	IdeDoc *Open();
	void Delete();
	void SetClean();
	void AddNodes(GArray<ProjectNode*> &Nodes);
	
	// Props
	bool IsWeb();
	IdeProject *GetDep();
	IdeProject *GetProject();
	char *GetFileName();
	void SetFileName(const char *f);
	char *GetName();
	void SetName(const char *f);
	NodeType GetType();
	void SetType(NodeType t);
	int GetImage(int f);
	char *GetText(int c);
	GString GetFullPath();
	ProjectNode *FindFile(const char *In, char **Full);
	/// \sa Some combination of PLATFORM_WIN32, PLATFORM_LINUX, PLATFORM_MAC, PLATFORM_HAIKU or PLATFORM_ALL
	int GetPlatforms();
	char *GetLocalCache();
	
	// Dnd
	bool GetFormats(List<char> &Formats);
	bool GetData(GVariant *Data, char *Format);
	
	// Ui events
	bool OnBeginDrag(GMouse &m);
	bool OnKey(GKey &k);
	void OnExpand(bool b);
	void OnMouseClick(GMouse &m);
	void OnProperties();

	// Serialization
	bool Load(GDocView *Edit, NodeView *Callback);
	bool Save(GDocView *Edit, NodeView *Callback);
	bool Serialize();
};

#endif