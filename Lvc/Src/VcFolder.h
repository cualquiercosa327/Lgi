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
	typedef bool (VcFolder::*ParseFn)(int, GString);
	
	struct Cmd : public GStream
	{
		GStringPipe Buf;
		GStream *Log;
		GAutoPtr<LThread> Rd;
		ParseFn PostOp;

		Cmd(GStream *log)
		{
			Log = log;
		}

		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0)
		{
			ssize_t Wr = Buf.Write(Ptr, Size, Flags);
			if (Log) Log->Write(Ptr, Size, Flags);
			return Wr;
		}
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
	bool CommitListDirty;
	int Unpushed, Unpulled;
	GString CountCache;
	
	GArray<Cmd*> Cmds;
	bool IsLogging, IsGetCur, IsUpdate, IsFilesCmd, IsWorkingFld, IsCommit, IsUpdatingCounts;

	void Init(AppPriv *priv);
	const char *GetVcName();
	bool StartCmd(const char *Args, ParseFn Parser, bool LogCmd = false);

	bool ParseDiffs(GString s, bool IsWorking);
	bool ParseLog(int Result, GString s);
	bool ParseInfo(int Result, GString s);
	bool ParseFiles(int Result, GString s);
	bool ParseWorking(int Result, GString s);
	bool ParseUpdate(int Result, GString s);
	bool ParseCommit(int Result, GString s);
	bool ParsePush(int Result, GString s);
	bool ParsePull(int Result, GString s);
	bool ParseCounts(int Result, GString s);
	
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