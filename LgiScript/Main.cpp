#include "Lgi.h"
#include "GScripting.h"
#include "../src/common/Coding/GScriptingPriv.h"
#include "GStringClass.h"
#include "LgiRes.h"

struct ConsoleLog : public GStream
{
	int Write(const void *Ptr, int Size, int Flags)
	{
		return printf("%.*s", Size, Ptr);
	}
};

class App : public GApp, public GScriptContext
{
	GScriptEngine *Engine;
	GAutoString SrcFile;
	ConsoleLog Log;
	
public:
	int Status;

	App(OsAppArguments &AppArgs) : GApp(AppArgs, "LgiScript")
	{
		GFile::Path p(LSP_APP_INSTALL);
		p += "Resources";
		p += "LgiScript.lr8";
		LgiGetResObj(true, p);	
	
		Engine = NULL;
		Status = 0;
	}

	GHostFunc *GetCommands() { return NULL; }
	void SetEngine(GScriptEngine *Eng) { Engine = Eng; }

	void OnReceiveFiles(GArray<char*> &Files)
	{
		for (int i=0; i<Files.Length(); i++)
		{
			if (!Run(Files[i]))
			{
				Status = -1;
			}
		}
	}
	
	char *GetIncludeFile(char *FileName)
	{
		char p[MAX_PATH];
		LgiMakePath(p, sizeof(p), SrcFile, "..");
		LgiMakePath(p, sizeof(p), p, FileName);
		if (FileExists(p))
		{
			return ::ReadTextFile(p);
		}
		
		return NULL;
	}
	
	bool Run(const char *File)
	{
		bool Disassemble = LgiApp->GetOption("disassemble");
		if (!FileExists(File))
		{
			printf("Error: '%s' not found.\n", File);
			return false;
		}
		
		if (!SrcFile.Reset(NewStr(File)))
		{
			printf("Error: Mem alloc failed.\n");
			return false;
		}
		
		GScriptEngine Eng(NULL, NULL, NULL);
		Eng.SetConsole(&Log);

		GAutoString Src(::ReadTextFile(SrcFile));
		if (!Src)
		{
			printf("Error: Failed to read '%s'.\n", SrcFile.Get());
			return false;
		}
		
		GAutoPtr<GCompiledCode> Obj;
		if (!Eng.Compile(Obj, NULL, Src, File))
		{
			printf("Error: Compilation failed '%s'.\n", SrcFile.Get());
			return false;
		}
		
		GVariant Ret;
		GExecutionStatus s = Eng.Run(Obj, &Ret);
		if (s == ScriptError)
		{
			printf("Error: Execution failed '%s'.\n", SrcFile.Get());
			return false;
		}
		else if (s == ScriptWarning)
		{
			printf("Warning: Execution succeeded with warnings '%s'.\n", SrcFile.Get());
			return false;
		}
		
		if (Ret.CastInt32())
			printf("Success: %s\n", File);
		else
		{
			printf("Failed: %s\n", File);
			s = ScriptError;
		}
		
		if (Disassemble)
		{
			GString f = File;
			int Idx = f.RFind(".");
			if (Idx > 0)
			{
				f = f(0, Idx) + ".asm";
				GAutoString a(ReadTextFile(f));
				if (a)
				{
					printf("%s\n", a.Get());
				}
			}
		}
		
		return s == ScriptSuccess;
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	App a(AppArgs);
	if (a.IsOk())
	{
		a.OnCommandLine();
	}
	return a.Status;
}