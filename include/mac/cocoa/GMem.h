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

LgiFunc bool LgiCheckHeap();
LgiFunc bool LgiCanReadMemory(void *p, int Len = 1);

#if 0 // def MEMORY_DEBUG

	LgiFunc void LgiDumpHeapInfo();

	LgiFunc char				*_vmem_file;
	LgiFunc int					_vmem_line;

	void *operator				new(unsigned size, char *file, int line);
	void *operator				new[](unsigned size, char *file, int line);
	void operator				delete(void *p);
	void operator				delete(void *p, char *file, int line);

	#define DeleteObj(obj)		if (obj) { _vmem_file = __FILE__; _vmem_line = __LINE__; delete obj; ((int*)obj) = 0; }
	#define DeleteArray(obj)	if (obj) { _vmem_file = __FILE__; _vmem_line = __LINE__; delete [] obj ; ((int*)obj) = 0; }

#else

	// Defined in LgiDefs.h

#endif

#endif
