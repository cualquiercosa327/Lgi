#if defined(WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "resdefs.h"
#include "GProcess.h"
#include "GCombo.h"
#include "INet.h"
#include "GListItemCheckBox.h"
#include "FtpThread.h"
#include "GClipBoard.h"
#include "GDropFiles.h"
#include "GSubProcess.h"
#include "ProjectNode.h"
#include "WebFldDlg.h"
#include "GCss.h"

extern const char *Untitled;

#ifdef WIN32
#define LGI_STATIC_LIBRARY_EXT		"lib"
#else
#define LGI_STATIC_LIBRARY_EXT		"a"
#endif

const char *PlatformNames[] =
{
	"Windows",
	"Linux",
	"Mac",
	"Haiku",
	0
};

int PlatformCtrlId[] =
{
	IDC_WIN32,
	IDC_LINUX,
	IDC_MAC,
	IDC_HAIKU,
	0
};

const char SourcePatterns[] = "*.c;*.h;*.cpp;*.java;*.d;*.php;*.html;*.css";

char *ToUnixPath(char *s)
{
	if (s)
	{
		char *c;
		while ((c = strchr(s, '\\')))
		{
			*c = '/';
		}
	}
	return s;
}

GAutoString ToNativePath(const char *s)
{
	GAutoString a(NewStr(s));
	if (a)
	{
		if (strnicmp(a, "ftp://", 6))
		{
			for (char *c = a; *c; c++)
			{
				#ifdef WIN32
				if (*c == '/')
					*c = '\\';
				#else
				if (*c == '\\')
					*c = '/';
				#endif
			}
		}
	}
	return a;
}

//////////////////////////////////////////////////////////////////////////////////
class ProjectNode;

class BuildThread : public GThread, public GStream
{
	IdeProject *Proj;
	GAutoString Makefile;
	GAutoString Args;
	GAutoPtr<GSubProcess> SubProc;

	enum CompilerType
	{
		DefaultCompiler,
		VisualStudio,
		MingW,
		Gcc,
		CrossCompiler		
	}
	    Compiler;

public:
	BuildThread(IdeProject *proj, char *mf, const char *args = 0);
	~BuildThread();
	
	int Write(const void *Buffer, int Size, int Flags = 0);
	char *FindExe();
	GAutoString WinToMingWPath(const char *path);
	int Main();
};

class IdeProjectPrivate
{
public:
	AppWnd *App;
	IdeProject *Project;
	bool Dirty;
	GAutoString FileName;
	IdeProject *ParentProject;
	IdeProjectSettings Settings;
	GAutoPtr<BuildThread> Thread;

	IdeProjectPrivate(AppWnd *a, IdeProject *project) :
		Project(project),
		Settings(project)
	{
		App = a;
		Dirty = false;
		ParentProject = 0;
	}

	void CollectAllFiles(GTreeNode *Base, GArray<ProjectNode*> &Files, bool SubProjects, int Platform);
};

/////////////////////////////////////////////////////////////////////////////////////
NodeSource::~NodeSource()
{
	if (nView)
	{
		nView->nSrc = 0;
	}
}

NodeView::~NodeView()
{
	if (nSrc)
	{
		nSrc->nView = 0;
	}
}


//////////////////////////////////////////////////////////////////////////////////
int NodeSort(GTreeItem *a, GTreeItem *b, int d)
{
	ProjectNode *A = dynamic_cast<ProjectNode*>(a);
	ProjectNode *B = dynamic_cast<ProjectNode*>(b);
	if (A && B)
	{
		if
		(
			(A->GetType() == NodeDir)
			^
			(B->GetType() == NodeDir)
		)
		{
			return A->GetType() == NodeDir ? -1 : 1;
		}
		else
		{
			char *Sa = a->GetText(0);
			char *Sb = b->GetText(0);
			if (Sa && Sb)
			{
				return stricmp(Sa, Sb);
			}
		}
	}

	return 0;
}

int XmlSort(GXmlTag *a, GXmlTag *b, int d)
{
	GTreeItem *A = dynamic_cast<GTreeItem*>(a);
	GTreeItem *B = dynamic_cast<GTreeItem*>(b);
	return NodeSort(A, B, d);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BuildThread::BuildThread(IdeProject *proj, char *mf, const char *args) : GThread("BuildThread")
{
	Proj = proj;
	Makefile.Reset(NewStr(mf));
	Args.Reset(NewStr(args));
	Compiler = DefaultCompiler;

	GAutoString Comp(NewStr(Proj->GetSettings()->GetStr(ProjCompiler)));
	if (Comp)
	{
		// Use the specified compiler...
		if (!stricmp(Comp, "VisualStudio"))
			Compiler = VisualStudio;
		else if (!stricmp(Comp, "MingW"))
			Compiler = MingW;
		else if (!stricmp(Comp, "gcc"))
			Compiler = Gcc;
		else if (!stricmp(Comp, "cross"))
			Compiler = CrossCompiler;
		else
		    LgiAssert(!"Unknown compiler.");
	}

	if (Compiler == DefaultCompiler)
	{
		// Use default compiler for platform...
		#ifdef WIN32
		Compiler = VisualStudio;
		#else
		Compiler = Gcc;
		#endif
	}
		
	if (Proj->GetApp())
		Proj->GetApp()->PostEvent(M_APPEND_TEXT, 0, 0);

	Run();
}

BuildThread::~BuildThread()
{
	if (SubProc)
		SubProc->Interrupt();

	while (!IsExited())
	{
		LgiSleep(10);
	}
}

int BuildThread::Write(const void *Buffer, int Size, int Flags)
{
	if (Proj->GetApp())
	{
		Proj->GetApp()->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr((char*)Buffer, Size), 0);
	}
	return Size;
}

char *BuildThread::FindExe()
{
	char Exe[256] = "";
	GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
	
	if (Compiler == VisualStudio)
	{
		for (int i=0; i<p.Length(); i++)
		{
			char Path[256];
			LgiMakePath(Path, sizeof(Path), p[i], "msdev.exe");
			if (FileExists(Path))
			{
				return NewStr(Path);
			}
		}
	}
	else
	{
		if (Compiler == MingW)
		{
			// Have a look in the default spot first...
			const char *Def = "C:\\MinGW\\msys\\1.0\\bin\\make.exe";
			if (FileExists(Def))
			{
				return NewStr(Def);
			}				
		}
		
		if (!FileExists(Exe))
		{
			for (int i=0; i<p.Length(); i++)
			{
				LgiMakePath
				(
					Exe,
					sizeof(Exe),
					p[i],
					"make"
					#ifdef WIN32
					".exe"
					#endif
				);
				if (FileExists(Exe))
				{
					return NewStr(Exe);
				}
			}
		}
	}
	
	return 0;
}

GAutoString BuildThread::WinToMingWPath(const char *path)
{
	GToken t(path, "\\");
	GStringPipe a(256);
	for (int i=0; i<t.Length(); i++)
	{
		char *p = t[i];
		char *colon = strchr(p, ':');
		if (colon)
		{
			*colon = 0;
			StrLwr(p);
		}
		a.Print("/%s", p);				
	}
	return GAutoString(a.NewStr());
}

int BuildThread::Main()
{
	const char *Err = 0;
	char ErrBuf[256];

// printf("BuildThread::Main.0\n");
	
	const char *Exe = FindExe();
// printf("BuildThread::Main.1 Exe='%s'\n", Exe);
	if (Exe)
	{
		bool Status = false;
		GAutoString MakePath(NewStr(Makefile));
		GVariant Jobs;
		if (!Proj->GetApp()->GetOptions()->GetValue(OPT_Jobs, Jobs) || Jobs.CastInt32() < 1)
			Jobs = 2;

// printf("BuildThread::Main.2 Jobs=%i\n", Jobs.CastInt32());
		if (Compiler == VisualStudio)
		{
			char a[256];
			sprintf(a, "\"%s\" /make \"All - Win32 Debug\"", Makefile.Get());
			GProcess Make;
			Status = Make.Run(Exe, a, 0, true, 0, this);

			if (!Status)
			{
				Err = "Running make failed";
				LgiTrace("%s,%i - %s.\n", _FL, Err);
			}
		}
		else
		{
			GStringPipe a;
			char InitDir[MAX_PATH];
			LgiMakePath(InitDir, sizeof(InitDir), MakePath, "..");
			
			if (Compiler == MingW)
			{
				char *Dir = strrchr(MakePath, DIR_CHAR);
				#if 1
				a.Print("/C \"%s\"", Exe);
				
				/*	As of MSYS v1.0.18 the support for multiple jobs causes make to hang:
					http://sourceforge.net/p/mingw/bugs/1950/
					http://mingw-users.1079350.n2.nabble.com/MSYS-make-freezes-td7579038.html
					
					Apparently it'll be "fixed" in v1.0.19. We'll see. >:-(
				
				if (Jobs.CastInt32() > 1)
					a.Print(" -j %i", Jobs.CastInt32());
				
				*/
				
				a.Print(" -f \"%s\"", Dir ? Dir + 1 : MakePath.Get());
				#else
				a.Print("/C set");
				#endif
				Exe = "C:\\Windows\\System32\\cmd.exe";
			}
			else
			{
				if (Jobs.CastInt32())
					a.Print("-j %i -f \"%s\"", Jobs.CastInt32(), MakePath.Get());
				else
					a.Print("-f \"%s\"", MakePath.Get());
			}

			if (Args)
				a.Print(" %s", Args.Get());
			GAutoString Temp(a.NewStr());
			
			GString Msg;
			Msg.Printf("Making: %s\n", Temp.Get());
			Proj->GetApp()->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr(Msg), 0);

// printf("BuildThread::Main.3 SubProc(%s)\n", Temp.Get());
			if (SubProc.Reset(new GSubProcess(Exe, Temp)))
			{
// printf("BuildThread::Main.4 SubProc(%s)\n", InitDir);
				SubProc->SetInitFolder(InitDir);
				if (Compiler == MingW)
					SubProc->SetEnvironment("PATH", "c:\\MingW\\bin;C:\\MinGW\\msys\\1.0\\bin;%PATH%");
				
// printf("BuildThread::Main.5 SubProc starting\n");
				if ((Status = SubProc->Start(true, false)))
				{
// printf("BuildThread::Main.6 SubProc reading output\n");
					// Read all the output					
					char Buf[256];
					int rd;
					while ( (rd = SubProc->Read(Buf, sizeof(Buf))) > 0 )
					{
						Write(Buf, rd);
					}
					
// printf("BuildThread::Main.7 SubProc getting exit value\n");
					uint32 ex = SubProc->Wait();
					Print("Make exited with %i (0x%x)\n", ex, ex);
				}
				else
				{
					// Create a nice error message.
// printf("BuildThread::Main.8 getting err\n");
					GAutoString ErrStr = LgiErrorCodeToString(SubProc->GetErrorCode());
					if (ErrStr)
					{
						char *e = ErrStr + strlen(ErrStr);
						while (e > ErrStr && strchr(" \t\r\n.", e[-1]))
							*(--e) = 0;
					}
					
					sprintf_s(ErrBuf, sizeof(ErrBuf), "Running make failed with %i (%s)\n",
						SubProc->GetErrorCode(),
						ErrStr.Get());
					Err = ErrBuf;
				}
			}
		}
	}
	else
	{
		Err = "Couldn't find 'make'";
		LgiTrace("%s,%i - %s.\n", _FL, Err);
	}

// printf("BuildThread::Main.9 posting done\n");
	AppWnd *w = Proj->GetApp();
	if (w)
	{
		w->PostEvent(M_BUILD_DONE);
		if (Err)
			Proj->GetApp()->PostEvent(M_BUILD_ERR, 0, (GMessage::Param)NewStr(Err));
	}
	else LgiAssert(0);
	
// printf("BuildThread::Main.10 exiting\n");
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////
IdeProject::IdeProject(AppWnd *App) : IdeCommon(NULL)
{
	Project = this;
	d = new IdeProjectPrivate(App, this);
	Tag = NewStr("Project");
}

IdeProject::~IdeProject()
{
	d->App->OnProjectDestroy(this);
	DeleteObj(d);
}

void IdeProject::ShowFileProperties(const char *File)
{
	ProjectNode *Node = NULL;
	char *fp = FindFullPath(File, &Node);
	if (Node)
	{
		Node->OnProperties();
	}
}

const char *IdeProject::GetFileComment()
{
	return d->Settings.GetStr(ProjCommentFile);
}

const char *IdeProject::GetFunctionComment()
{
	return d->Settings.GetStr(ProjCommentFunction);
}

IdeProject *IdeProject::GetParentProject()
{
	return d->ParentProject;
}

void IdeProject::SetParentProject(IdeProject *p)
{
	d->ParentProject = p;
}

bool IdeProject::GetChildProjects(List<IdeProject> &c)
{
	CollectAllSubProjects(c);
	return c.First() != 0;
}

bool IdeProject::RelativePath(char *Out, const char *In, bool Debug)
{
	if (Out && In)
	{
		GAutoString Base = GetBasePath();
		if (Base)
		{
if (Debug) LgiTrace("XmlBase='%s'\n     In='%s'\n", Base.Get(), In);
			
			GToken b(Base, DIR_STR);
			GToken i(In, DIR_STR);
			
if (Debug) LgiTrace("Len %i-%i\n", b.Length(), i.Length());
			
			int ILen = i.Length() + (DirExists(In) ? 0 : 1);
			int Max = min(b.Length(), ILen);
			int Common = 0;
			for (; Common < Max; Common++)
			{
				#ifdef WIN32
				#define StrCompare stricmp
				#else
				#define StrCompare strcmp
				#endif
				
if (Debug) LgiTrace("Cmd '%s'-'%s'\n", b[Common], i[Common]);
				if (StrCompare(b[Common], i[Common]) != 0)
				{
					break;
				}
			}
			
if (Debug) LgiTrace("Common=%i\n", Common);
			if (Common > 0)
			{
				if (Common < b.Length())
				{
					Out[0] = 0;
					int Back = b.Length() - Common;
if (Debug) LgiTrace("Back=%i\n", Back);
					for (int n=0; n<Back; n++)
					{
						strcat(Out, "..");
						if (n < Back - 1) strcat(Out, DIR_STR);
					}
				}
				else
				{
					strcpy(Out, ".");
				}
				for (int n=Common; n<i.Length(); n++)
				{
					sprintf(Out+strlen(Out), "%s%s", DIR_STR, i[n]);
				}
				
				return true;
			}	
			else
			{
				strcpy(Out, In);
				return true;
			}		
		}
	}
	
	return false;
}

bool IdeProject::GetExePath(char *Path, int Len)
{
	const char *PExe = d->Settings.GetStr(ProjExe);
	if (PExe)
	{
		if (LgiIsRelativePath(PExe))
		{
			GAutoString Base = GetBasePath();
			if (Base)
			{
				LgiMakePath(Path, Len, Base, PExe);
			}
			else return false;
		}
		else
		{
			strcpy_s(Path, Len, PExe);
		}
		
		return true;
	}
	else return false;
}

GAutoString IdeProject::GetMakefile()
{
	GAutoString Path;
	const char *PMakefile = d->Settings.GetStr(ProjMakefile);
	if (PMakefile)
	{
		if (LgiIsRelativePath(PMakefile))
		{
			GAutoString Base = GetBasePath();
			if (Base)
			{
				char p[MAX_PATH];
				LgiMakePath(p, sizeof(p), Base, PMakefile);
				Path.Reset(NewStr(p));
			}
		}
		else
		{
			Path.Reset(NewStr(PMakefile));
		}
	}
	
	return Path;
}

void IdeProject::Clean(bool Release)
{
	if (!d->Thread &&
		d->Settings.GetStr(ProjMakefile))
	{
		GAutoString m = GetMakefile();
		if (m)
		{
			GString a = "clean";
			if (Release)
				a += " Build=Release";
			d->Thread.Reset(new BuildThread(this, m, a));
		}
	}
}

char *QuoteStr(char *s)
{
	GStringPipe p(256);
	while (s && *s)
	{
		if (*s == ' ')
		{
			p.Push("\\ ", 2);
		}
		else p.Push(s, 1);
		s++;
	}
	return p.NewStr();
}

class ExecuteThread : public GThread, public GStream
{
	IdeProject *Proj;
	char *Exe, *Args, *Path;
	int Len;
	ExeAction Act;

public:
	ExecuteThread(IdeProject *proj, const char *exe, const char *args, char *path, ExeAction act) : GThread("ExecuteThread")
	{
		Len = 32 << 10;
		Proj = proj;
		Act = act;
		Exe = NewStr(exe);
		Args = NewStr(args);
		Path = NewStr(path);
		DeleteOnExit = true;
		Run();
	}
	
	~ExecuteThread()
	{
		DeleteArray(Exe);
		DeleteArray(Args);
		DeleteArray(Path);
	}
	
	int Main()
	{
		if (Proj->GetApp())
		{
			Proj->GetApp()->PostEvent(M_APPEND_TEXT, 0, 1);
		}
		
		if (Exe)
		{
			GProcess p;
			if (Act == ExeDebug)
			{
				char *a = QuoteStr(Exe);
				char *b = QuoteStr(Path);				
				
				p.Run("kdbg", a, b, true, 0, this);
				
				DeleteArray(a);
				DeleteArray(b);
			}
			else if (Act == ExeValgrind)
			{
				#ifdef LINUX
				if (Proj->GetExecutable())
				{
					char Path[256];
					strcpy_s(Path, sizeof(Path), Exe);
					LgiTrimDir(Path);
									
					char *Term = 0;
					char *WorkDir = 0;
					char *Execute = 0;
					switch (LgiGetWindowManager())
					{
						case WM_Kde:
							Term = "konsole";
							WorkDir = "--workdir ";
							Execute = "-e";
							break;
						case WM_Gnome:
							Term = "gnome-terminal";
							WorkDir = "--working-directory=";
							Execute = "-x";
							break;
					}
					
					if (Term && WorkDir && Execute)
					{					
						char *e = QuoteStr(Proj->GetExecutable());
						char *p = QuoteStr(Path);
						char *a = Proj->GetExeArgs() ? Proj->GetExeArgs() : (char*)"";
						char Args[512];
						sprintf(Args,
								"%s%s "
								"--noclose "
								"%s valgrind --tool=memcheck --num-callers=20 %s %s",
								WorkDir,
								p,
								Execute,
								e, a);
								
						LgiExecute(Term, Args);
					}
				}
				#endif
			}
			else
			{
				p.Run(Exe, Args, Path, true, 0, this);
			}
		}
		
		return 0;
	}

	int Write(const void *Buffer, int Size, int Flags = 0)
	{
		if (Len > 0)
		{
			if (Proj->GetApp())
			{
				Size = min(Size, Len);
				Proj->GetApp()->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr((char*)Buffer, Size), 1);
				Len -= Size;
			}
			return Size;
		}
		
		return 0;
	}
};

GDebugContext *IdeProject::Execute(ExeAction Act)
{
	GAutoString Base = GetBasePath();
	if (d->Settings.GetStr(ProjExe) &&
		Base)
	{
		char e[MAX_PATH];
		if (GetExePath(e, sizeof(e)))
		{
			if (FileExists(e))
			{
				const char *Args = d->Settings.GetStr(ProjArgs);
				if (Act == ExeDebug)
				{
					return new GDebugContext(d->App, this, e, Args);
				}
				else
				{
					new ExecuteThread(this, e, Args, Base, Act);
				}
			}
			else
			{
				LgiMsg(Tree, "Executable '%s' doesn't exist.\n", AppName, MB_OK, e);
			}
		}
	}
	
	return NULL;
}

void IdeProject::Build(bool All, bool Release)
{
	if (d->Thread)
	{
		d->App->GetBuildLog()->Print("Error: Already building (%s:%i)\n", _FL);
		return;
	}

	GAutoString m = GetMakefile();
	if (!m)
	{		
		d->App->GetBuildLog()->Print("Error: no makefile? (%s:%i)\n", _FL);
		return;
	}

	d->Thread.Reset
	(
		new BuildThread
		(
			this,
			m,
			Release ? "Build=Release" : NULL
		)
	);
}

void IdeProject::StopBuild()
{
	d->Thread.Reset();
}

bool IdeProject::Serialize()
{
	return true;
}

AppWnd *IdeProject::GetApp()
{
	return d->App;
}

const char *IdeProject::GetIncludePaths()
{
	return d->Settings.GetStr(ProjIncludePaths);
}

const char *IdeProject::GetPreDefinedValues()
{
	return d->Settings.GetStr(ProjDefines);
}

const char *IdeProject::GetExeArgs()
{
	return d->Settings.GetStr(ProjArgs);
}

const char *IdeProject::GetExecutable()
{
	return d->Settings.GetStr(ProjExe);
}

char *IdeProject::GetFileName()
{
	return d->FileName;
}

int IdeProject::GetPlatforms()
{
	return PLATFORM_ALL;
}
	
GAutoString IdeProject::GetFullPath()
{
	GAutoString Status;
	if (!d->FileName)
	{
		// LgiAssert(!"No path.");
		return Status;
	}

	GArray<char*> sections;
	IdeProject *proj = this;
	while (	proj &&
			proj->GetFileName() &&
			LgiIsRelativePath(proj->GetFileName()))
	{
		sections.AddAt(0, proj->GetFileName());
		proj = proj->GetParentProject();
	}

	if (!proj)
	{
		LgiAssert(!"All projects have a relative path?");
		return Status; // No absolute path in the parent projects?
	}

	char p[MAX_PATH];
	strcpy_s(p, sizeof(p), proj->GetFileName()); // Copy the base path
	if (sections.Length() > 0)
		LgiTrimDir(p); // Trim off the filename
	for (int i=0; i<sections.Length(); i++) // For each relative section
	{
		LgiMakePath(p, sizeof(p), p, sections[i]); // Append relative path
		if (i < sections.Length() - 1)
			LgiTrimDir(p); // Trim filename off
	}
	
	Status.Reset(NewStr(p));
	return Status;
}

GAutoString IdeProject::GetBasePath()
{
	GAutoString a = GetFullPath();
	LgiTrimDir(a);
	return a;
}

void IdeProject::CreateProject()
{
	Empty();
	
	d->FileName.Reset();
	d->App->GetTree()->Insert(this);
	
	ProjectNode *f = new ProjectNode(this);
	if (f)
	{
		f->SetName("Source");
		f->SetType(NodeDir);
		InsertTag(f);
	}
	
	f = new ProjectNode(this);
	if (f)
	{
		f->SetName("Headers");
		f->SetType(NodeDir);
		InsertTag(f);
	}
	
	d->Settings.Set(ProjEditorTabSize, 4);
	d->Settings.Set(ProjEditorIndentSize, 4);
	d->Settings.Set(ProjEditorUseHardTabs, true);
	
	d->Dirty = true;
	Expanded(true);	
}

bool IdeProject::OpenFile(char *FileName)
{
	bool Status = false;
	
	Empty();

	if (LgiIsRelativePath(FileName))
	{
		char p[MAX_PATH];
		getcwd(p, sizeof(p));
		LgiMakePath(p, sizeof(p), p, FileName);
		d->FileName.Reset(NewStr(p));
	}
	else
	{
		d->FileName.Reset(NewStr(FileName));
	}

	if (d->FileName)
	{
		GFile f;
		if (f.Open(d->FileName, O_READWRITE))
		{
			GXmlTree x;
			GXmlTag r;
			if ((Status = x.Read(&r, &f)))
			{
				OnOpen(&r);
				d->App->GetTree()->Insert(this);
				Expanded(true);
				
				d->Settings.Serialize(&r, false /* read */);
			}
			else
			{
				LgiMsg(Tree, x.GetErrorMsg(), AppName);
			}
		}
	}

	return Status;
}

bool IdeProject::SaveFile(char *FileName)
{
	GAutoString Full = GetFullPath();
	if (ValidStr(Full))
	{
		GFile f;
		
		if (f.Open(Full, O_WRITE))
		{
			GXmlTree x;

			d->Settings.Serialize(this, true /* write */);
			if (x.Write(this, &f))
			{
				d->Dirty = false;
				return true;
			}
		}
	}

	return false;
}

void IdeProject::SetDirty()
{
	d->Dirty = true;
}

void IdeProject::SetClean()
{
	if (d->Dirty)
	{
		if (!ValidStr(d->FileName))
		{
			GFileSelect s;
			s.Parent(Tree);
			s.Name("Project.xml");
			if (s.Save())
			{
				d->FileName.Reset(NewStr(s.Name()));
				d->App->OnFile(d->FileName, true);
				Update();
			}
		}

		SaveFile(0);
	}

	ForAllProjectNodes(p)
	{
		p->SetClean();
	}
}

char *IdeProject::GetText(int Col)
{
	if (d->FileName)
	{
		char *s = strrchr(d->FileName, DIR_CHAR);
		return s ? s + 1 : d->FileName;
	}

	return (char*)Untitled;
}

int IdeProject::GetImage(int Flags)
{
	return 0;
}

void IdeProject::Empty()
{
	GXmlTag *t;
	while ((t = Children.First()))
	{
		ProjectNode *n = dynamic_cast<ProjectNode*>(t);
		if (n)
		{
			n->Remove();
		}
		DeleteObj(t);
	}
}

GXmlTag *IdeProject::Create(char *Tag)
{
	if (!stricmp(Tag, TagSettings))
		return false;

	return new ProjectNode(this);
}

void IdeProject::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		GSubMenu Sub;
		Sub.AppendItem("New Folder", IDM_NEW_FOLDER, true);
		Sub.AppendItem("New Web Folder", IDM_WEB_FOLDER, true);
		Sub.AppendSeparator();
		Sub.AppendItem("Build", IDM_BUILD_PROJECT, true);
		Sub.AppendItem("Clean", IDM_CLEAN_PROJECT, true);
		Sub.AppendSeparator();
		Sub.AppendItem("Sort Children", IDM_SORT_CHILDREN, true);
		Sub.AppendSeparator();
		Sub.AppendItem("Settings", IDM_SETTINGS, true);
		Sub.AppendItem("Insert Dependency", IDM_INSERT_DEP, true);

		m.ToScreen();
		GdcPt2 c = _ScrollPos();
		m.x -= c.x;
		m.y -= c.y;
		switch (Sub.Float(Tree, m.x, m.y))
		{
			case IDM_NEW_FOLDER:
			{
				GInput Name(Tree, "", "Name:", AppName);
				if (Name.DoModal())
				{
					GetSubFolder(this, Name.Str, true);
				}
				break;
			}
			case IDM_WEB_FOLDER:
			{
				WebFldDlg Dlg(Tree, 0, 0, 0);
				if (Dlg.DoModal())
				{
					if (Dlg.Ftp && Dlg.Www)
					{
						IdeCommon *f = GetSubFolder(this, Dlg.Name, true);
						if (f)
						{
							f->SetAttr(OPT_Ftp, Dlg.Ftp);
							f->SetAttr(OPT_Www, Dlg.Www);
						}
					}
				}
				break;
			}
			case IDM_BUILD_PROJECT:
			{
				StopBuild();
				Build(true, d->App->IsReleaseMode());
				break;
			}
			case IDM_CLEAN_PROJECT:
			{
				Clean(d->App->IsReleaseMode());
				break;
			}
			case IDM_SORT_CHILDREN:
			{
				SortChildren();
				Project->SetDirty();
				break;
			}
			case IDM_SETTINGS:
			{
				if (d->Settings.Edit(Tree))
				{
					SetDirty();
				}
				break;
			}
			case IDM_INSERT_DEP:
			{
				GFileSelect s;
				s.Parent(Tree);
				s.Type("Project", "*.xml");
				if (s.Open())
				{
					ProjectNode *New = new ProjectNode(this);
					if (New)
					{
						New->SetFileName(s.Name());
						New->SetType(NodeDependancy);
						InsertTag(New);
						SetDirty();
					}
				}
				break;
			}
		}
	}
}

char *IdeProject::FindFullPath(const char *File, ProjectNode **Node)
{
	char *Full = 0;
	
	ForAllProjectNodes(c)
	{
		ProjectNode *n = c->FindFile(File, &Full);
		if (n)
		{
			if (Node)
				*Node = n;
			break;
		}
	}
	
	return Full;
}

bool IdeProject::GetAllNodes(GArray<ProjectNode*> &Nodes)
{
	ForAllProjectNodes(c)
	{
		c->AddNodes(Nodes);
	}
	
	return true;
}

bool IdeProject::InProject(const char *Path, bool Open, IdeDoc **Doc)
{
	ProjectNode *n = 0;

	ForAllProjectNodes(c)
	{
		if ((n = c->FindFile(Path, 0)))
		{
			break;
		}
	}
	
	if (n && Doc)
	{
		*Doc = n->Open();
	}
	
	return n != 0;
}

char *GetQuotedStr(char *Str)
{
	char *s = strchr(Str, '\"');
	if (s)
	{
		s++;
		char *e = strchr(s, '\"');
		if (e)
		{
			return NewStr(s, e - s);
		}
	}

	return 0;
}

void IdeProject::ImportDsp(char *File)
{
	if (File && FileExists(File))
	{
		char Base[256];
		strcpy(Base, File);
		LgiTrimDir(Base);
		
		char *Dsp = ReadTextFile(File);
		if (Dsp)
		{
			GToken Lines(Dsp, "\r\n");
			IdeCommon *Current = this;
			bool IsSource = false;
			for (int i=0; i<Lines.Length(); i++)
			{
				char *L = Lines[i];
				if (strnicmp(L, "# Begin Group", 13) == 0)
				{
					char *Folder = GetQuotedStr(L);
					if (Folder)
					{
						IdeCommon *Sub = Current->GetSubFolder(this, Folder, true);
						if (Sub)
						{
							Current = Sub;
						}
						DeleteArray(Folder);
					}
				}
				else if (strnicmp(L, "# End Group", 11) == 0)
				{
					IdeCommon *Parent = dynamic_cast<IdeCommon*>(Current->GetParent());
					if (Parent)
					{
						Current = Parent;
					}
				}
				else if (strnicmp(L, "# Begin Source", 14) == 0)
				{
					IsSource = true;
				}
				else if (strnicmp(L, "# End Source", 12) == 0)
				{
					IsSource = false;
				}
				else if (Current && IsSource && strncmp(L, "SOURCE=", 7) == 0)
				{
					ProjectNode *New = new ProjectNode(this);
					if (New)
					{
						char *Src = 0;
						if (strchr(L, '\"'))
						{
							Src = GetQuotedStr(L);
						}
						else
						{
							Src = NewStr(L + 7);
						}						
						if (Src)
						{
							// Make absolute path
							char Abs[256];
							LgiMakePath(Abs, sizeof(Abs), Base, ToUnixPath(Src));
							
							// Make relitive path
							New->SetFileName(Src);
							DeleteArray(Src);
						}

						Current->InsertTag(New);
						SetDirty();
					}
				}
			}
			
			DeleteArray(Dsp);
		}
	}
}

IdeProjectSettings *IdeProject::GetSettings()
{
	return &d->Settings;
}

bool IdeProject::BuildIncludePaths(GArray<GString> &Paths, bool Recurse, IdePlatform Platform)
{
	List<IdeProject> Projects;
	if (Recurse)
	{
		GetChildProjects(Projects);
	}
	Projects.Insert(this, 0);
	
	for (IdeProject *p=Projects.First(); p; p=Projects.Next())
	{
		const char *AllIncludes = d->Settings.GetStr(ProjIncludePaths, NULL, Platform);
		GAutoString IncludePaths = ToNativePath(AllIncludes);
		if (IncludePaths)
		{
			GAutoString Base = p->GetBasePath();
			
			GArray<GAutoString> Inc;
			GToken Parts(IncludePaths, (char*)",;\r\n");
			for (int i=0; i<Parts.Length(); i++)
			{
				char *Path = Parts[i];
				while (*Path && strchr(WhiteSpace, *Path))
					Path++;
				
				if (*Path == '`')
				{
					// Run config app to get the full path list...
					GAutoString a(TrimStr(Path, "`"));
					char *Args = strchr(a, ' ');
					if (Args)
						*Args++ = 0;
					GProcess p;
					GStringPipe Out;
					if (p.Run(a, Args, NULL, true, NULL, &Out))
					{
						GAutoString result(Out.NewStr());
						GToken t(result, " \t\r\n");
						for (int i=0; i<t.Length(); i++)
						{
							char *inc = t[i];
							if (inc[0] == '-' &&
								inc[1] == 'I')
							{
								Inc.New().Reset(NewStr(inc + 2));
							}
						}
					}
					else LgiTrace("%s:%i - Error: failed to run process for '%s'\n", _FL, a.Get());
				}
				else
				{
					// Add path literal
					Inc.New().Reset(NewStr(Path));
				}
			}
			
			for (int i=0; i<Inc.Length(); i++)
			{
				char *Path = Inc[i];
				char *Full = 0, Buf[MAX_PATH];
				if
				(
					*Path != '/'
					&&
					!(
						IsAlpha(*Path)
						&&
						Path[1] == ':'
					)
				)
				{
					LgiMakePath(Buf, sizeof(Buf), Base, Path);
					Full = Buf;
				}
				else
				{
					Full = Inc[i];
				}
				
				bool Has = false;
				for (int n=0; n<Paths.Length(); n++)
				{
					if (stricmp(Paths[n], Full) == 0)
					{
						Has = true;
						break;
					}
				}
				
				if (!Has)
				{
					Paths.Add(NewStr(Full));
				}
			}
		}
	}

	return true;
}

void IdeProjectPrivate::CollectAllFiles(GTreeNode *Base, GArray<ProjectNode*> &Files, bool SubProjects, int Platform)
{
	for (GTreeItem *i = Base->GetChild(); i; i = i->GetNext())
	{
		IdeProject *Proj = dynamic_cast<IdeProject*>(i);
		if (Proj)
		{
			if (Proj->GetParentProject() && !SubProjects)
			{
				continue;
			}
		}
		else
		{
			ProjectNode *p = dynamic_cast<ProjectNode*>(i);
			if (p)
			{
				if (p->GetType() == NodeSrc ||
					p->GetType() == NodeHeader)
				{
					if (p->GetPlatforms() & Platform)
					{
						Files.Add(p);
					}
				}
			}
		}

		CollectAllFiles(i, Files, SubProjects, Platform);
	}
}

GAutoString IdeProject::GetTargetName(IdePlatform Platform)
{
	GAutoString Status;
	const char *t = d->Settings.GetStr(ProjTargetName, NULL, Platform);
	if (ValidStr(t))
	{
		// Take target name from the settings
		Status.Reset(NewStr(t));
	}
	else
	{
		char *s = strrchr(d->FileName, DIR_CHAR);
		if (s)
		{
			// Generate the target executable name
			char Target[MAX_PATH];
			strcpy_s(Target, sizeof(Target), s + 1);
			s = strrchr(Target, '.');
			if (s) *s = 0;
			strlwr(Target);
			s = Target;
			for (char *i = Target; *i; i++)
			{
				if (*i != '.' && *i != ' ')
				{
					*s++ = *i;
				}
			}
			*s = 0;
			Status.Reset(NewStr(Target));
		}
	}
	
	return Status;
}

bool IdeProject::GetTargetFile(char *Buf, int BufSize)
{
	bool Status = false;
	GAutoString Target = GetTargetName(PlatformCurrent);
	if (Target)
	{
		const char *TargetType = d->Settings.GetStr(ProjTargetType);
		if (TargetType)
		{
			if (!stricmp(TargetType, "Executable"))
			{
				strcpy_s(Buf, BufSize, Target);
				Status = true;
			}
			else if (!stricmp(TargetType, "DynamicLibrary"))
			{
				char t[MAX_PATH];
				strcpy_s(t, sizeof(t), Target);
				char *ext = LgiGetExtension(t);
				if (!ext)
					sprintf(t + strlen(t), ".%s", LGI_LIBRARY_EXT);
				else if (stricmp(ext, LGI_LIBRARY_EXT))
					strcpy(ext, LGI_LIBRARY_EXT);
				strcpy_s(Buf, BufSize, t);
				Status = true;
			}
			else if (!stricmp(TargetType, "StaticLibrary"))
			{
				snprintf(Buf, BufSize, "lib%s.%s", Target.Get(), LGI_STATIC_LIBRARY_EXT);
				Status = true;
			}
		}
	}
	
	return Status;
}

int StrCmp(char *a, char *b, int d)
{
	return stricmp(a, b);
}

int StrSort(char **a, char **b)
{
	return stricmp(*a, *b);
}

struct Dependency
{
	bool Scanned;
	GAutoString File;
	
	Dependency(const char *f)
	{
		File.Reset(NewStr(f));
	}
};

const char *CastEmpty(char *s)
{
	return s ? s : "";
}

bool IdeProject::GetAllDependencies(GArray<char*> &Files, IdePlatform Platform)
{
	GHashTbl<char*, Dependency*> Deps;
	GAutoString Base = GetBasePath();
	
	// Build list of all the source files...
	GArray<GString> Src;
	CollectAllSource(Src, Platform);
	
	// Get all include paths
	GArray<GString> IncPaths;
	BuildIncludePaths(IncPaths, false, Platform);
	
	// Add all source to dependencies
	for (int i=0; i<Src.Length(); i++)
	{
		char *f = Src[i];
		Dependency *dep = Deps.Find(f);
		if (!dep)
			Deps.Add(f, new Dependency(f));
	}
	
	// Scan all dependencies for includes
	GArray<Dependency*> Unscanned;
	do
	{
		// Find all the unscanned dependencies
		Unscanned.Length(0);
		for (Dependency *d = Deps.First(); d; d = Deps.Next())
		{
			if (!d->Scanned)
				Unscanned.Add(d);
		}		

		for (int i=0; i<Unscanned.Length(); i++)
		{
			// Then scan source for includes...
			Dependency *d = Unscanned[i];
			d->Scanned = true;
			
			char *Src = d->File;
			char Full[MAX_PATH];
			if (LgiIsRelativePath(d->File))
			{
			    LgiMakePath(Full, sizeof(Full), Base, d->File);
			    Src = Full;
			}
			
			GArray<char*> SrcDeps;
			if (GetDependencies(Src, IncPaths, SrcDeps, Platform))
			{
				for (int n=0; n<SrcDeps.Length(); n++)
				{
					// Add include to dependencies...
					char *File = SrcDeps[n];

					if (LgiIsRelativePath(File))
					{
						LgiMakePath(Full, sizeof(Full), Base, File);
						File = Full;
					}

					if (!Deps.Find(File))
					{						
						Deps.Add(File, new Dependency(File));
					}
				}
				SrcDeps.DeleteArrays();
			}
		}
	}
	while (Unscanned.Length() > 0);
	
	for (Dependency *d = Deps.First(); d; d = Deps.Next())
	{
		Files.Add(d->File.Release());
	}
	
	Deps.DeleteObjects();
	return true;
}

bool IdeProject::GetDependencies(const char *SourceFile, GArray<GString> &IncPaths, GArray<char*> &Files, IdePlatform Platform)
{
    if (!FileExists(SourceFile))
    {
        LgiTrace("%s:%i - can't read '%s'\n", _FL, SourceFile);
        return false;
    }

	GAutoString c8(ReadTextFile(SourceFile));
	if (!c8)
		return false;

	GArray<char*> Headers;
	if (!BuildHeaderList(c8, Headers, IncPaths, false))
		return false;
	
	for (int n=0; n<Headers.Length(); n++)
	{
		char *i = Headers[n];
		char p[MAX_PATH];
		if (!RelativePath(p, i))
		{
			strcpy_s(p, sizeof(p), i);
		}
		ToUnixPath(p);
		Files.Add(NewStr(p));
	}
	Headers.DeleteArrays();
	
	return true;
}

static bool RenameMakefileForPlatform(GAutoString &MakeFile, IdePlatform Platform)
{
	if (!MakeFile)
		return false;

	char *Dot = strrchr(MakeFile, '.');
	if (Dot && stricmp(sCurrentPlatform, ++Dot) != 0)
	{
		char mk[MAX_PATH];
		*Dot = 0;
		sprintf_s(mk, sizeof(mk), "%s%s", MakeFile.Get(), PlatformNames[Platform]);
		if ((Dot = strrchr(mk, '.')))
			strlwr(Dot);
		MakeFile.Reset(NewStr(mk));
	}
	
	return true;
}

bool IdeProject::CreateMakefile(IdePlatform Platform)
{
	const char *PlatformName = PlatformNames[Platform];
	const char *PlatformLibraryExt = NULL;
	const char *PlatformStaticLibExt = NULL;
	const char *PlatformExeExt = "";
	GString LinkerFlags;
	const char *TargetType = d->Settings.GetStr(ProjTargetType, NULL, Platform);
	const char *CompilerName = d->Settings.GetStr(ProjCompiler);
	const char *CCompilerBinary = "gcc";
	const char *CppCompilerBinary = "g++";
	bool IsDynamicLibrary = TargetType != NULL && !stricmp(TargetType, "DynamicLibrary");
	GStream *Log = d->App->GetBuildLog();
	
	LgiAssert(Log);
	if (!Log)
		return false;
	
	Log->Print("CreateMakefile...\n");
	
	if (Platform == PlatformWin32)
	{
		LinkerFlags = ",--enable-auto-import";
	}
	else
	{
		if (IsDynamicLibrary)
		{
			LinkerFlags = ",-soname,$(TargetFile)";
		}
		LinkerFlags += ",-export-dynamic,-R.";
	}

	char Buf[256];
	GAutoString MakeFile = GetMakefile();
	if (!MakeFile)
	{
		MakeFile.Reset(NewStr("../Makefile"));
	}
	
	// LGI_LIBRARY_EXT
	switch (Platform)
	{
		case PlatformWin32:
			PlatformLibraryExt = "dll";
			PlatformStaticLibExt = "lib";
			PlatformExeExt = ".exe";
			break;
		case PlatformLinux:
		case PlatformHaiku:
			PlatformLibraryExt = "so";
			PlatformStaticLibExt = "a";
			break;
		case PlatformMac:
			PlatformLibraryExt = "dylib";
			PlatformStaticLibExt = "a";
			break;
		default:
			LgiAssert(0);
			break;
	}

    if (CompilerName)
    {
        if (!stricmp(CompilerName, "cross"))
        {
            const char *CrossCompilerBin = d->Settings.GetStr(ProjCrossCompiler, NULL, Platform);
            if (FileExists(CrossCompilerBin))
            {
                CppCompilerBinary = CrossCompilerBin;
            }
            else
            {
                Log->Print("%s:%i - Error: cross compiler '%s' not found.\n", _FL, CrossCompilerBin);
            }
        }
    }
	
	GFile m;
	if (!m.Open(MakeFile, O_WRITE))
	{
		Log->Print("Error: Failed to open '%s' for writing.\n", MakeFile.Get());
		return false;
	}
	
	m.SetSize(0);
	
	m.Print("#!/usr/bin/make\n"
			"#\n"
			"# This makefile generated by LgiIde\n"
			"# http://www.memecode.com/lgi.php\n"
			"#\n"
			"\n"
			".SILENT :\n"
			"\n"
			"CC = %s\n"
			"CPP = %s\n",
			CCompilerBinary,
			CppCompilerBinary);

	// Collect all files that require building
	GArray<ProjectNode*> Files;
	d->CollectAllFiles
	(
		this,
		Files,
		false,
		1 << Platform
	);
	
	GAutoString Target = GetTargetName(Platform);
	if (Target)
	{
		m.Print("Target = %s\n", Target.Get());

		// Output the build mode, flags and some paths
		int BuildMode = d->App->GetBuildMode();
		char *BuildModeName = BuildMode ? (char*)"Release" : (char*)"Debug";
		m.Print("ifndef Build\n"
				"	Build = %s\n"
				"endif\n",
				BuildModeName);
		
		GString sDefines[2];
		GString sLibs[2];
		GString sIncludes[2];
		const char *ExtraLinkFlags = NULL;
		const char *ExeFlags = NULL;
		if (Platform == PlatformWin32)
		{
			ExtraLinkFlags = "";
			ExeFlags = " -mwindows";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"Flags = -fPIC -w -fno-inline -fpermissive\n");
			
			const char *DefDefs = "-DWIN32 -D_REENTRANT";
			sDefines[0] = DefDefs;
			sDefines[1] = DefDefs;
		}
		else
		{
			char PlatformCap[32];
			strcpy_s(	PlatformCap,
						sizeof(PlatformCap),
						Platform == PlatformHaiku ? "BEOS" : PlatformName);
			strupr(PlatformCap);
			
			ExtraLinkFlags = "";
			ExeFlags = "";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"Flags = -fPIC -w -fno-inline -fpermissive\n" // -fexceptions
					);
			sDefines[0].Printf("-D%s -D_REENTRANT", PlatformCap);
			#ifdef LINUX
			sDefines[0] += " -D_FILE_OFFSET_BITS=64"; // >:-(
			sDefines[0] += " -DPOSIX";
			#endif
			sDefines[1] = sDefines[0];
		}

		List<IdeProject> Deps;
		GetChildProjects(Deps);

		const char *sConfig[] = {"Debug", "Release"};
		for (int Cfg = 0; Cfg < CountOf(sConfig); Cfg++)
		{
			// Set the config
			d->Settings.SetCurrentConfig(sConfig[Cfg]);
		
			// Get the defines setup
			const char *PDefs = d->Settings.GetStr(ProjDefines, NULL, Platform);
			if (ValidStr(PDefs))
			{
				GToken Defs(PDefs, " ;,\r\n");
				for (int i=0; i<Defs.Length(); i++)
				{
					GString s;
					s.Printf(" -D%s", Defs[i]);
					sDefines[Cfg] += s;
				}
			}
		
			// Collect all dependencies, output their lib names and paths
			const char *PLibPaths = d->Settings.GetStr(ProjLibraryPaths, NULL, Platform);
			if (ValidStr(PLibPaths))
			{
				GToken LibPaths(PLibPaths, " \r\n");
				for (int i=0; i<LibPaths.Length(); i++)
				{
					GString s;
					s.Printf(" \\\n\t\t-L%s", ToUnixPath(LibPaths[i]));
					sLibs[Cfg] += s;
				}
			}

			const char *PLibs = d->Settings.GetStr(ProjLibraries, NULL, Platform);
			if (ValidStr(PLibs))
			{
				GToken Libs(PLibs, "\r\n");
				for (int i=0; i<Libs.Length(); i++)
				{
					char *l = Libs[i];
					GString s;
					if (*l == '`' || *l == '-')
						s.Printf(" \\\n\t\t%s", Libs[i]);
					else
						s.Printf(" \\\n\t\t-l%s", ToUnixPath(Libs[i]));
					sLibs[Cfg] += s;
				}
			}

			for (IdeProject *dep=Deps.First(); dep; dep=Deps.Next())
			{
				GAutoString Target = dep->GetTargetName(Platform);
				if (Target)
				{
					char t[MAX_PATH];
					strcpy_s(t, sizeof(t), Target);
					if (!strnicmp(t, "lib", 3))
						memmove(t, t + 3, strlen(t + 3) + 1);
					char *dot = strrchr(t, '.');
					if (dot)
						*dot = 0;
													
					GString s;
					s.Printf(" \\\n\t\t-l%s$(Tag)", ToUnixPath(t));
					sLibs[Cfg] += s;

					GAutoString Base = dep->GetBasePath();
					if (Base)
					{
						s.Printf(" \\\n\t\t-L%s/$(BuildDir)", ToUnixPath(Base));
						sLibs[Cfg] += s;
					}
				}
			}
			
			// Includes

			// Do include paths
			GHashTable Inc;
			const char *AllIncludes = d->Settings.GetStr(ProjIncludePaths, NULL, Platform);
			if (ValidStr(AllIncludes))
			{
				// Add settings include paths.
				GToken Paths(AllIncludes, "\r\n", Platform);
				for (int i=0; i<Paths.Length(); i++)
				{
					char *p = Paths[i];
					GAutoString pn = ToNativePath(p);
					if (!Inc.Find(pn))
					{
						Inc.Add(pn);
					}
				}
			}
			
			// Add paths of headers
			for (int i=0; i<Files.Length(); i++)
			{
				ProjectNode *n = Files[i];
				
				if (n->GetFileName())
				{
					char *e = LgiGetExtension(n->GetFileName());
					if (e && stricmp(e, "h") == 0)
					{
						for (char *Dir=n->GetFileName(); *Dir; Dir++)
						{
							if (*Dir == '/' || *Dir == '\\')
							{
								*Dir = DIR_CHAR;
							}
						}						

						char Path[256];
						strcpy_s(Path, sizeof(Path), n->GetFileName());

						LgiTrimDir(Path);
					
						char Rel[256];
						if (!RelativePath(Rel, Path))
						{
							strcpy(Rel, Path);
						}
						
						if (stricmp(Rel, ".") != 0)
						{
							GAutoString RelN = ToNativePath(Rel);
							if (!Inc.Find(RelN))
							{
								Inc.Add(RelN);
							}
						}
					}
				}
			}

			List<char> Incs;
			char *i;
			for (void *b=Inc.First(&i); b; b=Inc.Next(&i))
			{
				Incs.Insert(NewStr(i));
			}
			Incs.Sort(StrCmp, 0);
			for (i = Incs.First(); i; i = Incs.Next())
			{
				GString s;
				if (*i == '`')
					s.Printf(" \\\n\t\t%s", i);
				else
					s.Printf(" \\\n\t\t-I%s", ToUnixPath(i));
				sIncludes[Cfg] += s;
			}
		}

		// Output the defs section for Debug and Release		

		// Debug specific
		m.Print("\n"
				"ifeq ($(Build),Debug)\n"
				"	Flags += -g\n"
				"	Tag = d\n"
				"	Defs = -D_DEBUG %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n",
					CastEmpty(sDefines[0].Get()),
					CastEmpty(sLibs[0].Get()),
					CastEmpty(sIncludes[0].Get()));
		
		// Release specific
		m.Print("else\n"
				"	Flags += -s -Os\n"
				"	Defs = %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n"
				"endif\n"
				"\n",
					CastEmpty(sDefines[1].Get()),
					CastEmpty(sLibs[1].Get()),
					CastEmpty(sIncludes[1].Get()));
		
		if (Files.First())
		{
			ProjectNode *n;
			
			GArray<GString> IncPaths;
			if (BuildIncludePaths(IncPaths, false, Platform))
			{
				// Do dependencies
				m.Print("# Dependencies\n"
						"Depends =\t");
				
				for (int c = 0; c < Files.Length(); c++)
				{
					ProjectNode *n = Files[c];
					if (n->GetType() == NodeSrc)
					{
						GAutoString f = ToNativePath(n->GetFileName());
						char *d = f ? strrchr(f, DIR_CHAR) : 0;
						char *file = d ? d + 1 : f;
						d = file ? strrchr(file, '.') : 0;
						if (d)
						{
							if (c) m.Print(" \\\n\t\t\t");
							m.Print("%.*s.o", d - file, file);
						}
					}
				}
				m.Print("\n\n");

				// Write out the target stuff
				m.Print("# Target\n");

				GHashTable DepFiles;

				if (TargetType)
				{
					if (!stricmp(TargetType, "Executable"))
					{
						m.Print("# Executable target\n"
								"$(Target) :");
								
						GStringPipe Rules;
						IdeProject *Dep;
						for (Dep=Deps.First(); Dep; Dep=Deps.Next())
						{
							// Get dependency to create it's own makefile...
							Dep->CreateMakefile(Platform);
						
							// Build a rule to make the dependency if any of the source changes...
							char t[MAX_PATH] = "";
							GAutoString DepBase = Dep->GetBasePath();
							GAutoString Base = GetBasePath();
							
							if (DepBase && Base && Dep->GetTargetFile(t, sizeof(t)))
							{
								char Rel[MAX_PATH] = "";
								if (!RelativePath(Rel, DepBase))
								{
									strcpy_s(Rel, sizeof(Rel), DepBase);
								}
								ToUnixPath(Rel);
								
								// Add tag to target name
								GToken Parts(t, ".");
								if (Parts.Length() == 2)
									sprintf_s(t, sizeof(t), "lib%s$(Tag).%s", Parts[0], Parts[1]);
								else
									sprintf_s(t, sizeof(t), "%s", Parts[0]);

								sprintf(Buf, "%s/$(BuildDir)/%s", Rel, t);
								m.Print(" %s", Buf);
								
								GArray<char*> AllDeps;
								Dep->GetAllDependencies(AllDeps, Platform);
								LgiAssert(AllDeps.Length() > 0);
								AllDeps.Sort(StrSort);

								Rules.Print("%s : ", Buf);
								for (int i=0; i<AllDeps.Length(); i++)
								{
									if (i)
										Rules.Print(" \\\n\t");
									
									char Rel[MAX_PATH];
									char *f = RelativePath(Rel, AllDeps[i]) ? Rel : AllDeps[i];
									ToUnixPath(f);
									Rules.Print("%s", f);
									
									// Add these dependencies to this makefiles dep list
									if (!DepFiles.Find(f))
										DepFiles.Add(f);
								}
								
								AllDeps.DeleteArrays();
								
								Rules.Print("\n\texport Build=$(Build); \\\n"
											"\t$(MAKE) -C %s",
											Rel);

								GAutoString Mk = Dep->GetMakefile();
								// RenameMakefileForPlatform(Mk, Platform);

								char *DepMakefile = strrchr(Mk, DIR_CHAR);
								if (DepMakefile)
								{
									Rules.Print(" -f %s", DepMakefile + 1);
								}

								Rules.Print("\n\n");
							}
						}

						m.Print(" outputfolder $(Depends)\n"
								"	@echo Linking $(Target) [$(Build)]...\n"
								"	$(CPP)%s%s %s%s -o \\\n"
								"		$(Target) $(addprefix $(BuildDir)/,$(Depends)) $(Libs)\n",
								ExtraLinkFlags,
								ExeFlags,
								ValidStr(LinkerFlags) ? "-Wl" : "", LinkerFlags.Get());

						if (Platform == PlatformHaiku)
						{
							// Is there an application icon configured?
							const char *AppIcon = d->Settings.GetStr(ProjApplicationIcon, NULL, Platform);
							if (AppIcon)
							{
								m.Print("	addattr -f %s -t \"'VICN'\" \"BEOS:ICON\" $(Target)\n", AppIcon);
							}							
						}

						m.Print("	@echo Done.\n"
								"\n");

						GAutoString r(Rules.NewStr());
						if (r)
						{
							m.Write(r, strlen(r));
						}

						// Various fairly global rules
						m.Print("# Create the output folder\n"
								"outputfolder :\n"
								"	-mkdir -p $(BuildDir) 2> /dev/null\n"
								"\n");
							
						m.Print("# Clean out targets\n"
								"clean :\n"
								"	rm -f $(BuildDir)/*.o $(Target)%s\n"
								"	@echo Cleaned $(BuildDir)\n",
								LGI_EXECUTABLE_EXT);
						
						for (IdeProject *d=Deps.First(); d; d=Deps.Next())
						{
							GAutoString mk = d->GetMakefile();
							if (mk)
							{
								GAutoString my_base = GetBasePath();
								GAutoString dep_base = d->GetBasePath();
								GAutoString rel_dir = LgiMakeRelativePath(my_base, dep_base);
								char *mk_leaf = strrchr(mk, DIR_CHAR);
								m.Print("	+make -C \"%s\" -f \"%s\" clean\n",
									ToUnixPath(rel_dir ? rel_dir.Get() : dep_base.Get()),
									ToUnixPath(mk_leaf ? mk_leaf + 1 : mk.Get()));
							}
						}
						m.Print("\n");
					}
					// Shared library
					else if (!stricmp(TargetType, "DynamicLibrary"))
					{
						m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
								"$(TargetFile) : outputfolder $(Depends)\n"
								"	@echo Linking $(TargetFile) [$(Build)]...\n"
								"	$(CPP)$s -shared \\\n"
								"		%s%s \\\n"
								"		-o $(BuildDir)/$(TargetFile) \\\n"
								"		$(addprefix $(BuildDir)/,$(Depends)) \\\n"
								"		$(Libs)\n"
								"	@echo Done.\n"
								"\n",
								PlatformLibraryExt,
								ValidStr(ExtraLinkFlags) ? "-Wl" : "", ExtraLinkFlags,
								LinkerFlags.Get());

						// Cleaning target
						m.Print("# Create the output folder\n"
								"outputfolder :\n"
								"	-mkdir -p $(BuildDir) 2> /dev/null\n"
								"\n"
								"# Clean out targets\n"
								"clean :\n"
								"	rm -f $(BuildDir)/*.o $(BuildDir)/$(TargetFile)\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								PlatformLibraryExt);
					}
					// Static library
					else if (!stricmp(TargetType, "StaticLibrary"))
					{
						m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
								"$(TargetFile) : outputfolder $(Depends)\n"
								"	@echo Linking $(TargetFile) [$(Build)]...\n"
								"	ar rcs $(BuildDir)/$(TargetFile) $(addprefix $(BuildDir)/,$(Depends))\n"
								"	@echo Done.\n"
								"\n",
								PlatformStaticLibExt);

						// Cleaning target
						m.Print("# Create the output folder\n"
								"outputfolder :\n"
								"	-mkdir -p $(BuildDir) 2> /dev/null\n"
								"\n"
								"# Clean out targets\n"
								"clean :\n"
								"	rm -f $(BuildDir)/*.o $(BuildDir)/$(TargetFile)\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								PlatformStaticLibExt);
					}
				}

				// Create dependency tree, starting with all the source files.
				for (int idx=0; idx<Files.Length(); idx++)
				{
					ProjectNode *n = Files[idx];
					if (n->GetType() == NodeSrc)
					{
						GString Src = n->GetFullPath();
						if (Src)
						{
							char Part[256];
							
							char *d = strrchr(Src, DIR_CHAR);
							d = d ? d + 1 : Src.Get();
							strcpy(Part, d);
							char *Dot = strrchr(Part, '.');
							if (Dot) *Dot = 0;

							char Rel[MAX_PATH];
							
							if (Platform != PlatformHaiku)
							{
								if (!RelativePath(Rel, Src))
						 			strcpy_s(Rel, sizeof(Rel), Src);
							}
							else
							{
								// Use full path for Haiku because the Debugger needs it to
								// find the source correctly. As there are duplicate filenames
								// for different platforms it's better to rely on full paths
								// rather than filename index to find the right file.
					 			strcpy_s(Rel, sizeof(Rel), Src);
							}
							
							m.Print("%s.o : %s ", Part, ToUnixPath(Rel));

							GArray<char*> SrcDeps;
							if (GetDependencies(Src, IncPaths, SrcDeps, Platform))
							{
								for (int i=0; i<SrcDeps.Length(); i++)
								{
									char *SDep = SrcDeps[i];
									
									if (stricmp(Src.Get(), SDep) != 0)
									{
										if (i) m.Print(" \\\n\t");
										m.Print("%s", SDep);
										if (!DepFiles.Find(SDep))
										{
											DepFiles.Add(SDep);
										}
									}
									else printf("%s:%i - not add dep: '%s' '%s'\n", _FL, Src.Get(), SDep);
								}
								SrcDeps.DeleteArrays();
							}

							char *Ext = LgiGetExtension(Src);
							const char *Compiler = Src && !stricmp(Ext, "c") ? "CC" : "CPP";

							m.Print("\n"
									"	@echo $(<F) [$(Build)]\n"
									"	$(%s) $(Inc) $(Flags) $(Defs) -c $< -o $(BuildDir)/$(@F)\n"
									"\n",
									Compiler);
						}
					}
				}
				
				// Do remaining include file dependencies
				bool Done = false;
				GHashTable Processed;
				GAutoString Base = GetBasePath();
				while (!Done)
				{
					Done = true;
					char *Src;
					for (void *b=DepFiles.First(&Src); b; b=DepFiles.Next(&Src))
					{
						if (Processed.Find(Src))
							continue;

						Done = false;
						Processed.Add(Src);
						
						char Full[MAX_PATH], Rel[MAX_PATH];
						if (LgiIsRelativePath(Src))
						{
							LgiMakePath(Full, sizeof(Full), Base, Src);
							strcpy_s(Rel, sizeof(Rel), Src);
						}
						else
						{
							strcpy_s(Full, sizeof(Full), Src);
							GAutoString a = LgiMakeRelativePath(Base, Src);
							if (a)
							{
								strcpy_s(Rel, sizeof(Rel), a);
							}
							else
							{
								strcpy_s(Rel, sizeof(Rel), a);
								LgiTrace("%s:%i - Failed to make relative path '%s' '%s'\n",
									_FL,
									Base.Get(), Src);
							}
						}
						
						char *c8 = ReadTextFile(Full);
						if (c8)
						{
							GArray<char*> Headers;
							if (BuildHeaderList(c8, Headers, IncPaths, false))
							{
								m.Print("%s : ", Rel);

								for (int n=0; n<Headers.Length(); n++)
								{
									char *i = Headers[n];
									
									if (n) m.Print(" \\\n\t");
									
									char Rel[MAX_PATH];
									if (!RelativePath(Rel, i))
									{
				 						strcpy(Rel, i);
									}

									if (stricmp(i, Full) != 0)
										m.Print("%s", ToUnixPath(Rel));
									
									if (!DepFiles.Find(i))
									{
										DepFiles.Add(i);
									}
								}
								Headers.DeleteArrays();

								m.Print("\n\n");
							}
							else LgiTrace("%s:%i - Error: BuildHeaderList failed for '%s'\n", _FL, Full);
							
							DeleteArray(c8);
						}
						else LgiTrace("%s:%i - Error: Failed to read '%s'\n", _FL, Full);
						
						break;
					}
				}

				// Output VPATH
				m.Print("VPATH=%%.cpp \\\n");
				for (int i=0; i<IncPaths.Length(); i++)
				{
					char *p = IncPaths[i];
					if (p && !strchr(p, '`'))
					{
						if (!LgiIsRelativePath(p))
						{
							GAutoString a = LgiMakeRelativePath(Base, p);
							m.Print("\t%s \\\n", a?a:p);
						}
						else
						{
							m.Print("\t%s \\\n", p);
						}
					}
				}
				m.Print("\t$(BuildDir)\n\n");

				const char *OtherMakefileRules = d->Settings.GetStr(ProjMakefileRules, NULL, Platform);
				if (ValidStr(OtherMakefileRules))
				{
					m.Print("\n%s\n", OtherMakefileRules);
				}					
			}
		}
		else
		{
			m.Print("# No files require building.\n");
		}			
	}

	Log->Print("...Done: '%s'\n", MakeFile.Get());

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
IdeTree::IdeTree() : GTree(100, 0, 0, 100, 100)
{
	Hit = 0;
}

void IdeTree::OnAttach()
{
	SetWindow(this);
}

void IdeTree::OnDragExit()
{
	SelectDropTarget(0);
}

int IdeTree::WillAccept(List<char> &Formats, GdcPt2 p, int KeyState)
{
	static bool First = true;
	
	for (char *f=Formats.First(); f; )
	{
		if (First)
			LgiTrace("    WillAccept='%s'\n", f);
		
		if (stricmp(f, NODE_DROP_FORMAT) == 0 ||
			stricmp(f, LGI_FileDropFormat) == 0)
		{
			f = Formats.Next();
		}
		else
		{
			Formats.Delete(f);
			DeleteArray(f);
			f = Formats.Current();
		}
	}
	
	First = false;
		
	if (Formats.Length() > 0)
	{
		Hit = ItemAtPoint(p.x, p.y);
		if (Hit)
		{
			if (!stricmp(Formats.First(), LGI_FileDropFormat))
			{
				SelectDropTarget(Hit);
				return DROPEFFECT_LINK;
			}
			else
			{
				IdeCommon *Src = dynamic_cast<IdeCommon*>(Selection());
				IdeCommon *Dst = dynamic_cast<IdeCommon*>(Hit);
				if (Src && Dst)
				{
					// Check this folder is not a child of the src
					for (IdeCommon *n=Dst; n; n=dynamic_cast<IdeCommon*>(n->GetParent()))
					{
						if (n == Src)
						{
							return DROPEFFECT_NONE;
						}
					}
				}

				// Valid target
				SelectDropTarget(Hit);
				return DROPEFFECT_MOVE;
			}
		}
	}
	else LgiTrace("%s:%i - No valid drop formats.\n", _FL);

	return DROPEFFECT_NONE;
}

int IdeTree::OnDrop(GArray<GDragData> &Data, GdcPt2 p, int KeyState)
{
	SelectDropTarget(0);

	if (!Hit)
		Hit = ItemAtPoint(p.x, p.y);

	if (!Hit)
		return DROPEFFECT_NONE;
	
	for (unsigned n=0; n<Data.Length(); n++)
	{
		GDragData &dd = Data[n];
		GVariant *Data = &dd.Data[0];
		if (dd.IsFormat(NODE_DROP_FORMAT))
		{
			if (Data->Type == GV_BINARY && Data->Value.Binary.Length == sizeof(ProjectNode*))
			{
				ProjectNode *Src = ((ProjectNode**)Data->Value.Binary.Data)[0];
				if (Src)
				{
					ProjectNode *Folder = dynamic_cast<ProjectNode*>(Hit);
					while (Folder && Folder->GetType() > NodeDir)
					{
						Folder = dynamic_cast<ProjectNode*>(Folder->GetParent());
					}

					IdeCommon *Dst = dynamic_cast<IdeCommon*>(Folder?Folder:Hit);
					if (Dst)
					{
						// Check this folder is not a child of the src
						for (IdeCommon *n=Dst; n; n=dynamic_cast<IdeCommon*>(n->GetParent()))
						{
							if (n == Src)
							{
								return DROPEFFECT_NONE;
							}
						}

						// Detach
						GTreeItem *i = dynamic_cast<GTreeItem*>(Src);
						i->Detach();
						if (Src->GXmlTag::Parent)
						{
							LgiAssert(Src->GXmlTag::Parent->Children.HasItem(Src));
							Src->GXmlTag::Parent->Children.Delete(Src);
						}
						
						// Attach
						Src->GXmlTag::Parent = Dst;
						Dst->Children.Insert(Src);
						Dst->Insert(Src);
						
						// Dirty
						Src->GetProject()->SetDirty();
					}
				
					return DROPEFFECT_MOVE;
				}
			}
		}
		else if (dd.IsFileDrop())
		{
			ProjectNode *Folder = dynamic_cast<ProjectNode*>(Hit);
			while (Folder && Folder->GetType() > NodeDir)
			{
				Folder = dynamic_cast<ProjectNode*>(Folder->GetParent());
			}
			
			IdeCommon *Dst = dynamic_cast<IdeCommon*>(Folder?Folder:Hit);
			if (Dst)
			{
				GDropFiles Df(*Data);
				for (int i=0; i<Df.Length(); i++)
				{
					Dst->AddFiles(Df[i]);
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - Unknown drop format: %s.\n", _FL, dd.Format.Get());
		}
	}

	return DROPEFFECT_NONE;
}

