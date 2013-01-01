/*hdr
**      FILE:           Memory.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           10/4/98
**      DESCRIPTION:    Memory subsystem
**
**      Copyright (C) 2007, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>

#include "LgiOsDefs.h"
#include "GMem.h"
#include "GMutex.h"

bool LgiCheckHeap()
{
	return true;
}

bool LgiDumpMemoryStats(char *filename)
{
	return false;
}

void MemorySizeToStr(char *Str, uint64 Size)
{
	#define K				1024.0
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
		sprintf(Str, LGI_PrintfInt64" bytes", Size);
	}

	#undef K
	#undef M
	#undef G
}

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

#ifdef LGI_MEM_DEBUG

#undef malloc
#undef free

void *lgi_malloc(size_t size)
{
	if (size > 0)
		return malloc(size);

	return 0;
}

void lgi_free(void *ptr)
{
	return free(ptr);
}

#endif
