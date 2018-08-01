/**
	\file
	\brief Sub-process wrapper.

	This class runs one or more sub-processes chained together by pipes.
	
	Example:
	
		GSubProcess p1("ls", "-l");
		GSubProcess p2("grep", "string");
		p1.Connect(&p2);
		p1.Start(true, false);
		int r;
		char Buf[256];
		while ((r = p1.Read(Buf, sizeof(Buf))) > 0)
		{
			// So something with 'Buf'
		}
		
*/ 

#if defined(POSIX)
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#endif
#ifdef BEOS
#include <sys/select.h>
#endif

#include "Lgi.h"
#include "GSubProcess.h"
#include "GToken.h"

#define DEBUG_SUBPROCESS		0

#if defined(WIN32)
#define NULL_PIPE NULL
#define ClosePipe CloseHandle
#else
#define NULL_PIPE -1
#define ClosePipe close
#define INVALID_PID -1
#endif
#ifdef __GTK_H__
using namespace Gtk;
#endif

GSubProcess::Pipe::Pipe()
{
	Read = Write = NULL_PIPE;
}
	
bool GSubProcess::Pipe::Create
(
	#ifdef WIN32
	LPSECURITY_ATTRIBUTES pAttr
	#else
	void *UnusedParam
	#endif
)
{
	#if defined(WIN32)
	return CreatePipe(&Read, &Write, pAttr, 0) != 0;
	#else
	return pipe(Handles) != NULL_PIPE;
	#endif
}

void GSubProcess::Pipe::Close()
{
	if (Read != NULL_PIPE)
	{
		ClosePipe(Read);
		Read = NULL_PIPE;
	}
	if (Write != NULL_PIPE)
	{
		ClosePipe(Write);
		Write = NULL_PIPE;
	}
}

GSubProcess::GSubProcess(const char *exe, const char *args)
{
	#if defined(POSIX)
	ChildPid = INVALID_PID;
	ExitValue = -1;
	#elif defined(WIN32)
	ChildPid = NULL;
	ChildHnd = NULL;
	ExitValue = 0;
	#endif
	ErrorCode = 0;
	Parent = Child = NULL;
	Exe = exe;
	Args.Add(Exe);
	EnvironmentChanged = false;

	ExternIn = NULL_PIPE;
	ExternOut = NULL_PIPE;

	#if DEBUG_SUBPROCESS
	LgiTrace("%s:%i - %p::GSubProcess('%s','%s')\n", _FL, this, exe, args);
	#endif
	
	char *s;
	while ((s = LgiTokStr(args)))
	{
		Args.Add(s);
	}
}

GSubProcess::~GSubProcess()
{
	#if defined(POSIX)
	Io.Close();
	#endif
	
	if (Child)
	{
		LgiAssert(Child->Parent == this);
		Child->Parent = NULL;
	}
	if (Parent)
	{
		LgiAssert(Parent->Child == this);
		Parent->Child = NULL;
	}
}

GSubProcess::Variable *GSubProcess::GetEnvVar(const char *Var, bool Create)
{
	if (Environment.Length() == 0)
	{
		// Read all variables in
		#if defined(WIN32)
		LPWCH e = GetEnvironmentStringsW();
		if (e)
		{
			char16 *s = e;
			while (*s)
			{
				char16 *eq = StrchrW(s, '=');
				if (!eq)
					break;

				ptrdiff_t NameChars = eq - s;
				if (NameChars > 0)
				{					
					Variable &v = Environment.New();
					v.Var.Reset(WideToUtf8(s, eq - s));
					eq++;
					v.Val.Reset(WideToUtf8(eq));
				}
				
				eq += StrlenW(eq);
				s = eq + 1;
			}
			
			FreeEnvironmentStringsW(e);
		}
		#else
		LgiAssert(0);
		#endif
	}
	
	for (unsigned i=0; i<Environment.Length(); i++)
	{
		if (!_stricmp(Environment[i].Var, Var))
		{
			return &Environment[i];
		}
	}
	
	if (Create)
	{
		Variable &v = Environment.New();
		v.Var.Reset(NewStr(Var));
		return &v;
	}
	
	return NULL;
}

#if defined(POSIX)
bool GSubProcess::Dupe(PipeHandle Old, PipeHandle New)
{
	while ((dup2(Old, New) == -1) && (errno == EINTR))
		;

	return true;
}
#elif defined(WIN32)
bool GSubProcess::Dupe(PipeHandle Old, PipeHandle &New)
{
	return DuplicateHandle(	GetCurrentProcess(), Old,
							GetCurrentProcess(), &New,
							0, false, DUPLICATE_SAME_ACCESS) != 0;
}
#endif

bool GSubProcess::IsRunning()
{
	#if defined(BEOS)
		LgiAssert(!"Impl me.");
		return false;
	#elif defined(POSIX)
		int Status = 0;
		pid_t r = waitpid(ChildPid, &Status, WNOHANG);
		if (r == ChildPid)
		{
			ChildPid = INVALID_PID;
			ExitValue = Status;
		}
		return ChildPid != INVALID_PID;
	#elif defined(WIN32)
		if (!GetExitCodeProcess(ChildHnd, &ExitValue))
			return false;
	
		if (ExitValue != STILL_ACTIVE)
		{
			ChildPid = 0;
			return false;
		}
		else
		{
			return true;
		}
	#endif
}

uint32 GSubProcess::GetErrorCode()
{
	return ErrorCode;
}

int32 GSubProcess::GetExitValue()
{
	#if defined(POSIX)
	if (ChildPid != INVALID_PID)
	 	// This will set ExitValue if the process has finished.
		IsRunning();
	#elif defined(WIN32)
	GetExitCodeProcess(ChildHnd, &ExitValue);
	#endif
	return ExitValue;
}

void GSubProcess::SetInitFolder(const char *f)
{
	InitialFolder = f;
}

const char *GSubProcess::GetEnvironment(const char *Var)
{
	Variable *v = GetEnvVar(Var);
	return v ? v->Val.Get() : NULL;
}

bool GSubProcess::SetEnvironment(const char *Var, const char *Value)
{	
	Variable *v = GetEnvVar(Var, true);
	if (!v)
		return false;
	
	bool IsPath = !_stricmp(Var, "PATH");

	GStringPipe a;
	const char *s = Value;
	while (*s)
	{
		char *n = strchr(s, '%');
		char *e = n ? strchr(n + 1, '%') : NULL;
		if (n && e)
		{
			a.Write(s, (int) (n-s));
			
			n++;
			ptrdiff_t bytes = e - n;
			char Name[128];	
			if (bytes > sizeof(Name) - 1) bytes = sizeof(Name)-1;			
			memcpy(Name, n, bytes);
			Name[bytes] = 0;
			
			const char *existing = GetEnvironment(Name);
			if (existing)
			{
				a.Write(existing, (int)strlen(existing));
			}

			s = e + 1;
		}
		else
		{
			a.Write(s, (int)strlen(s));
			break;
		}
	}
	
	v->Val.Reset(a.NewStr());
	if (IsPath)
	{
		// Remove missing paths from the list
		GToken t(v->Val, LGI_PATH_SEPARATOR);
		GStringPipe p;
		for (unsigned i=0; i<t.Length(); i++)
		{
			char *Dir = t[i];
			if (DirExists(Dir))
				p.Print("%s%s", p.GetSize() ? LGI_PATH_SEPARATOR : "", Dir);
			else
				LgiTrace("%s:%i - Removing missing path '%s'\n", _FL, Dir);
		}
		v->Val.Reset(p.NewStr());
	}
	
	EnvironmentChanged = true;
	
	return true;
}	

bool GSubProcess::GetValue(const char *Var, ::GVariant &Value)
{
	switch (LgiStringToDomProp(Var))
	{
		case StreamReadable:
		{
			#ifdef WINNATIVE
			char Buf[32] = "";
			DWORD lpBytesRead = 0;
			BOOL b = PeekNamedPipe(	ChildOutput.Read,
									Buf,
									sizeof(Buf),
									&lpBytesRead,
									NULL,
									NULL);
			Value = b && lpBytesRead > 0;
			break;
			#endif
		}
		/*
		case StreamWritable:
		{
			break;
		}
		*/
		default:
			return false;
	}
	
	return true;
}

void GSubProcess::SetStdin(OsFile Hnd)
{
	ExternIn = Hnd;
}

void GSubProcess::SetStdout(OsFile Hnd)
{
	ExternOut = Hnd;
}

void GSubProcess::Connect(GSubProcess *child)
{
	Child = child;
	if (Child)
	{
		Child->Parent = this;
	}
}

bool GSubProcess::Start(bool ReadAccess, bool WriteAccess, bool MapStderrToStdout)
{
	bool Status = false;

	#if USE_SIMPLE_FORK
	
		int in[2];
		if (pipe(in) == -1)
		{
			printf("parent: Failed to create stdin pipe");
			return false;
		}

		int out[2];
		if (pipe(out) == -1)
		{
			printf("parent: Failed to create stdout pipe");
			return false;
		}
		
		ChildPid = fork();
		if (ChildPid == 0)
		{
			// We are in the child process.
			if (InitialFolder)
			{
				chdir(InitialFolder);
			}

			// Child shouldn't write to its stdin.
			if (close(in[1]))
				printf("%s:%i - close failed.\n", _FL);

			// Child shouldn't read from its stdout.
			if (close(out[0]))
				printf("%s:%i - close failed.\n", _FL);

			// Redirect stdin and stdout for the child process.
			if (dup2(in[0], fileno(stdin)) == -1)
			{
				printf("%s:%i - child[pre-exec]: Failed to redirect stdin for child\n", _FL);
				return false;
			}
			if (close(in[0]))
				printf("%s:%i - close failed.\n", _FL);
			
			if (dup2(out[1], fileno(stdout)) == -1)
			{
				printf("%s:%i - child[pre-exec]: Failed to redirect stdout for child\n", _FL);
				return false;
			}

			if (dup2(out[1], fileno(stderr)) == -1)
			{
				printf("%s:%i - child[pre-exec]: Failed to redirect stderr for child\n", _FL);
				return false;
			}
			close(out[1]);

			// Execute the child
			Args.Add(NULL);
			execvp(Exe, &Args[0]);

			// Execution will pass to here if the 'Exe' can't run or doesn't exist
			// So by exiting with an error the parent process can handle it.
			exit(GSUBPROCESS_ERROR);
		}
		else
		{
			// We are in the parent process.
			if (ChildPid == -1)
			{
				printf("%s:%i - parent: Failed to create child", _FL);
				return false;
			}

			// Parent shouldn't read from child's stdin.
			if (close(in[0]))
				printf("%s:%i - close failed.\n", _FL);

			// Parent shouldn't write to child's stdout.
			if (close(out[1]))
				printf("%s:%i - close failed.\n", _FL);

			Io.Read = out[0];
			Io.Write = in[1];
			
			// printf("USE_SIMPLE_FORK success.\n");
			return true;
		}
	
	#else
	
		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - %p::Start(%i,%i,%i)\n", _FL, this, ReadAccess, WriteAccess, MapStderrToStdout);
		#endif
		
		// Find the end of the process list
		::GArray<GSubProcess*> p;
		for (GSubProcess *s=this; s; s=s->Child)
		{
			LgiAssert(!s->Child || s->Child->Parent == s);
			p.Add(s);
		}
		size_t Kids = p.Length() + 1;

		#ifdef WIN32
		SECURITY_ATTRIBUTES Attr;
		Attr.nLength = sizeof(SECURITY_ATTRIBUTES);
		Attr.bInheritHandle = true;
		Attr.lpSecurityDescriptor = NULL;
		#else
		int Attr = 0;
		#endif		
		
		#if defined(POSIX)
		::GArray<Pipe> Pipes;
		Pipes.Length(Kids);
		Pipes[0].Create(&Attr);
		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - *PARENT* pipe[%i].create %i,%i\n", _FL, 0, Pipes[0].Read, Pipes[0].Write);
		#endif

		Status = true;
		for (int i=1; i<Kids; i++)
		{
			Pipes[i].Create(&Attr);
			
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[%i].create %i,%i\n", _FL, i, Pipes[i].Read, Pipes[i].Write);
			#endif
			
			GSubProcess *sp = p[i-1];
			sp->ChildPid = fork();

			if (sp->ChildPid == INVALID_PID)
			{
				LgiTrace("%s:%i - fork failed with %i", _FL, errno);
				exit(1);
			}
			else if (sp->ChildPid == 0)
			{
				if (InitialFolder)
				{
					chdir(InitialFolder);
				}

				// Close irrelevant pipes
				for (int j = 0; j < i-1; j++)
				{
					#if DEBUG_SUBPROCESS
					LgiTrace("%s:%i - *CHILD* pipe[%i].close %i,%i\n", _FL, j, Pipes[j].Read, Pipes[j].Write);
					#endif
					Pipes[j].Close();
				}
				
				// Set up STDIN and STDOUT
				Pipe &in = Pipes[i-1];
				Pipe &out = Pipes[i];

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* %i) Child init %i->'%s'->%i\n", _FL, i, in.Read, sp->Exe.Get(), out.Write);
				#endif

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Dupe %i->%i\n", _FL, in.Read, STDIN_FILENO);
				#endif
				Dupe(in.Read, STDIN_FILENO);
				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Close %i\n", _FL, in.Write);
				#endif
				close(in.Write);

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Dupe %i->%i\n", _FL, out.Write, STDOUT_FILENO);
				#endif
				Dupe(out.Write, STDOUT_FILENO);
				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Dupe %i->%i\n", out.Write, STDERR_FILENO);
				#endif
				Dupe(out.Write, STDERR_FILENO);
				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Close %i\n", _FL, out.Read);
				#endif
				close(out.Read);
				fsync(STDOUT_FILENO);
				LgiSleep(100);
				
				// Execute the child
				sp->Args.Add(NULL);
				execvp(sp->Exe, &sp->Args[0]);
			
	        	LgiTrace("%s:%i - execvp('%s').\n", _FL, sp->Exe.Get());
				for (int i=0; i<sp->Args.Length(); i++)
					LgiTrace("%s:%i - Args[%i]='%s'\n", _FL, i, sp->Args[i]);
				Status = false;
				break;
			}
		}
		
		// Close irrelevant pipes
		for (int j = 1; j < Kids - 1; j++)
		{
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[%i].close %i,%i\n", _FL, j, Pipes[j].Read, Pipes[j].Write);
			#endif
			Pipes[j].Close();
		}
		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - *PARENT* pipe[0].close %i, pipe[%i].close %i\n",
			_FL,
			Pipes[0].Read,
			Pipes.Length()-1, Pipes.Last().Write);
		#endif
		close(Pipes[0].Read);
		close(Pipes.Last().Write);

		// Set the input and output pipes for this sub-process.
		if (WriteAccess)
			Io.Write = Pipes[0].Write;
		else
		{
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[0].close %i\n", _FL, Pipes[0].Write);
			#endif
			close(Pipes[0].Write);
		}
		if (ReadAccess)
			Io.Read  = Pipes.Last().Read;
		else
		{
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[%i].close %i\n", _FL, Pipes.Length()-1, Pipes.Last().Read);
			#endif
			close(Pipes.Last().Read);
		}
		
		// LgiTrace("Final Handles %i, %i\n", Io.Read, Io.Write);
		
		#elif defined(WIN32)
		
		GAutoWString WExe;
		if (FileExists(Exe))
		{
			WExe.Reset(Utf8ToWide(Exe));
		}
		else
		{
			char *Ext = LgiGetExtension(Exe);
			bool HasExt = Ext && _stricmp(Ext, "exe") == 0;
			
			#if defined(WIN32) && !defined(PLATFORM_MINGW)
			GToken p;
			char *sPath = NULL;
			size_t sSize;
			errno_t err = _dupenv_s(&sPath, &sSize, "PATH");
			if (err == 0)
				p.Parse(sPath, LGI_PATH_SEPARATOR);
			free(sPath);
			#else
			GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
			#endif
			for (unsigned i=0; i<p.Length(); i++)
			{
				char s[MAX_PATH];
				LgiMakePath(s, sizeof(s), p[i], Exe);
				if (FileExists(s))
				{
					WExe.Reset(Utf8ToWide(s));
					break;
				}
				if (!HasExt)
				{
					strcat_s(s, sizeof(s), ".exe");
					if (FileExists(s))
					{
						WExe.Reset(Utf8ToWide(s));
						break;
					}
				}
			}
		}		

		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - Exe='%S'\n", _FL, WExe.Get());
		#endif
		
		char16 WArg[1024];
		int Ch = 0;
		for (unsigned i=0; i<Args.Length(); i++)
		{
			char *a = Args[i];
			GAutoWString aw(Utf8ToWide(a));
			
			if (i > 0)
			{
				WArg[Ch++] = ' ';
			}
			
			if (strchr(a, ' '))
				Ch += swprintf_s(WArg+Ch, CountOf(WArg)-Ch, L"\"%s\"", aw.Get());
			else
				Ch += swprintf_s(WArg+Ch, CountOf(WArg)-Ch, L"%s", aw.Get());
		}

		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - Args='%S'\n", _FL, WArg);
		#endif
		
		HANDLE OldStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		HANDLE OldStdin = GetStdHandle(STD_INPUT_HANDLE);
		bool HasExternIn = ExternIn != NULL_PIPE;

		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - Oringinal handles, out=%p, in=%p, HasExternIn=%i\n", _FL, OldStdout, OldStdin, HasExternIn);
		#endif
		
		if (ChildOutput.Create(&Attr) &&
			(HasExternIn || ChildInput.Create(&Attr)) &&
			SetStdHandle(STD_OUTPUT_HANDLE, ChildOutput.Write) &&
			SetStdHandle(STD_INPUT_HANDLE,  HasExternIn ? ExternIn : ChildInput.Read))
		{
			if (!SetHandleInformation(ChildOutput.Read, HANDLE_FLAG_INHERIT, 0))
				LgiTrace("%s:%i - SetHandleInformation failed.\n", _FL);
			
			if (!HasExternIn && !SetHandleInformation(ChildInput.Write, HANDLE_FLAG_INHERIT, 0))
				LgiTrace("%s:%i - SetHandleInformation failed.\n", _FL);

			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - Output Pipe: rd=%p, wr=%p\n", _FL, ChildOutput.Read, ChildOutput.Write);
			if (!HasExternIn)
				LgiTrace("%s:%i - Input Pipe: rd=%p, wr=%p\n", _FL, ChildInput.Read, ChildInput.Write);
			#endif

			STARTUPINFOW Info;
			ZeroObj(Info);
			Info.cb = sizeof(Info);
			
			PROCESS_INFORMATION ProcInfo;
			ZeroObj(ProcInfo);

			Info.dwFlags = STARTF_USESTDHANDLES;
			Info.hStdOutput = ChildOutput.Write;
			Info.hStdInput = HasExternIn ? ExternIn : ChildInput.Read;
			if (MapStderrToStdout)
				Info.hStdError = ChildOutput.Write;
			GAutoWString WInitialFolder(Utf8ToWide(InitialFolder));

			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - WInitialFolder=%S, EnvironmentChanged=%i\n", _FL, WInitialFolder.Get(), EnvironmentChanged);
			#endif

			GAutoWString WEnv;
			if (EnvironmentChanged)
			{
				GMemQueue q(256);
				for (unsigned i=0; i<Environment.Length(); i++)
				{
					Variable &v = Environment[i];
					GAutoWString Var(Utf8ToWide(v.Var));
					GAutoWString Val(Utf8ToWide(v.Val));
					q.Write(Var, sizeof(char16)*(StrlenW(Var)));
					q.Write(L"=", sizeof(char16));
					q.Write(Val, sizeof(char16)*(StrlenW(Val)+1));
				}
				WEnv.Reset((char16*)q.New(4));
			}

			if (CreateProcessW(	WExe,
								WArg,
								&Attr,				// lpProcessAttributes
								NULL,				// lpThreadAttributes
								TRUE,				// bInheritHandles
								CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|CREATE_NEW_PROCESS_GROUP, // dwCreationFlags
								WEnv,				// lpEnvironment
								WInitialFolder,		// lpCurrentDirectory
								&Info,				// lpStartupInfo
								&ProcInfo))
			{
				ChildPid = ProcInfo.dwProcessId;
				ChildHnd = ProcInfo.hProcess;

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - CreateProcessW OK, ChildPid=%p, ChildHnd=%p\n", _FL, ChildPid, ChildHnd);
				#endif

				Status = true;
			}
			else
			{
				ErrorCode = GetLastError();
				LgiTrace("%s:%i - CreateProces('%S', '%S'...) failed with %i\n",
					_FL, WExe.Get(), WArg, ErrorCode);
			}
			
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - Closing handles: %p, %p\n", _FL, ChildOutput.Write, ChildInput.Read);
			#endif
			CloseHandle(ChildOutput.Write);
			CloseHandle(ChildInput.Read);
		}

		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - Restoring original handles\n", _FL, OldStdout, OldStdin);
		#endif

		SetStdHandle(STD_OUTPUT_HANDLE, OldStdout);
		SetStdHandle(STD_INPUT_HANDLE, OldStdin);
		#endif
	
	#endif
	
	return Status;
}

int GSubProcess::Wait()
{
	int Status = -1;

	#if defined(POSIX)
		if (ChildPid != INVALID_PID)
		{
			pid_t r = waitpid(ChildPid, &Status, NULL);
			if (r == ChildPid)
			{
				ChildPid = INVALID_PID;
				ExitValue = Status;
			}
		}
	#elif defined(WIN32)
		if (ChildHnd)
		{
			DWORD r = WaitForSingleObject(ChildHnd, INFINITE);
			if (r == WAIT_OBJECT_0)
			{
				if (GetExitCodeProcess(ChildHnd, &r))
				{
					Status = r;
				}
			}
		}
	#endif

	return Status;
}

void GSubProcess::Interrupt()
{
	#if defined(POSIX)
		if (ChildPid != INVALID_PID)
			kill(ChildPid, SIGINT);
	#elif defined(WIN32)
		if (ChildHnd)
			GenerateConsoleCtrlEvent(CTRL_C_EVENT, ChildPid);
	#endif
}

int GSubProcess::Kill()
{
	#if defined(POSIX)
		if (ChildPid != INVALID_PID)
		{
			kill(ChildPid, SIGTERM);
			printf("%s:%i - kill(%i).\n", _FL, ChildPid);
			ChildPid = INVALID_PID;
		}
		else printf("%s:%i - child pid doesn't exist.\n", _FL);
	#elif defined(WIN32)
		if (ChildHnd)
		{
			TerminateProcess(ChildHnd, -1);
			ChildHnd = NULL;
		}
		else return false;;
	#endif
	
	return true;
}

GString GSubProcess::Read()
{
	GStringPipe p(512);
	char Buf[512];
	ssize_t Rd;
	while ((Rd = Read(Buf, sizeof(Buf))) > 0)
		p.Write(Buf, Rd);

	return p.NewGStr();
}

ssize_t GSubProcess::Read(void *Buf, ssize_t Size, int TimeoutMs)
{
	#if defined(POSIX)
		bool DoRead = true;
		if (TimeoutMs)
		{
			OsSocket s = Io.Read;
			if (ValidSocket(s))
			{
				struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

				fd_set r;
				FD_ZERO(&r);
				FD_SET(s, &r);
				
				int v = select((int)s+1, &r, 0, 0, &t);
				if (v > 0 && FD_ISSET(s, &r))
				{
					DoRead = true;
				}
				else
				{
					// printf("SubProc not readable..\n");
					return 0;
				}
			}
			else LgiTrace("%s:%i - Invalid socket.\n", _FL);
		}
		
		return (int)read(Io.Read, Buf, Size);
	#else		
		DWORD Rd = -1, Sz;
		if (!ReadFile(ChildOutput.Read, Buf, AssertCast(Sz, Size), &Rd, NULL))
			return -1;
		return Rd;
	#endif
}

int GSubProcess::Peek()
{
	#if defined(POSIX)
		#warning FIXME
		LgiAssert(0);
		return 0;
	#else		
		DWORD Rd = 0, Avail = 0;
		char Buf[32];
		if (PeekNamedPipe(ChildOutput.Read, Buf, sizeof(Buf), &Rd, &Avail, NULL))
			return Rd;
		
		return 0;
	#endif	
}

ssize_t GSubProcess::Write(const void *Buf, ssize_t Size, int Flags)
{
	#if defined(POSIX)
		return (int)write(Io.Write, Buf, Size);
	#else
		DWORD Wr = -1, Sz;
		if (!WriteFile(ChildInput.Write, Buf, AssertCast(Sz, Size), &Wr, NULL))
			return -1;
		return Wr;
	#endif
}

