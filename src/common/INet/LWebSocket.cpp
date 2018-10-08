// https://tools.ietf.org/html/rfc6455
#include "Lgi.h"
#include "LWebSocket.h"
#include "INetTools.h"
#include "../../src/common/Hash/sha1/sha1.h"
#include "Base64.h"

////////////////////////////////////////////////////////////////////////////
LSelect::LSelect(GSocket *sock)
{
	if (sock)
		*this += sock;
}
	
LSelect &LSelect::operator +=(GSocket *sock)
{
	if (sock)
		s.Add(sock);
	return *this;
}
	
int LSelect::Select(GArray<GSocket*> &Results, int Flags, int TimeoutMs)
{
	if (s.Length() == 0)
		return 0;

	#ifdef LINUX

	// Because Linux doesn't return from select() when the socket is
	// closed elsewhere we have to do something different... damn Linux,
	// why can't you just like do the right thing?
		
	struct pollfd fds;
	fds.fd = s;
	fds.events = POLLIN | POLLRDHUP | POLLERR;
	fds.revents = 0;

	int r = poll(&fds, 1, TimeoutMs);
	if (r > 0)
	{
		return fds.revents != 0;
	}
	else if (r < 0)
	{
		Error();
	}
		
	#else
		
	struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

	fd_set r;
	FD_ZERO(&r);
	OsSocket Max = 0;
	for (auto Sock : s)
	{
		auto h = Sock->Handle();
		if (Max < h)
			Max = h;
		FD_SET(h, &r);
	}
		
	int v = select(	(int)Max+1,
					Flags == O_READ ? &r : NULL,
					Flags == O_WRITE ? &r : NULL,
					NULL, &t);
	if (v > 0)
	{
		for (auto Sock : s)
		{
			if (FD_ISSET(Sock->Handle(), &r))
				Results.Add(Sock);
		}
	}

	return v;

	#endif
}

GArray<GSocket*> LSelect::Readable(int TimeoutMs)
{
	GArray<GSocket*> r;
	Select(r, O_READ, TimeoutMs);
	return r;
}

GArray<GSocket*> LSelect::Writeable(int TimeoutMs)
{
	GArray<GSocket*> r;
	Select(r, O_WRITE, TimeoutMs);
	return r;
}

////////////////////////////////////////////////////////////////////////////
enum WebSocketState
{
	WsReceiveHdr,
	WsMessages,
	WsClosed,
};

enum WsOpCode
{
	WsContinue = 0,
	WsText = 1,
	WsBinary = 2,
	WsClose = 8,
	WsPing = 9,
	WsPong = 10,
};

struct LWebSocketPriv
{
	LWebSocket *Ws;
	bool Server;

	WebSocketState State;
	GString InHdr, OutHdr;
	GArray<char> Data;
	size_t Start, Used;
	char Buf[512];
	GArray<GRange> Msg;
	LWebSocket::OnMsg onMsg;

	LWebSocketPriv(LWebSocket *ws, bool server) : Ws(ws), Server(server)
	{
		State = WsReceiveHdr;
		Used = 0;
		Start = 0;
	}

	template<typename T>
	ssize_t Write(T *p, size_t len)
	{
		size_t i = 0;
		while (i < len)
		{
			auto Wr = Ws->Write(p + i, len - i);
			if (Wr <= 0)
				return i;
			i += Wr;
		}
		return i;
	}

	bool AddData(char *p, size_t Len)
	{
		if (Used + Len > Data.Length())
			Data.Length(Used + Len + 1024);
		memcpy(Data.AddressOf(Used), p, Len);
		Used += Len;
		
		if (State != WsMessages)
			return false;
		return CheckMsg();
	}

	void OnMsg(char *p, uint64 Len)
	{
		if (onMsg)
			onMsg(p, Len);
	}

	bool CheckMsg()
	{
		// Check for valid message..
		if (Used < 2)
			return false; // too short

		uint8 *p = (uint8*)Data.AddressOf(Start);
		uint8 *End = p + Used;
		bool Fin = (p[0] & 0x80) != 0;
		WsOpCode OpCode = (WsOpCode) (p[0] & 0xf);
		bool Masked = (p[1] & 0x80) != 0;
		uint64 Len = p[1] & 0x7f;
		if (Len == 126)
		{
			if (Used < 4)
				return false; // Too short
			Len = (p[2] << 8) || p[3];
			p += 4;
		}
		else if (Len == 127)
		{
			if (Used < 10)
				return false; // Too short
			p += 2;
			Len = ((uint64)p[0] << 54) | ((uint64)p[1] << 48) | ((uint64)p[2] << 40) | ((uint64)p[3] << 32) |
				  ((uint64)p[4] << 24) | ((uint64)p[5] << 16) | ((uint64)p[6] << 8)  | p[7];
			p += 8;
		}
		else p += 2;

		uint8 Mask[4];
		if (Masked)
		{
			if (p > End - 4)
				return false; // Too short
			Mask[0] = *p++;
			Mask[1] = *p++;
			Mask[2] = *p++;
			Mask[3] = *p++;

			if (End - p < (ssize_t)Len)
				return false; // Too short

			// Unmask
			for (uint64 i=0; i<Len; i++)
				p[i] ^= Mask[i & 3];
		}
		else
		{
			if (End - p < (ssize_t)Len)
				return false; // Too short
		}

		bool Status = false;
		if (Fin)
		{
			if (Msg.Length() == 0)
			{
				// Process the message
				OnMsg((char*)p, Len);
				p += Len;
				Status = true;

				// Consume the data
				auto Remaining = End - p;
				auto Pos = (char*)p - Data.AddressOf();
				memcpy(Data.AddressOf(), Data.AddressOf(Pos), Remaining);
				Used = Remaining;				
			}
			else
				LgiAssert(!"Impl me");
		}
		else
		{
			auto Pos = (char*)p - Data.AddressOf();
			Msg.New().Set(Pos, Len);
			Start = Pos + Len;
		}

		if (OpCode == WsClose)
		{
			Ws->Close();
			State = WsClosed;
		}

		return Status;
	}

	bool Error(const char *Msg)
	{
		return false;
	}

	bool SendResponse()
	{
		// Create the response hdr and send it...
		static const char *Key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

		GAutoString Upgrade(InetGetHeaderField(InHdr, "Upgrade", InHdr.Length()));
		if (!Upgrade || stricmp(Upgrade, "websocket"))
			return false;

		GAutoString SecWebSocketKey(InetGetHeaderField(InHdr, "Sec-WebSocket-Key", InHdr.Length()));
		if (!SecWebSocketKey)
			return Error("No Sec-WebSocket-Key header");
		
		GString s = SecWebSocketKey.Get();
		s += Key;

		SHA1Context Ctx;
		SHA1Reset(&Ctx);
		SHA1Input(&Ctx, (const uchar*) s.Get(), (unsigned) s.Length());
		if (!SHA1Result(&Ctx))
			return Error("SHA1Result failed");

		uint32 Digest[5];
		for (int i=0; i<CountOf(Digest); i++)
			Digest[i] = htonl(Ctx.Message_Digest[i]);

		char B64[64];
		auto c = ConvertBinaryToBase64(B64, sizeof(B64), (uchar*) &Digest[0], sizeof(Digest));
		if (c <= 0)
			return Error("ConvertBinaryToBase64 failed");

		B64[c] = 0;
		OutHdr.Printf("HTTP/1.1 101 Switching Protocols\r\n"
					"Upgrade: websocket\r\n"
					"Connection: Upgrade\r\n"
					"Sec-WebSocket-Accept: %s\r\n\r\n", B64);
		auto Wr = Write(OutHdr.Get(), OutHdr.Length());
		if (Wr != OutHdr.Length())
			return Error("Writing HTTP response failed.");

		State = WsMessages;
		return CheckMsg();
	}
};

LWebSocket::LWebSocket(bool Server, OnMsg onMsg)
{
	d = new LWebSocketPriv(this, Server);
	if (onMsg)
		ReceiveHandler(onMsg);
}

LWebSocket::~LWebSocket()
{
	delete d;
}

void LWebSocket::ReceiveHandler(OnMsg onMsg)
{
	d->onMsg = onMsg;
}

bool LWebSocket::SendMessage(char *Data, uint64 Len)
{
	if (d->State != WsMessages)
		return false;

	uint8 Masked = d->Server ? 0 : 0x80;
	int8 Hdr[2 + 8 + 4];
	GPointer p = {Hdr};
	*p.u8++ =	0x80 | // Fin
				WsText;
	if (Len < 126) // 1 byte
	{
		// Direct len
		*p.u8++ = Masked | (uint8)Len;
	}
	else if (Len <= 0xffff) // 2 byte
	{
		// 126 + 2 bytes
		*p.u8++ = Masked | 126;
		*p.u16++ = htons(Len);
	}
	else
	{
		// 127 + 8 bytes
		*p.u8++ = Masked | 127;
		*p.u64++ = htonll(Len);
	}

	GAutoPtr<uint8> MaskData;
	if (Masked)
	{
		uint8 *Mask = p.u8;
		*p.u32++ = LgiRand();
		if (!MaskData.Reset(new uint8[Len]))
			return d->Error("Alloc failed.");
		
		uint8 *Out = MaskData.Get();
		for (uint64 i=0; i<Len; i++)	
			Out[i] = Data[i] ^ Mask[i&3];
	}

	auto Sz = p.s8 - Hdr;
	auto Wr = d->Write(Hdr, Sz);
	if (Wr != Sz)
		return d->Error("SendMessage.Hdr failed to write to socket");

	Wr = d->Write(MaskData ? MaskData.Get() : (uint8*)Data, Len);
	if (Wr != Len)
		return d->Error("SendMessage.Body failed to write to socket");

	return true;
}

bool LWebSocket::InitFromHeaders(GString Data, OsSocket Sock)
{
	bool HasMsg = false;

	if (d->State == WsReceiveHdr)
	{
		d->InHdr = Data;

		auto End = d->InHdr.Find("\r\n\r\n");
		if (End >= 0)
		{
			if (d->InHdr.Length() > (size_t)End + 4)
			{
				d->AddData(d->InHdr.Get() + End + 4, d->InHdr.Length() - End - 4);
				d->InHdr.Length(End);
			}

			HasMsg = d->SendResponse();
		}

		Handle(Sock);
	}

	return HasMsg;
}

bool LWebSocket::OnData()
{
	bool GotData = false;

	auto Rd = Read(d->Buf, sizeof(d->Buf));
	if (Rd > 0)
	{
		if (d->State == WsReceiveHdr)
		{
			d->InHdr += GString(d->Buf, Rd);
			auto End = d->InHdr.Find("\r\n\r\n");
			if (End >= 0)
			{
				if (d->InHdr.Length() > (size_t)End + 4)
				{
					d->AddData(d->InHdr.Get() + End + 4, d->InHdr.Length() - End - 4);
					d->InHdr.Length(End);
				}

				GotData = d->SendResponse();
			}
		}
		else
		{
			GotData = d->AddData(d->Buf, Rd);
		}
	}

	return GotData;
}
