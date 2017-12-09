#ifndef _IDE_DOC_H_
#define _IDE_DOC_H_

#include "GMdi.h"
#include "GTextView3.h"
#include "SimpleCppParser.h"

extern void FilterFiles(GArray<ProjectNode*> &Perfect, GArray<ProjectNode*> &Nodes, GString InputStr);

class IdeDoc : public GMdiChild
{
	friend class DocEdit;
	class IdeDocPrivate *d;

	static GString CurIpDoc;
	static int CurIpLine;

public:
	IdeDoc(class AppWnd *a, NodeSource *src, const char *file);
	~IdeDoc();

	AppWnd *GetApp();

	void SetProject(IdeProject *p);	
	IdeProject *GetProject();
	char *GetFileName();
	void SetFileName(const char *f, bool Write);
	void Focus(bool f);
	bool SetClean();
	void SetDirty();
	bool OnRequestClose(bool OsShuttingDown);
	GTextView3 *GetEdit();
	void OnPosChange();
	void OnPaint(GSurface *pDC);
	bool IsFile(const char *File);
	bool AddBreakPoint(int Line, bool Add);
	
	void SetLine(int Line, bool CurIp);
	static void ClearCurrentIp();
	bool IsCurrentIp();
	void GotoSearch(int CtrlId, char *InitialText = NULL);
	void SearchSymbol();
	void SearchFile();

	// Source tools
	bool BuildIncludePaths(GArray<GString> &Paths, IdePlatform Platform, bool IncludeSysPaths);
	bool BuildHeaderList(char16 *Cpp, GArray<char*> &Headers, GArray<GString> &IncPaths);
	bool FindDefn(char16 *Def, char16 *Source, List<DefnInfo> &Matches);

	// Events
	void OnLineChange(int Line);
	void OnMarginClick(int Line);
	void OnProjectChange();
	
	// Impl
	void OnTitleClick(GMouse &m);
	GMessage::Result OnEvent(GMessage *Msg);
	int OnNotify(GViewI *v, int f);
	void OnPulse();
};

#endif
