#define _WIN32_WINNT 0x600
#ifndef _MSC_VER
#include <basetyps.h>
#endif
#include "Lgi.h"
#include <ole2.h>
#include <commctrl.h>
#include <time.h>
#include "GSkinEngine.h"
#include "GSymLookup.h"
#include "GDocView.h"
#include "GToken.h"
#include <stdio.h>

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
	GAutoString Mime, ProductId;
	bool ThemeAware;

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
		ThemeAware = true;
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
		while ((NativeInt)p.u8 & 3)
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
		while ((NativeInt)p.u8 & 3)
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
				}
			}
		}
	}

	return GAutoString();
}

/////////////////////////////////////////////////////////////////////////////
#include <Shlwapi.h>
#include <uxtheme.h>
extern int MouseRollMsg;
typedef HRESULT (CALLBACK *fDllGetVersion)(DLLVERSIONINFO *);

bool GApp::Win9x = LgiGetOs() == LGI_OS_WIN9X;

GApp::GApp(OsAppArguments &AppArgs, const char *AppName, GAppArguments *ObjArgs)
{
	// GApp instance
	LgiAssert(TheApp == 0);
	TheApp = this;
	LgiAssert(AppName);
	Name(AppName);

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
	char Mime[256];
	sprintf_s(Mime, sizeof(Mime), "application/x-%s", AppName);
	d->Mime.Reset(NewStr(Mime));

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
	}
	if (ProductName && ProductVer)
	{
		char s[256];
		sprintf_s(s, sizeof(s), "%s-%s", ProductName.Get(), ProductVer.Get());
		d->ProductId.Reset(NewStr(s));
	}

    InitializeCriticalSection(&StackTraceSync);
	#if !defined(_DEBUG)
	_PrevExceptionHandler = SetUnhandledExceptionFilter(_ExceptionFilter_Redir);
	#endif

DumpTime("exception handler");

	// Initialize windows dll's
	OleInitialize(NULL);
	CoInitialize(NULL);
	InitCommonControls();
	
	{
		/*
		GLibrary ComCtl32("ComCtl32.dll");
		DLLVERSIONINFO info;
		ZeroObj(info);
		info.cbSize = sizeof(info);
		fDllGetVersion DllGetVersion = (fDllGetVersion)ComCtl32.GetAddress("DllGetVersion");
		if (DllGetVersion)
		{
			HRESULT ret = DllGetVersion(&info);
			d->ThemeAware = info.dwMajorVersion >= 6;
			LgiTrace("ComCtl32.dll v%i.%i found (ret=%x)\n", info.dwMajorVersion, info.dwMinorVersion, ret);
			)
		*/
			
		GArray<int> Ver;
		LgiGetOs(&Ver);
		if (Ver.Length() > 1)
		{
			// LgiTrace("Windows v%i.%i\n", Ver[0], Ver[1]);
			if (Ver[0] < 6)
			{
				d->ThemeAware = false;
			}
		}
		
		#ifdef _MSC_VER
		if (!d->ThemeAware)
		{
			SetThemeAppProperties(0);
		}
		#endif
	}
		
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
		#if 0 // defined(LGI_STATIC) && _MSC_VER < 1600
		0
		#else
		(
			!ObjArgs
			||
			!ObjArgs->NoSkin
		)
		&&
		!GetOption("noskin")
		#endif
	)
	{
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
		
		#if HAS_SHARED_OBJECT_SKIN
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
		#else
		extern GSkinEngine *CreateSkinEngine(GApp *App);
		SkinEngine = CreateSkinEngine(this);
		#endif
	}

DumpTime("skin");
}

GApp::~GApp()
{
	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	DeleteObj(MouseHook);

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
	bool Status =	(this != 0) &&
					(d != 0) &&
					(d->FileSystem != 0) &&
					(d->GdcSystem != 0);
	if (!Status)
		LgiAssert(!"Hash table error");
	return Status;
}

int GApp::GetCpuCount()
{
	SYSTEM_INFO si;
	ZeroObj(si);
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
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

GXmlTag *GApp::GetConfig(const char *Tag)
{
	if (IsOk() && !d->Config)
	{
		const char File[] = "lgi.conf";
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

char *GApp::GetArgumentAt(int n)
{
	if (d->Args.lpCmdLine)
	{
		char16 *s = d->Args.lpCmdLine;
		for (int i=0; i<=n; i++)
		{
			char16 *e = 0;
			while (*s && strchr(WhiteSpace, *s)) s++;
			if (*s == '\'' || *s == '\"')
			{
				char16 Delim = *s++;
				e = StrchrW(s, Delim);
			}
			else
			{
				for (e = s; *e && !strchr(WhiteSpace, *e); e++)
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

bool GApp::GetOption(const char *Option, GAutoString &Buf)
{
	if (!ValidStr(Option))
	{
		return false;
	}

	char16 *c = d->Args.lpCmdLine;
	char16 *Opt = LgiNewUtf8To16(Option);
	int OptLen = StrlenW(Opt);

	while (c && *c)
	{
		if (*c == '/' || *c == '-')
		{
			c++;
			char16 *e = c;
			while (*e && (IsAlpha(*e)) || StrchrW(L"_-", *e))
				e++;

			if (e - c == OptLen &&
				!StrnicmpW(c, Opt, OptLen))
			{
				c += OptLen;
				if (*c)
				{
					// skip leading whitespace
					while (*c && strchr(WhiteSpace, *c))
					{
						c++;
					}

					// write str out if they want it
					char16 End = (*c == '\'' || *c == '\"') ? *c++ : ' ';
					char16 *e = StrchrW(c, End);
					if (!e) e = c + StrlenW(c);
					Buf.Reset(LgiNewUtf16To8(c, (NativeInt)e-(NativeInt)c));
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

bool GApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	GAutoString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
			strcpy_s(Dest, DestLen, &Buf[0]);
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
		// LgiTrace("CmdLine='%s'\n", CmdLine);
		GArray<char*> Files;
		
		char *Delim = "\'\"";
		char *s;
		for (s = CmdLine; *s; )
		{
			// skip ws
			while (*s && strchr(WhiteSpace, *s)) s++;

			// read to end of token
			char *e = s;
			if (strchr(Delim, *s))
			{
				char Delim = *s++;
				e = strchr(s, Delim);
				if (!e)
					e = s + strlen(s);
			}
			else
			{
				for (; *e && !strchr(WhiteSpace, *e); e++)
					;
			}

			char *Arg = NewStr(s, e - s);
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

int32 GApp::GetMetric(LgiSystemMetric Metric)
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
		case LGI_MET_THEME_AWARE:
		{
			Status = d->ThemeAware;
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
	if (this && IsOk())
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
	GMutex *GetLock(GWindow *w)
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
					
					#if 0
					char m[256];
					sprintf_s(m, sizeof(m), "Msg=%i hwnd=%p %i,%i\n", Msg.message, Msg.hwnd, Msg.wParam, Msg.lParam);
					OutputDebugStringA(m);
					#endif
					
					if (!Status || Msg.message == WM_QUIT)
						break;

					#ifdef _DEBUG
					int64 Last = LgiCurrentTime();
					#endif
					
					TranslateMessage(&Msg);
					DispatchMessage(&Msg);

					#ifdef _DEBUG
					int64 Now = LgiCurrentTime();
					if (Now - Last > 10000)
					{
						LgiTrace("%s:%i - Msg Loop Blocked: %i ms (Msg: 0x%.4x)\n",
							_FL, (int) (Now - Last), Msg.message);
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
					LgiTrace("%s:%i - Msg Loop Blocked: %i ms (Msg: 0x%.4x)\n",
						_FL,
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

GAutoString GApp::GetFileMimeType(const char *File)
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

bool GApp::IsElevated()
{
    bool fRet = false;

    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken ))
    {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(Elevation);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize))
            fRet = Elevation.TokenIsElevated;
    }
    if (hToken)
        CloseHandle(hToken);

    return fRet;
}