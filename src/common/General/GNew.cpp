#include "GMem.h"

#ifdef LGI_MEM_DEBUG

void *operator new(unsigned int size)
{
	return lgi_malloc(size);
}

void *operator new[](unsigned int size)
{
	return lgi_malloc(size);
}

void operator delete(void *p)
{
	lgi_free(p);
}

void operator delete[](void *p)
{
	lgi_free(p);
}

#endif