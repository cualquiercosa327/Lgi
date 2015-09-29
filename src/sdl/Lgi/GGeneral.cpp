#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "Lgi.h"
#include "GRegKey.h"

////////////////////////////////////////////////////////////////
// Local helper functions
bool LgiCheckFile(char *Path, int PathSize)
{
	if (Path)
	{
		if (FileExists(Path))
		{
			// file is there
			return true;
		}
		else
		{
			// shortcut?
			char Link[MAX_PATH];
			sprintf_s(Link, sizeof(Link), "%s.lnk", Path);
			
			// resolve shortcut
			if (FileExists(Link) &&
				ResolveShortcut(Link, Link, sizeof(Link)))
			{
				// check destination of link
				if (FileExists(Link))
				{
					strcpy_s(Path, PathSize, Link);
					return true;
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////
// Implementations
void LgiSleep(uint32 i)
{
	SDL_Delay(i);
}

bool LgiGetMimeTypeExtensions(const char *Mime, GArray<char*> &Ext)
{
	int Start = Ext.Length();
	char *e;

	GRegKey t(false, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type\\%s", Mime);
	if (t.IsOk() && (e = t.GetStr("Extension")))
	{
		if (*e == '.') e++;
		Ext.Add(NewStr(e));
	}
	else
	{
		#define HardCodeExtention(mime, Ext1, Ext2) \
			else if (!stricmp(Mime, mime)) \
			{	if (Ext1) Ext.Add(NewStr(Ext1)); \
				if (Ext2) Ext.Add(NewStr(Ext2)); }

		if (!Mime);
		HardCodeExtention("text/calendar", "ics", 0)
		HardCodeExtention("text/x-vcard", "vcf", 0)
		HardCodeExtention("text/mbox", "mbx", "mbox")
		HardCodeExtention("text/html", "html", 0)
		HardCodeExtention("text/plain", "txt", 0)
		HardCodeExtention("message/rfc822", "eml", 0)
		HardCodeExtention("audio/mpeg", "mp3", 0)
		HardCodeExtention("application/msword", "doc", 0)
		HardCodeExtention("application/pdf", "pdf", 0)
	}

	return Ext.Length() > Start;
}

bool LgiGetFileMimeType(const char *File, char *Mime, int BufLen)
{
	bool Status = false;

	if (File && Mime)
	{
		char *Dot = strrchr((char*)File, '.');
		if (Dot)
		{
			bool AssertOnError = GRegKey::AssertOnError;
			GRegKey::AssertOnError = false;
			
			GRegKey Key(false, "HKEY_CLASSES_ROOT\\%s", Dot);
			if (Key.IsOk())
			{
				char *Ct = Key.GetStr("Content Type");
				if (Ct &&
					!stricmp(Dot, ".dsw") == 0 &&
					!stricmp(Dot, ".dsp") == 0)
				{
					strcpy_s(Mime, BufLen, Ct);
					Status = true;
				}
				else
				{
					// Search mime type DB.
					GRegKey Db(false, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type");
					List<char> Sub;
					Db.GetKeyNames(Sub);
					for (char *k = Sub.First(); k; k = Sub.Next())
					{
						GRegKey Type(false, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type\\%s", k);
						char *Ext = Type.GetStr("Extension");
						if (Ext && stricmp(Ext, Dot) == 0)
						{
							strcpy_s(Mime, BufLen, k);
							Status = true;
							break;
						}
					}

					Sub.DeleteArrays();

					if (!Status)
					{
						// This is a hack to get around file types without a MIME database entry
						// but do have a .ext entry. LgiGetAppsForMimeType knows about the hack too.
						snprintf(Mime, BufLen, "application/%s", Dot);
						Status = true;
					}
				}
			}

			GRegKey::AssertOnError = AssertOnError;
		}

		if (!Status)
		{
			// no extension?
			// no registry entry for file type?
			strcpy_s(Mime, BufLen, "application/octet-stream");
			Status = true;
		}
	}

	return Status;
}

bool _GetApps_Add(GArray<GAppInfo*> &Apps, char *In)
{
	GAutoString Path;

	if (!In)
		return false;
	while (*In && strchr(WhiteSpace, *In))
		In++;
	if (!*In)
		return false;

	for (char *i = In; true; i++)
	{
		if (*i == '\'' || *i == '\"')
		{
			char delim = *i++;
			char *end = strchr(i, delim);
			if (!end) end = i + strlen(i);
			Path.Reset(NewStr(i, end-i));
			In = end + (*end != 0);
			break;
		}
		else if (!*i || strchr(WhiteSpace, *i))
		{
			char old = *i;
			*i = 0;
			if (FileExists(In))
			{
				Path.Reset(NewStr(In));
			}
			*i = old;
			if (Path)
			{
				In = i + (*i != 0);
				break;
			}
		}

		if (!*i)
			break;
	}

	if (Path)
	{
		GStringPipe p;

		char *RootVar = "%SystemRoot%";
		char *SysRoot = stristr(Path, RootVar);
		if (SysRoot)
		{
			// correct path for variables
			char Temp[256];
			GetWindowsDirectory(Temp, sizeof(Temp));
			if (Temp[strlen(Temp)-1] != DIR_CHAR) strcat(Temp, DIR_STR);
			p.Push(Temp);

			char *End = SysRoot + strlen(RootVar);
			p.Push(*End == DIR_CHAR ? End + 1 : End);
		}
		else
		{
			p.Push(Path);
		}

		GAppInfo *a = new GAppInfo;
		if (a)
		{
			Apps[Apps.Length()] = a;
			
			a->Params.Reset(TrimStr(In));
			a->Path.Reset(p.NewStr());
			if (a->Path)
			{
				char e[MAX_PATH];
				char *d = strrchr(a->Path, DIR_CHAR);
				if (d) strcpy_s(e, sizeof(e), d + 1);
				else strcpy_s(e, sizeof(e), a->Path);
				d = strchr(e, '.');
				if (d) *d = 0;
				e[0] = toupper(e[0]);
				a->Name.Reset(NewStr(e));
				if (ValidStr(a->Name))
				{
					bool AllCaps = true;
					for (char *s=a->Name; *s; s++)
					{
						if (islower(*s))
						{
							AllCaps = false;
							break;
						}
					}
					if (AllCaps)
					{
						strlwr(a->Name + 1);
					}
				}
			}

			return true;
		}
	}

	return false;
}

bool LgiGetAppsForMimeType(const char *Mime, GArray<GAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	if (Mime)
	{
		if (stricmp(Mime, "application/email") == 0)
		{
			// get email app
			GRegKey Key(false, "HKEY_CLASSES_ROOT\\mailto\\shell\\open\\command");
			if (Key.IsOk())
			{
				// get app path
				char *Str = Key.GetStr();
				// if (RegQueryValueEx(hKey, 0, 0, &Type, (uchar*)Str, &StrLen) == ERROR_SUCCESS)
				if (Str)
				{
					Status = _GetApps_Add(Apps, Str);
				}
			}
		}
		else if (!stricmp(Mime, "application/browser"))
		{
			// get default browser
			char *Keys[] = { "HKCU", "HKLM" };
			char Base[] = "SOFTWARE\\Clients\\StartMenuInternet";
			for (int i=0; i<CountOf(Keys); i++)
			{
				GRegKey k1(false, "%s\\%s", Keys[i], Base);
				if (k1.IsOk())
				{
					char *Def = k1.GetStr();
					if (Def)
					{
						for (int n=0; n<CountOf(Keys); n++)
						{
							GRegKey k2(false, "%s\\SOFTWARE\\Classes\\Applications\\%s\\shell\\open\\command", Keys[n], Def);
							if (k2.IsOk())
							{
								char *App = k2.GetStr();
								if (Status = _GetApps_Add(Apps, App))
								{
									break;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// get generic app
			char Ext[64] = "";
			char *EndPart = strrchr((char*)Mime, '/');
			if (EndPart && EndPart[1] == '.')
			{
				strcpy_s(Ext, sizeof(Ext), EndPart);

				// This is a hack to get around file types without a MIME database entry
				// but do have a .ext entry. LgiGetFileMimeType knows about the hack too.
				GRegKey ExtEntry(false, "HKEY_CLASSES_ROOT\\%s", Ext + 1);
				char *Name = ExtEntry.GetStr();
				if (Name)
				{
					GRegKey Edit(false, "HKEY_CLASSES_ROOT\\%s\\shell\\edit\\command", Name);
					char *App = Edit.GetStr();
					if (App)
					{
						Status = _GetApps_Add(Apps, App);
					}
					else
					{
						GRegKey Open(false, "HKEY_CLASSES_ROOT\\%s\\shell\\open\\command", Name);
						char *App = Open.GetStr();
						if (App)
						{
							Status = _GetApps_Add(Apps, App);
						}
					}
				}
			}
			else
			{
				// This branch gets the list of application available to edit/open the file

				// Map the MIME type to a .ext
				GRegKey MimeEntry(false, "HKEY_CLASSES_ROOT\\MIME\\Database\\Content Type\\%s", Mime);
				char *e = MimeEntry.IsOk() ? MimeEntry.GetStr("Extension") : NULL;
				if (e)
					strcpy_s(Ext, sizeof(Ext), e);
				if (Ext[0])
				{
					// Get list of "Open With" apps
					GRegKey Other(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithList", Ext);
					char *Mru = NewStr(Other.GetStr("MRUList"));
					if (Mru)
					{
						for (char *a = Mru; *a; a++)
						{
							char Str[] = {*a, 0};
							char *Application = Other.GetStr(Str);
							if (Application)
							{
								GRegKey Shell(false, "HKEY_CLASSES_ROOT\\Applications\\%s\\shell", Application);
								List<char> Keys;
								if (Shell.GetKeyNames(Keys))
								{
									GRegKey First(false, "HKEY_CLASSES_ROOT\\Applications\\%s\\shell\\%s\\command", Application, Keys.First());
									char *Path;
									if (Path = First.GetStr())
									{
										Status |= _GetApps_Add(Apps, Path);
									}
								}
								Keys.DeleteArrays();
							}
						}

						DeleteArray(Mru);
					}

					if (!Status)
					{
						// Explorers file extensions
						GRegKey FileExt(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s", Ext);
						char *Application;
						if (Application = FileExt.GetStr("Application"))
						{
							GRegKey App(false, "HKEY_CLASSES_ROOT\\Applications\\%s\\shell\\open\\command", Application);
							char *Path;
							if (Path = App.GetStr())
							{
								Status = _GetApps_Add(Apps, Path);
							}
						}
					}

					if (!Status)
					{
						// get classes location
						GRegKey ExtEntry(false, "HKEY_CLASSES_ROOT\\%s", Ext);
						GRegKey TypeEntry(false, "HKEY_CLASSES_ROOT\\%s\\shell\\open\\command", ExtEntry.GetStr());
						char *Path = TypeEntry.GetStr();
						if (Path)
						{
							const char *c = Path;
							char *Part = LgiTokStr(c);
							if (Part)
							{
								char AppPath[256];
								snprintf(AppPath, sizeof(AppPath), "\"%s\"", Part);
								Status = _GetApps_Add(Apps, AppPath);

								DeleteArray(Part);
							}
							else
							{
								Status = _GetApps_Add(Apps, Path);
							}
						}
					}
				}
			}
		}
	}

	return Status;
}

bool LgiGetAppForMimeType(const char *Mime, char *AppPath, int BufSize)
{
	bool Status = false;
	if (AppPath)
	{
		GArray<GAppInfo*> Apps;
		Status = LgiGetAppsForMimeType(Mime, Apps, 1);
		if (Status)
		{
			strcpy_s(AppPath, BufSize, Apps[0]->Path);
			Apps.DeleteObjects();
		}
	}
	return Status;
}

uint LgiRand(uint i)
{
	return (rand() % i);
}

bool LgiPlaySound(const char *FileName, int Flags)
{
	bool Status = false;

	HMODULE hDll = LoadLibrary("winmm.dll");
	if (hDll)
	{
		typedef BOOL (__stdcall *Proc_sndPlaySound)(LPCSTR pszSound, UINT fuSound);
		Proc_sndPlaySound psndPlaySound = (Proc_sndPlaySound)GetProcAddress(hDll, "sndPlaySoundA");

		if (psndPlaySound)
		{
			if (LgiGetOs() == LGI_OS_WIN9X)
			{
				// async broken on 98?
				Flags = 0;
			}

			Status = psndPlaySound(FileName, Flags);
		}

		FreeLibrary(hDll);
	}

	return Status;
}

static char *LgiFindArgsStart(char *File)
{
	for (char *a = File; *a; a++)
	{
		if (*a == '\'' || *a == '\"')
		{
			char delim = *a++;
			char *e = strchr(a, delim);
			if (e)
				a = e;
			else
				return 0;
		}
		else if (strchr(" \t\r\n", *a))
		{
			return a;
		}
	}

	return 0;
}

#include <lmerr.h>

GAutoString LgiErrorCodeToString(uint32 ErrorCode)
{
	GAutoString Str;
    HMODULE hModule = NULL;
    LPSTR MessageBuffer;
    DWORD dwBufferLength;
    DWORD dwFormatFlags =	FORMAT_MESSAGE_ALLOCATE_BUFFER |
							FORMAT_MESSAGE_IGNORE_INSERTS |
							FORMAT_MESSAGE_FROM_SYSTEM ;

    if (ErrorCode >= NERR_BASE && ErrorCode <= MAX_NERR)
    {
        hModule = LoadLibraryEx(	TEXT("netmsg.dll"),
									NULL,
									LOAD_LIBRARY_AS_DATAFILE);
        if (hModule != NULL)
            dwFormatFlags |= FORMAT_MESSAGE_FROM_HMODULE;
    }

    if (dwBufferLength = FormatMessageA(dwFormatFlags,
										hModule, // module to get message from (NULL == system)
										ErrorCode,
										MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // default language
										(LPSTR) &MessageBuffer,
										0,
										NULL))
    {
        DWORD dwBytesWritten;
		Str.Reset(TrimStr(NewStr(MessageBuffer, dwBufferLength)));
        LocalFree(MessageBuffer);
    }

    if (hModule != NULL)
        FreeLibrary(hModule);
    
    return Str;
}

bool LgiExecute(const char *File, const char *Arguments, const char *Dir, GAutoString *ErrorMsg)
{
	int Status = 0, Error = 0;
	
	if (!File)
		return false;

	uint64 Now = LgiCurrentTime();
	if (LgiGetOs() == LGI_OS_WIN9X)
	{
		GAutoString f(LgiToNativeCp(File));
		GAutoString a(LgiToNativeCp(Arguments));
		GAutoString d(LgiToNativeCp(Dir));
		if (f)
		{
			Status = (NativeInt) ShellExecuteA(NULL, "open", f, a, d, 5);
			if (Status <= 32)
				Error = GetLastError();
		}
	}
	else
	{
		GAutoWString f(LgiNewUtf8To16(File));
		GAutoWString a(LgiNewUtf8To16(Arguments));
		GAutoWString d(LgiNewUtf8To16(Dir));
		if (f)
		{
			Status = (NativeInt) ShellExecuteW(NULL, L"open", f, a, d, 5);
			if (Status <= 32)
				Error = GetLastError();
		}
	}

	#ifdef _DEBUG
	if (Status <= 32)
		LgiTrace("ShellExecuteW failed with %i (LastErr=0x%x)\n", Status, Error);
	if (LgiCurrentTime() - Now > 1000)
		LgiTrace("ShellExecuteW took %I64i\n", LgiCurrentTime() - Now);
	#endif

	if (ErrorMsg)
		*ErrorMsg = LgiErrorCodeToString(Error);
	
	return Status > 32;
}

////////////////////////////////////////////////////////////////////////////////////
HKEY GetRootKey(char *s)
{
	HKEY Root = 0;

	#define TestKey(Name) \
		if (strncmp(s, #Name, strlen(#Name)) == 0) \
		{ \
			Root = Name; \
		}

	TestKey(HKEY_CLASSES_ROOT)
	else TestKey(HKEY_CURRENT_CONFIG)
	else TestKey(HKEY_CURRENT_USER)
	else TestKey(HKEY_LOCAL_MACHINE)
	else TestKey(HKEY_USERS)

	#undef TestKey

	return Root;
}

bool GRegKey::AssertOnError = true;

GRegKey::GRegKey(bool WriteAccess, char *Key, ...)
{
	char Buffer[1025];

	k = 0;
	s[0] = 0;
	Root = (HKEY)-1;

	va_list Arg;
	va_start(Arg, Key);
	vsprintf(Buffer, Key, Arg);
	va_end(Arg);
	KeyName = NewStr(Buffer);

	if (KeyName)
	{
		int Len = 0;
		char *SubKey = 0;

		#define TestKey(Long, Short) \
			if (!strnicmp(KeyName, #Long, Len = strlen(#Long)) || \
				!strnicmp(KeyName, #Short, Len = strlen(#Short))) \
			{ \
				Root = Long; \
				SubKey = KeyName[Len] ? KeyName + Len + 1 : 0; \
			}
		TestKey(HKEY_CLASSES_ROOT, HKCR)
		else TestKey(HKEY_CURRENT_CONFIG, HKCC)
		else TestKey(HKEY_CURRENT_USER, HKCU)
		else TestKey(HKEY_LOCAL_MACHINE, HKLM)
		else TestKey(HKEY_USERS, HKU)
		else return;

		LONG ret = RegOpenKeyEx(Root, SubKey, 0, WriteAccess ? KEY_ALL_ACCESS : KEY_READ, &k);
		if (ret != ERROR_SUCCESS && ret != ERROR_FILE_NOT_FOUND)
		{			
			DWORD err = GetLastError();
			if (AssertOnError)
				LgiAssert(!"RegOpenKeyEx failed");
		}
	}
}

GRegKey::~GRegKey()
{
	if (k) RegCloseKey(k);
	DeleteArray(KeyName);
}

bool GRegKey::IsOk()
{
	return k != NULL;
}

bool GRegKey::Create()
{
	bool Status = false;

	if (!k && KeyName)
	{
		char *Sub = strchr(KeyName, '\\');
		if (Sub)
		{
			LONG Ret = RegCreateKey(Root, Sub+1, &k);
			if (Ret == ERROR_SUCCESS)
			{
				Status = IsOk();
			}
			else
			{
				DWORD err = GetLastError();
				if (AssertOnError)
					LgiAssert(!"RegCreateKey failed");
			}
		}
	}

	return Status;
}

char *GRegKey::Name()
{
	return KeyName;
}

bool GRegKey::DeleteValue(char *Name)
{
	if (k)
	{
		if (RegDeleteValue(k, Name) == ERROR_SUCCESS)
		{
			return true;
		}
		else
		{
			DWORD Err = GetLastError();
			LgiAssert(!"RegDeleteValue failed");
		}
	}

	return false;
}

bool GRegKey::DeleteKey()
{
	bool Status = false;

	if (k)
	{
		char *n = strchr(KeyName, '\\');
		if (n++)
		{
			RegCloseKey(k);
			k = 0;

			HKEY Root = GetRootKey(KeyName);
			int Ret = RegDeleteKey(Root, n);
			Status = Ret == ERROR_SUCCESS;
			if (!Status)
			{
				if (AssertOnError)
					LgiAssert(!"RegDeleteKey failed.");
			}
			DeleteArray(KeyName);
		}
	}

	return false;
}

char *GRegKey::GetStr(const char *Name)
{
	if (!k)
	{
		LgiAssert(!"No key to read from.");
		return NULL;
	}

	DWORD Size = sizeof(s), Type;
	LONG Ret = RegQueryValueEx(k, Name, 0, &Type, (uchar*)s, &Size);
	if (Ret != ERROR_SUCCESS)
	{
		if (AssertOnError)
			LgiAssert(!"RegQueryValueEx failed.");
		return NULL;
	}

	return s;
}

bool GRegKey::SetStr(const char *Name, const char *Value)
{
	if (!k)
	{
		LgiAssert(!"No key open.");
		return false;
	}

	LONG Ret = RegSetValueEx(k, Name, 0, REG_SZ, (uchar*)Value, Value ? strlen(Value) : 0);
	if (Ret != ERROR_SUCCESS)
	{
		if (AssertOnError)
			LgiAssert(!"RegSetValueEx failed.");
		return false;
	}
	
	return true;
}

int GRegKey::GetInt(char *Name)
{
	int i = 0;
	DWORD Size = sizeof(i), Type;
	if (k && RegQueryValueEx(k, Name, 0, &Type, (uchar*)&i, &Size) != ERROR_SUCCESS)
	{
		i = 0;
	}

	return i;
}

bool GRegKey::SetInt(char *Name, int Value)
{
	if (k)
	{
		LONG r = RegSetValueEx(k, Name, 0, REG_DWORD, (uchar*)&Value, sizeof(Value));
		if (r == ERROR_SUCCESS)
		{
			return true;
		}
	}

	return false;
}

bool GRegKey::GetBinary(char *Name, void *&Ptr, int &Len)
{
	DWORD Size = 0, Type;
	if (k && RegQueryValueEx(k, Name, 0, &Type, 0, &Size) == ERROR_SUCCESS)
	{
		Len = Size;
		Ptr = new uchar[Len];
		return RegQueryValueEx(k, Name, 0, &Type, (uchar*)Ptr, &Size) == ERROR_SUCCESS;
	}

	return false;
}

bool GRegKey::SetBinary(char *Name, void *Ptr, int Len)
{
	return false;
}

bool GRegKey::GetKeyNames(List<char> &n)
{
	FILETIME t;
	char Buf[256];
	DWORD Size = sizeof(Buf), i = 0;
	while (RegEnumKeyEx(k, i++, Buf, &Size, 0, 0, 0, &t) == ERROR_SUCCESS)
	{
		n.Insert(NewStr(Buf));
		Size = sizeof(Buf);
	}
	return n.First() != 0;
}

bool GRegKey::GetValueNames(List<char> &n)
{
	char Buf[256];
	DWORD Type, Size = sizeof(Buf), i = 0;
	while (RegEnumValue(k, i++, Buf, &Size, 0, &Type, 0, 0) == ERROR_SUCCESS)
	{
		n.Insert(NewStr(Buf));
		Size = sizeof(Buf);
	}
	return n.First() != 0;
}

//////////////////////////////////////////////////////////////////////////////////////
GString WinGetSpecialFolderPath(int Id)
{
	GLibrary Shell("Shell32");
	GString s;
	char16 wp[MAX_PATH] = { 0 };
	pSHGetSpecialFolderPathW w = (pSHGetSpecialFolderPathW) Shell.GetAddress("SHGetSpecialFolderPathW");
	if (w)
	{
		BOOL result = w(0, wp, Id, false);
		if (result && ValidStrW(wp))
		{
			GAutoString Tmp(LgiNewUtf16To8(wp));
			s = Tmp;
		}
		else
		{
			DWORD e = GetLastError();
			LgiAssert(!"Error getting system folder.");
		}
	}

	return s;
}

//////////////////////////////////////////////////////////////////////
void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	static bool Asserting = false;

	if (!b)
	{
		#ifdef LGI_STATIC
		assert(b);
		#else
		if (Asserting || !LgiApp)
		{
			// Woah boy...
			assert(0);
		}
		else
		{
			Asserting = true;
			
			printf("%s:%i - Assert failed:\n%s\n", file, line, test);

			#ifdef _DEBUG

			GStringPipe p;
			p.Print("Assert failed, file: %s, line: %i\n%s", file, line, test);
			GAutoPtr<char,true> Msg(p.NewStr());
			GAlert a(0, "Assert Failed", Msg, "Abort", "Debug", "Ignore");
			a.SetAppModal();
			switch (a.DoModal())
			{
				case 1:
				{
					exit(-1);
					break;
				}
				case 2:
				{
					// Bring up the debugger...
					#if defined(_WIN64) || !defined(_MSC_VER)
					assert(0);
					#else
					_asm int 3
					#endif
					break;
				}
				case 3:
				{
					break;
				}
			}

			#endif

			Asserting = false;
		}
		#endif
	}
}

//////////////////////////////////////////////////////////////////////
// The following code is from:
// Web: http://www.codeproject.com/Articles/28071/Toggle-hardware-data-read-execute-breakpoints-prog
// License: http://www.codeproject.com/info/cpol10.aspx
struct HWBRK
{
	void *a;
	HANDLE hT;
	HWBRK_TYPE Type;
	HWBRK_SIZE Size;
	HANDLE hEv;
	int iReg;
	int Opr;
	bool SUCC;

	HWBRK()
	{
		Opr = 0;
		a = 0;
		hT = 0;
		hEv = 0;
		iReg = 0;
		SUCC = false;
	}
};

static void SetBits(DWORD_PTR& dw, int lowBit, int bits, int newValue)
{
	DWORD_PTR mask = (1 << bits) - 1; 
	dw = (dw & ~(mask << lowBit)) | (newValue << lowBit);
}

static DWORD WINAPI BreakpointThread(LPVOID lpParameter)
{
	HWBRK* h = (HWBRK*)lpParameter;
	int j = 0;
	int y = 0;

	j = SuspendThread(h->hT);
    y = GetLastError();

	CONTEXT ct = {0};
	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	j = GetThreadContext(h->hT,&ct);
    y = GetLastError();

	int FlagBit = 0;

	bool Dr0Busy = false;
	bool Dr1Busy = false;
	bool Dr2Busy = false;
	bool Dr3Busy = false;
	if (ct.Dr7 & 1)
		Dr0Busy = true;
	if (ct.Dr7 & 4)
		Dr1Busy = true;
	if (ct.Dr7 & 16)
		Dr2Busy = true;
	if (ct.Dr7 & 64)
		Dr3Busy = true;

	if (h->Opr == 1)
	{
		// Remove
		if (h->iReg == 0)
		{
			FlagBit = 0;
			ct.Dr0 = 0;
			Dr0Busy = false;
		}
		if (h->iReg == 1)
		{
			FlagBit = 2;
			ct.Dr1 = 0;
			Dr1Busy = false;
		}
		if (h->iReg == 2)
		{
			FlagBit = 4;
			ct.Dr2 = 0;
			Dr2Busy = false;
		}
		if (h->iReg == 3)
		{
			FlagBit = 6;
			ct.Dr3 = 0;
			Dr3Busy = false;
		}

		ct.Dr7 &= ~(1 << FlagBit);
	}
	else
	{
		if (!Dr0Busy)
		{
			h->iReg = 0;
			ct.Dr0 = (DWORD_PTR)h->a;
			Dr0Busy = true;
		}
		else if (!Dr1Busy)
		{
			h->iReg = 1;
			ct.Dr1 = (DWORD_PTR)h->a;
			Dr1Busy = true;
		}
		else if (!Dr2Busy)
		{
			h->iReg = 2;
			ct.Dr2 = (DWORD_PTR)h->a;
			Dr2Busy = true;
		}
		else if (!Dr3Busy)
		{
			h->iReg = 3;
			ct.Dr3 = (DWORD_PTR)h->a;
			Dr3Busy = true;
		}
		else
		{
			h->SUCC = false;
			j = ResumeThread(h->hT);
			y = GetLastError();
			SetEvent(h->hEv);
			return 0;
		}
		ct.Dr6 = 0;
		int st = 0;
		if (h->Type == HWBRK_TYPE_CODE)
			st = 0;
		if (h->Type == HWBRK_TYPE_READWRITE)
			st = 3;
		if (h->Type == HWBRK_TYPE_WRITE)
			st = 1;
		int le = 0;
		if (h->Size == HWBRK_SIZE_1)
			le = 0;
		if (h->Size == HWBRK_SIZE_2)
			le = 1;
		if (h->Size == HWBRK_SIZE_4)
			le = 3;
		if (h->Size == HWBRK_SIZE_8)
			le = 2;

		SetBits(ct.Dr7, 16 + h->iReg*4, 2, st);
		SetBits(ct.Dr7, 18 + h->iReg*4, 2, le);
		SetBits(ct.Dr7, h->iReg*2,1,1);
	}

	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	j = SetThreadContext(h->hT,&ct);
    y = GetLastError();

	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	j = GetThreadContext(h->hT,&ct);
    y = GetLastError();

	j = ResumeThread(h->hT);
    y = GetLastError();

	h->SUCC = true;

	SetEvent(h->hEv);
	return 0;
}

HANDLE SetHardwareBreakpoint(HANDLE hThread, HWBRK_TYPE Type, HWBRK_SIZE Size, void *s)
{
	HWBRK *h = new HWBRK;
	h->a = s;
	h->Size = Size;
	h->Type = Type;
	h->hT = hThread;

	if (hThread == GetCurrentThread())
	{
		DWORD pid = GetCurrentThreadId();
		h->hT = OpenThread(THREAD_ALL_ACCESS,0,pid);
	}

	h->hEv = CreateEvent(0, 0, 0, 0);
	h->Opr = 0; // Set Break
	HANDLE hY = CreateThread(0, 0, BreakpointThread, (LPVOID)h, 0, 0);
	WaitForSingleObject(h->hEv, INFINITE);
	CloseHandle(h->hEv);
	h->hEv = 0;

	if (hThread == GetCurrentThread())
	{
		CloseHandle(h->hT);
	}

	h->hT = hThread;

	if (!h->SUCC)
	{
		delete h;
		return 0;
	}

	return (HANDLE)h;
}

bool RemoveHardwareBreakpoint(HANDLE hBrk)
{
	HWBRK* h = (HWBRK*)hBrk;
	if (!h)
		return false;

	bool C = false;
	if (h->hT == GetCurrentThread())
	{
		DWORD pid = GetCurrentThreadId();
		h->hT = OpenThread(THREAD_ALL_ACCESS,0,pid);
		C = true;
	}

	h->hEv = CreateEvent(0,0,0,0);
	h->Opr = 1; // Remove Break
	HANDLE hY = CreateThread(0,0,BreakpointThread,(LPVOID)h,0,0);
	WaitForSingleObject(h->hEv,INFINITE);
	CloseHandle(h->hEv);
	h->hEv = 0;

	if (C)
	{
		CloseHandle(h->hT);
	}

	delete h;
	return true;
}

