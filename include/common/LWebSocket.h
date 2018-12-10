#ifndef _LWEBSOCKET_H_
#define _LWEBSOCKET_H_

#include <functional>
#include "INet.h"

class LSelect
{
protected:
	GArray<GSocket*> s;
	int Select(GArray<GSocket*> &Results, bool Rd, bool Wr, int TimeoutMs);

public:
	LSelect(GSocket *sock = NULL);
	
	LSelect &operator +=(GSocket *sock);
	
	GArray<GSocket*> Readable(int Timeout = -1);
	GArray<GSocket*> Writeable(int Timeout = -1);
};

class LWebSocket : public GSocket
{
	struct LWebSocketPriv *d;

public:
	typedef std::function<bool(char *Data, uint64 Len)> OnMsg;
	LWebSocket(bool Server = true, OnMsg onMsg = nullptr);
	~LWebSocket();

	bool InitFromHeaders(GString Data, OsSocket Sock);
	void ReceiveHandler(OnMsg onMsg);
	bool SendMessage(char *Data, uint64 Len);
	bool SendMessage(GString s) { return SendMessage(s.Get(), s.Length()); }
	bool OnData();
};

#endif