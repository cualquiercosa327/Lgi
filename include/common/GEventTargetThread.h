#ifndef _GEVENTTARGETTHREAD_H_
#define _GEVENTTARGETTHREAD_H_

#include "GThread.h"
#include "GMutex.h"
#include "GThreadEvent.h"

/// This class is a worker thread that accepts messages on it's GEventSinkI interface.
/// To use, sub class and implement the OnEvent handler.
class GEventTargetThread :
	public GThread,
	public GMutex,
	public GEventSinkI,
	public GEventTargetI // Sub-class has to implement OnEvent
{
	GArray<GMessage*> Msgs;
	GThreadEvent Event;
	bool Loop;

public:
	GEventTargetThread(GString Name) :
		GThread(Name + ".Thread"),
		GMutex(Name + ".Mutex"),
		Event(Name + ".Event")
	{
		Loop = true;
		Run();
	}
	
	~GEventTargetThread()
	{
		Loop = false;
		Event.Signal();
		while (!IsExited())
			LgiSleep(10);
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