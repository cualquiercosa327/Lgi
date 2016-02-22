#include "Lgi.h"
#include "GThreadEvent.h"

#if USE_SEM
    #define SEM_NULL -1
#endif
#ifdef LINUX
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

	#if WINNATIVE
	
        Event = CreateEventA(NULL, false, false, name);
        if (Event)
            LastError = GetLastError();
        else
            LgiAssert(!"Failed to create event.");
    
	#elif USE_SEM
    
        char Name[256];
        sprintf_s(Name, sizeof(Name), "lgi.sem.%p", this);
        Sem = sem_open(Name, O_CREAT, 0666, 0);
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
	
    #endif
}

GThreadEvent::~GThreadEvent()
{
	#if WINNATIVE

        CloseHandle(Event);

    #elif USE_SEM

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

	#endif
}

bool GThreadEvent::IsOk()
{
	#if WINNATIVE

        return Event != NULL;

    #elif USE_SEM

        return Sem != SEM_FAILED;

	#elif defined(POSIX)

        return true;

	#else
	
		return false;

	#endif
}

bool GThreadEvent::Signal()
{
	#if WINNATIVE

        if (Event)
            SetEvent(Event);
        else
        {
            LgiAssert(!"No event handle");
            return false;
        }

    #elif USE_SEM
    
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

	#endif
	
	return true;
}

GThreadEvent::WaitStatus GThreadEvent::Wait(int32 Timeout)
{
	#if WINNATIVE

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

    #elif USE_SEM
    
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

	#endif
	
	return WaitError;
}	

uint32 GThreadEvent::GetError()
{
	return LastError;
}
