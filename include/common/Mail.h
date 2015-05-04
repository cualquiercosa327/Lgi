/**		\file
		\author Matthew Allen
*/

#ifndef __MAIL_H
#define __MAIL_H

#include "INet.h"
#include "Base64.h"
#include "Progress.h"
#include "GVariant.h"

#ifndef GPL_COMPATIBLE
#define GPL_COMPATIBLE						0
#endif

// Defines
#define MAX_LINE_SIZE						1024
#define MAX_NAME_SIZE						64
#define EMAIL_LINE_SIZE						76

// #define IsDigit(c) ((c) >= '0' AND (c) <= '9')

// MIME content types
#define CONTENT_NONE						0
#define CONTENT_BASE64						1
#define CONTENT_QUOTED_PRINTABLE			2
#define CONTENT_OCTET_STREAM				3

// Mail logging defines
#define MAIL_SEND_COLOUR					Rgb24(0, 0, 0xff)
#define MAIL_RECEIVE_COLOUR					Rgb24(0, 0x8f, 0)
#define MAIL_ERROR_COLOUR					Rgb24(0xff, 0, 0)
#define MAIL_WARNING_COLOUR					Rgb24(0xff, 0x7f, 0)
#define MAIL_INFO_COLOUR					Rgb24(0, 0, 0)

// Helper functions
extern void TokeniseStrList(char *Str, List<char> &Output, const char *Delim);
extern char ConvHexToBin(char c);
#define ConvBinToHex(i) (((i)<10)?'0'+(i):'A'+(i)-10)
extern void DecodeAddrName(char *Start, GAutoString &Name, GAutoString &Addr, char *DefaultDomain);
extern bool IsValidEmail(GAutoString &Email);
extern char *DecodeRfc2047(char *Str);
extern char *EncodeRfc2047(char *Str, const char *CodePage, List<char> *CharsetPrefs, int LineLength = 0);
extern char *DecodeBase64Str(char *Str, int Len = -1);
extern char *DecodeQuotedPrintableStr(char *Str, int Len = -1);
extern bool Is8Bit(char *Text);
extern int MaxLineLen(char *Text);
extern char *EncodeImapString(char *s);
extern char *DecodeImapString(char *s);

// Classes
class MailProtocol;

struct MailProtocolError
{
	int Code;
	char *Msg;

	MailProtocolError()
	{
		Code = 0;
		Msg = 0;
	}

	~MailProtocolError()
	{
		DeleteArray(Msg);
	}
};

class MailProtocolProgress
{
public:
	uint64 Start;
	int Value;
	int Range;

	MailProtocolProgress()
	{
		Empty();
	}

	void Empty()
	{
		Start = 0;
		Value = 0;
		Range = 0;
	}
};

class LogEntry
{
public:
	char *Text;
	GColour c;

	LogEntry(const char *t, int len, COLOUR col);
	~LogEntry();
};

/// Attachment descriptor
class FileDescriptor : public GBase
{
protected:
	// Global
	int64 Size;
	char *MimeType;
	char *ContentId;

	// Read from file
	GFile File;
	GStreamI *Embeded;
	bool OwnEmbeded;
	int64 Offset;
	GMutex *Lock;

	// Write to memory
	uchar *Data;
	GAutoPtr<GStream> DataStream;

public:
	FileDescriptor(GStreamI *embed, int64 Offset, int64 Size, char *Name);
	FileDescriptor(char *name);
	FileDescriptor(char *data, int64 len);
	FileDescriptor();
	~FileDescriptor();

	void SetLock(GMutex *l);
	GMutex *GetLock();
	void SetOwnEmbeded(bool i);

	// Access functions
	GStreamI *GotoObject();		// Get data to read
	uchar *GetData();			// Get data from write
	int Sizeof();
	char *GetMimeType() { return MimeType; }
	void SetMimeType(char *s) { DeleteArray(MimeType); MimeType = NewStr(s); }
	char *GetContentId() { return ContentId; }
	void SetContentId(char *s) { DeleteArray(ContentId); ContentId = NewStr(s); }

	// Decode MIME data to memory
	bool Decode(char *ContentType,
				char *ContentTransferEncoding,
				char *MimeData,
				int MimeDataLength);
};

#define MAIL_ADDR_TO				0
#define MAIL_ADDR_CC				1
#define MAIL_ADDR_BCC				2
#define MAIL_ADDR_FROM				3

/// Address dscriptor
class AddressDescriptor : public GBase
{
public:
	uint8 Status;
	uchar CC;	// MAIL_ADDR_??
	char *Name;
	char *Addr;
	
	void *Data; // application defined

	AddressDescriptor(AddressDescriptor *Copy = 0);
	~AddressDescriptor();
	void _Delete();

	void Print(char *Str, int Len);
	int Sizeof()
	{
		return	SizeofStr(Name) +
				SizeofStr(Addr);
	}

	bool Serialize(GFile &f, bool Write)
	{
		bool Status = true;
		if (Write)
		{
			WriteStr(f, Name);
			WriteStr(f, Addr);
		}
		else
		{
			DeleteArray(Name);
			Name = ReadStr(f PassDebugArgs);
			DeleteArray(Addr);
			Addr = ReadStr(f PassDebugArgs);
		}
		return Status;
	}
};

// FIELD_PRIORITY is equivilent to the header field: X-Priority
//	1 - High
//	...
//	5 - Low
#define MAIL_PRIORITY_HIGH				1
#define MAIL_PRIORITY_NORMAL			3 // ???
#define MAIL_PRIORITY_LOW				5

class MailMessage
{
	char*						Text;
	char*						TextCharset;
	
	char*						Html;
	char*						HtmlCharset;

public:
	List<AddressDescriptor>		To;
	AddressDescriptor			*From;
	AddressDescriptor			*Reply;
	GAutoString					Subject;
	GAutoString					MessageID;
	GAutoString					FwdMsgId;
	GAutoString					BounceMsgId;
	List<FileDescriptor>		FileDesc;
	char*						InternetHeader;
	char						Priority;
	int							MarkColour;
	uint8						DispositionNotificationTo; // read receipt
	GAutoString					References;
	
	// Protocol specific
	GAutoString					UserData;

	// Class
	MailMessage();
	virtual ~MailMessage();
	void Empty();

	virtual char *GetBody();
	virtual bool SetBody(const char *Txt, int Bytes = -1, bool Copy = true, const char *Cs = 0);
	virtual char *GetBodyCharset();
	virtual bool SetBodyCharset(const char *Cs);

	virtual char *GetHtml();
	virtual bool SetHtml(const char *Txt, int Bytes = -1, bool Copy = true, const char *Cs = 0);
	virtual char *GetHtmlCharset();
	virtual bool SetHtmlCharset(const char *Cs);
	
	// Conversion to/from MIME
	GStringPipe					*Raw;

	// High level encoding functions
	bool Encode					(GStreamI &Out, GStream *HeadersSink, MailProtocol *Protocol, bool Mime = true);
	bool EncodeHeaders			(GStreamI &Out, MailProtocol *Protocol, bool Mime = true);
	bool EncodeBody				(GStreamI &Out, MailProtocol *Protocol, bool Mime = true);

	// Encoding mime segment data	
	int EncodeText				(GStreamI &Out, GStreamI &In);
	int EncodeQuotedPrintable	(GStreamI &Out, GStreamI &In);
	int EncodeBase64			(GStreamI &Out, GStreamI &In);
};

/// Base class for mail protocol implementations
class MailProtocol
{
protected:
	char Buffer[4<<10];
	GMutex SocketLock;
	GAutoPtr<GSocketI> Socket;

	bool Error(const char *file, int line, const char *msg, ...);
	bool Read();
	bool Write(const char *Buf = NULL, bool Log = false);
	
	virtual void OnUserMessage(char *Str) {}

public:
	// Logging
	GStreamI *Logger;
	void Log(const char *Str, GSocketI::SocketMsgType type);

	// Task Progress
	MailProtocolProgress *Items;
	MailProtocolProgress *Transfer;

	// Settings
	GAutoString ServerMsg;
	char *ProgramName;
	char *DefaultDomain;
	char *ExtraOutgoingHeaders;
	List<char> CharsetPrefs;

	// Object
	MailProtocol();
	virtual ~MailProtocol();

	// Methods
	// GSocketI *GetSocket() { return Socket; }
	
	/// Thread safe hard close (quit now)
	bool CloseSocket()
	{
		GMutex::Auto l(&SocketLock, _FL);

		if (Socket != NULL)
			return Socket->Close() != 0;

		return false;
	}

};

/////////////////////////////////////////////////////////////////////
// Mail IO parent classes

/// Enable STARTTLS support (requires an SSL capable socket)
#define MAIL_USE_STARTTLS			0x01
/// Use authentication
#define MAIL_USE_AUTH				0x02
/// Force the use of PLAIN type authentication
#define MAIL_USE_PLAIN				0x04
/// Force the use of LOGIN type authentication
#define MAIL_USE_LOGIN				0x08
/// Force the use of NTLM type authentication
#define MAIL_USE_NTLM				0x10
/// Secure auth
#define MAIL_SECURE_AUTH			0x20
/// Use SSL
#define MAIL_SSL					0x40

/// Mail sending protocol
class MailSink : public MailProtocol
{
public:
	/// Connection setup/shutdown
	virtual bool Open
	(
		/// The transport layer to use
		GSocketI *S,
		/// The host to connect to
		const char *RemoteHost,
		/// The local domain
		const char *LocalDomain,
		/// The sink username (or NULL)
		const char *UserName,
		/// The sink password (or NULL)
		const char *Password,
		/// The port to connect with or 0 for default.
		int Port,
		/// Options: Use any of #MAIL_SSL, #MAIL_USE_STARTTLS, #MAIL_SECURE_AUTH, #MAIL_USE_PLAIN, #MAIL_USE_LOGIN etc or'd together.
		int Flags
	) = 0;
	/// Close the connection
	virtual bool Close() = 0;

	// Commands available while connected

	/// Write the email's contents into the GStringPipe returned from
	/// SendStart and then call SendEnd to finish the transaction
	virtual GStringPipe *SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = 0) = 0;
	
	/// Finishes the mail send
	virtual bool SendEnd(GStringPipe *Sink) = 0;

	// Deprecated
	virtual bool Send(MailMessage *Msg, bool Mime) { return false; }
};

struct ImapMailFlags
{
	uint8 ImapAnswered : 1;
	uint8 ImapDeleted : 1;
	uint8 ImapDraft : 1;
	uint8 ImapFlagged : 1;
	uint8 ImapRecent : 1;
	uint8 ImapSeen : 1;
	uint8 ImapExpunged :1;

	ImapMailFlags(char *init = 0)
	{
		ImapAnswered = 0;
		ImapDeleted = 0;
		ImapDraft = 0;
		ImapFlagged = 0;
		ImapRecent = 0;
		ImapSeen = 0;
		ImapExpunged = 0;

		if (init)
			Set(init);
	}

	char *Get()
	{
		char s[256] = "";
		int ch = 0;

		if (ImapAnswered) ch += sprintf_s(s+ch, sizeof(s)-ch, "\\answered ");
		if (ImapDeleted) ch += sprintf_s(s+ch, sizeof(s)-ch, "\\deleted ");
		if (ImapDraft) ch += sprintf_s(s+ch, sizeof(s)-ch, "\\draft ");
		if (ImapFlagged) ch += sprintf_s(s+ch, sizeof(s)-ch, "\\flagged ");
		if (ImapRecent) ch += sprintf_s(s+ch, sizeof(s)-ch, "\\recent ");
		if (ImapSeen) ch += sprintf_s(s+ch, sizeof(s)-ch, "\\seen ");

		if (ch == 0)
			return NULL;

		LgiAssert(ch < sizeof(s));
		s[--ch] = 0;
		return NewStr(s);
	}

	void Set(const char *s)
	{
		ImapAnswered = false;
		ImapDeleted = false;
		ImapDraft = false;
		ImapFlagged = false;
		ImapRecent = false;
		ImapSeen = false;
		ImapExpunged = false;

		if (!s) s = "";
		while (*s)
		{
			if (*s == '/' || *s == '\\')
			{
				while (*s == '/' || *s == '\\')
					s++;
				const char *e = s;
				while (*e && isalpha(*e))
					e++;

				if (!_strnicmp(s, "answered", e-s))
					ImapAnswered = true;
				else if (!_strnicmp(s, "deleted", e-s))
					ImapDeleted = true;
				else if (!_strnicmp(s, "draft", e-s))
					ImapDraft = true;
				else if (!_strnicmp(s, "flagged", e-s))
					ImapFlagged = true;
				else if (!_strnicmp(s, "recent", e-s))
					ImapRecent = true;
				else if (!_strnicmp(s, "seen", e-s))
					ImapSeen = true;
				
				s = e;
			}
			else s++;
		}
	}

	bool operator ==(ImapMailFlags &f)
	{
		return 	ImapAnswered == f.ImapAnswered &&
				ImapDeleted == f.ImapDeleted &&
				ImapDraft == f.ImapDraft &&
				ImapFlagged == f.ImapFlagged &&
				ImapRecent == f.ImapRecent &&
				ImapSeen == f.ImapSeen &&
				ImapExpunged == f.ImapExpunged;
	}

	bool operator !=(ImapMailFlags &f)
	{
		return !(ImapAnswered == f.ImapAnswered &&
				ImapDeleted == f.ImapDeleted &&
				ImapDraft == f.ImapDraft &&
				ImapFlagged == f.ImapFlagged &&
				ImapRecent == f.ImapRecent &&
				ImapSeen == f.ImapSeen &&
				ImapExpunged == f.ImapExpunged);
	}
};

/// A bulk mail handling class
class MailTransaction
{
public:
	/// The index of the mail in the folder
	int Index;
	
	/// \sa #MAIL_POSTED_TO_GUI, #MAIL_EXPLICIT
	int Flags;
	
	// bool Delete;
	bool Status;
	bool Oversize;

	/// The mail protocol handler writes the email to this stream
	GStreamI *Stream;

	/// Flags used on the IMAP protocolf
	ImapMailFlags Imap;

	/// The user app can use this for whatever
	void *UserData;

	MailTransaction();
	~MailTransaction();
};

/// Return code from MailSrcCallback
enum MailSrcStatus
{
	/// Download the whole email
	DownloadAll,
	/// Download just the top part
	DownloadTop,
	/// Skip this email
	DownloadNone,
	/// About the whole receive
	DownloadAbort
};

/// The callback function used by MailSource::Receive
typedef MailSrcStatus (*MailSrcCallback)
(
	/// The currently executing transaction
	MailTransaction *Trans,
	/// The size of the email about to be downloaded
	uint64 Size,
	/// If DownloadTop is returned, you can set the number of lines to retreive here
	int *LinesToDownload,
	/// The data cookie passed into MailSource::Receive
	void *Data
);

/// The callback function used by MailSource::Receive
typedef bool (*MailReceivedCallback)
(
	/// The currently executing transaction
	MailTransaction *Trans,
	/// The data cookie passed into MailSource::Receive
	void *Data
);

/// Collection of callbacks called during mail receive. You should zero this
/// entire object before using it. Because if someone adds new callbacks after
/// you write the calling code you wouldn't want to leave some callbacks un-
/// initialized. A NULL callback is ignored.
struct MailCallbacks
{
	/// The callback data
	void *CallbackData;
	/// Called before receiving mail
	MailSrcCallback OnSrc;
	/// Called after mail received
	MailReceivedCallback OnReceive;
};

/*
/// Enable STARTTLS support (requires an SSL capable socket)
#define MAIL_SOURCE_STARTTLS			0x01
/// Use authentication
#define MAIL_SOURCE_AUTH				0x02
/// Force the use of PLAIN type authentication
#define MAIL_SOURCE_USE_PLAIN			0x04
/// Force the use of LOGIN type authentication
#define MAIL_SOURCE_USE_LOGIN			0x08
*/

/// A generic mail source object
class MailSource : public MailProtocol
{
public:
	/// Opens a connection to the server
	virtual bool Open
	(
		/// The transport socket
		GSocketI *S,
		/// The hostname or IP of the server
		char *RemoteHost,
		/// The port on the host to connect to
		int Port,
		/// The username for authentication
		char *User,
		/// The password for authentication
		char *Password,
		/// A cookie that the implementation can store things in, which persists across sessions. (Dynamically allocated string)
		char *&Cookie,
		/// Any optional flags: #MAIL_SOURCE_STARTTLS, #MAIL_SOURCE_AUTH, #MAIL_SOURCE_USE_PLAIN, #MAIL_SOURCE_USE_LOGIN
		int Flags = 0) = 0;
	/// Closes the connection
	virtual bool Close() = 0;

	/// Returns the number of messages available on the server
	virtual int GetMessages() = 0;
	/// Receives a list of messages from the server.
	virtual bool Receive
	(
		/// An array of messages to receive. The MailTransaction objects contains the index of the message to receive
		/// and various status values returned after the operation.
		GArray<MailTransaction*> &Trans,
		/// An optional set of callback functions.
		MailCallbacks *Callbacks = 0
	) = 0;
	/// Deletes a message on the server
	virtual bool Delete(int Message) = 0;
	/// Gets the size of the message on the server
	virtual int Sizeof(int Message) = 0;
	/// Gets the size of all the messages on the server
	virtual bool GetSizes(GArray<int> &Sizes) { return false; }
	/// Gets the unique identifier of the message
	virtual bool GetUid(int Message, char *Id, int IdLen) = 0;
	/// Gets the unique identifiers of a list of messages
	virtual bool GetUidList(List<char> &Id) = 0;
	/// Gets the headers associated with a given message
	virtual char *GetHeaders(int Message) = 0;
	/// Sets the proxy server. e.g. HTTP mail.
	virtual void SetProxy(char *Server, int Port) {}
};

/////////////////////////////////////////////////////////////////////
// Mail IO implementations

/// SMTP implementation
class MailSmtp : public MailSink
{
protected:
	bool ReadReply(const char *Str, GStringPipe *Pipe = 0, MailProtocolError *Err = 0);
	bool WriteText(const char *Str);

public:
	MailSmtp();
	~MailSmtp();

	bool Open(GSocketI *S, const char *RemoteHost, const char *LocalDomain, const char *UserName, const char *Password, int Port = 0, int Flags = 0);
	bool Close();

	bool SendToFrom(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = 0);
	GStringPipe *SendData(MailProtocolError *Err = 0);

	GStringPipe *SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = 0);
	bool SendEnd(GStringPipe *Sink);

	bool Send(MailMessage *Msg, bool Mime = false);
};

class MailSendFolder : public MailSink
{
	class MailPostFolderPrivate *d;

public:
	MailSendFolder(char *Path);
	~MailSendFolder();

	bool Open(GSocketI *S, const char *RemoteHost, const char *LocalDomain, const char *UserName, const char *Password, int Port = 0, int Flags = 0);
	bool Close();

	GStringPipe *SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = 0);
	bool SendEnd(GStringPipe *Sink);
};

class MailPop3 : public MailSource
{
protected:
	bool ReadReply();
	bool ReadMultiLineReply(char *&Str);
	int GetInt();
	bool MailIsEnd(char *Ptr, int Len);
	bool ListCmd(const char *Cmd, GHashTbl<const char *, bool> &Results);

	const char *End;
	const char *Marker;
	int Messages;

public:
	MailPop3();
	~MailPop3();

	// Connection
	bool Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags = 0);
	bool Close();

	// Commands available while connected
	int GetMessages();
	bool Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = 0);
	bool Delete(int Message);
	int Sizeof(int Message);
	bool GetSizes(GArray<int> &Sizes);
	bool GetUid(int Message, char *Id, int IdLen);
	bool GetUidList(List<char> &Id);
	char *GetHeaders(int Message);
};

class MailReceiveFolder : public MailSource
{
protected:
	class MailReceiveFolderPrivate *d;

public:
	MailReceiveFolder(char *Path);
	~MailReceiveFolder();

	// Connection
	bool Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags = 0);
	bool Close();

	// Commands available while connected
	int GetMessages();
	bool Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = 0);
	bool Delete(int Message);
	int Sizeof(int Message);
	bool GetUid(int Message, char *Id, int IdLen);
	bool GetUidList(List<char> &Id);
	char *GetHeaders(int Message);
};

class MailPhp : public MailSource
{
protected:
	class MailPhpPrivate *d;

	bool Get(GSocketI *S, char *Uri, GStream &Out, bool ChopDot);

public:
	MailPhp();
	~MailPhp();

	// Connection
	bool Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags = 0);
	bool Close();

	// Commands available while connected
	int GetMessages();
	bool Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = 0);
	bool Delete(int Message);
	int Sizeof(int Message);
	bool GetSizes(GArray<int> &Sizes);
	bool GetUid(int Message, char *Id, int IdLen);
	bool GetUidList(List<char> &Id);
	char *GetHeaders(int Message);
	void SetProxy(char *Server, int Port);
};

class MailImapFolder
{
	friend class MailIMap;
	friend struct ImapThreadPrivate;

	char Sep;
	char *Path;

public:
	bool NoSelect;
	bool NoInferiors;
	bool Marked;
	int Exists;
	int Recent;
	int Deleted;
	// int UnseenIndex;

	MailImapFolder();
	virtual ~MailImapFolder();

	char *GetPath();
	void SetPath(char *s);
	char *GetName();
	void SetName(char *s);
	void operator =(GHashTbl<const char*,int> &v);
};

class MailIMap : public MailSource, public GMutex
{
protected:
	class MailIMapPrivate *d;

	char Buf[1024];
	List<char> Uid;
	GStringPipe ReadBuf;
	List<char> Dialog;

	void ClearDialog();
	void ClearUid();
	bool FillUidList();
	bool WriteBuf(bool ObsurePass = false, const char *Buffer = 0);
	bool ReadResponse(int Cmd = -1, bool Plus = false);
	bool Read(GStreamI *Out = 0);
	bool ReadLine();
	bool IsResponse(const char *Buf, int Cmd, bool &Ok);

public:
	// Typedefs
	struct Untagged
	{
		GAutoString Cmd;
		GAutoString Param;
		int Id;
	};
	
	struct OAuthParams
	{
		GString ClientID;
		GString ClientSecret;
		GString RedirURIs;
		GString AuthUri;
		GString RevokeUri;
		GString TokenUri;
		GUri Proxy;
		
		GString AccessToken;
		GString RefreshToken;
		int ExpiresIn;
		
		bool IsValid()
		{
			return ClientID &&
				ClientSecret &&
				RedirURIs &&
				AuthUri &&
				RevokeUri &&
				TokenUri;
		}
	};

	/// This callback is used to notify the application using this object of IMAP fetch responses.
	/// \returns true if the application wants to continue reading and has taken ownership of the strings in "Parts".
	typedef bool (*FetchCallback)
	(
		/// The IMAP object
		class MailIMap *Imap,
		/// The message sequence number
		char *Msg,
		/// The fetch parts (which the callee needs to own if returning true)
		GHashTbl<const char*, char*> &Parts,
		/// The user data passed to the Fetch function
		void *UserData
	);

	// Object
	MailIMap();
	~MailIMap();

	// General
	char GetFolderSep();
	char *EncodePath(const char *Path);
	char *GetCurrentPath();
	bool GetExpungeOnExit();
	void SetExpungeOnExit(bool b);
	bool ServerOption(char *Opt);
	bool IsOnline();
	const char *GetWebLoginUri();
	void SetOAuthParams(OAuthParams &p);
	void SetParentWindow(GViewI *wnd);

	// Connection
	bool Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags = 0);
	bool Close(); // Non-threadsafe soft close (normal operation)
	bool GetCapabilities(GArray<char*> &s);

	// Commands available while connected
	bool Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = 0);
	int GetMessages();
	bool Delete(int Message);
	int Sizeof(int Message);
	bool GetSizes(GArray<int> &Sizes);
	bool GetUid(int Message, char *Id, int IdLen);
	bool GetUidList(List<char> &Id);
	char *GetHeaders(int Message);
	char *SequenceToString(GArray<int> *Seq);

	// Imap specific commands

	/// This method wraps the imap FETCH command
	bool Fetch
	(
		/// True if 'Seq' is a UID, otherwise it's a sequence
		bool ByUid,
		/// The sequence number or UID
		const char *Seq,
		/// The parts to retrieve
		const char *Parts,
		/// Data is returned to the caller via this callback function
		FetchCallback Callback,
		/// A user defined param to pass back to the 'Callback' function.
		void *UserData,
		/// [Optional] The raw data received will be written to this stream if provided, else NULL.
		GStreamI *RawCopy = 0
	);

	/// Appends a message to the specified folder
	bool Append
	(
		/// The folder to write to
		char *Folder,
		/// [Optional] Flags for the message
		ImapMailFlags *Flags,
		/// The rfc822 body of the message
		char *Msg,
		/// [Out] The UID of the message appended (if known, can be empty if not known)
		GAutoString &NewUid
	);

	bool GetFolders(List<MailImapFolder> &Folders);
	bool SelectFolder(const char *Path, GHashTbl<const char*,int> *Values = 0);
	char *GetSelectedFolder();
	int GetMessages(const char *Path);
	bool CreateFolder(MailImapFolder *f);
	bool DeleteFolder(char *Path);
	bool RenameFolder(char *From, char *To);
	bool SetFolderFlags(MailImapFolder *f);
	
	/// Expunges (final delete) any deleted messages the current folder.
	bool ExpungeFolder();
	
	// Uid methods
	bool CopyByUid(GArray<char*> &InUids, char *DestFolder);
	bool SetFlagsByUid(GArray<char*> &Uids, const char *Flags);

	/// Idle processing...
	/// \returns true if something happened
	bool StartIdle();
	bool OnIdle(int Timeout, GArray<Untagged> &Resp);
	bool FinishIdle();
	bool Poll(int *Recent = 0, GArray<GAutoString> *New = 0);
	bool Status(char *Path, int *Recent);
	bool Search(bool Uids, GArray<GAutoString> &SeqNumbers, const char *Filter);

	// Utility
	static bool Http(GSocketI *S,
					GAutoString *OutHeaders,
					GAutoString *OutBody,
					int *StatusCode,
					const char *InMethod,
					const char *InUri,
					const char *InHeaders,
					const char *InBody);
};

#endif
