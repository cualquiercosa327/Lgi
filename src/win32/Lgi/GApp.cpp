#include <stdio.h>

#include "Lgi.h"
#include <ole2.h>
#include <commctrl.h>
#include <time.h>
#include "GSkinEngine.h"
#include "GSymLookup.h"
#include "GDocView.h"
#include "GToken.h"

HINSTANCE _lgi_app_instance = 0;
extern LPTOP_LEVEL_EXCEPTION_FILTER _PrevExceptionHandler;

OsAppArguments::OsAppArguments()
{
	_Default();
}

OsAppArguments::OsAppArguments(int Args, char **Arg)
{
	_Default();
	Set(Args, Arg);
}

OsAppArguments &OsAppArguments::operator =(OsAppArguments &p)
{
	hInstance = p.hInstance;
	Pid = p.Pid;
	nCmdShow = p.nCmdShow;
	lpCmdLine = 0;
	
	CmdLine.Reset(NewStrW(p.lpCmdLine));
	lpCmdLine = CmdLine;
	return *this;
}

void OsAppArguments::_Default()
{
	hInstance = 0;
	lpCmdLine = 0;
	Pid = GetCurrentProcessId();
	nCmdShow = SW_RESTORE;
}

void OsAppArguments::Set(int Args, char **Arg)
{
	GStringPipe p;
	for (int i=0; i<Args; i++)
	{
		p.Print("%s%s", i?" ":"", Arg[i]);
	}
	GAutoString s(p.NewStr());
	Set(s);
}

void OsAppArguments::Set(char *Utf)
{
	CmdLine.Reset(LgiNewUtf8To16(Utf));
	lpCmdLine = CmdLine;
}

////////////////////////////////////////////////////////////////////////////////
static GApp *TheApp = 0;
GApp *GApp::ObjInstance()
{
	return TheApp;
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
	OsThreadId GuiThread;
	int LinuxWine;
	GAutoString Name, Mime, ProductId;

	// Win32
	bool QuitReceived;
	List<GWin32Class> Classes;
	GSymLookup *SymLookup;

	GAppPrivate()
	{
		LinuxWine = -1;
		SymLookup = 0;
		QuitReceived = false;
		SkinLib = 0;
		GuiThread = LgiGetCurrentThread();
	}

	~GAppPrivate()
	{
		DeleteObj(SkinLib);
		DeleteObj(SymLookup);
	}
};

/////////////////////////////////////////////////////////////////////////////
LONG __stdcall _ExceptionFilter_Redir(LPEXCEPTION_POINTERS e)
{
	if (LgiApp)
		return LgiApp->_ExceptionFilter(e, LgiApp->d->ProductId);
	else
		LgiTrace("_ExceptionFilter_Redir error: No application ptr.\n");

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
GSkinEngine *GApp::SkinEngine = 0;
GMouseHook *GApp::MouseHook = 0;
GMouseHook *GApp::GetMouseHook()
{
	return MouseHook;
}

static GAutoString ParseStr(GPointer &p, bool Pad = true)
{
	char16 *Key = p.w;
	// Skip 'key' string
	while (*p.w)
		p.w++;
	// Skip NULL
	p.w++;
	if (Pad)
	{
		// Align to 32-bit boundry
		while ((int)p.u8 & 3)
			p.u8++;
	}

	return GAutoString(LgiNewUtf16To8(Key));
}

static GAutoString ParseVer(void *Resource, char *Part)
{
	GToken Parts(Part, ".");
	if (Parts.Length() == 3)
	{
		GPointer p;
		p.u8 = (uint8*)Resource;
		uint16 Len = *p.u16++;
		uint16 ValueLen = *p.u16++;
		uint16 Type = *p.u16++;
		GAutoString Key = ParseStr(p);

		// Read VS_FIXEDFILEINFO structure
		DWORD dwSig = *p.u32++;
		DWORD dwStrucVersion = *p.u32++;
		DWORD dwFileVersionMS = *p.u32++;
		DWORD dwFileVersionLS = *p.u32++;
		DWORD dwProductVersionMS = *p.u32++;
		DWORD dwProductVersionLS = *p.u32++;
		DWORD dwFileFlagsMask = *p.u32++;
		DWORD dwFileFlags = *p.u32++;
		DWORD dwFileOS = *p.u32++;
		DWORD dwFileType = *p.u32++;
		DWORD dwFileSubtype = *p.u32++;
		DWORD dwFileDateMS = *p.u32++;
		DWORD dwFileDateLS = *p.u32++;

		// Align to 32-bit boundry
		while ((int)p.u8 & 3)
			p.u8++;

		// Children
		while (p.u8 < (uint8*)Resource + Len)
		{
			// Read StringFileInfo structures...
			uint8 *fStart = p.u8;
			uint16 fLength = *p.u16++;
			uint16 fValueLength = *p.u16++;
			uint16 fType = *p.u16++;
			GAutoString fKey = ParseStr(p);
			if (strcmp(fKey, "StringFileInfo"))
				break;

			while (p.u8 < fStart + fLength)
			{
				// Read StringTable entries
				uint8 *tStart = p.u8;
				uint16 tLength = *p.u16++;
				uint16 tValueLength = *p.u16++;
				uint16 tType = *p.u16++;
				GAutoString tKey = ParseStr(p);

				while (p.u8 < tStart + tLength)
				{
					// Read String entries
					uint8 *sStart = p.u8;
					uint16 sLength = *p.u16++;
					uint16 sValueLength = *p.u16++;
					uint16 sType = *p.u16++;
					GAutoString sKey = ParseStr(p);
					GAutoString sValue;
					if (p.u8 < sStart + sLength)
						sValue = ParseStr(p);

					if (!stricmp(Parts[0], fKey) &&
						!stricmp(Parts[1], tKey) &&
						!stricmp(Parts[2], sKey))
					{
						return sValue;
					}
					
					// LgiTrace("%s.%s.%s = %s\n", fKey.Get(), tKey.Get(), sKey.Get(), sValue.Get());
				}
			}
		}
	}

	return GAutoString();
}

/////////////////////////////////////////////////////////////////////////////
extern int MouseRollMsg;

bool GApp::Win9x = LgiGetOs() == LGI_OS_WIN9X;

GApp::GApp(char *MimeType, OsAppArguments &AppArgs, GAppArguments *ObjArgs)
{
int64 Time = LgiCurrentTime();
#define DumpTime(str) /* \
	{ int64 n = LgiCurrentTime(); \
	LgiTrace("%s=%ims\n", str, (int)(n-Time)); \
	Time = n; } */

	// Sanity Checks
	LgiAssert(sizeof(int8) == 1);
	LgiAssert(sizeof(uint8) == 1);

	LgiAssert(sizeof(int16) == 2);
	LgiAssert(sizeof(uint16) == 2);

	LgiAssert(sizeof(int32) == 4);
	LgiAssert(sizeof(uint32) == 4);

	LgiAssert(sizeof(int64) == 8);
	LgiAssert(sizeof(uint64) == 8);

	LgiAssert(sizeof(char16) == 2);

DumpTime("start");

	// Private data
	d = new GAppPrivate;
	d->Mime.Reset(NewStr(MimeType));

DumpTime("priv");

	// Setup exception handler
	HRSRC hRsrc = ::FindResource(NULL, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
	HGLOBAL hGlobal = ::LoadResource(NULL, hRsrc);
    LPVOID pVersionResource = ::LockResource(hGlobal);
	GAutoString ProductName, ProductVer;

    // replace "040904e4" with the language ID of your resources
	if (pVersionResource)
	{
		ProductName = ParseVer(pVersionResource, "StringFileInfo.0c0904b0.ProductName");
		ProductVer = ParseVer(pVersionResource, "StringFileInfo.0c0904b0.ProductVersion");
		// VerQueryValue(pVersionResource, "\\StringFileInfo\\0c0904b0\\ProductName", &pvProductName, &iProductNameLen);
		// VerQueryValue(pVersionResource, "\\StringFileInfo\\0c0904b0\\ProductVersion", &pvProductVersion, &iProductVersionLen);
	}
	if (ProductName && ProductVer)
	{
		char s[256];
		sprintf(s, "%s-%s", ProductName.Get(), ProductVer.Get());
		d->ProductId.Reset(NewStr(s));
	}

    InitializeCriticalSection(&StackTraceSync);
	#ifndef _DEBUG
	_PrevExceptionHandler = SetUnhandledExceptionFilter(_ExceptionFilter_Redir);
	#endif

DumpTime("exception handler");

	// Initialize windows dll's
	OleInitialize(NULL);
	CoInitialize(NULL);
	InitCommonControls();

DumpTime("init common ctrls");

	// Setup LGI Sub-systems
	GFontSystem::Inst();

DumpTime("font sys");

	d->FileSystem = new GFileSystem;

DumpTime("file sys");

	d->GdcSystem = new GdcDevice;

DumpTime("gdc");

	// Vars...
	d->Config = 0;
	AppWnd = 0;
	SetAppArgs(AppArgs);

	// GApp instance
	LgiAssert(TheApp == 0);
	TheApp = this;

DumpTime("vars");

	// System font
	SystemNormal = 0;
	SystemBold = 0;
	GFontType SysFontType;
	if (SysFontType.GetSystemFont("System"))
	{
		SystemNormal = SysFontType.Create();
		if (SystemNormal)
		{
			// Force load
			SystemNormal->Create();
		}

		SystemBold = SysFontType.Create();
		if (SystemBold)
		{
			SystemBold->Bold(true);
			SystemBold->Create();
		}
	}

	if (!SystemNormal)
	{
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error");
		LgiExitApp();
	}

DumpTime("fonts");

	// Other vars and init
	hNormalCursor = LoadCursor(NULL, IDC_ARROW);
	LgiRandomize(LgiCurrentTime()*LgiGetCurrentThread());
	MouseRollMsg = RegisterWindowMessage("MSWHEEL_ROLLMSG");

DumpTime("cursor/rand/msg");

	LgiInitColours();

DumpTime("colours");

	// Setup mouse hook
	MouseHook = new GMouseHook;

DumpTime("ms hook");

	if
	(
		(
			!ObjArgs
			OR
			!ObjArgs->NoSkin
		)
		AND
		!GetOption("noskin")
	)
	{
		#ifdef SKIN_MAGIC

		char *SkinFile = LgiFindFile("PC_Angel_2.smf");
		if (!SkinFile)
		{
			SkinFile = NewStr("C:\\Public\\SoftThinks\\Skinning\\PC_Angel_2.smf");
		}

		bool SkinInit = InitSkinMagicLib(AppArgs.hInstance,	"Dialog", 0, 0);
		if (SkinInit)
		{
			if (LoadSkinFile(SkinFile))
			{
				SetDialogSkin("Dialog");
			}
			else
			{
				LgiMsg(0, "LoadSkinFile failed.\n", "Error");
			}
		}
		else
		{
			LgiMsg(0, "InitSkinMagicLib failed.\n", "Error");
		}

		DeleteArray(SkinFile);

		#else

		// Load library
		char SkinLibName[] =
		#ifdef __GNUC__
			"lib"
		#endif
			"lgiskin"
		#if _MSC_VER == 1300
			"7"
		#endif
		#if _MSC_VER == 1400
			"8"
		#endif
		#if _MSC_VER == 1500
			"9"
		#endif
		#if _MSC_VER == 1600
			"10"
		#endif
		#ifdef _DEBUG
			"d"
		#endif
			;
		
		d->SkinLib = new GLibrary(SkinLibName);
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
				printf("Failed to load '%s'\n", SkinLibName);
			}
		}

		#endif
	}

DumpTime("skin");
}

GApp::~GApp()
{
	#ifdef SKIN_MAGIC
	
	ExitSkinMagicLib();
	
	#endif

	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	DeleteObj(MouseHook);

	LgiResources::ResourceContainers.DeleteObjects();

	TheApp = 0;
	DeleteObj(SkinEngine);

	DeleteObj(GFontSystem::Me);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(d->Config);
	d->Classes.DeleteObjects();

	CoUninitialize();
	OleUninitialize();

	DeleteObj(d);
	DeleteCriticalSection(&StackTraceSync);
}

bool GApp::IsOk()
{
	bool Status =	(this != 0) AND
					(d != 0) AND
					(d->FileSystem != 0) AND
					(d->GdcSystem != 0);
	LgiAssert(Status);
	return Status;
}

char *GApp::GetName()
{
	if (d->Name)
		return d->Name;
	
	if (d->Mime)
	{
		char *i = strchr(d->Mime, '/');
		if (i)
		{
			static char s[128];
			i++;
			if (ToLower(i[0]) == 'x' && i[1] == '-')
				i += 2;
			char *o = s;
			bool First = true;
			while (*i && o < s + sizeof(s) - 1)
			{
				if (*i != '-')
				{
					if (First)
					{
						*o++ = ToUpper(*i);
						First = false;
					}
					else
						*o++ = *i;
				}
				else First = true;
				i++;
			}
			*o = 0;
			
			return s;
		}
	}
	
	return 0;
}

void GApp::SetName(char *Name)
{
	d->Name.Reset(Name);
}

OsThreadId GApp::GetGuiThread()
{
	return d->GuiThread;
}

List<GWin32Class> *GApp::GetClasses()
{
	return IsOk() ? &d->Classes : 0;
}

OsAppArguments *GApp::GetAppArgs()
{
	return IsOk() ? &d->Args : 0;
}

GXmlTag *GApp::GetConfig(char *Tag)
{
	if (IsOk() AND !d->Config)
	{
		char File[] = "lgi.conf";
		char Path[256];
		if (LgiGetExePath(Path, sizeof(Path)))
		{
			if (Path[strlen(Path)-1] != DIR_CHAR) strcat(Path, DIR_STR);
			strcat(Path, File);

			/*
			if (!FileExists(Path))
			{
				char *f = LgiFindFile(File);
				if (f)
				{
					strcpy(Path, f);
					DeleteArray(f);
				}
			}
			*/

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

	if (Tag AND d->Config)
	{
		return d->Config->GetTag(Tag);
	}

	return 0;
}

void GApp::SetConfig(GXmlTag *Tag)
{
	if (IsOk() AND Tag)
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
	if (d->Args.lpCmdLine)
	{
		char16 *s = d->Args.lpCmdLine;
		for (int i=0; i<=n; i++)
		{
			char16 *e = 0;
			while (*s AND strchr(WhiteSpace, *s)) s++;
			if (*s == '\'' OR *s == '\"')
			{
				char16 Delim = *s++;
				e = StrchrW(s, Delim);
			}
			else
			{
				for (e = s; *e AND !strchr(WhiteSpace, *e); e++)
					;
			}

			if (i == n)
			{
				return LgiNewUtf16To8(s, (e - s) * sizeof(char16));
			}

			s = *e ? e + 1 : e;
		}
	}

	return 0;
}

bool GApp::GetOption(char *Option, GArray<char> &Buf)
{
	if (!ValidStr(Option))
	{
		return false;
	}

	char16 *c = d->Args.lpCmdLine;
	char16 *Opt = LgiNewUtf8To16(Option);
	int OptLen = StrlenW(Opt);

	while (c AND *c)
	{
		if (*c == '/' OR *c == '-')
		{
			c++;
			char16 *e = c;
			while (*e && (IsLetter(*e)) || StrchrW(L"_-", *e))
				e++;

			if (e - c == OptLen &&
				!StrnicmpW(c, Opt, OptLen))
			{
				c += OptLen;
				if (*c)
				{
					// skip leading whitespace
					while (*c AND strchr(WhiteSpace, *c))
					{
						c++;
					}

					// write str out if they want it
					char16 End = (*c == '\'' OR *c == '\"') ? *c++ : ' ';
					char16 *e = StrchrW(c, End);
					if (!e) e = c + StrlenW(c);
					char *Arg = LgiNewUtf16To8(c, (int)e-(int)c);
					if (Arg)
					{
						int ArgLen = strlen(Arg);
						Buf.Length(ArgLen + 1);
						strcpy(&Buf[0], Arg);
						DeleteArray(Arg);
					}
				}

				// yeah we got the option
				DeleteArray(Opt);
				return true;
			}
		}

		c = StrchrW(c, ' ');
		if (c) c++;
	}

	DeleteArray(Opt);
	return false;
}

bool GApp::GetOption(char *Option, char *Dest, int DestLen)
{
	GArray<char> Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
		{
			strsafecpy(Dest, &Buf[0], DestLen);
		}
		return true;
	}
	return false;
}

// #include "GInput.h"

void GApp::SetAppArgs(OsAppArguments &AppArgs)
{
	d->Args = AppArgs;
}

void GApp::OnCommandLine()
{
	char WhiteSpace[] = " \r\n\t";
	char *CmdLine = LgiNewUtf16To8(d->Args.lpCmdLine);

	if (ValidStr(CmdLine))
	{
		GArray<char*> Files;
		
		char *Delim = "\'\"";
		char *s;
		for (s = CmdLine; *s; )
		{
			// skip ws
			while (*s AND strchr(WhiteSpace, *s)) s++;

			// read to end of token
			char *e = s;
			if (strchr(Delim, *s))
			{
				char Delim = *s++;
				e = strchr(s, Delim);
			}
			else
			{
				for (; *e AND !strchr(WhiteSpace, *e); e++);
			}

			char *Arg = NewStr(s, (int)e-(int)s);
			if (Arg)
			{
				if (FileExists(Arg))
				{
					Files.Add(Arg);
				}
				else
				{
					DeleteArray(Arg);
				}
			}

			// next
			s = (*e) ? e + 1 : e;
		}

		// call app
		if (Files.Length() > 0)
		{
			OnReceiveFiles(Files);
		}

		// clear up
		Files.DeleteArrays();
	}

	DeleteArray(CmdLine);
}

void GApp::OnReceiveFiles(GArray<char*> &Files)
{
	if (AppWnd)
	{
		AppWnd->OnReceiveFiles(Files);
	}
}

int32 GApp::GetMetric(int Metric)
{
	int32 Status = 0;

	switch (Metric)
	{
		case LGI_MET_DECOR_X:
		{
			Status = GetSystemMetrics(SM_CXFRAME) * 2;
			break;
		}
		case LGI_MET_DECOR_Y:
		{
			Status = GetSystemMetrics(SM_CYFRAME) * 2;
			Status += GetSystemMetrics(SM_CYCAPTION);
			break;
		}
		case LGI_MET_DECOR_CAPTION:
		{
			Status = GetSystemMetrics(SM_CYCAPTION);
			break;
		}
		case LGI_MET_MENU:
		{
			Status = GetSystemMetrics(SM_CYMENU);
			break;
		}
	}

	return Status;
}

GViewI *GApp::GetFocus()
{
	HWND h = ::GetFocus();
	if (h)
	{
		return GWindowFromHandle(h);
	}

	return 0;
}

HINSTANCE GApp::GetInstance()
{
	if (this AND IsOk())
	{
		return d->Args.hInstance;
	}

	return (HINSTANCE)GetCurrentProcess();
}

OsProcessId GApp::GetProcessId()
{
	if (this)
	{
		return IsOk() ? d->Args.Pid : 0;
	}

	return GetCurrentProcessId();
}

int GApp::GetShow()
{
	return IsOk() ? d->Args.nCmdShow : 0;
}

class GWnd
{
public:
	GSemaphore *GetLock(GWindow *w)
	{
		return w->_Lock;
	}
};

bool GApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	MSG Msg;
	bool status = true;
	ZeroObj(Msg);

	if (Loop)
	{
		OnCommandLine();

		if (IdleCallback)
		{
			bool DontWait = true;

			while (!d->QuitReceived)
			{
				while (1)
				{
					bool Status;
					if (DontWait)
					{
						Status = PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE);
					}
					else
					{
						Status = GetMessage(&Msg, NULL, 0, 0) > 0;
						DontWait = true;
					}
					if (!Status || Msg.message == WM_QUIT)
						break;

					#if 0 //def _DEBUG
					int64 Last = LgiCurrentTime();
					#endif
					
					TranslateMessage(&Msg);
					DispatchMessage(&Msg);

					#if 0 //def _DEBUG
					int64 Now = LgiCurrentTime();
					if (Now - Last > 1000)
					{
						LgiTrace("%s:%i - Msg Loop Blocked: %i ms (Msg: %i)\n",
							__FILE__, __LINE__,
							(int) (Now - Last), Msg.message);
					}
					#endif
				}

				if (Msg.message == WM_QUIT)
				{
					d->QuitReceived = true;
				}
				else
				{
					DontWait = IdleCallback(IdleParam);
				}
			}
		}
		else
		{	
			while (!d->QuitReceived && GetMessage(&Msg, NULL, 0, 0) > 0)
			{
				#ifdef _DEBUG
				int64 Last = LgiCurrentTime();
				#endif
				
				TranslateMessage(&Msg);
				DispatchMessage(&Msg);

				#ifdef _DEBUG
				int64 Now = LgiCurrentTime();
				if (Now - Last > 1000)
				{
					LgiTrace("%s:%i - Msg Loop Blocked: %i ms (Msg: %i)\n",
						__FILE__, __LINE__,
						(int) (Now - Last), Msg.message);
				}
				#endif
			}
		}
	}
	else
	{
		while (	PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) &&
				Msg.message != WM_QUIT)
		{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}

		if (Msg.message == WM_QUIT)
		{
			d->QuitReceived = true;
		}
	}

	return Msg.message != WM_QUIT;
}

void GApp::Exit(int Code)
{
	if (Code)
	{
		exit(0);
	}
	else
	{
		PostQuitMessage(Code);
	}
}

GAutoString GApp::GetFileMimeType(char *File)
{
	GAutoString r;
	char m[128];
	if (LgiGetFileMimeType(File, m, sizeof(m)))
		r.Reset(NewStr(m));
	return r;
}

bool GApp::GetAppsForMimeType(char *Mime, GArray<GAppInfo*> &Apps)
{
	LgiAssert(!"Not impl.");
	return false;
}

GSymLookup *GApp::GetSymLookup()
{
	if (!this)
		return 0;

	if (!d->SymLookup)
		d->SymLookup = new GSymLookup;
	
	return d->SymLookup;
}

bool GApp::IsWine()
{
	if (d->LinuxWine < 0)
	{
		HMODULE hntdll = GetModuleHandle("ntdll.dll");
		if (hntdll)
		{
			typedef const char * (CDECL *pwine_get_version)(void);
			pwine_get_version wine_get_version = (pwine_get_version)GetProcAddress(hntdll, "wine_get_version");
			d->LinuxWine = wine_get_version != 0;
		}
	}

	return d->LinuxWine > 0;
}
