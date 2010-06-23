#include "Lgi.h"

////////////////////////////////////////////////////////////////////////////
int32 ThreadEntryPoint(void *i)
{
	if (i)
	{
		GThread *Thread = (GThread*) i;
		Thread->OnBeforeMain();
		Thread->ReturnValue = Thread->Main();
		Thread->OnAfterMain();
		Thread->State = LGITHREAD_EXITED;
		return Thread->ReturnValue;
	}
	return 0;
}

GThread::GThread()
{
	State = LGITHREAD_ASLEEP;
	ReturnValue = -1;
	hThread = spawn_thread(	ThreadEntryPoint,
							"Lgi.GThread", 
							B_NORMAL_PRIORITY, 
							this);
}

GThread::~GThread()
{
	if (NOT IsExited())
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
	thread_info info;
	if (get_thread_info(hThread, &info) == B_OK)
	{
		return false;
	}

	return State == LGITHREAD_EXITED;
}

void GThread::Run()
{
	resume_thread(hThread);
	State = LGITHREAD_RUNNING;
}

void GThread::Terminate()
{
	if (kill_thread(hThread) == B_OK)
	{
		State = LGITHREAD_EXITED;
	}
}

int GThread::Main()
{
	return 0;
}

