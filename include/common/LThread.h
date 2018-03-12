#ifndef __GTHREAD_H
#define __GTHREAD_H

//////////////////////////////////////////////////////////////////////////
// Thread types are defined in LMutex.h
#include "LMutex.h"

class LgiClass LThread
{
public:
    enum ThreadState
    {
	    THREAD_INIT = 1,
	    THREAD_RUNNING = 2,
	    THREAD_ASLEEP = 3,
	    THREAD_EXITED = 4,
	    THREAD_ERROR = 5
    };
    
    enum ThreadPriority
    {
		ThreadPriorityIdle,
		ThreadPriorityNormal,
		ThreadPriorityHigh,
		ThreadPriorityRealtime,
    };

protected:
	ThreadState State;
	int ReturnValue;
	OsThread hThread;
	GString Name;
	ThreadPriority Priority;
	OsThreadId ThreadId;

	#if defined WIN32

	friend uint WINAPI ThreadEntryPoint(void *i);
    void Create(class LThread *Thread, OsThread &hThread, uint &ThreadId);

	#elif defined BEOS

	friend int32 ThreadEntryPoint(void *i);

	#elif defined POSIX

	friend void *ThreadEntryPoint(void *i);

	#else

	friend int32 ThreadEntryPoint(void *i);

	#endif

protected:
	bool DeleteOnExit;

public:
	LThread(const char *Name);
	virtual ~LThread();

	// Properties
	OsThread Handle() { return hThread; }
	const char *GetName() { return Name; }
	OsThreadId GetId() { return ThreadId; }
	ThreadState GetState() { return State; } // Volitile at best... only use for 'debug'
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
class LThreadOwner;
class LThreadTarget;
class LThreadWorker;

/// A generic threaded job parent class.
class LgiClass LThreadJob
{
	friend class LThreadWorker;
	LThreadTarget *Owner;

public:
	LThreadJob(LThreadTarget *o) { Owner = o; }
	virtual ~LThreadJob() {}
	virtual void Do() {}
};

/// The thread target is a virtual API to receive work units executed 
/// in the worker thread.
class LgiClass LThreadTarget : public LMutex
{
	friend class LThreadWorker;

protected:
	/// The thread doing the work.
	LThreadWorker *Worker;

public:
	LThreadTarget();
	virtual ~LThreadTarget() {}
	
	virtual void SetWorker(LThreadWorker *w);
	virtual void Detach();
	
	/// This function gets called when the job is finished
	virtual void OnDone(GAutoPtr<LThreadJob> j) {}
};

#undef AddJob

/// This parent class does the actual work of processing jobs.
class LgiClass LThreadWorker : public LThread, public LMutex
{
	GArray<LThreadTarget*> Owners;
	GArray<LThreadJob*> Jobs;
	bool Loop;

public:
	LThreadWorker(LThreadTarget *First, const char *ThreadName);
	virtual ~LThreadWorker();
	
	void Stop();
	void Attach(LThreadTarget *o);
	void Detach(LThreadTarget *o);
	virtual void AddJob(LThreadJob *j);
	virtual void DoJob(LThreadJob *j);
	int Main();
};

class LgiClass LThreadOwner : public LThreadTarget
{
public:
	virtual ~LThreadOwner();
};

#endif
