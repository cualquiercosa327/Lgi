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

	class UncommitedItem : public LListItem
	{
		AppPriv *d;

	public:
		UncommitedItem(AppPriv *priv)
		{
			d = priv;
		}

		void OnPaint(GItem::ItemPaintCtx &Ctx);
		void Select(bool b);
	};

	AppPriv *d;
	VersionCtrl Type;
	GString Path, CurrentCommit;
	GArray<VcCommit*> Log;
	GAutoPtr<UncommitedItem> Uncommit;
	GString Cache, NewRev;
	
	GArray<Cmd*> Cmds;
	bool IsLogging, IsGetCur, IsUpdate, IsFilesCmd, IsWorkingFld, IsCommit;

	void Init(AppPriv *priv);
	const char *GetVcName();
	bool StartCmd(const char *Args, ParseFn Parser, bool Debug = false);

	bool ParseDiffs(GString s, bool IsWorking);
	bool ParseLog(GString s);
	bool ParseInfo(GString s);
	bool ParseFiles(GString s);
	bool ParseWorking(GString s);
	bool ParseUpdate(GString s);
	bool ParseCommit(GString s);
	bool ParsePush(GString s);
	bool ParsePull(GString s);
	
public:
	VcFolder(AppPriv *priv, const char *p);
	VcFolder(AppPriv *priv, GXmlTag *t);

	VersionCtrl GetType();
	char *GetText(int Col);
	bool Serialize(GXmlTag *t, bool Write);
	GXmlTag *Save();
	void Select(bool b);
	void ListCommit(const char *Rev);
	void ListWorkingFolder();
	void Commit(const char *Msg);
	void Push();
	void Pull();

	void OnPulse();
	void OnUpdate(const char *Rev);
	void OnMouseClick(GMouse &m);
	void OnRemove();
};

#endif