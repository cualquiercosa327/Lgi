#include "Lgi.h"
#include "../Resources/resdefs.h"
#include "Lvc.h"
#include "GTableLayout.h"
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

class ToolBar : public GLayout, public GLgiRes
{
public:
	ToolBar()
	{
		GAutoString Name;
		GRect Pos;
		if (LoadFromResource(IDD_TOOLBAR, this, &Pos, &Name))
		{
			GTableLayout *v;
			if (GetViewById(IDC_TABLE, v))
			{
				GRect r = v->GetPos();
				r.Offset(-r.x1, -r.y1);
				v->SetPos(r);
				
				v->OnPosChange();
				r = v->GetUsedArea();
				GetCss(true)->Height(GCss::Len(GCss::LenPx, (float)r.Y()+3));
			}
		}
	}

	void OnCreate()
	{
		AttachChildren();
	}
};

class App : public GWindow, public AppPriv
{
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
			GBox *ToolsBox = new GBox(IDC_TOOLS_BOX, true);
			GBox *FoldersBox = new GBox(IDC_FOLDERS_BOX, false);
			GBox *CommitsBox = new GBox(IDC_COMMITS_BOX, true);

			ToolBar *Tools = new ToolBar;

			ToolsBox->Attach(this);
			Tools->Attach(ToolsBox);
			FoldersBox->Attach(ToolsBox);

			Tree = new GTree(IDC_TREE, 0, 0, 200, 200);
			Tree->GetCss(true)->Width(GCss::Len("300px"));
			Tree->Attach(FoldersBox);
			CommitsBox->Attach(FoldersBox);

			Lst = new LList(IDC_LIST, 0, 0, 200, 200);
			Lst->Attach(CommitsBox);
			Lst->AddColumn("---", 40);
			Lst->AddColumn("Commit", 270);
			Lst->AddColumn("Author", 240);
			Lst->AddColumn("Date", 130);
			Lst->AddColumn("Message", 400);

			Files = new LList(IDC_FILES, 0, 0, 200, 200);
			Files->Attach(CommitsBox);
			Files->AddColumn("Name", 400);

			AttachChildren();
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
			case IDC_CLEAR_FILTER:
			{
				SetCtrlName(IDC_FILTER, NULL);
				// Fall through
			}
			case IDC_FILTER:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Select(true);
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

