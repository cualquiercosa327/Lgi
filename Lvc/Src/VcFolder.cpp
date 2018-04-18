#include "Lvc.h"
#include "../Resources/resdefs.h"

#ifndef CALL_MEMBER_FN
#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))
#endif

ReaderThread::ReaderThread(GSubProcess *p, GStream *out) : LThread("ReaderThread")
{
	Process = p;
	Out = out;
	Run();
}

ReaderThread::~ReaderThread()
{
	Out = NULL;
	while (!IsExited())
		LgiSleep(1);
}

int ReaderThread::Main()
{
	Process->Start(true, false);

	while (Process->IsRunning())
	{
		if (Out)
		{
			char Buf[1024];
			ssize_t r = Process->Read(Buf, sizeof(Buf));
			if (r > 0)
				Out->Write(Buf, r);
		}
		else
		{
			Process->Kill();
			return -1;
			break;
		}
	}

	if (Out)
	{
		char Buf[1024];
		ssize_t r = Process->Read(Buf, sizeof(Buf));
		if (r > 0)
			Out->Write(Buf, r);
	}

	return (int) Process->GetExitValue();
}

/////////////////////////////////////////////////////////////////////////////////////////////
void VcFolder::Init(AppPriv *priv)
{
	d = priv;
	
	IsCommit = false;
	IsLogging = false;
	IsGetCur = false;
	IsUpdate = false;
	IsFilesCmd = false;
	IsWorkingFld = false;
	CommitListDirty = false;
	IsUpdatingCounts = false;

	Unpushed = Unpulled = -1;
	Type = VcNone;

	Expanded(false);
	Insert(Tmp = new GTreeItem);
	Tmp->SetText("Loading...");

	LgiAssert(d != NULL);
}

VcFolder::VcFolder(AppPriv *priv, const char *p)
{
	Init(priv);
	Path = p;
}

VcFolder::VcFolder(AppPriv *priv, GXmlTag *t)
{
	Init(priv);
	Serialize(t, false);
}

VersionCtrl VcFolder::GetType()
{
	if (Type == VcNone)
		Type = DetectVcs(Path);
	return Type;
}

char *VcFolder::GetText(int Col)
{
	switch (Col)
	{
		case 0:
		{
			if (Cmds.Length())
			{
				Cache = Path;
				Cache += " (...)";
				return Cache;
			}
			return Path;
		}
		case 1:
		{
			CountCache.Printf("%i/%i", Unpulled, Unpushed);
			CountCache = CountCache.Replace("-1", "--");
			return CountCache;
		}
	}

	return NULL;
}

bool VcFolder::Serialize(GXmlTag *t, bool Write)
{
	if (Write)
		t->SetContent(Path);
	else
		Path = t->GetContent();
	return true;
}

GXmlTag *VcFolder::Save()
{
	GXmlTag *t = new GXmlTag(OPT_Folder);
	if (t)
		Serialize(t, true);
	return t;
}

const char *VcFolder::GetVcName()
{
	switch (GetType())
	{
		case VcGit:
			return "git";
		case VcSvn:
			return "svn";
		case VcHg:
			return "hg";
		case VcCvs:
			return "cvs";
		default:
			LgiAssert(!"Impl me.");
			break;
	}				

	return NULL;
}

bool VcFolder::StartCmd(const char *Args, ParseFn Parser, GString Param, bool LogCmd)
{
	const char *Exe = GetVcName();
	if (!Exe)
		return false;

	if (d->Log && LogCmd)
		d->Log->Print("%s %s\n", Exe, Args);

	GAutoPtr<GSubProcess> Process(new GSubProcess(Exe, Args));
	if (!Process)
		return false;

	Process->SetInitFolder(Path);

	GAutoPtr<Cmd> c(new Cmd(LogCmd ? d->Log : NULL));
	if (!c)
		return false;

	c->PostOp = Parser;
	c->Param = Param;
	c->Rd.Reset(new ReaderThread(Process.Release(), c));
	Cmds.Add(c.Release());

	Update();

	LgiTrace("Cmd: %s %s\n", Exe, Args);

	return true;
}

int LogDateCmp(LListItem *a, LListItem *b, NativeInt Data)
{
	VcCommit *A = dynamic_cast<VcCommit*>(a);
	VcCommit *B = dynamic_cast<VcCommit*>(b);

	if ((A != NULL) ^ (B != NULL))
	{
		// This handles keeping the "working folder" list item at the top
		return (A != NULL) - (B != NULL);
	}

	// Sort the by date from most recent to least
	return -A->GetTs().Compare(&B->GetTs());
}

bool VcFolder::ParseBranches(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		{
			GString::Array a = s.SplitDelimit("\r\n");
			for (GString *l = NULL; a.Iterate(l); )
			{
				GString n = l->Strip();
				if (n(0) == '*')
				{
					GString::Array c = n.SplitDelimit(" \t", 1);
					if (c.Length() > 1)
						Branches.New() = CurrentBranch = c[1];
					else
						Branches.New() = n;
				}
				else Branches.New() = n;
			}
			break;
		}
		case VcHg:
		{
			Branches = s.SplitDelimit("\r\n");
			break;
		}
		default:
		{
			break;
		}
	}

	OnBranchesChange();
	return false;
}

void VcFolder::OnBranchesChange()
{
	GWindow *w = d->Tree->GetWindow();
	if (!w)
		return;

	DropDownBtn *dd;
	if (w->GetViewById(IDC_BRANCH_DROPDOWN, dd))
	{
		dd->SetList(IDC_BRANCH, Branches);
	}

	if (Branches.Length() > 0)
	{
		GViewI *b;
		if (w->GetViewById(IDC_BRANCH, b))
		{
			if (!ValidStr(b->Name()))
				b->Name(Branches.First());
		}
	}
}

void VcFolder::Select(bool b)
{
	GTreeItem::Select(b);
	
	if (b)
	{
		if ((Log.Length() == 0 || CommitListDirty) && !IsLogging)
		{
			CommitListDirty = false;
			IsLogging = StartCmd("log", &VcFolder::ParseLog);
		}

		if (Branches.Length() == 0)
		{
			switch (GetType())
			{
				case VcGit:
					StartCmd("branch -a", &VcFolder::ParseBranches);
					break;
				case VcSvn:
					Branches.New() = "trunk";
					OnBranchesChange();
					break;
				case VcHg:
					StartCmd("branch", &VcFolder::ParseBranches);
					break;
				case VcCvs:
					break;
				default:
					LgiAssert(!"Impl me.");
					break;
			}				
		}

		/*
		if (!IsUpdatingCounts && Unpushed < 0)
		{			
			switch (GetType())
			{
				case VcGit:
					IsUpdatingCounts = StartCmd("cherry -v", &VcFolder::ParseCounts);
					break;
				case VcSvn:
					IsUpdatingCounts = StartCmd("status -u", &VcFolder::ParseCounts);
					break;
				default:
					LgiAssert(!"Impl me.");
					break;
			}
		}
		*/

		char *Ctrl = d->Lst->GetWindow()->GetCtrlName(IDC_FILTER);
		GString Filter = ValidStr(Ctrl) ? Ctrl : NULL;

		if (d->CurFolder != this)
		{
			d->CurFolder = this;
			d->Lst->RemoveAll();
		}

		if (!Uncommit)
			Uncommit.Reset(new UncommitedItem(d));
		d->Lst->Insert(Uncommit, 0);

		int64 CurRev = Atoi(CurrentCommit.Get());
		List<LListItem> Ls;
		for (unsigned i=0; i<Log.Length(); i++)
		{
			if (CurrentCommit &&
				Log[i]->GetRev())
			{
				switch (GetType())
				{
					case VcSvn:
					{
						int64 LogRev = Atoi(Log[i]->GetRev());
						if (CurRev >= 0 && CurRev >= LogRev)
						{
							CurRev = -1;
							Log[i]->SetCurrent(true);
						}
						else
						{
							Log[i]->SetCurrent(false);
						}
						break;
					}
					default:
						Log[i]->SetCurrent(!_stricmp(CurrentCommit, Log[i]->GetRev()));
						break;
				}
			}

			bool Add = !Filter;
			if (Filter)
			{
				const char *s = Log[i]->GetRev();
				if (s && strstr(s, Filter) != NULL)
					Add = true;

				s = Log[i]->GetAuthor();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
				s = Log[i]->GetMsg();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
			}

			LList *CurOwner = Log[i]->GetList();
			if (Add ^ (CurOwner != NULL))
			{
				if (Add)
					Ls.Insert(Log[i]);
				else
					d->Lst->Remove(Log[i]);
			}
		}

		d->Lst->Insert(Ls);
		d->Lst->Sort(LogDateCmp);

		if (GetType() == VcGit)
		{
			d->Lst->ColumnAt(0)->Width(40);
			d->Lst->ColumnAt(1)->Width(270);
			d->Lst->ColumnAt(2)->Width(240);
			d->Lst->ColumnAt(3)->Width(130);
			d->Lst->ColumnAt(4)->Width(400);
		}
		else d->Lst->ResizeColumnsToContent();
		d->Lst->UpdateAllItems();

		if (!CurrentCommit && !IsGetCur)
		{
			switch (GetType())
			{
				case VcGit:
					IsGetCur = StartCmd("rev-parse HEAD", &VcFolder::ParseInfo);
					break;
				case VcSvn:
					IsGetCur = StartCmd("info", &VcFolder::ParseInfo);
					break;
				case VcHg:
					IsGetCur = StartCmd("id -i", &VcFolder::ParseInfo);
					break;
				case VcCvs:
					break;
				default:
					LgiAssert(!"Impl me.");
					break;
			}				
		}
	}
}

int CommitRevCmp(VcCommit **a, VcCommit **b)
{
	int64 arev = Atoi((*a)->GetRev());
	int64 brev = Atoi((*b)->GetRev());
	int64 diff = (int64)brev - arev;
	if (diff < 0) return -1;
	return (diff > 0) ? 1 : 0;
}

int CommitDateCmp(VcCommit **a, VcCommit **b)
{
	uint64 ats, bts;
	(*a)->GetTs().Get(ats);
	(*b)->GetTs().Get(bts);
	int64 diff = (int64)bts - ats;
	if (diff < 0) return -1;
	return (diff > 0) ? 1 : 0;
}

bool VcFolder::ParseLog(int Result, GString s, ParseParams *Params)
{
	GHashTbl<char*, VcCommit*> Map;
	for (VcCommit **pc = NULL; Log.Iterate(pc); )
		Map.Add((*pc)->GetRev(), *pc);

	int Skipped = 0, Errors = 0;
	switch (GetType())
	{
		case VcGit:
		{
			GString::Array c;
			c.SetFixedLength(false);
			char *prev = s.Get();

			for (char *i = s.Get(); *i; )
			{
				if (!strnicmp(i, "commit ", 7))
				{
					if (i > prev)
					{
						c.New().Set(prev, i - prev);
						// LgiTrace("commit=%i\n", (int)(i - prev));
					}
					prev = i;
				}

				while (*i)
				{
					if (*i++ == '\n')
						break;
				}
			}

			for (unsigned i=0; i<c.Length(); i++)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d));
				if (Rev->GitParse(c[i]))
				{
					if (!Map.Find(Rev->GetRev()))
						Log.Add(Rev.Release());
					else
						Skipped++;
				}
				else
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, c[i].Get());
					Errors++;
				}
			}
			Log.Sort(CommitDateCmp);
			break;
		}
		case VcSvn:
		{
			GString::Array c = s.Split("------------------------------------------------------------------------");
			for (unsigned i=0; i<c.Length(); i++)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d));
				GString Raw = c[i].Strip();
				if (Rev->SvnParse(Raw))
				{
					if (!Map.Find(Rev->GetRev()))
						Log.Add(Rev.Release());
				}
				else if (Raw)
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, Raw.Get());
					Errors++;
				}
			}
			Log.Sort(CommitRevCmp);
			break;
		}
		case VcHg:
		{
			GString::Array c = s.Split("\n\n");
			for (GString *Commit = NULL; c.Iterate(Commit); )
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d));
				if (Rev->HgParse(*Commit))
				{
					if (!Map.Find(Rev->GetRev()))
						Log.Add(Rev.Release());
				}
			}
			break;
		}
		case VcCvs:
		{
			GHashTbl<uint64, VcCommit*> Map;
			GString::Array c = s.Split("=============================================================================");
			for (GString *Commit = NULL; c.Iterate(Commit);)
			{
				if (Commit->Strip().Length())
				{
					GString Head, File;
					GString::Array Versions = Commit->Split("----------------------------");
					GString::Array Lines = Versions[0].SplitDelimit("\r\n");
					for (GString *Line = NULL; Lines.Iterate(Line);)
					{
						GString::Array p = Line->Split(":", 1);
						if (p.Length() == 2)
						{
							// LgiTrace("Line: %s\n", Line->Get());

							GString Var = p[0].Strip().Lower();
							GString Val = p[1].Strip();
							if (Var.Equals("branch"))
							{
								if (Val.Length())
									Branches.Add(Val);
							}
							else if (Var.Equals("head"))
							{
								Head = Val;
							}
							else if (Var.Equals("rcs file"))
							{
								GString::Array f = Val.SplitDelimit(",");
								File = f.First();
							}
						}
					}

					// LgiTrace("%s\n", Commit->Get());

					for (unsigned i=1; i<Versions.Length(); i++)
					{
						GString::Array Lines = Versions[i].SplitDelimit("\r\n");
						if (Lines.Length() >= 3)
						{
							GString Ver = Lines[0].Split(" ").Last();
							GString::Array a = Lines[1].SplitDelimit(";");
							GString Date = a[0].Split(":", 1).Last().Strip();
							GString Author = a[1].Split(":", 1).Last().Strip();
							GString Id = a[2].Split(":", 1).Last().Strip();
							GString Msg = Lines[2];
							LDateTime Dt;
							
							if (Dt.Parse(Date))
							{
								uint64 Ts;
								if (Dt.Get(Ts))
								{
									VcCommit *Cc = Map.Find(Ts);
									if (!Cc)
									{
										Map.Add(Ts, Cc = new VcCommit(d));
										Log.Add(Cc);
										Cc->CvsParse(Dt, Author, Msg);
									}
									Cc->Files.Add(File.Get());
								}
								else LgiAssert(!"NO ts for date.");
							}
							else LgiAssert(!"Date parsing failed.");
						}
					}
				}
			}
			break;
		}
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	// LgiTrace("%s:%i - ParseLog: Skip=%i, Error=%i\n", _FL, Skipped, Errors);
	IsLogging = false;

	return true;
}

bool VcFolder::ParseInfo(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		case VcHg:
		{
			CurrentCommit = s.Strip();
			break;
		}
		case VcSvn:
		{
			GString::Array c = s.Split("\n");
			for (unsigned i=0; i<c.Length(); i++)
			{
				GString::Array a = c[i].SplitDelimit(":", 1);
				if (a.First().Strip().Equals("Revision"))
					CurrentCommit = a[1].Strip();
				else if (a.First().Strip().Equals("URL"))
					RepoUrl = a[1].Strip();
			}
			break;
		}			
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	IsGetCur = false;

	return true;
}

bool VcFolder::ParseCommit(int Result, GString s, ParseParams *Params)
{
	Select(true);
	
	CommitListDirty = Result == 0;
	CurrentCommit.Empty();
	IsCommit = false;

	if (Result)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			Unpushed++;
			Update();

			d->ClearFiles();

			GWindow *w = d->Diff ? d->Diff->GetWindow() : NULL;
			if (w)
				w->SetCtrlName(IDC_MSG, NULL);

			
			if (Params->Str.Find("Push") >= 0)
				Push();
			break;
		}
		case VcSvn:
		{
			CurrentCommit.Empty();
			Pull();
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

bool VcFolder::ParseUpdate(int Result, GString s, ParseParams *Params)
{
	if (Result == 0)
	{
		CurrentCommit = NewRev;
	}
	
	IsUpdate = false;
	return true;
}

bool VcFolder::ParseWorking(int Result, GString s, ParseParams *Params)
{
	d->ClearFiles();
	ParseDiffs(s, NULL, true);
	IsWorkingFld = false;
	d->Files->ResizeColumnsToContent();

	if (GetType() == VcSvn)
	{
		Unpushed = d->Files->Length() > 0 ? 1 : 0;
		Update();
	}

	return false;
}

bool VcFolder::ParseDiffs(GString s, GString Rev, bool IsWorking)
{
	LgiAssert(IsWorking || Rev.Get() != NULL);

	switch (GetType())
	{
		case VcGit:
		{
			GString::Array a = s.Split("\n");
			GString Diff;
			VcFile *f = NULL;
			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "diff", 4))
				{
					if (f)
						f->SetDiff(Diff);
					Diff.Empty();

					GString Fn = a[i].Split(" ").Last()(2, -1);
					f = new VcFile(d, this, Rev, IsWorking);
					f->SetText(Fn, COL_FILENAME);
					d->Files->Insert(f);
				}
				else if (!_strnicmp(Ln, "index", 5) ||
						 !_strnicmp(Ln, "commit", 6)   ||
						 !_strnicmp(Ln, "Author:", 7)   ||
						 !_strnicmp(Ln, "Date:", 5)   ||
						 !_strnicmp(Ln, "+++", 3)   ||
						 !_strnicmp(Ln, "---", 3))
				{
					// Ignore
				}
				else
				{
					if (Diff) Diff += "\n";
					Diff += a[i];
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			break;
		}
		case VcHg:
		{
			GString::Array a = s.Split("\n");
			GString Diff;
			VcFile *f = NULL;
			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "diff", 4))
				{
					if (f)
						f->SetDiff(Diff);
					Diff.Empty();

					GString Fn = a[i].Split(" ").Last();
					f = new VcFile(d, this, Rev, IsWorking);
					f->SetText(Fn, COL_FILENAME);
					d->Files->Insert(f);
				}
				else if (!_strnicmp(Ln, "index", 5) ||
						 !_strnicmp(Ln, "commit", 6)   ||
						 !_strnicmp(Ln, "Author:", 7)   ||
						 !_strnicmp(Ln, "Date:", 5)   ||
						 !_strnicmp(Ln, "+++", 3)   ||
						 !_strnicmp(Ln, "---", 3))
				{
					// Ignore
				}
				else
				{
					if (Diff) Diff += "\n";
					Diff += a[i];
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			break;
		}
		case VcSvn:
		{
			GString::Array a = s.Replace("\r").Split("\n");
			GString Diff;
			VcFile *f = NULL;
			bool InPreamble = false;
			bool InDiff = false;
			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "Index:", 6))
				{
					if (f)
					{
						f->SetDiff(Diff);
						f->Select(false);
					}
					Diff.Empty();
					InDiff = false;
					InPreamble = false;

					GString Fn = a[i].Split(":", 1).Last().Strip();
					f = new VcFile(d, this, Rev, IsWorking);
					f->SetText(Fn, COL_FILENAME);
					d->Files->Insert(f);
				}
				else if (!_strnicmp(Ln, "------", 6))
				{
					InPreamble = !InPreamble;
				}
				else if (!_strnicmp(Ln, "======", 6))
				{
					InPreamble = false;
					InDiff = true;
				}
				else if (InDiff)
				{
					if (!strncmp(Ln, "--- ", 4) ||
						!strncmp(Ln, "+++ ", 4))
					{
					}
					else
					{
						if (Diff) Diff += "\n";
						Diff += a[i];
					}
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			break;
		}
		case VcCvs:
		{
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

bool VcFolder::ParseFiles(int Result, GString s, ParseParams *Params)
{
	d->ClearFiles();
	ParseDiffs(s, Params->Str, false);
	IsFilesCmd = false;
	d->Files->ResizeColumnsToContent();

	return false;
}

void VcFolder::OnPulse()
{
	bool Reselect = false, CmdsChanged = false;
		
	for (unsigned i=0; i<Cmds.Length(); i++)
	{
		Cmd *c = Cmds[i];
		if (c && c->Rd->IsExited())
		{
			GString s = c->Buf.NewGStr();
			int Result = c->Rd->ExitCode();
			ParseParams Params;
			Params.Str = c->Param;
			Reselect |= CALL_MEMBER_FN(*this, c->PostOp)(Result, s, &Params);
			Cmds.DeleteAt(i--, true);
			delete c;
			CmdsChanged = true;
		}
	}

	if (Reselect)
		Select(true);
	if (CmdsChanged)
		Update();
}

void VcFolder::OnRemove()
{
	GXmlTag *t = d->Opts.LockTag(NULL, _FL);
	if (t)
	{
		for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
		{
			if (c->IsTag(OPT_Folder) &&
				c->GetContent() &&
				!_stricmp(c->GetContent(), Path))
			{
				c->RemoveTag();
				delete c;
				break;
			}
		}
		d->Opts.Unlock();
	}
}

void VcFolder::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		GSubMenu s;
		s.AppendItem("Remove", IDM_REMOVE);
		int Cmd = s.Float(GetTree(), m);
		switch (Cmd)
		{
			case IDM_REMOVE:
			{
				OnRemove();
				delete this;
				break;
			}
			default:
				break;
		}
	}
}

void VcFolder::OnUpdate(const char *Rev)
{
	if (!Rev)
		return;
	
	if (!IsUpdate)
	{
		GString Args;

		NewRev = Rev;
		switch (GetType())
		{
			case VcGit:
				Args.Printf("checkout %s", Rev);
				IsUpdate = StartCmd(Args, &VcFolder::ParseUpdate, NULL, true);
				break;
			case VcSvn:
				Args.Printf("up -r %s", Rev);
				IsUpdate = StartCmd(Args, &VcFolder::ParseUpdate, NULL, true);
				break;
			default:
			{
				LgiAssert(!"Impl me.");
				break;
			}
		}
	}
}

class VcLeaf : public GTreeItem
{
	VcFolder *Parent;
	bool Folder;
	GString Path, Leaf;
	GTreeItem *Tmp;

public:
	VcLeaf(VcFolder *parent, GTreeItem *Item, GString path, GString leaf, bool folder)
	{
		Parent = parent;
		Path = path;
		Leaf = leaf;
		Folder = folder;
		Tmp = NULL;
		Item->Insert(this);

		if (Folder)
		{
			Insert(Tmp = new GTreeItem);
			Tmp->SetText("Loading...");
		}
	}

	GString Full()
	{
		GFile::Path p(Path);
		p += Leaf;
		return p.GetFull();
	}

	void OnExpand(bool b)
	{
		if (Tmp && b)
		{
			Tmp->Remove();
			DeleteObj(Tmp);
			
			GFile::Path p(Path);
			p += Leaf;
			Parent->ReadDir(this, p);
		}
	}

	char *GetText(int Col)
	{
		if (Col == 0)
			return Leaf;

		return NULL;
	}

	int GetImage(int Flags)
	{
		return Folder ? IcoFolder : IcoFile;
	}

	int Compare(VcLeaf *b)
	{
		// Sort folders to the top...
		if (Folder ^ b->Folder)
			return (int)b->Folder - (int)Folder;
	
		// Then alphabetical
		return Stricmp(Leaf.Get(), b->Leaf.Get());
	}

	void OnMouseClick(GMouse &m)
	{
		if (m.IsContextMenu())
		{
			GSubMenu s;
			s.AppendItem("Log", IDM_LOG);
			s.AppendItem("Blame", IDM_BLAME, !Folder);
			int Cmd = s.Float(GetTree(), m);
			switch (Cmd)
			{
				case IDM_LOG:
				{
					break;
				}
				case IDM_BLAME:
				{
					Parent->Blame(Full());
					break;
				}
			}
		}
	}
};

int FolderCompare(GTreeItem *a, GTreeItem *b, NativeInt UserData)
{
	VcLeaf *A = dynamic_cast<VcLeaf*>(a);
	VcLeaf *B = dynamic_cast<VcLeaf*>(b);
	if (!A || !B)
		return 0;
	return A->Compare(B);
}

void VcFolder::ReadDir(GTreeItem *Parent, const char *Path)
{
	// Read child items
	GDirectory Dir;
	for (int b = Dir.First(Path); b; b = Dir.Next())
	{
		if (Dir.IsDir())
		{
			if (Dir.GetName()[0] != '.')
			{
				new VcLeaf(this, Parent, Path, Dir.GetName(), true);
			}
		}
		else if (!Dir.IsHidden())
		{
			char *Ext = LgiGetExtension(Dir.GetName());
			if (!Ext) continue;
			if (!stricmp(Ext, "c") ||
				!stricmp(Ext, "cpp") ||
				!stricmp(Ext, "h"))
			{
				new VcLeaf(this, Parent, Path, Dir.GetName(), false);
			}
		}
	}

	Parent->SortChildren(FolderCompare);
}

void VcFolder::OnExpand(bool b)
{
	if (Tmp && b)
	{
		Tmp->Remove();
		DeleteObj(Tmp);
		ReadDir(this, Path);
	}
}

void VcFolder::ListCommit(VcCommit *c)
{
	if (!IsFilesCmd)
	{
		GString Args;
		switch (GetType())
		{
			case VcGit:
				// Args.Printf("show --oneline --name-only %s", Rev);
				Args.Printf("show %s", c->GetRev());
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles, c->GetRev());
				break;
			case VcSvn:
				Args.Printf("log --verbose --diff -r %s", c->GetRev());
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles, c->GetRev());
				break;
			case VcCvs:
			{
				d->ClearFiles();
				for (unsigned i=0; i<c->Files.Length(); i++)
				{				
					VcFile *f = new VcFile(d, this, c->GetRev(), false);
					if (f)
					{
						f->SetText(c->Files[i], COL_FILENAME);
						d->Files->Insert(f);
					}
				}
				d->Files->ResizeColumnsToContent();
				break;
			}
			default:
				LgiAssert(!"Impl me.");
				break;
		}

		if (IsFilesCmd)
			d->ClearFiles();
	}
}

void VcFolder::ListWorkingFolder()
{
	if (!IsWorkingFld)
	{
		d->ClearFiles();

		switch (GetType())  
		{
			case VcCvs:
				IsWorkingFld = StartCmd("-q diff --brief", &VcFolder::ParseWorking);
				break;
			case VcGit:
				IsWorkingFld = StartCmd("diff --diff-filter=ACDMRTU", &VcFolder::ParseWorking);
				break;
			default:
				IsWorkingFld = StartCmd("diff", &VcFolder::ParseWorking);
				break;
		}
	}
}

void VcFolder::Commit(const char *Msg, const char *Branch, bool AndPush)
{
	VcFile *f = NULL;
	GArray<VcFile*> Add;
	bool Partial = false;
	while (d->Files->Iterate(f))
	{
		int c = f->Checked();
		if (c > 0)
			Add.Add(f);
		else
			Partial = true;
	}

	if (CurrentBranch && Branch &&
		!CurrentBranch.Equals(Branch))
	{
		int Response = LgiMsg(GetTree(), "Do you want to start a new branch?", AppName, MB_YESNO);
		if (Response != IDYES)
			return;
	}

	if (!IsCommit)
	{
		GString Args;
		GString Param = AndPush ? "Push" : NULL;

		switch (GetType())
		{
			case VcGit:
			{
				if (Partial)
				{
					LgiMsg(GetTree(), "%s:%i - Not impl.", AppName, MB_OK, _FL);
					break;
				}
				else
				{
					GString m(Msg);
					m = m.Replace("\"", "\\\"");
					Args.Printf("commit -am \"%s\"", m.Get());
				}
				IsCommit = StartCmd(Args, &VcFolder::ParseCommit, Param, true);
				break;
			}
			case VcSvn:
			{
				GString::Array a;
				a.New().Printf("commit -m \"%s\"", Msg);
				for (VcFile **pf = NULL; Add.Iterate(pf); )
					a.New() = (*pf)->GetFileName();

				Args = GString(" ").Join(a);
				IsCommit = StartCmd(Args, &VcFolder::ParseCommit, NULL, true);
				break;
			}
			default:
			{
				LgiAssert(!"Impl me.");
				break;
			}
		}
	}
}

void VcFolder::Push()
{
	GString Args;
	bool Working = false;
	switch (GetType())
	{
		case VcGit:
			Working = StartCmd("push", &VcFolder::ParsePush, NULL, true);
			break;
		case VcSvn:
			// Nothing to do here.. the commit pushed the data already
			break;
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	if (d->Tabs && Working)
	{
		d->Tabs->Value(1);
		GetTree()->SendNotify(LvcCommandStart);
	}
}

bool VcFolder::ParsePush(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
			break;
		case VcSvn:
			break;
		default:
			break;
	}

	if (Result == 0)
	{
		Unpushed = 0;
		Update();
	}

	GetTree()->SendNotify(LvcCommandEnd);
	return false; // no reselect
}

void VcFolder::Pull()
{
	GString Args;
	bool Status = false;
	switch (GetType())
	{
		case VcGit:
			Status = StartCmd("pull", &VcFolder::ParsePull, NULL, true);
			break;
		case VcSvn:
			Status = StartCmd("up", &VcFolder::ParsePull, NULL, true);
			break;
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	if (d->Tabs && Status)
	{
		d->Tabs->Value(1);
		GetTree()->SendNotify(LvcCommandStart);
	}
}

bool VcFolder::ParsePull(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		{
			// Git does a merge by default, so the current commit changes...
			CurrentCommit.Empty();
			break;
		}
		case VcSvn:
		{
			// Svn also does a merge by default and can update our current position...
			CurrentCommit.Empty();

			GString::Array a = s.SplitDelimit("\r\n");
			for (GString *Ln = NULL; a.Iterate(Ln); )
			{
				if (Ln->Find("At revision") >= 0)
				{
					GString::Array p = Ln->SplitDelimit(" .");
					CurrentCommit = p.Last();
					break;
				}
			}
			break;
		}
		default:
			break;
	}

	GetTree()->SendNotify(LvcCommandEnd);
	CommitListDirty = true;
	return true; // Yes - reselect and update
}

bool VcFolder::ParseRevert(int Result, GString s, ParseParams *Params)
{
	ListWorkingFolder();
	return false;
}

bool VcFolder::Revert(const char *Path, const char *Revision)
{
	if (!Path)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			GString a;
			a.Printf("checkout \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseRevert);
			break;
		}
		case VcSvn:
		{
			GString a;
			a.Printf("revert \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseRevert);
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return false;
}

bool VcFolder::ParseBlame(int Result, GString s, ParseParams *Params)
{
	new BlameUi(d, GetType(), s);
	return false;
}

bool VcFolder::Blame(const char *Path)
{
	if (!Path)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			GString a;
			a.Printf("blame \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseBlame);
			break;
		}
		case VcHg:
		{
			GString a;
			a.Printf("annotate -un \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseBlame);
			break;
		}
		case VcSvn:
		{
			GString a;
			a.Printf("blame \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseBlame);
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

bool VcFolder::SaveFileAs(const char *Path, const char *Revision)
{
	if (!Path || !Revision)
		return false;

	return true;
}

bool VcFolder::ParseSaveAs(int Result, GString s, ParseParams *Params)
{
	return false;
}

bool VcFolder::ParseCounts(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		{
			Unpushed = (int) s.Strip().Split("\n").Length();
			break;
		}
		case VcSvn:
		{
			int64 ServerRev = 0;
			bool HasUpdate = false;

			GString::Array c = s.Split("\n");
			for (unsigned i=0; i<c.Length(); i++)
			{
				GString::Array a = c[i].SplitDelimit();
				if (a.Length() > 1 && a[0].Equals("Status"))
					ServerRev = a.Last().Int();
				else if (a[0].Equals("*"))
					HasUpdate = true;
			}

			if (ServerRev > 0 && HasUpdate)
			{
				int64 CurRev = CurrentCommit.Int();
				Unpulled = (int) (ServerRev - CurRev);
			}
			else Unpulled = 0;
			Update();
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	IsUpdatingCounts = false;
	Update();
	return false; // No re-select
}

void VcFolder::SetEol(const char *Path, int Type)
{
	if (!Path) return;

	switch (Type)
	{
		case IDM_EOL_LF:
		{
			ConvertEol(Path, false);
			break;
		}
		case IDM_EOL_CRLF:
		{
			ConvertEol(Path, true);
			break;
		}
		case IDM_EOL_AUTO:
		{
			#ifdef WINDOWS
			ConvertEol(Path, true);
			#else
			ConvertEol(Path, false);
			#endif
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
void VcFolder::UncommitedItem::Select(bool b)
{
	LListItem::Select(b);
	if (b)
	{
		GTreeItem *i = d->Tree->Selection();
		VcFolder *f = dynamic_cast<VcFolder*>(i);
		if (f)
			f->ListWorkingFolder();

		if (d->Msg)
		{
			d->Msg->Name(NULL);

			GWindow *w = d->Msg->GetWindow();
			if (w)
			{
				w->SetCtrlEnabled(IDC_COMMIT, true);
				w->SetCtrlEnabled(IDC_COMMIT_AND_PUSH, true);
			}
		}
	}
}

void VcFolder::UncommitedItem::OnPaint(GItem::ItemPaintCtx &Ctx)
{
	GFont *f = GetList()->GetFont();
	f->Transparent(false);
	f->Colour(Ctx.Fore, Ctx.Back);
	
	GDisplayString ds(f, "(working folder)");
	ds.Draw(Ctx.pDC, Ctx.x1 + ((Ctx.X() - ds.X()) / 2), Ctx.y1 + ((Ctx.Y() - ds.Y()) / 2), &Ctx);
}
