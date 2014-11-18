#ifndef _GDEBUGGER_H_
#define _GDEBUGGER_H_

#include "GVariant.h"

class GDebugEvents : public GStream
{
public:
	virtual ~GDebugEvents() {}
	
	virtual void OnChildLoaded(bool Loaded) = 0;
	virtual void OnRunState(bool Running) = 0;
	virtual void OnFileLine(const char *File, int Line) = 0;
	virtual void OnError(int Code, const char *Str) = 0;
	virtual void OnCrash(int Code) = 0;
};

class GDebugger
{
public:
	struct BreakPoint
	{
		GAutoString File;
		uint8 *Data;
		int Param;
		bool Enabled;
	};	
	
	struct Variable
	{
		GAutoString Name;
		GVariant Value;
	};
	
	virtual ~GDebugger() {}
	
	virtual bool Load(GDebugEvents *EventHandler, const char *Exe, const char *Args, const char *InitDir) = 0;
	virtual bool Restart() = 0;
	virtual bool Unload() = 0;
	
	virtual bool GetCallStack(GArray<GAutoString> &Stack) = 0;
	virtual bool GetFrame(int &Frame, GAutoString &File, int &Line) = 0;
	virtual bool SetFrame(int Frame) = 0;

	virtual bool SetBreakPoint(BreakPoint *bp) = 0;
	virtual bool GetBreakPoints(GArray<BreakPoint> &bps) = 0;

	virtual bool GetVariables(bool Locals, GArray<Variable> &vars) = 0;

	virtual bool GetLocation(GAutoString &File, int &Line) = 0;
	virtual bool SetLocation(const char *File, int Line) = 0;

	virtual bool GetRunning() = 0;
	virtual bool SetRuning(bool Run) = 0;
	virtual bool StepInto() = 0;
	virtual bool StepOver() = 0;
	virtual bool StepOut() = 0;
	virtual bool Break() = 0;
	
	virtual bool UserCommand(const char *Cmd) = 0;
};

extern GDebugger *CreateGdbDebugger();

#endif