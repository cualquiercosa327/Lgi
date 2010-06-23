#ifndef _FIND_IN_FILES_H_
#define _FIND_IN_FILES_H_

#include "GHistory.h"

class FindParams
{
public:
	char *Text;
	char *Ext;
	char *Dir;
	bool MatchWord;
	bool MatchCase;
	bool SubDirs;
	
	FindParams()
	{
		Text = 0;

		Ext = NewStr("*.c* *.h *.java");

		char Exe[256];
		LgiGetExePath(Exe, sizeof(Exe));
		LgiTrimDir(Exe);
		Dir = NewStr(Exe);
		
		MatchWord = false;
		MatchCase = false;
		SubDirs = true;
	}
	
	~FindParams()
	{
		DeleteArray(Text);
		DeleteArray(Ext);
		DeleteArray(Dir);
	}
};

class FindInFiles : public GDialog
{
	AppWnd *App;
	GHistory *TypeHistory;
	GHistory *FolderHistory;

public:
	FindParams *Params;
	
	FindInFiles(AppWnd *app);
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