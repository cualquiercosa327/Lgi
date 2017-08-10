#include "Lgi.h"
#include "GEventTargetThread.h"

GEventSinkMap GEventSinkMap::Dispatch(128);

//////////////////////////////////////////////////////////////////////////////////
GThreadTarget::GThreadTarget()
{
	Worker = 0;
}

void GThreadTarget::SetWorker(GThreadWorker *w)
{
	if (w && Lock(_FL))
	{
		Worker = w;
		Unlock();
	}
}

void GThreadTarget::Detach()
{
	if (Lock(_FL))
	{
		Worker = 0;
		Unlock();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GThreadWorker::GThreadWorker(GThreadTarget *First, const char *ThreadName) :
	LThread(ThreadName),
	LMutex("GThreadWorker")
{
	Loop = false;
	if (First)
		Attach(First);
}

GThreadWorker::~GThreadWorker()
{
	Stop();
}

void GThreadWorker::Stop()
{
	if (Loop)
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
}

void GThreadWorker::Attach(GThreadTarget *o)
{
	LMutex::Auto a(this, _FL);
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

void GThreadWorker::Detach(GThreadTarget *o)
{
	LMutex::Auto a(this, _FL);
	LgiAssert(Owners.HasItem(o));
	Owners.Delete(o);
}

void GThreadWorker::AddJob(GThreadJob *j)
{
	if (Lock(_FL))
	{
		Jobs.Add(j);

		if (!Owners.HasItem(j->Owner))
			Attach(j->Owner);

		Unlock();
	}
}

void GThreadWorker::DoJob(GThreadJob *j)
{
	j->Do();
}

int GThreadWorker::Main()
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
				else
				{
					// Pass job back to owner.
					// This needs to be in the lock so that the owner can't delete itself
					// from the owner list...
					j->Owner->OnDone(j);
				}
				Unlock();
			}
		}
		else LgiSleep(50);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
GThreadOwner::~GThreadOwner()
{
	if (Lock(_FL))
	{
		if (Worker)
			Worker->Detach(this);
		Unlock();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
/*
GEventSinkPtr::GEventSinkPtr(GEventTargetThread *p, bool own)
{
	Ptr = p;
	OwnPtr = own;
	if (p && p->Lock(_FL))
	{
		p->Ptrs.Add(this);
		p->Unlock();
	}
}

GEventSinkPtr::~GEventSinkPtr()
{
	if (Lock(_FL))
	{
		GEventTargetThread *tt = dynamic_cast<GEventTargetThread*>(Ptr);
		if (tt)
		{
			if (tt->Lock(_FL))
			{
				if (!tt->Ptrs.Delete(this))
					LgiAssert(0);
				tt->Unlock();
			}
		}

		if (OwnPtr)
			delete Ptr;
		Ptr = NULL;
	}
}
*/
