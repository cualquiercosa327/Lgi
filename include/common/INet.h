/**
	\file
	\author Matthew Allen
	\date 28/5/1998
	\brief Network sockets classes
	Copyright (C) 1998, <a href="mailto:fret@memecode.com">Matthew Allen</a>
*/

#ifndef __INET_H
#define __INET_H

#include "LgiNetInc.h"
#include "LgiInterfaces.h"
#include "GMem.h"
#include "GContainers.h"
#include "GStream.h"
#include "GString.h"
#include "LCancel.h"

#if defined WIN32
	
	#include "ws2ipdef.h"

#elif defined POSIX

    #include <netinet/in.h>

#elif defined BEOS

	#include <sys/socket.h>
	#include <netdb.h>

#else

typedef int SOCKET;

#endif

#define DEFAULT_TIMEOUT			30

// Standard ports
#define FTP_PORT				21
#define SMTP_PORT				25
#define HTTP_PORT				80
#define HTTPS_PORT				443
#define SMTP_SSL_PORT			465
#define POP3_PORT				110
#define POP3_SSL_PORT			995
#define IMAP_PORT				143
#define IMAP_SSL_PORT			993

// Parameters for passing to GSocket::SetVariant

/// Turn on/off logging. Used with GSocket::SetParameter.
#define GSocket_Log				"Log"
/// Set the progress object. Used with GSocket::SetParameter. Value = (GProgress*)Prog
#define GSocket_Progress		"Progress"
/// Set the size of the transfer. Used with GSocket::SetParameter. Value = (int)Size
#define GSocket_TransferSize	"TransferSize"
/// Set the type of protocol. Used with GSocket::SetParameter. Value = (char*)Protocol
#define GSocket_Protocol		"Protocol"

// Functions
LgiNetFunc bool HaveNetConnection();
LgiNetFunc bool WhatsMyIp(GAutoString &Ip);

/// Make md5 hash
LgiNetFunc void MDStringToDigest
(
	/// Buffer to receive md5 hash
	unsigned char digest[16],
	/// Input string
	char *Str,
	/// Length of input string or -1 for null terminated
	int Len = -1
);


/// Implementation of a network socket
class LgiNetClass GSocket :
	public GSocketI,
	public GStream
{
protected:
	class GSocketImplPrivate *d;

	// Methods
	void Log(const char *Msg, ssize_t Ret, const char *Buf, ssize_t Len);
	bool CreateUdpSocket();

public:
	ssize_t	BytesRead, BytesWritten;

	/// Creates the class
	GSocket(GStreamI *logger = 0, void *unused_param = 0);
	
	/// Destroys the class
	~GSocket();

	/// Gets the active cancellation object
	LCancel *GetCancel();

	/// Sets the active cancellation object
	void SetCancel(LCancel *c);

	/// Returns the operating system handle to the socket.
	OsSocket Handle(OsSocket Set = INVALID_SOCKET);

	/// Returns true if the internal state of the class is ok
	bool IsOK();
	
	/// Returns the IP address at this end of the socket.
	bool GetLocalIp(char *IpAddr);
	
	/// Returns the port at this end of the socket.
	int GetLocalPort();
	
	/// Gets the IP address at the remote end of the socket.
	bool GetRemoteIp(char *IpAddr);

	/// Gets the IP address at the remote end of the socket.
	int GetRemotePort();

	/// Gets the current timeout for operations in ms
	int GetTimeout();

	/// Sets the current timeout for operations in ms
	void SetTimeout(int ms);

	/// Returns whether there is data available for reading.
	bool IsReadable(int TimeoutMs = 0);

	/// Returns whether there is data available for reading.
	bool IsWritable(int TimeoutMs = 0);

	/// Returns if the socket is ready to accept a connection
	bool CanAccept(int TimeoutMs = 0);

	/// Returns if the socket is set to block
	bool IsBlocking();

	/// Set the socket to block
	void IsBlocking(bool block);

	/// Get the send delay setting
	bool IsDelayed();

	/// Set the send delay setting
	void IsDelayed(bool Delay);

	/// Opens a connection.
	int Open
	(
		/// The name of the remote host.
		const char *HostAddr,
		/// The port on the remote host.
		int Port
	);
	
	/// Returns true if the socket is connected.
	bool IsOpen();
	
	/// Closes the connection to the remote host.
	int Close();

	/// Listens on a given port for an incomming connection.
	bool Listen(int Port = 0);
	
	/// Accepts an incomming connection and connects the socket you pass in to the remote host.
	bool Accept
	(
		/// The socket to handle the connection.
		GSocketI *c
	);

	/// \brief Sends data to the remote host.
	/// \return the number of bytes written or <= 0 on error.
	ssize_t Write
	(
		/// Pointer to the data to write
		const void *Data,
		/// Numbers of bytes to write
		ssize_t Len,
		/// Flags to pass to send
		int Flags = 0
	);
	
	/// \brief Reads data from the remote host.
	/// \return the number of bytes read or <= 0 on error.
	///
	/// Generally the number of bytes returned is less than the buffer size. Depending on how much data
	/// you are expecting you will need to keep reading until you get and end of field marker or the number
	/// of bytes your looking for.
	ssize_t Read
	(
		/// Pointer to the buffer to write output to
		void *Data,
		/// The length of the receive buffer.
		ssize_t Len,
		/// The flags to pass to recv
		int Flags = 0
	);
	
	/// Returns the last error or 0.
	int Error(void *Param = 0);
	const char *GetErrorString();

	/// Not supported
	int64 GetSize() { return -1; }
	/// Not supported
	int64 SetSize(int64 Size) { return -1; }
	/// Not supported
	int64 GetPos() { return -1; }
	/// Not supported
	int64 SetPos(int64 Pos) { return -1; }

	/// Gets called when the connection is disconnected
	void OnDisconnect();
	
	/// Gets called when data is received.
	void OnRead(char *Data, ssize_t Len) {}

	/// Gets called when data is sent.
	void OnWrite(const char *Data, ssize_t Len) {}
	
	/// Gets called when an error occurs.
	void OnError(int ErrorCode, const char *ErrorDescription);
	
	/// Gets called when some information is available.
	void OnInformation(const char *Str) {}
	
	/// Parameter change handler.
	int SetParameter
	(
		/// e.g. #GSocket_Log
		int Param,
		int Value
	) { return false; }

	/// Get UPD mode
	bool GetUdp();
	/// Set UPD mode
	void SetUdp(bool b);
	/// Makes the socket able to broadcast
	void SetBroadcast();
	/// Read UPD packet
	int ReadUdp(void *Buffer, int Size, int Flags, uint32 *Ip = 0, uint16 *Port = 0);
	/// Write UPD packet
	int WriteUdp(void *Buffer, int Size, int Flags, uint32 Ip, uint16 Port);
	
	bool AddMulticastMember(uint32 MulticastIp, uint32 LocalInterface);
	bool SetMulticastInterface(uint32 Interface);

	// Impl
	GStreamI *Clone() { return new GSocket; }

	// Statics

	/// Enumerates the current interfaces
	struct Interface
	{
		GString Name;
		uint32 Ip4; // Host order...
		uint32 Netmask4;

		GString ToString(uint32 ip = 0)
		{
			GString s;
			if (!ip)
				ip = Ip4;
			s.Printf("%i.%i.%i.%i",
					(ip>>24)&0xff,
					(ip>>16)&0xff,
					(ip>>8)&0xff,
					(ip)&0xff);
			return s;
		}
	};

	static bool EnumInterfaces(GArray<Interface> &Out);
};

class LgiNetClass GSocks5Socket : public GSocket
{
	GAutoString Proxy;
	int Port;
	GAutoString UserName;
	GAutoString Password;

protected:
	bool Socks5Connected;

public:
	GSocks5Socket();

	GSocks5Socket &operator=(const GSocks5Socket &s)
	{
		Proxy.Reset(NewStr(s.Proxy));
		UserName.Reset(NewStr(s.UserName));
		Password.Reset(NewStr(s.Password));
		Port = s.Port;
		return *this;
	}

	// Connection
	void SetProxy(char *proxy, int port, char *username, char *password);
	void SetProxy(const GSocks5Socket *s);
	int Open(const char *HostAddr, int port);

	// Server
	bool Listen(int Port) { return false; }
};

/// Uri parser
class LgiNetClass GUri
{
public:
	char *Protocol;
	char *User;
	char *Pass;
	char *Host;
	int Port;
	char *Path;
	char *Anchor;

	/// Parser for URI's.
	GUri
	(
		/// Optional URI to start parsing
		const char *uri = 0
	);
	~GUri();

	/// Parse a URI into it's sub fields...
	bool Set(const char *uri);

	/// Re-constructs the URI
	GAutoString GetUri();

	/// Empty this object...
	void Empty();

	/// URL encode
	GAutoString Encode
	(
		/// The string to encode
		const char *s,
		/// [Optional] Any extra characters you want encoded
		const char *ExtraCharsToEncode = 0
	);

	/// URL decode
	GAutoString Decode(char *s);

	GUri &operator =(const GUri &u);
	GUri &operator =(char *s) { Set(s); return *this; }
};

/// Proxy settings lookup
class LgiNetClass GProxyUri : public GUri
{
public:
	GProxyUri();
};

#define MAX_UDP_SIZE		512

class LUdpListener : public GSocket
{
	GArray<Interface> Intf;
	GStream *Log;
	GString Context;

public:
	LUdpListener(uint32 mc_ip, uint16 port, GStream *log = NULL) : Log(log)
	{
		SetBroadcast();
		SetUdp(true);
		EnumInterfaces(Intf);

		struct sockaddr_in addr;
		ZeroObj(addr);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		#ifdef WINDOWS
		addr.sin_addr.S_un.S_addr = INADDR_ANY;
		#else
		addr.sin_addr.s_addr = INADDR_ANY;
		#endif

		#if 1

		if (mc_ip)
		{
			for (auto &i : Intf)
			{
				Context.Printf("AddMulticastMember(%x,%x)", mc_ip, i.Ip4);
				AddMulticastMember(mc_ip, i.Ip4);
			}
		}

		#endif

		int r = bind(Handle(), (struct sockaddr*)&addr, sizeof(addr));
		if (r)
		{
			#ifdef WIN32
			int err = WSAGetLastError();
			OnError(err, NULL);
			#endif
		}
	}

	bool ReadPacket(GString &d, uint32 &Ip, uint16 &Port)
	{
		if (!IsReadable(10))
			return false;

		char Data[MAX_UDP_SIZE];
		int Rd = ReadUdp(Data, sizeof(Data), 0, &Ip, &Port);
		if (Rd <= 0)
			return false;

		d.Set(Data, Rd);
		return true;
	}

	void OnError(int ErrorCode, const char *ErrorDescription)
	{
		if (Log)
			Log->Print("Error: %s - %i, %s\n", Context.Get(), ErrorCode, ErrorDescription);
	}
};

class LUdpBroadcast : public GSocket
{
	GArray<Interface> Intf;

public:
	LUdpBroadcast()
	{
        SetBroadcast();
		SetUdp(true);
		EnumInterfaces(Intf);
	}

	bool BroadcastPacket(GString Data, uint32 Ip, uint16 Port)
	{
		return BroadcastPacket(Data.Get(), Data.Length(), Ip, Port);
	}

	bool BroadcastPacket(void *Ptr, size_t Size, uint32 Ip, uint16 Port)
	{
		if (Size > MAX_UDP_SIZE)
			return false;
		
		uint32 Netmask = 0xffffff00;
		for (auto &i : Intf)
		{
			if (i.Ip4 == Ip)
			{
				Netmask = i.Netmask4;
				break;
			}
		}

		uint32 BroadcastIp = Ip | ~Netmask;
		#if 0
		printf("Broadcast %i.%i.%i.%i\n", 
			(BroadcastIp >> 24) & 0xff,
			(BroadcastIp >> 16) & 0xff,
			(BroadcastIp >> 8) & 0xff,
			(BroadcastIp) & 0xff);
		#endif
		int wr = WriteUdp(Ptr, (int)Size, 0, BroadcastIp, Port);
		return wr == Size;
	}
};

#endif

