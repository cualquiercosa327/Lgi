#include <math.h>
#include "Lgi.h"
#include "GScripting.h"
#include "GScriptingPriv.h"
#include "GBox.h"
#include "GTabView.h"
#include "GTextLog.h"
#include "GList.h"
#include "GToolBar.h"

#define TIME_INSTRUCTIONS		0
#define POST_EXECUTE_STATE		0

// #define BREAK_POINT				0x0000001F

#define ExitScriptExecution		c.u8 = e
#define SetScriptError			c.u8 = e; LgiAssert(0); Status = ScriptError
#define CurrentScriptAddress	(c.u8 - Base)

class GVirtualMachinePriv
{
	struct StackFrame
	{
		uint32 CurrentFrameSize;
		int PrevFrameStart;
		int ReturnIp;
		GVariant *ReturnValue;
	};

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
	enum RunType
	{
		RunContinue,
		RunStepInto,
		RunStepOver,
		RunStepOut
	};

	GStream *Log;
	GCompiledCode *Code;
	GExecutionStatus Status;
	GPtr c;
	GVariant Reg[MAX_REGISTER];
	GArray<GVariant> Locals;
	GVariant *Scope[3];
	GArray<StackFrame> Frames;
	RunType StepType;
	bool Loop;
	GVmDebuggerCallback *DbgCallback;
	GVmDebugger *Debugger;

	GVirtualMachinePriv(GVmDebuggerCallback *Callback)
	{
		Log = NULL;
		Code = NULL;
		Debugger = NULL;
		DbgCallback = Callback;
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
				Log->Print("null");
				break;
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

		GHashTbl<int, char*> Fn(0, false, -1, NULL);
		for (unsigned m=0; m<Code->Methods.Length(); m++)
		{
			GFunctionInfo *Info = Code->Methods[m];
			if (Info->StartAddr >= 0)
				Fn.Add(Info->StartAddr, Info->Name.Str());
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
				// #include "Instructions.h"
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
		Log->Print("\t%p Unknown instruction: %i\n", CurrentScriptAddress - 1, c.u8[-1]);
	#endif
	LgiAssert(!"Unknown instruction");
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
	LgiAssert(Dst != Src);
	*Dst = *Src;
	#endif
	break;
}
case IJump:
{
	#if VM_DECOMP
	if (Log)
		Log->Print("%p Jump by 0x%x\n",
			CurrentScriptAddress - 1,
			c.i32[0]);
	#endif

	int32 Jmp = *c.i32++;
	#ifdef VM_EXECUTE
	LgiAssert(Jmp != 0);
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
	LgiAssert(Jmp != 0);
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
	if (Var->Type == GV_DOUBLE)
		*Var = -Var->CastDouble();
	else
		*Var = -Var->CastInt32();
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
	else if (Dst->Type == GV_DOUBLE ||
			 Src->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() + Src->CastDouble();
	else
		*Dst = Dst->CastInt32() + Src->CastInt32();
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
	if (Dst->Type == GV_DOUBLE ||
		Src->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() - Src->CastDouble();
	else
		*Dst = Dst->CastInt32() - Src->CastInt32();
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
	if (Dst->Type == GV_DOUBLE ||
		Src->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() * Src->CastDouble();
	else
		*Dst = Dst->CastInt32() * Src->CastInt32();
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
	if (Dst->Type == GV_DOUBLE ||
		Src->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() / Src->CastDouble();
	else
		*Dst = Dst->CastInt32() / Src->CastInt32();
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
	if (Dst->Type == GV_DOUBLE ||
		Src->Type == GV_DOUBLE)
	{
		*Dst = fmod(Dst->CastDouble(), Src->CastDouble());
	}
	else
	{
		*Dst = Dst->CastInt32() % Src->CastInt32();
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
	if (v->Type == GV_DOUBLE)
		*v = v->CastDouble() + 1;
	else
		*v = v->CastInt32() + 1;
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
	if (v->Type == GV_DOUBLE)
		*v = v->CastDouble() - 1;
	else
		*v = v->CastInt32() - 1;
	#endif
	break;
}
case ICallMethod:
{
	GHostFunc *Meth = dynamic_cast<GHostFunc*>(*c.fn++);
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
		Log->Print("\t%p Call: %s = %s(",
				CurrentScriptAddress - sizeof(Meth),
				c.r[0].GetStr(),
				Meth->Method);
	}
	#endif
	
	GResolveRef Ret = Resolve();
	uint16 Args = *c.u16++;

	#ifdef VM_EXECUTE			
	GArray<GVariant*> Arg;
	#endif
	
	for (int i=0; i<Args; i++)
	{
		#if VM_DECOMP
		if (Log)
			Log->Print("%s%s", i?", ":"", c.r[0].GetStr());
		#endif

		#if VM_EXECUTE
		Arg[i] = Resolve();
		LgiAssert(Arg[i] != NULL);
		#else
		c.r++;
		#endif
	}

	#if VM_DECOMP
	if (Log)
		Log->Print(")\n");
	#endif
	
	#if VM_EXECUTE
	if (!(Meth->Context->*(Meth->Func))(Ret, Arg))
	{
		if (Log)
			Log->Print(	"%s ICallMethod error: Method '%s' failed.\n",
						Code->AddrToSourceRef(CurrentScriptAddress),
						Meth->Method);
		SetScriptError;
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
		Log->Print("%p ScriptCall: %s = %p(frame=%i)(",
			CurrentScriptAddress - 5,
			c.r[0].GetStr(),
			FuncAddr,
			Frame);
	#endif

	#ifdef VM_EXECUTE

	// Set up stack for function call
	StackFrame &Sf = Frames.New();
	Sf.CurrentFrameSize = Frame;
	Sf.PrevFrameStart = Locals.Length() ? Scope[1] - &Locals[0] : 0;
	Sf.ReturnValue = Resolve();
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
	// Now adjust the local stack to point to the locals for the function
	Scope[1] = Locals.Length() ? &Locals[LocalsBase] : NULL;

	// Set IP to start of function
	Sf.ReturnIp = CurrentScriptAddress;
	c.u8 = Base + FuncAddr;
	
	if (StepType == RunStepInto)
		Loop = false;
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
		Log->Print("\t%p Ret %s\n", CurrentScriptAddress - 1, c.r[0].GetStr());
	#endif

	GResolveRef ReturnValue = Resolve();

	#ifdef VM_EXECUTE
	if (Frames.Length() > 0)
	{
		StackFrame Sf = Frames[Frames.Length()-1];
		if (Sf.ReturnValue)
		{
			*Sf.ReturnValue = *ReturnValue;
			LgiAssert(Sf.ReturnValue->Type == ReturnValue->Type);
		}

		Frames.Length(Frames.Length()-1);
		
		Locals.SetFixedLength(false);
		if (Locals.Length() >= Sf.CurrentFrameSize)
		{
			Locals.Length(Locals.Length() - Sf.CurrentFrameSize);
			Scope[1] = &Locals[Sf.PrevFrameStart];
		}
		else
		{
			Locals.Length(0);
			Scope[1] = 0;
		}
		Locals.SetFixedLength();

		c.u8 = Base + Sf.ReturnIp;
	}
	else
	{
		ExitScriptExecution;
	}
	
	if (StepType == RunStepOut)
		Loop = false;
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
					else LgiAssert(0);
				}
				else *Dst = *t;
			}
			else Dst->Empty();
			break;
		}
		case GV_HASHTABLE:
		{
			GVariant *t = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			if (t) *Dst = *t;
			else Dst->Empty();
			break;
		}
		case GV_STRING:
		{
			LgiAssert(!"Implement me");
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
			(*Var->Value.Lst).Insert(new GVariant(*Val), Idx->CastInt32());
			break;
		}
		case GV_HASHTABLE:
		{
			GVariant *Old = (GVariant*)Var->Value.Hash->Find(Idx->CastString());
			DeleteObj(Old);
			Var->Value.Hash->Add(Idx->CastString(), new GVariant(*Val));
			break;
		}
		case GV_STRING:
		{
			LgiAssert(!"Implement me");
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
	if (Src->Type == GV_DATETIME && Dst->Type == GV_DATETIME)
		*Dst = *Dst->Value.Date == *Src->Value.Date;
	else if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() == Src->CastDouble();
	else if (Src->Type == GV_STRING || Dst->Type == GV_STRING)
	{
		char *d = Dst->Str();
		char *s = Src->Str();
		if (!d && !s)
			*Dst = true;
		else if (s && d)
			*Dst = strcmp(s, d) == 0;
		else
			*Dst = false;
	}
	else
		*Dst = Dst->CastInt32() == Src->CastInt32();
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
	if (Src->Type == GV_NULL || Dst->Type == GV_NULL)
	{
		if ((Src->Type == GV_NULL) ^ (Dst->Type == GV_NULL))
			*Dst = (Src->Type == GV_NULL ? Dst : Src)->CastVoidPtr() != 0;
		else
			*Dst = false;
	}
	else if (Src->Type == GV_DATETIME && Dst->Type == GV_DATETIME)
		*Dst = *Dst->Value.Date != *Src->Value.Date;
	else if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() != Src->CastDouble();
	else if (Src->Type == GV_STRING)
	{
		char *d = Dst->Str();
		char *s = Src->Str();
		if (!d || !s)
			*Dst = (s == 0) ^ (d == 0);
		else
			*Dst = strcmp(s, d) != 0;
	}
	else
		*Dst = Dst->CastInt32() != Src->CastInt32();
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
	if (Src->Type == GV_DATETIME && Dst->Type == GV_DATETIME)
		*Dst = *Dst->Value.Date < *Src->Value.Date;
	else if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() < Src->CastDouble();
	else if (Src->Type == GV_STRING)
	{
		char *d = Dst->Str();
		char *s = Src->Str();
		if (!d || !s)
			*Dst = false;
		else
			*Dst = strcmp(d, s) < 0;
	}
	else
		*Dst = Dst->CastInt32() < Src->CastInt32();
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
	if (Src->Type == GV_DATETIME && Dst->Type == GV_DATETIME)
		*Dst = *Dst->Value.Date <= *Src->Value.Date;
	else if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() <= Src->CastDouble();
	else if (Src->Type == GV_STRING)
	{
		char *d = Dst->Str();
		char *s = Src->Str();
		if (!d || !s)
			*Dst = false;
		else
			*Dst = strcmp(d, s) <= 0;
	}
	else
		*Dst = Dst->CastInt32() <= Src->CastInt32();
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
	if (Src->Type == GV_DATETIME && Dst->Type == GV_DATETIME)
		*Dst = *Dst->Value.Date > *Src->Value.Date;
	else if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() > Src->CastDouble();
	else if (Dst->Type == GV_STRING && Src->Type == GV_STRING)
	{
		char *d = Dst->Str();
		char *s = Src->Str();
		if (!d || !s)
			*Dst = false;
		else
			*Dst = strcmp(d, s) > 0;
	}
	else
		*Dst = Dst->CastInt32() > Src->CastInt32();
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
	if (Src->Type == GV_DATETIME && Dst->Type == GV_DATETIME)
		*Dst = *Dst->Value.Date >= *Src->Value.Date;
	else if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
		*Dst = Dst->CastDouble() >= Src->CastDouble();
	else if (Src->Type == GV_STRING)
	{
		char *d = Dst->Str();
		char *s = Src->Str();
		if (!d || !s)
			*Dst = false;
		else
			*Dst = strcmp(d, s) >= 0;
	}
	else
		*Dst = Dst->CastInt32() >= Src->CastInt32();
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

		// Check that we have a valid name part
		char *sName = Name->Str();
		if (!sName)
			break;

		switch (Dom->Type)
		{
			case GV_DOM:
			case GV_GFILE:
			case GV_GSURFACE:
			{
				GDom *dom = Dom->CastDom();
				LgiAssert(dom != NULL);
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
				LgiAssert(Dom->Value.Date != NULL);
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
				GTypeDef *Type = dynamic_cast<GTypeDef*>(Dom->Value.Custom.Dom);
				if (Type)
				{
					Type->Object = Dom->Value.Custom.Data;
					Type->GetVariant(sName, *Dst, Arr->Str());
				}
				break;
			}
			case GV_LIST:
			{
				GDomProperty p = GStringToProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Lst->Length();
				break;
			}
			case GV_HASHTABLE:
			{
				GDomProperty p = GStringToProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Hash->Length();
				break;
			}
			case GV_STRING:
			{
				GDomProperty p = GStringToProp(sName);
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
			default:
			{
				if (Log)
					Log->Print("%s IDomGet warning: Unexpected type %s.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								GVariant::TypeToString(Dom->Type));
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
		case GV_GFILE:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			LgiAssert(dom != NULL);
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
			LgiAssert(Dom->Value.Date != NULL);
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
			GTypeDef *Type = dynamic_cast<GTypeDef*>(Dom->Value.Custom.Dom);
			if (Type)
			{
				Type->Object = Dom->Value.Custom.Data;
				Type->SetVariant(sName, *Value, Arr->Str());
			}
			break;
		}
		case GV_STRING:
		{
			GDomProperty p = GStringToProp(sName);
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
		Log->Print("%p %s = %s->DomCall(%s, %s, ....)\n",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr(),
					c.r[3].GetStr());
	#endif

	GResolveRef Dst = Resolve();
	GResolveRef Dom = Resolve();
	GResolveRef Name = Resolve();
	GResolveRef Args = Resolve();

	#ifdef VM_EXECUTE

	GArray<GVariant*> Arg;
	Arg.Length(Args->CastInt32());
	for (unsigned i=0; i<Arg.Length(); i++)
	{
		Arg[i] = Resolve();
	}
	
	char *sName = Name->Str();
	if (!sName)
	{
		LgiAssert(!"No value");
		break;
	}

	switch (Dom->Type)
	{
		case GV_DOM:
		case GV_GFILE:
		case GV_GSURFACE:
		{
			GDom *dom = Dom->CastDom();
			LgiAssert(dom != NULL);
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
			LgiAssert(Dom->Value.Date != NULL);
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
			LgiAssert(Dom->Value.Lst != NULL);
			
			GDomProperty p = GStringToProp(sName);
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
			LgiAssert(Dom->Value.Hash != NULL);
			
			GDomProperty p = GStringToProp(sName);
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
		case GV_STRING:
		{
			if (Arg.Length() > 0 && !Arg[0])
			{
				Dst->Empty();
				break;
			}

			GDomProperty p = GStringToProp(sName);
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
					if (end > start)
						pos = strnstr(sStart, sub, end - start);
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
					LgiAssert(0);
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
					Dst->Empty();
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
						{
							Dst->OwnStr(NewStr(s + Start, End - Start));
						}
					}
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
			
			GDomProperty p = GStringToProp(sName);
			if (p == ObjType)
			{
				*Dst = Type;
			}
			else
			{
				Dst->Empty();
				if (Log)
				{
					Log->Print("%s IDomCall warning: Unexpected type %s.\n",
								Code->AddrToSourceRef(CurrentScriptAddress),
								Type);
				}
				Status = ScriptWarning;
			}
			break;
		}
	}
	
	#else

	for (int i=0; i<Args.CastInt32(); i++)
	{
		GResolveRef Arg = Resolve();
		int asd=0;
	}


	#endif
	break;
}
/*
case ICallSystem:
{
	break;
}
*/

#undef Resolve
#undef GResolveRef
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
		else
			LgiTrace("%s:%i - Execution without a log?\n", _FL);
			
		LgiAssert(sizeof(GVarRef) == 4);

		uint8 *Base = c.u8 = &Code->ByteCode[0];
		uint8 *e = c.u8 + Code->ByteCode.Length();

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
		    GAutoString DataPath = Code->UserContext->GetDataFolder();
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
			if (OutOfDate)
			{			
				GFile f;
				if (f.Open(Obj, O_WRITE))
				{
					f.SetSize(0);
					GExecutionStatus Decomp = Decompile(Code->UserContext, Code, &f);
					f.Close();
					if (Decomp != ScriptSuccess)
					{
						LgiAssert(!"Decompilation failed.");
						return ScriptError;
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

		Scope[0] = Reg;
		Scope[1] = NULL;
		Scope[2] = &Code->Globals[0];

		// Calling a function only, not the whole script
		if (Func)
		{
			uint16 Frame = Func->FrameSize;

			// Set up stack for function call
			StackFrame &Sf = Frames.New();
			Sf.CurrentFrameSize = Frame;
			Sf.PrevFrameStart = 0;
			Sf.ReturnIp = e - c.u8;
			Sf.ReturnValue = Ret;

			int LocalsBase = Locals.Length();
			Locals.SetFixedLength(false);
			Locals.Length(LocalsBase + Frame);
			Scope[1] = &Locals[LocalsBase];
			Locals.SetFixedLength();

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
		else if (StartOffset > 0)
		{
			c.u8 = Base + StartOffset;
		}

		return Status;
	}
	
	GExecutionStatus Run(RunType Type)
	{
		LgiAssert(Code != NULL);
		
		uint8 *Base = &Code->ByteCode[0];
		uint8 *e = Base + Code->ByteCode.Length();
		Loop = true;
		StepType = Type;
		
		while (Loop && c.u8 < e)
		{
			#if TIME_INSTRUCTIONS
			uint8 TimedOpCode = *c.u8;
			QueryPerformanceCounter(&start);
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
		
		return Status;
	}
};

GVirtualMachine::GVirtualMachine(GVmDebuggerCallback *callback)
{
	d = new GVirtualMachinePriv(callback);
}

GVirtualMachine::~GVirtualMachine()
{
	DeleteObj(d);
}

GExecutionStatus GVirtualMachine::Execute(GCompiledCode *Code, uint32 StartOffset, GStream *Log)
{
	if (!Code)
		return ScriptError;

	GExecutionStatus s = d->Setup(Code, StartOffset, Log, NULL, NULL, NULL);
	if (s != ScriptSuccess)
		return s;

	return d->Run(GVirtualMachinePriv::RunContinue);
}

GExecutionStatus GVirtualMachine::ExecuteFunction(GScriptObj *Code, GFunctionInfo *Func, ArgumentArray &Args, GVariant *Ret, GStream *Log)
{
	GCompiledCode *Cc = dynamic_cast<GCompiledCode*>(Code);
	if (!Cc)
		return ScriptError;

	GExecutionStatus s = d->Setup(Cc, 0, Log, Func, &Args, Ret);
	if (s != ScriptSuccess)
		return s;

	return d->Run(GVirtualMachinePriv::RunContinue);
}

GVmDebugger *GVirtualMachine::OpenDebugger()
{
	if (!d->Debugger)
	{
		if (!d->DbgCallback)
			return NULL;
		
		d->Debugger = d->DbgCallback->AttachVm(this, NULL, NULL);
	}
	
	return d->Debugger;
}

bool GVirtualMachine::StepInto()
{
	return false;
}

bool GVirtualMachine::StepOver()
{
	return false;
}

bool GVirtualMachine::StepOut()
{
	return false;
}

bool GVirtualMachine::BreakExecution()
{
	return false;
}

bool GVirtualMachine::Run(bool Start)
{
	return false;
}

bool GVirtualMachine::BreakPoint(const char *File, int Line, bool Add)
{
	return false;
}

////////////////////////////////////////////////////////////////////
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


struct GScriptVmDebuggerPriv
{
	// Current script
	bool OwnVm;
	GVirtualMachine *Vm;
	GAutoString Script, Assembly;

	// Ui
	GView *Parent;
	GBox *Main;
	GTabView *Tabs;
	GTextLog *Text;
	GList *Locals, *Globals, *Stack;
	GTextLog *Log;
	GToolBar *Tools;

	GScriptVmDebuggerPriv()
	{
		OwnVm = false;
		Main = NULL;
		Tabs = NULL;
		Log = NULL;
		Text = NULL;
		Locals = NULL;
		Globals = NULL;
		Stack = NULL;
		Tools = NULL;
	}
};

enum DbgCtrls
{
	IDC_TABS = 300,
	IDC_BOX,
	IDC_TEXT,
	IDC_LOCALS,
	IDC_GLOBALS,
	IDC_STACK,
	IDC_LOG,
	IDC_RUN,
	IDC_PAUSE,
	IDC_STOP,
	IDC_RESTART,
	IDC_GOTO,
	IDC_STEP_INTO,
	IDC_STEP_OVER,
	IDC_STEP_OUT,
};

GVmDebuggerWnd::GVmDebuggerWnd(GView *Parent, GVirtualMachine *Vm, const char *Script, const char *Assembly)
{
	d = new GScriptVmDebuggerPriv;
	d->Parent = Parent;
	d->Vm = Vm;
	d->Script.Reset(NewStr(Script));
	d->Assembly.Reset(NewStr(Assembly));
	
	GRect r(0, 0, 1000, 900);
	SetPos(r);
	if (Parent)
		MoveSameScreen(Parent);
	else
		MoveToCenter();
	
	if (Attach(NULL))
	{
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
		d->Tools->AppendButton("Step Into", IDC_STEP_INTO);
		d->Tools->AppendButton("Step Over", IDC_STEP_OVER);
		d->Tools->AppendButton("Step Out", IDC_STEP_OUT);
		
		AddView(d->Main = new GBox(IDC_BOX));
		d->Main->SetVertical(true);
		d->Main->AddView(d->Text = new GTextLog(IDC_TEXT));
		d->Main->AddView(d->Tabs = new GTabView(IDC_TABS));
		d->Tabs->GetCss(true)->Height(GCss::Len("150px"));
		
		GTabPage *p = d->Tabs->Append("Locals");
		p->Append(d->Locals = new GList(IDC_LOCALS, 0, 0, 100, 100));
		d->Locals->SetPourLargest(true);
		
		p = d->Tabs->Append("Globals");
		p->Append(d->Globals = new GList(IDC_GLOBALS, 0, 0, 100, 100));
		d->Globals->SetPourLargest(true);
		
		p = d->Tabs->Append("Stack");
		p->Append(d->Stack = new GList(IDC_STACK, 0, 0, 100, 100));
		d->Stack->SetPourLargest(true);
		
		p = d->Tabs->Append("Log");
		p->Append(d->Log = new GTextLog(IDC_LOG));
		
		AttachChildren();
		Visible(true);
	}
}

GVmDebuggerWnd::~GVmDebuggerWnd()
{
	delete d;
}

void GVmDebuggerWnd::OwnVm(bool Own)
{
	d->OwnVm = Own;
}

void GVmDebuggerWnd::OnPosition(const char *File, int Line)
{
}

void GVmDebuggerWnd::OnError(const char *Msg)
{
}

void GVmDebuggerWnd::OnRun(bool Running)
{
}

int GVmDebuggerWnd::OnNotify(GViewI *Ctrl, int Flags)
{
	return GWindow::OnNotify(Ctrl, Flags);
}

GMessage::Param GVmDebuggerWnd::OnEvent(GMessage *Msg)
{
	return GWindow::OnEvent(Msg);
}
