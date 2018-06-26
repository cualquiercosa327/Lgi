#include "Lgi.h"

void LgiShowFileProperties(OsView Parent, const char *Filename)
{
	LgiAssert(!"Impl me.");
}

bool LgiBrowseToFile(const char *Filename)
{
	if (!Filename)
		return false;

	char Browser[MAX_PATH];
	if (!LgiGetAppForMimeType("inode/directory", Browser, sizeof(Browser)))
		return false;
	
	GString f;
	if (strchr(Filename, ' '))
		f.Printf("\"%s\"", Filename);
	else
		f = Filename;
	
	return LgiExecute(Browser, f);
}