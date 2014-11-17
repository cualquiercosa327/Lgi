#include "Lgi.h"
#include "GDebugger.h"
#include "GSubProcess.h"

class Gdb : public GDebugger, public GThread
{
	GDebugEvents *Events;
	GAutoPtr<GSubProcess> Sp;
	GAutoString Exe, Args, InitDir;
	bool Running;
	bool AtPrompt;
	char Line[256], *LinePtr;
	
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
			
			Events->Write(Buf, Ch);
		}
	}
	
	void OnRead(const char *Ptr, int Bytes)
	{
		Events->Write(Ptr, Bytes);
		LgiTrace("Wrote %i bytes to log.\n", Bytes);
		
		// Check for prompt
		const char *End = Ptr + Bytes;
		char *LineEnd = Line + sizeof(Line) - 1;
		while (Ptr < End)
		{
			if (*Ptr == '\n')
			{
				LinePtr = Line;
			}
			else if (LinePtr < LineEnd)
			{
				*LinePtr++ = *Ptr;
			}

			Ptr++;
		}
		*LinePtr = 0;
		
		AtPrompt = !_stricmp(Line, "(gdb) ");
	}
		
	int Main()
	{
		char s[MAX_PATH];
		const char *Shell = "C:\\Windows\\System32\\cmd.exe";
		const char *Path = "C:\\MinGW\\bin\\gdb.exe";
		sprintf_s(s, sizeof(s), "\"%s\"", Exe.Get());		
		if (!Sp.Reset(new GSubProcess(Path, Args)))
			return false;

		// Log("Starting: %s %s\n", Path, s);
		
		if (InitDir)
			Sp->SetInitFolder(InitDir);
		
		if (!Sp->Start(true, true))
			return -1;
			
		State = Looping;
		char Buf[512];
		while (State == Looping && Sp->IsRunning())
		{
			#if 1
			#ifdef _DEBUG
			ZeroObj(Buf);
			#endif
			int Rd = Sp->Read(Buf, sizeof(Buf));
			if (Rd > 0)
			{
				OnRead(Buf, Rd);
			}
			#else
			int Pk = Sp->Peek();
			if (Pk > 0)
			{
				int Rd = Sp->Read(Buf, sizeof(Buf));
				if (Rd > 0)
				{
					Events->Write(Buf, Rd);
				}
			}
			else
			{			
				LgiSleep(20);
			}
			#endif
		}

		Log("Debugger exited.\n");
		return 0;
	}

	bool WaitPrompt()
	{
		if (State == Init)
		{
			uint64 Start = LgiCurrentTime();
			while (State == Init /*&& LgiCurrentTime() - Start < 2000*/)
			{
				LgiSleep(1);
			}
			/*
			if (State == Init)
			{
				LgiAssert(0);
				return false;
			}
			*/
			
			LgiTrace("Waited %I64ims for Looping state...\n", LgiCurrentTime() - Start);
		}

		uint64 Start = LgiCurrentTime();
		while (!AtPrompt /*&& LgiCurrentTime() - Start < 2000*/)
		{
			LgiSleep(1);
		}

		/*
		if (!AtPrompt)
		{
			LgiAssert(0);
			return false;
		}
		*/

		LgiTrace("Waited %I64ims for AtPrompt...\n", LgiCurrentTime() - Start);
		return true;
	}
	
	bool Cmd(const char *c)
	{
		if (!WaitPrompt())
			return false;

		char str[256];
		int ch = sprintf_s(str, sizeof(str), "%s\n", c);
		Events->Write(str, ch);
		int Wr = Sp->Write(str, ch);
		
		return Wr == ch;
	}
	
public:
	Gdb() : GThread("Gdb")
	{
		Events = NULL;
		State = Init;
		AtPrompt = false;
		LinePtr = Line;
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

	bool UserCommand(const char *cmd)
	{
		char c[256];
		sprintf_s(c, sizeof(c), "%s\n", cmd);
		return Cmd(c);
	}
};

GDebugger *CreateGdbDebugger()
{
	return new Gdb;
}
