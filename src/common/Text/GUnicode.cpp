#include "Lgi.h"

char16 *LgiNewUtf8To16(const char *In, int InLen)
{
	if (!In)
		return NULL;

	return (char16*) LgiNewConvertCp(LGI_WideCharset, In, "utf-8", InLen);
}

char *LgiNewUtf16To8(const char16 *In, int InLen)
{
	if (!In)
		return NULL;

	return (char*) LgiNewConvertCp("utf-8", In, LGI_WideCharset, InLen);
}

