/// \file
/// \author Matthew Allen <fret@memecode.com>
#ifndef __IFTP_H
#define __IFTP_H

#include "INet.h"
#include "GDateTime.h"

#define DefaultFtpCharset	"iso-8859-1"

#define IFTP_READ			0x0400
#define IFTP_WRITE			0x0200
#define IFTP_EXECUTE		0x0100
#define IFTP_DIR			0x0800

#define IFTP_GRP_READ		0x0040
#define IFTP_GRP_WRITE		0x0020
#define IFTP_GRP_EXECUTE	0x0010

#define IFTP_GLOB_READ		0x0004
#define IFTP_GLOB_WRITE		0x0002
#define IFTP_GLOB_EXECUTE	0x0001

#define IFTP_ARCHIVE		0x1000
#define IFTP_HIDDEN			0x2000
#define IFTP_SYSTEM			0x4000

#define IFTP_SYM_LINK		0x8000

// Options
#define OPT_LogOpen			"LogOpen"

// Messages
#define M_CREATE_FOLDER		(M_USER+300)
#define M_RENAME			(M_USER+302)
#define M_PERMISSIONS		(M_USER+303)
#define M_RUN				(M_USER+304)
#define M_OPEN_AS_TEXT		(M_USER+305)
#define M_TRANSFER			(M_USER+306)
#define M_REFRESH			(M_USER+307)
#define M_UP				(M_USER+308)

// Classes
enum FtpOpenStatus
{
	FO_ConnectFailed,
	FO_LoginFailed,
	FO_Error,
	FO_Connected
};

/// Represents a file or folder in the remote systemß
class IFtpEntry
{
public:
	int Attributes;
	int64 Size;
	GAutoString Name;
	GAutoString Path;
	GAutoString User;
	GAutoString Group;
	GDateTime Date;

	IFtpEntry();
	IFtpEntry(char *Entry, const char *Cs);
	IFtpEntry(IFtpEntry *Entry);
	virtual ~IFtpEntry();

	bool IsDir() { return (Attributes & IFTP_DIR) != 0; }
	bool IsHidden() { return TestFlag(Attributes, IFTP_HIDDEN); }
};

/// The remote folder system interfaceß
class IFileProtocol
{
public:
	virtual ~IFileProtocol() {}

	// Properties
	virtual const char *GetCharset() = 0;
	virtual void SetCharset(const char *cs) = 0;
	virtual bool IsForceActive() = 0;
	virtual void IsForceActive(bool i) = 0;
	virtual bool IsLongList() = 0;
	virtual void IsLongList(bool i) = 0;
	virtual bool IsShowHidden() = 0;
	virtual void IsShowHidden(bool i) = 0;
	virtual Progress *GetMeter() = 0;
	virtual void SetMeter(Progress *m) = 0;
	virtual bool GetAuthed() = 0;

	// Data
	virtual GSocketI *Handle() = 0;
	
	// Connection
	virtual FtpOpenStatus Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password) = 0;
	virtual bool Close() = 0;
	virtual bool IsOpen() = 0;
	virtual void Noop() = 0;
	virtual void GetHost(GAutoString *Host, int *Port) = 0;

	// Directory
	virtual bool GetDir(GAutoString &Dir) = 0;
	virtual bool SetDir(const char *Dir) = 0;
	virtual bool CreateDir(const char *Dir) = 0;
	virtual bool DeleteDir(const char *Dir) = 0;
	virtual bool ListDir(GArray<IFtpEntry*> &Dir) = 0;
	virtual bool UpDir() = 0;
	
	// File
	virtual bool DeleteFile(const char *Remote) = 0;
	virtual bool DownloadFile(const char *Local, IFtpEntry *Remote, bool Binary = true) = 0;
	virtual bool UploadFile(const char *Local, const char *Remote, bool Binary = true) = 0;
	virtual bool RenameFile(const char *From, const char *To) = 0;
	virtual bool SetPerms(const char *File, int Perms) = 0;
	virtual bool ResumeAt(int64 Pos) = 0;
	virtual void Abort() = 0;
};

/// An implementation of the remote file system interface for FTP connections
class IFtp : public IFileProtocol
{
protected:
	class IFtpPrivate *d;

	/// The command connection
	GAutoPtr<GSocketI> Socket;	// commands

	int WriteLine(char *Msg = 0);
	int ReadLine(char *Msg = 0, int MsgSize = 0);
	bool TransferFile(const char *Local, const char *Remote, int64 RemoteSize, bool Upload, bool Binary);

	// Data connections
	char Ip[64];
	int Port;
	bool PassiveMode;
	bool ForceActive;
	bool LongList;
	bool ShowHidden;
	Progress *Meter;
	bool Authenticated;

	// Socket factory details.
	// FtpSocketFactory SockFactory;
	// void *FactoryParam;

	// State
	int64 RestorePos;
	bool AbortTransfer;
	
	bool SetupData(bool Binary);
	bool ConnectData();
	char *ToFtpCs(const char *s);
	char *FromFtpCs(const char *s);

public:
	/// Construct an FTP protocol handler.
	IFtp();
	virtual ~IFtp();

	/// \returns the current charset
	const char *GetCharset();
	/// Set the charset used for converting ftp listings to local utf
	void SetCharset(const char *cs);
	/// \returns the active connections only setting.
	bool IsForceActive() { return ForceActive; }
	/// Set the active connections only option
	void IsForceActive(bool i) { ForceActive = i; }
	bool IsLongList() { return LongList; }
	void IsLongList(bool i) { LongList = i; }
	bool IsShowHidden() { return ShowHidden; }
	void IsShowHidden(bool i) { ShowHidden = i; }
	Progress *GetMeter() { return Meter; }
	void SetMeter(Progress *m) { Meter = m; }
	bool GetAuthed() { return Authenticated; }
	const char *GetError();

	/// Returns the socket used for the command connection.
	GSocketI *Handle() { return Socket; }
	
	/// Opens a new command connection to a remote server
	FtpOpenStatus Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password);
	/// Returns the host and port
	void GetHost(GAutoString *Host, int *Port);
	/// Closes the currently active connection
	bool Close();
	/// \returns true if the connection is open and active
	bool IsOpen();
	void Noop();

	/// \returns the current remote folder.
	bool GetDir(GAutoString &Dir);
	/// Sets the current remote folder.
	bool SetDir(const char *Dir);
	/// Create a new sub-folder under the current remote folder.
	bool CreateDir(const char *Dir);
	/// Delete a sub-folder under the current folder.
	bool DeleteDir(const char *Dir);
	/// List the current remote folder contents.
	bool ListDir(GArray<IFtpEntry*> &Dir);
	/// Move up to the parent remote folder.
	bool UpDir();
	
	/// Delete a file in the current remote folder
	bool DeleteFile(const char *Remote);
	/// Download a file from the current remote folder
	bool DownloadFile(const char *Local, IFtpEntry *Remote, bool Binary = true);
	/// Upload a local file to the current remote folder
	bool UploadFile(const char *Local, const char *Remote, bool Binary = true);
	/// Rename a file or folder in the current remote folder
	bool RenameFile(const char *From, const char *To);
	/// Set the permissions on a file in the current remote folder
	bool SetPerms(const char *File, int Perms);
	/// Set the resume point before downloading a file
	bool ResumeAt(int64 Pos);
	/// Abort the current transfer
	void Abort() { AbortTransfer = true; }
};

#endif
