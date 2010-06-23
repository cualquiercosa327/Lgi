/// \file
#ifndef __GSEMAPHORE_H
#define __GSEMAPHORE_H

/// Thread locking
class LgiClass GSemaphore
{
	OsThreadId _Thread;
	OsSemaphore _Sem;
	char *File;
	int Line;
	#ifdef _DEBUG
	bool _DebugSem;
	#endif
	
	bool _Lock();
	void _Unlock();
	char *_Name;

protected:
	int _Count;

public:
	/// Constructor
	GSemaphore
	(
		/// Optional name for the semaphore
		char *name = 0
	);
	virtual ~GSemaphore();

	/// Lock the semaphore, waiting forever
	///
	/// \returns true if locked successfully.
	bool Lock
	(
		/// The file name of the locker
		char *file,
		/// The line number of the locker
		int line
	);
	/// \returns true if the semephore was locked in the time
	bool LockWithTimeout
	(
		/// In ms
		int Timeout,
		/// The file of the locker
		char *file,
		/// The line number of the locker
		int line
	);
	/// Unlocks the semaphore
	void Unlock();

	char *GetName();
	void SetName(char *s);
	
	#ifdef _DEBUG
	void SetDebug(bool b = true) { _DebugSem = b; }
	int GetCount() { return _Count; }
	#endif
	
	class Auto
	{
	    GSemaphore *Sem;
	    bool Locked;
	
	public:
	    Auto(GSemaphore *s, char *file, int line)
	    {
	        LgiAssert(s);
	        Locked = (Sem = s) ? Sem->Lock(file, line) : 0;
			LgiAssert(Locked);
	    }

	    Auto(GSemaphore *s, int timeout, char *file, int line)
	    {
	        LgiAssert(s);
	        Locked = (Sem = s) ? Sem->LockWithTimeout(timeout, file, line) : 0;
	    }
	    
	    ~Auto()
	    {
	        if (Locked) Sem->Unlock();
	    }

		bool GetLocked() { return Locked; }
	};
};

#endif
