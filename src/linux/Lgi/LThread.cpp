#include "Lgi.h"
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

OsThreadId GetCurrentThreadId()
{
	#ifdef SYS_gettid
	return syscall(SYS_gettid);
	#else
	LgiAssert(0);
	return 0;
	#endif
}

////////////////////////////////////////////////////////////////////////////
void *ThreadEntryPoint(void *i)
{
	if (i)
	{
		GThread *Thread = (GThread*) i;
		Thread->ThreadId = GetCurrentThreadId();

		// Make sure we have finished executing the setup
		while (Thread->State == GThread::THREAD_INIT)
		{
			LgiSleep(1);
		}
		
		pthread_detach(Thread->hThread);
		
		GString Nm = Thread->Name;
		if (Nm)
			pthread_setname_np(pthread_self(), Nm);

		// Do thread's work
		Thread->OnBeforeMain();
		Thread->ReturnValue = Thread->Main();
		Thread->OnAfterMain();

		// mark thread over...
		Thread->State = GThread::THREAD_EXITED;

		if (Thread->DeleteOnExit)
		{
			DeleteObj(Thread);
		}

		pthread_exit(0);
	}
	return 0;
}

GThread::GThread(const char *ThreadName)
{
	Name = ThreadName;
	ThreadId = 0;
	State = GThread::THREAD_ASLEEP;
	ReturnValue = -1;
	hThread = 0;
	DeleteOnExit = false;
}

GThread::~GThread()
{
	if (!IsExited())
	{
		Terminate();
	}
}

int GThread::ExitCode()
{
	return ReturnValue;
}

bool GThread::IsExited()
{
	return State == GThread::THREAD_EXITED;
}

void GThread::Run()
{
	if (!hThread)
	{
		State = GThread::THREAD_INIT;

		static int Creates = 0;
		int e;
		if (!(e = pthread_create(&hThread, NULL, ThreadEntryPoint, (void*)this)))
		{
			Creates++;
			State = GThread::THREAD_RUNNING;
		}
		else
		{
			const char *Err = "(unknown)";
			switch (e)
			{
				case EAGAIN: Err = "EAGAIN"; break;
				case EINVAL: Err = "EINVAL"; break;
				case EPERM: Err = "EPERM"; break;
				case ENOMEM: Err = "ENOMEM"; break;
			}
			printf("%s,%i - pthread_create failed with the error %i (%s) (After %i creates)\n", __FILE__, __LINE__, e, Err, Creates);
			
			State = GThread::THREAD_EXITED;
		}
	}
}

void GThread::Terminate()
{
	if (hThread &&
		pthread_cancel(hThread) == 0)
	{
		State = GThread::THREAD_EXITED;
	}
}

int GThread::Main()
{
	return 0;
}

