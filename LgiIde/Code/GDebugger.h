#ifndef _GDEBUGGER_H_
#define _GDEBUGGER_H_

#include "GVariant.h"

class GDebugEvents
{
public:
	virtual ~GDebugEvents() {}
	
	virtual void OnRunStateChange(bool Running) = 0;
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
	virtual bool GetRunning() = 0;
	virtual bool SetRuning(bool Run) = 0;
	virtual bool SetBreakPoint(BreakPoint *bp) = 0;
	virtual bool GetBreakPoints(GArray<BreakPoint> &bps) = 0;
	virtual bool GetVariables(GArray<Variable> &vars) = 0;
	virtual bool GetLocation(GAutoString &File, int &Line) = 0;
	virtual bool SetLocation(const char *File, int Line) = 0;
};

extern GDebugger *CreateGdbDebugger();

#endif