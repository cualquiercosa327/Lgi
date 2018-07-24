/**
 * \file
 * \author Matthew Allen
 * \date 27/7/1995
 * \brief Container classes.\n
	Copyright (C) 1995-2003, <a href="mailto:fret@memecode.com">Matthew Allen</a>
 */

#ifdef LINUX
#define _ISOC99_SOURCE		1
#include <wchar.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "GMem.h"
#include "GContainers.h"
#include "GString.h"
#include "LgiCommon.h"


/////////////////////////////////////////////////////////////////////////////////////////////////
int ACmp(GString *a, GString *b)
{
	return stricmp(*a, *b);
}

int LCmp(char *a, char *b, NativeInt d)
{
	return stricmp(a, b);
}

void UnitTest_CreateList(GArray<GString> &a, List<char> &l, int sz = 100)
{
	a.Empty();
	for (unsigned i=0; i<sz; i++)
		a[i].Printf("obj.%i", i);

	l.Empty();
	for (unsigned i=0; i<sz; i++)
		l.Add(a[i]);
}

bool UnitTest_Err(const char *Fmt, ...)
{
	printf("Error: %s\n", Fmt);
	LgiAssert(0);
	return false;
}

bool UnitTest_Check(GArray<GString> &a, List<char> &l)
{
	if (a.Length() != l.Length())
		return UnitTest_Err("Wrong size.");
	int n = 0;
	for (auto s : l)
	{
		GString t = s;
		if (t.Equals(a[n]))
			;//printf("%i: %s\n", n, s);
		else
			return UnitTest_Err("Wrong value.");
		n++;
	}
	return true;
}

#define CHECK(val) if (!(val)) return false

bool UnitTest_ListClass()
{
	GArray<GString> a;
	List<char> l;
	
	// Create test..
	UnitTest_CreateList(a, l, 100);
	CHECK(UnitTest_Check(a, l));
	
	// Delete tests..
	l.Delete(a[3].Get());
	a.DeleteAt(3, true);
	CHECK(UnitTest_Check(a, l));
	
	while (a.Length() > 60)
	{
		a.DeleteAt(60, true);
		l.DeleteAt(60);
	}
	CHECK(UnitTest_Check(a, l));

	// Sort test..
	a.Sort(ACmp);
	l.Sort(LCmp);
	CHECK(UnitTest_Check(a, l));

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////
GMemQueue::GMemQueue(int prealloc)
{
	PreAlloc = prealloc;
}

GMemQueue::~GMemQueue()
{
	Empty();
}

GMemQueue &GMemQueue::operator=(GMemQueue &p)
{
	Empty();

	PreAlloc = p.PreAlloc;
	for (Block *b = p.Mem.First(); b; b = p.Mem.Next())
	{
		int Alloc = sizeof(Block) + b->Size;
		Alloc = LGI_ALLOC_ALIGN(Alloc);
		Block *n = (Block*) malloc(Alloc);
		if (n)
		{
			memcpy(n, b, sizeof(Block) + b->Size);
			Mem.Insert(n);
		}
		else break;		
	}

	return *this;
}

void GMemQueue::Empty()
{
	for (Block *b = Mem.First(); b; b = Mem.First())
	{
		Mem.Delete(b);
		free(b);
	}
}

int64 GMemQueue::GetSize()
{
	int Size = 0;
	for (Block *b = Mem.First(); b; b = Mem.Next())
	{
		Size += b->Used - b->Next;
	}
	return Size;
}

#if 0
int GMemQueue::Find(char *Str)
{
	if (Str)
	{
		int Pos = 0;
		uint8 *UStr = (uint8*) Str;

		int SLen = strlen(Str);
		if (SLen > 0)
		{
			for (List<Block>::I Blocks = Mem.Start(); Blocks.In(); Blocks++)
			{
				Block *m = *Blocks;
				uint8 *Base = ((uint8*)m->Ptr()) + m->Next;
				uint8 *End = Base + (m->Used - m->Next);

				for (uint8 *Ptr = Base; Ptr < End; Ptr++)
				{
					if (*Ptr == *UStr)
					{
						// Check rest of string
						if (Ptr + SLen > End)
						{
							// Setup separate iterator to get to the next block
							List<Block>::I Match = Blocks;
							Block *Cur = *Match;
							if (Cur == m)
							{
								uint8 *b = Base;
								uint8 *e = End;
								uint8 *p = Ptr;
								bool m = true;

								for (int i=1; i<SLen; i++)
								{
									if (p + i >= e)
									{
										Match++;
										Cur = *Match;
										if (Cur)
										{
											b = ((uint8*)Cur->Ptr()) + Cur->Next;
											e = b + (Cur->Used - Cur->Next);
											p = b - i;
										}
										else break;
									}

									if (p[i] != UStr[i])
									{
										m = false;
										break;
									}
								}

								if (m)
								{
									return Pos + (Ptr - Base);
								}
							}
							else
							{
								LgiAssert(0);
							}
						}
						else
						{
							// String is entirely in this block...
							bool m = true;

							for (int i=1; i<SLen; i++)
							{
								if (Ptr[i] != UStr[i])
								{
									m = false;
									break;
								}
							}

							if (m)
							{
								return Pos + (Ptr - Base);
							}
						}
					}
				}

				Pos += m->Used - m->Next;
			}
		}
	}

	return -1;
}

int64 GMemQueue::Peek(GStreamI *Str, int Size)
{
	int64 Status = 0;

	if (Str && Size <= GetSize())
	{
		Block *b = 0;
		for (b = Mem.First(); b && Size > 0; b = Mem.Next())
		{
			int Copy = min(Size, b->Size - b->Next);
			if (Copy > 0)
			{
				Str->Write(b->Ptr() + b->Next, Copy);
				Size -= Copy;
				Status += Copy;
			}
		}
	}

	return Status;
}

int GMemQueue::Pop(short &i)
{
	short n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

int GMemQueue::Pop(int &i)
{
	int n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

int GMemQueue::Pop(double &i)
{
	double n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

#endif

int64 GMemQueue::Peek(uchar *Ptr, int Size)
{
	int64 Status = 0;

	if (Ptr && Size <= GetSize())
	{
		Block *b = 0;
		for (b = Mem.First(); b && Size > 0; b = Mem.Next())
		{
			int Copy = MIN(Size, b->Size - b->Next);
			if (Copy > 0)
			{
				memcpy(Ptr, b->Ptr() + b->Next, Copy);
				Ptr += Copy;
				Size -= Copy;
				Status += Copy;
			}
		}
	}

	return Status;
}

void *GMemQueue::New(int AddBytes)
{
	int64 Len = GetSize();
	uchar *Data = Len > 0 ? new uchar[Len+AddBytes] : 0;
	if (Data)
	{
		ssize_t Rd = Read(Data, Len);
		if (Rd >= 0 && AddBytes)
		{
			memset(Data+Len, 0, AddBytes);
		}
	}

	return Data;
}

ssize_t GMemQueue::Read(void *Ptr, ssize_t Size, int Flags)
{
	int Status = 0;

	if (Ptr && Size > 0)
	{
		Block *b = 0;
		for (b = Mem.First(); b && Size > 0; b = Mem.Next())
		{
			ssize_t Copy = MIN(Size, b->Used - b->Next);
			if (Copy > 0)
			{
				memcpy(Ptr, b->Ptr() + b->Next, Copy);
				((uchar*&)Ptr) += Copy;
				Size -= Copy;
				b->Next += Copy;
				Status += Copy;
			}
		}

		for (b = Mem.First(); b && b->Next >= b->Used; b = Mem.First())
		{
			Mem.Delete(b);
			free(b);
		}
	}

	return Status;
}

ssize_t GMemQueue::Write(const void *Ptr, ssize_t Size, int Flags)
{
	int Status = 0;

	/*
	char m[256];
	sprintf_s(m, sizeof(m), "%p::Write(%p, %i, %i)\n", this, Ptr, Size, Flags);
	OutputDebugStringA(m);
	*/

	if (Ptr && Size > 0)
	{
		if (PreAlloc > 0)
		{
			Block *Last = Mem.Last();
			if (Last)
			{
				ssize_t Len = MIN(Size, Last->Size - Last->Used);
				if (Len > 0)
				{
					memcpy(Last->Ptr() + Last->Used, Ptr, Len);
					Last->Used += Len;
					Size -= Len;
					((uchar*&)Ptr) += Len;
					Status += Len;
				}
			}
		}

		if (Size > 0)
		{
			ssize_t Bytes = MAX(PreAlloc, Size);
			ssize_t Alloc = sizeof(Block) + Bytes;
			Alloc = LGI_ALLOC_ALIGN(Alloc);
			Block *b = (Block*) malloc(Alloc);
			if (b)
			{
				void *p = b->Ptr();
				memcpy(p, Ptr, Size);
				b->Size = (int)Bytes;
				b->Used = (int)Size;
				b->Next = 0;
				Mem.Insert(b);
				Status += Size;
			}
		}
	}

	return Status;
}

//////////////////////////////////////////////////////////////////////////
GString GStringPipe::NewGStr()
{
	GString s;
	
	int64 Sz = GetSize();
	if (Sz > 0)
	{
		if (s.Length(Sz))
		{
			ssize_t Rd = Read(s.Get(), s.Length());
			if (Rd > 0 && Rd <= Sz)
			{
				s.Get()[Rd] = 0;
			}
		}
		else s.Empty();
	}
	
	return s;
}

ssize_t GStringPipe::LineChars()
{
	ssize_t Len = 0;
	for (Block *m = Mem.First(); m; m = Mem.Next())
	{
		uint8 *p = m->Ptr();

		for (int i = m->Next; i < m->Used; i++)
		{
			if (p[i] == '\n')
				return Len;
			Len++;
		}
	}

	return -1;
}

ssize_t GStringPipe::SaveToBuffer(char *Start, ssize_t BufSize, ssize_t Chars)
{
	char *Str = Start;
	char *End = Str + BufSize; // Not including NULL

	Block *m = Mem.First();
	while (m)
	{
		for (	char *MPtr = (char*)m->Ptr();
				m->Next < m->Used;
				m->Next++)
		{
			if (Str < End)
				*Str++ = MPtr[m->Next];
			if (MPtr[m->Next] == '\n')
			{
				m->Next++;
				goto EndPop;
			}
		}

		if (m->Next >= m->Used)
		{
			Mem.Delete(m);
			free(m);
			m = Mem.First();
		}
	}

	EndPop:
	*Str = 0;

	return Str - Start;
}

GString GStringPipe::Pop()
{
	GString s;
	ssize_t Chars = LineChars();
	if (Chars > 0 &&
		s.Length(Chars))
	{
		SaveToBuffer(s.Get(), Chars, Chars);
	}

	return s;
}

ssize_t GStringPipe::Pop(char *Str, ssize_t BufSize)
{
	if (!Str)
		return 0;

	ssize_t Chars = LineChars();
	if (Chars <= 0)
		return 0;

	return SaveToBuffer(Str, BufSize-1 /* for the NULL */, Chars);
}

ssize_t GStringPipe::Push(const char *Str, ssize_t Chars)
{
	if (!Str)
		return 0;

	if (Chars < 0)
		Chars = strlen(Str);

	return Write((void*)Str, Chars);
}

ssize_t GStringPipe::Push(const char16 *Str, ssize_t Chars)
{
	if (!Str)
		return 0;

	if (Chars < 0)
		Chars = StrlenW(Str);

	return Write((void*)Str, Chars * sizeof(char16));
}

#ifdef _DEBUG
bool GStringPipe::UnitTest()
{
	char Buf[16];
	memset(Buf, 0x1, sizeof(Buf));
	GStringPipe p(8);
	const char s[] = "1234567890abc\n"
					"abcdefghijklmn\n";
	p.Write(s, sizeof(s)-1);
	ssize_t rd = p.Pop(Buf, 10);
	int cmp = memcmp(Buf, "123456789\x00\x01\x01\x01\x01\x01\x01", 16);
	if (cmp)
		return false;
	rd = p.Pop(Buf, 10);
	cmp = memcmp(Buf, "abcdefghi\x00\x01\x01\x01\x01\x01\x01", 16);
	if (cmp)
		return false;

	p.Empty();
	p.Write(s, sizeof(s)-1);
	GString r;
	r = p.Pop();
	if (!r.Equals("1234567890abc"))
		return false;
	r = p.Pop();
	if (!r.Equals("abcdefghijklmn"))
		return false;
	return true;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
GMemFile::GMemFile(int BlkSize)
{
	CurPos = 0;
	Blocks = 0;
	BlockSize = BlkSize < 16 ? 16 : BlkSize;
	ZeroObj(Local);
}

GMemFile::~GMemFile()
{
	Empty();
}

GMemFile::Block *GMemFile::Get(int Index)
{
	if (Blocks == 0 || Index < 0)
		return NULL;

	if (Index < GMEMFILE_BLOCKS)
		return Local[Index];
	
	if (Index - GMEMFILE_BLOCKS >= Extra.Length())
	{
		LgiAssert(!"Index beyond last block");
		return NULL;
	}

	return Extra[Index - GMEMFILE_BLOCKS];
}

GMemFile::Block *GMemFile::Create()
{
	int Alloc = sizeof(Block) + BlockSize - 1;
	Alloc = LGI_ALLOC_ALIGN(Alloc);
	Block *b = (Block*) malloc(Alloc);
	if (!b)
		return NULL;
	
	b->Used = 0;
	b->Offset = Blocks * BlockSize;
	
	if (Blocks < GMEMFILE_BLOCKS)
		Local[Blocks] = b;
	else
		Extra.Add(b);

	Blocks++;
	return b;
}

void GMemFile::Empty()
{
	CurPos = 0;
	for (int i=0; i<GMEMFILE_BLOCKS; i++)
		free(Local[i]);
	for (int i=0; i<Extra.Length(); i++)
		free(Extra[i]);
}

int64 GMemFile::GetSize()
{
	#ifndef __llvm__
	if (this == NULL)
		return -1;
	#endif
	if (Blocks == 0)
		return 0;

	Block *b = GetLast();
	int64 Sz = (Blocks - 1) * BlockSize;
	return Sz + b->Used;	
}

bool GMemFile::FreeBlock(Block *b)
{
	if (!b)
		return false;
	
	ssize_t Idx = b->Offset / BlockSize;
	if (Idx < GMEMFILE_BLOCKS)
	{
		// Local block
		if (Local[Idx] != b ||
			Idx < Blocks-1)
		{
			LgiAssert(!"Block index error.");
			return false;
		}
		
		free(b);
		Local[Idx] = NULL;		
		Blocks--;
		return true;
	}

	// Extra block
	ssize_t Off = Idx - GMEMFILE_BLOCKS;
	if (Off != Extra.Length() - 1)
	{
		LgiAssert(!"Block index error.");
		return false;
	}
	
	free(b);
	Extra.DeleteAt(Off, true);
	Blocks--;
	return true;
}

int64 GMemFile::SetSize(int64 Size)
{
	if (Size <= 0)
	{
		Empty();
	}
	else
	{
		int64 CurSize = GetSize();
		if (Size > CurSize)
		{
			// Increase size...
			int64 Diff = Size - CurSize;
			Block *b = GetLast();
			if (b->Used < BlockSize)
			{
				// Add size to last incomplete block
				ssize_t Remaining = BlockSize - b->Used;
				ssize_t Add = MIN(Diff, Remaining);
				b->Used += Add;
				Diff -= Add;
			}
			while (Diff > 0)
			{
				// Add new blocks to cover the size...
				ssize_t Add = MIN(BlockSize, Diff);
				b = Create();
				b->Used = Add;
				Diff -= Add;
			}
		}
		else
		{
			// Decrease size...
			uint64 Diff = CurSize - Size;
			while (Diff > 0 && Blocks > 0)
			{
				Block *b = GetLast();
				if (!b)
					break;

				ssize_t Sub = MIN(b->Used, Diff);
				b->Used -= Sub;
				Diff -= Sub;
				if (b->Used == 0)
					FreeBlock(b);
			}
		}
	}
	
	return GetSize();
}

int64 GMemFile::GetPos()
{
	return CurPos;
}

int64 GMemFile::SetPos(int64 Pos)
{
	if (Pos <= 0)
		return CurPos = 0; // Off the start of the structure
	
	ssize_t BlockIndex = Pos / BlockSize;
	if (BlockIndex >= Blocks)
		return CurPos = GetSize(); // Off the end of the structure
	
	if (BlockIndex >= 0 && BlockIndex < Blocks - 1)
		return CurPos = Pos; // Inside a full block
	
	Block *Last = GetLast();
	uint64 Offset = Pos - Last->Offset;
	if (Offset >= Last->Used)
		return CurPos = Last->Offset + Last->Used; // End of last block
	
	return CurPos = Pos; // Inside the last block
}

ssize_t GMemFile::Read(void *Ptr, ssize_t Size, int Flags)
{
	if (!Ptr || Size < 1)
		return 0;
	
	uint8 *p = (uint8*) Ptr;
	uint8 *end = p + Size;
	while (p < end)
	{
		int Cur = CurBlock();
		if (Cur >= Blocks)
			break;
		Block *b = Get(Cur);
		
		// Where are we in the current block?
		ssize_t BlkOffset = CurPos - b->Offset;
		LgiAssert(b && BlkOffset >= 0 && BlkOffset <= b->Used);
		ssize_t Remaining = b->Used - BlkOffset;
		if (Remaining > 0)
		{
			ssize_t Common = MIN(Remaining, end - p);
			memcpy(p, b->Data + BlkOffset, Common);
			CurPos += Common;
			p += Common;
		}
		else break;
		
		LgiAssert(p <= end);
		if (p >= end)	// End of read buffer reached?
			break;		// Exit loop
	}	
	
	return p - (uint8*) Ptr;
}

ssize_t GMemFile::Write(const void *Ptr, ssize_t Size, int Flags)
{
	if (!Ptr || Size < 1)
		return 0;

	uint8 *p = (uint8*) Ptr;
	ssize_t len = Size;
	
	Block *b = GetLast();
	if (b && b->Used < BlockSize)
	{
		// Any more space in the last block?
		ssize_t Remaining = BlockSize - b->Used;
		ssize_t Common = MIN(Remaining, Size);
		if (Common > 0)
		{
			memcpy(b->Data + b->Used, p, Common);
			p += Common;
			len -= Common;
			b->Used += Common;
		}
	}

	// Store remaining data into new blocks		
	while (len > 0)
	{
		b = Create();
		if (!b)
			break;
		
		ssize_t Common = MIN(BlockSize, len);
		memcpy(b->Data, p, Common);
		b->Used = Common;
		p += Common;
		len -= Common;
	}
	
	return Size - len;
}
