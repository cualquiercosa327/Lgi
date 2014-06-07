#include "Lgi.h"
#include "GThreadEvent.h"

#if defined(POSIX)
#include <errno.h>
#include <sys/time.h>
#endif

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
	#elif defined(POSIX)
	pthread_mutexattr_t  mattr;
    int e = pthread_cond_init(&Cond, NULL);
	if (e)
		printf("%s:%i - pthread_cond_init failed %i\n", _FL, e);
    e = pthread_mutexattr_init(&mattr);
	if (e)
		printf("%s:%i - pthread_mutexattr_init failed %i\n", _FL, e);
    e = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	if (e)
		printf("%s:%i - pthread_mutexattr_settype failed %i\n", _FL, e);
    e = pthread_mutex_init(&Mutex, &mattr);
	if (e)
		printf("%s:%i - pthread_mutex_init failed %i\n", _FL, e);
	#endif
}

GThreadEvent::~GThreadEvent()
{
	#if defined(WIN32NATIVE)
	CloseHandle(Event);
	#elif defined(POSIX)
    pthread_cond_destroy(&Cond);
    pthread_mutex_destroy(&Mutex);
	#endif
}

bool GThreadEvent::IsOk()
{
	#if defined(WIN32NATIVE)
	return Event != NULL;
	#elif defined(POSIX)
	return true;
	#endif
}

bool GThreadEvent::Signal()
{
	#if defined(WIN32NATIVE)
	if (Event)
		SetEvent(Event);
	else
	{
		LgiAssert(!"No event handle");
		return false;
	}
	#elif defined(POSIX)
    int e = pthread_mutex_lock(&Mutex);
	if (e)
		printf("%s:%i - pthread_mutex_lock failed %i\n", _FL, e);
    e = pthread_cond_signal(&Cond);    /* signal SendThread */
	if (e)
		printf("%s:%i - pthread_cond_signal failed %i\n", _FL, e);
    e = pthread_mutex_unlock(&Mutex);
	if (e)
		printf("%s:%i - pthread_mutex_unlock failed %i\n", _FL, e);
	#endif
	
	return true;
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
	#elif defined(POSIX)
	int e = pthread_mutex_lock(&Mutex);
	if (e)
		printf("%s:%i - pthread_mutex_lock failed %i\n", _FL, e);

	int result;
	if (Timeout < 0)
	{
		result = pthread_cond_wait(&Cond, &Mutex);
	}
	else
	{
		timeval tv;
		gettimeofday(&tv, NULL);

		timespec to;
		to.tv_sec = tv.tv_sec + (Timeout / 1000);
		to.tv_nsec = (tv.tv_usec + ((Timeout % 1000) * 1000)) * 1000;

		result = pthread_cond_timedwait(&Cond, &Mutex, &to);
	}
	
	e = pthread_mutex_unlock(&Mutex);
	if (e)
		printf("%s:%i - pthread_mutex_unlock failed %i\n", _FL, e);

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
