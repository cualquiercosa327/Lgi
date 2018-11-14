#ifndef _VcFolder_h_
#define _VcFolder_h_

#include "GSubProcess.h"

class VcLeaf;

enum LvcError
{
	ErrNone,
	ErrSubProcessFailed = GSUBPROCESS_ERROR,
};

class ReaderThread : public LThread
{
	GStream *Out;
	GSubProcess *Process;

public:
	ReaderThread(GSubProcess *p, GStream *out);
	~ReaderThread();

	int Main();
};

extern int Ver2Int(GString v);
extern int ToolVersion[VcMax];

class VcFolder : public GTreeItem, public GCss
{
	struct ParseParams
	{
		GString Str;
		GString AltInitPath;
		VcLeaf *Leaf;
		bool IsWorking;

		ParseParams(const char *str = NULL)
		{
			Str = str;
			Leaf = NULL;
			IsWorking = false;
		}
	};

	typedef bool (VcFolder::*ParseFn)(int, GString, ParseParams*);
	
	struct Cmd : public GStream
	{
		GStringPipe Buf;
		GStream *Log;
		GAutoPtr<LThread> Rd;
		ParseFn PostOp;
		GAutoPtr<ParseParams> Params;
		LvcError Err;

		Cmd(GStream *log)
		{
			Log = log;
			Err = ErrNone;
		}

		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0)
		{
			ssize_t Wr = Buf.Write(Ptr, Size, Flags);
			if (Log) Log->Write(Ptr, Size, Flags);
			if (Flags) Err = (LvcError) Flags;
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
	GString Path, CurrentCommit, RepoUrl, VcCmd;
	GArray<VcCommit*> Log;
	GString CurrentBranch;
	GString::Array Branches;
	GAutoPtr<UncommitedItem> Uncommit;
	GString Cache, NewRev;
	bool CommitListDirty;
	int Unpushed, Unpulled;
	GString CountCache;
	GTreeItem *Tmp;
	int CmdErrors;
	
	GArray<Cmd*> Cmds;
	bool IsLogging, IsGetCur, IsUpdate, IsFilesCmd, IsWorkingFld, IsCommit, IsUpdatingCounts;

	void Init(AppPriv *priv);
	const char *GetVcName();
	bool StartCmd(const char *Args, ParseFn Parser, ParseParams *Params = NULL, bool LogCmd = false);
	void OnBranchesChange();
	void OnCmdError(GString Output, const char *Msg);
	void OnChange(PropType Prop) { Update(); }
	VcFile *FindFile(const char *Path);

	bool ParseDiffs(GString s, GString Rev, bool IsWorking);
	
	bool ParseLog(int Result, GString s, ParseParams *Params);
	bool ParseInfo(int Result, GString s, ParseParams *Params);
	bool ParseFiles(int Result, GString s, ParseParams *Params);
	bool ParseWorking(int Result, GString s, ParseParams *Params);
	bool ParseUpdate(int Result, GString s, ParseParams *Params);
	bool ParseCommit(int Result, GString s, ParseParams *Params);
	bool ParsePush(int Result, GString s, ParseParams *Params);
	bool ParsePull(int Result, GString s, ParseParams *Params);
	bool ParseCounts(int Result, GString s, ParseParams *Params);
	bool ParseRevert(int Result, GString s, ParseParams *Params);
	bool ParseResolve(int Result, GString s, ParseParams *Params);
	bool ParseBlame(int Result, GString s, ParseParams *Params);
	bool ParseSaveAs(int Result, GString s, ParseParams *Params);
	bool ParseBranches(int Result, GString s, ParseParams *Params);
	bool ParseStatus(int Result, GString s, ParseParams *Params);
	bool ParseAddFile(int Result, GString s, ParseParams *Params);
	bool ParseVersion(int Result, GString s, ParseParams *Params);
	bool ParseClean(int Result, GString s, ParseParams *Params);
	bool ParseDiff(int Result, GString s, ParseParams *Params);
	
public:
	VcFolder(AppPriv *priv, const char *p);
	VcFolder(AppPriv *priv, GXmlTag *t);

	VersionCtrl GetType();
	AppPriv *GetPriv() { return d; }
	const char *GetPath() { return Path; }
	char *GetText(int Col);
	bool Serialize(GXmlTag *t, bool Write);
	GXmlTag *Save();
	void Select(bool b);
	void ListCommit(VcCommit *c);
	void ListWorkingFolder();
	void FolderStatus(const char *Path = NULL, VcLeaf *Notify = NULL);
	void Commit(const char *Msg, const char *Branch, bool AndPush);
	void Push();
	void Pull();
	void Clean();
	bool Revert(const char *Path, const char *Revision = NULL);
	bool Resolve(const char *Path);
	bool AddFile(const char *Path, bool AsBinary = true);
	bool Blame(const char *Path);
	bool SaveFileAs(const char *Path, const char *Revision);
	void ReadDir(GTreeItem *Parent, const char *Path);
	void SetEol(const char *Path, int Type);
	void GetVersion();
	void Diff(VcFile *file);

	void OnPulse();
	void OnUpdate(const char *Rev);
	void OnMouseClick(GMouse &m);
	void OnRemove();
	void OnExpand(bool b);
	void OnPaint(ItemPaintCtx &Ctx);
};

class VcLeaf : public GTreeItem
{
	AppPriv *d;
	VcFolder *Parent;
	bool Folder;
	GString Path, Leaf;
	GTreeItem *Tmp;

public:
	VcLeaf(VcFolder *parent, GTreeItem *Item, GString path, GString leaf, bool folder);

	GString Full();
	void OnBrowse();
	void AfterBrowse();
	void OnExpand(bool b);
	char *GetText(int Col);
	int GetImage(int Flags);
	int Compare(VcLeaf *b);
	bool Select();
	void Select(bool b);
	void OnMouseClick(GMouse &m);
};

#endif