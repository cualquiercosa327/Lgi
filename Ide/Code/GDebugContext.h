#ifndef _GDEBUG_CONTEXT_H_
#define _GDEBUG_CONTEXT_H_

class GDebugContext : public GDebugEvents
{
	class GDebugContextPriv *d;
	
public:
	GTree *Watch;
	LList *Locals;
	LList *CallStack;
	LList *Threads;
	class GTextLog *ObjectDump;
	class GTextLog *MemoryDump;
	class GTextLog *DebuggerLog;
	class GTextLog *Registers;

	// Object
	GDebugContext(AppWnd *App, class IdeProject *Proj, const char *Exe, const char *Args, bool RunAsAdmin = false);
	virtual ~GDebugContext();

	// Impl
	bool ParseFrameReference(const char *Frame, GAutoString &File, int &Line);
	bool SetFrame(int Frame);
	bool UpdateLocals();
	bool UpdateWatches();
	bool UpdateRegisters();
	void UpdateCallStack();
	void UpdateThreads();
	bool SelectThread(int ThreadId);
	bool DumpObject(const char *Var, const char *Val);
	bool OnBreakPoint(GDebugger::BreakPoint &b, bool Add);
	
	// Ui events...
	bool OnCommand(int Cmd);
	void OnUserCommand(const char *Cmd);
	GMessage::Param OnEvent(GMessage *m);
	void OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex);
	void FormatMemoryDump(int WordSize, int Width, bool InHex);
	
	// Debugger events...
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0);
	void OnState(bool Debugging, bool Running);
	void OnFileLine(const char *File, int Line, bool CurrentIp);
	void OnError(int Code, const char *Str);
	void OnCrash(int Code);
};

#endif