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

	void OpenLocalCache(IdeDoc *&Doc);
	void OnCmdComplete(FtpCmd *Cmd);

public:
	ProjectNode(IdeProject *p);
	~ProjectNode();

	int GetPlatforms();
	char *GetLocalCache();
	bool Load(GDocView *Edit, NodeView *Callback);
	bool Save(GDocView *Edit, NodeView *Callback);
	bool IsWeb();
	ProjectNode *ChildNode();
	ProjectNode *NextNode();
	void AddNodes(GArray<ProjectNode*> &Nodes);
	void SetClean();
	IdeProject *GetDep();
	IdeProject *GetProject();
	bool GetFormats(List<char> &Formats);
	bool GetData(GVariant *Data, char *Format);
	bool OnBeginDrag(GMouse &m);
	char *GetFileName();
	void SetFileName(const char *f);
	char *GetName();
	void SetName(const char *f);
	NodeType GetType();
	void SetType(NodeType t);
	int GetImage(int f);
	char *GetText(int c);
	void OnExpand(bool b);
	bool Serialize();
	GString GetFullPath();
	IdeDoc *Open();
	void Delete();
	bool OnKey(GKey &k);
	void OnMouseClick(GMouse &m);
	ProjectNode *FindFile(const char *In, char **Full);
	void OnProperties();
};

#endif