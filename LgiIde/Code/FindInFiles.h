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
	
	FindParams(const FindParams *Set = NULL)
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
		
		if (Set)
			*this = *Set;
	}
	
	FindParams &operator =(const FindParams *p)
	{
		Type = p->Type;
		
		// Make explicit copies of the GString's to ensure thread safety.
		Text = p->Text.Get();
		Ext = p->Ext.Get();
		Dir = p->Dir.Get();
		
		ProjectFiles.Length(0);
		for (unsigned i=0; i<p->ProjectFiles.Length(); i++)
			ProjectFiles.New() = p->ProjectFiles[i].Get();
		
		MatchWord = p->MatchWord;
		MatchCase = p->MatchCase;
		SubDirs = p->SubDirs;
		
		return *this;
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