#include "Lgi.h"
#include "LgiIde.h"
#include "IdeProject.h"
#include "ProjectNode.h"
#include "AddFtpFile.h"
#include "GToken.h"
#include "GCombo.h"
#include "WebFldDlg.h"
#include "GClipBoard.h"
#include "GTableLayout.h"

//////////////////////////////////////////////////////////////////////////////////
class FileProps : public GDialog
{
public:
	NodeType Type;
	int Platforms;

	FileProps(GView *p, char *m, NodeType t, int plat)
	{
		Platforms = plat;
		Type = t;
		SetParent(p);
		if (LoadFromResource(IDD_FILE_PROPS))
		{
			MoveToCenter();
			SetCtrlName(IDC_MSG, m);
			
			GCombo *c;
			if (GetViewById(IDC_TYPE, c))
			{
				c->Insert("None");
				c->Insert("Folder");
				c->Insert("Source");
				c->Insert("Header");
				c->Insert("Dependency");
				c->Insert("Resource");
				c->Insert("Graphic");
				c->Insert("Text");
				c->Value(Type);
			}
			else LgiTrace("%s:%i - Failed to get Type combo.\n", _FL);

			for (int i=0; PlatformNames[i]; i++)
			{
				SetCtrlValue(PlatformCtrlId[i], ((1 << i) & Platforms) != 0);
			}
			
			OnPosChange();
			
			// Make sure the dialog can display the whole table...
			GTableLayout *t;
			if (GetViewById(IDC_TABLE, t))
			{
				GRect u = t->GetUsedArea();
				u.Size(-GTableLayout::CellSpacing, -GTableLayout::CellSpacing);
				GRect p = GetPos();
				if (u.X() < p.X() ||
					u.Y() < p.Y())
				{
					p.Dimension(max(u.X(), p.X()),
								max(u.Y(), p.Y()));
					SetPos(p);
				}				
			}
		}
	}
	
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDOK:
			{
				int t = GetCtrlValue(IDC_TYPE);
				if (t >= NodeSrc)
				{
					Type = (NodeType) t;
				}

				Platforms = 0;
				for (int i=0; PlatformNames[i]; i++)
				{
					if (GetCtrlValue(PlatformCtrlId[i]))
					{
						Platforms |= 1 << i;
					}
				}
				
				// fall thru
			}
			case IDCANCEL:
			case IDC_COPY_PATH:
			{
				EndModal(c->GetId());
				break;
			}
		}
		
		return 0;
	}
};

////////////////////////////////////////////////////////////////////
ProjectNode::ProjectNode(IdeProject *p) : IdeCommon(p)
{
	Platforms = PLATFORM_ALL;
	Type = NodeNone;
	File = 0;
	Name = 0;
	IgnoreExpand = false;
	Dep = 0;
	LocalCache = 0;
	Tag = NewStr("Node");
}

ProjectNode::~ProjectNode()
{
	if (File && Project)
		Project->OnNode(File, this, false);

	if (Dep)
	{
		DeleteObj(Dep);
	}

	DeleteArray(File);
	DeleteArray(Name);
	DeleteArray(LocalCache);
}

void ProjectNode::OpenLocalCache(IdeDoc *&Doc)
{
	if (LocalCache)
	{
		Doc = Project->GetApp()->OpenFile(LocalCache, this);
		if (Doc)
		{
			if (Doc && Doc->GetEdit())
			{
				Doc->SetProject(Project);
				
				IdeProjectSettings *Settings = Project->GetSettings();
				int i = Settings->GetInt(ProjEditorIndentSize);
				Doc->GetEdit()->SetIndentSize(i > 0 ? i : 4);
				i = Settings->GetInt(ProjEditorTabSize);
				Doc->GetEdit()->SetTabSize(i > 0 ? i : 4);
				Doc->GetEdit()->SetHardTabs(Settings->GetInt(ProjEditorUseHardTabs));
				Doc->GetEdit()->SetShowWhiteSpace(Settings->GetInt(ProjEditorShowWhiteSpace));
				Doc->GetEdit()->Invalidate();
			}
		}
		else
		{
			LgiMsg(Tree, "Couldn't open file '%s'", AppName, MB_OK, LocalCache);
		}
	}
}

void ProjectNode::OnCmdComplete(FtpCmd *Cmd)
{
	if (!Cmd)
		return;

	if (Cmd->Status && Cmd->File)
	{
		if (Cmd->Cmd == FtpRead)
		{
			DeleteArray(LocalCache);
			LocalCache = NewStr(Cmd->File);

			IdeDoc *Doc;
			OpenLocalCache(Doc);
		}
		else if (Cmd->Cmd == FtpWrite)
		{
			Cmd->View->OnSaveComplete(Cmd->Status);
		}
	}
}

int ProjectNode::GetPlatforms()
{
	return Platforms;
}

char *ProjectNode::GetLocalCache()
{
	return LocalCache;
}

bool ProjectNode::Load(GDocView *Edit, NodeView *Callback)
{
	bool Status = false;

	if (IsWeb())
	{
		if (LocalCache)
		{
			Status = Edit->Open(LocalCache);
		}
		else LgiAssert(!"No LocalCache");
	}
	else
	{
		GString Full = GetFullPath();
		Status = Edit->Open(Full);
	}
	
	return Status;
}

bool ProjectNode::Save(GDocView *Edit, NodeView *Callback)
{
	bool Status = false;

	if (IsWeb())
	{
		if (LocalCache)
		{
			if (Edit->Save(LocalCache))
			{
				FtpThread *t = GetFtpThread();
				if (t)
				{
					FtpCmd *c = new FtpCmd(FtpWrite, this);
					if (c)
					{
						c->View = Callback;
						c->Watch = Project->GetApp()->GetFtpLog();
						c->Uri = NewStr(File);
						c->File = NewStr(LocalCache);
						t->Post(c);
					}
				}
			}
			else LgiAssert(!"Editbox save failed.");
		}
		else LgiAssert(!"No LocalCache");
	}
	else
	{
		GString f = GetFullPath();
		Status = Edit->Save(f);

		if (Callback)
			Callback->OnSaveComplete(Status);
	}
	
	
	return Status;
}

bool ProjectNode::IsWeb()
{
	char *Www = GetAttr(OPT_Www);
	char *Ftp = GetAttr(OPT_Ftp);

	if
	(
		Www ||
		Ftp ||
		(File && strnicmp(File, "ftp://", 6) == 0)
	)
		return true;

	return false;
}

ProjectNode *ProjectNode::ChildNode()
{
	return dynamic_cast<ProjectNode*>(GetChild());
}

ProjectNode *ProjectNode::NextNode()
{
	return dynamic_cast<ProjectNode*>(GetNext());
}

void ProjectNode::AddNodes(GArray<ProjectNode*> &Nodes)
{
	Nodes.Add(this);
	for (ProjectNode *c = ChildNode(); c; c = c->NextNode())
	{
		c->AddNodes(Nodes);
	}
}

void ProjectNode::SetClean()
{
	if (Dep)
	{
		Dep->SetClean();
	}
	
	ForAllProjectNodes(p)
	{
		p->SetClean();
	}
}

IdeProject *ProjectNode::GetDep()
{
	return Dep;
}

IdeProject *ProjectNode::GetProject()
{
	return Project;
}

bool ProjectNode::GetFormats(List<char> &Formats)
{
	Formats.Insert(NewStr(NODE_DROP_FORMAT));
	return true;
}

bool ProjectNode::GetData(GVariant *Data, char *Format)
{
	if (Format && stricmp(Format, NODE_DROP_FORMAT) == 0)
	{
		void *t = this;
		Data->SetBinary(sizeof(this), (void*) &t);
		return true;
	}
	
	return false;
}

bool ProjectNode::OnBeginDrag(GMouse &m)
{
	return Drag(Tree, DROPEFFECT_MOVE);
}

char *ProjectNode::GetFileName()
{
	return File;
}

void ProjectNode::SetFileName(const char *f)
{
	char Rel[MAX_PATH];

	if (File && Project)
		Project->OnNode(File, this, false);

	DeleteArray(File);
	if (Project->RelativePath(Rel, f))
	{
		File = NewStr(Rel);
	}
	else
	{
		File = NewStr(f);
	}

	if (File)
	{
		char MimeType[256];

		GString FullPath = GetFullPath();

		if (Project)
			Project->OnNode(FullPath, this, true);

		if (FullPath)
		{
			if (LgiGetFileMimeType(FullPath, MimeType, sizeof(MimeType)) &&
				strnicmp(MimeType, "image/", 6) == 0)
			{
				Type = NodeGraphic;
			}
		}
		
		if (!Type)
		{
			char *Ext = LgiGetExtension(File);

			if ((Ext && stricmp(Ext, "h") == 0) ||
				stristr(File, "makefile") != 0)
			{
				Type = NodeHeader;
			}
			else if (Ext)
			{
				if (stricmp(Ext, "lr8") == 0)
				{
					Type = NodeResources;
				}
				else if (stricmp(Ext, "cpp") == 0 ||
						stricmp(Ext, "c") == 0 ||
						stricmp(Ext, "h") == 0)
				{
					Type = NodeSrc;
				}
				else if (stricmp(Ext, "php") == 0 ||
						 stricmp(Ext, "asp") == 0 ||
						 stricmp(Ext, "html") == 0 ||
						 stricmp(Ext, "htm") == 0 ||
						 stricmp(Ext, "css") == 0)
				{
					Type = NodeWeb;
				}
				else if (stricmp(Ext, "txt") == 0)
				{
					Type = NodeText;
				}
			}
		}
	}
	
	Project->SetDirty();
}

char *ProjectNode::GetName()
{
	return Name;
}

void ProjectNode::SetName(const char *f)
{
	DeleteArray(Name);
	Name = NewStr(f);
	Type = NodeDir;
}

NodeType ProjectNode::GetType()
{
	return Type;
}

void ProjectNode::SetType(NodeType t)
{
	Type = t;
}

int ProjectNode::GetImage(int f)
{
	if (IsWeb())
	{
		return File ? ICON_SOURCE : ICON_WEB;
	}

	switch (Type)
	{
		default:
			break;
		case NodeSrc:
			return ICON_SOURCE;
		case NodeNone:
		case NodeHeader:
		case NodeWeb:
			return ICON_HEADER;
		case NodeDependancy:
			return ICON_DEPENDANCY;
		case NodeGraphic:
			return ICON_GRAPHIC;
		case NodeResources:
			return ICON_RESOURCE;
	}

	return ICON_FOLDER;
}

char *ProjectNode::GetText(int c)
{
	if (File)
	{
		char *d = 0;

		if (IsWeb())
		{
			d = File ? strrchr(File, '/') : 0;
		}
		else
		{
			#ifdef WIN32
			char Other = '/';
			#else
			char Other = '\\';
			#endif
			char *s;
			while ((s = strchr(File, Other)))
			{
				*s = DIR_CHAR;
			}
			
			d = strrchr(File, DIR_CHAR);
		}
		
		if (d) return d + 1;
		else return File;
	}

	return Name ? Name : (char*)Untitled;
}

void ProjectNode::OnExpand(bool b)
{
	if (!IgnoreExpand)
	{
		Project->SetDirty();
	}
}

bool ProjectNode::Serialize()
{
	if (!Write && File)
		Project->OnNode(File, this, false);

	SerializeAttr("File", File);
	
	if (!Write && File)
		Project->OnNode(File, this, true);

	
	SerializeAttr("Name", Name);
	SerializeAttr("Type", (int&)Type);
	SerializeAttr("Platforms", (int&)Platforms);

	char *Ext = LgiGetExtension(File);

	if (Ext)
	{
		if (stricmp(Ext, "cpp") == 0 ||
			stricmp(Ext, "c") == 0)
		{
			Type = NodeSrc;
		}
		else if (stricmp(Ext, "h") == 0)
		{
			Type = NodeHeader;
		}
		else if (stricmp(Ext, "lr8") == 0)
		{
			Type = NodeResources;
		}			
	}

	if (Type == NodeDir)
	{
		int Open = Expanded();
		SerializeAttr("Open", Open);
		if (!Write)
		{
			IgnoreExpand = true;
			Expanded(Open);
			IgnoreExpand = false;
		}
	}
	else if (Type == NodeDependancy)
	{
		if (!Write)
		{
			GString Full = GetFullPath();				
			Dep = Project->GetApp()->OpenProject(Full, Project, false, true);
		}
	}
	else
	{
		if (!Write)
		{
			// Check that file exists.
			GString p = GetFullPath();
			if (p)
			{
				if (FileExists(p))
				{
					if (!LgiIsRelativePath(File))
					{
						// Try and fix up any non-relative paths that have crept in...
						char Rel[MAX_PATH];
						if (Project->RelativePath(Rel, File))
						{
							if (File)
								Project->OnNode(File, this, false);

							DeleteArray(File);
							File = NewStr(Rel);
							Project->SetDirty();

							Project->OnNode(File, this, true);
						}
					}
				}
				else
				{
					// File doesn't exist... has it moved???
					GAutoString Path = Project->GetBasePath();
					if (Path)
					{
						// Find the file.
						char *d = strrchr(p, DIR_CHAR);
						GArray<char*> Files;
						GArray<const char*> Ext;
						Ext.Add(d ? d + 1 : p.Get());
						if (LgiRecursiveFileSearch(Path, &Ext, &Files))
						{
							if (Files.Length())
							{
								GStringPipe Buf;
								Buf.Print(	"The file:\n"
											"\n"
											"\t%s\n"
											"\n"
											"doesn't exist. Is this the right file:\n"
											"\n"
											"\t%s",
											p.Get(),
											Files[0]);
								char *Msg = Buf.NewStr();
								if (Msg)
								{
									GAlert a(Project->GetApp(), "Missing File", Msg, "Yes", "No", "Browse...");
									switch (a.DoModal())
									{
										case 1: // Yes
										{
											SetFileName(Files[0]);
											break;
										}
										case 2: // No
										{
											break;
										}
										case 3: // Browse
										{
											GFileSelect s;
											s.Parent(Project->GetApp());
											s.Type("Code", SourcePatterns);
											if (s.Open())
											{
												SetFileName(s.Name());
											}
											break;
										}
									}
									
									DeleteArray(Msg);
								}
							}
							else
							{
								GStringPipe Buf;
								Buf.Print(	"The file:\n"
											"\n"
											"\t%s\n"
											"\n"
											"doesn't exist.",
											p.Get());
								char *Msg = Buf.NewStr();
								if (Msg)
								{
									GAlert a(Project->GetApp(), "Missing File", Msg, "Skip", "Delete", "Browse...");
									switch (a.DoModal())
									{
										case 1: // Skip
										{
											break;
										}
										case 2: // Delete
										{
											Project->SetDirty();
											delete this;
											return false;
											break;
										}
										case 3: // Browse
										{
											GFileSelect s;
											s.Parent(Project->GetApp());
											s.Type("Code", SourcePatterns);
											if (s.Open())
											{
												SetFileName(s.Name());
											}
											break;
										}
									}
									
									DeleteArray(Msg);
								}
							}
						}
					}
					else
					{
						LgiTrace("%s:%i - Project::GetBasePath failed.\n", _FL);
					}
				}
			}
		}
	}

	return true;
}

GString ProjectNode::GetFullPath()
{
	GString FullPath;
	
	if (LgiIsRelativePath(File))
	{
		// Relative path
		GAutoString Path = Project->GetBasePath();
		if (Path)
		{
			char p[MAX_PATH];
			LgiMakePath(p, sizeof(p), Path, File);
			FullPath = p;
		}
	}
	else
	{
		// Absolute path
		FullPath = File;
	}
	
	return FullPath;
}

IdeDoc *ProjectNode::Open()
{
	static bool Processing = false;
	IdeDoc *Doc = 0;

	if (Processing)
	{
		LgiAssert(!"Not recursive");
	}

	if (!Processing)
	{
		Processing = true;

		if (IsWeb())
		{
			if (File)
			{
				if (LocalCache)
				{
					OpenLocalCache(Doc);
				}
				else
				{
					FtpThread *t = GetFtpThread();
					if (t)
					{
						FtpCmd *c = new FtpCmd(FtpRead, this);
						if (c)
						{
							c->Watch = Project->GetApp()->GetFtpLog();
							c->Uri = NewStr(File);
							t->Post(c);
						}
					}
				}
			}
		}
		else
		{		
			switch (Type)
			{
				case NodeDir:
				{
					Expanded(true);
					break;
				}
				case NodeResources:
				{
					GString FullPath = GetFullPath();
					if (FullPath)
					{
						char Exe[256];
						LgiGetExePath(Exe, sizeof(Exe));
						LgiTrimDir(Exe);
						#if defined WIN32
						LgiMakePath(Exe, sizeof(Exe), Exe, "Debug\\LgiRes.exe");
						#elif defined LINUX
						LgiMakePath(Exe, sizeof(Exe), Exe, "LgiRes/lgires");
						#endif
						
						if (FileExists(Exe))
						{
							LgiExecute(Exe, FullPath);
						}
					}
					else
					{
						LgiMsg(Tree, "No Path", AppName);
					}
					break;
				}
				case NodeGraphic:
				{
					GString FullPath = GetFullPath();
					if (FullPath)
					{
						LgiExecute(FullPath);
					}
					else
					{
						LgiMsg(Tree, "No Path", AppName);
					}
					break;
				}
				default:
				{
					GString FullPath = GetFullPath();
					if (FullPath)
					{
						Doc = Project->GetApp()->FindOpenFile(FullPath);
						if (!Doc)
						{
							Doc = Project->GetApp()->NewDocWnd(0, this);
							if (Doc && Doc->GetEdit())
							{
								if (Doc->GetEdit()->Open(FullPath))
								{
									Doc->SetProject(Project);
									
									IdeProjectSettings *Settings = Project->GetSettings();
									int i = Settings->GetInt(ProjEditorIndentSize);
									Doc->GetEdit()->SetIndentSize(i > 0 ? i : 4);
									i = Settings->GetInt(ProjEditorTabSize);
									Doc->GetEdit()->SetTabSize(i > 0 ? i : 4);
									Doc->GetEdit()->SetHardTabs(Settings->GetInt(ProjEditorUseHardTabs));
									Doc->GetEdit()->SetShowWhiteSpace(Settings->GetInt(ProjEditorShowWhiteSpace));

									Doc->GetEdit()->Invalidate();
								}
								else
								{
									LgiMsg(Tree, "Couldn't open file '%s'", AppName, MB_OK, FullPath.Get());
								}
							}
						}
						else
						{
							Doc->Raise();
							Doc->Focus(true);
						}
					}
					else
					{
						LgiMsg(Tree, "No Path", AppName);
					}
					break;
				}
			}
		}

		Processing = false;
	}
	
	return Doc;
}

void ProjectNode::Delete()
{
	if (nView)
	{
		nView->OnDelete();
	}

	Project->SetDirty();
	GXmlTag::RemoveTag();							
	delete this;
}

bool ProjectNode::OnKey(GKey &k)
{
	if (k.Down() && k.IsChar)
	{
		if (k.vkey == VK_RETURN)
		{
			Open();
			return true;
		}
		else if (k.vkey == VK_DELETE)
		{
			GTreeItem *s = GetNext();
			if (s || (s = GetParent()))
			{
				s->Select(true);
			}
			
			Delete();
			return true;
		}
	}
	
	return false;
}

void ProjectNode::OnMouseClick(GMouse &m)
{
	GTreeItem::OnMouseClick(m);
	
	if (m.IsContextMenu())
	{
		GSubMenu Sub;

		Select(true);

		if (Type == NodeDir)
		{
			if (IsWeb())
			{
				Sub.AppendItem("Insert FTP File", IDM_INSERT_FTP, true);
			}
			else
			{
				Sub.AppendItem("Insert File", IDM_INSERT, true);
			}
			
			Sub.AppendItem("New Folder", IDM_NEW_FOLDER, true);
			Sub.AppendItem("Import Folder", IDM_IMPORT_FOLDER, true);
			Sub.AppendSeparator();

			if (!IsWeb())
			{
				Sub.AppendItem("Rename", IDM_RENAME, true);
			}
		}
		Sub.AppendItem("Remove", IDM_DELETE, true);
		Sub.AppendItem("Sort", IDM_SORT_CHILDREN, true);
		Sub.AppendSeparator();
		Sub.AppendItem("Browse Folder", IDM_BROWSE_FOLDER, ValidStr(File));
		Sub.AppendItem("Open Terminal", IDM_OPEN_TERM, ValidStr(File));
		Sub.AppendItem("Properties", IDM_PROPERTIES, true);

		m.ToScreen();
		GdcPt2 c = _ScrollPos();
		m.x -= c.x;
		m.y -= c.y;
		switch (Sub.Float(Tree, m.x, m.y))
		{
			case IDM_INSERT_FTP:
			{
				AddFtpFile Add(Tree, GetAttr(OPT_Ftp));
				if (Add.DoModal())
				{
					for (int i=0; i<Add.Uris.Length(); i++)
					{
						ProjectNode *New = new ProjectNode(Project);
						if (New)
						{
							New->SetFileName(Add.Uris[i]);
							InsertTag(New);
							SortChildren();
							Project->SetDirty();
						}
					}
				}
				break;
			}
			case IDM_INSERT:
			{
				GFileSelect s;
				s.Parent(Tree);
				s.Type("Source", SourcePatterns);
				s.Type("Makefiles", "*makefile");
				s.Type("All Files", LGI_ALL_FILES);
				s.MultiSelect(true);

				GAutoString Dir = Project->GetBasePath();
				if (Dir)
				{
					s.InitialDir(Dir);
				}

				if (s.Open())
				{
					for (int i=0; i<s.Length(); i++)
					{
						if (!Project->InProject(s[i], false))
						{
							ProjectNode *New = new ProjectNode(Project);
							if (New)
							{
								New->SetFileName(s[i]);
								InsertTag(New);
								SortChildren();
								Project->SetDirty();
							}
						}
						else
						{
							LgiMsg(Tree, "'%s' is already in the project.\n", AppName, MB_OK, s[i]);
						}
					}
				}
				break;
			}
			case IDM_IMPORT_FOLDER:
			{
				GFileSelect s;
				s.Parent(Tree);

				GAutoString Dir = Project->GetBasePath();
				if (Dir)
				{
					s.InitialDir(Dir);
				}

				if (s.OpenFolder())
				{
					GArray<char*> Files;
					GArray<const char*> Ext;
					GToken e(SourcePatterns, ";");
					for (int i=0; i<e.Length(); i++)
					{
						Ext.Add(e[i]);
					}
					
					if (LgiRecursiveFileSearch(s.Name(), &Ext, &Files))
					{
						int Start = strlen(s.Name()) + 1;
						for (int i=0; i<Files.Length(); i++)
						{
							char *f = Files[i];
							GToken p(f + Start, DIR_STR);
							ProjectNode *Insert = this;
							
							// Find sub nodes, and drill into directory heirarchy,
							// creating the nodes if needed.
							for (int i=0; Insert && i<p.Length()-1; i++)
							{
								// Find existing node...
								bool Found = false;
								for (ProjectNode *c=Insert->ChildNode(); c; c=c->NextNode())
								{
									if (c->GetType() == NodeDir &&
										c->GetName() &&
										stricmp(c->GetName(), p[i]) == 0)
									{
										Insert = c;
										Found = true;
										break;
									}
								}
								
								if (!Found)
								{
									// Create the node
									IdeCommon *Com = Insert->GetSubFolder(Project, p[i], true);												
									Insert = dynamic_cast<ProjectNode*>(Com);
									LgiAssert(Insert);
								}
							}
							
							// Insert the file into the tree...
							if (Insert)
							{
								ProjectNode *New = new ProjectNode(Project);
								if (New)
								{
									New->SetFileName(f);
									Insert->InsertTag(New);
									Insert->SortChildren();
									Project->SetDirty();
								}
							}
						}
						
						Files.DeleteArrays();
					}														
				}
				break;
			}
			case IDM_SORT_CHILDREN:
			{
				SortChildren();
				Project->SetDirty();
				break;
			}
			case IDM_NEW_FOLDER:
			{
				GInput Name(Tree, "", "Name:", AppName);
				if (Name.DoModal())
				{
					GetSubFolder(Project, Name.Str, true);
				}
				break;
			}
			case IDM_RENAME:
			{
				GInput Name(Tree, "", "Name:", AppName);
				if (Name.DoModal())
				{
					SetName(Name.Str);
					Project->SetDirty();
					Update();
				}
				break;
			}
			case IDM_DELETE:
			{
				Delete();
				return;
				break;
			}
			case IDM_BROWSE_FOLDER:
			{
				GString Path = GetFullPath();
				if (Path)
					LgiBrowseToFile(Path);
				break;
			}
			case IDM_OPEN_TERM:
			{
				if (Type == NodeDir)
				{
				}
				else
				{
					GString Path = GetFullPath();
					if (Path)
					{
						LgiTrimDir(Path);
						
						#if defined LINUX
						
						char *Term = 0;
						char *Format = 0;
						switch (LgiGetWindowManager())
						{
							case WM_Kde:
								Term = "konsole";
								Format = "--workdir ";
								break;
							case WM_Gnome:
								Term = "gnome-terminal";
								Format = "--working-directory=";
								break;
						}
						
						if (Term)
						{
							char s[256];
							strcpy_s(s, sizeof(s), Format);
							
							char *o = s + strlen(s), *i = Path;
							*o++ = '\"';
							while (*i)
							{
								if (*i == ' ')
								{
									*o++ = '\\';
								}
								*o++ = *i++;
							}
							*o++ = '\"';
							*o++ = 0;
							LgiExecute(Term, s);
						}
						
						#elif defined WIN32
						
						LgiExecute("cmd", 0, Path);
						
						#endif
					}
				}
				break;
			}
			case IDM_PROPERTIES:
			{
				OnProperties();
				break;
			}
		}
	}
	else if (m.Left())
	{
		if (Type != NodeDir && m.Double())
		{
			if
			(
				(
					IsWeb()
					||
					Type == NodeSrc
					||
					Type == NodeHeader
					||
					Type == NodeResources
					||
					Type == NodeGraphic
					||
					Type == NodeWeb
					||
					Type == NodeText
				)
				&&
				ValidStr(File)
			)
			{
				Open();
			}
			else
			{
				LgiAssert(!"Unknown file type");
			}
		}
	}
}

ProjectNode *ProjectNode::FindFile(const char *In, char **Full)
{
	if (File)
	{
		bool Match = false;

		if (IsWeb())
		{
			Match = File ? stricmp(In, File) == 0 : false;
		}
		else if (strchr(In, DIR_CHAR))
		{
			// Match partial or full path
			char Full[MAX_PATH] = "";
			
			if (LgiIsRelativePath(File))
			{
				GAutoString Base = Project->GetBasePath();
				if (Base)
					LgiMakePath(Full, sizeof(Full), Base, File);
				else
					LgiTrace("%s,%i - Couldn't get full IDoc path.\n", __FILE__, __LINE__);
			}
			else
			{
				strcpy_s(Full, sizeof(Full), File);
			}
			
			GString MyPath(Full);
			GString::Array MyArr = MyPath.Split(DIR_STR);
			GString InPath(In);
			GString::Array InArr = InPath.Split(DIR_STR);
			int Common = min(MyArr.Length(), InArr.Length());
			Match = true;
			for (int i = 0; i < Common; i++)
			{
				char *a = MyArr[MyArr.Length()-(i+1)];
				char *b = InArr[InArr.Length()-(i+1)];
				if (_stricmp(a, b))
				{
					Match = false;
					break;
				}
			}
		}
		else
		{
			// Match file name only
			char *Dir = strrchr(File, DIR_CHAR);
			if (Dir)
			{
				Match = stricmp(Dir + 1, In) == 0;
			}
		}
		
		if (Match)
		{
			if (Full)
			{
				if (File[0] == '.')
				{
					GAutoString Base = Project->GetBasePath();
					if (Base)
					{
						char f[256];
						LgiMakePath(f, sizeof(f), Base, File);
						*Full = NewStr(f);
					}
				}
				else
				{
					*Full = NewStr(File);
				}
			}
			
			return this;
		}
	}
	
	ForAllProjectNodes(c)
	{
		ProjectNode *n = c->FindFile(In, Full);
		if (n)
		{
			return n;
		}
	}
	
	return 0;
}

void ProjectNode::OnProperties()
{
	if (IsWeb())
	{
		bool IsFolder = File == 0;

		WebFldDlg Dlg(Tree, Name, IsFolder ? GetAttr(OPT_Ftp) : File, GetAttr(OPT_Www));
		if (Dlg.DoModal())
		{
			if (IsFolder)
			{
				SetName(Dlg.Name);
				SetAttr(OPT_Ftp, Dlg.Ftp);
				SetAttr(OPT_Www, Dlg.Www);
			}
			else
			{
				DeleteArray(File);
				File = NewStr(Dlg.Ftp);
			}

			Project->SetDirty();
			Update();
		}
	}
	else if (Type == NodeDir)
	{
	}
	else if (Type == NodeDependancy)
	{
		GString Path = GetFullPath();
		LgiMsg(Tree, "Path: %s", AppName, MB_OK, Path.Get());
	}
	else
	{
		GString Path = GetFullPath();
		if (Path)
		{
			char Size[32];
			int64 FSize = LgiFileSize(Path);
			LgiFormatSize(Size, sizeof(Size), FSize);
			char Msg[512];
			sprintf(Msg, "Source Code:\n\n\t%s\n\nSize: %s (%i bytes)", Path.Get(), Size, (int32)FSize);
		
			FileProps Dlg(Tree, Msg, Type, Platforms);
			switch (Dlg.DoModal())
			{
				case IDOK:
				{
					if (Type != Dlg.Type)
					{
						Type = Dlg.Type;
						Project->SetDirty();
					}
					if (Platforms != Dlg.Platforms)
					{
						Platforms = Dlg.Platforms;
						Project->SetDirty();
					}
					
					Update();
					break;
				}
				case IDC_COPY_PATH:
				{
					GClipBoard Clip(Tree);
					Clip.Text(Path);
					break;
				}
			}
		}
	}
}
