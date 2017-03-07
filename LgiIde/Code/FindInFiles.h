#ifndef _FIND_IN_FILES_H_
#define _FIND_IN_FILES_H_

#include "GHistory.h"

enum FifSearchType
{
	FifSearchSolution,
	FifSearchDirectory,
};

class FindParams
{
public:
	FifSearchType Type;
	GString Text;
	GString Ext;
	GString Dir;
	bool MatchWord;
	bool MatchCase;
	bool SubDirs;
	GArray<GString> ProjectFiles;
	
	FindParams()
	{
		Type = FifSearchDirectory;

		Ext = "*.c* *.h *.java";

		char Exe[MAX_PATH];
		LgiGetExePath(Exe, sizeof(Exe));
		LgiTrimDir(Exe);
		Dir = Exe;
		
		MatchWord = false;
		MatchCase = false;
		SubDirs = true;
	}
};

class FindInFiles : public GDialog
{
	AppWnd *App;
	GHistory *TypeHistory;
	GHistory *FolderHistory;
	bool OwnParams;

public:
	FindParams *Params;
	
	FindInFiles(AppWnd *app, FindParams *params = NULL);
	~FindInFiles();
	
	int OnNotify(GViewI *v, int f);
	void OnCreate();
};

class FindInFilesThread : public GThread
{
	class FindInFilesThreadPrivate *d;

	void SearchFile(char *File);

public:
	FindInFilesThread(AppWnd *app, FindParams *Params);
	~FindInFilesThread();
	
	int Main();
	void Stop();
};

#endif