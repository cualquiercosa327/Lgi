//
//	Cross platform LGI functions
//

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef _WINDOWS
#include <winsock2.h>
#include <shlobj.h>
#endif

#include "Lgi.h"
#include "GToken.h"
#include "GCapabilities.h"

#ifdef LINUX
#include "LgiWinManGlue.h"
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
#ifdef MAC
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
			GAutoString u(LgiNewUtf16To8((char16*)Name.unicode, Name.length * sizeof(char16)));
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
	#if WIN32NATIVE

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
		bool Status = false;
		
		if (Wnd->LockLooper())
		{
			BMessage *Msg = new BMessage(Event);
			if (Msg)
			{
				Msg->AddInt32("a", a);
				Msg->AddInt32("b", b);
				BMessenger m(Wnd);
				Status = m.SendMessage(Msg) == B_OK;
				DeleteObj(Msg);
			}
			Wnd->UnlockLooper();
		}
		
		return Status;
	}

	#elif defined(MAC)
	
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
	
	#else
	
	#error "Not Impl."
	
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
		typedef HRESULT (STDAPICALLTYPE *p_DllRegisterServer)(void);

		p_DllRegisterServer DllRegisterServer = (p_DllRegisterServer)Lib.GetAddress("DllRegisterServer");
		if (DllRegisterServer)
		{
			return DllRegisterServer() == S_OK;
		}
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
	#if defined WIN32

	static int Os = LGI_OS_UNKNOWN;
	static int Version = 0, Revision = 0;

	if (Os == LGI_OS_UNKNOWN)
	{
		OSVERSIONINFO v;
		v.dwOSVersionInfoSize=sizeof(v);
		GetVersionEx(&v);

		Version = v.dwMajorVersion;
		Revision = v.dwMinorVersion;

		Os = (v.dwPlatformId == VER_PLATFORM_WIN32_NT) ? LGI_OS_WINNT : LGI_OS_WIN9X;
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

	#elif defined MAC

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
			sprintf(s, "%x", (int)i);
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
	const char *Str[] =
	{
		"Unknown",
		"Win32",
		"Win32",
		"BeOS",
		"Linux",
		"AtheOS",
		"MacOS9",
		"MacOSX",
		0,
		0,
		0,
		0
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
					if (RawFile AND
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
					*Count++;
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

void LgiTrace(const char *Msg, ...)
{
	#if defined _INC_MALLOC && WIN32NATIVE
	if (_heapchk() != _HEAPOK)
	{
		return;
	}
	#endif

	if (Msg)
	{
		#ifdef WIN32
		static GSemaphore Sem;
		Sem.Lock(_FL);
		#endif

		char Buffer[2049] = "";
		#ifdef LGI_TRACE_TO_FILE
		GFile f;
		if (LgiGetExeFile(Buffer, sizeof(Buffer)))
		{
			#ifdef MAC
			char *Dir = strrchr(Buffer, DIR_CHAR);
			if (Dir)
			{
				char Part[256];
				strsafecpy(Part, Dir+1, sizeof(Part));
				Buffer[0] = 0;
				LgiMakePath(Buffer, sizeof(Buffer), Buffer, "~/Library/Logs");
				LgiMakePath(Buffer, sizeof(Buffer), Buffer, Dir+1);
				strsafecat(Buffer, ".txt", sizeof(Buffer));
			}
			else
			#endif
			{
				char *Dot = strrchr(Buffer, '.');
				if (Dot AND !strchr(Dot, DIR_CHAR))
				{
					strcpy(Dot+1, "txt");
				}
				else
				{
					strcat(Buffer, ".txt");
				}
			}
			f.Open(Buffer, O_WRITE);
		}
		#endif

		va_list Arg;
		va_start(Arg, Msg);
		int MsgLen = _vsnprintf(Buffer, sizeof(Buffer)-1, Msg, Arg);
		va_end(Arg);

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
		Sem.Unlock();
		#else
		printf("%s", Buffer);
		#endif
	}
}

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
		// strcpy(Buffer, "C:\\scribe.exe");
		if (LgiGetExeFile(Buffer, sizeof(Buffer)))
		{
			char *Dot = strrchr(Buffer, '.');
			if (Dot AND !strchr(Dot, DIR_CHAR))
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

GAutoString LgiMakeRelativePath(char *Base, char *Path)
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
	if (IsAlpha(Path[0]) AND Path[1] == ':') // Drive letter path
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

	if (Str AND Path AND File)
	{
		if (Str != Path)
		{
			strsafecpy(Str, Path, StrSize);
		}

		char Dir[] = { '/', '\\', 0 };
		#define EndStr() Str[strlen(Str)-1]
		#define EndDir() if (!strchr(Dir, EndStr())) strcat(Str, DIR_STR);

		int Len = strlen(Str);
		char *End = Str + (Len ? Len - 1 : 0);
		if (strchr(Dir, *End) AND End > Str)
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
				EndDir();
				strsafecat(Str, T[i], StrSize);
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
	bool Status = false;

	#if defined(WIN32)
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

	if (Dst)
	{
		#ifdef LINUX
		// Ask our window manager add-on if it knows the path
		GLibrary *WmLib = LgiApp->GetWindowManagerLib();
		if (WmLib)
		{
			Proc_LgiWmGetPath WmGetPath = (Proc_LgiWmGetPath) WmLib->GetAddress("LgiWmGetPath");
			if (WmGetPath AND WmGetPath(Which, Dst, DstSize))
			{
				return true;
			}
		}
		#endif
		
		switch (Which)
		{
			case LSP_USER_DOWNLOADS:
			{
				#if defined WIN32

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
						{
							strsafecpy(Dst, u8, DstSize);
							Status = true;
						}
						CoTaskMemFree(ptr);
					}
				}

				if (!Status)
				{
					GRegKey k("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders");
					char *p = k.GetStr("{374DE290-123F-4565-9164-39C4925E467B}");
					if (DirExists(p))
					{
						strsafecpy(Dst, p, DstSize);
						Status = true;
					}
				}
				
				if (!Status)
				{
					GAutoString MyDoc(GetWindowsFolder(CSIDL_MYDOCUMENTS));
					if (MyDoc)
					{
						LgiMakePath(Dst, DstSize, MyDoc, "Downloads");
						Status = DirExists(Dst);
					}
				}

				if (!Status)
				{
					GAutoString MyDoc(GetWindowsFolder(CSIDL_PROFILE));
					if (MyDoc)
					{
						LgiMakePath(Dst, DstSize, MyDoc, "Downloads");
						Status = DirExists(Dst);
					}
				}

				#else

				LgiAssert(!"Not implemented");

				#endif
				break;
			}
			case LSP_USER_DOCUMENTS:
			{
				#if defined WIN32

				char *f = GetWindowsFolder(CSIDL_MYDOCUMENTS);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}

				#else

				LgiAssert(!"Not implemented");

				#endif
				
				break;
			}
			case LSP_USER_MUSIC:
			{
				#if defined WIN32

				char *f = GetWindowsFolder(CSIDL_MYMUSIC);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}
				
				#elif defined MAC
				
				FSRef Ref;
				OSErr e = FSFindFolder(kUserDomain, kMusicDocumentsFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
				else
				{
					GAutoString a = FSRefPath(Ref);
					if (a)
					{
						strsafecpy(Dst, a, DstSize);
						return true;
					}
				}				

				#else

				LgiAssert(!"Not implemented");

				#endif
				
				break;
			}
			case LSP_USER_VIDEO:
			{
				#if defined WIN32

				char *f = GetWindowsFolder(CSIDL_MYVIDEO);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}

				#elif defined MAC

				FSRef Ref;
				OSErr e = FSFindFolder(kUserDomain, kMovieDocumentsFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
				else
				{
					GAutoString a = FSRefPath(Ref);
					if (a)
					{
						strsafecpy(Dst, a, DstSize);
						return true;
					}
				}

				#else

				LgiAssert(!"Not implemented");

				#endif
				
				break;
			}
			case LSP_APP_INSTALL:
			{
				if (LgiGetExePath(Dst, DstSize))
				{
					#if defined WIN32

					char *Last = strrchr(Dst, DIR_CHAR);
					if (Last)
					{
						if (stristr(Last,
							#ifdef _DEBUG
							"Debug"
							#else
							"Release"
							#endif
							))
							*Last = 0;
					}
					
					#elif defined MAC

					char *Last = strrchr(Dst, DIR_CHAR);
					if (Last)
					{
						if (!stricmp(Last,
							#ifdef _DEBUG
							"/Debug"
							#else
							"/Release"
							#endif
							))
							*Last = 0;
					}
					if (Last = strrchr(Dst, DIR_CHAR))
					{
						if (!stricmp(Last, "/build"))
							*Last = 0;
					}

					#endif

					return true;
				}
				break;
			}
			case LSP_APP_ROOT:
			{
				if (LgiApp)
				{
					char *Name = LgiApp->GetName();
					if (Name)
					{
						GAutoString Base;
						
						#if defined MAC
						FSRef Ref;
						OSErr e = FSFindFolder(kUserDomain, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
						if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
						else
						{
							Base = FSRefPath(Ref);
						}
						#elif defined WIN32
						Base.Reset(GetWindowsFolder(CSIDL_APPDATA));
						#elif defined LINUX
						char Dot[128];
						snprintf(Dot, sizeof(Dot), ".%s", Name);
						Name = Dot;
						struct passwd *pw = getpwuid(getuid());
						if (pw)
						{
							Base.Reset(NewStr(pw->pw_dir));
						}
						#else
							#error "Not implemented."
						#endif

						if (Base)
						{
							LgiMakePath(Dst, DstSize, Base, Name);
							// Status = DirExists(Dst) || FileDev->CreateDirectory(Dst);
							Status = true;
						}
					}
				}
				break;
			}
			case LSP_OS:
			{
				#if defined WIN32

				char p[MAX_PATH];
				if (GetWindowsDirectory(p, sizeof(p)) > 0)
				{
					strcpy(Dst, p);
					Status = true;
				}

				#elif defined MAC
				
				FSRef Ref;
				OSErr e = FSFindFolder(kOnAppropriateDisk,  kSystemFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
				else
				{
					GAutoString u = FSRefPath(Ref);
					if (u)
					{
						strsafecpy(Dst, u, DstSize);
						Status = true;
					}
				}

				#else

				strcpy(Dst, "/boot"); // it'll do for now...
				Status = true;

				#endif
				break;
			}
			case LSP_OS_LIB:
			{
				#if defined WIN32

				char p[MAX_PATH];
				if (GetSystemDirectory(p, sizeof(p)) > 0)
				{
					strcpy(Dst, p);
					Status = true;
				}

				
				#elif defined MAC
				
				strcpy(Dst, "/Library");
				Status = true;

				#else

				strcpy(Dst, "/lib"); // it'll do for now...
				Status = true;

				#endif
				break;
			}
			case LSP_TEMP:
			{
				#if defined WIN32

				if (LgiGetOs() == LGI_OS_WIN9X)
				{
					char t[256];
					if (GetTempPath(sizeof(t), t) > 0)
					{
						char *utf = LgiToNativeCp(t);
						if (utf)
						{
							strcpy(Dst, utf);
							DeleteArray(utf);
							Status = true;
						}
					}
				}
				else
				{
					char16 t[256];
					if (GetTempPathW(CountOf(t), t) > 0)
					{
						char *utf = LgiNewUtf16To8(t);
						if (utf)
						{
							strcpy(Dst, utf);
							DeleteArray(utf);
							Status = true;
						}
					}
				}

				#elif defined MAC
				
				FSRef Ref;
				OSErr e = FSFindFolder(kUserDomain, kTemporaryFolderType, kCreateFolder, &Ref);
				if (e) LgiTrace("%s:%i - FSFindFolder failed e=%i\n", _FL, e);
				else
				{
					GAutoString u = FSRefPath(Ref);
					if (u)
					{
						strsafecpy(Dst, u, DstSize);
						Status = true;
					}
					else LgiTrace("%s:%i - FSRefPath failed.\n", _FL);
				}

				#else

				strcpy(Dst, "/tmp"); // it'll do for now...
				Status = true;

				#endif
				break;
			}
			case LSP_COMMON_APP_DATA:
			{
				#if defined WIN32

				char *f = GetWindowsFolder(CSIDL_COMMON_APPDATA);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}
				
				#elif defined MAC
				
				FSRef Ref;
				OSErr e = FSFindFolder(kOnSystemDisk, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
				else
				{
					GAutoString u = FSRefPath(Ref);
					if (u)
					{
						strsafecpy(Dst, u, DstSize);
						Status = true;
					}
				}

				#elif defined LINUX

				strcpy(Dst, "/usr");
				Status = true;

				#elif defined BEOS

				strsafecpy(Dst, "/boot/apps", DstSize);
				Status = true;

				#endif
				break;
			}
			case LSP_USER_APP_DATA:
			{
				#if defined WIN32
				char *f = GetWindowsFolder(CSIDL_APPDATA);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}

				#elif defined MAC
				
				FSRef Ref;
				OSErr e = FSFindFolder(kUserDomain, kDomainLibraryFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
				else
				{
					GAutoString u = FSRefPath(Ref);
					if (u)
					{
						strsafecpy(Dst, u, DstSize);
						Status = true;
					}
				}

				#elif defined LINUX

				strcpy(Dst, "/usr");
				Status = true;

				#endif
				break;
			}
			case LSP_LOCAL_APP_DATA:
			{
				#if defined WIN32
				char *f = GetWindowsFolder(CSIDL_LOCAL_APPDATA);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}
				#endif
				break;
			}
			case LSP_DESKTOP:
			{
				#if defined(_WINDOWS)

				char *f = GetWindowsFolder(CSIDL_DESKTOPDIRECTORY);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}

				#elif defined MAC
				
				FSRef Ref;
				OSErr e = FSFindFolder(kOnAppropriateDisk, kDesktopFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
				else
				{
					GAutoString u = FSRefPath(Ref);
					if (u)
					{
						strsafecpy(Dst, u, DstSize);
						Status = true;
					}
				}

				#elif defined POSIX

				struct passwd *pw = getpwuid(getuid());
				if (pw)
				{
					#ifdef LINUX
					WindowManager wm = LgiGetWindowManager();
					if (wm == WM_Gnome)
						sprintf(Dst, "%s/.gnome-desktop", pw->pw_dir);
					else
					#endif
						sprintf(Dst, "%s/Desktop", pw->pw_dir);
						
					Status = true;
				}

				#elif defined BEOS

				strsafecpy(Dst, "/boot/home/Desktop", DstSize);
				Status = true;
				
				#endif
				break;
			}
			case LSP_HOME:
			{
				#if defined WIN32

				#ifndef CSIDL_PROFILE
				#define CSIDL_PROFILE 0x0028
				#endif 

				char *f = GetWindowsFolder(CSIDL_PROFILE);
				if (f)
				{
					strsafecpy(Dst, f, DstSize);
					DeleteArray(f);
					Status = true;
				}

				#elif defined POSIX

				struct passwd *pw = getpwuid(getuid());
				if (pw)
				{
					strcpy(Dst, pw->pw_dir);
					Status = true;
				}

				#elif defined BEOS

				strsafecpy(Dst, "/boot/home", DstSize);
				Status = true;

				#endif
				break;
			}
			case LSP_EXE:
			{
				if (LgiGetExeFile(Dst, DstSize))
				{
					LgiTrimDir(Dst);
					Status = true;
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
									strsafecpy(KdeTrash, s, sizeof(KdeTrash));
									DeleteArray(s);

									// Clear out any crap at the end...
									char *e = KdeTrash + strlen(KdeTrash) - 1;
									while (e > KdeTrash AND strchr(" \r\n\t/", *e))
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
						{
							strsafecpy(Dst, KdeTrash, DstSize);
							Status = true;
						}
						break;
					}
					default:
					{
						printf("%s:%i - Unknown window manager.\n", __FILE__, __LINE__);
						break;
					}
				}
				
				#elif defined MAC
				
				FSRef Ref;
				OSErr e = FSFindFolder(kUserDomain, kTrashFolderType, kDontCreateFolder, &Ref);
				if (e) printf("%s:%i - FSFindFolder failed e=%i\n", __FILE__, __LINE__, e);
				else
				{
					GAutoString u = FSRefPath(Ref);
					if (u)
					{
						strsafecpy(Dst, u, DstSize);
						Status = true;
					}
				}

				#elif defined WIN32

				// This should work but doesn't... *shrug*
				// char *f = GetWin32Folder(CSIDL_BITBUCKET);

				#else

				#endif
				break;
			}
		}
	}

	return Status;
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
					strcpy(Dst, e);
					DeleteArray(e);
					return true;
				}
				else
				{
					LgiMsg(0, "LgiFromNativeCp returned 0, ANSI CodePage=%i (%s)", "LgiGetExeFile Error", MB_OK, GetACP(), LgiAnsiToLgiCp());
				}
			}

			char m[256];
			sprintf(m, "GetModuleFileName failed err: %08.8X", GetLastError());
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
					strcpy(Dst, e);
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
				strcpy(Dst, p.Path());
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
				sprintf(Dst, "%s%s%s", p.c_str(), DIR_STR, LgiApp->_AppFile);
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
				sprintf(ProcFile, "/proc/%i/maps", getpid());
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
								strcpy(ExePathCache, s);
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
					// sprintf(Cmd, "ps | grep \"%i\"", getpid());
					
					GStringPipe Ps;
					GProcess p;
					if (p.Run("ps", 0, 0, true, 0, &Ps))
					{
						char *PsOutput = Ps.NewStr();
						if (PsOutput)
						{
							GToken n(PsOutput, "\r\n");
							for (int i=0; !Status AND i<n.Length(); i++)
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
										strcpy(ExePathCache, t[t.Length()-1]);
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
			strsafecpy(Dst, ExePathCache, DstSize);
		}
		
		return Status;
		
		#elif defined MAC
		
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
					strsafecpy(Dst, u, DstSize);
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

char *LgiGetExtension(const char *File)
{
	if (File)
	{
		char *Dot = strrchr((char*)File, '.');
		if (Dot AND !strchr(Dot, DIR_CHAR))
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
		
		if (strlen(Name) > 200)
		{
			int asd=0;
		}
		
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
			sprintf(Exe+strlen(Exe), "/%s", p[i]);

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
			"Contents/Resources",
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
			char Path[256];
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

		if (LgiRecursiveFileSearch(Exe, &Ext, &Files) AND
			Files.Length())
		{
			Result = Files[0];
			Files.DeleteAt(0);
		}
		Files.DeleteArrays();
	}

	return Result;
}

uint64 LgiCurrentTime()
{
	#if defined WIN32

	static LARGE_INTEGER freq = {0};
	static bool Init = false;
	if (!Init)
	{
		Init = true;
		if (!QueryPerformanceFrequency(&freq))
			freq.QuadPart = 0;
	}

	if (freq.QuadPart)
	{
		// Return performance counter in ms
		LARGE_INTEGER i;
		if (QueryPerformanceCounter(&i))
		{
			return i.QuadPart * 1000 / freq.QuadPart;
		}

		// Now what?? Give up and go back to tick count I guess.
		freq.QuadPart = 0;
	}

	// Fall back for systems without a performance counter.
	return GetTickCount();

	#elif defined BEOS

	return system_time() / 1000;

	#elif defined MAC
	
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

int LgiIsReleaseBuild()
{
	#ifdef _DEBUG
	return 0;
	#else
	return 1;
 	#endif
}

void LgiFormatSize(char *Str, uint64 Size)
{
	int64 K = 1024;
	int64 M = K * K;
	int64 G = K * M;

	if (Size == 1)
	{
		strcpy(Str, "1 byte");
	}
	else if (Size < K)
	{
		sprintf(Str, "%u bytes", (int)Size);
	}
	else if (Size < 10 * K)
	{
		double d = (double)(int64)Size;
		sprintf(Str, "%.2f K", d / K);
	}
	else if (Size < M)
	{
		sprintf(Str, "%u K", (int) (Size / K));
	}
	else if (Size < G)
	{
		double d = (double)(int64)Size;
		sprintf(Str, "%.2f M", d / M);
	}
	else
	{
		double d = (double)(int64)Size;
		sprintf(Str, "%.2f G", d / G);
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
#endif

//////////////////////////////////////////////////////////////////////
bool GCapabilityClient::NeedsCapability(const char *Name)
{
    for (int i=0; i<Targets.Length(); i++)
        Targets[i]->NeedsCapability(Name);
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

