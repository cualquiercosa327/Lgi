#ifndef _IDE_DOC_H_
#define _IDE_DOC_H_

#include "GMdi.h"
#include "GTextView3.h"

enum DefnType
{
	DefnNone,
	DefnDefine,
	DefnFunc,
	DefnClass,
	DefnEnum,
	DefnEnumValue,
	DefnTypedef,
};

class DefnInfo
{
public:
	DefnType Type;
	char *Name;
	char *File;
	int Line;
	
	DefnInfo(DefnType type, char *file, char16 *s, int line)
	{
		Type = type;
		File = NewStr(file);
		
		while (strchr(" \t\r\n", *s)) s++;	
		Line = line;
		Name = LgiNewUtf16To8(s);
		if (Name && Type == DefnFunc)
		{
			if (strlen(Name) > 42)
			{
				char *b = strchr(Name, '(');
				if (b)
				{
					if (strlen(b) > 5)
					{
						strcpy(b, "(...)");
					}
					else
					{
						*b = 0;
					}
				}
			}
			
			char *t;
			while ((t = strchr(Name, '\t')))
			{
				*t = ' ';
			}
		}		
	}
	
	~DefnInfo()
	{
		DeleteArray(Name);
		DeleteArray(File);
	}
};

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
	int OnNotify(GViewI *v, int f);
	GTextView3 *GetEdit();
	void OnPosChange();
	void OnPaint(GSurface *pDC);
	bool IsFile(const char *File);
	bool AddBreakPoint(int Line, bool Add);
	
	void SetLine(int Line, bool CurIp);
	static void ClearCurrentIp();
	bool IsCurrentIp();
	void GotoSearch();

	// Source tools
	bool BuildIncludePaths(GArray<char*> &Paths, IdePlatform Platform, bool IncludeSysPaths);
	bool BuildHeaderList(char16 *Cpp, GArray<char*> &Headers, GArray<char*> &IncPaths);
	bool BuildDefnList(char *FileName, char16 *Cpp, List<DefnInfo> &Funcs, DefnType LimitTo, bool Debug = false);
	bool FindDefn(char16 *Def, char16 *Source, List<DefnInfo> &Matches);

	// Events
	void OnLineChange(int Line);
	void OnMarginClick(int Line);
	
	// Impl
	void OnTitleClick(GMouse &m);
};

#endif
