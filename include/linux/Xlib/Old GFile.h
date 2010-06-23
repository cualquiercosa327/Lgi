/*
**	FILE:			File.h
**	AUTHOR:			Matthew Allen
**	DATE:			11/7/95
**	DESCRIPTION:	File subsystem header
**
**
**	Copyright (C) 1995, Matthew Allen
**		fret@memecode.com
*/

#ifndef __FILE_H
#define __FILE_H

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "LibDefs.h"
#include "LgiOsDefs.h"
#include "GMem.h"

#define MAX_PATH					256

#define O_READ						O_RDONLY
#define O_WRITE						O_WRONLY
#define O_READWRITE					O_RDWR

#define ValidHandle(h)				(h >= 0)
#define INVALID_HANDLE				-1

// Time/Date related functions infomation
#define DECNANOPERSEC				10000000
#define SECPERMINUTE				60
#define SECPERHOUR					(60 * SECPERMINUTE)
#define SECPERDAY					(24 * SECPERHOUR)

// File attributes
#define FA_NORMAL					0x0000
#define FA_READONLY					0x0001
#define FA_HIDDEN					0x0002
#define FA_SYSTEM					0x0004
#define FA_VOLUME					0x0008
#define FA_DIRECTORY				0x0010
#define FA_ARCHIVE					0x0020

class NodeInfo {
protected:
	char *Name;
	int Attributes;
	quad Size;

public:
	NodeInfo();
	NodeInfo(char *n, int a, quad s);
	virtual ~NodeInfo();

	virtual int operator ^(NodeInfo &N);
	char *GetName() { return Name; }
	int GetAttributes() { return Attributes; }
	quad GetSize() { return Size; }
};

class VDirectory {
public:
	VDirectory() { }
	virtual ~VDirectory() { Close(); }
	virtual void GetDirType(char *Str, int Len) {}
	virtual bool First(char *Name, char Must, char Search) { return true; }
	virtual bool Next() { return true; }
	virtual bool Close() { return true; }

	virtual long GetAttributes() { return 0; }
	virtual bool IsDir() { return false; }
	virtual char *GetName() { return NULL; }
	virtual void GetName(char *Buf, int Len) { }
	virtual const quad GetCreationTime() { return 0; }
	virtual const quad GetLastAccessTime() { return 0; }
	virtual const quad GetLastWriteTime() { return 0; }
	virtual const quad GetSize() { return 0; }

	bool ConvertToTime(char *Str, quad Time);
	bool ConvertToDate(char *Str, quad Time);
};

class VDirView : public VDirectory {

	int Items;
	int Files;
	int Dirs;
	char CurrentDir[MAX_PATH];

	class Node : public NodeInfo {

		Node *Left, *Right;

	public:
		Node();
		Node(char *n, int a, quad s);
		~Node();

		Node **Traverse(Node **ppNode, int Type);
		void Add(Node *Info);

	} *Root, **Index;

public:
	VDirView();
	~VDirView();

	bool Read(char *Dir);

	int GetFiles() { return Files; }
	NodeInfo *File(int i)
	{
		if (Index) return (NodeInfo*) Index[Dirs + i];
		return 0;
	}

	int GetDirs() { return Dirs; }
	NodeInfo *Dir(int i)
	{
		if (Index) return (NodeInfo*) Index[i];
		return 0;
	}

	int GetItems() { return Items; }
	NodeInfo *operator [](int i)
	{
		if (Index) return (NodeInfo*) Index[i];
		return 0;
	}
};

class VLinuxDir : public VDirView {

	DIR *Dir;
	char *Path;

	struct dirent *De;
	struct stat Stat;

public:
	VLinuxDir();
	~VLinuxDir();

	bool First(char *Name, char Must, char Search);
	bool Next();
	bool Close();

	long GetAttributes();
	bool IsDir();
	char *GetName();
	void GetName(char *Buf, int Len);
	const quad GetCreationTime();
	const quad GetLastAccessTime();
	const quad GetLastWriteTime();
	const quad GetSize();
};

// classes
class VolumeInfo {
public:
	char		Drive;
	char		Name[32];
	char		System[32];
	ulong		MaxPath;
	ulong		Flags;
};

#define DIF_NONE					0
#define DIF_3_5FLOPPY					1
#define DIF_5_25FLOPPY					2
#define DIF_HARDDISK					3
#define DIF_CDROM					4
#define DIF_NET						5
#define DIF_RAM						6
#define DIF_REMOVABLE					7
#define DIF_MAX						9

class DriveInfo {
public:
	int		Type;
	char		Name[64];
	long		TotalSpace;
	long		FreeSpace;
};

#define FileDev			(FileDevice::GetInstance())

class FileDevice
{
	friend class GFile;

	char CurDir[256];
	int NumDrive;
	DriveInfo *DriveList;
	int FillDiskList();

	static FileDevice *Instance;

public:
	FileDevice();
	virtual ~FileDevice();
	static FileDevice *GetInstance() { return Instance; }

	int GetNumDrives() { return NumDrive; }
	DriveInfo *GetDriveInfo(int Drive);
	VDirectory *GetDir(char *DirName);

	bool DeleteFile(char *FileName);
	bool CreateDirectory(char *PathName);
	bool RemoveDirectory(char *PathName);
	bool SetCurrentDirectory(char *PathName);
	bool GetCurrentDirectory(char *PathName, int Length);
	bool MoveFile(char *OldName, char *NewName);
	bool GetVolumeInfomation(char Drive, VolumeInfo *pInfo);
};

class GFile {
protected:
	char	*Name;
	bool	Swap;
	bool	Status;
	int		Pos;
	int		Handle;
	int		Attributes;

	uchar *Buffer;
	int BufferSize;
	int Start, End;

	int BufferedRead(uchar *Buf, int Size);
	int BufferedWrite(uchar *Buf, int Size);

	int SwapRead(uchar *Buf, int Size);
	int SwapWrite(uchar *Buf, int Size);

public:
	GFile();
	virtual ~GFile();

	virtual bool Open(char *Name, int Attrib);
	virtual bool Open();
	virtual bool Close();

	virtual int Read(void *Buffer, int Size);
	virtual int Write(void *Buffer, int Size);
	virtual int Seek(int To, int Whence);
	virtual int GetPosition();
	virtual int GetSize();
	virtual bool SetSize(int Size);
	virtual bool Eof();
	void SetStatus(bool s);
	bool GetStatus();

	// swap bytes on variable io
	void SetSwap(bool s) { Swap = s; }
	bool GetSwap() { return Swap; }

	int ReadStr(char *Buf, int Size);
	int WriteStr(char *Buf, int Size);

	int Print(char *Format, ...);

	// Read
	virtual GFile &operator >> (char		&i);
	virtual GFile &operator >> (signed char		&i);
	virtual GFile &operator >> (unsigned char	&i);
	virtual GFile &operator >> (signed short	&i);
	virtual GFile &operator >> (unsigned short	&i);
	virtual GFile &operator >> (signed int		&i);
	virtual GFile &operator >> (unsigned int	&i);
	virtual GFile &operator >> (signed long		&i);
	virtual GFile &operator >> (unsigned long	&i);
	virtual GFile &operator >> (float		&i);
	virtual GFile &operator >> (double		&i);
	virtual GFile &operator >> (long double		&i);
	virtual GFile &operator >> (quad		&i);

	// Write
	virtual GFile &operator << (char		i);
	virtual GFile &operator << (signed char		i);
	virtual GFile &operator << (unsigned char	i);
	virtual GFile &operator << (signed short	i);
	virtual GFile &operator << (unsigned short	i);
	virtual GFile &operator << (signed int		i);
	virtual GFile &operator << (unsigned int	i);
	virtual GFile &operator << (signed long		i);
	virtual GFile &operator << (unsigned long	i);
	virtual GFile &operator << (float		i);
	virtual GFile &operator << (double		i);
	virtual GFile &operator << (long double		i);
	virtual GFile &operator << (quad		i);
};

// C functions
extern bool Match(char *Name, char *Mask);
extern bool ResolveShortcut(char *LinkFile, char *Path, int Length);
extern int FileSize(char *FileName);
extern bool FileExists(char *Name);
extern int SizeofStr(char *s);
extern void WriteStr(GFile &f, char *s);
extern char *ReadStr(GFile &f);
extern bool TrimDir(char *Path);

#endif
