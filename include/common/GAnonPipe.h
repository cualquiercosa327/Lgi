#ifndef _GANONPIPE_H_
#define _GANONPIPE_H_

/// Anonymous pipe
class GAnonPipe
{
	struct GAnonPipePriv *d;

public:
	GAnonPipe();
	~GAnonPipe();

	void Close();
	bool IsOk();

	// Timers
	bool SetPulse(int i = -1);
	virtual void OnPulse() {}

	// Messages
	void PostEvent(int cmd, int a = 0, int b = 0);
    GMessage::Result OnEvent(GMessage *Msg);
	GMessage *GetMessage();
};


#endif