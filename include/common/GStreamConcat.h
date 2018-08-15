#ifndef _STREAM_CONCAT_H_
#define _STREAM_CONCAT_H_

#include "LgiInterfaces.h"

/// This class concatenates several streams into one large logical stream
/// Note: currently only reading is implemented...
class GStreamConcat : public GStreamI
{
	struct StreamBlock
	{
		int64 Start;
		int64 End;
		bool Own;
		GStreamI *Stream;
	};

	unsigned Cur;
	int64 Sz, Pos;
	GArray<StreamBlock> a;

public:
	GStreamConcat()
	{
		Cur = 0;
		Sz = Pos = 0;
	}

	GStreamConcat(const GStreamConcat &c)
	{
		Cur = c.Cur;
		Sz = c.Sz;
		Pos = c.Pos;
		a = c.a;
	}
	
	~GStreamConcat()
	{
		for (unsigned i=0; i<a.Length(); i++)
		{
			StreamBlock &sb = a[i];
			if (sb.Own)
				delete sb.Stream;
		}
	}
	
	bool Add(GStreamI *s, bool Own = true)
	{
		if (!s)
		{
			LgiAssert(0);
			return false;
		}
		
		int64 Size = s->GetSize();
		if (Size < 0)
		{
			LgiAssert(0);
			return false;
		}
		
		StreamBlock &sb = a.New();
		sb.Start = Sz;
		sb.End = Sz + Size;
		sb.Stream = s;
		sb.Own = Own;
		Sz += Size;
		return true;
	}
	
	bool IsOpen()
	{
		return true;
	}
	
	int Close()
	{
		Sz = Pos = 0;
		a.Length(0);
		Cur = 0;
		return true;
	}
	
	int64 GetSize()
	{
		return Sz;
	}
	
	int64 SetSize(int64 Size)
	{
		return Sz;
	}
	
	int64 GetPos()
	{
		return Pos;
	}
	
	int64 SetPos(int64 p)
	{
		if (p < 0)
			p = 0;
		if (p > Sz)
			p = Sz;
		
		Pos = p;
		
		for (unsigned i=0; i<a.Length(); i++)
		{
			StreamBlock &sb = a[i];
			if (Pos >= sb.Start && Pos < sb.End)
			{
				Cur = i;
				break;
			}
		}
		
		return Pos;
	}
	
	ssize_t Read(void *Buffer, ssize_t Size, int Flags = 0)
	{
		if (Cur >= a.Length() || Size <= 0 || !Buffer)
			return 0;
		
		StreamBlock &sb = a[Cur];
		ssize_t Rd = 0;
		if (Pos >= sb.Start && Pos < sb.End)
		{
			int64 Remaining = sb.End - Pos;
			ssize_t Common = MIN(Size, (int)Remaining);
			if (Common > 0)
			{
				int64 StrOffset = Pos - sb.Start;
				int64 StrPos = sb.Stream->SetPos(StrOffset);
				if (StrPos == StrOffset)
				{
					Rd = sb.Stream->Read(Buffer, Common, Flags);
					if (Rd > 0)
					{
						Pos += Rd;
						if (Pos >= sb.End)
							Cur++;
					}
				}
				else LgiAssert(0);
			}
			else LgiAssert(0);
		}
		else LgiAssert(0);
		
		return Rd;
	}
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0)
	{
		int Wr = 0;
		LgiAssert(0);
		return Wr;
	}
	
	GStreamI *Clone()
	{
		return new GStreamConcat(*this);
	}
};

#endif