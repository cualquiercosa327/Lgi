#include <math.h>
#include "Lgi.h"
#include "GScripting.h"
#include "GScriptingPriv.h"
#include "GBox.h"
#include "GTabView.h"
#include "GTextLog.h"
#include "GList.h"
#include "GToolBar.h"
#include "GToken.h"
#include "GTableLayout.h"
#include "GTextLabel.h"
#include "GScrollBar.h"

#define TIME_INSTRUCTIONS		0
#define POST_EXECUTE_STATE		0

// #define BREAK_POINT				0x0000009F

#define ExitScriptExecution		c.u8 = e
#define SetScriptError			c.u8 = e; Status = ScriptError
#define CurrentScriptAddress	(c.u8 - Base)
#define CheckParam(ptr)			if (!(ptr)) \
								{ \
									OnException(_FL, CurrentScriptAddress-1, #ptr); \
									c.u8 = e; \
									Status = ScriptError; \
									break; \
								}

#define AddLocalSize(NewSize) \
	int LocalsBase = Locals.Length(); \
	Locals.SetFixedLength(false); \
	/* LgiTrace("%s:%i - Locals %i -> %i\n", _FL, LocalsBase, LocalsBase + NewSize); */ \
	Locals.Length(LocalsBase + NewSize); \
	Scope[SCOPE_LOCAL] = &Locals[LocalsBase]; \
	Locals.SetFixedLength();


#ifdef WIN32
extern "C" uint64 __cdecl CallExtern64(void *FuncAddr, NativeInt *Ret, uint32 Args, void *Arg);
#endif

int GVariantCmp(GVariant *a, GVariant *b, NativeInt Data)
{
	GVariant *Param = (GVariant*) Data;
	if (!a || !b)
		return 0;
		
	if (a->Type == GV_STRING &&
		b->Type == GV_STRING)
	{	
		const char *Empty = "";
		const char *as = a->Str();
		const char *bs = b->Str();

		return _stricmp(as?as:Empty, bs?bs:Empty);
	}
	else if (a->Type == GV_DOM &&
			 b->Type == GV_DOM &&
			 Param)
	{
		const char *Fld = Param->Str();
		GVariant av, bv;
		if (a->Value.Dom->GetValue(Fld, av) &&
			b->Value.Dom->GetValue(Fld, bv))
		{
			return GVariantCmp(&av, &bv, 0);
		}
	}
	else if (a->Type == GV_INT32 &&
			 b->Type == GV_INT32)
	{
		return a->CastInt32() - b->CastInt32();
	}
	else
	{
		LgiAssert(!"Impl a handler for this type.");
	}
	
	return 0;
}

inline GVariantType DecidePrecision(GVariantType a, GVariantType b)
{
	if (a == GV_DOUBLE || b == GV_DOUBLE)
		return GV_DOUBLE;

	if (a == GV_INT64 || b == GV_INT64)
		return GV_INT64;

	return GV_INT32;
}

inline GVariantType ComparePrecision(GVariantType a, GVariantType b)
{
	if (a == GV_NULL || b == GV_NULL)
		return GV_NULL;

	if (a == GV_DATETIME && b == GV_DATETIME)
		return GV_DATETIME;
	
	if (a == GV_DOUBLE || b == GV_DOUBLE)
		return GV_DOUBLE;

	if (a == GV_STRING || b == GV_STRING)
		return GV_STRING;

	if (a == GV_INT64 || b == GV_INT64)
		return GV_INT64;

	return GV_INT32;
}

inline int CompareVariants(GVariant *a, GVariant *b)
{
	// Calculates "a - b"

	switch (ComparePrecision(a->Type, b->Type))
	{
		case GV_DATETIME:
			return a->Value.Date->Compare(b->Value.Date);
			break;
		case GV_DOUBLE:
		{
			double d = a->CastDouble() - b->CastDouble();
			if (d < 0.0)
				return -1;
			return d > 0.0;
		}
		case GV_STRING:
		{
			char *A = a->Str();
			char *B = b->Str();
			if (!A || !B)
				return -1;
			else
				return strcmp(A, B);
			break;
		}
		case GV_INT64:
		{
			int64 d = a->CastInt64() - b->CastInt64();
			if (d < 0)
				return -1;
			return d > 0;
		}
		default:
			return a->CastInt32() - b->CastInt32();
			break;
	}
}

GExecutionStatus GExternFunc::Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args)
{
	if (!Lib || !Method)
		return ScriptError;

	GStream *Log = Ctx ? Ctx->GetLog() : NULL;
	if (Args.Length() != ArgType.Length())
	{
		if (Log)
			Log->Print("Error: Extern '%s.%s' expecting %i arguments, not %i given.\n",
						Lib.Get(), Method.Get(),
						ArgType.Length(),
						Args.Length());
		return ScriptError;
	}

	GArray<NativeInt> Val;
	GArray<char*> Mem;
	bool UnsupportedArg = false;

	Val.Length(Args.Length() << 1);
	GPointer Ptr;
	Ptr.ni = &Val[0];
	for (unsigned i=0; !UnsupportedArg && i<Args.Length(); i++)
	{
		GVariant *v = Args[i];
		ExternType &t = ArgType[i];
		if (!v)
			return ScriptError;
		
		if (t.Ptr)
		{
			// Allocate some memory for the pointer.
			switch (t.Base)
			{
				case GV_STRING:
				{
					if (t.Out)
					{
						void *cp = v->CastVoidPtr();
						*Ptr.vp++ = cp;
					}
					else
					{
						char *s = NewStr(v->CastString());
						if (!s)
						{
							UnsupportedArg = true;
							break;
						}
						
						Mem.Add(s);
						*Ptr.vp++ = s;
					}					
					break;
				}
				case GV_VOID_PTR:
				{
					*Ptr.vp++ = v->CastVoidPtr();
					break;
				}
				default:
				{
					UnsupportedArg = true;
					break;
				}
			}
		}
		else
		{
			// Plain type
			switch (t.Base)
			{
				case GV_INT32:
				{
					#if defined(_WIN64)
					*Ptr.s64++ = v->CastInt32();
					#else
					*Ptr.s32++ = v->CastInt32();
					#endif
					break;
				}
				case GV_INT64:
				{
					*Ptr.s64++ = v->CastInt64();
					break;
				}
				default:
				{
					UnsupportedArg = true;
					break;
				}
			}
		}
	}

	GLibrary Library(Lib);
	if (!Library.IsLoaded())
	{
		if (Log)
			Log->Print("Error: Extern library '%s' missing.\n", Lib.Get());
		return ScriptError;
	}
			
	void *c = Library.GetAddress(Method);
	if (!c)
	{
		if (Log)
			Log->Print("Error: Extern library '%s' has no method '%s'.\n", Lib.Get(), Method.Get());
		return ScriptError;
	}
	
	uint32 a = Ptr.ni - &Val[0];
	NativeInt r = 0;

	#if defined(_MSC_VER)
		#if defined(_WIN64)
			// 64bit... boooo no inline asm!
			void *b = &Val[0];
			r = CallExtern64(c, &r, a, b);
		#else
			// 32bit... yay inline asm!
			void *b = Ptr.ni - 1;
			_asm
			{
				mov ecx, a
				mov ebx, b
			}
			label1:
			_asm
			{
				push [ebx]
				sub ebx, 4
				loop label1
				
				mov ebx, c
				call ebx
				mov r, eax
			}
		#endif
	#elif defined(MAC)
		#ifdef COCOA
		#warning FIXME
		#else
		// 32bit only
		void *b = Ptr.ni - 1;
        asm (	"movl %2, %%ecx;"
				"movl %3, %%ebx;"
				"label1:"
				"pushl (%%ebx);"
				"subl %%ebx, 4;"
				"loop label1;"
				"call *%1;"
				:"=a"(r)				/* output */
				:"r"(c), "r"(a), "r"(b)	/* input */
				:"%eax", "%ecx", "%ebx"	/* clobbered register */
				);
		#endif
	#else
		// Not implemented, gcc???
		LgiAssert(0);
	#endif

	if (Ret) (*Ret) = (int) r;
	for (unsigned i=0; i<Args.Length(); i++)
	{
		GVariant *v = Args[i];
		ExternType &t = ArgType[i];
		if (t.Out)
		{
			if (v->Type == GV_BINARY &&
				v->Value.Binary.Data != NULL &&
				t.Base == GV_STRING)
			{
				// Cast the type back to t.Base
				char *p = (char*)v->Value.Binary.Data;
				v->Type = t.Base;
				v->Value.String = p;
			}
		}
	}	

	Mem.DeleteArrays();

	return ScriptSuccess;
}

struct CodeBlock
{
	unsigned SrcLine;
	GArray<unsigned> AsmAddr;
	unsigned ViewLine;

	GAutoString Source;
	int SrcLines;

	GAutoString Asm;
	int AsmLines;
};

class GVirtualMachinePriv : public GRefCount
{
	GVariant ArrayTemp;
	
	char *CastArrayIndex(GVariant *Idx)	
	{
		if (Idx == NULL || Idx->Type == GV_NULL)
			return NULL;

		if (Idx->Type == GV_STRING)
			return Idx->Str();
		
		ArrayTemp = *Idx;
		return ArrayTemp.CastString();
	}

public:
	struct StackFrame
	{
		uint32 CurrentFrameSize;
		int PrevFrameStart;
		size_t ReturnIp;
		GVarRef ReturnValue;
	};

	enum RunType
	{
		RunContinue,
		RunStepInstruction,
		RunStepLine,
		RunStepOut
	};

	GStream *Log;
	GCompiledCode *Code;
	GExecutionStatus Status;
	GPtr c;
	GVariant Reg[MAX_REGISTER];
	GArray<GVariant> Locals;
	GVariant *Scope[SCOPE_MAX];
	GArray<StackFrame> Frames;
	RunType StepType;
	GVmDebuggerCallback *DbgCallback;
	GVmDebugger *Debugger;
	GVirtualMachine *Vm;
	ArgumentArray *ArgsOutput;
	GVariant UnusedReturn;
	bool BreakCpp;
	GArray<int> BreakPts;

	GVirtualMachinePriv(GVirtualMachine *vm, GVmDebuggerCallback *Callback)
	{
		Vm = vm;
		BreakCpp = false;
		ArgsOutput = NULL;
		Log = NULL;
		Code = NULL;
		Debugger = NULL;
		DbgCallback = Callback;
		ZeroObj(Scope);
	}
	
	~GVirtualMachinePriv()
	{
		int asd=0;
	}

	void DumpVariant(GStream *Log, GVariant &v)
	{
		if (!Log)
			return;

		switch (v.Type)
		{
			case GV_INT32:
				Log->Print("(int) %i", v.Value.Int);
				break;
			case GV_INT64:
				Log->Print("(int64) %I64i", v.Value.Int64);
				break;
			case GV_STRING:
			{
				char *nl = strchr(v.Value.String, '\n');
				if (nl)
					Log->Print("(string) '%.*s...' (%i bytes)", nl - v.Value.String, v.Value.String, strlen(v.Value.String));
				else
					Log->Print("(string) '%s'", v.Value.String);
				break;
			}
			case GV_DOUBLE:
				Log->Print("(double) %g", v.Value.Dbl);
				break;
			case GV_BOOL:
				Log->Print("(bool) %s", v.Value.Bool ? "true" : "false");
				break;
			case GV_DOM:
				Log->Print("(GDom*) %p", v.Value.Dom);
				break;
			case GV_HASHTABLE:
			{
				Log->Print("(GHashTable*) %p {", v.Value.Hash);

				char *k;
				int n = 0;
				for (void *p = v.Value.Hash->First(&k); p; p = v.Value.Hash->Next(&k), n++)
				{
					Log->Print("%s\"%s\"=", n?",":"", k);
					DumpVariant(Log, *((GVariant*)p));
				}

				Log->Print("}");
				break;
			}
			case GV_LIST:
			{
				Log->Print("(GList*) %p {", v.Value.Lst);

				int n=0; 
				for (GVariant *i=v.Value.Lst->First(); i; i = v.Value.Lst->Next(), n++)
				{
					Log->Print("%s%i=", n?",":"", n);
					DumpVariant(Log, *i);
				}

				Log->Print("}");
				break;
			}
			case GV_NULL:
			{
				Log->Print("null");
				break;
			}
			case GV_BINARY:
			{
				Log->Print("(Binary[%i])", v.Value.Binary.Length);
				if (v.Value.Binary.Data)
				{
					int i;
					for (i=0; i<16 && i < v.Value.Binary.Length; i++)
						Log->Print(" %.2x", ((uint8*)v.Value.Binary.Data)[i]);
					if (i < v.Value.Binary.Length)
						Log->Print("...");
				}
				break;
			}
			default:
				Log->Print("(Type-%i) ????", v.Type);
				break;
		}
	}

	void DumpVariables(GVariant *v, int len)
	{
		if (!Log)
			return;

		for (int i=0; i<len; i++)
		{
			if (v[i].Type != GV_NULL)
			{
				Log->Print("[%i] = ", i);
				DumpVariant(Log, v[i]);
				Log->Print("\n");
			}
		}
	}

	void OnException(const char *File, int Line, uint32 Address, const char *Msg)
	{
		if (Log)
		{
			char *Last = strrchr((char*)File, DIR_CHAR);
			Log->Print("%s Exception: %s (%s:%i)\n", Code->AddrToSourceRef(Address), Msg, Last?Last+1:File, Line);
		}
		
		const char *Fn = Code->GetFileName();
		GAutoString Src(ReadTextFile(Fn));
		if (Vm && Vm->OpenDebugger(Src))
		{
			if (!Debugger->GetCode())
			{
				GAutoPtr<GCompiledCode> Cp(new GCompiledCode(*Code));
				Debugger->OwnCompiledCode(Cp);
				
				GStringPipe AsmBuf;
				Decompile(Code->UserContext, Code, &AsmBuf);
				GAutoString Asm(AsmBuf.NewStr());
				Debugger->SetSource(Asm);
			}
			
			Debugger->OnAddress(Address);
			
			GString m;
			m.Printf("%s (%s:%i)", Msg, LgiGetLeaf(File), Line);
			Debugger->OnError(m);
			
			Debugger->Run();
		}
		else
		{
			LgiAssert(!"Scripting engine exception");
		}
	}

	GExecutionStatus Decompile(GScriptContext *Context, GCompiledCode *Code, GStream *log)
	{
		GExecutionStatus Status = ScriptSuccess;
		LgiAssert(sizeof(GVarRef) == 4);

		GPtr c;
		uint8 *Base = &Code->ByteCode[0];
		c.u8 = Base;
		uint8 *e = c.u8 + Code->ByteCode.Length();

		GStream *OldLog = Log;
		if (log)
			Log = log;
		for (unsigned k=0; k<Code->Globals.Length(); k++)
		{
			Log->Print("G%i = ", k);
			DumpVariant(Log, Code->Globals[k]);
			Log->Print("\n");
		}
		Log->Print("\n");

		GHashTbl<NativeInt, char*> Fn(0, false, -1, NULL);
		for (unsigned m=0; m<Code->Methods.Length(); m++)
		{
			GFunctionInfo *Info = Code->Methods[m];
			if (Info->StartAddr >= 0)
				Fn.Add(Info->StartAddr, Info->Name.Get());
			else
				LgiAssert(!"Method not defined.");
		}

		int OldLineNum = 0;
		while (c.u8 < e)
		{
			char *Meth = Fn.Find(CurrentScriptAddress);
			if (Meth)
			{
				Log->Print("%s:\n", Meth);
			}

			int LineNum = Code->ObjectToSourceAddress(CurrentScriptAddress);
			if (LineNum >= 0 && LineNum != OldLineNum)
			{
				Log->Print("  %i:\n", OldLineNum = LineNum);
			}

			switch (*c.u8++)
			{
				#define VM_DECOMP 1
				#include "Instructions.h"
				#undef VM_DECOMP
			}
		}

		if (log)
			Log = OldLog;

		return Status;
	}

	GExecutionStatus Setup(GCompiledCode *code, uint32 StartOffset, GStream *log, GFunctionInfo *Func, ArgumentArray *Args, GVariant *Ret)
	{
		Status = ScriptSuccess;
		
		Code = code;
		if (!Code)
			return ScriptError;
		
		if (log)
			Log = log;
		else if (Code->SysContext && Code->SysContext->GetLog())
			Log = Code->SysContext->GetLog();
		else if (Code->UserContext && Code->UserContext->GetLog())
			Log = Code->UserContext->GetLog();
		// else LgiTrace("%s:%i - Execution without a log?\n", _FL);
			
		LgiAssert(sizeof(GVarRef) == 4);

		uint8 *Base = c.u8 = &Code->ByteCode[0];
		uint8 *e = c.u8 + Code->ByteCode.Length();

		Scope[SCOPE_REGISTER] = Reg;
		Scope[SCOPE_LOCAL] = NULL;
		Scope[SCOPE_GLOBAL] = &Code->Globals[0];
		Scope[SCOPE_OBJECT] = NULL;
		Scope[SCOPE_RETURN] = Ret ? Ret : &UnusedReturn;

		#if 1
		const char *SourceFileName = Code->GetFileName();
	    char Obj[MAX_PATH];
		if (SourceFileName)
		{
		    strcpy_s(Obj, sizeof(Obj), SourceFileName);
		    char *Ext = LgiGetExtension(Obj);
		    if (Ext)
		        strcpy_s(Ext, sizeof(Obj)-(Ext-Obj), "asm");
		    else
		        strcat_s(Obj, sizeof(Obj), ".asm");
		}
		else
		{
		    GAutoString DataPath;
		    if (Code->UserContext)
				DataPath = Code->UserContext->GetDataFolder();
		    if (!DataPath)
		    {
			    char p[256];
			    if (LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p)))
				    DataPath.Reset(NewStr(p));
		    }
    		
		    LgiMakePath(Obj, sizeof(Obj), DataPath, "Script.asm");
		}
		
		{
			GDirectory SrcD, ObjD;
			bool OutOfDate = true;
			if (SrcD.First(SourceFileName, NULL) != 0 && ObjD.First(Obj, NULL) != 0)
			{
				OutOfDate = ObjD.GetLastWriteTime() < SrcD.GetLastWriteTime();
			}			
			if (OutOfDate || Debugger)
			{
				GFile f;
				GStringPipe p;
				GStream *Out = NULL;
			
				if (Debugger)
				{
					Out = &p;
				}
				else if (f.Open(Obj, O_WRITE))
				{
					f.SetSize(0);
					Out = &f;
				}

				GExecutionStatus Decomp = Decompile(Code->UserContext, Code, Out);
				f.Close();
				if (Decomp != ScriptSuccess)
				{
					LgiAssert(!"Decompilation failed.");
					return ScriptError;
				}

				if (Debugger)
				{
					GAutoString a(p.NewStr());
					Debugger->OnAddress(CurrentScriptAddress);
					Debugger->SetSource(a);
				}
			}
		}
		#endif

		#if TIME_INSTRUCTIONS
		LARGE_INTEGER freq = {0}, start, end;
		QueryPerformanceFrequency(&freq);
		GHashTbl<int, int64> Timings;
		GHashTbl<int, int> TimingFreq;
		#endif

		// Calling a function only, not the whole script
		StackFrame &Sf = Frames.New();
		Sf.ReturnIp = e - c.u8;
		Sf.PrevFrameStart = 0;
		Sf.ReturnValue.Scope = SCOPE_RETURN;
		Sf.ReturnValue.Index = 0; // array is only one item long anyway

		if (Func)
		{
			// Set up stack for function call
			Sf.CurrentFrameSize = Func->FrameSize;

			AddLocalSize(Func->FrameSize);

			if (Args)
			{
				// Put the arguments of the function call into the local array
				for (unsigned i=0; i<Args->Length(); i++)
				{
					Locals[LocalsBase+i] = *(*Args)[i];
				}
			}

			// Set IP to start of function
			c.u8 = Base + Func->StartAddr;
		}
		else
		{
			// Executing body of script
			Sf.CurrentFrameSize = 0;
			if (StartOffset > 0)
				c.u8 = Base + StartOffset;
		}
		
		return Status;
	}

	int NearestLine(size_t Addr)
	{
		int l = Code->Debug.Find(Addr);
		if (l >= 0)
			return l;

		for (int Off = 1; Off < 20; Off++)
		{
			int l = Code->Debug.Find(Addr + Off);
			if (l >= 0)
			{
				return l;
			}

			l = Code->Debug.Find(Addr - Off);
			if (l >= 0)
			{
				return l;
			}
		}
		return -1;
	}
	
	GExecutionStatus Run(RunType Type)
	{
		LgiAssert(Code != NULL);
		
		uint8 *Base = &Code->ByteCode[0];
		uint8 *e = Base + Code->ByteCode.Length();
		
		if (Type == RunContinue && BreakPts.Length() == 0)
		{
			// Unconstrained execution
			while (c.u8 < e)
			{
				#if TIME_INSTRUCTIONS
				uint8 TimedOpCode = *c.u8;
				QueryPerformanceCounter(&start);
				#endif
				
				#ifdef BREAK_POINT
				if (c.u8 - Base == BREAK_POINT)
					_asm int 3
				#endif

				switch (*c.u8++)
				{
					#define VM_EXECUTE 1
					#include "Instructions.h"
					#undef VM_EXECUTE
				}
				
				#if TIME_INSTRUCTIONS
				QueryPerformanceCounter(&end);
				int Ticks = end.QuadPart - start.QuadPart;
				int64 i = Timings.Find(TimedOpCode);
				Timings.Add(TimedOpCode, i + Ticks);
				i = TimingFreq.Find(TimedOpCode);
				TimingFreq.Add(TimedOpCode, i + 1);
				#endif
			}

			if (Log)
			{
				#if TIME_INSTRUCTIONS
				Log->Print("\nTimings:\n");
				Log->Print("%-20s%-10s%-10s%-10s\n", "Instr", "Total", "Freq", "Ave");
				int Op;
				for (int64 t=Timings.First(&Op); t; t=Timings.Next(&Op))
				{
					int Frq = TimingFreq.Find(Op);
					int MilliSec = t * 1000000 / freq.QuadPart;
					Log->Print("%-20s%-10i%-10i%-10i\n", InstToString((GInstruction)Op), MilliSec, Frq, MilliSec / Frq);
				}
				Log->Print("\n");
				#endif

				#if POST_EXECUTE_STATE
				Log->Print("Stack:\n");
				char *v;
				for (void *i=Code->Globals.Lut.First(&v); i; i=Code->Globals.Lut.Next(&v))
				{
					int Idx = (int)i - 1;
					if (Idx >= 0 && Idx < Code->Globals.Length())
					{
						Log->Print("%s = ", v);
						DumpVariant(Log, Code->Globals[Idx]);
						Log->Print("\n");
					}
				}
				Log->Print("\nRegisters:\n");
				DumpVariables(Reg, MAX_REGISTER);
				#endif
			}
		}
		else
		{
			// Stepping through code
			// GHashTbl<int, int> &Debug = Code->Debug;
			int Param;
			switch (Type)
			{
				case RunStepLine:
					Param = NearestLine(CurrentScriptAddress);
					break;
				case RunStepOut:
					Param = Frames.Length();
					break;
				default:
					break;
			}
			
			if (BreakCpp)
			#if defined(WIN32) && !defined(_WIN64)
				_asm int 3
			#else
				assert(!"BreakPoint");
			#endif

			while (c.u8 < e)
			{
				if (Type == RunContinue &&
					BreakPts.HasItem(c.u8 - Base))
					break;
				
				switch (*c.u8++)
				{
					#define VM_EXECUTE 1
					#include "Instructions.h"
					#undef VM_EXECUTE
				}

				if (Type == RunStepLine)
				{
					int CurLine = NearestLine(CurrentScriptAddress);
					if (CurLine && CurLine != Param)
						break;
				}
				else if (Type == RunStepOut)
				{
					if ((int)Frames.Length() < Param)
						break;
				}
				else if (Type == RunStepInstruction)
					break;
			}
		}

		if (Debugger && Status != ScriptError)
			Debugger->OnAddress(CurrentScriptAddress);
		
		return Status;
	}
};

GVirtualMachine::GVirtualMachine(GVmDebuggerCallback *callback)
{
	d = new GVirtualMachinePriv(this, callback);
	d->AddRef();
}

GVirtualMachine::GVirtualMachine(GVirtualMachine *vm)
{
	d = vm->d;
	d->AddRef();
}

GVirtualMachine::~GVirtualMachine()
{
	if (d->Vm == this)
		d->Vm = NULL;
	d->DecRef();
}

GExecutionStatus GVirtualMachine::Execute(GCompiledCode *Code, uint32 StartOffset, GStream *Log, bool StartImmediately, GVariant *Return)
{
	if (!Code)
		return ScriptError;

	GExecutionStatus s = d->Setup(Code, StartOffset, Log, NULL, NULL, Return);
	if (s != ScriptSuccess || !StartImmediately)
		return s;

	return d->Run(GVirtualMachinePriv::RunContinue);
}

GExecutionStatus GVirtualMachine::ExecuteFunction(GCompiledCode *Code, GFunctionInfo *Func, ArgumentArray &Args, GVariant *Ret, GStream *Log, ArgumentArray *ArgsOut)
{
	GCompiledCode *Cc = dynamic_cast<GCompiledCode*>(Code);
	if (!Cc)
		return ScriptError;

	GExecutionStatus s = d->Setup(Cc, 0, Log, Func, &Args, Ret);
	if (s != ScriptSuccess)
		return s;

	d->ArgsOutput = ArgsOut;
	return d->Run(GVirtualMachinePriv::RunContinue);
}

GVmDebugger *GVirtualMachine::OpenDebugger(const char *Script)
{
	if (!d->Debugger)
	{
		if (!d->DbgCallback)
			return NULL;
		
		d->Debugger = d->DbgCallback->AttachVm(this, Script, NULL);
	}
	
	return d->Debugger;
}

bool GVirtualMachine::StepInstruction()
{
	GExecutionStatus s = d->Run(GVirtualMachinePriv::RunStepInstruction);
	return s != ScriptError;
}

bool GVirtualMachine::StepLine()
{
	GExecutionStatus s = d->Run(GVirtualMachinePriv::RunStepLine);
	return s != ScriptError;
}

bool GVirtualMachine::StepOut()
{
	GExecutionStatus s = d->Run(GVirtualMachinePriv::RunStepOut);
	return s != ScriptError;
}

bool GVirtualMachine::BreakExecution()
{
	return false;
}

bool GVirtualMachine::Continue()
{
	GExecutionStatus s = d->Run(GVirtualMachinePriv::RunContinue);
	return s != ScriptError;
}

bool GVirtualMachine::BreakPoint(const char *File, int Line, bool Add)
{
	return false;
}

bool GVirtualMachine::BreakPoint(int Addr, bool Add)
{
	if (Add)
		d->BreakPts.Add(Addr);
	else
		d->BreakPts.Delete(Addr);
	return true;
}

void GVirtualMachine::SetBreakCpp(bool Brk)
{
	d->BreakCpp = Brk;
}

////////////////////////////////////////////////////////////////////
/*
bool GTypeDef::GetVariant(const char *Name, GVariant &Value, char *Arr)
{
	GMember *m = Members.Find(Name);
	if (!m || !Object)
	{
		LgiAssert(!"No member?");
		return false;
	}

	GPtr p;
	p.i8 = Object;
	p.i8 += m->Offset;
	switch (m->Type)
	{
		case GV_INT32:
		{
			Value = *p.i32;
			break;
		}
		case GV_DOUBLE:
		{
			Value = *p.dbl;
			break;
		}
		case GV_STRING:
		{
			Value = p.i8;
			break;
		}
		case GV_CUSTOM:
		{
			Value.Empty();
			Value.Type = GV_CUSTOM;
			Value.Value.Custom.Dom = m->Nest;
			Value.Value.Custom.Data = p.i8;
			break;
		}
		default:
		{
			return false;
		}
	}

	return true;
}

bool GTypeDef::SetVariant(const char *Name, GVariant &Value, char *Arr)
{
	GMember *m = Members.Find(Name);
	if (!m || !Object)
	{
		LgiAssert(!"No member?");
		return false;
	}

	GPtr p;
	p.i8 = Object;
	p.i8 += m->Offset;
	switch (m->Type)
	{
		case GV_INT32:
		{
			*p.i32 = Value.CastInt32();
			break;
		}
		case GV_DOUBLE:
		{
			*p.dbl = Value.CastDouble();
			break;
		}
		case GV_STRING:
		{
			char *s = Value.CastString();
			if (!s)
				return false;

			int i;
			for (i = 0; *s && i < m->Size - 1; i++)
			{
				*p.i8++ = *s++;
			}
			if (i < m->Size - 1)
				*p.i8 = 0;
			break;
		}
		case GV_CUSTOM:
		{
			GTypeDef *t = dynamic_cast<GTypeDef*>(Value.Value.Custom.Dom);
			if (m->Nest == t)
			{
				memcpy(p.i8, Value.Value.Custom.Data, t->Sizeof());
			}
			break;
		}
		default:
		{
			return false;
		}
	}

	return true;
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32 IconsData[] =
{
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x9D9CCEBE, 0x3B166419, 0x74594357, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x543CF81F, 0xCEDE647C, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x7C998D1B, 0xF81FB61C, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCEBFF81F, 0x43DB4C1C, 0xDF1E955B, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8C0CF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8D5CF81F, 0x43595C1A, 0x3AF74338, 0x8CFA4B57, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xC69D6C39, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0x647BADFD, 0x543C53FB, 0x3B1553FB, 0x329132B2, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F64CB, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8D9FF81F, 0x8D9F8D9F, 0x855E857E, 0x7CFD7D1D, 0x74BC74DC, 0xF81F74DC, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8BEBF81F, 0xF81F83AB, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x7CBB8D5C, 0xF81FB63D, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F5BD8, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x959D647C, 0xCEBDF81F, 0x32913AD3, 0xB61B5353, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x3BA564CB, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8D9FF81F, 0x74DC8D9F, 
0x8D9FF81F, 0x74DC8D9F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8D9F8D9F, 0x855E857E, 0x7CFD7D1D, 0x74BC74DC, 0xF81F74DC, 0xF81FF81F, 0xF81FF81F, 0x8D9FF81F, 0xAE1EB65E, 0xD71FA5FE, 0x853D8D7D, 0x6CBD7CFD, 0xF81F2AB5, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8BEBF81F, 0x7329FF98, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCE9E5C1A, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81F5398, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81F6C7C, 0x5BD6F81F, 0xD6DD5BB5, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xB6D45CAA, 0xF81F3BA5, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8D9FF81F, 0x2AB5D71F, 0x8D9FF81F, 0x2AB5D71F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xD71F8D9F, 0xC6DFD71F, 0xB65FBE7F, 0x9DDEAE3F, 0xF81F2AB5, 0xF81FF81F, 0xF81FF81F, 0x857EF81F, 0x9DDEAE1E, 0xFFFFDF3F, 0x74FD853D, 0x647C6CBC, 0xF81F2274, 0xF81FF81F, 0xF81FF81F, 0x9C6CF81F, 0x944D944C, 0x944D8C0C, 0xFF54FF76, 0xF81F62A8, 0xF81FF81F, 0xF81FF81F, 0xBE5D4B99, 0x543CF81F, 0xC6BE5C7C, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F5398, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F53DB, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x75ED5489, 0x3BA5B6D4, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x857EF81F, 0x2274DF3F, 0x857EF81F, 0x2274DF3F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
0xDF3F857E, 0xB65FCEDF, 0x9DBEAE1F, 0x851B959E, 0xF81F2274, 0xF81FF81F, 0xF81FF81F, 0x855EF81F, 0xE75F9DDE, 0xFFFFFFFF, 0x6CBC74DD, 0x543C647C, 0xF81F1A33, 0xF81FF81F, 0xF81FF81F, 0x944BF81F, 0xFFD9F756, 0xFF53FF97, 0xFEEEFF31, 0x5226F66A, 0xF81FF81F, 0xF81FF81F, 0x84DA6C3A, 0xBE7EADDC, 0x43DB4C1C, 0xF81F953B, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81F4B77, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0x9D7C5C3B, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x75ED4C48, 0x5D0A75ED, 0xF81F3BA5, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x855EF81F, 0x1A33D71F, 0x855EF81F, 0x1A33D71F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xD71F855E, 0xA5FFBE7F, 0x855E959E, 
0x6C7A7CFD, 0xF81F1A33, 0xF81FF81F, 0xF81FF81F, 0x7D1DF81F, 0xFFFFDF1E, 0xFFFFFFFF, 0xFFFFFFFF, 0x4BFCC69D, 0xF81F11F2, 0xF81FF81F, 0xF81FF81F, 0x940AF81F, 0xFF75FF97, 0xFEEEFF31, 0xF6ABFECD, 0xBCA7E5E8, 0xF81F49C5, 0xF81FF81F, 0x7CBAB61C, 0x541C53FA, 0x3B1553FB, 0x329132B2, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F4B57, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x6C9CB63D, 0x43584BBA, 0x3AD53B16, 0x743742F4, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x6D8B4407, 0x3C055D0A, 0x1B212B84, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x7D1DF81F, 0x11F2CEBF, 0x7D1DF81F, 0x11F2CEBF, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCEBF7D1D, 0x9DBEAE3F, 0x7CFD8D5E, 0x53B76CBC, 0xF81F11F2, 0xF81FF81F, 
0xF81FF81F, 0x7CFDF81F, 0xCEBD853D, 0xFFFFFFFF, 0x541C5C5C, 0x43BBFFFF, 0xF81F09B1, 0xF81FF81F, 0xF81FF81F, 0x83A9F81F, 0xFF31FF74, 0xF6ACF6CF, 0xDDEAF68B, 0x41A4B467, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xC69DF81F, 0x32913AD3, 0xB5FB4B53, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81F4336, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x5D0A33E5, 0x2B843C05, 0xF81F0B00, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x7CFDF81F, 0x09B1BE9F, 0x7CFDF81F, 0x09B1BE9F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xBE9F7CFD, 0x959EA5FF, 0x6C9C7CFD, 0x4B565C3B, 0xF81F09B1, 0xF81FF81F, 0xF81FF81F, 0x74DCF81F, 0x6CBC74FD, 
0xFFFFBE3B, 0x43DC541C, 0x337BFFFF, 0xF81F0990, 0xF81FF81F, 0xF81FF81F, 0x8389F81F, 0x6AA472E7, 0x72C56264, 0xB487DDA9, 0xF81F41A4, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x5BD6F81F, 0xF81F5BB5, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F4336, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x3C052BA4, 0x0B002B84, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x74DCF81F, 0x0990AE5F, 0x74DCF81F, 0x0990AE5F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xAE5F74DC, 0x7D1D95BE, 0x5C3B6CBC, 0x43354BD9, 0xF81F0990, 0xF81FF81F, 0xF81FF81F, 0x74BCF81F, 0x647C6CBC, 0x9D7A543C, 0x3B9B43DB, 0x2B5BFFFF, 
0xF81F0170, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x5A45F81F, 0x41A4AC26, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81F5377, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x2B842363, 0xF81F0B00, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x74BCF81F, 0x0170A5FE, 0x74BCF81F, 0x0170A5FE, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xA5FE74BC, 0x6C79851A, 0x4B555BF8, 0x32D34314, 0xF81F0170, 0xF81FF81F, 0xF81FF81F, 0x74DCF81F, 0x543C5C7C, 0x43BB4BFC, 0x335B3B9B, 0x231BFFFF, 0xF81F0170, 0xF81FF81F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0x49E4F81F, 0xF81F41A4, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x9D7A53B7, 0x4BBAF81F, 0xB61C6459, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0B001B42, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x74DCF81F, 0x01700170, 0x74DCF81F, 0x01700170, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x3B1674DC, 0x2A9432F6, 0x11F22253, 0x017009B1, 0xF81F0170, 0xF81FF81F, 0xF81FF81F, 0x74DCF81F, 0x3B163B16, 0x2A9432F6, 0x11F22253, 0x017009B1, 0xF81F0170, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x41A4F81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0x747863F7, 0x953BB61C, 0x4BB843B9, 0xB61B7C98, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F1301, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x6C18ADBA, 0x53D953B7, 0x3B144B98, 0x32503291, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xB61BF81F, 0x32503291, 0xA5794B12, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x5B95F81F, 0xB61A5373, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 
};
GInlineBmp DbgIcons = {128, 16, 16, IconsData};

enum DbgCtrls
{
	IDC_STATIC = -1,
	IDC_TABS = 300,
	IDC_BOX,
	IDC_BOX2,
	IDC_TEXT,
	IDC_LOCALS,
	IDC_GLOBALS,
	IDC_REGISTERS,
	IDC_STACK,
	IDC_LOG,
	IDC_RUN,
	IDC_PAUSE,
	IDC_STOP,
	IDC_RESTART,
	IDC_GOTO,
	IDC_STEP_INSTR,
	IDC_STEP_LINE,
	IDC_STEP_OUT,
	IDC_SOURCE_LST,
	IDC_BREAK_POINT,
	IDC_BREAK_CPP,
	IDC_VARS_TBL
};

struct GScriptVmDebuggerPriv;
class GDebugView : public GTextView3
{
	GScriptVmDebuggerPriv *d;
	int CurLine;
	
	int ErrorLine;
	GString Error;
	
	GArray<int> BreakPts;
	
public:
	GDebugView(GScriptVmDebuggerPriv *priv);
	~GDebugView();

	void SetError(const char *Err);
	int GetCurLine() { return CurLine; }
	int GetAddr();
	void ScrollToCurLine();
	void PourText(int Start, int Length);
	void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour);
	void OnPaint(GSurface *pDC);
	bool Breakpoint(int Addr);
};

struct GScriptVmDebuggerPriv
{
	// Current script
	bool OwnVm;
	GAutoPtr<GVirtualMachine> Vm;
	GVmDebuggerCallback *Callback;
	GAutoString Script, Assembly;
	GArray<CodeBlock> Blocks;
	size_t CurrentAddr;
	GArray<bool> LineIsAsm;
	GAutoPtr<GCompiledCode> Obj;
	GVariant Return;

	// Ui
	bool RunLoop;
	GView *Parent;
	GBox *Main;
	GBox *Sub;
	GList *SourceLst;
	GTabView *Tabs;
	GDebugView *Text;
	GList *Locals, *Globals, *Registers, *Stack;
	GTextLog *Log;
	GToolBar *Tools;
	GTableLayout *VarsTbl;

	GScriptVmDebuggerPriv()
	{
		RunLoop = false;
		OwnVm = false;
		CurrentAddr = -1;
		Main = NULL;
		Tabs = NULL;
		Log = NULL;
		Text = NULL;
		Locals = NULL;
		Globals = NULL;
		Registers = NULL;
		Stack = NULL;
		Tools = NULL;
		SourceLst = NULL;
		Callback = NULL;
		VarsTbl = NULL;
	}
};

GDebugView::GDebugView(GScriptVmDebuggerPriv *priv) : GTextView3(IDC_TEXT, 0, 0, 100, 100)
{
	d = priv;
	ErrorLine = -1;
	SetWrapType(TEXTED_WRAP_NONE);
	GetCss(true)->PaddingLeft(GCss::Len(GCss::LenPx, 18));
}

GDebugView::~GDebugView()
{
}

void GDebugView::SetError(const char *Err)
{
	ErrorLine = CurLine;
	Error = Err;
}

#define IsHexChar(c) \
	( \
		IsDigit(c) \
		|| \
		((c) >= 'a' && (c) <= 'f') \
		|| \
		((c) >= 'A' && (c) <= 'F') \
	)

int IsAddr(char16 *Ln)
{
	int Addr = 0;
	for (char16 *s = Ln; *s && *s != '\n' && s < Ln + 8; s++)
	{
		Addr += IsHexChar(*s);
	}
	
	if (Addr != 8)
		return -1;
	
	return HtoiW(Ln);
}

int GDebugView::GetAddr()
{
	int Index;
	GTextLine *t = GetTextLine(Cursor, &Index);
	if (!t)
		return -1;
		
	int Addr = IsAddr(Text + t->Start);
	return Addr;
}

void GDebugView::ScrollToCurLine()
{
	SetLine(CurLine);
}

bool GDebugView::Breakpoint(int Addr)
{
	if (BreakPts.HasItem(Addr))
	{
		BreakPts.Delete(Addr);
		Invalidate();
		return false;
	}
	else
	{
		BreakPts.Add(Addr);
		Invalidate();
		return true;
	}
}

void GDebugView::OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour)
{
	GTextView3::OnPaintLeftMargin(pDC, r, colour);

	pDC->Colour(GColour(192, 0, 0));
	GFont *f = GetFont();
	f->Colour(LC_LOW, LC_WORKSPACE);
	f->Transparent(true);
	
	int Fy = f->GetHeight();
	int Start = VScroll ? (int)VScroll->Value() : 0;
	int Page = (r.Y() + Fy - 1) / Fy;
	int Ln = Start;
	int Rad = (Fy >> 1) - 1;
	int PadY = GetCss(true)->PaddingTop().ToPx(Y(), f) + ((Fy - Rad) >> 1);

	for (GTextLine *i = Line[Start]; i && Ln <= Start + Page; i = Line.Next(), Ln++)
	{
		int OffY = (Ln - Start) * f->GetHeight();
		/*
		GString Num;
		Num.Printf("%i", Ln);
		GDisplayString Ds(f, Num);
		Ds.Draw(pDC, 0, r.y1+OffY);
		*/
		
		char16 *s = Text + i->Start;
		int Addr = IsAddr(s);
		if (BreakPts.HasItem(Addr))
		{
			pDC->FilledCircle(r.x1 + Rad + 2, OffY + PadY + Rad, Rad);
		}		
	}

	f->Transparent(false);
	f->Colour(LC_TEXT, LC_WORKSPACE);
}

void GDebugView::OnPaint(GSurface *pDC)
{
	GTextView3::OnPaint(pDC);
	if (Error)
	{
		GTextLine *Ln = Line[ErrorLine];

		GFont *f = GetFont();
		GRect c = GetClient();
		int Pad = 3;
		GDisplayString Ds(f, Error);
		GRect r(0, 0, Ds.X()-1, Ds.Y()-1);
		r.Size(-Pad, -Pad);
		r.Offset(c.X()-r.X(), Ln ? Ln->r.y1 - ScrollYPixel(): 0);
		f->Transparent(false);
		f->Colour(GColour::White, GColour::Red);
		Ds.Draw(pDC, r.x1 + Pad, r.y1 + Pad, &r);
	}
}

void GDebugView::PourText(int Start, int Len)
{
	GTextView3::PourText(Start, Len);

	CurLine = -1;
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		CodeBlock &b = d->Blocks[i];
		for (unsigned n=0; n<b.AsmAddr.Length(); n++)
		{
			if (d->CurrentAddr >= b.AsmAddr[n])
			{
				CurLine = b.ViewLine + b.SrcLines + n - 1;
			}
		}
	}

	unsigned Idx = 0;
	for (GTextLine *l=Line.First(); l; l=Line.Next(), Idx++)
	{
		// char16 *t = Text + l->Start;
		// char16 *e = t + l->Len;
		if (CurLine == Idx)
		{
			l->c.Rgb(0, 0, 0);
			l->Back.Set(LC_DEBUG_CURRENT_LINE, 24);
		}
		else
		{
			bool IsAsm = Idx < d->LineIsAsm.Length() ? d->LineIsAsm[Idx] : false;
			if (IsAsm)
			{
				l->c.Rgb(0, 0, 255);
				l->Back.Rgb(0xf0, 0xf0, 0xf0);
			}
		}
	}
}

GVmDebuggerWnd::GVmDebuggerWnd(GView *Parent, GVmDebuggerCallback *Callback, GVirtualMachine *Vm, const char *Script, const char *Assembly)
{
	d = new GScriptVmDebuggerPriv;
	d->Parent = Parent;
	if (Vm)
		d->Vm.Reset(new GVirtualMachine(Vm));
	d->Callback = Callback;
	d->Script.Reset(NewStr(Script));
	d->Assembly.Reset(NewStr(Assembly));
	
	GRect r(0, 0, 1000, 900);
	SetPos(r);
	if (Parent)
		MoveSameScreen(Parent);
	else
		MoveToCenter();
	Name("Script Debugger");
	
	if (Attach(NULL))
	{
		if ((Menu = new GMenu))
		{
			Menu->Attach(this);
			GSubMenu *s = Menu->AppendSub("Debug");
			s->AppendItem("Run", IDC_RUN, true, -1, "F5");
			s->AppendItem("Pause", IDC_PAUSE, true, -1, NULL);
			s->AppendItem("Stop", IDC_STOP, true, -1, "Ctrl+Break");
			s->AppendItem("Restart", IDC_RESTART, true, -1, NULL);
			s->AppendItem("Goto", IDC_GOTO, true, -1, NULL);
			s->AppendSeparator();
			s->AppendItem("Step Instruction", IDC_STEP_INSTR, true, -1, "F11");
			s->AppendItem("Step Line", IDC_STEP_LINE, true, -1, "F10");
			s->AppendItem("Step Out", IDC_STEP_OUT, true, -1, "Shift+F11");
			s->AppendSeparator();
			s->AppendItem("Breakpoint", IDC_BREAK_POINT, true, -1, "F9");
			s->AppendItem("Break Into C++", IDC_BREAK_CPP, true, -1, "Ctrl+F9");
		}
		
		AddView(d->Tools = new GToolBar);
		
		uint16 *Px = (uint16*) DbgIcons.Data;
		GImageList *il = new GImageList(16, 16, DbgIcons.Create(*Px));
		if (il)
			d->Tools->SetImageList(il, 16, 16, true);

		d->Tools->AppendButton("Run", IDC_RUN);		
		d->Tools->AppendButton("Pause", IDC_PAUSE);
		d->Tools->AppendButton("Stop", IDC_STOP);
		d->Tools->AppendButton("Restart", IDC_RESTART);
		d->Tools->AppendButton("Goto", IDC_GOTO);
		d->Tools->AppendSeparator();
		d->Tools->AppendButton("Step Instruction", IDC_STEP_INSTR);
		d->Tools->AppendButton("Step Line", IDC_STEP_LINE);
		d->Tools->AppendButton("Step Out", IDC_STEP_OUT);
		
		AddView(d->Main = new GBox(IDC_BOX));		
		d->Main->SetVertical(true);
		
		d->Main->AddView(d->Sub = new GBox(IDC_BOX2));
		d->Sub->SetVertical(false);
		
		d->Sub->AddView(d->SourceLst = new GList(IDC_SOURCE_LST, 0, 0, 100, 100));
		d->SourceLst->GetCss(true)->Width(GCss::Len("200px"));
		d->SourceLst->AddColumn("Source", 200);
		d->Sub->AddView(d->Text = new GDebugView(d));
		
		d->Main->AddView(d->Tabs = new GTabView(IDC_TABS));
		d->Tabs->GetCss(true)->Height(GCss::Len("250px"));
		
		GTabPage *p = d->Tabs->Append("Variables");
		p->Append(d->VarsTbl = new GTableLayout(IDC_VARS_TBL));		
		
		int x = 0, y = 0;
		GLayoutCell *c = d->VarsTbl->GetCell(x++, y);
		c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Globals:"));
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Locals:"));
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Registers:"));

		x = 0; y++;
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(d->Globals = new GList(IDC_GLOBALS, 0, 0, 100, 100));
		d->Globals->AddColumn("Name",100);
		d->Globals->AddColumn("Value",400);
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(d->Locals = new GList(IDC_LOCALS, 0, 0, 100, 100));
		d->Locals->AddColumn("Name",100);
		d->Locals->AddColumn("Value",400);
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(d->Registers = new GList(IDC_REGISTERS, 0, 0, 100, 100));
		d->Registers->AddColumn("Name",100);
		d->Registers->AddColumn("Value",400);

		p = d->Tabs->Append("Stack");
		p->Append(d->Stack = new GList(IDC_STACK, 0, 0, 100, 100));
		d->Stack->SetPourLargest(true);
		d->Stack->AddColumn("Address", 100);
		d->Stack->AddColumn("Function", 300);
		
		p = d->Tabs->Append("Log");
		p->Append(d->Log = new GTextLog(IDC_LOG));
		
		AttachChildren();
		Visible(true);
		
		{
			char p[MAX_PATH];
			LgiGetExePath(p, sizeof(p));
			LgiMakePath(p, sizeof(p), p, "../Scripts");
			GDirectory dir;
			for (int b = dir.First(p); b; b = dir.Next())
			{
				if (!dir.IsDir())
				{
					char *n = dir.GetName();
					if (stristr(n, ".script") &&
						dir.Path(p, sizeof(p)))
					{
						GListItem *it = new GListItem;
						it->SetText(dir.GetName(), 0);
						it->SetText(p, 1);
						d->SourceLst->Insert(it);
					}
				}
			}
		}
	}
}

GVmDebuggerWnd::~GVmDebuggerWnd()
{
	LgiAssert(d->RunLoop == false);
}

bool GVmDebuggerWnd::OnRequestClose(bool OsShuttingDown)
{
	if (!d->RunLoop)
		return GWindow::OnRequestClose(OsShuttingDown);

	d->RunLoop = false;
	return false; // Wait for Run() to exit in it's own time.
}

void GVmDebuggerWnd::Run()
{
	// This is to allow objects on the application's stack to 
	// still be valid while the debugger UI is shown.
	d->RunLoop = true;
	while (d->RunLoop && Visible())
	{
		LgiApp->Run(false);
		LgiSleep(1);
	}
	Quit();
}

GStream *GVmDebuggerWnd::GetLog()
{
	return d->Log;
}

void GVmDebuggerWnd::OwnVm(bool Own)
{
	d->OwnVm = Own;
}

void GVmDebuggerWnd::OwnCompiledCode(GAutoPtr<GCompiledCode> Cc)
{
	d->Obj = Cc;
}

GCompiledCode *GVmDebuggerWnd::GetCode()
{
	return d->Obj;
}

void GVmDebuggerWnd::SetSource(const char *Mixed)
{
	#if 1
	GStringPipe Glob(256);
	GStringPipe Tmp(256);
	
	d->Blocks.Length(0);
	CodeBlock *Cur = &d->Blocks.New();

	// Parse the mixed source
	GToken t(Mixed, "\n", false);
	bool InGlobals = true;
	int InAsm = -1;
	
	for (unsigned i=0; i<t.Length(); i++)
	{
		char *l = t[i];
		if (!l)
			continue;
		if (InGlobals)
		{
			if (*l == 'G')
			{
				Glob.Print("%s\n", l);
				continue;
			}

			InGlobals = false;
		}

		int IsAsm = IsDigit(*l);
		if (InAsm < 0)
			InAsm = IsAsm;
		if (IsAsm ^ InAsm)
		{
			if (InAsm)
			{
				// Asm -> Code
				Cur->Asm.Reset(Tmp.NewStr());
				Cur = &d->Blocks.New();
			}
			else
			{
				// Code -> Asm
				Tmp.Empty();
			}
			InAsm = IsAsm;
		}
		
		Tmp.Print("%s\n", l);
		if (InAsm)
		{
			Cur->AsmLines++;
			Cur->AsmAddr.Add(htoi(l));
		}
		else if (!Cur->SrcLine)
		{
			while (*l == ' ') l++;
			if (IsDigit(*l))
				Cur->SrcLine = atoi(l);
		}
	}
	if (InAsm)
		Cur->Asm.Reset(Tmp.NewStr());

	Tmp.Empty();
	
	GStringPipe Txt;
	GToken Src(d->Script, "\n", false);
	unsigned SrcLine = 1;
	unsigned ViewLine = 1;
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		CodeBlock &b = d->Blocks[i];
		if (b.SrcLine > 0)
		{
			while (SrcLine <= b.SrcLine)
			{
				char *s = Src[SrcLine-1];
				Tmp.Print("%i: %s\n", SrcLine, s ? s : "");
				b.SrcLines++;
				SrcLine++;
			}
			
			b.Source.Reset(Tmp.NewStr());
		}

		if (b.Source && b.Asm)
		{
			b.ViewLine = ViewLine;
			ViewLine += b.SrcLines + b.AsmLines;
			Txt.Print("%s%s", b.Source.Get(), b.Asm.Get());
		}
		else if (b.Source)
		{
			b.ViewLine = ViewLine;
			ViewLine += b.SrcLines;
			Txt.Print("%s", b.Source.Get());
		}
		else if (b.Asm)
		{
			b.ViewLine = ViewLine;
			ViewLine += b.AsmLines;
			Txt.Print("%s", b.Asm.Get());
		}
	}
	while (SrcLine <= Src.Length())
	{
		Txt.Print("%i: %s\n", SrcLine, Src[SrcLine-1]);
		SrcLine++;
	}
	
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		CodeBlock &b = d->Blocks[i];
		int Base = b.ViewLine + b.SrcLines;
		for (int n = Base; n<Base+b.AsmLines; n++)
			d->LineIsAsm[n-1] = true;
	}
	
	GAutoString a(Txt.NewStr());
	d->Text->Name(a);
	#else
	d->Text->Name(Mixed);
	#endif
}

void GVmDebuggerWnd::UpdateVariables(GList *Lst, GVariant *Arr, int Len, char Prefix)
{
	if (!d->Vm || !Lst || !Arr)
		return;

	List<GListItem> all;
	Lst->GetAll(all);
	
	GListItem *it;
	for (int i=0; i<Len; i++)
	{
		GVariant *v = Arr + i;
		GStringPipe p(64);
		d->Vm->d->DumpVariant(&p, *v);
		GAutoString a(p.NewStr());
		char nm[32];
		sprintf_s(nm, sizeof(nm), "%c%i", Prefix, i);
		
		if (i >= all.Length())
		{
			it = new GListItem;
			all.Insert(it);
			Lst->Insert(it);
		}
		it = i < all.Length() ? all[i] : NULL;
		if (it)
		{
			it->SetText(nm, 0);
			it->SetText(a, 1);
		}
	}
	
	Lst->ResizeColumnsToContent();
}

void GVmDebuggerWnd::OnAddress(size_t Addr)
{
	d->CurrentAddr = Addr;
	if (d->Text)
	{
		int Sz = d->Text->GetSize();
		d->Text->PourText(0, Sz);
		d->Text->ScrollToCurLine();
		d->Text->Invalidate();
	}

	OnNotify(d->Tabs, 0);	
}

void GVmDebuggerWnd::OnError(const char *Msg)
{
	if (Msg)
		d->Text->SetError(Msg);
}

void GVmDebuggerWnd::OnRun(bool Running)
{
}

void GVmDebuggerWnd::LoadFile(const char *File)
{
	if (!d->Vm || !d->Callback)
	{
		LgiAssert(0);
		return;
	}

	d->Script.Reset(ReadTextFile(File));
	d->Obj.Reset();
	if (d->Callback->CompileScript(d->Obj, File, d->Script))
	{
		GCompiledCode *Code = dynamic_cast<GCompiledCode*>(d->Obj.Get());
		if (Code)
		{
			d->Return.Empty();
			d->Vm->d->Frames.Length(0);
			d->Vm->d->Setup(Code, 0, d->Log, NULL, NULL, &d->Return);
		}
	}
}

int GVmDebuggerWnd::OnCommand(int Cmd, int Event, OsView Wnd)
{
	if (d->Vm &&
		d->Vm->d->Vm == NULL)
	{
		// This happens when the original VM decides to go away and leave
		// our copy of the VM as the only one left. This means we have to
		// update the pointer in the VM's private data to point to us.
		d->Vm->d->Vm = d->Vm;
	}
	
	switch (Cmd)
	{
		case IDC_RUN:
		{
			if (d->Vm)
				d->Vm->Continue();
			break;
		}
		case IDC_PAUSE:
		{
			if (d->Vm)
				d->Vm->BreakExecution();
			break;
		}
		case IDC_STOP:
		{
			d->Vm.Reset();
			if (d->RunLoop)
				d->RunLoop = false;
			else
				Quit();
			break;
		}
		case IDC_RESTART:
		{
			if (d->Vm && d->Obj)
			{
				GCompiledCode *Code = dynamic_cast<GCompiledCode*>(d->Obj.Get());
				if (Code)
					d->Vm->Execute(Code, 0, d->Log, false);
			}
			break;
		}
		case IDC_GOTO:
		{
			break;
		}
		case IDC_STEP_INSTR:
		{
			if (d->Vm)
				d->Vm->StepInstruction();
			break;
		}
		case IDC_STEP_LINE:
		{
			if (d->Vm)
				d->Vm->StepLine();
			break;
		}
		case IDC_STEP_OUT:
		{
			if (d->Vm)
				d->Vm->StepOut();
			break;
		}
		case IDC_BREAK_POINT:
		{
			int Addr = d->Text->GetAddr();
			if (Addr >= 0)
				d->Vm->BreakPoint(Addr, d->Text->Breakpoint(Addr));
			break;
		}
		case IDC_BREAK_CPP:
		{
			if (!d->Vm)
			{
				LgiAssert(0);
				break;
			}

			GMenuItem *i = Menu->FindItem(IDC_BREAK_CPP);
			if (!i)
			{
				LgiAssert(0);
				break;
			}
			
			bool b = !i->Checked();
			i->Checked(b);
			d->Vm->SetBreakCpp(b);
			break;
		}
	}

	return GWindow::OnCommand(Cmd, Event, Wnd);
}

int GVmDebuggerWnd::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_TABS:
		{
			switch (Ctrl->Value())
			{
				case 0: // Variables
				{
					if (d->Obj)
					{
						UpdateVariables(d->Globals,
										d->Vm->d->Scope[SCOPE_GLOBAL],
										d->Obj->Globals.Length(),
										'G');
					}
					if (d->Vm->d->Frames.Length())
					{
						GVirtualMachinePriv::StackFrame &frm = d->Vm->d->Frames.Last();
						UpdateVariables(d->Locals,
									d->Vm->d->Scope[SCOPE_LOCAL],
									frm.CurrentFrameSize,
									'L');
					}
					else d->Locals->Empty();
					
					UpdateVariables(d->Registers,
									d->Vm->d->Scope[SCOPE_REGISTER],
									MAX_REGISTER,
									'R');
					break;
				}
				case 1: // Call stack
				{
					d->Stack->Empty();
					
					GArray<GVirtualMachinePriv::StackFrame> &Frames = d->Vm->d->Frames;
					for (int i=(int)Frames.Length()-1; i>=0; i--)
					{
						GVirtualMachinePriv::StackFrame &Sf = Frames[i];
						GListItem *li = new GListItem;
						GString s;
						s.Printf("%p/%i", Sf.ReturnIp, Sf.ReturnIp);
						li->SetText(s, 0);
						
						const char *Src = d->Vm->d->Code->AddrToSourceRef(Sf.ReturnIp);
						li->SetText(Src, 1);
						
						d->Stack->Insert(li);
					}
					break;
				}					
				case 2: // Log
				{
					break;
				}
			}
			break;
		}
		case IDC_SOURCE_LST:
		{
			if (Flags == GNotifyItem_Select)
			{
				GListItem *it = d->SourceLst->GetSelected();
				if (!it)
					break;

				char *full = it->GetText(1);
				if (!FileExists(full))
					break;

				LoadFile(full);
			}
			break;
		}
	}
	
	return GWindow::OnNotify(Ctrl, Flags);
}

GMessage::Param GVmDebuggerWnd::OnEvent(GMessage *Msg)
{
	return GWindow::OnEvent(Msg);
}
