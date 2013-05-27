#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "FindSymbol.h"
#include "GThreadEvent.h"
#include "GList.h"
#include "GProcess.h"
#include "GToken.h"

#include "resdefs.h"

const char *CTagsExeName = "ctags"
	#ifdef WIN32
	".exe"
	#endif
	;

class FindSymbolDlg : public GDialog
{
	AppWnd *App;
	FindSymbolSystemPriv *d;

public:
	FindSymResult Result;

	FindSymbolDlg(FindSymbolSystemPriv *priv, GViewI *parent);
	~FindSymbolDlg();
	
	int OnNotify(GViewI *v, int f);
	void OnPulse();
	void OnCreate();
	bool OnViewKey(GView *v, GKey &k);
};

struct SearchRequest
{
	GAutoString Str;
	bool NoResults, Finished;
	GArray<FindSymResult> Results;
	
	SearchRequest()
	{
		NoResults = false;
		Finished = false;
	}
};

class FindSymbolThread : public GThread
{
	struct Symbol
	{
		const char *Sym;
		const char *File;
		int Line;
	};

	bool Loop;
	FindSymbolSystemPriv *d;
	GAutoString data;
	GArray<Symbol> Syms;

public:
	enum ThreadState
	{
		Initializing,
		Waiting,
		Working,
		ExitSearch,
	} State;

	FindSymbolThread(FindSymbolSystemPriv *priv);
	~FindSymbolThread();

	void UpdateTags();
	void Msg(const char *s);
	void MatchesToResults(struct SearchRequest *Req, GArray<Symbol*> &Matches);
	int Main();
	void Stop();
};

struct FindSymbolSystemPriv : public GMutex
{
	AppWnd *App;
	GList *Lst;
	
	// All the input files from the projects that are open
	GArray<char*> Files;

	GAutoString CTagsExe;
	GAutoString CTagsIndexFile;
	GArray<char*> ThreadMsg;

	bool TagsDirty;

	GThreadEvent Sync;
	GAutoPtr<FindSymbolThread> Thread;

	GArray<SearchRequest*> Requests, Done;
	
	FindSymbolSystemPriv() : GMutex("FindSymbolLock")
	{
		App = NULL;
		Lst = NULL;
		TagsDirty = false;
		
		CTagsExe.Reset(LgiFindFile(CTagsExeName));
		if (!CTagsExe)
		{
			char p[MAX_PATH];
			if (LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p)) &&
				LgiMakePath(p, sizeof(p), p, "../../../../CodeLib/ctags58") &&
				LgiMakePath(p, sizeof(p), p, CTagsExeName))
			{
				if (FileExists(p))
					CTagsExe.Reset(NewStr(p));
			}
		}
	}
};

FindSymbolThread::FindSymbolThread(FindSymbolSystemPriv *priv) : GThread("FindSymbolThread")
{
	State = Initializing;
	d = priv;
	Loop = true;
	Run();
}

FindSymbolThread::~FindSymbolThread()
{
	Loop = false;
	State = ExitSearch;
	d->Sync.Signal();
	while (!IsExited())
		LgiSleep(1);
}

void FindSymbolThread::Msg(const char *s)
{
	if (d->Lock(_FL))
	{
		d->ThreadMsg.Add(NewStr(s));
		d->Unlock();
	}
}

void FindSymbolThread::MatchesToResults(SearchRequest *Req, GArray<Symbol*> &Matches)
{
	if (State == Working &&
		Req &&
		d->Lock(_FL))
	{
		for (int i=0; i<Matches.Length(); i++)
		{
			FindSymResult &r = Req->Results.New();
			r.Symbol.Reset(NewStr(Matches[i]->Sym));
			r.File.Reset(NewStr(Matches[i]->File));
			r.Line = Matches[i]->Line;
		}
		d->Unlock();
		Matches.Length(0);
	}
}

void FindSymbolThread::UpdateTags()
{
	// Create index...
	char tmp[MAX_PATH];
	if (!LgiGetSystemPath(LSP_TEMP, tmp, sizeof(tmp)))
	{
		Msg("Error getting temp folder.");
		return;
	}
	if (!LgiMakePath(tmp, sizeof(tmp), tmp, "lgiide_ctags_filelist.txt"))
	{
		Msg("Error making temp path.");
		return;
	}
	if (!d->CTagsIndexFile)
	{
		Msg("Error: no index file.");
		return;
	}

	char msg[MAX_PATH];
	sprintf_s(msg, sizeof(msg), "Generating index: %s", d->CTagsIndexFile.Get());
	Msg(msg);

	if (d->Lock(_FL))
	{
		GFile tmpfile;
		if (tmpfile.Open(tmp, O_WRITE))
		{
			tmpfile.SetSize(0);
			for (int i=0; i<d->Files.Length(); i++)
			{
				char *file = d->Files[i];
				tmpfile.Print("%s\n", file);
			}
		}
		d->Unlock();
	}		
	
	char args[MAX_PATH];
	sprintf_s(args, sizeof(args), "--excmd=number -f \"%s\" -L \"%s\"", d->CTagsIndexFile.Get(), tmp);
	GProcess proc;
	bool b = proc.Run(d->CTagsExe, args, NULL, true);
	// FileDev->Delete(tmp);
	
	// Read in the tags file...
	Msg("Reading tags file...");
	char *end = NULL;
	
	GFile in;
	if (in.Open(d->CTagsIndexFile, O_READ))
	{
		int64 sz = in.GetSize();
		data.Reset(new char[sz+1]);
		int r = in.Read(data, sz);
		if (r < 0)
			return;
		end = data + r;
		*end = 0;
		in.Close();
	}

	// Parse
	Syms.Length(0);
	if (data)
	{
		for (char *b = data; *b; )
		{
			LgiAssert(b < end);
			
			Symbol &s = Syms.New();
			s.Sym = b;
			while (*b && *b != '\t') b++;
			if (*b != '\t')
				break;
			*b++ = 0;
			
			s.File = b;
			while (*b && *b != '\t' && *b != '\n') b++;
			if (*b != '\t')
				break;
			*b++ = 0;
			
			s.Line = atoi(b);
			while (*b && *b != '\n') b++;
			if (*b != '\n')
				break;
			b++;
		}
	}
}

int FindSymbolThread::Main()
{
	// Start waiting for search terms
	Msg("Ready.");
	while (Loop)
	{
		State = Waiting;
		if (d->Sync.Wait())
		{
			if (!Loop)
				break;
			
			State = Working;
			if (d->TagsDirty)
			{
				Msg("Updating tags...");
				UpdateTags();
				d->TagsDirty = false;
			}

			SearchRequest *Req = NULL;
			if (d->Lock(_FL))
			{
				if (d->Requests.Length())
				{
					Req = d->Requests[0];
					d->Requests.DeleteAt(0, true);
				}
				d->Unlock();
			}
			if (Req)
			{			
				Symbol *s = &Syms[0];
				GArray<Symbol*> Matches;
				for (int i=0; State == Working && i<Syms.Length(); i++)
				{
					if (stristr(s[i].Sym, Req->Str))
					{
						Matches.Add(s + i);
					}
				}
				if (Matches.Length())
					MatchesToResults(Req, Matches);
				else
					Req->NoResults = true;
				
				if (d->Lock(_FL))
				{
					d->Done.Add(Req);
					Req->Finished = true;
					d->Unlock();
				}
				else delete Req;
			}
		}
	}
	return 0;
}

void FindSymbolThread::Stop()
{
	State = ExitSearch;
	while (State == ExitSearch)
		LgiSleep(1);
}

int AlphaCmp(GListItem *a, GListItem *b, NativeInt d)
{
	return stricmp(a->GetText(0), b->GetText(0));
}

FindSymbolDlg::FindSymbolDlg(FindSymbolSystemPriv *priv, GViewI *parent)
{
	d = priv;
	SetParent(parent);
	if (LoadFromResource(IDD_FIND_SYMBOL))
	{
		MoveToCenter();

		GViewI *f;
		if (GetViewById(IDC_STR, f))
			f->Focus(true);
		
		if (GetViewById(IDC_RESULTS, d->Lst))
		{
			d->Lst->MultiSelect(false);
			if (!d->CTagsExe)
			{
				GListItem *i = new GListItem;
				i->SetText("Ctags binary missing.");
				d->Lst->Insert(i);
			}
		}
	}
}

FindSymbolDlg::~FindSymbolDlg()
{
	d->Lst = NULL;
}

void FindSymbolDlg::OnPulse()
{
	if (d->Lst && d->Lock(_FL))
	{
		if (d->ThreadMsg.Length())
		{
			GArray<char*> Lines;
			GToken t(GetCtrlName(IDC_LOG), "\n");
			int i;
			for (i=0; i<t.Length(); i++)
				Lines.Add(t[i]);
			for (i=0; i<d->ThreadMsg.Length(); i++)
				Lines.Add(d->ThreadMsg[i]);

			GStringPipe p;
			int Start = max(0, (int)Lines.Length()-3);
			for (i=Start; i<Lines.Length(); i++)
				p.Print("%s\n", Lines[i]);
			d->ThreadMsg.DeleteArrays();

			GAutoString a(p.NewStr());
			SetCtrlName(IDC_LOG, a);
		}

		SearchRequest *Req = NULL;
		if (d->Done.Length())
		{
			Req = d->Done[d->Done.Length()-1];
			d->Done.DeleteAt(d->Done.Length()-1);
			d->Done.DeleteObjects();
		}
		d->Unlock();
		if (Req)
		{
			if (Req->NoResults)
			{
				d->Lst->Empty();
			}
			else if (Req->Results.Length())
			{
				// Three types of items:
				// 1) New results that need to go in the list.
				// 2) Duplicates of existing results.
				// 3) Old results that are no longer relevant.
				GHashTbl<char*, GListItem*> Old;

				// Setup previous hash table to track stuff from the last search. This
				// is used to clear old results that are no longer relevant.
				List<GListItem> All;
				d->Lst->GetAll(All);
				GListItem *i;
				for (i=All.First(); i; i=All.Next())
				{
					i->_UserInt = false;
					Old.Add(i->GetText(1), i);
				}

				for (int n=0; n<Req->Results.Length(); n++)
				{
					FindSymResult &r = Req->Results[n];
					char str[MAX_PATH];
					sprintf_s(str, sizeof(str), "%s:%i", r.File.Get(), r.Line);				
					i = Old.Find(str);
					if (!i)
					{
						i = new GListItem;
						i->SetText(r.Symbol, 0);
						i->SetText(str, 1);
						d->Lst->Insert(i);
					}
					if (i)
					{
						i->_UserInt = true;
					}
				}
				Req->Results.Length(0);

				// Thread has finished searching and there are previous entries to remove.
				d->Lst->GetAll(All);
				for (i=All.First(); i; i=All.Next())
				{
					if (!i->_UserInt)
						d->Lst->Delete(i);
				}
				
				d->Lst->Sort(AlphaCmp, 0);
				d->Lst->ResizeColumnsToContent();
				if (!d->Lst->GetSelected())
					d->Lst->Value(0);
				delete Req;
			}
		}
	}
}

void FindSymbolDlg::OnCreate()
{
	SetPulse(500);
	RegisterHook(this, GKeyEvents);
}

bool FindSymbolDlg::OnViewKey(GView *v, GKey &k)
{
	switch (k.vkey)
	{
		case VK_UP:
		case VK_DOWN:
		case VK_PAGEDOWN:
		case VK_PAGEUP:
		{
			return d->Lst->OnKey(k);
			break;
		}
	}
	
	return false;
}

int FindSymbolDlg::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDC_STR:
		{
			if (f != VK_RETURN)
			{
				char *Str = v->Name();
				if (Str && strlen(Str) > 1)
				{
					// Ask the thread to stop any search
					if (d->Thread->State != FindSymbolThread::Working)
						d->Thread->State = FindSymbolThread::ExitSearch;
					
					// Setup new search string
					if (d->Lock(_FL))
					{
						SearchRequest *Req = new SearchRequest;
						Req->Str.Reset(NewStr(Str));
						d->Requests.Add(Req);
						d->Unlock();
						d->Sync.Signal();
					}
				}
			}
			break;
		}
		case IDOK:
		{
			if (d->Lst)
			{
				GListItem *i = d->Lst->GetSelected();
				if (i)
				{
					char *r = i->GetText(1);
					if (r)
					{
						char *colon = strrchr(r, ':');
						if (colon)
						{
							Result.File.Reset(NewStr(r, colon - r));
							Result.Line = atoi(++colon);
						}
					}
				}
			}
			
			EndModal(true);
			break;
		}
		case IDCANCEL:
		{
			EndModal(false);
			break;
		}
	}
	
	return GDialog::OnNotify(v, f);
}

///////////////////////////////////////////////////////////////////////////
FindSymbolSystem::FindSymbolSystem(AppWnd *app)
{
	d = new FindSymbolSystemPriv;
	d->App = app;
}

FindSymbolSystem::~FindSymbolSystem()
{
	delete d;
}

void FindSymbolSystem::OnProject()
{
	List<IdeProject> Projects;
	IdeProject *p = d->App->RootProject();
	if (p)
	{
		char *ProjFile = p->GetFileName();
		if (ProjFile)
		{
			char p[MAX_PATH];
			if (LgiMakePath(p, sizeof(p), ProjFile, "../ctags.index"))
			{
				d->CTagsIndexFile.Reset(NewStr(p));
			}
		}
		
		Projects.Add(p);
		p->GetChildProjects(Projects);
	}
	for (p = Projects.First(); p; p = Projects.Next())
	{
		p->CollectAllSource(d->Files);
	}

	d->TagsDirty = true;
	if (!d->Thread)
	{
		d->Thread.Reset(new FindSymbolThread(d));
		d->Sync.Signal();
	}
}

FindSymResult FindSymbolSystem::OpenSearchDlg(GViewI *Parent)
{
	FindSymbolDlg Dlg(d, Parent);
	Dlg.DoModal();
	return Dlg.Result;	
}

void FindSymbolSystem::Search(const char *SearchStr, GArray<FindSymResult> &Results)
{
	if (d->Lock(_FL))
	{
		LgiAssert(d->Lst == NULL);
		
		// Create and add request
		SearchRequest *Req = new SearchRequest;
		Req->Str.Reset(NewStr(SearchStr));
		d->Requests.Add(Req);
		d->Unlock();
		d->Sync.Signal();
		
		// Wait for it to complete...
		uint64 Start = LgiCurrentTime();
		while (LgiCurrentTime() - Start < 3000)
		{
			LgiYield();
			LgiSleep(1);
			
			if (d->Lock(_FL))
			{
				if (d->Done.HasItem(Req))
					Start = 0;
				d->Unlock();
			}
		}
		
		if (d->Lock(_FL))
		{
			if (d->Done.HasItem(Req))
			{
				Results = Req->Results;
				d->Done.Delete(Req);
				delete Req;
			}
			
			d->Unlock();
		}
	}
}
