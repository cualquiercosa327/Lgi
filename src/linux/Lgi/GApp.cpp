#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/wait.h>
#endif
#include <ctype.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GSkinEngine.h"
#include "GArray.h"
#include "LgiWinManGlue.h"
#include "GVariant.h"
#if defined(WIN32) && defined(__GTK_H__)
#include "../win32/GSymLookup.h"
#else
#include "GSymLookup.h"
#endif
#include "GToken.h"
#include "GUtf8.h"

#define DEBUG_MSG_TYPES				0
#define DEBUG_HND_WARNINGS			0

#ifndef WIN32
namespace Gtk {
#include <gdk/gdkx.h>
#undef Status
}
#endif

////////////////////////////////////////////////////////////////
struct OsAppArgumentsPriv
{
	GArray<char*> Ptr;
	
	~OsAppArgumentsPriv()
	{
		Ptr.DeleteArrays();
	}
};

OsAppArguments::OsAppArguments(int args, char **arg)
{
	d = new OsAppArgumentsPriv;

	for (int i=0; i<args; i++)
	{
		d->Ptr.Add(NewStr(arg[i]));
	}
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
}

OsAppArguments::~OsAppArguments()
{
	DeleteObj(d);
}

void OsAppArguments::Set(char *CmdLine)
{
	d->Ptr.DeleteArrays();
	
	if (!CmdLine)
		return;
	
	for (char *s = CmdLine; *s; )
	{
		while (*s && strchr(WhiteSpace, *s)) s++;
		if (*s == '\'' || *s == '\"')
		{
			char delim = *s++;
			char *e = strchr(s, delim);
			if (e)
				d->Ptr.Add(NewStr(s, e - s));
			else
				break;
			
			s = e + 1;
		}
		else
		{
			char *e = s;
			while (*e && !strchr(WhiteSpace, *e))
				e++;
			d->Ptr.Add(NewStr(s, e-s));
			s = e;
		}
	}	

	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
}

OsAppArguments &OsAppArguments::operator =(OsAppArguments &a)
{
	d->Ptr.DeleteArrays();

	for (int i=0; i<a.Args; i++)
	{
		if (a.Arg[i])
			d->Ptr.Add(NewStr(a.Arg[i]));
	}
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];

	return *this;
}

////////////////////////////////////////////////////////////////
#if HAS_SHARED_MIME

#include "GFilterUtils.h"
#include "mime-types.h"

class GSharedMime : public GLibrary
{
public:
	GSharedMime() :
		#ifdef _DEBUG
		GLibrary("libsharedmime1d")
		#else
		GLibrary("libsharedmime1")
		#endif
	{
	}
	
	DynFunc0(int, mimetypes_init);
	DynFunc1(const char*, mimetypes_set_default_type, const char *, default_type);	
	DynFunc2(const char*, mimetypes_get_file_type, const char*, pathname, mimetypes_flags, flags);
	DynFunc2(const char*, mimetypes_get_data_type, const void*, data, int, length);
	DynFunc3(bool, mimetypes_decode, const char *, type, char **, media_type, char **, sub_type);
	DynFunc2(char *, mimetypes_convert_filename, const char *, pathname, const char *, mime_type);
	DynFunc3(bool, mimetypes_add_mime_dir, const char *, path, bool, high_priority, bool, rescan);
	DynFunc2(const char *, mimetypes_get_type_info, const char *, mime_type, const char *, lang);
};

#endif

#ifdef XWIN
/////////////////////////////////////////////////////////////////////////////
//
// Attempts to cleanup and call drkonqi to process the crash
//
void LgiCrashHandler(int Sig)
{
	// Don't get into an infinite loop
   	signal(SIGSEGV, SIG_DFL);

	/*
	// Our pid
	int MyPid = getpid();	
	printf("LgiCrashHandler trigger MyPid=%i\n", MyPid);

	// Fork to run the crash handler
	pid_t Pid = fork();
	if (Pid <= 0)
	{
		// Pass our state down to the crash handler...
		XObject o;
		int Args = 0;
		char *Arg[32];
		char SigName[16], PidName[16];
		ZeroObj(Arg);

		sprintf(SigName, "%i", Sig);	
		sprintf(PidName, "%i", MyPid);

		Arg[Args++] = "drkonqi";
		Arg[Args++] = "--display";
		Arg[Args++] = XDisplayString(o.XDisplay());
		Arg[Args++] = "--appname";
        Arg[Args++] = LgiApp->AppWnd ? LgiApp->AppWnd->Name() : (char*)"Lgi";
        Arg[Args++] = "--signal";
        Arg[Args++] = SigName;
        Arg[Args++] = "--pid";
        Arg[Args++] = PidName;
        
		setgid(getgid());
		setuid(getuid());

		execvp("drkonqi", Arg);
	}
	else
	{
		// Wait for child to exit
		waitpid(Pid, NULL, 0);
		_exit(253);
	}
	*/
}
#endif

/////////////////////////////////////////////////////////////////////////////
typedef GArray<GAppInfo*> AppArray;

#ifndef XK_Num_Lock
#define XK_Num_Lock                      0xff7f
#endif
#ifndef XK_Shift_Lock
#define XK_Shift_Lock                    0xffe6
#endif
#ifndef XK_Caps_Lock
#define XK_Caps_Lock                     0xffe5
#endif

class GAppPrivate : public GSymLookup
{
public:
	// Common
	GXmlTag *Config;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	GHashTable MimeToApp;
	#if HAS_SHARED_MIME
	GSharedMime *Sm;
	#endif
	GLibrary *WmLib;
	GHashTbl<int, GView*> Handles;
	OsThreadId GuiThread;
	int MessageLoopDepth;
	int CurEvent;
	#if DEBUG_MSG_TYPES
	GArray<int> Types;
	#endif
	GArray<GViewI*> DeleteLater;
	GMouse LastMove;
	GAutoString Name;
	
	// Clipboard handling
	int Clipboard, Utf8, Utf8String;
	GVariant ClipData;
	GHashTbl<int, GVariant*> ClipNotify;

	// Mouse click info
	uint64 LastClickTime;
	OsView LastClickWnd;
	int LastClickX;
	int LastClickY;

	GAppPrivate() : Args(0, 0)
	{
		CurEvent = 0;
		GuiThread = LgiGetCurrentThread();
		WmLib = 0;
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
		MessageLoopDepth = 0;
		#if HAS_SHARED_MIME
		Sm = 0;
		#endif

		LastClickTime = 0;
		LastClickWnd = 0;
		LastClickX = 0;
		LastClickY = 0;
	}

	~GAppPrivate()
	{
		if (WmLib)
		{
			Proc_LgiWmExit WmExit = (Proc_LgiWmExit) WmLib->GetAddress("LgiWmExit");
			if (WmExit AND WmExit())
			{
				DeleteObj(WmLib);
			}
			else
			{
				// Do not unload the library is the exit function fails...
				// It could still be doing something or have AtExit handlers
				// in place.
				WmLib = 0;
				// Do just leak the memory. We're shutting down anyway.
			}
		}
		DeleteObj(SkinLib);
		
		#if HAS_SHARED_MIME
		DeleteObj(Sm);
		#endif
		
		for (void *p = MimeToApp.First(); p; p = MimeToApp.Next())
		{
			AppArray *a = (AppArray*)p;
			a->DeleteObjects();
			DeleteObj(a);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////
GSkinEngine *GApp::SkinEngine = 0;
GApp *TheApp = 0;
GMouseHook *GApp::MouseHook = 0;

GApp::GApp(const char *AppMime, OsAppArguments &AppArgs, GAppArguments *Args) :
	OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	SystemNormal = 0;
	SystemBold = 0;
	d = new GAppPrivate;
	
	// We want our printf's NOW!
	setvbuf(stdout,(char *)NULL,_IONBF,0); // print mesgs immediately.

	// Setup the file and graphics sub-systems
	d->FileSystem = new GFileSystem;
	d->GdcSystem = new GdcDevice;

	SetAppArgs(AppArgs);

	srand(LgiCurrentTime());
	LgiInitColours();
	AppWnd = 0;

	MouseHook = new GMouseHook;

	#if defined XWIN
	// Setup the SIGSEGV signal to call the KDE crash handler
	if (!GetOption("nch") AND
		LgiGetWindowManager() == WM_Kde)
	{
		signal(SIGSEGV, LgiCrashHandler);
	}
	#endif

	// System font setup
	SystemNormal = 0;
	GFontType SysFontType;

	if (SysFontType.GetSystemFont("System"))
	{
		SystemNormal = SysFontType.Create();

		if (SystemNormal)
		{
			SystemNormal->Transparent(true);
		}
		
		SystemBold = SysFontType.Create();
		if (SystemBold)
		{
			SystemBold->Bold(true);
			SystemBold->Transparent(true);
			SystemBold->Create();
		}
	}
	else
	{
		printf("%s:%i - Couldn't get system font setting.\n", __FILE__, __LINE__);
	}

	if (!SystemNormal)
	{
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error: GApp::GApp", MB_OK);
		LgiExitApp();
	}
	
	if (!GetOption("noskin"))
	{
		#if HAS_SHARED_OBJECT_SKIN
		char Name[64];
		d->SkinLib = new GLibrary("liblgiskin"
			#ifdef _DEBUG
			"d"
			#endif
			);
		if (d->SkinLib)
		{
			if (d->SkinLib->IsLoaded())
			{
				Proc_CreateSkinEngine CreateSkinEngine =
					(Proc_CreateSkinEngine)d->SkinLib->GetAddress(LgiSkinEntryPoint);
				if (CreateSkinEngine)
				{
					SkinEngine = CreateSkinEngine(this);
				}
			}
			else
			{
				DeleteObj(d->SkinLib);
			}
		}
		#else
		extern GSkinEngine *CreateSkinEngine(GApp *App);
		SkinEngine = CreateSkinEngine(this);
		#endif
	}
}

GApp::~GApp()
{
	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(d->Config);
	DeleteObj(GFontSystem::Me);
	DeleteObj(d);
	TheApp = 0;
}

char *GApp::GetName()
{
	if (!d->Name)
	{
		char n[MAX_PATH];
		if (LgiGetExeFile(n, sizeof(n)))
		{
			char *dir = strrchr(n, DIR_CHAR);
			if (dir)
			{
				dir++;
				char *e = LgiGetExtension(dir);
				if (e) e[-1] = 0;
				d->Name.Reset(NewStr(dir));
			}
		}
	}
	
    return d->Name;
}

GApp *GApp::ObjInstance()
{
	return TheApp;
}

bool GApp::IsOk()
{
	bool Status = 	(this != 0) &&
					(d != 0)
					/*
					#ifdef XWIN
					AND (XDisplay() != 0)
					#endif
					*/
					;
					
	LgiAssert(Status);
	return Status;
}

GMouseHook *GApp::GetMouseHook()
{
	return MouseHook;
}

int GApp::GetMetric(LgiSystemMetric Metric)
{
	switch (Metric)
	{
		case LGI_MET_DECOR_X:
		{
			return 8;
		}
		case LGI_MET_DECOR_Y:
		{
			return 8 + 19;
		}
		case LGI_MET_DECOR_CAPTION:
		{
			return 19;
		}
	}

	return 0;
}

GViewI *GApp::GetFocus()
{
	return 0;
}

OsThreadId GApp::GetGuiThread()
{
	return d->GuiThread;;
}

OsProcessId GApp::GetProcessId()
{
    #ifdef WIN32
	return GetCurrentProcessId();
    #else
	return getpid();
	#endif
}

OsAppArguments *GApp::GetAppArgs()
{
	return IsOk() ? &d->Args : 0;
}

void GApp::SetAppArgs(OsAppArguments &AppArgs)
{
	if (IsOk())
	{
		d->Args = AppArgs;
	}
}

#ifndef WIN32
void GApp::RegisterHandle(GView *v)
{
	LgiAssert(v);
	if (v)
		d->Handles.Add(v->Handle(), v);
}

void GApp::UnregisterHandle(GView *v)
{
	LgiAssert(v);
	if (v)
		d->Handles.Delete(v->Handle());
}

void GApp::OnEvents()
{
}
#endif

struct GtkIdle
{
	GAppI::OnIdleProc cb;
	void *param;
};

Gtk::gboolean IdleWrapper(Gtk::gpointer data)
{
	GtkIdle *i = (GtkIdle*) data;
	i->cb(i->param);
	return TRUE;
}

bool GApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (Loop)
	{
		GtkIdle idle;
		if (IdleCallback)
		{
			idle.cb = IdleCallback;
			idle.param = IdleParam;
			Gtk::g_idle_add(IdleWrapper, &idle);
		}
		
		Gtk::gtk_main();
	}
	else
	{
	    while (Gtk::gtk_events_pending())
	        Gtk::gtk_main_iteration();
	}

	return false;
}

void GApp::OnUrl(const char *Url)
{
	if (AppWnd)
		AppWnd->OnUrl(Url);
}

void GApp::OnReceiveFiles(GArray<char*> &Files)
{
	if (AppWnd)
		AppWnd->OnReceiveFiles(Files);
}

void GApp::SetConfig(GXmlTag *Tag)
{
	if (IsOk() && Tag)
	{
		GXmlTag *Old = GetConfig(Tag->Tag);
		if (Old)
		{
			Old->RemoveTag();
			DeleteObj(Old);
		}

		if (!d->Config)
		{
			d->Config = new GXmlTag("Config");
		}
		if (d->Config)
		{
			d->Config->InsertTag(Tag);
		}
	}
}

GXmlTag *GApp::GetConfig(const char *Tag)
{
	if (IsOk() && !d->Config)
	{
		char File[] = "lgi.conf";
		char Path[256];
		if (LgiGetExePath(Path, sizeof(Path)))
		{
			if (Path[strlen(Path)-1] != DIR_CHAR) strcat(Path, DIR_STR);
			strcat(Path, File);

			if (!FileExists(Path))
			{
				char *f = LgiFindFile(File);
				if (f)
				{
					strcpy(Path, f);
					DeleteArray(f);
				}
			}

			if (FileExists(Path))
			{
				d->Config = new GXmlTag("Config");
				if (d->Config)
				{
					GFile f;
					if (f.Open(Path, O_READ))
					{
						GXmlTree t;
						t.Read(d->Config, &f, 0);
					}
				}
			}
		}
	}

	if (Tag AND d->Config)
	{
		return d->Config->GetTag(Tag);
	}

	return 0;
}

char *GApp::GetArgumentAt(int n)
{
	return n >= 0 AND n < d->Args.Args ? NewStr(d->Args.Arg[n]) : 0;
}

bool GApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	GAutoString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
		{
			if (DestLen > 0)
			{
				strsafecpy(Dest, Buf, DestLen);
			}
			else return false;
		}
		return true;
	}
	return false;
}

bool GApp::GetOption(const char *Option, GAutoString &Buf)
{
	if (IsOk() AND Option)
	{
		int OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			char *a = d->Args.Arg[i];
			if (!a)
				continue;

			if (strchr("-/\\", a[0]))
			{
				if (strnicmp(a+1, Option, OptLen) == 0)
				{
					char *Arg = 0;
					if (strlen(a+1+OptLen) > 0)
					{
						Arg = a + 1 + OptLen;
					}
					else if (i < d->Args.Args - 1)
					{
						Arg = d->Args.Arg[i + 1];
					}
					
					if (Arg)
					{
						if (strchr("\'\"", *Arg))
						{
							char Delim = *Arg++;
							char *End = strchr(Arg, Delim);
							if (End)
							{
								int Len = (int)End-(int)Arg;
								if (Len > 0)
								    Buf.Reset(NewStr(Arg, Len));
								else
								    return false;
							}
							else return false;
						}
						else
						{
						    Buf.Reset(NewStr(Arg));
						}
					}

					return true;
				}
			}
		}
	}

	return false;
}

void GApp::OnCommandLine()
{
	char WhiteSpace[] = " \r\n\t";

	GArray<char*> Files;

	for (int i=1; i<GetAppArgs()->Args; i++)
	{
		char *a = GetAppArgs()->Arg[i];
		if (FileExists(a))
		{
			Files.Add(NewStr(a));
		}
	}

	// call app
	if (Files.Length() > 0)
	{
		OnReceiveFiles(Files);
	}

	// clear up
	Files.DeleteArrays();
}

void GApp::Exit(int Code)
{
	if (Code)
	{
		// hard exit
		::exit(Code);
	}
	else
	{
		// soft exit
		Gtk::gtk_main_quit();
	}
}

GAutoString GApp::GetFileMimeType(const char *File)
{
	GAutoString Status;
	char Full[300] = "";

	if (!FileExists(File))
	{
		// Look in the path
		GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
		for (int i=0; i<p.Length(); i++)
		{
			LgiMakePath(Full, sizeof(Full), p[i], File);
			if (FileExists(Full))
			{
				File = Full;
				break;
			}
		}		
	}

	#if HAS_SHARED_MIME
	// freedesktop.org rocks...
	if (!d->Sm)
	{
		// Not loaded, go and try to load it...
		d->Sm = new GSharedMime;
		if (d->Sm AND d->Sm->IsLoaded())
		{
			d->Sm->mimetypes_init();
		}
	}
	if (d->Sm AND d->Sm->IsLoaded())
	{
		// Loaded...
		char *m = (char*)d->Sm->mimetypes_get_file_type(File, MIMETYPES_CHECK_ALL);
		if (m)
		{
			#if HAS_FILE_CMD
			if (stricmp(m, "application/x-not-recognised") != 0)
			#endif
			{
				strcpy(Mime, m);
				return true;
			}
		}
		else
		{
			printf("%s:%i - mimetypes_get_file_type failed for '%s'\n", __FILE__, __LINE__, File);
		}
	}
	else
	{
		printf("%s:%i - Shared Mime not loaded!!!\n", __FILE__, __LINE__);
	}
	#endif

	#if HAS_FILE_CMD
	// doh! not installed... :(
	GStringPipe Output;
	char Args[256];
	sprintf(Args, "-i \"%s\"", File);
	GProcess p;
	if (p.Run("file", Args, 0, true, 0, &Output))
	{
		char *Out = Output.NewStr();
		if (Out)
		{
			char *s = strchr(Out, ':');
			if (s AND strchr(Out, '/'))
			{
				s += 2;
				
				char *e = s;
				while (*e AND (isalpha(*e) OR strchr("-_/", *e))) e++;
				*e = 0;
				
				strcpy(Mime, s);
				Status = true;
			}
			DeleteArray(Out);
		}
	}
	#endif
	
	return Status;
}

bool GApp::GetAppsForMimeType(char *Mime, GArray<GAppInfo*> &Apps)
{
	// Find alternative version of the MIME type (e.g. x-type and type).
	char AltMime[256];
	strcpy(AltMime, Mime);
	char *s = strchr(AltMime, '/');
	if (s)
	{
		s++;
		
		int Len = strlen(s) + 1;
		if (strnicmp(s, "x-", 2) == 0)
		{
			memmove(s, s+2, Len - 2);
		}
		else
		{
			memmove(s+2, s, Len);
			s[0] = 'x';
			s[1] = '-';
		}
	}

	LgiGetAppsForMimeType(Mime, Apps);
	LgiGetAppsForMimeType(AltMime, Apps);
	
	return Apps.Length() > 0;

	/*
	if (!d->MimeToApp.Length())
	{
		// printf("%s:%i - Building MimeToApp.\n", __FILE__, __LINE__);

		GToken Paths;
		if (_GetKdePaths(Paths, "apps"))
		{
			// No default... look in the apps dir
			List<char> f, e;
			e.Insert("*.desktop");
			for (int i=0; i<Paths.Length(); i++)
			{
				LgiRecursiveFileSearch(Paths[i], &e, &f);
			}

			for (char *File=f.First(); File; File=f.Next())
			{
				if (stristr(File, "kde-cervisia"))
				{
					continue;
				}
				
				char *DesktopFile = ReadTextFile(File);
				if (DesktopFile)
				{
					char MimeTypes[512];
					char AppPath[256] = "";

					// this is an app that supports the MimeTypes...
					if (_GetIniField("Desktop Entry", "MimeType", DesktopFile, MimeTypes, sizeof(MimeTypes)) AND
						_GetIniField("Desktop Entry", "Exec", DesktopFile, AppPath, sizeof(AppPath)))
					{
						// look through the types..
						GToken t(MimeTypes, ";");
						for (int i=0; i<t.Length(); i++)
						{
							// Fliter out invalid mime types
							if (strchr(t[i], '/'))
							{
								// Check for existing app array
								AppArray *Group = (AppArray*) d->MimeToApp.Find(t[i]);
								if (!Group)
								{
									// Not there so create it...
									d->MimeToApp.Add(t[i], Group = new AppArray);
								}									
								if (Group)
								{
									// Check the application is not in the array already
									bool Has = false;
									for (int n=0; n<Group->Length(); n++)
									{
										GAppInfo *Existing = (*Group)[n];
										if (Existing AND Existing->Path)
										{
											char *a = strchr(AppPath, ' ');
											int alen = a ? (int)a-(int)AppPath : strlen(AppPath);

											char *b = strchr(Existing->Path, ' ');
											int blen = b ? (int)b-(int)Existing->Path : strlen(Existing->Path);

											if (alen == blen AND
												strncmp(AppPath, Existing->Path, alen) == 0)
											{
												Has = true;
												break;
											}
										}
									}
									
									if (!Has)
									{
										// It's not, so add it...
										GAppInfo *a = new GAppInfo;
										if (a)
										{
											(*Group)[Group->Length()] = a;											
											a->Path = NewStr(AppPath);
											
											char s[256];
											
											// store the user visible name
											if (_GetIniField("Desktop Entry", "Name", DesktopFile, s, sizeof(s)))
											{
												a->Name = NewStr(s);
											}
											
											// store the icon file...
											if (_GetIniField("Desktop Entry", "Icon", DesktopFile, s, sizeof(s)))
											{
												a->Icon = NewStr(s);
											}
										}
									}
								}
							}
						}
					}

					DeleteArray(DesktopFile);
				}
			}
		}
		else
		{
			printf("LgiGetAppForMimeType: Couldn't get apps path(s)\n");
		}
	}
	
	AppArray *p = (AppArray*)d->MimeToApp.Find(Mime);
	if (p)
	{
		for (int i=0; i<p->Length(); i++)
		{
			Apps[i] = (*p)[i];
		}
		
		return true;
	}
	*/
}

#ifdef LINUX
GLibrary *GApp::GetWindowManagerLib()
{
	if (!d->WmLib)
	{
		char Lib[32];
		WindowManager Wm = LgiGetWindowManager();
		switch (Wm)
		{
			case WM_Kde:
				strcpy(Lib, "liblgikde3");
				break;
			case WM_Gnome:
				strcpy(Lib, "liblgignome2");
				break;
			default:
				strcpy(Lib, "liblgiother");
				break;
		}
		#ifdef _DEBUG
		strcat(Lib, "d");
		#endif
		
		d->WmLib = new GLibrary(Lib);
		if (d->WmLib)
		{
			if (d->WmLib->IsLoaded())
			{
				Proc_LgiWmInit WmInit = (Proc_LgiWmInit) d->WmLib->GetAddress("LgiWmInit");
				if (WmInit)
				{
					WmInitParams Params;
					// Params.Dsp = XObject::XDisplay();
					Params.Args = d->Args.Args;
					Params.Arg = d->Args.Arg;
					
					// printf("%s:%i - Params = %p, %i, %p\n", _FL, Params.Dsp, Params.Args, Params.Arg);
					
					WmInit(&Params);
				}
				else printf("%s:%i - Failed to find method 'LgiWmInit' in WmLib.\n", __FILE__, __LINE__);
			}
			// else printf("%s:%i - couldn't load '%s.so'\n", __FILE__, __LINE__, Lib);
		}
		else printf("%s:%i - alloc error\n", __FILE__, __LINE__);
	}
	
	return d->WmLib AND d->WmLib->IsLoaded() ? d->WmLib : 0;
}

void GApp::DeleteMeLater(GViewI *v)
{
	d->DeleteLater.Add(v);
}

void GApp::SetClipBoardContent(OsView Hnd, GVariant &v)
{
	// Store the clipboard data we will serve
	d->ClipData = v;
}

bool GApp::GetClipBoardContent(OsView Hnd, GVariant &v, GArray<char*> &Types)
{
	return false;
}
#endif

GSymLookup *GApp::GetSymLookup()
{
	return d;
}

using namespace Gtk;

////////////////////////////////////////////////////////////////
OsApplication *OsApplication::Inst = 0;

class OsApplicationPriv
{
public:
	OsApplicationPriv()
	{
	}
};

OsApplication::OsApplication(int Args, char **Arg)
{
    Inst = this;
    d = new OsApplicationPriv;	
	
    gtk_init(&Args, &Arg);
}

OsApplication::~OsApplication()
{
	DeleteObj(d);
	Inst = 0;
}

////////////////////////////////////////////////////////////////
GMessage::GMessage(int m, Param a, Param b)
{
	Event = 0;
	OwnEvent = false;
	Set(m, a, b);
}

GMessage::~GMessage()
{
    /*
	if (OwnEvent && Event)
		gdk_event_free(Event);
	*/
}

void GMessage::Set(int m, Param a, Param b)
{
	if (!Event)
	{
		OwnEvent = true;
		Event = gdk_event_new(GDK_CLIENT_EVENT);
	}
	if (Event)
	{
		Event->client.data.l[0] = m;
		Event->client.data.l[1] = a;
		Event->client.data.l[2] = b;
	}
}

struct GlibEventParams
{
    GtkWidget *w;
    GdkEvent *e;
};

static gboolean 
GlibPostMessage(GlibEventParams *p)
{
    GDK_THREADS_ENTER();
    gtk_propagate_event(p->w, p->e);
    gdk_event_free(p->e);
    DeleteObj(p);
    GDK_THREADS_LEAVE();

    return FALSE;
}

bool GMessage::Send(GtkWidget *Wnd)
{
	bool Status = false;

	if (Event && Wnd)
	{
	    GlibEventParams *p = new GlibEventParams;
	    p->w = Wnd;
	    p->e = gdk_event_new(GDK_CLIENT_EVENT);
	    *p->e = *Event;
	    g_idle_add((GSourceFunc)GlibPostMessage, p);
	}
	else LgiAssert(!"No Event or Wnd");
	
	return Status;
}

int GMessage::Type()
{
	if (Event && Event->type == Gtk::GDK_CLIENT_EVENT)
		return Event->client.data.l[0];
	return 0;
}

GMessage::Param GMessage::A()
{
	if (Event && Event->type == Gtk::GDK_CLIENT_EVENT)
		return Event->client.data.l[1];
	return 0;
}

GMessage::Param GMessage::B()
{
	if (Event && Event->type == Gtk::GDK_CLIENT_EVENT)
		return Event->client.data.l[2];
	return 0;
}

