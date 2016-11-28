#include "Lgi.h"
#include "GUnicode.h"

wchar_t *Utf8ToWide(const char *In, int InLen)
{
	if (!In)
		return NULL;

	return (wchar_t*) LgiNewConvertCp(LGI_WideCharset, In, "utf-8", InLen);
}

char *WideToUtf8(const wchar_t *In, int InLen)
{
	if (!In)
		return NULL;

	return (char*) LgiNewConvertCp("utf-8", In, LGI_WideCharset, InLen*sizeof(*In));
}

