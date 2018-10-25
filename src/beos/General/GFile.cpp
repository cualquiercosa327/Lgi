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
#include <SymLink.h>
#include <Directory.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "GFile.h"
#include "GString.h"

/****************************** Defines *************************************************************************************/

#define FILEDEBUG

#define FLOPPY_360K				0x0001
#define FLOPPY_720K				0x0002
#define FLOPPY_1_2M				0x0004
#define FLOPPY_1_4M				0x0008
#define FLOPPY_5_25				(FLOPPY_360K | FLOPPY_1_2M)
#define FLOPPY_3_5				(FLOPPY_720K | FLOPPY_1_4M)

/****************************** Helper Functions ****************************************************************************/

extern void GetFlagsStr(char *flagstr, short flags);
extern int LeapYear(int year);

int SizeofStr(const char *s)
{
	return sizeof(uint32) + ((s) ? strlen(s) : 0);
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
	uint32 Len;
	f >> Len;
	char *s = 0;

	if (Len > 0)
	{
		s = new char[Len+1];
		if (s)
		{
			f.Read(s, Len);
			s[Len] = 0;
		}
		else
		{
			f.Seek(Len, SEEK_CUR);
		}
	}

	return s;
}

bool ResolveShortcut(const char *LinkFile, char *Path, int Length) 
{
	bool Status = FALSE;

	if (LinkFile && Path)
	{
		BSymLink Link(LinkFile);
		if (Link.InitCheck())
		{
			Link.ReadLink(Path, Length);
		}
		else
		{
			strcpy(Path, LinkFile);
		}
	}
	
	return Status;
}

int64 LgiFileSize(const char *FileName)
{
	BEntry e(FileName);
	off_t s = 0;
	e.GetSize(&s);
	return s;
}

bool FileExists(const char *FileName, char *CorrectCase)
{
	BEntry e(FileName);
	return e.Exists();
}

bool DirExists(const char *Dir, char *CorrectCase)
{
	BEntry e(Dir);
	return e.Exists();
}

bool TrimDir(char *Path)
{
	if (Path)
	{
		char *p = Path + (strlen(Path) - 1);
		if (*p == DIR_CHAR)
		{
			*p = 0;
			p--;
		}
		
		for (; p > Path; p--)
		{
			if (*p == DIR_CHAR)
			{
				*p = 0;
				return true;
			}
		}

		p[1] = 0;
		return true;
	}
	return false;
}

char *ReadTextFile(const char *File)
{
	if (File)
	{
		GFile f;
		if (f.Open(File, O_READ))
		{
			int Size = f.GetSize();
			char *m = new char[Size+1];
			if (m)
			{
				int r = f.Read(m, Size);
				if (r > 0)
				{
					m[r] = 0;
					return m;
				}
				DeleteArray(m);
			}
		}
	}
	
	return 0;
}

/****************************** Classes *************************************************************************************/
GFileSystem *GFileSystem::Instance = 0;
#include "GContainers.h"
#include "LgiCommon.h"
#include <Volume.h>
#include <Path.h>
#include <VolumeRoster.h>

GVolume::GVolume()
{
	_Name = 0;
	_Path = 0;
	_Type = VT_NONE;
	_Flags = 0;
	_Size = 0;
	_Free = 0;
}

class BeOSVol : public GVolume
{
	List<GVolume> c;

public:
	BeOSVol(BVolume *Vol = 0)
	{
		_Type = VT_HARDDISK;
		if (Vol)
		{
			_Size = Vol->Capacity();
			_Free = Vol->FreeBytes();
			
			BDirectory d;
			if (Vol->GetRootDirectory(&d) == B_OK)
			{
				BPath p(&d, ".", true);
				_Name = NewStr((char*)p.Path());
				_Path = NewStr((char*)p.Path());
			}
		}
		else
		{
			// Desktop...
			_Name = NewStr("Desktop");

			char d[256];
			if (LgiGetSystemPath(LSP_DESKTOP, d, sizeof(d)))
			{
				_Path = NewStr(d);
			}
			
			// Iterate Children
			BVolumeRoster r;
			BVolume v;
			while (r.GetNextVolume(&v) == B_NO_ERROR)
			{
				c.Insert(new BeOSVol(&v));
			}
		}
	}
	
	~BeOSVol()
	{
		c.DeleteObjects();
	}
	
	

	bool IsMounted()
	{
		return true;
	}
	
	bool SetMounted(bool Mount)
	{
		return false;
	}
	
	GVolume *First()
	{
		return c.First();
	}
	
	GVolume *Next()
	{
		return c.Next();
	}
	
	GDirectory *GetContents()
	{
		GDirectory *Dir = new GDirectory;
		if (_Path)
		{
			if (!Dir->First(_Path))
			{
				DeleteObj(Dir);
			}
		}
		return Dir;
	}
	
	void Insert(GAutoPtr<GVolume> v)
	{
		c.Insert(v.Release());
	}
};

class GFileSystemPrivate
{
public:
	GVolume *RootVol;
	char *CurDir;
	
	GFileSystemPrivate()
	{
		RootVol = 0;
		CurDir = 0;

		char e[256];
		if (LgiGetExePath(e, sizeof(e)))
		{
			CurDir = NewStr(e);
		}
	}
	
	~GFileSystemPrivate()
	{
		DeleteObj(RootVol);
		DeleteArray(CurDir);
	}
};

GFileSystem::GFileSystem()
{
	Instance = this;
	d = new GFileSystemPrivate;
}

GFileSystem::~GFileSystem()
{
	DeleteObj(d);
}

GVolume *GFileSystem::GetRootVolume()
{
	if (!d->RootVol)
	{
		d->RootVol = new BeOSVol;
	}

	return d->RootVol;
}

bool GFileSystem::Copy(char *From, char *To, int *Status, CopyFileCallback Callback, void *Token)
{
	LgiAssert(0);
	return false;
}
	
bool GFileSystem::Delete(const char *FileName, bool ToTrash)
{
	if (FileName)
	{
		return unlink(FileName) == 0;
	}
	return false;
}

bool GFileSystem::Move(char *OldName, char *NewName)
{
	if (!ValidStr(OldName) ||
		!ValidStr(NewName))
	{
		LgiTrace("%s:%i - Invalid params to move file.\n", _FL);
		return false;
	}
	
	int r = rename(OldName, NewName);
	if (r == 0)
		return true;
	
	LgiTrace("%s:%i - rename(%s, %s) failed with %i\n", _FL, OldName, NewName, errno);
	return false;
}

bool GFileSystem::CreateFolder(const char *PathName, bool CreateParentFoldersIfNeeded)
{
	BDirectory Old;
	status_t Status = Old.CreateDirectory(PathName, &Old);
	return (Status == B_OK) || (Status == B_FILE_EXISTS);
}

bool GFileSystem::RemoveFolder(char *PathName, bool Recurse)
{
	bool Status = FALSE;
	BEntry Entry(PathName);
	if (Entry.Exists())
	{
		Status = (Entry.Remove() == B_NO_ERROR);
	}
	return Status;
}

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
	strupr(Name);
	strupr(Mask);

	while (*Name && *Mask)
	{
		if (*Mask == '*')
		{
			if (*Name == *(Mask+1))
			{
				Mask++;
			}
			else
			{
				Name++;
			}
		}
		else if (*Mask == '?' || *Mask == *Name)
		{
			Mask++;
			Name++;
		}
		else
		{
			return FALSE;
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

/////////////////////////////////////////////////////////////////////////////////
bool GDirectory::ConvertToTime(char *Str, int StrSize, uint64 Time)
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, StrSize, "%I:%M:%S", t);
		return true;
	}
	return false;
}

bool GDirectory::ConvertToDate(char *Str, int StrSize, uint64 Time)
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, StrSize, "%d/%m/%y", t);
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Directory //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class GDirectoryPriv
{
public:
	char *BasePath;
	BDirectory *Dir;
	BEntry Entry;								
	struct stat Stat;

	GDirectoryPriv()
	{
		Dir = 0;
		BasePath = 0;
	}
	
	~GDirectoryPriv()
	{
		DeleteArray(BasePath);
		DeleteObj(Dir);
	}
};

GDirectory::GDirectory()
{
	d = new GDirectoryPriv;
}

GDirectory::~GDirectory()
{
	DeleteObj(d);
}

bool GDirectory::IsReadOnly()
{
	return (GetAttributes() & S_IWUSR) == 0;
}

GDirectory *GDirectory::Clone()
{
	return new GDirectory;
}

int GDirectory::GetUser(bool Group)
{
	return 0;
}

int GDirectory::GetType()
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

bool GDirectory::Path(char *s, int BufLen)
{
	if (!s)
	{
		return false;
	}
	
	if (BufLen > 0)
	{
		if (!GetName() ||
			(strlen(d->BasePath) + strlen(GetName()) + 2 > BufLen) )
		{
			return false;
		}
	}

	LgiMakePath(s, BufLen, d->BasePath, GetName());
	return true;
}

int GDirectory::First(const char *Path, const char *Pattern)
{
	bool Status = false;
	if (Path && Pattern)
	{
		DeleteArray(d->BasePath);
		d->BasePath = NewStr(Path);
		
		if (!d->Dir)
		{
			d->Dir = new BDirectory(Path);
		}
		else
		{
			d->Dir->Rewind();
		}
		if (d->Dir)
		{
			Status = d->Dir->GetNextEntry(&d->Entry) == B_OK;
			if (Status)
			{
				d->Entry.GetStat(&d->Stat);
			}
		}
	}
	return Status;
}

int GDirectory::Next()
{
	bool Status = false;
	if (d->Dir)
	{
		Status = (d->Dir->GetNextEntry(&d->Entry) == B_OK);
		if (Status)
		{
			d->Entry.GetStat(&d->Stat);
		}
	}
	return Status;
}

int GDirectory::Close()
{
	DeleteObj(d->Dir);
	return true;
}

bool GDirectory::IsSymLink()
{
	return d->Entry.IsSymLink();
}

bool GDirectory::IsDir()
{
	return d->Entry.IsDirectory();
}

bool GDirectory::IsHidden()
{
	return false;
}

long GDirectory::GetAttributes()
{
	if (d->Dir)
	{
		return d->Stat.st_mode;
	}
	return 0;
}

char *GDirectory::GetName()
{
	static char Name[MAX_PATH];
	Name[0] = 0;
	
	if (d->Dir)
	{
		d->Entry.GetName(Name);
	}
	
	return Name;
}

const uint64 GDirectory::GetCreationTime()
{
	time_t t;
	if (d->Entry.GetCreationTime(&t) == B_OK)
	{
		return t;
	}
	
	return 0;
}

const uint64 GDirectory::GetLastAccessTime()
{
	time_t t;
	if (d->Entry.GetAccessTime(&t) == B_OK)
	{
		return t;
	}
	
	return 0;
}

const uint64 GDirectory::GetLastWriteTime()
{
	time_t t;
	if (d->Entry.GetModificationTime(&t) == B_OK)
	{
		return t;
	}
	
	return 0;
}

const uint64 GDirectory::GetSize()
{
	if (d->Dir)
	{
		return d->Stat.st_size;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// File ///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class GFilePrivate : public BFile
{
public:
	char *Name;
	int Status;
	bool Swap;
	int Mode;

	GFilePrivate()
	{
		Name = 0;
		Mode = 0;
		Status = true;
		Swap = false;
	}
	
	~GFilePrivate()
	{
		DeleteObj(Name);
	}
};

GFile::GFile()
{
	d = new GFilePrivate;
}

GFile::~GFile()
{
	Close();
	DeleteObj(d);
}

void GFile::SetSwap(bool s)
{
	d->Swap = s;
}

bool GFile::GetSwap()
{
	return d->Swap;
}

char *GFile::GetName()
{
	return d->Name;
}

int GFile::GetOpenMode()
{
	return d->Mode;
}

int GFile::Open(const char *File, int Mode)
{
	int Status = false;

	if (File)
	{
		int Other = 0;
		
		if (Mode == O_WRITE || Mode == O_READWRITE)
		{
			Other |= B_CREATE_FILE;
		}
		
		Close();
		if (d->SetTo(File, Mode | Other) == B_NO_ERROR)
		{
			d->Mode = Mode;
			d->Name = NewStr(File);
			Status = true;
		}
	}

	return Status;
}

bool GFile::IsOpen()
{
	return d->InitCheck() == B_NO_ERROR;
}

int GFile::Close()
{
	int Status = false;

	if (IsOpen())
	{
		d->Unset();
		Status = true;
	}
	DeleteArray(d->Name);

	return Status;
}

/*
int GFile::Print(char *Format, ...)
{
	int Chars = 0;

	if (Format)
	{
		char Buffer[1024];
		va_list Arg;

		va_start(Arg, Format);
		Chars = vsprintf(Buffer, Format, Arg);
		va_end(Arg);

		if (Chars > 0)
		{
			Write(Buffer, Chars);
		}
	}

	return Chars;
}
*/

#define CHUNK		0xFFF0

int GFile::Read(void *Buffer, int Size, int Flags)
{
	return d->Read(Buffer, Size);
}

int GFile::Write(const void *Buffer, int Size, int Flags)
{
	return d->Write(Buffer, Size);
}

int64 GFile::Seek(int64 To, int Whence)
{
	int64 p;
	switch (Whence)
	{
		default:
		case SEEK_SET:
			p = To;
			break;
		case SEEK_CUR:
			p = GetPos() + To;
			break;
		case SEEK_END:
			p = GetSize() +  To;
			break;
	}
	return SetPos(p);
}

int64 GFile::SetPos(int64 Pos)
{
	return d->Seek(Pos, SEEK_SET);
}

int64 GFile::GetPos()
{
	return d->Position();
}

int64 GFile::GetSize()
{
	off_t Size = 0;
	d->GetSize(&Size);
	return Size;
}

int64 GFile::SetSize(int64 Size)
{
	d->SetSize(Size);
	return GetSize();
}

bool GFile::Eof()
{
	return GetPos() >= GetSize();
}

int GFile::SwapRead(uchar *Buf, int Size)
{
	int i = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		i += d->Read(Buf--, 1);
	}
	return i;
}

int GFile::SwapWrite(uchar *Buf, int Size)
{
	int i = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		i += d->Write(Buf--, 1);
	}
	return i;
}

int GFile::ReadStr(char *Buf, int Size)
{
	int i = 0;
	if (Buf && Size > 0)
	{
		char c;

		Size--;

		do
		{
			i += d->Read((uchar*) &c, 1);
			if (Eof())
			{
				break;
			}
			
			*Buf++ = c;
		
		} while (i < Size - 1 && c != '\n');

		*Buf = 0;
	}

	return i;
}

int GFile::WriteStr(char *Buf, int Size)
{
	int i = 0;

	while (i <= Size)
	{
		i += d->Write((uchar*) Buf, 1);
		Buf++;
		if (!*Buf || *Buf == '\n') break;
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

#define RdIO { d->Status |= ((d->Swap) ? SwapRead((uchar*) &i, sizeof(i)) : d->Read((uchar*) &i, sizeof(i))) != sizeof(i); return *this; }
#define WrIO { d->Status |= ((d->Swap) ? SwapWrite((uchar*) &i, sizeof(i)) : d->Write((uchar*) &i, sizeof(i))) != sizeof(i); return *this; }

GFile &GFile::operator >> (char &i) RdIO
GFile &GFile::operator >> (int8 &i) RdIO
GFile &GFile::operator >> (uint8 &i) RdIO
GFile &GFile::operator >> (int16 &i) RdIO
GFile &GFile::operator >> (uint16 &i) RdIO
GFile &GFile::operator >> (signed int &i) RdIO
GFile &GFile::operator >> (unsigned int &i) RdIO
GFile &GFile::operator >> (signed long &i) RdIO
GFile &GFile::operator >> (unsigned long &i) RdIO
GFile &GFile::operator >> (double &i) RdIO
GFile &GFile::operator >> (int64 &i) RdIO
GFile &GFile::operator >> (uint64 &i) RdIO

GFile &GFile::operator << (char i) WrIO
GFile &GFile::operator << (int8 i) WrIO
GFile &GFile::operator << (uint8 i) WrIO
GFile &GFile::operator << (int16 i) WrIO
GFile &GFile::operator << (uint16 i) WrIO
GFile &GFile::operator << (signed int i) WrIO
GFile &GFile::operator << (unsigned int i) WrIO
GFile &GFile::operator << (signed long i) WrIO
GFile &GFile::operator << (unsigned long i) WrIO
GFile &GFile::operator << (double i) WrIO
GFile &GFile::operator << (int64 i) WrIO
GFile &GFile::operator << (uint64 i) WrIO

int GFile::GetError()
{
	LgiAssert(!"Impl me.");
	return 0;
}

const char *GetErrorName(int e)
{
	LgiAssert(!"Not impl.");
	return 0;
}

bool LgiGetDriveInfo(char *Path, uint64 *Free, uint64 *Size, uint64 *Available)
{
	LgiAssert(!"Not impl.");
	return false;
}
