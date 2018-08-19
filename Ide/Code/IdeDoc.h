#ifndef _IDE_DOC_H_
#define _IDE_DOC_H_

#include "GMdi.h"
#include "GTextView3.h"
#include "SimpleCppParser.h"

extern void FilterFiles(GArray<ProjectNode*> &Perfect, GArray<ProjectNode*> &Nodes, GString InputStr);

class IdeDoc : public GMdiChild, public GStream
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
	void Focus(bool f) override;
	bool SetClean();
	void SetDirty();
	bool OnRequestClose(bool OsShuttingDown) override;
	void OnPosChange() override;
	void OnPaint(GSurface *pDC) override;
	bool IsFile(const char *File);
	bool AddBreakPoint(int Line, bool Add);
	
	bool OpenFile(const char *File);
	void SetEditorParams(int IndentSize, int TabSize, bool HardTabs, bool ShowWhiteSpace);
	bool HasFocus(int Set = -1);
	void ConvertWhiteSpace(bool ToTabs);
	void SetCrLf(bool CrLf);
	int GetLine();
	void SetLine(int Line, bool CurIp);
	static void ClearCurrentIp();
	bool IsCurrentIp();
	void GotoSearch(int CtrlId, char *InitialText = NULL);
	void SearchSymbol();
	void SearchFile();
	void UpdateControl();
	bool Build();

	// Source tools
	bool BuildIncludePaths(GArray<GString> &Paths, IdePlatform Platform, bool IncludeSysPaths);
	bool BuildHeaderList(char16 *Cpp, GArray<char*> &Headers, GArray<GString> &IncPaths);
	bool FindDefn(char16 *Def, char16 *Source, List<DefnInfo> &Matches);

	// Events
	void OnLineChange(int Line);
	void OnMarginClick(int Line);
	void OnProjectChange();
	
	// Impl
	void OnTitleClick(GMouse &m) override;
	GMessage::Result OnEvent(GMessage *Msg) override;
	int OnNotify(GViewI *v, int f) override;
	void OnPulse() override;
	bool SetPos(GRect &p, bool Repaint = false) override { return GView::SetPos(p, Repaint); }
	GString Read();
	ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override { return 0; }
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override;
};

#endif
