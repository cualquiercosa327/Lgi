/*hdr
**      FILE:           INet.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           28/5/98
**      DESCRIPTION:    Internet sockets
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#ifdef LINUX
#include <netinet/tcp.h>
#endif
#include <ctype.h>

#include "GFile.h"
#include "INet.h"
#include "GToken.h"
#include "GString.h"
#include "LgiCommon.h"
#include "LgiOsClasses.h"
#include "../../src/common/Hash/md5/md5.h"
#include "GRegKey.h"
#ifdef MAC
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#endif

#define USE_BSD_SOCKETS			1
#define DEBUG_CONNECT			0
#define ETIMEOUT				400
#define PROTO_UDP				0x100

#if defined WIN32

 	#include <stdio.h>
	#include <stdlib.h>
	#include <ras.h>

	typedef HOSTENT HostEnt;
	typedef int socklen_t;
	typedef unsigned long in_addr_t;

	#define MSG_NOSIGNAL		0
	#define EWOULDBLOCK			WSAEWOULDBLOCK
	#define EISCONN				WSAEISCONN

	#define OsAddr				S_un.S_addr

#elif defined POSIX

	#include <stdio.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <ctype.h>
	#include <netdb.h>
	#include <errno.h>
    #include "LgiCommon.h"
	
	#define SOCKET_ERROR -1
	typedef hostent HostEnt;
	#define OsAddr				s_addr
	
#elif defined BEOS

	#include <stdio.h>
	#include <NetEndpoint.h>

	#define SOCKET_ERROR -1
	#define MSG_NOSIGNAL 0

	typedef struct hostent HostEnt;
	typedef int socklen_t;

#endif

#include "LgiNetInc.h"

///////////////////////////////////////////////////////////////////////////////
#ifdef BEOS
uint32 inet_addr(char *HostAddr)
{
	uint32 s = 0;
	GToken Ip(HostAddr, ".");
	if (Ip.Length() == 4)
	{
		uchar *IpAddress = (uchar*)&s;
		for (int i=0; i<4; i++)
		{
			IpAddress[i] = atoi(Ip[i]);
		}				
	}
	return s;
}
#endif


///////////////////////////////////////////////////////////////////////////////
GNetwork::GNetwork()
{
	#ifdef WIN32
	SocketsOpen = false;

	#ifndef WINSOCK_VERSION
	#define WINSOCK_VERSION MAKEWORD(2,2)
	#endif

	// Start sockets
	WSADATA WsaData;
	int Result = WSAStartup(WINSOCK_VERSION, &WsaData);
	if (!Result)
	{
		if (WsaData.wVersion == WINSOCK_VERSION)
		{
			SocketsOpen = Result == 0;
		}
		else
		{
			WSACleanup();
		}
	}

	#else
	SocketsOpen = true;
	#endif
}

GNetwork::~GNetwork()
{
	#ifdef WIN32
	if (SocketsOpen)
	{
		WSACleanup();
		SocketsOpen = false;
	}
	#endif
}

#ifdef WIN32
#include "..\..\Win32\INet\MibAccess.h"
#endif

bool GNetwork::EnumInterfaces(List<char> &Lst)
{
	bool Status = false;

	#ifdef WIN32
	UINT IPArray[50];
	UINT Size = 50;
	MibII m;

	m.Init();
	if (m.GetIPAddress(IPArray, Size))
	{
		for (int i=0; i<Size; i++)
		{
			if (IPArray[i])
			{
				char Str[256];
				uchar *Ip = (uchar*) &IPArray[i];
				sprintf(Str, "%i.%i.%i.%i", Ip[0], Ip[1], Ip[2], Ip[3]);
				Lst.Insert(NewStr(Str));

				Status = true;
			}
		}
	}	
	#endif

	return Status;
}

////////////////////////////////////////////////////////////////////////////////////////////////
class GSocketImplPrivate
{
public:
	// Data
	GNetwork	*Parent;
	bool		Blocking;
	int			LogType;
	char		*LogFile;
	int			Timeout;
	OsSocket	Socket;
	int			Flags;
	int			LastError;

	GSocketImplPrivate()
	{	
		Blocking = true;
		LogType = NET_LOG_NONE;
		LogFile = 0;
		Timeout = -1;
		Socket = INVALID_SOCKET;
		Flags = 0;
		LastError = 0;
	}

	~GSocketImplPrivate()
	{
		DeleteArray(LogFile);
	}
};

GSocket::GSocket(GStreamI *logger)
{
	BytesWritten = 0;
	BytesRead = 0;
	d = new GSocketImplPrivate;
}

GSocket::~GSocket()
{
	Close();
	DeleteObj(d);
}

bool GSocket::IsOK()
{
	return	this != 0 AND
			d != 0 AND
			(d->Parent ? d->Parent->IsValid() : false);
}


void GSocket::OnDisconnect()
{
}

OsSocket GSocket::Handle(OsSocket Set)
{
	if (Set != INVALID_SOCKET)
	{
		d->Socket = Set;
	}

	return d->Socket;
}

/*
int GSocket::Is(GSocketProp p)
{
	switch (p)
	{
		case GReadable:
		{
			return IsReadable();
			break;
		}
		case GWriteable:
		{
			fd_set s;
			FD_ZERO(&s);
			FD_SET(d->Socket, &s);

			timeval t = {0, 0};
			int r = select(d->Socket+1, 0, &s, 0, &t);
			return r > 0;
			break;
		}
		case GAcceptable:
		{
			fd_set s;
			FD_ZERO(&s);
			FD_SET(d->Socket, &s);

			timeval t = {0, 0};
			int r = select(d->Socket+1, &s, 0, 0, &t);
			return r > 0;
			break;
		}
		case GException:
		{
			fd_set s;
			FD_ZERO(&s);
			FD_SET(d->Socket, &s);

			timeval t = {0, 0};
			int r = select(d->Socket+1, 0, 0, &s, &t);
			return r > 0;
			break;
		}
		case GAsync:
		{
			return d->Async;
			break;
		}
		case GNoDelay:
		{
			#if defined(MAC) || defined(BEOS)
			// FIXME?
			return false;
			#else
			return TestFlag(d->Flags, TCP_NODELAY);
			#endif
		}
	}

	return 0;
}

void GSocket::Is(GSocketProp p, int i)
{
	switch (p)
	{
		case GReadable:
		{
			break;
		}
		case GWriteable:
		{
			break;
		}
		case GAcceptable:
		{
			break;
		}
		case GAsync:
		{
			d->Async = i;
			#if defined WIN32
			ulong NonBlocking = d->Async;
			ioctlsocket(d->Socket, FIONBIO, &NonBlocking);
			#elif defined POSIX
			fcntl(d->Socket, F_SETFL, (d->Async = i) ? O_NONBLOCK : 0);
			#elif defined BEOS
			uint32 b = d->Async ? -1 : 0;
			setsockopt(d->Socket, SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
			#else
			#error Impl me.
			#endif
			break;
		}
		case GNoDelay:
		{
			#if defined(MAC) || defined(BEOS)
			// FIXME?
			#else
			d->Flags = TCP_NODELAY;
			#endif
			break;
		}
	}
}
*/

bool GSocket::IsOpen()
{
	if (ValidSocket(d->Socket))
	{
		return true;
	}

	return false;
}

int GSocket::GetTimeout()
{
	return d->Timeout;
}

void GSocket::SetTimeout(int ms)
{
	d->Timeout = ms;
}

bool GSocket::IsReadable(int TimeoutMs)
{
	ulong Val = 0;
	
	// Assign to local var to avoid a thread changing it
	// on us between the validity check and the select.
	// Which is important because a socket value of -1
	// (ie invalid) will crash the FD_SET macro.
	OsSocket s = d->Socket; 
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
			Error();
		}
	}
	else LgiTrace("%s:%i - Not a valid socket.\n", _FL);

	return false;
}

bool GSocket::IsWritable(int TimeoutMs)
{
	ulong Val = 0;
	
	// Assign to local var to avoid a thread changing it
	// on us between the validity check and the select.
	// Which is important because a socket value of -1
	// (ie invalid) will crash the FD_SET macro.
	OsSocket s = d->Socket; 
	if (ValidSocket(s))
	{
		struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

		fd_set set;
		FD_ZERO(&set);
		FD_SET(s, &set);		
		int ret = select(s+1, 0, &set, 0, &t);
		// printf("select ret=%i isset=%i\n", ret, FD_ISSET(s, &set));
		if (ret > 0 && FD_ISSET(s, &set))
		{
			return true;
		}
	}

	return false;
}

bool GSocket::CanAccept(int TimeoutMs)
{
	return IsReadable(TimeoutMs);
}

bool GSocket::IsBlocking()
{
	return d->Blocking;
}

void GSocket::IsBlocking(bool block)
{
	d->Blocking = block;
	
	#if defined WIN32
	ulong NonBlocking = !block;
	ioctlsocket(d->Socket, FIONBIO, &NonBlocking);
	#elif defined POSIX
	fcntl(d->Socket, F_SETFL, d->Blocking ? 0 : O_NONBLOCK);
	#elif defined BEOS
	uint32 b = d->Blocking ? 0 : -1;
	setsockopt(d->Socket, SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
	#else
	#error Impl me.
	#endif
}

bool GSocket::IsDelayed()
{
	return true;
}

void GSocket::IsDelayed(bool Delay)
{
}

int GSocket::GetLocalPort()
{
	struct sockaddr_in addr;
	socklen_t size;
	
	ZeroObj(addr);
	size = sizeof(addr);
	if ((getsockname(Handle(), (sockaddr*)&addr, &size)) >= 0)
	{
		return ntohs(addr.sin_port);
	}
	return 0;
}

bool GSocket::GetLocalIp(char *IpAddr)
{
	if (IpAddr)
	{
		struct sockaddr_in addr;
		socklen_t size;
		
		size = sizeof(addr);
		if ((getsockname(Handle(), (sockaddr*)&addr, &size)) < 0)
			return false;
		
		if (addr.sin_addr.s_addr == INADDR_ANY)
			return false;
		
		uchar *a = (uchar*)&addr.sin_addr.s_addr;
		sprintf(	IpAddr,
					"%i.%i.%i.%i",
					a[0],
					a[1],
					a[2],
					a[3]);
	
		return true;
	}

	return false;
}

bool GSocket::GetRemoteIp(char *IpAddr)
{
	if (IpAddr)
	{
		struct sockaddr_in a;
		socklen_t addrlen = sizeof(a);
		if (!getpeername(Handle(), (sockaddr*)&a, &addrlen))
		{
			uchar *addr = (uchar*)&a.sin_addr.s_addr;
			sprintf(IpAddr,
					"%u.%u.%u.%u",
					addr[0],
					addr[1],
					addr[2],
					addr[3]);
			return true;
		}
	}

	return false;
}

int GSocket::GetRemotePort()
{
	struct sockaddr_in a;
	socklen_t addrlen = sizeof(a);
	if (!getpeername(Handle(), (sockaddr*)&a, &addrlen))
	{
		return a.sin_port;
	}

	return 0;
}

int GSocket::Open(char *HostAddr, int Port)
{
	int Status = -1;
	
	Close();

	if (HostAddr)
	{
		char Buf[1024];

		BytesWritten = 0;
		BytesRead = 0;
		
		sockaddr_in RemoteAddr;
		HostEnt *Host = 0;
		in_addr_t IpAddress = 0;

		ZeroObj(RemoteAddr);
		#ifdef WIN32
		d->Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		#else
		d->Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		#endif

		if (d->Socket >= 0)
		{
			#if !defined(MAC) && !defined(BEOS)
			if (d->Flags & TCP_NODELAY)
			{
				int flag = 1;
				setsockopt
				(
					d->Socket,
					IPPROTO_TCP,
					TCP_NODELAY,
					(char *) &flag,
					sizeof(int)
				);
			}
			#endif

			if (isdigit(*HostAddr) AND strchr(HostAddr, '.'))
			{
				// Ip address
				IpAddress = inet_addr(HostAddr);
				
				#if defined(WIN32) || defined(BEOS)

				Host = gethostbyaddr((const char*) &IpAddress, 4, AF_INET);
				if (!Host)
					Error();

				#else

				Host = gethostbyaddr
				(
					#ifdef MAC
					HostAddr,
					#else
					&IpAddress,
					#endif
					4,
					AF_INET
				);

				#endif
			}
			else
			{
				// Name address
				#ifdef LINUX

				Host = new HostEnt;
				if (Host)
				{
					memset(Host, 0, sizeof(*Host));
					
					HostEnt *Result = 0;
					int Err = 0;
					if (gethostbyname_r(HostAddr, Host, Buf, sizeof(Buf), &Result, &Err))
					{
						char *ErrStr = GetErrorName(Err);
						printf("%s:%i - gethostbyname_r('%s') failed (%i - %s)\n",
							__FILE__, __LINE__,
							HostAddr, Err, ErrStr);
						DeleteObj(Host);
					}
				}
				
				#if DEBUG_CONNECT
				printf("%s:%i - Host=%p\n", __FILE__, __LINE__, Host);
				#endif
				
				#else
				
				Host = gethostbyname(HostAddr);
				
				#endif
				
				if (!Host)
				{
					Error();
					Close();
					return false;
				}
			}
			
			if (1)
			{
				RemoteAddr.sin_family = AF_INET;
				RemoteAddr.sin_port = htons(Port);
				
				if (Host)
				{
					if (Host->h_addr_list AND Host->h_addr_list[0])
					{
						memcpy(&RemoteAddr.sin_addr, Host->h_addr_list[0], sizeof(in_addr) );
					}
					else return false;
				}
				else
				{
					memcpy(&RemoteAddr.sin_addr, &IpAddress, sizeof(IpAddress) );
				}

				#ifdef WIN32
				if (d->Timeout < 0)
				{
					// Do blocking connect
					Status = connect(d->Socket, (sockaddr*) &RemoteAddr, sizeof(sockaddr_in));
				}
				else
				#endif
				{
					#define CONNECT_LOGGING			0
					
					// Setup the connect
					bool Block = IsBlocking();
					if (Block)
					{
						#if CONNECT_LOGGING
						printf("%p - Setting non blocking\n", d->Socket);
						#endif
						IsBlocking(false);
					}
				
					// Do initial connect to kick things off..
					#if CONNECT_LOGGING
					printf("%p - Doing initial connect to %s:%i\n", d->Socket, HostAddr, Port);
					#endif
					Status = connect(d->Socket, (sockaddr*) &RemoteAddr, sizeof(sockaddr_in));
					#if CONNECT_LOGGING
					printf("%p - Initial connect=%i Block=%i\n", d->Socket, Status, Block);
					#endif

					// Wait for the connect to finish?
					if (Status && Block)
					{
						Error(Host);

						#ifdef WIN32
						// yeah I know... wtf? (http://itamarst.org/writings/win32sockets.html)
						#define IsWouldBlock() (d->LastError == EWOULDBLOCK || d->LastError == WSAEINVAL)
						#else
						#define IsWouldBlock() (d->LastError == EWOULDBLOCK || d->LastError == EINPROGRESS)
						#endif

						#if CONNECT_LOGGING
						printf("%p - IsWouldBlock()=%i d->LastError=%i\n", d->Socket, IsWouldBlock(), d->LastError);
						#endif

						int64 End = LgiCurrentTime() + (d->Timeout > 0 ? d->Timeout : 30000);
						while (ValidSocket(d->Socket) && IsWouldBlock())
						{
							int64 Remaining = End - LgiCurrentTime();

							#if CONNECT_LOGGING
							printf("%p - Remaining "LGI_PrintfInt64"\n", d->Socket, Remaining);
							#endif

							if (Remaining < 0)
							{
								#if CONNECT_LOGGING
								printf("%p - Leaving loop\n", d->Socket);
								#endif
								break;
							}
							
							if (IsWritable(min(Remaining, 1000)))
							{
								// Should be ready to connect now...
								#if CONNECT_LOGGING
								printf("%p - Secondary connect...\n", d->Socket);
								#endif
								Status = connect(d->Socket, (sockaddr*) &RemoteAddr, sizeof(sockaddr_in));
								#if CONNECT_LOGGING
								printf("%p - Secondary connect=%i\n", d->Socket, Status);
								#endif
								if (Status != 0)
								{
									Error(Host);
									#if CONNECT_LOGGING
									printf("%p - Connect=%i Err=%i\n", d->Socket, Status, d->LastError);
									#endif
									if (IsWouldBlock())
										continue;
									if (d->LastError == EISCONN)
										Status = 0;
									break;
								}
								else
								{
									#if CONNECT_LOGGING
									printf("%p - Connected...\n", d->Socket);
									#endif
									break;
								}
							}
							else
							{
								#if CONNECT_LOGGING
								printf("%p - Timout...\n", d->Socket);
								#endif
							}
						}
					}

					if (Block)
						IsBlocking(true);
				}

				if (!Status)
				{
					char Info[256];
					sprintf(Info,
							"[INET] Socket Connect: %s [%i.%i.%i.%i], port: %i",
							HostAddr,
							(RemoteAddr.sin_addr.s_addr) & 0xFF,
							(RemoteAddr.sin_addr.s_addr >> 8) & 0xFF,
							(RemoteAddr.sin_addr.s_addr >> 16) & 0xFF,
							(RemoteAddr.sin_addr.s_addr >> 24) & 0xFF,
							Port);
					OnInformation(Info);
				}
			}

			if (Status)
			{
				#ifdef WIN32
				closesocket(d->Socket);
				#else
				close(d->Socket);
				#endif
				d->Socket = INVALID_SOCKET;
			}
		}
		else
		{
			Error();
		}
	}
	
	return Status == 0;
}

bool GSocket::Listen(int Port)
{
	Close();

	d->Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (d->Socket >= 0)
	{
		BytesWritten = 0;
		BytesRead = 0;

		sockaddr Addr;
		sockaddr_in *a = (sockaddr_in*) &Addr;
		ZeroObj(Addr);
		a->sin_family = AF_INET;
		a->sin_port = htons(Port);
		a->sin_addr.OsAddr = INADDR_ANY;

		if (bind(d->Socket, &Addr, sizeof(Addr)) >= 0)
		{
			if (listen(d->Socket, SOMAXCONN) != SOCKET_ERROR)
			{
				return true;
			}
			else
			{
				Error();
			}
		}
		else
		{
			Error();
		}
	}
	else
	{
		Error();
	}

	return false;
}

bool GSocket::Accept(GSocketI *c)
{
	if (c)
	{
		OsSocket NewSocket = INVALID_SOCKET;
		sockaddr Address;
		
		#ifdef WIN32
		int Length = sizeof(Address);
		NewSocket = accept(d->Socket, &Address, &Length);
		#else
		
		/*
		socklen_t Length = sizeof(Address);
		NewSocket = accept(d->Socket, &Address, &Length);
		#endif

		*/
		int Loop = 0;
		socklen_t Length = sizeof(Address);
		while (ValidSocket(d->Socket))
		{
			if (IsReadable(1000))
			{
				NewSocket = accept(d->Socket, &Address, &Length);
				break;
			}
			else
			{
				printf("%s:%i - Accept wait %i\n", _FL, Loop++);
			}
		}
		#endif
		
		if (ValidSocket(NewSocket))
		{
			c->Handle(NewSocket);
			return true;
		}
		
		return false;
	}

	return false;
}

int GSocket::Close()
{
	if (ValidSocket(d->Socket))
	{
		#if defined WIN32
		closesocket(d->Socket);
		#else
		close(d->Socket);
		#endif
		d->Socket = INVALID_SOCKET;
		OnDisconnect();
	}

	return true;
}

void GSocket::Log(char *Msg, int Ret, char *Buf, int Len)
{
	if (d->LogFile)
	{
		GFile f;
		if (f.Open(d->LogFile, O_WRITE))
		{
			f.Seek(f.GetSize(), SEEK_SET);

			switch (d->LogType)
			{
				case NET_LOG_HEX_DUMP:
				{
					char s[256];

					sprintf(s, "%s = %i\r\n", Msg, Ret);
					f.Write(s, strlen(s));

					for (int i=0; i<Len;)
					{
						char Ascii[32], *a = Ascii;
						sprintf(s, "%08.8X\t", i);
						for (int n = i + 16; i<Len AND i<n; i++)
						{
							*a = (uchar)Buf[i];
							if (*a < ' ') *a = '.';
							a++;

							sprintf(s+strlen(s), "%02.2X ", (uchar)Buf[i]);
						}
						*a++ = 0;
						while (strlen(s) < 58)
						{
							strcat(s, " ");
						}
						strcat(s, Ascii);
						strcat(s, (char*)"\r\n");
						f.Write(s, strlen(s));
					}
					break;
				}
				case NET_LOG_ALL_BYTES:
				{
					f.Write(Buf, Len);
					break;
				}
			}
		}
	}
}

int GSocket::Write(void *Data, int Len, int Flags)
{
	if (!ValidSocket(d->Socket) || !Data)
		return -1;

	int Status = 0;

	if (d->Timeout < 0 || IsWritable(d->Timeout))
	{
		Status = send
		(
			d->Socket,
			(char*)Data,
			Len,
			Flags
			#ifndef MAC
			| MSG_NOSIGNAL
			#endif
		);
	}
	
	if (Status < 0)
		Error();
	else if (Status == 0)
		OnDisconnect();
	else
	{
		if (Status < Len)
		{
			// Just in case it's a string lets be safe
			((char*)Data)[Status] = 0;
		}

		BytesWritten += Status;
		OnWrite((char*)Data, Status);
	}

	return Status;
}

int GSocket::Read(void *Data, int Len, int Flags)
{
	if (!ValidSocket(d->Socket) || !Data)
		return -1;

	int Status = -1;

	if (d->Timeout < 0 || IsReadable(d->Timeout))
	{
		Status = recv(d->Socket, (char*)Data, Len, Flags
			#ifdef MSG_NOSIGNAL
			| MSG_NOSIGNAL
			#endif
			);
	}

	Log("Read", Status, (char*)Data, Status>0 ? Status : 0);

	if (Status < 0)
		Error();
	else if (Status == 0)
		OnDisconnect();
	else
	{
		if (Status < Len)
		{
			// Just in case it's a string lets be safe
			((char*)Data)[Status] = 0;
		}

		BytesRead += Status;
		OnRead((char*)Data, Status);
	}

	return Status;
}

int GSocket::Error(void *Param)
{
	// Get the most recent error.
	if (!(d->LastError =
		#ifdef WIN32
		WSAGetLastError()
		#else
		errno
		#endif
		))
		return 0;

	// These are not really errors...
	if (d->LastError == EWOULDBLOCK ||
		d->LastError == EISCONN)
		return 0;

	class ErrorMsg {
	public:
		int Code;
		char *Msg;
	}
	ErrorCodes[] =
	{
		{0,					"Socket disconnected."},

		#if defined WIN32
		{WSAEACCES,			"Permission denied."},
		{WSAEADDRINUSE,		"Address already in use."},
		{WSAEADDRNOTAVAIL,	"Cannot assign requested address."},
		{WSAEAFNOSUPPORT,	"Address family not supported by protocol family."},
		{WSAEALREADY,		"Operation already in progress."},
		{WSAECONNABORTED,	"Software caused connection abort."},
		{WSAECONNREFUSED,	"Connection refused."},
		{WSAECONNRESET,		"Connection reset by peer."},
		{WSAEDESTADDRREQ,	"Destination address required."},
		{WSAEFAULT,			"Bad address."},
		{WSAEHOSTDOWN,		"Host is down."},
		{WSAEHOSTUNREACH,	"No route to host."},
		{WSAEINPROGRESS,	"Operation now in progress."},
		{WSAEINTR,			"Interrupted function call."},
		{WSAEINVAL,			"Invalid argument."},
		{WSAEISCONN,		"Socket is already connected."},
		{WSAEMFILE,			"Too many open files."},
		{WSAEMSGSIZE,		"Message too long."},
		{WSAENETDOWN,		"Network is down."},
		{WSAENETRESET,		"Network dropped connection on reset."},
		{WSAENETUNREACH,	"Network is unreachable."},
		{WSAENOBUFS,		"No buffer space available."},
		{WSAENOPROTOOPT,	"Bad protocol option."},
		{WSAENOTCONN,		"Socket is not connected."},
		{WSAENOTSOCK,		"Socket operation on non-socket."},
		{WSAEOPNOTSUPP,		"Operation not supported."},
		{WSAEPFNOSUPPORT,	"Protocol family not supported."},
		{WSAEPROCLIM,		"Too many processes."},
		{WSAEPROTONOSUPPORT,"Protocol not supported."},
		{WSAEPROTOTYPE,		"Protocol wrong type for socket."},
		{WSAESHUTDOWN,		"Cannot send after socket shutdown."},
		{WSAESOCKTNOSUPPORT,"Socket type not supported."},
		{WSAETIMEDOUT,		"Connection timed out."},
		{WSAEWOULDBLOCK,	"Operation would block."},
		{WSAHOST_NOT_FOUND,	"Host not found."},
		{WSANOTINITIALISED,	"Successful WSAStartup not yet performed."},
		{WSANO_DATA,		"Valid name, no data record of requested type."},
		{WSANO_RECOVERY,	"This is a non-recoverable error."},
		{WSASYSNOTREADY,	"Network subsystem is unavailable."},
		{WSATRY_AGAIN,		"Non-authoritative host not found."},
		{WSAVERNOTSUPPORTED,"WINSOCK.DLL version out of range."},
		{WSAEDISCON,		"Graceful shutdown in progress."},
		#else
		{EACCES,			"Permission denied."},
		{EADDRINUSE,		"Address already in use."},
		{EADDRNOTAVAIL,		"Cannot assign requested address."},
		{EAFNOSUPPORT,		"Address family not supported by protocol family."},
		{EALREADY,			"Operation already in progress."},
		{ECONNABORTED,		"Software caused connection abort."},
		{ECONNREFUSED,		"Connection refused."},
		{ECONNRESET,		"Connection reset by peer."},
		{EFAULT,			"Bad address."},
		{EHOSTUNREACH,		"No route to host."},
		{EINPROGRESS,		"Operation now in progress."},
		{EINTR,				"Interrupted function call."},
		{EINVAL,			"Invalid argument."},
		{EISCONN,			"Socket is already connected."},
		{EMFILE,			"Too many open files."},
		{EMSGSIZE,			"Message too long."},
		{ENETDOWN,			"Network is down."},
		{ENETRESET,			"Network dropped connection on reset."},
		{ENETUNREACH,		"Network is unreachable."},
		{ENOBUFS,			"No buffer space available."},
		{ENOPROTOOPT,		"Bad protocol option."},
		{ENOTCONN,			"Socket is not connected."},
		{ENOTSOCK,			"Socket operation on non-socket."},
		{EOPNOTSUPP,		"Operation not supported."},
		{EPFNOSUPPORT,		"Protocol family not supported."},
		{EPROTONOSUPPORT,	"Protocol not supported."},
		{EPROTOTYPE,		"Protocol wrong type for socket."},
		{ESHUTDOWN,			"Cannot send after socket shutdown."},
		{ETIMEDOUT,			"Connection timed out."},
		{EWOULDBLOCK,		"Resource temporarily unavailable."},
		{HOST_NOT_FOUND,	"Host not found."},
		{NO_DATA,			"Valid name, no data record of requested type."},
		{NO_RECOVERY,		"This is a non-recoverable error."},
		{TRY_AGAIN,			"Non-authoritative host not found."},
		{ETIMEOUT,			"Operation timed out."},

		#ifndef BEOS
		{EDESTADDRREQ,		"Destination address required."},
		{EHOSTDOWN,			"Host is down."},
		{ESOCKTNOSUPPORT,	"Socket type not supported."},
		#endif
		
		#endif
		{-1, 0}
	};

	ErrorMsg *Error = ErrorCodes;
	while (Error->Code >= 0 AND Error->Code != d->LastError)
	{
		Error++;
	}

	if (d->LastError == 10060 && Param)
	{
		HostEnt *He = (HostEnt*)Param;
		char s[256];
		sprintf(s, "%s (gethostbyname returned '%s')", Error->Msg, He->h_name);
		OnError(d->LastError, s);
	}
	else
		#if defined(MAC)
		if (d->LastError != 36)
		#endif
	{
		OnError(d->LastError, (Error->Code >= 0) ? Error->Msg : (char*)"<unknown error>");
	}


	switch (d->LastError)
	{
		case 0:
		#ifdef WIN32
		case 183: // I think this is a XP 'disconnect' ???
		case WSAECONNABORTED:
		case WSAECONNRESET:
		case WSAENETRESET:
		#else
		case ECONNABORTED:
		case ECONNRESET:
		case ENETRESET:
		#endif
		{
			Close();
			break;
		}
	}

	return d->LastError;
}

/*
void GSocket::SetLogFile(char *FileName, int Type)
{
	DeleteArray(d->LogFile);

	if (FileName)
	{
		switch (Type)
		{
			case NET_LOG_HEX_DUMP:
			case NET_LOG_ALL_BYTES:
			{
				d->LogFile = NewStr(FileName);
				d->LogType = Type;
				break;
			}
		}
	}
}
*/

bool GSocket::GetUdp()
{
	return TestFlag(d->Flags, PROTO_UDP);
}

void GSocket::SetUdp(bool b)
{
	if (b) SetFlag(d->Flags, PROTO_UDP);
	else ClearFlag(d->Flags, PROTO_UDP);
}

int GSocket::ReadUdp(void *Buffer, int Size, int Flags, uint32 *Ip, uint16 *Port)
{
	if (!Buffer || Size < 0)
		return -1;

	if (!ValidSocket(d->Socket))
	{
		d->Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (!ValidSocket(d->Socket))
			return -1;
	}

	sockaddr_in a;
	socklen_t AddrSize = sizeof(a);
	ZeroObj(a);
	a.sin_family = AF_INET;
	if (Port)
		a.sin_port = htons(*Port);

	int b = recvfrom(d->Socket, (char*)Buffer, Size, Flags, (sockaddr*)&a, &AddrSize);
	if (b > 0)
	{
		OnRead((char*)Buffer, b);

		if (Ip)
			*Ip = a.sin_addr.OsAddr;
		if (Port)
			*Port = ntohs(a.sin_port);
	}
	return b;
}

int GSocket::WriteUdp(void *Buffer, int Size, int Flags, uint32 Ip, uint16 Port)
{
	if (!Buffer || Size < 0)
		return -1;

	if (!ValidSocket(d->Socket))
	{
		d->Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (!ValidSocket(d->Socket))
			return -1;
	}

	sockaddr_in a;
	ZeroObj(a);
	a.sin_family = AF_INET;
	a.sin_port = htons(Port);
	a.sin_addr.OsAddr = Ip;
	int b = sendto(d->Socket, (char*)Buffer, Size, Flags, (sockaddr*)&a, sizeof(a));
	if (b > 0)
	{
		OnWrite((char*)Buffer, b);
	}
	return b;
}

//////////////////////////////////////////////////////////////////////////////
bool HaveNetConnection()
{
	bool Status = false;

	// Check for dial up connection
	#if defined WIN32
	typedef DWORD (__stdcall *RasEnumConnections_Proc)(LPRASCONN lprasconn, LPDWORD lpcb, LPDWORD lpcConnections); 
	typedef DWORD (__stdcall *RasGetConnectStatus_Proc)(HRASCONN hrasconn, LPRASCONNSTATUS lprasconnstatus);

	HMODULE hRas = (HMODULE) LoadLibrary("rasapi32.dll");
	if (hRas)
	{
		RASCONN Con[10];
		DWORD Connections = 0;
		DWORD Bytes = sizeof(Con);

		ZeroObj(Con);
		Con[0].dwSize = sizeof(Con[0]);

		RasEnumConnections_Proc pRasEnumConnections = (RasEnumConnections_Proc) GetProcAddress(hRas, "RasEnumConnectionsA"); 
		RasGetConnectStatus_Proc pRasGetConnectStatus = (RasGetConnectStatus_Proc) GetProcAddress(hRas, "RasGetConnectStatusA"); 

		if (pRasEnumConnections AND
			pRasGetConnectStatus)
		{
			pRasEnumConnections(Con, &Bytes, &Connections);

			for (int i=0; i<Connections; i++)
			{
				RASCONNSTATUS Stat;
				char *Err = "";

				ZeroObj(Stat);
				Stat.dwSize = sizeof(Stat);

				DWORD Error = 0;				
				if (!(Error=pRasGetConnectStatus(Con[i].hrasconn, &Stat)))
				{
					if (Stat.rasconnstate == RASCS_Connected)
					{
						Status = true;
					}
				}

				/*
				else
				{
					switch (Error)
					{
						case ERROR_NOT_ENOUGH_MEMORY:
						{
							Err = "Not enuf mem.";
							break;
						}
						case ERROR_INVALID_HANDLE:
						{
							Err = "Invalid Handle.";
							break;
						}
					}
				}

				int n=0;
				*/
			}
		}

		FreeLibrary(hRas);
	}
	#endif

	// Check for ethernet connection
	if (!Status)
	{
	}

	return Status;
}


///////////////////////////////////////////////////////////////////////////////////////////
bool WhatsMyIp(char *IpAddr)
{
	bool Status = false;

	strcpy(IpAddr, "(none)");
	GNetwork Sockets;
	if (Sockets.IsValid())
	{
		#if defined WIN32
		char hostname[256];
		HostEnt *e = NULL;
		if (gethostname(hostname, sizeof(hostname)) != SOCKET_ERROR)
		{
			e = gethostbyname(hostname);
		}

		if (e)
		{
			int Which = 0;
			for (; e->h_addr_list[Which]; Which++);
			Which--;

			sprintf(	IpAddr,
						"%i.%i.%i.%i",
						(uchar)e->h_addr_list[Which][0],
						(uchar)e->h_addr_list[Which][1],
						(uchar)e->h_addr_list[Which][2],
						(uchar)e->h_addr_list[Which][3]);
			Status = true;
		}
		#elif defined BEOS

		struct sockaddr_in addr;
		int sock, size;
		
		memset((char *)&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = 0;
		
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			return false;
		
		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
			return false;
		
		size = sizeof(addr);
		if ((getsockname(sock, (struct sockaddr *)&addr, &size)) < 0)
			return false;
		
		close(sock);
		
		if (addr.sin_addr.s_addr == INADDR_ANY)
			return false;
		
		uchar *a = (uchar*)&addr.sin_addr.s_addr;
		sprintf(	IpAddr,
					"%i.%i.%i.%i",
					a[0],
					a[1],
					a[2],
					a[3]);
	
		Status = true;
		
		#endif
	}

	return Status;
}

//////////////////////////////////////////////////////////////////////
#define SOCKS5_VER					5

#define SOCKS5_CMD_CONNECT			1
#define SOCKS5_CMD_BIND				2
#define SOCKS5_CMD_ASSOCIATE		3

#define SOCKS5_ADDR_IPV4			1
#define SOCKS5_ADDR_DOMAIN			3
#define SOCKS5_ADDR_IPV6			4

#define SOCKS5_AUTH_NONE			0
#define SOCKS5_AUTH_GSSAPI			1
#define SOCKS5_AUTH_USER_PASS		2

GSocks5Socket::GSocks5Socket()
{
	Socks5Connected = false;
}

void GSocks5Socket::SetProxy(const GSocks5Socket *s)
{
	Proxy.Reset(s ? NewStr(s->Proxy) : 0);
	Port = s ? s->Port : 0;
	UserName.Reset(s ? NewStr(s->UserName) : 0);
	Password.Reset(s ? NewStr(s->Password) : 0);
}

void GSocks5Socket::SetProxy(char *proxy, int port, char *username, char *password)
{
	Proxy.Reset(NewStr(proxy));
	Port = port;
	UserName.Reset(NewStr(username));
	Password.Reset(NewStr(password));
}

int GSocks5Socket::Open(char *HostAddr, int port)
{
	bool Status = false;
	if (HostAddr)
	{
		char Msg[256];
		sprintf(Msg, "[SOCKS5] Connecting to proxy server '%s'", HostAddr);
		OnInformation(Msg);

		Status = GSocket::Open(Proxy, Port);
		if (Status)
		{
			char Buf[1024];
			Buf[0] = SOCKS5_VER;
			Buf[1] = 2; // methods
			Buf[2] = SOCKS5_AUTH_NONE;
			Buf[3] = SOCKS5_AUTH_USER_PASS;
			
			// No idea how to implement this.
			// AuthReq[3] = SOCKS5_AUTH_GSSAPI;

			OnInformation("[SOCKS5] Connected, Requesting authentication type.");
			GSocket::Write(Buf, 4, 0);
			if (GSocket::Read(Buf, 2, 0) == 2)
			{
				if (Buf[0] == SOCKS5_VER)
				{
					bool Authenticated = false;
					switch (Buf[1])
					{
						case SOCKS5_AUTH_NONE:
						{
							Authenticated = true;
							OnInformation("[SOCKS5] No authentication needed.");
							break;
						}
						case SOCKS5_AUTH_USER_PASS:
						{
							OnInformation("[SOCKS5] User/Pass authentication needed.");
							if (UserName AND Password)
							{
								char *b = Buf;
								*b++ = 1; // ver of sub-negotiation ??
								
								int NameLen = strlen(UserName);
								*b++ = NameLen;
								strcpy(b, UserName);
								b += NameLen;

								int PassLen = strlen(Password);
								*b++ = PassLen;
								strcpy(b, Password);
								b += PassLen;

								GSocket::Write(Buf, 3 + NameLen + PassLen, 0);
								if (GSocket::Read(Buf, 2, 0) == 2)
								{
									Authenticated = (Buf[0] == 1 AND Buf[1] == 0);
								}

								if (!Authenticated)
								{
									OnInformation("[SOCKS5] User/Pass authentication failed.");
								}
							}
							break;
						}
					}

					if (Authenticated)
					{
						OnInformation("[SOCKS5] Authentication successful.");

						int HostPort = htons(port);
						
						// Header
						char *b = Buf;
						*b++ = SOCKS5_VER;
						*b++ = SOCKS5_CMD_CONNECT;
						*b++ = 0; // reserved

						long IpAddr = inet_addr(HostAddr);
						if (IpAddr != -1)
						{
							// Ip
							*b++ = SOCKS5_ADDR_IPV4;
							memcpy(b, &IpAddr, 4);
							b += 4;
						}
						else
						{
							// Domain Name
							*b++ = SOCKS5_ADDR_DOMAIN;
							int Len = strlen(HostAddr);
							*b++ = Len;
							strcpy(b, HostAddr);
							b += Len;
						}

						// Port
						memcpy(b, &HostPort, 2);
						b += 2;

						GSocket::Write(Buf, (int)b - (int)Buf, 0);
						if (GSocket::Read(Buf, 10, 0) == 10)
						{
							if (Buf[0] == SOCKS5_VER)
							{
								switch (Buf[1])
								{
									case 0:
										Socks5Connected = true;
										OnInformation("[SOCKS5] Connected!");
										break;
									case 1:
										OnInformation("[SOCKS5] General SOCKS server failure");
										break;
									case 2:
										OnInformation("[SOCKS5] Connection not allowed by ruleset");
										break;
									case 3:
										OnInformation("[SOCKS5] Network unreachable");
										break;
									case 4:
										OnInformation("[SOCKS5] Host unreachable");
										break;
									case 5:
										OnInformation("[SOCKS5] Connection refused");
										break;
									case 6:
										OnInformation("[SOCKS5] TTL expired");
										break;
									case 7:
										OnInformation("[SOCKS5] Command not supported");
										break;
									case 8:
										OnInformation("[SOCKS5] Address type not supported");
										break;
									default:
										OnInformation("[SOCKS5] Unknown SOCKS server failure");
										break;
								}
							}
							else
							{
								OnInformation("[SOCKS5] Wrong socks version.");
							}
						}
						else
						{
							OnInformation("[SOCKS5] Connection request read failed.");
						}
					}
					else
					{
						OnInformation("[SOCKS5] Not authenticated.");
					}
				}
				else
				{
					OnInformation("[SOCKS5] Wrong socks version.");
				}
			}
			else
			{
				OnInformation("[SOCKS5] Authentication type read failed.");
			}

			Status = Socks5Connected;
			if (!Status)
			{
				GSocket::Close();
				OnInformation("[SOCKS5] Failure: Disconnecting.");
			}
		}
	}

	return Status;
}

/////////////////////////////////////////////////////////////////////////////////
static char *Ws = " \t\r\n";
#define SkipWs(s) while (*s AND strchr(Ws, *s)) s++;

GUri::GUri(char *uri)
{
	Protocol = 0;
	User = 0;
	Pass = 0;
	Host = 0;
	Port = 0;
	Path = 0;
	Anchor = 0;
	if (uri)
		Set(uri);
}

GUri::~GUri()
{
	Empty();
}

GUri &GUri::operator =(GUri &u)
{
	Empty();
	Protocol = NewStr(u.Protocol);
	User = NewStr(u.User);
	Pass = NewStr(u.Pass);
	Host = NewStr(u.Host);
	Path = NewStr(u.Path);
	Anchor = NewStr(u.Anchor);
	return *this;
}

void GUri::Empty()
{
	Port = 0;
	DeleteArray(Protocol);
	DeleteArray(User);
	DeleteArray(Pass);
	DeleteArray(Host);
	DeleteArray(Path);
	DeleteArray(Anchor);
}

char *GUri::Get()
{
	GStringPipe p;
	if (Protocol)
	{
		p.Print("%s://", Protocol);
	}
	if (User OR Pass)
	{
		char *Empty = "";
		p.Print("%s:%s@", User?User:Empty, Pass?Pass:Empty);
	}
	if (Host)
	{
		p.Write(Host, strlen(Host));
	}
	if (Port)
	{
		p.Print(":%i", Port);
	}
	if (Path)
	{
		GAutoString e = Encode(Path);
		char *s = e ? e : Path;
		p.Print("%s%s", *s == '/' ? "" : "/", s);
	}
	if (Anchor)
		p.Print("#%s", Anchor);
	return p.NewStr();
}

bool GUri::Set(char *uri)
{
	if (!uri)
		return false;

	Empty();

	char *s = uri;
	SkipWs(s);

	// Scan ahead and check for protocol...
	char *p = s;
	while (*s && isalpha(*s)) s++;
	if (s[0] == ':') // && s[1] == '/' && s[2] == '/'
	{
		Protocol = NewStr(p, s - p);
		s++;
		if (*s == '/') s++;
		if (*s == '/') s++;
	}
	else
	{
		// No protocol, so assume it's a host name or path
		s = p;
	}

	// Check for path
	bool HasPath = false;
	if
	(
		(s[0] && s[1] == ':')
		||
		(s[0] == '/')
		||
		(s[0] == '\\')
	)
	{
		HasPath = true;
	}
	else
	{
		// Scan over the host name
		p = s;
		while (	*s AND
				*s > ' ' AND
				*s < 127 AND
				*s != '/' AND
				*s != '\\')
		{
			s++;
		}

		Host = NewStr(p, s - p);
		if (Host)
		{
			char *At = strchr(Host, '@');
			if (At)
			{
				*At++ = 0;
				char *Col = strchr(Host, ':');
				if (Col)
				{
					*Col++ = 0;
					Pass = NewStr(Col);
				}
				User = NewStr(Host);

				memmove(Host, At, strlen(At) + 1);
			}

			char *Col = strchr(Host, ':');
			if (Col)
			{
				*Col++ = 0;
				Port = atoi(Col);
			}
		}

		HasPath = *s == '/';
	}

	if (HasPath)
	{
		char *Start = s;
		while (*s && *s != '#')
			s++;

		if (*s)
		{
			Path = NewStr(Start, s - Start);
			Anchor = NewStr(s + 1);
		}
		else
		{
			Path = NewStr(Start);
		}
		
		char *i = Path, *o = Path;
		while (*i)
		{
			if (*i == '%' AND i[1] AND i[2])
			{
				char h[3] = {i[1], i[2], 0};
				*o++ = htoi(h);
				i+=2;
			}
			else
			{
				*o++ = *i;
			}
			i++;
		}
		*o = 0;
	}

	return Host || Path;
}

GAutoString GUri::Encode(char *s, char *ExtraCharsToEncode)
{
	GStringPipe p(256);
	if (s)
	{
		while (*s)
		{
			if (*s == ' ' || (ExtraCharsToEncode && strchr(ExtraCharsToEncode, *s)))
			{
				char h[4];
				sprintf(h, "%%%02.2X", (uint32)(uchar)*s++);
				p.Write(h, 3);
			}
			else
			{
				p.Write(s++, 1);
			}
		}
	}
	return GAutoString(p.NewStr());
}

GAutoString GUri::Decode(char *s)
{
	GStringPipe p(256);
	if (s)
	{
		while (*s)
		{
			if (s[0] == '%' && s[1] && s[2])
			{
				char h[3] = { s[1], s[2], 0 };
				char c = htoi(h);
				p.Write(&c, 1);
				s += 3;
			}
			else
			{
				p.Write(s++, 1);
			}
		}
	}

	return GAutoString(p.NewStr());
}

#ifdef MAC
int CFNumberRefToInt(CFNumberRef r, int Default = 0)
{
	int i = Default;
	if (r && 
		CFGetTypeID(r) == CFNumberGetTypeID())
	{
		CFNumberGetValue(r, kCFNumberIntType, &r);
	}
	return i;
}
#endif

GProxyUri::GProxyUri()
{
	#if defined(WIN32)

	GRegKey k("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
	if (k.IsOk())
	{
		char *p = k.GetStr("ProxyServer");
		if (p)
		{
			Set(p);
		}
	}

	#elif defined LINUX
	
	char *HttpProxy = getenv("http_proxy");
	if (HttpProxy)
	{
		Set(HttpProxy);
	}
	
	#elif defined MAC
	
	CFDictionaryRef Proxies = SCDynamicStoreCopyProxies(0);
	if (!Proxies)
		LgiTrace("%s:%i - SCDynamicStoreCopyProxies failed.\n", _FL);
	else
	{
		int enable = CFNumberRefToInt((CFNumberRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPEnable));
		if (enable)
		{
			Host = CFStringToUtf8((CFStringRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPProxy));
			Port = CFNumberRefToInt((CFNumberRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPPort));
		}
		
		CFRelease(Proxies);
	}

	#else

	#error "Impl me."

	#endif
}

/////////////////////////////////
void MDStringToDigest(unsigned char digest[16], char *Str, int Len)
{
	md5_state_t ms;
	md5_init(&ms);
	md5_append(&ms, (md5_byte_t*)Str, Len < 0 ? strlen(Str) : Len);
	md5_finish(&ms, (char*)digest);
}

