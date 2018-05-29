#include "Lgi.h"
#include "LgiIde.h"
#include "resdefs.h"
#include "ProjectNode.h"

//////////////////////////////////////////////////////////////////////////////////
IdeCommon::IdeCommon(IdeProject *p)
{
	Project = p;
}

IdeCommon::~IdeCommon()
{
	Remove();
}

bool IdeCommon::OnOpen(GProgressDlg *Prog, GXmlTag *Src)
{
	if (Prog)
		Prog->Value(Prog->Value() + 1);

	Copy(*Src);
	if (!Serialize(Write = false))
		return false;

	List<GXmlTag>::I it = Src->Children.begin();
	for (GXmlTag *c = *it; c && (!Prog || !Prog->IsCancelled()); c = *++it)
	{
		bool Processed = false;
		if (c->IsTag("Node"))
		{
			ProjectNode *pn = new ProjectNode(Project);
			if (pn && (Processed = pn->OnOpen(Prog, c)))
				InsertTag(pn);
		}
		else if (!c->IsTag("Settings"))
			return false;
		if (!Processed && Prog)
			Prog->Value(Prog->Value() + 1);
	}
	
	return true;
}

void IdeCommon::CollectAllSubProjects(List<IdeProject> &c)
{
	ForAllProjectNodes(p)
	{
		if (p->GetType() == NodeDependancy)
		{
			if (p->GetDep())
				c.Insert(p->GetDep());
		}
		
		p->CollectAllSubProjects(c);
	}
}

IdePlatform GetCurrentPlatform()
{
	#if defined(WIN32)
	return PlatformWin32;
	#elif defined(LINUX)
	return PlatformLinux;
	#elif defined(MAC)
	return PlatformMac;
	#elif defined(BEOS)
	return PlatformHaiku;
	#else
	#error "Not impl."
	#endif
}

void IdeCommon::CollectAllSource(GArray<GString> &c, IdePlatform Platform)
{
	ForAllProjectNodes(p)
	{
		switch (p->GetType())
		{
			case NodeSrc:
			case NodeHeader:
			{
				if (Platform == PlatformCurrent)
					Platform = GetCurrentPlatform();
					
				int Flags = p->GetPlatforms();
				if (Flags & (1 << Platform))
				{
					GString path = p->GetFullPath();
					if (path)
					{
						c.Add(path);
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
		
		p->CollectAllSource(c, Platform);
	}
}

void IdeCommon::SortChildren()
{
	Items.Sort(NodeSort, 0);
	Children.Sort(XmlSort, 0);
	
	if (Tree)
	{
		_RePour();
		Tree->Invalidate();
	}
}

void IdeCommon::InsertTag(GXmlTag *t)
{
	GXmlTag::InsertTag(t);

	GTreeItem *i = dynamic_cast<GTreeItem*>(t);
	if (i)
	{
		Insert(i);
	}
}

void IdeCommon::RemoveTag()
{
	GXmlTag::RemoveTag();
	Detach();
}

bool IdeCommon::AddFiles(AddFilesProgress *Prog, const char *Path)
{
	bool IsDir = DirExists(Path);
	if (IsDir)
	{
		GString s = Path;
		GString::Array a = s.Split(DIR_STR);
		IdeCommon *Sub = GetSubFolder(Project, a.Last(), true);
		if (Sub)
		{
			GDirectory d;
			for (int b = d.First(Path); b && !Prog->Cancel; b = d.Next())
			{
				char p[MAX_PATH];
				if (d.Path(p, sizeof(p)))
				{
					char *Name = d.GetName();
					char *Ext = LgiGetExtension(p);
					if
					(
						(
							d.IsDir()
							||
							Prog->Exts.Find(Ext ? Ext : Name)
						)
						&&
						Name != NULL
						&&
						Name[0] != '.'
					)
					{
						Sub->AddFiles(Prog, p);
					}
				}
			}
			
			// If the folder is empty then just delete...
			if (Sub->GetChild() == NULL)
			{
				Sub->Remove();
				DeleteObj(Sub);
			}
			else
			{
				Sub->SortChildren();
			}
		}
		else LgiTrace("%s:%i - GetSubFolder failed\n", _FL);
	}
	else
	{
		// GProfile p("IdeCommon::AddFiles");
		// p.HideResultsIfBelow(50);
		if (!Project->InProject(false, Path, false))
		{
			// p.Add("new node");
			ProjectNode *New = new ProjectNode(Project);
			if (New)
			{
				// p.Add("set filename");
				New->SetFileName(Path);
				// p.Add("insert");
				InsertTag(New);
				// p.Add("set dirty");
				Project->SetDirty();

				if (Prog)
					Prog->Value(Prog->Value() + 1);
				
				return true;
			}
		}
	}
	
	return false;
}

IdeCommon *IdeCommon::GetSubFolder(IdeProject *Project, char *Name, bool Create)
{
	if (Name)
	{
		ForAllProjectNodes(c)
		{
			if (c->GetType() == NodeDir)
			{
				if (c->GetName() && stricmp(c->GetName(), Name) == 0)
				{
					return c;
				}
			}
		}

		ProjectNode *New = new ProjectNode(Project);
		if (New)
		{
			New->SetName(Name);
			InsertTag(New);
			Project->SetDirty();
			return New;
		}
	}

	return 0;
}
