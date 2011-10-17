/*hdr
**	FILE:			File.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			8/10/2000
**	DESCRIPTION:	The new file subsystem
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes **********************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>

#include "GFile.h"
#include "GContainers.h"
#include "GToken.h"
#include "Gdc2.h"
#include "LgiCommon.h"
#include "GString.h"

/****************************** Defines ***********************************/

// #define FILEDEBUG

#define FLOPPY_360K				0x0001
#define FLOPPY_720K				0x0002
#define FLOPPY_1_2M				0x0004
#define FLOPPY_1_4M				0x0008
#define FLOPPY_5_25				(FLOPPY_360K | FLOPPY_1_2M)
#define FLOPPY_3_5				(FLOPPY_720K | FLOPPY_1_4M)

#define stat64					stat
#define lseek64					lseek
#define ftrucate64				ftruncate
#define O_LARGEFILE				0

/****************************** Globals ***********************************/

struct ErrorCodeType
{
	const char *Name;
	int Code;
	const char *Desc;
}
ErrorCodes[] =
{
	#if defined(WIN32)

	{"EPERM", 1, "Not owner"},
	{"ENOENT", 2, "No such file"},
	{"ESRCH", 3, "No such process"},
	{"EINTR", 4, "Interrupted system"},
	{"EIO", 5, "I/O error"},
	{"ENXIO", 6, "No such device"},
	{"E2BIG", 7, "Argument list too long"},
	{"ENOEXEC", 8, "Exec format error"},
	{"EBADF", 9, "Bad file number"},
	{"ECHILD", 10, "No children"},
	{"EAGAIN", 11, "No more processes"},
	{"ENOMEM", 12, "Not enough core"},
	{"EACCES", 13, "Permission denied"},
	{"EFAULT", 14, "Bad address"},
	{"ENOTBLK", 15, "Block device required"},
	{"EBUSY", 16, "Mount device busy"},
	{"EEXIST", 17, "File exists"},
	{"EXDEV", 18, "Cross-device link"},
	{"ENODEV", 19, "No such device"},
	{"ENOTDIR", 20, "Not a directory"},
	{"EISDIR", 21, "Is a directory"},
	{"EINVAL", 22, "Invalid argument"},
	{"ENFILE", 23, "File table overflow"},
	{"EMFILE", 24, "Too many open file"},
	{"ENOTTY", 25, "Not a typewriter"},
	{"ETXTBSY", 26, "Text file busy"},
	{"EFBIG", 27, "File too large"},
	{"ENOSPC", 28, "No space left on"},
	{"ESPIPE", 29, "Illegal seek"},
	{"EROFS", 30, "Read-only file system"},
	{"EMLINK", 31, "Too many links"},
	{"EPIPE", 32, "Broken pipe"},
	{"EWOULDBLOCK", 35, "Operation would block"},
	{"EINPROGRESS", 36, "Operation now in progress"},
	{"EALREADY", 37, "Operation already in progress"},
	{"ENOTSOCK", 38, "Socket operation on"},
	{"EDESTADDRREQ", 39, "Destination address required"},
	{"EMSGSIZE", 40, "Message too long"},
	{"EPROTOTYPE", 41, "Protocol wrong type"},
	{"ENOPROTOOPT", 42, "Protocol not available"},
	{"EPROTONOSUPPORT", 43, "Protocol not supported"},
	{"ESOCKTNOSUPPORT", 44, "Socket type not supported"},
	{"EOPNOTSUPP", 45, "Operation not supported"},
	{"EPFNOSUPPORT", 46, "Protocol family not supported"},
	{"EAFNOSUPPORT", 47, "Address family not supported"},
	{"EADDRINUSE", 48, "Address already in use"},
	{"EADDRNOTAVAIL", 49, "Can't assign requested address"},
	{"ENETDOWN", 50, "Network is down"},
	{"ENETUNREACH", 51, "Network is unreachable"},
	{"ENETRESET", 52, "Network dropped connection"},
	{"ECONNABORTED", 53, "Software caused connection"},
	{"ECONNRESET", 54, "Connection reset by peer"},
	{"ENOBUFS", 55, "No buffer space available"},
	{"EISCONN", 56, "Socket is already connected"},
	{"ENOTCONN", 57, "Socket is not connected"	},
	{"ESHUTDOWN", 58, "Can't send after shutdown"},
	{"ETOOMANYREFS", 59, "Too many references"},
	{"ETIMEDOUT", 60, "Connection timed out"},
	{"ECONNREFUSED", 61, "Connection refused"},
	{"ELOOP", 62, "Too many levels of nesting"},
	{"ENAMETOOLONG", 63, "File name too long"	},
	{"EHOSTDOWN", 64, "Host is down"},
	{"EHOSTUNREACH", 65, "No route to host"},
	{"ENOTEMPTY", 66, "Directory not empty"},
	{"EPROCLIM", 67, "Too many processes"},
	{"EUSERS", 68, "Too many users"},
	{"EDQUOT", 69, "Disc quota exceeded"},
	{"ESTALE", 70, "Stale NFS file handle"},
	{"EREMOTE", 71, "Too many levels of remote in the path"},
	{"ENOSTR", 72, "Device is not a stream"},
	{"ETIME", 73, "Timer expired"},
	{"ENOSR", 74, "Out of streams resources"},
	{"ENOMSG", 75, "No message"},
	{"EBADMSG", 76, "Trying to read unreadable message"},
	{"EIDRM", 77, "Identifier removed"},
	{"EDEADLK", 78, "Deadlock condition"},
	{"ENOLCK", 79, "No record locks available"},
	{"ENONET", 80, "Machine is not on network"},
	{"ERREMOTE", 81, "Object is remote"},
	{"ENOLINK", 82, "The link has been severed"},
	{"EADV", 83, "ADVERTISE error"},
	{"ESRMNT", 84, "SRMOUNT error"},
	{"ECOMM", 85, "Communication error"},
	{"EPROTO", 86, "Protocol error"},
	{"EMULTIHOP", 87, "Multihop attempted"},
	{"EDOTDOT", 88, "Cross mount point"},
	{"EREMCHG", 89, "Remote address change"},

	#elif defined(MAC)

	{"EPERM",			EPERM, "Operation not permitted"},
	{"ENOENT",			ENOENT, "No such file or directory"},
	{"ESRCH",			ESRCH, "No such process"},
	{"EINTR",			EINTR, "Interrupted system call"},
	{"EIO",				EIO, "I/O error"},
	{"ENXIO",			ENXIO, "No such device or address"},
	{"E2BIG",			E2BIG, "Argument list too long"},
	{"ENOEXEC",			ENOEXEC, "Exec format error"},
	{"EBADF",			EBADF, "Bad file number"},
	{"ECHILD",			ECHILD, "No child processes"},
	{"EAGAIN",			EAGAIN, "Try again"},
	{"ENOMEM",			ENOMEM, "Out of memory"},
	{"EACCES",			EACCES, "Permission denied"},
	{"EFAULT",			EFAULT, "Bad address"},
	{"ENOTBLK",			ENOTBLK, "Block device required"},
	{"EBUSY",			EBUSY, "Device or resource busy"},
	{"EEXIST",			EEXIST, "File exists"},
	{"EXDEV",			EXDEV, "Cross-device link"},
	{"ENODEV",			ENODEV, "No such device"},
	{"ENOTDIR",			ENOTDIR, "Not a directory"},
	{"EISDIR",			EISDIR, "Is a directory"},
	{"EINVAL",			EINVAL, "Invalid argument"},
	{"ENFILE",			ENFILE, "File table overflow"},
	{"EMFILE",			EMFILE, "Too many open files"},
	{"ENOTTY",			ENOTTY, "Not a typewriter"},
	{"ETXTBSY",			ETXTBSY, "Text file busy"},
	{"EFBIG",			EFBIG, "File too large"},
	{"ENOSPC",			ENOSPC, "No space left on device"},
	{"ESPIPE",			ESPIPE, "Illegal seek"},
	{"EROFS",			EROFS, "Read-only file system"},
	{"EMLINK",			EMLINK, "Too many links"},
	{"EPIPE",			EPIPE, "Broken pipe"},
	{"EDOM",			EDOM, "Math argument out of domain of func"},
	{"ERANGE",			ERANGE, "Math result not representable"},
	{"EDEADLK",			EDEADLK, "Resource deadlock would occur"},
	{"ENAMETOOLONG",	ENAMETOOLONG, "File name too long"},
	{"ENOLCK",			ENOLCK, "No record locks available"},
	{"ENOSYS",			ENOSYS, "Function not implemented"},
	{"ENOTEMPTY",		ENOTEMPTY, "Directory not empty"},
	{"ELOOP",			ELOOP, "Too many symbolic links encountered"},
	{"EWOULDBLOCK",		EWOULDBLOCK, "Operation would block"},
	{"ENOMSG",			ENOMSG, "No message of desired type"},
	{"EIDRM",			EIDRM, "Identifier removed"},
	{"EREMOTE",			EREMOTE, "Object is remote"},
	{"EOVERFLOW",		EOVERFLOW, "Value too large for defined data type"},
	{"EILSEQ",			EILSEQ, "Illegal byte sequence"},
	{"EUSERS",			EUSERS, "Too many users"},
	{"ENOTSOCK",		ENOTSOCK, "Socket operation on non-socket"},
	{"EDESTADDRREQ",	EDESTADDRREQ, "Destination address required"},
	{"EMSGSIZE",		EMSGSIZE, "Message too long"},
	{"EPROTOTYPE",		EPROTOTYPE, "Protocol wrong type for socket"},
	{"ENOPROTOOPT",		ENOPROTOOPT, "Protocol not available"},
	{"EPROTONOSUPPORT",	EPROTONOSUPPORT, "Protocol not supported"},
	{"ESOCKTNOSUPPORT",	ESOCKTNOSUPPORT, "Socket type not supported"},
	{"EOPNOTSUPP",		EOPNOTSUPP, "Operation not supported on transport endpoint"},
	{"EPFNOSUPPORT",	EPFNOSUPPORT, "Protocol family not supported"},
	{"EAFNOSUPPORT",	EAFNOSUPPORT, "Address family not supported by protocol"},
	{"EADDRINUSE",		EADDRINUSE, "Address already in use"},
	{"EADDRNOTAVAIL",	EADDRNOTAVAIL, "Cannot assign requested address"},
	{"ENETDOWN",		ENETDOWN, "Network is down"},
	{"ENETUNREACH",		ENETUNREACH, "Network is unreachable"},
	{"ENETRESET",		ENETRESET, "Network dropped connection because of reset"},
	{"ECONNABORTED",	ECONNABORTED, "Software caused connection abort"},
	{"ECONNRESET",		ECONNRESET, "Connection reset by peer"},
	{"ENOBUFS",			ENOBUFS, "No buffer space available"},
	{"EISCONN",			EISCONN, "Transport endpoint is already connected"},
	{"ENOTCONN",		ENOTCONN, "Transport endpoint is not connected"},
	{"ESHUTDOWN",		ESHUTDOWN, "Cannot send after transport endpoint shutdown"},
	{"ETOOMANYREFS",	ETOOMANYREFS, "Too many references: cannot splice"},
	{"ETIMEDOUT",		ETIMEDOUT, "Connection timed out"},
	{"ECONNREFUSED",	ECONNREFUSED, "Connection refused"},
	{"EHOSTDOWN",		EHOSTDOWN, "Host is down"},
	{"EHOSTUNREACH",	EHOSTUNREACH, "No route to host"},
	{"EALREADY",		EALREADY, "Operation already in progress"},
	{"EINPROGRESS",		EINPROGRESS, "Operation now in progress"},
	{"ESTALE",			ESTALE, "Stale NFS file handle"},
	{"EDQUOT",			EDQUOT, "Quota exceeded"},

	#else
	#error impl me
	#endif

	{"NONE", 0, "No error"},
};

const char *GetErrorName(int e)
{
	for (ErrorCodeType *c=ErrorCodes; c->Code; c++)
	{
		if (e == c->Code)
		{
			return c->Name;
		}
	}

	static char s[32];
	sprintf(s, "Unknown(%i)", e);
	return s;
}

const char *GetErrorDesc(int e)
{
	for (ErrorCodeType *c=ErrorCodes; c->Code; c++)
	{
		if (e == c->Code)
		{
			return c->Desc;
		}
	}

	return 0;
}

/****************************** Helper Functions **************************/
char *ReadTextFile(char *File)
{
	char *s = 0;
	GFile f;
	if (File AND f.Open(File, O_READ))
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
	struct stat64 s;
	if (FileName AND
		stat64(FileName, &s) == 0)
	{
		return s.st_size;
	}

	return 0;
}

bool DirExists(const char *FileName)
{
	bool Status = false;
	
	if (FileName)
	{
		struct stat s;
		
		// Check for exact match...
		if (stat(FileName, &s) == 0)
		{
			Status = S_ISDIR(s.st_mode);
		}
	}

	return Status;
}

bool FileExists(const char *FileName)
{
	bool Status = false;
	
	if (FileName)
	{
		struct stat s;
		
		// Check for exact match...
		if (stat(FileName, &s) == 0)
		{
			Status = true;
		}
		else
		{
			// Look for altenate case by enumerating the directory
			char d[256];
			strcpy(d, FileName);
			char *e = strrchr(d, DIR_CHAR);
			if (e)
			{
				*e++ = 0;
				
				DIR *Dir = opendir(d);
				if (Dir)
				{
					dirent *De;
					while (De = readdir(Dir))
					{
						if (stricmp(De->d_name, e) == 0)
						{
							try
							{
								// Tell the calling program the actual case of the file...
								e = strrchr(FileName, DIR_CHAR);
								
								// If this crashes because the argument is read only then we get caught by the try catch
								strcpy(e+1, De->d_name);
								
								// It worked!
								Status = true;
							}
							catch (...)
							{
								// It didn't work :(
								#ifdef _DEBUG
								printf("%s,%i - FileExists(%s) found an alternate case version but couldn't return it to the caller.\n",
										__FILE__, __LINE__, FileName);
								#endif
							}
							
							break;
						}
					}
					
					closedir(Dir);
				}
			}
		}
	}

	return Status;
}

bool ResolveShortcut(char *LinkFile, char *Path, int Len)
{
	bool Status = false;

	return Status;
}

void WriteStr(GFile &f, char *s)
{
	ulong Len = (s) ? strlen(s) : 0;
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
		#if defined(_DEBUG) && defined MEMORY_DEBUG
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
	
	if (Path)
	{
		struct stat s;
		if (lstat(Path, &s) == 0)
		{
			// printf("LgiGetDriveInfo dev=%i\n", s.st_dev);
		}
	}
	
	return Status;
}

/****************************** Classes *************************************************************************************/
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

/////////////////////////////////////////////////////////////////////////
#include <sys/types.h>
#include <pwd.h>

class GMacVolume : public GVolume
{
	int Which;
	List<GVolume> _Sub;

public:
	GMacVolume(int w)
	{
		Which = w;
		_Name = 0;
		_Path = 0;
		_Type = VT_NONE;
		_Flags = 0;
		_Size = 0;
		_Free = 0;

		if (Which < 0)
		{
			_Name = NewStr("Desktop");

			char p[256];
			if (LgiGetSystemPath(LSP_DESKTOP, p, sizeof(p)))
			{
				_Path = NewStr(p);
			}
		}
	}
	
	~GMacVolume()
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

	GVolume *First()
	{
		if (Which < 0 AND
			!_Sub.Length())
		{
			// Get various shortcuts to points of interest
			GMacVolume *v = new GMacVolume(0);
			if (v)
			{
				v->_Path = NewStr("/");
				v->_Name = NewStr("Root");
				v->_Type = VT_HARDDISK;
				_Sub.Insert(v);
			}

			struct passwd *pw = getpwuid(getuid());
			if (pw)
			{
				v = new GMacVolume(0);
				if (v)
				{
					v->_Path = NewStr(pw->pw_dir);
					v->_Name = NewStr("Home");
					v->_Type = VT_HARDDISK;
					_Sub.Insert(v);
				}
			}

			/*
			// Get mount list
			// this is just a hack at this stage to establish some base
			// functionality. I would appreciate someone telling me how
			// to do this properly. Till then...
			GFile f;
			if (f.Open("/etc/fstab", O_READ))
			{
				int Len = f.GetSize();
				char *Buf = new char[Len+1];
				if (Buf)
				{
					f.Read(Buf, Len);
					Buf[Len] = 0;
					f.Close();

					GToken L(Buf, "\r\n");
					for (int l=0; l<L.Length(); l++)
					{
						GToken M(L[l], " \t");
						if (M.Length() > 2)
						{
							char *Mount = M[1];
							if (Mount AND
								strnicmp(M[0], "/dev/", 5) == 0 AND
								strlen(M[1]) > 1 AND
								stricmp(M[2], "swap") != 0)
							{
								v = new GMacVolume(0);
								if (v)
								{
									char *MountName = strrchr(Mount, '/');
									v->_Name = NewStr(MountName ? MountName + 1 : Mount);
									v->_Path = NewStr(Mount);
									v->_Type = VT_HARDDISK;

									char *Device = M[0];
									char *FileSys = M[2];
									if (stristr(Device, "fd"))
									{
										v->_Type = VT_3_5FLOPPY;
									}
									else if (stristr(Device, "cdrom"))
									{
										v->_Type = VT_CDROM;
									}

									_Sub.Insert(v);
								}
							}
						}
					}
				}
			}
			*/
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
		if (Which >= 0 AND
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

///////////////////////////////////////////////////////////////////////////////
GFileSystem *GFileSystem::Instance = 0;

GFileSystem::GFileSystem()
{
	Instance = this;
	Root = 0;
}

GFileSystem::~GFileSystem()
{
	DeleteObj(Root);
}

void GFileSystem::OnDeviceChange(char *Reserved)
{
}

GVolume *GFileSystem::GetRootVolume()
{
	if (!Root)
	{
		Root = new GMacVolume(-1);
	}

	return Root;
}

int FloppyType(int Letter)
{

	/*
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
	*/

	return 0;
}

GDirectory *GFileSystem::GetDir()
{
	return new GDirImpl;
}

OSType gFinderSignature = 'MACS';

OSStatus MoveFileToTrash(CFURLRef fileURL)
{
	AppleEvent event, reply;
	OSStatus err;
	FSRef fileRef;
	AliasHandle fileAlias;

	if (CFURLGetFSRef(fileURL, &fileRef) == false)
		return coreFoundationUnknownErr;

	err = FSNewAliasMinimal(&fileRef, &fileAlias);
	if (err == noErr)
	{
		err = AEBuildAppleEvent(kAECoreSuite,
								kAEDelete,
								typeApplSignature,
								&gFinderSignature,
								sizeof(OSType),
								kAutoGenerateReturnID,
								kAnyTransactionID,
								&event,
								NULL,
								"'----':alis(@@)",
								fileAlias);
		if (err == noErr)
		{
			err = AESendMessage(&event, &reply, kAEWaitReply, kAEDefaultTimeout);
			if (err == noErr)
				AEDisposeDesc(&reply);
			AEDisposeDesc(&event);
		}

		DisposeHandle((Handle)fileAlias);
	}
	return err;
}

bool GFileSystem::Copy(char *From, char *To, int *ErrorCode, CopyFileCallback Callback, void *Token)
{
	if (!From || !To)
	{
		if (ErrorCode) *ErrorCode = paramErr;
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
		if (ErrorCode) *ErrorCode = writErr;
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
		if (ErrorCode) *ErrorCode = notEnoughBufferSpace;
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
					if (ErrorCode) *ErrorCode = writErr;
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
		if (ErrorCode) *ErrorCode = noErr;
	}
	else
	{
		Out.Close();
		Delete(To, false);
	}

	return i == Size;

	/*
	bool Status = false;

	if (From AND To)
	{
		GFile In, Out;
		if (In.Open(From, O_READ) AND
			Out.Open(To, O_WRITE))
		{
			Out.SetSize(0);
			int64 Size = In.GetSize();
			int64 Block = min((1 << 20), Size);
			char *Buf = new char[Block];
			if (Buf)
			{
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
							else goto ExitCopyLoop;
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
				Status = i == Size;
				if (!Status)
				{
					Delete(To, false);
				}
			}
		}
	}

	return Status;
	*/
}

bool GFileSystem::Delete(GArray<char*> &Files, GArray<int> *Status, bool ToTrash)
{
	bool Error = false;

	if (ToTrash)
	{
		#if 1
		// Apple events method
		
		for (int i=0; i<Files.Length(); i++)
		{
			UInt8 *Tmp = (UInt8*) Files[i];
			CFStringRef s = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)Tmp, strlen(Files[i]), kCFStringEncodingUTF8, false);
			if (!s)
			{
				printf("%s:%i - CFStringCreateWithBytes failed\n", __FILE__, __LINE__);
				Error = true;
				break;
			}
			
			CFURLRef f = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, s, kCFURLPOSIXPathStyle, DirExists(Files[i]));
			CFRelease(s);
			if (!f)
			{
				printf("%s:%i - CFURLCreateWithFileSystemPath failed\n", __FILE__, __LINE__);
				Error = true;
				break;
			}
			
			if (MoveFileToTrash(f))
			{
				printf("%s:%i - MoveFileToTrash failed\n", __FILE__, __LINE__);
				Error = true;
				break;
			}
			
			CFRelease(f);
		}
		
		#else
		// Posix

		char p[300];
		if (LgiGetSystemPath(LSP_TRASH, p, sizeof(p)))
		{
			for (int i=0; i<Files.Length(); i++)
			{
				char *f = strrchr(Files[i], DIR_CHAR);
				LgiMakePath(p, sizeof(p), p, f?f+1:Files[i]);
				if (NOT MoveFile(Files[i], p))
				{
					if (Status)
					{
						(*Status)[i] = errno;
					}
					
					printf("%s:%i - MoveFile(%s,%s) failed.\n", __FILE__, __LINE__, Files[i], p);
					Error = true;
				}
			}
		}
		else
		{
			printf("%s:%i - LgiGetSystemPath(LSP_TRASH) failed.\n", __FILE__, __LINE__);
		}
		#endif
	}
	else
	{
		for (int i=0; i<Files.Length(); i++)
		{
			if (unlink(Files[i]))
			{
				if (Status)
				{
					(*Status)[i] = errno;
				}
				
				Error = true;
			}			
		}
	}
	
	return !Error;
}

bool GFileSystem::Delete(char *FileName, bool ToTrash)
{
	if (FileName)
	{
		GArray<char*> f;
		f.Add(FileName);
		return Delete(f, 0, ToTrash);
	}
	return false;
}

bool GFileSystem::CreateFolder(char *PathName)
{
	return mkdir(PathName, S_IRWXU | S_IXGRP | S_IXOTH) == 0;
}

bool GFileSystem::RemoveFolder(char *PathName, bool Recurse)
{
	if (Recurse)
	{
		GDirectory *Dir = FileDev->GetDir();
		if (Dir AND
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

	return rmdir(PathName) == 0;
}

bool GFileSystem::SetCurrentDirectory(char *PathName)
{
	return chdir(PathName) == 0;
}

bool GFileSystem::GetCurrentDirectory(char *PathName, int Length)
{
	return getcwd(PathName, Length) != 0;
}

bool GFileSystem::Move(char *OldName, char *NewName)
{
	if (rename(OldName, NewName))
	{
		printf("%s:%i - rename failed, error: %s(%i)\n",
			__FILE__, __LINE__,
			GetErrorName(errno), errno);
		return false;
	}
	
	return true;
}


/*
bool Match(char *Name, char *Mask)
{
	strupr(Name);
	strupr(Mask);

	while (*Name AND *Mask)
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
		else if (*Mask == '?' OR *Mask == *Name)
		{
			Mask++;
			Name++;
		}
		else
		{
			return false;
		}
	}

	while (*Mask AND ((*Mask == '*') OR (*Mask == '.'))) Mask++;

	return (*Name == 0 AND *Mask == 0);
}
*/

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
bool GDirectory::ConvertToTime(char *Str, uint64 Time)
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, 256, "%I:%M:%S", t);
		return true;
	}
	return false;
}

bool GDirectory::ConvertToDate(char *Str, uint64 Time)
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, 256, "%d/%m/%y", t);
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Directory //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class GDirImplPrivate
{
public:
	char			BasePath[256];
	DIR				*Dir;
	struct dirent	*De;
	struct stat		Stat;
	char			*Pattern;

	GDirImplPrivate()
	{	
		Dir = 0;
		De = 0;
		BasePath[0] = 0;
		Pattern = 0;
	}
	
	~GDirImplPrivate()
	{
		DeleteArray(Pattern);
	}
	
	bool Ignore()
	{
		return	De
				AND
				(
					strcmp(De->d_name, ".") == 0
					OR
					strcmp(De->d_name, "..") == 0
					OR
					(
						Pattern
						AND
						!MatchStr(Pattern, De->d_name)
					)
				);
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

int GDirImpl::First(const char *Name, const char *Pattern)
{
	Close();

	if (Name)
	{
		strcpy(d->BasePath, Name);
		if (!Pattern OR stricmp(Pattern, LGI_ALL_FILES) == 0)
		{
			struct stat S;
			if (lstat(Name, &S) == 0)
			{
				if (S_ISREG(S.st_mode))
				{
					char *Dir = strrchr(d->BasePath, DIR_CHAR);
					if (Dir)
					{
						*Dir++ = 0;
						d->Pattern = NewStr(Dir);
					}
				}
			}
		}
		else
		{
			d->Pattern = NewStr(Pattern);
		}
		
		d->Dir = opendir(d->BasePath);
		if (d->Dir)
		{
			d->De = readdir(d->Dir);
			if (d->De)
			{
				char s[256];
				LgiMakePath(s, sizeof(s), d->BasePath, GetName());
				lstat(s, &d->Stat);

				if (d->Ignore())
				{
					if (!Next())
					{
						return false;
					}
				}
			}
		}
	}

	return d->Dir != 0 AND d->De != 0;
}

int GDirImpl::Next()
{
	int Status = false;

	while (d->Dir AND d->De)
	{
		if (d->De = readdir(d->Dir))
		{
			char s[256];
			LgiMakePath(s, sizeof(s), d->BasePath, GetName());			
			lstat(s, &d->Stat);
			if (!d->Ignore())
			{
				Status = true;
				break;
			}
		}
	}

	return Status;
}

int GDirImpl::Close()
{
	if (d->Dir)
	{
		closedir(d->Dir);
		d->Dir = 0;
	}
	d->De = 0;

	return true;
}

bool GDirImpl::Path(char *s, int BufLen)
{
	if (!s)
	{
		return false;
	}

	return LgiMakePath(s, BufLen, d->BasePath, GetName());
}

int GDirImpl::GetType()
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

int GDirImpl::GetUser(bool Group)
{
	if (Group)
	{
		return d->Stat.st_gid;
	}
	else
	{
		return d->Stat.st_uid;
	}
}

bool GDirImpl::IsReadOnly()
{
	if (getuid() == d->Stat.st_uid)
	{
		// Check user perms
		return !TestFlag(GetAttributes(), S_IWUSR);
	}
	else if (getgid() == d->Stat.st_gid)
	{
		// Check group perms
		return !TestFlag(GetAttributes(), S_IWGRP);
	}
	
	// Check global perms
	return !TestFlag(GetAttributes(), S_IWOTH);
}

bool GDirImpl::IsHidden()
{
	return GetName() AND GetName()[0] == '.';
}

bool GDirImpl::IsDir()
{
	int a = GetAttributes();
	return !S_ISLNK(a) AND S_ISDIR(a);
}

long GDirImpl::GetAttributes()
{
	return d->Stat.st_mode;
}

char *GDirImpl::GetName()
{
	return (d->De) ? d->De->d_name : 0;
}

const uint64 GDirImpl::GetCreationTime()
{
	return (uint64)d->Stat.st_ctime * 1000;
}

const uint64 GDirImpl::GetLastAccessTime()
{
	return (uint64)d->Stat.st_atime * 1000;
}

const uint64 GDirImpl::GetLastWriteTime()
{
	return (uint64)d->Stat.st_mtime * 1000;
}

const uint64 GDirImpl::GetSize()
{
	return (uint32)d->Stat.st_size;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// File ///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class GFilePrivate
{
public:
	int hFile;
	char *Name;
	bool Swap;
	int Status;
	int Attributes;
	int LastError;
	
	GFilePrivate()
	{
		hFile = INVALID_HANDLE;
		Name = 0;
		Swap = false;
		Status = true;
		Attributes = 0;
		LastError = noErr;
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
	if (d AND ValidHandle(d->hFile))
	{
		Close();
	}
	DeleteObj(d);
}

int GFile::GetError()
{
	return d->LastError;
}

OsFile GFile::Handle()
{
	return d->hFile;
}

bool GFile::IsOpen()
{
	return ValidHandle(d->hFile);
}

// #define _FILE_OPEN
#ifdef _FILE_OPEN
GSemaphore _FileLock;
GHashTable _FileOpen;

LgiFunc void _DumpOpenFiles()
{
	if (_FileLock.Lock(_FL))
	{
		char *k;
		int i=0;
		for (void *p=_FileOpen.First(&k); p; p=_FileOpen.Next(&k))
		{
			printf("File[%i]='%s'\n", i++, k);
		}
		_FileLock.Unlock();
	}
}
#endif

int GFile::Open(const char *File, int Mode)
{
	if (!File)
	{
		d->LastError = paramErr;
		return false;
	}
	
	if (TestFlag(Mode, O_WRITE) OR
		TestFlag(Mode, O_READWRITE))
	{
		Mode |= O_CREAT;
	}

	Close();
	d->hFile = open(File, Mode | O_LARGEFILE, S_IRUSR | S_IWUSR);
	if (!ValidHandle(d->hFile))
	{
		d->LastError = errno;
		printf("GFile::Open failed\n\topen(%s,%8.8x) = %i\n\terrno=%s (%s)\n", File, Mode, d->hFile, GetErrorName(errno), GetErrorDesc(errno));
		return false;
	}
	
	#ifdef _FILE_OPEN
	if (_FileLock.Lock(_FL))
	{5
		_FileOpen.Add(File, this);
		_FileLock.Unlock();
	}
	#endif
	
	d->Attributes = Mode;
	d->Name = new char[strlen(File)+1];
	if (d->Name)
	{
		strsafecpy(d->Name, File, sizeof(d->Name));
	}
	d->Status = true;

	return true;
}

int GFile::Close()
{
	if (ValidHandle(d->hFile))
	{
		#ifdef _FILE_OPEN
		if (_FileLock.Lock(_FL))
		{
			_FileOpen.Delete(d->Name);
			_FileLock.Unlock();
		}
		#endif

		close(d->hFile);
		d->hFile = INVALID_HANDLE;
		DeleteArray(d->Name);
	}

	return true;
}

/*
int GFile::Print(char *Format, ...)
{
	int Chars = 0;

	if (Format)
	{
		va_list Arg;

		va_start(Arg, Format);
		int Size = vsnprintf(0, 0, Format, Arg);
		char *Buffer = new char[Size+1];
		if (Buffer)
		{
			vsprintf(Buffer, Format, Arg);
		}
		va_end(Arg);

		if (Size > 0)
		{
			Write(Buffer, Size);
		}
		DeleteArray(Buffer);
	}

	return Chars;
}
*/

#define CHUNK		0xFFF0

int GFile::Read(void *Buffer, int Size, int Flags)
{
	int Red = 0;

	if (Buffer AND Size > 0)
	{
		Red = read(d->hFile, Buffer, Size);
	}
	d->Status = Red == Size;

	return max(Red, 0);
}

int GFile::Write(const void *Buffer, int Size, int Flags)
{
	int Written = 0;

	if (Buffer AND Size > 0)
	{
		Written = write(d->hFile, Buffer, Size);
	}
	d->Status = Written == Size;

	return max(Written, 0);
}

int64 GFile::Seek(int64 To, int Whence)
{
	#if LINUX64
	return lseek64(d->hFile, To, Whence); // If this doesn't compile, switch off LINUX64
	#else
	return lseek(d->hFile, To, Whence);
	#endif
}

int64 GFile::SetPos(int64 Pos)
{
	#if LINUX64
	int64 p = lseek64(d->hFile, Pos, SEEK_SET);
	if (p < 0)
	{
		int e = errno;
		printf("%s:%i - lseek64(%Lx) failed (error %i: %s).\n",
			__FILE__, __LINE__,
			Pos, e, GetErrorName(e));
		
	}
	#else
	return lseek(d->hFile, Pos, SEEK_SET);
	#endif
}

int64 GFile::GetPos()
{
	#if LINUX64
	int64 p = lseek64(d->hFile, 0, SEEK_CUR);
	if (p < 0)
	{
		int e = errno;
		printf("%s:%i - lseek64 failed (error %i: %s).\n",
			__FILE__, __LINE__,
			e, GetErrorName(e));
		
	}
	return p;
	#else
	return lseek(d->hFile, 0, SEEK_CUR);
	#endif	
}

int64 GFile::GetSize()
{
	int64 Here = GetPos();
	#if LINUX64
	int64 Ret = lseek64(d->hFile, 0, SEEK_END);
	#else
	int64 Ret = lseek(d->hFile, 0, SEEK_END);
	#endif
	SetPos(Here);
	return Ret;
}

int64 GFile::SetSize(int64 Size)
{
	if (ValidHandle(d->hFile))
	{
		int64 Pos = GetPos();
		
		/*
		close(d->hFile);
		if (d->Name)
		{
			#if LINUX64
			truncate64(Name, Size);
			#else
			truncate(Name, Size);
			#endif
		}
		d->hFile = open(Name, Attributes, 0);
		*/
		
		#if LINUX64
		ftruncate64(d->hFile, Size);
		#else
		ftruncate(d->hFile, Size);
		#endif
		
		if (d->hFile)
		{
			SetPos(Pos);
		}
	}

	return GetSize();
}

bool GFile::Eof()
{
	return GetPos() >= GetSize();
}

int GFile::SwapRead(uchar *Buf, int Size)
{
	int r = Read(Buf, Size);
	if (r == Size)
	{
		uint8 *s = Buf, *e = Buf + r - 1;
		for (; s < e; s++, e--)
		{
			uchar c = *s;
			*s = *e;
			*e = c;
		}
	}
	else return 0;
	
	return r;
}

int GFile::SwapWrite(uchar *Buf, int Size)
{
	switch (Size)
	{
		case 1:
		{
			return Write(Buf, Size);
			break;
		}
		case 2:
		{
			uint16 i = *((uint16*)Buf);
			i = LgiSwap16(i);
			return Write(&i, Size);
			break;
		}
		case 4:
		{
			uint32 i = *((uint32*)Buf);
			i = LgiSwap32(i);
			return Write(&i, Size);
			break;
		}
		case 8:
		{
			uint64 i = *((uint64*)Buf);
			i = LgiSwap64(i);
			return Write(&i, Size);
			break;
		}
		default:
		{
			int i, n;
			for (i=0, n=Size-1; i<n; i++, n--)
			{
				uchar c = Buf[i];
				Buf[i] = Buf[n];
				Buf[n] = c;
			}

			int w = Write(Buf, Size);

			for (i=0, n=Size-1; i<n; i++, n--)
			{
				uchar c = Buf[i];
				Buf[i] = Buf[n];
				Buf[n] = c;
			}
			
			return w;
			break;
		}
	}
	
	return 0;
}

int GFile::ReadStr(char *Buf, int Size)
{
	int i = 0;
	int r = 0;
	if (Buf AND Size > 0)
	{
		char c;

		Size--;

		do
		{
			r = read(d->hFile, &c, 1);
			if (Eof())
			{
				break;
			}

			*Buf++ = c;
			i++;

		} while (i < Size - 1 AND c != '\n');

		*Buf = 0;
	}

	return i;
}

int GFile::WriteStr(char *Buf, int Size)
{
	int i = 0;
	int w;

	while (i <= Size)
	{
		w = write(d->hFile, Buf, 1);
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

char *GFile::GetName()
{
	return d->Name;
}

#define RdIO { d->Status |= ((d->Swap) ? SwapRead((uchar*) &i, sizeof(i)) : Read(&i, sizeof(i))) != sizeof(i); return *this; }
#define WrIO { d->Status |= ((d->Swap) ? SwapWrite((uchar*) &i, sizeof(i)) : Write(&i, sizeof(i))) != sizeof(i); return *this; }

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

