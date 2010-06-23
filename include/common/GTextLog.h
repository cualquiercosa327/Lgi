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
    GSemaphore Sem;
    GArray<char16*> Txt;

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

	void Add(char16 *w)
	{
		int Len;
		
		if (RemoveReturns)
		{
		    char16 *i = w, *o = w;
		    while (*i)
		    {
		        if (*i != '\r')
		        {
		            *o++ = *i++;
		        }
		        else
		        {
		            i++;
		        }
		    }
		    *o = 0;
		    Len = o - w;
		}
		else
		{
		    Len = StrlenW(w);
		}
		
		Insert(Size, w, Len);
		DeleteArray(w);
		Invalidate();
		SetCursor(Size, false);
	}

	int64 SetSize(int64 s)
	{
		Name(0);
		return 0;
	}
	
	int Write(void *Buffer, int Size, int Flags = 0)
	{
		char16 *w = LgiNewUtf8To16((char*)Buffer, Size);
		if (w)
		{
			if (InThread())
			{
				Add(w);
			}
			else if (Sem.Lock(_FL))
			{
			    Txt.Add(w);
			    Sem.Unlock();
				PostEvent(M_LOG);
			}
		}
		return Size;
	}

	int OnEvent(GMessage *m)
	{
		if (MsgCode(m) == M_LOG)
		{
		    if (Sem.Lock(_FL))
		    {
		        for (int i=0; i<Txt.Length(); i++)
		        {
				    Add(Txt[i]);
				}
				Txt.Length(0);
				Sem.Unlock();
			}
		}

		return GTextView3::OnEvent(m);
	}
};

#endif
