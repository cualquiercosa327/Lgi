/* The purpose of this code is to allow various Lgi applications to update their files
   even when installed in the Program Files tree, where they do not have write access to
   their own install folder. This util is built with a manifest that requests admin rights,
   allowing it to write to the target folder. */
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "../include/common/GArray.h"

const TCHAR *AppName = _T("Updater");

void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	if (!b)
		assert(!test);
}

char *NewStr(char *s, size_t len = 0)
{
	if (!s) return NULL;
	if (len == 0) len = strlen(s);
	char *n = new char[len];
	memcpy(n, s, sizeof(*n)*len);
	n[len] = 0;
	return n;
}

TCHAR *NewStr(TCHAR *s, size_t len = 0)
{
	if (!s) return NULL;
	if (len == 0) len = _tcslen(s);
	TCHAR *n = new TCHAR[len];
	memcpy(n, s, sizeof(*n)*len);
	n[len] = 0;
	return n;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	GArray<WCHAR *> Files;

	// Parse the command line for the filename(s) to copy....
	WCHAR *CmdLine = GetCommandLineW();
	const char *WhiteSpace = " \r\t\n";
	for (WCHAR *s = CmdLine; s && *s; )
	{
		if (strchr(WhiteSpace, *s))
		{
			s++;
		}
		else if (strchr("\'\"", *s))
		{
			int delim = *s++;
			WCHAR *e = s;
			while (*e && *e != delim)
				e++;
			Files.Add(NewStr(s, e-s));
			s = *e ? e + 1 : e;
		}
		else
		{
			WCHAR *e = s;
			while (*e && !strchr(WhiteSpace, *e))
				e++;
			Files.Add(NewStr(s, e-s));
			s = e;
		}
	}

	// Get the location of the executable...
	WCHAR Exe[MAX_PATH];
	if (GetModuleFileNameW(NULL, Exe, sizeof(Exe)) <= 0)
	{
		WCHAR Msg[256];
		swprintf_s(Msg, 256, L"GetModuleFileNameW failed with the error 0x%x", GetLastError());
		MessageBox(NULL, Msg, AppName, MB_OK);
		return -1;
	}
	
	// Chop off the filename...
	WCHAR *e = wcsrchr(Exe, '\\');
	if (!e)
	{
		MessageBox(NULL, _T("No directory char in the exe file name?"), AppName, MB_OK);
		return -1;
	}
	*e++;
	*e = 0;
	
	// Copy the files to the same location as the exe...
	int Copied = 0;
	for (unsigned i=1; i<Files.Length(); i++)
	{
		WCHAR *Leaf = wcsrchr(Files[i], '\\');
		if (!Leaf)
		{
			MessageBox(NULL, _T("No filename in arguments"), AppName, MB_OK);
			return -1;
		}

		// Work out the output filename...
		WCHAR Output[MAX_PATH];
		swprintf_s(Output, MAX_PATH, L"%s%s", Exe, Leaf + 1);
		
		// Copy the file...
		BOOL Success = CopyFile(Files[i], Output, FALSE);
		if (!Success)
		{
			WCHAR Msg[MAX_PATH << 1];
			DWORD Err = GetLastError();
			swprintf_s(	Msg,
						MAX_PATH << 1,
						L"Failed to copy\n'%s'\nto\n'%s'\nError: 0x%x",
						Files[i],
						Output,
						Err);
			MessageBox(NULL, Msg, AppName, MB_OK);
			return -1;
		}
		
		Copied++;
	}
	
	// Return success
	WCHAR Msg[256];
	swprintf_s(	Msg,
				256,
				L"Success: %i files processed.",
				Copied);
	MessageBox(NULL, Msg, AppName, MB_OK);
	return 0;
}

