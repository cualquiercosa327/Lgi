/*hdr
**	FILE:		Core.h
**	AUTHOR:		Matthew Allen
**	DATE:		11/3/97
**	DESCRIPTION:	Fast core routines
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

#ifndef _CORE_H_
#define _CORE_H_

extern void MemSet(void *d, int c, uint size);
extern void MemCpy(void *d, void *s, uint size);
extern void MemAnd(void *d, void *s, uint size);
extern void MemXor(void *d, void *s, uint size);
extern void MemOr(void *d, void *s, uint size);

#endif
