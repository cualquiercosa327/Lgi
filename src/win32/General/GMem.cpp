/*hdr
**      FILE:           Memory.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           10/4/98
**      DESCRIPTION:    Memory subsystem
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "GMem.h"
#include "process.h"

#ifdef LGI_MEM_DEBUG

#include "LgiDefs.h"
#include "ImageHlp.h"
#include <direct.h>

#undef malloc
#undef realloc
#undef free

#define MEM_TRACKING				1
#define MEM_STACK_SIZE				8
#define MEM_FILL					0xcccccccc

#ifndef _WIN64
#define mem_assert(c)				if (!(c)) _asm int 3
#else
#define mem_assert					LgiAssert
#endif

struct stack_addr
{
	UNativeInt Ip;
};

struct block
{
	block *Next, *Prev;
	uint32 Size;
	stack_addr Stack[MEM_STACK_SIZE];
	uint32 PreBytes;
};

static bool DoLeakCheck = true;
static bool MemInit = false;
static uint BlockCount = 0;
static block *First = 0;
static block *Last = 0;
static CRITICAL_SECTION Lock;

void LgiSetLeakDetect(bool On)
{
	DoLeakCheck = On;
}

#pragma warning(disable:4074) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg(compiler)
struct GLeakCheck
{
	GLeakCheck()
	{
		InitializeCriticalSection(&Lock);
		MemInit = true;
	}

	~GLeakCheck()
	{
		char DefName[] = "leaks.mem";
		if (First && DoLeakCheck)
		{
			LgiDumpMemoryStats(DefName);
		}
		else
		{
			unlink(DefName);
		}

		DeleteCriticalSection(&Lock);
		MemInit = false;
	}

} _LeakCheck;

typedef struct _CALL_FRAME
{
   struct _CALL_FRAME* Next;
   void*               ReturnAddress;
} CALL_FRAME, * PCALL_FRAME;

LONG
StackwalkExceptionHandler(PEXCEPTION_POINTERS ExceptionPointers)
{
   if (ExceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
      return EXCEPTION_EXECUTE_HANDLER;

   return EXCEPTION_CONTINUE_SEARCH;
}

void *lgi_malloc(size_t size)
{
	#if MEM_TRACKING
	LgiAssert(MemInit);

	if (size > 0)
	{
		void *Ret = 0;
		EnterCriticalSection(&Lock);

		int total = sizeof(block) + sizeof(int32) + size;
		block *b = (block*) malloc(total);
		if (b)
		{
			// Init the structure
			b->Size = size;
			b->PreBytes = MEM_FILL;
			memset(b->Stack, 0, sizeof(b->Stack));

			// Save the stack trace

			void *_ebp = 0;
			#ifndef _WIN64
			_asm {
				mov eax, ebp
				mov _ebp, eax
			}

			#if 1
			PCALL_FRAME frame = (PCALL_FRAME)_ebp;
			__try
			{
				for (int i=0; i<=MEM_STACK_SIZE; i++)
				{
					if (!frame || (*((unsigned*)frame) & 0x3))
						break;

					if (i)
						b->Stack[i-1].Ip = (UNativeInt) *((uint8**)frame + 1);
					frame = frame->Next;
				}
			}
			__except(StackwalkExceptionHandler(GetExceptionInformation()))
			{
			}
			#else
			for (int i=-1; i<MEM_STACK_SIZE; i++)
			{
				if ((RegEbp & 3) != 0 ||
					IsBadReadPtr( (void*)RegEbp, 8 ))
					break;

				if (i >= 0)
					b->Stack[i].Ip = (uint) *((uint8**)RegEbp + 1);
				RegEbp = *(uint*)RegEbp;
			}
			#endif
			#endif

			// Add to linked list
			if (Last)
			{
				Last->Next = b;
				b->Prev = Last;
				Last = b;
			}
			else
			{
				First = Last = b;
				b->Prev = 0;
			}
			b->Next = 0;
			BlockCount++;

			// Return the data area
			Ret = (b + 1);
		}

		LeaveCriticalSection(&Lock);
		return Ret;
	}
	#else
	if (size > 0)
		return malloc(size);
	#endif

	return 0;
}

void *lgi_realloc(void *ptr, size_t size)
{
	#if MEM_TRACKING

	if (ptr)
	{
		void *ret = 0;
		EnterCriticalSection(&Lock);

		block *b = ((block*)ptr) - 1;
		if (b)
		{
			block *prev = b->Prev;
			block *next = b->Next;

			int total = sizeof(block) + sizeof(int32) + size;
			block *n = (block*)realloc(b, total);
			if (n)
			{
				n->Size = size;

				if (prev)
					prev->Next = n;
				else
					First = n;
				LgiAssert(n->Prev == prev);

				if (next)
					next->Prev = n;
				else
					Last = n;
				LgiAssert(n->Next == next);

				ret = n + 1;
			}
		}

		LeaveCriticalSection(&Lock);
		return ret;
	}

	#endif

	return 0;
}

void lgi_free(void *ptr)
{
	#if MEM_TRACKING
	if (ptr)
	{
		EnterCriticalSection(&Lock);

		block *b = ((block*)ptr) - 1;
		if (b)
		{
			// Check struct
			if (b->PreBytes != MEM_FILL)
			{
				mem_assert(!"Pre-fill bytes wrong");
			}

			// Remove from list
			if (b == First)
			{
				mem_assert(b->Prev == 0);
				if (b->Next)
				{
					b->Next->Prev = 0;
				}
				First = b->Next;
			}
			else if (b == Last)
			{
				mem_assert(b->Next == 0);
				if (b->Prev)
				{
					b->Prev->Next = 0;
				}
				Last = b->Prev;
			}
			else
			{
				mem_assert(b->Next != 0);
				if (b->Next)
				{
					b->Next->Prev = b->Prev;
				}
				mem_assert(b->Prev != 0);
				if (b->Prev)
				{
					b->Prev->Next = b->Next;
				}
			}
			BlockCount--;

			// Free the memory
			free(b);
		}

		LeaveCriticalSection(&Lock);
	}
	#else
	if (ptr)
		free(ptr);
	#endif
}

typedef BOOL (__stdcall *SYMINITIALIZEPROC)( HANDLE, LPSTR, BOOL );
typedef BOOL (__stdcall *SYMCLEANUPPROC)( HANDLE );

typedef BOOL (__stdcall * STACKWALKPROC)
            ( DWORD, HANDLE, HANDLE, LPSTACKFRAME, LPVOID,
             PREAD_PROCESS_MEMORY_ROUTINE,PFUNCTION_TABLE_ACCESS_ROUTINE,
             PGET_MODULE_BASE_ROUTINE, PTRANSLATE_ADDRESS_ROUTINE );

typedef LPVOID (__stdcall *SYMFUNCTIONTABLEACCESSPROC)( HANDLE, DWORD );

typedef DWORD (__stdcall *SYMGETMODULEBASEPROC)( HANDLE, DWORD );

typedef BOOL (__stdcall *SYMGETSYMFROMADDRPROC)
                             ( HANDLE, DWORD, PDWORD, PIMAGEHLP_SYMBOL );

typedef BOOL (__stdcall *proc_SymGetLineFromAddr)(	HANDLE hProcess,
													DWORD dwAddr,
													PDWORD pdwDisplacement,
													PIMAGEHLP_LINE Line);

typedef DWORD (__stdcall *proc_SymGetOptions)(VOID);
typedef DWORD (__stdcall *proc_SymSetOptions)(DWORD SymOptions);

bool LgiDumpMemoryStats(char *filename)
{
	bool Status = true;

	HMODULE DbgHelp = LoadLibrary("dbghelp.dll");
	if (DbgHelp)
	{
		HANDLE hProcess = GetCurrentProcess();

		SYMINITIALIZEPROC			SymInitialize;
		SYMCLEANUPPROC				SymCleanup;
		STACKWALKPROC				StackWalk;
		SYMFUNCTIONTABLEACCESSPROC	SymFunctionTableAccess;
		SYMGETMODULEBASEPROC		SymGetModuleBase;
		SYMGETSYMFROMADDRPROC		SymGetSymFromAddr;
		proc_SymGetLineFromAddr		SymGetLineFromAddr;
		proc_SymGetOptions			SymGetOptions;
		proc_SymSetOptions			SymSetOptions;

		SymInitialize = (SYMINITIALIZEPROC) GetProcAddress(DbgHelp, "SymInitialize");
		SymCleanup = (SYMCLEANUPPROC) GetProcAddress(DbgHelp, "SymCleanup");
		StackWalk = (STACKWALKPROC) GetProcAddress(DbgHelp, "StackWalk");
		SymFunctionTableAccess = (SYMFUNCTIONTABLEACCESSPROC) GetProcAddress(DbgHelp, "SymFunctionTableAccess");
		SymGetModuleBase = (SYMGETMODULEBASEPROC) GetProcAddress(DbgHelp, "SymGetModuleBase");
		SymGetSymFromAddr = (SYMGETSYMFROMADDRPROC) GetProcAddress(DbgHelp, "SymGetSymFromAddr");
		SymGetLineFromAddr = (proc_SymGetLineFromAddr) GetProcAddress(DbgHelp, "SymGetLineFromAddr");
		SymGetOptions = (proc_SymGetOptions) GetProcAddress(DbgHelp, "SymGetOptions");
		SymSetOptions = (proc_SymSetOptions) GetProcAddress(DbgHelp, "SymSetOptions");

		DWORD dwOpts = SymGetOptions();
		DWORD Set = SymSetOptions(dwOpts | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST);

		char s[512] = "";
		GetModuleFileName(NULL, s, sizeof(s));
		char *end = strrchr(s, '\\');
		if (end)
		{
			*end = 0;
		}

		char *path = getenv("path");
		int pathlen = strlen(path);
		
		char *all = (char*)malloc(pathlen + strlen(s) + 256);
		if (all)
		{
			strcpy(all, path);
			if (all[strlen(all)-1] != ';')
				strcat(all, ";");
			strcat(all, s);

			if (SymInitialize(hProcess, all, true))
			{
			    char FullPath[MAX_PATH];
			    _getcwd(FullPath, sizeof(FullPath));
			    strcat(FullPath, "\\");
			    strcat(FullPath, filename ? filename : "stats.mem");
			    
				FILE *f = fopen(FullPath, "w");
				if (f)
				{
					block *b;

					uint TotalBytes = 0;
					for (b = First; b; b = b->Next)
					{
						TotalBytes += b->Size;
					}

					sprintf(s, "%i bytes (%.1f MB) in %i blocks:\n\n", TotalBytes, (double)TotalBytes / 1024 / 1024, BlockCount);
					fwrite(s, 1, strlen(s), f);
					OutputDebugString(s);

					int64 Start = GetTickCount();
					int64 Last = Start;
					uint Current = 0;
					for (b = First; b; b = b->Next, Current++)
					{
						int64 Now = GetTickCount();
						if (Now > Last + 1000)
						{
							Last = Now;
							double Sec = (double)(Now-Start)/1000;
							sprintf(s, "Dumping %.1f%%, %i of %i (%i/sec)\n",
										(double)Current * 100 / BlockCount,
										Current,
										BlockCount,
										(int)(Current/Sec));
							OutputDebugString(s);
						}

						uint8 *Data = (uint8*)(b + 1);
						char *Str = s + sprintf(s, "Block %p, %i bytes = {", b + 1, b->Size);
						for (int n=0; n<b->Size AND n<16; n++)
						{
							if (Data[n] >= ' ' AND Data[n] < 127)
							{
								Str += sprintf(Str, "'%c', ", Data[n]);
							}
							else
							{
								Str += sprintf(Str, "0x%02.2x, ", (int)Data[n]);
							}
						}
						if (b->Size > 16)
						{
							Str += sprintf(Str, "...}\n");
						}
						else
						{
							Str += sprintf(Str, "}\n");
						}
						fwrite(s, 1, Str - s, f);

						for (int i=0; b->Stack[i].Ip AND i<MEM_STACK_SIZE; i++)
						{
							MEMORY_BASIC_INFORMATION mbi;
							VirtualQuery((void*)b->Stack[i].Ip, &mbi, sizeof(mbi));
							HINSTANCE Pid = (HINSTANCE)mbi.AllocationBase;

							char module[256] = "";
							if (GetModuleFileName(Pid, module, sizeof(module)))
							{
								BYTE symbolBuffer[ sizeof(IMAGEHLP_SYMBOL) + 512 ];
								PIMAGEHLP_SYMBOL pSymbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
								DWORD symDisplacement = 0;
								IMAGEHLP_LINE Line;
								DWORD Offset = b->Stack[i].Ip - (UNativeInt)Pid;

								pSymbol->SizeOfStruct = sizeof(symbolBuffer);
								pSymbol->MaxNameLength = 512;
								memset(&Line, 0, sizeof(Line));
								Line.SizeOfStruct = sizeof(Line);

								char *mod = strrchr(module, '\\');
								if (mod) mod++;
								else mod = module;

								if (SymGetSymFromAddr(hProcess, b->Stack[i].Ip, &symDisplacement, pSymbol))
								{
									if (SymGetLineFromAddr(hProcess, b->Stack[i].Ip, &symDisplacement, &Line))
									{
										sprintf(s, "\t%s %s:%i\n", mod, Line.FileName, Line.LineNumber);
									}
									else
									{
										if (pSymbol->Name[0] == '$')
											break;

										sprintf(s, "\t%s %s+0x%x\n", mod, pSymbol->Name, symDisplacement);
										// dumpBuffer.Printf("%p: %s Offset: 0x%X (%hs)\n", caller, module, symDisplacement, pSymbol->Name);
									}
								}
								else
								{
									sprintf(s, "\t%s 0x%x\n", mod, b->Stack[i].Ip);
								}

								fwrite(s, 1, strlen(s), f);
							}
						}

						fwrite("\n", 1, 1, f);
					}

					fclose(f);
				}
				else Status = false;

				SymCleanup(hProcess);
			}
			else Status = false;
		}

		FreeLibrary(DbgHelp);
	}
	else Status = false;

	return Status;
}

void *operator new(size_t size)
{
	return lgi_malloc(size);
}

void *operator new[](size_t size)
{
	return lgi_malloc(size);
}

void operator delete(void *p)
{
	lgi_free(p);
}

void operator delete[](void *p)
{
	lgi_free(p);
}

#else

bool LgiDumpMemoryStats(char *f)
{
	return false;
}

void LgiSetLeakDetect(bool On)
{
}

#endif // LGI_MEM_DEBUG


/*
#ifdef MEMORY_DEBUG

#if 0

#define _malloc(s)		VirtualAlloc(0, s, MEM_COMMIT, PAGE_READWRITE)
#define _free(p)		VirtualFree(p, 0, MEM_RELEASE);

#else

#include <malloc.h>
#define abs(a)			((a)>0 ? (a) : -(a))
#define _malloc(s)		malloc(s)
#define _free(p)		free(p);

#endif

char *_vmem_file = 0;
int _vmem_line = 0;

#include <crtdbg.h>

#define _VMEM_MAGIC		0x12345678
#define _VMEM_BUFFER	16
#define _VMEM_FILL		0xcdcdcdcd

#ifdef _METRICS
class _vmem_metrics
{
	class _metric
	{
	public:
		char *File;
		int Line;
		int Count;

		_metric()
		{
			File = 0;
			Line = 0;
			Count = 0;
		}
	};

	int Length;
	_metric *m;
	GMutex Lock;

public:
	_vmem_metrics()
	{
		Length = 16 << 10;
		
		int Bytes = sizeof(_metric) * Length;
		m = (_metric*) malloc(Bytes);
		if (m)
		{
			memset(m, 0, Bytes);
		}
	}

	~_vmem_metrics()
	{
		for (int i=0; i<10; i++)
		{
			_metric *Max = m;
			for (int n=0; n<Length AND m[n].File; n++)
			{
				if (m[n].Count > Max->Count)
				{
					Max = m + n;
				}
			}

			if (Max->Count)
			{
				char s[256];
				sprintf(s, "Metrics: %s,%i Count: %i\n", Max->File, Max->Line, Max->Count);
				OutputDebugString(s);
				Max->Count = 0;
			}
		}

		free(m);
	}

	void OnAlloc(char *file, int line)
	{
		if (Lock.Lock())
		{
			int i;
			for (i=0; i<Length AND m[i].File; i++)
			{
				if (stricmp(m[i].File, file) == 0 AND
				// if (m[i].File == file AND
					m[i].Line == line)
				{
					m[i].Count++;
					return;
				}
			}

			if (i<Length)
			{
				m[i].File = file;
				m[i].Line = line;
				m[i].Count = 1;
			}

			Lock.Unlock();
		}
	}

} VmemMetrics;
#endif

struct _vmem_info
{
	int Magic;
	char *File;
	int Line;
	int Size;
	_vmem_info *Prev, *Next;

	bool CheckFill()
	{
		uint32 *Pre = (uint32*)(this+1);
		uint32 *Post = (uint32*) (((uchar*)Pre) + Size + _VMEM_BUFFER);
		uint32 *e;

		for (e = Pre+(_VMEM_BUFFER/sizeof(uint32)); Pre < e; Pre++)
		{
			if (*Pre != _VMEM_FILL) return false;
		}

		for (e = Post+(_VMEM_BUFFER/sizeof(uint32)); Post < e; Post++)
		{
			if (*Post != _VMEM_FILL) return false;
		}

		return true;
	}

	void Dump()
	{
		char s[256];
		sprintf(s, "Leak: %s,%i (%i bytes, '", File, Line, Size);

		char *Data = ((char*)(this+1)) + _VMEM_BUFFER;
		char *e = s + strlen(s);
		for (int i=0; i<min(Size, 32) AND Data[i]; i++)
		{
			if (Data[i] >= ' ')
				*e++ = Data[i];
			else
				*e++ = '.';
		}
		strcpy(e, "')\n");

		OutputDebugString(s);
	}
};

int _used = 0;
int _max_used = 0;
_vmem_info *_First, *_Last;

class _VMemCleanup
{
public:
	_VMemCleanup()
	{
		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_WNDW);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
	}

	~_VMemCleanup()
	{
		LgiAssert(LgiCheckHeap());

		char s[256];
		if (_First)
		{
			sprintf(s, "Memory Leaks: %i (Max blocks %i)\n", _used, _max_used);
			OutputDebugString(s);

			int k=0;
			for (_vmem_info *i=_First; i; k++)
			{
				i->Dump();
				void *p = i;
				i = i->Next;
				_free(p);
			}
		}
		else
		{
			sprintf(s, "No Memory Leaks. (Max blocks %i)\n", _max_used);
			OutputDebugString(s);
		}
	}

} VMemCleanup;

int _Allocated = 0;

void *_vmem_alloc(size_t Size, char *file, int line)
{
	int TotalSize = Size + (_VMEM_BUFFER * 2);
	_vmem_info *p = (_vmem_info*)_malloc(sizeof(_vmem_info) + TotalSize);
	if (p)
	{
		#ifdef _METRICS
		VmemMetrics.OnAlloc(file, line);
		#endif

		// set the members
		p->Magic = _VMEM_MAGIC;
		p->File = file;
		p->Line = line;
		p->Size = Size;
		_used++;
		_max_used = max(_used, _max_used);

		// insert into the list
		if (NOT _First)
		{
			_First = _Last = p;
			p->Prev = p->Next = 0;
		}
		else
		{
			p->Prev = 0;
			p->Next = _First;
			_First->Prev = p;
			_First = p;
		}

		return ((uchar*)(p+1)) + _VMEM_BUFFER;
	}
	else
	{
		// out of memory
		_asm int 3
	}

	return 0;
}

void _vmem_free(void *ptr)
{
	if (ptr)
	{
		_vmem_info *p = ((_vmem_info*) ( ((uchar*)ptr) - _VMEM_BUFFER) ) - 1;

		if (p->Magic == _VMEM_MAGIC)
		{
			// Check fill
			if (NOT p->CheckFill())
			{
				_asm int 3
			}

			// Remove from the list
			if (p == _First AND p == _Last)
			{
				_First = _Last = 0;
			}
			else if (p == _First)
			{
				if (p->Prev) _asm int 3

				_First = p->Next;
				if (_First) _First->Prev = 0;
			}
			else if (p == _Last)
			{
				if (p->Next) _asm int 3

				_Last = p->Prev;
				if (p->Prev) p->Prev->Next = 0;
			}
			else
			{
				if (NOT p->Next OR NOT p->Prev) _asm int 3

				p->Next->Prev = p->Prev;
				p->Prev->Next = p->Next;
			}
			p->Prev = p->Next = 0;

			_used--;

			_free(p);
		}
		else
		{
			// this pointer wasn't allocated by us
			_asm int 3
		}
	}
	else
	{
		// no pointer
		_asm int 3
	}
}
#endif
*/

bool LgiCheckHeap()
{
	#ifdef MEMORY_DEBUG

	_vmem_info *i = 0;
	int n = 0;
	int MinD = 0x7fffffff;
	try
	{
		for (i=_First; i; i=i->Next, n++)
		{
			char c = i->File[0];

			if (i->Magic != _VMEM_MAGIC OR
				NOT i->CheckFill())
			{
				_asm int 3
			}
		}
	}
	catch (...)
	{
		_asm int 3
		return false;
	}

	#endif

	#ifdef _MSC_VER
	if (_heapchk() != _HEAPOK)
	{
		assert(!"Heap not ok.");
		return false;
	}
	#endif

	return true;
}

bool LgiCanReadMemory(void *p, int Len)
{
	/*
	try
	{
		int i = 0;
		for (char *Ptr = (char*)p; Len > 0; Len--)
		{
			i += *Ptr++;
		}
	}
	catch (...)
	{
		// I guess not... then..
		return false;
	}
	*/

	return true;
}
