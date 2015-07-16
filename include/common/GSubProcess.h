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

class GSubProcess : public GStreamI
{
public:
	#if defined(WIN32)
	typedef HANDLE PipeHandle;
	typedef DWORD ProcessId;
	#else
	typedef int PipeHandle;
	typedef pid_t ProcessId;
	#endif

	union Pipe
	{
		PipeHandle Handles[2];
		struct
		{
			PipeHandle Read;
			PipeHandle Write;
		};
		
		Pipe();
		
		bool Create
		(
			#ifdef WIN32
			LPSECURITY_ATTRIBUTES pAttr
			#else
			void *UnusedParam
			#endif
		);
		
		void Close();
	};

protected:
	GAutoString Exe;
	GArray<char*> Args;
	GAutoString InitialFolder;
	
	struct Variable
	{
		GAutoString Var, Val;
	};
	bool EnvironmentChanged;
	GArray<Variable> Environment;
	uint32 ErrorCode;
	
	Variable *GetEnvVar(const char *Var, bool Create = false);

	ProcessId ChildPid;
	#if defined(POSIX)
	Pipe Io;
	int ExitValue; // was uint32
	bool Dupe(PipeHandle Old, PipeHandle New);
	#elif defined(WIN32)
	HANDLE ChildHnd;
	DWORD ExitValue;
	Pipe ChildOutput, ChildInput;
	bool Dupe(PipeHandle Old, PipeHandle &New);
	#endif

	GSubProcess *Parent, *Child;

public:
	// Object
	GSubProcess(const char *exe, const char *args = NULL);	
	~GSubProcess();

	// Environment	
	void SetInitFolder(const char *f);
	const char *GetEnvironment(const char *Var);	
	bool SetEnvironment(const char *Var, const char *Value);

	// Process lifecycle
	ProcessId Handle() { return ChildPid; }
	bool IsRunning();
	uint32 GetErrorCode();
	uint32 GetExitValue();
	void Connect(GSubProcess *child);
	bool Start(bool ReadAccess, bool WriteAccess, bool MapStderrToStdout = true);
	int Wait();
	void Interrupt();
	
	// IO
	int Read(void *Buf, int Size, int Flags = 0);
	int Peek();
	int Write(const void *Buf, int Size, int Flags = 0);
};

#endif
