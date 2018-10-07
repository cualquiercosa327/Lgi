#include <stdlib.h>
#include <memory.h>

/*
 * A merge sort implementation, simplified from the qsort implementation
 * by Mike Haertel, which is a part of the GNU C Library.
 */

static
void
msort_with_tmp(	void *Base,
				size_t NumElem,
				size_t SizeElem,
				int (*FuncCmp)(void *Ctx, const void *, const void *),
				void *Context,
				char *ExtTmp)
{
	char *LocalTmp;
	char *b1, *b2;
	size_t n1, n2;

	if (NumElem <= 1)
		return;

	n1 = NumElem / 2;
	n2 = NumElem - n1;
	b1 = Base;
	b2 = (char *)Base + (n1 * SizeElem);

	msort_with_tmp(b1, n1, SizeElem, FuncCmp, Context, ExtTmp);
	msort_with_tmp(b2, n2, SizeElem, FuncCmp, Context, ExtTmp);

	LocalTmp = ExtTmp;

	while (n1 > 0 && n2 > 0)
	{
		if (FuncCmp(Context, b1, b2) <= 0)
		{
			memcpy(LocalTmp, b1, SizeElem);
			LocalTmp += SizeElem;
			b1 += SizeElem;
			--n1;
		}
		else
		{
			memcpy(LocalTmp, b2, SizeElem);
			LocalTmp += SizeElem;
			b2 += SizeElem;
			--n2;
		}
	}
	
	if (n1 > 0)
		memcpy(LocalTmp, b1, n1 * SizeElem);
	
	memcpy(Base, ExtTmp, (NumElem - n2) * SizeElem);
}

void qsort_s(void *Base,
			size_t NumOfElements,
			size_t SizeOfElements,
			int (__cdecl *FuncCompare)(void *,const void *,const void *),
			void *Context)
{
	if (!FuncCompare)
		return;

	const size_t size = NumOfElements * SizeOfElements;
	char buf[1024];

	if (size < sizeof(buf))
	{
		/* The temporary array fits on the small on-stack buffer. */
		msort_with_tmp(Base, NumOfElements, SizeOfElements, FuncCompare, Context, buf);
	}
	else
	{
		/* It's somewhat large, so malloc it.  */
		char *tmp = malloc(size);
		if (tmp)
		{
			msort_with_tmp(Base, NumOfElements, SizeOfElements, FuncCompare, Context, tmp);
			free(tmp);
		}
	}
}