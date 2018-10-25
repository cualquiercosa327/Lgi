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
#include <stdio.h>

#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>
#include <unistd.h>

#include "LgiDefs.h"
#include "GFile.h"
#include "GContainers.h"
#include "GToken.h"
#include "Gdc2.h"
#include "LgiCommon.h"
#include "GString.h"
#include "LDateTime.h"

/****************************** Defines ***********************************/

// #define FILEDEBUG

#define FLOPPY_360K				0x0001
#define FLOPPY_720K				0x0002
#define FLOPPY_1_2M				0x0004
#define FLOPPY_1_4M				0x0008
#define FLOPPY_5_25				(FLOPPY_360K | FLOPPY_1_2M)
#define FLOPPY_3_5				(FLOPPY_720K | FLOPPY_1_4M)

/****************************** Globals ***********************************/

struct ErrorCodeType
{
	char *Name;
	int Code;
	char *Desc;
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

	#elif defined(LINUX)

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
	{"ECHRNG",			ECHRNG, "Channel number out of range"},
	{"EL2NSYNC",		EL2NSYNC, "Level 2 not synchronized"},
	{"EL3HLT",			EL3HLT, "Level 3 halted"},
	{"EL3RST",			EL3RST, "Level 3 reset"},
	{"ELNRNG",			ELNRNG, "Link number out of range"},
	{"EUNATCH",			EUNATCH, "Protocol driver not attached"},
	{"ENOCSI",			ENOCSI, "No CSI structure available"},
	{"EL2HLT",			EL2HLT, "Level 2 halted"},
	{"EBADE",			EBADE, "Invalid exchange"},
	{"EBADR",			EBADR, "Invalid request descriptor"},
	{"EXFULL",			EXFULL, "Exchange full"},
	{"ENOANO",			ENOANO, "No anode"},
	{"EBADRQC",			EBADRQC, "Invalid request code"},
	{"EBADSLT",			EBADSLT, "Invalid slot"},
	{"EBFONT",			EBFONT, "Bad font file format"},
	{"ENOSTR",			ENOSTR, "Device not a stream"},
	{"ENODATA",			ENODATA, "No data available"},
	{"ETIME",			ETIME, "Timer expired"},
	{"ENOSR",			ENOSR, "Out of streams resources"},
	{"ENONET",			ENONET, "Machine is not on the network"},
	{"ENOPKG",			ENOPKG, "Package not installed"},
	{"EREMOTE",			EREMOTE, "Object is remote"},
	{"ENOLINK",			ENOLINK, "Link has been severed"},
	{"EADV",			EADV, "Advertise error"},
	{"ESRMNT",			ESRMNT, "Srmount error"},
	{"ECOMM",			ECOMM, "Communication error on send"},
	{"EPROTO",			EPROTO, "Protocol error"},
	{"EMULTIHOP",		EMULTIHOP, "Multihop attempted"},
	{"EDOTDOT",			EDOTDOT, "RFS specific error"},
	{"EBADMSG",			EBADMSG, "Not a data message"},
	{"EOVERFLOW",		EOVERFLOW, "Value too large for defined data type"},
	{"ENOTUNIQ",		ENOTUNIQ, "Name not unique on network"},
	{"EBADFD",			EBADFD, "File descriptor in bad state"},
	{"EREMCHG",			EREMCHG, "Remote address changed"},
	{"ELIBACC",			ELIBACC, "Can not access a needed shared library"},
	{"ELIBBAD",			ELIBBAD, "Accessing a corrupted shared library"},
	{"ELIBSCN",			ELIBSCN, ".lib section in a.out corrupted"},
	{"ELIBMAX",			ELIBMAX, "Attempting to link in too many shared libraries"},
	{"ELIBEXEC",		ELIBEXEC, "Cannot exec a shared library directly"},
	{"EILSEQ",			EILSEQ, "Illegal byte sequence"},
	{"ERESTART",		ERESTART, "Interrupted system call should be restarted"},
	{"ESTRPIPE",		ESTRPIPE, "Streams pipe error"},
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
	{"EUCLEAN",			EUCLEAN, "Structure needs cleaning"},
	{"ENOTNAM",			ENOTNAM, "Not a XENIX named type file"},
	{"ENAVAIL",			ENAVAIL, "No XENIX semaphores available"},
	{"EISNAM",			EISNAM, "Is a named type file"},
	{"EREMOTEIO",		EREMOTEIO, "Remote I/O error"},
	{"EDQUOT",			EDQUOT, "Quota exceeded"},
	{"ENOMEDIUM",		ENOMEDIUM, "No medium found"},
	{"EMEDIUMTYPE",		EMEDIUMTYPE, "Wrong medium type"},

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

char *GetErrorDesc(int e)
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
	struct stat64 s;
	if (FileName &&
		stat64(FileName, &s) == 0)
	{
		return s.st_size;
	}

	return 0;
}

bool DirExists(const char *FileName, char *CorrectCase)
{
	bool Status = false;
	
	if (FileName)
	{
		struct stat s;
		
		// Check for exact match...
		int r = lstat(FileName, &s);
		if (r == 0)
		{
			Status = S_ISDIR(s.st_mode) ||
					 S_ISLNK(s.st_mode);
			// printf("DirStatus(%s) lstat = %i, %i\n", FileName, Status, s.st_mode);
		}
		else
		{
			r = stat(FileName, &s);
			if (r == 0)
			{
				Status = S_ISDIR(s.st_mode) ||
					 	S_ISLNK(s.st_mode);
				// printf("DirStatus(%s) stat ok = %i, %i\n", FileName, Status, s.st_mode);
			}
			else
			{
				// printf("DirStatus(%s) lstat and stat failed, r=%i, errno=%i\n", FileName, r, errno);
			}
		}
	}

	return Status;
}

bool FileExists(const char *FileName, char *CorrectCase)
{
	bool Status = false;
	
	if (FileName)
	{
		struct stat s;
		
		// Check for exact match...
		if (stat(FileName, &s) == 0)
		{
			Status = !S_ISDIR(s.st_mode);
		}
		else if (CorrectCase)
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
						if (De->d_type != DT_DIR &&
							stricmp(De->d_name, e) == 0)
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

bool ResolveShortcut(const char *LinkFile, char *Path, ssize_t Len)
{
	bool Status = false;

	return Status;
}

void WriteStr(GFile &f, const char *s)
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

ssize_t SizeofStr(const char *s)
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
	_Type = VT_NONE;
	_Flags = 0;
	_Size = 0;
	_Free = 0;
}

/////////////////////////////////////////////////////////////////////////
#include <sys/types.h>
#include <pwd.h>

class GLinuxVolume : public GVolume
{
	int Which;
	List<GVolume> _Sub;

public:
	GLinuxVolume(int w)
	{
		Which = w;
		_Type = VT_NONE;
		_Flags = 0;
		_Size = 0;
		_Free = 0;

		if (Which < 0)
		{
			_Name = "Desktop";
			_Type = VT_DESKTOP;
			_Path = LGetSystemPath(LSP_DESKTOP);
		}
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
		LgiAssert(0);
	}

	GVolume *First()
	{
		if (Which < 0 && !_Sub.Length())
		{
			// Get various shortcuts to points of interest
			GLinuxVolume *v = new GLinuxVolume(0);
			if (v)
			{
				v->_Path = "/";
				v->_Name = "Root";
				v->_Type = VT_HARDDISK;
				_Sub.Insert(v);
			}

			struct passwd *pw = getpwuid(getuid());
			if (pw)
			{
				v = new GLinuxVolume(0);
				if (v)
				{
					v->_Path = pw->pw_dir;
					v->_Name = "Home";
					v->_Type = VT_HARDDISK;
					_Sub.Insert(v);
				}
			}

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
							if (Mount &&
								strnicmp(M[0], "/dev/", 5) == 0 &&
								strlen(M[1]) > 1 &&
								stricmp(M[2], "swap") != 0)
							{
								v = new GLinuxVolume(0);
								if (v)
								{
									char *MountName = strrchr(Mount, '/');
									v->_Name = (MountName ? MountName + 1 : Mount);
									v->_Path = Mount;
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
			Dir = new GDirectory;
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
		Root = new GLinuxVolume(-1);
	}

	return Root;
}

int FloppyType(int Letter)
{
	uchar MaxTrack;
	uchar SecPerTrack;

	/*
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

bool GFileSystem::Copy(const char *From, const char *To, LError *Status, CopyFileCallback Callback, void *Token)
{
	GArray<char> Buf;

	if (Status)
		*Status = 0;

	if (Buf.Length(2 << 20))
	{
		GFile In, Out;
		if (!In.Open(From, O_READ))
		{
			if (Status)
				*Status = In.GetError();
			return false;
		}
		
		if (!Out.Open(To, O_WRITE))
		{
			if (Status)
				*Status = Out.GetError();
			return false;
		}

		int64 Size = In.GetSize(), Done = 0;
		for (int64 i=0; i<Size; i++)
		{
			int Copy = MIN(Size - i, Buf.Length());
			int r = In.Read(&Buf[0], Copy);
			if (r <= 0)
				break;
			
			while (r > 0)
			{
				int w = Out.Write(&Buf[0], r);
				if (w <= 0)
					break;
				
				r -= w;
				Done += w;
				if (Callback)
					Callback(Token, Done, Size);
			}
			
			if (r > 0)
				break;
		}
		
		return Done == Size;
	}
	
	return false;
}

bool GFileSystem::Delete(GArray<const char*> &Files, GArray<LError> *Status, bool ToTrash)
{
	bool Error = false;

	if (ToTrash)
	{
		char p[MAX_PATH];
		if (LGetSystemPath(LSP_TRASH, p, sizeof(p)))
		{
			for (int i=0; i<Files.Length(); i++)
			{
				char *f = strrchr(Files[i], DIR_CHAR);
				LgiMakePath(p, sizeof(p), p, f?f+1:Files[i]);
				if (!Move(Files[i], p))
				{
					if (Status)
					{
						(*Status)[i] = errno;
					}
					
					LgiTrace("%s:%i - MoveFile(%s,%s) failed.\n", _FL, Files[i], p);
					Error = true;
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - LgiGetSystemPath(LSP_TRASH) failed.\n", _FL);
		}
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

bool GFileSystem::Delete(const char *FileName, bool ToTrash)
{
	if (FileName)
	{
		GArray<const char*> f;
		f.Add(FileName);
		return Delete(f, 0, ToTrash);
	}
	return false;
}

bool GFileSystem::CreateFolder(const char *PathName, bool CreateParentTree, LError *ErrorCode)
{
	int r = mkdir(PathName, S_IRWXU | S_IXGRP | S_IXOTH);
	if (r)
	{
		if (ErrorCode)
			*ErrorCode = errno;
		printf("%s:%i - mkdir('%s') failed with %i, errno=%i\n", _FL, PathName, r, errno);
	}
	
	return r == 0;
}

bool GFileSystem::RemoveFolder(const char *PathName, bool Recurse)
{
	if (Recurse)
	{
		GDirectory *Dir = new GDirectory;
		if (Dir && Dir->First(PathName))
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

bool GFileSystem::Move(const char *OldName, const char *NewName, LError *Err)
{
	if (rename(OldName, NewName))
	{
		printf("%s:%i - rename failed, error: %s(%i)\n",
			_FL, GetErrorName(errno), errno);
		return false;
	}
	
	return true;
}


/*
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
			return false;
		}
	}

	while (*Mask && ((*Mask == '*') || (*Mask == '.'))) Mask++;

	return (*Name == 0 && *Mask == 0);
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
bool GDirectory::ConvertToTime(char *Str, int SLen, uint64 Time) const
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, SLen, "%I:%M:%S", t);
		return true;
	}
	return false;
}

bool GDirectory::ConvertToDate(char *Str, int SLen, uint64 Time) const
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, SLen, "%d/%m/%y", t);
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
	char			BasePath[MAX_PATH];
	DIR				*Dir;
	struct dirent	*De;
	struct stat		Stat;
	char			*Pattern;

	GDirectoryPriv()
	{	
		Dir = 0;
		De = 0;
		BasePath[0] = 0;
		Pattern = 0;
	}
	
	~GDirectoryPriv()
	{
		DeleteArray(Pattern);
	}
	
	bool Ignore()
	{
		return	De
				&&
				(
					strcmp(De->d_name, ".") == 0
					||
					strcmp(De->d_name, "..") == 0
					||
					(
						Pattern
						&&
						!MatchStr(Pattern, De->d_name)
					)
				);
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

int GDirectory::First(const char *Name, const char *Pattern)
{
	Close();

	if (Name)
	{
		strcpy(d->BasePath, Name);
		if (!Pattern || stricmp(Pattern, LGI_ALL_FILES) == 0)
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

	return d->Dir != 0 && d->De != 0;
}

int GDirectory::Next()
{
	int Status = false;

	while (d->Dir && d->De)
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

int GDirectory::Close()
{
	if (d->Dir)
	{
		closedir(d->Dir);
		d->Dir = 0;
	}
	d->De = 0;

	return true;
}

const char *GDirectory::FullPath()
{
	static char s[MAX_PATH];
	#warning this should really be optimized, and thread safe...
	Path(s, sizeof(s));
	return s;
}

GString GDirectory::FileName() const
{
	return GetName();
}

bool GDirectory::Path(char *s, int BufLen) const
{
	if (!s)
	{
		return false;
	}

	return LgiMakePath(s, BufLen, d->BasePath, GetName());
}

int GDirectory::GetType() const
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

int GDirectory::GetUser(bool Group) const
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

bool GDirectory::IsReadOnly() const
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

bool GDirectory::IsHidden() const
{
	return GetName() && GetName()[0] == '.';
}

bool GDirectory::IsDir() const
{
	int a = GetAttributes();
	return !S_ISLNK(a) && S_ISDIR(a);
}

bool GDirectory::IsSymLink() const
{
	int a = GetAttributes();
	return S_ISLNK(a);
}

long GDirectory::GetAttributes() const
{
	return d->Stat.st_mode;
}

char *GDirectory::GetName() const
{
	return (d->De) ? d->De->d_name : 0;
}

uint64 GDirectory::GetCreationTime() const
{
	return (uint64) d->Stat.st_ctime * LDateTime::Second64Bit;
}

uint64 GDirectory::GetLastAccessTime() const
{
	return (uint64) d->Stat.st_atime * LDateTime::Second64Bit;
}

uint64 GDirectory::GetLastWriteTime() const
{
	return (uint64) d->Stat.st_mtime * LDateTime::Second64Bit;
}

uint64 GDirectory::GetSize() const
{
	return (uint32)d->Stat.st_size;
}

int64 GDirectory::GetSizeOnDisk()
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
	int ErrorCode;
	
	GFilePrivate()
	{
		hFile = INVALID_HANDLE;
		Name = 0;
		Swap = false;
		Status = true;
		Attributes = 0;
		ErrorCode = 0;
	}
	
	~GFilePrivate()
	{
		DeleteArray(Name);
	}
};

GFile::GFile(const char *Path, int Mode)
{
	d = new GFilePrivate;
	if (Path)
		Open(Path, Mode);
}

GFile::~GFile()
{
	if (d && ValidHandle(d->hFile))
	{
		Close();
	}
	DeleteObj(d);
}

OsFile GFile::Handle()
{
	return d->hFile;
}

int GFile::GetError()
{
	return d->ErrorCode;
}

bool GFile::IsOpen()
{
	return ValidHandle(d->hFile);
}

#define DEBUG_OPEN_FILES 	0

#if DEBUG_OPEN_FILES
LMutex Lck;
GArray<GFile*> OpenFiles;
#endif

int GFile::Open(const char *File, int Mode)
{
	int Status = false;

	if (File)
	{
		if (TestFlag(Mode, O_WRITE) ||
			TestFlag(Mode, O_READWRITE))
		{
			Mode |= O_CREAT;
		}

		Close();
		d->hFile = open(File, Mode | O_LARGEFILE, S_IRUSR | S_IWUSR);
		if (ValidHandle(d->hFile))
		{
			d->Attributes = Mode;
			d->Name = new char[strlen(File)+1];
			if (d->Name)
			{
				strcpy(d->Name, File);
			}
			Status = true;
			d->Status = true;

			#if DEBUG_OPEN_FILES			
			if (Lck.Lock(_FL))
			{
				if (!OpenFiles.HasItem(this))
					OpenFiles.Add(this);
				Lck.Unlock();
			}
			#endif
		}
		else
		{
			d->ErrorCode = errno;

			#if DEBUG_OPEN_FILES
			if (Lck.Lock(_FL))
			{
				for (unsigned i=0; i<OpenFiles.Length(); i++)
					printf("%i: %s\n", i, OpenFiles[i]->GetName());
				Lck.Unlock();
			}
			#endif

			printf("GFile::Open failed\n\topen(%s,%08.8x) = %i\n\terrno=%s (%s)\n",
				File, 
				Mode, 
				d->hFile,
				GetErrorName(d->ErrorCode),
				GetErrorDesc(d->ErrorCode));
		}
	}

	return Status;
}

int GFile::Close()
{
	if (ValidHandle(d->hFile))
	{
		close(d->hFile);
		d->hFile = INVALID_HANDLE;
		DeleteArray(d->Name);

		#if DEBUG_OPEN_FILES
		if (Lck.Lock(_FL))
		{
			OpenFiles.Delete(this);
			Lck.Unlock();
		}
		#endif
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

ssize_t GFile::Read(void *Buffer, ssize_t Size, int Flags)
{
	int Red = 0;

	if (Buffer && Size > 0)
	{
		Red = read(d->hFile, Buffer, Size);
	}
	d->Status = Red == Size;

	return MAX(Red, 0);
}

ssize_t GFile::Write(const void *Buffer, ssize_t Size, int Flags)
{
	int Written = 0;

	if (Buffer && Size > 0)
	{
		Written = write(d->hFile, Buffer, Size);
	}
	d->Status = Written == Size;

	return MAX(Written, 0);
}

#define LINUX64 	1

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

ssize_t GFile::SwapRead(uchar *Buf, ssize_t Size)
{
	ssize_t i = 0;
	ssize_t r = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		r = read(d->hFile, Buf--, 1);
		i += r;
	}
	return i;
}

ssize_t GFile::SwapWrite(uchar *Buf, ssize_t Size)
{
	ssize_t i = 0;
	ssize_t w = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		w = write(d->hFile, Buf--, 1);
		i += w;
	}
	return i;
}

ssize_t GFile::ReadStr(char *Buf, ssize_t Size)
{
	ssize_t i = 0;
	ssize_t r = 0;
	if (Buf && Size > 0)
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

		} while (i < Size - 1 && c != '\n');

		*Buf = 0;
	}

	return i;
}

ssize_t GFile::WriteStr(char *Buf, ssize_t Size)
{
	ssize_t i = 0;
	ssize_t w;

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

