/*hdr
**	FILE:			Memory.h
**	AUTHOR:			Matthew Allen
**	DATE:			30/11/93
**	DESCRIPTION:	Memory subsystem header
**
**
**	Copyright (C) 1995, Matthew Allen
**		fret@memecode.com
*/

#ifndef __MEMORY_H
#define __MEMORY_H

#include "LgiInc.h"
#include "LgiOsDefs.h"

#ifndef __cplusplus
typedef unsigned char bool;
#endif

/// \returns true if heap is ok.
LgiFunc bool LgiCheckHeap();

/// Turns on/off leak detection.
LgiFunc void LgiSetLeakDetect(bool On);

/// \return true if the block of memory referenced by 'p' is readable for Len bytes.
LgiFunc bool LgiCanReadMemory
(
	void *p,
	int Len
	#ifdef __cplusplus
	 = 1
	#endif
);

/// Dumps info about the currently allocated blocks of memory
/// to 'filename'. Only work when LGI_MEM_DEBUG defined.
/// \returns true on success.
LgiFunc bool LgiDumpMemoryStats(char *filename);

#if (!defined(_MSC_VER) || _MSC_VER != 1310) && defined(WIN32) && !defined(WIN64)

// Set this to '1' to switch on memory tracking, else '0'.
#if 0 // defined(_DEBUG)

    #if defined(_MSC_VER) && _MSC_VER == 1310
    #error "Visual C++ 2003 does not support redefining new and delete."
    #endif
    
	#include <stdlib.h>
	
	/// '1' indicates that we want to track memory usage.
	/// To use this feature every DLL should include the
	/// source file 'GNew.cpp' and each EXE should include
	/// 'LgiMain.cpp' OR 'GNew.cpp' for this to work.
	#define LGI_MEM_DEBUG		1

	#ifdef __cplusplus
	void *operator				new(size_t size);
	void *operator				new[](size_t size);
	void operator				delete[](void *p);
	void operator				delete(void *p);
	#endif

	LgiFunc void *lgi_malloc(size_t size);
	LgiFunc void *lgi_realloc(void *ptr, size_t size);
	LgiFunc void lgi_free(void *ptr);

	#define malloc				lgi_malloc
	#define free				lgi_free
	#define realloc				lgi_realloc

#else

	// Defined in LgiDefs.h

#endif

#endif

#include <string.h>

#endif
