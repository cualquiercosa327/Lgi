#include "Lgi.h"
#include "../Resources/resdefs.h"
#include "Lvc.h"
#include "GTableLayout.h"
#ifdef WINDOWS
#include "../Resources/resource.h"
#endif
#include "GTextLog.h"
#include "GButton.h"

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


class DiffView : public GTextLog
{
public:
	DiffView(int id) : GTextLog(id)
	{
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		List<GTextLine>::I it = GTextView3::Line.Start();
		for (GTextLine *ln = *it; ln; ln = *++it)
		{
			if (!ln->c.IsValid())
			{
				char16 *t = Text + ln->Start;
				
				if (*t == '+')
				{
					ln->c = GColour::Green;
					ln->Back.Rgb(245, 255, 245);
				}
				else if (*t == '-')
				{
					ln->c = GColour::Red;
					ln->Back.Rgb(255, 245, 245);
				}
				else if (*t == '@')
				{
					ln->c.Rgb(128, 128, 128);
					ln->Back.Rgb(235, 235, 235);
				}
				else
					ln->c.Set(LC_TEXT, 24);
			}
		}
	}
};

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
				r.x2++;
				printf("pos=%s\n", r.GetStr());
				v->SetPos(r);
				
				v->OnPosChange();
				r = v->GetUsedArea();
				if (r.Y() <= 1)
					r.Set(0, 0, 30, 30);
				GetCss(true)->Height(GCss::Len(GCss::LenPx, (float)r.Y()+3));
			}
			else LgiAssert(!"Missing table ctrl");
		}
		else LgiAssert(!"Missing toolbar resource");
	}

	void OnCreate()
	{
		AttachChildren();
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
	}
};

class CommitCtrls : public GLayout, public GLgiRes
{
public:
	CommitCtrls()
	{
		GAutoString Name;
		GRect Pos;
		if (LoadFromResource(IDD_COMMIT, this, &Pos, &Name))
		{
			GTableLayout *v;
			if (GetViewById(IDC_TABLE, v))
			{
				GRect r = v->GetPos();
				r.Offset(-r.x1, -r.y1);
				r.x2++;
				v->SetPos(r);
				
				v->OnPosChange();
				r = v->GetUsedArea();
				if (r.Y() <= 1)
					r.Set(0, 0, 30, 30);
				GetCss(true)->Height(GCss::Len(GCss::LenPx, (float)r.Y()));
			}
			else LgiAssert(!"Missing table ctrl");
		}
		else LgiAssert(!"Missing toolbar resource");
	}

	void OnPosChange()
	{
		GTableLayout *v;
		if (GetViewById(IDC_TABLE, v))
			v->SetPos(GetClient());
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
			Tree->AddColumn("Folder", 250);
			Tree->AddColumn("Counts", 50);
			Tree->Attach(FoldersBox);
			CommitsBox->Attach(FoldersBox);

			Lst = new LList(IDC_LIST, 0, 0, 200, 200);
			Lst->Attach(CommitsBox);
			Lst->AddColumn("---", 40);
			Lst->AddColumn("Commit", 270);
			Lst->AddColumn("Author", 240);
			Lst->AddColumn("Date", 130);
			Lst->AddColumn("Message", 400);

			GBox *FilesBox = new GBox(IDC_FILES_BOX, false);
			FilesBox->Attach(CommitsBox);

			Files = new LList(IDC_FILES, 0, 0, 200, 200);
			Files->Attach(FilesBox);
			Files->AddColumn("[ ]", 30);
			Files->AddColumn("State", 100);
			Files->AddColumn("Name", 400);

			GBox *MsgBox = new GBox(IDC_MSG_BOX, true);
			MsgBox->Attach(FilesBox);

			CommitCtrls *Commit = new CommitCtrls;
			Commit->Attach(MsgBox);

			Msg = new GTextView3(IDC_MSG, 0, 0, 200, 200);
			Msg->Sunken(true);
			Msg->SetWrapType(TEXTED_WRAP_NONE);
			Msg->Attach(MsgBox);
			Msg->GetCss(true)->Height(GCss::Len("100px"));

			Tabs = new GTabView(IDC_TAB_VIEW);
			Tabs->Attach(MsgBox);

			GTabPage *p = Tabs->Append("Diff");
			p->Append(Diff = new DiffView(IDC_TXT));
			// Diff->Sunken(true);
			Diff->SetWrapType(TEXTED_WRAP_NONE);

			p = Tabs->Append("Log");
			p->Append(Log = new GTextLog(IDC_LOG));
			// Log->Sunken(true);
			Log->SetWrapType(TEXTED_WRAP_NONE);

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
			case IDC_FILES:
			{
				switch (flag)
				{
					case GNotifyItem_ColumnClicked:
					{
						int Col = -1;
						GMouse m;
						if (Files->GetColumnClickInfo(Col, m))
						{
							if (Col == 0)
							{
								// Select / deselect all checkboxes..
								List<VcFile> n;
								if (Files->GetAll(n))
								{
									bool Checked = false;
									for (VcFile *f = n.First(); f; f = n.Next())
										Checked |= f->Checked() > 0;
									for (VcFile *f = n.First(); f; f = n.Next())
										f->Checked(Checked ? 0 : 1);
								}
							}
						}
						break;
					}
				}
				break;
			}
			case IDC_TREE:
			{
				switch (flag)
				{
					case GNotifyContainer_Click:
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
										if (DetectVcs(s.Name()))
											Tree->Insert(new VcFolder(this, s.Name()));
										else
											LgiMsg(this, "Folder not under version control.", AppName);
									}
								}
							}
						}
						break;
					}
					case LvcCommandStart:
					{
						SetCtrlEnabled(IDC_PUSH, false);
						SetCtrlEnabled(IDC_PULL, false);
						break;
					}
					case LvcCommandEnd:
					{
						SetCtrlEnabled(IDC_PUSH, true);
						SetCtrlEnabled(IDC_PULL, true);
						break;
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
			case IDC_COMMIT:
			{
				const char *Msg = GetCtrlName(IDC_MSG);
				if (ValidStr(Msg))
				{
					VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
					if (f)
					{
						f->Commit(Msg);
					}
				}
				else LgiMsg(this, "No message for commit.", AppName);
				break;
			}
			case IDC_PUSH:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Push();
				break;
			}
			case IDC_PULL:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Pull();
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

