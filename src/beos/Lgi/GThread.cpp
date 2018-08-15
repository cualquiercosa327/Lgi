#include "Lgi.h"

////////////////////////////////////////////////////////////////////////////
int32 ThreadEntryPoint(void *i)
{
	if (i)
	{
		LThread *Thread = (LThread*) i;
		Thread->OnBeforeMain();
		Thread->ReturnValue = Thread->Main();
		Thread->OnAfterMain();
		Thread->State = LThread::THREAD_EXITED;
		return Thread->ReturnValue;
	}
	return 0;
}

LThread::LThread(const char *Name)
{
	State = THREAD_ASLEEP;
	ReturnValue = -1;
	hThread = spawn_thread(	ThreadEntryPoint,
							"Lgi.LThread", 
							B_NORMAL_PRIORITY, 
							this);
}

LThread::~LThread()
{
	if (!IsExited())
	{
		Terminate();
	}
}

int LThread::ExitCode()
{
	return ReturnValue;
}

bool LThread::IsExited()
{
	thread_info info;
	if (get_thread_info(hThread, &info) == B_OK)
	{
		return false;
	}

	return State == THREAD_EXITED;
}

void LThread::Run()
{
	resume_thread(hThread);
	State = THREAD_RUNNING;
}

void LThread::Terminate()
{
	if (kill_thread(hThread) == B_OK)
	{
		State = THREAD_EXITED;
	}
}

int LThread::Main()
{
	return 0;
}

