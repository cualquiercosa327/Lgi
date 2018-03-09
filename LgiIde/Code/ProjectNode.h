#ifndef _PROJECT_NODE_H_
#define _PROJECT_NODE_H_

#include "FtpThread.h"

// Linked to 'TypeNames' global variable, update that too.
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
	NodeMakeFile,

	NodeMax, // Always last
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
	int NodeId;
	int Platforms;
	char *File;
	char *LocalCache;
	char *Name;
	IdeProject *Dep;
	bool IgnoreExpand;
	int64 ChildCount;
	GString Label;

	void OpenLocalCache(IdeDoc *&Doc);
	void OnCmdComplete(FtpCmd *Cmd) override;
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
	bool HasNode(ProjectNode *Node);
	
	// Props
	int GetId();
	bool IsWeb() override;
	IdeProject *GetDep();
	IdeProject *GetProject() override;
	char *GetFileName() override;
	void SetFileName(const char *f);
	char *GetName();
	void SetName(const char *f);
	NodeType GetType();
	void SetType(NodeType t);
	int GetImage(int f) override;
	char *GetText(int c) override;
	GString GetFullPath() override;
	ProjectNode *FindFile(const char *In, char **Full);
	/// \sa Some combination of PLATFORM_WIN32, PLATFORM_LINUX, PLATFORM_MAC, PLATFORM_HAIKU or PLATFORM_ALL
	int GetPlatforms() override;
	char *GetLocalCache() override;
	void AutoDetectType();
	
	// Dnd
	bool GetFormats(List<char> &Formats) override;
	bool GetData(GArray<GDragData> &Data) override;
	
	// Ui events
	bool OnBeginDrag(GMouse &m) override;
	bool OnKey(GKey &k) override;
	void OnExpand(bool b) override;
	void OnMouseClick(GMouse &m) override;
	void OnProperties();

	// Serialization
	bool Load(GDocView *Edit, NodeView *Callback) override;
	bool Save(GDocView *Edit, NodeView *Callback) override;
	bool Serialize(bool Write) override;
};

#endif
