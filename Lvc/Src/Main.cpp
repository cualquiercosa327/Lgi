#include "Lgi.h"
#include "resdefs.h"
#include "Lvc.h"
#ifdef WINDOWS
#include "../Resources/resource.h"
#endif

//////////////////////////////////////////////////////////////////
const char *AppName = "Lvc";

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

		#ifdef WINDOWS
		SetIcon(MAKEINTRESOURCEA(IDI_ICON1));
		#else
		SetIcon("icon32.png");
		#endif

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

