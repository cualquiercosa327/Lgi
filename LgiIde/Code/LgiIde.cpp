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
#include "FindSymbol.h"

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
#define IDM_MAKEFILE_BASE		1300

//////////////////////////////////////////////////////////////////////////////////////////
char AppName[] = "LgiIde";

char *dirchar(char *s, bool rev = false)
{
	if (rev)
	{
		char *last = 0;
		while (s && *s)
		{
			if (*s == '/' || *s == '\\')
				last = s;
			s++;
		}
		return last;
	}
	else
	{
		while (s && *s)
		{
			if (*s == '/' || *s == '\\')
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
	Dependency(const char *file)
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
		if (b && !Loaded)
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
						if (SharedLib &&
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
	Depends(GView *Parent, const char *File)
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
			
			Root = new Dependency(File);
			if (Root)
			{
				t->Insert(Root);
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
		int Changed = -1;
		
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
				Changed = Channel;
				Buf[Channel].Length(0);
				Txt[Channel]->Invalidate();
			}
		}
		
		if (Changed >= 0)
			Tab->Value(Changed);
	}

	void OnPosChange()
	{
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
	if (A && B)
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

struct FileLoc
{
	GAutoString File;
	int Line;
	
	void Set(const char *f, int l)
	{
		File.Reset(NewStr(f));
		Line = l;
	}
};

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
	GSubMenu *CreateMakefileMenu;
	FindSymbolSystem FindSym;
	GArray<GAutoString> SystemIncludePaths;
	
	// Cursor history tracking
	int HistoryLoc;
	GArray<FileLoc> CursorHistory;
	bool InHistorySeek;
	
	void SeekHistory(int Direction)
	{
		if (CursorHistory.Length())
		{
			InHistorySeek = true;
			
			HistoryLoc += Direction;
			if (HistoryLoc < 0) HistoryLoc = 0;
			if (HistoryLoc >= CursorHistory.Length()) HistoryLoc = CursorHistory.Length() - 1;

			FileLoc &Loc = CursorHistory[HistoryLoc];
			App->GotoReference(Loc.File, Loc.Line);

			InHistorySeek = false;
		}
	}
	
	// Find in files
	FindInFilesThread *Finder;
	
	// Mru
	List<char> RecentFiles;
	GSubMenu *RecentFilesMenu;
	List<char> RecentProjects;
	GSubMenu *RecentProjectsMenu;

	// Object
	AppWndPrivate(AppWnd *a) :
		Options(GOptionsFile::DesktopMode, AppName),
		FindSym(a)
	{
		HistoryLoc = 0;
		InHistorySeek = false;
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

		Options.Serialize(false);
		App->SerializeState(&Options, "WndPos", true);

		SerializeStringList("RecentFiles", &RecentFiles, false);
		SerializeStringList("RecentProjects", &RecentProjects, false);
	}
	
	~AppWndPrivate()
	{
		App->SerializeState(&Options, "WndPos", false);
		SerializeStringList("RecentFiles", &RecentFiles, true);
		SerializeStringList("RecentProjects", &RecentProjects, true);
		Options.Serialize(true);
		
		RecentFiles.DeleteArrays();
		RecentProjects.DeleteArrays();
		Docs.DeleteObjects();
		Projects.DeleteObjects();
		DeleteObj(Icons);
	}

	void ViewMsg(char *File, int Line, char *Context)
	{
		GAutoString Full;

		if (!LgiIsRelativePath(File))
		{
			Full.Reset(NewStr(File));
		}

		char *ContextPath = 0;
		if (Context && !Full)
		{
			char *Dir = strrchr(Context, DIR_CHAR);
			for (IdeProject *p=Projects.First(); p && !ContextPath; p=Projects.Next())
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
					Full.Reset(NewStr(p));
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
				GAutoString Base = p->GetBasePath();
				if (Base)
				{
					char Path[MAX_PATH];
					LgiMakePath(Path, sizeof(Path), Base, File);
					if (FileExists(Path))
					{
						Full.Reset(NewStr(Path));
						break;
					}
				}
			}
		}
		
		if (!Full)
		{
			char *Dir = dirchar(File, true);
			for (IdeProject *p=Projects.First(); p && !Full; p=Projects.Next())
			{
				Full.Reset(p->FindFullPath(Dir?Dir+1:File));
			}
			
			if (!Full)
			{
				if (FileExists(File))
				{
					Full.Reset(NewStr(File));
				}
			}
		}
		
		if (Full)
		{
			IdeDoc *Doc = App->GotoReference(Full, Line);			
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
					while (Txt[i] && strchr(" \t\r\n", Txt[i])) i++;
					
					// Check for 'from'
					if (StrncmpW(FromMsg, Txt + i, 5) == 0)
					{
						i += 5;
						char16 *Start = Txt + i;

						// Skip to end of doc or line
						char16 *Colon = 0;
						while (Txt[i] && Txt[i] != '\n')
						{
							if (Txt[i] == ':' && Txt[i+1] != '\n')
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
		if (Output &&
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
							(Txt[i] == ':' || Txt[i] == '(')
							&&
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
								(Txt[i] == ':' || Txt[i] == '(')
								&&
								isdigit(Txt[i+1])
							)
							{
								break;
							}
						}
					}
					
					// If match found?
					if (Txt[i] == ':' || Txt[i] == '(')
					{
						// Scan back to the start of the filename
						int Line = i;
						while (Line > 0 && Txt[Line-1] != '\n')
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
								ViewMsg(File, LineNumber, Context8);
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

		if (!App->GetMenu())
			return; // This happens in GTK during window destruction

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
		if (!File)
		{
			printf("%s:%i - No input File?\n", _FL);
			return NULL;
		}
		
		for (IdeDoc *Doc = Docs.First(); Doc; Doc = Docs.Next())
		{
			if (Doc->IsFile(File))
			{
				return Doc;
			}
		}

		// printf("%s:%i - '%s' not found in %i docs.\n", _FL, File, Docs.Length());
		return 0;
	}

	IdeProject *IsProjectOpen(char *File)
	{
		if (File)
		{
			for (IdeProject *p = Projects.First(); p; p = Projects.Next())
			{
				if (p->GetFileName() && stricmp(p->GetFileName(), File) == 0)
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
			GMemQueue p;
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
				for (char *s=Data; (NativeInt)s<(NativeInt)Data+v.Value.Binary.Length; s += strlen(s) + 1)
				{
					Lst->Insert(NewStr(s));
				}
			}
		}
	}
};

AppWnd::AppWnd()
{
	#ifdef __GTK_H__
	LgiGetResObj(true, AppName);
	#endif

	GRect r(0, 0, 1000, 630);
	SetPos(r);
	MoveToCenter();

	d = new AppWndPrivate(this);
	Name(AppName);
	SetQuitOnClose(true);

	#if WINNATIVE
	SetIcon(MAKEINTRESOURCE(IDI_APP));
	#else
	SetIcon("Icon64.png");
	#endif
	if (Attach(0))
	{
		Menu = new GMenu;
		if (Menu)
		{
			Menu->Attach(this);
			bool Loaded = Menu->Load(this, "IDM_MENU");
			LgiAssert(Loaded);
			if (Loaded)
			{
				d->RecentFilesMenu = Menu->FindSubMenu(IDM_RECENT_FILES);
				d->RecentProjectsMenu = Menu->FindSubMenu(IDM_RECENT_PROJECTS);
				d->WindowsMenu = Menu->FindSubMenu(IDM_WINDOW_LST);
				d->CreateMakefileMenu = Menu->FindSubMenu(IDM_CREATE_MAKEFILE);
				if (d->CreateMakefileMenu)
				{
					d->CreateMakefileMenu->Empty();
					for (int i=0; PlatformNames[i]; i++)
					{
						d->CreateMakefileMenu->AppendItem(PlatformNames[i], IDM_MAKEFILE_BASE + i);
					}
				}

				GMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
				if (Debug)
				{
					Debug->Checked(true);
				}
				
				d->UpdateMenus();
			}
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
	SaveAll();
	
	if (d->Sp)
	{
		GVariant v = d->Sp->Value();
		d->Options.SetValue("SplitPos", v);
	}

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
		
		char *ext = LgiGetExtension(f);
		if (ext && !stricmp(ext, "mem"))
		{
			NewMemDumpViewer(this, f);
			return;
		}
		else if (ext && !stricmp(ext, "xml"))
		{
			OpenProject(f, NULL);
		}
		else
		{
			OpenFile(f);
		}
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
	if (d->Output &&
		Channel < CountOf(d->Output->Txt) &&
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

void AppWnd::OnLocationChange(const char *File, int Line)
{
	if (!d->InHistorySeek)
	{
		// Destroy any history after the current...
		d->HistoryLoc++;
		d->CursorHistory.Length(d->HistoryLoc);
		
		// Add new entry
		d->CursorHistory.New().Set(File, Line);
	}
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
		Doc->Focus(true);
		Doc->Raise();

		if (FileName)
			d->OnFile(FileName);
	}

	return Doc;
}

IdeDoc *AppWnd::GotoReference(char *File, int Line)
{
	IdeDoc *Doc = OpenFile(File);
	if (Doc)
	{
		Doc->GetEdit()->SetLine(Line);
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
				GAutoString Base = p->GetBasePath();
				if (Base)
				{
					char Path[MAX_PATH];
					if (*f == '.')
						LgiMakePath(Path, sizeof(Path), Base, f);
					else
						strcpy_s(Path, sizeof(Path), f);

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
	if (Src || FileExists(File))
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
				for (IdeProject *p=*Proj; p && !Doc; p=*++Proj)
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

			Doc->Focus(true);
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

IdeProject *AppWnd::OpenProject(char *FileName, IdeProject *ParentProj, bool Create, bool Dep)
{
	IdeProject *p = 0;
	
	if (FileName && !d->IsProjectOpen(FileName))
	{
		d->Projects.Insert(p = new IdeProject(this));
		if (p)
		{
			p->SetParentProject(ParentProj);
			
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
			if (!ParentProj)
				d->FindSym.OnProject();
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
			if (Font.GetDescription(s, sizeof(s)))
			{
				SetCtrlName(IDC_FONT, s);
			}

			GVariant v;
			if (App->GetOptions()->GetValue(OPT_Jobs, v))
				SetCtrlValue(IDC_JOBS, v.CastInt32());
			else
				SetCtrlValue(IDC_JOBS, 2);
			
			DoModal();
		}
	}
	
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDOK:
			{
				GVariant v;
				Font.Serialize(App->GetOptions(), OPT_EditorFont, true);
				App->GetOptions()->SetValue(OPT_Jobs, v = GetCtrlValue(IDC_JOBS));
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
					if (Font.GetDescription(s, sizeof(s)))
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
			LgiMsg(this, "LGI Integrated Development Environment", AppName);
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
				Doc->Focus(true);
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
			else printf("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_REDO:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->Redo();
			}
			else printf("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_FIND:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoFind();
			}
			else printf("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_FIND_NEXT:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoFindNext();
			}
			else printf("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_REPLACE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoReplace();
			}
			else printf("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_GOTO:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GetEdit()->DoGoto();
			}
			else printf("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_CUT:
		{
			GViewI *v = GetFocus();
			if (v)
			{
				v->PostEvent(M_CUT);
			}
			break;
		}
		case IDM_COPY:
		{
			GViewI *v = GetFocus();
			if (v)
			{
				v->PostEvent(M_COPY);
			}
			break;
		}
		case IDM_PASTE:
		{
			GViewI *v = GetFocus();
			if (v)
			{
				v->PostEvent(M_PASTE);
			}
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

				GViewI *Focus = GetFocus();
				if (Focus)
				{
					GTextView3 *Edit = dynamic_cast<GTextView3*>(Focus);
					if (Edit && Edit->HasSelection())
					{
						Dlg.Params->Text = Edit->GetSelection();
					}
				}
				
				IdeProject *p = RootProject();
				if (p)
				{
					GAutoString Base = p->GetBasePath();
					if (Base)
					{
						DeleteArray(Dlg.Params->Dir);
						Dlg.Params->Dir = NewStr(Base);
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
		case IDM_FIND_SYMBOL:
		{
			FindSymResult r = d->FindSym.OpenSearchDlg(this);
			if (r.File)
			{
				GotoReference(r.File, r.Line);
			}
			break;
		}
		case IDM_FIND_REFERENCES:
		{
			LgiMsg(this, "Not implemented yet.", AppName);
			break;
		}
		case IDM_PREV_LOCATION:
		{
			d->SeekHistory(-1);
			break;
		}
		case IDM_NEXT_LOCATION:
		{
			d->SeekHistory(1);
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
				OpenProject(s.Name(), NULL, Cmd == IDM_NEW_PROJECT);
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
		case IDM_DEBUG_MODE:
		{
			GMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
			GMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
			if (Debug && Release)
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
			if (Debug && Release)
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
				const char *Exe = p->GetExecutable();
				if (FileExists(Exe))
				{
					Depends Dlg(this, Exe);
				}
				else
				{
					LgiMsg(this, "Couldn't find '%s'\n", AppName, MB_OK, Exe ? Exe : "<project_executable_undefined>");
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
				OpenProject(p, NULL, false);
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
			
			IdePlatform PlatIdx = (IdePlatform) (Cmd - IDM_MAKEFILE_BASE);
			const char *Platform =	PlatIdx >= 0 && PlatIdx < PlatformMax
									?
									PlatformNames[Cmd - IDM_MAKEFILE_BASE]
									:
									NULL;
			if (Platform)
			{
				IdeProject *p = RootProject();
				if (p)
				{
					p->CreateMakefile(PlatIdx);
				}
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
	return dynamic_cast<IdeDoc*>(d->Mdi->GetTop());
}
	
IdeDoc *AppWnd::FocusDoc()
{
	IdeDoc *Doc = TopDoc();
	if (Doc && Doc->GetEdit())
	{
		if (Doc->GetEdit()->Focus())
		{
			return Doc;
		}
		else
		{
			GViewI *f = GetFocus();
			printf("%s:%i - Edit doesn't have focus, f=%p %s doc.edit=%p %s\n",
				_FL, f, f ? f->GetClass() : 0,
				Doc->GetEdit(),
				Doc->Name());
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
	if (Release && Release->Checked())
	{
		return BUILD_TYPE_RELEASE;
	}

	return BUILD_TYPE_DEBUG;
}

GList *AppWnd::GetFtpLog()
{
	return d->Output->FtpLog;
}

bool AppWnd::FindSymbol(const char *Sym, GArray<FindSymResult> &Results)
{
	d->FindSym.Search(Sym, Results);
	return Results.Length() > 0;
}

#include "GSubProcess.h"
bool AppWnd::GetSystemIncludePaths(::GArray<char*> &Paths)
{
	if (d->SystemIncludePaths.Length() == 0)
	{
		#if defined(LINUX)
		// echo | gcc -v -x c++ -E -
		GSubProcess sp1("echo");
		GSubProcess sp2("gcc", "-v -x c++ -E -");
		sp1.Connect(&sp2);
		sp1.Start(true, false);
		
		char Buf[256];
		int r;

		GStringPipe p;
		while ((r = sp1.Read(Buf, sizeof(Buf))) > 0)
		{
			p.Write(Buf, r);
		}

		bool InIncludeList = false;
		while (p.Pop(Buf, sizeof(Buf)))
		{
			if (stristr(Buf, "#include"))
			{
				InIncludeList = true;
			}
			else if (stristr(Buf, "End of search"))
			{
				InIncludeList = false;
			}
			else if (InIncludeList)
			{
				GAutoString a(TrimStr(Buf));
				d->SystemIncludePaths.New() = a;
			}
		}
		#elif defined(WINNATIVE)
		char p[MAX_PATH];
		LgiGetSystemPath(LSP_USER_DOCUMENTS, p, sizeof(p));
		LgiMakePath(p, sizeof(p), p, "Visual Studio 2008\\Settings\\CurrentSettings.xml");
		if (FileExists(p))
		{
			GFile f;
			if (f.Open(p, O_READ))
			{
				GXmlTree t;
				GXmlTag r;
				if (t.Read(&r, &f))
				{
					GXmlTag *Opts = r.GetTag("ToolsOptions");
					if (Opts)
					{
						GXmlTag *Projects = NULL;
						char *Name;
						for (GXmlTag *c = Opts->Children.First(); c; c = Opts->Children.Next())
						{
							if (c->IsTag("ToolsOptionsCategory") &&
								(Name = c->GetAttr("Name")) &&
								!stricmp(Name, "Projects"))
							{
								Projects = c;
								break;
							}
						}

						GXmlTag *VCDirectories = NULL;
						for (GXmlTag *c = Projects ? Projects->Children.First() : NULL;
							c;
							c = Projects->Children.Next())
						{
							if (c->IsTag("ToolsOptionsSubCategory") &&
								(Name = c->GetAttr("Name")) &&
								!stricmp(Name, "VCDirectories"))
							{
								VCDirectories = c;
								break;
							}
						}

						for (GXmlTag *prop = VCDirectories ? VCDirectories->Children.First() : NULL;
							prop;
							prop = VCDirectories->Children.Next())
						{
							if (prop->IsTag("PropertyValue") &&
								(Name = prop->GetAttr("Name")) &&
								!stricmp(Name, "IncludeDirectories"))
							{
								char *Bar = strchr(prop->Content, '|');
								GToken t(Bar ? Bar + 1 : prop->Content, ";");
								for (int i=0; i<t.Length(); i++)
								{
									char *s = t[i];
									d->SystemIncludePaths.New().Reset(NewStr(s));
								}
							}
						}
					}
				}
			}
		}		
		#else
		LgiAssert(!"Not impl");
		#endif
	}
	
	for (int i=0; i<d->SystemIncludePaths.Length(); i++)
	{
		Paths.Add(NewStr(d->SystemIncludePaths[i]));
	}
	
	return true;
}

/*
class Test : public GWindow
{
public:
	Test()
	{
		GRect r(100, 100, 900, 700);
		SetPos(r);
		Name("Test");
		SetQuitOnClose(true);
		
		if (Attach(0))
		{
			Visible(true);
		}
	}
	
	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle();
		
		for (int i=0; i<6; i++)
		{
			GDisplayString d1(SysFont, "test1");
			d1.Draw(pDC, 10, 10+(i*45));

			GDisplayString d2(SysBold, "test2");
			d2.Draw(pDC, 10, 30+(i*45));
		}
	}
};
*/

int LgiMain(OsAppArguments &AppArgs)
{
	printf("LgiIde v%.2f\n", LgiIdeVer);
	GApp a(AppArgs, "LgiIde");
	if (a.IsOk())
	{
		a.AppWnd = new AppWnd;
		// a.AppWnd = new Test;
		a.Run();
	}

	return 0;
}

