/**
	\file
	\brief Unix sub-process wrapper.

	This class runs one or more unix style sub-processes chained together by pipes.
	
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

#if !defined(WIN32)

namespace Gtk {
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
}

using namespace Gtk;

class GSubProcess : public GStreamI
{
public:
	union Pipe
	{
		int Handles[2];
		struct
		{
			int Read;
			int Write;
		};
		
		Pipe()
		{
			Read = Write = -1;
		}
		
		bool Create()
		{
			return pipe(Handles) != -1;
		}
		
		void Close()
		{
			close(Read);
			close(Write);
		}
	};

protected:
	GAutoString Exe;
	::GArray<char*> Args;
	pid_t ChildPid;
	Pipe Io;
	GSubProcess *Parent, *Child;

	void Dupe(int Old, int New)
	{
		while ((dup2(Old, New) == -1) && (errno == EINTR))
			;
	}

public:
	GSubProcess(const char *exe, const char *args = NULL)
	{
		ChildPid = -1;
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
		close(Io.Read);
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
		::GArray<Pipe> Pipes;
		for (GSubProcess *s=this; s; s=s->Child)
		{
			LgiAssert(!s->Child || s->Child->Parent == s);
			p.Add(s);
		}
		int Kids = p.Length() + 1;
		Pipes.Length(Kids);
		Pipes[0].Create();
		
		// printf("Start %i processes... pipe[0]=%i,%i\n", p.Length(), Pipes[0].Read, Pipes[0].Write);
		
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
	}
	
	int Wait()
	{
		int Status = -1;
		if (ChildPid != -1)
			waitpid(ChildPid, &Status, NULL);
		return Status;
   	}
	
	int Read(void *Buf, int Size, int Flags = 0)
	{
		return read(Io.Read, Buf, Size);
	}
	
	int Write(const void *Buf, int Size, int Flags = 0)
	{
		return write(Io.Write, Buf, Size);
	}
};

#endif

#endif
