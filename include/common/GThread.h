#ifndef __GTHREAD_H
#define __GTHREAD_H

//////////////////////////////////////////////////////////////////////////
// Thread types are defined in GSemaphore.h
#include "GSemaphore.h"

enum GThreadState
{
	LGITHREAD_INIT = 1,
	LGITHREAD_RUNNING = 2,
	LGITHREAD_ASLEEP = 3,
	LGITHREAD_EXITED = 4,
	LGITHREAD_ERROR = 5
};

class LgiClass GThread
{
	GThreadState State; // LGITHREAD_???
	int ReturnValue;
	OsThread hThread;

	#if defined WIN32
	friend uint WINAPI ThreadEntryPoint(int i);
	uint ThreadId;
	#elif defined POSIX
	friend void *ThreadEntryPoint(void *i);
	#else
	friend int32 ThreadEntryPoint(void *i);
	#endif

protected:
	bool DeleteOnExit;

public:
	GThread();
	virtual ~GThread();

	// Properties
	OsThread Handle() { return hThread; }
	#ifdef WIN32
	uint GetId() { return ThreadId; }
	#endif
	GThreadState GetState() { return State; } // Volitle as best... only use for 'debug'
	virtual int ExitCode();
	virtual bool IsExited();

	// Methods
	virtual void Run();
	virtual void Terminate();

	// Override to do something
	virtual int Main();

	// Events
	virtual void OnBeforeMain() {}
	virtual void OnAfterMain() {}
};

////////////////////////////////////////////////////////////////////////////////////////
class GThreadWork
{
public:
	virtual ~GThreadWork() {}
	virtual bool Do() = 0;
	virtual void OnComplete() {}
};

class LgiClass GThreadOwner
{
protected:
	class GThreadOwnerPriv *d;

public:
	GThreadOwner(GViewI *notify = 0);
	virtual ~GThreadOwner();

	void SetNotifyView(GViewI *notify);
	void AddWork(GThreadWork *w);
	virtual void OnComplete(GThreadWork *Result)
	{
		if (Result) Result->OnComplete();
	}
};

class LgiClass GWorkerThread : public GThread, public GSemaphore
{
public:
	GThreadOwnerPriv *d;

	GWorkerThread(GThreadOwnerPriv *data);
	int Main();
};

#endif
