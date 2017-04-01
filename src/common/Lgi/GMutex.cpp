#include <stdio.h>
#include <errno.h>

#include "Lgi.h"

//////////////////////////////////////////////////////////////////////////////
char *SemPrint(OsSemaphore *s)
{
	static char Buf[256];
	
	int ch = 0;
	for (int i=0; i<sizeof(OsSemaphore); i++)
	{
		ch += sprintf_s(Buf+ch, sizeof(Buf)-ch, "%2.2X,", ((uchar*)s)[i]);
	}
	
	return Buf;
}

GMutex::GMutex(const char *name)
{
	_Thread = 0;
	_Count = 0;
	File = 0;
	Line = 0;
	_Name = NewStr(name);
	#ifdef _DEBUG
	_DebugSem = false;
	#endif

	#if defined WIN32
	
	// _Sem = CreateMutex(NULL, FALSE, NULL);
	InitializeCriticalSection(&_Sem);
	
	#elif defined BEOS
	
	_Sem = create_sem(1, _Name ? _Name :"LGI.GMutex");

	#elif defined POSIX

	ZeroObj(_Sem);
	if (pthread_mutex_init(&_Sem, 0))
	{
		LgiTrace("%s:%i - Couldn't create mutex for GMutex\n", __FILE__, __LINE__);
	}

	#endif
}

GMutex::~GMutex()
{
	#if defined WIN32

	// CloseHandle(_Sem);
	// _Sem = 0;
	DeleteCriticalSection(&_Sem);

	#elif defined BEOS

	if (_Sem > 0)
	{
		delete_sem(_Sem);
		_Sem = 0;
	}
	
	#elif defined POSIX

	pthread_mutex_destroy(&_Sem);

	#endif
	
	DeleteArray(_Name);
}

char *GMutex::GetName()
{
	return _Name;
}

void GMutex::SetName(const char *s)
{
	DeleteArray(_Name);
	_Name = NewStr(s);
}

bool GMutex::_Lock()
{
	#if defined WIN32

	// LgiAssert(_Sem != 0);
	// return WaitForSingleObject(_Sem, INFINITE) == WAIT_OBJECT_0;
	EnterCriticalSection(&_Sem);
	return true;

	#elif defined BEOS

	return acquire_sem(_Sem) == B_OK;
	
	#elif defined POSIX

	if (pthread_mutex_trylock(&_Sem))
	{
		// printf("\t%s:%i - pthread_mutex_trylock errored: %s\n\t_Sem=%s\n", __FILE__, __LINE__, GetErrorName(errno), SemPrint(&_Sem));
		return false;
	}
	
	return true;

	#endif
}

void GMutex::_Unlock()
{
	#if defined WIN32

	// ReleaseMutex(_Sem);
	LeaveCriticalSection(&_Sem);

	#elif defined BEOS

	release_sem(_Sem);
	
	#elif defined POSIX

	if (pthread_mutex_unlock(&_Sem))
	{
		printf("\t%s:%i - pthread_mutex_unlock errored\n", _FL);
	}

	#endif
}

bool GMutex::Lock(const char *file, int line)
{
	int64 Start = LgiCurrentTime();
	bool Status = false;
	OsThreadId CurrentThread = LgiGetCurrentThread();
	bool Warn = true;

	while (!Status)
	{
		if (_Lock())
		{
			if (!_Thread ||
				_Thread == CurrentThread)
			{
				_Thread = CurrentThread;
				_Count++;
				File = file;
				Line = line;

				/*
				if (_Name && stricmp(_Name, "ScribeWnd") == 0)
				{
					char m[256];
					sprintf_s(m, sizeof(m), "Lock '%s' by 0x%x, count=%i\n", _Name, _Thread, _Count);
					OutputDebugString(m);
				}
				*/

				Status = true;
			}
			_Unlock();
		}

		if (!Status)
		{
			LgiSleep(1);
		}

		#if 1 // _DEBUG
		int64 Now = LgiCurrentTime();
		if (Warn && Now > Start + 5000)
		{
			LgiTrace("GMutex=%p(%s): Can't lock after %ims... LockingThread=%i ThisThread=%x Count=%x Locker=%s:%i.\n",
					this,
					_Name,
					(int)(Now - Start),
					_Thread,
					CurrentThread,
					_Count,
					File,
					Line);
			Start = Now;

			// Warn = false;
		}
		
		if (LgiCurrentTime() > Start + (2 * 60 * 1000))
		{
			// Obviously we've locked up and to un-deadlock things we'll fail the lock
			LgiTrace("::Lock timeout ask_thread=%i hold_thread=%i sem=%i count=%i\n", _Thread, CurrentThread, _Sem, _Count);
			break;
		}
		#endif
	}

	#ifdef _DEBUG
    /*
	if (_DebugSem)
		LgiStackTrace("%p::Lock %i\n", this, _Count);
    */
	#endif
	return Status;
}

bool GMutex::LockWithTimeout(int Timeout, const char *file, int line)
{
	int64 Start = LgiCurrentTime();
	bool Status = false;

	while (!Status &&
			LgiCurrentTime() < Start + Timeout)
	{
		if (_Lock())
		{
			OsThreadId CurrentThread = LgiGetCurrentThread();
			if (!_Thread ||
				_Thread == CurrentThread)
			{
				_Thread = CurrentThread;
				_Count++;
				File = file;
				Line = line;
				Status = true;
			}
			_Unlock();
		}

		if (!Status)
		{
			LgiSleep(5);
		}
	}

	#ifdef _DEBUG
    /*
	if (_DebugSem)
		LgiStackTrace("%p::LockWi %i\n", this, _Count);
    */
	#endif
	return Status;
}

void GMutex::Unlock()
{
	#ifdef _DEBUG
    /*
	if (_DebugSem)
		LgiStackTrace("%p::Unlock %i\n", this, _Count);
    */
	#endif
	while (!_Lock())
	{
		LgiSleep(1);
	}
	
	if (_Count < 1)
	{
		printf("%s:%i - _Count=%i\n", __FILE__, __LINE__, _Count);
	
		#if 0
		// I want a stack trace the next time this happens
		int *i = 0;
		*i = 0;
		#endif
		
		// if this assert fails then you tryed to unlock an object that
		// wasn't locked in the first place
		LgiAssert(0);
	}

	// decrement the lock counter
	if (_Count > 0)
	{
		_Count--;
	}
	if (_Count < 1)
	{
		_Thread = 0;
		File = 0;
		Line = 0;
	}

	_Unlock();
}

