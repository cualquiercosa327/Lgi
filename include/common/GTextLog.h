/// \file
/// \author Matthew Allen
#ifndef _GTEXTLOG_H_
#define _GTEXTLOG_H_

#include "GTextView3.h"

#define M_LOG			(M_USER + 0x3000)

class GTextLog : public GTextView3, public GStream
{
protected:
    bool RemoveReturns;
    LMutex Sem;
    GArray<char16*> Txt;

	void ProcessTxt()
	{
	    if (Sem.Lock(_FL))
	    {
	        for (uint32 i=0; i<Txt.Length(); i++)
	        {
			    Add(Txt[i]);
			}
			Txt.Length(0);
			Sem.Unlock();
		}
	}

public:
	GTextLog(int id) : GTextView3(id, 0, 0, 2000, 1000)
	{
	    RemoveReturns = true;
		Sunken(true);
		SetPourLargest(true);
	}
	
	~GTextLog()
	{
	    if (Sem.Lock(_FL))
	    {
	        Txt.DeleteArrays();
	        Sem.Unlock();
	    }
	}
	
	void OnCreate()
	{
		ProcessTxt();
	}

	virtual void Add(char16 *w, ssize_t chars = -1)
	{
		int Len = chars >= 0 ? chars : StrlenW(w);
		
		if (RemoveReturns)
		{
			char16 *end = w + Len;
			for (char16 *s = w; *s; )
			{
				char16 *e = s;
				while (e < end && *e != '\r')
					e++;
				if (e > s)
					Insert(Size, s, e - s);
				if (e >= end)
					break;
				s = e + 1;
			}
		}
		else
		{
			Insert(Size, w, Len);
		}
		
		Invalidate();
		SetCaret(Size, false);
	}

	int64 SetSize(int64 s)
	{
		Name(0);
		return 0;
	}
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0)
	{
		GAutoWString w(Utf8ToWide((char*)Buffer, Size));
		// printf("GTextLog::Write(%p)\n", w.Get());
		if (w)
		{
			if (Sem.LockWithTimeout(200, _FL))
			{
				Txt.Add(w.Release());
			    Sem.Unlock();
			}
			if (Handle())
				PostEvent(M_LOG);
		}
		return Size;
	}

	GMessage::Result OnEvent(GMessage *m)
	{
		if (MsgCode(m) == M_LOG)
		{
			ProcessTxt();
		}

		return GTextView3::OnEvent(m);
	}
};

#endif
