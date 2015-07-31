#include "Lgi.h"
#include "GDebugger.h"
#include "GSubProcess.h"
#include "GToken.h"
#ifdef POSIX
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#endif

#define DEBUG_SHOW_GDB_IO		0

const char sPrompt[] = "(gdb) ";

class Gdb : public GDebugger, public GThread
{
	typedef GArray<GAutoString> StrArray;

	GDebugEvents *Events;
	GAutoPtr<GSubProcess> Sp;
	GAutoString Exe, Args, InitDir;
	bool DebuggingProcess;
	bool Running;
	bool AtPrompt;
	char Line[256], *LinePtr;
	int CurFrame;
	bool SetAsmType;
	bool SetPendingOn;
	GArray<BreakPoint> BreakPoints;
	int BreakPointIdx;
	int ProcessId;

	// Current location tracking
	GString CurFile;
	int CurLine;
	StrArray Untagged;

	// Various output modes.
	GStream *OutStream;
	StrArray *OutLines;
	
	enum ThreadState
	{
		Init,
		Looping,
		Exiting,
		ProcessError
	}	State;

	bool ParseLocation(StrArray &p)
	{
		for (int i=0; i<p.Length(); i++)
		{
			GString s(p[i]);
			GString::Array a(s.SplitDelimit(WhiteSpace));
			if (a.Length() > 0)
			{
				int At = 0;
				for (; At < a.Length() && stricmp(a[At], "at") != 0; At++)
					;
				
				if (At < a.Length() - 1) // Found the 'at'
				{
					GString::Array ref = a[At+1].Split(":");
					if (ref.Length() == 2)
					{
						Events->OnFileLine(NativePath(ref[0]), ref[1].Int(), true);
						return true;
					}
				}
				else
				{
					int Line = a[0].Int();
					if (Line)
					{
						Events->OnFileLine(NULL, Line, true);
						return true;
					}
				}
			}
		}
		return false;
	}
	
	void SetRunState(bool r)
	{
		if (r != Running)
		{
			Running = r;
			Events->OnState(DebuggingProcess, Running);
		}
	}

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
	
	void OnExit()
	{
		DebuggingProcess = false;
		SetRunState(false);
	}
	
	char *NativePath(char *p)
	{
		for (char *c = p; *c; c++)
		{
			if (*c == '/' || *c == '\\')
				*c = DIR_CHAR;
		}
		return p;
	}
	
	void OnBreakPoint(const char *Str, int Len)
	{
		// Trim off the whitespace...
		while (Len > 0 && strchr(WhiteSpace, *Str))
		{
			Str++;
			Len--;
		}
		while (Len > 0 && strchr(WhiteSpace, Str[Len-1]))
		{
			Len--;
		}

		GString f(Str, Len);
		GString::Array a = f.SplitDelimit(" \t");
		int At = 0;

		// Find 'at'
		for (; At < a.Length() && stricmp(a[At], "at") != 0; At++)
			;
		
		if (At < a.Length() - 1) // Found the 'at'
		{
			GString::Array ref = a[At+1].SplitDelimit(":");
			if (ref.Length() == 2)
			{
				Events->OnFileLine(NativePath(ref[0]), ref[1].Int(), true);
			}
			else LgiTrace("%s:%i - Not the right arg count.\n", _FL);
			
			BreakPointIdx = -1;
		}
		else LgiTrace("Not end of breakpoint.\n");
	}
	
	void OnLine(const char *Start, int Length)
	{
		#if DEBUG_SHOW_GDB_IO
		LgiTrace("Receive: '%.*s'\n", Length-1, Start);
		#endif

		// Send output
		if (OutLines)
			OutLines->New().Reset(NewStr(Start, Length - 1));
		else if (OutStream)
			OutStream->Write(Start, Length);
		else
			Untagged.New().Reset(NewStr(Start, Length));

		Events->Write(Start, Length);

		if (BreakPointIdx > 0)
		{
			OnBreakPoint(Start, Length);
		}
		else if (stristr(Start, "received signal SIGSEGV"))
		{
			Events->OnCrash(0);
		}
		else if (*Start == '[')
		{
			if (stristr(Start, "Inferior") && stristr(Start, "exited"))
			{
				OnExit();
			}
			else if (stristr(Start, "New Thread"))
			{
				GString s(Start, Length);
				GString::Array a = s.SplitDelimit("[] ()");
				int ThreadId = -1;
				for (unsigned i=0; i<a.Length(); i++)
				{
					if (a[i] == GString("LWP") && i < a.Length() - 1)
					{
						ThreadId = a[i+1].Int();
						break;
					}
				}
				if (ThreadId > 0)
				{
					// Ok so whats the process ID?
					#ifdef POSIX
					int Pid = Gtk::getpgid(ThreadId);
					if (Pid > 0 && ProcessId < 0)
					{
						LgiTrace("Got the thread id: %i, and pid: %i\n", ThreadId, Pid);
						ProcessId = Pid;
					}
					else
						printf("Not setting pid: %i && %i\n", Pid > 0, ProcessId < 0);
					#else
					LgiAssert(!"Impl me.");
					#endif
				}
				else LgiTrace("%s:%i - No thread id?\n", _FL);
			}
		}
		else if (BreakPointIdx < 0 &&
				strncmp(Start, "Breakpoint ", 11) == 0 &&
				IsDigit(Start[11]))
		{
			Start += 11;
			Length -= 11;
			BreakPointIdx = atoi(Start);
			OnBreakPoint(Start, Length);
		}
		else
		{
			// Untagged file/line?
			if (ParseLocation(Untagged))
			{
				Untagged.Length(0);
			}
		}
	}
	
	void OnRead(const char *Ptr, int Bytes)
	{
		// Parse output into lines
		const char *p = Ptr;
		const char *End = p + Bytes;
		char *LineEnd = Line + sizeof(Line) - 2;
		while (p < End)
		{
			if (*p == '\n')
			{
				*LinePtr++ = *p;
				*LinePtr = 0;
				OnLine(Line, LinePtr - Line);
				LinePtr = Line;
			}
			else if (LinePtr < LineEnd)
			{
				*LinePtr++ = *p;
			}

			p++;
		}
		*LinePtr = 0;

		// Check for prompt
		int bytes = LinePtr - Line;
		if (bytes > 0)
		{
			if (bytes == 6)
			{
				AtPrompt = !_strnicmp(Line, sPrompt, bytes);
				// LgiTrace("%I64i: AtPrompt=%i\n", LgiCurrentTime(), AtPrompt);
				if (AtPrompt)
				{
					if (Running ^ !AtPrompt)
					{
						SetRunState(!AtPrompt);
					}
					
					if (OutStream)
						OutStream = NULL;
					if (OutLines)
						OutLines = NULL;

					Events->Write(Line, bytes);
				}
			}
		}
	}
	
	int Main()
	{
		char s[MAX_PATH];
		#ifdef WIN32
		const char *Shell = "C:\\Windows\\System32\\cmd.exe";
		const char *Path = "C:\\MinGW\\bin\\gdb.exe";
		#else
		const char *Path = "gdb";
		#endif
		if (ValidStr(Args))
			sprintf_s(s, sizeof(s), "\"%s\" -- %s", Exe.Get(), Args.Get());
		else
			sprintf_s(s, sizeof(s), "\"%s\"", Exe.Get());
		
		if (!Sp.Reset(new GSubProcess(Path, s)))
			return false;

		if (InitDir)
			Sp->SetInitFolder(InitDir);
		
		LgiTrace("Starting gdb subprocess...\n");
		if (!Sp->Start(true, true, false))
		{
			State = ProcessError;
			GAutoString ErrMsg = LgiErrorCodeToString(Sp->GetErrorCode());
			char s[256];
			sprintf_s(s, sizeof(s), "Failed to start gdb, error: 0x%x (%s)\n", Sp->GetErrorCode(), ErrMsg.Get());
			Events->OnError(Sp->GetErrorCode(), s);
			return -1;
		}

		LgiTrace("Entering gdb loop...\n");
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
				#ifdef _DEBUG
				// LgiTrace("%I64i: %p,%p Read(%i)='%.*s'\n", LgiCurrentTime(), OutLines, OutStream, Rd, Rd, Buf);
				#endif
				OnRead(Buf, Rd);
			}			
		}

		LgiTrace("Exiting gdb loop...\n");
		if (Events)
		{
			DebuggingProcess = false;
			Running = true;
			SetRunState(false);
		}

		Log("Debugger exited.\n");
		return 0;
	}

	bool WaitPrompt()
	{
		if (State == Init)
		{
			uint64 Start = LgiCurrentTime();
			while (State == Init)
			{
				uint64 Now = LgiCurrentTime();
				if (Now - Start < 5000)
				{
					LgiSleep(10);
				}
				else
				{
					LgiTrace("%s:%i - WaitPrompt init wait failed.\n", _FL);
					return false;
				}
			}
		}

		uint64 Start = LgiCurrentTime();
		while (!AtPrompt &&
				LgiCurrentTime() - Start < 2000 &&
				State == Looping)
		{
			LgiSleep(1);
		}

		if (!AtPrompt)
		{
			Log("Error: Not at prompt...\n");
			return false;
		}

		return true;
	}
	
	bool Cmd(const char *c, GStream *Output = NULL, StrArray *Arr = NULL)
	{
		if (!ValidStr(c))
		{
			LgiTrace("%s:%i - Not a valid command.\n", _FL);
			LgiAssert(!"Not a valid command.");
			return false;
		}
		
		if (!WaitPrompt())
		{
			return false;
		}

		char str[256];
		int ch = sprintf_s(str, sizeof(str), "%s\n", c);

		#if DEBUG_SHOW_GDB_IO
		LgiTrace("Send: '%s'\n", c);
		#endif
		Events->Write(str, ch);
		LinePtr = Line;
		OutStream = Output;
		OutLines = Arr;		
		AtPrompt = false;

		uint64 Start = LgiCurrentTime();
		int Wr = Sp->Write(str, ch);
		if (Wr != ch)
			return false;

		if (OutStream || OutLines)
		{	
			/*	
			uint64 Wait0 = LgiCurrentTime();
			*/
			WaitPrompt();
			/*
			uint64 Wait1 = LgiCurrentTime();
			LgiTrace("Cmd timing "LGI_PrintfInt64" "LGI_PrintfInt64"\n", Wait0-Start, Wait1-Wait0);
			*/
			LgiAssert(OutStream == NULL && OutLines == NULL);
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
		OutLines = NULL;
		CurFrame = 0;
		SetAsmType = false;
		SetPendingOn = false;
		DebuggingProcess = false;
		BreakPointIdx = -1;
		ProcessId = -1;
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
		StrArray Bt;
		if (!Cmd("bt", NULL, &Bt))
			return false;

		for (int i=0; i<Bt.Length(); i++)
		{
			char *l = Bt[i];
			if (*l == '#')
			{
				Stack.New().Reset(NewStr(l));
			}
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
		if (Running)
			Break();
		
		GString a;
		if (Args)
			a.Printf("r %s", Args.Get());
		else if (Args)
			a = "r";

		bool Status = Cmd(a);
		if (Status)
		{
			DebuggingProcess = true;
			Running = false;
			SetRunState(true);
		}
		
		return Status;
	}

	bool Unload()
	{
		Cmd("q");
		if (DebuggingProcess)
		{
			DebuggingProcess = false;
			Running = true;
			SetRunState(false);
		}
		return false;
	}
	
	bool GetRunning()
	{
		return Running;
	}
	
	bool SetRunning(bool Run)
	{
		if (Run)
		{
			if (!SetAsmType)
			{
				SetAsmType = true;
				Cmd("set disassembly-flavor intel");
			}

			GString a;
			if (DebuggingProcess)
				a = "c";
			else if (Args)
				a.Printf("r %s", Args.Get());
			else
				a = "r";

			if (Cmd(a))
			{
				DebuggingProcess = true;
				SetRunState(true);
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

	bool AddBp(BreakPoint &bp)
	{
		bool Ret = false;
		if (!bp.Added)
		{
			if (!SetPendingOn)
			{
				Cmd("set breakpoint pending on");
				SetPendingOn = true;
			}
			
			char cmd[MAX_PATH];
			char *File = bp.File.Get();
			char *Last = strrchr(File, DIR_CHAR);
			sprintf_s(cmd, sizeof(cmd), "break %s:%i", Last ? Last + 1 : File, bp.Line);
			
			BreakPointIdx = 0;
			
			StrArray Lines;
			Ret = Cmd(cmd, NULL, &Lines);
			WaitPrompt();
			
			for (unsigned i=0; i<Lines.Length(); i++)
			{
				GString s;
				s = Lines[i];
				GString::Array p = s.Split(" ");
				if (p.Length() >= 2 &&
					!_stricmp(p[0], "breakpoint"))
				{
					int Idx = p[1].Int();
					if (Idx)
					{
						bp.Index = Idx;
					}
				}
			}
			
			BreakPointIdx = -1;
			if (Ret)
				bp.Added = true;
		}
		return Ret;
	}
	
	bool SetBreakPoint(BreakPoint *bp)
	{
		if (!bp)
		{
			LgiTrace("%s:%i - SetBreakPoint failed, param error.\n", _FL);
			return false;
		}
		
		// Make sure the child 'gdb' is running...
		uint64 Start = LgiCurrentTime();
		while (State == Init)
		{
			LgiSleep(5);
			if (LgiCurrentTime()-Start > 3000)
			{
				LgiTrace("%s:%i - SetBreakPoint init wait failed...\n", _FL);
				return false;
			}
		}

		bp->Added = false;
		if (Running)
		{
			LgiTrace("%s:%i - Can't add break point while running.\n", _FL);
			return false;
		}
		else
		{
			if (AddBp(*bp))
			{
				BreakPoint &n = BreakPoints.New();
				n = *bp;
			}
		}
		
		return true;
	}

	bool RemoveBreakPoint(BreakPoint *bp)
	{
		if (!bp)
			return false;

		// Make sure the child 'gdb' is running...
		uint64 Start = LgiCurrentTime();
		while (State == Init)
		{
			LgiSleep(5);
			if (LgiCurrentTime()-Start > 3000)
			{
				LgiTrace("%s:%i - SetBreakPoint init wait failed...\n", _FL);
				return false;
			}
		}

		if (Running)
		{
			LgiTrace("%s:%i - Can't add break point while running.\n", _FL);
			return false;
		}
		else
		{
			unsigned i;
			for (i=0; i<BreakPoints.Length(); i++)
			{
				if (*bp == BreakPoints[i])
				{
					break;
				}
			}
			
			if (i < BreakPoints.Length())
			{
				char c[256];
				sprintf_s(c, sizeof(c), "delete %i", BreakPoints[i].Index);
				bool Status = Cmd(c);
				if (Status)
				{				
					BreakPoints.DeleteAt(i);
				}
				else
				{
					LgiTrace("%s:%i - The cmd '%s' failed.\n", _FL, c);
					return false;
				}
			}
			else
			{
				LgiTrace("%s:%i - Failed to find break point '%s:%i'\n",
					_FL,
					bp->File.Get(),
					bp->Line);
				return false;
			}
		}
		
		return true;
	}
	
	bool GetBreakPoints(GArray<BreakPoint> &bps)
	{
		bps = BreakPoints;
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

	bool GetRegisters(GStream *Out)
	{
		if (!Out)
			return false;
		
		return Cmd("info registers", Out);
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

	bool ReadMemory(NativeInt Addr, int Length, GArray<uint8> &OutBuf)
	{
		StrArray Out;
		char c[256];
		int words = Length >> 2;
		int bytes = Length % 4;
		sprintf_s(c, sizeof(c), "x/%iw 0x%p", words, (void*)Addr);
		if (!Cmd(c, NULL, &Out))
			return false;
		
		if (!OutBuf.Length(words << 2))
			return false;
		
		uint32 *buf = (uint32*) &(OutBuf)[0];
		uint32 *ptr = buf;
		uint32 *end = ptr + (OutBuf.Length() / sizeof(*buf));
		
		for (int i=0; i<Out.Length() && ptr < end; i++)
		{
			char *l = Out[i];
			char *s = strchr(l, ':');
			if (!s)
				continue;
			
			s++;
			while (*s)
			{
				while (*s && strchr(WhiteSpace, *s)) s++;
				uint32 word = atoi(s);
				*ptr++ = word;
				while (*s && !strchr(WhiteSpace, *s)) s++;
				
				if (ptr >= end)
					break;
			}
		}
		
		OutBuf.Length((ptr - buf) << 2);		
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
		bool Status = Cmd("step");
		if (Status)
			SetRunState(true);
		return Status;
	}

	bool StepOver()
	{
		bool Status = Cmd("next");
		if (Status)
			SetRunState(true);
		return Status;
	}

	bool StepOut()
	{
		bool Status = Cmd("finish");
		if (Status)
			SetRunState(true);
		return Status;
	}

	bool Break()
	{
		#ifdef POSIX
		if (ProcessId < 0)
		{
			LgiTrace("%s:%i - No process ID (yet?).\n", _FL);
			return false;
		}
		
		LgiTrace("%s:%i - sending SIGINT to %i(0x%x)...\n", _FL, ProcessId, ProcessId);
		int result = Gtk::kill(ProcessId, SIGINT);
		if (!result)
		{
			LgiTrace("%s:%i - success... waiting prompt\n", _FL);
			return WaitPrompt();
		}
		
		LgiTrace("%s:%i - kill failed with %i(0x%x)\n", _FL, errno, errno);
		return false;
		#else
		LgiAssert(!"Impl me");
		return false;
		#endif		
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
