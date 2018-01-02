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
		static const char *ShortDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
		for (unsigned n=0; n<CountOf(ShortDays); n++)
		{
			if (!_stricmp(ShortDays[n], s))
				return n;
		}
		return -1;
	}

	int IsMonth(const char *s)
	{
		static const char *Short[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		for (unsigned n=0; n<CountOf(Short); n++)
		{
			if (!_stricmp(Short[n], s))
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
};

class VcFolder : public GTreeItem
{
	AppPriv *d;
	GString Path;
	GArray<VcCommit*> Log;
	GSubProcess *Process;
	GString Cache;
	GArray<char> Out;
	
public:
	VcFolder(AppPriv *priv, const char *p)
	{
		d = priv;
		Path = p;
		Process = NULL;
	}

	VcFolder(AppPriv *priv, GXmlTag *t)
	{
		d = priv;
		Process = NULL;
		Serialize(t, false);
	}

	char *GetText(int Col)
	{
		if (Process)
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
			if (Log.Length() == 0 && !Process)
			{
				Process = new GSubProcess("git", "log HEAD");
				if (Process)
				{
					Process->Start(true, false);
					Update();
				}
			}

			d->Lst->RemoveAll();
			List<LListItem> it;
			for (unsigned i=0; i<Log.Length(); i++)
				it.Insert(Log[i]);
			d->Lst->Insert(it);
		}
	}

	void ParseLog(GString s)
	{
		GString::Array c = s.Split("commit");
		for (unsigned i=0; i<c.Length(); i++)
		{
			GAutoPtr<VcCommit> Rev(new VcCommit(d));
			if (Rev->GitParse(c[i]))
				Log.Add(Rev.Release());
		}
	}

	void OnPulse()
	{
		if (Process && Process->Peek())
		{
			char Buf[1024];
			int r;
			while ((r = Process->Read(Buf, sizeof(Buf))) > 0)
			{
				Out.Add(Buf, r);
				// LgiTrace("%.*s", r, Buf);
			}

			if (!Process->IsRunning())
			{
				DeleteObj(Process);

				Out.Add(0);
				ParseLog(Out.AddressOf());
				Update();

				Select(true);
			}
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

