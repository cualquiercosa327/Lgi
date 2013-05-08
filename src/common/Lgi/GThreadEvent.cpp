#include "Lgi.h"
#include "GThreadEvent.h"

#define DEBUG_THREADING	0

GThreadEvent::GThreadEvent(const char *name)
{
	Name(name);

	#if defined(WIN32NATIVE)
	Event = CreateEventA(NULL, false, false, name);
	if (Event)
		LastError = GetLastError();
	else
		LgiAssert(!"Failed to create event.");
	#elif defined(MAC)
	// NSCondition?
	#elif defined(LINUX)
    pthread_mutexattr_t  mattr;

    pthread_cond_init(&Cond, NULL);
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&Mutex, &mattr);
	#endif
}

GThreadEvent::~GThreadEvent()
{
	#if defined(WIN32NATIVE)
	CloseHandle(Event);
	#elif defined(MAC)
	#elif defined(LINUX)
    pthread_cond_destroy(&Cond);
    pthread_mutex_destroy(&Mutex);
	#endif
}

bool GThreadEvent::IsOk()
{
	#if defined(WIN32NATIVE)
	return Event != NULL;
	#elif defined(MAC)
	#elif defined(LINUX)
	return true;
	#endif
}

bool GThreadEvent::Signal()
{
	#if defined(WIN32NATIVE)
	if (Event)
		SetEvent(Event);
	else
		LgiAssert(!"No event handle");
	#elif defined(MAC)
	#elif defined(LINUX)
    pthread_mutex_lock(&hMutex);
    pthread_cond_signal(&Cond);    /* signal SendThread */
    pthread_mutex_unlock(&Mutex);
	#endif
	
	return false;
}

GThreadEvent::WaitStatus GThreadEvent::Wait(int32 Timeout)
{
	#if defined(WIN32NATIVE)
	DWORD Status = WaitForSingleObject(Event, Timeout < 0 ? INFINITE : Timeout);
	switch (Status)
	{
		case WAIT_OBJECT_0:
			return WaitSignaled;
		case WAIT_TIMEOUT:
			return WaitTimeout;
		default:
			LastError = GetLastError();
			return WaitError;
	}
	#elif defined(MAC)
	#elif defined(LINUX)
	pthread_mutex_lock(&Mutex);
	int result;
	if (Timeout < 0)
	{
		result = pthread_cond_wait(&Event, &Mutex);
	}
	else
	{
		timespec to = {Timeout / 1000, (Timeout % 1000) * 10000000 };
		result = pthread_cond_timedwait(&Cond, &Mutex, &to);
	}
	pthread_mutex_unlock(&Mutex);
	if (result == ETIMEDOUT)
		return WaitTimeout;
	else if (result == 0)
		return WaitSignaled;
	else
		LastError = result;
	#endif
	
	return WaitError;
}	

uint32 GThreadEvent::GetError()
{
	return LastError;
}
