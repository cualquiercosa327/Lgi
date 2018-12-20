#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#import <AppKit/AppKit.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GSkinEngine.h"
#include "GArray.h"
#include "GToken.h"
#include "LThread.h"
#include "GXmlTree.h"
#include "GSymLookup.h"
#include "GFontCache.h"

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

extern int hndstate(int hnd);

struct OsAppArgumentsPriv
{
	GAutoString Str;
	GArray<char*> Ptr;
};

OsAppArguments::OsAppArguments(int args, const char **arg)
{
	d = new OsAppArgumentsPriv;
	Args = args;
	Arg = arg;
}

OsAppArguments::~OsAppArguments()
{
	DeleteObj(d);
}

void OsAppArguments::Set(char *CmdLine)
{
	d->Str.Reset();
	d->Ptr.Length(0);
	
	GArray<char> Raw;
	GArray<size_t> Offsets;
	
	char Exe[256];
	Offsets.Add(0);
	if (LgiGetExeFile(Exe, sizeof(Exe)))
	{
		size_t Len = strlen(Exe);
		Raw.Length(Len + 1);
		strcpy(&Raw[0], Exe);
	}
	else
	{
		Raw.Add(0);
	}
	
	if (CmdLine)
	{
		for (char *s = CmdLine; *s; )
		{
			while (*s && strchr(WhiteSpace, *s)) s++;
			char *e;
			if (*s == '\'' || *s == '\"')
			{
				char delim = *s++;
				Offsets.Add(Raw.Length());
				e = s;
				while (*e && *e != delim)
				{
					Raw.Add(*e++);
				}
				Raw.Add(0);
			}
			else
			{
				Offsets.Add(Raw.Length());
				for (e = s; *e && !strchr(WhiteSpace, *e); e++)
				{
					Raw.Add(*e);
				}
				Raw.Add(0);
			}
			s = *e ? e + 1 : e;
		}
	}
	
	d->Str.Reset(Raw.Release());
	for (int n=0; n<Offsets.Length(); n++)
	{
		d->Ptr[n] = d->Str + Offsets[n];
	}
	
	Args = (int)d->Ptr.Length();
	Arg = (const char**) &d->Ptr[0];
}

OsAppArguments &OsAppArguments::operator =(OsAppArguments &a)
{
	GArray<char> Raw;
	GArray<size_t> Offsets;
	for (int i=0; i<a.Args; i++)
	{
		size_t Len = strlen(a.Arg[i]) + 1;
		Offsets[i] = Raw.Length();
		Raw.Length(Offsets[i] + Len);
		memcpy(&Raw[Offsets[i]], a.Arg[i], Len);
	}
	
	d->Str.Reset(new char[Raw.Length()]);
	memcpy(d->Str, &Raw[0], Raw.Length());
	for (int n=0; n<a.Args; n++)
	{
		d->Ptr[n] = d->Str + Offsets[n];
	}
	
	Args = (int)d->Ptr.Length();
	Arg = (const char**) &d->Ptr[0];
	
	return *this;
}

////////////////////////////////////////////////////////////////
#if defined(MAC_OS_X_VERSION_10_12) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
	#define SDK_10_12(newSym, oldSym) newSym
#else
	#define SDK_10_12(newSym, oldSym) oldSym
#endif

void GMouse::SetFromEvent(NSEvent *ev, NSView *view)
{
	auto r = view.superview.frame;

	GMouse m;
	x = (int)ev.locationInWindow.x;
	y = (int)(r.size.height - ev.locationInWindow.y);
	SetModifer((uint32)ev.modifierFlags);
	Double(ev.clickCount == 2);

	switch (ev.type)
	{
		case SDK_10_12(NSEventTypeLeftMouseDown,  NX_LMOUSEDOWN):
			Down(true); Left(true);
			break;
		case SDK_10_12(NSEventTypeLeftMouseUp,    NX_LMOUSEUP):
			Down(false); Left(true);
			break;
		case SDK_10_12(NSEventTypeRightMouseDown, NX_RMOUSEDOWN):
			Down(true); Right(true);
			break;
		case SDK_10_12(NSEventTypeRightMouseUp,   NX_RMOUSEUP):
			Down(false); Right(true);
			break;
		case SDK_10_12(NSEventTypeOtherMouseDown, NX_OMOUSEDOWN):
			Down(true); Middle(true);
			break;
		case SDK_10_12(NSEventTypeOtherMouseUp,   NX_OMOUSEUP):
			Down(false); Middle(true);
			break;
		case SDK_10_12(NSEventTypeMouseMoved,     NX_MOUSEMOVED):
			IsMove(true);
			break;
		default:
			LgiAssert(!"Unknown event.");
			break;
	}
}

void GUiEvent::SetModifer(uint32 modifierKeys)
{
	System(modifierKeys & SDK_10_12(NSEventModifierFlagCommand, NSCommandKeyMask));
	Shift (modifierKeys & SDK_10_12(NSEventModifierFlagShift,   NSShiftKeyMask));
	Alt   (modifierKeys & SDK_10_12(NSEventModifierFlagOption,  NSAlternateKeyMask));
	Ctrl  (modifierKeys & SDK_10_12(NSEventModifierFlagControl, NSControlKeyMask));
}

void GMessage::Set(int msg, Param A, Param B)
{
	m = msg;
	a = A;
	b = B;
}

////////////////////////////////////////////////////////////////
typedef GArray<GAppInfo*> AppArray;

void OnSigPipe(int i)
{
}

void OnCrash(int i)
{
	printf("%s:%i - on crash.\n", __FILE__, __LINE__);
	signal(SIGBUS, 0);
	signal(SIGSEGV, 0);
	
	struct Pipe
	{
		int Read;
		int Write;
		
		Pipe()
		{
			Read = -1;
			Write = -1;
		}
	};
	
	
	Pipe Read;
	Pipe Write;
	Pipe Error;
	int Pid;
	
	pipe((int*)&Read);
	pipe((int*)&Error);
	
	char Exe[256] = "";
	LgiGetExeFile(Exe, sizeof(Exe));
	
	// Has stdin pipe
	pipe((int*)&Write);
	if (!(Pid = fork()))
	{
		// stdin -> Write
		close(0);			// close stdin
		dup(Write.Read);
		close(Write.Write);
		
		// stdout -> Read
		close(1);			// close stdout
		dup(Read.Write);
		close(Read.Read);
		
		// stderr -> Error
		close(2);			// close stderr
		dup(Error.Write);
		close(Error.Read);
		
		// setup read & write handles
		char sPid[32];
		sprintf(sPid, "--pid=%i", getpid());
		char *Args[] = {sPid, Exe, 0};
		// printf("Calling: execv('gdb', '%s', '%s');\n", Exe, sPid);
		execv("/usr/bin/gdb", Args);
		
		// We should never get here
		printf("%s:%i - execv(gdb) failed.\n", __FILE__, __LINE__);
		exit(-1);
	}
	
	ssize_t r, Used = 0;
	char Buf[1025];
	bool Capture = false;
	while ((r = read(Read.Read, Buf + Used, sizeof(Buf) - Used - 1)) > 0)
	{
		printf("Got %i bytes\n", (int)r);
		Used += r;
		Buf[Used] = 0;
		
		printf("Capt=%i Buf='%s'\n", Capture, Buf);
		if (!Capture)
		{
			if (stristr(Buf, "(gdb)"))
			{
				char c[] = "info pid\n";
				ssize_t w = write(Write.Write, c, strlen(c));
				printf("Writing cmd %i bytes\n", (int)w);
				Capture = true;
				Used = 0;
				Buf[0] = 0;
			}
		}
		
		char *Eol;
		while ((Eol = strstr(Buf, "\n")))
		{
			*Eol = 0;
			
			if (Capture)
			{
				printf("Capture '%s'\n", Buf);
			}
			
			Eol += 1;
			ptrdiff_t Len = Eol - Buf;
			memmove(Buf, Eol, Used + 1 - Len);
			Used -= Len;
		}
	}
	
	exit(-1);
}

class GAppPrivate
{
public:
	NSApplication *NsApp;
	int RunDepth;

	// Common
	GXmlTag *Config;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	GSymLookup SymLookup;
	GAutoString Mime;
	GAutoString Name;
	GAutoString UrlArg;
	
	/// Any fonts needed for styling the elements
	GAutoPtr<GFontCache> FontCache;
	
	GAppPrivate()
	{
		NsApp = NULL;
		RunDepth = 0;
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
		GuiThread = LgiGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
	}
	
	~GAppPrivate()
	{
		DeleteObj(SkinLib);
		
		for (auto p : MimeToApp)
		{
			p.value->DeleteObjects();
			DeleteObj(p.value);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////
GSkinEngine *GApp::SkinEngine = 0;
GApp *TheApp = 0;
GMouseHook *GApp::MouseHook = 0;

GApp::GApp(OsAppArguments &AppArgs, const char *AppName, GAppArguments *ObjArgs) :
OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	d = new GAppPrivate;
	d->Name.Reset(NewStr(AppName));
	AppWnd = 0;
	Name(AppName);
	LgiArgsAppPath = AppArgs.Arg[0];
	
	// Catch and ignore SIGPIPE
	signal(SIGPIPE, OnSigPipe);
	
#if 0
	// Crash handler...
	signal(SIGBUS, OnCrash);
	signal(SIGSEGV, OnCrash);
#endif
	
	// We want our printf's NOW!
	setvbuf(stdout,(char *)NULL,_IONBF,0); // print mesgs immediately.
	
	// Connect to the server
	d->NsApp = [NSApplication sharedApplication];
	
	// Setup the file and graphics sub-systems
	d->FileSystem = new GFileSystem;
	d->GdcSystem = new GdcDevice;
	
	srand((unsigned)LgiCurrentTime());
	LgiInitColours();
	
	SetAppArgs(AppArgs);
	MouseHook = new GMouseHook;
	
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
		else
		{
			printf("%s:%i - Error creating system font.\n", __FILE__, __LINE__);
		}
		
		SystemBold = SysFontType.Create();
		if (SystemBold)
		{
			SystemBold->Bold(true);
			SystemBold->Transparent(true);
			SystemBold->Create();
		}
		else
		{
			printf("%s:%i - Error creating bold version of the system font.\n", __FILE__, __LINE__);
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
#if 0
		// Load dynamic library skin engine
		char Name[64];
#ifdef _DEBUG
		char *Build = "d";
#else
		char *Build = "";
#endif
		
		sprintf(Name, "lgiskin%s.%s", Build, LGI_LIBRARY_EXT);
		d->SkinLib = new GLibrary(Name);
#else
		// Static version of the skinning engine
#endif
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
	}
	
#if 0
	OSStatus e;
	
	// Setup apple event handlers
	/*
	 e = AEInstallEventHandler(	kInternetEventClass,
	 kAEGetURL,
	 NewAEEventHandlerUPP(AppleEventProc),
	 (SRefCon)this,
	 false);
	 printf("AEInstallEventHandler(kInternetEventClass, kAEGetURL, ...)=%i\n", (int)e);
	 if (e) LgiTrace("%s:%i - AEInstallEventHandler = %i\n", _FL, e);
	 e = AEInstallEventHandler(	kCoreEventClass,
	 kAEOpenApplication,
	 NewAEEventHandlerUPP(AppleEventProc),
	 (SRefCon)this,
	 false);
	 LgiTrace("%s:%i - AEInstallEventHandler = %i\n", _FL, e);
	 */
	
	// Setup application handler
	EventTypeSpec	AppEvents[] =
	{
		{ kEventClassApplication, kEventAppActivated       },
		{ kEventClassApplication, kEventAppFrontSwitched   },
		{ kEventClassApplication, kEventAppGetDockTileMenu },
		{ kEventClassCommand,     kEventCommandProcess     },
	};
	
	EventHandlerRef Handler = 0;
	e =	InstallApplicationEventHandler(	NewEventHandlerUPP(AppProc),
									   GetEventTypeCount(AppEvents),
									   AppEvents,
									   (void*)this, &Handler);
	if (e) LgiTrace("%s:%i - InstallEventHandler for app failed (%i)\n", _FL, e);
#endif
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

GApp *GApp::ObjInstance()
{
	return TheApp;
}

bool GApp::IsOk()
{
	bool Status =
					#if !defined(__clang__)
					(this != 0) &&
					#endif
					(d != 0)
	/*
	 #ifdef XWIN
	 && (XDisplay() != 0)
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
		default:
			break;
		case LGI_MET_DECOR_X:
		{
			return 0;
		}
		case LGI_MET_DECOR_Y:
		case LGI_MET_DECOR_CAPTION:
		{
			if (AppWnd)
			{
				#if 0
				Rect r;
				OSStatus e = GetWindowBounds(AppWnd->WindowHandle(), kWindowTitleBarRgn, &r);
				if (e) printf("%s:%i - GetWindowBounds failed with %i\n", _FL, (int)e);
				else
				{
					int y = r.bottom - r.top;
					return y;
				}
				#endif
			}
			
			return 22;
		}
	}
	
	return 0;
}

GViewI *GApp::GetFocus()
{
	#if 0
	WindowRef w = FrontNonFloatingWindow();
	if (w)
	{
		ControlRef v;
		OSErr e = GetKeyboardFocus(w, &v);
		if (e) printf("%s:%i - GetKeyboardFocus failed with %i\n", __FILE__, __LINE__, e);
		else
		{
			GViewI *Ptr = 0;
			UInt32 Size = 0;
			e = GetControlProperty(v, 'meme', 'view', sizeof(Ptr), &Size, &Ptr);
			if (e) printf("%s:%i - GetControlProperty failed with %i\n", __FILE__, __LINE__, e);
			else
			{
				return Ptr;
			}
		}
	}
	else printf("FrontNonFloatingWindow failed.\n");
	#endif
	
	return 0;
}

OsThread GApp::_GetGuiThread()
{
	return d->GuiThread;
}

OsThreadId GApp::GetGuiThreadId()
{
	return d->GuiThreadId;
}

bool GApp::InThread()
{
	return GetCurrentThreadId() == d->GuiThreadId;
}

OsProcessId GApp::GetProcessId()
{
	return getpid();
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

struct IdleGluePtrs
{
	GApp::OnIdleProc Callback;
	void *Param;
};

#define CUSTOM_LOOP		0

#if 0
void IdleGlue(EventLoopTimerRef inTimer, void *inUserData)
{
	IdleGluePtrs *p = (IdleGluePtrs*)inUserData;
	p->Callback(p->Param);
}
#endif

bool GApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (!d->NsApp)
	{
		LgiAssert(!"No d->NsApp");
		return false;
	}

	if (Loop)
	{
		#if CUSTOM_LOOP
		// This impl allows for us to exit gracefully.
		// NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		int Depth = ++d->RunDepth;
		
		do
		{
			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

			NSEvent *event =
				[	d->NsApp
					nextEventMatchingMask:NSAnyEventMask
					untilDate:[NSDate distantFuture]
					inMode:NSDefaultRunLoopMode
					dequeue:YES];

			[d->NsApp sendEvent:event];
			[d->NsApp updateWindows];

			[pool release];
		}
		while (d->RunDepth >= Depth);
		
		// [pool release];
		#else
		NSApplicationMain(GetArgs(), GetArg());
		#endif
		return true;
	}
	else
	{
	}
	
	#if 0
	EventRef theEvent;
	EventTargetRef theTarget;
	theTarget = GetEventDispatcherTarget();
	
	if (Loop)
	{
		OnCommandLine();
		
		EventLoopTimerRef timer;
		IdleGluePtrs Ptrs;
		if (IdleCallback)
		{
			Ptrs.Callback = IdleCallback;
			Ptrs.Param = IdleParam;
			InstallEventLoopTimer(	GetCurrentEventLoop(),
								  0,
								  kEventDurationSecond / 10,
								  NewEventLoopTimerUPP(IdleGlue),
								  &Ptrs,
								  &timer);
		}
		
		do
		{
			RunApplicationEventLoop();
		}
		while (AppWnd &&
			   !AppWnd->CloseRequestDone() &&
			   !AppWnd->OnRequestClose(false));
	}
	else
	{
		while
			(
			 ReceiveNextEvent
			 (
			  0,
			  NULL,
			  0.001, // kEventDurationForever,
			  true,
			  &theEvent
			  )
			 ==
			 noErr
			 )
		{
			SendEventToEventTarget (theEvent, theTarget);
			ReleaseEvent(theEvent);
		}
	}
	#endif
	
	return 0;
}

void GApp::Exit(int Code)
{
	#if CUSTOM_LOOP
	if (!Code)
	{
		if (d->RunDepth > 0)
			d->RunDepth--;
	}
	#else
	if (!Code)
	{
		DeleteObj(AppWnd);
		[NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
	}
	else
	#endif
	{
		// hard exit
		::exit(Code);
	}
}

void GApp::OnUrl(const char *Url)
{
	if (AppWnd)
		AppWnd->OnUrl(Url);
	else
		d->UrlArg.Reset(NewStr(Url));
}

void GApp::OnReceiveFiles(GArray<char*> &Files)
{
	if (AppWnd)
		AppWnd->OnReceiveFiles(Files);
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
		
		if (!d->Config)
		{
			d->Config = new GXmlTag("Options");
		}
	}
	
	if (Tag && d->Config)
	{
		return d->Config->GetChildTag(Tag);
	}
	
	return 0;
}

void GApp::SetConfig(GXmlTag *Tag)
{
	if (IsOk() && Tag)
	{
		GXmlTag *Old = GetConfig(Tag->GetTag());
		if (Old)
		{
			Old->RemoveTag();
			DeleteObj(Old);
		}
		
		if (!d->Config)
		{
			GetConfig(0);
		}
		if (d->Config)
		{
			d->Config->InsertTag(Tag);
		}
	}
}

const char *GApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? d->Args.Arg[n] : 0;
}

bool GApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	GString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
			strcpy_s(Dest, DestLen, Buf);
		return true;
	}
	return false;
}

bool GApp::GetOption(const char *Option, GString &Buf)
{
	if (IsOk() && Option)
	{
		size_t OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			const char *a = d->Args.Arg[i];
			
			if (strchr("-/\\", a[0]))
			{
				if (strncmp(a+1, Option, OptLen) == 0)
				{
					const char *Arg = 0;
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
								size_t Len = End-Arg;
								if (Len > 0)
								{
									Buf.Set(Arg, Len);
								}
								else return false;
							}
							else return false;
						}
						else
						{
							Buf = Arg;
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
	GArray<char*> Files;
	
	for (int i=1; i<GetAppArgs()->Args; i++)
	{
		const char *a = GetAppArgs()->Arg[i];
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

GAutoString GApp::GetFileMimeType(const char *File)
{
	GAutoString Ret;
	
	if (!FileExists(File))
	{
		// Look in the path
		GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
		for (int i=0; i<p.Length(); i++)
		{
			char Full[MAX_PATH];
			LgiMakePath(Full, sizeof(Full), p[i], File);
			if (FileExists(Full))
			{
				break;
			}
		}
	}
	
	char *Ext = LgiGetExtension((char*)File);
	if (Ext)
	{
		#if 0
		CFStringRef e = CFStringCreateWithCString(NULL, Ext, kCFStringEncodingUTF8);
		CFStringRef uti = e != NULL ? UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, e, NULL) : 0;
		CFStringRef mime = uti != NULL ? UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType) : 0;
		
		Ret.Reset(CFStringToUtf8(mime));
		
		if (uti != NULL)
			CFRelease(uti);
		if (mime != NULL)
			CFRelease(mime);
		if (e != NULL)
			CFRelease(e);
		#endif
	}
	
	if (!Ret)
		Ret.Reset(NewStr("application/octet-stream"));
	
	return Ret;
}

bool GApp::GetAppsForMimeType(char *Mime, GArray<GAppInfo*> &Apps)
{
	// Use LSCopyApplicationForMIMEType?
	
	// Find alternative version of the MIME type (e.g. x-type and type).
	char AltMime[256];
	strcpy(AltMime, Mime);
	char *s = strchr(AltMime, '/');
	if (s)
	{
		s++;
		
		size_t Len = strlen(s) + 1;
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
	
	if (!d->MimeToApp.Length())
	{
		// printf("%s:%i - Building MimeToApp.\n", __FILE__, __LINE__);
		
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
	
	return false;
}

GSymLookup *GApp::GetSymLookup()
{
	return &d->SymLookup;
}

bool GApp::IsElevated()
{
	return geteuid() == 0;
}

int GApp::GetCpuCount()
{
	return 1;
}

GFontCache *GApp::GetFontCache()
{
	if (!d->FontCache)
		d->FontCache.Reset(new GFontCache(SystemNormal));
	return d->FontCache;
}

