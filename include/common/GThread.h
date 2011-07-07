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
	friend uint WINAPI ThreadEntryPoint(void *i);
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
class GThreadOwner;
class GThreadTarget;
class GThreadWorker;

class GThreadJob
{
	friend class GThreadWorker;
	GThreadTarget *Owner;

public:
	GThreadJob(GThreadTarget *o) { Owner = o; }
	virtual ~GThreadJob() {}
	virtual void Do() {}
};

class GThreadTarget : public GSemaphore
{
	friend class GThreadWorker;

protected:
	GThreadWorker *Worker;

public:
	GThreadTarget()
	{
		Worker = 0;
	}

	virtual ~GThreadTarget() {}
	
	virtual void Detach()
	{
		if (Lock(_FL))
		{
			Worker = 0;
			Unlock();
		}
	}
	
	virtual void OnDone(GThreadJob *j) {}
};

class GThreadWorker : public GThread, public GSemaphore
{
	GArray<GThreadTarget*> Owners;
	GArray<GThreadJob*> Jobs;
	bool Loop;

public:
	GThreadWorker(GThreadTarget *First) : GSemaphore("GThreadWorker")
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
			GThreadJob *j = 0;
			if (Lock(_FL))
			{
				if (Jobs.Length())
				{
					j = Jobs[0];
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
						DeleteObj(j);
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

/*
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
*/

#endif
