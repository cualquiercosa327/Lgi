/*hdr
**	FILE:		File.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		11/7/95
**	DESCRIPTION:	The new file subsystem
**
**	Copyright (C) 1995, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes ************************************************************************************/
#include <winsock2.h>
#include <shlobj.h>
#include <fcntl.h>

#include "LgiInc.h"
#include "LgiOsDefs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifdef _MSC_VER
#include <direct.h>
#endif

#include "GFile.h"
#include "GContainers.h"
#include "GLibrary.h"
#include "GToken.h"
#include "LgiCommon.h"
#include "GSemaphore.h"
#include "GString.h"
#include "GLibrary.h"


/****************************** Defines *************************************************************************************/
#define FILEDEBUG
#define CHUNK					0xFFF0

#define FLOPPY_360K				0x0001
#define FLOPPY_720K				0x0002
#define FLOPPY_1_2M				0x0004
#define FLOPPY_1_4M				0x0008
#define FLOPPY_5_25				(FLOPPY_360K | FLOPPY_1_2M)
#define FLOPPY_3_5				(FLOPPY_720K | FLOPPY_1_4M)

/****************************** Helper Functions ****************************************************************************/
char *ReadTextFile(char *File)
{
	char *s = 0;
	GFile f;
	if (File && f.Open(File, O_READ))
	{
		int Len = f.GetSize();
		s = new char[Len+1];
		if (s)
		{
			int Read = f.Read(s, Len);
			s[Read] = 0;
		}
	}
	return s;
}

int64 LgiFileSize(char *FileName)
{
	int64 Size = 0;
	GDirectory *Dir = FileDev->GetDir();
	if (Dir &&
		Dir->First(FileName, 0))
	{
		Size = Dir->GetSize();
	}
	DeleteObj(Dir);
	return Size;
}

typedef wchar_t char16;

bool FileExists(const char *Name)
{
	bool Status = false;
	if (Name)
	{
		HANDLE hFind = INVALID_HANDLE_VALUE;
		
		if (GFileSystem::Win9x)
		{
			char *n = LgiToNativeCp(Name);
			if (n)
			{
				WIN32_FIND_DATA Info;
				hFind = FindFirstFile(n, &Info);
				if (hFind != INVALID_HANDLE_VALUE)
					Status = strcmp(Info.cFileName, ".") != 0;
				DeleteArray(n);
			}
		}
		else
		{
			
			char16 *n = LgiNewUtf8To16(Name);
			if (n)
			{
				WIN32_FIND_DATAW Info;
				hFind = FindFirstFileW(n, &Info);
				if (hFind != INVALID_HANDLE_VALUE)
					Status = StrcmpW(Info.cFileName, L".") != 0;
				DeleteArray(n);
			}
		}

		if (hFind != INVALID_HANDLE_VALUE)
		{
			FindClose(hFind);
			return true;
		}
	}

	return false;
}

bool DirExists(const char *Dir)
{
	DWORD e = 0;
	if (GFileSystem::Win9x)
	{
		char *n = LgiToNativeCp(Dir);
		e = GetFileAttributesA(n);
		DeleteArray(n);
	}
	else
	{
		char16 *n = LgiNewUtf8To16(Dir);
		e = GetFileAttributesW(n);
		DeleteArray(n);
	}
	return e != 0xFFFFFFFF && TestFlag(e, FILE_ATTRIBUTE_DIRECTORY);
}

const char *GetErrorName(int e)
{
	static char Buf[256];
	char *s = Buf;

	Buf[0] = 0;
	::FormatMessageA(	// FORMAT_MESSAGE_ALLOCATE_BUFFER | 
						FORMAT_MESSAGE_IGNORE_INSERTS |
						FORMAT_MESSAGE_FROM_SYSTEM,
						NULL, 
						e,
						MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
						(LPTSTR)Buf, 
						sizeof(Buf), 
						NULL);

	s = Buf + strlen(Buf);
	while (s > Buf && strchr(" \t\r\n", s[-1]))
	{
		s--;
		*s = 0;
	}

	return Buf[0] ? Buf : 0;
}

int _GetLongPathName(char *Short, char *Long, int Buf)
{
	bool Status = false;
	int Len = 0;

	if (Short && Long)
	{
		HMODULE hDll = 0;

		hDll = LoadLibrary("kernel32.dll");
		if (hDll)
		{
			// Win98/ME/2K... short->long conversion supported
			typedef DWORD (__stdcall *Proc_GetLongPathName)(LPCSTR, LPSTR, DWORD);
			Proc_GetLongPathName Proc = (Proc_GetLongPathName)GetProcAddress(hDll, "GetLongPathNameA");
			if (Proc)
			{
				Len = Proc(Short, Long, Buf);
				Status = true;
			}
			FreeLibrary(hDll);
		}
		
		if (!Status)
		{
			// Win95/NT4 doesn't support this function... so we do the conversion ourselves
			char *In = Short;
			char *Out = Long;

			*Out = 0;

			while (*In)
			{
				// find end of segment
				char *e = strchr(In, DIR_CHAR);
				if (!e) e = In + strlen(In);

				// process segment
				char Old = *e;
				*e = 0;
				if (strchr(In, ':'))
				{
					// drive specification
					strcat(Out, In); // just copy over
				}
				else
				{
					strcat(Out, DIR_STR);

					if (strlen(In) > 0)
					{
						// has segment to work on
						WIN32_FIND_DATA Info;
						HANDLE Search = FindFirstFile(Short, &Info);
						if (Search != INVALID_HANDLE_VALUE)
						{
							// have long name segment, copy over
							strcat(Out, Info.cFileName);
							FindClose(Search);
						}
						else
						{
							// no long name segment... copy over short segment :(
							strcat(Out, In);
						}
					}
					// else is a double path char... i.e. as in a M$ network path
				}

				In = (Old) ? e + 1 : e;
				Out += strlen(Out);
				*e = Old;
			}
		}
	}

	return Len;
}

bool ResolveShortcut(char *LinkFile, char *Path, int Len) 
{
	bool Status = false;
	/*
	HMODULE hDll = LoadLibrary("ole32.dll");
	if (hDll)
	{
		typedef HRESULT (__stdcall *Proc_CoInitialize)(LPVOID pvReserved);
		typedef HRESULT (__stdcall *Proc_CoCreateInstance)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID FAR* ppv);
		typedef void (__stdcall *Proc_CoUninitialize)(void);

		Proc_CoInitialize pCoInitialize = (Proc_CoInitialize)GetProcAddress(hDll, "CoInitialize");
		Proc_CoCreateInstance pCoCreateInstance = (Proc_CoCreateInstance)GetProcAddress(hDll, "CoCreateInstance");
		Proc_CoUninitialize pCoUninitialize = (Proc_CoUninitialize)GetProcAddress(hDll, "CoUninitialize");

		if (pCoInitialize &&
			pCoCreateInstance &&
			pCoUninitialize)
		{
		*/
			HWND hwnd = NULL;
			HRESULT hres;
			IShellLink* psl;
			char szGotPath[MAX_PATH] = "";
			WIN32_FIND_DATA wfd;

			CoInitialize(0);
			hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**) &psl);

			if (SUCCEEDED(hres))
			{
				IPersistFile* ppf;

				hres = psl->QueryInterface(IID_IPersistFile, (void**) &ppf);
				if (SUCCEEDED(hres))
				{
					char16 wsz[MAX_PATH];

					MultiByteToWideChar(CP_ACP, 0, LinkFile, -1, wsz, MAX_PATH);

					hres = ppf->Load(wsz, STGM_READ);
					if (SUCCEEDED(hres))
					{
						hres = psl->Resolve(hwnd, SLR_ANY_MATCH);
						if (SUCCEEDED(hres))
						{
							hres = psl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATA *)&wfd, SLGP_SHORTPATH );

							if (SUCCEEDED(hres) && strlen(szGotPath) > 0)
							{
								// lstrcpy(Path, szGotPath);
								_GetLongPathName(szGotPath, Path, Len);
								Status = true;
							}
						}
					}

					ppf->Release();
				}

				psl->Release();
			}

			CoUninitialize();

		/*
		}

		FreeLibrary(hDll);
	}
	*/

	return Status;
}

void WriteStr(GFile &f, char *s)
{
	uint32 Len = (s) ? strlen(s) : 0;
	f << Len;
	if (Len > 0)
	{
		f.Write(s, Len);
	}
}

char *ReadStr(GFile &f DeclDebugArgs)
{
	char *s = 0;

	// read the strings length...
	uint32 Len;
	f >> Len;

	if (Len > 0)
	{
		// 16mb sanity check.... anything over this
		// is _probably_ an error
		if (Len >= (16 << 20))
		{
			// LgiAssert(0);
			return 0;
		}

		// allocate the memory buffer
		#if defined MEMORY_DEBUG
		s = new(_file, _line) char[Len+1];
		#else
		s = new char[Len+1];
		#endif
		if (s)
		{
			// read the bytes from disk
			f.Read(s, Len);
			s[Len] = 0;


		}
		else
		{
			// memory allocation error, skip the data
			// on disk so the caller is where they think 
			// they are in the file.
			f.Seek(Len, SEEK_CUR);
		}
	}

	return s;
}

int SizeofStr(char *s)
{
	return sizeof(ulong) + ((s) ? strlen(s) : 0);
}

bool LgiGetDriveInfo
(
	char *Path,
	uint64 *Free,
	uint64 *Size,
	uint64 *Available
)
{
	bool Status = false;
	ULARGE_INTEGER available = {0, 0};
	ULARGE_INTEGER total = {0, 0};
	ULARGE_INTEGER free = {0, 0};

	if (Path)
	{
		if (GFileSystem::Win9x)
		{
			char *a = LgiToNativeCp(Path);
			if (a)
			{
				char *d = strchr(a, DIR_CHAR);
				if (d) *d = 0;

				if (GetDiskFreeSpaceExA(a,
										&available,
										&total,
										&free))
				{
					if (Free) *Free = free.QuadPart;
					if (Size) *Size = total.QuadPart;
					if (Available) *Available = available.QuadPart;
					Status = true;
				}

				DeleteArray(a);
			}
		}
		else
		{
			char16 *w = LgiNewUtf8To16(Path);
			if (w)
			{
				char16 *d = StrchrW(w, DIR_CHAR);
				if (d) *d = 0;

				if (GetDiskFreeSpaceExW(w,
										&available,
										&total,
										&free))
				{
					if (Free) *Free = free.QuadPart;
					if (Size) *Size = total.QuadPart;
					if (Available) *Available = available.QuadPart;
					Status = true;
				}

				DeleteArray(w);
			}
		}
	}

	return Status;
}
 
/****************************** Classes ***************************************/
GVolume::GVolume()
{
	_Name = 0;
	_Path = 0;
	_Type = VT_NONE;
	_Flags = 0;
	_Size = 0;
	_Free = 0;
}

GVolume::~GVolume()
{
	DeleteArray(_Name);
	DeleteArray(_Path);
}

////////////////////////////////////////////////////////////////////////////////
class GWin32Volume : public GVolume
{
	int Which;
	List<GVolume> _Sub;

public:
	GWin32Volume(int w)
	{
		Which = w;

		if (Which >= 0)
		{
			char Drive[] = { 'A'+w, ':', '\\', 0 };
			char VolName[256], System[256];
			DWORD MaxPath;

			int type = GetDriveType(Drive);
			if (type != DRIVE_UNKNOWN &&
				type != DRIVE_NO_ROOT_DIR)
			{
				char Buf[256];
				char *Desc = 0;
				switch (type)
				{
					case DRIVE_REMOVABLE:
					{
						_Type = VT_REMOVABLE;

						if (w > 1 &&
							GetVolumeInformation(Drive, Buf, sizeof(Buf), 0, 0, 0, 0, 0) &&
							ValidStr(Buf))
						{
							Desc = Buf;
						}
						else
						{
							Desc = (char*)(Which > 1 ? "Removable" : "Floppy");
						}
						break;
					}
					case DRIVE_REMOTE:
						Desc = (char*)"Network";
						_Type = VT_NETWORK_SHARE;
						break;
					case DRIVE_CDROM:
						Desc = (char*)"Cdrom";
						_Type = VT_CDROM;
						break;
					case DRIVE_RAMDISK:
						Desc = (char*)"Ramdisk";
						_Type = VT_RAMDISK;
						break;
					case DRIVE_FIXED:
					default:
					{
						if (GetVolumeInformation(Drive, Buf, sizeof(Buf), 0, 0, 0, 0, 0) &&
							ValidStr(Buf))
						{
							Desc = Buf;
						}
						else
						{
							Desc = "Hard Disk";
						}
						_Type = VT_HARDDISK;
						break;
					}
				}

				char s[256];
				sprintf(s, "%s (%.2s)", Desc, Drive);
				_Name = NewStr(s);
				_Path = NewStr(Drive);
			}
		}
		else
		{
			_Name = NewStr("Desktop");
			_Type = VT_DESKTOP;
			_Path = GetWindowsFolder(CSIDL_DESKTOPDIRECTORY);
		}
	}

	~GWin32Volume()
	{
		_Sub.DeleteObjects();
		DeleteArray(_Name);
		DeleteArray(_Path);
	}

	bool IsMounted()
	{
		return false;
	}

	bool SetMounted(bool Mount)
	{
		return Mount;
	}
	
	GVolume *First()
	{
		if (Which < 0 &&
			!_Sub.Length())
		{
			// Get drive list
			char Str[512];
			if (GetLogicalDriveStrings(sizeof(Str), Str) > 0)
			{
				for (char *d=Str; *d; d+=strlen(d)+1)
				{
					GWin32Volume *v = new GWin32Volume(d[0]-'A');
					if (v)
					{
						_Sub.Insert(v);
					}
				}
			}
			else
			{
			}
		}

		return _Sub.First();
	}

	GVolume *Next()
	{
		return _Sub.Next();
	}

	GDirectory *GetContents()
	{
		GDirectory *Dir = 0;
		if (Which >= 0 &&
			_Path)
		{
			Dir = FileDev->GetDir();
			if (Dir)
			{
				if (!Dir->First(_Path))
				{
					DeleteObj(Dir);
				}
			}
		}
		return Dir;
	}
};

////////////////////////////////////////////////////////////////////////////////
bool GFileSystem::Win9x = false;
GFileSystem *GFileSystem::Instance = 0;

class GFileSystemPrivate
{
public:
	GLibrary *Shell;

	GFileSystemPrivate()
	{
		Shell = 0;
	}

	~GFileSystemPrivate()
	{
		DeleteObj(Shell);
	}
};

GFileSystem::GFileSystem()
{
	Instance = this;
	d = new GFileSystemPrivate;
	Root = 0;
	Win9x = LgiGetOs() == LGI_OS_WIN9X;
}

GFileSystem::~GFileSystem()
{
	DeleteObj(Root);
	DeleteObj(d);
}

void GFileSystem::OnDeviceChange(char *Reserved)
{
	DeleteObj(Root);
}

GVolume *GFileSystem::GetRootVolume()
{
	if (!Root)
	{
		Root = new GWin32Volume(-1);
	}

	return Root;
}

GDirectory *GFileSystem::GetDir()
{
	return new GDirImpl;
}

bool GFileSystem::Copy(char *From, char *To, int *ErrorCode, CopyFileCallback Callback, void *Token)
{
	if (!From || !To)
	{
		if (ErrorCode) *ErrorCode = ERROR_INVALID_PARAMETER;
		return false;
	}

	GFile In, Out;
	if (!In.Open(From, O_READ))
	{
		if (ErrorCode) *ErrorCode = In.GetError();
		return false;
	}

	if (!Out.Open(To, O_WRITE))
	{
		if (ErrorCode) *ErrorCode = Out.GetError();
		return false;
	}

	if (Out.SetSize(0))
	{
		if (ErrorCode) *ErrorCode = ERROR_WRITE_FAULT;
		return false;
	}

	int64 Size = In.GetSize();
	if (!Size)
	{
		return true;
	}

	int64 Block = min((1 << 20), Size);
	char *Buf = new char[Block];
	if (!Buf)
	{
		if (ErrorCode) *ErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		return false;
	}

	int64 i = 0;
	while (i < Size)
	{
		int r = In.Read(Buf, Block);
		if (r > 0)
		{
			int Written = 0;
			while (Written < r)
			{
				int w = Out.Write(Buf + Written, r - Written);
				if (w > 0)
				{
					Written += w;
				}
				else
				{
					if (ErrorCode) *ErrorCode = ERROR_WRITE_FAULT;
					goto ExitCopyLoop;
				}
			}
			i += Written;

			if (Callback)
			{
				if (!Callback(Token, i, Size))
				{
					break;
				}
			}
		}
		else break;
	}

	ExitCopyLoop:
	DeleteArray(Buf);
	if (i == Size)
	{
		if (ErrorCode) *ErrorCode = ERROR_SUCCESS;
	}
	else
	{
		Out.Close();
		Delete(To, false);
	}

	return i == Size;
}

bool GFileSystem::Delete(GArray<char*> &Files, GArray<int> *Status, bool ToTrash)
{
	bool Ret = true;

	if (ToTrash)
	{
		if (!d->Shell)
		{
			d->Shell = new GLibrary("Shell32");
		}

		if (d->Shell)
		{
			int e;

			if (Win9x)
			{
				pSHFileOperationA f = (pSHFileOperationA) d->Shell->GetAddress("SHFileOperationA");
				if (f)
				{
					GArray<char> Name;
					for (int i=0; i<Files.Length(); i++)
					{
						char *Mem = LgiToNativeCp(Files[i]);
						if (Mem)
						{
							int In = strlen(Mem);
							int Len = Name.Length();

							Name.Length(Len + In + 1);
							strcpy(&Name[Len], Mem);

							DeleteArray(Mem);
						}
					}
					Name.Add(0);

					SHFILEOPSTRUCTA s;
					ZeroObj(s);
					s.hwnd = 0;
					s.wFunc = FO_DELETE;
					s.pFrom = &Name[0];
					s.fFlags = FOF_ALLOWUNDO;

					e = f(&s);
				}
			}
			else
			{
				pSHFileOperationW f = (pSHFileOperationW) d->Shell->GetAddress("SHFileOperationW");
				if (f)
				{
					GArray<char16> Name;
					for (int i=0; i<Files.Length(); i++)
					{
						int InSize = strlen(Files[i]);
						char16 Buf[300];
						ZeroObj(Buf);
						const void *InPtr = Files[i];
						LgiBufConvertCp(Buf, LGI_WideCharset, sizeof(Buf), InPtr, "utf-8", InSize);
						int Chars = StrlenW(Buf);
						int Len = Name.Length();
						Name.Length(Len + Chars + 1);
						StrcpyW(&Name[Len], Buf);
					}
					Name.Add(0);

					SHFILEOPSTRUCTW s;
					ZeroObj(s);
					s.hwnd = 0;
					s.wFunc = FO_DELETE;
					s.pFrom = &Name[0];
					s.fFlags = FOF_ALLOWUNDO;

					e = f(&s);
				}
			}

			Ret = e == 0;
			if (Status && e)
			{
				for (int i=0; i<Files.Length(); i++)
				{
					(*Status)[i] = e;
				}
			}
		}
	}
	else
	{
		for (int i=0; i<Files.Length(); i++)
		{
			DWORD e = 0;

			if (Win9x)
			{
				char *n = LgiToNativeCp(Files[i]);
				if (n)
				{
					SetFileAttributes(n, FILE_ATTRIBUTE_ARCHIVE);
					if (!::DeleteFile(n))
					{
						e = GetLastError();
					}
					DeleteArray(n);
				}
			}
			else
			{
				char16 *n = LgiNewUtf8To16(Files[i]);
				if (n)
				{
					SetFileAttributesW(n, FILE_ATTRIBUTE_ARCHIVE);
					if (!::DeleteFileW(n))
					{
						e = GetLastError();
					}
					DeleteArray(n);
				}
			}

			if (e && Status)
			{
				(*Status)[i] = e;
				Ret = false;
			}
		}
	}

	return Ret;
}

bool GFileSystem::Delete(char *FileName, bool ToTrash)
{
	if (!FileName)
		return false;

	GArray<char*> Files;
	Files.Add(FileName);
	return Delete(Files, 0, ToTrash);
}

bool GFileSystem::CreateFolder(char *PathName)
{
	bool Status = false;
	if (Win9x)
	{
		char *c8 = LgiToNativeCp(PathName);
		Status = ::CreateDirectoryA(c8, NULL);
		DeleteArray(c8);
	}
	else
	{
		char16 *w = LgiNewUtf8To16(PathName);
		Status = ::CreateDirectoryW(w, NULL);
		DeleteArray(w);
	}
	return Status;
}

bool GFileSystem::RemoveFolder(char *PathName, bool Recurse)
{
	if (Recurse)
	{
		GDirectory *Dir = FileDev->GetDir();
		if (Dir &&
			Dir->First(PathName))
		{
			do
			{
				char Str[256];
				Dir->Path(Str, sizeof(Str));

				if (Dir->IsDir())
				{
					RemoveFolder(Str, Recurse);
				}
				else
				{
					Delete(Str, false);
				}
			}
			while (Dir->Next());
		}
		DeleteObj(Dir);
	}

	if (!::RemoveDirectory(PathName))
	{
		#ifdef _DEBUG
		DWORD e = GetLastError();
		#endif

		return false;
	}

	return true;
}

bool GFileSystem::SetCurrentDirectory(char *PathName)
{
	bool Status = false;
	
	if (Win9x)
	{
		char *n = LgiToNativeCp(PathName);
		if (n)
		{
			Status = ::SetCurrentDirectory(n);
			DeleteArray(n);
		}
	}
	else
	{
		char16 *w = LgiNewUtf8To16(PathName);
		if (w)
		{
			Status = ::SetCurrentDirectoryW(w);
			DeleteArray(w);
		}
	}

	return Status;
}

bool GFileSystem::GetCurrentDirectory(char *PathName, int Length)
{
	bool Status = false;

	if (Win9x)
	{
		char *s = new char[Length+1];
		if (s)
		{
			if (::GetCurrentDirectory(Length, s) > 0)
			{
				char *n = LgiFromNativeCp(s);
				if (n)
				{
					strsafecpy(PathName, n, Length);
					Status = true;
					DeleteArray(n);
				}
			}

			DeleteArray(s);
		}
	}
	else
	{
		char16 *w = new char16[Length+1];
		if (w)
		{
			if (::GetCurrentDirectoryW(Length, w) > 0)
			{
				char *s = LgiNewUtf16To8(w);
				if (s)
				{
					strsafecpy(PathName, s, Length);
					DeleteArray(s);
					Status = true;
				}
			}

			DeleteArray(w);
		}
	}

	return Status;
}

bool GFileSystem::Move(char *OldName, char *NewName)
{
	bool Status = false;

	if (OldName && NewName)
	{
		if (Win9x)
		{
			char *New = LgiToNativeCp(NewName);
			char *Old = LgiToNativeCp(OldName);

			if (New && Old)
				Status = ::MoveFileA(Old, New);

			DeleteArray(New);
			DeleteArray(Old);
		}
		else
		{
			char16 *New = LgiNewUtf8To16(NewName);
			char16 *Old = LgiNewUtf8To16(OldName);

			if (New && Old)
				Status = ::MoveFileW(Old, New);

			DeleteArray(New);
			DeleteArray(Old);
		}

	}

	return Status;
}

/*
bool GFileSystem::GetVolumeInfomation(char Drive, VolumeInfo *pInfo)
{
	bool Status = false;

	if (pInfo)
	{
		char Name[] = "A:\\";
		ulong Serial;

		Name[0] = Drive + 'A';

		if (GetVolumeInformation(	Name,
						pInfo->Name,
						sizeof(pInfo->Name),
						&Serial,
						&pInfo->MaxPath,
						&pInfo->Flags,
						pInfo->System,
						sizeof(pInfo->System)))
		{
			pInfo->Drive = Drive;
			Status = true;
		}
	}

	return Status;
}
*/

void GetFlagsStr(char *flagstr, short flags)
{
	if (flagstr)
	{
		flagstr[0] = 0;

		if (flags & O_WRONLY)
		{
			strcat(flagstr,"w");
		}
		else if (flags & O_APPEND)
		{
			strcat(flagstr,"a");
		}
		else
		{
			strcat(flagstr,"r");
		}

		if ((flags & O_RDWR) == O_RDWR)
		{
			strcat(flagstr,"+");
		}

		strcat(flagstr,"b");
	}
}

bool Match(char *Name, char *Mask)
{
	while (*Name && *Mask)
	{
		if (*Mask == '*')
		{
			if (toupper(*Name) == toupper(Mask[1]))
			{
				Mask++;
			}
			else
			{
				Name++;
			}
		}
		else if (*Mask == '?' || toupper(*Mask) == toupper(*Name))
		{
			Mask++;
			Name++;
		}
		else
		{
			return false;
		}
	}

	while (*Mask && ((*Mask == '*') || (*Mask == '.'))) Mask++;

	return (*Name == 0 && *Mask == 0);
}

short DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int LeapYear(int year)
{
	if (year & 3)
	{
		return 0;
	}
	if ((year % 100 == 0) && !(year % 400 == 0))
	{
		return 0;
	}
	
	return 1;
}

// Disk Infomation
typedef struct
{
    uchar	bsJump[3];
    uchar	bsOemName[8];
    ushort	bsBytePerSec;
    uchar	bsSecPerCluster;
    ushort	bsResSectores;
    uchar	bsFAT;
    ushort	bsRootDirEnts;
    ushort	bsSectors;
    uchar	bsMedia;
    ushort	bsFATsecs;
    ushort	bsSecPerTrack;
    ushort	bsHeads;
    ulong	bsHiddenSecs;
    ulong	bsHugeSectoes;
    uchar	bsDriveNumber;
    uchar	bsReserved;
    uchar	bsBootSig;
    ulong	bsVolumeID; // serial number
    uchar	bsVolumeLabel[11];
    uchar	bsFileSysType[8];

} BOOTSECTOR;

typedef struct
{
    ulong	diStartSector;
    ushort	diSectors;
    char	*diBuffer;
} DISKIO;

/////////////////////////////////////////////////////////////////////////////////
bool GDirectory::ConvertToTime(char *Str, uint64 Time)
{
	if (Str)
	{
		FILETIME t;
		SYSTEMTIME s;
		t.dwHighDateTime = Time >> 32;
		t.dwLowDateTime = Time & 0xFFFFFFFF;
		if (FileTimeToSystemTime(&t, &s))
		{
			int Hour = (s.wHour < 1) ? s.wHour + 23 : s.wHour - 1;
			char AP = (Hour >= 12) ? 'a' : 'p';
			Hour %= 12;
			if (Hour == 0) Hour = 12;
			sprintf(Str, "%i:%02.2i:%02.2i %c", Hour-1, s.wMinute, s.wSecond, AP);
		}
	}
	return false;
}

bool GDirectory::ConvertToDate(char *Str, uint64 Time)
{
	if (Str)
	{
		FILETIME t;
		SYSTEMTIME s;
		t.dwHighDateTime = Time >> 32;
		t.dwLowDateTime = Time & 0xFFFFFFFF;
		if (FileTimeToSystemTime(&t, &s))
		{
			sprintf(Str, "%i/%i/%i", s.wDay, s.wMonth, s.wYear);
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Directory //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class GDirImplPrivate
{
public:
	char			BasePath[MAX_PATH];
	GAutoString     Utf;
	HANDLE			Handle;
	union
	{
		WIN32_FIND_DATAA a;
		WIN32_FIND_DATAW w;
	} Data;

	GDirImplPrivate()
	{
		Handle = INVALID_HANDLE_VALUE;
		BasePath[0] = 0;
	}

	~GDirImplPrivate()
	{
	}
};

GDirImpl::GDirImpl()
{
	d = new GDirImplPrivate;
}

GDirImpl::~GDirImpl()
{
	Close();
	DeleteObj(d);
}

GDirectory *GDirImpl::Clone()
{
	return new GDirImpl;
}

bool GDirImpl::Path(char *s, int BufLen)
{
	char *Name = GetName();
	int Len = strlen(d->BasePath) + 2;
	if (Name) Len += strlen(Name);
	bool Status = false;

	if (Name && (BufLen < 0 || Len <= BufLen))
	{
		LgiMakePath(s, BufLen, d->BasePath, Name);
		Status = true;
	}
	else LgiAssert(!"Not enough output buffer to write path.");

	return Status;
}

int GDirImpl::First(const char *Name, const char *Pattern)
{
	Close();

    d->Utf.Reset();

	if (Name)
	{
		if (IsAlpha(Name[0]) &&
			Name[1] == ':' &&
			strlen(Name) <= 3)
		{
			// raw drive
			if (!strchr(Name, DIR_CHAR))
			{
				strcat((char*)Name, DIR_STR);
			}
		}

		char *End = strrchr((char*)Name, DIR_CHAR);
		if (End)
		{
			// dir
			if (GFileSystem::Win9x)
			{
				char *path = LgiToNativeCp(Name);
				if (path)
				{
					char n[256] = "";
					GetFullPathName(path, sizeof(n), n, NULL);
					DeleteArray(path);
					
					char *utf = LgiFromNativeCp(n);
					if (utf)
					{
						strsafecpy(d->BasePath, utf, sizeof(d->BasePath));
						DeleteArray(utf);
					}
				}
			}
			else
			{
				char16 *p = LgiNewUtf8To16(Name);
				if (p)
				{
					char16 w[256];
					w[0] = 0;
					DWORD Chars = GetFullPathNameW(p, CountOf(w), w, NULL);
					if (Chars == 0)
					{
						DWORD e = GetLastError();
						StrcpyW(w, p);
					}
					DeleteArray(p);

					char *utf = LgiNewUtf16To8(w);
					if (utf)
					{
						strsafecpy(d->BasePath, utf, sizeof(d->BasePath));
						DeleteArray(utf);
					}
				}
			}

			char Str[MAX_PATH];
			if (Pattern)
				LgiMakePath(Str, sizeof(Str), d->BasePath, Pattern);
			else
				strsafecpy(Str, d->BasePath, sizeof(Str));

			if (GFileSystem::Win9x)
			{
				char *s = LgiToNativeCp(Str);
				if (s)
				{
					d->Handle = FindFirstFile(s, &d->Data.a);
					DeleteArray(s);
				}
			}
			else
			{
				char16 *s = LgiNewUtf8To16(Str);
				if (s)
				{
					d->Handle = FindFirstFileW(s, &d->Data.w);
					DeleteArray(s);
				}
			}

			if (d->Handle != INVALID_HANDLE_VALUE)
			{
				while (	stricmp(GetName(), ".") == 0 ||
						stricmp(GetName(), "..") == 0)
				{
					if (!Next()) return false;
				}
			}
		}
	}

	return d->Handle != INVALID_HANDLE_VALUE;
}

int GDirImpl::Next()
{
	int Status = false;
	
    d->Utf.Reset();

	if (d->Handle != INVALID_HANDLE_VALUE)
	{
		if (GFileSystem::Win9x)
		{
			Status = FindNextFile(d->Handle, &d->Data.a);
		}
		else
		{
			Status = FindNextFileW(d->Handle, &d->Data.w);
		}
	}

	return Status;
}

int GDirImpl::Close()
{
	if (d->Handle != INVALID_HANDLE_VALUE)
	{
		FindClose(d->Handle);
		d->Handle = INVALID_HANDLE_VALUE;
	}

    d->Utf.Reset();

	return true;
}

int GDirImpl::GetUser(bool Group)
{
	return 0;
}

bool GDirImpl::IsReadOnly()
{
	if (GFileSystem::Win9x)
	{
		return (d->Data.a.dwFileAttributes & FA_READONLY) != 0;
	}
	else
	{
		return (d->Data.w.dwFileAttributes & FA_READONLY) != 0;
	}
}

bool GDirImpl::IsDir()
{
	if (GFileSystem::Win9x)
	{
		return (d->Data.a.dwFileAttributes & FA_DIRECTORY) != 0;
	}
	else
	{
		return (d->Data.w.dwFileAttributes & FA_DIRECTORY) != 0;
	}
}

bool GDirImpl::IsHidden()
{
	if (GFileSystem::Win9x)
	{
		return (d->Data.a.dwFileAttributes & FA_HIDDEN) != 0;
	}
	else
	{
		return (d->Data.w.dwFileAttributes & FA_HIDDEN) != 0;
	}
}

long GDirImpl::GetAttributes()
{
	if (GFileSystem::Win9x)
	{
		return d->Data.a.dwFileAttributes;
	}
	else
	{
		return d->Data.w.dwFileAttributes;
	}
}

int GDirImpl::GetType()
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

char *GDirImpl::GetName()
{
	if (!d->Utf)
	{
	    if (GFileSystem::Win9x)
	    {
		    d->Utf.Reset(LgiFromNativeCp(d->Data.a.cFileName));
	    }
	    else
	    {
		    d->Utf.Reset(LgiNewUtf16To8(d->Data.w.cFileName));
	    }
	}
	
	return d->Utf;
}

const uint64 GDirImpl::GetCreationTime()
{
	if (GFileSystem::Win9x)
	{
		return ((uint64) d->Data.a.ftCreationTime.dwHighDateTime) << 32 | d->Data.a.ftCreationTime.dwLowDateTime;
	}
	else
	{
		return ((uint64) d->Data.w.ftCreationTime.dwHighDateTime) << 32 | d->Data.w.ftCreationTime.dwLowDateTime;
	}
}

const uint64 GDirImpl::GetLastAccessTime()
{
	if (GFileSystem::Win9x)
	{
		return ((uint64) d->Data.a.ftLastAccessTime.dwHighDateTime) << 32 | d->Data.a.ftLastAccessTime.dwLowDateTime;
	}
	else
	{
		return ((uint64) d->Data.w.ftLastAccessTime.dwHighDateTime) << 32 | d->Data.w.ftLastAccessTime.dwLowDateTime;
	}
}

const uint64 GDirImpl::GetLastWriteTime()
{
	if (GFileSystem::Win9x)
	{
		return ((uint64) d->Data.a.ftLastWriteTime.dwHighDateTime) << 32 | d->Data.a.ftLastWriteTime.dwLowDateTime;
	}
	else
	{
		return ((uint64) d->Data.w.ftLastWriteTime.dwHighDateTime) << 32 | d->Data.w.ftLastWriteTime.dwLowDateTime;
	}
}

const uint64 GDirImpl::GetSize()
{
	if (GFileSystem::Win9x)
	{
		return ((uint64) d->Data.a.nFileSizeHigh) << 32 | d->Data.a.nFileSizeLow;
	}
	else
	{
		return ((uint64) d->Data.w.nFileSizeHigh) << 32 | d->Data.w.nFileSizeLow;
	}
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// File ///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class GFilePrivate
{
public:
	OsFile	hFile;
	char	*Name;
	int		Attributes;
	bool	Swap;
	bool	Status;
	int		CreateThread;
	DWORD	LastError;

	GFilePrivate()
	{
		hFile = INVALID_HANDLE;
		Name = 0;
		Attributes = 0;
		Swap = false;
		Status = false;
		LastError = 0;
	}

	~GFilePrivate()
	{
		DeleteArray(Name);
	}
};

GFile::GFile()
{
	d = new GFilePrivate;
}

GFile::~GFile()
{
	if (ValidHandle(d->hFile))
	{
		Close();
	}
	DeleteObj(d);
}

OsFile GFile::Handle()
{
	return d->hFile;
}

char *GFile::GetName()
{
	return d->Name;
}

void GFile::SetSwap(bool s)
{
	d->Swap = s;
}

bool GFile::GetSwap()
{
	return d->Swap;
}

int GFile::GetOpenMode()
{
	return d->Attributes;
}

int GFile::GetBlockSize()
{
	DWORD SectorsPerCluster;
	DWORD BytesPerSector = 0;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	
	char *n = NewStr(GetName());
	if (n)
	{
		char *dir = n ? strchr(n, '\\') : 0;
		if (dir) dir[1] = 0;
		GetDiskFreeSpace(	n,
							&SectorsPerCluster,
							&BytesPerSector,
							&NumberOfFreeClusters,
							&TotalNumberOfClusters);
		DeleteArray(n);
	}

	return BytesPerSector;
}

int GFile::Open(const char *File, int Mode)
{
	int Status = false;
	bool NoCache = (Mode & O_NO_CACHE) != 0;
	Mode &= ~O_NO_CACHE;

	Close();

	if (File)
	{
		bool SharedAccess = Mode & O_SHARE;
		Mode &= ~O_SHARE;

		if (GFileSystem::Win9x)
		{
			char *n = LgiToNativeCp(File);
			if (n)
			{
				d->hFile = CreateFile(	n,
										Mode,
										FILE_SHARE_READ | (SharedAccess ? FILE_SHARE_WRITE : 0),
										0,
										(Mode & O_WRITE) ? OPEN_ALWAYS : OPEN_EXISTING,
										NoCache ? FILE_FLAG_NO_BUFFERING : 0,
										NULL);
				DeleteArray(n);
			}
		}
		else
		{
			char16 *n = LgiNewUtf8To16(File);
			if (n)
			{
				d->hFile = CreateFileW(	n,
										Mode,
										FILE_SHARE_READ | (SharedAccess ? FILE_SHARE_WRITE : 0),
										0,
										(Mode & O_WRITE) ? OPEN_ALWAYS : OPEN_EXISTING,
										NoCache ? FILE_FLAG_NO_BUFFERING : 0,
										NULL);
				DeleteArray(n);
			}
		}

		if (!ValidHandle(d->hFile))
		{
			switch (d->LastError = GetLastError())
			{
				case 2:
				case 3:
				{
					// Path/File not found;
					int i=0;
					break;
				}
				case 5:
				{
					// Access is denied:
					// Read-Only or another process has the file open
					int i=0;
					break;
				}
			}
		}
		else
		{
			d->Attributes = Mode;
			d->Name = NewStr(File);

			Status = true;
			d->Status = true;
			d->CreateThread = LgiGetCurrentThread();
		}
	}

	return Status;
}

bool GFile::IsOpen()
{
	return ValidHandle(d->hFile);
}

int GFile::GetError()
{
	return d->LastError;
}

int GFile::Close()
{
	int Status = false;

	if (ValidHandle(d->hFile))
	{
		::CloseHandle(d->hFile);

		d->hFile = INVALID_HANDLE;
	}

	DeleteArray(d->Name);

	return Status;
}

int64 GFile::GetSize()
{
	LgiAssert(IsOpen());

	DWORD High = -1;
	DWORD Low = GetFileSize(d->hFile, &High);
	return Low | ((int64)High<<32);
}

int64 GFile::SetSize(int64 Size)
{
	LgiAssert(IsOpen());

	LONG OldPosHigh = 0;
	DWORD OldPosLow = SetFilePointer(d->hFile, 0, &OldPosHigh, SEEK_CUR);
	
	LONG SizeHigh = Size >> 32;
	SetFilePointer(d->hFile, Size, &SizeHigh, FILE_BEGIN);
	
	SetEndOfFile(d->hFile);
	
	SetFilePointer(d->hFile, OldPosLow, &OldPosHigh, FILE_BEGIN);
	return GetSize();
}

int64 GFile::GetPos()
{
	LgiAssert(IsOpen());

	LONG PosHigh = 0;
	DWORD PosLow = SetFilePointer(d->hFile, 0, &PosHigh, FILE_CURRENT);
	return PosLow | ((int64)PosHigh<<32);
}

int64 GFile::SetPos(int64 Pos)
{
	LgiAssert(IsOpen());

	LONG PosHigh = Pos >> 32;
	DWORD PosLow = SetFilePointer(d->hFile, Pos, &PosHigh, FILE_BEGIN);
	return PosLow | ((int64)PosHigh<<32);
}

int GFile::Read(void *Buffer, int Size, int Flags)
{
	DWORD Bytes = 0;
	if (ReadFile(d->hFile, Buffer, Size, &Bytes, NULL))
	{
		d->Status &= Bytes > 0;
	}
	else
	{
		d->LastError = GetLastError();
	}

	return Bytes;
}

int GFile::Write(const void *Buffer, int Size, int Flags)
{
	DWORD Bytes = 0;
	if (WriteFile(d->hFile, Buffer, Size, &Bytes, NULL))
	{
		d->Status &= Bytes > 0;
	}
	else
	{
		d->LastError = GetLastError();
	}

	return Bytes;
}

int64 GFile::Seek(int64 To, int Whence)
{
	LgiAssert(IsOpen());

	int Mode;
	switch (Whence)
	{
		default:
		case SEEK_SET:
			Mode = FILE_BEGIN;
			break;
		case SEEK_CUR:
			Mode = FILE_CURRENT;
			break;
		case SEEK_END:
			Mode = FILE_END;
			break;
	}

	LONG ToHigh = To >> 32;
	DWORD ToLow = SetFilePointer(d->hFile, To, &ToHigh, Mode);
	return ToLow | ((int64)ToHigh<<32);
}

bool GFile::Eof()
{
	LgiAssert(IsOpen());
	return GetPos() >= GetSize();
}

int GFile::SwapRead(uchar *Buf, int Size)
{
	int i = 0;
	DWORD r;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		if (!(d->Status &= ReadFile(d->hFile, Buf--, 1, &r, NULL) != 0 && r == 1))
			break;
		i += r;
	}
	return i;
}

int GFile::SwapWrite(uchar *Buf, int Size)
{
	int i = 0;
	DWORD w;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		if (!(d->Status &= WriteFile(d->hFile, Buf--, 1, &w, NULL) != 0 && w == 1))
			break;
		i += w;
	}
	return i;
}

int GFile::ReadStr(char *Buf, int Size)
{
	int i = 0;
	DWORD r;
	if (Buf && Size > 0)
	{
		char c;

		Size--;

		do
		{
			ReadFile(d->hFile, &c, 1, &r, NULL);
			if (Eof())
			{
				break;
			}
			
			*Buf++ = c;
			i++;
		
		} while (i < Size - 1 && c != '\n');

		*Buf = 0;
	}

	return i;
}

int GFile::WriteStr(char *Buf, int Size)
{
	int i = 0;
	DWORD w;

	while (i <= Size)
	{
		WriteFile(d->hFile, Buf, 1, &w, NULL);
		Buf++;
		i++;
		if (*Buf == '\n') break;
	}

	return i;
}

void GFile::SetStatus(bool s)
{
	d->Status = s;
}

bool GFile::GetStatus()
{
	return d->Status;
}

#define RdIO \
{ \
	if (d->Swap) \
		SwapRead((uchar*) &i, sizeof(i)); \
	else \
		Read(&i, sizeof(i)); \
	return *this; \
}

#define WrIO \
{ \
	if (d->Swap) \
		SwapWrite((uchar*) &i, sizeof(i)); \
	else \
		Write(&i, sizeof(i)); \
	return *this; \
}

#define GFilePre		GFile &GFile::operator >> (
#define GFilePost		&i) RdIO
GFileOps(); 
#undef GFilePre
#undef GFilePost

#define GFilePre		GFile &GFile::operator << (
#define GFilePost		i) WrIO
GFileOps(); 
#undef GFilePre
#undef GFilePost

/*
int FloppyType(int Letter)
{
	uchar MaxTrack;
	uchar SecPerTrack;

	_asm {
		mov eax, 0800h
		mov edx, Letter
		int 13h
		mov MaxTrack, ch
		mov SecPerTrack, cl
	}
	
	if (MaxTrack > 39)
	{
		switch (SecPerTrack)
		{
			case 9:
			{
				return FLOPPY_720K;
			}
			case 15:
			{
				return FLOPPY_1_2M;
			}
			case 18:
			{
				return FLOPPY_1_4M;
			}
		}
	}
	else
	{
		return FLOPPY_360K;
	}

	return 0;
}

int GFileSystem::FillDiskList()
{
	NumDrive = 0;
	DriveList = new DriveInfo[26];
	if (DriveList)
	{
		memset(DriveList, 0, sizeof(DriveInfo[26]));

		DriveInfo *d = DriveList;
		char Str[256];
		if (GetLogicalDriveStrings(sizeof(Str), Str) > 0)
		{
			for (char *s=Str; *s; s+=strlen(s)+1)
			{
				strcpy(d->Name, s);
				d->Type = GetDriveType(s);
				NumDrive++;
				d++;
			}
		}
	}

	return true;
}
*/
