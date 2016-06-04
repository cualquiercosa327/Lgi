#ifndef _GTHREADEVENT_H_
#define _GTHREADEVENT_H_

#if defined(MAC) || defined(LINUX)
    #define USE_SEM         1
#endif
#if USE_SEM
    #include <semaphore.h>
#endif

class LgiClass GThreadEvent : public GBase
{
	uint32 LastError;
    #if USE_SEM
        sem_t *Sem;
	#elif defined(POSIX)
        pthread_cond_t Cond;
        pthread_mutex_t Mutex;
	#elif defined(WIN32)
        HANDLE Event;
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
