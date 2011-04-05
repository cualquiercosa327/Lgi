/// \file
#ifndef _GPROCESS_H_
#define _GPROCESS_H_

#include "GStream.h"

LgiFunc bool LgiIsProcess(OsProcessId Pid);

/// A process wrapper class
class LgiClass GProcess
{
	class GProcessPrivate *d;

public:
	GProcess();
	~GProcess();
	
	/// \returns the process handle
	OsProcess Handle();
	/// \returns the process ID [Win32]
	OsProcessId GetId();
	/// \returns the value the process exited with
	int ExitValue();
	/// Stops the process right now, use with care
	bool Terminate();
	/// \returns true if still running, else false.
	bool IsRunning();
	/// Starts a new process
	bool Run
	(
		/// The path to the executable to run
		const char *Exe,
		/// The arguments to pass to the program
		const char *Args,
		/// The current directory to start the program in
		const char *Dir,
		/// If true, calling Run will block until the process
		/// exits, else Run will exit after starting the process.
		///
		/// The parameters In and Out are only used if Wait=true
		bool Wait,
		/// A stream to supply input to the process. Only used if
		/// Wait=true.
		GStream *In = 0,
		/// A stream to capture output from the process. Only used if
		/// Wait=true.
		GStream *Out = 0,
		/// The priority to run the process at.
		/// <li> -2 = idle
		/// <li> -1 = low
		/// <li> 0 = normal
		/// <li> 1 = high
		/// <li> 2 = realtime(ish)
		int Priority = 0
	);
};

#endif
