/*hdr
**      FILE:           Memory.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           10/4/98
**      DESCRIPTION:    Memory subsystem
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>

#include "LgiDefs.h"
#include "LgiOsDefs.h"
#include "GMem.h"
#include "GMutex.h"

#ifdef MEMORY_DEBUG

#include <ctype.h>

char *_vmem_file = 0;
int _vmem_line = 0;

static int _UsedEntries = 0;

class MInfo
{
public:
	char File[16];
	int Line;
	int Count;
	
	MInfo()
	{
		File[0] = 0;
		Line = 0;
		Count = 0;
	}
	
	void Set(char *f, int l)
	{
		if (Line == 0)
		{
			char *d = strrchr(f, DIR_CHAR);
			if (d) strncpy(File, d+1, sizeof(File)-1);
			else strncpy(File, f, sizeof(File)-1);
			File[sizeof(File)-1] = 0;
			Line = l;
			Count = 0;
			_UsedEntries++;
		}
	}
};

struct MBlock
{
	MInfo *Info;
	int Size;
};

#define MTable_Size (64 << 10)
static MInfo _Table[MTable_Size];
GSemaphore _Lock;

uint _Hash(char *v)
{
	uint32 h = 0;

	// case insensitive
	char c;
	for (; *v; v++)
	{
		c = tolower(*v);
		h = (h << 5) - h + c;
	}

	return h;
}

MInfo *_GetInfo(char *f, int l)
{
	uint Hash = _Hash(f);

	int i = Hash % MTable_Size;
	for (; _Table[i].Line AND stricmp(_Table[i].File, f) != 0; i = (i + 1) % MTable_Size)
	{
		i++;
	}
	if (NOT _Table[i].Line)
	{
		if (_UsedEntries > (MTable_Size >> 1))
		{
			printf("%s:%i - _GetInfo is running out of table space: UsedEntries=%i\n", __FILE__, __LINE__, _UsedEntries);
			exit(-1);
		}
		
		_Table[i].Set(f, l);
	}
	
	return _Table + i;
}

void *operator new(unsigned size, char *file, int line)
{
	void *p = 0;

	if (_Lock.Lock())
	{
		MInfo *i = _GetInfo(file, line);
		if (i)
		{
			i->Count += size;

			MBlock *b = (MBlock*) malloc(size + sizeof(MBlock));
			if (b)
			{
				b->Info = i;
				b->Size = size;
				p = b + 1;
			}
		}
		
		_Lock.Unlock();
	}
	else
	{
		printf("%s:%i - Couldnt lock mem debug table\n", __FILE__, __LINE__);
		exit(0);
	}
	
	return p;
}

void *operator new[](unsigned size, char *file, int line)
{
	void *p = 0;

	if (_Lock.Lock())
	{
		MInfo *i = _GetInfo(file, line);
		if (i)
		{
			i->Count += size;

			MBlock *b = (MBlock*) malloc(size + sizeof(MBlock));
			if (b)
			{
				b->Info = i;
				b->Size = size;
				p = b + 1;
			}
		}
		
		_Lock.Unlock();
	}
	else
	{
		printf("%s:%i - Couldnt lock mem debug table\n", __FILE__, __LINE__);
		exit(0);
	}
	
	return p;
}

void operator delete(void *p)
{
	if (p)
	{
		if (_Lock.Lock())
		{
			MBlock *b = ((MBlock*)p)-1;
			if (b->Info)
			{
				b->Info->Count -= b->Size;
			}
			free(b);

			_Lock.Unlock();
		}
		else
		{
			printf("%s:%i - Couldnt lock mem debug table\n", __FILE__, __LINE__);
			exit(0);
		}
	}
}

void operator delete [] (void *p)
{
	if (p)
	{
		if (_Lock.Lock())
		{
			MBlock *b = ((MBlock*)p)-1;
			if (b->Info)
			{
				b->Info->Count -= b->Size;
			}
			free(b);

			_Lock.Unlock();
		}
		else
		{
			printf("%s:%i - Couldnt lock mem debug table\n", __FILE__, __LINE__);
			exit(0);
		}
	}
}

void LgiDumpHeapInfo()
{
	printf("Heap Info\n");
	for (int i=0; i<MTable_Size; i++)
	{
		if (_Table[i].Line)
		{
			char s[32];
			FormatSize(s, _Table[i].Count);
			printf("[%i] %s:%i (%s)\n", i, _Table[i].File, _Table[i].Line, s);
		}
	}
}

#endif

#ifdef LGI_MEM_DEBUG
#undef malloc
#undef free

void *lgi_malloc(size_t size)
{
	return malloc(size);
}

void lgi_free(void *ptr)
{
	return free(ptr);
}

#endif

bool LgiCheckHeap()
{
	return 1;
}

void MemorySizeToStr(char *Str, uint64 Size)
{
	#define K				1024.0
	#define M				(K*K)
	#define G				(M*K)

	if (Size >= G)
	{
		sprintf(Str, "%.2 G", Size / G);
	}
	else if (Size >= M)
	{
		sprintf(Str, "%.2f M", Size / M);
	}
	else if (Size >= (10 * K))
	{
		sprintf(Str, "%.2f K", Size / K);
	}
	else
	{
		sprintf(Str, "%ld bytes", Size);
	}

	#undef K
	#undef M
	#undef G
}

/////////////////////////////////////////////////////////////////////////
bool LgiCanReadMemory(void *p, int Len)
{
	try
	{
		int i = 0;
		for (char *Ptr = (char*)p; Len > 0; Len--)
		{
			i += *Ptr++;
		}
	}
	catch (...)
	{
		// I guess not... then..
		return false;
	}

	return true;
}

bool LgiDumpMemoryStats(char *filename)
{
	return false;
}

