#include "Lgi.h"
#include "GDebugger.h"
#include "GSubProcess.h"
#include "GToken.h"
#include "GDocView.h"
#include "GStringClass.h"
#include "GString.h"
#ifdef POSIX
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#endif

#define DEBUG_SHOW_GDB_IO		0
#define ECHO_GDB_OUTPUT			0

const char sPrompt[] = "(gdb) ";

class Callback
{
public:
	virtual GString GetResponse(const char *c) = 0;
};

class Visualizer
{
public:
	virtual ~Visualizer() {}
	
	virtual bool Match(GString s) = 0;
	virtual bool Transform(GString name, GString val, Callback *Cb, GVariant &Value, GString &Detail) = 0;
};

class GStringVis : public Visualizer
{
public:
	bool Match(GString s)
	{
		return s == "GString";
	}
	
	bool Transform(GString name, GString val, Callback *Cb, GVariant &Value, GString &Detail)	
	{
		GString::Array a = val.SplitDelimit("{} \t\r\n");
		if (a.Length() == 3 &&
			a[1] == "=")
		{
			void *Ptr = (void*)htoi(a[2].Get());
			if (Ptr == NULL)
			{
				Value = "NULL";
			}
			else
			{
				GString cmd;
				cmd.Printf("p (char*)%s.Str->Str", name.Get());
				GString r = Cb->GetResponse(cmd);
				int Pos = r.Find("=");
				if (Pos >= 0)
					Value = r(Pos, r.Length()).Strip().Get();
				else
					Value = r.Get();
			}
		}
		
		return true;
	}
};

class Gdb : public GDebugger, public GThread, public Callback
{
	GDebugEvents *Events;
	GAutoPtr<GSubProcess> Sp;
	GAutoString Exe, Args, InitDir;
	bool RunAsAdmin;
	bool AtPrompt;
	char Line[256], *LinePtr;
	int CurFrame;
	bool SetAsmType;
	bool SetPendingOn;
	GArray<BreakPoint> BreakPoints;
	int BreakPointIdx;
	int ProcessId;
	bool SuppressNextFileLine;
	GArray<Visualizer*> Vis;

	GMutex StateMutex;
	bool DebuggingProcess;
	bool Running;

	// Current location tracking
	GString CurFile;
	int CurLine;
	GString::Array Untagged;

	// Parse state
	enum ParseType
	{
		ParseNone,
		ParseBreakPoint,
	}	ParseState;
	GString::Array BreakInfo;

	// Various output modes.
	GStream *OutStream;
	GString::Array *OutLines;
	
	enum ThreadState
	{
		Init,
		Looping,
		Exiting,
		ProcessError
	}	State;

	void OnFileLine(const char *File, int Line, bool CurrentIp)
	{
		if (SuppressNextFileLine)
		{
			// printf("%s:%i - SuppressNextFileLine\n", _FL);
			SuppressNextFileLine = false;
		}
		else if (Events)
		{
			if (File)
				CurFile = File;
			if (Line > 0)
				CurLine = Line;
			
			if (CurFile && CurLine > 0)
				Events->OnFileLine(CurFile, CurLine, CurrentIp);
			/*
			else
				printf("%s:%i - Error: Cur loc incomplete: %s %i.\n", _FL, CurFile.Get(), CurLine);
			*/
		}
	}

	bool ParseLocation(GString::Array &p)
	{
		for (int i=0; i<p.Length(); i++)
		{
			GString::Array a = p[i].SplitDelimit(WhiteSpace);
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
						OnFileLine(NativePath(ref[0]), ref[1].Int(), true);
						return true;
					}
				}
				else
				{
					int Line = a[0].Int();
					if (Line)
					{
						OnFileLine(NULL, Line, true);
						return true;
					}
				}
			}
		}
		return false;
	}
	
	void SetState(bool is_debug, bool is_run)
	{
		if (StateMutex.Lock(_FL))
		{
			if (is_debug != DebuggingProcess ||
				is_run != Running)
			{
				DebuggingProcess = is_debug;
				Running = is_run;
				
				StateMutex.Unlock();

				if (Events)
				{
					#if DEBUG_SESSION_LOGGING
					LgiTrace("Gdb::SetRunState(%i,%i) calling OnState...\n", is_debug, is_run);
					#endif
					Events->OnState(DebuggingProcess, Running);
					#if DEBUG_SESSION_LOGGING
					LgiTrace("Gdb::SetRunState(%i,%i) OnState returned.\n", is_debug, is_run);
					#endif
				}
			}
			else
			{
				StateMutex.Unlock();
			}
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
		SetState(false, false);
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
	
	void OnBreakPoint(GString f)
	{
		if (!f.Get() || ProcessId < 0)
		{
			// printf("Error: Param error: %s, %i (%s:%i)\n", f.Get(), ProcessId, _FL);
			return;
		}
		
		GString File, Line;
		GString::Array a = f.Split("at");
		/*
		printf("%s:%i - a.len=%i\n", _FL, a.Length());
		for (unsigned n=0; n<a.Length(); n++)
			printf("\t[%i]='%s'\n", n, a[n].Get());
		*/
		
		if (a.Length() == 2)
		{
			GString k = a[1].Strip();
			// printf("%s:%i - k='%s'\n", _FL, k.Get());
			
			if (k.Find("0x") == 0)
			{			
				GString::Array b = a[1].SplitDelimit(":,");
				// printf("%s:%i - b.len=%i\n", _FL, b.Length());
				for (unsigned i=0; i<b.Length(); i++)
				{
					GString s = b[i].Strip();
					if (s.Find("file ") == 0)
					{
						File = s(5, -1);
					}
					else if (s.Find("line ") == 0)
					{
						Line = s(5, -1);
					}
				}
			}
			else
			{
				int e = k.Find(":");
				// printf("%s:%i - e=%i\n", _FL, e);
				if (e > 0)
				{
					e++;
					while (e < k.Length() && IsDigit(k(e)))
						e++;
					// printf("%s:%i - e=%i\n", _FL, e);
					GString::Array b = k(0, e).RSplit(":", 1);
					//  printf("%s:%i - b.len=%i\n", _FL, b.Length());
					if (b.Length() == 2)
					{
						File = b[0];
						Line = b[1];
					}					
				}
				else printf("Error: no ':' in '%s'. (%s:%i)\n", k.Get(), _FL);
			}
		}
		else printf("Error: %i parts (%s:%i).\n", a.Length(), _FL);

		if (File && Line > 0)
		{
			OnFileLine(NativePath(File), Line.Int(), true);
		}
		else
		{
			printf("%s:%i - No file='%s' or line='%s'\n%s\n", _FL, File.Get(), Line.Get(), f.Get());
		}
	}
	
	void OnLine(const char *Start, int Length)
	{
		#if DEBUG_SHOW_GDB_IO
		LgiTrace("Receive: '%.*s' ParseState=%i, OutLine=%p, OutStream=%p\n", Length-1, Start, ParseState, OutLines, OutStream);
		#endif

		// Send output
		if (OutLines)
		{
			OutLines->New().Set(Start, Length - 1);
			return;
		}
		else if (OutStream)
		{
			OutStream->Write(Start, Length);
			return;
		}
		else
		{
			Untagged.New().Set(Start, Length);
			#if !ECHO_GDB_OUTPUT
			Events->Write(Start, Length);
			#endif
		}

		#if ECHO_GDB_OUTPUT
		Events->Write(Start, Length);
		#endif

		if (ParseState == ParseBreakPoint)
		{
			if (Length > 0 && IsDigit(*Start))
			{
				// printf("ParsingBp.Parse=%s\n", Start);
				GString Bp = GString(" ").Join(BreakInfo).Strip();
				OnBreakPoint(Bp);
				ParseState = ParseNone;
				BreakInfo.Length(0);
			}
			else
			{
				// printf("ParsingBp.Add=%s\n", Start);
				BreakInfo.New().Set(Start, Length);
			}
		}

		if (ParseState == ParseNone)
		{
			if (stristr(Start, "received signal SIGSEGV"))
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
						int Pid = 
						#if 0 // def __GTK_H__
						Gtk::
						#endif
						getpgid(ThreadId);
						if (Pid > 0 && ProcessId < 0)
						{
							// LgiTrace("Got the thread id: %i, and pid: %i\n", ThreadId, Pid);
							ProcessId = Pid;
						}
						/*
						else
							LgiTrace("Not setting pid: pid=%i, processid=%i\n", Pid, ProcessId);
						*/
						#else
						LgiAssert(!"Impl me.");
						#endif
					}
					else LgiTrace("%s:%i - No thread id?\n", _FL);
				}
			}
			else if (strncmp(Start, "Breakpoint ", 11) == 0 &&
					IsDigit(Start[11]))
			{
				ParseState = ParseBreakPoint;
				// printf("ParseState=%i\n", ParseState);
				BreakInfo.New().Set(Start, Length);
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
						SetState(DebuggingProcess, !AtPrompt);
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
		#ifdef WIN32
		const char *Shell = "C:\\Windows\\System32\\cmd.exe";
		const char *Path = "C:\\MinGW\\bin\\gdb.exe";
		#else
		const char *Path = "gdb";
		#endif
		GString p;
		if (RunAsAdmin)
			p.Printf("pkexec %s \"%s\"", Path, Exe.Get());
		else
			p.Printf("%s \"%s\"", Path, Exe.Get());
		GString::Array a = p.Split(" ", 1);

		if (!Sp.Reset(new GSubProcess(a[0], a[1])))
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

		#if DEBUG_SESSION_LOGGING
		LgiTrace("Gdb::Main - entering loop...\n");
		#endif
		State = Looping;

		char Buf[513];
		bool IsRun;
		while (State == Looping && (IsRun = Sp->IsRunning()))
		{
			#ifdef _DEBUG
			ZeroObj(Buf);
			#endif
			int Rd = Sp->Read(Buf, sizeof(Buf)-1, 50);
			if (Rd > 0)
			{
				#if 0 // DEBUG_SESSION_LOGGING
				printf("GDB: %.*s\n", Rd, Buf);
				#endif
				OnRead(Buf, Rd);
			}			
		}

		Break();
		Cmd("q");

		#if DEBUG_SESSION_LOGGING
		LgiTrace("Gdb::Main - exited loop.\n");
		#endif
		SetState(false, false);

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
		uint64 Now = Start;
		while (!AtPrompt &&
				Now - Start < 2000 &&
				State == Looping)
		{
			Now = LgiCurrentTime();
			LgiSleep(50);
			uint64 After = LgiCurrentTime();
			if (After - Now > 65)
			{
				printf("Sleep took=%i\n", (int)(After - Now));
			}
		}

		if (!AtPrompt)
		{
			Log("Error: Not at prompt...\n");
			return false;
		}

		return true;
	}
	
	bool Cmd(const char *c, GStream *Output = NULL, GString::Array *Arr = NULL)
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
		RunAsAdmin = false;
		OutStream = NULL;
		OutLines = NULL;
		CurFrame = 0;
		SetAsmType = false;
		SetPendingOn = false;
		DebuggingProcess = false;
		BreakPointIdx = -1;
		ProcessId = -1;
		SuppressNextFileLine = false;
		ParseState = ParseNone;
		
		Vis.Add(new GStringVis);
	}
	
	~Gdb()
	{
		if (State == Looping)
		{
			#if DEBUG_SESSION_LOGGING
			LgiTrace("Gdb::~Gdb - waiting for thread to exit...\n");
			#endif
			State = Exiting;
			while (!IsExited())
			{
				LgiSleep(1);
			}
			#if DEBUG_SESSION_LOGGING
			LgiTrace("Gdb::~Gdb - thread has exited.\n");
			#endif
		}
	}

	bool Load(GDebugEvents *EventHandler, const char *exe, const char *args, bool runAsAdmin, const char *initDir)
	{
		Events = EventHandler;
		Exe.Reset(NewStr(exe));
		Args.Reset(NewStr(args));
		RunAsAdmin = runAsAdmin;
		InitDir.Reset(NewStr(initDir));
		Running = false;
		Run();
		
		return true;
	}
	
	bool SetCurrentThread(int ThreadId)
	{
		if (ThreadId < 1)
			return false;
		
		GString c;
		c.Printf("thread %i", ThreadId);
		if (!Cmd(c))
			return false;
		
		return true;
	}

	bool GetThreads(GArray<GString> &Threads, int *pCurrentThread)
	{
		GString::Array t;
		if (!Cmd("info threads", NULL, &t))
			return false;
		
		GString *Cur = NULL;
		for (int i=0; i<t.Length(); i++)
		{
			char *l = t[i];			
			if (i == 0)
				continue;
			
			while (*l && IsWhiteSpace(*l))
				l++;
			
			bool Current = *l == '*';
			if (Current)
			{
				l++;
				while (*l && IsWhiteSpace(*l))
					l++;
			}
			
			if (IsDigit(*l))
			{
				if (Current && pCurrentThread != NULL)
				{
					int ThreadId = atoi(l);
					*pCurrentThread = ThreadId;
				}
				
				Cur = &Threads.New();
				*Cur = l;
			}
			else if (Cur)
			{
				GString s;
				s.Printf("%s %s", Cur->Get(), l);
				*Cur = s;
			}
		}
		
		return true;
	}

	bool GetCallStack(GArray<GAutoString> &Stack)
	{
		GString::Array Bt;
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
				
				GString s;
				s.Printf("%s%s", Prev.Get(), l);
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
			Break(true);
		
		GString a;
		if (Args)
			a.Printf("r %s", Args.Get());
		else if (Args)
			a = "r";

		bool Status = Cmd(a);
		if (Status)
		{
			SetState(true, false);
		}
		
		return Status;
	}

	bool Unload()
	{
		if (Running)
			Break(true);
		
		Cmd("q");
		SetState(false, false);
		State = Exiting;
		
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

			if (a(0) == 'r' && ProcessId < 0)
			{
				BreakPoint bp;
				bp.Symbol = "main";
				if (SetBreakPoint(&bp))
				{
					if (!Cmd(a))
						return false;
					
					if (!WaitPrompt())
						return false;
					
					RemoveBreakPoint(&bp);
					
					// Get process info
					GStringPipe p;
					if (Cmd("info inferiors", &p))
					{
						GString::Array Ln = p.NewGStr().SplitDelimit("\r\n");
						if (Ln.Length() >= 2)
						{
							GString::Array a = Ln[1].SplitDelimit(" \t");
							for (unsigned i=0; i<a.Length()-1; i++)
							{
								if (a[i].Equals("process"))
								{
									int Id = a[i+1].Int();
									if (Id >= 0)
									{
										ProcessId = Id;
									}
									break;
								}
							}
						}
					}
					
					bool Status = Cmd("c");
					if (Status)
						SetState(true, true);

					Log("[ProcessId=%i]\n", ProcessId);
					return Status;					
				}
			}

			if (Cmd(a))
			{
				SetState(true, true);
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
			if (File)
			{
				char *Last = strrchr(File, DIR_CHAR);
				sprintf_s(cmd, sizeof(cmd), "break %s:%i", Last ? Last + 1 : File, bp.Line);
			}
			else if (bp.Symbol)
			{
				sprintf_s(cmd, sizeof(cmd), "break %s", bp.Symbol.Get());
			}
			else return false;
			
			BreakPointIdx = 0;
			
			GString::Array Lines;
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
		GString CurLine;
		for (int i=0; i<t.Length(); i++)
		{
			CurLine = t[i];
			while (	i < t.Length() - 1 &&
					strchr(WhiteSpace, t[i+1][0]))
			{
				CurLine += t[++i];
				continue;
			}

			int EqPos = CurLine.Find("=");
			if (EqPos > 0)
			{
				char *val = CurLine.Get() + EqPos + 1;
				while (*val && strchr(WhiteSpace, *val)) val++;
				
				Variable &v = vars.New();
				v.Scope = scope;
				v.Name = CurLine(0, EqPos).Strip();
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
									v.Type.Set(s, e - s);
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
		// GProfile Prof("GetVars");
		GStringPipe p(256);

		if (vars.Length())
		{
			GString c;

			for (unsigned i=0; i<vars.Length(); i++)
			{
				Variable &v = vars[i];

				c.Printf("whatis %s", v.Name.Get());
				if (Cmd(c, &p))
				{
					GString a = p.NewGStr();
					printf("Type='%s'\n", a.Get());
					if (a.Find("=") >= 0)
					{
						GString::Array tmp = a.Split("=", 1);
						v.Type = tmp[1].Strip().Replace("\n", " ");
					}
					else
					{
						v.Type = a.Get();
					}
				}
				else printf("%s:%i - Cmd failed '%s'\n", _FL, c.Get());
				
				c.Printf("p %s", v.Name.Get());
				if (Cmd(c, &p))
				{
					GString a = p.NewGStr();
					GString Val;
					if (a.Find("=") >= 0)
					{
						GString::Array tmp = a.Split("=", 1);
						Val = tmp[1].Strip().Replace("\n", " ").Get();
					}
					else
					{
						Val = a.Get();
					}
					
					unsigned i;
					for (i=0; i<Vis.Length(); i++)
					{
						Visualizer *vs = Vis[i];
						if (vs->Match(v.Type))
						{
							if (vs->Transform(v.Name, Val, this, v.Value, v.Detail))
								break;
						}
					}
					if (i >= Vis.Length())
						v.Value = Val.Get();
				}
				else printf("%s:%i - Cmd failed '%s'\n", _FL, c.Get());
			}
			
			return true;
		}
		else
		{
			if (!Cmd("info args", &p))
				return false;

			// Prof.Add("ParseArgs");
			
			GAutoString a(p.NewStr());
			ParseVariables(a, vars, Variable::Arg, Detailed);

			// Prof.Add("InfoLocals");

			if (!Cmd("info locals", &p))
				return false;

			// Prof.Add("ParseLocals");
			
			a.Reset(p.NewStr());
			ParseVariables(a, vars, Variable::Local, Detailed);
		}
		
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

	bool ReadMemory(GString &BaseAddr, int Length, GArray<uint8> &OutBuf, GString *ErrorMsg)
	{
		if (!BaseAddr)
		{
			if (ErrorMsg) *ErrorMsg = "No base address supplied.";
			return false;
		}
		BaseAddr = BaseAddr.Strip();

		GString::Array Out;
		char c[256];
		int words = Length >> 2;
		int bytes = Length % 4;
		
		if (BaseAddr.Find("0x") >= 0)
		{
			// Looks like an literal address...
			LiteralAddr:
			sprintf_s(c, sizeof(c), "x/%iw %s", words, BaseAddr.Get());
		}
		else
		{
			// Maybe it's a ptr variable?
			GString c;
			GString::Array r;
			c.Printf("p %s", BaseAddr.Get());
			if (Cmd(c, NULL, &r))
			{
				GString::Array p = r[0].SplitDelimit(" \t");
				for (unsigned i=0; i<p.Length(); i++)
				{
					if (p[i].Find("0x") >= 0)
					{
						BaseAddr = p[i];
						goto LiteralAddr;
					}
					
					/*
					GString Msg;
					Msg.Printf("%s\n", p[i].Get());
					Events->Write(Msg, Msg.Length());
					*/
				}
				
				if (ErrorMsg) *ErrorMsg = "No address in variable value response.";
				return false;
			}
			else 
			{
				if (ErrorMsg) *ErrorMsg = "Couldn't convert variable to address.";
				return false;
			}
		}		
		
		if (!Cmd(c, NULL, &Out))
		{
			if (ErrorMsg) *ErrorMsg = "Gdb command failed.";
			return false;
		}
		
		if (!OutBuf.Length(words << 2))
		{
			if (ErrorMsg) ErrorMsg->Printf("Failed to allocate %i bytes.", words << 2);
			return false;
		}
		
		uint32 *buf = (uint32*) &(OutBuf)[0];
		uint32 *ptr = buf;
		uint32 *end = ptr + (OutBuf.Length() / sizeof(*buf));
		
		for (int i=0; i<Out.Length() && ptr < end; i++)
		{
			char *l = Out[i];
			if (i == 0 && stristr(l, "Invalid number"))
				return false;
			
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
			SetState(DebuggingProcess, true);
		return Status;
	}

	bool StepOver()
	{
		bool Status = Cmd("next");
		if (Status)
			SetState(DebuggingProcess, true);
		return Status;
	}

	bool StepOut()
	{
		bool Status = Cmd("finish");
		if (Status)
			SetState(DebuggingProcess, true);
		return Status;
	}

	bool Break(bool SuppressFL = false)
	{
		#ifdef POSIX
		if (ProcessId < 0)
		{
			LgiTrace("%s:%i - No process ID (yet?).\n", _FL);
			return false;
		}
		
		SuppressNextFileLine = SuppressFL;
		LgiTrace("%s:%i - sending SIGINT to %i(0x%x)...\n", _FL, ProcessId, ProcessId);
		int result = 
			#ifdef __GTK_H__
			Gtk::
			#endif
			kill(ProcessId, SIGINT);
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

	GString GetResponse(const char *c)
	{
		GString r;
		GStringPipe p;
		if (Cmd(c, &p))
			r = p.NewGStr();
		return r;
	}
};

GDebugger *CreateGdbDebugger()
{
	return new Gdb;
}
