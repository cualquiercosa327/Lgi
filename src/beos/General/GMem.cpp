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
#include <stdarg.h>

#include "GMem.h"
#include "LgiDefs.h"

#define ToUpper(c) ( ( (c) >= 'a' && (c) <= 'z') ? (c)+('A'-'a') : (c) )
#define ToLower(c) (((c) >= 'A' && (c) <= 'Z') ? (c)+('a'-'A') : (c))

#ifdef MEMORY_DEBUG
void *operator new(size_t Size, char *file, int line)
{
	long *Ptr = (long*) malloc(Size + 4);
	if (Ptr)
	{
		memset(Ptr+1, 0xCC, Size);
		*Ptr = Size;
	}
	return Ptr + 1;
}

void *operator new[](size_t Size, char *file, int line)
{
	long *Ptr = (long*) malloc(Size + 4);
	if (Ptr)
	{
		memset(Ptr+1, 0xCC, Size);
		*Ptr = Size;
	}
	return Ptr + 1;
}

void operator delete(void *p)
{
	long *Ptr = (long*) p;
	Ptr--;
	memset(Ptr+1, 0xCC, *Ptr);
	free(Ptr);
}
#endif

void MemorySizeToStr(char *Str, uint Size)
{
	#define K				1024
	#define M				(K*K)
	#define G				(M*K)

	if (Size >= G)
	{
		sprintf(Str, "%.2f G", (double) Size / G);
	}
	else if (Size >= M)
	{
		sprintf(Str, "%.2f M", (double) Size / M);
	}
	else if (Size >= (10 * K))
	{
		sprintf(Str, "%.2f K", (double) Size / K);
	}
	else
	{
		sprintf(Str, "%ld bytes", Size);
	}

	#undef K
	#undef M
	#undef G
}

bool LgiDumpMemoryStats(char *filename)
{
	return false;
}

bool LgiCanReadMemory(void *p, int Len)
{
	LgiAssert(0);
	return p != NULL;
}
