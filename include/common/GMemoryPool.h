#ifndef _GMEM_POOL_H_
#define _GMEM_POOL_H_

#include "GArray.h"
#include "LgiInterfaces.h"

class GMemoryPool : public GMemoryPoolI
{
	struct Block
	{
		int Used, Len;
		char *Ptr;

		Block()
		{
			Used = Len = 0;
			Ptr = NULL;
		}

		~Block()
		{
			delete [] Ptr;
		}

		bool Has(int Bytes)
		{
			return Used + Bytes <= Len;
		}
	};

	size_t BlockSize;
	GArray<Block> Mem;

public:
	GMemoryPool(int block_size = 32 << 10)
	{
		BlockSize = block_size;
	}

	void *Alloc(size_t Size)
	{
		Block *b = 0;

		if (Mem.Length() == 0 || !Mem.Last().Has(Size))
		{
			// New block
			b = &Mem.New();
			b->Len = BlockSize;
			b->Used = 0;
			b->Ptr = new char[b->Len];
		}
		else b = &Mem.Last();

		char *p = b->Ptr + b->Used;
		b->Used += Size;

		return p;
	}
	
	void Free(void *Ptr)
	{
	}
	
	void Empty()
	{
		Mem.Length(0);
	}
	
	char *Strdup(const char *s, int len = -1)
	{
		if (!s)
			return NULL;
		if (len < 0)
			len = strlen(s);
		char *n = (char*)Alloc(len + 1);
		if (!n)
			return NULL;
		memcpy(n, s, len);
		n[len] = 0;
		return n;
	}

	char16 *StrdupW(const char16 *s, int len = -1)
	{
		if (!s)
			return NULL;
		if (len < 0)
			len = StrlenW(s);
		int bytes = len * sizeof(char16);
		char16 *n = (char16*)Alloc(bytes + sizeof(char16));
		if (!n)
			return NULL;
		memcpy(n, s, bytes);
		n[len] = 0;
		return n;
	}
};

#endif