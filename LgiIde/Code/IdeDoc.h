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
	GString Name;
	GString File;
	int Line;
	
	DefnInfo()
	{
		Type = DefnNone;
		Line = 0;
	}

	DefnInfo(const DefnInfo &d)
	{
		Type = d.Type;
		Name = d.Name;
		File = d.File;
		Line = d.Line;
	}
	
	void Set(DefnType type, char *file, char16 *s, int line)
	{
		Type = type;
		File = file;
		
		while (strchr(" \t\r\n", *s)) s++;	
		Line = line;
		Name = s;
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
};

extern bool BuildDefnList(char *FileName, char16 *Cpp, GArray<DefnInfo> &Funcs, DefnType LimitTo, bool Debug = false);

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
	void GotoSearch(int CtrlId);

	// Source tools
	bool BuildIncludePaths(GArray<GString> &Paths, IdePlatform Platform, bool IncludeSysPaths);
	bool BuildHeaderList(char16 *Cpp, GArray<char*> &Headers, GArray<GString> &IncPaths);
	bool FindDefn(char16 *Def, char16 *Source, List<DefnInfo> &Matches);

	// Events
	void OnLineChange(int Line);
	void OnMarginClick(int Line);
	
	// Impl
	void OnTitleClick(GMouse &m);
	GMessage::Result OnEvent(GMessage *Msg);
	int OnNotify(GViewI *v, int f);
};

#endif
