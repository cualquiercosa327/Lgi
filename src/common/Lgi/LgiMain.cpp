/** \file
	\author Matthew Allen
 */

#include "Lgi.h"
#include "GToken.h"

/** \brief The main entry point of a Lgi program

	To hide the differences between the different OS's standard entry points for programs
	the file LgiMain.cpp implements a simple platform specific entry point and then calls
	LgiMain() to pass execution onto your program.

	A simple LgiMain() looks like this:
	\code
	int LgiMain(OsAppArguments &Args)
	{
		GApp *App = new GApp("application/x-MyProgram", Args);
		if (App && App->IsOk())
		{
			App->AppWnd = new MyWindow;
			App->Run();
			delete App;
		}

		return 0;
	}
	\endcode
*/
extern int LgiMain
(
	/// The arguments passed in from the OS.
	OsAppArguments &AppArgs
);

bool _BuildCheck()
{
	#ifdef _DEBUG
	const char *AppBuild = "Debug";
	#else
	const char *AppBuild = "Release";
	#endif
	char *LgiBuild = (char*) (LgiIsReleaseBuild() ? "Release" : "Debug");

	if (_stricmp(AppBuild, LgiBuild))
	{
		LgiTrace("Build check failed, app=%s lgi=%s\n", AppBuild, LgiBuild);
		return LgiMsg(	NULL,
						"This application and it's Dll's are mismatched:\n"
						"\n"
						"   Application:\t%s\n"
						"   Lgi:\t\t%s\n"
						"\n"
						"If you continue you may encounter crashes and other undesirable problems.\n"
						"Do you want to continue anyway?",
						"Warning",
						MB_YESNO,
						AppBuild,
						LgiBuild) == IDYES;
	}

	return true;
}

#ifdef WIN32

int
#if _CONSOLE
main(int args, char **arg)
#else
WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif
{
	int Status = 0;
	#if !_CONSOLE && WINNATIVE
	_lgi_app_instance = hInstance;
	#endif
	if (_BuildCheck())
	{
		char16 *CL = (char16*)GetCommandLineW();

    	#if WINNATIVE
		OsAppArguments AppArgs;
		#if !_CONSOLE
		AppArgs.hInstance = hInstance;
		AppArgs.nCmdShow = nCmdShow;
		#endif

		if (*CL == '\"')
		{
			AppArgs.lpCmdLine = StrchrW(CL+1, '\"');
		}
		else
		{
			AppArgs.lpCmdLine = StrchrW(CL+1, ' ');
		}
		if (AppArgs.lpCmdLine) AppArgs.lpCmdLine++;

		#else
		GArray<char*> Args;
		char16 *Ws = L" \t\r\n";
		for (char16 *c = CL; *c; )
		{
		    while (*c && StrchrW(Ws, *c)) c++;
		    if (*c == '\"' || *c == '\'')
		    {
		        char16 delim = *c++;
		        char16 *end = StrchrW(c, delim);
		        if (end)
		        {
		            Args.Add(LgiNewUtf16To8(c, (end-c)*sizeof(char16)));
		            c = end + 1;
		        }
		        else
		        {
		            Args.Add(LgiNewUtf16To8(c));
		            break;
		        }
		    }
		    else
		    {
		        char16 *end = c;
    		    while (*end && !StrchrW(Ws, *end)) end++;
    		    if (end > c)
	                Args.Add(LgiNewUtf16To8(c, (end-c) * sizeof(char16)));
	            c = end + (*end != 0);
		    }
		}
		
		OsAppArguments AppArgs(Args.Length(), &Args[0]);
		#endif
		
		Status = LgiMain(AppArgs);
	}
	return Status;
}

#else

#ifdef MAC
pascal OSErr TestProc(const AppleEvent *ae, AppleEvent *reply, SRefCon handlerRefcon)
{
	OSErr err = eventNotHandledErr;
	LgiTrace("TestEvent called\n");
	return err;
}
#endif

int main(int Args, char **Arg)
{
	int Status = 0;
	OsAppArguments AppArgs(Args, Arg);
	
	#ifdef MAC
	
		#if 0
		LgiTrace("Args=%i\n", Args);
		for (int i=0; i<Args; i++)
			LgiTrace("\t[%i]='%s'\n", i, Arg[i]);
		#endif
		
		// Setup apple event handlers
		OSStatus e = AEInstallEventHandler(	kInternetEventClass,
									kAEGetURL,
									NewAEEventHandlerUPP(TestProc),
									0,
									false);
		if (e) LgiTrace("%s:%i - AEInstallEventHandler = %i\n", _FL, e);
	
	#elif 1 && defined(__GTK_H__) && defined(_DEBUG)
	
		// This turns on fatal GKT warnings all the time... 
		// Useful for debugging.
		GArray<char*> a;
		for (int i=0; i<AppArgs.Args; i++)
			a[i] = AppArgs.Arg[i];
		a.Add("--g-fatal-warnings");
		AppArgs.Arg = &a[0];
		AppArgs.Args = a.Length();
		
	#endif
	
	if (_BuildCheck())
	{
		Status = LgiMain(AppArgs);
	}
	else LgiTrace("%s:%i - _BuildCheck failed.\n", _FL);

	return Status;
}

#endif

#if defined(LGI_MEM_DEBUG) && !defined(LGI_STATIC)

void *operator new(size_t size)
{
	return lgi_malloc(size);
}

void *operator new[](size_t size)
{
	return lgi_malloc(size);
}

void operator delete(void *p)
{
	lgi_free(p);
}

void operator delete[](void *p)
{
	lgi_free(p);
}

#endif