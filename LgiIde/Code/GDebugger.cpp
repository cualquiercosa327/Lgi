#include "Lgi.h"
#include "GDebugger.h"
#include "GSubProcess.h"

class Gdb : public GDebugger, public GThread
{
	GDebugEvents *Events;
	GAutoPtr<GSubProcess> Sp;
	GAutoString Exe, Args, InitDir;
	bool Running;
	
	enum ThreadState
	{
		Init,
		Looping,
		Exiting,
	} State;

	void Log(const char *Fmt, ...)
	{
		if (Events)
		{
			va_list Arg;
			va_start(Arg, Fmt);
			char Buf[512];
			int Ch = vsprintf_s(Buf, sizeof(Buf), Fmt, Arg);
			va_end(Arg);
			
			Events->OnDebugLog(Buf, Ch);
		}
	}
		
	int Main()
	{
		char s[MAX_PATH];
		const char *Shell = "C:\\Windows\\System32\\cmd.exe";
		const char *Path = "C:\\MinGW\\bin\\gdb.exe";
		sprintf_s(s, sizeof(s), "\"%s\"", Exe.Get());		
		if (!Sp.Reset(new GSubProcess(Path, Args)))
			return false;

		Log("Starting: %s %s\n", Path, s);
		
		if (InitDir)
			Sp->SetInitFolder(InitDir);
		
		if (!Sp->Start(true, true))
			return -1;
			
		State = Looping;
		char Buf[256];
		while (State == Looping && Sp->IsRunning())
		{
			int Pk = Sp->Peek();
			if (Pk > 0)
			{
				int Rd = Sp->Read(Buf, sizeof(Buf));
				if (Rd > 0)
				{
					Events->OnDebugLog(Buf, Rd);
				}
			}
			else
			{			
				LgiSleep(20);
			}
		}

		Log("Debugger exited.\n");
		return 0;
	}
	
	bool Cmd(const char *c)
	{
		if (State == Init)
		{
			uint64 Start = LgiCurrentTime();
			while (State == Init && LgiCurrentTime() - Start < 2000)
			{
				LgiSleep(10);				
			}
			
			if (!Sp)
			{
				LgiAssert(0);
				return false;
			}
		}

		char str[256];
		int ch = sprintf_s(str, sizeof(str), "%s\r", c);
		int Wr = Sp->Write(str, ch);
		if (Wr > 0)
		{
			Events->OnDebugLog(str, Wr);
		}
		
		return Wr == ch;
	}
	
public:
	Gdb() : GThread("Gdb")
	{
		Events = NULL;
		State = Init;
	}
	
	~Gdb()
	{
		if (State == Looping)
		{
			State = Exiting;
			while (!IsExited())
			{
				LgiSleep(10);
			}
		}
	}

	bool Load(GDebugEvents *EventHandler, const char *exe, const char *args, const char *initDir)
	{
		Events = EventHandler;
		Exe.Reset(NewStr(exe));
		Args.Reset(NewStr(args));
		InitDir.Reset(NewStr(initDir));
		Running = false;
		Run();
		return true;
	}

	bool Restart()
	{
		Cmd("r");
		return false;
	}

	bool Unload()
	{
		Cmd("q");
		return false;
	}
	
	bool GetRunning()
	{
		return Running;
	}
	
	bool SetRuning(bool Run)
	{
		if (Run)
		{
			if (Cmd("r"))
			{
				Running = true;
				return true;
			}
		}
		else
		{
			if (Break())
			{
				return true;
			}
		}
		
		return false;
	}
	
	bool SetBreakPoint(BreakPoint *bp)
	{
		LgiAssert(0);
		return false;
	}
	
	bool GetBreakPoints(GArray<BreakPoint> &bps)
	{
		LgiAssert(0);
		return false;
	}
	
	bool GetVariables(bool Locals, GArray<Variable> &vars)
	{
		LgiAssert(0);
		return false;
	}
	
	bool GetLocation(GAutoString &File, int &Line)
	{
		LgiAssert(0);
		return false;
	}
	
	bool SetLocation(const char *File, int Line)
	{
		LgiAssert(0);
		return false;
	}

	bool StepInto()
	{
		return Cmd("step");
	}

	bool StepOver()
	{
		return Cmd("next");
	}

	bool StepOut()
	{
		return Cmd("finish");
	}

	bool Break()
	{
		if (Cmd("\x03"))
		{
			Running = false;
			return true;
		}
		return false;
	}

	bool UserCommand(const char *Cmd)
	{
		LgiAssert(0);
		return false;
	}
};

GDebugger *CreateGdbDebugger()
{
	return new Gdb;
}
