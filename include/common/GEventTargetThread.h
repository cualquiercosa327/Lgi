#ifndef _GEVENTTARGETTHREAD_H_
#define _GEVENTTARGETTHREAD_H_

#include "GThread.h"
#include "GMutex.h"
#include "GThreadEvent.h"

class GEventTargetThread;

/// This class is a smart pointer to a GEventTargetThread. When the
/// GEventSinkPtr::PostEvent is safe to call even if the GEventTargetThread
/// object could be deleted asynchronously.
class LgiClass GEventSinkPtr : public GEventSinkI, public GMutex
{
	friend class GEventTargetThread;
	bool OwnPtr;
	GEventSinkI *Ptr;

	bool OnDelete(GEventSinkI *Sink)
	{
		if (!Lock(_FL))
			return false;
		bool Status = false;
		if (Sink == Ptr)
		{
			Status = true;
			Ptr = NULL;
		}
		else LgiAssert(0);
		Unlock();
		
		return Status;
	}

public:	
	GEventSinkPtr(GEventSinkI *p, bool own)
	{
		Ptr = p;
		OwnPtr = own;
	}
	GEventSinkPtr(GEventTargetThread *p, bool own);
	~GEventSinkPtr();

	bool PostEvent(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0)
	{
		bool Status = false;
		if (Lock(_FL))
		{
			if (Ptr)
				Status = Ptr->PostEvent(Cmd, a, b);
			Unlock();
		}
		return Status;
	}
};

/// This class is a worker thread that accepts messages on it's GEventSinkI interface.
/// To use, sub class and implement the OnEvent handler.
class LgiClass GEventTargetThread :
	public GThread,
	public GMutex,
	public GEventSinkI,
	public GEventTargetI // Sub-class has to implement OnEvent
{
	friend class GEventSinkPtr;

	GArray<GMessage*> Msgs;
	GThreadEvent Event;
	bool Loop;
	GArray<GEventSinkPtr*> Ptrs;

	// This makes the event name unique on windows to 
	// prevent multiple instances clashing.
	GString ProcessName(GString obj, const char *desc)
	{
		OsProcessId process = LgiGetCurrentProcess();
		OsThreadId thread = LgiGetCurrentThread();
		GString s;
		s.Printf("%s.%s.%i.%i", obj.Get(), desc, process, thread);
		return s;
	}

public:
	GEventTargetThread(GString Name) :
		GThread(Name + ".Thread"),
		GMutex(Name + ".Mutex"),
		Event(ProcessName(Name, "Event"))
	{
		Loop = true;

		Run();
	}
	
	virtual ~GEventTargetThread()
	{
		if (Lock(_FL))
		{
			for (unsigned i=0; i<Ptrs.Length(); i++)
			{
				Ptrs[i]->OnDelete(this);
			}
			Ptrs.Length(0);
			Unlock();
		}

		EndThread();
	}

	void EndThread()
	{
		if (Loop)
		{
			// We can't be locked here, because GEventTargetThread::Main needs
			// to lock to check for messages...
			Loop = false;
			Event.Signal();
			while (!IsExited())
				LgiSleep(10);
		}
	}

	uint32 GetQueueSize()
	{
		return Msgs.Length();
	}

	bool PostEvent(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0)
	{
		if (!Loop)
			return false;
		if (!Lock(_FL))
			return false;
		
		Msgs.Add(new GMessage(Cmd, a, b));
		Unlock();
		
		return Event.Signal();
	}
	
	int Main()
	{
		while (Loop)
		{
			GThreadEvent::WaitStatus s = Event.Wait();
			if (s == GThreadEvent::WaitSignaled)
			{
				while (true)
				{
					GAutoPtr<GMessage> Msg;
					if (Lock(_FL))
					{
						if (Msgs.Length())
						{
							Msg.Reset(Msgs.First());
							Msgs.DeleteAt(0, true);
						}
						Unlock();
					}

					if (!Loop || !Msg)
						break;

					OnEvent(Msg);
				}
			}
			else
			{
				LgiTrace("%s:%i - Event.Wait failed.\n", _FL);
				break;
			}
		}
		
		return 0;
	}
};



#endif