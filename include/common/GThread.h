#ifndef __GTHREAD_H
#define __GTHREAD_H

//////////////////////////////////////////////////////////////////////////
// Thread types are defined in GSemaphore.h
#include "GSemaphore.h"

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
class GThreadJob
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
class GThreadTarget : public GSemaphore
{
	friend class GThreadWorker;

protected:
	/// The thread doing the work.
	GThreadWorker *Worker;

public:
	GThreadTarget()
	{
		Worker = 0;
	}

	virtual ~GThreadTarget() {}
	
	virtual void SetWorker(GThreadWorker *w)
	{
		if (w && Lock(_FL))
		{
			Worker = w;
			Unlock();
		}
	}
	
	virtual void Detach()
	{
		if (Lock(_FL))
		{
			Worker = 0;
			Unlock();
		}
	}
	
	/// This function gets called when the job is finished
	virtual void OnDone(GAutoPtr<GThreadJob> j) {}
};

/// This parent class does the actual work of processing jobs.
class GThreadWorker : public GThread, public GSemaphore
{
	GArray<GThreadTarget*> Owners;
	GArray<GThreadJob*> Jobs;
	bool Loop;

public:
	GThreadWorker(GThreadTarget *First, const char *ThreadName) :
		GThread(ThreadName),
		GSemaphore("GThreadWorker")
	{
		Loop = false;
		if (First)
			Attach(First);
	}

	virtual ~GThreadWorker()
	{
		Loop = false;
		while (!IsExited())
			LgiSleep(1);
		if (Lock(_FL))
		{
			for (uint32 i=0; i<Owners.Length(); i++)
				Owners[i]->Detach();
			Unlock();
		}
	}

	void Attach(GThreadTarget *o)
	{
		GSemaphore::Auto a(this, _FL);
		if (!Owners.HasItem(o))
		{
			LgiAssert(o->Worker == this);
			Owners.Add(o);
			if (Owners.Length() == 1)
			{
				Loop = true;
				Run();
			}
		}
	}

	void Detach(GThreadTarget *o)
	{
		GSemaphore::Auto a(this, _FL);
		LgiAssert(Owners.HasItem(o));
		Owners.Delete(o);
	}

	virtual void AddJob(GThreadJob *j)
	{
		if (Lock(_FL))
		{
			Jobs.Add(j);

			if (!Owners.HasItem(j->Owner))
				Attach(j->Owner);

			Unlock();
		}
	}

	virtual void DoJob(GThreadJob *j)
	{
		j->Do();
	}

	int Main()
	{
		while (Loop)
		{
			GAutoPtr<GThreadJob> j;
			if (Lock(_FL))
			{
				if (Jobs.Length())
				{
					j.Reset(Jobs[0]);
					Jobs.DeleteAt(0, true);
				}
				Unlock();
			}
			if (j)
			{
				DoJob(j);
				if (Lock(_FL))
				{
					if (Owners.IndexOf(j->Owner) < 0)
					{
						// Owner is gone already... delete the job.
						j.Reset();
					}
					Unlock();
				}
				if (j)
				{
					// Pass job back to owner.
					j->Owner->OnDone(j);
				}
			}
			else LgiSleep(50);
		}
		return 0;
	}
};

class GThreadOwner : public GThreadTarget
{
public:
	GThreadOwner()
	{
	}

	virtual ~GThreadOwner()
	{
		if (Lock(_FL))
		{
			if (Worker)
				Worker->Detach(this);
			Unlock();
		}
	}
};

#endif
