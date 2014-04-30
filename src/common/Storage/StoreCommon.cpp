#include <stdio.h>

#include "Lgi.h"
#include "StoreCommon.h"

/// \brief This class can limit the reading/writing to a specific sub section of a file
/// Which should protect the application using the storage system from overwriting parts
/// of the file is shouldn't have access to. The default region is the whole file.
GSubFilePtr::GSubFilePtr(GSubFile *Parent, const char *file, int line)
{
	File = Parent;
	SrcFile = NewStr(file);
	SrcLine = line;
	OurStatus = true;
	Pos = 0;
	SetSwap(Parent->GetSwap());
	
	ClearSub();
}

GSubFilePtr::~GSubFilePtr()
{
	File->Detach(this);
	DeleteArray(SrcFile);
}

bool GSubFilePtr::SaveState()
{
	ActualPos = File->GetPos();
	ActualStatus = File->GetStatus();
	ActualSwap = File->GetSwap();
	
	File->SetSwap(GetSwap());
	File->SetStatus(OurStatus);
	
	int64 p = File->SetPos(Start + Pos);
	if (p == Start + Pos)
	{
		return true;
	}
	else
	{
		printf("%s:%i SaveState failed Pos="LGI_PrintfInt64" OurPos="LGI_PrintfInt64"\n", _FL, Pos, Pos);
	}
	return false;
}

bool GSubFilePtr::RestoreState()
{
	SetSwap(File->GetSwap());
	OurStatus = File->GetStatus();
	Pos = File->GetPos() - Start;

	File->SetStatus(ActualStatus);
	File->SetSwap(ActualSwap);
	return File->SetPos(ActualPos) == ActualPos;
}

bool GSubFilePtr::GetSub(int64 &start, int64 &len)
{
	start = Start;
	len = Len;
	return Sub;
}

bool GSubFilePtr::SetSub(int64 start, int64 len)
{
	Sub = true;
	Start = start;
	Len = len;
	Pos = 0;

	return true;
}

void GSubFilePtr::ClearSub()
{
	Sub = false;
	Start = Len = 0;
}

int GSubFilePtr::Open(char *Str, int Int)
{
	LgiAssert(0);
	return 0;
}

bool GSubFilePtr::IsOpen()
{
	return true;
}

int GSubFilePtr::Close()
{
	LgiAssert(0);
	return 0;
}

int64 GSubFilePtr::GetSize()
{
	int64 s = -1;
	
	if (Sub)
	{
		s = Len;
	}
	else
	{
		GSubFile::SubLock Lock = File->Lock(_FL);
		s = File->GetSize();
	}
	
	return s;
}

int64 GSubFilePtr::SetSize(int64 Size)
{
	LgiAssert(0);
	return -1;
}

int64 GSubFilePtr::GetPos()
{
	return Pos;
}

int64 GSubFilePtr::SetPos(int64 pos)
{
	if (pos < 0)
	{
		printf("%s:%i - Pos < 0???\n", __FILE__, __LINE__);
	}
	
	return Pos = pos;
}

int64 GSubFilePtr::Seek(int64 To, int Whence)
{
	switch (Whence)
	{
		case SEEK_SET:
			SetPos(To);
			break;
		case SEEK_CUR:
			SetPos(GetPos() + To);
			break;
		case SEEK_END:
			SetPos(GetSize() + To);
			break;
	}

	return GetPos();
}

GStreamI *GSubFilePtr::Clone()
{
	LgiAssert(0);
	return 0;
}

bool GSubFilePtr::Eof()
{
	bool Status = false;
	
	if (Sub)
	{
		Status = Pos < 0 || Pos >= Len;
	}
	else
	{
		GSubFile::SubLock Lock = File->Lock(_FL);
		Status = File->Eof();
	}
	
	return Status;		
}

bool GSubFilePtr::GetStatus()
{
	return OurStatus;
}

void GSubFilePtr::SetStatus(bool s)
{
	OurStatus = s;
}

int GSubFilePtr::Read(void *Buffer, int Size, int Flags)
{
	int Status = 0;
	
	GSubFile::SubLock Lock = File->Lock(_FL);
	if (!Sub || (Pos >= 0 && Pos <= Len))
	{
		if (SaveState())
		{
			int64 Remaining = Len - Pos;
			uint32 RdSize = (int) (Sub ? min(Remaining, Size) : Size);
			Status = File->Read(Buffer, RdSize, Flags);
			RestoreState();
		}
	}
	else
	{
		LgiAssert(0);
	}
	
	return Status;
}

int GSubFilePtr::Write(const void *Buffer, int Size, int Flags)
{
	int Status = 0;
	
	GSubFile::SubLock Lock = File->Lock(_FL);

	int64 End = Pos + Size;
	if (!Sub ||
		(Pos >= 0 && End <= Len))
	{
		if (SaveState())
		{
			Status = File->Write(Buffer, Size, Flags);
			RestoreState();
		}
	}
	else
	{
		LgiTrace("GSubFilePtr error, Pos="LGI_PrintfInt64" Start="LGI_PrintfInt64" End="LGI_PrintfInt64" Len="LGI_PrintfInt64"\n",
			Pos, Start, End, Len);
	}
	
	return Status;
}

int GSubFilePtr::ReadStr(char *Buf, int Size)
{
	LgiAssert(0);
	return 0;
}

int GSubFilePtr::WriteStr(char *Buf, int Size)
{
	LgiAssert(0);
	return 0;
}

///////////////////////////////////////////////////////////////////////
GSubFile::GSubFile(GMutex *lock, bool Buffering)
{
	Lck = lock;

	#if GSUBFILE_NOBUFFERING
	Buffer = Buffering;
	Block = 0;
	Buf = 0;
	Shift = 0;
	Cur = -1;
	Pos = -1;
	Dirty = false;
	#endif
}

GSubFile::~GSubFile()
{
	while (Ptrs.Length())
	{
		GSubFilePtr *p = Ptrs[0];
		if (p)
		{
			printf("%s:%i - GSubFilePtr not released.\n", p->SrcFile, p->SrcLine);
			DeleteObj(p);
		}
		else break;
	}

	#if GSUBFILE_NOBUFFERING
	DeleteArray(Buf);
	#endif
}

GSubFilePtr *GSubFile::Create(const char *file, int line)
{
	GSubFilePtr *p = 0;
	
	p = new GSubFilePtr(this, file, line);
	if (p)
	{
		SubLock Lck = Lock(_FL);
		Ptrs[Ptrs.Length()] = p;
	}
	
	return p;
}

void GSubFile::Detach(GSubFilePtr *Ptr)
{
	SubLock Lck = Lock(_FL);
	LgiAssert(Ptrs.HasItem(Ptr));
	Ptrs.Delete(Ptr);
}

GSubFile::SubLock GSubFile::Lock(const char *file, int line)
{
	return SubLock(new GMutex::Auto(Lck, file, line));
}

#if GSUBFILE_NOBUFFERING
int GSubFile::Open(char *Str, int Int)
{
	int s = GFile::Open(Str, Int | (!Buffer ? O_NO_CACHE : 0));
	if (s && !Buffer)
	{
		Block = GetBlockSize();
		Shift = 1;
		while ((1 << Shift) < Block)
			Shift++;
		Buf = new uint8[Block];
		Pos = 0;
	}
	return s;
}

int GSubFile::Read(void *OutBuf, int Size, int Flags)
{
	int Status = 0;

	if (!Buffer && Buf)
	{
		uint8 *Out = (uint8*)OutBuf;
		int64 Blk = Pos >> Shift;
		int Mask = Block - 1;

		while (Size)
		{
			if (Blk != Cur)
			{
				SetBlock(Blk);
			}

			int Off = Pos & Mask;
			int Len = Block - Off;
			if (Len > Size)
				Len = Size;

			memcpy(Out, Buf + Off, Len);

			Pos += Len;
			Out += Len;
			Size -= Len;
			Status += Len;
			Blk++;
		}
	}
	else
	{
		Status = GFile::Read(OutBuf, Size, Flags);
		if (Status > 0)
		{
			Pos += Status;
		}
	}

	return Status;
}

int GSubFile::Write(const void *InBuf, int Size, int Flags)
{
	int Status = 0;

	if (!Buffer && Buf)
	{
		uint8 *In = (uint8*)InBuf;
		int64 Blk = Pos >> Shift;
		int Mask = Block - 1;

		while (Size)
		{
			if (Blk != Cur)
			{
				SetBlock(Blk);
			}

			int Off = Pos & Mask;
			int Len = Block - Off;
			if (Len > Size)
				Len = Size;

			memcpy(Buf + Off, In, Len);
			Dirty = true;

			Pos += Len;
			In += Len;
			Size -= Len;
			Status += Len;
			Blk++;
		}
	}
	else
	{
		Status = GFile::Write(InBuf, Size, Flags);
		if (Status > 0)
		{
			Pos += Status;
		}
	}

	return Status;
}

int64 GSubFile::GetPos()
{
	return Pos;
}

int64 GSubFile::SetPos(int64 pos)
{
	return Pos = pos;
}

bool GSubFile::SetBlock(int64 Blk)
{
	if (Blk != Cur)
	{
		int64 p, n;

		if (Dirty)
		{
			p = GFile::SetPos(Cur << Shift);
			n = GFile::Write(Buf, Block);
			if (n != Block)
				return false;
			
			Dirty = false;
		}

		Cur = Blk;

		p = GFile::SetPos(Cur << Shift);
		n = GFile::Read(Buf, Block);

		return n > 0;
	}

	return true;
}

#endif
