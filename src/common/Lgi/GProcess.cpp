/*
	Process related functions and classes.

	FYI: There is an alternative class with better streaming: GSubProcess
		 (in include\common\GSubProcess.h)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(LINUX) || defined(MAC)
#include <signal.h>
#include <unistd.h>
#endif

#include "Lgi.h"
#include "GProcess.h"
#include "GToken.h"

#ifndef STILL_ACTIVE
#define STILL_ACTIVE	254
#endif

#define IO_WAIT			10

#if defined(LINUX) || defined(MAC)

#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

int hndstate(int hnd)
{
	pollfd p;
	
	p.fd = hnd;
	p.events = 0x3f;
	p.revents = 0;
	
	if (poll(&p, 1, 1) > 0)
	{
		return p.revents;
	}
	
	return 0;
}

#elif defined BEOS

#include <sys/wait.h>

#define POLLOUT 1
#define POLLIN 2
int hndstate(int h)
{
	return 0;
}

#endif

bool LgiIsProcess(OsProcessId Pid)
{
	bool Status = false;

	#if defined WIN32

		HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, false, Pid);
		if (hProc)
		{
			DWORD ExitCode = 0;
			if (GetExitCodeProcess(hProc, &ExitCode) &&
				ExitCode == STILL_ACTIVE)
			{
				Status = true;
			}
			CloseHandle(hProc);
		}
		else LgiTrace("%s:%i - OpenProcess failed with 0x%x\n", _FL, GetLastError());
	
	#elif defined(MAC)
	
		#if COCOA
	
		#warning FIXME
	
		#else
	
        ProcessSerialNumber psn;
        OSStatus e = GetProcessForPID(Pid, &psn);
		return e == 0;
	
		#endif

	#elif defined(LINUX)

		char ProcPath[256];
		sprintf_s(ProcPath, sizeof(ProcPath), "/proc/%i", Pid);
		Status = DirExists(ProcPath);
	
	#elif defined BEOS
	
		BRoster r;
		app_info a;
		if (r.GetRunningAppInfo(Pid, &a) == B_OK)
		{
			Status = true;
		}

	#else

		#error Impl me.

	#endif

	return Status;
}

class GProcessPrivate
{
public:
	OsProcess Pid;
	OsProcessId ProcessId;
	ulong ExitValue;
	uint32 ErrorCode;
	char *Exe;
	
	GProcessPrivate()
	{
		Pid = 0;
		ProcessId = 0;
		Exe = NULL;
		ErrorCode = 0;
		ExitValue = STILL_ACTIVE;
	}
	
	~GProcessPrivate()
	{
		DeleteArray(Exe);
	}
};

GProcess::GProcess()
{
	d = new GProcessPrivate;
}

GProcess::~GProcess()
{
	DeleteObj(d);
}

OsProcess GProcess::Handle()
{
	return d->Pid;
}

OsProcessId GProcess::GetId()
{
	return d->ProcessId;
}

uint32 GProcess::GetErrorCode()
{
	return d->ErrorCode;
}

ulong GProcess::ExitValue()
{
	return d->ExitValue;
}

bool GProcess::IsRunning()
{
	if (d->Pid)
	{
		#if defined(LINUX)||defined(MAC)
		
		int Status = 0;
		int i = wait4(d->Pid, &Status, WNOHANG, 0);
		if (i)
		{
			// printf("IsRunning wait4(%i)=%i\n", d->Pid, i);
			
			d->ExitValue = Status;
			d->Pid = 0;
		}
		
		#elif defined BEOS
		
		int Status = 0;
		int i = waitpid(d->Pid, &Status, WNOHANG);
		if (i)
		{
			d->ExitValue = Status;
			d->Pid = 0;
		}
		
		#elif defined(WIN32)
		
		if (GetExitCodeProcess(d->Pid, &d->ExitValue))
		{
			if (d->ExitValue != STILL_ACTIVE)
			{
				d->Pid = 0;
			}
		}

		#else

			#error Impl me.

		#endif
	}
	
	return d->Pid != 0;
}

class GNativeString
{
	static bool WinNT;

	char *n;
	char16 *w;

public:
	GNativeString()
	{
		n = 0;
		w = 0;
	}

	GNativeString(const char *utf)
	{
		n = 0;
		w = 0;

		*this = utf;
	}

	~GNativeString()
	{
		DeleteArray(n);
		DeleteArray(w);
	}

	GNativeString &operator =(const char *utf)
	{
		DeleteArray(w);
		DeleteArray(n);
		#ifdef WIN32
		if (WinNT)
		{
			if (utf) w = Utf8ToWide(utf);
		}
		else
		{
			if (utf) n = LgiToNativeCp(utf);
		}
		#else
		n = NewStr(utf);
		#endif
		
		return *this;
	}

	GNativeString &operator =(GNativeString &s)
	{
		DeleteArray(w);
		DeleteArray(n);
		#ifdef WIN32
		if (WinNT)
		{
			w = NewStrW(s.w);
		}
		else
		{
			n = NewStr(s.n);
		}
		#else
		n = NewStr(s.n);
		#endif
		return *this;
	}

	GNativeString &operator += (GNativeString &add)
	{
		size_t AddSize = add.GetSize();
		if (AddSize)
		{
			size_t Len = GetSize() + AddSize;
			if (WinNT)
			{
				char16 *s = new char16[Len+1];
				if (s)
				{
					if (w)
						StrcpyW(s, w);
					else
						s[0] = 0;
					StrcatW(s, (char16*)add);
					DeleteArray(w);
					w = s;
				}
			}
			else
			{
				char *s = new char[Len+1];
				if (s)
				{
					if (n)
						strcpy(s, n);
					else
						s[0] = 0;
					strcat(s, (char*)add);
					DeleteArray(n);
					n = s;
				}
			}
		}
		return *this;
	}

	operator char *()
	{
		LgiAssert(!WinNT);
		return n;
	}

	operator char16 *()
	{
		LgiAssert(WinNT);
		return w;
	}

	char *NewUtf8()
	{
		if (w)
		{
			return WideToUtf8(w);
		}
		else if (n)
		{
			return LgiFromNativeCp(n);
		}
		return 0;
	}

	size_t GetSize()
	{
		if (n)
		{
			return strlen(n);
		}
		else if (w)
		{
			return StrlenW(w);
		}
		return 0;
	}
};

bool GProcess::Terminate()
{
	#if defined(WIN32)
	return TerminateProcess(d->Pid, -1);
	#else
	LgiAssert(0);
	return false;
	#endif
}

bool GNativeString::WinNT = LgiGetOs() == LGI_OS_WIN32 || LgiGetOs() == LGI_OS_WIN64;

bool GProcess::Run(const char *Exe, const char *Arguments, const char *Dir, bool Wait, GStream *In, GStream *Out, int Priority)
{
	bool Status = false;

	#ifdef WIN32
	char _exe[256];
	if (!LgiGetExtension((char*)Exe))
	{
		sprintf_s(_exe, sizeof(_exe), "%s.exe", Exe);
		Exe = _exe;
	}
	#endif

	GNativeString NExe;
	if (Exe)
	{
		if (FileExists((char*)Exe))
		{
			NExe = Exe;
		}
		else
		{
			GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
			for (int i=0; i<p.Length(); i++)
			{
				char s[256];
				LgiMakePath(s, sizeof(s), p[i], Exe);
				if (FileExists(s))
				{
					NExe = s;
					break;
				}
			}
		}
	}

	if (NExe.GetSize())
	{
		GNativeString Buf;
		GNativeString Delim;
		#ifdef WIN32
		if (FileExists(Exe))
			Delim = "\"";
		#endif
		GNativeString Space = " ";

		GNativeString NPath = Dir;
		GNativeString NArgs = Arguments;
		
		Buf += Delim;
		Buf += NExe;
		Buf += Delim;
		Buf += Space;
		Buf += NArgs;
		// sprintf_s(Buf, sizeof(Buf), "%s%s%s %s", Delim, NExe, Delim, NArgs ? NArgs : (char*)"");
		
		#ifdef WIN32

		HANDLE	hChildStdinRd = 0, hChildStdinWr = 0, hChildStdinWrDup = 0,
				hChildStdoutRd = 0, hChildStdoutWr = 0, hChildStdoutRdDup = 0,
				hInputFile = 0, hSaveStdin = GetStdHandle(STD_INPUT_HANDLE),
				hSaveStdout = GetStdHandle(STD_OUTPUT_HANDLE);

		SECURITY_ATTRIBUTES saAttr;
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = true;
		saAttr.lpSecurityDescriptor = NULL;

		if (!Out ||
			(CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0) &&
			SetStdHandle(STD_OUTPUT_HANDLE, hChildStdoutWr) &&
			DuplicateHandle(GetCurrentProcess(), hChildStdoutRd,
							GetCurrentProcess(), &hChildStdoutRdDup,
							0, FALSE, DUPLICATE_SAME_ACCESS)))
		{
			if (Out)
			{
				CloseHandle(hChildStdoutRd);
			}

			if (!In ||
				(CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0) &&
				SetStdHandle(STD_INPUT_HANDLE, hChildStdinRd) &&
				DuplicateHandle(GetCurrentProcess(), hChildStdinWr,
								GetCurrentProcess(), &hChildStdinWrDup,
								0, false, DUPLICATE_SAME_ACCESS)))
			{
				PROCESS_INFORMATION piProcInfo;
				ZeroObj(piProcInfo);

				STARTUPINFOA SiA;
				STARTUPINFOW SiW;
				ZeroObj(SiA);
				ZeroObj(SiW);

				if (LgiGetOs() == LGI_OS_WIN9X)
				{
					SiA.cb = sizeof(SiA);
					SiA.dwFlags = In || Out ? STARTF_USESTDHANDLES : 0;
					if (Out)
					{
						SiA.hStdOutput = hChildStdoutWr;
						SiA.hStdError = hChildStdoutWr;
					}
					if (In)
					{
						CloseHandle(hChildStdinWr);
						SiA.hStdInput = hChildStdinRd;
					}
				}
				else
				{
					SiW.cb = sizeof(SiW);
					SiW.dwFlags = In || Out ? STARTF_USESTDHANDLES : 0;
					if (Out)
					{
						SiW.hStdOutput = hChildStdoutWr;
						SiW.hStdError = hChildStdoutWr;
					}
					if (In)
					{
						CloseHandle(hChildStdinWr);
						SiW.hStdInput = hChildStdinRd;
					}
				}

				GNativeString StartingPath;
				if (NPath.GetSize())
				{
					StartingPath = NPath;
				}
				else
				{
					char *u = NExe.NewUtf8();
					if (u)
					{
						LgiTrimDir(u);
						StartingPath = u;
						DeleteArray(u);
					}
				}

				DWORD Flags = Wait ? CREATE_NO_WINDOW : 0;
				switch (Priority)
				{
					case -2:
					{
						Flags |= IDLE_PRIORITY_CLASS;
						break;
					}
					case -1:
					{
						Flags |= 0x00004000;
						break;
					}
					case 1:
					{
						Flags |= HIGH_PRIORITY_CLASS;
						break;
					}
					case 2:
					{
						Flags |= REALTIME_PRIORITY_CLASS;
						break;
					}
				}

				bool Ok = false;				
				if (LgiGetOs() == LGI_OS_WIN9X)
				{
					Ok = CreateProcessA(0, 
										Buf,			// command line 
										NULL,			// process security attributes 
										NULL,			// primary thread security attributes 
										TRUE,			// handles are inherited 
										Flags,			// Flags
										NULL,			// use parent's environment 
										StartingPath,	// use parent's current directory 
										&SiA,			// STARTUPINFO pointer 
										&piProcInfo);	// receives PROCESS_INFORMATION 
				}
				else
				{
					Ok = CreateProcessW(0, 
										Buf,			// command line 
										NULL,			// process security attributes 
										NULL,			// primary thread security attributes 
										TRUE,			// handles are inherited 
										Flags,			// Flags
										NULL,			// use parent's environment 
										StartingPath,	// use parent's current directory 
										&SiW,			// STARTUPINFO pointer 
										&piProcInfo);	// receives PROCESS_INFORMATION 
				}

				d->ProcessId = piProcInfo.dwProcessId;

				if (!Ok)
				{
					d->ErrorCode = GetLastError();
				}

				SetStdHandle(STD_INPUT_HANDLE, hSaveStdin);
				SetStdHandle(STD_OUTPUT_HANDLE, hSaveStdout);
				CloseHandle(hChildStdoutWr);
 
				if (Ok)
				{
					Status = true;
					d->Pid = piProcInfo.hProcess;
					
					// Write stdin data into app...
					if (In)
					{
						if (In)
						{
							int r;
							char Buf[256];
							while ((r = In->Read(Buf, sizeof(Buf))) > 0)
							{
								DWORD w = 0;
								WriteFile(hChildStdinWrDup, Buf, r, &w, 0);								
							}
						}

						CloseHandle(hChildStdinWrDup);
					}

					if (Wait)
					{
						do
						{
							bool Running = IsRunning();

							if (Out)
							{
								// Look in the read queue
								char Buf[256] = "";
								DWORD r = 0;
								while (ReadFile(hChildStdoutRdDup, Buf, sizeof(Buf), &r, 0) && r > 0)
								{
									Out->Write(Buf, r);
								}
							}

							// Check it's not exited..
							if (!Running)
							{
								break;
							}

							// Don't use the cpu
							LgiSleep(100);
						}
						while (true);
					}
				}
			}
		}

		#else // Linux/Mac etc
		
		struct Pipe
		{
			int Read;
			int Write;
			
			Pipe()
			{
				Read = -1;
				Write = -1;
			}
		};
		
		Pipe Read;
		Pipe Write;
		Pipe Error;
		
		GArray<char*> a;
		a.Add((char*)Exe);
		for (const char *s=Arguments; s && *s; )
		{
			// skip white
			while (*s && strchr(" \t\r\n", *s)) s++;
			
			// Find token
			GStringPipe p(256);
			const char *n;
			char d = 0;
			if (strchr("\"\'", *s))
			{
				d = *s++;
				n = strchr(s, d);
				if (!n) n = s + strlen(s);
				p.Push(s, n-s);
			}
			else
			{
				n = s;
				while (*n)
				{
					if (*n == '\\' && n[1] == ' ')
					{
						n++;
					}
					else if (strchr(" \t\r\n", *n))
					{
						break;
					}
					
					p.Push(n, 1);
					
					n++;
				}
			}
			
			char *Arg = p.NewStr();
			if (Arg) a.Add(Arg);

			if (d) n++;
			s = n;
		}
		
		char **Args = &a[0];		
		if (Args)
		{
			char ExeName[256] = "";
			if (!FileExists(Exe))
			{
				// Find exe in path
				GToken Path(getenv("PATH"), ":");
				for (int i=0; i<Path.Length(); i++)
				{
					LgiMakePath(ExeName, sizeof(ExeName), Path[i], Exe);
					if (FileExists(ExeName))
					{
						Exe = ExeName;
						break;
					}
				}
			}
			
			pipe((int*)&Read);
			pipe((int*)&Error);
			if (In)
			{
				// Has stdin pipe
				pipe((int*)&Write);
				if (!(d->Pid = fork()))
				{
					// Child process
					if (Dir)
					{
						chdir(Dir);
					}

					// stdin -> Write
					close(0);			// close stdin
					dup(Write.Read);
					close(Write.Write);

					// stdout -> Read
					close(1);			// close stdout
					dup(Read.Write);
					close(Read.Read);

					// stderr -> Error
					close(2);			// close stderr
					dup(Error.Write);
					close(Error.Read);

					// setup read & write handles
					printf("execv(%s, ...)\n", Exe);
					execv(Exe, Args);

					// We should never get here
					printf("execv(%s) failed.\n", Exe);
					exit(-1);
				}

				close(Write.Read);
			}
			else
			{
				// Just output
				if (!(d->Pid = fork()))
				{
					// Child process
					if (Dir)
					{
						chdir(Dir);
					}

					// stdout -> Read
					dup2(Read.Write, fileno(stdout));
					close(Read.Write);
					close(Read.Read);

					// stderr -> Error
					dup2(Error.Write, fileno(stderr));
					close(Error.Write);
					close(Error.Read);

					execv(Exe, Args);

					// We should never get here
					printf("execv(%s) failed.\n", Exe);
					exit(-1);
				}
			}
			close(Read.Write);
			close(Error.Write);

			Status = true;

			ssize_t r;
			if (In && In->GetSize() > 0)
			{
				/*
				while (!TestFlag(hndstate(Read.Read), POLLOUT))
				{
					LgiSleep(IO_WAIT);
				}
				*/

				char Buf[256];
				while ((r = In->Read(Buf, sizeof(Buf))) > 0)
				{
					write(Write.Write, Buf, r);
				}
			}

			while (Wait)
			{
				// printf("parent: IsRunning()\n");
				bool Running = IsRunning();

				if (Out)
				{
					char Buf[256];
					
					while (	TestFlag(hndstate(Read.Read), POLLIN) &&
							(r = read(Read.Read, Buf, sizeof(Buf)-1)) > 0)
					{
						Buf[r] = 0;

						// printf("stdout: %s\n", Buf);

						Out->Write(Buf, (int)r);
					}

					while (	TestFlag(hndstate(Error.Read), POLLIN) &&
							(r = read(Error.Read, Buf, sizeof(Buf)-1)) > 0)
					{
						Buf[r] = 0;

						// printf("stderr: %s\n", Buf);

						Out->Write(Buf, (int)r);
					}
				}

				if (!Running)
				{
					break;
				}				

				LgiSleep(IO_WAIT);
			}

			if (In)
			{
				close(Write.Write);
			}

			close(Read.Read);
			close(Error.Read);

			for (char **a = Args + 1; *a; a++)
			{
				DeleteArray(*a);
			}
		}
		#endif
	}
	else
	{
		printf("%s,%i - Not a valid exe.\n", __FILE__, __LINE__); 
	}
	
	return Status;
}
