/**
	\file
	\author Matthew Allen
	\date 24/5/2002
	\brief Common file system header
	Copyright (C) 1995-2002, <a href="mailto:fret@memecode.com">Matthew Allen</a>
*/

#ifndef __FILE_H
#define __FILE_H

#include <fcntl.h>

#include "GMem.h"
#include "GStream.h"
#include "GArray.h"
#include "GRefCount.h"
#include "GStringClass.h"

#ifdef WIN32

// #include <dos.h>
// #include <sys\types.h>
// #include <sys\stat.h>
// #include <io.h>

typedef HANDLE							OsFile;
#define INVALID_HANDLE					INVALID_HANDLE_VALUE
#define ValidHandle(hnd)				((hnd) != INVALID_HANDLE_VALUE)

#define O_READ							GENERIC_READ
#define O_WRITE							GENERIC_WRITE
#define O_READWRITE						(GENERIC_READ | GENERIC_WRITE)
#define O_SHARE							0x01000000
#define O_NO_CACHE						0x00800000

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

typedef int								OsFile;
#define INVALID_HANDLE					-1
#define ValidHandle(hnd)				((hnd) >= 0)

#define O_READ							O_RDONLY
#define O_WRITE							O_WRONLY
#ifdef MAC
#define O_SHARE							O_SHLOCK
#else
#define O_SHARE							0
#endif
#define O_READWRITE						O_RDWR

#endif

/////////////////////////////////////////////////////////////////////
// Defines
#define FileDev							(GFileSystem::GetInstance())
#define MAX_PATH						260

// File system types (used by GDirectory and GVolume)
#define VT_NONE							0
#define VT_3_5FLOPPY					1
#define VT_5_25FLOPPY					2
#define VT_HARDDISK						3
#define VT_CDROM						4
#define VT_RAMDISK						5
#define VT_REMOVABLE					6
#define VT_FOLDER						7
#define VT_FILE							8
#define VT_DESKTOP						9
#define VT_NETWORK_NEIGHBOURHOOD		10
#define VT_NETWORK_MACHINE				11
#define VT_NETWORK_SHARE				12
#define VT_NETWORK_PRINTER				13
#define VT_NETWORK_GROUP				14	 // e.g. workgroup
#define VT_MAX							15

// Volume attributes
#define VA_CASE_SENSITIVE				0x0001
#define VA_CASE_PRESERVED				0x0002
#define VA_UNICODE_ON_DISK				0x0004
#define VA_LFN_API						0x4000
#define VA_COMPRESSED					0x8000

// File attributes
#define FA_NORMAL						0x0000
#define FA_READONLY						0x0001
#define FA_HIDDEN						0x0002
#define FA_SYSTEM						0x0004
#define FA_VOLUME						0x0008
#define FA_DIRECTORY					0x0010
#define FA_ARCHIVE						0x0020
#define FA_COMPUTER						0x0040

/////////////////////////////////////////////////////////////////////
// Abstract classes

/// Generic directory iterator
class LgiClass GDirectory
{
	struct GDirectoryPriv *d;

public:
	GDirectory();
	virtual ~GDirectory();

	/// \brief Starts the search. The entries '.' and '..' are never returned.
	/// The default pattern returns all files.
	/// \return Non zero on success
	virtual int First
	(
		/// The path of the directory
		const char *Name,
		/// The pattern to match files against.
		/// \sa The default LGI_ALL_FILES matchs all files.
		const char *Pattern = LGI_ALL_FILES
	);
	
	/// \brief Get the next match
	/// \return Non zero on success
	virtual int Next();

	/// \brief Finish the search
	/// \return Non zero on success
	virtual int Close();

	/// \brief Constructs the full path of the current directory entry
	/// \return Non zero on success
	virtual bool Path
	(
		// The buffer to write to
		char *s,
		// The size of the output buffer in bytes
		int BufSize
	);

	/// Gets the current entries attributes (platform specific)
	virtual long GetAttributes();
	
	/// Gets the name of the current entry. (Doesn't include the path).
	virtual char *GetName();
	
	/// Gets the user id of the current entry. (Doesn't have any meaning on Win32).
	virtual int GetUser
	(
		/// If true gets the group id instead of the user id.
		bool Group
	);
	
	/// Gets the entries creation time. You can convert this to an easy to read for using GDateTime.
	virtual const uint64 GetCreationTime();

	/// Gets the entries last access time. You can convert this to an easy to read for using GDateTime.
	virtual const uint64 GetLastAccessTime();

	/// Gets the entries last modified time.  You can convert this to an easy to read for using GDateTime.
	virtual const uint64 GetLastWriteTime();

	/// Returns the size of the entry.
	virtual const uint64 GetSize();

	/// Returns true if the entry is a sub-directory.
	virtual bool IsDir();
	
	/// Returns true if the entry is a symbolic link.
	virtual bool IsSymLink();
	
	/// Returns true if the entry is read only.
	virtual bool IsReadOnly();

	/// \brief Returns true if the entry is hidden.
	/// This is equivilant to a attribute flag on win32 and a leading '.' on unix.
	virtual bool IsHidden();

	/// Creates an copy of this type of GDirectory class.
	virtual GDirectory *Clone();
	
	/// Gets the type code of the current entry. See the VT_?? defines for possible values.
	virtual int GetType();

	/// Converts a string to the 64-bit value returned from the date functions.
	bool ConvertToTime(char *Str, int SLen, uint64 Time);

	/// Converts the 64-bit value returned from the date functions to a string.
	bool ConvertToDate(char *Str, int SLen, uint64 Time);
};

/// Describes a volume connected to the system
class LgiClass GVolume
{
	friend class GFileSystem;

protected:
	GString _Name;
	GString _Path;
	int _Type;			// VT_??
	int _Flags;			// VA_??
	int64 _Size;
	int64 _Free;

public:
	GVolume();
	virtual ~GVolume() {}

	char *Name() { return _Name; }
	char *Path() { return _Path; }
	int Type() { return _Type; } // VT_??
	int Flags() { return _Flags; }
	uint64 Size() { return _Size; }
	uint64 Free() { return _Free; }

	virtual bool IsMounted() = 0;
	virtual bool SetMounted(bool Mount) = 0;
	virtual GVolume *First() = 0;
	virtual GVolume *Next() = 0;
	virtual GDirectory *GetContents() = 0;
	virtual void Insert(GAutoPtr<GVolume> v) = 0;
};

typedef int (*CopyFileCallback)(void *token, int64 Done, int64 Total);

/// A singleton class for accessing the file system
class LgiClass GFileSystem
{
	friend class GFile;
	static GFileSystem *Instance;
	class GFileSystemPrivate *d;

	GVolume *Root;

public:
	GFileSystem();
	~GFileSystem();
	
	/// Return the current instance of the file system. The shorthand for this is "FileDev".
	static GFileSystem *GetInstance() { return Instance; }

	/// Call this when the devices on the system change. For instance on windows
	/// when you receive WM_DEVICECHANGE.
	void OnDeviceChange(char *Reserved = 0);

	/// Gets the root volume of the system.
	GVolume *GetRootVolume();
	
	/// Copies a file
	bool Copy
	(
		/// The file to copy from...
		char *From,
		/// The file to copy to. Any existing file there will be overwritten without warning.
		char *To,
		/// The error code or zero on success
		int *Status = 0,
		/// Optional callback when some data is copied.
		CopyFileCallback Callback = 0,
		/// A user defined token passed to the callback function
		void *Token = 0
	);

	/// Delete file
	bool Delete(const char *FileName, bool ToTrash = true);
	/// Delete files
	bool Delete
	(
		/// The list of files to delete
		GArray<const char*> &Files,
		/// A list of status codes where 0 means success and non-zero is an error code, usually an OS error code. NULL if not required.
		GArray<int> *Status = 0,
		/// true if you want the files moved to the trash folder, false if you want them deleted directly
		bool ToTrash = true
	);
	
	/// Create a directory
	bool CreateFolder(const char *PathName, bool CreateParentFoldersIfNeeded = false);
	
	/// Remove's a directory	
	bool RemoveFolder
	(
		/// The path to remove
		const char *PathName,
		/// True if you want this function to recursively delete all contents of the path passing in.
		bool Recurse = false
	);
	
	bool SetCurrentFolder(char *PathName);
	bool GetCurrentFolder(char *PathName, int Length);

	/// Moves a file to a new location. Only works on the same device.
	bool Move(const char *OldName, const char *NewName);
};

#ifdef BEOS

#define GFileOps()						\
	GFilePre char GFilePost;			\
	GFilePre int8 GFilePost;			\
	GFilePre uint8 GFilePost;			\
	GFilePre int16 GFilePost;			\
	GFilePre uint16 GFilePost;			\
	GFilePre signed int GFilePost;		\
	GFilePre unsigned int GFilePost;	\
	GFilePre signed long GFilePost;		\
	GFilePre unsigned long GFilePost;	\
	GFilePre int64 GFilePost;			\
	GFilePre uint64 GFilePost;			\
	GFilePre double GFilePost

#else

#define GFileOps()						\
	GFilePre char GFilePost;			\
	GFilePre int8 GFilePost;			\
	GFilePre uint8 GFilePost;			\
	GFilePre int16 GFilePost;			\
	GFilePre uint16 GFilePost;			\
	GFilePre signed int GFilePost;		\
	GFilePre unsigned int GFilePost;	\
	GFilePre signed long GFilePost;		\
	GFilePre unsigned long GFilePost;	\
	GFilePre int64 GFilePost;			\
	GFilePre uint64 GFilePost;			\
	GFilePre float GFilePost;			\
	GFilePre double GFilePost

#endif

/// Generic file access class
class LgiClass GFile : public GStream, public GRefCount
{
protected:
	class GFilePrivate *d;

	int SwapRead(uchar *Buf, int Size);
	int SwapWrite(uchar *Buf, int Size);

public:
	GFile();
	virtual ~GFile();

	OsFile Handle();

	/// \brief Opens a file
	/// \return Non zero on success
	int Open
	(
		/// The path of the file to open
		const char *Name,
		/// The mode to open the file with. One of O_READ, O_WRITE or O_READWRITE.
		int Attrib
	);
	
	/// Returns non zero if the class is associated with an open file handle.
	bool IsOpen();

	/// Returns the most recent error code encountered.
	int GetError();
	
	/// Closes the file.
	int Close();
	
	/// Gets the mode that the file was opened with.
	int GetOpenMode();

	/// Gets the block size
	int GetBlockSize();

	/// \brief Gets the current file pointer.
	/// \return The file pointer or -1 on error.
	int64 GetPos();
	
	/// \brief Sets the current file pointer.
	/// \return The new file pointer or -1 on error.
	int64 SetPos(int64 Pos);

	/// \brief Gets the file size.
	/// \return The file size or -1 on error.
	int64 GetSize();

	/// \brief Sets the file size.
	/// \return The new file size or -1 on error.
	int64 SetSize(int64 Size);

	/// \brief Reads bytes into memory from the current file pointer.
	/// \return The number of bytes read or <= 0.
	int Read(void *Buffer, int Size, int Flags = 0);
	
	/// \brief Writes bytes from memory to the current file pointer.
	/// \return The number of bytes written or <= 0.
	int Write(const void *Buffer, int Size, int Flags = 0);

	/// Gets the path used to open the file
	virtual char *GetName();
	
	/// Moves the current file pointer.
	virtual int64 Seek(int64 To, int Whence);
	
	/// Returns true if the current file pointer is at the end of the file.
	virtual bool Eof();

	/// Resets the status value.
	virtual void SetStatus(bool s = false);
	
	/// \brief Returns true if all operations were successful since the file was openned or SetStatus
	/// was used to reset the file's status.
	virtual bool GetStatus();

	/// \brief Sets the swap option. When switched on all integer reads/writes will have their bytes 
	/// swaped.
	virtual void SetSwap(bool s);
	
	/// Gets the current swap setting.
	virtual bool GetSwap();

	// String
	virtual int ReadStr(char *Buf, int Size);
	virtual int WriteStr(char *Buf, int Size);

	// Operators
	#define GFilePre		virtual GFile &operator >> (
	#define GFilePost		&i)
	GFileOps();
	#undef GFilePre
	#undef GFilePost

	#define GFilePre		virtual GFile &operator << (
	#define GFilePost		i)
	GFileOps();
	#undef GFilePre
	#undef GFilePost

	// GDom impl
	bool GetVariant(const char *Name, GVariant &Value, char *Array = NULL);
	bool SetVariant(const char *Name, GVariant &Value, char *Array = NULL);
	bool CallMethod(const char *Name, GVariant *ReturnValue, GArray<GVariant*> &Args);

	// Path handling
	class LgiClass Path : public GString::Array
	{
		GString Full;
		
	public:
		Path(const char *init = NULL)
		{
			SetFixedLength(false);
			if (init)
				*this = init;
		}
		
		Path(LgiSystemPath Which)
		{
			SetFixedLength(false);
			*this = GetSystem(Which);
		}

		Path &operator =(const char *p)
		{
			GString s(p);
			*((GString::Array*)this) = s.SplitDelimit("\\/");
			SetFixedLength(false);
			return *this;
		}

		Path &operator =(const GString &s)
		{
			*((GString::Array*)this) = s.SplitDelimit("\\/");
			SetFixedLength(false);
			return *this;
		}
		
		Path &operator =(const Path &p)
		{
			*((GString::Array*)this) = p;
			Full = p.Full;
			SetFixedLength(false);
			return *this;
		}
		
		Path &operator +=(const GString &p)
		{
			GString::Array a = p.SplitDelimit("\\/");
			SetFixedLength(false);
			for (unsigned i=0; i<a.Length(); i++)
			{
				GString &s = a[i];
				if (!_stricmp(s, "."))
					;
				else if (!_stricmp(s, ".."))
					Parent();
				else
					New() = s;
			}
			return *this;
		}
		
		Path &Parent()
		{
			if (Length() > 0)
				Length(Length()-1);
			return *this;
		}
		
		operator const char *()
		{
			GString Sep(DIR_STR);
			#ifdef WINDOWS
			Full = Sep.Join(*this);
			#else
			Full = Sep + Sep.Join(*this);
			#endif
			return Full;
		}

		GString GetFull()
		{
			GString Sep(DIR_STR);
			#ifdef WINDOWS
			Full = Sep.Join(*this);
			#else
			Full = Sep + Sep.Join(*this);
			#endif
			return Full;
		}

		bool IsFile();
		bool IsFolder();
		static GString GetSystem(LgiSystemPath Which);
	};

	/// Read the whole file into a string
	GString Read()
	{
		GString s;
		int64 sz = GetSize();
		NativeInt nsz = (NativeInt)sz;
		if (nsz > 0 &&
			nsz == sz &&
			s.Set(NULL, nsz))
		{
			int Block = 1 << 30;
			NativeInt i;
			for (i = 0; i < nsz; )
			{
				int Len = min(Block, (int) (nsz - i));
				int rd = Read(s.Get() + i, Len);
				if (rd <= 0)
					break;
				i += rd;
			}

			s.Get()[i] = 0;
		}
		return s;
	}
};

// Functions
LgiFunc int64 LgiFileSize(const char *FileName);

/// This function checks for the existance of a file.
LgiFunc bool FileExists(const char *File, char *CorrectCase = NULL);

/// This function checks for the existance of a directory.
LgiFunc bool DirExists(const char *Dir, char *CorrectCase = NULL);

LgiFunc bool ResolveShortcut(const char *LinkFile, char *Path, int Len);
LgiFunc void WriteStr(GFile &f, const char *s);
LgiFunc char *ReadStr(GFile &f DeclDebugArgs);
LgiFunc int SizeofStr(const char *s);
LgiFunc char *ReadTextFile(const char *File);
LgiFunc bool LgiTrimDir(char *Path);
LgiFunc bool LgiIsRelativePath(const char *Path);
LgiClass GAutoString LgiMakeRelativePath(const char *Base, const char *Path);
LgiFunc bool LgiMakePath(char *Str, int StrBufLen, const char *Dir, const char *File);
LgiFunc char *LgiGetExtension(char *File);
LgiFunc bool LgiIsFileNameExecutable(const char *FileName);
LgiFunc bool LgiIsFileExecutable(const char *FileName, GStreamI *f, int64 Start, int64 Len);
LgiFunc const char *GetErrorName(int e);

/// Get information about the disk that a file resides on.
LgiFunc bool LgiGetDriveInfo(char *Path, uint64 *Free, uint64 *Size = 0, uint64 *Available = 0);

/// Shows the file's properties dialog
LgiFunc void LgiShowFileProperties(OsView Parent, const char *Filename);

/// Opens to the file or folder in the OS file browser (Explorer/Finder etc)
LgiFunc bool LgiBrowseToFile(const char *Filename);

template<typename T>
T *LgiGetLeaf(T *Path)
{
	if (!Path)
		return NULL;

	T *l = strrchr(Path, DIR_CHAR);
	return l ? (T*)l + 1 : Path;
}

#endif
