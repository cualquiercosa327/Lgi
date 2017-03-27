#include "Lgi.h"
#include "GThreadEvent.h"

#if USE_SEM
	#define SEM_NULL -1
#endif
#if defined(LINUX) || COCOA
	#include <errno.h>
#endif

#if POSIX
	
	#include <sys/time.h>

	void TimeoutToTimespec(struct timespec &to, int TimeoutMs)
	{
		timeval tv;
		gettimeofday(&tv, NULL);

		to.tv_sec = tv.tv_sec + (TimeoutMs / 1000);
		to.tv_nsec = (tv.tv_usec + ((TimeoutMs % 1000) * 1000)) * 1000;
		
		int sec = 1000000000;
		while (to.tv_nsec > sec)
		{
			to.tv_nsec -= sec;
			to.tv_sec++;
		}
	}

#endif

#define DEBUG_THREADING     0

GThreadEvent::GThreadEvent(const char *name)
{
	Name(name);

	#if USE_SEM
	
		char Str[256];
		sprintf_s(Str, sizeof(Str), "lgi.sem.%p", this);
		Sem = sem_open(Str, O_CREAT, 0666, 0);
		if (Sem == SEM_FAILED)
		{
			printf("%s:%i - sem_open failed with %i.\n", _FL, errno);
		}
		else
		{
			#if DEBUG_THREADING
			printf("%p::GThreadEvent init\n", this);
			#endif
		}
	
	#elif !defined(BEOS) && defined(POSIX)
	
		Value = 0;
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
	
	#elif defined(WIN32)
	
		Event = CreateEventA(NULL, false, false, name);
		if (Event)
			LastError = GetLastError();
		else
			LgiAssert(!"Failed to create event.");
	
	#else
	
		#error "Impl me."
	
	#endif
}

GThreadEvent::~GThreadEvent()
{
	#if USE_SEM

		if (Sem != SEM_FAILED)
		{
			sem_close(Sem);
			Sem = SEM_FAILED;

			#if DEBUG_THREADING
			printf("%p::~GThreadEvent destroy\n", this);
			#endif
		}

	#elif defined(POSIX)

		pthread_cond_destroy(&Cond);
		pthread_mutex_destroy(&Mutex);

	#elif defined(WIN32)

		CloseHandle(Event);
	
	#else
	
		#error "Impl me."

	#endif
}

bool GThreadEvent::IsOk()
{
	#if USE_SEM

		return Sem != SEM_FAILED;

	#elif defined(POSIX)

		return true;

	#elif defined(WIN32)

		return Event != NULL;

	#else
	
		#error "Impl me."
		return false;

	#endif
}

bool GThreadEvent::Signal()
{
	#if USE_SEM
	
		if (!IsOk())
			return false;
			
		int r = sem_post(Sem);
		if (r)
		{
			printf("%s:%i - sem_post failed.\n", _FL);
			return false;
		}
		else
		{
			#if DEBUG_THREADING
			printf("%p::GThreadEvent signal\n", this);
			#endif
		}

	#elif defined(POSIX)

		int e = pthread_mutex_lock(&Mutex);
		if (e)
			printf("%s:%i - pthread_mutex_lock failed %i\n", _FL, e);
		e = pthread_mutex_unlock(&Mutex);
		if (e)
			printf("%s:%i - pthread_mutex_unlock failed %i\n", _FL, e);

		e = pthread_cond_signal(&Cond);    /* signal SendThread */
		if (e)
			printf("%s:%i - pthread_cond_signal failed %i\n", _FL, e);

	#elif defined(WIN32)

		if (Event)
		{
			BOOL b = SetEvent(Event);
			if (!b)
			{
				DWORD e = GetLastError();
				LgiTrace("%s:%i - SetEvent failed with: %u\n", _FL, e);
			}
		}
		else
		{
			LgiAssert(!"No event handle");
			return false;
		}
	
	#else
	
		#error "Impl me."

	#endif
	
	return true;
}

GThreadEvent::WaitStatus GThreadEvent::Wait(int32 Timeout)
{
	#if USE_SEM
	
		if (!IsOk())
			return WaitError;
		
		int r;
		if (Timeout < 0)
		{
			r = sem_wait(Sem);
			if (r)
			{
				printf("%s:%i - sem_wait failed with %i.\n", _FL, errno);
				return WaitError;
			}

			#if DEBUG_THREADING
			printf("%p::GThreadEvent signalled\n", this);
			#endif
			return WaitSignaled;
		}
		else
		{
			#ifdef MAC
			
				// No sem_timedwait, so poll instead :(

				#if DEBUG_THREADING
				printf("%p::GThreadEvent starting sem_trywait loop...\n", this);
				#endif

				uint64 Start = LgiCurrentTime();
				while (true)
				{
					r = sem_trywait(Sem);
					if (r)
					{
						if (errno != EAGAIN)
						{
							printf("%s:%i - sem_trywait failed with %i.\n", _FL, errno);
							return WaitError;
						}
					}
					else
					{
						#if DEBUG_THREADING
						printf("%p::GThreadEvent signalled\n", this);
						#endif
						return WaitSignaled;
					}
					
					if (LgiCurrentTime() - Start >= Timeout)
					{
						#if DEBUG_THREADING
						printf("%p::GThreadEvent timed out\n", this);
						#endif
						return WaitTimeout;
					}
					
					LgiSleep(1);
				}

			#else

				timespec to;
				TimeoutToTimespec(to, Timeout);
				r = sem_timedwait(Sem, &to);

			#endif
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
			timespec to;
			TimeoutToTimespec(to, Timeout);
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

	#elif defined(WIN32)

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
	
	#else
	
		#error "Impl me.")
	
	#endif
	
	return WaitError;
}

uint32 GThreadEvent::GetError()
{
	return LastError;
}
