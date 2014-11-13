#include "Lgi.h"
#include "GDebugger.h"

class Gdb : public GDebugger
{
public:
	bool Load(GDebugEvents *EventHandler, const char *Exe, const char *Args, const char *InitDir)
	{
		return false;
	}
	
	bool GetRunning()
	{
		return false;
	}
	
	bool SetRuning(bool Run)
	{
		return false;
	}
	
	bool SetBreakPoint(BreakPoint *bp)
	{
		return false;
	}
	
	bool GetBreakPoints(GArray<BreakPoint> &bps)
	{
		return false;
	}
	
	bool GetVariables(GArray<Variable> &vars)
	{
		return false;
	}
	
	bool GetLocation(GAutoString &File, int &Line)
	{
		return false;
	}
	
	bool SetLocation(const char *File, int Line)
	{
		return false;
	}
};

GDebugger *CreateGdbDebugger()
{
	return new Gdb;
}
