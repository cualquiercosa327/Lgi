#include <stdio.h>
#include "Lgi.h"
#include "LDateTime.h"

LPTOP_LEVEL_EXCEPTION_FILTER _PrevExceptionHandler = 0;

#if _MSC_VER >= 1400

// VS2005 or later, write a mini-dump
#include <dbghelp.h>

class DbgHelp : public GLibrary
{
public:
	DbgHelp() : GLibrary("Dbghelp")
	{
		WCHAR   DllPath[MAX_PATH] = {0};
		GetModuleFileNameW(Handle(), DllPath, _countof(DllPath));		
		LgiTrace("Loaded '%S'\n", DllPath);
	}

	#undef GLibCallType
	#define GLibCallType __stdcall
	GLibFunc7(	BOOL, MiniDumpWriteDump,
				HANDLE, hProcess,
				DWORD, ProcessId,
				HANDLE, hFile,
				MINIDUMP_TYPE, DumpType,
				PMINIDUMP_EXCEPTION_INFORMATION, ExceptionParam,
				PMINIDUMP_USER_STREAM_INFORMATION, UserStreamParam,
				PMINIDUMP_CALLBACK_INFORMATION, CallbackParam);
};

/*
HRESULT GenerateCrashDump(MINIDUMP_TYPE flags, EXCEPTION_POINTERS *seh=NULL)
{
	HRESULT error = S_OK;
	DbgHelp Dll;

	// get the time
	SYSTEMTIME sysTime = {0};
	GetSystemTime(&sysTime);

	// get the computer name
	char compName[MAX_COMPUTERNAME_LENGTH + 1] = {0};
	DWORD compNameLen = ARRAYSIZE(compName);
	GetComputerNameA(compName, &compNameLen);

	// build the filename: APPNAME_COMPUTERNAME_DATE_TIME.DMP
	char path[MAX_PATH] = {0};
	sprintf_s(path, ARRAYSIZE(path),
			"c:\\myapp_%s_%04u-%02u-%02u_%02u-%02u-%02u.dmp",
			compName, sysTime.wYear, sysTime.wMonth, sysTime.wDay,
			sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

	// open the file
	HANDLE hFile = CreateFileA(	path,
								GENERIC_READ|GENERIC_WRITE,
								FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE,
								NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); 
	if (hFile == INVALID_HANDLE_VALUE)
	{
		error = GetLastError();
		error = HRESULT_FROM_WIN32(error);
		return error;
	}

	// get the process information
	HANDLE hProc = GetCurrentProcess();
	DWORD procID = GetProcessId(hProc);

	// if we have SEH info, package it up
	MINIDUMP_EXCEPTION_INFORMATION sehInfo = {0};
	MINIDUMP_EXCEPTION_INFORMATION *sehPtr = NULL;
	if (seh)
	{
		sehInfo.ThreadId = GetCurrentThreadId();
		sehInfo.ExceptionPointers = seh;
		sehInfo.ClientPointers = FALSE;
		sehPtr = &sehInfo;
	}

	// generate the crash dump
	BOOL result = Dll.MiniDumpWriteDump(hProc, procID, hFile,
									flags, sehPtr, NULL, NULL);

	if (!result)
	{
		error = (HRESULT)GetLastError(); // already an HRESULT
	}

	// close the file
	CloseHandle(hFile);

	return error;
}
*/

LONG __stdcall GApp::_ExceptionFilter(LPEXCEPTION_POINTERS e, char *ProductId)
{
	char *Title = "Application Crash";

	LgiSetLeakDetect(false);
	SetUnhandledExceptionFilter(0); // We can't handle crashes within
									// ourself, so let the default handler
									// do it.

	char p[MAX_PATH];
	LgiGetSystemPath(LSP_APP_ROOT, p, sizeof(p));
	if (!DirExists(p))
		FileDev->CreateFolder(p);

	int len = strlen(p);
	if (ProductId)
		sprintf_s(p+len, sizeof(p)-len, "%s%s-crash.dmp", DIR_STR, ProductId);
	else
		sprintf_s(p+len, sizeof(p)-len, "%scrash-dump.dmp", DIR_STR);

	LgiTrace("GApp::_ExceptionFilter, Crash dump path '%s'\n", p);
	
	GFile File;
	if (File.Open(p, O_READWRITE))
	{
		DbgHelp Help;
		if (Help.IsLoaded())
		{
			MINIDUMP_EXCEPTION_INFORMATION Info;

			ZeroObj(Info);

			Info.ThreadId = GetCurrentThreadId();
			Info.ExceptionPointers = e;
			Info.ClientPointers = false;

			LgiTrace("Calling MiniDumpWriteDump...\n");
			BOOL Ret = Help.MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), File.Handle(), MiniDumpNormal, &Info, NULL, NULL);
			// LgiTrace("MiniDumpWriteDump=%i\n", Ret);
			if (Ret)
			{
				LgiMsg(0, "This application has crashed. A mini dump has been written to:\n%s\n", Title, MB_OK|MB_APPLMODAL, p);
			}
			else
			{
				DWORD Err = GetLastError();
				char m[256];
				#if _MSC_VER >= _MSC_VER_VS2005
				sprintf_s
				#else
				snprintf
				#endif
				    (m, sizeof(m), "This application has crashed. MiniDumpWriteDump failed with %i 0x%x.", Err, Err);
				LgiTrace("%s:%i - %s\n", _FL, m);
				LgiMsg(0, m, Title, MB_OK|MB_APPLMODAL);
			}
		}
		else
		{
			LgiTrace("%s:%i - Can't find 'dbghelp.dll'\n", _FL);
			LgiMsg(0, "This application has crashed. Can't find 'dbghelp.dll' either.", Title, MB_OK|MB_APPLMODAL);
		}
	}
	else
	{
		LgiTrace("%s:%i - Can't open minidump '%s'\n", _FL, p);
		LgiMsg(	0,
				"This application has crashed. Can't open mini dump '%s' either.",
				Title,
				MB_OK|MB_APPLMODAL,
				p);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

#else // VC6


/*
#include "StackWalker.h"

class MyStackWalker : public StackWalker
{
	char *Cur;

public:
	char Buf[4096];

	MyStackWalker()
	{
		Cur = Buf;
	}

	void OnOutput(LPCSTR szText)
	{
		LgiTrace("%s", szText);

		int Len = strlen(szText);
		int Cp = min(sizeof(Buf)-1-(Cur-Buf), Len);
		memcpy(Cur, szText, Cp);
		Cur += Cp;
		*Cur = 0;
		LgiAssert(Cur < Buf+sizeof(Buf));
	}
};

LONG __stdcall GApp::_ExceptionFilter(LPEXCEPTION_POINTERS e, char *ProductId)
{
	SetUnhandledExceptionFilter(0); // We can't handle crashes within
									// ourself, so let the default handler
									// do it.

	MyStackWalker s;
	LgiTrace("Application crash:\n\n");
	s.ShowCallstack();
	MessageBox(0, s.Buf, "Application Crash", MB_OK);

	return EXCEPTION_EXECUTE_HANDLER;
}
*/
#include "GSymLookup.h"

LONG __stdcall GApp::_ExceptionFilter(LPEXCEPTION_POINTERS e, char *ProductId)
{
	SetUnhandledExceptionFilter(0); // We can't handle crashes within
									// ourself, so let the default handler
									// do it.

	
	char buf[1024];
	GSymLookup s;
	GSymLookup::Addr a[12];
	int len = s.BackTrace(e->ContextRecord->Eip, e->ContextRecord->Ebp, a, 12);
	if (s.Lookup(buf, sizeof(buf), a, len))
	{
		LgiTrace("%s\n", buf);
		MessageBox(0, buf, "Application Crash", MB_OK);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

#endif
