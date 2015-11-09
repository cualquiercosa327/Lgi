#ifndef _LGI_IDE_H_
#define _LGI_IDE_H_

//
// Compiler specific macros:
//
//		Microsoft Visual C++: _MSC_VER
//
//		GCC: __GNUC__
//

#ifdef WIN32
#include "resource.h"
#endif
#include "resdefs.h"
#include "GDocView.h"
#include "GOptionsFile.h"
#include "FindSymbol.h"
#include "GStringClass.h"
#include "GDebugger.h"

#define LgiIdeVer				0.0

#define DEBUG_FIND_DEFN			0

enum IdeMessages
{
	M_APPEND_TEXT = M_USER+1,
	M_BUILD_ERR,
	M_BUILD_DONE,
};

#define ICON_PROJECT			0
#define ICON_DEPENDANCY			1
#define ICON_FOLDER				2
#define ICON_SOURCE				3
#define ICON_HEADER				4
#define ICON_RESOURCE			5
#define ICON_GRAPHIC			6
#define ICON_WEB				7

enum IdeIcon
{
	CMD_NEW,
	CMD_OPEN,
	CMD_SAVE,
	CMD_CUT,
	CMD_COPY,
	CMD_PASTE,
	CMD_COMPILE,
	CMD_BUILD,
	CMD_STOP_BUILD,
	CMD_EXECUTE,
	CMD_DEBUG,
	CMD_PAUSE,
	CMD_KILL,
	CMD_RESTART,
	CMD_RUN_TO,
	CMD_STEP_INTO,
	CMD_STEP_OVER,
	CMD_STEP_OUT,
	CMD_FIND_IN_FILES,
	CMD_SEARCH,
	CMD_SAVE_ALL,
};

enum IdeControls
{
	IDC_STATIC = -1,
	IDC_WATCH_LIST = 700,
	IDC_LOCALS_LIST,
	IDC_DEBUG_EDIT,
	IDC_DEBUGGER_LOG,
	IDC_BUILD_LOG,
	IDC_OUTPUT_LOG,
	IDC_FIND_LOG,
	IDC_CALL_STACK,
	IDC_DEBUG_TAB,
	IDC_OBJECT_DUMP,
	IDC_MEMORY_DUMP,
	IDC_MEMORY_TABLE,
	IDC_MEM_ADDR,
	IDC_MEM_SIZE,
	IDC_MEM_ROW_LEN,
	IDC_MEM_HEX,
	IDC_REGISTERS
};

enum IdeMenuCmds
{
	IDM_CONTINUE = 900
};

#define BUILD_TYPE_DEBUG		0
#define BUILD_TYPE_RELEASE		1

#define OPT_EditorFont			"EdFont"
#define OPT_Jobs				"Jobs"

//////////////////////////////////////////////////////////////////////
// Platform stuff
enum IdePlatform
{
	PlatformCurrent = -1,
	PlatformWin32 = 0,
	PlatformLinux,
	PlatformMac,
	PlatformHaiku,
	PlatformMax,
};
#define PLATFORM_WIN32			(1 << PlatformWin32)
#define PLATFORM_LINUX			(1 << PlatformLinux)
#define PLATFORM_MAC			(1 << PlatformMac)
#define PLATFORM_HAIKU			(1 << PlatformHaiku)
#define PLATFORM_ALL			(PLATFORM_WIN32|PLATFORM_LINUX|PLATFORM_MAC|PLATFORM_HAIKU)

extern const char *PlatformNames[];
extern const char sCurrentPlatform[];

//////////////////////////////////////////////////////////////////////
class IdeDoc;
class IdeProject;

extern char AppName[];
extern char *FindHeader(char *Short, GArray<char*> &Paths);
extern bool BuildHeaderList(char *Cpp, GArray<char*> &Headers, GArray<char*> &IncPaths, bool Recurse);

class NodeView;
class NodeSource
{
	friend class NodeView;

protected:
	NodeView *nView;

public:
	NodeSource()
	{
		nView = 0;
	}
	virtual ~NodeSource();

	virtual GAutoString GetFullPath() = 0;
	virtual bool IsWeb() = 0;
	virtual char *GetFileName() = 0;
	virtual char *GetLocalCache() = 0;
	virtual bool Load(GDocView *Edit, NodeView *Callback) = 0;
	virtual bool Save(GDocView *Edit, NodeView *Callback) = 0;
};

class NodeView
{
	friend class NodeSource;

protected:
	NodeSource *nSrc;

public:
	NodeView(NodeSource *s)
	{
		if ((nSrc = s))
		{
			nSrc->nView = this;
		}
	}

	virtual ~NodeView();

	virtual void OnDelete() = 0;
	virtual void OnSaveComplete(bool Status) = 0;
};

class AppWnd : public GWindow
{
	class AppWndPrivate *d;

	void UpdateMemoryDump();

public:
	enum Channels
	{
		BuildTab,
		OutputTab,
		FindTab,
		FtpTab,
		DebugTab,
	};

	enum DebugTabs
	{
		LocalsTab,
		ObjectTab,
		WatchTab,
		MemoryTab,
		CallStackTab,
		RegistersTab
	};

	AppWnd();
	~AppWnd();

	void SaveAll();
	void CloseAll();
	IdeDoc *OpenFile(const char *FileName, NodeSource *Src = 0);
	IdeDoc *NewDocWnd(const char *FileName, NodeSource *Src);
	IdeDoc *GetCurrentDoc();
	IdeProject *OpenProject(char *FileName, IdeProject *ParentProj, bool Create = false, bool Dep = false);
	IdeProject *RootProject();
	IdeDoc *TopDoc();
	IdeDoc *FocusDoc();
	void AppendOutput(char *Txt, int Channel);
	void UpdateState(int Debugging = -1, int Building = -1);
	void OnReceiveFiles(GArray<char*> &Files);
	int GetBuildMode();
	GTree *GetTree();
	GOptionsFile *GetOptions();
	GList *GetFtpLog();
	GStream *GetBuildLog();
	IdeDoc *FindOpenFile(char *FileName);
	IdeDoc *GotoReference(const char *File, int Line, bool CurIp, bool WithHistory = true);
	bool FindSymbol(const char *Syn, GArray<FindSymResult> &Results);
	bool GetSystemIncludePaths(GArray<char*> &Paths);
	
	// Events
	void OnLocationChange(const char *File, int Line);
	int OnCommand(int Cmd, int Event, OsView Wnd);
	void OnDocDestroy(IdeDoc *Doc);
	void OnProjectDestroy(IdeProject *Proj);
	void OnFile(char *File, bool IsProject = false);
	void OnFindFinished();
	bool OnRequestClose(bool IsClose);
	int OnNotify(GViewI *Ctrl, int Flags);
	GMessage::Result OnEvent(GMessage *m);

	// Debugging support
	class GDebugContext *GetDebugContext();
	bool ToggleBreakpoint(const char *File, int Line);
	bool OnBreakPoint(GDebugger::BreakPoint &b, bool Add);
	bool LoadBreakPoints(IdeDoc *doc);
	bool LoadBreakPoints(GDebugger *db);
	void OnDebugState(bool Debugging, bool Running);
};

#include "IdeDoc.h"
#include "IdeProject.h"
#include "FindInFiles.h"

extern void NewMemDumpViewer(AppWnd *App, char *file = 0);

class SysCharSupport : public GWindow
{
	class SysCharSupportPriv *d;

public:
	SysCharSupport(AppWnd *app);
	~SysCharSupport();

	int OnNotify(GViewI *v, int f);
	void OnPosChange();
};

#endif
