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
#ifndef _SUB_PROCESS_H_
#define _SUB_PROCESS_H_

#ifdef __GTK_H__
namespace Gtk {
#endif
#ifdef POSIX
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#endif
#ifdef __GTK_H__
}
using namespace Gtk;
#endif

class GSubProcess : public GStreamI
{
public:
	#if defined(WIN32)
	typedef HANDLE PipeHandle;
	#define NULL_PIPE NULL
	#define ClosePipe CloseHandle
	#else
	typedef int PipeHandle;
	#define NULL_PIPE -1
	#define ClosePipe close
	#endif

	union Pipe
	{
		PipeHandle Handles[2];
		struct
		{
			PipeHandle Read;
			PipeHandle Write;
		};
		
		Pipe()
		{
			Read = Write = NULL_PIPE;
		}
		
		bool Create
		(
			#ifdef WIN32
			LPSECURITY_ATTRIBUTES pAttr
			#else
			void *UnusedParam
			#endif
		)
		{
			#if defined(WIN32)
			return CreatePipe(&Read, &Write, pAttr, 0);
			#else
			return pipe(Handles) != NULL_PIPE;
			#endif
		}
		
		void Close()
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
	};

protected:
	GAutoString Exe;
	::GArray<char*> Args;

	#if defined(POSIX)
	pid_t ChildPid;
	Pipe Io;

	void Dupe(int Old, int New)
	{
		while ((dup2(Old, New) == -1) && (errno == EINTR))
			;
	}
	#elif defined(WIN32)
	DWORD ChildPid;
	HANDLE ChildHnd;

	void Dupe(PipeHandle Old, PipeHandle &New)
	{
		DuplicateHandle(GetCurrentProcess(), Old,
						GetCurrentProcess(), &New,
						0, false, DUPLICATE_SAME_ACCESS);
	}
	#endif

	GSubProcess *Parent, *Child;

public:
	GSubProcess(const char *exe, const char *args = NULL)
	{
		#if defined(POSIX)
		ChildPid = -1;
		#elif defined(WIN32)
		ChildPid = NULL;
		ChildHnd = NULL;
		#endif
		Parent = Child = NULL;
		Exe.Reset(NewStr(exe));
		Args.Add(Exe);
		
		char *s;
		while (s = LgiTokStr(args))
		{
			Args.Add(s);
		}
	}
	
	~GSubProcess()
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
	
	void Connect(GSubProcess *child)
	{
		Child = child;
		if (Child)
		{
			Child->Parent = this;
		}
	}
	
	void Start(bool ReadAccess, bool WriteAccess)
	{
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
		
		::GArray<Pipe> Pipes;
		Pipes.Length(Kids);
		Pipes[0].Create(&Attr);

		// printf("Start %i processes... pipe[0]=%i,%i\n", p.Length(), Pipes[0].Read, Pipes[0].Write);

		#if defined(POSIX)
		for (int i=1; i<Kids; i++)
		{
			Pipes[i].Create();
			// printf("%i) pipe[%i]=%i,%i\n", i, i, Pipes[i].Read, Pipes[i].Write);
			
			GSubProcess *sp = p[i-1];
			sp->ChildPid = fork();

			if (sp->ChildPid == -1)
			{
				printf("%s:%i - fork failed with %i", _FL, errno);
				exit(1);
			}
			else if (sp->ChildPid == 0)
			{
				// Close irrelevant pipes
				for (int j = 0; j < i-1; j++)
				{
					Pipes[j].Close();
				}
				
				// Set up STDIN and STDOUT
				Pipe &in = Pipes[i-1];
				Pipe &out = Pipes[i];

				// printf("%i) Child init %i->'%s'->%i\n", i, in.Read, sp->Exe.Get(), out.Write);

				Dupe(in.Read, STDIN_FILENO);
				close(in.Write);

				Dupe(out.Write, STDOUT_FILENO);
				Dupe(out.Write, STDERR_FILENO);
				close(out.Read);
				
				// Execute the child
				sp->Args.Add(NULL);
				execvp(sp->Exe, &sp->Args[0]);
			
				printf("%s:%i - execvp failed.\n", _FL);
				_exit(1);
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
		
		// printf("Final Handles %i, %i\n", Io.Read, Io.Write);
		
		#elif defined(WIN32)
		
		GAutoWString WExe(LgiNewUtf8To16(Exe));
		char16 WArg[512];
		int Ch = 0;
		for (int i=0; i<Args.Length(); i++)
		{
			char *a = Args[i];
			GAutoWString aw(LgiNewUtf8To16(a));
			
			if (i)
			{
				WArg[Ch++] = ' ';
			}
			
			if (strchr(a, ' '))
				Ch += swprintf_s(WArg+Ch, CountOf(WArg)-Ch, L"\"%s\"", aw.Get());
			else
				Ch += swprintf_s(WArg+Ch, CountOf(WArg)-Ch, L"%s", aw.Get());
		}
		
		STARTUPINFOW Info;
		ZeroObj(Info);
		Info.cb = sizeof(Info);
		
		PROCESS_INFORMATION ProcInfo;
		ZeroObj(ProcInfo);

		if (CreateProcessW(	WExe,
							WArg,
							&Attr,	// lpProcessAttributes
							NULL,	// lpThreadAttributes
							TRUE,	// bInheritHandles
							0,		// dwCreationFlags
							NULL,	// lpEnvironment
							NULL,	// lpCurrentDirectory
							&Info,	// lpStartupInfo
							&ProcInfo))
		{
			ChildPid = ProcInfo.dwProcessId;
			ChildHnd = ProcInfo.hProcess;
		}		
		#endif
	}
	
	int Wait()
	{
		int Status = -1;
		#ifdef POSIX
		if (ChildPid != -1)
			waitpid(ChildPid, &Status, NULL);
		#else
		#endif
		return Status;
   	}
	
	int Read(void *Buf, int Size, int Flags = 0)
	{
		#ifdef POSIX
		return read(Io.Read, Buf, Size);
		#else
		return -1;
		#endif
	}
	
	int Write(const void *Buf, int Size, int Flags = 0)
	{
		#ifdef POSIX
		return write(Io.Write, Buf, Size);
		#else
		return -1;
		#endif
	}
};

#endif
