#include "Lgi.h"
#include "resdefs.h"
#include "LList.h"
#include "GBox.h"
#include "GTree.h"
#include "GOptionsFile.h"
#include "GSubProcess.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "Lvc";
#define OPT_Folders	"Folders"
#define OPT_Folder	"Folder"

enum Ctrls
{
	IDC_LIST = 100,
	IDC_BOX,
	IDC_TREE,

	IDM_ADD = 200,
};

enum VersionCtrl
{
	VcNone,
	VcCvs,
	VcSvn,
	VcGit,
	VcHg,
};

VersionCtrl DetectVcs(const char *Path)
{
	char p[MAX_PATH];

	if (!Path)
		return VcNone;

	if (LgiMakePath(p, sizeof(p), Path, ".git") &&
		DirExists(p))
		return VcGit;

	if (LgiMakePath(p, sizeof(p), Path, ".svn") &&
		DirExists(p))
		return VcSvn;

	if (LgiMakePath(p, sizeof(p), Path, ".hg") &&
		DirExists(p))
		return VcHg;

	if (LgiMakePath(p, sizeof(p), Path, "CVS") &&
		DirExists(p))
		return VcCvs;

	return VcNone;
}

struct AppPriv
{
	GTree *Tree;
	LList *Lst;
	GOptionsFile Opts;

	AppPriv()  : Opts(GOptionsFile::PortableMode, AppName)
	{
		Lst = NULL;
		Tree = NULL;
	}
};

class VcCommit : public LListItem
{
	AppPriv *d;
	bool Current;
	GString Rev;
	GString Author;
	LDateTime Ts;
	GString TsCache;
	GString Msg;

public:
	VcCommit(AppPriv *priv)
	{
		d = priv;
		Current = false;
	}

	char *GetRev()
	{
		return Rev;
	}

	void SetCurrent(bool b)
	{
		Current = b;
	}

	char *GetText(int Col)
	{
		switch (Col)
		{
			case 0:
				return Current ? "***" : NULL;
			case 1:
				return Rev;
			case 2:
				return Author;
			case 3:
				TsCache = Ts.Get();
				return TsCache;
			case 4:
				return Msg;
		}

		return NULL;
	}

	int IsWeekDay(const char *s)
	{
		static const char *Short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
		static const char *Long[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
		for (unsigned n=0; n<CountOf(Short); n++)
		{
			if (!_stricmp(Short[n], s) ||
				!_stricmp(Long[n], s))
				return n;
		}
		return -1;
	}

	int IsMonth(const char *s)
	{
		static const char *Short[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		static const char *Long[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
		for (unsigned n=0; n<CountOf(Short); n++)
		{
			if (!_stricmp(Short[n], s) ||
				!_stricmp(Long[n], s))
				return n;
		}
		return -1;
	}

	LDateTime ParseDate(GString s)
	{
		LDateTime d;

		GString::Array a = s.Split(" ");
		for (unsigned i=0; i<a.Length(); i++)
		{
			const char *c = a[i];
			if (IsDigit(*c))
			{
				if (strchr(c, ':'))
				{
					GString::Array t = a[i].Split(":");
					if (t.Length() == 3)
					{
						d.Hours((int)t[0].Int());
						d.Minutes((int)t[1].Int());
						d.Seconds((int)t[2].Int());
					}
				}
				else if (strchr(c, '-'))
				{
					GString::Array t = a[i].Split("-");
					if (t.Length() == 3)
					{
						d.Year((int)t[0].Int());
						d.Month((int)t[1].Int());
						d.Day((int)t[2].Int());
					}
				}
				else if (a[i].Length() == 4)
					d.Year((int)a[i].Int());
				else if (!d.Day())
					d.Day((int)a[i].Int());
			}
			else if (IsAlpha(*c))
			{
				int WkDay = IsWeekDay(c);
				if (WkDay >= 0)
					continue;
				
				int Mnth = IsMonth(c);
				if (Mnth >= 0)
					d.Month(Mnth + 1);
			}
			else if (*c == '-' || *c == '+')
			{
				c++;
				if (strlen(c) == 4)
				{
					// Timezone..
					int64 Tz = a[i].Int();
					int Hrs = (int) (Tz / 100);
					int Min = (int) (Tz % 100);
					d.SetTimeZone(Hrs * 60 + Min, false);
				}
			}
		}

		return d;
	}

	bool GitParse(GString s)
	{
		GString::Array lines = s.Split("\n");
		if (lines.Length() <= 3)
			return false;

		for (unsigned ln = 0; ln < lines.Length(); ln++)
		{
			GString &l = lines[ln];
			if (ln == 0)
				Rev = l.Strip();
			else if (l.Find("Author:") >= 0)
				Author = l.Split(":", 1)[1].Strip();
			else if (l.Find("Date:") >= 0)
				Ts = ParseDate(l.Split(":", 1)[1].Strip());
			else if (l.Strip().Length() > 0)
			{
				if (Msg)
					Msg += "\n";
				Msg += l.Strip();
			}
		}

		return Author && Msg && Rev;
	}

	bool SvnParse(GString s)
	{
		GString::Array lines = s.Split("\n");
		if (lines.Length() < 3)
			return false;

		for (unsigned ln = 0; ln < lines.Length(); ln++)
		{
			GString &l = lines[ln];
			if (ln == 0)
			{
				GString::Array a = l.Split("|");
				if (a.Length() > 3)
				{
					Rev = a[0].Strip(" \tr");
					Author = a[1].Strip();
					Ts = ParseDate(a[2]);
				}
			}
			else
			{
				if (Msg)
					Msg += "\n";
				Msg += l.Strip();
			}
		}

		return Author && Msg && Rev;
	}
};

class ReaderThread : public LThread
{
	GStream *Out;
	GSubProcess *Process;

public:
	ReaderThread(GSubProcess *p, GStream *out) : LThread("ReaderThread")
	{
		Process = p;
		Out = out;
		Run();
	}

	~ReaderThread()
	{
		Out = NULL;
		while (!IsExited())
			LgiSleep(1);
	}

	int Main()
	{
		Process->Start(true, false);

		while (Process->IsRunning())
		{
			if (Out)
			{
				char Buf[1024];
				int r = Process->Read(Buf, sizeof(Buf));
				if (r > 0)
					Out->Write(Buf, r);
			}
			else
			{
				Process->Kill();
				break;
			}
		}

		return 0;
	}
};

class VcFolder : public GTreeItem
{
	AppPriv *d;
	VersionCtrl Type;
	GString Path, CurrentCommit;
	GArray<VcCommit*> Log;
	GString Cache;
	
	GStringPipe LogBuf, InfoBuf;

	GAutoPtr<LThread> ReadCurrent;
	GAutoPtr<LThread> ReadLog;
	
public:
	VcFolder(AppPriv *priv, const char *p)
	{
		d = priv;
		Path = p;
		Type = VcNone;
	}

	VcFolder(AppPriv *priv, GXmlTag *t)
	{
		d = priv;
		Type = VcNone;
		Serialize(t, false);
	}

	VersionCtrl GetType()
	{
		if (Type == VcNone)
			Type = DetectVcs(Path);
		return Type;
	}

	char *GetText(int Col)
	{
		if (ReadCurrent || ReadLog)
		{
			Cache = Path;
			Cache += " (loading...)";
			return Cache;
		}

		return Path;
	}

	bool Serialize(GXmlTag *t, bool Write)
	{
		if (Write)
			t->SetContent(Path);
		else
			Path = t->GetContent();
		return true;
	}

	GXmlTag *Save()
	{
		GXmlTag *t = new GXmlTag(OPT_Folder);
		if (t)
			Serialize(t, true);
		return t;
	}

	void Select(bool b)
	{
		if (b)
		{
			if (Log.Length() == 0 && !ReadLog && LogBuf.GetSize() == 0)
			{
				GSubProcess *Process = NULL;

				switch (GetType())
				{
					case VcGit:
						Process = new GSubProcess("git", "log HEAD");
						break;
					case VcSvn:
						Process = new GSubProcess("svn", "log");
						break;
				}				
				if (Process)
				{
					Process->SetInitFolder(Path);
					ReadLog.Reset(new ReaderThread(Process, &LogBuf));
					Update();
				}
			}

			d->Lst->RemoveAll();
			List<LListItem> it;
			int64 CurRev = Atoi(CurrentCommit.Get());
			LgiTrace("%s:%i - CurrentCommit = %s\n", _FL, CurrentCommit.Get());
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
				it.Insert(Log[i]);
			}
			d->Lst->Insert(it);
			if (GetType() == VcSvn)
				d->Lst->ResizeColumnsToContent();
			d->Lst->UpdateAllItems();

			if (!CurrentCommit && !ReadCurrent && InfoBuf.GetSize() == 0)
			{
				GSubProcess *Process = NULL;

				switch (GetType())
				{
					case VcGit:
						Process = new GSubProcess("git", "rev-parse HEAD");
						break;
					case VcSvn:
						Process = new GSubProcess("svn", "info");
						break;
				}				
				if (Process)
				{
					Process->SetInitFolder(Path);
					ReadCurrent.Reset(new ReaderThread(Process, &InfoBuf));
					Update();
				}
			}
		}
	}

	void ParseLog(GString s)
	{
		switch (GetType())
		{
			case VcGit:
			{
				GString::Array c = s.Split("commit");
				for (unsigned i=0; i<c.Length(); i++)
				{
					GAutoPtr<VcCommit> Rev(new VcCommit(d));
					if (Rev->GitParse(c[i]))
						Log.Add(Rev.Release());
				}
				break;
			}
			case VcSvn:
			{
				GString::Array c = s.Split("------------------------------------------------------------------------");
				for (unsigned i=0; i<c.Length(); i++)
				{
					GAutoPtr<VcCommit> Rev(new VcCommit(d));
					if (Rev->SvnParse(c[i].Strip()))
						Log.Add(Rev.Release());
				}
				break;
			}			
			default:
				LgiAssert(!"Impl me.");
				break;
		}
	}

	void ParseInfo(GString s)
	{
		switch (GetType())
		{
			case VcGit:
			{
				CurrentCommit = s.Strip();
				break;
			}
			case VcSvn:
			{
				GString::Array c = s.Split("\n");
				for (unsigned i=0; i<c.Length(); i++)
				{
					if (c[i].Find("Revision:") >= 0)
					{
						CurrentCommit = c[i].Split(":", 1)[1].Strip();
						break;
					}
				}
				break;
			}			
			default:
				LgiAssert(!"Impl me.");
				break;
		}
	}

	void OnPulse()
	{
		bool Reselect = false;
		
		if (ReadLog && ReadLog->IsExited())
		{
			ParseLog(LogBuf.NewGStr());
			ReadLog.Reset();
			Reselect = true;
		}

		if (ReadCurrent && ReadCurrent->IsExited())
		{
			ParseInfo(InfoBuf.NewGStr());
			ReadCurrent.Reset();
			Reselect = true;
		}

		if (Reselect)
		{
			Select(true);
			Update();
		}
	}
};

class App : public GWindow, public AppPriv
{
	GBox *Box;

public:
    App()
    {
        Name(AppName);

        GRect r(0, 0, 1400, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (Attach(0))
        {
			Box = new GBox(IDC_BOX);
			Box->Attach(this);

			Tree = new GTree(IDC_TREE, 0, 0, 200, 200);
			Tree->GetCss(true)->Width(GCss::Len("300px"));
			Tree->Attach(Box);

			Lst = new LList(IDC_LIST, 0, 0, 200, 200);
			Lst->SetPourLargest(true);
			Lst->Attach(Box);
			Lst->AddColumn("---", 40);
			Lst->AddColumn("Commit", 270);
			Lst->AddColumn("Author", 240);
			Lst->AddColumn("Date", 130);
			Lst->AddColumn("Message", 400);

            Visible(true);
        }

		Opts.SerializeFile(false);
		GXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (!f)
		{
			Opts.CreateTag(OPT_Folders);
			f = Opts.LockTag(OPT_Folders, _FL);
		}
		if (f)
		{
			for (GXmlTag *c = f->Children.First(); c; c = f->Children.Next())
				if (c->IsTag(OPT_Folder))
					Tree->Insert(new VcFolder(this, c));
			Opts.Unlock();
		}

		SetPulse(500);
    }

	~App()
	{
		GXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (f)
		{
			f->EmptyChildren();

			VcFolder *vcf = NULL; bool b;
			while (b = Tree->Iterate(vcf))
				f->InsertTag(vcf->Save());
			Opts.Unlock();
		}
		Opts.SerializeFile(true);
	}

	void OnPulse()
	{
		VcFolder *vcf = NULL; bool b;
		while (b = Tree->Iterate(vcf))
			vcf->OnPulse();
	}

	int OnNotify(GViewI *c, int flag)
	{
		switch (c->GetId())
		{
			case IDC_TREE:
			{
				if (flag == GNotifyContainer_Click)
				{
					GMouse m;
					c->GetMouse(m);
					if (m.Right())
					{
						GSubMenu s;
						s.AppendItem("Add", IDM_ADD);
						int Cmd = s.Float(c->GetGView(), m);
						switch (Cmd)
						{
							case IDM_ADD:
							{
								GFileSelect s;
								s.Parent(c);
								if (s.OpenFolder())
								{
									Tree->Insert(new VcFolder(this, s.Name()));
								}
							}
						}
					}
				}
				break;
			}
		}

		return 0;
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

