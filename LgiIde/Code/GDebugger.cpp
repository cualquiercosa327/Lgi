#include "Lgi.h"
#include "GDebugger.h"
#include "GSubProcess.h"
#include "GToken.h"

const char sPrompt[] = "(gdb) ";

class Gdb : public GDebugger, public GThread
{
	GDebugEvents *Events;
	GAutoPtr<GSubProcess> Sp;
	GAutoString Exe, Args, InitDir;
	bool Running;
	bool AtPrompt;
	char Line[256], *LinePtr;
	GStream *OutStream;
	int CurFrame;
	
	enum ThreadState
	{
		Init,
		Looping,
		Exiting,
	}	State;

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
	
	void OnLine()
	{
		if (stristr(Line, "received signal SIGSEGV"))
		{
			Events->OnCrash(0);
		}
	}
	
	void OnRead(const char *Ptr, int Bytes)
	{
		if (OutStream)
			OutStream->Write(Ptr, Bytes);
		else
			Events->Write(Ptr, Bytes);
		
		// Check for prompt
		const char *End = Ptr + Bytes;
		char *LineEnd = Line + sizeof(Line) - 1;
		while (Ptr < End)
		{
			if (*Ptr == '\n')
			{
				*LinePtr = 0;
				OnLine();
				LinePtr = Line;
			}
			else if (LinePtr < LineEnd)
			{
				*LinePtr++ = *Ptr;
			}

			Ptr++;
		}
		*LinePtr = 0;

		int Ch = LinePtr - Line;
		int Offset = max(Ch - 6, 0);
		AtPrompt = !_stricmp(Line + Offset, sPrompt);
		if (AtPrompt)
		{
			if (Running ^ !AtPrompt)
			{
				Running = !AtPrompt;
				Events->OnRunState(Running);
			}
		}
	}
	
	int Main()
	{
		char s[MAX_PATH];
		const char *Shell = "C:\\Windows\\System32\\cmd.exe";
		const char *Path = "C:\\MinGW\\bin\\gdb.exe";
		if (ValidStr(Args))
			sprintf_s(s, sizeof(s), "\"%s\"", Exe.Get());
		else
			sprintf_s(s, sizeof(s), "\"%s\" -- %s", Exe.Get(), Args.Get());
		
		if (!Sp.Reset(new GSubProcess(Path, s)))
			return false;

		// Log("Starting: %s %s\n", Path, s);
		
		if (InitDir)
			Sp->SetInitFolder(InitDir);
		
		if (!Sp->Start(true, true))
			return -1;
		
		State = Looping;
		char Buf[513];
		while (State == Looping && Sp->IsRunning())
		{
			#ifdef _DEBUG
			ZeroObj(Buf);
			#endif
			int Rd = Sp->Read(Buf, sizeof(Buf)-1);
			if (Rd > 0)
			{
				OnRead(Buf, Rd);
				#ifdef _DEBUG
				// LgiTrace("Read(%i)='%.*s'\n", Rd, Rd, Buf);
				#endif
			}			
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
		while (!AtPrompt && LgiCurrentTime() - Start < 2000)
		{
			LgiSleep(1);
		}

		if (!AtPrompt)
		{
			Log("Error: Not at prompt...\n");
			return false;
		}

		LgiTrace("Waited %I64ims for AtPrompt...\n", LgiCurrentTime() - Start);
		return true;
	}
	
	bool Cmd(const char *c, GStream *Output = NULL)
	{
		if (!WaitPrompt())
			return false;

		char str[256];
		int ch = sprintf_s(str, sizeof(str), "%s\n", c);
		Events->Write(str, ch);

		OutStream = Output;

		int Wr = Sp->Write(str, ch);
		if (Wr != ch)
			return false;

		if (Output)
		{		
			AtPrompt = false;
			WaitPrompt();
			OutStream = NULL;
		}
		
		return true;
	}
	
public:
	Gdb() : GThread("Gdb")
	{
		Events = NULL;
		State = Init;
		AtPrompt = false;
		LinePtr = Line;
		OutStream = NULL;
		CurFrame = 0;
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

	bool GetCallStack(GArray<GAutoString> &Stack)
	{
		GStringPipe Bt;
		if (!Cmd("bt", &Bt))
			return false;

		GAutoString a(Bt.NewStr());
		GToken Lines(a, "\r\n");
		for (int i=0; i<Lines.Length(); i++)
		{
			char *l = Lines[i];
			if (stristr(l, sPrompt))
				Events->Write(l, strlen(l));
			else if (*l == '#')
				Stack.New().Reset(NewStr(l));
			else if (Stack.Length() > 0)
			{
				// Append to the last line..
				GAutoString &Prev = Stack.Last();
				char *End = Prev + strlen(Prev);
				while (End > Prev && strchr(WhiteSpace, End[-1]))
					*(--End) = 0;
				char s[512];
				sprintf_s(s, sizeof(s), "%s%s", Prev.Get(), l);
				Prev.Reset(NewStr(s));
			}
		}
		
		return true;
	}

	bool GetFrame(int &Frame, GAutoString &File, int &Line)
	{
		LgiAssert(0);
		return false;
	}
	
	bool SetFrame(int Frame)
	{
		if (CurFrame != Frame)
		{
			CurFrame = Frame;
			
			char c[256];
			sprintf_s(c, sizeof(c), "frame %i", Frame);
			return Cmd(c);
		}
		
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
	
	void ParseVariables(const char *a, GArray<Variable> &vars, GDebugger::Variable::ScopeType scope, bool Detailed)
	{
		GToken t(a, "\r\n");
		for (int i=0; i<t.Length(); i++)
		{
			char *line = t[i];
			char *val = strchr(line, '=');
			if (val)
			{
				*val++ = 0;
				while (*val && strchr(WhiteSpace, *val)) val++;
				
				Variable &v = vars.New();
				v.Scope = scope;
				v.Name.Reset(TrimStr(line));
				if (!strnicmp(val, "0x", 2))
				{
					v.Value.Type = GV_VOID_PTR;
					v.Value.Value.Ptr = (void*) htoi(val);
				}
				else if (IsDigit(*val))
				{
					int64 tmp = atoi64(val);
					if (tmp & 0xffffffff00000000L)
						v.Value = tmp;
					else
						v.Value = (int)tmp;
				}
				else
				{
					v.Value.OwnStr(TrimStr(val));
				}
				
				if (Detailed)
				{
					GStringPipe p;
					char c[256];
					sprintf_s(c, sizeof(c), "p %s", v.Name.Get());
					if (Cmd(c, &p))
					{
						GAutoString tmp(p.NewStr());
						for (char *s = tmp; s && *s; )
						{
							if (*s == '\"')
							{
								char *e = strchr(++s, '\"');
								if (!e)
									break;
								
								v.Value.OwnStr(NewStr(s, e - s));
								break;
							}
							else if (*s == '(' && !v.Type)
							{
								char *e = strchr(++s, ')');
								if (!e)
									break;

								if (strnicmp(s, "gdb", 3))
									v.Type.Reset(NewStr(s, e - s));
								s = e + 1;
								continue;
							}						
							
							s = LgiSkipDelim(s, WhiteSpace, true);
							s = LgiSkipDelim(s, WhiteSpace);						
						}
					}
				}
			}
		}
	}
	
	bool GetVariables(bool Locals, GArray<Variable> &vars, bool Detailed)
	{
		GStringPipe p(256);

		if (!Cmd("info args", &p))
			return false;

		GAutoString a(p.NewStr());
		ParseVariables(a, vars, Variable::Arg, Detailed);

		if (!Cmd("info locals", &p))
			return false;
		
		a.Reset(p.NewStr());
		ParseVariables(a, vars, Variable::Local, Detailed);
		return true;
	}

	bool PrintObject(const char *Var, GStream *Output)
	{
		if (!Var || !Output)
			return false;
	
		char c[256];
		sprintf_s(c, sizeof(c), "p *%s", Var);
		
		GMemQueue q;
		if (!Cmd(c, &q))
			return false;
		
		GAutoString a((char*)q.New(1));
		if (!a)
			return false;
			
		int Depth = 0;
		char *Start = NULL;
		char Spaces[256];
		memset(Spaces, ' ', sizeof(Spaces));
		int IndentShift = 2;

		#define Emit() \
			if (Start) \
			{ \
				int bytes = s - Start; \
				char *last = s-1; while (last > Start && strchr(WhiteSpace, *last)) last--; \
				Output->Print("%.*s%.*s%s\n", Depth<<IndentShift, Spaces, bytes, Start, *last == '=' ? "" : ";"); \
				Start = NULL; \
			}
		
		for (char *s = a; *s; s++)
		{
			if (*s == '{')
			{
				Emit();
				Output->Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
				Depth++;
			}
			else if (*s == '}')
			{
				Emit();
				Depth--;
				Output->Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
			}
			else if (*s == ',')
			{
				Emit();
			}
			else if (!strchr(WhiteSpace, *s))
			{
				if (Start == NULL)
					Start = s;
			}
		}
	
		return true;
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
		sprintf_s(c, sizeof(c), "%s", cmd);
		return Cmd(c);
	}
};

GDebugger *CreateGdbDebugger()
{
	return new Gdb;
}
