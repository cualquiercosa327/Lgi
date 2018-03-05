/*hdr
**      FILE:           OpenSSLSocket.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           24/9/2004
**      DESCRIPTION:    Open SSL wrapper socket
**
**      Copyright (C) 2004-2014, Matthew Allen
**              fret@memecode.com
**
*/

#include <stdio.h>
#ifdef WINDOWS
#pragma comment(lib,"Ws2_32.lib")
#else
#include <unistd.h>
#endif

#include "Lgi.h"
#include "OpenSSLSocket.h"
#ifdef WIN32
#include <mmsystem.h>
#endif
#include "GToken.h"
#include "GVariant.h"
#include "INet.h"

#define PATH_OFFSET				"../"
#ifdef WIN32
#define SSL_LIBRARY				"ssleay32"
#define EAY_LIBRARY             "libeay32"
#else
#define SSL_LIBRARY				"libssl"
#endif
#define HasntTimedOut()			((To < 0) || (LgiCurrentTime() - Start < To))

static const char*
	MinimumVersion				= "1.0.1g";

void
SSL_locking_function(int mode, int n, const char *file, int line);
unsigned long
SSL_id_function();

class LibSSL : public GLibrary
{
public:
	LibSSL()
	{
		char p[MAX_PATH];
		#if defined MAC
		if (LgiGetExeFile(p, sizeof(p)))
		{
			LgiMakePath(p, sizeof(p), p, "Contents/MacOS/libssl.1.0.0.dylib");
			if (FileExists(p))
			{
				Load(p);
			}
		}

        if (!IsLoaded())
        {
			Load("/opt/local/lib/" SSL_LIBRARY);
		}
		#elif defined LINUX
		if (LgiGetExePath(p, sizeof(p)))
		{
			LgiMakePath(p, sizeof(p), p, "libssl.so");
			if (FileExists(p))
			{
			    LgiTrace("%s:%i - loading SSL library '%s'\n", _FL, p);
				Load(p);
			}
		}
		#endif

		if (!IsLoaded())
			Load(SSL_LIBRARY);

		if (!IsLoaded())
		{
			LgiGetExePath(p, sizeof(p));
			LgiMakePath(p, sizeof(p), p, PATH_OFFSET "../OpenSSL");
			#ifdef WIN32
			char old[300];
			FileDev->GetCurrentFolder(old, sizeof(old));
			FileDev->SetCurrentFolder(p);
			#endif
			Load(SSL_LIBRARY);
			#ifdef WIN32
			FileDev->SetCurrentFolder(old);
			#endif
		}
    }

	#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	DynFunc0(int, OPENSSL_library_init);
	DynFunc0(int, OPENSSL_load_error_strings);
	DynFunc2(int, OPENSSL_init_crypto, uint64_t, opts, const OPENSSL_INIT_SETTINGS *, settings);
	DynFunc2(int, OPENSSL_init_ssl, uint64_t, opts, const OPENSSL_INIT_SETTINGS *, settings);
	#else
	DynFunc0(int, SSL_library_init);
	DynFunc0(int, SSL_load_error_strings);
	#endif
	DynFunc1(int, SSL_open, SSL*, s);
	DynFunc1(int, SSL_connect, SSL*, s);
	DynFunc4(long, SSL_ctrl, SSL*, ssl, int, cmd, long, larg, void*, parg);
	DynFunc1(int, SSL_shutdown, SSL*, s);
	DynFunc1(int, SSL_free, SSL*, ssl);
	DynFunc1(int, SSL_get_fd, const SSL *, s);
	DynFunc2(int, SSL_set_fd, SSL*, s, int, fd);
	DynFunc1(SSL*, SSL_new, SSL_CTX*, ctx);
	DynFunc1(BIO*, BIO_new_ssl_connect, SSL_CTX*, ctx);
	DynFunc1(X509*, SSL_get_peer_certificate, SSL*, s);
	DynFunc1(int, SSL_set_connect_state, SSL*, s);
	DynFunc1(int, SSL_set_accept_state, SSL*, s);
	DynFunc2(int, SSL_get_error, SSL*, s, int, ret_code);
	DynFunc3(int, SSL_set_bio, SSL*, s, BIO*, rbio, BIO*, wbio);
	DynFunc3(int, SSL_write, SSL*, ssl, const void*, buf, int, num);
	DynFunc3(int, SSL_read, SSL*, ssl, const void*, buf, int, num);
	DynFunc1(int, SSL_pending, SSL*, ssl);
	DynFunc1(BIO *,	SSL_get_rbio, const SSL *, s);
	DynFunc1(int, SSL_accept, SSL *, ssl);
	
	DynFunc0(SSL_METHOD*, SSLv23_client_method);
	DynFunc0(SSL_METHOD*, SSLv23_server_method);

  	DynFunc1(SSL_CTX*, SSL_CTX_new, SSL_METHOD*, meth);	
	DynFunc3(int, SSL_CTX_load_verify_locations, SSL_CTX*, ctx, const char*, CAfile, const char*, CApath);
	DynFunc3(int, SSL_CTX_use_certificate_file, SSL_CTX*, ctx, const char*, file, int, type);
	DynFunc3(int, SSL_CTX_use_PrivateKey_file, SSL_CTX*, ctx, const char*, file, int, type);
	DynFunc1(int, SSL_CTX_check_private_key, const SSL_CTX*, ctx);
	DynFunc1(int, SSL_CTX_free, SSL_CTX*, ctx);

#ifdef WIN32
// If this is freaking you out then good... openssl-win32 ships 
// in 2 DLL's and on Linux everything is 1 shared object. Thus
// the code reflects that.
};

class LibEAY : public GLibrary
{
public:
	LibEAY() : GLibrary(EAY_LIBRARY)
	{
		if (!IsLoaded())
		{
			char p[300];
			LgiGetExePath(p, sizeof(p));
			LgiMakePath(p, sizeof(p), p, PATH_OFFSET "../OpenSSL");
			#ifdef WIN32
			char old[300];
			FileDev->GetCurrentFolder(old, sizeof(old));
			FileDev->SetCurrentFolder(p);
			#endif
			Load("libeay32");
			#ifdef WIN32
			FileDev->SetCurrentFolder(old);
			#endif
		}
	}
#endif

	typedef void (*locking_callback)(int mode,int type, const char *file,int line);
	typedef unsigned long (*id_callback)();

	DynFunc1(const char *, SSLeay_version, int, type);

	DynFunc1(BIO*, BIO_new, BIO_METHOD*, type);
	DynFunc0(BIO_METHOD*, BIO_s_socket);
	DynFunc0(BIO_METHOD*, BIO_s_mem);
	DynFunc1(BIO*, BIO_new_connect, char *, host_port);
	DynFunc4(long, BIO_ctrl, BIO*, bp, int, cmd, long, larg, void*, parg);
	DynFunc4(long, BIO_int_ctrl, BIO *, bp, int, cmd, long, larg, int, iarg);
	DynFunc3(int, BIO_read, BIO*, b, void*, data, int, len);
	DynFunc3(int, BIO_write, BIO*, b, const void*, data, int, len);
	DynFunc1(int, BIO_free, BIO*, a);
	DynFunc1(int, BIO_free_all, BIO*, a);
	DynFunc2(int, BIO_test_flags, const BIO *, b, int, flags);


	DynFunc0(int, ERR_load_BIO_strings);
	#if OPENSSL_VERSION_NUMBER < 0x10100000L
	DynFunc0(int, ERR_free_strings);
	DynFunc0(int, EVP_cleanup);
	DynFunc0(int, OPENSSL_add_all_algorithms_noconf);
	DynFunc1(int, CRYPTO_set_locking_callback, locking_callback, func);
	DynFunc1(int, CRYPTO_set_id_callback, id_callback, func);
	DynFunc0(int, CRYPTO_num_locks);
	#endif
	DynFunc1(const char *, ERR_lib_error_string, unsigned long, e);
	DynFunc1(const char *, ERR_func_error_string, unsigned long, e);
	DynFunc1(const char *, ERR_reason_error_string, unsigned long, e);
	DynFunc1(int, ERR_print_errors, BIO *, bp);

	DynFunc3(char*, X509_NAME_oneline, X509_NAME*, a, char*, buf, int, size);
	DynFunc1(X509_NAME*, X509_get_subject_name, X509*, a);
	DynFunc2(char*, ERR_error_string, unsigned long, e, char*, buf);
	DynFunc0(unsigned long, ERR_get_error);
};

typedef GArray<int> SslVer;

SslVer ParseSslVersion(const char *v)
{
	GToken t(v, ".");
	SslVer out;
	for (unsigned i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		if (IsDigit(*c))
		{
			out.Add(atoi(c));
			while (*c && IsDigit(*c))
				c++;
		}
		
		if (IsAlpha(*c))
		{
			int Idx = 0;
			while (IsAlpha(*c))
			{
				Idx += ToLower(*c) - 'a';
				c++;
			}
			out.Add(Idx);
		}
	}
	return out;
}

int CompareSslVersion(SslVer &a, SslVer &b)
{
	for (unsigned i=0; i<a.Length() && i<b.Length(); i++)
	{
		int Diff = a[i] - b[i];
		if (Diff)
			return Diff;
	}
	
	return 0;
}

bool operator <(SslVer &a, SslVer &b)
{
	return CompareSslVersion(a, b) < 0;
}

bool operator >(SslVer &a, SslVer &b)
{
	return CompareSslVersion(a, b) > 0;
}

static const char *FileLeaf(const char *f)
{
	const char *l = strrchr(f, DIR_CHAR);
	return l ? l + 1 : f;
}

#undef _FL
#define _FL FileLeaf(__FILE__), __LINE__


class OpenSSL :
	#ifdef WINDOWS
	public LibEAY,
	#endif
	public LibSSL
{
	SSL_CTX *Server;

public:
	SSL_CTX *Client;
	GArray<LMutex*> Locks;
	GAutoString ErrorMsg;

    bool IsLoaded()
    {
        return LibSSL::IsLoaded()
            #ifdef WINDOWS
            && LibEAY::IsLoaded()
            #endif
            ;
    }
	
    bool InitLibrary(SslSocket *sock)
    {
		GStringPipe Err;
		GArray<int> Ver;
		GArray<int> MinimumVer = ParseSslVersion(MinimumVersion);
		GToken t;
		int Len = 0;
		const char *v = NULL;

		if (!IsLoaded())
		{
			Err.Print("%s:%i - SSL libraries missing.\n", _FL);
			goto OnError;
		}
		
		SSL_library_init();
		SSL_load_error_strings();
		ERR_load_BIO_strings();
		OpenSSL_add_all_algorithms();
		
	    Len = CRYPTO_num_locks();
	    Locks.Length(Len);
	    CRYPTO_set_locking_callback(SSL_locking_function);
	    CRYPTO_set_id_callback(SSL_id_function);

		v = SSLeay_version(SSLEAY_VERSION);
		if (!v)
		{
			Err.Print("%s:%i - SSLeay_version failed.\n", _FL);
			goto OnError;
		}

		t.Parse(v, " ");
		if (t.Length() < 2)
		{
			Err.Print("%s:%i - SSLeay_version: no version\n", _FL);
			goto OnError;
		}

		Ver = ParseSslVersion(t[1]);
		if (Ver.Length() < 3)
		{
			Err.Print("%s:%i - SSLeay_version: not enough tokens\n", _FL);
			goto OnError;
		}

		if (Ver < MinimumVer)
		{
			#if WINDOWS
			char FileName[MAX_PATH] = "";
			DWORD r = GetModuleFileNameA(LibEAY::Handle(), FileName, sizeof(FileName));
			#endif

			Err.Print("%s:%i - SSL version '%s' is too old (minimum '%s')\n"
				#if WINDOWS
				"%s\n"
				#endif
				,
				_FL,
				t[1],
				MinimumVersion
				#if WINDOWS
				,FileName
				#endif
				);
			goto OnError;
		}
		
		Client = SSL_CTX_new(SSLv23_client_method());
		if (!Client)
		{
			long e = ERR_get_error();
			char *Msg = ERR_error_string(e, 0);
			
			Err.Print("%s:%i - SSL_CTX_new(client) failed with '%s' (%i)\n", _FL, Msg, e);
			goto OnError;
		}
		
		return true;

	OnError:
		ErrorMsg.Reset(Err.NewStr());
		if (sock)
			sock->DebugTrace("%s", ErrorMsg.Get());
		return false;
    }

	OpenSSL()
	{
		Client = NULL;
		Server = NULL;
	}

	~OpenSSL()
	{
		if (Client)
		{
			SSL_CTX_free(Client);
			Client = NULL;
		}
		if (Server)
		{
			SSL_CTX_free(Server);
			Server = NULL;
		}
		Locks.DeleteObjects();
	}
	
	SSL_CTX *GetServer(SslSocket *sock, const char *CertFile, const char *KeyFile)
	{
		if (!Server)
		{
			Server = SSL_CTX_new(SSLv23_server_method());
			if (Server)
			{
				if (CertFile)
					SSL_CTX_use_certificate_file(Server, CertFile, SSL_FILETYPE_PEM);
				if (KeyFile)
					SSL_CTX_use_PrivateKey_file(Server, KeyFile, SSL_FILETYPE_PEM);
				if (!SSL_CTX_check_private_key(Server))
				{
					LgiAssert(0);
				}
 			}
			else
			{
				long e = ERR_get_error();
				char *Msg = ERR_error_string(e, 0);
				GStringPipe p;
				p.Print("%s:%i - SSL_CTX_new(server) failed with '%s' (%i)\n", _FL, Msg, e);
				ErrorMsg.Reset(p.NewStr());
				sock->DebugTrace("%s", ErrorMsg.Get());
			}
		}
		return Server;
	}

	bool IsOk(SslSocket *sock)
	{
	    bool Loaded =
    		#ifdef WIN32
		    LibSSL::IsLoaded() && LibEAY::IsLoaded();
	    	#else
		    IsLoaded();
		    #endif
		if (Loaded)
		    return true;

	    // Try and load again... cause the library can be provided by install on demand.
		#ifdef WIN32
	    Loaded = LibSSL::Load(SSL_LIBRARY) &&
	             LibEAY::Load(EAY_LIBRARY);
    	#else
		Loaded = Load(SSL_LIBRARY);
	    #endif
	    if (Loaded)
	        InitLibrary(sock);
	    return Loaded;
	}
};

static OpenSSL *Library = 0;

#if defined(WIN32) && defined(_DEBUG)
// #define SSL_DEBUG_LOCKING
#endif

void
SSL_locking_function(int mode, int n, const char *file, int line)
{
	LgiAssert(Library != NULL);
	if (Library)
	{
		if (!Library->Locks[n])
		{
			#ifdef SSL_DEBUG_LOCKING
			sprintf_s(Buf, sizeof(Buf), "SSL[%i] create\n", n);
			OutputDebugStr(Buf);
			#endif
			Library->Locks[n] = new LMutex;
		}

		#ifdef SSL_DEBUG_LOCKING
	    sprintf_s(Buf, sizeof(Buf),
			"SSL[%i] lock=%i, unlock=%i, read=%i, write=%i (mode=0x%x, count=%i, thread=0x%x)\n", n,
			TestFlag(mode, CRYPTO_LOCK),
			TestFlag(mode, CRYPTO_UNLOCK),
			TestFlag(mode, CRYPTO_READ),
			TestFlag(mode, CRYPTO_WRITE),
			mode,
			Library->Locks[n]->GetCount(),
			LgiGetCurrentThread()
			);
	    OutputDebugStr(Buf);
		#endif
		
		if (mode & CRYPTO_LOCK)
			Library->Locks[n]->Lock((char*)file, line);
		else if (mode & CRYPTO_UNLOCK)
			Library->Locks[n]->Unlock();
	}
}

unsigned long
SSL_id_function()
{
	return (unsigned long) LgiGetCurrentThread();
}

bool StartSSL(GAutoString &ErrorMsg, SslSocket *sock)
{
	static LMutex Lock;
	
	if (Lock.Lock(_FL))
	{
		if (!Library)
		{
			Library = new OpenSSL;
			if (Library && !Library->InitLibrary(sock))
			{
				ErrorMsg = Library->ErrorMsg;
				DeleteObj(Library);
			}
		}

		Lock.Unlock();
	}

	return Library != NULL;
}

void EndSSL()
{
	DeleteObj(Library);
}

struct SslSocketPriv
{
	GCapabilityClient *Caps;
	bool SslOnConnect;
	bool IsSSL;
	bool UseSSLrw;
	int Timeout;
	bool RawLFCheck;
	#ifdef _DEBUG
	bool LastWasCR;
	#endif
	bool IsBlocking;

	// This is just for the UI.
	GStreamI *Logger;

	// This is for the connection logging.
	GAutoString LogFile;
	GAutoPtr<GStream> LogStream;
	int LogFormat;

	SslSocketPriv()
	{
		#ifdef _DEBUG
		LastWasCR = false;
		#endif
		Timeout = 20 * 1000;
		IsSSL = false;
		UseSSLrw = false;
		LogFormat = 0;
	}
};

bool SslSocket::DebugLogging = false;

SslSocket::SslSocket(GStreamI *logger, GCapabilityClient *caps, bool sslonconnect, bool RawLFCheck)
{
	d = new SslSocketPriv;
	Bio = 0;
	Ssl = 0;
	d->RawLFCheck = RawLFCheck;
	d->SslOnConnect = sslonconnect;
	d->Caps = caps;
	d->Logger = logger;
	d->IsBlocking = true;
	
	GAutoString ErrMsg;
	if (StartSSL(ErrMsg, this))
	{
		#ifdef WIN32
		if (Library->IsOk(this))
		{
			char n[MAX_PATH];
			char s[MAX_PATH];
			if (GetModuleFileNameA(Library->LibSSL::Handle(), n, sizeof(n)))
			{
				sprintf_s(s, sizeof(s), "Using '%s'", n);
				OnInformation(s);
			}
			if (GetModuleFileNameA(Library->LibEAY::Handle(), n, sizeof(n)))
			{
				sprintf_s(s, sizeof(s), "Using '%s'", n);
				OnInformation(s);
			}
		}
		#endif
	}
	else if (caps)
	{
	    caps->NeedsCapability("openssl", ErrMsg);
	}
	else
	{
		OnError(0, "Can't load or find OpenSSL library.");
	}
}

SslSocket::~SslSocket()
{
	Close();
	DeleteObj(d);
}

GStreamI *SslSocket::Clone()
{
	return new SslSocket(d->Logger, d->Caps, true);
}

int SslSocket::GetTimeout()
{
	return d->Timeout;
}

void SslSocket::SetTimeout(int ms)
{
	d->Timeout = ms;
}

void SslSocket::SetLogger(GStreamI *logger)
{
	d->Logger = logger;
}

void SslSocket::SetSslOnConnect(bool b)
{
	d->SslOnConnect = b;
}

GStream *SslSocket::GetLogStream()
{
	if (!d->LogStream && d->LogFile)
	{
		if (!d->LogStream.Reset(new GFile))
			return NULL;
		
		if (!d->LogStream->Open(d->LogFile, O_WRITE))
			return NULL;
		
		// Seek to the end
		d->LogStream->SetPos(d->LogStream->GetSize());
	}
	
	return d->LogStream;
}

bool SslSocket::GetVariant(const char *Name, GVariant &Val, char *Arr)
{
	if (!Name)
		return false;

	if (!_stricmp(Name, "isSsl")) // Type: Bool
	{
		Val = true;
		return true;
	}

	return false;
}

void SslSocket::Log(const char *Str, ssize_t Bytes, SocketMsgType Type)
{
	if (!ValidStr(Str))
		return;

	if (d->Logger)
		d->Logger->Write(Str, Bytes<0?(int)strlen(Str):Bytes, Type);
	else if (Type == SocketMsgError)
		LgiTrace("%.*s", Bytes, Str);
}

const char *SslSocket::GetErrorString()
{
	return ErrMsg;
}

void SslSocket::SslError(const char *file, int line, const char *Msg)
{
	char *Part = strrchr((char*)file, DIR_CHAR);
	#ifndef WIN32
	printf("%s:%i - %s\n", file, line, Msg);
	#endif

	ErrMsg.Printf("Error: %s:%i - %s\n", Part ? Part + 1 : file, line, Msg);
	Log(ErrMsg, ErrMsg.Length(), SocketMsgError);
}

OsSocket SslSocket::Handle(OsSocket Set)
{
	OsSocket h = INVALID_SOCKET;
	
	if (Set != INVALID_SOCKET)
	{
		long r;
		bool IsError = false;
		if (!Ssl)
		{
			Ssl = Library->SSL_new(Library->GetServer(this, NULL, NULL));
		}
		if (Ssl)
		{
			r = Library->SSL_set_fd(Ssl, Set);
			Bio = Library->SSL_get_rbio(Ssl);
			r = Library->SSL_accept(Ssl);
			if (r <= 0)
				IsError = true;
			else if (r == 1)
				h = Set;
		}
		else IsError = true;

		if (IsError)
		{
			long e = Library->ERR_get_error();
			char *Msg = Library->ERR_error_string(e, 0);
			Log(Msg, -1, SocketMsgError);
			return INVALID_SOCKET;
		}
	}
	else if (Bio)
	{
		uint32 hnd = INVALID_SOCKET;
		Library->BIO_get_fd(Bio, &hnd);
		h = hnd;
	}
	
	return h;
}

bool SslSocket::IsOpen()
{
	return Bio != 0;
}

GString SslGetErrorAsString(OpenSSL *Library)
{
	BIO *bio = Library->BIO_new (Library->BIO_s_mem());
	Library->ERR_print_errors (bio);
	
	char *buf = NULL;
	size_t len = Library->BIO_get_mem_data (bio, &buf);
	GString s(buf, len);
	Library->BIO_free (bio);
	return s;
}

int SslSocket::Open(const char *HostAddr, int Port)
{
	bool Status = false;
	LMutex::Auto Lck(&Lock, _FL);

DebugTrace("%s:%i - SslSocket::Open(%s,%i)\n", _FL, HostAddr, Port);
	
	if (Library &&
		Library->IsOk(this) &&
		HostAddr)
	{
		char h[256];
		sprintf_s(h, sizeof(h), "%s:%i", HostAddr, Port);

		// Do SSL handshake?
		if (d->SslOnConnect)
		{
			// SSL connection..
			d->IsSSL = true;
			if (Library->Client)
			{
				const char *CertDir = "/u/matthew/cert";
				long r = Library->SSL_CTX_load_verify_locations(Library->Client, 0, CertDir);
DebugTrace("%s:%i - SSL_CTX_load_verify_locations=%i\n", _FL, r);
				if (r > 0)
				{
					Bio = Library->BIO_new_ssl_connect(Library->Client);
DebugTrace("%s:%i - BIO_new_ssl_connect=%p\n", _FL, Bio);
					if (Bio)
					{
						Library->BIO_get_ssl(Bio, &Ssl);
DebugTrace("%s:%i - BIO_get_ssl=%p\n", _FL, Ssl);
						if (Ssl)
						{
							// SNI setup
							Library->SSL_set_tlsext_host_name(Ssl, HostAddr);
					
							// Library->SSL_CTX_set_timeout()
							Library->BIO_set_conn_hostname(Bio, HostAddr);
							#if OPENSSL_VERSION_NUMBER < 0x10100000L
							Library->BIO_set_conn_int_port(Bio, &Port);
							#else
							GString sPort;
							sPort.Printf("%i");
							Library->BIO_set_conn_port(Bio, sPort.Get());
							#endif

							// Do non-block connect
							uint64 Start = LgiCurrentTime();
							int To = GetTimeout();
							
							IsBlocking(false);
							
							r = Library->SSL_connect(Ssl);
DebugTrace("%s:%i - initial SSL_connect=%i\n", _FL, r);
							while (r != 1 && !IsCancelled())
							{
								int err = Library->SSL_get_error(Ssl, r);
								if (err != SSL_ERROR_WANT_CONNECT)
								{
DebugTrace("%s:%i - SSL_get_error=%i\n", _FL, err);
								}

								LgiSleep(50);
								r = Library->SSL_connect(Ssl);
DebugTrace("%s:%i - SSL_connect=%i (%i of %i ms)\n", _FL, r, (int)(LgiCurrentTime() - Start), (int)To);

								bool TimeOut = !HasntTimedOut();
								if (TimeOut)
								{
DebugTrace("%s:%i - SSL connect timeout, to=%i\n", _FL, To);
									SslError(_FL, "Connection timeout.");
									break;
								}
							}
DebugTrace("%s:%i - open loop finished, r=%i, Cancelled=%i\n", _FL, r, IsCancelled());

							if (r == 1)
							{
								IsBlocking(true);
								Library->SSL_set_mode(Ssl, SSL_MODE_AUTO_RETRY);
								Status = true;
								// d->UseSSLrw = true;
								
								char m[256];
								sprintf_s(m, sizeof(m), "Connected to '%s' using SSL", h);
								OnInformation(m);
							}
							else
							{
								GString Err = SslGetErrorAsString(Library).Strip();
								if (!Err)
									Err.Printf("BIO_do_connect(%s:%i) failed.", HostAddr, Port);
								SslError(_FL, Err);
							}
						}
						else SslError(_FL, "BIO_get_ssl failed.");
					}
					else SslError(_FL, "BIO_new_ssl_connect failed.");
				}
				else SslError(_FL, "SSL_CTX_load_verify_locations failed.");
			}
			else SslError(_FL, "No Ctx.");
		}
		else
		{
			Bio = Library->BIO_new_connect(h);
DebugTrace("%s:%i - BIO_new_connect=%p\n", _FL, Bio);
			if (Bio)
			{
				// Non SSL... go into non-blocking mode so that if ::Close() is called we
				// can quit out of the connect loop.
				IsBlocking(false);

				uint64 Start = LgiCurrentTime();
				int To = GetTimeout();

				long r = Library->BIO_do_connect(Bio);
DebugTrace("%s:%i - BIO_do_connect=%i\n", _FL, r);
				while (r != 1 && !IsCancelled())
				{
					if (!Library->BIO_should_retry(Bio))
					{
						break;
					}

					LgiSleep(50);
					r = Library->BIO_do_connect(Bio);
DebugTrace("%s:%i - BIO_do_connect=%i\n", _FL, r);

					if (!HasntTimedOut())
					{
DebugTrace("%s:%i - open timeout, to=%i\n", _FL, To);
						OnError(0, "Connection timeout.");
						break;
					}
				}

DebugTrace("%s:%i - open loop finished=%i\n", _FL, r);

				if (r == 1)
				{
					IsBlocking(true);
					Status = true;

					char m[256];
					sprintf_s(m, sizeof(m), "Connected to '%s'", h);
					OnInformation(m);
				}
				else SslError(_FL, "BIO_do_connect failed");
			}
			else SslError(_FL, "BIO_new_connect failed");
		}
	}
	
	if (!Status)
	{
		Close();
	}

DebugTrace("%s:%i - SslSocket::Open status=%i\n", _FL, Status);
 
	return Status;
}

bool SslSocket::SetVariant(const char *Name, GVariant &Value, char *Arr)
{
	bool Status = false;

	if (!Library || !Name)
		return false;

	if (!_stricmp(Name, SslSocket_LogFile))
	{
		d->LogFile.Reset(Value.ReleaseStr());
	}
	else if (!_stricmp(Name, SslSocket_LogFormat))
	{
		d->LogFormat = Value.CastInt32();
	}
	else if (!_stricmp(Name, GSocket_Protocol))
	{
		char *v = Value.CastString();
		
		if (v && stristr(v, "SSL"))
		{
			if (!Bio)
			{
				d->SslOnConnect = true;
			}
			else
			{
				if (!Library->Client)
				{
					SslError(_FL, "Library->Client is null.");
				}
				else
				{
					Ssl = Library->SSL_new(Library->Client);
DebugTrace("%s:%i - SSL_new=%p\n", _FL, Ssl);
					if (!Ssl)
					{
						SslError(_FL, "SSL_new failed.");
					}
					else
					{
						int r = Library->SSL_set_bio(Ssl, Bio, Bio);
DebugTrace("%s:%i - SSL_set_bio=%i\n", _FL, r);
		
						uint64 Start = LgiCurrentTime();
						int To = GetTimeout();
						while (HasntTimedOut())
						{
							r = Library->SSL_connect(Ssl);
DebugTrace("%s:%i - SSL_connect=%i\n", _FL, r);
							if (r < 0)
								LgiSleep(100);
							else
								break;
						}

						if (r > 0)
						{
							Status = d->UseSSLrw = d->IsSSL = true;
							OnInformation("Session is now using SSL");

							X509 *ServerCert = Library->SSL_get_peer_certificate(Ssl);
DebugTrace("%s:%i - SSL_get_peer_certificate=%p\n", _FL, ServerCert);
							if (ServerCert)
							{
								char Txt[256] = "";
								Library->X509_NAME_oneline(Library->X509_get_subject_name(ServerCert), Txt, sizeof(Txt));
DebugTrace("%s:%i - X509_NAME_oneline=%s\n", _FL, Txt);
								OnInformation(Txt);
							}

							// SSL_get_verify_result
						}
						else
						{
							SslError(_FL, "SSL_connect failed.");

							r = Library->SSL_get_error(Ssl, r);
							char *Msg = Library->ERR_error_string(r, 0);
							if (Msg)
							{
								OnError(r, Msg);
							}
						}
					}
				}
			}
		}
	}

	return Status;
}

int SslSocket::Close()
{
	Cancel(true);
	LMutex::Auto Lck(&Lock, _FL);

	if (Library)
	{
		if (Ssl)
		{
DebugTrace("%s:%i - SSL_shutdown\n", _FL);

			int r = 0;
			if ((r = Library->SSL_shutdown(Ssl)) >= 0)
			{
				#ifdef WIN32
				closesocket
				#else
				close
				#endif
					(Library->SSL_get_fd(Ssl));
			}

			Library->SSL_free(Ssl);
			OnInformation("SSL connection closed.");

			// I think the Ssl object "owns" the Bio object...
			// So assume it gets fread by SSL_shutdown
		}
		else if (Bio)
		{
DebugTrace("%s:%i - BIO_free\n", _FL);
			Library->BIO_free(Bio);
			OnInformation("Connection closed.");
		}

		Ssl = 0;
		Bio = 0;
	}
	else return false;

	return true;
}

bool SslSocket::Listen(int Port)
{
	return false;
}

bool SslSocket::IsBlocking()
{
	return d->IsBlocking;
}

void SslSocket::IsBlocking(bool block)
{
	d->IsBlocking = block;
	if (Bio)
	{
		Library->BIO_set_nbio(Bio, !d->IsBlocking);
	}
}

bool SslSocket::IsReadable(int TimeoutMs)
{
	// Assign to local var to avoid a thread changing it
	// on us between the validity check and the select.
	// Which is important because a socket value of -1
	// (ie invalid) will crash the FD_SET macro.
	OsSocket s = Handle();
	if (ValidSocket(s))
	{
		struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

		fd_set r;
		FD_ZERO(&r);
		FD_SET(s, &r);
		
		int v = select(s+1, &r, 0, 0, &t);
		if (v > 0 && FD_ISSET(s, &r))
		{
			return true;
		}
		else if (v < 0)
		{
			// Error();
		}
	}
	else LgiTrace("%s:%i - Not a valid socket.\n", _FL);

	return false;
}

bool SslSocket::IsWritable(int TimeoutMs)
{
	// Assign to local var to avoid a thread changing it
	// on us between the validity check and the select.
	// Which is important because a socket value of -1
	// (ie invalid) will crash the FD_SET macro.
	OsSocket s = Handle();
	if (ValidSocket(s))
	{
		struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

		fd_set w;
		FD_ZERO(&w);
		FD_SET(s, &w);
		
		int v = select(s+1, &w, 0, 0, &t);
		if (v > 0 && FD_ISSET(s, &w))
		{
			return true;
		}
		else if (v < 0)
		{
			// Error();
		}
	}
	else LgiTrace("%s:%i - Not a valid socket.\n", _FL);

	return false;
}

void SslSocket::OnWrite(const char *Data, ssize_t Len)
{
	#ifdef _DEBUG
	if (d->RawLFCheck)
	{
		const char *End = Data + Len;
		while (Data < End)
		{
			LgiAssert(*Data != '\n' || d->LastWasCR);
			d->LastWasCR = *Data == '\r';
			Data++;
		}
	}
	#endif
	
	// Log(Data, Len, SocketMsgSend);
}

void SslSocket::OnRead(char *Data, ssize_t Len)
{
	#ifdef _DEBUG
	if (d->RawLFCheck)
	{
		const char *End = Data + Len;
		while (Data < End)
		{
			LgiAssert(*Data != '\n' || d->LastWasCR);
			d->LastWasCR = *Data == '\r';
			Data++;
		}
	}
	#endif

	// Log(Data, Len, SocketMsgReceive);
}

ssize_t SslSocket::Write(const void *Data, ssize_t Len, int Flags)
{
	LMutex::Auto Lck(&Lock, _FL);

	if (!Library)
	{
		DebugTrace("%s:%i - Library is NULL\n", _FL);
		return -1;
	}

	if (!Bio)
	{
		DebugTrace("%s:%i - BIO is NULL\n", _FL);
		return -1;
	}

	int r = 0;
	if (d->UseSSLrw)
	{
		if (Ssl)
		{
			uint64 Start = LgiCurrentTime();
			int To = GetTimeout();

			while (HasntTimedOut())
			{
				r = Library->SSL_write(Ssl, Data, Len);
				if (r < 0)
				{
					LgiSleep(10);
				}
				else
				{
					DebugTrace("%s:%i - SSL_write(%p,%i)=%i\n", _FL, Data, Len, r);
					OnWrite((const char*)Data, r);
					break;
				}
			}

			if (r < 0)
			{
				DebugTrace("%s:%i - SSL_write failed (timeout=%i, %ims)\n",
							_FL,
							To,
							(int) (LgiCurrentTime() - Start));
			}
		}
		else
		{
			r = -1;
			DebugTrace("%s:%i - No SSL\n", _FL);
		}
	}
	else
	{
		uint64 Start = LgiCurrentTime();
		int To = GetTimeout();
		while (HasntTimedOut())
		{
			if (!Library)
				break;
			r = Library->BIO_write(Bio, Data, Len);
			DebugTrace("%s:%i - BIO_write(%p,%i)=%i\n", _FL, Data, Len, r);
			if (r < 0)
			{
				LgiSleep(10);
			}
			else
			{
				OnWrite((const char*)Data, r);
				break;
			}
		}			

		if (r < 0)
		{
			DebugTrace("%s:%i - BIO_write failed (timeout=%i, %ims)\n",
						_FL,
						To,
						(int) (LgiCurrentTime() - Start));
		}
	}
	
	if (r > 0)
	{
		GStream *l = GetLogStream();
		if (l)
			l->Write(Data, r);
	}
	
	if (Ssl)
	{
		if (r < 0)
		{
			int Err = Library->SSL_get_error(Ssl, r);
			char Buf[256] = "";
			char *e = Library->ERR_error_string(Err, Buf);

			DebugTrace("%s:%i - ::Write error %i, %s\n",
						_FL,
						Err,
						e);

			if (e)
			{
				OnError(Err, e);
			}
		}
		
		if (r <= 0)
		{
			DebugTrace("%s:%i - ::Write closing %i\n",
						_FL,
						r);
			Close();
		}
	}

	return r;
}

ssize_t SslSocket::Read(void *Data, ssize_t Len, int Flags)
{
	LMutex::Auto Lck(&Lock, _FL);

	if (!Library)
		return -1;

	if (Bio)
	{
		int r = 0;
		if (d->UseSSLrw)
		{
			if (Ssl)
			{
				uint64 Start = LgiCurrentTime();
				int To = GetTimeout();
				while (HasntTimedOut())
				{
					r = Library->SSL_read(Ssl, Data, Len);
DebugTrace("%s:%i - SSL_read(%p,%i)=%i\n", _FL, Data, Len, r);
					if (r < 0)
						LgiSleep(10);
					else
					{
						OnRead((char*)Data, r);
						break;
					}
				}
			}
			else
			{
				r = -1;
			}
		}
		else
		{
			uint64 Start = LgiCurrentTime();
			int To = GetTimeout();
			while (HasntTimedOut())
			{
				r = Library->BIO_read(Bio, Data, Len);
				if (r < 0)
				{
					if (d->IsBlocking)
						LgiSleep(10);
					else
						break;
				}
				else
				{
DebugTrace("%s:%i - BIO_read(%p,%i)=%i\n", _FL, Data, Len, r);
					OnRead((char*)Data, r);
					break;
				}
			}
		}

		if (r > 0)
		{
			GStream *l = GetLogStream();
			if (l)
				l->Write(Data, r);
		}

		if (Ssl && d->IsBlocking)
		{
			if (r < 0)
			{
				int Err = Library->SSL_get_error(Ssl, r);
				char Buf[256];
				char *e = Library->ERR_error_string(Err, Buf);
				if (e)
				{
					OnError(Err, e);
				}
				Close();
			}
			
			if (r <= 0)
			{
				Close();
			}
		}
		return r;
	}

	return -1;
}

void SslSocket::OnError(int ErrorCode, const char *ErrorDescription)
{
DebugTrace("%s:%i - OnError=%i,%s\n", _FL, ErrorCode, ErrorDescription);
	GString s;
	s.Printf("Error %i: %s\n", ErrorCode, ErrorDescription);
	Log(s, s.Length(), SocketMsgError);
}

void SslSocket::DebugTrace(const char *fmt, ...)
{
	if (DebugLogging)
	{
		char Buffer[512];
		va_list Arg;
		va_start(Arg, fmt);
		int Ch = vsprintf_s(Buffer, sizeof(Buffer), fmt, Arg);
		va_end(Arg);
		
		if (Ch > 0)
		{
			// LgiTrace("SSL:%p: %s", this, Buffer);
			OnInformation(Buffer);
		}
	}
}

void SslSocket::OnInformation(const char *Str)
{
	while (Str && *Str)
	{
		GAutoString a;
		const char *nl = Str;
		while (*nl && *nl != '\n')
			nl++;

		int Len = (int) (nl - Str + 2);
		a.Reset(new char[Len]);
		char *o;
		for (o = a; Str < nl; Str++)
		{
			if (*Str != '\r')
				*o++ = *Str;
		}
		*o++ = '\n';
		*o++ = 0;
		LgiAssert((o-a) <= Len);
				
		Log(a, -1, SocketMsgInfo);
		
		Str = *nl ? nl + 1 : nl;
	}
}
