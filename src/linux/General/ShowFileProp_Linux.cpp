#include "Lgi.h"
#include <tchar.h>

void LgiShowFileProperties(OsView Parent, const char *Filename)
{
	LgiAssert(!"Impl me.");
}

bool LgiBrowseToFile(const char *Filename)
{
	char Browser[MAX_PATH];
	if (!LgiGetAppForMimeType("inode/directory", Browser, sizeof(Browser)))
		return false;
	return LgiExecute(Browser, Filename);
}