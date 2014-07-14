#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GSkinEngine.h"
#include "GArray.h"
#include "GToken.h"
#include "GThread.h"
#include "GXmlTree.h"
#include "GSymLookup.h"

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

OsAppArguments::OsAppArguments(int args, char **arg)
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
	GArray<int> Offsets;

	char Exe[256];
	Offsets.Add(0);
	if (LgiGetExeFile(Exe, sizeof(Exe)))
	{
		int Len = strlen(Exe);
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
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
}

OsAppArguments &OsAppArguments::operator =(OsAppArguments &a)
{
	GArray<char> Raw;
	GArray<int> Offsets;
	for (int i=0; i<a.Args; i++)
	{
		int Len = strlen(a.Arg[i]) + 1;
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
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
	
	return *this;
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
	
	int r, Used = 0;
	char Buf[1025];
	bool Capture = false;
	while ((r = read(Read.Read, Buf + Used, sizeof(Buf) - Used - 1)) > 0)
	{
		printf("Got %i bytes\n", r);
		Used += r;
		Buf[Used] = 0;

		printf("Capt=%i Buf='%s'\n", Capture, Buf);
		if (!Capture)
		{
			if (stristr(Buf, "(gdb)"))
			{
				char c[] = "info pid\n";
				int w = write(Write.Write, c, strlen(c));
				printf("Writing cmd %i bytes\n", w);
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
			int Len = Eol - Buf;
			memmove(Buf, Eol, Used + 1 - Len);
			Used -= Len;
		}
	}

	exit(-1);
}

class GAppPrivate
{
public:
	// Common
	GXmlTag *Config;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	GHashTable MimeToApp;
	OsThread GuiThread;
	GSymLookup SymLookup;
	GAutoString Mime;
	GAutoString Name;
	GAutoString UrlArg;

	GAppPrivate()
	{
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
		GuiThread = LgiGetCurrentThread();
	}

	~GAppPrivate()
	{
		DeleteObj(SkinLib);
		
		for (void *p = MimeToApp.First(); p; p = MimeToApp.Next())
		{
			AppArray *a = (AppArray*)p;
			a->DeleteObjects();
			DeleteObj(a);
		}
	}
};

pascal OSStatus AppProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	OSStatus result = eventNotHandledErr;
	
	GApp *a = (GApp*)inUserData;
	if (!a) return result;
	
	UInt32 eventClass = GetEventClass( inEvent );
	UInt32 eventKind = GetEventKind( inEvent );

	#if 0
	UInt32 ec = LgiSwap32(eventClass);
	UInt32 ek = LgiSwap32(eventKind);
	printf("AppProc %4.4s - %u\n", (char*)&ec, (unsigned)eventKind);
	#endif

	switch (eventClass)
	{
		case kEventClassCommand:
		{
			switch (eventKind)
			{
				case kEventCommandProcess:
				{
					HICommand command;
					result = GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(command), NULL, &command);
					if (result)
						printf("%s:%i - GetEventParameter failed with %i\n", _FL, (int)result);
					else if (a->AppWnd)
					{
						a->AppWnd->OnTrayMenuResult(command.commandID);
					}
				}
			}
			break;
		}
		case kEventClassApplication:
		{
			switch (eventKind)
			{
				case kEventAppGetDockTileMenu:
				{
					if (a->AppWnd)
					{
						GSubMenu s;
						a->AppWnd->OnTrayMenu(s);
						MenuRef gDockMenu = s.Release();
						CFRetain(gDockMenu);
						SetEventParameter(	inEvent,
											kEventParamMenuRef,
											typeMenuRef,
											sizeof(MenuRef),
											&gDockMenu);
						result = noErr;
					}
					break;
				}
				case kEventAppActivated:
				case kEventAppFrontSwitched:
				{
					if (eventKind == kEventAppFrontSwitched)
					{
						ProcessSerialNumber frontProcess, ourProcess;
						
						GetCurrentProcess(&ourProcess);
					
						// Get the new process ID out
						if (GetEventParameter(inEvent,
											kEventParamProcessID,
											typeProcessSerialNumber,
											NULL,
											sizeof(ProcessSerialNumber),
											NULL,
											&frontProcess) == noErr)
						{
							if (frontProcess.highLongOfPSN != ourProcess.highLongOfPSN ||
								frontProcess.lowLongOfPSN != ourProcess.lowLongOfPSN)
								break;
						}
					}
				
					GWindow *a = LgiApp->AppWnd;
					if (a)
					{
						/*
						printf("%s:%i - onfrontswitch(true) %x act=%i switch=%i\n", _FL,
							eventKind,
							eventKind == kEventAppActivated,
							eventKind == kEventAppFrontSwitched);
						*/
						a->OnFrontSwitch(true);
					}
					
					result = noErr;
					break;
				}
			}
			break;
		}
	}
	
	return result;
}

pascal OSErr AppleEventProc(const AppleEvent *ae, AppleEvent *reply, SRefCon handlerRefcon)
{
	OSErr err = eventNotHandledErr;
	GApp *App = (GApp*) handlerRefcon;

	printf("********** AppleEventProc called ***************\n");
	#if 0
	LgiTrace("AppleEvent received\n");

    OSType aeID = typeWildCard;
    OSType aeClass = typeWildCard;
	
	AEGetAttributePtr(ae, keyEventClassAttr, typeType, 0, &aeClass, sizeof(aeClass), 0);
    AEGetAttributePtr(ae, keyEventIDAttr,    typeType, 0, &aeID,    sizeof(aeID), 0);
	
	uint32 id = LgiSwap32(aeID), cls = LgiSwap32(aeClass);

	if (aeClass == kInternetEventClass &&
		aeID    == kAEGetURL)
	{
		DescType type;
		Size dataSize = 0, size = 0;

		err = AESizeOfParam(ae, keyDirectObject, &type, &dataSize);
		if (err != noErr)
			return err;

		GAutoString Url(new char[dataSize + 1]);

		err = AEGetParamPtr(ae, keyDirectObject, typeChar, &type, Url, dataSize, &size);
		if (err != noErr)
			return err;

		Url[size] = 0;
		App->OnUrl(Url);
	}
	else if (aeClass == kCoreEventClass &&
			 aeID    == kAEOpenApplication)
	{
		AEDescList docs;
		err = AEGetParamDesc(ae, keyDirectObject, typeAEList, &docs);
		LgiTrace("%s:%i - AEGetParamDesc = %i\n", _FL, err);
		if (err == noErr)
		{
			long n = 0;
			AECountItems(&docs, &n);
			LgiTrace("%s:%i - n = %i\n", _FL, n);

			UInt8 strBuffer[256];
			GArray<char*> Files;
			GArray<GAutoString> Mem;
			for (int i = 0; i < n; i++)
			{
				FSRef ref;
				
				err = AEGetNthPtr(&docs, i + 1, typeFSRef, 0, 0, &ref, sizeof(ref), 0);
				LgiTrace("%s:%i - AEGetNthPtr = %i\n", _FL, err);
				if (err != noErr)
					continue;
					
				err = FSRefMakePath(&ref, strBuffer, 256);
				LgiTrace("%s:%i - FSRefMakePath = %i\n", _FL, err);
				if (err == noErr)
				{
					Mem[i].Reset(NewStr((char*)strBuffer));
					Files[i] = Mem[i].Get();
				}
			}

			if (Files.Length())
				App->OnReceiveFiles(Files);
		}
	}
	#endif
	
	return err;
}

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

	// Catch and ignore SIGPIPE
	signal(SIGPIPE, OnSigPipe);
	
#if 0
	// Crash handler...
	signal(SIGBUS, OnCrash);
	signal(SIGSEGV, OnCrash);
#endif

	// We want our printf's NOW!
	setvbuf(stdout,(char *)NULL,_IONBF,0); // print mesgs immediately.

	// Setup the file and graphics sub-systems
	d->FileSystem = new GFileSystem;
	d->GdcSystem = new GdcDevice;

	srand(LgiCurrentTime());
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

	#if 1
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
	bool Status = 	(this != 0) &&
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
				Rect r;
				OSStatus e = GetWindowBounds(AppWnd->WindowHandle(), kWindowTitleBarRgn, &r);
				if (e) printf("%s:%i - GetWindowBounds failed with %i\n", _FL, (int)e);
				else
				{
					int y = r.bottom - r.top;
					return y;
				}
			}
			
			return 22;
		}
	}

	return 0;
}

GViewI *GApp::GetFocus()
{
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
	
	return 0;
}

OsThreadId GApp::GetGuiThread()
{
	return d->GuiThread;
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

void IdleGlue(EventLoopTimerRef inTimer, void *inUserData)
{
	IdleGluePtrs *p = (IdleGluePtrs*)inUserData;
	p->Callback(p->Param);
}

bool GApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
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

	return 0;
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
		QuitApplicationEventLoop();
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
		return d->Config->GetTag(Tag);
	}

	return 0;
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
			GetConfig(0);
		}
		if (d->Config)
		{
			d->Config->InsertTag(Tag);
		}
	}
}

char *GApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? d->Args.Arg[n] : 0;
}

bool GApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	GAutoString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
			strcpy_s(Dest, DestLen, Buf);
		return true;
	}
	return false;
}

bool GApp::GetOption(const char *Option, GAutoString &Buf)
{
	if (IsOk() && Option)
	{
		int OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			char *a = d->Args.Arg[i];

			if (strchr("-/\\", a[0]))
			{
				if (strncmp(a+1, Option, OptLen) == 0)
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
								{
									Buf.Reset(NewStr(Arg, Len));
								}
								else return false;
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
