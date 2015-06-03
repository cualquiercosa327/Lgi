/*hdr
**	FILE:		File.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		11/7/95
**	DESCRIPTION:	The new file subsystem
**
**	Copyright (C) 1995, Matthew Allen
**		fret@memecode.com
*/

#define DIR_PATH_SIZE	512

/****************************** Includes ************************************************************************************/
#include <winsock2.h>
#include <shlobj.h>
#include <fcntl.h>
#include <Userenv.h>

#include "LgiInc.h"
#include "LgiOsDefs.h"
#include "GVariant.h"

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
#include "GMutex.h"
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
char *ReadTextFile(const char *File)
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

int64 LgiFileSize(const char *FileName)
{
	int64 Size = 0;
	GDirectory Dir;
	if (Dir.First(FileName, 0))
	{
		Size = Dir.GetSize();
	}
	return Size;
}

typedef wchar_t char16;

bool FileExists(const char *Name, char *CorrectCase)
{
	bool Status = false;
	if (Name)
	{
		HANDLE hFind = INVALID_HANDLE_VALUE;
		
		GAutoWString n(LgiNewUtf8To16(Name));
		if (n)
		{
			WIN32_FIND_DATAW Info;
			hFind = FindFirstFileW(n, &Info);
			if (hFind != INVALID_HANDLE_VALUE)
				Status = StrcmpW(Info.cFileName, L".") != 0;
		}

		if (hFind != INVALID_HANDLE_VALUE)
		{
			FindClose(hFind);
			return true;
		}
	}

	return false;
}

bool DirExists(const char *Dir, char *CorrectCase)
{
	GAutoWString n(LgiNewUtf8To16(Dir));
	DWORD e = GetFileAttributesW(n);
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

bool ResolveShortcut(const char *LinkFile, char *Path, int Len) 
{
	bool Status = false;

	HWND hwnd = NULL;
	HRESULT hres;
	IShellLink* psl;
	char szGotPath[DIR_PATH_SIZE] = "";
	WIN32_FIND_DATA wfd;

	CoInitialize(0);
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**) &psl);

	if (SUCCEEDED(hres))
	{
		IPersistFile* ppf;

		hres = psl->QueryInterface(IID_IPersistFile, (void**) &ppf);
		if (SUCCEEDED(hres))
		{
			char16 wsz[DIR_PATH_SIZE];

			MultiByteToWideChar(CP_ACP, 0, LinkFile, -1, wsz, DIR_PATH_SIZE);

			char16 *l = StrrchrW(wsz, '.');
			if (l && StricmpW(l, L".lnk"))
			{
				StrcatW(wsz, L".lnk");
			}

			hres = ppf->Load(wsz, STGM_READ);
			if (SUCCEEDED(hres))
			{
				hres = psl->Resolve(hwnd, SLR_ANY_MATCH);
				if (SUCCEEDED(hres))
				{
					hres = psl->GetPath(szGotPath, DIR_PATH_SIZE, (WIN32_FIND_DATA *)&wfd, SLGP_SHORTPATH );

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

	return Status;
}

void WriteStr(GFile &f, const char *s)
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

int SizeofStr(const char *s)
{
	return sizeof(uint32) + ((s) ? strlen(s) : 0);
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
		GAutoWString w(LgiNewUtf8To16(Path));
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
		}
	}

	return Status;
}
 
/****************************** Classes ***************************************/
GVolume::GVolume()
{
	_Type = VT_NONE;
	_Flags = 0;
	_Size = 0;
	_Free = 0;
}

class GWin32Volume : public GVolume
{
	bool IsRoot;
	List<GVolume> _Sub;

public:
	GWin32Volume(LgiSystemPath Type, char *Name)
	{
		IsRoot = Type == LSP_ROOT;
		_Name = Name;
		_Type = VT_FOLDER;

		int Id = 0;
		switch (Type)
		{
			case LSP_HOME:
			{
				Id = CSIDL_PROFILE;
				break;
			}
			case LSP_DESKTOP:
			{
				_Type = VT_DESKTOP;
				Id = CSIDL_DESKTOPDIRECTORY;
				break;
			}
		}
		
		char p[DIR_PATH_SIZE];
		if (Id)
		{
			_Path = WinGetSpecialFolderPath(Id);
		}
		else
		{
			_Path = LgiGetSystemPath(Type);
		}
	}

	GWin32Volume(const char *Drive)
	{
		char VolName[DIR_PATH_SIZE], System[DIR_PATH_SIZE];
		DWORD MaxPath;

		IsRoot = false;
		int type = GetDriveType(Drive);
		if (type != DRIVE_UNKNOWN &&
			type != DRIVE_NO_ROOT_DIR)
		{
			char Buf[DIR_PATH_SIZE];
			const char *Desc = 0;
			switch (type)
			{
				case DRIVE_REMOVABLE:
				{
					_Type = VT_REMOVABLE;

					if (GetVolumeInformation(Drive, Buf, sizeof(Buf), 0, 0, 0, 0, 0) &&
						ValidStr(Buf))
					{
						Desc = Buf;
					}
					break;
				}
				case DRIVE_REMOTE:
					Desc = "Network";
					_Type = VT_NETWORK_SHARE;
					break;
				case DRIVE_CDROM:
					Desc = "Cdrom";
					_Type = VT_CDROM;
					break;
				case DRIVE_RAMDISK:
					Desc = "Ramdisk";
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

			if (Desc)
			{
				char s[DIR_PATH_SIZE];
				sprintf_s(s, sizeof(s), "%s (%.2s)", Desc, Drive);
				_Name = s;
			}
			
			_Path = Drive;
		}
	}

	~GWin32Volume()
	{
		_Sub.DeleteObjects();
	}

	bool IsMounted()
	{
		return false;
	}

	bool SetMounted(bool Mount)
	{
		return Mount;
	}

	void Insert(GAutoPtr<GVolume> v)
	{
		_Sub.Insert(v.Release());		
	}
	
	GVolume *First()
	{
		if (IsRoot)
		{
			// Get drive list
			IsRoot = false;
			
			char Str[512];
			if (GetLogicalDriveStrings(sizeof(Str), Str) > 0)
			{
				for (char *d = Str; *d; d += strlen(d) + 1)
				{
					GWin32Volume *v = new GWin32Volume(d);
					if (v)
					{
						if (v->Name())
							_Sub.Insert(v);
						else
							DeleteObj(v);
					}
				}
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
		if (_Path.Get())
		{
			if (Dir = new GDirectory)
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
		Root = new GWin32Volume(LSP_ROOT, "Computer");
		
		GAutoPtr<GVolume> v(new GWin32Volume(LSP_DESKTOP, "Desktop"));
		Root->Insert(v);
		v.Reset(new GWin32Volume(LSP_USER_DOWNLOADS, "Downloads"));
		Root->Insert(v);
	}

	return Root;
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

bool GFileSystem::Delete(GArray<const char*> &Files, GArray<int> *Status, bool ToTrash)
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

			GAutoWString n(LgiNewUtf8To16(Files[i]));
			if (n)
			{
				SetFileAttributesW(n, FILE_ATTRIBUTE_ARCHIVE);
				if (!::DeleteFileW(n))
				{
					e = GetLastError();
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

bool GFileSystem::Delete(const char *FileName, bool ToTrash)
{
	if (!FileName)
		return false;

	GArray<const char*> Files;
	Files.Add(FileName);
	return Delete(Files, 0, ToTrash);
}

bool GFileSystem::CreateFolder(const char *PathName, bool CreateParentFoldersIfNeeded)
{
	GAutoWString w(LgiNewUtf8To16(PathName));
	bool Status = ::CreateDirectoryW(w, NULL);
	
	if (!Status &&
		CreateParentFoldersIfNeeded)
	{
		DWORD err = GetLastError();
		if (err == ERROR_PATH_NOT_FOUND)
		{
			char Base[DIR_PATH_SIZE];
			strcpy_s(Base, sizeof(Base), PathName);
			do
			{
				char *Leaf = strrchr(Base, DIR_CHAR);
				if (!Leaf) return false;
				*Leaf = 0;
			}
			while (!DirExists(Base));
			
			GToken Parts(PathName + strlen(Base), DIR_STR);
			for (int i=0; i<Parts.Length(); i++)
			{
				LgiMakePath(Base, sizeof(Base), Base, Parts[i]);
				GAutoWString w(LgiNewUtf8To16(Base));
				Status = ::CreateDirectoryW(w, NULL);
				if (!Status)
					break;
			}
		}
	}	
	
	return Status;
}

bool GFileSystem::RemoveFolder(char *PathName, bool Recurse)
{
	if (Recurse)
	{
		GDirectory Dir;
		if (Dir.First(PathName))
		{
			do
			{
				char Str[DIR_PATH_SIZE];
				if (Dir.Path(Str, sizeof(Str)))
				{
					if (Dir.IsDir())
					{
						RemoveFolder(Str, Recurse);
					}
					else
					{
						Delete(Str, false);
					}
				}
			}
			while (Dir.Next());
		}
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

bool GFileSystem::SetCurrentFolder(char *PathName)
{
	bool Status = false;
	
	GAutoWString w(LgiNewUtf8To16(PathName));
	if (w)
		Status = ::SetCurrentDirectoryW(w);

	return Status;
}

bool GFileSystem::GetCurrentFolder(char *PathName, int Length)
{
	bool Status = false;

	GAutoWString w(new char16[DIR_PATH_SIZE+1]);
	if (w && ::GetCurrentDirectoryW(DIR_PATH_SIZE, w) > 0)
	{
		GAutoString s(LgiNewUtf16To8(w));
		if (s)
		{
			strcpy_s(PathName, Length, s);
			Status = true;
		}
	}

	return Status;
}

bool GFileSystem::Move(char *OldName, char *NewName)
{
	bool Status = false;

	if (OldName && NewName)
	{
		GAutoWString New(LgiNewUtf8To16(NewName));
		GAutoWString Old(LgiNewUtf8To16(OldName));
		if (New && Old)
			Status = ::MoveFileW(Old, New);
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
		uint32 Serial;

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
bool GDirectory::ConvertToTime(char *Str, int SLen, uint64 Time)
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
			sprintf_s(Str, SLen, "%i:%02.2i:%02.2i %c", Hour-1, s.wMinute, s.wSecond, AP);
		}
	}
	return false;
}

bool GDirectory::ConvertToDate(char *Str, int SLen, uint64 Time)
{
	if (Str)
	{
		FILETIME t;
		SYSTEMTIME s;
		t.dwHighDateTime = Time >> 32;
		t.dwLowDateTime = Time & 0xFFFFFFFF;
		if (FileTimeToSystemTime(&t, &s))
		{
			sprintf_s(Str, SLen, "%i/%i/%i", s.wDay, s.wMonth, s.wYear);
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Directory //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
struct GDirectoryPriv
{
	char			BasePath[DIR_PATH_SIZE];
	GAutoString     Utf;
	HANDLE			Handle;
	WIN32_FIND_DATAW Data;

	GDirectoryPriv()
	{
		Handle = INVALID_HANDLE_VALUE;
		BasePath[0] = 0;
	}

	~GDirectoryPriv()
	{
	}
};

GDirectory::GDirectory()
{
	d = new GDirectoryPriv;
}

GDirectory::~GDirectory()
{
	Close();
	DeleteObj(d);
}

GDirectory *GDirectory::Clone()
{
	return new GDirectory;
}

bool GDirectory::Path(char *s, int BufLen)
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

int GDirectory::First(const char *InName, const char *Pattern)
{
	Close();

    d->Utf.Reset();

	if (!InName)
		return false;

	char Name[DIR_PATH_SIZE];
	strcpy_s(Name, sizeof(Name), InName);

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

	// dir
	GAutoWString p(LgiNewUtf8To16(Name));
	if (p)
	{
		char16 w[DIR_PATH_SIZE];
		w[0] = 0;
		DWORD Chars = GetFullPathNameW(p, CountOf(w), w, NULL);
		if (Chars == 0)
		{
			DWORD e = GetLastError();
			StrcpyW(w, p);
		}

		GAutoString utf(LgiNewUtf16To8(w));
		if (utf)
			strcpy_s(d->BasePath, sizeof(d->BasePath), utf);
		else
			d->BasePath[0] = 0;
	}

	char Str[DIR_PATH_SIZE];
	if (Pattern)
	{
		if (!LgiMakePath(Str, sizeof(Str), d->BasePath, Pattern))
		{
			return false;
		}
	}
	else
	{
		strcpy_s(Str, sizeof(Str), d->BasePath);
	}

	GAutoWString s(LgiNewUtf8To16(Str));
	if (s)
		d->Handle = FindFirstFileW(s, &d->Data);

	if (d->Handle != INVALID_HANDLE_VALUE)
	{
		while (	stricmp(GetName(), ".") == 0 ||
				stricmp(GetName(), "..") == 0)
		{
			if (!Next()) return false;
		}
	}

	return d->Handle != INVALID_HANDLE_VALUE;
}

int GDirectory::Next()
{
	int Status = false;
	
    d->Utf.Reset();

	if (d->Handle != INVALID_HANDLE_VALUE)
	{
		Status = FindNextFileW(d->Handle, &d->Data);
	}

	return Status;
}

int GDirectory::Close()
{
	if (d->Handle != INVALID_HANDLE_VALUE)
	{
		FindClose(d->Handle);
		d->Handle = INVALID_HANDLE_VALUE;
	}

    d->Utf.Reset();

	return true;
}

int GDirectory::GetUser(bool Group)
{
	return 0;
}

bool GDirectory::IsReadOnly()
{
	return (d->Data.dwFileAttributes & FA_READONLY) != 0;
}

bool GDirectory::IsDir()
{
	return (d->Data.dwFileAttributes & FA_DIRECTORY) != 0;
}

bool GDirectory::IsSymLink()
{
	return (d->Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool GDirectory::IsHidden()
{
	return (d->Data.dwFileAttributes & FA_HIDDEN) != 0;
}

long GDirectory::GetAttributes()
{
	return d->Data.dwFileAttributes;
}

int GDirectory::GetType()
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

char *GDirectory::GetName()
{
	if (!d->Utf)
	{
	    d->Utf.Reset(LgiNewUtf16To8(d->Data.cFileName));
	}
	
	return d->Utf;
}

const uint64 GDirectory::GetCreationTime()
{
	return ((uint64) d->Data.ftCreationTime.dwHighDateTime) << 32 | d->Data.ftCreationTime.dwLowDateTime;
}

const uint64 GDirectory::GetLastAccessTime()
{
	return ((uint64) d->Data.ftLastAccessTime.dwHighDateTime) << 32 | d->Data.ftLastAccessTime.dwLowDateTime;
}

const uint64 GDirectory::GetLastWriteTime()
{
	return ((uint64) d->Data.ftLastWriteTime.dwHighDateTime) << 32 | d->Data.ftLastWriteTime.dwLowDateTime;
}

const uint64 GDirectory::GetSize()
{
	return ((uint64) d->Data.nFileSizeHigh) << 32 | d->Data.nFileSizeLow;
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

		if (File[0] == '/' && File[1] == '/')
		{
			// This hangs the process, so just bail safely.
			return false;
		}
		
		GAutoWString n(LgiNewUtf8To16(File));
		if (n)
		{
			d->hFile = CreateFileW(	n,
									Mode,
									FILE_SHARE_READ | (SharedAccess ? FILE_SHARE_WRITE : 0),
									0,
									(Mode & O_WRITE) ? OPEN_ALWAYS : OPEN_EXISTING,
									NoCache ? FILE_FLAG_NO_BUFFERING : 0,
									NULL);
		}

		if (!ValidHandle(d->hFile))
		{
			switch (d->LastError = GetLastError())
			{
				case ERROR_FILE_NOT_FOUND:
				case ERROR_PATH_NOT_FOUND:
				{
					// Path/File not found;
					int i=0;
					break;
				}
				case ERROR_ACCESS_DENIED:
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
	DWORD r = 0;
	if (!ReadFile(d->hFile, Buf, Size, &r, NULL) || r != Size)
	{
		d->LastError = GetLastError();
		return 0;
	}

	// Swap the bytes
	uchar *s = Buf, *e = Buf + Size - 1;
	while (s < e)
	{
		uchar t = *s;
		*s++ = *e;
		*e++ = t;
	}		
	return r;
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

