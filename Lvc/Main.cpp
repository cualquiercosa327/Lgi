#include "Lgi.h"
#include "resdefs.h"
#include "LList.h"
#include "GBox.h"
#include "GTree.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "Lvc";

enum Ctrls
{
	IDC_LIST = 100,
	IDC_BOX,
	IDC_TREE,

	IDM_ADD = 200,
};

class App : public GWindow
{
	GBox *Box;
	GTree *Tree;
	LList *Lst;

public:
    App()
    {
		Lst = NULL;
		Tree = NULL;
        Name(AppName);

        GRect r(0, 0, 1200, 800);
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
			Lst->AddColumn("Commit", 200);
			Lst->AddColumn("Author", 150);
			Lst->AddColumn("Date", 150);
			Lst->AddColumn("Message", 400);

            Visible(true);
        }
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

