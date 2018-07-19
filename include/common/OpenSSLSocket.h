#ifndef _OPEN_SSL_SOCKET_H_
#define _OPEN_SSL_SOCKET_H_

#include "GLibraryUtils.h"
#include "LCancel.h"

// If you get a compile error on Linux:
// sudo apt-get install libssl-dev
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

#define SslSocket_LogFile				"LogFile"
#define SslSocket_LogFormat				"LogFmt"

class SslSocket :
	public GSocketI,
	virtual public GDom
{
	friend class OpenSSL;
	struct SslSocketPriv *d;
	
	LMutex Lock;
	BIO *Bio;
	SSL *Ssl;
	GString ErrMsg;

	// Local stuff
	virtual void Log(const char *Str, ssize_t Bytes, SocketMsgType Type);
	void SslError(const char *file, int line, const char *Msg);
	GStream *GetLogStream();
	void DebugTrace(const char *fmt, ...);

public:
	static bool DebugLogging;

	SslSocket(GStreamI *logger = NULL, GCapabilityClient *caps = NULL, bool SslOnConnect = false, bool RawLFCheck = false);
	~SslSocket();

	void SetLogger(GStreamI *logger);
	void SetSslOnConnect(bool b);
	LCancel *GetCancel();
	void SetCancel(LCancel *c);
	
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

	ssize_t Write(const void *Data, ssize_t Len, int Flags = 0);
	ssize_t Read(void *Data, ssize_t Len, int Flags = 0);
	void OnWrite(const char *Data, ssize_t Len);
	void OnRead(char *Data, ssize_t Len);

	bool IsReadable(int TimeoutMs = 0);
	bool IsWritable(int TimeoutMs = 0);
	bool IsBlocking();
	void IsBlocking(bool block);

	bool SetVariant(const char *Name, GVariant &Val, char *Arr = NULL);
	bool GetVariant(const char *Name, GVariant &Val, char *Arr = NULL);

	GStreamI *Clone();
	const char *GetErrorString();
};

extern bool StartSSL(GAutoString &ErrorMsg, SslSocket *Sock);
extern void EndSSL();

#endif