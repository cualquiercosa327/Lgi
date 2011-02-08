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
extern void TokeniseStrList(char *Str, List<char> &Output, char *Delim);
extern char ConvHexToBin(char c);
#define ConvBinToHex(i) (((i)<10)?'0'+(i):'A'+(i)-10)
extern void DecodeAddrName(char *Start, GAutoString &Name, GAutoString &Addr, char *DefaultDomain);
extern char *DecodeRfc2047(char *Str);
extern char *EncodeRfc2047(char *Str, char *CodePage, List<char> *CharsetPrefs, int LineLength = 0);
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
	int Start;
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
	COLOUR c;

	LogEntry(char *t, int len, COLOUR col)
	{
		c = col;
		Text = 0;
		
		if (t)
		{
			char *n = strnchr(t, '\r', len);
			if (n)
			{
				Text = NewStr(t, n-t);
			}
			else
			{
				Text = NewStr(t, len);
			}
		}
	}

	~LogEntry()
	{
		DeleteArray(Text);
	}
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
	GSemaphore *Lock;

	// Write to memory
	uchar *Data;
	GAutoPtr<GStream> DataStream;

public:
	FileDescriptor(GStreamI *embed, int64 Offset, int64 Size, char *Name);
	FileDescriptor(char *name);
	FileDescriptor(char *data, int64 len);
	FileDescriptor();
	~FileDescriptor();

	void SetLock(GSemaphore *l);
	GSemaphore *GetLock();
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

	void Print(char *Str);
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
	void						*Private;

	// Class
	MailMessage();
	virtual ~MailMessage();
	void Empty();

	virtual char *GetBody();
	virtual bool SetBody(char *Txt, int Bytes = -1, bool Copy = true, char *Cs = 0);
	virtual char *GetBodyCharset();
	virtual bool SetBodyCharset(char *Cs, bool Copy = true);

	virtual char *GetHtml();
	virtual bool SetHtml(char *Txt, int Bytes = -1, bool Copy = true, char *Cs = 0);
	virtual char *GetHtmlCharset();
	virtual bool SetHtmlCharset(char *Cs, bool Copy = true);
	
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
	GSemaphore SocketLock;
	GAutoPtr<GSocketI> Socket;

	bool Error(char *file, int line, char *msg, ...);
	bool Read();
	bool Write(char *Buf = NULL, bool Log = false);
	
	virtual void OnUserMessage(char *Str) {}

public:
	// Logging
	GStreamI *Logger;
	void Log(const char *Str, COLOUR c);

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
		GSemaphore::Auto l(&SocketLock, _FL);

		if (Socket)
			return Socket->Close();

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
		char *RemoteHost,
		/// The local domain
		char *LocalDomain,
		/// The sink username (or NULL)
		char *UserName,
		/// The sink password (or NULL)
		char *Password,
		/// The port to connect with or 0 for default (25)
		int Port,
		/// Options: Use any of #MAIL_SINK_STARTTLS, #MAIL_SINK_AUTH, #MAIL_SINK_USE_PLAIN, #MAIL_SINK_USE_LOGIN or'd together.
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
		char s[256] = "", *c = s;

		if (ImapAnswered) c += sprintf(c, "\\answered ");
		if (ImapDeleted) c += sprintf(c, "\\deleted ");
		if (ImapDraft) c += sprintf(c, "\\draft ");
		if (ImapFlagged) c += sprintf(c, "\\flagged ");
		if (ImapRecent) c += sprintf(c, "\\recent ");
		if (ImapSeen) c += sprintf(c, "\\seen ");

		if (c == s)
			return 0;

		LgiAssert(c < s + sizeof(s));
		c--;
		*c = 0;
		return NewStr(s);
	}

	void Set(char *s)
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
				char *e = s;
				while (*e && isalpha(*e))
					e++;

				if (!strnicmp(s, "answered", e-s))
					ImapAnswered = true;
				else if (!strnicmp(s, "deleted", e-s))
					ImapDeleted = true;
				else if (!strnicmp(s, "draft", e-s))
					ImapDraft = true;
				else if (!strnicmp(s, "flagged", e-s))
					ImapFlagged = true;
				else if (!strnicmp(s, "recent", e-s))
					ImapRecent = true;
				else if (!strnicmp(s, "seen", e-s))
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
	int64 Size,
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

/// Enable STARTTLS support (requires an SSL capable socket)
#define MAIL_SOURCE_STARTTLS			0x01
/// Use authentication
#define MAIL_SOURCE_AUTH				0x02
/// Force the use of PLAIN type authentication
#define MAIL_SOURCE_USE_PLAIN			0x04
/// Force the use of LOGIN type authentication
#define MAIL_SOURCE_USE_LOGIN			0x08

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
	virtual bool GetUid(int Message, char *Id) = 0;
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
	bool ReadReply(char *Str, GStringPipe *Pipe = 0, MailProtocolError *Err = 0);
	bool WriteText(char *Str);

public:
	MailSmtp();
	~MailSmtp();

	bool Open(GSocketI *S, char *RemoteHost, char *LocalDomain, char *UserName, char *Password, int Port = SMTP_PORT, int Flags = 0);
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

	bool Open(GSocketI *S, char *RemoteHost, char *LocalDomain, char *UserName, char *Password, int Port = 0, int Flags = 0);
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
	bool ListCmd(char *Cmd, GHashTable &Results);

	char *End;
	char *Marker;
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
	bool GetUid(int Message, char *Id);
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
	bool GetUid(int Message, char *Id);
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
	bool GetUid(int Message, char *Id);
	bool GetUidList(List<char> &Id);
	char *GetHeaders(int Message);
	void SetProxy(char *Server, int Port);
};

class MailImapFolder
{
	friend class MailIMap;
	friend class ImapThreadPrivate;

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
	void operator =(GHashTbl<char*,int> &v);
};

class MailIMap : public MailSource, public GSemaphore
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
	bool ReadResponse(int Cmd = -1, GStringPipe *Out = 0, bool Plus = false);
	bool Read(GStreamI *Out = 0);
	bool ReadLine();

public:
	// Typedefs
	struct Untagged
	{
		GAutoString Cmd;
		GAutoString Param;
		int Id;
	};

	typedef bool (*FetchCallback)(class MailIMap *Imap, char *Msg, GHashTbl<char*, char*> &Parts, void *UserData);

	// Object
	MailIMap();
	~MailIMap();

	// General
	char GetFolderSep();
	char *EncodePath(char *Path);
	char *GetCurrentPath();
	bool GetExpungeOnExit();
	void SetExpungeOnExit(bool b);
	bool ServerOption(char *Opt);
	bool IsOnline();

	// Connection
	bool Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags = 0);
	bool Close(); // Non-threadsafe soft close (normal operation)
	bool GetCapabilities(GArray<char*> &s);

	// Commands available while connected
	bool Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = 0);
	bool GetParts(int Message, GStreamI &Out, char *Parts, char **Flags = 0);
	int GetMessages();
	bool Delete(int Message);
	int Sizeof(int Message);
	bool GetUid(int Message, char *Id);
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
		char *Seq,
		/// The parts to retrieve
		char *Parts,
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
	bool SelectFolder(char *Path, GHashTbl<char*,int> *Values = 0);
	char *GetSelectedFolder();
	int GetMessages(char *Path);
	bool CreateFolder(MailImapFolder *f);
	bool DeleteFolder(char *Path);
	bool RenameFolder(char *From, char *To);
	bool SetFolderFlags(MailImapFolder *f);
	
	/// Expunges (final delete) any deleted messages the current folder.
	bool ExpungeFolder();
	
	// Uid methods
	bool CopyByUid(GArray<char*> &InUids, char *DestFolder);
	bool SetFlagsByUid(GArray<char*> &Uids, char *Flags);

	/// Idle processing...
	/// \returns true if something happened
	bool StartIdle();
	bool OnIdle(int Timeout, GArray<Untagged> &Resp);
	bool FinishIdle();
	bool Poll(int *Recent = 0, GArray<GAutoString> *New = 0);
	bool Status(char *Path, int *Recent);
	bool Search(bool Uids, GArray<GAutoString> &SeqNumbers, char *Filter);

	// Utility
	static bool Http(GSocketI *S,
					GAutoString *OutHeaders,
					GAutoString *OutBody,
					GAutoString *OutMsg,
					char *InMethod,
					char *InUri,
					char *InHeaders,
					char *InBody);
};

#endif
