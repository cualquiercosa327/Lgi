//
//  lldb_callback.cpp
//  lldb_callback
//
//  Created by Matthew Allen on 29/10/18.
//
//

#include "Lgi.h"
#include "INet.h"
#include "lldb_callback.h"

static GSocket Conn;

static bool WriteBuf(void *p, int len)
{
	char *c = (char*)p;
	char *e = c + len;
	while (c < e)
	{
		int remain = e - c;
		auto wr = Conn.Write(c, remain);
		if (wr > 0)
			c += wr;
		else
			return false;
	}
	return true;
}

bool LBreakOnWrite(void *p, bool set)
{
	if (!Conn.IsOpen())
	{
		if (!Conn.Open("localhost", LLDB_CB_PORT))
			return false;
	}

	LcbMsg req, resp;
	req.Length = sizeof(req);
	req.Type = set ? MsgSetWatch : MsgClearWatch;
	req.vp = p;
	
	if (!WriteBuf(&req, req.Length))
	{
		printf("LBreakOnWrite: write failed %s:%i\n", _FL);
		return false;
	}
	int Rd = Conn.Read(&resp, sizeof(resp));
	if (Rd < 0)
		Rd = 0;
	while (Rd < 4 || Rd < resp.Length)
	{
		int r = Conn.Read((char*)&resp + Rd, sizeof(resp) - Rd);
		if (r < 0)
		{
			printf("LBreakOnWrite: read failed %s:%i\n", _FL);
			return 0;
		}
		Rd += r;
	}
	printf("LBreakOnWrite: gotmsg=%i,%i %s:%i\n", resp.Type, (int)resp.u64, _FL);
	return resp.Type == MsgStatus && resp.u64 != 0;
}
