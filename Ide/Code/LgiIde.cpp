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
#include "GBox.h"
#include "GSubProcess.h"
#include "GAbout.h"

#define IDM_RECENT_FILE			1000
#define IDM_RECENT_PROJECT		1100
#define IDM_WINDOWS				1200
#define IDM_MAKEFILE_BASE		1300

#define USE_HAIKU_PULSE_HACK	1

#define OPT_ENTIRE_SOLUTION		"SearchSolution"
#define OPT_SPLIT_PX			"SplitPos"
#define OPT_OUTPUT_PX			"OutputPx"

#define IsSymbolChar(c)			( IsDigit(c) || IsAlpha(c) || strchr("-_", c) )

//////////////////////////////////////////////////////////////////////////////////////////
class FindInProject : public GDialog
{
	AppWnd *App;
	LList *Lst;

public:
	FindInProject(AppWnd *app)
	{
		Lst = NULL;
		App = app;
		if (LoadFromResource(IDC_FIND_PROJECT_FILE))
		{
			MoveSameScreen(App);

			GViewI *v;
			if (GetViewById(IDC_TEXT, v))
				v->Focus(true);
			if (!GetViewById(IDC_FILES, Lst))
				return;

			RegisterHook(this, GKeyEvents, 0);
		}
	}

	bool OnViewKey(GView *v, GKey &k)
	{
		switch (k.vkey)
		{
			case VK_UP:
			case VK_DOWN:
			case VK_PAGEDOWN:
			case VK_PAGEUP:
			{
				return Lst->OnKey(k);
				break;
			}
			case VK_RETURN:
			{
				LListItem *i = Lst->GetSelected();
				if (i)
				{
					char *Ref = i->GetText(0);
					App->GotoReference(Ref, 1, false);
				}
				EndModal(1);
				break;
			}
			case VK_ESCAPE:
			{
				EndModal(0);
				break;
			}
		}
	
		return false;
	}

	void Search(const char *s)
	{
		IdeProject *p = App->RootProject();
		if (!p || !s)
			return;
		
		GArray<ProjectNode*> Matches, Nodes;

		List<IdeProject> All;
		p->GetChildProjects(All);
		All.Insert(p);							
		for (p=All.First(); p; p=All.Next())
		{
			p->GetAllNodes(Nodes);
		}

		FilterFiles(Matches, Nodes, s);

		Lst->Empty();
		for (unsigned i=0; i<Matches.Length(); i++)
		{
			LListItem *li = new LListItem;
			li->SetText(Matches[i]->GetFileName());
			Lst->Insert(li);
		}

		Lst->ResizeColumnsToContent();
	}

	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDC_FILES:
				if (f == GNotifyItem_DoubleClick)
				{
					LListItem *i = Lst->GetSelected();
					if (i)
					{
						App->GotoReference(i->GetText(0), 1, false);
						EndModal(1);
					}
				}
				break;
			case IDC_TEXT:
				if (f != GNotify_ReturnKey)
					Search(c->Name());
				break;
			case IDCANCEL:
				EndModal(0);
				break;
		}

		return 0;
	}
};

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
	
	char *GetFile()
	{
		return File;
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
	
	void PourText(size_t Start, ssize_t Len) override
	{
		GTextView3::PourText(Start, Len);

		for (GTextLine *l=Line.First(); l; l=Line.Next())
		{
			char16 *t = Text + l->Start;
			
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

WatchItem::WatchItem(IdeOutput *out, const char *Init)
{
	Out = out;
	Expanded(false);
	if (Init)
		SetText(Init);
	Insert(PlaceHolder = new GTreeItem);
}

WatchItem::~WatchItem()
{
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

bool WatchItem::SetText(const char *s, int i)
{
	if (ValidStr(s))
	{
		GTreeItem::SetText(s, i);

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

class BuildLog : public GTextLog
{
public:
	BuildLog(int id) : GTextLog(id)
	{
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		List<GTextLine>::I it = GTextView3::Line.begin();
		for (GTextLine *ln = *it; ln; ln = *++it)
		{
			if (!ln->c.IsValid())
			{
				char16 *t = Text + ln->Start;
				char16 *Err = Strnistr(t, L"error", ln->Len);
				char16 *Undef = Strnistr(t, L"undefined reference", ln->Len);
				char16 *Warn = Strnistr(t, L"warning", ln->Len);
				
				if
				(
					(Err && strchr(":[", Err[5]))
					||
					(Undef != NULL)
				)
					ln->c.Rgb(222, 0, 0);
				else if (Warn && strchr(":[", Warn[7]))
					ln->c.Rgb(255, 128, 0);
				else
					ln->c.Set(LC_TEXT, 24);
			}
		}
	}
};

class IdeOutput : public GTabView
{
public:
	AppWnd *App;
	GTabPage *Build;
	GTabPage *Output;
	GTabPage *Debug;
	GTabPage *Find;
	GTabPage *Ftp;
	LList *FtpLog;
	GTextLog *Txt[3];
	GArray<char> Buf[3];
	GFont Small;
	GFont Fixed;

	GTabView *DebugTab;
	GBox *DebugBox;
	GBox *DebugLog;
	LList *Locals, *CallStack, *Threads;
	GTree *Watch;
	GTextLog *ObjectDump, *MemoryDump, *Registers;
	GTableLayout *MemTable;
	GEdit *DebugEdit;
	GTextLog *DebuggerLog;

	IdeOutput(AppWnd *app)
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
		Small.PointSize(Small.PointSize()-1);
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

		GetCss(true)->MinHeight("60px");

		Build = Append("Build");
		Output = Append("Output");
		Find = Append("Find");
		Ftp = Append("Ftp");
		Debug = Append("Debug");

		SetFont(&Small);
		Build->SetFont(&Small);
		Output->SetFont(&Small);
		Find->SetFont(&Small);
		Ftp->SetFont(&Small);
		Debug->SetFont(&Small);
			
		if (Build)
			Build->Append(Txt[AppWnd::BuildTab] = new BuildLog(IDC_BUILD_LOG));
		if (Output)
			Output->Append(Txt[AppWnd::OutputTab] = new GTextLog(IDC_OUTPUT_LOG));
		if (Find)
			Find->Append(Txt[AppWnd::FindTab] = new GTextLog(IDC_FIND_LOG));
		if (Ftp)
			Ftp->Append(FtpLog = new LList(104, 0, 0, 100, 100));
		if (Debug)
		{
			Debug->Append(DebugBox = new GBox);
			if (DebugBox)
			{
				DebugBox->SetVertical(false);
					
				if ((DebugTab = new GTabView(IDC_DEBUG_TAB)))
				{
					DebugTab->GetCss(true)->Padding("0px");
					DebugTab->SetFont(&Small);
					DebugBox->AddView(DebugTab);

					GTabPage *Page;
					if ((Page = DebugTab->Append("Locals")))
					{
						Page->SetFont(&Small);
						if ((Locals = new LList(IDC_LOCALS_LIST, 0, 0, 100, 100, "Locals List")))
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
							GTextLabel *txt;
							GEdit *ed;
							MemTable->SetFont(&Small);
							
							int x = 0, y = 0;
							GLayoutCell *c = MemTable->GetCell(x++, y);
							if (c)
							{
								c->VerticalAlign(GCss::VerticalMiddle);
								c->Add(txt = new GTextLabel(IDC_STATIC, 0, 0, -1, -1, "Address:"));
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
								c->Add(txt = new GTextLabel(IDC_STATIC, 0, 0, -1, -1, "Page width:"));
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
						if ((Threads = new LList(IDC_THREADS, 0, 0, 100, 100, "Threads")))
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
						if ((CallStack = new LList(IDC_CALL_STACK, 0, 0, 100, 100, "Call Stack")))
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
					DebugEdit->GetCss(true)->Height(GCss::Len(GCss::LenPx, (float)(SysFont->GetHeight() + 8)));
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
		}
	}

	~IdeOutput()
	{
	}

	const char *GetClass()
	{
		return "IdeOutput";
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
		AttachChildren();
	}

	void RemoveAnsi(GArray<char> &a)
	{
		char *s = a.AddressOf();
		char *e = s + a.Length();
		while (s < e)
		{
			if (*s == 0x7)
			{
				a.DeleteAt(s - a.AddressOf(), true);
				s--;
			}
			else if
			(
				*s == 0x1b
				&&
				s[1] >= 0x40
				&&
				s[1] <= 0x5f
			)
			{
				// ANSI seq
				char *end;
				if (s[1] == '[' &&
					s[2] == '0' &&
					s[3] == ';')
					end = s + 4;
				else
				{
					end = s + 2;
					while (end < e && !IsAlpha(*end))
					{
						end++;
					}
					if (*end) end++;
				}

				auto len = end - s;
				memmove(s, end, e - end);
				a.Length(a.Length() - len);
				s--;
			}
			s++;
		}
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
				#ifdef _DEBUG
				
				if (!LgiIsUtf8(Utf, (ssize_t)Size))
				{
					LgiTrace("Ch %i not utf len=" LPrintfInt64 "\n", Channel, Size);
					continue;
				}
				#endif

				RemoveAnsi(Buf[Channel]);
				
				GAutoPtr<char16, true> w(Utf8ToWide(Utf, (ssize_t)Size));
				char16 *OldText = Txt[Channel]->NameW();
				size_t OldLen = 0;
				if (OldText)
					OldLen = StrlenW(OldText);

				auto Cur = Txt[Channel]->GetCaret();
				Txt[Channel]->Insert(OldLen, w, StrlenW(w));
				if (Cur > OldLen - 1)
				{
					Txt[Channel]->SetCaret(OldLen + StrlenW(w), false);
				}
				Changed = Channel;
				Buf[Channel].Length(0);
				Txt[Channel]->Invalidate();
			}
		}
		
		if (Changed >= 0)
			Value(Changed);
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
	GBox *HBox, *VBox;
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
	GAutoPtr<FindSymbolSystem> FindSym;
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
			int Loc = HistoryLoc + Direction;
			if (Loc >= 0 && Loc < CursorHistory.Length())
			{
				HistoryLoc = Loc;
				FileLoc &Loc = CursorHistory[HistoryLoc];
				App->GotoReference(Loc.File, Loc.Line, false, false);

				App->DumpHistory();
			}
		}
	}
	
	// Find in files
	GAutoPtr<FindParams> FindParameters;
	GAutoPtr<FindInFilesThread> Finder;
	int AppHnd;
	
	// Mru
	List<char> RecentFiles;
	GSubMenu *RecentFilesMenu;
	List<char> RecentProjects;
	GSubMenu *RecentProjectsMenu;

	// Object
	AppWndPrivate(AppWnd *a) :
		AppHnd(GEventSinkMap::Dispatch.AddSink(a)),
		Options(GOptionsFile::DesktopMode, AppName)
	{
		FindSym.Reset(new FindSymbolSystem(AppHnd));
		HistoryLoc = 0;
		InHistorySeek = false;
		WindowsMenu = 0;
		App = a;
		HBox = VBox = NULL;
		Tree = 0;
		DbgContext = NULL;
		Output = 0;
		Debugging = false;
		Running = false;
		Building = false;
		RecentFilesMenu = 0;
		RecentProjectsMenu = 0;
		Icons = LgiLoadImageList("icons.png", 16, 16);

		Options.SerializeFile(false);
		App->SerializeState(&Options, "WndPos", true);

		SerializeStringList("RecentFiles", &RecentFiles, false);
		SerializeStringList("RecentProjects", &RecentProjects, false);
	}
	
	~AppWndPrivate()
	{
		FindSym.Reset();
		Finder.Reset();
		Output->Save();
		App->SerializeState(&Options, "WndPos", false);
		SerializeStringList("RecentFiles", &RecentFiles, true);
		SerializeStringList("RecentProjects", &RecentProjects, true);
		Options.SerializeFile(true);
		
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
			List<IdeProject>::I Projs = Projects.begin();
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
			App->GotoReference(Full, Line, false);
		}
	}
	
	void GetContext(char16 *Txt, ssize_t &i, char16 *&Context)
	{
		static char16 NMsg[] = L"In file included ";
		static char16 FromMsg[] = L"from ";
		auto NMsgLen = StrlenW(NMsg);

		if (Txt[i] != '\n')
			return;

		if (StrncmpW(Txt + i + 1, NMsg, NMsgLen))
			return;

		i += NMsgLen + 1;								
				
		while (Txt[i])
		{
			// Skip whitespace
			while (Txt[i] && strchr(" \t\r\n", Txt[i]))
				i++;
					
			// Check for 'from'
			if (StrncmpW(FromMsg, Txt + i, 5))
				break;

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
	}

	#define PossibleLineSep(ch) \
		( (ch) == ':' || (ch) == '(' )
		
	void SeekMsg(int Direction)
	{
		GString Comp;
		IdeProject *p = App->RootProject();
		if (p)
			p ->GetSettings()->GetStr(ProjCompiler);
		// bool IsIAR = Comp.Equals("IAR");
		
		if (!Output)
			return;

		int64 Current = Output->Value();
		GTextView3 *o = Current < CountOf(Output->Txt) ? Output->Txt[Current] : 0;
		if (!o)
			return;

		char16 *Txt = o->NameW();
		if (!Txt)
			return;

		ssize_t Cur = o->GetCaret();
		char16 *Context = NULL;
		
		// Scan forward to the end of file for the next filename/line number separator.
		ssize_t i;
		for (i=Cur; Txt[i]; i++)
		{
			GetContext(Txt, i, Context);
						
			if
			(
				PossibleLineSep(Txt[i])
				&&
				isdigit(Txt[i+1])
			)
			{
				break;
			}
		}
					
		// If not found then scan from the start of the file for the next filename/line number separator.
		if (!PossibleLineSep(Txt[i]))
		{
			for (i=0; i<Cur; i++)
			{
				GetContext(Txt, i, Context);
							
				if
				(
					PossibleLineSep(Txt[i])
					&&
					isdigit(Txt[i+1])
				)
				{
					break;
				}
			}
		}
					
		// If match found?
		if (!PossibleLineSep(Txt[i]))
			return;

		// Scan back to the start of the filename
		auto Line = i;
		while (Line > 0 && Txt[Line-1] != '\n')
		{
			Line--;
		}
						
		// Store the filename
		GAutoString File(WideToUtf8(Txt+Line, i-Line));
		if (!File)
			return;

		// Scan over the line number..
		auto NumIndex = ++i;
		while (isdigit(Txt[NumIndex]))
			NumIndex++;
							
		// Store the line number
		GAutoString NumStr(WideToUtf8(Txt + i, NumIndex - i));
		if (!NumStr)
			return;

		// Convert it to an integer
		int LineNumber = atoi(NumStr);
		o->SetCaret(Line, false);
		o->SetCaret(NumIndex + 1, true);
								
		GString Context8 = Context;
		ViewMsg(File, LineNumber, Context8);
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
			Docs.Sort(DocSorter);
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
						// LgiTrace("Remove '%s'\n", f);

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
			
			ssize_t Size = (ssize_t)p.GetSize();
			
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

#ifdef COCOA
#define Chk printf("%s:%i - Cnt=%i\n", LgiGetLeaf(__FILE__), __LINE__, (int)WindowHandle().p.retainCount)
#else
#define Chk
#endif

AppWnd::AppWnd()
{
	#ifdef __GTK_H__
	LgiGetResObj(true, AppName);
	#endif
	
Chk;

	GRect r(0, 0, 1300, 900);
	#ifdef BEOS
	r.Offset(GdcD->X() - r.X() - 10, GdcD->Y() - r.Y() - 10);
	SetPos(r);
	#else
	SetPos(r);
Chk;
	MoveToCenter();
	#endif

Chk;

	d = new AppWndPrivate(this);
	Name(AppName);
	SetQuitOnClose(true);

Chk;

	#if WINNATIVE
	SetIcon((char*)MAKEINTRESOURCE(IDI_APP));
	#else
	SetIcon("Icon64.png");
	#endif
Chk;
	if (Attach(0))
	{
Chk;
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

Chk;
		GToolBar *Tools;
		if (GdcD->Y() > 1200)
			Tools = LgiLoadToolbar(this, "cmds-32px.png", 32, 32);
		else
			Tools = LgiLoadToolbar(this, "cmds-16px.png", 16, 16);
		
		if (Tools)
		{
Chk;
			Tools->AppendButton("New", IDM_NEW, TBT_PUSH, true, CMD_NEW);
			Tools->AppendButton("Open", IDM_OPEN, TBT_PUSH, true, CMD_OPEN);
			Tools->AppendButton("Save", IDM_SAVE_ALL, TBT_PUSH, true, CMD_SAVE_ALL);
			Tools->AppendSeparator();
			Tools->AppendButton("Cut", IDM_CUT, TBT_PUSH, true, CMD_CUT);
			Tools->AppendButton("Copy", IDM_COPY, TBT_PUSH, true, CMD_COPY);
			Tools->AppendButton("Paste", IDM_PASTE, TBT_PUSH, true, CMD_PASTE);
			Tools->AppendSeparator();
Chk;
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
			
			Tools->GetCss(true)->Padding("4px");
			Tools->Attach(this);
		}
		else LgiTrace("%s:%i - No tools obj?", _FL);
		
Chk;
		GVariant v = 270, OutPx = 250;
		d->Options.GetValue(OPT_SPLIT_PX, v);
		d->Options.GetValue(OPT_OUTPUT_PX, OutPx);

		AddView(d->VBox = new GBox);
		d->VBox->SetVertical(true);

		d->HBox = new GBox;
		d->VBox->AddView(d->HBox);
		d->VBox->AddView(d->Output = new IdeOutput(this));

		d->HBox->AddView(d->Tree = new IdeTree);
		if (d->Tree)
		{
			d->Tree->SetImageList(d->Icons, false);
			d->Tree->Sunken(false);
		}
		d->HBox->AddView(d->Mdi = new GMdiParent);
		if (d->Mdi)
		{
			d->Mdi->HasButton(true);
		}

Chk;
		d->HBox->Value(MAX(v.CastInt32(), 20));

		GRect c = GetClient();
		if (c.Y() > OutPx.CastInt32())
		{
			GCss::Len y(GCss::LenPx, OutPx.CastDouble());
			d->Output->GetCss(true)->Height(y);
		}

		AttachChildren();
		OnPosChange();
	
Chk;
		#ifdef LINUX
		GString f = LgiFindFile("lgiide.png");
		if (f)
		{
			// Handle()->setIcon(f);
		}
		#endif
		
		UpdateState();
		
Chk;
		Visible(true);
		DropTarget(true);

Chk;
		SetPulse(1000);
	}
	
	#ifdef LINUX
	LgiFinishXWindowsStartup(this);
	#endif
	
	#if USE_HAIKU_PULSE_HACK
	d->Output->SetPulse(1000);
	#endif
Chk;
}

AppWnd::~AppWnd()
{
	if (d->HBox)
	{
		GVariant v = d->HBox->Value();
		d->Options.SetValue(OPT_SPLIT_PX, v);
	}
	if (d->Output)
	{
		GVariant v = d->Output->Y();
		d->Options.SetValue(OPT_OUTPUT_PX, v);
	}

	ShutdownFtpThread();

	CloseAll();
	
	LgiApp->AppWnd = 0;
	DeleteObj(d);
}

void AppWnd::OnPulse()
{
	IdeDoc *Top = TopDoc();
	if (Top)
		Top->OnPulse();
}

GDebugContext *AppWnd::GetDebugContext()
{
	return d->DbgContext;
}

struct DumpBinThread : public LThread
{
	GStream *Out;
	GString InFile;

public:
	DumpBinThread(GStream *out, GString file) : LThread("DumpBin.Thread")
	{
		Out = out;
		InFile = file;
		DeleteOnExit = true;
		Run();
	}

	bool DumpBin(GString Args, GStream *Str)
	{
		char Buf[256];
		ssize_t Rd;

		GSubProcess s("c:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\bin\\amd64\\dumpbin.exe", Args);
		if (!s.Start(true, false))
			return false;

		while ((Rd = s.Read(Buf, sizeof(Buf))) > 0)
			Str->Write(Buf, Rd);

		return true;
	}

	GString::Array Dependencies()
	{
		GString Args;
		GStringPipe p;
		Args.Printf("/dependents \"%s\"", InFile.Get());
		DumpBin(Args, &p);

		auto Parts = p.NewGStr().Replace("\r", "").Split("\n\n");
		auto Files = Parts[4].Strip().Split("\n");
		auto Path = LGetPath();
		for (auto &f : Files)
		{
			f = f.Strip();

			bool Found = false;
			for (auto s : Path)
			{
				GFile::Path c(s);
				c += f.Get();
				if (c.IsFile())
				{
					f = c.GetFull();
					Found = true;
					break;
				}
			}

			if (!Found)
				f += " (not found in path)";
		}

		return Files;
	}

	GString GetArch()
	{
		GString Args;
		GStringPipe p;
		Args.Printf("/headers \"%s\"", InFile.Get());
		DumpBin(Args, &p);
	
		const char *Key = " machine ";
		GString Arch;
		auto Lines = p.NewGStr().SplitDelimit("\r\n");
		int64 Machine = 0;
		for (auto &Ln : Lines)
		{
			if (Ln.Find(Key) >= 0)
			{
				auto p = Ln.Strip().Split(Key);
				if (p.Length() == 2)
				{
					Arch = p[1].Strip("()");
					Machine = p[0].Int(16);
				}
			}
		}

		if (Machine == 0x14c)
			Arch += " 32bit";
		else if (Machine == 0x200)
			Arch += " 64bit Itanium";
		else if (Machine == 0x8664)
			Arch += " 64bit";

		return Arch;
	}

	GString GetExports()
	{
		GString Args;
		GStringPipe p;
		Args.Printf("/exports \"%s\"", InFile.Get());
		DumpBin(Args, &p);
	
		GString Exp;
		auto Sect = p.NewGStr().Replace("\r", "").Split("\n\n");
		bool Ord = false;
		for (auto &s : Sect)
		{
			if (s.Strip().Find("ordinal") == 0)
				Ord = true;
			else if (Ord)
			{
				Exp = s;
				break;
			}
			else Ord = false;
		}

		return Exp;
	}

	int Main()
	{
		auto Deps = Dependencies();
		Out->Print("Dependencies:\n\t%s\n\n", GString("\n\t").Join(Deps).Get());

		auto Arch = GetArch();
		if (Arch)
			Out->Print("Arch: %s\n\n", Arch.Get());

		auto Exp = GetExports();
		if (Arch)
			Out->Print("Exports:\n%s\n\n", Exp.Get());
		return 0;
	}
};

void AppWnd::OnReceiveFiles(GArray<char*> &Files)
{
	for (int i=0; i<Files.Length(); i++)
	{
		char *f = Files[i];
		// char *d = strrchr(f, DIR_CHAR);
		
		char *ext = LgiGetExtension(f);
		if (ext && !stricmp(ext, "mem"))
		{
			NewMemDumpViewer(this, f);
			return;
		}
		else if (ext && !stricmp(ext, "xml"))
		{
			if (!OpenProject(f, NULL))
				OpenFile(f);
		}
		else if
		(
			LgiIsFileNameExecutable(Files[i])
			/*
			ext
			&&
			(
				!stricmp(ext, "dll")
				||
				!stricmp(ext, "node")
			)
			*/
		)
		{
			// dumpbin /exports csp.dll
			GFile::Path Docs(LSP_USER_DOCUMENTS);
			GString Name;
			Name.Printf("%s.txt", Files[i]);
			Docs += Name;
			IdeDoc *Doc = NewDocWnd(NULL, NULL);
			if (Doc)
			{
				Doc->SetFileName(Docs, false);
				new DumpBinThread(Doc, Files[i]);
			}
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

void AppWnd::SaveAll()
{
	List<IdeDoc>::I Docs = d->Docs.begin();
	for (IdeDoc *Doc = *Docs; Doc; Doc = *++Docs)
	{
		Doc->SetClean();
		d->OnFile(Doc->GetFileName());
	}
	
	List<IdeProject>::I Projs = d->Projects.begin();
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
	List<IdeDoc>::I it = d->Docs.begin();

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

bool AppWnd::ToggleBreakpoint(const char *File, ssize_t Line)
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

void AppWnd::DumpHistory()
{
	#if 0
	LgiTrace("History %i of %i\n", d->HistoryLoc, d->CursorHistory.Length());
	for (int i=0; i<d->CursorHistory.Length(); i++)
	{
		FileLoc &p = d->CursorHistory[i];		
		LgiTrace("    [%i] = %s, %i %s\n", i, p.File.Get(), p.Line, d->HistoryLoc == i ? "<-----":"");
	}
	#endif
}

/*
void CheckHistory(GArray<FileLoc> &CursorHistory)
{
	if (CursorHistory.Length() > 0)
	{
		FileLoc *loc = &CursorHistory[0];
		for (unsigned i=CursorHistory.Length(); i<CursorHistory.GetAlloc(); i++)
		{
			if ((NativeInt)loc[i].File.Get() == 0xcdcdcdcdcdcdcdcd)
			{
				int asd=0;
			}
		}
	}
}
*/

void AppWnd::OnLocationChange(const char *File, int Line)
{
	if (!File)
		return;

	if (!d->InHistorySeek)
	{
		if (d->CursorHistory.Length() > 0)
		{
			FileLoc &Last = d->CursorHistory.Last();
			if (_stricmp(File, Last.File) == 0 && abs(Last.Line - Line) <= 1)
			{
				// Previous or next line... just update line number
				Last.Line = Line;
				DumpHistory();
				return;
			}

			// Add new entry
			d->HistoryLoc++;

			FileLoc &loc = d->CursorHistory[d->HistoryLoc];
			#ifdef WIN64
			if ((NativeInt)loc.File.Get() == 0xcdcdcdcdcdcdcdcd)
				LgiAssert(0); // wtf?
			else
			#endif
				loc.Set(File, Line);
		}
		else
		{
			// Add new entry
			d->CursorHistory[0].Set(File, Line);
		}

		// Destroy any history after the current...
		d->CursorHistory.Length(d->HistoryLoc+1);

		DumpHistory();
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
		Doc->SetLine(Line, CurIp);
	/*
	else
		LgiTrace("%s:%i - No file '%s' found.\n", _FL, File);
	*/

	if (!WithHistory)
		d->InHistorySeek = false;

	return Doc;			
}

IdeDoc *AppWnd::FindOpenFile(char *FileName)
{
	List<IdeDoc>::I it = d->Docs.begin();
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
	if (!Src && !ValidStr(File))
	{
		LgiTrace("%s:%i - No source or file?\n", _FL);
		return NULL;
	}
	
	GString FullPath;
	if (LgiIsRelativePath(File))
	{
		IdeProject *Proj = Src && Src->GetProject() ? Src->GetProject() : RootProject();
		if (Proj)
		{
			List<IdeProject> Projs;
			Projs.Insert(Proj);
			Proj->CollectAllSubProjects(Projs);

			for (auto Project : Projs)
			{
				GAutoString ProjPath = Project->GetBasePath();
				char p[MAX_PATH];
				LgiMakePath(p, sizeof(p), ProjPath, File);
				if (FileExists(p))
				{
					FullPath = p;
					File = FullPath;
					break;
				}
			}
		}
	}
		
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
			List<IdeProject>::I Proj = d->Projects.begin();
			for (IdeProject *p=*Proj; p && !Doc; p=*++Proj)
			{
				p->InProject(LgiIsRelativePath(File), File, true, &Doc);				
			}
			DoingProjectFind = false;

			d->OnFile(File);
		}
	}

	if (!Doc && FileExists(File))
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
	if (!FileName)
	{
		LgiTrace("%s:%i - Error: No filename.\n", _FL);
		return NULL;
	}
	
	if (d->IsProjectOpen(FileName))
	{
		LgiTrace("%s:%i - Warning: Project already open.\n", _FL);
		return NULL;
	}
	

	IdeProject *p = new IdeProject(this);
	if (!p)
	{
		LgiTrace("%s:%i - Error: mem alloc.\n", _FL);
		return NULL;
	}
	
	GString::Array Inc;
	p->BuildIncludePaths(Inc, false, false, PlatformCurrent);
	d->FindSym->SetIncludePaths(Inc);

	p->SetParentProject(ParentProj);
	
	ProjectStatus Status = p->OpenFile(FileName);
	if (Status == OpenOk)
	{
		d->Projects.Insert(p);
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
		LgiTrace("%s:%i - Failed to open '%s'\n", _FL, FileName);

		DeleteObj(p);
		if (Status == OpenError)
			d->RemoveRecent(FileName);
	}

	if (!GetTree()->Selection())
	{
		GetTree()->Select(GetTree()->GetChild());
	}

	GetTree()->Focus(true);

	return p;
}

GMessage::Result AppWnd::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_START_BUILD:
		{
			IdeProject *p = RootProject();
			if (p)
				p->Build(true, IsReleaseMode());
			else
				printf("%s:%i - No root project.\n", _FL);
			break;
		}
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
				bool RunToNotRun = d->Running && !Running;
				
				d->Running = Running;
				
				if (RunToNotRun &&
					d->Output &&
					d->Output->DebugTab)
				{
					d->Output->DebugTab->SendNotify(GNotifyValueChanged);
				}
			}
			if (d->Debugging != Debugging)
			{
				d->Debugging = Debugging;
				if (!Debugging)
				{
					IdeDoc::ClearCurrentIp();
					IdeDoc *c = GetCurrentDoc();
					if (c)
						c->UpdateControl();
						
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

bool AppWnd::OnNode(const char *Path, ProjectNode *Node, FindSymbolSystem::SymAction Action)
{
	// This takes care of adding/removing files from the symbol search engine.
	if (!Path || !Node)
		return false;

	d->FindSym->OnFile(Path, Action, Node->GetPlatforms());
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

		d->DbgContext->FormatMemoryDump(iWord, (int)RowLen, InHex);
	}
}

int AppWnd::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_PROJECT_TREE:
		{
			if (Flags == GNotify_DeleteKey)
			{
				ProjectNode *n = dynamic_cast<ProjectNode*>(d->Tree->Selection());
				if (n)
					n->Delete();
			}
			break;
		}
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
						d->DbgContext->OnMemoryDump(s, iWord, (int)GetCtrlValue(IDC_MEM_ROW_LEN), GetCtrlValue(IDC_MEM_HEX) != 0);
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
				LListItem *it = d->Output->Locals->GetSelected();
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
					LListItem *item = d->Output->CallStack->GetSelected();
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
					LListItem *item = d->Output->Threads->GetSelected();
					if (item)
					{
						GString sId = item->GetText(0);
						int ThreadId = (int)sId.Int();
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

bool AppWnd::Build()
{
	SaveAll();
	
	IdeDoc *Top;
	IdeProject *p = RootProject();
	if (p)
	{		
		UpdateState(-1, true);

		GMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
		bool IsRelease = Release ? Release->Checked() : false;
		p->Build(false, IsRelease);

		return true;
	}
	else if ((Top = TopDoc()))
	{
		return Top->Build();
	}

	return false;
}

bool AppWnd::ShowInProject(const char *Fn)
{
	if (!Fn)
		return false;
	
	for (IdeProject *p=d->Projects.First(); p; p=d->Projects.Next())
	{
		ProjectNode *Node = NULL;
		if (p->FindFullPath(Fn, &Node))
		{
			for (GTreeItem *i = Node->GetParent(); i; i = i->GetParent())
			{
				i->Expanded(true);
			}
			Node->Select(true);
			Node->ScrollTo();		
			return true;
		}	
	}
	
	return false;
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
		case IDM_HELP:
		{
			LgiExecute(APP_URL);
			break;
		}
		case IDM_ABOUT:
		{
			GAbout a(this,
					AppName, APP_VER,
					"\nLGI Integrated Development Environment",
					"icon128.png",
					APP_URL,
					"fret@memecode.com");
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
			if (Top)
				Top->SetClean();
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
				Doc->DoGoto();
			else
			{
				GInput Inp(this, NULL, LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto [file:]line:"), "Goto");
				if (Inp.DoModal())
				{
					GString s = Inp.GetStr();
					GString::Array p = s.SplitDelimit(":,");
					if (p.Length() == 2)
					{
						GString file = p[0];
						int line = (int)p[1].Int();
						GotoReference(file, line, false, true);
					}
					else LgiMsg(this, "Error: Needs a file name as well.", AppName);
				}
			}
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
			if (!d->Finder)
			{
				d->Finder.Reset(new FindInFilesThread(d->AppHnd));
			}
			if (d->Finder)
			{
				if (!d->FindParameters &&
					d->FindParameters.Reset(new FindParams))
				{
					GVariant var;
					if (GetOptions()->GetValue(OPT_ENTIRE_SOLUTION, var))
						d->FindParameters->Type = var.CastInt32() ? FifSearchSolution : FifSearchDirectory;
				}		

				FindInFiles Dlg(this, d->FindParameters);

				GViewI *Focus = GetFocus();
				if (Focus)
				{
					GTextView3 *Edit = dynamic_cast<GTextView3*>(Focus);
					if (Edit && Edit->HasSelection())
					{
						GAutoString a(Edit->GetSelection());
						Dlg.Params->Text = a;
					}
				}
				
				IdeProject *p = RootProject();
				if (p)
				{
					GAutoString Base = p->GetBasePath();
					if (Base)
						Dlg.Params->Dir = Base;
				}

				if (Dlg.DoModal())
				{
					if (p && Dlg.Params->Type == FifSearchSolution)
					{
						Dlg.Params->ProjectFiles.Length(0);

						
						List<IdeProject> Projects;
						Projects.Insert(p);
						p->GetChildProjects(Projects);

						GArray<ProjectNode*> Nodes;
						for (IdeProject *p = Projects.First(); p; p = Projects.Next())
							p->GetAllNodes(Nodes);

						for (unsigned i=0; i<Nodes.Length(); i++)
						{
							GString s = Nodes[i]->GetFullPath();
							if (s)
								Dlg.Params->ProjectFiles.Add(s);
						}
					}

					GVariant var = d->FindParameters->Type == FifSearchSolution;
					GetOptions()->SetValue(OPT_ENTIRE_SOLUTION, var);

					d->Finder->Stop();
					d->Finder->PostEvent(FindInFilesThread::M_START_SEARCH, (GMessage::Param) new FindParams(d->FindParameters));
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
				FindSymResult r = d->FindSym->OpenSearchDlg(this);
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
		case IDM_FIND_PROJECT_FILE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->SearchFile();
			}
			else
			{
				FindInProject Dlg(this);
				Dlg.DoModal();
			}
			break;
		}
		case IDM_FIND_REFERENCES:
		{
			GViewI *f = LgiApp->GetFocus();
			GDocView *doc = dynamic_cast<GDocView*>(f);
			if (!doc)
				break;

			ssize_t c = doc->GetCaret();
			if (c < 0)
				break;

			GString Txt = doc->Name();
			char *s = Txt.Get() + c;
			char *e = s;
			while (	s > Txt.Get() &&
					IsSymbolChar(s[-1]))
				s--;
			while (*e &&
					IsSymbolChar(*e))
				e++;
			if (e <= s)
				break;

			GString Word(s, e - s);

			if (!d->Finder)
				d->Finder.Reset(new FindInFilesThread(d->AppHnd));
			if (!d->Finder)
				break;

			IdeProject *p = RootProject();
			if (!p)
				break;
			List<IdeProject> Projects;
			Projects.Insert(p);
			p->GetChildProjects(Projects);

			GArray<ProjectNode*> Nodes;
			for (p = Projects.First(); p; p = Projects.Next())
				p->GetAllNodes(Nodes);

			GAutoPtr<FindParams> Params(new FindParams);
			Params->Type = FifSearchSolution;
			Params->MatchWord = true;
			Params->Text = Word;
			for (unsigned i = 0; i < Nodes.Length(); i++)
			{
				Params->ProjectFiles.New() = Nodes[i]->GetFullPath();
			}

			d->Finder->Stop();
			d->Finder->PostEvent(FindInFilesThread::M_START_SEARCH, (GMessage::Param) Params.Release());
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
		case IDM_FIX_MISSING_FILES:
		{
			IdeProject *p = RootProject();
			if (p)
				p->FixMissingFiles();
			else
				LgiMsg(this, "No project loaded.", AppName);
			break;
		}
		case IDM_FIND_DUPE_SYM:
		{
			IdeProject *p = RootProject();
			if (p)
				p->FindDuplicateSymbols();
			else
				LgiMsg(this, "No project loaded.", AppName);
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
				
				d->Output->Value(AppWnd::DebugTab);
				d->Output->DebugEdit->Focus(true);
			}
			break;
		}
		case IDM_TOGGLE_BREAKPOINT:
		{
			IdeDoc *Cur = GetCurrentDoc();
			if (Cur)
				ToggleBreakpoint(Cur->GetFileName(), Cur->GetLine());
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
			Build();
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
				p->Clean(true, IsReleaseMode());
			break;
		}
		case IDM_NEXT_MSG:
		{
			d->SeekMsg(1);
			break;
		}
		case IDM_PREV_MSG:
		{
			d->SeekMsg(-1);
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
		case IDM_LOOKUP_SYMBOLS:
		{
			IdeDoc *Cur = GetCurrentDoc();
			if (Cur)
			{
				// LookupSymbols(Cur->Read());
			}
			break;
		}
		case IDM_DEPENDS:
		{
			IdeProject *p = RootProject();
			if (p)
			{
				GString Exe = p->GetExecutable(GetCurrentPlatform());
				if (FileExists(Exe))
				{
					Depends Dlg(this, Exe);
				}
				else
				{
					LgiMsg(this, "Couldn't find '%s'\n", AppName, MB_OK, Exe ? Exe.Get() : "<project_executable_undefined>");
				}
			}			
			break;
		}
		case IDM_SP_TO_TAB:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->ConvertWhiteSpace(true);
			break;
		}
		case IDM_TAB_TO_SP:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->ConvertWhiteSpace(false);
			break;
		}
		case IDM_ESCAPE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->EscapeSelection(true);
			break;
		}
		case IDM_DESCAPE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->EscapeSelection(false);
			break;
		}
		case IDM_EOL_LF:
		{
			IdeDoc *Doc = FocusDoc();
			if (!Doc)
				break;
			Doc->SetCrLf(false);
			break;
		}
		case IDM_EOL_CRLF:
		{
			IdeDoc *Doc = FocusDoc();
			if (!Doc)
				break;
			Doc->SetCrLf(true);
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
					p->CreateMakefile(PlatIdx, false);
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
	if (Doc)
	{
		if (Doc->HasFocus())
		{
			return Doc;
		}
		else
		{
			GViewI *f = GetFocus();
			LgiTrace("%s:%i - Edit doesn't have focus, f=%p %s doc.edit=%s\n",
				_FL, f, f ? f->GetClass() : 0,
				Doc->Name());
		}
	}
	
	return 0;
}

void AppWnd::OnProjectDestroy(IdeProject *Proj)
{
	d->Projects.Delete(Proj);
}

void AppWnd::OnProjectChange()
{
	GArray<GMdiChild*> Views;
	if (d->Mdi->GetChildren(Views))
	{
		for (unsigned i=0; i<Views.Length(); i++)
		{
			IdeDoc *Doc = dynamic_cast<IdeDoc*>(Views[i]);
			if (Doc)
				Doc->OnProjectChange();
		}
	}
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

LList *AppWnd::GetFtpLog()
{
	return d->Output->FtpLog;
}

GStream *AppWnd::GetBuildLog()
{
	return d->Output->Txt[AppWnd::BuildTab];
}

void AppWnd::FindSymbol(int ResultsSinkHnd, const char *Sym, bool AllPlatforms)
{
	d->FindSym->Search(ResultsSinkHnd, Sym, AllPlatforms);
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
		ssize_t r;

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
		LGetSystemPath(LSP_USER_DOCUMENTS, p, sizeof(p));
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
#include "GSubProcess.h"
void Test()
{
	GDirectory d;
	for (int b = d.First("C:\\Users\\matthew\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cache"); b; b = d.Next())
	{
		if (!d.IsDir())
		{
			char p[MAX_PATH];
			d.Path(p, sizeof(p));
			
			GFile f;
			if (f.Open(p, O_READ))
			{
				char Buf[256];
				ssize_t Rd = f.Read(Buf, sizeof(Buf));
				if (Rd > 3 && !strnicmp(Buf, "Ogg", 3))
				{
					char out[MAX_PATH];
					f.Close();
					LgiMakePath(out, sizeof(out), "C:\\Users\\matthew\\Desktop\\new day", d.GetName());
					strcat(out, ".ogg");
					if (!FileDev->Copy(p, out))
					{
						LgiTrace("%s:%i - Failed to copy '%s'\n", _FL, d.GetName());
					}
				}
				else
				{
					LgiTrace("%s:%i - Not an ogg '%s'\n", _FL, d.GetName());
				}
			}
			else
			{
				LgiTrace("%s:%i - Can't open '%s'\n", _FL, d.GetName());
			}
		}
	}
}
*/

int LgiMain(OsAppArguments &AppArgs)
{
	printf("LgiIde v%s\n", APP_VER);
	GApp a(AppArgs, "LgiIde");
	if (a.IsOk())
	{
		/*
		GString mt = LGetAppForProtocol("mailto");
		GString https = LGetAppForProtocol("https");
		printf("%s\n%s\n", mt.Get(), https.Get());

		GArray<GSocket::Interface> Out;
		if (GSocket::EnumInterfaces(Out))
		{
			for (auto &i : Out)
			{
				printf("%s %s %s\n", i.Name.Get(), i.ToString().Get(), i.ToString(i.Netmask4).Get());
			}
		}
		*/

		a.AppWnd = new AppWnd;
		a.Run();
	}

	return 0;
}

