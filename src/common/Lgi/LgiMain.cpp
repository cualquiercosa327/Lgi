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
	char *AppBuild = "Debug";
	#else
	char *AppBuild = "Release";
	#endif
	char *LgiBuild = (char*) (LgiIsReleaseBuild() ? "Release" : "Debug");

	if (stricmp(AppBuild, LgiBuild))
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
WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int Status = 0;
	#if WIN32NATIVE
	_lgi_app_instance = hInstance;
	#endif
	if (_BuildCheck())
	{
		char16 *CL = (char16*)GetCommandLineW();

    	#if WIN32NATIVE
		OsAppArguments AppArgs;
		AppArgs.hInstance = hInstance;

		if (*CL == '\"')
		{
			AppArgs.lpCmdLine = StrchrW(CL+1, '\"');
		}
		else
		{
			AppArgs.lpCmdLine = StrchrW(CL+1, ' ');
		}
		if (AppArgs.lpCmdLine) AppArgs.lpCmdLine++;

		AppArgs.nCmdShow = nCmdShow;
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

int main(int Args, char **Arg)
{
	int Status = 0;
	if (_BuildCheck())
	{
		OsAppArguments AppArgs(Args, Arg);
		Status = LgiMain(AppArgs);
	}

	return Status;
}

#endif

#ifdef LGI_MEM_DEBUG

void *operator new(unsigned int size)
{
	return lgi_malloc(size);
}

void *operator new[](unsigned int size)
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