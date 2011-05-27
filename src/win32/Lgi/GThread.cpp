#include "Lgi.h"

//////////////////////////////////////////////////////////////////////////////
#include <process.h>

uint WINAPI ThreadEntryPoint(void *i)
{
#if defined(_MT) || defined(__MINGW32__)
	GThread *Thread = (GThread*) i;
	if (Thread)
	{
		// Wait for it...
		int Status = 0;
		int Start = LgiCurrentTime();

		while (Thread->State == LGITHREAD_INIT)
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

		if (Thread->State == LGITHREAD_RUNNING)
		{
			// Ok now we're ready to go...
			Thread->OnBeforeMain();
			Status = Thread->ReturnValue = Thread->Main();
			Thread->OnAfterMain();
		}

		// Shutdown...
		ThreadError:
		Thread->State = LGITHREAD_EXITED;
		if (Thread->DeleteOnExit)
		{
			DeleteObj(Thread);
		}
		_endthreadex(Status);
	}
#endif

	return 0;
}

void _Create(GThread *Thread, OsThread &hThread, uint &ThreadId)
{
#if defined(_MT) || defined(__MINGW32__)

	hThread = (HANDLE) _beginthreadex(NULL,
									16<<10,
									(unsigned int (__stdcall *)(void *)) ThreadEntryPoint,
									(LPVOID) Thread,
									CREATE_SUSPENDED,
									&ThreadId);

#elif defined(__CYGWIN__)

	// Cygwin doesn't support stable threading
	#ifdef _DEBUG
	LgiTrace("%s:%i - Cygwin doesn't have stable threading support.\n", __FILE__, __LINE__);
	#endif

#else

	#error "You are tryed to compile GThread support without the WIN32 multithreaded libs.";

#endif
}

GThread::GThread()
{
	State = LGITHREAD_INIT;
	hThread = 0;
	ThreadId = 0;
	ReturnValue = 0;
	DeleteOnExit = false;
	_Create(this, hThread, ThreadId);
}

GThread::~GThread()
{
	LgiAssert(State == LGITHREAD_INIT OR State == LGITHREAD_EXITED);
	if (hThread)
	{
		CloseHandle(hThread);
		hThread = 0;
	}
}

bool GThread::IsExited()
{
	bool Alive1 = ExitCode() == STILL_ACTIVE;
	
	#ifdef _DEBUG
	
	DWORD w = ::WaitForSingleObject(hThread, 0);
	// LgiTrace("%p Wait=%i Alive=%i\n", hThread, w, Alive1);

	bool Alive2 = WAIT_OBJECT_0 != w;
	if (!Alive2)
	{
		State = LGITHREAD_EXITED;
		return true;
	}

	#endif

	if (!Alive1)
	{
		State = LGITHREAD_EXITED;
		return true;
	}

	return false;
}

void GThread::Run()
{
	if (State == LGITHREAD_EXITED)
	{
		_Create(this, hThread, ThreadId);
		if (hThread != INVALID_HANDLE_VALUE)
		{
			State = LGITHREAD_INIT;
		}
	}

	if (State == LGITHREAD_INIT)
	{
		DWORD Status = ResumeThread(hThread);
		if (Status)
		{
			State = LGITHREAD_RUNNING;
		}
		else
		{
			State = LGITHREAD_ERROR;
		}
	}
}

void GThread::Terminate()
{
	if (hThread)
	{
		TerminateThread(hThread, 0);
		while (!IsExited());
	}
}

int GThread::ExitCode()
{
	DWORD d;
	GetExitCodeThread(hThread, &d);
	ReturnValue = d;
	return ReturnValue;
}

int GThread::Main()
{
	return 0;
}

