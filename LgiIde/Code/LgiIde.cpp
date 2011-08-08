#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GMdi.h"
#include "GToken.h"
#include "GXmlTree.h"
#include "GPanel.h"
#include "GProcess.h"
#include "SpaceTabConv.h"
#include "GButton.h"
#include "GTabView.h"
#include "FtpThread.h"
#include "GClipBoard.h"

#define IDM_SAVE				102
#define IDM_BREAKPOINT			313
#define IDM_RESTART				314
#define IDM_KILL				315
#define IDM_STEP_INTO			316
#define IDM_STEP_OVER			317
#define IDM_STEP_OUT			318
#define IDM_RUN_TO				319

#define IDM_RECENT_FILE			1000
#define IDM_RECENT_PROJECT		1100
#define IDM_WINDOWS				1200


//////////////////////////////////////////////////////////////////////////////////////////
char AppName[] = "LgiIde";

char *dirchar(char *s, bool rev = false)
{
	if (rev)
	{
		char *last = 0;
		while (s AND *s)
		{
			if (*s == '/' OR *s == '\\')
				last = s;
			s++;
		}
		return last;
	}
	else
	{
		while (s AND *s)
		{
			if (*s == '/' OR *s == '\\')
				return s;
			s++;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
class Dependency : public GTreeItem
{
	char *File;
	bool Loaded;
	GTreeItem *Fake;

public:
	Dependency(char *file)
	{
		File = NewStr(file);
		char *d = strrchr(File, DIR_CHAR);
		Loaded = false;
		Insert(Fake = new GTreeItem);

		if (FileExists(File))
		{
			SetText(d?d+1:File);
		}
		else
		{
			char s[256];
			sprintf(s, "%s (missing)", d?d+1:File);
			SetText(s);
		}
	}
	
	~Dependency()
	{
		DeleteArray(File);
	}
	
	void Copy(GStringPipe &p, int Depth = 0)
	{
		{
			char s[1024];
			ZeroObj(s);
			memset(s, ' ', Depth * 4);
			sprintf(s+(Depth*4), "[%c] %s\n", Expanded() ? '-' : '+', GetText(0));
			p.Push(s);
		}
		
		if (Loaded)
		{
			for (GTreeItem *i=GetChild(); i; i=i->GetNext())
			{
				((Dependency*)i)->Copy(p, Depth+1);
			}
		}
	}
	
	char *Find(char *Paths, char *e)
	{
		GToken Path(Paths, LGI_PATH_SEPARATOR);
		for (int p=0; p<Path.Length(); p++)
		{
			char Full[256];
			LgiMakePath(Full, sizeof(Full), Path[p], e);
			if (FileExists(Full))
			{
				return NewStr(Full);
			}
		}
		
		return 0;
	}
	
	char *Find(char *e)
	{
		char *s = Find("/lib:/usr/X11R6/lib:/usr/lib", e);
		if (s) return s;

		char *Env = getenv("LD_LIBRARY_PATH");
		if (Env)
		{
			s = Find(Env, e);
			if (s) return s;
		}
		
		return 0;
	}

	void OnExpand(bool b)
	{
		if (b AND !Loaded)
		{
			Loaded = true;
			DeleteObj(Fake);
			
			GStringPipe Out;
			char Args[256];
			sprintf(Args, "-d %s", File);
			
			GProcess p;
			if (p.Run("readelf", Args, 0, true, 0, &Out))
			{
				char *o = Out.NewStr();
				// printf("o=%s\n", o);
				if (o)
				{
					GToken t(o, "\r\n");
					for (int i=0; i<t.Length(); i++)
					{
						char *SharedLib = stristr(t[i], "Shared library:");
						if (SharedLib AND
							(SharedLib = strchr(SharedLib, '[')) != 0)
						{
							char *e = strchr(++SharedLib, ']');
							if (e)
							{
								*e++ = 0;

								char *Dep = Find(SharedLib);
								Insert(new Dependency(Dep?Dep:SharedLib));
								DeleteArray(Dep);
							}
						}
					}
					
					DeleteArray(o);
				}
			}
			else
			{
				LgiMsg(Tree, "Couldn't read executable.\n", AppName);
			}
		}
	}
};

#define IDC_COPY 100
class Depends : public GDialog
{
	Dependency *Root;

public:
	Depends(GView *Parent, char *File)
	{
		Root = 0;
		SetParent(Parent);
		GRect r(0, 0, 600, 700);
		SetPos(r);
		MoveToCenter();
		Name("Dependencies");
		
		GTree *t = new GTree(100, 10, 10, r.X() - 20, r.Y() - 50);
		if (t)
		{
			t->Sunken(true);
			t->Insert(Root = new Dependency(File));
			if (Root)
			{
				Root->Expanded(true);
			}
			
			Children.Insert(t);
			Children.Insert(new GButton(IDC_COPY, 10, t->GView::GetPos().y2 + 10, 60, 20, "Copy"));
			Children.Insert(new GButton(IDOK, 80, t->GView::GetPos().y2 + 10, 60, 20, "Ok"));
		}
		
		
		DoModal();
	}
	
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDC_COPY:
			{
				if (Root)
				{
					GStringPipe p;
					Root->Copy(p);
					char *s = p.NewStr();
					if (s)
					{
						GClipBoard c(this);
						c.Text(s);
						DeleteArray(s);
					}
					break;
				}
				break;
			}			
			case IDOK:
			{
				EndModal(0);
				break;
			}
		}
		
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
class IdeOutput : public GPanel
{
public:
	GTabView *Tab;
	GTabPage *Build;
	GTabPage *Debug;
	GTabPage *Find;
	GTabPage *Ftp;
	GList *FtpLog;
	GTextView3 *Txt[3];
	GArray<char> Buf[3];

	IdeOutput() : GPanel("Panel", 200, true)
	{
		Build = Debug = Find = Ftp = 0;
		FtpLog = 0;

		Alignment(GV_EDGE_BOTTOM);
		Children.Insert(Tab=new GTabView(100, 18, 3, 200, 200, "Output"));
		if (Tab)
		{
			Build = Tab->Append("Build");
			Debug = Tab->Append("Output");
			Find = Tab->Append("Find");
			Ftp = Tab->Append("Ftp");
			
			if (Build)
				Build->Append(Txt[0] = new GTextView3(101, 0, 0, 100, 100));
			if (Debug)
				Debug->Append(Txt[1] = new GTextView3(102, 0, 0, 100, 100));
			if (Find)
				Find->Append(Txt[2] = new GTextView3(103, 0, 0, 100, 100));
			if (Ftp)
				Ftp->Append(FtpLog = new GList(104, 0, 0, 100, 100));

			if (FtpLog)
			{
				FtpLog->SetPourLargest(true);
				FtpLog->Sunken(true);
				FtpLog->AddColumn("Entry", 1000);
				FtpLog->ShowColumnHeader(false);
			}

			for (int n=0; n<CountOf(Txt); n++)
			{
				Txt[n]->SetTabSize(8);
				Txt[n]->Sunken(true);
				
				/*
				GFontType Type;
				if (Type.GetSystemFont("Fixed"))
				{
					Type.SetPointSize(11);
					
					GFont *f = Type.Create();
					if (f)
					{
						Txt[n]->SetFont(f, true);
					}
				}
				*/
			}
		}
	}

	~IdeOutput()
	{
	}

	void OnCreate()
	{
		SetPulse(1000);
	}
	
	void OnPulse()
	{
		for (int Channel = 0; Channel<CountOf(Buf); Channel++)
		{
			int64 Size = Buf[Channel].Length();
			if (Size)
			{
				char *Utf = &Buf[Channel][0];
				if (!LgiIsUtf8(Utf, Size))
				{
					printf("Ch %i not utf len=%i\n", Channel, Size);
					continue;
				}
				
				GAutoPtr<char16, true> w(LgiNewUtf8To16(Utf, Size));
				char16 *OldText = Txt[Channel]->NameW();
				int OldLen = 0;
				if (OldText)
					OldLen = StrlenW(OldText);

				int Cur = Txt[Channel]->GetCursor();
				Txt[Channel]->Insert(OldLen, w, StrlenW(w));
				if (Cur > OldLen - 1)
				{
					Txt[Channel]->SetCursor(OldLen + StrlenW(w), false);
				}
				Tab->Value(Channel);
				Buf[Channel].Length(0);
			}
		}
	}

	void OnPosChange()
	{
		printf("panel onposchange %s\n", GetPos().GetStr());
		GPanel::OnPosChange();
		if (Tab)
		{
			GRect p = Tab->GetPos();
			p.x2 = X() - 3;
			p.y2 = Y() - 3;
			Tab->SetPos(p);
			
			GRect c = Tab->GetTabClient();
			c.Offset(-c.x1, -c.y1);
			p.Size(3, 3);

			for (int n=0; n<CountOf(Txt); n++)
			{
				if (Txt[n])
					Txt[n]->SetPos(c);
			}

			FtpLog->SetPos(c);
		}
	}
};

int DocSorter(IdeDoc *a, IdeDoc *b, int d)
{
	char *A = a->GetFileName();
	char *B = b->GetFileName();
	if (A AND B)
	{
		char *Af = strrchr(A, DIR_CHAR);
		char *Bf = strrchr(B, DIR_CHAR);
		return stricmp(Af?Af+1:A, Bf?Bf+1:B);
	}
	
	return 0;
}

/*
class Options : public GDialog
{
	AppWnd *App;
	
public:
	Options(AppWnd *a)
	{
		SetParent(App = a);
		if (LoadFromResource(IDD_OPTIONS))
		{
			MoveToCenter();
		}
	}

	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDOK:
			case IDCANCEL:
			{
				EndModal(c->GetId() == IDOK);
				break;
			}
		}
		
		return 0;
	}
};
*/

class AppWndPrivate
{
public:
	AppWnd *App;
	GMdiParent *Mdi;
	GOptionsFile Options;
	GSplitter *Sp;
	List<IdeDoc> Docs;
	List<IdeProject> Projects;
	GImageList *Icons;
	GTree *Tree;
	IdeOutput *Output;
	bool Debugging;
	bool Building;
	GSubMenu *WindowsMenu;
	
	// Find in files
	FindInFilesThread *Finder;
	
	// Mru
	List<char> RecentFiles;
	GSubMenu *RecentFilesMenu;
	List<char> RecentProjects;
	GSubMenu *RecentProjectsMenu;

	void ViewMsg(char *File, int Line, char *Context)
	{
		char *Full = 0;

		char *ContextPath = 0;
		if (Context)
		{
			char *Dir = strrchr(Context, DIR_CHAR);
			for (IdeProject *p=Projects.First(); p AND !ContextPath; p=Projects.Next())
			{
				ContextPath = p->FindFullPath(Dir?Dir+1:Context);
			}
			
			if (ContextPath)
			{
				LgiTrimDir(ContextPath);
				
				char p[300];
				LgiMakePath(p, sizeof(p), ContextPath, File);
				if (FileExists(p))
				{
					Full = NewStr(p);
				}
			}
			else
			{
				printf("%s:%i - Context '%s' not found in project.\n", __FILE__, __LINE__, Context);
			}
		}

		if (!Full)
		{
			List<IdeProject>::I Projs = Projects.Start();
			for (IdeProject *p=*Projs; p; p=*++Projs)
			{
				char Path[300];
				if (p->GetBasePath(Path))
				{
					LgiMakePath(Path, sizeof(Path), Path, File);
					if (FileExists(Path))
					{
						Full = NewStr(Path);
						break;
					}
				}
			}
		}
		
		if (!Full)
		{
			char *Dir = dirchar(File, true);
			for (IdeProject *p=Projects.First(); p AND !Full; p=Projects.Next())
			{
				Full = p->FindFullPath(Dir?Dir+1:File);
			}
			
			if (!Full)
			{
				if (FileExists(File))
				{
					Full = NewStr(File);
				}
			}
		}
		
		if (Full)
		{
			IdeDoc *Doc = App->OpenFile(Full);
			if (Doc)
			{
				Doc->GetEdit()->GotoLine(Line);
			}
			
			DeleteArray(Full);
		}
	}
	
	void GetContext(char16 *Txt, int &i, char16 *&Context)
	{
		static char16 NMsg[] = { 'I', 'n', ' ', 'f', 'i', 'l', 'e', ' ', 'i', 'n', 'c', 'l', 'u', 'd', 'e', 'd', ' ', 0 };
		static char16 FromMsg[] = { 'f', 'r', 'o', 'm', ' ', 0 };
		int NMsgLen = StrlenW(NMsg);

		if (Txt[i] == '\n')
		{
			if (StrncmpW(Txt + i + 1, NMsg, NMsgLen) == 0)
			{
				i += NMsgLen + 1;								
				
				while (Txt[i])
				{
					// Skip whitespace
					while (Txt[i] AND strchr(" \t\r\n", Txt[i])) i++;
					
					// Check for 'from'
					if (StrncmpW(FromMsg, Txt + i, 5) == 0)
					{
						i += 5;
						char16 *Start = Txt + i;

						// Skip to end of doc or line
						char16 *Colon = 0;
						while (Txt[i] AND Txt[i] != '\n')
						{
							if (Txt[i] == ':' AND Txt[i+1] != '\n')
							{
								Colon = Txt + i;
							}
							i++;
						}
						if (Colon)
						{
							DeleteArray(Context);
							Context = NewStrW(Start, Colon-Start);
						}
					}
					else
					{
						break;
					}
				}
			}
		}
		
	}
		
	void NextMsg()
	{
		if (Output AND
			Output->Tab)
		{
			int Current = Output->Tab->Value();
			GTextView3 *o = Current < CountOf(Output->Txt) ? Output->Txt[Current] : 0;
			if (o)
			{
				char16 *Txt = o->NameW();
				if (Txt)
				{
					int Cur = o->GetCursor();
					char16 *Context = 0;
					
					// Scan forward to the end of file for the next filename/linenumber separator.
					int i;
					for (i=Cur; Txt[i]; i++)
					{
						GetContext(Txt, i, Context);
						
						if
						(
							(Txt[i] == ':' OR Txt[i] == '(')
							AND
							isdigit(Txt[i+1])
						)
						{
							break;
						}
					}
					
					// If not found then scan from the start of the file for the next filename/linenumber separator.
					if (Txt[i] != ':')
					{
						for (i=0; i<Cur; i++)
						{
							GetContext(Txt, i, Context);
							
							if
							(
								(Txt[i] == ':' OR Txt[i] == '(')
								AND
								isdigit(Txt[i+1])
							)
							{
								break;
							}
						}
					}
					
					// If match found?
					if (Txt[i] == ':' OR Txt[i] == '(')
					{
						// Scan back to the start of the filename
						int Line = i;
						while (Line > 0 AND Txt[Line-1] != '\n')
						{
							Line--;
						}
						
						// Store the filename
						char *File = LgiNewUtf16To8(Txt+Line, (i-Line)*sizeof(char16));
						if (File)
						{
							// Scan over the linenumber..
							int NumIndex = ++i;
							while (isdigit(Txt[NumIndex])) NumIndex++;
							
							// Store the linenumber
							char *NumStr = LgiNewUtf16To8(Txt + i, (NumIndex - i) * sizeof(char16));
							if (NumStr)
							{
								// Convert it to an integer
								int LineNumber = atoi(NumStr);
								DeleteArray(NumStr);
								o->SetCursor(Line, false);
								o->SetCursor(NumIndex + 1, true);
								
								char *Context8 = LgiNewUtf16To8(Context);
								ViewMsg(File, LineNumber - 1, Context8);
								DeleteArray(Context8);
							}
							DeleteArray(File);
						}
					}					
				}
			}
		}
	}

	void UpdateMenus()
	{
		static char *None = "(none)";

		if (RecentFilesMenu)
		{
			RecentFilesMenu->Empty();
			int n=0;
			char *f=RecentFiles.First();
			if (f)
			{
				for (; f; f=RecentFiles.Next())
				{
					RecentFilesMenu->AppendItem(f, IDM_RECENT_FILE+n++, true);
				}
			}
			else
			{
				RecentFilesMenu->AppendItem(None, 0, false);
			}
		}

		if (RecentProjectsMenu)
		{
			RecentProjectsMenu->Empty();
			int n=0;
			char *f=RecentProjects.First();
			if (f)
			{
				for (; f; f=RecentProjects.Next())
				{
					RecentProjectsMenu->AppendItem(f, IDM_RECENT_PROJECT+n++, true);
				}
			}
			else
			{
				RecentProjectsMenu->AppendItem(None, 0, false);
			}
		}
		
		if (WindowsMenu)
		{
			WindowsMenu->Empty();
			Docs.Sort(DocSorter, 0);
			int n=0;
			for (IdeDoc *d=Docs.First(); d; d=Docs.Next())
			{
				char *File = d->GetFileName();
				if (!File) File = "(untitled)";
				char *Dir = strrchr(File, DIR_CHAR);
				WindowsMenu->AppendItem(Dir?Dir+1:File, IDM_WINDOWS+n++, true);
			}
			
			if (!Docs.First())
			{
				WindowsMenu->AppendItem(None, 0, false);
			}
		}
	}
	
	void OnFile(char *File, bool IsProject = false)
	{
		if (File)
		{
			List<char> *Recent = IsProject ? &RecentProjects : &RecentFiles;
			for (char *f=Recent->First(); f; f=Recent->Next())
			{
				if (stricmp(f, File) == 0)
				{
					Recent->Delete(f);
					Recent->Insert(f, 0);
					UpdateMenus();
					return;
				}
			}

			Recent->Insert(NewStr(File), 0);
			while (Recent->Length() > 10)
			{
				char *f = Recent->Last();
				Recent->Delete(f);
				DeleteArray(f);
			}

			UpdateMenus();
		}
	}
	
	void RemoveRecent(char *File)
	{
		if (File)
		{
			List<char> *Recent[3] = { &RecentProjects, &RecentFiles, 0 };
			for (List<char> **r = Recent; *r; r++)
			{
				for (char *f=(*r)->First(); f; f=(*r)->Next())
				{
					if (stricmp(f, File) == 0)
					{
						printf("Remove '%s'\n", f);

						(*r)->Delete(f);
						DeleteArray(f);
						break;
					}
				}
			}

			UpdateMenus();
		}
	}

	IdeDoc *IsFileOpen(char *File)
	{
		if (File)
		{
			for (IdeDoc *Doc = Docs.First(); Doc; Doc = Docs.Next())
			{
				if (Doc->IsFile(File))
				{
					return Doc;
				}
			}
		}

		return 0;
	}

	IdeProject *IsProjectOpen(char *File)
	{
		if (File)
		{
			for (IdeProject *p = Projects.First(); p; p = Projects.Next())
			{
				if (p->GetFileName() AND stricmp(p->GetFileName(), File) == 0)
				{
					return p;
				}
			}
		}

		return 0;
	}

	void SerializeStringList(char *Opt, List<char> *Lst, bool Write)
	{
		GVariant v;
		if (Write)
		{
			GBytePipe p;
			for (char *s = Lst->First(); s; s = Lst->Next())
			{
				p.Write((uchar*)s, strlen(s)+1);
			}
			
			int Size = p.GetSize();
			
			v.SetBinary(Size, p.New(), true);
			Options.SetValue(Opt, v);
		}
		else
		{
			Lst->DeleteArrays();

			if (Options.GetValue(Opt, v) && v.Type == GV_BINARY)
			{
				char *Data = (char*)v.Value.Binary.Data;
				for (char *s=Data; (int)s<(int)Data+v.Value.Binary.Length; s += strlen(s) + 1)
				{
					Lst->Insert(NewStr(s));
				}
			}
		}
	}

	// Object
	AppWndPrivate(AppWnd *a)
	{
		WindowsMenu = 0;
		App = a;
		Sp = 0;
		Tree = 0;
		Finder = 0;
		Output = 0;
		Debugging = false;
		Building = false;
		RecentFilesMenu = 0;
		RecentProjectsMenu = 0;
		Icons = LgiLoadImageList("icons.png", 16, 16);

		GVariant s;
		if (Options.GetValue("WndPos", s))
		{
			GRect p;
			if (p.SetStr(s.Str()) && p.x1 >= 0 && p.y1 >= 0)
			{
				App->SetPos(p);
			}
		}

		SerializeStringList("RecentFiles", &RecentFiles, false);
		SerializeStringList("RecentProjects", &RecentProjects, false);
	}
	
	~AppWndPrivate()
	{
		GVariant v;
		Options.SetValue("WndPos", v = App->GetPos().Describe());
		if (Sp)
			Options.SetValue("SplitPos", v = (int)Sp->Value());
		
		SerializeStringList("RecentFiles", &RecentFiles, true);
		SerializeStringList("RecentProjects", &RecentProjects, true);
		
		RecentFiles.DeleteArrays();
		RecentProjects.DeleteArrays();
		Docs.DeleteObjects();
		Projects.DeleteObjects();
		DeleteObj(Icons);
	}
};

AppWnd::AppWnd()
{
	GRect r(0, 0, 1000, 900);
	SetPos(r);
	d = new AppWndPrivate(this);
	MoveToCenter();
	Name(AppName);
	SetQuitOnClose(true);

	#ifdef WIN32
	CreateClassW32(AppName, LoadIcon(LgiProcessInst(), MAKEINTRESOURCE(IDI_APP)));
	#endif
	if (Attach(0))
	{
		Menu = new GMenu;
		if (Menu)
		{
			Menu->Attach(this);
			Menu->Load(this, "IDM_MENU");

			d->RecentFilesMenu = Menu->FindSubMenu(IDM_RECENT_FILES);
			d->RecentProjectsMenu = Menu->FindSubMenu(IDM_RECENT_PROJECTS);
			d->WindowsMenu = Menu->FindSubMenu(IDM_WINDOW_LST);

			GMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
			if (Debug)
			{
				Debug->Checked(true);
			}
			
			d->UpdateMenus();
		}

		GToolBar *Tools = LgiLoadToolbar(this, "cmds.png", 16, 16);
		if (Tools)
		{
			Tools->AppendButton("New", IDM_NEW, TBT_PUSH, true, CMD_NEW);
			Tools->AppendButton("Open", IDM_OPEN, TBT_PUSH, true, CMD_OPEN);
			Tools->AppendButton("Save", IDM_SAVE_ALL, TBT_PUSH, true, CMD_SAVE_ALL);
			Tools->AppendSeparator();
			Tools->AppendButton("Cut", IDM_CUT, TBT_PUSH, true, CMD_CUT);
			Tools->AppendButton("Copy", IDM_COPY, TBT_PUSH, true, CMD_COPY);
			Tools->AppendButton("Paste", IDM_PASTE, TBT_PUSH, true, CMD_PASTE);
			Tools->AppendSeparator();
			Tools->AppendButton("Compile", IDM_COMPILE, TBT_PUSH, true, CMD_COMPILE);
			Tools->AppendButton("Build", IDM_BUILD, TBT_PUSH, true, CMD_BUILD);
			Tools->AppendButton("Stop", IDM_STOP_BUILD, TBT_PUSH, true, CMD_STOP_BUILD);
			Tools->AppendButton("Execute", IDM_EXECUTE, TBT_PUSH, true, CMD_EXECUTE);
			
			/*
			Tools->AppendButton("Debug", IDM_DEBUG, TBT_PUSH, true, CMD_DEBUG);
			Tools->AppendButton("Breakpoint", IDM_BREAKPOINT, TBT_PUSH, true, CMD_BREAKPOINT);
			Tools->AppendSeparator();
			Tools->AppendButton("Restart", IDM_RESTART, TBT_PUSH, true, CMD_RESTART);
			Tools->AppendButton("Kill", IDM_KILL, TBT_PUSH, true, CMD_KILL);
			Tools->AppendButton("Step Into", IDM_STEP_INTO, TBT_PUSH, true, CMD_STEP_INTO);
			Tools->AppendButton("Step Over", IDM_STEP_OVER, TBT_PUSH, true, CMD_STEP_OVER);
			Tools->AppendButton("Step Out", IDM_STEP_OUT, TBT_PUSH, true, CMD_STEP_OUT);
			Tools->AppendButton("Run To", IDM_RUN_TO, TBT_PUSH, true, CMD_RUN_TO);
			*/

			Tools->AppendSeparator();
			Tools->AppendButton("Find In Files", IDM_FIND_IN_FILES, TBT_PUSH, true, CMD_FIND_IN_FILES);
			
			Tools->Attach(this);
		}
		
		d->Output = new IdeOutput;
		if (d->Output)
		{
			if (Tools)
			{
				Pour();
				d->Output->SetClosedSize(Tools->Y());
			}
			d->Output->Attach(this);
		}

		d->Sp = new GSplitter;
		if (d->Sp)
		{
			GVariant v = 200;
			d->Options.GetValue("SplitPos", v);
			d->Sp->Value(max(v.CastInt32(), 20));
			d->Sp->Attach(this);
			d->Tree = new IdeTree;
			if (d->Tree)
			{
				d->Tree->SetImageList(d->Icons, false);
				d->Tree->Sunken(false);
				d->Sp->SetViewA(d->Tree);
			}
			d->Sp->SetViewB(d->Mdi = new GMdiParent);
		}
	
		#ifdef LINUX
		char *f = LgiFindFile("lgiide.png");
		if (f)
		{
			// Handle()->setIcon(f);
			DeleteArray(f);
		}
		#endif
		
		UpdateState();
		
		Visible(true);
		DropTarget(true);
	}
	
	#ifdef LINUX
	LgiFinishXWindowsStartup(this);
	#endif
}

AppWnd::~AppWnd()
{
	ShutdownFtpThread();

	CloseAll();
	
	LgiApp->AppWnd = 0;
	DeleteObj(d);
}

void AppWnd::OnReceiveFiles(GArray<char*> &Files)
{
	for (int i=0; i<Files.Length(); i++)
	{
		char *f = Files[i];
		char *d = strrchr(f, DIR_CHAR);
		if (d && !stricmp(d, DIR_STR "leaks.mem"))
		{
			NewMemDumpViewer(this, f);
			return;
		}
		else
			OpenFile(f);
	}
	
	Raise();
}

void AppWnd::UpdateState(int Debugging, int Building)
{
	if (Debugging >= 0) d->Debugging = Debugging;
	if (Building >= 0) d->Building = Building;

	SetCtrlEnabled(IDM_COMPILE, !d->Building);
	SetCtrlEnabled(IDM_BUILD, !d->Building);
	SetCtrlEnabled(IDM_STOP_BUILD, d->Building);
	SetCtrlEnabled(IDM_EXECUTE, !d->Building);
	SetCtrlEnabled(IDM_DEBUG, !d->Building);
	SetCtrlEnabled(IDM_BREAKPOINT, !d->Building);
	SetCtrlEnabled(IDM_RESTART, d->Debugging);
	SetCtrlEnabled(IDM_KILL, d->Debugging);
	SetCtrlEnabled(IDM_STEP_INTO, d->Debugging);
	SetCtrlEnabled(IDM_STEP_OVER, d->Debugging);
	SetCtrlEnabled(IDM_STEP_OUT, d->Debugging);
	SetCtrlEnabled(IDM_RUN_TO, d->Debugging);
}

void AppWnd::AppendOutput(char *Txt, int Channel)
{
	if (d->Output AND
		Channel < CountOf(d->Output->Txt) AND
		d->Output->Txt[Channel])
	{
		if (Txt)
		{
			d->Output->Buf[Channel].Add(Txt, strlen(Txt));
		}
		else
		{
			d->Output->Txt[Channel]->Name("");
		}
	}
}

void AppWnd::OnFindFinished()
{
	d->Finder = 0;
}

void AppWnd::SaveAll()
{
	List<IdeDoc>::I Docs = d->Docs.Start();
	for (IdeDoc *Doc = *Docs; Doc; Doc = *++Docs)
	{
		Doc->SetClean();
		d->OnFile(Doc->GetFileName());
	}
	
	List<IdeProject>::I Projs = d->Projects.Start();
	for (IdeProject *Proj = *Projs; Proj; Proj = *++Projs)
	{
		Proj->SetClean();
		d->OnFile(Proj->GetFileName(), true);
	}
}

void AppWnd::CloseAll()
{
	SaveAll();
	d->Docs.DeleteObjects();
	
	IdeProject *p = RootProject();
	if (p)
	{
		DeleteObj(p);
	}
	
	d->Projects.DeleteObjects();	
}

bool AppWnd::OnRequestClose(bool IsClose)
{
	SaveAll();
	return GWindow::OnRequestClose(IsClose);
}

void AppWnd::OnFile(char *File, bool IsProject)
{
	d->OnFile(File, IsProject);
}

IdeDoc *AppWnd::NewDocWnd(char *FileName, NodeSource *Src)
{
	IdeDoc *Doc = new IdeDoc(this, Src, 0);
	if (Doc)
	{
		d->Docs.Insert(Doc);

		GRect p = d->Mdi->NewPos();
		Doc->SetPos(p);
		Doc->Attach(d->Mdi);
		Doc->SetFocus();
		Doc->Raise();

		char *File = Src ? Src->GetFileName() : FileName;
		d->OnFile(File);
	}

	return Doc;
}

IdeDoc *AppWnd::FindOpenFile(char *FileName)
{
	List<IdeDoc>::I it = d->Docs.Start();
	for (IdeDoc *i=*it; i; i=*++it)
	{
		char *f = i->GetFileName();
		if (f)
		{
			IdeProject *p = i->GetProject();
			if (p)
			{
				char Path[256];
				if (p->GetBasePath(Path))
				{
					if (*f == '.')
						LgiMakePath(Path, sizeof(Path), Path, f);
					else
						strsafecpy(Path, f, sizeof(Path));

					if (stricmp(Path, FileName) == 0)
						return i;
				}
			}
			else
			{
				if (stricmp(f, FileName) == 0)
					return i;
			}
		}
	}

	return 0;
}

IdeDoc *AppWnd::OpenFile(char *FileName, NodeSource *Src)
{
	static bool DoingProjectFind = false;
	IdeDoc *Doc = 0;
	
	char *File = Src ? Src->GetFileName() : FileName;
	if (Src OR FileExists(File))
	{
		Doc = d->IsFileOpen(File);

		if (!Doc)
		{
			if (Src)
			{
				Doc = NewDocWnd(FileName, Src);
			}
			else if (!DoingProjectFind)
			{
				DoingProjectFind = true;
				List<IdeProject>::I Proj = d->Projects.Start();
				for (IdeProject *p=*Proj; p AND !Doc; p=*++Proj)
				{
					p->InProject(File, true, &Doc);				
				}
				DoingProjectFind = false;

				d->OnFile(File);
			}
		}

		if (!Doc)
		{
			Doc = new IdeDoc(this, 0, File);
			if (Doc)
			{
				GRect p = d->Mdi->NewPos();
				Doc->SetPos(p);
				d->Docs.Insert(Doc);
				d->OnFile(File);
			}
		}

		if (Doc)
		{
			if (!Doc->IsAttached())
			{
				Doc->Attach(d->Mdi);
			}

			Doc->SetFocus();
			Doc->Raise();
		}
	}
	
	return Doc;
}

IdeProject *AppWnd::RootProject()
{
	IdeProject *Root = 0;
	
	for (IdeProject *p=d->Projects.First(); p; p=d->Projects.Next())
	{
		if (!p->GetParentProject())
		{
			LgiAssert(Root == 0);
			Root = p;
		}
	}
	
	return Root;
}

IdeProject *AppWnd::OpenProject(char *FileName, bool Create, bool Dep)
{
	IdeProject *p = 0;
	
	if (FileName AND !d->IsProjectOpen(FileName))
	{
		d->Projects.Insert(p = new IdeProject(this));
		if (p)
		{
			if (p->OpenFile(FileName))
			{
				d->OnFile(FileName, true);
				
				if (!Dep)
				{
					char *d = strrchr(FileName, DIR_CHAR);
					if (d++)
					{
						char n[256];
						sprintf(n, "%s [%s]", AppName, d);
						Name(n);						
					}
				}
			}
			else
			{
				DeleteObj(p);
				d->RemoveRecent(FileName);
			}

			if (!GetTree()->Selection())
			{
				GetTree()->Select(GetTree()->GetChild());
			}

			GetTree()->Focus(true);
		}
	}

	return p;
}

GMessage::Result AppWnd::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_BUILD_ERR:
		{
			char *Msg = (char*)MsgB(m);
			if (Msg)
			{
				LgiMsg(this, "Build Error: %s", AppName, MB_OK, Msg);
				DeleteArray(Msg);
			}
			break;
		}
		case M_APPEND_TEXT:
		{
			char *Text = (char*) MsgA(m);
			AppendOutput(Text, MsgB(m));
			DeleteArray(Text);
			break;
		}
	}

	return GWindow::OnEvent(m);
}

GOptionsFile *AppWnd::GetOptions()
{
	return &d->Options;
}

class Options : public GDialog
{
	AppWnd *App;
	GFontType Font;

public:
	Options(AppWnd *a)
	{
		SetParent(App = a);
		if (LoadFromResource(IDD_OPTIONS))
		{
			SetCtrlEnabled(IDC_FONT, false);
			MoveToCenter();
			
			if (!Font.Serialize(App->GetOptions(), OPT_EditorFont, false))
			{
				Font.GetSystemFont("Fixed");
			}			
			char s[256];
			if (Font.GetDescription(s))
			{
				SetCtrlName(IDC_FONT, s);
			}
			
			DoModal();
		}
	}
	
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDOK:
			{
				Font.Serialize(App->GetOptions(), OPT_EditorFont, true);
			}
			case IDCANCEL:
			{
				EndModal(c->GetId());
				break;
			}
			case IDC_SET_FONT:
			{
				if (Font.DoUI(this))
				{
					char s[256];
					if (Font.GetDescription(s))
					{
						SetCtrlName(IDC_FONT, s);
					}
				}
				break;
			}
		}
		return 0;
	}
};

int AppWnd::OnCommand(int Cmd, int Event, OsView Wnd)
{
	switch (Cmd)
	{
		case IDM_EXIT:
		{
			LgiCloseApp();
			break;
		}
		case IDM_OPTIONS:
		{
			Options Dlg(this);
			break;
		}
		case IDM_ABOUT:
		{
			LgiMsg(this, "Lgi Intergrated Development Environment", AppName);
			break;
		}
		case IDM_NEW:
		{
			IdeDoc *Doc;
			d->Docs.Insert(Doc = new IdeDoc(this, 0, 0));
			if (Doc)
			{
				GRect p = d->Mdi->NewPos();
				Doc->SetPos(p);
				Doc->Attach(d->Mdi);
				Doc->SetFocus();
			}
			break;
		}
		case IDM_OPEN:
		{
			GFileSelect s;
			s.Parent(this);
			if (s.Open())
			{
				OpenFile(s.Name());
			}
			break;
		}
		case IDM_SAVE_ALL:
		{
			SaveAll();
			break;
		}
		case IDM_SAVE:
		{
			IdeDoc *Top = TopDoc();
			if (Top) Top->SetClean();
			break;
		}
		case IDM_SAVEAS:
		{
			IdeDoc *Top = TopDoc();
			if (Top)
			{
				GFileSelect s;
				s.Parent(this);
				if (s.Save())
				{
					Top->SetFileName(s.Name(), true);
					d->OnFile(s.Name());
				}
			}
			break;
		}
		case IDM_CLOSE:
		{
			IdeDoc *Top = TopDoc();
			if (Top)
			{
				if (Top->OnRequestClose(false))
				{
					Top->Quit();
				}
			}
			break;
		}
		case IDM_CLOSE_ALL:
		{
			CloseAll();
			Name(AppName);
			break;
		}
		
		
		//
		// Editor
		//
		case IDM_UNDO:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->Undo();
			}
			break;
		}
		case IDM_REDO:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->Redo();
			}
			break;
		}
		case IDM_FIND:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoFind();
			}
			break;
		}
		case IDM_FIND_NEXT:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoFindNext();
			}
			break;
		}
		case IDM_REPLACE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoReplace();
			}
			break;
		}
		case IDM_GOTO:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoGoto();
			}
			break;
		}
		case IDM_CUT:
		{
			GViewI *v = LgiApp->GetFocus();
			if (v)
			{
				v->PostEvent(M_CUT);
			}
			break;
		}
		case IDM_COPY:
		{
			GViewI *v = LgiApp->GetFocus();
			if (v)
			{
				v->PostEvent(M_COPY);
			}
			break;
		}
		case IDM_PASTE:
		{
			GViewI *v = LgiApp->GetFocus();
			if (v)
			{
				v->PostEvent(M_PASTE);
			}
			break;
		}
		
		//
		// Project
		//
		case IDM_NEW_PROJECT:
		{
			CloseAll();
			
			IdeProject *p;
			d->Projects.Insert(p = new IdeProject(this));
			if (p)
			{
				p->CreateProject();
			}
			break;
		}		
		case IDM_OPEN_PROJECT:
		{
			GFileSelect s;
			s.Parent(this);
			s.Type("Projects", "*.xml");
			if (s.Open())
			{
				CloseAll();
				OpenProject(s.Name(), Cmd == IDM_NEW_PROJECT);
				if (d->Tree)
				{
					d->Tree->Focus(true);
				}
			}
			break;
		}
		case IDM_IMPORT_DSP:
		{
			IdeProject *p = RootProject();
			if (p)
			{
				GFileSelect s;
				s.Parent(this);
				s.Type("Developer Studio Project", "*.dsp");
				if (s.Open())
				{
					p->ImportDsp(s.Name());
				}
			}
			break;
		}
		case IDM_EXECUTE:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				p->Execute();
			}
			break;
		}
		case IDM_VALGRIND:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				p->Execute(ExeValgrind);
			}
			break;
		}		
		case IDM_DEBUG:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				p->Execute(ExeDebug);
			}
			break;
		}
		case IDM_BUILD:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				UpdateState(-1, true);
				p->Build(false);
			}
			break;			
		}
		case IDM_CLEAN:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				p->Clean();
			}
			break;
		}
		case IDM_NEXT_MSG:
		{
			d->NextMsg();
			break;
		}
		case IDM_CREATE_MAKEFILE:
		{
			IdeProject *p = RootProject();
			if (p)
			{
				p->CreateMakefile();
			}			
			break;
		}
		case IDM_DEBUG_MODE:
		{
			GMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
			GMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
			if (Debug AND Release)
			{
				Debug->Checked(true);
				Release->Checked(false);
			}
			break;
		}
		case IDM_RELEASE_MODE:
		{
			GMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
			GMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
			if (Debug AND Release)
			{
				Debug->Checked(false);
				Release->Checked(true);
			}
			break;
		}
		
		//
		// Other
		//
		case IDM_DEPENDS:
		{
			IdeProject *p = RootProject();
			if (p)
			{
				char *Exe = p->GetExecutable();
				if (FileExists(Exe))
				{
					Depends Dlg(this, Exe);
				}
				else
				{
					LgiMsg(this, "COuldn't find '%s'\n", AppName, MB_OK, Exe);
				}
			}			
			break;
		}
		case IDM_SP_TO_TAB:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				GTextView3 *Text = Doc->GetEdit();
				if (Text)
				{
					char *Sp = SpacesToTabs(Text->Name(), Text->GetTabSize());
					if (Sp)
					{
						Text->Name(Sp);
						Doc->SetDirty();
						DeleteArray(Sp);
					}
				}
			}
			break;
		}
		case IDM_TAB_TO_SP:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				GTextView3 *Text = Doc->GetEdit();
				if (Text)
				{
					char *Sp = TabsToSpaces(Text->Name(), Text->GetTabSize());
					if (Sp)
					{
						Text->Name(Sp);
						Doc->SetDirty();
						DeleteArray(Sp);
					}
				}
			}
			break;
		}
		case IDM_LOAD_MEMDUMP:
		{
			NewMemDumpViewer(this);
			break;
		}
		case IDM_SYS_CHAR_SUPPORT:
		{
			new SysCharSupport(this);
			break;
		}
		case IDM_FIND_IN_FILES:
		{
			if (d->Finder)
			{
				d->Finder->Stop();
			}
			else
			{
				FindInFiles Dlg(this);

				GViewI *Focus = LgiApp->GetFocus();
				if (Focus)
				{
					GTextView3 *Edit = dynamic_cast<GTextView3*>(Focus);
					if (Edit AND Edit->HasSelection())
					{
						Dlg.Params->Text = Edit->GetSelection();
					}
				}
				
				IdeProject *p = RootProject();
				if (p)
				{
					char Path[256];
					if (p->GetBasePath(Path))
					{
						DeleteArray(Dlg.Params->Dir);
						Dlg.Params->Dir = NewStr(Path);
					}
				}

				if (Dlg.DoModal())
				{
					d->Finder = new FindInFilesThread(this, Dlg.Params);
					Dlg.Params = 0;
				}
			}
			break;
		}
		default:
		{
			char *r = d->RecentFiles[Cmd - IDM_RECENT_FILE];
			if (r)
			{
				IdeDoc *f = d->IsFileOpen(r);
				if (f)
				{
					f->Raise();
				}
				else
				{
					OpenFile(r);
				}
			}

			char *p = d->RecentProjects[Cmd - IDM_RECENT_PROJECT];
			if (p)
			{
				CloseAll();
				OpenProject(p, false);
				if (d->Tree)
				{
					d->Tree->Focus(true);
				}
			}
			
			IdeDoc *Doc = d->Docs[Cmd - IDM_WINDOWS];
			if (Doc)
			{
				Doc->Raise();
			}
			break;
		}
	}
	
	return 0;
}

GTree *AppWnd::GetTree()
{
	return d->Tree;
}

IdeDoc *AppWnd::TopDoc()
{
	GAutoPtr<GViewIterator> i(d->Mdi->IterateViews());
	return i ? dynamic_cast<IdeDoc *>(i->Last()) : 0;
}
	
IdeDoc *AppWnd::FocusDoc()
{
	IdeDoc *Doc = TopDoc();
	if (Doc AND Doc->GetEdit())
	{
		if (Doc->GetEdit()->Focus())
		{
			return Doc;
		}
	}
	
	return 0;
}

void AppWnd::OnProjectDestroy(IdeProject *Proj)
{
	d->Projects.Delete(Proj);
}

void AppWnd::OnDocDestroy(IdeDoc *Doc)
{
	if (d)
	{
		d->Docs.Delete(Doc);
		d->UpdateMenus();
	}
}

int AppWnd::GetBuildMode()
{
	GMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
	if (Release AND Release->Checked())
	{
		return BUILD_TYPE_RELEASE;
	}

	return BUILD_TYPE_DEBUG;
}

GList *AppWnd::GetFtpLog()
{
	return d->Output->FtpLog;
}

class Worker : public GThread
{
	GStream *s;
	uint8 *Result;
	char Ip[32];

public:
	Worker(GStream *str, int x, int y, uint8 *r)
	{
		s = str;
		Result = r;
		sprintf(Ip, "192.168.%i.%i", x, y);
		DeleteOnExit = true;
		Run();
	}

	int Main()
	{
		GSocket sock;
		sock.SetTimeout(5000);
		// s->Print("Opening '%s'\n", Ip);
		if (sock.Open(Ip, 80))
		{
			s->Print("%s\n", Ip);
		}

		*Result = 2;
		return 0;
	}
};

class Owner : public GThread, public GNetwork
{
	GStream *s;
	GArray<uint8> Results;

public:
	Owner(GStream *str)
	{
		s = str;
		Results.Length(256*256);
		memset(&Results[0], 0, Results.Length());
		Run();
	}

	int Working()
	{
		int c = 0;
		for (int i=0; i<Results.Length(); i++)
		{
			if (Results[i] == 1)
				c++;
		}
		return c;
	}

	int Main()
	{
		int64 Start = LgiCurrentTime();
		for (int x=0; x<255; x++)
		{
			for (int y=0; y<255; y++)
			{
				while (Working() >= 500)
				{
					LgiSleep(2);
				}

				int Idx = (x << 8) + y;
				Results[Idx] = 1;
				new Worker(s, x, y, &Results[Idx]);

				int64 Now = LgiCurrentTime();
				if (Now - Start > 10000)
				{
					Start = Now;
					s->Print("Doing 192.168.%i.%i...\n", x, y);
				}
			}
		}
		return 0;
	}
};

#include "GTextLog.h"
class Ps : public GWindow
{
	GTextLog *t;

public:
	Ps()
	{
		GRect r(0, 0, 1000, 800);
		
		t = 0;		
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);
		if (Attach(0))
		{
			if (t = new GTextLog(100))
			{
				t->SetPourLargest(true);
				t->Attach(this);
			}

			Visible(true);
			new Owner(t);
		}
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	printf("LgiIde v%.2f\n", LgiIdeVer);
	GApp a("application/LgiIde", AppArgs);
	if (a.IsOk())
	{	
		a.AppWnd = new AppWnd;
		// a.AppWnd = new Ps;
		a.Run();
	}

	return 0;
}
