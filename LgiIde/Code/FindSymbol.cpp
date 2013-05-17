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

struct FindSymbolPriv : public GMutex
{
	FindSymbolDlg *Dlg;
	GList *Lst;
	GArray<char*> Files;

	GAutoString CTagsExe;
	GAutoString CTagsIndexFile;
	GAutoString SearchStr;
	GArray<char*> ThreadMsg;

	bool RunSearch;
	GThreadEvent Sync;
	GAutoPtr<class FindSymbolThread> Thread;

	struct Result
	{
		GAutoString Symbol, File;
		int Line;
	};
	GArray<Result> Results;
	
	FindSymbolPriv(FindSymbolDlg *dlg) : GMutex("FindSymbolLock")
	{
		Dlg = dlg;
		Lst = 0;
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

class FindSymbolThread : public GThread
{
	bool Loop;
	FindSymbolPriv *d;

	struct Symbol
	{
		const char *Sym;
		const char *File;
		int Line;
	};

public:
	FindSymbolThread(FindSymbolPriv *priv) : GThread("FindSymbolThread")
	{
		d = priv;
		Loop = true;
		Run();
	}
	
	~FindSymbolThread()
	{
		Loop = false;
		d->RunSearch = false;
		d->Sync.Signal();
		while (!IsExited())
			LgiSleep(1);
	}
	
	void Msg(const char *s)
	{
		if (d->Lock(_FL))
		{
			d->ThreadMsg.Add(NewStr(s));
			d->Unlock();
		}
	}
	
	void MatchesToResults(GArray<Symbol*> &Matches)
	{
		if (d->RunSearch && d->Lock(_FL))
		{
			for (int i=0; i<Matches.Length(); i++)
			{
				FindSymbolPriv::Result &r = d->Results.New();
				r.Symbol.Reset(NewStr(Matches[i]->Sym));
				r.File.Reset(NewStr(Matches[i]->File));
				r.Line = Matches[i]->Line;
			}
			d->Unlock();
			Matches.Length(0);
		}
	}
	
	int Main()
	{
		// Create index...
		char tmp[MAX_PATH];
		if (!LgiGetSystemPath(LSP_TEMP, tmp, sizeof(tmp)))
		{
			Msg("Error getting temp folder.");
			return 0;
		}
		if (!LgiMakePath(tmp, sizeof(tmp), tmp, "lgiide_ctags_filelist.txt"))
		{
			Msg("Error making temp path.");
			return 0;
		}
		if (!d->CTagsIndexFile)
		{
			Msg("Error: no index file.");
			return 0;
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
		GAutoString data;
		char *end = NULL;
		
		GFile in;
		if (in.Open(d->CTagsIndexFile, O_READ))
		{
			int64 sz = in.GetSize();
			data.Reset(new char[sz+1]);
			int r = in.Read(data, sz);
			if (r < 0)
				return 0;
			end = data + r;
			*end = 0;
			in.Close();
		}

		// Parse		
		GArray<Symbol> Syms;
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
	
		// Start waiting for search terms
		Msg("Ready.");
		while (Loop)
		{
			if (d->Sync.Wait())
			{
				if (!Loop)
					break;
				
				// Find stuff...
				d->RunSearch = true;
				GAutoString SearchStr;
				if (d->Lock(_FL))
				{
					SearchStr = d->SearchStr;
					d->Unlock();
				}
				
				Symbol *s = &Syms[0];
				GArray<Symbol*> Matches;
				for (int i=0; d->RunSearch && i<Syms.Length(); i++)
				{
					if (stristr(s[i].Sym, SearchStr))
					{
						Matches.Add(s + i);
						
						if (Matches.Length() > 10)
						{
							MatchesToResults(Matches);
						}
					}
				}
				MatchesToResults(Matches);
			}
		}
		return 0;
	}
};

int AlphaCmp(GListItem *a, GListItem *b, NativeInt d)
{
	return stricmp(a->GetText(0), b->GetText(0));
}

FindSymbolDlg::FindSymbolDlg(AppWnd *app)
{
	d = new FindSymbolPriv(this);
	SetParent(App = app);
	if (LoadFromResource(IDD_FIND_SYMBOL))
	{
		MoveToCenter();

		List<IdeProject> Projects;
		IdeProject *p = App->RootProject();
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
		
		GViewI *f;
		if (GetViewById(IDC_STR, f))
			f->Focus(true);
		
		if (GetViewById(IDC_RESULTS, d->Lst))
		{
			if (!d->CTagsExe)
			{
				GListItem *i = new GListItem;
				i->SetText("Ctags binary missing.");
				d->Lst->Insert(i);
			}
		}
		
		d->Thread.Reset(new FindSymbolThread(d));
	}
}

FindSymbolDlg::~FindSymbolDlg()
{
	delete d;
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
		if (d->Results.Length())
		{
			for (int n=0; n<d->Results.Length(); n++)
			{
				FindSymbolPriv::Result &r = d->Results[n];
				
				GListItem *i = new GListItem;
				char str[MAX_PATH];
				sprintf_s(str, sizeof(str), "%s:%i", r.File.Get(), r.Line);
				i->SetText(r.Symbol, 0);
				i->SetText(str, 1);
				d->Lst->Insert(i);
			}
			d->Results.Length(0);
			d->Lst->ResizeColumnsToContent();
		}
		d->Unlock();
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
				d->RunSearch = false;
				d->Lst->Empty();
				
				char *Str = v->Name();
				if (Str && strlen(Str) > 1)
				{
					if (d->Lock(_FL))
					{
						d->SearchStr.Reset(NewStr(Str));
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
							File.Reset(NewStr(r, colon - r));
							Line = atoi(++colon);
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
