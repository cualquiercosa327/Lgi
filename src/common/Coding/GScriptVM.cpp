#include <math.h>
#include "Lgi.h"
#include "GScripting.h"
#include "GScriptingPriv.h"
#include "GBox.h"
#include "GTabView.h"
#include "GTextLog.h"
#include "LList.h"
#include "GToolBar.h"
#include "GToken.h"
#include "GTableLayout.h"
#include "GTextLabel.h"
#include "GScrollBar.h"
#include "GMatrix.h"

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
	size_t LocalsBase = Locals.Length(); \
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

inline char *CastString(GVariant *v, GVariant &cache)
{
	if (v->Type == GV_STRING)
		return v->Str();

	cache = *v;
	return cache.CastString();
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
			if (d < -MATRIX_DOUBLE_EPSILON)
				return -1;
			return d > MATRIX_DOUBLE_EPSILON;
		}
		case GV_STRING:
		{
			GVariant as, bs;
			char *A = CastString(a, as);
			char *B = CastString(b, bs);
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
		case GV_NULL:
		{
			// One or more values is NULL
			if (a->IsNull() && b->IsNull())
				return 0; // The same..
			GVariant *Val = a->IsNull() ? b : a;
			if (Val->IsNull())
			{
				LgiAssert(0);
				return 0;
			}
			switch (Val->Type)
			{
				case GV_INT32:
				case GV_INT64:
					return Val->CastInt64() != 0;
				case GV_STRING:
					return Val->Str() != NULL;
				default:
					return Val->CastVoidPtr() != NULL;
			}			
			break;
		}
		default:
			return a->CastInt32() - b->CastInt32();
			break;
	}
}

GExecutionStatus GExternFunc::Call(GScriptContext *Ctx, LScriptArguments &Args)
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
	
	ssize_t a = Ptr.ni - &Val[0];
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
				:/*"%eax",*/ "%ecx", "%ebx"	/* clobbered register */
				);
		#endif
	#else
		// Not implemented, gcc???
		LgiAssert(0);
	#endif

	*Args.GetReturn() = (int) r;
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
	LScriptArguments *ArgsOutput;
	bool BreakCpp;
	GArray<ssize_t> BreakPts;
	GString TempPath;

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

				const char *k;
				int n = 0;
				for (GVariant *p = v.Value.Hash->First(&k); p; p = v.Value.Hash->Next(&k), n++)
				{
					Log->Print("%s\"%s\"=", n?",":"", k);
					DumpVariant(Log, *p);
				}

				Log->Print("}");
				break;
			}
			case GV_LIST:
			{
				Log->Print("(LList*) %p {", v.Value.Lst);

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
		
		if (Vm && Vm->OpenDebugger(Code))
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
			LgiTrace("%s:%i - Exception @ %i: %s\n", File, Line, Address, Msg);
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
				/*
	This file is included in both the GVirtualMachinePriv::Execute and GVirtualMachinePriv::Decompile
	That way the "parsing" of instructions is the same.
	
	During decompile the define VM_DECOMP is active.
	
	During execution the define VM_EXECUTE is active.

*/

#ifdef VM_EXECUTE
#define Resolve()		&Scope[c.r->Scope][c.r->Index]; c.r++
#define GResolveRef		GVariant *
#else
#define Resolve()		c.r++
#define GResolveRef		GVarRef *
#endif

default:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("\t%p Unknown instruction %i (0x%x)\n",
					CurrentScriptAddress - 1,
					c.u8[-1], c.u8[-1]);
	#endif
	OnException(_FL, CurrentScriptAddress, "Unknown instruction");
	SetScriptError;
	break;
}
case INop:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Nop\n", CurrentScriptAddress - 1);
	#endif
	break;
}
case ICast:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Cast %s",
					CurrentScriptAddress - 1,
					c.r[0].GetStr());
	#endif
	GResolveRef Var = Resolve();
	uint8 Type = *c.u8++;
	#if VM_DECOMP
	if (Log)
		Log->Print(" to %s\n", GVariant::TypeToString((GVariantType)Type));
	#endif
	#if VM_EXECUTE
	switch (Type)
	{
		case GV_INT32:
		{
			*Var = Var->CastInt32();
			break;
		}
		case GV_STRING:
		{
			*Var = Var->CastString();
			break;
		}
		case GV_DOM:
		{
			*Var = Var->CastDom();
			break;
		}
		case GV_DOUBLE:
		{
			*Var = Var->CastDouble();
			break;
		}
		case GV_INT64:
		{
			*Var = Var->CastInt32();
			break;
		}
		case GV_BOOL:
		{
			*Var = Var->CastBool();
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s ICast warning: unknown type %i/%s\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Var->Type,
							GVariant::TypeToString(Var->Type));
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IBreak:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Break\n", CurrentScriptAddress - 1);
	#endif
	break;
}
case IAssign:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Assign %s <- %s\n",
				CurrentScriptAddress - 1,
				c.r[0].GetStr(),
				c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	CheckParam(Dst != Src);
	*Dst = *Src;
	#endif
	break;
}
case IJump:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Jump by %i (to 0x%x)\n",
			CurrentScriptAddress - 1,
			c.i32[0],
			CurrentScriptAddress + 4 + c.i32[0]);
	#endif

	int32 Jmp = *c.i32++;
	#ifdef VM_EXECUTE
	CheckParam(Jmp != 0);
	c.u8 += Jmp;
	#endif
	break;
}
case IJumpZero:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p JumpZ(%s) by 0x%x\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.i32[1]);
	#endif

	GResolveRef Exp = Resolve();
	int32 Jmp = *c.i32++;
	#ifdef VM_EXECUTE
	CheckParam(Jmp != 0);
	if (!Exp->CastInt32())
	{
		c.u8 += Jmp;
	}
	#endif
	break;
}
// case IUnaryPlus:
case IUnaryMinus:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p UnaryMinus %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef Var = Resolve();
	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_DOUBLE:
			*Var = -Var->CastDouble();
			break;
		case GV_INT64:
			*Var = -Var->CastInt64();
			break;
		default:
			*Var = -Var->CastInt32();
			break;
	}
	#endif
	break;
}
case IPlus:
case IPlusEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Plus %s += %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	if (Dst->Str())
	{
		int dlen = strlen(Dst->Str());
		char *ss;
		GVariant SrcTmp;
		
		switch (Src->Type)
		{
			case GV_NULL:
				ss = (char*)"(null)";
				break;
			case GV_STRING:
				ss = Src->Str();
				break;
			default:
				SrcTmp = *Src;
				ss = SrcTmp.CastString();
				break;
		}

		if (ss)
		{
			int slen = strlen(ss);
			char *s = new char[slen + dlen + 1];
			if (s)
			{
				memcpy(s, Dst->Value.String, dlen);
				memcpy(s + dlen, ss, slen);
				s[dlen + slen] = 0;
				DeleteArray(Dst->Value.String);
				Dst->Value.String = s;
			}
		}
	}
	else switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() + Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() + Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() + Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMinus:
case IMinusEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Minus %s -= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() - Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() - Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() - Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMul:
case IMulEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Mul %s *= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() * Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() * Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() * Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IDiv:
case IDivEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Div %s /= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() / Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() / Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() / Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMod:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Mod %s %= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = fmod(Dst->CastDouble(), Src->CastDouble());
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() % Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() % Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IPostInc:
case IPreInc:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p PostInc %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef v = Resolve();
	#ifdef VM_EXECUTE
	switch (v->Type)
	{
		case GV_DOUBLE:
			*v = v->Value.Dbl + 1;
			break;
		case GV_INT64:
			*v = v->Value.Int64 + 1;
			break;
		default:
			*v = v->CastInt32() + 1;
			break;
	}
	#endif
	break;
}
case IPostDec:
case IPreDec:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p PostDec %sn",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef v = Resolve();
	#ifdef VM_EXECUTE
	switch (v->Type)
	{
		case GV_DOUBLE:
			*v = v->Value.Dbl - 1;
			break;
		case GV_INT64:
			*v = v->Value.Int64 - 1;
			break;
		default:
			*v = v->CastInt32() - 1;
			break;
	}
	#endif
	break;
}
case IEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s == %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();

	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) == 0;
	#endif
	break;
}
case INotEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p %s != %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();

	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) != 0;
	#endif
	break;
}
case ILessThan:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) < 0;
	#endif
	break;
}
case ILessThanEqual:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) <= 0;
	#endif
	break;
}
case IGreaterThan:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) > 0;
	#endif
	break;
}
case IGreaterThanEqual:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) >= 0;
	#endif
	break;
}
case ICallMethod:
{
	GFunc *Meth = *c.fn++;
	if (!Meth)
	{
		Log->Print(	"%s ICallMethod error: No method struct.\n",
					Code->AddrToSourceRef(CurrentScriptAddress - sizeof(Meth)));
		SetScriptError;
		break;
	}

	#ifdef VM_DECOMP
	if (Log)
	{
		Log->Print("%p Call: %s = %s(",
				CurrentScriptAddress - sizeof(Meth) - 1,
				c.r[0].GetStr(),
				Meth->Method.Get());
	}
	#endif
	
	GResolveRef Ret = Resolve();
	uint16 Args = *c.u16++;

	#ifdef VM_EXECUTE			
	LScriptArguments Arg(Vm, Ret);
	#endif
	
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i?", ":"", c.r[0].GetStr());
		#endif

		#if VM_EXECUTE
		Arg[i] = Resolve();
		CheckParam(Arg[i] != NULL);
		#else
		c.r++;
		#endif
	}

	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif
	
	#if VM_EXECUTE
	GHostFunc *Hf = dynamic_cast<GHostFunc*>(Meth);
	if (Hf)
	{
		if (!(Hf->Context->*(Hf->Func))(Arg))
		{
			if (Log)
				Log->Print(	"%s ICallMethod error: Method '%s' failed.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Meth->Method.Get());
			SetScriptError;
		}		
	}
	else
	{
		// Fixme
		if (!Meth->Call(NULL, Arg))
		{
			if (Log)
				Log->Print(	"%s ICallMethod error: Method '%s' failed.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Meth->Method.Get());
			SetScriptError;
		}
	}
	#endif
	break;
}
case ICallScript:
{
	int32 FuncAddr = *c.i32++;
	if (FuncAddr < 0 || (uint32)FuncAddr >= Code->ByteCode.Length())
	{
		Log->Print(	"%s ICallScript error: Script function call invalid addr '%p'.\n",
					Code->AddrToSourceRef(CurrentScriptAddress - sizeof(FuncAddr)),
					FuncAddr);
		SetScriptError;
		break;
	}

	uint16 Frame = *c.u16++;

	#if VM_DECOMP
	if (Log)
		Log->Print("%p CallScript: %s = %p(frame=%i)(",
			CurrentScriptAddress - 5,
			c.r[0].GetStr(),
			FuncAddr,
			Frame);
	#endif

	#ifdef VM_EXECUTE

	// Set up stack for function call
	int CurFrameSize = Frames.Last().CurrentFrameSize;
	StackFrame &Sf = Frames.New();
	Sf.CurrentFrameSize = Frame;
	Sf.PrevFrameStart = Locals.Length() ? Scope[1] - &Locals[0] : 0;
	Sf.ReturnValue = *c.r++;
	Sf.ReturnValue.Index -= CurFrameSize;
	uint16 Args = *c.u16++;

	// Increase the local stack size
	int LocalsBase = Locals.Length();
	Locals.SetFixedLength(false);
	Locals.Length(LocalsBase + Frame);
	Locals.SetFixedLength();
	
	// Put the arguments of the function call into the local array
	GArray<GVariant*> Arg;
	
	#else
	
	GResolveRef Ret = Resolve();
	int Args = *c.u16++;
	
	#endif
	
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i?",":"", c.r[0].GetStr());
		#endif

		#if VM_EXECUTE
		Locals[LocalsBase+i] = *Resolve();
		#else
		c.r++;
		#endif
	}

	#if VM_EXECUTE
	Scope[SCOPE_LOCAL] = &Locals[LocalsBase];

	// Set IP to start of function
	Sf.ReturnIp = CurrentScriptAddress;
	c.u8 = Base + FuncAddr;
	#endif

	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif
	break;
}
case IRet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Ret %s\n", CurrentScriptAddress - 1, c.r[0].GetStr());
	#endif

	GResolveRef ReturnValue = Resolve();

	#ifdef VM_EXECUTE
	if (Frames.Length() > 0)
	{
		StackFrame Sf = Frames[Frames.Length()-1];
		GVarRef &Ret = Sf.ReturnValue;
		GVariant *RetVar = &Scope[Ret.Scope][Ret.Index];
		// LgiTrace("IRet to %i:%i\n", Ret.Scope, Ret.Index);
		if (Ret.Scope == SCOPE_LOCAL)
			LgiAssert(Locals.PtrCheck(RetVar));
		*RetVar = *ReturnValue;
		CheckParam(RetVar->Type == ReturnValue->Type);

		Frames.Length(Frames.Length()-1);
		
		Locals.SetFixedLength(false);
		if (Locals.Length() >= Sf.CurrentFrameSize)
		{
			int Base = Locals.Length() - Sf.CurrentFrameSize;
			if (ArgsOutput)
			{
				if (Frames.Length() == 0)
				{
					for (unsigned i=0; i<ArgsOutput->Length(); i++)
					{
						*(*ArgsOutput)[i] = Locals[Base+i];
					}
				}
			}
			// LgiTrace("%s:%i Locals %i -> %i\n", _FL, Locals.Length(), Base);
			Locals.Length(Base);
			Scope[SCOPE_LOCAL] = &Locals[Sf.PrevFrameStart];
		}
		else
		{
			// LgiTrace("%s:%i - Locals %i -> %i\n", _FL, Locals.Length(), 0);
			Locals.Length(0);
			Scope[SCOPE_LOCAL] = NULL;
		}
		Locals.SetFixedLength();

		c.u8 = Base + Sf.ReturnIp;
	}
	else
	{
		ExitScriptExecution;
	}
	#endif
	break;
}
case IArrayGet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p ArrayGet %s = %s[%s]\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Var = Resolve();
	GResolveRef Idx = Resolve();

	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_LIST:
		{
			CheckParam(Var->Value.Lst);
			GVariant *t = Var->Value.Lst->ItemAt(Idx->CastInt32());
			if (t)
			{
				if (Var == Dst)
				{
					if (Var->Value.Lst->Delete(t))
					{
						*Var = *t;
						DeleteObj(t);
					}
					else CheckParam(!"List delete failed.");
				}
				else *Dst = *t;
			}
			else Dst->Empty();
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Var->Value.Hash);
			GVariant *t = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			if (t) *Dst = *t;
			else Dst->Empty();
			break;
		}
		case GV_CUSTOM:
		{
			GCustomType *T = Var->Value.Custom.Dom;
			size_t Sz = T->Sizeof();
			int Index = Idx->CastInt32();

			Dst->Type = GV_CUSTOM;
			Dst->Value.Custom.Dom = T;
			Dst->Value.Custom.Data = Var->Value.Custom.Data + (Sz * Index);
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s IArrayGet warning: Can't array deref variant type %i\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Var->Type);
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IArraySet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p ArraySet %s[%s] = %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GResolveRef Var = Resolve();
	GResolveRef Idx = Resolve();
	GResolveRef Val = Resolve();
	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_LIST:
		{
			CheckParam(Var->Value.Lst);
			(*Var->Value.Lst).Insert(new GVariant(*Val), Idx->CastInt32());
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Var->Value.Hash);
			GVariant *Old = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			DeleteObj(Old);
			Var->Value.Hash->Add(Idx->CastString(), new GVariant(*Val));
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s IArraySet warning: Can't dereference type '%s'\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							GVariant::TypeToString(Var->Type));
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IAnd:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s && %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	*Dst = (Dst->CastInt32() != 0) && (Src->CastInt32() != 0);
	#endif
	break;
}
case IOr:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s || %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	*Dst = (Dst->CastInt32() != 0) || (Src->CastInt32() != 0);
	#endif
	break;
}
case INot:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = !%s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[0].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	#ifdef VM_EXECUTE
	*Dst = !Dst->CastBool();
	#endif
	break;
}
case IDomGet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = %s->DomGet(%s, %s)\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr(),
					c.r[3].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();
	GResolveRef Arr = Resolve();

	#ifdef VM_EXECUTE

		// Return "NULL" in Dst on error
		if (Dst != Dom)
			Dst->Empty();

		switch (Dom->Type)
		{
			case GV_DOM:
			case GV_STREAM:
			case GV_GSURFACE:
			{
				GDom *dom = Dom->CastDom();
				CheckParam(dom != NULL);
				char *sName = Name->Str();
				CheckParam(sName);
				bool Ret = dom->GetVariant(sName, *Dst, CastArrayIndex(Arr));
				if (!Ret)
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomGet warning: Unexpected %s member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									GVariant::TypeToString(Dom->Type),
									sName);
					Status = ScriptWarning;
				}
				break;
			}
			case GV_DATETIME:
			{
				CheckParam(Dom->Value.Date != NULL);
				char *sName = Name->Str();
				CheckParam(sName);
				bool Ret = Dom->Value.Date->GetVariant(sName, *Dst, CastArrayIndex(Arr));
				if (!Ret)
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomGet warning: Unexpected %s member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									GVariant::TypeToString(Dom->Type),
									sName);
					Status = ScriptWarning;
				}
				break;
			}
			case GV_CUSTOM:
			{
				GCustomType *Type = Dom->Value.Custom.Dom;
				if (Type)
				{
					int Fld;
					if (Name->Type == GV_INT32)
						Fld = Name->Value.Int;
					else
						Fld = Type->IndexOf(Name->Str());
					
					int Index = Arr ? Arr->CastInt32() : 0;
					Type->Get(Fld, *Dst, Dom->Value.Custom.Data, Index);
				}
				break;
			}
			case GV_LIST:
			{
				CheckParam(Dom->Value.Lst);
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Lst->Length();
				break;
			}
			case GV_HASHTABLE:
			{
				CheckParam(Dom->Value.Hash);
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Hash->Length();
				break;
			}
			case GV_BINARY:
			{
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = Dom->Value.Binary.Length;
				break;
			}
			case GV_STRING:
			{
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				switch (p)
				{
					case ObjLength:
					{
						(*Dst) = (int)strlen(Dom->Str());
						break;
					}
					case StrInt:
					{
						(*Dst) = Dom->CastInt32();
						break;
					}
					case StrDouble:
					{
						(*Dst) = Dom->CastDouble();
						break;
					}
					default:
					{
						Dst->Empty();
						if (Log)
							Log->Print("%s IDomGet warning: Unexpected string member '%s'.\n",
										Code->AddrToSourceRef(CurrentScriptAddress),
										sName);
						Status = ScriptWarning;
						break;
					}
				}
				break;
			}
			case GV_NULL:
			{
				#if 1
				if (Log)
					Log->Print("%s IDomGet warning: Can't deref NULL object.\n",
								Code->AddrToSourceRef(CurrentScriptAddress));
				#else // If you want exceptions on NULL deref
				OnException(_FL, CurrentScriptAddress-1, "NULL Dom Ptr");
				#endif
				Status = ScriptWarning;
				break;
			}
			default:
			{
				if (Log)
					Log->Print("%s IDomGet warning: Unexpected type %s (Src=%s:%i IP=0x%x).\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								_FL,
								CurrentScriptAddress);
				Status = ScriptWarning;
				break;
			}
		}

	#endif
	break;
}
case IDomSet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s->DomSet(%s, %s) = %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr(),
					c.r[3].GetStr());
	#endif

	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();
	GResolveRef Arr = Resolve();
	GResolveRef Value = Resolve();
	
	#ifdef VM_EXECUTE

	char *sName = Name->Str();
	if (!sName)
	{
		if (Log)
			Log->Print("%s IDomSet error: No name string.\n",
						Code->AddrToSourceRef(CurrentScriptAddress));
		SetScriptError;
		break;
	}

	switch (Dom->Type)
	{
		case GV_DOM:
		// case GV_GFILE:
		case GV_STREAM:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			CheckParam(dom != NULL);
			bool Ret = dom->SetVariant(sName, *Value, CastArrayIndex(Arr));
			if (!Ret)
			{
				if (Log)
					Log->Print("%s IDomSet warning: Unexpected %s member '%s'.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_DATETIME:
		{
			CheckParam(Dom->Value.Date != NULL);
			bool Ret = Dom->Value.Date->SetVariant(sName, *Value, CastArrayIndex(Arr));
			if (!Ret)
			{
				if (Log)
					Log->Print("%s IDomSet warning: Unexpected %s member '%s'.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_CUSTOM:
		{
			GCustomType *Type = Dom->Value.Custom.Dom;
			if (Type)
			{
				int Fld;
				if (IsDigit(*sName))
					Fld = atoi(sName);
				else
					Fld = Type->IndexOf(sName);
				
				int Index = Arr ? Arr->CastInt32() : 0;
				if (!Type->Set(Fld, *Value, Dom->Value.Custom.Data, Index) &&
					Log)
				{
					Log->Print("%s IDomSet warning: Couldn't set '%s' on custom type.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				}
			}
			break;
		}
		case GV_STRING:
		{
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					char *s;
					int DLen = Value->CastInt32();
					if (DLen && (s = new char[DLen+1]))
					{
						int SLen = Dom->Str() ? strlen(Dom->Str()) : 0;
						if (SLen)
							memcpy(s, Dom->Str(), SLen);
						memset(s+SLen, ' ', DLen-SLen);
						s[DLen] = 0;
						DeleteArray(Dom->Value.String);
						Dom->Value.String = s;
					}
					else Dom->Empty();

					break;
				}
				case StrInt:
				{
					*Dom = Value->CastInt32();
					Dom->Str();
					break;
				}
				case StrDouble:
				{
					*Dom = Value->CastDouble();
					Dom->Str();
					break;
				}
				default:
				{
					if (Log)
						Log->Print("%s IDomSet warning: Unexpected string member %s.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		default:
		{
			if (Log)
				Log->Print("%s IDomSet warning: Unexpected type %s.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							GVariant::TypeToString(Dom->Type));
			Status = ScriptWarning;
			break;
		}
	}

	#endif
	break;
}
case IDomCall:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = %s->DomCall(%s, ",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GVarRef DstRef = *c.r;
	GResolveRef Dst = Resolve();
	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();

	#ifdef VM_EXECUTE

	GResolveRef Args = Resolve();
	int ArgCount = Args->CastInt32();
	char *sName = Name->Str();
	CheckParam(sName)

	if (Dom->Type == GV_CUSTOM)
	{
		#define DEBUG_CUSTOM_METHOD_CALL	1
		
		GCustomType *t = Dom->Value.Custom.Dom;
		CheckParam(t);
		GCustomType::Method *m = t->GetMethod(sName);
		CheckParam(m);
		CheckParam(m->Params.Length() == ArgCount);
		
		// Set up new stack frame...
		StackFrame &Sf = Frames.New();
		Sf.CurrentFrameSize = m->FrameSize;
		Sf.PrevFrameStart = Locals.Length() ? Scope[1] - &Locals[0] : 0;
		Sf.ReturnValue = DstRef;

		// Increase the local stack size
		AddLocalSize(m->FrameSize + 1);
		
		#if DEBUG_CUSTOM_METHOD_CALL
		LgiTrace("CustomType.Call(%s) Args=%i, Frame=%i, Addr=%i, LocalsBase=%i ",
				sName, ArgCount, m->FrameSize, m->Address, LocalsBase);
		#endif
		
		int i = LocalsBase;
		Locals[i++] = *Dom; // this pointer...
		#if DEBUG_CUSTOM_METHOD_CALL
		GString s = Locals[i-1].ToString();
		LgiTrace("This=%s, ", s.Get());
		#endif
		int end = i + ArgCount;
		while (i < end)
		{
			Locals[i++] = *Resolve();
			#if DEBUG_CUSTOM_METHOD_CALL
			s = Locals[i-1].ToString();
			LgiTrace("[%i]=%s, ", i-1, s.Get());
			#endif
		}

		// Now adjust the local stack to point to the locals for the function
		Scope[1] = Locals.Length() ? &Locals[LocalsBase] : NULL;

		// Set IP to start of function
		Sf.ReturnIp = CurrentScriptAddress;
		c.u8 = Base + m->Address;

		#if DEBUG_CUSTOM_METHOD_CALL
		LgiTrace("\n");
		#endif
		break;
	}	

	GArray<GVariant*> Arg;
	Arg.Length(ArgCount);
	for (int i=0; i<ArgCount; i++)
	{
		Arg[i] = Resolve();
	}
	
	switch (Dom->Type)
	{
		case GV_DOM:
		case GV_STREAM:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			CheckParam(dom);
			bool Ret = dom->CallMethod(sName, Dst, Arg);
			if (!Ret)
			{
				Dst->Empty();
				if (Log)
					Log->Print("%s IDomCall warning: %s(...) failed.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_DATETIME:
		{
			CheckParam(Dom->Value.Date);
			bool Ret = Dom->Value.Date->CallMethod(sName, Dst, Arg);
			if (!Ret)
			{
				Dst->Empty();
				if (Log)
					Log->Print("%s IDomCall warning: %s(...) failed.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_LIST:
		{
			CheckParam(Dom->Value.Lst);
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					*Dst = Dom->Value.Lst->Length();
					break;
				}
				case ObjType:
				{
					*Dst = "List";
					break;
				}
				case ContainerAdd:
				{
					if (Arg.Length() > 0 &&
						Arg[0])
					{
						int Index = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;

						GVariant *v = new GVariant;
						*v = *Arg[0];
						Dom->Value.Lst->Insert(v, Index);
					}
					break;
				}
				case ContainerDelete:
				{
					for (unsigned i=0; i<Arg.Length(); i++)
					{
						GVariant *Idx = Arg[i];
						if (Idx)
						{
							int32 n = Arg[i]->CastInt32();
							GVariant *Elem = Dom->Value.Lst->ItemAt(n);
							if (Elem)
							{
								Dom->Value.Lst->Delete(Elem);
								DeleteObj(Elem);
							}
						}
					}
					break;
				}
				case ContainerHasKey:
				{
					if (Arg.Length() > 0 && Arg[0])
					{
						int Index = Arg[0]->CastInt32();
						*Dst = (bool) (Index >= 0 && Index < Dom->Value.Lst->Length());
					}
					else
					{
						*Dst = false;
					}
					break;
				}
				case ContainerSort:
				{
					GVariant *Param = Arg.Length() > 0 ? Arg[0] : NULL;
					Dom->Value.Lst->Sort(GVariantCmp, (NativeInt)Param);
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print(	"%s IDomCall warning: Unexpected list member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Dom->Value.Hash);
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					*Dst = Dom->Value.Hash->Length();
					break;
				}
				case ObjType:
				{
					*Dst = "HashTable";
					break;
				}
				case ContainerAdd:
				{
					if (Arg.Length() == 2 &&
						Arg[0] &&
						Arg[1])
					{
						char *Key = Arg[1]->Str();
						if (Key)
						{
							GVariant *v = new GVariant;
							*v = *Arg[0];
							Dom->Value.Hash->Add(Key, v);
						}
					}
					break;
				}
				case ContainerDelete:
				{
					if (Arg.Length() == 1 &&
						Arg[0])
					{
						char *Key = Arg[0]->Str();
						if (Key)
						{
							GVariant *v = (GVariant*) Dom->Value.Hash->Find(Key);
							if (v)
							{
								Dom->Value.Hash->Delete(Key);
								delete v;
							}
						}
					}
					break;
				}
				case ContainerHasKey:
				{
					if (Arg.Length() > 0 && Arg[0])
					{
						char *Key = Arg[0]->Str();
						*Dst = (bool) (Dom->Value.Hash->Find(Key) != NULL);
					}
					else
					{
						*Dst = false;
					}
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomCall warning: Unexpected hashtable member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		case GV_BINARY:
		{
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				default:
					break;
				case ObjLength:
					*Dst = Dom->Value.Binary.Length;
					break;
			}
			break;
		}
		case GV_STRING:
		{
			if (Arg.Length() > 0 && !Arg[0])
			{
				Dst->Empty();
				break;
			}

			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					char *s = Dom->Str();
					*Dst = (int) (s ? strlen(s) : 0);
					break;
				}
				case ObjType:
				{
					*Dst = "String";
					break;
				}
				case StrJoin:
				{
					switch (Arg[0]->Type)
					{
						case GV_LIST:
						{
							GStringPipe p(256);
							List<GVariant> *Lst = Arg[0]->Value.Lst;
							const char *Sep = Dom->CastString();
							GVariant *v = Lst->First();
							if (v)
							{
								GVariant Tmp = *v;
								p.Print("%s", Tmp.CastString());
								while ((v = Lst->Next()))
								{
									Tmp = *v;
									p.Print("%s%s", Sep, Tmp.CastString());
								}
							}
							Dst->OwnStr(p.NewStr());
							break;
						}
						default:
						{
							*Dst = *Arg[0];
							Dst->CastString();
							break;
						}
					}
					break;
				}								
				case StrSplit:
				{
					const char *Sep = Arg[0]->Str();
					if (!Sep)
					{
						Dst->Empty();
						break;
					}
					
					GVariant Tmp;
					if (Dst == Dom)
					{
						Tmp = *Dom;
						Dom = &Tmp;
					}

					Dst->SetList();
					
					int SepLen = strlen(Sep);
					int MaxSplit = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;
					const char *c = Dom->CastString();
					while (c && *c)
					{
						if (MaxSplit > 0 && Dst->Value.Lst->Length() >= MaxSplit)
							break;

						const char *next = strstr(c, Sep);
						if (!next)
							break;
						
						GVariant *v = new GVariant;
						v->OwnStr(NewStr(c, next - c));
						Dst->Value.Lst->Insert(v);
						
						c = next + SepLen;
					}

					if (c && *c)
					{
						GVariant *v = new GVariant;
						v->OwnStr(NewStr(c));
						Dst->Value.Lst->Insert(v);
					}
					break;
				}								
				case StrFind:
				{
					const char *s = Dom->Str();
					if (!s)
					{
						*Dst = -1;
						break;
					}

					int sLen = strlen(s);
					const char *sub = Arg[0]->Str();
					int start = Arg.Length() > 1 ? Arg[1]->CastInt32() : 0;
					int end = Arg.Length() > 2 ? Arg[2]->CastInt32() : -1;								

					if (start >= sLen)
					{
						*Dst = -1;
						break;
					}
					char *sStart = (char*)s + start;
					char *pos;
					if (end >= 0)
						pos = strnstr(sStart, sub, end);
					else
						pos = strstr(sStart, sub);

					if (pos)
						*Dst = (int64) (pos - s);
					else
						*Dst = -1;
					break;
				}
				case StrRfind:
				{
					const char *s = Dom->Str();
					if (!s)
					{
						*Dst = -1;
						break;
					}

					int sLen = strlen(s);
					const char *sub = Arg[0]->Str();
					int start_idx = Arg.Length() > 1 ? Arg[1]->CastInt32() : 0;
					int end_idx = Arg.Length() > 2 ? Arg[2]->CastInt32() : -1;								

					if (start_idx >= sLen)
					{
						*Dst = -1;
						break;
					}
					int sublen = strlen(sub);
					char *cur = (char*)s + start_idx;
					char *end = end_idx >= 0 ? cur + end_idx : NULL;
					char *pos = NULL;
					while (true)
					{
						cur =	(end)
								?
								strnstr(cur, sub, end - cur)
								:
								strstr(cur, sub);
						if (cur)
						{
							pos = cur;
							cur += sublen;
						}
						else break;
					}
					
					if (pos)
						*Dst = (int64) (pos - s);
					else
						*Dst = -1;
					break;
				}
				case StrLower:
				{
					if (Dst != Dom)
						*Dst = Dom->CastString();
					
					StrLwr(Dst->Str());
					break;
				}
				case StrUpper:
				{
					if (Dst != Dom)
						*Dst = Dom->CastString();

					StrUpr(Dst->Str());
					break;
				}
				case StrStrip:
				{
					char *s = Dom->Str();
					if (s)
					{
						char *start = s;
						char *end = s + strlen(s);
						while (start < end && strchr(WhiteSpace, *start))
							start++;

						while (end > start && strchr(WhiteSpace, end[-1]))
							end--;
						
						Dst->OwnStr(NewStr(start, end - start));
					}
					else Dst->Empty();
					break;
				}
				case StrSub:
				{
					char *s = Dom->Str();
					if (s)
					{
						int Start = Arg.Length() > 0 ? Arg[0]->CastInt32() : 0;
						int End = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;
						int Len = strlen(s);
						if (End < 0 || End > Len)
							End = Len;
						if (Start < 0)
							Start = 0;
						if (Start <= End)
							Dst->OwnStr(NewStr(s + Start, End - Start));
						else
							Dst->Empty();
					}
					else Dst->Empty();
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print("%p IDomCall warning: Unexpected string member %s (%s:%i).\n",
									CurrentScriptAddress,
									sName,
									_FL);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		default:
		{
			const char *Type = GVariant::TypeToString(Dom->Type);
			char t[32];
			if (!Type)
			{
				sprintf_s(t, sizeof(t), "UnknownType(%i)", Dom->Type);
				Type = t;
			}
			
			GDomProperty p = LgiStringToDomProp(sName);
			if (p == ObjType)
			{
				*Dst = Type;
			}
			else
			{
				Dst->Empty();
				if (Log)
				{
					Log->Print("%s IDomCall warning: Unexpected type %s (Src=%s:%i IP=0x%x).\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								Type,
								_FL,
								CurrentScriptAddress);
				}
				Status = ScriptWarning;
			}
			break;
		}
	}

	#else

	GVariant *Count = NULL;
	switch (c.r->Scope)
	{
		case SCOPE_GLOBAL:
			Count = &Code->Globals[c.r->Index];
			c.r++;
			break;
		default:
			OnException(_FL, CurrentScriptAddress, "Unsupported scope.");
			return ScriptError;
	}
	
	int Args = Count->CastInt32();
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i ? ", " : "", c.r->GetStr());
		#endif
		c.r++;
	}
	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif

	#endif
	break;
}
case IDebug:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Debugger\n", CurrentScriptAddress-1);
	#elif VM_EXECUTE
	OnException(_FL, CurrentScriptAddress-1, "ShowDebugger");
	return ScriptWarning;
	#endif
	break;
}

#undef Resolve
#undef GResolveRef
				#undef VM_DECOMP
			}
		}

		if (log)
			Log = OldLog;

		return Status;
	}

	GExecutionStatus Setup(GCompiledCode *code, uint32 StartOffset, GStream *log, GFunctionInfo *Func, LScriptArguments *Args)
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
		Scope[SCOPE_RETURN] = Args->GetReturn();

		#if 1
		const char *SourceFileName = Code->GetFileName();
		char Obj[MAX_PATH];
		if (SourceFileName)
		{
			if (strchr(SourceFileName, DIR_CHAR))
				strcpy_s(Obj, sizeof(Obj), SourceFileName);
			else if (TempPath != NULL)
				LgiMakePath(Obj, sizeof(Obj), TempPath, SourceFileName);
			else
			{
				LgiGetSystemPath(LSP_TEMP, Obj, sizeof(Obj));
				LgiMakePath(Obj, sizeof(Obj), Obj, SourceFileName);
			}

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

				if (Out)
				{
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
				{
					int asd=0;
				}
				#endif

				switch (*c.u8++)
				{
					#define VM_EXECUTE 1
					/*
	This file is included in both the GVirtualMachinePriv::Execute and GVirtualMachinePriv::Decompile
	That way the "parsing" of instructions is the same.
	
	During decompile the define VM_DECOMP is active.
	
	During execution the define VM_EXECUTE is active.

*/

#ifdef VM_EXECUTE
#define Resolve()		&Scope[c.r->Scope][c.r->Index]; c.r++
#define GResolveRef		GVariant *
#else
#define Resolve()		c.r++
#define GResolveRef		GVarRef *
#endif

default:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("\t%p Unknown instruction %i (0x%x)\n",
					CurrentScriptAddress - 1,
					c.u8[-1], c.u8[-1]);
	#endif
	OnException(_FL, CurrentScriptAddress, "Unknown instruction");
	SetScriptError;
	break;
}
case INop:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Nop\n", CurrentScriptAddress - 1);
	#endif
	break;
}
case ICast:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Cast %s",
					CurrentScriptAddress - 1,
					c.r[0].GetStr());
	#endif
	GResolveRef Var = Resolve();
	uint8 Type = *c.u8++;
	#if VM_DECOMP
	if (Log)
		Log->Print(" to %s\n", GVariant::TypeToString((GVariantType)Type));
	#endif
	#if VM_EXECUTE
	switch (Type)
	{
		case GV_INT32:
		{
			*Var = Var->CastInt32();
			break;
		}
		case GV_STRING:
		{
			*Var = Var->CastString();
			break;
		}
		case GV_DOM:
		{
			*Var = Var->CastDom();
			break;
		}
		case GV_DOUBLE:
		{
			*Var = Var->CastDouble();
			break;
		}
		case GV_INT64:
		{
			*Var = Var->CastInt32();
			break;
		}
		case GV_BOOL:
		{
			*Var = Var->CastBool();
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s ICast warning: unknown type %i/%s\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Var->Type,
							GVariant::TypeToString(Var->Type));
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IBreak:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Break\n", CurrentScriptAddress - 1);
	#endif
	break;
}
case IAssign:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Assign %s <- %s\n",
				CurrentScriptAddress - 1,
				c.r[0].GetStr(),
				c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	CheckParam(Dst != Src);
	*Dst = *Src;
	#endif
	break;
}
case IJump:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Jump by %i (to 0x%x)\n",
			CurrentScriptAddress - 1,
			c.i32[0],
			CurrentScriptAddress + 4 + c.i32[0]);
	#endif

	int32 Jmp = *c.i32++;
	#ifdef VM_EXECUTE
	CheckParam(Jmp != 0);
	c.u8 += Jmp;
	#endif
	break;
}
case IJumpZero:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p JumpZ(%s) by 0x%x\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.i32[1]);
	#endif

	GResolveRef Exp = Resolve();
	int32 Jmp = *c.i32++;
	#ifdef VM_EXECUTE
	CheckParam(Jmp != 0);
	if (!Exp->CastInt32())
	{
		c.u8 += Jmp;
	}
	#endif
	break;
}
// case IUnaryPlus:
case IUnaryMinus:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p UnaryMinus %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef Var = Resolve();
	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_DOUBLE:
			*Var = -Var->CastDouble();
			break;
		case GV_INT64:
			*Var = -Var->CastInt64();
			break;
		default:
			*Var = -Var->CastInt32();
			break;
	}
	#endif
	break;
}
case IPlus:
case IPlusEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Plus %s += %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	if (Dst->Str())
	{
		int dlen = strlen(Dst->Str());
		char *ss;
		GVariant SrcTmp;
		
		switch (Src->Type)
		{
			case GV_NULL:
				ss = (char*)"(null)";
				break;
			case GV_STRING:
				ss = Src->Str();
				break;
			default:
				SrcTmp = *Src;
				ss = SrcTmp.CastString();
				break;
		}

		if (ss)
		{
			int slen = strlen(ss);
			char *s = new char[slen + dlen + 1];
			if (s)
			{
				memcpy(s, Dst->Value.String, dlen);
				memcpy(s + dlen, ss, slen);
				s[dlen + slen] = 0;
				DeleteArray(Dst->Value.String);
				Dst->Value.String = s;
			}
		}
	}
	else switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() + Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() + Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() + Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMinus:
case IMinusEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Minus %s -= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() - Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() - Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() - Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMul:
case IMulEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Mul %s *= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() * Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() * Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() * Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IDiv:
case IDivEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Div %s /= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() / Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() / Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() / Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMod:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Mod %s %= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = fmod(Dst->CastDouble(), Src->CastDouble());
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() % Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() % Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IPostInc:
case IPreInc:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p PostInc %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef v = Resolve();
	#ifdef VM_EXECUTE
	switch (v->Type)
	{
		case GV_DOUBLE:
			*v = v->Value.Dbl + 1;
			break;
		case GV_INT64:
			*v = v->Value.Int64 + 1;
			break;
		default:
			*v = v->CastInt32() + 1;
			break;
	}
	#endif
	break;
}
case IPostDec:
case IPreDec:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p PostDec %sn",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef v = Resolve();
	#ifdef VM_EXECUTE
	switch (v->Type)
	{
		case GV_DOUBLE:
			*v = v->Value.Dbl - 1;
			break;
		case GV_INT64:
			*v = v->Value.Int64 - 1;
			break;
		default:
			*v = v->CastInt32() - 1;
			break;
	}
	#endif
	break;
}
case IEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s == %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();

	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) == 0;
	#endif
	break;
}
case INotEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p %s != %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();

	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) != 0;
	#endif
	break;
}
case ILessThan:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) < 0;
	#endif
	break;
}
case ILessThanEqual:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) <= 0;
	#endif
	break;
}
case IGreaterThan:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) > 0;
	#endif
	break;
}
case IGreaterThanEqual:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) >= 0;
	#endif
	break;
}
case ICallMethod:
{
	GFunc *Meth = *c.fn++;
	if (!Meth)
	{
		Log->Print(	"%s ICallMethod error: No method struct.\n",
					Code->AddrToSourceRef(CurrentScriptAddress - sizeof(Meth)));
		SetScriptError;
		break;
	}

	#ifdef VM_DECOMP
	if (Log)
	{
		Log->Print("%p Call: %s = %s(",
				CurrentScriptAddress - sizeof(Meth) - 1,
				c.r[0].GetStr(),
				Meth->Method.Get());
	}
	#endif
	
	GResolveRef Ret = Resolve();
	uint16 Args = *c.u16++;

	#ifdef VM_EXECUTE			
	LScriptArguments Arg(Vm, Ret);
	#endif
	
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i?", ":"", c.r[0].GetStr());
		#endif

		#if VM_EXECUTE
		Arg[i] = Resolve();
		CheckParam(Arg[i] != NULL);
		#else
		c.r++;
		#endif
	}

	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif
	
	#if VM_EXECUTE
	GHostFunc *Hf = dynamic_cast<GHostFunc*>(Meth);
	if (Hf)
	{
		if (!(Hf->Context->*(Hf->Func))(Arg))
		{
			if (Log)
				Log->Print(	"%s ICallMethod error: Method '%s' failed.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Meth->Method.Get());
			SetScriptError;
		}		
	}
	else
	{
		// Fixme
		if (!Meth->Call(NULL, Arg))
		{
			if (Log)
				Log->Print(	"%s ICallMethod error: Method '%s' failed.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Meth->Method.Get());
			SetScriptError;
		}
	}
	#endif
	break;
}
case ICallScript:
{
	int32 FuncAddr = *c.i32++;
	if (FuncAddr < 0 || (uint32)FuncAddr >= Code->ByteCode.Length())
	{
		Log->Print(	"%s ICallScript error: Script function call invalid addr '%p'.\n",
					Code->AddrToSourceRef(CurrentScriptAddress - sizeof(FuncAddr)),
					FuncAddr);
		SetScriptError;
		break;
	}

	uint16 Frame = *c.u16++;

	#if VM_DECOMP
	if (Log)
		Log->Print("%p CallScript: %s = %p(frame=%i)(",
			CurrentScriptAddress - 5,
			c.r[0].GetStr(),
			FuncAddr,
			Frame);
	#endif

	#ifdef VM_EXECUTE

	// Set up stack for function call
	int CurFrameSize = Frames.Last().CurrentFrameSize;
	StackFrame &Sf = Frames.New();
	Sf.CurrentFrameSize = Frame;
	Sf.PrevFrameStart = Locals.Length() ? Scope[1] - &Locals[0] : 0;
	Sf.ReturnValue = *c.r++;
	Sf.ReturnValue.Index -= CurFrameSize;
	uint16 Args = *c.u16++;

	// Increase the local stack size
	int LocalsBase = Locals.Length();
	Locals.SetFixedLength(false);
	Locals.Length(LocalsBase + Frame);
	Locals.SetFixedLength();
	
	// Put the arguments of the function call into the local array
	GArray<GVariant*> Arg;
	
	#else
	
	GResolveRef Ret = Resolve();
	int Args = *c.u16++;
	
	#endif
	
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i?",":"", c.r[0].GetStr());
		#endif

		#if VM_EXECUTE
		Locals[LocalsBase+i] = *Resolve();
		#else
		c.r++;
		#endif
	}

	#if VM_EXECUTE
	Scope[SCOPE_LOCAL] = &Locals[LocalsBase];

	// Set IP to start of function
	Sf.ReturnIp = CurrentScriptAddress;
	c.u8 = Base + FuncAddr;
	#endif

	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif
	break;
}
case IRet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Ret %s\n", CurrentScriptAddress - 1, c.r[0].GetStr());
	#endif

	GResolveRef ReturnValue = Resolve();

	#ifdef VM_EXECUTE
	if (Frames.Length() > 0)
	{
		StackFrame Sf = Frames[Frames.Length()-1];
		GVarRef &Ret = Sf.ReturnValue;
		GVariant *RetVar = &Scope[Ret.Scope][Ret.Index];
		// LgiTrace("IRet to %i:%i\n", Ret.Scope, Ret.Index);
		if (Ret.Scope == SCOPE_LOCAL)
			LgiAssert(Locals.PtrCheck(RetVar));
		*RetVar = *ReturnValue;
		CheckParam(RetVar->Type == ReturnValue->Type);

		Frames.Length(Frames.Length()-1);
		
		Locals.SetFixedLength(false);
		if (Locals.Length() >= Sf.CurrentFrameSize)
		{
			int Base = Locals.Length() - Sf.CurrentFrameSize;
			if (ArgsOutput)
			{
				if (Frames.Length() == 0)
				{
					for (unsigned i=0; i<ArgsOutput->Length(); i++)
					{
						*(*ArgsOutput)[i] = Locals[Base+i];
					}
				}
			}
			// LgiTrace("%s:%i Locals %i -> %i\n", _FL, Locals.Length(), Base);
			Locals.Length(Base);
			Scope[SCOPE_LOCAL] = &Locals[Sf.PrevFrameStart];
		}
		else
		{
			// LgiTrace("%s:%i - Locals %i -> %i\n", _FL, Locals.Length(), 0);
			Locals.Length(0);
			Scope[SCOPE_LOCAL] = NULL;
		}
		Locals.SetFixedLength();

		c.u8 = Base + Sf.ReturnIp;
	}
	else
	{
		ExitScriptExecution;
	}
	#endif
	break;
}
case IArrayGet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p ArrayGet %s = %s[%s]\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Var = Resolve();
	GResolveRef Idx = Resolve();

	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_LIST:
		{
			CheckParam(Var->Value.Lst);
			GVariant *t = Var->Value.Lst->ItemAt(Idx->CastInt32());
			if (t)
			{
				if (Var == Dst)
				{
					if (Var->Value.Lst->Delete(t))
					{
						*Var = *t;
						DeleteObj(t);
					}
					else CheckParam(!"List delete failed.");
				}
				else *Dst = *t;
			}
			else Dst->Empty();
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Var->Value.Hash);
			GVariant *t = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			if (t) *Dst = *t;
			else Dst->Empty();
			break;
		}
		case GV_CUSTOM:
		{
			GCustomType *T = Var->Value.Custom.Dom;
			size_t Sz = T->Sizeof();
			int Index = Idx->CastInt32();

			Dst->Type = GV_CUSTOM;
			Dst->Value.Custom.Dom = T;
			Dst->Value.Custom.Data = Var->Value.Custom.Data + (Sz * Index);
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s IArrayGet warning: Can't array deref variant type %i\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Var->Type);
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IArraySet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p ArraySet %s[%s] = %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GResolveRef Var = Resolve();
	GResolveRef Idx = Resolve();
	GResolveRef Val = Resolve();
	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_LIST:
		{
			CheckParam(Var->Value.Lst);
			(*Var->Value.Lst).Insert(new GVariant(*Val), Idx->CastInt32());
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Var->Value.Hash);
			GVariant *Old = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			DeleteObj(Old);
			Var->Value.Hash->Add(Idx->CastString(), new GVariant(*Val));
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s IArraySet warning: Can't dereference type '%s'\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							GVariant::TypeToString(Var->Type));
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IAnd:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s && %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	*Dst = (Dst->CastInt32() != 0) && (Src->CastInt32() != 0);
	#endif
	break;
}
case IOr:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s || %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	*Dst = (Dst->CastInt32() != 0) || (Src->CastInt32() != 0);
	#endif
	break;
}
case INot:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = !%s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[0].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	#ifdef VM_EXECUTE
	*Dst = !Dst->CastBool();
	#endif
	break;
}
case IDomGet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = %s->DomGet(%s, %s)\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr(),
					c.r[3].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();
	GResolveRef Arr = Resolve();

	#ifdef VM_EXECUTE

		// Return "NULL" in Dst on error
		if (Dst != Dom)
			Dst->Empty();

		switch (Dom->Type)
		{
			case GV_DOM:
			case GV_STREAM:
			case GV_GSURFACE:
			{
				GDom *dom = Dom->CastDom();
				CheckParam(dom != NULL);
				char *sName = Name->Str();
				CheckParam(sName);
				bool Ret = dom->GetVariant(sName, *Dst, CastArrayIndex(Arr));
				if (!Ret)
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomGet warning: Unexpected %s member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									GVariant::TypeToString(Dom->Type),
									sName);
					Status = ScriptWarning;
				}
				break;
			}
			case GV_DATETIME:
			{
				CheckParam(Dom->Value.Date != NULL);
				char *sName = Name->Str();
				CheckParam(sName);
				bool Ret = Dom->Value.Date->GetVariant(sName, *Dst, CastArrayIndex(Arr));
				if (!Ret)
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomGet warning: Unexpected %s member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									GVariant::TypeToString(Dom->Type),
									sName);
					Status = ScriptWarning;
				}
				break;
			}
			case GV_CUSTOM:
			{
				GCustomType *Type = Dom->Value.Custom.Dom;
				if (Type)
				{
					int Fld;
					if (Name->Type == GV_INT32)
						Fld = Name->Value.Int;
					else
						Fld = Type->IndexOf(Name->Str());
					
					int Index = Arr ? Arr->CastInt32() : 0;
					Type->Get(Fld, *Dst, Dom->Value.Custom.Data, Index);
				}
				break;
			}
			case GV_LIST:
			{
				CheckParam(Dom->Value.Lst);
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Lst->Length();
				break;
			}
			case GV_HASHTABLE:
			{
				CheckParam(Dom->Value.Hash);
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Hash->Length();
				break;
			}
			case GV_BINARY:
			{
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = Dom->Value.Binary.Length;
				break;
			}
			case GV_STRING:
			{
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				switch (p)
				{
					case ObjLength:
					{
						(*Dst) = (int)strlen(Dom->Str());
						break;
					}
					case TypeInt:
					{
						(*Dst) = Dom->CastInt32();
						break;
					}
					case TypeDouble:
					{
						(*Dst) = Dom->CastDouble();
						break;
					}
					default:
					{
						Dst->Empty();
						if (Log)
							Log->Print("%s IDomGet warning: Unexpected string member '%s'.\n",
										Code->AddrToSourceRef(CurrentScriptAddress),
										sName);
						Status = ScriptWarning;
						break;
					}
				}
				break;
			}
			case GV_NULL:
			{
				#if 1
				if (Log)
					Log->Print("%s IDomGet warning: Can't deref NULL object.\n",
								Code->AddrToSourceRef(CurrentScriptAddress));
				#else // If you want exceptions on NULL deref
				OnException(_FL, CurrentScriptAddress-1, "NULL Dom Ptr");
				#endif
				Status = ScriptWarning;
				break;
			}
			default:
			{
				if (Log)
					Log->Print("%s IDomGet warning: Unexpected type %s (Src=%s:%i IP=0x%x).\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								_FL,
								CurrentScriptAddress);
				Status = ScriptWarning;
				break;
			}
		}

	#endif
	break;
}
case IDomSet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s->DomSet(%s, %s) = %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr(),
					c.r[3].GetStr());
	#endif

	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();
	GResolveRef Arr = Resolve();
	GResolveRef Value = Resolve();
	
	#ifdef VM_EXECUTE

	char *sName = Name->Str();
	if (!sName)
	{
		if (Log)
			Log->Print("%s IDomSet error: No name string.\n",
						Code->AddrToSourceRef(CurrentScriptAddress));
		SetScriptError;
		break;
	}

	switch (Dom->Type)
	{
		case GV_DOM:
		// case GV_GFILE:
		case GV_STREAM:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			CheckParam(dom != NULL);
			bool Ret = dom->SetVariant(sName, *Value, CastArrayIndex(Arr));
			if (!Ret)
			{
				if (Log)
					Log->Print("%s IDomSet warning: Unexpected %s member '%s'.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_DATETIME:
		{
			CheckParam(Dom->Value.Date != NULL);
			bool Ret = Dom->Value.Date->SetVariant(sName, *Value, CastArrayIndex(Arr));
			if (!Ret)
			{
				if (Log)
					Log->Print("%s IDomSet warning: Unexpected %s member '%s'.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_CUSTOM:
		{
			GCustomType *Type = Dom->Value.Custom.Dom;
			if (Type)
			{
				int Fld;
				if (IsDigit(*sName))
					Fld = atoi(sName);
				else
					Fld = Type->IndexOf(sName);
				
				int Index = Arr ? Arr->CastInt32() : 0;
				if (!Type->Set(Fld, *Value, Dom->Value.Custom.Data, Index) &&
					Log)
				{
					Log->Print("%s IDomSet warning: Couldn't set '%s' on custom type.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				}
			}
			break;
		}
		case GV_STRING:
		{
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					char *s;
					int DLen = Value->CastInt32();
					if (DLen && (s = new char[DLen+1]))
					{
						int SLen = Dom->Str() ? strlen(Dom->Str()) : 0;
						if (SLen)
							memcpy(s, Dom->Str(), SLen);
						memset(s+SLen, ' ', DLen-SLen);
						s[DLen] = 0;
						DeleteArray(Dom->Value.String);
						Dom->Value.String = s;
					}
					else Dom->Empty();

					break;
				}
				case TypeInt:
				{
					*Dom = Value->CastInt32();
					Dom->Str();
					break;
				}
				case TypeDouble:
				{
					*Dom = Value->CastDouble();
					Dom->Str();
					break;
				}
				default:
				{
					if (Log)
						Log->Print("%s IDomSet warning: Unexpected string member %s.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		default:
		{
			if (Log)
				Log->Print("%s IDomSet warning: Unexpected type %s.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							GVariant::TypeToString(Dom->Type));
			Status = ScriptWarning;
			break;
		}
	}

	#endif
	break;
}
case IDomCall:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = %s->DomCall(%s, ",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GVarRef DstRef = *c.r;
	GResolveRef Dst = Resolve();
	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();

	#ifdef VM_EXECUTE

	GResolveRef Args = Resolve();
	int ArgCount = Args->CastInt32();
	char *sName = Name->Str();
	CheckParam(sName)

	if (Dom->Type == GV_CUSTOM)
	{
		#define DEBUG_CUSTOM_METHOD_CALL	1
		
		GCustomType *t = Dom->Value.Custom.Dom;
		CheckParam(t);
		GCustomType::Method *m = t->GetMethod(sName);
		CheckParam(m);
		CheckParam(m->Params.Length() == ArgCount);
		
		// Set up new stack frame...
		StackFrame &Sf = Frames.New();
		Sf.CurrentFrameSize = m->FrameSize;
		Sf.PrevFrameStart = Locals.Length() ? Scope[1] - &Locals[0] : 0;
		Sf.ReturnValue = DstRef;

		// Increase the local stack size
		AddLocalSize(m->FrameSize + 1);
		
		#if DEBUG_CUSTOM_METHOD_CALL
		LgiTrace("CustomType.Call(%s) Args=%i, Frame=%i, Addr=%i, LocalsBase=%i ",
				sName, ArgCount, m->FrameSize, m->Address, LocalsBase);
		#endif
		
		int i = LocalsBase;
		Locals[i++] = *Dom; // this pointer...
		#if DEBUG_CUSTOM_METHOD_CALL
		GString s = Locals[i-1].ToString();
		LgiTrace("This=%s, ", s.Get());
		#endif
		int end = i + ArgCount;
		while (i < end)
		{
			Locals[i++] = *Resolve();
			#if DEBUG_CUSTOM_METHOD_CALL
			s = Locals[i-1].ToString();
			LgiTrace("[%i]=%s, ", i-1, s.Get());
			#endif
		}

		// Now adjust the local stack to point to the locals for the function
		Scope[1] = Locals.Length() ? &Locals[LocalsBase] : NULL;

		// Set IP to start of function
		Sf.ReturnIp = CurrentScriptAddress;
		c.u8 = Base + m->Address;

		#if DEBUG_CUSTOM_METHOD_CALL
		LgiTrace("\n");
		#endif
		break;
	}	

	GArray<GVariant*> Arg;
	Arg.Length(ArgCount);
	for (int i=0; i<ArgCount; i++)
	{
		Arg[i] = Resolve();
	}
	
	switch (Dom->Type)
	{
		case GV_DOM:
		case GV_STREAM:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			CheckParam(dom);
			bool Ret = dom->CallMethod(sName, Dst, Arg);
			if (!Ret)
			{
				Dst->Empty();
				if (Log)
					Log->Print("%s IDomCall warning: %s(...) failed.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_DATETIME:
		{
			CheckParam(Dom->Value.Date);
			bool Ret = Dom->Value.Date->CallMethod(sName, Dst, Arg);
			if (!Ret)
			{
				Dst->Empty();
				if (Log)
					Log->Print("%s IDomCall warning: %s(...) failed.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_LIST:
		{
			CheckParam(Dom->Value.Lst);
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					*Dst = Dom->Value.Lst->Length();
					break;
				}
				case ObjType:
				{
					*Dst = "List";
					break;
				}
				case ContainerAdd:
				{
					if (Arg.Length() > 0 &&
						Arg[0])
					{
						int Index = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;

						GVariant *v = new GVariant;
						*v = *Arg[0];
						Dom->Value.Lst->Insert(v, Index);
					}
					break;
				}
				case ContainerDelete:
				{
					for (unsigned i=0; i<Arg.Length(); i++)
					{
						GVariant *Idx = Arg[i];
						if (Idx)
						{
							int32 n = Arg[i]->CastInt32();
							GVariant *Elem = Dom->Value.Lst->ItemAt(n);
							if (Elem)
							{
								Dom->Value.Lst->Delete(Elem);
								DeleteObj(Elem);
							}
						}
					}
					break;
				}
				case ContainerHasKey:
				{
					if (Arg.Length() > 0 && Arg[0])
					{
						int Index = Arg[0]->CastInt32();
						*Dst = (bool) (Index >= 0 && Index < Dom->Value.Lst->Length());
					}
					else
					{
						*Dst = false;
					}
					break;
				}
				case ContainerSort:
				{
					GVariant *Param = Arg.Length() > 0 ? Arg[0] : NULL;
					Dom->Value.Lst->Sort(GVariantCmp, (NativeInt)Param);
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print(	"%s IDomCall warning: Unexpected list member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Dom->Value.Hash);
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					*Dst = Dom->Value.Hash->Length();
					break;
				}
				case ObjType:
				{
					*Dst = "HashTable";
					break;
				}
				case ContainerAdd:
				{
					if (Arg.Length() == 2 &&
						Arg[0] &&
						Arg[1])
					{
						char *Key = Arg[1]->Str();
						if (Key)
						{
							GVariant *v = new GVariant;
							*v = *Arg[0];
							Dom->Value.Hash->Add(Key, v);
						}
					}
					break;
				}
				case ContainerDelete:
				{
					if (Arg.Length() == 1 &&
						Arg[0])
					{
						char *Key = Arg[0]->Str();
						if (Key)
						{
							GVariant *v = (GVariant*) Dom->Value.Hash->Find(Key);
							if (v)
							{
								Dom->Value.Hash->Delete(Key);
								delete v;
							}
						}
					}
					break;
				}
				case ContainerHasKey:
				{
					if (Arg.Length() > 0 && Arg[0])
					{
						char *Key = Arg[0]->Str();
						*Dst = (bool) (Dom->Value.Hash->Find(Key) != NULL);
					}
					else
					{
						*Dst = false;
					}
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomCall warning: Unexpected hashtable member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		case GV_BINARY:
		{
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				default:
					break;
				case ObjLength:
					*Dst = Dom->Value.Binary.Length;
					break;
			}
			break;
		}
		case GV_STRING:
		{
			if (Arg.Length() > 0 && !Arg[0])
			{
				Dst->Empty();
				break;
			}

			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					char *s = Dom->Str();
					*Dst = (int) (s ? strlen(s) : 0);
					break;
				}
				case ObjType:
				{
					*Dst = "String";
					break;
				}
				case StrJoin:
				{
					switch (Arg[0]->Type)
					{
						case GV_LIST:
						{
							GStringPipe p(256);
							List<GVariant> *Lst = Arg[0]->Value.Lst;
							const char *Sep = Dom->CastString();
							GVariant *v = Lst->First();
							if (v)
							{
								GVariant Tmp = *v;
								p.Print("%s", Tmp.CastString());
								while ((v = Lst->Next()))
								{
									Tmp = *v;
									p.Print("%s%s", Sep, Tmp.CastString());
								}
							}
							Dst->OwnStr(p.NewStr());
							break;
						}
						default:
						{
							*Dst = *Arg[0];
							Dst->CastString();
							break;
						}
					}
					break;
				}								
				case StrSplit:
				{
					const char *Sep = Arg[0]->Str();
					if (!Sep)
					{
						Dst->Empty();
						break;
					}
					
					GVariant Tmp;
					if (Dst == Dom)
					{
						Tmp = *Dom;
						Dom = &Tmp;
					}

					Dst->SetList();
					
					int SepLen = strlen(Sep);
					int MaxSplit = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;
					const char *c = Dom->CastString();
					while (c && *c)
					{
						if (MaxSplit > 0 && Dst->Value.Lst->Length() >= MaxSplit)
							break;

						const char *next = strstr(c, Sep);
						if (!next)
							break;
						
						GVariant *v = new GVariant;
						v->OwnStr(NewStr(c, next - c));
						Dst->Value.Lst->Insert(v);
						
						c = next + SepLen;
					}

					if (c && *c)
					{
						GVariant *v = new GVariant;
						v->OwnStr(NewStr(c));
						Dst->Value.Lst->Insert(v);
					}
					break;
				}								
				case StrFind:
				{
					const char *s = Dom->Str();
					if (!s)
					{
						*Dst = -1;
						break;
					}

					int sLen = strlen(s);
					const char *sub = Arg[0]->Str();
					int start = Arg.Length() > 1 ? Arg[1]->CastInt32() : 0;
					int end = Arg.Length() > 2 ? Arg[2]->CastInt32() : -1;								

					if (start >= sLen)
					{
						*Dst = -1;
						break;
					}
					char *sStart = (char*)s + start;
					char *pos;
					if (end >= 0)
						pos = strnstr(sStart, sub, end);
					else
						pos = strstr(sStart, sub);

					if (pos)
						*Dst = (int64) (pos - s);
					else
						*Dst = -1;
					break;
				}
				case StrRfind:
				{
					const char *s = Dom->Str();
					if (!s)
					{
						*Dst = -1;
						break;
					}

					int sLen = strlen(s);
					const char *sub = Arg[0]->Str();
					int start_idx = Arg.Length() > 1 ? Arg[1]->CastInt32() : 0;
					int end_idx = Arg.Length() > 2 ? Arg[2]->CastInt32() : -1;								

					if (start_idx >= sLen)
					{
						*Dst = -1;
						break;
					}
					int sublen = strlen(sub);
					char *cur = (char*)s + start_idx;
					char *end = end_idx >= 0 ? cur + end_idx : NULL;
					char *pos = NULL;
					while (true)
					{
						cur =	(end)
								?
								strnstr(cur, sub, end - cur)
								:
								strstr(cur, sub);
						if (cur)
						{
							pos = cur;
							cur += sublen;
						}
						else break;
					}
					
					if (pos)
						*Dst = (int64) (pos - s);
					else
						*Dst = -1;
					break;
				}
				case StrLower:
				{
					if (Dst != Dom)
						*Dst = Dom->CastString();
					
					StrLwr(Dst->Str());
					break;
				}
				case StrUpper:
				{
					if (Dst != Dom)
						*Dst = Dom->CastString();

					StrUpr(Dst->Str());
					break;
				}
				case StrStrip:
				{
					char *s = Dom->Str();
					if (s)
					{
						char *start = s;
						char *end = s + strlen(s);
						while (start < end && strchr(WhiteSpace, *start))
							start++;

						while (end > start && strchr(WhiteSpace, end[-1]))
							end--;
						
						Dst->OwnStr(NewStr(start, end - start));
					}
					else Dst->Empty();
					break;
				}
				case StrSub:
				{
					char *s = Dom->Str();
					if (s)
					{
						int Start = Arg.Length() > 0 ? Arg[0]->CastInt32() : 0;
						int End = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;
						int Len = strlen(s);
						if (End < 0 || End > Len)
							End = Len;
						if (Start < 0)
							Start = 0;
						if (Start <= End)
							Dst->OwnStr(NewStr(s + Start, End - Start));
						else
							Dst->Empty();
					}
					else Dst->Empty();
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print("%p IDomCall warning: Unexpected string member %s (%s:%i).\n",
									CurrentScriptAddress,
									sName,
									_FL);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		default:
		{
			const char *Type = GVariant::TypeToString(Dom->Type);
			char t[32];
			if (!Type)
			{
				sprintf_s(t, sizeof(t), "UnknownType(%i)", Dom->Type);
				Type = t;
			}
			
			GDomProperty p = LgiStringToDomProp(sName);
			if (p == ObjType)
			{
				*Dst = Type;
			}
			else
			{
				Dst->Empty();
				if (Log)
				{
					Log->Print("%s IDomCall warning: Unexpected type %s (Src=%s:%i IP=0x%x).\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								Type,
								_FL,
								CurrentScriptAddress);
				}
				Status = ScriptWarning;
			}
			break;
		}
	}

	#else

	GVariant *Count = NULL;
	switch (c.r->Scope)
	{
		case SCOPE_GLOBAL:
			Count = &Code->Globals[c.r->Index];
			c.r++;
			break;
		default:
			OnException(_FL, CurrentScriptAddress, "Unsupported scope.");
			return ScriptError;
	}
	
	int Args = Count->CastInt32();
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i ? ", " : "", c.r->GetStr());
		#endif
		c.r++;
	}
	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif

	#endif
	break;
}
case IDebug:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Debugger\n", CurrentScriptAddress-1);
	#elif VM_EXECUTE
	OnException(_FL, CurrentScriptAddress-1, "ShowDebugger");
	return ScriptWarning;
	#endif
	break;
}

#undef Resolve
#undef GResolveRef
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
			int Param = 0;
			switch (Type)
			{
				case RunStepLine:
					Param = NearestLine(CurrentScriptAddress);
					break;
				case RunStepOut:
					Param = (int)Frames.Length();
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
					/*
	This file is included in both the GVirtualMachinePriv::Execute and GVirtualMachinePriv::Decompile
	That way the "parsing" of instructions is the same.
	
	During decompile the define VM_DECOMP is active.
	
	During execution the define VM_EXECUTE is active.

*/

#ifdef VM_EXECUTE
#define Resolve()		&Scope[c.r->Scope][c.r->Index]; c.r++
#define GResolveRef		GVariant *
#else
#define Resolve()		c.r++
#define GResolveRef		GVarRef *
#endif

default:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("\t%p Unknown instruction %i (0x%x)\n",
					CurrentScriptAddress - 1,
					c.u8[-1], c.u8[-1]);
	#endif
	OnException(_FL, CurrentScriptAddress, "Unknown instruction");
	SetScriptError;
	break;
}
case INop:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Nop\n", CurrentScriptAddress - 1);
	#endif
	break;
}
case ICast:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Cast %s",
					CurrentScriptAddress - 1,
					c.r[0].GetStr());
	#endif
	GResolveRef Var = Resolve();
	uint8 Type = *c.u8++;
	#if VM_DECOMP
	if (Log)
		Log->Print(" to %s\n", GVariant::TypeToString((GVariantType)Type));
	#endif
	#if VM_EXECUTE
	switch (Type)
	{
		case GV_INT32:
		{
			*Var = Var->CastInt32();
			break;
		}
		case GV_STRING:
		{
			*Var = Var->CastString();
			break;
		}
		case GV_DOM:
		{
			*Var = Var->CastDom();
			break;
		}
		case GV_DOUBLE:
		{
			*Var = Var->CastDouble();
			break;
		}
		case GV_INT64:
		{
			*Var = Var->CastInt32();
			break;
		}
		case GV_BOOL:
		{
			*Var = Var->CastBool();
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s ICast warning: unknown type %i/%s\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Var->Type,
							GVariant::TypeToString(Var->Type));
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IBreak:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Break\n", CurrentScriptAddress - 1);
	#endif
	break;
}
case IAssign:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Assign %s <- %s\n",
				CurrentScriptAddress - 1,
				c.r[0].GetStr(),
				c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	CheckParam(Dst != Src);
	*Dst = *Src;
	#endif
	break;
}
case IJump:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Jump by %i (to 0x%x)\n",
			CurrentScriptAddress - 1,
			c.i32[0],
			CurrentScriptAddress + 4 + c.i32[0]);
	#endif

	int32 Jmp = *c.i32++;
	#ifdef VM_EXECUTE
	CheckParam(Jmp != 0);
	c.u8 += Jmp;
	#endif
	break;
}
case IJumpZero:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p JumpZ(%s) by 0x%x\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.i32[1]);
	#endif

	GResolveRef Exp = Resolve();
	int32 Jmp = *c.i32++;
	#ifdef VM_EXECUTE
	CheckParam(Jmp != 0);
	if (!Exp->CastInt32())
	{
		c.u8 += Jmp;
	}
	#endif
	break;
}
// case IUnaryPlus:
case IUnaryMinus:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p UnaryMinus %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef Var = Resolve();
	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_DOUBLE:
			*Var = -Var->CastDouble();
			break;
		case GV_INT64:
			*Var = -Var->CastInt64();
			break;
		default:
			*Var = -Var->CastInt32();
			break;
	}
	#endif
	break;
}
case IPlus:
case IPlusEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Plus %s += %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	if (Dst->Str())
	{
		int dlen = strlen(Dst->Str());
		char *ss;
		GVariant SrcTmp;
		
		switch (Src->Type)
		{
			case GV_NULL:
				ss = (char*)"(null)";
				break;
			case GV_STRING:
				ss = Src->Str();
				break;
			default:
				SrcTmp = *Src;
				ss = SrcTmp.CastString();
				break;
		}

		if (ss)
		{
			int slen = strlen(ss);
			char *s = new char[slen + dlen + 1];
			if (s)
			{
				memcpy(s, Dst->Value.String, dlen);
				memcpy(s + dlen, ss, slen);
				s[dlen + slen] = 0;
				DeleteArray(Dst->Value.String);
				Dst->Value.String = s;
			}
		}
	}
	else switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() + Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() + Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() + Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMinus:
case IMinusEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Minus %s -= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() - Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() - Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() - Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMul:
case IMulEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Mul %s *= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() * Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() * Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() * Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IDiv:
case IDivEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Div %s /= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = Dst->CastDouble() / Src->CastDouble();
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() / Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() / Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IMod:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Mod %s %= %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr(),
			c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	switch (DecidePrecision(Dst->Type, Src->Type))
	{
		case GV_DOUBLE:
			*Dst = fmod(Dst->CastDouble(), Src->CastDouble());
			break;
		case GV_INT64:
			*Dst = Dst->CastInt64() % Src->CastInt64();
			break;
		default:
			*Dst = Dst->CastInt32() % Src->CastInt32();
			break;
	}
	#endif
	break;
}
case IPostInc:
case IPreInc:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p PostInc %s\n",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef v = Resolve();
	#ifdef VM_EXECUTE
	switch (v->Type)
	{
		case GV_DOUBLE:
			*v = v->Value.Dbl + 1;
			break;
		case GV_INT64:
			*v = v->Value.Int64 + 1;
			break;
		default:
			*v = v->CastInt32() + 1;
			break;
	}
	#endif
	break;
}
case IPostDec:
case IPreDec:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p PostDec %sn",
			CurrentScriptAddress - 1,
			c.r[0].GetStr());
	#endif

	GResolveRef v = Resolve();
	#ifdef VM_EXECUTE
	switch (v->Type)
	{
		case GV_DOUBLE:
			*v = v->Value.Dbl - 1;
			break;
		case GV_INT64:
			*v = v->Value.Int64 - 1;
			break;
		default:
			*v = v->CastInt32() - 1;
			break;
	}
	#endif
	break;
}
case IEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s == %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();

	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) == 0;
	#endif
	break;
}
case INotEquals:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p %s != %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();

	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) != 0;
	#endif
	break;
}
case ILessThan:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) < 0;
	#endif
	break;
}
case ILessThanEqual:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) <= 0;
	#endif
	break;
}
case IGreaterThan:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) > 0;
	#endif
	break;
}
case IGreaterThanEqual:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s < %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	
	#ifdef VM_EXECUTE
	*Dst = CompareVariants(Dst, Src) >= 0;
	#endif
	break;
}
case ICallMethod:
{
	GFunc *Meth = *c.fn++;
	if (!Meth)
	{
		Log->Print(	"%s ICallMethod error: No method struct.\n",
					Code->AddrToSourceRef(CurrentScriptAddress - sizeof(Meth)));
		SetScriptError;
		break;
	}

	#ifdef VM_DECOMP
	if (Log)
	{
		Log->Print("%p Call: %s = %s(",
				CurrentScriptAddress - sizeof(Meth) - 1,
				c.r[0].GetStr(),
				Meth->Method.Get());
	}
	#endif
	
	GResolveRef Ret = Resolve();
	uint16 Args = *c.u16++;

	#ifdef VM_EXECUTE			
	LScriptArguments Arg(Vm, Ret);
	#endif
	
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i?", ":"", c.r[0].GetStr());
		#endif

		#if VM_EXECUTE
		Arg[i] = Resolve();
		CheckParam(Arg[i] != NULL);
		#else
		c.r++;
		#endif
	}

	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif
	
	#if VM_EXECUTE
	GHostFunc *Hf = dynamic_cast<GHostFunc*>(Meth);
	if (Hf)
	{
		if (!(Hf->Context->*(Hf->Func))(Arg))
		{
			if (Log)
				Log->Print(	"%s ICallMethod error: Method '%s' failed.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Meth->Method.Get());
			SetScriptError;
		}		
	}
	else
	{
		// Fixme
		if (!Meth->Call(NULL, Arg))
		{
			if (Log)
				Log->Print(	"%s ICallMethod error: Method '%s' failed.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Meth->Method.Get());
			SetScriptError;
		}
	}
	#endif
	break;
}
case ICallScript:
{
	int32 FuncAddr = *c.i32++;
	if (FuncAddr < 0 || (uint32)FuncAddr >= Code->ByteCode.Length())
	{
		Log->Print(	"%s ICallScript error: Script function call invalid addr '%p'.\n",
					Code->AddrToSourceRef(CurrentScriptAddress - sizeof(FuncAddr)),
					FuncAddr);
		SetScriptError;
		break;
	}

	uint16 Frame = *c.u16++;

	#if VM_DECOMP
	if (Log)
		Log->Print("%p CallScript: %s = %p(frame=%i)(",
			CurrentScriptAddress - 5,
			c.r[0].GetStr(),
			FuncAddr,
			Frame);
	#endif

	#ifdef VM_EXECUTE

	// Set up stack for function call
	int CurFrameSize = Frames.Last().CurrentFrameSize;
	StackFrame &Sf = Frames.New();
	Sf.CurrentFrameSize = Frame;
	Sf.PrevFrameStart = Locals.Length() ? Scope[1] - &Locals[0] : 0;
	Sf.ReturnValue = *c.r++;
	Sf.ReturnValue.Index -= CurFrameSize;
	uint16 Args = *c.u16++;

	// Increase the local stack size
	int LocalsBase = Locals.Length();
	Locals.SetFixedLength(false);
	Locals.Length(LocalsBase + Frame);
	Locals.SetFixedLength();
	
	// Put the arguments of the function call into the local array
	GArray<GVariant*> Arg;
	
	#else
	
	GResolveRef Ret = Resolve();
	int Args = *c.u16++;
	
	#endif
	
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i?",":"", c.r[0].GetStr());
		#endif

		#if VM_EXECUTE
		Locals[LocalsBase+i] = *Resolve();
		#else
		c.r++;
		#endif
	}

	#if VM_EXECUTE
	Scope[SCOPE_LOCAL] = &Locals[LocalsBase];

	// Set IP to start of function
	Sf.ReturnIp = CurrentScriptAddress;
	c.u8 = Base + FuncAddr;
	#endif

	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif
	break;
}
case IRet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Ret %s\n", CurrentScriptAddress - 1, c.r[0].GetStr());
	#endif

	GResolveRef ReturnValue = Resolve();

	#ifdef VM_EXECUTE
	if (Frames.Length() > 0)
	{
		StackFrame Sf = Frames[Frames.Length()-1];
		GVarRef &Ret = Sf.ReturnValue;
		GVariant *RetVar = &Scope[Ret.Scope][Ret.Index];
		// LgiTrace("IRet to %i:%i\n", Ret.Scope, Ret.Index);
		if (Ret.Scope == SCOPE_LOCAL)
			LgiAssert(Locals.PtrCheck(RetVar));
		*RetVar = *ReturnValue;
		CheckParam(RetVar->Type == ReturnValue->Type);

		Frames.Length(Frames.Length()-1);
		
		Locals.SetFixedLength(false);
		if (Locals.Length() >= Sf.CurrentFrameSize)
		{
			int Base = Locals.Length() - Sf.CurrentFrameSize;
			if (ArgsOutput)
			{
				if (Frames.Length() == 0)
				{
					for (unsigned i=0; i<ArgsOutput->Length(); i++)
					{
						*(*ArgsOutput)[i] = Locals[Base+i];
					}
				}
			}
			// LgiTrace("%s:%i Locals %i -> %i\n", _FL, Locals.Length(), Base);
			Locals.Length(Base);
			Scope[SCOPE_LOCAL] = &Locals[Sf.PrevFrameStart];
		}
		else
		{
			// LgiTrace("%s:%i - Locals %i -> %i\n", _FL, Locals.Length(), 0);
			Locals.Length(0);
			Scope[SCOPE_LOCAL] = NULL;
		}
		Locals.SetFixedLength();

		c.u8 = Base + Sf.ReturnIp;
	}
	else
	{
		ExitScriptExecution;
	}
	#endif
	break;
}
case IArrayGet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p ArrayGet %s = %s[%s]\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Var = Resolve();
	GResolveRef Idx = Resolve();

	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_LIST:
		{
			CheckParam(Var->Value.Lst);
			GVariant *t = Var->Value.Lst->ItemAt(Idx->CastInt32());
			if (t)
			{
				if (Var == Dst)
				{
					if (Var->Value.Lst->Delete(t))
					{
						*Var = *t;
						DeleteObj(t);
					}
					else CheckParam(!"List delete failed.");
				}
				else *Dst = *t;
			}
			else Dst->Empty();
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Var->Value.Hash);
			GVariant *t = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			if (t) *Dst = *t;
			else Dst->Empty();
			break;
		}
		case GV_CUSTOM:
		{
			GCustomType *T = Var->Value.Custom.Dom;
			size_t Sz = T->Sizeof();
			int Index = Idx->CastInt32();

			Dst->Type = GV_CUSTOM;
			Dst->Value.Custom.Dom = T;
			Dst->Value.Custom.Data = Var->Value.Custom.Data + (Sz * Index);
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s IArrayGet warning: Can't array deref variant type %i\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							Var->Type);
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IArraySet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print(	"%p ArraySet %s[%s] = %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GResolveRef Var = Resolve();
	GResolveRef Idx = Resolve();
	GResolveRef Val = Resolve();
	#ifdef VM_EXECUTE
	switch (Var->Type)
	{
		case GV_LIST:
		{
			CheckParam(Var->Value.Lst);
			(*Var->Value.Lst).Insert(new GVariant(*Val), Idx->CastInt32());
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Var->Value.Hash);
			GVariant *Old = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			DeleteObj(Old);
			Var->Value.Hash->Add(Idx->CastString(), new GVariant(*Val));
			break;
		}
		default:
		{
			if (Log)
				Log->Print(	"%s IArraySet warning: Can't dereference type '%s'\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							GVariant::TypeToString(Var->Type));
			Status = ScriptWarning;
			break;
		}
	}
	#endif
	break;
}
case IAnd:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s && %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	*Dst = (Dst->CastInt32() != 0) && (Src->CastInt32() != 0);
	#endif
	break;
}
case IOr:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s || %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Src = Resolve();
	#ifdef VM_EXECUTE
	*Dst = (Dst->CastInt32() != 0) || (Src->CastInt32() != 0);
	#endif
	break;
}
case INot:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = !%s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[0].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	#ifdef VM_EXECUTE
	*Dst = !Dst->CastBool();
	#endif
	break;
}
case IDomGet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = %s->DomGet(%s, %s)\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr(),
					c.r[3].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();
	GResolveRef Arr = Resolve();

	#ifdef VM_EXECUTE

		// Return "NULL" in Dst on error
		if (Dst != Dom)
			Dst->Empty();

		switch (Dom->Type)
		{
			case GV_DOM:
			case GV_STREAM:
			case GV_GSURFACE:
			{
				GDom *dom = Dom->CastDom();
				CheckParam(dom != NULL);
				char *sName = Name->Str();
				CheckParam(sName);
				bool Ret = dom->GetVariant(sName, *Dst, CastArrayIndex(Arr));
				if (!Ret)
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomGet warning: Unexpected %s member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									GVariant::TypeToString(Dom->Type),
									sName);
					Status = ScriptWarning;
				}
				break;
			}
			case GV_DATETIME:
			{
				CheckParam(Dom->Value.Date != NULL);
				char *sName = Name->Str();
				CheckParam(sName);
				bool Ret = Dom->Value.Date->GetVariant(sName, *Dst, CastArrayIndex(Arr));
				if (!Ret)
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomGet warning: Unexpected %s member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									GVariant::TypeToString(Dom->Type),
									sName);
					Status = ScriptWarning;
				}
				break;
			}
			case GV_CUSTOM:
			{
				GCustomType *Type = Dom->Value.Custom.Dom;
				if (Type)
				{
					int Fld;
					if (Name->Type == GV_INT32)
						Fld = Name->Value.Int;
					else
						Fld = Type->IndexOf(Name->Str());
					
					int Index = Arr ? Arr->CastInt32() : 0;
					Type->Get(Fld, *Dst, Dom->Value.Custom.Data, Index);
				}
				break;
			}
			case GV_LIST:
			{
				CheckParam(Dom->Value.Lst);
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Lst->Length();
				break;
			}
			case GV_HASHTABLE:
			{
				CheckParam(Dom->Value.Hash);
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Hash->Length();
				break;
			}
			case GV_BINARY:
			{
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				if (p == ObjLength)
					(*Dst) = Dom->Value.Binary.Length;
				break;
			}
			case GV_STRING:
			{
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = LgiStringToDomProp(sName);
				switch (p)
				{
					case ObjLength:
					{
						(*Dst) = (int)strlen(Dom->Str());
						break;
					}
					case TypeInt:
					{
						(*Dst) = Dom->CastInt32();
						break;
					}
					case TypeDouble:
					{
						(*Dst) = Dom->CastDouble();
						break;
					}
					default:
					{
						Dst->Empty();
						if (Log)
							Log->Print("%s IDomGet warning: Unexpected string member '%s'.\n",
										Code->AddrToSourceRef(CurrentScriptAddress),
										sName);
						Status = ScriptWarning;
						break;
					}
				}
				break;
			}
			case GV_NULL:
			{
				#if 1
				if (Log)
					Log->Print("%s IDomGet warning: Can't deref NULL object.\n",
								Code->AddrToSourceRef(CurrentScriptAddress));
				#else // If you want exceptions on NULL deref
				OnException(_FL, CurrentScriptAddress-1, "NULL Dom Ptr");
				#endif
				Status = ScriptWarning;
				break;
			}
			default:
			{
				if (Log)
					Log->Print("%s IDomGet warning: Unexpected type %s (Src=%s:%i IP=0x%x).\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								_FL,
								CurrentScriptAddress);
				Status = ScriptWarning;
				break;
			}
		}

	#endif
	break;
}
case IDomSet:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s->DomSet(%s, %s) = %s\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr(),
					c.r[3].GetStr());
	#endif

	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();
	GResolveRef Arr = Resolve();
	GResolveRef Value = Resolve();
	
	#ifdef VM_EXECUTE

	char *sName = Name->Str();
	if (!sName)
	{
		if (Log)
			Log->Print("%s IDomSet error: No name string.\n",
						Code->AddrToSourceRef(CurrentScriptAddress));
		SetScriptError;
		break;
	}

	switch (Dom->Type)
	{
		case GV_DOM:
		// case GV_GFILE:
		case GV_STREAM:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			CheckParam(dom != NULL);
			bool Ret = dom->SetVariant(sName, *Value, CastArrayIndex(Arr));
			if (!Ret)
			{
				if (Log)
					Log->Print("%s IDomSet warning: Unexpected %s member '%s'.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_DATETIME:
		{
			CheckParam(Dom->Value.Date != NULL);
			bool Ret = Dom->Value.Date->SetVariant(sName, *Value, CastArrayIndex(Arr));
			if (!Ret)
			{
				if (Log)
					Log->Print("%s IDomSet warning: Unexpected %s member '%s'.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_CUSTOM:
		{
			GCustomType *Type = Dom->Value.Custom.Dom;
			if (Type)
			{
				int Fld;
				if (IsDigit(*sName))
					Fld = atoi(sName);
				else
					Fld = Type->IndexOf(sName);
				
				int Index = Arr ? Arr->CastInt32() : 0;
				if (!Type->Set(Fld, *Value, Dom->Value.Custom.Data, Index) &&
					Log)
				{
					Log->Print("%s IDomSet warning: Couldn't set '%s' on custom type.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				}
			}
			break;
		}
		case GV_STRING:
		{
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					char *s;
					int DLen = Value->CastInt32();
					if (DLen && (s = new char[DLen+1]))
					{
						int SLen = Dom->Str() ? strlen(Dom->Str()) : 0;
						if (SLen)
							memcpy(s, Dom->Str(), SLen);
						memset(s+SLen, ' ', DLen-SLen);
						s[DLen] = 0;
						DeleteArray(Dom->Value.String);
						Dom->Value.String = s;
					}
					else Dom->Empty();

					break;
				}
				case TypeInt:
				{
					*Dom = Value->CastInt32();
					Dom->Str();
					break;
				}
				case TypeDouble:
				{
					*Dom = Value->CastDouble();
					Dom->Str();
					break;
				}
				default:
				{
					if (Log)
						Log->Print("%s IDomSet warning: Unexpected string member %s.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		default:
		{
			if (Log)
				Log->Print("%s IDomSet warning: Unexpected type %s.\n",
							Code->AddrToSourceRef(CurrentScriptAddress),
							GVariant::TypeToString(Dom->Type));
			Status = ScriptWarning;
			break;
		}
	}

	#endif
	break;
}
case IDomCall:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p %s = %s->DomCall(%s, ",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

	GVarRef DstRef = *c.r;
	GResolveRef Dst = Resolve();
	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();

	#ifdef VM_EXECUTE

	GResolveRef Args = Resolve();
	int ArgCount = Args->CastInt32();
	char *sName = Name->Str();
	CheckParam(sName)

	if (Dom->Type == GV_CUSTOM)
	{
		#define DEBUG_CUSTOM_METHOD_CALL	1
		
		GCustomType *t = Dom->Value.Custom.Dom;
		CheckParam(t);
		GCustomType::Method *m = t->GetMethod(sName);
		CheckParam(m);
		CheckParam(m->Params.Length() == ArgCount);
		
		// Set up new stack frame...
		StackFrame &Sf = Frames.New();
		Sf.CurrentFrameSize = m->FrameSize;
		Sf.PrevFrameStart = Locals.Length() ? Scope[1] - &Locals[0] : 0;
		Sf.ReturnValue = DstRef;

		// Increase the local stack size
		AddLocalSize(m->FrameSize + 1);
		
		#if DEBUG_CUSTOM_METHOD_CALL
		LgiTrace("CustomType.Call(%s) Args=%i, Frame=%i, Addr=%i, LocalsBase=%i ",
				sName, ArgCount, m->FrameSize, m->Address, LocalsBase);
		#endif
		
		int i = LocalsBase;
		Locals[i++] = *Dom; // this pointer...
		#if DEBUG_CUSTOM_METHOD_CALL
		GString s = Locals[i-1].ToString();
		LgiTrace("This=%s, ", s.Get());
		#endif
		int end = i + ArgCount;
		while (i < end)
		{
			Locals[i++] = *Resolve();
			#if DEBUG_CUSTOM_METHOD_CALL
			s = Locals[i-1].ToString();
			LgiTrace("[%i]=%s, ", i-1, s.Get());
			#endif
		}

		// Now adjust the local stack to point to the locals for the function
		Scope[1] = Locals.Length() ? &Locals[LocalsBase] : NULL;

		// Set IP to start of function
		Sf.ReturnIp = CurrentScriptAddress;
		c.u8 = Base + m->Address;

		#if DEBUG_CUSTOM_METHOD_CALL
		LgiTrace("\n");
		#endif
		break;
	}	

	GArray<GVariant*> Arg;
	Arg.Length(ArgCount);
	for (int i=0; i<ArgCount; i++)
	{
		Arg[i] = Resolve();
	}
	
	switch (Dom->Type)
	{
		case GV_DOM:
		case GV_STREAM:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			CheckParam(dom);
			bool Ret = dom->CallMethod(sName, Dst, Arg);
			if (!Ret)
			{
				Dst->Empty();
				if (Log)
					Log->Print("%s IDomCall warning: %s(...) failed.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_DATETIME:
		{
			CheckParam(Dom->Value.Date);
			bool Ret = Dom->Value.Date->CallMethod(sName, Dst, Arg);
			if (!Ret)
			{
				Dst->Empty();
				if (Log)
					Log->Print("%s IDomCall warning: %s(...) failed.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								sName);
				Status = ScriptWarning;
			}
			break;
		}
		case GV_LIST:
		{
			CheckParam(Dom->Value.Lst);
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					*Dst = Dom->Value.Lst->Length();
					break;
				}
				case ObjType:
				{
					*Dst = "List";
					break;
				}
				case ContainerAdd:
				{
					if (Arg.Length() > 0 &&
						Arg[0])
					{
						int Index = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;

						GVariant *v = new GVariant;
						*v = *Arg[0];
						Dom->Value.Lst->Insert(v, Index);
					}
					break;
				}
				case ContainerDelete:
				{
					for (unsigned i=0; i<Arg.Length(); i++)
					{
						GVariant *Idx = Arg[i];
						if (Idx)
						{
							int32 n = Arg[i]->CastInt32();
							GVariant *Elem = Dom->Value.Lst->ItemAt(n);
							if (Elem)
							{
								Dom->Value.Lst->Delete(Elem);
								DeleteObj(Elem);
							}
						}
					}
					break;
				}
				case ContainerHasKey:
				{
					if (Arg.Length() > 0 && Arg[0])
					{
						int Index = Arg[0]->CastInt32();
						*Dst = (bool) (Index >= 0 && Index < Dom->Value.Lst->Length());
					}
					else
					{
						*Dst = false;
					}
					break;
				}
				case ContainerSort:
				{
					GVariant *Param = Arg.Length() > 0 ? Arg[0] : NULL;
					Dom->Value.Lst->Sort(GVariantCmp, (NativeInt)Param);
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print(	"%s IDomCall warning: Unexpected list member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		case GV_HASHTABLE:
		{
			CheckParam(Dom->Value.Hash);
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					*Dst = Dom->Value.Hash->Length();
					break;
				}
				case ObjType:
				{
					*Dst = "HashTable";
					break;
				}
				case ContainerAdd:
				{
					if (Arg.Length() == 2 &&
						Arg[0] &&
						Arg[1])
					{
						char *Key = Arg[1]->Str();
						if (Key)
						{
							GVariant *v = new GVariant;
							*v = *Arg[0];
							Dom->Value.Hash->Add(Key, v);
						}
					}
					break;
				}
				case ContainerDelete:
				{
					if (Arg.Length() == 1 &&
						Arg[0])
					{
						char *Key = Arg[0]->Str();
						if (Key)
						{
							GVariant *v = (GVariant*) Dom->Value.Hash->Find(Key);
							if (v)
							{
								Dom->Value.Hash->Delete(Key);
								delete v;
							}
						}
					}
					break;
				}
				case ContainerHasKey:
				{
					if (Arg.Length() > 0 && Arg[0])
					{
						char *Key = Arg[0]->Str();
						*Dst = (bool) (Dom->Value.Hash->Find(Key) != NULL);
					}
					else
					{
						*Dst = false;
					}
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print("%s IDomCall warning: Unexpected hashtable member '%s'.\n",
									Code->AddrToSourceRef(CurrentScriptAddress),
									sName);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		case GV_BINARY:
		{
			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				default:
					break;
				case ObjLength:
					*Dst = Dom->Value.Binary.Length;
					break;
			}
			break;
		}
		case GV_STRING:
		{
			if (Arg.Length() > 0 && !Arg[0])
			{
				Dst->Empty();
				break;
			}

			GDomProperty p = LgiStringToDomProp(sName);
			switch (p)
			{
				case ObjLength:
				{
					char *s = Dom->Str();
					*Dst = (int) (s ? strlen(s) : 0);
					break;
				}
				case ObjType:
				{
					*Dst = "String";
					break;
				}
				case StrJoin:
				{
					switch (Arg[0]->Type)
					{
						case GV_LIST:
						{
							GStringPipe p(256);
							List<GVariant> *Lst = Arg[0]->Value.Lst;
							const char *Sep = Dom->CastString();
							GVariant *v = Lst->First();
							if (v)
							{
								GVariant Tmp = *v;
								p.Print("%s", Tmp.CastString());
								while ((v = Lst->Next()))
								{
									Tmp = *v;
									p.Print("%s%s", Sep, Tmp.CastString());
								}
							}
							Dst->OwnStr(p.NewStr());
							break;
						}
						default:
						{
							*Dst = *Arg[0];
							Dst->CastString();
							break;
						}
					}
					break;
				}								
				case StrSplit:
				{
					const char *Sep = Arg[0]->Str();
					if (!Sep)
					{
						Dst->Empty();
						break;
					}
					
					GVariant Tmp;
					if (Dst == Dom)
					{
						Tmp = *Dom;
						Dom = &Tmp;
					}

					Dst->SetList();
					
					int SepLen = strlen(Sep);
					int MaxSplit = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;
					const char *c = Dom->CastString();
					while (c && *c)
					{
						if (MaxSplit > 0 && Dst->Value.Lst->Length() >= MaxSplit)
							break;

						const char *next = strstr(c, Sep);
						if (!next)
							break;
						
						GVariant *v = new GVariant;
						v->OwnStr(NewStr(c, next - c));
						Dst->Value.Lst->Insert(v);
						
						c = next + SepLen;
					}

					if (c && *c)
					{
						GVariant *v = new GVariant;
						v->OwnStr(NewStr(c));
						Dst->Value.Lst->Insert(v);
					}
					break;
				}								
				case StrFind:
				{
					const char *s = Dom->Str();
					if (!s)
					{
						*Dst = -1;
						break;
					}

					int sLen = strlen(s);
					const char *sub = Arg[0]->Str();
					int start = Arg.Length() > 1 ? Arg[1]->CastInt32() : 0;
					int end = Arg.Length() > 2 ? Arg[2]->CastInt32() : -1;								

					if (start >= sLen)
					{
						*Dst = -1;
						break;
					}
					char *sStart = (char*)s + start;
					char *pos;
					if (end >= 0)
						pos = strnstr(sStart, sub, end);
					else
						pos = strstr(sStart, sub);

					if (pos)
						*Dst = (int64) (pos - s);
					else
						*Dst = -1;
					break;
				}
				case StrRfind:
				{
					const char *s = Dom->Str();
					if (!s)
					{
						*Dst = -1;
						break;
					}

					int sLen = strlen(s);
					const char *sub = Arg[0]->Str();
					int start_idx = Arg.Length() > 1 ? Arg[1]->CastInt32() : 0;
					int end_idx = Arg.Length() > 2 ? Arg[2]->CastInt32() : -1;								

					if (start_idx >= sLen)
					{
						*Dst = -1;
						break;
					}
					int sublen = strlen(sub);
					char *cur = (char*)s + start_idx;
					char *end = end_idx >= 0 ? cur + end_idx : NULL;
					char *pos = NULL;
					while (true)
					{
						cur =	(end)
								?
								strnstr(cur, sub, end - cur)
								:
								strstr(cur, sub);
						if (cur)
						{
							pos = cur;
							cur += sublen;
						}
						else break;
					}
					
					if (pos)
						*Dst = (int64) (pos - s);
					else
						*Dst = -1;
					break;
				}
				case StrLower:
				{
					if (Dst != Dom)
						*Dst = Dom->CastString();
					
					StrLwr(Dst->Str());
					break;
				}
				case StrUpper:
				{
					if (Dst != Dom)
						*Dst = Dom->CastString();

					StrUpr(Dst->Str());
					break;
				}
				case StrStrip:
				{
					char *s = Dom->Str();
					if (s)
					{
						char *start = s;
						char *end = s + strlen(s);
						while (start < end && strchr(WhiteSpace, *start))
							start++;

						while (end > start && strchr(WhiteSpace, end[-1]))
							end--;
						
						Dst->OwnStr(NewStr(start, end - start));
					}
					else Dst->Empty();
					break;
				}
				case StrSub:
				{
					char *s = Dom->Str();
					if (s)
					{
						int Start = Arg.Length() > 0 ? Arg[0]->CastInt32() : 0;
						int End = Arg.Length() > 1 ? Arg[1]->CastInt32() : -1;
						int Len = strlen(s);
						if (End < 0 || End > Len)
							End = Len;
						if (Start < 0)
							Start = 0;
						if (Start <= End)
							Dst->OwnStr(NewStr(s + Start, End - Start));
						else
							Dst->Empty();
					}
					else Dst->Empty();
					break;
				}
				default:
				{
					Dst->Empty();
					if (Log)
						Log->Print("%p IDomCall warning: Unexpected string member %s (%s:%i).\n",
									CurrentScriptAddress,
									sName,
									_FL);
					Status = ScriptWarning;
					break;
				}
			}
			break;
		}
		default:
		{
			const char *Type = GVariant::TypeToString(Dom->Type);
			char t[32];
			if (!Type)
			{
				sprintf_s(t, sizeof(t), "UnknownType(%i)", Dom->Type);
				Type = t;
			}
			
			GDomProperty p = LgiStringToDomProp(sName);
			if (p == ObjType)
			{
				*Dst = Type;
			}
			else
			{
				Dst->Empty();
				if (Log)
				{
					Log->Print("%s IDomCall warning: Unexpected type %s (Src=%s:%i IP=0x%x).\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								Type,
								_FL,
								CurrentScriptAddress);
				}
				Status = ScriptWarning;
			}
			break;
		}
	}

	#else

	GVariant *Count = NULL;
	switch (c.r->Scope)
	{
		case SCOPE_GLOBAL:
			Count = &Code->Globals[c.r->Index];
			c.r++;
			break;
		default:
			OnException(_FL, CurrentScriptAddress, "Unsupported scope.");
			return ScriptError;
	}
	
	int Args = Count->CastInt32();
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i ? ", " : "", c.r->GetStr());
		#endif
		c.r++;
	}
	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif

	#endif
	break;
}
case IDebug:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Debugger\n", CurrentScriptAddress-1);
	#elif VM_EXECUTE
	OnException(_FL, CurrentScriptAddress-1, "ShowDebugger");
	return ScriptWarning;
	#endif
	break;
}

#undef Resolve
#undef GResolveRef
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

	LScriptArguments Args(this, Return);
	GExecutionStatus s = d->Setup(Code, StartOffset, Log, NULL, &Args);
	if (s != ScriptSuccess || !StartImmediately)
		return s;

	return d->Run(GVirtualMachinePriv::RunContinue);
}

GExecutionStatus GVirtualMachine::ExecuteFunction(GCompiledCode *Code, GFunctionInfo *Func, LScriptArguments &Args, GStream *Log, LScriptArguments *ArgsOut)
{
	GCompiledCode *Cc = dynamic_cast<GCompiledCode*>(Code);
	if (!Cc)
		return ScriptError;

	GExecutionStatus s = d->Setup(Cc, 0, Log, Func, &Args);
	if (s != ScriptSuccess)
		return s;

	d->ArgsOutput = ArgsOut;
	Args.Vm = this;
	GExecutionStatus r = d->Run(GVirtualMachinePriv::RunContinue);
	Args.Vm = NULL;
	return r;
}

GVmDebugger *GVirtualMachine::OpenDebugger(GCompiledCode *Code, const char *Assembly)
{
	if (!d->Debugger)
	{
		if (!d->DbgCallback)
			return NULL;
		
		d->Debugger = d->DbgCallback->AttachVm(this, Code, Assembly);
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

void GVirtualMachine::SetTempPath(const char *Path)
{
	d->TempPath = Path;
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
	void PourText(size_t Start, ssize_t Length) override;
	void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour) override;
	void OnPaint(GSurface *pDC) override;
	bool Breakpoint(int Addr);
};

struct GScriptVmDebuggerPriv
{
	// Current script
	bool OwnVm;
	GAutoPtr<GVirtualMachine> Vm;
	GVmDebuggerCallback *Callback;
	GString Script, Assembly;
	GArray<CodeBlock> Blocks;
	size_t CurrentAddr;
	GArray<bool> LineIsAsm;
	GAutoPtr<GCompiledCode> Obj;
	GVariant Return;
	bool AcceptNotify;

	// Ui
	bool RunLoop;
	GView *Parent;
	GBox *Main;
	GBox *Sub;
	LList *SourceLst;
	GTabView *Tabs;
	GDebugView *Text;
	LList *Locals, *Globals, *Registers, *Stack;
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

void GDebugView::PourText(size_t Start, ssize_t Len)
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

GVmDebuggerWnd::GVmDebuggerWnd(GView *Parent, GVmDebuggerCallback *Callback, GVirtualMachine *Vm, GCompiledCode *Code, const char *Assembly)
{
	d = new GScriptVmDebuggerPriv;
	d->Parent = Parent;
	d->AcceptNotify = false;
	if (Vm)
		d->Vm.Reset(new GVirtualMachine(Vm));
	d->Callback = Callback;
	if (Code)
		d->Script = Code->GetSource();
	d->Assembly = Assembly;
	
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
		
		d->Sub->AddView(d->SourceLst = new LList(IDC_SOURCE_LST, 0, 0, 100, 100));
		d->SourceLst->GetCss(true)->Width(GCss::Len("200px"));
		d->SourceLst->AddColumn("Source", 200);
		d->Sub->AddView(d->Text = new GDebugView(d));
		
		d->Main->AddView(d->Tabs = new GTabView(IDC_TABS));
		d->Tabs->GetCss(true)->Height(GCss::Len("250px"));
		
		GTabPage *p = d->Tabs->Append("Variables");
		p->Append(d->VarsTbl = new GTableLayout(IDC_VARS_TBL));		
		
		int x = 0, y = 0;
		GLayoutCell *c = d->VarsTbl->GetCell(x++, y);
		c->Add(new GTextLabel(IDC_STATIC, 0, 0, -1, -1, "Globals:"));
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(new GTextLabel(IDC_STATIC, 0, 0, -1, -1, "Locals:"));
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(new GTextLabel(IDC_STATIC, 0, 0, -1, -1, "Registers:"));

		x = 0; y++;
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(d->Globals = new LList(IDC_GLOBALS, 0, 0, 100, 100));
		d->Globals->AddColumn("Name",100);
		d->Globals->AddColumn("Value",400);
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(d->Locals = new LList(IDC_LOCALS, 0, 0, 100, 100));
		d->Locals->AddColumn("Name",100);
		d->Locals->AddColumn("Value",400);
		c = d->VarsTbl->GetCell(x++, y);
		c->Add(d->Registers = new LList(IDC_REGISTERS, 0, 0, 100, 100));
		d->Registers->AddColumn("Name",100);
		d->Registers->AddColumn("Value",400);

		p = d->Tabs->Append("Stack");
		p->Append(d->Stack = new LList(IDC_STACK, 0, 0, 100, 100));
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
			LListItem *Match = NULL;

			d->SourceLst->MultiSelect(false);

			for (int b = dir.First(p); b; b = dir.Next())
			{
				if (!dir.IsDir())
				{
					char *n = dir.GetName();
					if (stristr(n, ".script") &&
						dir.Path(p, sizeof(p)))
					{
						LListItem *it = new LListItem;
						it->SetText(dir.GetName(), 0);
						it->SetText(p, 1);

						if (Code && Code->GetFileName())
						{
							if (_stricmp(p, Code->GetFileName()) == 0)
								Match = it;
						}

						d->SourceLst->Insert(it);
					}
				}
			}

			if (!Match && Code)
			{
				LListItem *it = new LListItem;
				if (it)
				{
					it->SetText(LgiGetLeaf(Code->GetFileName()), 0);
					it->SetText(Code->GetFileName(), 1);
					d->SourceLst->Insert(it);
					it->Select(true);
				}
			}
		}
	}

	d->AcceptNotify = true;
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

void GVmDebuggerWnd::UpdateVariables(LList *Lst, GVariant *Arr, ssize_t Len, char Prefix)
{
	if (!d->Vm || !Lst || !Arr)
		return;

	List<LListItem> all;
	Lst->GetAll(all);
	
	LListItem *it;
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
			it = new LListItem;
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
		ssize_t Sz = d->Text->GetSize();
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

	GFile f;
	if (f.Open(File, O_READ))
		d->Script = f.Read();
	else
		d->Script.Empty();
	d->Obj.Reset();
	if (d->Callback->CompileScript(d->Obj, File, d->Script))
	{
		GCompiledCode *Code = dynamic_cast<GCompiledCode*>(d->Obj.Get());
		if (Code)
		{
			d->Return.Empty();
			d->Vm->d->Frames.Length(0);

			LScriptArguments Args(d->Vm, &d->Return);
			d->Vm->d->Setup(Code, 0, d->Log, NULL, &Args);
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
	if (!d->AcceptNotify)
		return 0;

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
						LListItem *li = new LListItem;
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
				LListItem *it = d->SourceLst->GetSelected();
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

/////////////////////////////////////////////////////////////////////////////
bool LScriptArguments::Throw(const char *File, int Line, const char *Msg, ...)
{
	if (!Vm || !Vm->d)
		return false;

	va_list Arg;
	va_start(Arg, Msg);

	GString s;
	s.Printf(Arg, Msg);

	va_end(Arg);
	
	Vm->d->OnException(File, Line, 0, s);
	return true;
}
