#ifndef _VcFolder_h_
#define _VcFolder_h_

#include "GSubProcess.h"

class ReaderThread : public LThread
{
	GStream *Out;
	GSubProcess *Process;

public:
	ReaderThread(GSubProcess *p, GStream *out);
	~ReaderThread();

	int Main();
};

class VcFolder : public GTreeItem
{
	AppPriv *d;
	VersionCtrl Type;
	GString Path, CurrentCommit;
	GArray<VcCommit*> Log;
	GString Cache, NewRev;
	
	GStringPipe LogBuf, InfoBuf, UpBuf;

	GAutoPtr<LThread> ReadCurrent;
	GAutoPtr<LThread> ReadLog;
	GAutoPtr<LThread> UpdateCmd;
	
public:
	VcFolder(AppPriv *priv, const char *p);
	VcFolder(AppPriv *priv, GXmlTag *t);

	VersionCtrl GetType();
	char *GetText(int Col);
	bool Serialize(GXmlTag *t, bool Write);
	GXmlTag *Save();
	void Select(bool b);
	void ParseLog(GString s);
	void ParseInfo(GString s);
	void OnPulse();
	void OnUpdate(const char *Rev);
};

#endif