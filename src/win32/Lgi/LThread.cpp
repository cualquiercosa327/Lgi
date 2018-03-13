
#include "Lgi.h"

//////////////////////////////////////////////////////////////////////////////
#include <process.h>

const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
struct THREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.

    THREADNAME_INFO()
    {
        dwType = 0x1000;
        szName = 0;
        dwThreadID = -1;
        dwFlags = 0;
    }
};
#pragma pack(pop)

uint WINAPI ThreadEntryPoint(void *i)
{
#if defined(_MT) || defined(__MINGW32__)
	LThread *Thread = (LThread*) i;
	if (Thread)
	{
		// Wait for it...
		int Status = 0;
		int Start = LgiCurrentTime();

        while (Thread->State == LThread::THREAD_INIT)
		{
			LgiSleep(5);
			if (LgiCurrentTime() - Start > 2000)
			{
				// If the thread object doesn't set the running state we're
				// stuffed. So timeout in that case after 2 seconds. The only
				// reason I can think that this could happen is if the ResumeThread
				// worked but returned an error.
				break;
			}
		}

		if (Thread->State == LThread::THREAD_RUNNING)
		{
			#ifdef _MSC_VER
            // Set the name if provided...
            if (Thread->Name)
            {
                THREADNAME_INFO info;
                info.szName = Thread->Name;
                info.dwThreadID = Thread->ThreadId;
                __try
                {
                    RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
                }
                __except(EXCEPTION_CONTINUE_EXECUTION)
                {
                }
            }
            #endif

			// Ok now we're ready to go...
			Thread->OnBeforeMain();
			Status = Thread->ReturnValue = Thread->Main();
			Thread->OnAfterMain();
		}

		// Shutdown...
		ThreadError:
		Thread->State = LThread::THREAD_EXITED;
		if (Thread->DeleteOnExit)
		{
			DeleteObj(Thread);
		}
		_endthreadex(Status);
	}
#endif

	return 0;
}

LThread::LThread(const char *name)
{
	State = THREAD_INIT;
	hThread = 0;
	ThreadId = 0;
	ReturnValue = 0;
	DeleteOnExit = false;
	Priority = ThreadPriorityNormal;
	Name = name;
	Create(this, hThread, ThreadId);
}

LThread::~LThread()
{
	LgiAssert(State == THREAD_INIT || State == THREAD_EXITED);
	if (hThread)
	{
		CloseHandle(hThread);
		hThread = 0;
	}
}

void LThread::Create(LThread *Thread, OsThread &hThread, OsThreadId &ThreadId)
{
#if defined(_MT) || defined(__MINGW32__)

	hThread = (HANDLE) _beginthreadex(NULL,
									16<<10,
									(unsigned int (__stdcall *)(void *)) ThreadEntryPoint,
									(LPVOID) Thread,
									CREATE_SUSPENDED,
									(unsigned*) &ThreadId);
#elif defined(__CYGWIN__)

	// Cygwin doesn't support stable threading
	#ifdef _DEBUG
	LgiTrace("%s:%i - Cygwin doesn't have stable threading support.\n", __FILE__, __LINE__);
	#endif

#else

	#error "You are tryed to compile LThread support without the WIN32 multithreaded libs.";

#endif
}

bool LThread::IsExited()
{
	if (State == THREAD_INIT)
		return true;

	bool Alive1 = ExitCode() == STILL_ACTIVE;
	#ifdef _DEBUG
	DWORD w = ::WaitForSingleObject(hThread, 0);
	// LgiTrace("%p Wait=%i Alive=%i\n", hThread, w, Alive1);

	bool Alive2 = WAIT_OBJECT_0 != w;
	if (!Alive2)
	{
		State = THREAD_EXITED;
		return true;
	}
	#endif

	if (!Alive1)
	{
		State = THREAD_EXITED;
		return true;
	}

	return false;
}

void LThread::Run()
{
	if (State == THREAD_EXITED)
	{
		Create(this, hThread, ThreadId);
		if (hThread != INVALID_HANDLE_VALUE)
		{
			State = THREAD_INIT;
		}
		
		switch (Priority)
		{
			case ThreadPriorityIdle:
				SetThreadPriority(hThread, THREAD_PRIORITY_IDLE);
				break;
			case ThreadPriorityHigh:
				SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
				break;
			case ThreadPriorityRealtime:
				SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
				break;
		}
	}

	if (State == THREAD_INIT)
	{
		DWORD Status = ResumeThread(hThread);
		if (Status)
		{
			State = THREAD_RUNNING;
		}
		else
		{
			State = THREAD_ERROR;
		}
	}
}

void LThread::Terminate()
{
	if (hThread)
	{
		TerminateThread(hThread, 1);

		uint64 Start = LgiCurrentTime();
		while (!IsExited())
		{
			uint32 Now = LgiCurrentTime();
			if (Now - Start > 2000)
			{
				LgiTrace("%s:%i - TerminateThread didn't work for '%s'\n", _FL, Name.Get());
				LgiAssert(!"TerminateThread failure?");
				break;
			}

			LgiSleep(10);
		}
	}
}

int LThread::ExitCode()
{
	DWORD d;
	GetExitCodeThread(hThread, &d);
	ReturnValue = d;
	return ReturnValue;
}

int LThread::Main()
{
	return 0;
}


