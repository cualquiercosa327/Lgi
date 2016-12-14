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
#include "GBox.h"
#include "GTextLog.h"
#include "GEdit.h"
#include "GTableLayout.h"
#include "GTextLabel.h"
#include "GCombo.h"
#include "GCheckBox.h"
#include "GDebugger.h"
#include "LgiRes.h"
#include "ProjectNode.h"

#define IDM_SAVE				102
#define IDM_RECENT_FILE			1000
#define IDM_RECENT_PROJECT		1100
#define IDM_WINDOWS				1200
#define IDM_MAKEFILE_BASE		1300

#define USE_HAIKU_PULSE_HACK	1

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
	
	char *Find(const char *Paths, char *e)
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
				// LgiTrace("o=%s\n", o);
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
class DebugTextLog : public GTextLog
{
public:
	DebugTextLog(int id) : GTextLog(id)
	{
	}
	
	void PourText(int Start, int Len)
	{
		GTextView3::PourText(Start, Len);

		for (GTextLine *l=Line.First(); l; l=Line.Next())
		{
			int n=0;
			char16 *t = Text + l->Start;
			char16 *e = t + l->Len;

			if (l->Len > 5 && !StrnicmpW(t, L"(gdb)", 5))
			{
				l->c.Rgb(0, 160, 0);
			}
			else if (l->Len > 1 && t[0] == '[')
			{
				l->c.Rgb(192, 192, 192);
			}
		}
	}
};

WatchItem::WatchItem(IdeOutput *out, const char *Init = NULL)
{
	Out = out;
	Expanded(false);
	if (Init)
		SetText(Init);
	Insert(PlaceHolder = new GTreeItem);
}

WatchItem::~WatchItem()
{
	printf("~WatchItem\n");
}

bool WatchItem::SetValue(GVariant &v)
{
	char *Str = v.CastString();
	if (ValidStr(Str))
		SetText(Str, 2);
	else
		GTreeItem::SetText(NULL, 2);
	return true;
}

bool WatchItem::SetText(const char *s, int i = 0)
{
	if (ValidStr(s))
	{
		bool status = GTreeItem::SetText(s, i);

		if (i == 0 && Tree && Tree->GetWindow())
		{
			GViewI *Tabs = Tree->GetWindow()->FindControl(IDC_DEBUG_TAB);
			if (Tabs)
				Tabs->SendNotify(GNotifyValueChanged);
		}
		return true;
	}

	if (i == 0)	
		delete this;
		
	return false;
}

void WatchItem::OnExpand(bool b)
{
	if (b && PlaceHolder)
	{
		// Do something 
	}
}

class IdeOutput : public GPanel
{
public:
	AppWnd *App;
	GTabView *Tab;
	GTabPage *Build;
	GTabPage *Output;
	GTabPage *Debug;
	GTabPage *Find;
	GTabPage *Ftp;
	GList *FtpLog;
	GTextLog *Txt[3];
	GArray<char> Buf[3];
	GFont Small;
	GFont Fixed;

	GTabView *DebugTab;
	GBox *DebugBox;
	GBox *DebugLog;
	GList *Locals, *CallStack, *Threads;
	GTree *Watch;
	GTextLog *ObjectDump, *MemoryDump, *Registers;
	GTableLayout *MemTable;
	GEdit *DebugEdit;
	GTextLog *DebuggerLog;

	IdeOutput(AppWnd *app) : GPanel("Panel", 200, true)
	{
		App = app;
		Build = Output = Debug = Find = Ftp = 0;
		FtpLog = 0;
		DebugBox = NULL;
		Locals = NULL;
		Watch = NULL;
		DebugLog = NULL;
		DebugEdit = NULL;
		DebuggerLog = NULL;
		CallStack = NULL;
		ObjectDump = NULL;
		MemoryDump = NULL;
		MemTable = NULL;
		Threads = NULL;
		Registers = NULL;

		Small = *SysFont;
		Small.PointSize(Small.PointSize()-2);
		Small.Create();
		LgiAssert(Small.Handle());
		
		GFontType Type;
		if (Type.GetSystemFont("Fixed"))
		{
			Type.SetPointSize(SysFont->PointSize()-1);
			Fixed.Create(&Type);
		}
		else
		{
			Fixed.PointSize(SysFont->PointSize()-1);
			Fixed.Face("Courier");
			Fixed.Create();			
		}		

		Alignment(GV_EDGE_BOTTOM);
		Children.Insert(Tab = new GTabView(100, 18, 3, 200, 200, "Output"));
		if (Tab)
		{
			Build = Tab->Append("Build");
			Output = Tab->Append("Output");
			Find = Tab->Append("Find");
			Ftp = Tab->Append("Ftp");
			Debug = Tab->Append("Debug");

			Tab->SetFont(&Small);
			Build->SetFont(&Small);
			Output->SetFont(&Small);
			Find->SetFont(&Small);
			Ftp->SetFont(&Small);
			Debug->SetFont(&Small);
			
			if (Build)
				Build->Append(Txt[AppWnd::BuildTab] = new GTextLog(IDC_BUILD_LOG));
			if (Output)
				Output->Append(Txt[AppWnd::OutputTab] = new GTextLog(IDC_OUTPUT_LOG));
			if (Find)
				Find->Append(Txt[AppWnd::FindTab] = new GTextLog(IDC_FIND_LOG));
			if (Ftp)
				Ftp->Append(FtpLog = new GList(104, 0, 0, 100, 100));
			if (Debug)
			{
				Debug->Append(DebugBox = new GBox);
				if (DebugBox)
				{
					DebugBox->SetVertical(false);
					
					if ((DebugTab = new GTabView(IDC_DEBUG_TAB)))
					{
						DebugTab->SetFont(&Small);
						DebugBox->AddView(DebugTab);

						GTabPage *Page;
						if ((Page = DebugTab->Append("Locals")))
						{
							Page->SetFont(&Small);
							if ((Locals = new GList(IDC_LOCALS_LIST, 0, 0, 100, 100, "Locals List")))
							{
								Locals->SetFont(&Small);
								Locals->AddColumn("", 30);
								Locals->AddColumn("Type", 50);
								Locals->AddColumn("Name", 50);
								Locals->AddColumn("Value", 1000);
								Locals->SetPourLargest(true);

								Page->Append(Locals);
							}
						}
						if ((Page = DebugTab->Append("Object")))
						{
							Page->SetFont(&Small);
							if ((ObjectDump = new GTextLog(IDC_OBJECT_DUMP)))
							{
								ObjectDump->SetFont(&Fixed);
								ObjectDump->SetPourLargest(true);
								Page->Append(ObjectDump);
							}
						}
						if ((Page = DebugTab->Append("Watch")))
						{
							Page->SetFont(&Small);
							if ((Watch = new GTree(IDC_WATCH_LIST, 0, 0, 100, 100, "Watch List")))
							{
								Watch->SetFont(&Small);
								Watch->ShowColumnHeader(true);
								Watch->AddColumn("Watch", 80);
								Watch->AddColumn("Type", 100);
								Watch->AddColumn("Value", 600);
								Watch->SetPourLargest(true);

								Page->Append(Watch);
								
								GXmlTag *w = App->GetOptions()->LockTag("watches", _FL);
								if (!w)
								{
									App->GetOptions()->CreateTag("watches");
									w = App->GetOptions()->LockTag("watches", _FL);
								}
								if (w)
								{
									for (GXmlTag *c = w->Children.First(); c; c = w->Children.Next())
									{
										if (c->IsTag("watch"))
										{
											Watch->Insert(new WatchItem(this, c->GetContent()));
										}
									}
									
									App->GetOptions()->Unlock();
								}
							}
						}
						if ((Page = DebugTab->Append("Memory")))
						{
							Page->SetFont(&Small);
							
							if ((MemTable = new GTableLayout(IDC_MEMORY_TABLE)))
							{
								GCombo *cbo;
								GCheckBox *chk;
								GText *txt;
								GEdit *ed;
								MemTable->SetFont(&Small);
							
								int x = 0, y = 0;
								GLayoutCell *c = MemTable->GetCell(x++, y);
								if (c)
								{
									c->VerticalAlign(GCss::VerticalMiddle);
									c->Add(txt = new GText(IDC_STATIC, 0, 0, -1, -1, "Address:"));
									txt->SetFont(&Small);
								}
								c = MemTable->GetCell(x++, y);
								if (c)
								{
									c->PaddingRight(GCss::Len("1em"));
									c->Add(ed = new GEdit(IDC_MEM_ADDR, 0, 0, 60, 20));
									ed->SetFont(&Small);
								}								
								c = MemTable->GetCell(x++, y);
								if (c)
								{
									c->PaddingRight(GCss::Len("1em"));
									c->Add(cbo = new GCombo(IDC_MEM_SIZE, 0, 0, 60, 20));
									cbo->SetFont(&Small);
									cbo->Insert("1 byte");
									cbo->Insert("2 bytes");
									cbo->Insert("4 bytes");
									cbo->Insert("8 bytes");
								}
								c = MemTable->GetCell(x++, y);
								if (c)
								{
									c->VerticalAlign(GCss::VerticalMiddle);
									c->Add(txt = new GText(IDC_STATIC, 0, 0, -1, -1, "Page width:"));
									txt->SetFont(&Small);
								}
								c = MemTable->GetCell(x++, y);
								if (c)
								{
									c->PaddingRight(GCss::Len("1em"));
									c->Add(ed = new GEdit(IDC_MEM_ROW_LEN, 0, 0, 60, 20));
									ed->SetFont(&Small);
								}
								c = MemTable->GetCell(x++, y);								
								if (c)
								{
									c->VerticalAlign(GCss::VerticalMiddle);
									c->Add(chk = new GCheckBox(IDC_MEM_HEX, 0, 0, -1, -1, "Show Hex"));
									chk->SetFont(&Small);
									chk->Value(true);
								}

								int cols = x;
								x = 0;
								c = MemTable->GetCell(x++, ++y, true, cols);
								if ((MemoryDump = new GTextLog(IDC_MEMORY_DUMP)))
								{
									MemoryDump->SetFont(&Fixed);
									MemoryDump->SetPourLargest(true);
									c->Add(MemoryDump);
								}

								Page->Append(MemTable);
							}
						}
						if ((Page = DebugTab->Append("Threads")))
						{
							Page->SetFont(&Small);
							if ((Threads = new GList(IDC_THREADS, 0, 0, 100, 100, "Threads")))
							{
								Threads->SetFont(&Small);
								Threads->AddColumn("", 20);
								Threads->AddColumn("Thread", 1000);
								Threads->SetPourLargest(true);
								Threads->MultiSelect(false);

								Page->Append(Threads);
							}
						}
						if ((Page = DebugTab->Append("Call Stack")))
						{
							Page->SetFont(&Small);
							if ((CallStack = new GList(IDC_CALL_STACK, 0, 0, 100, 100, "Call Stack")))
							{
								CallStack->SetFont(&Small);
								CallStack->AddColumn("", 20);
								CallStack->AddColumn("Call Stack", 1000);
								CallStack->SetPourLargest(true);
								CallStack->MultiSelect(false);

								Page->Append(CallStack);
							}
						}

						if ((Page = DebugTab->Append("Registers")))
						{
							Page->SetFont(&Small);
							if ((Registers = new GTextLog(IDC_REGISTERS)))
							{
								Registers->SetFont(&Small);
								Registers->SetPourLargest(true);
								Page->Append(Registers);
							}
						}
					}
					
					if ((DebugLog = new GBox))
					{
						DebugLog->SetVertical(true);
						DebugBox->AddView(DebugLog);
						DebugLog->AddView(DebuggerLog = new DebugTextLog(IDC_DEBUGGER_LOG));
						DebuggerLog->SetFont(&Small);
						DebugLog->AddView(DebugEdit = new GEdit(IDC_DEBUG_EDIT, 0, 0, 60, 20));
						DebugEdit->GetCss(true)->Height(GCss::Len(GCss::LenPx, SysFont->GetHeight() + 8));
					}
				}
			}

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
	
	void Save()
	{
		if (Watch)
		{
			GXmlTag *w = App->GetOptions()->LockTag("watches", _FL);
			if (!w)
			{
				App->GetOptions()->CreateTag("watches");
				w = App->GetOptions()->LockTag("watches", _FL);
			}
			if (w)
			{
				w->EmptyChildren();
				for (GTreeItem *ti = Watch->GetChild(); ti; ti = ti->GetNext())
				{
					GXmlTag *t = new GXmlTag("watch");
					if (t)
					{
						t->SetContent(ti->GetText(0));
						w->InsertTag(t);
					}
				}
				
				App->GetOptions()->Unlock();
			}
		}
	}
	
	void OnCreate()
	{
		#if !USE_HAIKU_PULSE_HACK
		SetPulse(1000);
		#endif
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
					LgiTrace("Ch %i not utf len="LGI_PrintfInt64"\n", Channel, Size);
					continue;
				}
				
				GAutoPtr<char16, true> w(Utf8ToWide(Utf, Size));
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
					Txt[n]->GView::SetPos(c);
			}

			FtpLog->SetPos(c);
		}
	}
};

int DocSorter(IdeDoc *a, IdeDoc *b, NativeInt d)
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
	bool Running;
	bool Building;
	GSubMenu *WindowsMenu;
	GSubMenu *CreateMakefileMenu;
	FindSymbolSystem FindSym;
	GArray<GAutoString> SystemIncludePaths;
	GArray<GDebugger::BreakPoint> BreakPoints;
	
	// Debugging
	GDebugContext *DbgContext;
	
	// Cursor history tracking
	int HistoryLoc;
	GArray<FileLoc> CursorHistory;
	bool InHistorySeek;
	
	void SeekHistory(int Direction)
	{
		if (CursorHistory.Length())
		{
			HistoryLoc += Direction;
			if (HistoryLoc < 1) HistoryLoc = 1;
			if (HistoryLoc > CursorHistory.Length()) HistoryLoc = CursorHistory.Length();

			FileLoc &Loc = CursorHistory[HistoryLoc-1];
			App->GotoReference(Loc.File, Loc.Line, false, false);
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
		DbgContext = NULL;
		Finder = 0;
		Output = 0;
		Debugging = false;
		Running = false;
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
		Output->Save();
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

	bool FindSource(GAutoString &Full, char *File, char *Context)
	{
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
				LgiTrace("%s:%i - Context '%s' not found in project.\n", _FL, Context);
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
		
		return ValidStr(Full);
	}

	void ViewMsg(char *File, int Line, char *Context)
	{
		GAutoString Full;
		if (FindSource(Full, File, Context))
		{
			IdeDoc *Doc = App->GotoReference(Full, Line, false);			
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
						GAutoString File(WideToUtf8(Txt+Line, i-Line));
						if (File)
						{
							// Scan over the linenumber..
							int NumIndex = ++i;
							while (isdigit(Txt[NumIndex])) NumIndex++;
							
							// Store the linenumber
							GAutoString NumStr(WideToUtf8(Txt + i, NumIndex - i));
							if (NumStr)
							{
								// Convert it to an integer
								int LineNumber = atoi(NumStr);
								o->SetCursor(Line, false);
								o->SetCursor(NumIndex + 1, true);
								
								char *Context8 = WideToUtf8(Context);
								ViewMsg(File, LineNumber, Context8);
								DeleteArray(Context8);
							}
						}
					}					
				}
			}
		}
	}

	void UpdateMenus()
	{
		static const char *None = "(none)";

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
				const char *File = d->GetFileName();
				if (!File) File = "(untitled)";
				char *Dir = strrchr((char*)File, DIR_CHAR);
				WindowsMenu->AppendItem(Dir?Dir+1:File, IDM_WINDOWS+n++, true);
			}
			
			if (!Docs.First())
			{
				WindowsMenu->AppendItem(None, 0, false);
			}
		}
	}
	
	void OnFile(const char *File, bool IsProject = false)
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
						LgiTrace("Remove '%s'\n", f);

						(*r)->Delete(f);
						DeleteArray(f);
						break;
					}
				}
			}

			UpdateMenus();
		}
	}

	IdeDoc *IsFileOpen(const char *File)
	{
		if (!File)
		{
			LgiTrace("%s:%i - No input File?\n", _FL);
			return NULL;
		}
		
		for (IdeDoc *Doc = Docs.First(); Doc; Doc = Docs.Next())
		{
			if (Doc->IsFile(File))
			{
				return Doc;
			}
		}

		// LgiTrace("%s:%i - '%s' not found in %i docs.\n", _FL, File, Docs.Length());
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

	void SerializeStringList(const char *Opt, List<char> *Lst, bool Write)
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

	GRect r(0, 0, 1300, 900);
	#ifdef BEOS
	r.Offset(GdcD->X() - r.X() - 10, GdcD->Y() - r.Y() - 10);
	SetPos(r);
	#else
	SetPos(r);
	MoveToCenter();
	#endif

	d = new AppWndPrivate(this);
	Name(AppName);
	SetQuitOnClose(true);

	#if WINNATIVE
	SetIcon((char*)MAKEINTRESOURCE(IDI_APP));
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
				else LgiTrace("%s:%i - FindSubMenu failed.\n", _FL);

				GMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
				if (Debug)
				{
					Debug->Checked(true);
				}
				else LgiTrace("%s:%i - FindSubMenu failed.\n", _FL);
				
				d->UpdateMenus();
			}
		}

		GToolBar *Tools;
		if (GdcD->Y() > 1200)
			Tools = LgiLoadToolbar(this, "cmds-32px.png", 32, 32);
		else
			Tools = LgiLoadToolbar(this, "cmds-16px.png", 16, 16);
		
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
			// Tools->AppendButton("Execute", IDM_EXECUTE, TBT_PUSH, true, CMD_EXECUTE);
			
			Tools->AppendSeparator();
			Tools->AppendButton("Debug", IDM_START_DEBUG, TBT_PUSH, true, CMD_DEBUG);
			Tools->AppendButton("Pause", IDM_PAUSE_DEBUG, TBT_PUSH, true, CMD_PAUSE);
			Tools->AppendButton("Restart", IDM_RESTART_DEBUGGING, TBT_PUSH, true, CMD_RESTART);
			Tools->AppendButton("Kill", IDM_STOP_DEBUG, TBT_PUSH, true, CMD_KILL);
			Tools->AppendButton("Step Into", IDM_STEP_INTO, TBT_PUSH, true, CMD_STEP_INTO);
			Tools->AppendButton("Step Over", IDM_STEP_OVER, TBT_PUSH, true, CMD_STEP_OVER);
			Tools->AppendButton("Step Out", IDM_STEP_OUT, TBT_PUSH, true, CMD_STEP_OUT);
			Tools->AppendButton("Run To", IDM_RUN_TO, TBT_PUSH, true, CMD_RUN_TO);

			Tools->AppendSeparator();
			Tools->AppendButton("Find In Files", IDM_FIND_IN_FILES, TBT_PUSH, true, CMD_FIND_IN_FILES);
			
			Tools->Attach(this);
		}
		
		d->Output = new IdeOutput(this);
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
			GVariant v = 270;
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
			if (d->Mdi)
			{
				d->Mdi->HasButton(true);
			}
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
	
	#if USE_HAIKU_PULSE_HACK
	d->Output->SetPulse(1000);
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

GDebugContext *AppWnd::GetDebugContext()
{
	return d->DbgContext;
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

void AppWnd::OnDebugState(bool Debugging, bool Running)
{
	// Make sure this event is processed in the GUI thread.
	#if DEBUG_SESSION_LOGGING
	LgiTrace("AppWnd::OnDebugState(%i,%i) InThread=%i\n", Debugging, Running, InThread());
	#endif
	
	PostEvent(M_DEBUG_ON_STATE, Debugging, Running);
}

void AppWnd::UpdateState(int Debugging, int Building)
{
	if (Debugging >= 0) d->Debugging = Debugging;
	if (Building >= 0) d->Building = Building;

	SetCtrlEnabled(IDM_COMPILE, !d->Building);
	SetCtrlEnabled(IDM_BUILD, !d->Building);
	SetCtrlEnabled(IDM_STOP_BUILD, d->Building);
	// SetCtrlEnabled(IDM_RUN, !d->Building);
	// SetCtrlEnabled(IDM_TOGGLE_BREAKPOINT, !d->Building);
	
	SetCtrlEnabled(IDM_START_DEBUG, !d->Debugging && !d->Building);
	SetCtrlEnabled(IDM_PAUSE_DEBUG, d->Debugging);
	SetCtrlEnabled(IDM_RESTART_DEBUGGING, d->Debugging);
	SetCtrlEnabled(IDM_STOP_DEBUG, d->Debugging);
	SetCtrlEnabled(IDM_STEP_INTO, d->Debugging);
	SetCtrlEnabled(IDM_STEP_OVER, d->Debugging);
	SetCtrlEnabled(IDM_STEP_OUT, d->Debugging);
	SetCtrlEnabled(IDM_RUN_TO, d->Debugging);
}

void AppWnd::AppendOutput(char *Txt, AppWnd::Channels Channel)
{
	if (!d->Output)
	{
		LgiTrace("%s:%i - No output panel.\n", _FL);
		return;
	}
	
	if (Channel < 0 ||
		Channel >= CountOf(d->Output->Txt))
	{
		LgiTrace("%s:%i - Channel range: %i, %i.\n", _FL, Channel, CountOf(d->Output->Txt));
		return;
	}

	if (!d->Output->Txt[Channel])
	{
		LgiTrace("%s:%i - No log for channel %i.\n", _FL, Channel);
		return;
	}
	
	if (Txt)
	{
		d->Output->Buf[Channel].Add(Txt, strlen(Txt));
	}
	else
	{
		d->Output->Txt[Channel]->Name("");
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
	while (d->Docs.First())
		delete d->Docs.First();
	
	IdeProject *p = RootProject();
	if (p)
		DeleteObj(p);
	
	while (d->Projects.First())
		delete d->Projects.First();	

	DeleteObj(d->DbgContext);
}

bool AppWnd::OnRequestClose(bool IsClose)
{
	SaveAll();
	return GWindow::OnRequestClose(IsClose);
}

bool AppWnd::OnBreakPoint(GDebugger::BreakPoint &b, bool Add)
{
	List<IdeDoc>::I it = d->Docs.Start();

	for (IdeDoc *doc = *it; doc; doc = *++it)
	{
		char *fn = doc->GetFileName();
		bool Match = !_stricmp(fn, b.File);
		if (Match)
		{
			doc->AddBreakPoint(b.Line, Add);
		}
	}

	if (d->DbgContext)
	{
		d->DbgContext->OnBreakPoint(b, Add);
	}
	
	return true;
}

bool AppWnd::LoadBreakPoints(IdeDoc *doc)
{
	if (!doc)
		return false;

	char *fn = doc->GetFileName();
	for (int i=0; i<d->BreakPoints.Length(); i++)
	{
		GDebugger::BreakPoint &b = d->BreakPoints[i];
		if (!_stricmp(fn, b.File))
		{
			doc->AddBreakPoint(b.Line, true);
		}
	}

	return true;
}

bool AppWnd::LoadBreakPoints(GDebugger *db)
{
	if (!db)
		return false;

	for (int i=0; i<d->BreakPoints.Length(); i++)
	{
		GDebugger::BreakPoint &bp = d->BreakPoints[i];
		db->SetBreakPoint(&bp);
	}

	return true;
}

bool AppWnd::ToggleBreakpoint(const char *File, int Line)
{
	bool DeleteBp = false;

	for (int i=0; i<d->BreakPoints.Length(); i++)
	{
		GDebugger::BreakPoint &b = d->BreakPoints[i];
		if (!_stricmp(File, b.File) &&
			b.Line == Line)
		{
			OnBreakPoint(b, false);
			d->BreakPoints.DeleteAt(i);
			DeleteBp = true;
			break;
		}
	}
	
	if (!DeleteBp)
	{
		GDebugger::BreakPoint &b = d->BreakPoints.New();
		b.File = File;
		b.Line = Line;
		OnBreakPoint(b, true);
	}
	
	return true;
}

void AppWnd::OnLocationChange(const char *File, int Line)
{
	if (!File)
		return;

	if (!d->InHistorySeek)
	{
		// Destroy any history after the current...
		d->CursorHistory.Length(d->HistoryLoc++);

		if (d->CursorHistory.Length() > 0)
		{
			FileLoc &Last = d->CursorHistory.Last();
			if (_stricmp(File, Last.File) == 0 && abs(Last.Line - Line) <= 1)
			{
				// Previous or next line... just update line number
				Last.Line = Line;
				d->HistoryLoc--;
				return;
			}
		}
		
		// Add new entry
		d->CursorHistory.New().Set(File, Line);

		#if 0
		int alloc = 1;
		while (alloc < d->CursorHistory.Length())
		{
			alloc <<=1;
		}
		FileLoc *p = &d->CursorHistory[0];
		LgiTrace("Added %s, %i to %i items. Idx=%i\n", File, Line, d->CursorHistory.Length(), d->HistoryLoc);
		for (int i=0; i<alloc; i++)
		{
			LgiTrace("    [%i] = %s, %i\n", i, p[i].File.Get(), p[i].Line);
		}
		#endif
		
	}
}

void AppWnd::OnFile(char *File, bool IsProject)
{
	d->OnFile(File, IsProject);
}

IdeDoc *AppWnd::NewDocWnd(const char *FileName, NodeSource *Src)
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

IdeDoc *AppWnd::GetCurrentDoc()
{
	if (d->Mdi)
		return dynamic_cast<IdeDoc*>(d->Mdi->GetTop());
	return NULL;
}

IdeDoc *AppWnd::GotoReference(const char *File, int Line, bool CurIp, bool WithHistory)
{
	if (!WithHistory)
		d->InHistorySeek = true;

	IdeDoc *Doc = File ? OpenFile(File) : GetCurrentDoc();
	if (Doc)
	{
		Doc->SetLine(Line, CurIp);
	}
	else LgiTrace("%s:%i - No file '%s' found.\n", _FL, File);

	if (!WithHistory)
		d->InHistorySeek = true;

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

IdeDoc *AppWnd::OpenFile(const char *FileName, NodeSource *Src)
{
	static bool DoingProjectFind = false;
	IdeDoc *Doc = 0;
	
	const char *File = Src ? Src->GetFileName() : FileName;
	if (Src || ValidStr(File))
	{
		Doc = d->IsFileOpen(File);

		if (!Doc)
		{
			if (Src)
			{
				Doc = NewDocWnd(File, Src);
			}
			else if (!DoingProjectFind)
			{
				DoingProjectFind = true;
				List<IdeProject>::I Proj = d->Projects.Start();
				for (IdeProject *p=*Proj; p && !Doc; p=*++Proj)
				{
					p->InProject(true, File, true, &Doc);				
				}
				DoingProjectFind = false;

				d->OnFile(File);
			}
		}

		if (!Doc)
		{
			GString FullPath;
			if (LgiIsRelativePath(File))
			{
				IdeProject *Root = RootProject();
				if (Root)
				{
					GAutoString RootPath = Root->GetBasePath();
					char p[MAX_PATH];
					LgiMakePath(p, sizeof(p), RootPath, File);
					if (FileExists(p))
					{
						FullPath = p;
						File = FullPath;
						printf("Converted '%s' to '%s'\n", File, p);
					}
					else
					{
						printf("Rel Path '%s' doesn't exist\n", p);
					}
				}
			}
			
			if (FileExists(File))
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
		}

		if (Doc)
		{
			#ifdef BEOS
			BView *h = Doc->Handle();
			BWindow *w = h ? h->Window() : 0;
			bool att = Doc->IsAttached();
			printf("%s:%i - att=%i h=%p w=%p\n", _FL, att, h, w);
			#endif
			
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
			GString::Array Inc;
			p->BuildIncludePaths(Inc, false, PlatformCurrent);
			d->FindSym.SetIncludePaths(Inc);

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

			GArray<ProjectNode*> Files;
			if (p && p->GetAllNodes(Files))
			{

				/* This is handling in ::OnNode now
				GAutoString Base = p->GetBasePath();
				for (unsigned i=0; i<Files.Length(); i++)
				{
					ProjectNode *n = Files[i];
					if (n)
					{
						char *Fn = n->GetFileName();
						if (Fn)
						{
							GFile::Path Path;
							if (LgiIsRelativePath(Fn))
							{
								Path = Base;
								Path += Fn;
							}
							else
							{
								Path = Fn;
							}
							
							d->FindSym.OnFile(Path, FindSymbolSystem::FileAdd);
						}
					}
				}
				*/
			}
		}
	}

	return p;
}

GMessage::Result AppWnd::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_BUILD_DONE:
		{
			UpdateState(-1, false);
			
			IdeProject *p = RootProject();
			if (p)
				p->StopBuild();
			break;
		}
		case M_BUILD_ERR:
		{
			char *Msg = (char*)MsgB(m);
			if (Msg)
			{
				d->Output->Txt[AppWnd::BuildTab]->Print("Build Error: %s\n", Msg);
				DeleteArray(Msg);
			}
			break;
		}
		case M_APPEND_TEXT:
		{
			char *Text = (char*) MsgA(m);
			Channels Ch = (Channels) MsgB(m);
			AppendOutput(Text, Ch);
			DeleteArray(Text);
			break;
		}
		case M_DEBUG_ON_STATE:
		{
			bool Debugging = m->A();
			bool Running = m->B();

			if (d->Running != Running)
			{
				d->Running = Running;
				if (!d->Running &&
					d->Output &&
					d->Output->DebugTab)
				{
					d->Output->DebugTab->SendNotify();
				}
			}
			if (d->Debugging != Debugging)
			{
				d->Debugging = Debugging;
				if (!Debugging)
				{
					IdeDoc::ClearCurrentIp();
					IdeDoc *c = GetCurrentDoc();
					if (c && c->GetEdit())
						c->GetEdit()->Invalidate();
						
					// Shutdown the debug context and free the memory
					DeleteObj(d->DbgContext);
				}
			}
			
			SetCtrlEnabled(IDM_START_DEBUG, !Debugging || !Running);
			SetCtrlEnabled(IDM_PAUSE_DEBUG, Debugging && Running);
			SetCtrlEnabled(IDM_RESTART_DEBUGGING, Debugging);
			SetCtrlEnabled(IDM_STOP_DEBUG, Debugging);

			SetCtrlEnabled(IDM_STEP_INTO, Debugging && !Running);
			SetCtrlEnabled(IDM_STEP_OVER, Debugging && !Running);
			SetCtrlEnabled(IDM_STEP_OUT, Debugging && !Running);
			SetCtrlEnabled(IDM_RUN_TO, Debugging && !Running);
			break;
		}
		default:
		{
			if (d->DbgContext)
				d->DbgContext->OnEvent(m);
			break;
		}
	}

	return GWindow::OnEvent(m);
}

bool AppWnd::OnNode(const char *Path, ProjectNode *Node, bool Add)
{
	// This takes care of adding/removing files from the symbol search engine.
	if (!Path || !Node)
		return false;

	d->FindSym.OnFile(Path, Add ? FindSymbolSystem::FileAdd : FindSymbolSystem::FileRemove);
	return true;
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

void AppWnd::UpdateMemoryDump()
{
	if (d->DbgContext)
	{
		char *sWord = GetCtrlName(IDC_MEM_SIZE);
		int iWord = sWord ? atoi(sWord) : 1;
		int64 RowLen = GetCtrlValue(IDC_MEM_ROW_LEN);
		bool InHex = GetCtrlValue(IDC_MEM_HEX) != 0;

		d->DbgContext->FormatMemoryDump(iWord, RowLen, InHex);
	}
}

int AppWnd::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_DEBUG_EDIT:
		{
			if (Flags == VK_RETURN && d->DbgContext)
			{
				char *Cmd = Ctrl->Name();
				if (Cmd)
				{
					d->DbgContext->OnUserCommand(Cmd);
					Ctrl->Name(NULL);
				}
			}
			break;
		}
		case IDC_MEM_ADDR:
		{
			if (Flags == VK_RETURN)
			{
				if (d->DbgContext)
				{
					char *s = Ctrl->Name();
					if (s)
					{
						char *sWord = GetCtrlName(IDC_MEM_SIZE);
						int iWord = sWord ? atoi(sWord) : 1;
						d->DbgContext->OnMemoryDump(s, iWord, GetCtrlValue(IDC_MEM_ROW_LEN), GetCtrlValue(IDC_MEM_HEX) != 0);
					}
					else if (d->DbgContext->MemoryDump)
					{
						d->DbgContext->MemoryDump->Print("No address specified.");
					}
					else
					{
						LgiAssert(!"No MemoryDump.");
					}
				}
				else LgiAssert(!"No debug context.");
			}
			break;
		}
		case IDC_MEM_ROW_LEN:
		{
			if (Flags == VK_RETURN)
				UpdateMemoryDump();
			break;
		}
		case IDC_MEM_HEX:
		case IDC_MEM_SIZE:
		{
			UpdateMemoryDump();
			break;
		}
		case IDC_DEBUG_TAB:
		{
			printf("notify IDC_DEBUG_TAB %i %i\n", Flags == GNotifyValueChanged, (int)Ctrl->Value());
			if (d->DbgContext && Flags == GNotifyValueChanged)
			{
				switch (Ctrl->Value())
				{
					case AppWnd::LocalsTab:
					{
						d->DbgContext->UpdateLocals();
						break;
					}
					case AppWnd::WatchTab:
					{
						d->DbgContext->UpdateWatches();
						break;
					}
					case AppWnd::RegistersTab:
					{
						d->DbgContext->UpdateRegisters();
						break;
					}
					case AppWnd::CallStackTab:
					{
						d->DbgContext->UpdateCallStack();
						break;
					}
					case AppWnd::ThreadsTab:
					{
						d->DbgContext->UpdateThreads();
						break;
					}
					default:
						break;
				}
			}
			break;
		}
		case IDC_LOCALS_LIST:
		{
			if (d->Output->Locals &&
				Flags == GNotifyItem_DoubleClick &&
				d->DbgContext)
			{
				GListItem *it = d->Output->Locals->GetSelected();
				if (it)
				{
					char *Var = it->GetText(2);
					char *Val = it->GetText(3);
					if (Var)
					{
						if (d->Output->DebugTab)
							d->Output->DebugTab->Value(AppWnd::ObjectTab);
							
						d->DbgContext->DumpObject(Var, Val);
					}
				}
			}
			break;
		}
		case IDC_CALL_STACK:
		{
			if (Flags == M_CHANGE)
			{
				if (d->Output->DebugTab)
					d->Output->DebugTab->Value(AppWnd::CallStackTab);
			}
			else if (Flags == GNotifyItem_Select)
			{
				// This takes the user to a given call stack reference
				if (d->Output->CallStack && d->DbgContext)
				{
					GListItem *item = d->Output->CallStack->GetSelected();
					if (item)
					{
						GAutoString File;
						int Line;
						if (d->DbgContext->ParseFrameReference(item->GetText(1), File, Line))
						{
							GAutoString Full;
							if (d->FindSource(Full, File, NULL))
							{
								GotoReference(Full, Line, false);
								
								char *sFrame = item->GetText(0);
								if (sFrame && IsDigit(*sFrame))
									d->DbgContext->SetFrame(atoi(sFrame));
							}
						}
					}
				}
			}
			break;
		}
		case IDC_WATCH_LIST:
		{
			WatchItem *Edit = NULL;
			switch (Flags)
			{
				case GNotify_DeleteKey:
				{
					GArray<GTreeItem *> Sel;
					for (GTreeItem *c = d->Output->Watch->GetChild(); c; c = c->GetNext())
					{
						if (c->Select())
							Sel.Add(c);
					}
					Sel.DeleteObjects();
					break;
				}
				case GNotifyItem_Click:
				{
					Edit = dynamic_cast<WatchItem*>(d->Output->Watch->Selection());
					break;
				}
				case GNotifyContainer_Click:
				{
					// Create new watch.
					Edit = new WatchItem(d->Output);
					if (Edit)
						d->Output->Watch->Insert(Edit);
					break;
				}
			}
			
			if (Edit)
				Edit->EditLabel(0);
			break;
		}
		case IDC_THREADS:
		{
			if (Flags == GNotifyItem_Select)
			{
				// This takes the user to a given thread
				if (d->Output->Threads && d->DbgContext)
				{
					GListItem *item = d->Output->Threads->GetSelected();
					if (item)
					{
						GString sId = item->GetText(0);
						int ThreadId = sId.Int();
						if (ThreadId > 0)
						{
							d->DbgContext->SelectThread(ThreadId);
						}
					}
				}
			}
			break;
		}
	}
	
	return 0;
}

bool AppWnd::IsReleaseMode()
{
	GMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
	bool IsRelease = Release ? Release->Checked() : false;
	return IsRelease;
}

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
			DeleteObj(d->DbgContext);
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
			GTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->Undo();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_REDO:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->Redo();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_FIND:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->DoFind();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_FIND_NEXT:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->DoFindNext();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_REPLACE:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->DoReplace();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_GOTO:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->DoGoto();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_CUT:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
				Doc->PostEvent(M_CUT);
			break;
		}
		case IDM_COPY:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
				Doc->PostEvent(M_COPY);
			break;
		}
		case IDM_PASTE:
		{
			GTextView3 *Doc = FocusEdit();
			if (Doc)
				Doc->PostEvent(M_PASTE);
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
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GotoSearch(IDC_SYMBOL_SEARCH);
			}
			else
			{
				FindSymResult r = d->FindSym.OpenSearchDlg(this);
				if (r.File)
				{
					GotoReference(r.File, r.Line, false);
				}
			}
			break;
		}
		case IDM_GOTO_SYMBOL:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->SearchSymbol();
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
		case IDM_RUN:
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
		case IDM_START_DEBUG:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (!p)
			{
				LgiMsg(this, "No project loaded.", "Error");
				break;
			}

			if (d->DbgContext)
			{
				d->DbgContext->OnCommand(IDM_CONTINUE);
			}
			else if ((d->DbgContext = p->Execute(ExeDebug)))
			{
				d->DbgContext->DebuggerLog = d->Output->DebuggerLog;
				d->DbgContext->Watch = d->Output->Watch;
				d->DbgContext->Locals = d->Output->Locals;
				d->DbgContext->CallStack = d->Output->CallStack;
				d->DbgContext->Threads = d->Output->Threads;
				d->DbgContext->ObjectDump = d->Output->ObjectDump;
				d->DbgContext->Registers = d->Output->Registers;
				d->DbgContext->MemoryDump = d->Output->MemoryDump;
				
				d->DbgContext->OnCommand(IDM_START_DEBUG);
				
				d->Output->Tab->Value(AppWnd::DebugTab);
				d->Output->DebugEdit->Focus(true);
			}
			break;
		}
		case IDM_TOGGLE_BREAKPOINT:
		{
			IdeDoc *Cur = GetCurrentDoc();
			if (Cur)
			{
				if (Cur->GetEdit())
				{
					ToggleBreakpoint(Cur->GetFileName(), Cur->GetEdit()->GetLine());
				}
			}
			break;
		}
		case IDM_ATTACH_TO_PROCESS:
		case IDM_PAUSE_DEBUG:
		case IDM_RESTART_DEBUGGING:
		case IDM_RUN_TO:
		case IDM_STEP_INTO:
		case IDM_STEP_OVER:
		case IDM_STEP_OUT:
		{
			if (d->DbgContext)
				d->DbgContext->OnCommand(Cmd);
			break;
		}
		case IDM_STOP_DEBUG:
		{
			if (d->DbgContext && d->DbgContext->OnCommand(Cmd))
			{
				DeleteObj(d->DbgContext);
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

				GMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
				bool IsRelease = Release ? Release->Checked() : false;
				p->Build(false, IsRelease);
			}
			break;			
		}
		case IDM_STOP_BUILD:
		{
			IdeProject *p = RootProject();
			if (p)
				p->StopBuild();
			break;
		}
		case IDM_CLEAN:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				p->Clean(IsReleaseMode());
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

GTextView3 *AppWnd::FocusEdit()
{
	return dynamic_cast<GTextView3*>(GetWindow()->GetFocus());	
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
			LgiTrace("%s:%i - Edit doesn't have focus, f=%p %s doc.edit=%p %s\n",
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

GStream *AppWnd::GetBuildLog()
{
	return d->Output->Txt[AppWnd::BuildTab];
}

void AppWnd::FindSymbol(GEventSinkI *Results, const char *Sym)
{
	d->FindSym.Search(Results, Sym);
}

#include "GSubProcess.h"
bool AppWnd::GetSystemIncludePaths(::GArray<GString> &Paths)
{
	if (d->SystemIncludePaths.Length() == 0)
	{
		#if !defined(WINNATIVE)
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
		#else
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
					GXmlTag *Opts = r.GetChildTag("ToolsOptions");
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
								char *Bar = strchr(prop->GetContent(), '|');
								GToken t(Bar ? Bar + 1 : prop->GetContent(), ";");
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

#include "GSubProcess.h"
void Test()
{
	/*
	int r;

	#if 1
	// Basic test
	GSubProcess p1("grep", "-i tiff *.csv");
	p1.SetInitFolder("c:\\Users\\matthew");
	p1.Start(true, false);
	#else
	// More complex test
	GSubProcess p1("dir");
	GSubProcess p2("grep", "test");
	p1.Connect(&p2);
	p1.Start(true, false);
	#endif

	char Buf[256];
	while ((r = p1.Read(Buf, sizeof(Buf))) > 0)
	{
		// So something with 'Buf'
		Buf[r] = 0;
	}
	*/

}

int LgiMain(OsAppArguments &AppArgs)
{
	printf("LgiIde v%s\n", LgiIdeVer);
	GApp a(AppArgs, "LgiIde");
	if (a.IsOk())
	{
		Test();
		
		a.AppWnd = new AppWnd;
		// a.AppWnd = new Test;
		a.Run();
	}

	return 0;
}

