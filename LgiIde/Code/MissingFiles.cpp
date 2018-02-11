#include "Lgi.h"
#include "LgiIde.h"
#include "ProjectNode.h"
#include "levenshtein.h"

enum Msgs
{
	M_CHECK_FILE = M_USER + 100,
	M_MISSING_FILE,
	M_ADD_SEARCH_PATH,
	M_SEARCH,
	M_RESULTS,
	M_RECURSE,
};

struct SearchResults
{
	ProjectNode *Node;
	GString Path;
	GString::Array Matches;
	
	SearchResults()
	{
		Node = 0;
	}
};

class FileExistsThread : public GEventTargetThread
{
	int Hnd;

public:
	FileExistsThread(int hnd) : GEventTargetThread("FileExistsThread")
	{
		Hnd = hnd;
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_CHECK_FILE:
			{
				GAutoPtr<SearchResults> Sr((SearchResults*)Msg->A());
				bool e = FileExists(Sr->Path);
				printf("Checking '%s' = %i\n", Sr->Path.Get(), e);
				if (!e)
					PostObject(Hnd, M_MISSING_FILE, Sr);
				break;
			}
		}

		return 0;
	}
};

bool IsParentFolder(GString p, GString c)
{
	GString::Array d1 = p.Split(DIR_STR);
	GString::Array d2 = c.Split(DIR_STR);

	if (d1.Length() > d2.Length())
		return false;

	for (unsigned i=0; i<d1.Length(); i++)
	{
		bool Match = d1[i].Equals(d2[i]);
		if (!Match)
			return false;
	}

	return true;
}

class SearchThread : public GEventTargetThread
{
	int Hnd;
	GArray<GString> Search;
	GArray<char*> Files;

public:
	SearchThread(int hnd) : GEventTargetThread("SearchThread")
	{
		Hnd = hnd;
	}

	~SearchThread()
	{
		Files.DeleteArrays();
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_ADD_SEARCH_PATH:
			{
				GAutoPtr<GString> p((GString*)Msg->A());
				bool IsParent = false;
				for (unsigned i=0; i<Search.Length(); i++)
				{
					if (p->Equals(Search[i]) ||
						(IsParent = IsParentFolder(Search[i], *p)))
					{
						// printf("'%s' is parent of '%s'\n", Search[i].Get(), p->Get());
						break;
					}
				}
				if (!IsParent)
					Search.New() = *p;
				break;
			}
			case M_RECURSE:
			{
				for (unsigned i=0; i<Search.Length(); i++)
				{
					GArray<const char*> Ext;
					Ext.Add("*.h");
					Ext.Add("*.hpp");
					Ext.Add("*.c");
					Ext.Add("*.cpp");

					printf("Recursing '%s'\n", Search[i].Get());
					LgiRecursiveFileSearch(Search[i], &Ext, &Files);
				}
				break;
			}
			case M_SEARCH:
			{
				GAutoPtr<SearchResults> Sr((SearchResults*)Msg->A());
				char *leaf1 = LgiGetLeaf(Sr->Path);
				for (unsigned i=0; i<Files.Length(); i++)
				{
					char *leaf2 = LgiGetLeaf(Files[i]);
					unsigned int Dist = levenshtein(leaf1, leaf2);
					if (Dist < 4)
					{
						Sr->Matches.New() = Files[i];
					}
				}

				printf("Posting '%s' with %i results.\n", Sr->Path.Get(), Sr->Matches.Length());
				PostObject(Hnd, M_RESULTS, Sr);
				break;
			}
		}

		return 0;
	}
};

class MissingFiles : public GDialog
{
	IdeProject *Proj;
	int SearchHnd;
	int ExistsHnd;
	LList *Lst;
	GArray<SearchResults*> Files;

public:
	MissingFiles(IdeProject *proj)
	{
		Proj = proj;
		SearchHnd = -1;
		ExistsHnd = -1;
		Lst = NULL;
		
		SetParent(Proj->GetApp());
		if (LoadFromResource(IDD_MISSING_FILES))
		{
			GetViewById(IDC_RESULTS, Lst);
			MoveSameScreen(Proj->GetApp());

			SetCtrlEnabled(IDC_MISSING, false);
			SetCtrlEnabled(IDC_RESULTS, false);
			SetCtrlEnabled(IDC_BROWSE, false);

			ExistsHnd = (new FileExistsThread(AddDispatch()))->GetHandle();
			SearchHnd = (new SearchThread(AddDispatch()))->GetHandle();

			GHashTbl<char*,bool> Flds;

			List<IdeProject> Child;
			Proj->GetChildProjects(Child);
			Child.Add(Proj);

			GArray<ProjectNode*> Nodes;

			for (IdeProject *p = Child.First(); p; p = Child.Next())
			{
				if (p->GetAllNodes(Nodes))
				{
					for (unsigned i=0; i<Nodes.Length(); i++)
					{
						GString s = Nodes[i]->GetFullPath();
						if (s)
						{
							SearchResults *Sr = new SearchResults;
							Sr->Node = Nodes[i];
							Sr->Path = s;
							PostThreadEvent(ExistsHnd, M_CHECK_FILE, (GMessage::Param) Sr);
							
							GString Parent = s.Get();
							LgiTrimDir(Parent);
							Flds.Add(Parent, true);
						}
					}
				}

				GAutoString s = p->GetBasePath();
				if (s)
					Flds.Add(s, true);
			}

			char *Path;
			for (bool b = Flds.First(&Path); b; b = Flds.Next(&Path))
				PostThreadEvent(SearchHnd, M_ADD_SEARCH_PATH, (GMessage::Param) new GString(Path));

			PostThreadEvent(SearchHnd, M_RECURSE);
		}
	}

	~MissingFiles()
	{
		if (SearchHnd >= 0)
			GEventSinkMap::Dispatch.CancelThread(SearchHnd);
		if (ExistsHnd >= 0)
			GEventSinkMap::Dispatch.CancelThread(ExistsHnd);
	}

	void OnReplace(const char *NewPath)
	{
		SearchResults *Sr = Files.First();
		if (!Sr) return;

		printf("%s:%i - Setting node to '%s'\n", _FL, Sr->Path.Get());
		Sr->Node->SetFileName(NewPath);

		Files.DeleteAt(0, true);
		delete Sr;

		OnFile();
	}

	void OnDelete()
	{
		SearchResults *Sr = Files.First();
		if (!Sr) return;

		Sr->Node->Delete();

		Files.DeleteAt(0, true);
		delete Sr;

		OnFile();
	}

	void OnFile()
	{
		bool Has = Files.Length() > 0;

		SetCtrlEnabled(IDC_MISSING, Has);
		SetCtrlEnabled(IDC_BROWSE, Has);
		SetCtrlEnabled(IDC_RESULTS, Has);
		Lst->Empty();

		if (Has)
		{
			SetCtrlName(IDC_MISSING, Files[0]->Path);

			for (unsigned i=0; i<Files[0]->Matches.Length(); i++)
			{
				LListItem *li = new LListItem;
				li->SetText(Files[0]->Matches[i]);
				Lst->Insert(li);
			}

			Lst->ResizeColumnsToContent();
		}
		else
		{
			SetCtrlName(IDC_MISSING, NULL);
		}
	}

	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDC_BROWSE:
			{
				GFileSelect s;
				GAutoString Dir = Proj->GetBasePath();
				s.Parent(this);
				s.InitialDir(Dir);
				if (s.Open())
				{
					OnReplace(s.Name());
				}
				break;
			}
			case IDC_DELETE:
			{
				OnDelete();
				break;
			}
			case IDC_RESULTS:
			{
				switch (Flags)
				{
					case GNotifyItem_DoubleClick:
					{
						LListItem *li = Lst->GetSelected();
						if (li)
						{
							OnReplace(li->GetText(0));
						}
						break;
					}
				}
				break;
			}
			case IDOK:
			{
				EndModal();
				break;
			}
		}

		return 0;
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_MISSING_FILE:
			{
				GAutoPtr<SearchResults> Sr((SearchResults*)Msg->A());
				if (Sr)
				{
					printf("Missing file '%s'\n", Sr->Path.Get());
					PostThreadEvent(SearchHnd, M_SEARCH, (GMessage::Param) Sr.Release());
				}
				break;
			}
			case M_RESULTS:
			{
				Files.Add((SearchResults*)Msg->A());
				OnFile();
				break;
			}
		}

		return GDialog::OnEvent(Msg);
	}
};

void FixMissingFilesDlg(IdeProject *Proj)
{
	MissingFiles Dlg(Proj);
	Dlg.DoModal();
}
