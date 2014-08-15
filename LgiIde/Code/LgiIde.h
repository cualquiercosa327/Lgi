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

#define LgiIdeVer				0.0

#define M_APPEND_TEXT			(M_USER+1)
#define M_BUILD_ERR				(M_USER+2)

#define ICON_PROJECT			0
#define ICON_DEPENDANCY			1
#define ICON_FOLDER				2
#define ICON_SOURCE				3
#define ICON_HEADER				4
#define ICON_RESOURCE			5
#define ICON_GRAPHIC			6
#define ICON_WEB				7

#define CMD_NEW					0
#define CMD_OPEN				1
#define CMD_SAVE				2
#define CMD_CUT					3
#define CMD_COPY				4
#define CMD_PASTE				5
#define CMD_COMPILE				6
#define CMD_BUILD				7
#define CMD_STOP_BUILD			8
#define CMD_EXECUTE				9
#define CMD_DEBUG				10
#define CMD_BREAKPOINT			11
#define CMD_RESTART				12
#define CMD_KILL				13
#define CMD_STEP_INTO			14
#define CMD_STEP_OVER			15
#define CMD_STEP_OUT			16
#define CMD_RUN_TO				17
#define CMD_FIND_IN_FILES		18
#define CMD_SEARCH				19
#define CMD_SAVE_ALL			20

#define BUILD_TYPE_DEBUG		0
#define BUILD_TYPE_RELEASE		1

#define OPT_EditorFont			"EdFont"
#define OPT_Jobs				"Jobs"

#define DEBUG_FIND_DEFN			1

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
		if (nSrc = s)
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

public:
	AppWnd();
	~AppWnd();

	void SaveAll();
	void CloseAll();
	IdeDoc *OpenFile(char *FileName, NodeSource *Src = 0);
	IdeDoc *NewDocWnd(char *FileName, NodeSource *Src);
	IdeProject *OpenProject(char *FileName, IdeProject *ParentProj, bool Create = false, bool Dep = false);
	IdeProject *RootProject();
	int OnCommand(int Cmd, int Event, OsView Wnd);
	void OnDocDestroy(IdeDoc *Doc);
	void OnProjectDestroy(IdeProject *Proj);
	void OnFile(char *File, bool IsProject = false);
	IdeDoc *TopDoc();
	IdeDoc *FocusDoc();
	bool OnRequestClose(bool IsClose);
	void AppendOutput(char *Txt, int Channel);
	void UpdateState(int Debugging = -1, int Building = -1);
	void OnFindFinished();
	GMessage::Result OnEvent(GMessage *m);
	void OnReceiveFiles(GArray<char*> &Files);
	int GetBuildMode();
	GTree *GetTree();
	GOptionsFile *GetOptions();
	GList *GetFtpLog();
	IdeDoc *FindOpenFile(char *FileName);
	IdeDoc *GotoReference(char *File, int Line);
	bool FindSymbol(const char *Syn, GArray<FindSymResult> &Results);
	bool GetSystemIncludePaths(GArray<char*> &Paths);
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
