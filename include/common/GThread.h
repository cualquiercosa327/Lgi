#ifndef __GTHREAD_H
#define __GTHREAD_H

//////////////////////////////////////////////////////////////////////////
// Thread types are defined in GMutex.h
#include "GMutex.h"

class LgiClass GThread
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

protected:
	ThreadState State;
	int ReturnValue;
	OsThread hThread;
	const char *Name;

	#if defined WIN32

	friend uint WINAPI ThreadEntryPoint(void *i);
	uint ThreadId;
    void Create(class GThread *Thread, OsThread &hThread, uint &ThreadId);

	#elif defined POSIX

	friend void *ThreadEntryPoint(void *i);

	#else

	friend int32 ThreadEntryPoint(void *i);

	#endif

protected:
	bool DeleteOnExit;

public:
	GThread(const char *Name);
	virtual ~GThread();

	// Properties
	OsThread Handle() { return hThread; }
	#ifdef WIN32
	uint GetId() { return ThreadId; }
	#endif
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
class GThreadOwner;
class GThreadTarget;
class GThreadWorker;

/// A generic threaded job parent class.
class LgiClass GThreadJob
{
	friend class GThreadWorker;
	GThreadTarget *Owner;

public:
	GThreadJob(GThreadTarget *o) { Owner = o; }
	virtual ~GThreadJob() {}
	virtual void Do() {}
};

/// The thread target is a virtual API to receive work units executed 
/// in the worker thread.
class LgiClass GThreadTarget : public GMutex
{
	friend class GThreadWorker;

protected:
	/// The thread doing the work.
	GThreadWorker *Worker;

public:
	GThreadTarget();
	virtual ~GThreadTarget() {}
	
	virtual void SetWorker(GThreadWorker *w);
	virtual void Detach();
	
	/// This function gets called when the job is finished
	virtual void OnDone(GAutoPtr<GThreadJob> j) {}
};

#undef AddJob

/// This parent class does the actual work of processing jobs.
class LgiClass GThreadWorker : public GThread, public GMutex
{
	GArray<GThreadTarget*> Owners;
	GArray<GThreadJob*> Jobs;
	bool Loop;

public:
	GThreadWorker(GThreadTarget *First, const char *ThreadName);
	virtual ~GThreadWorker();
	
	void Stop();
	void Attach(GThreadTarget *o);
	void Detach(GThreadTarget *o);
	virtual void AddJob(GThreadJob *j);
	virtual void DoJob(GThreadJob *j);
	int Main();
};

class LgiClass GThreadOwner : public GThreadTarget
{
public:
	virtual ~GThreadOwner();
};

#endif
