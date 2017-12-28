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
};

struct SearchResults
{
	GString Path;
	GString::Array Matches;
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
				GAutoPtr<GString> p((GString*)Msg->A());
				if (!FileExists(*p))
					PostObject(Hnd, M_MISSING_FILE, p);
				break;
			}
		}

		return 0;
	}
};

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
				Search.New() = *p;

				GArray<const char*> Ext;
				Ext.Add("*.h");
				Ext.Add("*.hpp");
				Ext.Add("*.c");
				Ext.Add("*.cpp");
				LgiRecursiveFileSearch(*p, &Ext, &Files);
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
	GAutoPtr<SearchResults> Matches;

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

			GArray<ProjectNode*> Nodes;
			if (Proj->GetAllNodes(Nodes))
			{
				for (unsigned i=0; i<Nodes.Length(); i++)
				{
					GString s = Nodes[i]->GetFullPath();
					if (s)
						PostThreadEvent(ExistsHnd, M_CHECK_FILE, (GMessage::Param) new GString(s));						
				}
			}

			List<IdeProject> Child;
			Proj->GetChildProjects(Child);
			Child.Add(Proj);
			for (IdeProject *p = Child.First(); p; p = Child.Next())
			{
				GString s = p->GetBasePath();
				if (s)
					PostThreadEvent(SearchHnd, M_ADD_SEARCH_PATH, (GMessage::Param) new GString(s));						
			}
		}
	}

	~MissingFiles()
	{
		if (SearchHnd >= 0)
			GEventSinkMap::Dispatch.CancelThread(SearchHnd);
		if (ExistsHnd >= 0)
			GEventSinkMap::Dispatch.CancelThread(ExistsHnd);
	}

	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDOK:
			case IDCANCEL:
			{
				EndModal(Ctrl->GetId() == IDOK);
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
				GAutoPtr<GString> p((GString*)Msg->A());
				SetCtrlEnabled(IDC_MISSING, true);
				SetCtrlEnabled(IDC_BROWSE, true);
				SetCtrlName(IDC_MISSING, *p);

				SearchResults *Sr = new SearchResults;
				Sr->Path = *p;
				PostThreadEvent(SearchHnd, M_SEARCH, (GMessage::Param) Sr);
				break;
			}
			case M_RESULTS:
			{
				Matches.Reset((SearchResults*)Msg->A());
				SetCtrlEnabled(IDC_RESULTS, true);
				for (unsigned i=0; i<Matches->Matches.Length(); i++)
				{
					LListItem *li = new LListItem;
					li->SetText(Matches->Matches[i]);
					Lst->Insert(li);
				}
				Lst->ResizeColumnsToContent();
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
