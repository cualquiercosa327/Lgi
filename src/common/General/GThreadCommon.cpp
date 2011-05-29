#include "Lgi.h"

//////////////////////////////////////////////////////////////////////////////
/*
class GThreadOwnerPriv : public GSemaphore
{
public:
	GViewI *Notify;
	GThreadOwner *Owner;
	GWorkerThread *Thread;
	GArray<GThreadWork*> Work;

	GThreadOwnerPriv(GThreadOwner *o, GViewI *notify)
	{
		Owner = o;
		Thread = 0;
		Notify = notify;
	}

	~GThreadOwnerPriv()
	{
		GSemaphore::Auto Lck(this, _FL);
		if (Thread && Thread->Lock(_FL))
		{
			Thread->d = 0;
			Thread->Unlock();
		}
	}
};

GWorkerThread::GWorkerThread(GThreadOwnerPriv *data)
{
	d = data;
	DeleteOnExit = true;

	Run();
}

int GWorkerThread::Main()
{
	while (true)
	{
		GThreadWork *w = 0;

		if (Lock(_FL))
		{
			if (d && d->Lock(_FL))
			{
				w = d->Work.Length() > 0 ? d->Work[0] : 0;
				if (w)
					d->Work.DeleteAt(0, true);
				d->Unlock();
			}
			Unlock();
		}

		if (w)
		{
			w->Do();

			GSemaphore::Auto Lck(this, _FL);
			if (d && d->Lock(_FL))
			{
				if (d->Notify)
				{
					d->Notify->PostEvent(M_GTHREADWORK_COMPELTE, (GMessage::Param) d->Owner, (int) w);
				}
				else
				{
					d->Owner->OnComplete(w);
					DeleteObj(w);
				}

				d->Unlock();
			}
		}
		else break;
	}

	GSemaphore::Auto Lck(this, _FL);
	if (d && d->Lock(_FL))
	{
		d->Thread = 0;
		d->Unlock();
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
GThreadOwner::GThreadOwner(GViewI *notify)
{
	d = new GThreadOwnerPriv(this, notify);
}

GThreadOwner::~GThreadOwner()
{
	DeleteObj(d);
}

void GThreadOwner::AddWork(GThreadWork *w)
{
	if (!w)
		return;

	GSemaphore::Auto Lck(d, _FL);
	d->Work.Add(w);
	
	if (!d->Thread)
	{
		d->Thread = new GWorkerThread(d);
	}
}

*/