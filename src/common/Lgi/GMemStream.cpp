#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#include "Lgi.h"
#include "GStream.h"

//////////////////////////////////////////////////////////////////////////////
void GMemStream::_Init()
{
	GrowBlockSize = 0;
	Len = Pos = Alloc = 0;
	Mem = 0;
	Own = true;
}

GMemStream::GMemStream()
{
	_Init();
}

GMemStream::GMemStream(GStreamI *Src, int64 Start, int64 len)
{
	_Init();

	if (Src)
	{
		bool Error = false;
		if (Start >= 0)
		{
			Error = Src->SetPos(Start) != Start;
		}
		if (!Error)
		{
			if (Len < 0 && Src->GetSize() > 0)
			{
				Len = Src->GetSize();
				if (Start > 0)
					Len -= Start;
			}

			if (Len >= 0)
			{
				if (Mem = new char[Alloc = Len])
				{
					int r;
					while (	Pos < Len AND
							(r = Src->Read(Mem + Pos, Len - Pos)) > 0)
					{
						Pos += r;
					}
					Pos = 0;
				}
			}
			else
			{
				char Buf[512];
				GBytePipe p(4 << 10);
				int r;
				while ((r = Src->Read(Buf, sizeof(Buf))) > 0)
				{
					p.Write(Buf, r);
				}

				Len = Alloc = p.GetSize();
				Mem = (char*)p.New(0);
			}
		}
	}
}

GMemStream::GMemStream(void *mem, int64 len, bool copy)
{
	_Init();

	Len = len;
	if (Own = copy)
	{
		if (Mem = new char[Alloc = Len])
			memcpy(Mem, mem, Len);
	}
	else
	{
		Mem = (char*)mem;
	}
}

GMemStream::GMemStream(int growBlockSize)
{
	_Init();
	GrowBlockSize = growBlockSize;
}

GMemStream::~GMemStream()
{
	Close();
}

int GMemStream::Open(char *Str, int Int)
{
	GFile f;
	if (f.Open(Str, O_READ))
	{
		Close();
		Len = f.GetSize();
		Mem = new char[Alloc = Len];
		if (Mem)
		{
			Len = f.Read(Mem, Len);
		}

		return Len > 0;
	}

	return 0;
}

int GMemStream::Close()
{
	if (Own)
		DeleteArray(Mem);
	_Init();
	return true;
}

int64 GMemStream::SetSize(int64 Size)
{
	if (Mem)
	{
		int64 Common = min(Len, Size);
		char *NewMem = new char[Alloc = Size];
		if (NewMem)
		{
			memcpy(NewMem, Mem, Common);
			DeleteArray(Mem);
			Mem = NewMem;
			Len = Size;
		}
		else return -1;
	}
	else
	{
		Close();
		Len = Size;
		Mem = new char[Alloc = Len];
	}

	return Len;
}

bool GMemStream::IsOk()
{
	return Len == 0 OR Mem != 0;
}

int GMemStream::Read(void *Buffer, int Size, int Flags)
{
	int Bytes = 0;
	if (Buffer AND Pos >= 0 AND Pos < Len)
	{
		Bytes = min(Len - Pos, Size);
		memcpy(Buffer, Mem + Pos, Bytes);
		Pos += Bytes;			
	}
	return Bytes;
}

int GMemStream::Write(void *Buffer, int Size, int Flags)
{
	int Bytes = 0;
	if (Buffer && Size > 0)
	{
		if (GrowBlockSize)
		{
			// Allow mem stream to grow
			char *Ptr = (char*)Buffer;
			while (Size)
			{
				if (Pos >= Alloc)
				{
					// Grow the mem block
					int64 NewSize = Pos + Size;
					int Blocks = (NewSize + GrowBlockSize - 1) / GrowBlockSize;
					
					int64 NewAlloc = Blocks * GrowBlockSize;
					char *NewMem = new char[NewAlloc];
					if (!NewMem)
						return Bytes;
					memcpy(NewMem, Mem, Pos);
					if (Own)
						DeleteArray(Mem);
					Mem = NewMem;
					Alloc = NewAlloc;
				}

				if (Pos < Alloc)
				{
					// Fill available mem first...
					int Remaining = Alloc - Pos;
					int Copy = min(Size, Remaining);
					memcpy(Mem + Pos, Ptr, Copy);
					Size -= Copy;
					Ptr += Copy;
					Bytes += Copy;
					Pos += Copy;
					Len = max(Pos, Len);
				}
				else break;
			}
		}
		else if (Pos >= 0 AND Pos < Len)
		{
			// Fill fixed space...
			Bytes = min(Len - Pos, Size);
			memcpy(Mem + Pos, Buffer, Bytes);
			Pos += Bytes;
		}
	}
	return Bytes;
}

GStreamI *GMemStream::Clone()
{
	return new GMemStream(Mem, Len, true);
}

int GMemStream::Write(GStream *Out, int Size)
{
	int Wr = -1;

	if (Out && Size > 0)
	{
		int Common = min(Size, ((int)(Len-Pos)));
		Wr = Out->Write(Mem, Common);
	}

	return Wr;
}

//////////////////////////////////////////////////////////////////////////////////////////
GTempStream::GTempStream(char *tmp, int maxMemSize) : GProxyStream(0)
{
	s = &Null;
	MaxMemSize = maxMemSize;
	TmpFolder = NewStr(tmp);
	Tmp = 0;
	Mem = 0;
}

GTempStream::~GTempStream()
{
	Empty();
	DeleteArray(TmpFolder);
}

void GTempStream::Empty()
{
	DeleteObj(Mem);
	if (Tmp)
	{
		char s[MAX_PATH];
		strsafecpy(s, Tmp->GetName(), sizeof(s));
		DeleteObj(Tmp);
		FileDev->Delete(s, false);
	}
	s = 0;
}

int GTempStream::Write(void *Buffer, int Size, int Flags)
{
	if (s == &Null)
	{
		s = Mem = new GMemStream(16 << 10);
	}

	int Status = GProxyStream::Write(Buffer, Size, Flags);
	
	if (Mem != 0 && s->GetSize() > MaxMemSize)
	{
		// Convert stream from memory to disk as it's getting too large
		char c[MAX_PATH], f[32];
		int i=0;
		do
		{
			sprintf(f, "GTempStream-%x.tmp", ((int)this)+i++);
			
			char *t = TmpFolder;
			if (!TmpFolder)
				LgiGetTempPath(t = c, sizeof(c));

			LgiMakePath(c, sizeof(c), t, f);
		}
		while (FileExists(c));

		LgiAssert(Tmp == 0);
		if (Tmp = new GFile)
		{
			int64 Len = Mem->GetSize();
			if (!Tmp->Open(c, O_WRITE | O_READ))
			{
				// Fallback to keeping stuff in memory
				DeleteObj(Tmp);
			}
			else
			{
				Mem->SetPos(0);
				if (Mem->Write(Tmp, Len) != Len)
				{
					// Copy failed... fall back to mem
					DeleteObj(Tmp);
					FileDev->Delete(c, false);
				}
				else
				{
					// Copy ok...
					DeleteObj(Mem);
					s = Tmp;
					Tmp->SetPos(Tmp->GetSize());
				}
			}
		}
	}

	return Status;
}
