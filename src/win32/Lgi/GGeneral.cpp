// Win32 Implementation of General LGI functions
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "Lgi.h"
#include "GRegKey.h"

////////////////////////////////////////////////////////////////
// Local helper functions
bool _lgi_check_file(char *Path)
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
			char *e = Path + strlen(Path);
			strcpy(e, ".lnk");
			if (FileExists(Path))
			{
				// resolve shortcut
				char Link[256];
				if (ResolveShortcut(Path, Link, sizeof(Link)))
				{
					// check destination of link
					if (FileExists(Link))
					{
						strcpy(Path, Link);
						return true;
					}
				}
			}
			*e = 0;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////
// Implementations
void LgiSleep(DWORD i)
{
	::Sleep(i);
}

bool LgiGetMimeTypeExtensions(const char *Mime, GArray<char*> &Ext)
{
	int Start = Ext.Length();
	char *e;

	GRegKey t("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type\\%s", Mime);
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
			GRegKey Key("HKEY_CLASSES_ROOT\\%s", Dot);
			if (Key.IsOk())
			{
				char *Ct = Key.GetStr("Content Type");
				if (Ct AND
					!stricmp(Dot, ".dsw") == 0 AND
					!stricmp(Dot, ".dsp") == 0)
				{
					strsafecpy(Mime, Ct, BufLen);
					Status = true;
				}
				else
				{
					// Search mime type DB.
					GRegKey Db("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type");
					List<char> Sub;
					Db.GetKeyNames(Sub);
					for (char *k = Sub.First(); k; k = Sub.Next())
					{
						GRegKey Type("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type\\%s", k);
						char *Ext = Type.GetStr("Extension");
						if (Ext AND stricmp(Ext, Dot) == 0)
						{
							strsafecpy(Mime, k, BufLen);
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
		}

		if (!Status)
		{
			// no extension?
			// no registry entry for file type?
			strsafecpy(Mime, "application/octet-stream", BufLen);
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
				char e[256];
				char *d = strrchr(a->Path, DIR_CHAR);
				if (d) strcpy(e, d + 1);
				else strcpy(e, a->Path);
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
			GRegKey Key("HKEY_CLASSES_ROOT\\mailto\\shell\\open\\command");
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
				GRegKey k1("%s\\%s", Keys[i], Base);
				if (k1.IsOk())
				{
					char *Def = k1.GetStr();
					if (Def)
					{
						for (int n=0; n<CountOf(Keys); n++)
						{
							GRegKey k2("%s\\SOFTWARE\\Classes\\Applications\\%s\\shell\\open\\command", Keys[n], Def);
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
			if (EndPart AND EndPart[1] == '.')
			{
				strsafecpy(Ext, EndPart, sizeof(Ext));

				// This is a hack to get around file types without a MIME database entry
				// but do have a .ext entry. LgiGetFileMimeType knows about the hack too.
				GRegKey ExtEntry("HKEY_CLASSES_ROOT\\%s", Ext + 1);
				char *Name = ExtEntry.GetStr();
				if (Name)
				{
					GRegKey Edit("HKEY_CLASSES_ROOT\\%s\\shell\\edit\\command", Name);
					char *App = Edit.GetStr();
					if (App)
					{
						Status = _GetApps_Add(Apps, App);
					}
					else
					{
						GRegKey Open("HKEY_CLASSES_ROOT\\%s\\shell\\open\\command", Name);
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
				GRegKey MimeEntry("HKEY_CLASSES_ROOT\\MIME\\Database\\Content Type\\%s", Mime);
				char *e = MimeEntry.GetStr("Extension");
				if (e)
					strsafecpy(Ext, e, sizeof(Ext));
				if (Ext[0])
				{
					// Get list of "Open With" apps
					GRegKey Other("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithList", Ext);
					char *Mru = NewStr(Other.GetStr("MRUList"));
					if (Mru)
					{
						for (char *a = Mru; *a; a++)
						{
							char Str[] = {*a, 0};
							char *Application = Other.GetStr(Str);
							if (Application)
							{
								GRegKey Shell("HKEY_CLASSES_ROOT\\Applications\\%s\\shell", Application);
								List<char> Keys;
								if (Shell.GetKeyNames(Keys))
								{
									GRegKey First("HKEY_CLASSES_ROOT\\Applications\\%s\\shell\\%s\\command", Application, Keys.First());
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
						GRegKey FileExt("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s", Ext);
						char *Application;
						if (Application = FileExt.GetStr("Application"))
						{
							GRegKey App("HKEY_CLASSES_ROOT\\Applications\\%s\\shell\\open\\command", Application);
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
						GRegKey ExtEntry("HKEY_CLASSES_ROOT\\%s", Ext);
						GRegKey TypeEntry("HKEY_CLASSES_ROOT\\%s\\shell\\open\\command", ExtEntry.GetStr());
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
			strsafecpy(AppPath, Apps[0]->Path, BufSize);
			Apps.DeleteObjects();
		}
	}
	return Status;
}

int LgiRand(int i)
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

bool LgiExecute(const char *File, const char *Arguments, const char *Dir)
{
	int Status = 0;
	
	if (!File)
		return false;

	#if 0
	bool IsWeb = !strnicmp(File, "http://", 7) ||
				 !strnicmp(File, "https://", 8);
	GAutoString FileMem, ArgsMem;
	if (IsWeb)
	{
		// Find the application instead of letting ShellExecute do the work.
		GRegKey h("HKCR\\http\\shell\\open\\command");
		if (h.IsOk())
		{
			char *Path = h.GetStr();
			if (Path)
			{
				int PathLen = strlen(Path);
				int FileLen = strlen(File);
				char *Pc = strchr(Path, '%');
				if (Pc && Pc[1])
				{
					FileMem.Reset(new char[PathLen+FileLen]);
					memcpy(FileMem, Path, Pc-Path);
					sprintf(FileMem+(Pc-Path), "%s%s", File, Pc + 2);
					File = FileMem;

					char *a = LgiFindArgsStart(File);
					if (a)
					{
						*a++ = 0;
						Arguments = a;
					}
				}
				else
				{
					FileMem.Reset(NewStr(Path));
					char *a = LgiFindArgsStart(FileMem);
					if (a)
					{
						*a++ = 0;

						ArgsMem.Reset(new char[strlen(a)+strlen(File)+5]);
						sprintf(ArgsMem, "%s \"%s\"", a, File);
						Arguments = ArgsMem;
						File = FileMem;
					}
					else
					{
						Arguments = File;
						File = FileMem;
					}
				}
			}
		}
	}
	#endif

	if (LgiGetOs() == LGI_OS_WIN9X)
	{
		char *f = LgiToNativeCp(File);
		char *a = LgiToNativeCp(Arguments);
		char *d = LgiToNativeCp(Dir);
		if (f)
		{
			Status = (NativeInt) ShellExecute(NULL, "open", f, a, d, 5);
		}
		DeleteArray(f);
		DeleteArray(a);
		DeleteArray(d);
	}
	else
	{
		char16 *f = LgiNewUtf8To16(File);
		char16 *a = LgiNewUtf8To16(Arguments);
		char16 *d = LgiNewUtf8To16(Dir);
		if (f)
		{
			int64 Now = LgiCurrentTime();
			Status = (NativeInt) ShellExecuteW(NULL, L"open", f, a, d, 5);
			#ifdef _DEBUG
			if (LgiCurrentTime() - Now > 1000)
				LgiTrace("ShellExecuteW took %I64i\n", LgiCurrentTime() - Now);
			#endif
		}
		DeleteArray(f);
		DeleteArray(a);
		DeleteArray(d);
	}
	
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

GRegKey::GRegKey(char *Key, ...)
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

		RegOpenKeyEx(Root, SubKey, 0, KEY_ALL_ACCESS, &k);
	}
}

GRegKey::~GRegKey()
{
	if (k) RegCloseKey(k);
	DeleteArray(KeyName);
}

bool GRegKey::IsOk()
{
	return k != 0;
}

bool GRegKey::Create()
{
	bool Status = false;

	if (!k AND KeyName)
	{
		char *Sub = strchr(KeyName, '\\');
		if (Sub)
		{
			Status = RegCreateKey(Root, Sub+1, &k) == ERROR_SUCCESS;
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
			int asd=0;
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
			    LgiAssert(!"RegDeleteKey failed.");
			DeleteArray(KeyName);
		}
	}

	return false;
}

char *GRegKey::GetStr(char *Name)
{
	DWORD Size = sizeof(s), Type;
	if (k AND RegQueryValueEx(k, Name, 0, &Type, (uchar*)s, &Size) == ERROR_SUCCESS)
	{
		return s;
	}

	return 0;		
}

bool GRegKey::SetStr(char *Name, const char *Value)
{
	return k AND RegSetValueEx(k, Name, 0, REG_SZ, (uchar*)Value, Value ? strlen(Value) : 0) == ERROR_SUCCESS;
}

int GRegKey::GetInt(char *Name)
{
	int i = 0;
	DWORD Size = sizeof(i), Type;
	if (k AND RegQueryValueEx(k, Name, 0, &Type, (uchar*)&i, &Size) != ERROR_SUCCESS)
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
	if (k AND RegQueryValueEx(k, Name, 0, &Type, 0, &Size) == ERROR_SUCCESS)
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
char *GetWindowsFolder(int Id)
{
	GLibrary Shell("Shell32");
	
	if (LgiGetOs() == LGI_OS_WIN9X)
	{
		char ap[MAX_PATH] = "";
		pSHGetSpecialFolderPathA a = (pSHGetSpecialFolderPathA) Shell.GetAddress("SHGetSpecialFolderPathA");

		if (a AND a(0, ap, Id, false))
		{
			if (ValidStr(ap))
			{
				char *s = LgiFromNativeCp(ap);

				return s;
			}
		}
	}
	else
	{
		char16 wp[MAX_PATH] = { 0 };
		pSHGetSpecialFolderPathW w = (pSHGetSpecialFolderPathW) Shell.GetAddress("SHGetSpecialFolderPathW");
		if (w AND w(0, wp, Id, false))
		{
			if (ValidStrW(wp))
			{
				return LgiNewUtf16To8(wp);
			}
		}
		else
		{
			DWORD e = GetLastError();
			int asd=0;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////
void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	static bool Asserting = false;

	if (!b && !Asserting)
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
				#ifdef _WIN64
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
}