#ifndef _GTHREADEVENT_H_
#define _GTHREADEVENT_H_

class LgiClass GThreadEvent : public GBase
{
	uint32 LastError;
	#if defined(WIN32NATIVE)
	HANDLE Event;
	#elif defined(MAC)
	#elif defined(LINUX)
	pthread_cond_t Cond;
	pthread_mutex_t Mutex;
	#endif

public:
	enum WaitStatus
	{
		WaitError,
		WaitTimeout,
		WaitSignaled,
	};
	
	GThreadEvent(const char *name = NULL);
	~GThreadEvent();
	
	bool IsOk();
	bool Signal();
	WaitStatus Wait(int32 Timeout = -1);
	uint32 GetError();
};

#endif