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
#ifdef POSIX
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

#if defined(WIN32)
#define NULL_PIPE NULL
#define ClosePipe CloseHandle
#else
#define NULL_PIPE -1
#define ClosePipe close
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
	ChildPid = -1;
	#elif defined(WIN32)
	ChildPid = NULL;
	ChildHnd = NULL;
	ExitValue = 0;
	#endif
	ErrorCode = 0;
	Parent = Child = NULL;
	Exe.Reset(NewStr(exe));
	Args.Add(Exe);
	EnvironmentChanged = false;

	ExternIn = NULL_PIPE;
	ExternOut = NULL_PIPE;
	
	char *s;
	while ((s = LgiTokStr(args)))
	{
		Args.Add(s);
	}
}

GSubProcess::~GSubProcess()
{
	#ifdef POSIX
	close(Io.Read);
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

				int NameChars = eq - s;
				if (NameChars > 0)
				{					
					Variable &v = Environment.New();
					v.Var.Reset(LgiNewUtf16To8(s, sizeof(char16)*(eq-s)));
					eq++;
					v.Val.Reset(LgiNewUtf16To8(eq));
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
		#if 1
			int Status = 0;
			pid_t r = waitpid(ChildPid, &Status, WNOHANG);
			if (r == ChildPid)
			{
				ChildPid = 0;
				printf("%s:%i - waitpid signalled: %i\n", Status);
			}
			return ChildPid != 0;
		#else
			// Apparently this is deprecated...
			int i = wait4(ChildPid, &ExitValue, WNOHANG, 0);
			if (i)
				ChildPid = 0;
			return ChildPid != 0;
		#endif
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

uint32 GSubProcess::GetExitValue()
{
	#ifdef WIN32
	GetExitCodeProcess(ChildHnd, &ExitValue);
	#endif
	return ExitValue;	
}

void GSubProcess::SetInitFolder(const char *f)
{
	InitialFolder.Reset(NewStr(f));
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
			a.Write(s, n-s);
			
			n++;
			int bytes = e - n;				
			char Name[128];	
			if (bytes > sizeof(Name) - 1) bytes = sizeof(Name)-1;			
			memcpy(Name, n, bytes);
			Name[bytes] = 0;
			
			const char *existing = GetEnvironment(Name);
			if (existing)
			{
				a.Write(existing, strlen(existing));
			}

			s = e + 1;
		}
		else
		{
			a.Write(s, strlen(s));
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
	
	// Find the end of the process list
	::GArray<GSubProcess*> p;
	for (GSubProcess *s=this; s; s=s->Child)
	{
		LgiAssert(!s->Child || s->Child->Parent == s);
		p.Add(s);
	}
	int Kids = p.Length() + 1;

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

	Status = true;
	for (int i=1; i<Kids; i++)
	{
		Pipes[i].Create(&Attr);
		
// LgiTrace("%i) pipe[%i]=%i,%i\n", i, i, Pipes[i].Read, Pipes[i].Write);
		
		GSubProcess *sp = p[i-1];
		sp->ChildPid = fork();

		if (sp->ChildPid == -1)
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
				Pipes[j].Close();
			}
			
			// Set up STDIN and STDOUT
			Pipe &in = Pipes[i-1];
			Pipe &out = Pipes[i];

// LgiTrace("%i) Child init %i->'%s'->%i\n", i, in.Read, sp->Exe.Get(), out.Write);

			Dupe(in.Read, STDIN_FILENO);
			close(in.Write);

			Dupe(out.Write, STDOUT_FILENO);
			Dupe(out.Write, STDERR_FILENO);
			close(out.Read);
			
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
		Pipes[j].Close();
	}
	close(Pipes[0].Read);
	close(Pipes.Last().Write);

	// Set the input and output pipes for this sub-process.
	if (WriteAccess)
		Io.Write = Pipes[0].Write;
	else
		close(Pipes[0].Write);
	if (ReadAccess)
		Io.Read  = Pipes.Last().Read;
	else
		close(Pipes.Last().Read);
	
	// LgiTrace("Final Handles %i, %i\n", Io.Read, Io.Write);
	
	#elif defined(WIN32)
	
	GAutoWString WExe;
	if (FileExists(Exe))
	{
		WExe.Reset(LgiNewUtf8To16(Exe));
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
				WExe.Reset(LgiNewUtf8To16(s));
				break;
			}
			if (!HasExt)
			{
				strcat_s(s, sizeof(s), ".exe");
				if (FileExists(s))
				{
					WExe.Reset(LgiNewUtf8To16(s));
					break;
				}
			}
		}
	}		
	
	char16 WArg[512];
	int Ch = 0;
	for (unsigned i=0; i<Args.Length(); i++)
	{
		char *a = Args[i];
		GAutoWString aw(LgiNewUtf8To16(a));
		
		if (i > 0)
		{
			WArg[Ch++] = ' ';
		}
		
		if (strchr(a, ' '))
			Ch += swprintf_s(WArg+Ch, CountOf(WArg)-Ch, L"\"%s\"", aw.Get());
		else
			Ch += swprintf_s(WArg+Ch, CountOf(WArg)-Ch, L"%s", aw.Get());
	}
	
	HANDLE OldStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE OldStdin = GetStdHandle(STD_INPUT_HANDLE);
	bool HasExternIn = ExternIn != NULL_PIPE;
	
	if (ChildOutput.Create(&Attr) &&
		(HasExternIn || ChildInput.Create(&Attr)) &&
		SetStdHandle(STD_OUTPUT_HANDLE, ChildOutput.Write) &&
		SetStdHandle(STD_INPUT_HANDLE,  HasExternIn ? ExternIn : ChildInput.Read))
	{
		if (!SetHandleInformation(ChildOutput.Read, HANDLE_FLAG_INHERIT, 0))
			LgiTrace("%s:%i - SetHandleInformation failed.\n", _FL);
		
		if (!HasExternIn && !SetHandleInformation(ChildInput.Write, HANDLE_FLAG_INHERIT, 0))
			LgiTrace("%s:%i - SetHandleInformation failed.\n", _FL);

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
		GAutoWString WInitialFolder(LgiNewUtf8To16(InitialFolder));

		GAutoWString WEnv;
		if (EnvironmentChanged)
		{
			GMemQueue q(256);
			for (unsigned i=0; i<Environment.Length(); i++)
			{
				Variable &v = Environment[i];
				GAutoWString Var(LgiNewUtf8To16(v.Var));
				GAutoWString Val(LgiNewUtf8To16(v.Val));
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
			Status = true;
		}
		else
		{
			ErrorCode = GetLastError();
			LgiTrace("%s:%i - CreateProces('%S', '%S'...) failed with %i\n",
				_FL, WExe.Get(), WArg, ErrorCode);
		}
		
		CloseHandle(ChildOutput.Write);
		CloseHandle(ChildInput.Read);
	}

	SetStdHandle(STD_OUTPUT_HANDLE, OldStdout);
	SetStdHandle(STD_INPUT_HANDLE, OldStdin);
	#endif
	
	return Status;
}

int GSubProcess::Wait()
{
	int Status = -1;
	#ifdef POSIX
	if (ChildPid != -1)
		waitpid(ChildPid, &Status, NULL);
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
	#ifdef POSIX
	if (ChildPid != -1)
		kill(ChildPid, SIGINT);
	#elif defined(WIN32)
	if (ChildHnd)
		GenerateConsoleCtrlEvent(CTRL_C_EVENT, ChildPid);
	#endif
}

int GSubProcess::Read(void *Buf, int Size, int TimeoutMs)
{
	#ifdef POSIX
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
	
	return read(Io.Read, Buf, Size);
	#else		
	DWORD Rd = -1;
	if (!ReadFile(ChildOutput.Read, Buf, Size, &Rd, NULL))
		return -1;
	return Rd;
	#endif
}

int GSubProcess::Peek()
{
	#ifdef POSIX
	LgiAssert(!"Impl me.");
	#else		
	DWORD Rd = 0, Avail = 0;
	char Buf[32];
	if (PeekNamedPipe(ChildOutput.Read, Buf, sizeof(Buf), &Rd, &Avail, NULL))
		return Rd;
	
	return 0;
	#endif	
}

int GSubProcess::Write(const void *Buf, int Size, int Flags)
{
	#ifdef POSIX
	return write(Io.Write, Buf, Size);
	#else
	DWORD Wr = -1;
	if (!WriteFile(ChildInput.Write, Buf, Size, &Wr, NULL))
		return -1;
	return Wr;
	#endif
}

