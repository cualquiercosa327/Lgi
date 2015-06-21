//
//	Cross platform LGI functions
//
#define _WIN32_WINNT 0x501

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef WINDOWS
#include <winsock2.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

#include "Lgi.h"
#include "GToken.h"
#include "GCapabilities.h"

#if defined(LINUX)
#include "LgiWinManGlue.h"
#elif defined(WINDOWS)
#include "GRegKey.h"
#endif

#if defined POSIX
#include <sys/time.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/utsname.h>
#include "GProcess.h"
#elif defined BEOS
#include <Path.h>
#endif

#if defined(WIN32) && defined(__GTK_H__)
#include "../win32/GSymLookup.h"
#else
#include "GSymLookup.h"
#endif
#include "GLibrary.h"

//////////////////////////////////////////////////////////////////////////
// Misc stuff
#if defined MAC && !defined COCOA
bool _get_path_FSRef(FSRef &fs, GStringPipe &a)
{
	HFSUniStr255 Name;
	ZeroObj(Name);
	FSRef Parent;
	FSCatalogInfo Cat;
	ZeroObj(Cat);
	OSErr e = FSGetCatalogInfo(&fs,
								kFSCatInfoVolume|kFSCatInfoNodeID,
								&Cat,
								&Name,
								NULL,
								&Parent);
	if (!e)
	{
		if (_get_path_FSRef(Parent, a))
		{
			GAutoString u((char*)LgiNewConvertCp("utf-8", Name.unicode, "utf-16", Name.length * sizeof(Name.unicode[0]) ));

			// printf("CatInfo = '%s' %x %x\n", u.Get(), Cat.nodeID, Cat.volume);
			if (u && Cat.nodeID > 2)
			{
				a.Print("%s%s", DIR_STR, u.Get());
			}
		}
		
		return true;
	}

	return false;
}

GAutoString FSRefPath(FSRef &fs)
{
	GStringPipe a;
	if (_get_path_FSRef(fs, a))
	{
		return GAutoString(a.NewStr());
	}
	
	return GAutoString();
}
#endif

bool LgiPostEvent(OsView Wnd, int Event, GMessage::Param a, GMessage::Param b)
{
	#if WINNATIVE

	return PostMessage(Wnd, Event, a, b);

	#elif defined(__GTK_H__)

	if (Wnd)
	{
		GMessage m(0);
		m.Set(Event, a, b);
		return m.Send(Wnd);
	}
	else
	{
		printf("%s:%i - Warning: LgiPostEvent failed because View=0\n", _FL);
	}

	#elif defined(BEOS)
	
	if (Wnd)
	{
		BMessage Msg(Event);
		Msg.AddInt32("a", a);
		Msg.AddInt32("b", b);

		BMessenger m(Wnd);
		return m.SendMessage(&Msg) == B_OK;
	}

	#elif defined(MAC) && !defined COCOA
	
	#if 0
	int64 Now = LgiCurrentTime();
	static int64 Last = 0;
	static int Count = 0;
	
	Count++;
	if (Now > Last + 1000)
	{
		printf("Sent %i events in the last %ims\n", Count, (int)(Now-Last));
		Last = Now;
		Count = 0;
	}
	#endif
	
	EventRef Ev;
	OSStatus e = CreateEvent(NULL,
							kEventClassUser,
							kEventUser,
							0, // EventTime 
							kEventAttributeNone,
							&Ev);
	if (e)
	{
		printf("%s:%i - CreateEvent failed with %i\n", __FILE__, __LINE__, (int)e);
	}
	else
	{
		EventTargetRef t = GetControlEventTarget(Wnd);
		
		e = SetEventParameter(Ev, kEventParamLgiEvent, typeUInt32, sizeof(Event), &Event);
		if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
		e = SetEventParameter(Ev, kEventParamLgiA, typeUInt32, sizeof(a), &a);
		if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
		e = SetEventParameter(Ev, kEventParamLgiB, typeUInt32, sizeof(b), &b);
		if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
		
		// printf("Sent event %x,%x,%x\n", Event, a, b);
		bool Status = false;

		#if 1
		
		e = SetEventParameter(Ev, kEventParamPostTarget, typeEventTargetRef, sizeof(t), &t);
		if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
		e = PostEventToQueue(GetMainEventQueue(), Ev, kEventPriorityStandard);
		if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
		else Status = true;
		
		#else

		e = SendEventToEventTarget(Ev, GetControlEventTarget(Wnd));
		if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, e);
		else Status = true;
		
		#endif

		ReleaseEvent(Ev);
		
		return Status;
	}
	
	#endif

	return false;
}

void LgiExitApp()
{
	exit(0);
}

//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
bool RegisterActiveXControl(char *Dll)
{
	GLibrary Lib(Dll);
	if (Lib.IsLoaded())
	{
		#ifdef _MSC_VER
		typedef HRESULT (STDAPICALLTYPE *p_DllRegisterServer)(void);

		p_DllRegisterServer DllRegisterServer = (p_DllRegisterServer)Lib.GetAddress("DllRegisterServer");
		if (DllRegisterServer)
		{
			return DllRegisterServer() == S_OK;
		}
		#else
		LgiAssert(!"Not impl.");
		#endif
	}

	return false;
}
#endif

//////////////////////////////////////////////////////////////////////////

/// \brief Returns the operating system that Lgi is running on.
/// \sa Returns one of the defines starting with LGI_OS_UNKNOWN in LgiDefs.h
int LgiGetOs
(
	/// Returns the version of the OS or NULL if you don't care
	GArray<int> *Ver
)
{
	#if defined(WIN32) || defined(WIN64)

	static int Os = LGI_OS_UNKNOWN;
	static int Version = 0, Revision = 0;

	if (Os == LGI_OS_UNKNOWN)
	{
		OSVERSIONINFO v;
		v.dwOSVersionInfoSize=sizeof(v);
		GetVersionEx(&v);

		Version = v.dwMajorVersion;
		Revision = v.dwMinorVersion;
		
		#ifdef WIN32
		BOOL IsWow64 = FALSE;
		IsWow64Process(GetCurrentProcess(), &IsWow64);
		#endif

		Os = (v.dwPlatformId == VER_PLATFORM_WIN32_NT)
			?
			#ifdef WIN32
			(IsWow64 ? LGI_OS_WIN64 : LGI_OS_WIN32)
			#else
			LGI_OS_WIN64
			#endif
			:
			LGI_OS_WIN9X;
	}

	if (Ver)
	{
		Ver->Add(Version);
		Ver->Add(Revision);
	}

	return Os;

	#elif defined BEOS

	if (Ver)
	{
		Ver->Add(5);
		Ver->Add(0);
	}
	
	return LGI_OS_BEOS;

	#elif defined LINUX

	if (Ver)
	{
		utsname Buf;
		if (!uname(&Buf))
		{
			GToken t(Buf.release, ".");
			for (int i=0; i<t.Length(); i++)
			{
				Ver->Add(atoi(t[i]));
			}
		}
	}

	return LGI_OS_LINUX;

	#elif defined MAC && !defined COCOA

	if (Ver)
	{
		SInt32 i;

		if (Gestalt(gestaltSystemVersionMajor, &i) == noErr)
		{
			Ver->Add(i);
			if (Gestalt(gestaltSystemVersionMinor, &i) == noErr)
			{
				Ver->Add(i);
				if (Gestalt(gestaltSystemVersionBugFix, &i) == noErr)
				{
					Ver->Add(i);
				}
			}
		}
		else if (Gestalt(gestaltSystemVersion, &i) == noErr)
		{
			char s[10];
			sprintf_s(s, sizeof(s), "%x", (int)i);
			char *e = s + strlen(s) - 1;
			char a[3] = { e[-1], 0 };
			char b[3] = { e[0], 0 };
			e[-1] = 0;
			
			Ver->Add(atoi(s));
			Ver->Add(htoi(a));
			Ver->Add(htoi(b));
		}
	}
	
	return LGI_OS_MAC_OS_X;

	#else

	return LGI_OS_UNKNOWN;

	#endif
}

const char *LgiGetOsName()
{
	const char *Str[LGI_OS_MAX] =
	{
		"Unknown",
		"Win9x",
		"Win32",
		"Win64",
		"Haiku",
		"Linux",
		"MacOSX",
	};

	return Str[LgiGetOs()];
}

#ifdef WIN32
#define RecursiveFileSearch_Wildcard "*.*"
#else // unix'ish OS
#define RecursiveFileSearch_Wildcard "*"
#endif

bool LgiRecursiveFileSearch(const char *Root,
							GArray<const char*> *Ext,
							GArray<char*> *Files,
							uint64 *Size,
							uint64 *Count,
							RecursiveFileSearch_Callback Callback,
							void *UserData)
{
	bool Status = false;

	// validate args
	if (!Root) return 0;

	// get directory enumerator
	GDirectory Dir;
	Status = true;

	// enumerate the directory contents
	for (bool Found = Dir.First(Root); Found; Found = Dir.Next())
	{
		char Name[256];
		if (!Dir.Path(Name, sizeof(Name)))
			continue;

		if (Callback)
		{
			if (!Callback(UserData, Name, &Dir))
			{
				continue;
			}
		}

		if (Dir.IsDir())
		{
			// dir
			LgiRecursiveFileSearch(	Name,
									Ext,
									Files,
									Size,
									Count,
									Callback,
									UserData);

		}
		else
		{
			// process file
			bool Match = true; // if no Ext's then default to match
			if (Ext)
			{
				for (int i=0; i<Ext->Length(); i++)
				{
					const char *e = (*Ext)[i];
					char *RawFile = strrchr(Name, DIR_CHAR);
					if (RawFile &&
						(Match = MatchStr(e, RawFile+1)))
					{
						break;
					}
				}
			}

			if (Match)
			{
				// file matched... process:
				if (Files)
				{
					Files->Add(NewStr(Name));
				}

				if (Size)
				{
					*Size += Dir.GetSize();
				}

				if (Count)
				{
					Count++;
				}

				Status = true;
			}
		}
	}

	return Status;
}

#define LGI_TRACE_TO_FILE
// #include <malloc.h>

#ifndef WIN32
#define _vsnprintf vsnprintf
#endif

static GStreamI *_LgiTraceStream = NULL;

void LgiTraceSetStream(GStreamI *stream)
{
	_LgiTraceStream = stream;
}

bool LgiTraceGetFilePath(char *LogPath, int BufLen)
{
	if (!LogPath)
		return false;

	if (LgiGetExeFile(LogPath, BufLen))
	{
		#ifdef MAC
		char *Dir = strrchr(LogPath, DIR_CHAR);
		if (Dir)
		{
			char Part[256];
			strcpy_s(Part, sizeof(Part), Dir+1);
            LogPath[0] = 0;
			LgiMakePath(LogPath, BufLen, LogPath, "~/Library/Logs");
			LgiMakePath(LogPath, BufLen, LogPath, Dir+1);
			strcat_s(LogPath, BufLen, ".txt");
		}
		else
		#endif
		{
			char *Dot = strrchr(LogPath, '.');
			if (Dot && !strchr(Dot, DIR_CHAR))
				strcpy(Dot+1, "txt");
			else
				strcat(LogPath, ".txt");
		}

		GFile f;		
		if (f.Open(LogPath, O_WRITE))
		{
			f.Close();
		}
		else
		{
			char Leaf[64];
			char *Dir = strrchr(LogPath, DIR_CHAR);
			if (Dir)
			{
				strcpy_s(Leaf, sizeof(Leaf), Dir + 1);
				LgiGetSystemPath(LSP_APP_ROOT, LogPath, BufLen);
				if (!DirExists(LogPath))
					FileDev->CreateFolder(LogPath);
				LgiMakePath(LogPath, BufLen, LogPath, Leaf);
			}
			else goto OnError;
		}
		
		#if 0
		GFile tmp;
		if (tmp.Open("c:\\temp\\log.txt", O_WRITE))
		{
			tmp.SetSize(0);
			tmp.Write(LogPath, strlen(LogPath));
		}
		#endif
	}
	else
	{
		// Well what to do now? I give up
		OnError:
		strcpy_s(LogPath, BufLen, "trace.txt");
		return false;
	}
	
	return true;
}

void LgiTrace(const char *Msg, ...)
{
	#if defined _INC_MALLOC && WINNATIVE
	if (_heapchk() != _HEAPOK)
	{
		return;
	}
	#endif

	if (Msg)
	{
		#ifdef WIN32
		static GMutex Sem;
		Sem.Lock(_FL);
		#endif

		char Buffer[2049] = "";
		#ifdef LGI_TRACE_TO_FILE
		GFile f;
		static char LogPath[MAX_PATH] = "";
		
		if (!_LgiTraceStream && LogPath[0] == 0)
		{
			LgiTraceGetFilePath(LogPath, sizeof(LogPath));
		}
		#endif

		va_list Arg;
		va_start(Arg, Msg);
		_vsnprintf(Buffer, sizeof(Buffer)-1, Msg, Arg);
		va_end(Arg);

		#ifdef LGI_TRACE_TO_FILE
		GStreamI *Output = NULL;
		if (_LgiTraceStream)
			Output = _LgiTraceStream;
		else if (f.Open(LogPath, O_WRITE))
		{
			f.Seek(0, SEEK_END);
			Output = &f;
		}
		if (Output)
			Output->Write(Buffer, strlen(Buffer));
		if (!_LgiTraceStream)
			f.Close();
		#endif


		#if defined WIN32
		OutputDebugString(Buffer);
		Sem.Unlock();
		#else
		printf("%s", Buffer);
		#endif
	}
}

#ifndef LGI_STATIC
#define STACK_SIZE 12

void LgiStackTrace(const char *Msg, ...)
{
	GSymLookup::Addr Stack[STACK_SIZE];
	ZeroObj(Stack);
	GSymLookup *Lu = LgiApp->GetSymLookup();
	int Frames = Lu ? Lu->BackTrace(0, 0, Stack, STACK_SIZE) : 0;
	if (Msg)
	{
		char Buffer[2049] = "";
		#ifdef LGI_TRACE_TO_FILE
		GFile f;
		if (LgiGetExeFile(Buffer, sizeof(Buffer)))
		{
			char *Dot = strrchr(Buffer, '.');
			if (Dot && !strchr(Dot, DIR_CHAR))
			{
				strcpy(Dot+1, "txt");
			}
			else
			{
				strcat(Buffer, ".txt");
			}
			f.Open(Buffer, O_WRITE);
		}
		#endif

		va_list Arg;
		va_start(Arg, Msg);
		int Len = _vsnprintf(Buffer, sizeof(Buffer)-1, Msg, Arg);
		va_end(Arg);

		Lu->Lookup(Buffer+Len, sizeof(Buffer)-Len-1, Stack, Frames);

		#ifdef LGI_TRACE_TO_FILE
		if (f.IsOpen())
		{
			f.Seek(0, SEEK_END);
			f.Write(Buffer, strlen(Buffer));
			f.Close();
		}
		#endif

		#if defined WIN32
		OutputDebugString(Buffer);
		#else
		printf("Trace: %s", Buffer);
		#endif
	}
}
#endif

bool LgiTrimDir(char *Path)
{
	if (Path)
	{
		char *p = strrchr(Path, DIR_CHAR);
		if (p)
		{
			*p = 0;
			return true;
		}
	}
	return false;
}

GAutoString LgiMakeRelativePath(const char *Base, const char *Path)
{
	GStringPipe Status;

	if (Base && Path)
	{
		#ifdef WIN32
		bool SameNs = strnicmp(Base, Path, 3) == 0;
		#else
		bool SameNs = true;
		#endif
		if (SameNs)
		{
			GToken b(Base + 1, ":" DIR_STR);
			GToken p(Path + 1, ":" DIR_STR);
			int Same = 0;
			while (b[Same] && p[Same] && stricmp(b[Same], p[Same]) == 0)
			{
				Same++;
			}

			for (int i = Same; i<b.Length(); i++)
			{
				Status.Print("..%s", DIR_STR);
			}
			if (Same >= b.Length())
			{
				Status.Print(".%s", DIR_STR);
			}

			for (int n = Same; n<p.Length(); n++)
			{
				if (n > Same)
				{
					Status.Push(DIR_STR);
				}
				Status.Push(p[n]);
			}
		}
	}

	return GAutoString(Status.NewStr());
}

bool LgiIsRelativePath(const char *Path)
{
	if (!Path)
		return false;

	if (*Path == '.')
		return true;

	#ifdef WIN32
	if (IsAlpha(Path[0]) && Path[1] == ':') // Drive letter path
		return false;
	#endif

	if (*Path == DIR_CHAR)
		return false;

	if (strstr(Path, "://")) // Protocol def
		return false;

	return true; // Correct or not???
}

bool LgiMakePath(char *Str, int StrSize, const char *Path, const char *File)
{
	LgiAssert(Str != 0 &&
			  StrSize > 0 &&
			  Path != 0 &&
			  File != 0);

	if (StrSize <= 4)
	{
		printf("%s:%i - LgiMakeFile buf size=%i?\n", _FL, StrSize);
	}

	if (Str && Path && File)
	{
		if (Str != Path)
		{
			strcpy_s(Str, StrSize, Path);
		}

		char Dir[] = { '/', '\\', 0 };
		#define EndStr() Str[strlen(Str)-1]
		#define EndDir() if (!strchr(Dir, EndStr())) strcat(Str, DIR_STR);

		int Len = strlen(Str);
		char *End = Str + (Len ? Len - 1 : 0);
		if (strchr(Dir, *End) && End > Str)
		{
			*End = 0;
		}

		GToken T(File, Dir);
		for (int i=0; i<T.Length(); i++)
		{
			if (!stricmp(T[i], "."))
				;
			else if (!stricmp(T[i], ".."))
				LgiTrimDir(Str);
			else if (!stricmp(T[i], "~"))
				LgiGetSystemPath(LSP_HOME, Str, StrSize);
			else
			{
				int Len = strlen(Str);
				if (!strchr(Dir, Str[Len-1]))
				{
					if (Len >= StrSize - 1)
						return false;
					Str[Len++] = DIR_CHAR;
					Str[Len] = 0;
				}
				int SegLen = strlen(T[i]);
				if (Len + SegLen + 1 > StrSize)
				{
					return false;
				}				
				strcpy(Str + Len, T[i]);
			}
		}
	}
	
	return true;
}

bool LgiGetTempPath(char *Dst, int DstSize)
{
	return LgiGetSystemPath(LSP_TEMP, Dst, DstSize);
}

bool LgiGetSystemPath(LgiSystemPath Which, char *Dst, int DstSize)
{
	if (!Dst || DstSize <= 0)
		return false;

	GFile::Path p;
	GString s = p.GetSystem(Which);
	if (!s)
		return false;

	strcpy_s(Dst, DstSize, s);
	return true;
}

GString LgiGetSystemPath(LgiSystemPath Which)
{
	GFile::Path p;
	return p.GetSystem(Which);
}

bool GFile::Path::IsFile()
{
	return FileExists(GetFull());
}

bool GFile::Path::IsFolder()
{
	return DirExists(GetFull());
}

GString GFile::Path::GetSystem(LgiSystemPath Which)
{
	GString Path;

	#if defined(WIN32)
		#ifndef CSIDL_PROFILE
			#define CSIDL_PROFILE						0x0028
		#endif 
		#if !defined(CSIDL_MYDOCUMENTS)
			#define CSIDL_MYDOCUMENTS					0x0005
		#endif
		#if !defined(CSIDL_MYMUSIC)
			#define CSIDL_MYMUSIC						0x000d
		#endif
		#if !defined(CSIDL_MYVIDEO)
			#define CSIDL_MYVIDEO						0x000e
		#endif
		#if !defined(CSIDL_LOCAL_APPDATA)
			#define CSIDL_LOCAL_APPDATA					0x001c
		#endif
		#if !defined(CSIDL_COMMON_APPDATA)
			#define CSIDL_COMMON_APPDATA				0x0023
		#endif
		#if !defined(CSIDL_APPDATA)
			#define CSIDL_APPDATA						0x001a
		#endif
	#endif

	#ifdef LINUX
	// Ask our window manager add-on if it knows the path
	GLibrary *WmLib = LgiApp ? LgiApp->GetWindowManagerLib() : NULL;
	if (WmLib)
	{
		Proc_LgiWmGetPath WmGetPath = (Proc_LgiWmGetPath) WmLib->GetAddress("LgiWmGetPath");
		char Buf[MAX_PATH];
		if (WmGetPath && WmGetPath(Which, Buf, sizeof(Buf)))
		{
			Path = Buf;
			return Path;
		}
	}
	#endif
	
	switch (Which)
	{
		case LSP_USER_DOWNLOADS:
		{
			#if defined(WIN32) && defined(_MSC_VER)

			// OMG!!!! Really?
			
			#ifndef REFKNOWNFOLDERID
			typedef GUID KNOWNFOLDERID;
			#define REFKNOWNFOLDERID const KNOWNFOLDERID &
			GUID FOLDERID_Downloads = {0x374DE290,0x123F,0x4565,{0x91,0x64,0x39,0xC4,0x92,0x5E,0x46,0x7B}};
			#endif
			
			GLibrary Shell("Shell32.dll");
			typedef HRESULT (STDAPICALLTYPE *pSHGetKnownFolderPath)(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath);
			pSHGetKnownFolderPath SHGetKnownFolderPath = (pSHGetKnownFolderPath)Shell.GetAddress("SHGetKnownFolderPath");
			if (SHGetKnownFolderPath)
			{
				PWSTR ptr = NULL;
				HRESULT r = SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &ptr);
				if (SUCCEEDED(r))
				{
					GAutoString u8(LgiNewUtf16To8(ptr));
					if (u8)
						Path = u8;
					CoTaskMemFree(ptr);
				}
			}

			if (!Path.Get())
			{
				GRegKey k(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders");
				char *p = k.GetStr("{374DE290-123F-4565-9164-39C4925E467B}");
				if (DirExists(p))
					Path = p;
			}
			
			if (!Path.Get())
			{
				GString MyDoc = WinGetSpecialFolderPath(CSIDL_MYDOCUMENTS);
				if (MyDoc)
				{
					char Buf[MAX_PATH];
					LgiMakePath(Buf, sizeof(Buf), MyDoc, "Downloads");
					if (DirExists(Buf))
						Path = Buf;
				}
			}

			if (!Path.Get())
			{
				GString Profile = WinGetSpecialFolderPath(CSIDL_PROFILE);
				if (Profile)
				{
					char Buf[MAX_PATH];
					LgiMakePath(Buf, sizeof(Buf), Profile, "Downloads");
					if (DirExists(Buf))
						Path = Buf;
				}
			}

			#else

			LgiAssert(!"Not implemented");

			#endif
			break;
		}
		case LSP_USER_LINKS:
		{
			GString Home = LgiGetSystemPath(LSP_HOME);
			char p[MAX_PATH];
			
			#if defined(WIN32)

			if (LgiMakePath(p, sizeof(p), Home, "Links"))
				Path = p;

			#elif defined(LINUX)

			if (LgiMakePath(p, sizeof(p), Home, ".config/gtk-3.0"))
				Path = p;

			#else

			LgiAssert(!"Not implemented");

			#endif
			break;
		}
		case LSP_USER_DOCUMENTS:
		{
			#if defined(WIN32) && defined(_MSC_VER)

			Path = WinGetSpecialFolderPath(CSIDL_PERSONAL);

			#else

			LgiAssert(!"Not implemented");

			#endif
			
			break;
		}
		case LSP_USER_MUSIC:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_MYMUSIC);
			
			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kMusicDocumentsFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString a = FSRefPath(Ref);
				if (a)
					Path = a.Get();
			}				

			#else

			LgiAssert(!"Not implemented");

			#endif
			
			break;
		}
		case LSP_USER_VIDEO:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_MYVIDEO);

			#elif defined MAC && !defined COCOA

			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kMovieDocumentsFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString a = FSRefPath(Ref);
				if (a)
					Path = a.Get();
			}

			#else

			LgiAssert(!"Not implemented");

			#endif
			
			break;
		}
		case LSP_USER_APPS:
		{
			#if defined WIN32
			Path = WinGetSpecialFolderPath(
				#ifdef WIN64
				CSIDL_PROGRAM_FILES
				#else
				CSIDL_PROGRAM_FILESX86
				#endif
				);
			#elif defined MAC
			Path = "/Applications";
			#elif defined LINUX
			Path = "/usr/bin";
			#elif defined BEOS
			LgiAssert(!"Impl me.");
			#endif
			break;
		}
		case LSP_APP_INSTALL:
		{
			Path = LgiGetSystemPath(LSP_EXE);
			if (Path)
			{
				int Last = Path.RFind(DIR_STR);
				if (Last > 0)
				{
					GString s = Path(Last, -1);
					if
					(
						stristr(s,
								#ifdef _DEBUG
								"Debug"
								#else
								"Release"
								#endif
								)
					)
						Path = Path(0, Last);
				}					
			}
			break;
		}
		case LSP_APP_ROOT:
		{
			#ifndef LGI_STATIC
			if (!LgiApp)
			{
				LgiAssert(0);
				break;
			}
			char *Name = LgiApp->Name();
			if (!Name)
			{
				LgiAssert(0);
				break;
			}

			#if defined MAC && !defined COCOA
				FSRef Ref;
				OSErr e = FSFindFolder(kUserDomain, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
				if (e)
				{
					printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
					LgiAssert(0);
				}
				else
				{
					GAutoString Base = FSRefPath(Ref);
					Path = Base.Get();
				}
			#elif defined WIN32
				Path = WinGetSpecialFolderPath(CSIDL_APPDATA);
			#elif defined LINUX
				char Dot[128];
				snprintf(Dot, sizeof(Dot), ".%s", Name);
				Name = Dot;
				struct passwd *pw = getpwuid(getuid());
				if (pw)
					Path = pw->pw_dir;
				else
					LgiAssert(0);
			#else
				LgiAssert(0);
			#endif

			if (Path)
			{
				Path += DIR_STR;
				Path += Name;
			}
			#endif
			break;
		}
		case LSP_OS:
		{
			#if defined WIN32

			char p[MAX_PATH];
			if (GetWindowsDirectory(p, sizeof(p)) > 0)
				Path = p;

			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kOnAppropriateDisk,  kSystemFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}

			#else

			Path = "/boot"; // it'll do for now...

			#endif
			break;
		}
		case LSP_OS_LIB:
		{
			#if defined WIN32

			char p[MAX_PATH];
			if (GetSystemDirectory(p, sizeof(p)) > 0)
				Path = p;
			
			#elif defined MAC
			
			Path = "/Library";

			#else

			Path = "/lib"; // it'll do for now...

			#endif
			break;
		}
		case LSP_TEMP:
		{
			#if defined WIN32

			char16 t[MAX_PATH];
			if (GetTempPathW(CountOf(t), t) > 0)
			{
				GAutoString utf(LgiNewUtf16To8(t));
				if (utf)
					Path = utf;
			}

			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kTemporaryFolderType, kCreateFolder, &Ref);
			if (e) LgiTrace("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
				{
					Path = u.Get();
				}
				else LgiTrace("%s:%i - FSRefPath failed.\n", _FL);
			}

			#else

			Path = "/tmp"; // it'll do for now...

			#endif
			break;
		}
		case LSP_COMMON_APP_DATA:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_COMMON_APPDATA);
			
			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kOnSystemDisk, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}

			#elif defined LINUX

			Path = "/usr";

			#elif defined BEOS

			Path = "/boot/apps";

			#endif
			break;
		}
		case LSP_USER_APP_DATA:
		{
			#if defined WIN32
			
			Path = WinGetSpecialFolderPath(CSIDL_APPDATA);

			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}

			#elif defined LINUX

			Path = "/usr";

			#endif
			break;
		}
		case LSP_LOCAL_APP_DATA:
		{
			#if defined WIN32
			
			Path = WinGetSpecialFolderPath(CSIDL_LOCAL_APPDATA);
			
			#else
			LgiAssert(!"Impl me.");
			#endif
			break;
		}
		case LSP_DESKTOP:
		{
			#if defined(WINDOWS) && defined(_MSC_VER)

			Path = WinGetSpecialFolderPath(CSIDL_DESKTOPDIRECTORY);
			
			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kOnAppropriateDisk, kDesktopFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}

			#elif defined POSIX

			struct passwd *pw = getpwuid(getuid());
			if (pw)
			{
				#ifdef LINUX
				WindowManager wm = LgiGetWindowManager();
				if (wm == WM_Gnome)
					Path.Printf("%s/.gnome-desktop", pw->pw_dir);
				else
				#endif
					Path.Printf("%s/Desktop", pw->pw_dir);
			}

			#elif defined BEOS

			strcpy_s(Dst, DstSize, "/boot/home/Desktop");
			Status = true;
			
			#endif
			break;
		}
		case LSP_HOME:
		{
			#if defined WIN32

			Path = WinGetSpecialFolderPath(CSIDL_PROFILE);

			#elif defined POSIX

			struct passwd *pw = getpwuid(getuid());
			if (pw)
				Path = pw->pw_dir;

			#elif defined BEOS

			strcpy_s(Dst, DstSize, "/boot/home");
			Status = true;

			#endif
			break;
		}
		case LSP_EXE:
		{
			char Buf[MAX_PATH];
			if (LgiGetExeFile(Buf, sizeof(Buf)))
			{
				LgiTrimDir(Buf);
				Path = Buf;
			}
			break;
		}
		case LSP_TRASH:
		{
			#if defined LINUX
			
			switch (LgiGetWindowManager())
			{
				case WM_Kde:
				{
					static char KdeTrash[256] = "";
					
					if (!ValidStr(KdeTrash))
					{
						// Ask KDE where the current trash is...
						GProcess p;
						GStringPipe o;
						if (p.Run("kde-config", "--userpath trash", 0, true, 0, &o))
						{
							char *s = o.NewStr();
							if (s)
							{
								// Store it..
								strcpy_s(KdeTrash, sizeof(KdeTrash), s);
								DeleteArray(s);

								// Clear out any crap at the end...
								char *e = KdeTrash + strlen(KdeTrash) - 1;
								while (e > KdeTrash && strchr(" \r\n\t/", *e))
								{
									*e-- = 0;
								}
							}
							else
							{
								printf("%s:%i - No output from 'kde-config'.\n", __FILE__, __LINE__);
							}
						}
						else
						{
							printf("%s:%i - Run 'kde-config' failed.\n", __FILE__, __LINE__);
						}
					}

					if (ValidStr(KdeTrash))
						Path = KdeTrash;
					break;
				}
				default:
				{
					printf("%s:%i - Unknown window manager.\n", __FILE__, __LINE__);
					break;
				}
			}
			
			#elif defined MAC && !defined COCOA
			
			FSRef Ref;
			OSErr e = FSFindFolder(kUserDomain, kTrashFolderType, kDontCreateFolder, &Ref);
			if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
			else
			{
				GAutoString u = FSRefPath(Ref);
				if (u)
					Path = u.Get();
			}

			#elif defined WIN32

			// This should work but doesn't... *shrug*
			// char *f = GetWin32Folder(CSIDL_BITBUCKET);

			#else

			#endif
			break;
		}
	}

	return Path;
}

bool LgiGetExeFile(char *Dst, int DstSize)
{
	if (Dst)
	{
		#if defined WIN32

		if (LgiGetOs() == LGI_OS_WIN9X)
		{
			char Exe[256];
			if (GetModuleFileName(NULL, Exe, sizeof(Exe)) > 0)
			{
				char *e = LgiFromNativeCp(Exe);
				if (e)
				{
					strlwr(e);
					strcpy_s(Dst, DstSize, e);
					DeleteArray(e);
					return true;
				}
				else
				{
					LgiMsg(0, "LgiFromNativeCp returned 0, ANSI CodePage=%i (%s)", "LgiGetExeFile Error", MB_OK, GetACP(), LgiAnsiToLgiCp());
				}
			}

			char m[256];
			sprintf_s(m, sizeof(m), "GetModuleFileName failed err: %08.8X", GetLastError());
			MessageBox(0, m, "LgiGetExeFile Error", MB_OK);
			LgiExitApp();
		}
		else
		{
			char16 Exe[256];
			if (GetModuleFileNameW(NULL, Exe, sizeof(Exe)) > 0)
			{
				char *e = LgiNewUtf16To8(Exe);
				if (e)
				{
					strcpy_s(Dst, DstSize, e);
					DeleteArray(e);
					return true;
				}
			}
		}

		#elif defined BEOS

		app_info Info;
		if (LgiApp->GetAppInfo(&Info) == B_OK)
		{
			BEntry e(&Info.ref);
			BPath p;
			if (e.GetPath(&p) == B_OK)
			{
				strcpy_s(Dst, DstSize, p.Path());
				return true;
			}
		}

		#elif defined ATHEOS

		os::Directory AppPath;
		if (AppPath.SetTo("^/.") == 0)
		{
			std::string p;
			if (AppPath.GetPath(&p) == 0)
			{
				sprintf_s(Dst, DstSize, "%s%s%s", p.c_str(), DIR_STR, LgiApp->_AppFile);
				return true;
			}
		}

		#elif defined LINUX

		static char ExePathCache[256] = "";
		bool Status = false;

		// this is _REALLY_ lame way to do it... but hey there aren't any
		// other better ways to get the full path of the running executable
		if (!ExePathCache[0])
		{
			// First try the self method
			int Len = readlink("/proc/self/exe", ExePathCache, sizeof(ExePathCache));
			Status = FileExists(ExePathCache);
			// printf("readlink=%i Status=%i Exe='%s'\n", Len, Status, ExePathCache);
			if (!Status)
			{
				ExePathCache[0] = 0;
				
				// Next try the map file method
				char ProcFile[256];
				sprintf_s(ProcFile, sizeof(ProcFile), "/proc/%i/maps", getpid());
				int fd = open(ProcFile, O_RDONLY);

				if (fd >= 0)
				{
					int Len = 16 << 10; // is this enough?
										// no better way of determining the length of proc info?
					char *Buf = new char[Len+1];
					if (Buf)
					{
						int r = read(fd, Buf, Len);
						Buf[r] = 0;

						char *s = strchr(Buf, '/');
						if (s)
						{
							char *e = strchr(s, '\n');
							if (e)
							{
								*e = 0;
								strcpy_s(ExePathCache, sizeof(ExePathCache), s);
								Status = true;
							}
						}
						
						DeleteArray(Buf);
					}

					close(fd);
				}
				else
				{
					// Non proc system (like cygwin for example)
					// char Cmd[256];
					// sprintf_s(Cmd, sizeof(Cmd), "ps | grep \"%i\"", getpid());
					
					GStringPipe Ps;
					GProcess p;
					if (p.Run("ps", 0, 0, true, 0, &Ps))
					{
						char *PsOutput = Ps.NewStr();
						if (PsOutput)
						{
							GToken n(PsOutput, "\r\n");
							for (int i=0; !Status && i<n.Length(); i++)
							{
								GToken t(n[i], " \t\r\n");
								if (t.Length() > 7)
								{
									int LinePid = 0;
									for (int i=0; i<t.Length()-1; i++)
									{
										if (IsDigit(t[i][0]))
										{
											LinePid = atoi(t[i]);
											break;
										}
									}

									if (LinePid == getpid())
									{
										strcpy_s(ExePathCache, sizeof(ExePathCache), t[t.Length()-1]);
										Status = true;
									}
								}
							}
							
							DeleteArray(PsOutput);
						}					
					}
				}
			}
		}
		else Status = true;

		if (Status)
		{
			strcpy_s(Dst, DstSize, ExePathCache);
		}
		
		return Status;
		
		#elif defined MAC && !defined COCOA
		
		bool Status = false;
		ProcessSerialNumber ps;
		OSErr e = GetCurrentProcess(&ps);
		if (!e)
		{
			FSRef fs;
			OSStatus s = GetProcessBundleLocation(&ps, &fs);
			if (!s)
			{
				GAutoString u = FSRefPath(fs);
				if (!e)
				{
					strcpy_s(Dst, DstSize, u);
					Status = true;
				}
				else
				{
					printf("%s:%i - FSGetCatalogInfo failed (e=%i).\n", __FILE__, __LINE__, e);
				}
			}
			else
			{
				printf("%s:%i - GetProcessBundleLocation failed (e=%i).\n", __FILE__, __LINE__, (int)s);
			}
		}
		else
		{
			printf("%s:%i - GetCurrentProcess failed (e=%i).\n", __FILE__, __LINE__, e);
		}
		
		return Status;
		
		#endif
	}

	return false;
}

bool LgiGetExePath(char *Dst, int DstSize)
{
	return LgiGetSystemPath(LSP_EXE, Dst, DstSize);
}

char *LgiGetExtension(char *File)
{
	if (File)
	{
		char *Dot = strrchr((char*)File, '.');
		if (Dot && !strchr(Dot, DIR_CHAR))
		{
			return Dot + 1;
		}
	}

	return 0;
}

#define DEBUG_FIND_FILE		0

char *LgiFindFile(const char *Name)
{
	char *Result = 0;
	
	#if DEBUG_FIND_FILE
	printf("%s:%i - Name='%s'\n", __FILE__, __LINE__, Name);
	#endif
	
	if (Name)
	{
		char Exe[256];
		
		#ifndef MAC
		LgiGetExePath(Exe, sizeof(Exe));
		#else
		LgiGetExeFile(Exe, sizeof(Exe));
		/*
		GToken p(Exe, "/");
		Exe[0] = 0;
		for (int i=0; i<p.Length(); i++)
		{
			if (strcmp(p[i], "build") == 0)
				break;
			
			int len = strlen(Exe);
			sprintf_s(Exe+len, sizeof(Exe)-len, "/%s", p[i]);

			#if DEBUG_FIND_FILE
			printf("%s:%i - Exe='%s'\n", __FILE__, __LINE__, Exe);
			#endif
		}
		*/
		#endif

		#if DEBUG_FIND_FILE
		printf("%s:%i - Exe='%s'\n", __FILE__, __LINE__, Exe);
		#endif

		char CurWorking[MAX_PATH];
		getcwd(CurWorking, sizeof(CurWorking));
		const char *PrefPath[] =
		{
			".",
			"..",
			"Code",
			#ifdef MAC
			"./Contents/Resources",
			#else
			"Resources",
			#endif
			"../Code",
			"../Resources",
			#ifdef WIN32
			"../Debug",
			"../Release",
			"../../Debug",
			"../../Release",
			#endif
			CurWorking,
			0
		};

		// Look in prefered paths first...
		for (const char **Pref = PrefPath; *Pref; Pref++)
		{
			char Path[MAX_PATH];
			if (LgiIsRelativePath(*Pref))
			{
				LgiMakePath(Path, sizeof(Path), Exe, *Pref);
				LgiMakePath(Path, sizeof(Path), Path, Name);
			}
			else
			{
				LgiMakePath(Path, sizeof(Path), *Pref, Name);
			}
			int PathLen = strlen(Path);
			LgiAssert(PathLen < sizeof(Path));

			if (FileExists(Path))
				return NewStr(Path);

			#ifdef WIN32
			if (PathLen < sizeof(Path) - 4)
			{
				strcat(Path, ".lnk");
				if (ResolveShortcut(Path, Path, sizeof(Path)))
					return NewStr(Path);
			}
			#endif
		}

		// General search fall back...
		GArray<const char*> Ext;
		GArray<char*> Files;
		Ext.Add((char*)Name);

		#if DEBUG_FIND_FILE
		printf("%s:%i - Exe='%s'\n", __FILE__, __LINE__, Exe);
		#endif

		if (LgiRecursiveFileSearch(Exe, &Ext, &Files) &&
			Files.Length())
		{
			Result = Files[0];
			Files.DeleteAt(0);
		}
		Files.DeleteArrays();
	}

	return Result;
}

#if defined WIN32
static LARGE_INTEGER Freq = {0};
static bool CurTimeInit = false;
#endif

uint64 LgiCurrentTime()
{
	#if defined WIN32

	if (!CurTimeInit)
	{
		CurTimeInit = true;
		if (!QueryPerformanceFrequency(&Freq))
			Freq.QuadPart = 0;
	}

	if (Freq.QuadPart)
	{
		// Return performance counter in ms
		LARGE_INTEGER i;
		if (QueryPerformanceCounter(&i))
		{
			return i.QuadPart * 1000 / Freq.QuadPart;
		}

		// Now what?? Give up and go back to tick count I guess.
		Freq.QuadPart = 0;
	}

	// Fall back for systems without a performance counter.
	return GetTickCount();

	#elif defined BEOS

	return system_time() / 1000;

	#elif defined MAC && !defined COCOA
	
	UnsignedWide t;
	Microseconds(&t);
	uint64 i = ((uint64)t.hi << 32) | t.lo;
	return i / 1000;

	#else

	timeval tv;
	gettimeofday(&tv, 0);
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

	#endif
}

uint64 LgiMicroTime()
{
	#if defined WIN32

	if (!CurTimeInit)
	{
		CurTimeInit = true;
		if (!QueryPerformanceFrequency(&Freq))
			Freq.QuadPart = 0;
	}
	if (Freq.QuadPart)
	{
		// Return performance counter in ms
		LARGE_INTEGER i;
		if (QueryPerformanceCounter(&i))
		{
			return i.QuadPart * 1000000 / Freq.QuadPart;
		}
	}
	return 0;

	#elif defined BEOS

	LgiAssert(!"Not impl.");
	return 0;

	#elif defined MAC && !defined COCOA
	
	UnsignedWide t;
	Microseconds(&t);
	return ((uint64)t.hi << 32) | t.lo;

	#else

	timeval tv;
	gettimeofday(&tv, 0);
	return (tv.tv_sec * 1000000) + tv.tv_usec;

	#endif
}

int LgiIsReleaseBuild()
{
	#ifdef _DEBUG
	return 0;
	#else
	return 1;
 	#endif
}

bool LgiIsVolumeRoot(const char *Path)
{
	if (!Path)
		return false;
	
	#ifdef WIN32
		if
		(
			IsAlpha(Path[0])
			&&
			Path[1] == ':'
			&&
			(
				(Path[2] == 0)
				||
				(Path[2] == '\\' && Path[3] == 0)
			)
		)
		{
			return true;
		}
	#else
		GToken t(Path, DIR_STR);
		if (t.Length() == 0)
			return true;
		#ifdef MAC
			if (!stricmp(t[0], "Volumes") &&
				t.Length() == 2)
				return true;
		#else
			if (!stricmp(t[0], "mnt") &&
				t.Length() == 2)
				return true;
		#endif
	#endif
	
	return false;
}

void LgiFormatSize(char *Str, int SLen, uint64 Size)
{
	int64 K = 1024;
	int64 M = K * K;
	int64 G = K * M;
	int64 T = K * G;

	if (Size == 1)
	{
		strcpy_s(Str, SLen, "1 byte");
	}
	else if (Size < K)
	{
		sprintf_s(Str, SLen, "%u bytes", (int)Size);
	}
	else if (Size < 10 * K)
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f KiB", d / K);
	}
	else if (Size < M)
	{
		sprintf_s(Str, SLen, "%u KiB", (int) (Size / K));
	}
	else if (Size < G)
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f MiB", d / M);
	}
	else if (Size < T)
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f GiB", d / G);
	}
	else
	{
		double d = (double)(int64)Size;
		sprintf_s(Str, SLen, "%.2f TiB", d / T);
	}
}

char *LgiTokStr(const char *&s)
{
	char *Status = 0;

	if (s && *s)
	{
		// Skip whitespace
		static char Delim[] = ", \t\r\n";
		while (*s && strchr(Delim, *s)) s++;
		if (*s)
		{
			if (strchr("\'\"", *s))
			{
				char Delim = *s++;
				const char *e = strchr(s, Delim);
				if (!e)
				{
					// error, no end delimiter
					e = s;
					while (*e) e++;
				}
				Status = NewStr(s, e - s);
				s = *e ? e + 1 : e;
			}
			else
			{
				const char *e = s;
				while (*e && !strchr(Delim, *e)) e++;
				Status = NewStr(s, e - s);
				s = e;
			}
		}		
	}
	
	return Status;
}

//////////////////////////////////////////////////////////////////////////////
DoEvery::DoEvery(int p) // p = timeout in ms
{
	Init(p);
}

void DoEvery::Init(int p) // p = timeout in ms
{
	LastTime = LgiCurrentTime();
	if (p > 0)
	{
		Period = p;
	}
}

bool DoEvery::DoNow()
{
	int64 Now = LgiCurrentTime();
	if (LastTime + Period < Now)
	{
		LastTime = Now;
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////
#if 0
GViewFill::GViewFill(GColour c)
{
	Type = Solid;
	Col = c;
	#ifdef WIN32
	hBrush = NULL;
	#endif
}

GViewFill::GViewFill(COLOUR c, int Bits)
{
	Type = Solid;
	Col.c32(CBit(32, c, Bits));
	#ifdef WIN32
	hBrush = NULL;
	#endif
}

GViewFill::GViewFill(GSurface *dc, bool Copy)
{
    Col.c32(0);
	Type = Copy ? OwnBitmap : RefBitmap;
	#ifndef LGI_STATIC
	if (Copy)
		pDC = new GMemDC(dc);
	else
		pDC = dc;
	#endif

	#ifdef WIN32
	hBrush = NULL;
	#endif
}

GViewFill::GViewFill(const GViewFill &f)
{
	Col = f.GetFlat();
	Type = f.Type;
	if (Type == OwnBitmap)
	{
		#ifndef LGI_STATIC
    	pDC = new GMemDC(f.pDC);
		#endif
	}
	else if (Type == RefBitmap)
	{
    	pDC = f.pDC;
	}

	#ifdef WIN32
	hBrush = NULL;
	#endif
}

GViewFill::~GViewFill()
{
    Empty();
}

void GViewFill::Empty()
{
	if (Type == OwnBitmap)
    	DeleteObj(pDC);

    Type = None;
    pDC = 0;
    Col.c32(0);

	#ifdef WIN32
	if (hBrush)
	{
		DeleteObject(hBrush);
		hBrush = NULL;
	}
	#endif
}

void GViewFill::Fill(GSurface *pDC, GRect *r, GdcPt2 *Origin)
{
	#ifndef LGI_STATIC
	if (Type == Solid)
	{
		pDC->Colour(Col);
		pDC->Rectangle(r);
	}
	else if (Type == OwnBitmap || Type == RefBitmap)
	{
		if (pDC)
		{
			GRect a;
			if (!r)
			{
				a.ZOff(pDC->X()-1, pDC->Y()-1);
				r = &a;
			}

			for (int y = Origin ? (Origin->y % pDC->Y()) - pDC->Y() : 0; y < r->Y(); y += pDC->Y())
			{
				for (int x = Origin ? (Origin->x % pDC->X()) - pDC->X() : 0; x<r->X(); x += pDC->X())
				{
					pDC->Blt(r->x1 + x, r->y1 + y, pDC);
				}
			}
		}
		else
		{
			LgiAssert(0);
		}
	}
	#endif
}

#ifdef WIN32
/*
HBRUSH GViewFill::GetBrush()
{
	if (!hBrush)
	{
		LOGBRUSH LogBrush;
		LogBrush.lbStyle = BS_SOLID;
		LogBrush.lbColor = GetFlat().c24();
		LogBrush.lbHatch = 0;
		hBrush = CreateBrushIndirect(&LogBrush);
	}
	return hBrush;
}
*/
#endif
#endif

//////////////////////////////////////////////////////////////////////
bool GCapabilityClient::NeedsCapability(const char *Name, const char *Param)
{
    for (int i=0; i<Targets.Length(); i++)
        Targets[i]->NeedsCapability(Name, Param);
    return Targets.Length() > 0;
}

GCapabilityClient::~GCapabilityClient()
{
    for (int i=0; i<Targets.Length(); i++)
        Targets[i]->Clients.Delete(this);
}

void GCapabilityClient::Register(GCapabilityTarget *t)
{
    if (t)
    {
        Targets.Add(t);
        t->Clients.Add(this);
    }
}

GCapabilityTarget::~GCapabilityTarget()
{
    for (int i=0; i<Clients.Length(); i++)
        Clients[i]->Targets.Delete(this);
}

/////////////////////////////////////////////////////////////////////
GProfile::GProfile(const char *Name)
{
	MinMs = -1;
	Add(Name);
}

GProfile::~GProfile()
{
	Add("End");
	
	if (MinMs > 0)
	{
		uint64 TotalMs = s.Last().Time - s[0].Time;
		if (TotalMs < MinMs)
		{
			return;
		}
	}
	
	for (int i=0; i<s.Length()-1; i++)
	{
		Sample &a = s[i];
		Sample &b = s[i+1];
		LgiTrace("%s%s = %i ms\n", i ? "    " : "", a.Name, (int)(b.Time - a.Time));
	}
}

void GProfile::HideResultsIfBelow(int Ms)
{
	MinMs = Ms;
}

void GProfile::Add(const char *Name)
{
	s.Add(Sample(LgiCurrentTime(), Name));
}
