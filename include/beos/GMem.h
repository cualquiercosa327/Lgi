/*hdr
**	FILE:		Memory.h
**	AUTHOR:		Matthew Allen
**	DATE:		30/11/93
**	DESCRIPTION:	Memory subsystem header
**
**
**	Copyright (C) 1995, Matthew Allen
**		fret@memecode.com
*/

#ifndef __MEMORY_H
#define __MEMORY_H

#include <string.h>
#include "LgiDefs.h"
#include "LgiOsDefs.h"

// #define MEMORY_DEBUG

#ifdef MEMORY_DEBUG

	extern void *operator		new(unsigned size, char *file, int line);
	extern void *operator		new[](unsigned size, char *file, int line);
	extern void operator		delete(void *p);

#endif

#define DeleteObj(obj)		if (obj) { delete obj; obj = 0; }
#define DeleteArray(obj)	if (obj) { delete [] obj; obj = 0; }

class MemoryDevice
{
public:
	MemoryDevice();
	~MemoryDevice();

	static int GetFreeMemory() { return 0; }
};

extern void MemorySizeToStr(char *Str, uint Size);

#endif
