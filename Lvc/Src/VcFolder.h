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
	typedef bool (VcFolder::*ParseFn)(GString);
	
	struct Cmd
	{
		GStringPipe Buf;
		GAutoPtr<LThread> Rd;
		ParseFn PostOp;
	};

	AppPriv *d;
	VersionCtrl Type;
	GString Path, CurrentCommit;
	GArray<VcCommit*> Log;
	GString Cache, NewRev;
	
	GArray<Cmd*> Cmds;
	bool IsLogging, IsGetCur, IsUpdate, IsFilesCmd;

	void Init(AppPriv *priv);
	const char *GetVcName();
	bool StartCmd(const char *Args, ParseFn Parser);

	bool ParseLog(GString s);
	bool ParseInfo(GString s);
	bool ParseFiles(GString s);
	bool ParseUpdate(GString s);
	
public:
	VcFolder(AppPriv *priv, const char *p);
	VcFolder(AppPriv *priv, GXmlTag *t);

	VersionCtrl GetType();
	char *GetText(int Col);
	bool Serialize(GXmlTag *t, bool Write);
	GXmlTag *Save();
	void Select(bool b);
	void ListCommit(const char *Rev);

	void OnPulse();
	void OnUpdate(const char *Rev);
	void OnMouseClick(GMouse &m);
	void OnRemove();
};

#endif