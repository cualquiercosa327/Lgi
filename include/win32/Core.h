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

LgiFunc void MemSet(void *d, int c, uint size);
LgiFunc void MemCpy(void *d, void *s, uint size);
LgiFunc void MemAnd(void *d, void *s, uint size);
LgiFunc void MemXor(void *d, void *s, uint size);
LgiFunc void MemOr(void *d, void *s, uint size);

#endif
