#include <stdio.h>
#include "Lgi.h"
#include "LDateTime.h"

LPTOP_LEVEL_EXCEPTION_FILTER _PrevExceptionHandler = 0;

#if _MSC_VER >= 1400
// VS2005 or later, write a mini-dump

typedef enum _MINIDUMP_TYPE {
  MiniDumpNormal                           = 0x00000000,
  MiniDumpWithDataSegs                     = 0x00000001,
  MiniDumpWithFullMemory                   = 0x00000002,
  MiniDumpWithHandleData                   = 0x00000004,
  MiniDumpFilterMemory                     = 0x00000008,
  MiniDumpScanMemory                       = 0x00000010,
  MiniDumpWithUnloadedModules              = 0x00000020,
  MiniDumpWithIndirectlyReferencedMemory   = 0x00000040,
  MiniDumpFilterModulePaths                = 0x00000080,
  MiniDumpWithProcessThreadData            = 0x00000100,
  MiniDumpWithPrivateReadWriteMemory       = 0x00000200,
  MiniDumpWithoutOptionalData              = 0x00000400,
  MiniDumpWithFullMemoryInfo               = 0x00000800,
  MiniDumpWithThreadInfo                   = 0x00001000,
  MiniDumpWithCodeSegs                     = 0x00002000 
} MINIDUMP_TYPE;

typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
  DWORD               ThreadId;
  PEXCEPTION_POINTERS ExceptionPointers;
  BOOL                ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;

typedef void *PMINIDUMP_USER_STREAM_INFORMATION;
typedef void *PMINIDUMP_CALLBACK_INFORMATION;

class DbgHelp : public GLibrary
{
public:
	DbgHelp() : GLibrary("Dbghelp")
	{
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
			Info.ThreadId = GetCurrentThreadId();
			Info.ExceptionPointers = e;
			Info.ClientPointers = false;

			LgiTrace("Calling MiniDumpWriteDump...\n");
			BOOL Ret = Help.MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), File.Handle(), MiniDumpNormal, &Info, 0, 0);
			LgiTrace("MiniDumpWriteDump=%i\n", Ret);
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
