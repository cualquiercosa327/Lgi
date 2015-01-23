#ifndef _OPEN_SSL_SOCKET_H_
#define _OPEN_SSL_SOCKET_H_

#include "GLibraryUtils.h"

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

#define SslSocket_LogFile				"LogFile"
#define SslSocket_LogFormat				"LogFmt"

class SslSocket :
	public GSocketI, virtual public GDom
{
	friend class OpenSSL;
	struct SslSocketPriv *d;
	
	GMutex Lock;
	BIO *Bio;
	SSL *Ssl;

	// Local stuff
	virtual void Log(const char *Str, SocketMsgType Type);
	void Error(const char *file, int line, const char *Msg);
	GStream *GetLogStream();
	void DebugTrace(const char *fmt, ...);

public:
	static bool DebugLogging;

	SslSocket(GStreamI *logger = NULL, GCapabilityClient *caps = NULL, bool SslOnConnect = false, bool RawLFCheck = false);
	~SslSocket();

	void SetLogger(GStreamI *logger);
	void SetSslOnConnect(bool b);
	
	// Socket
	OsSocket Handle(OsSocket Set = INVALID_SOCKET);
	bool IsOpen();
	int Open(const char *HostAddr, int Port);
	int Close();
	bool Listen(int Port = 0);
	void OnError(int ErrorCode, const char *ErrorDescription);
	void OnInformation(const char *Str);
	int GetTimeout();
	void SetTimeout(int ms);

	int Write(const void *Data, int Len, int Flags);
	int Read(void *Data, int Len, int Flags);
	void OnWrite(const char *Data, int Len);
	void OnRead(char *Data, int Len);

	bool IsReadable(int TimeoutMs = 0);
	bool IsWritable(int TimeoutMs = 0);
	bool IsBlocking() { return false; }

	bool SetVariant(const char *Name, GVariant &Val, char *Arr = NULL);
	bool GetVariant(const char *Name, GVariant &Val, char *Arr = NULL);

	GStreamI *Clone();
};

extern bool StartSSL(GAutoString &ErrorMsg, SslSocket *Sock);
extern void EndSSL();

#endif