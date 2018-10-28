//
//  lldb_callback.h
//  lldb_callback
//
//  Created by Matthew Allen on 28/10/18.
//
//

#ifndef lldb_callback_h
#define lldb_callback_h

#define LLDB_CB_PORT			8001
#define LLDB_CMD_TIMEOUT		10000 // ms

enum MsgType
{
	MsgNull,
	MsgSetWatch,
	MsgClearWatch,
	MsgStatus,
};

struct LcbMsg
{
	uint32 Length;
	uint32 Type; // MsgType
	union {
		uint64 u64;
		void *vp;
	};
};

bool LBreakOnWrite(void *p, bool set);

#endif /* lldb_callback_h */
