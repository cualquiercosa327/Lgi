#include <math.h>
#include "Lgi.h"
#include "GScripting.h"
#include "GScriptingPriv.h"

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
	GStream *Log;

	GVirtualMachinePriv()
	{
		Log = NULL;
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
				#include "Instructions.h"
				#undef VM_DECOMP
			}
		}

		if (log)
			Log = OldLog;

		return Status;
	}

	GExecutionStatus Execute(GCompiledCode *Code, uint32 StartOffset, GStream *log, GFunctionInfo *Func, ArgumentArray *Args, GVariant *Ret)
	{
		GExecutionStatus Status = ScriptSuccess;
		
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

		GPtr c;
		uint8 *Base = &Code->ByteCode[0];
		c.u8 = Base;
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

		GVariant Reg[MAX_REGISTER];
		GArray<GVariant> Locals;
		GVariant *Scope[3] = { Reg, 0, &Code->Globals[0] };
		GArray<StackFrame> Frames;

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

		while (c.u8 < e)
		{
			#if TIME_INSTRUCTIONS
			uint8 TimedOpCode = *c.u8;
			QueryPerformanceCounter(&start);
			#endif
			
			#if defined(WIN32) && defined(BREAK_POINT)
			if (CurrentScriptAddress == BREAK_POINT)
			{
				_asm int 3
			}
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

GVirtualMachine::GVirtualMachine()
{
	d = new GVirtualMachinePriv;
}

GVirtualMachine::~GVirtualMachine()
{
	DeleteObj(d);
}

GExecutionStatus GVirtualMachine::Execute(GCompiledCode *Code, uint32 StartOffset, GStream *Log)
{
	if (!Code)
		return ScriptError;

	return d->Execute(Code, StartOffset, Log, NULL, NULL, NULL);
}

GExecutionStatus GVirtualMachine::ExecuteFunction(GScriptObj *Code, GFunctionInfo *Func, ArgumentArray &Args, GVariant *Ret, GStream *Log)
{
	GCompiledCode *Cc = dynamic_cast<GCompiledCode*>(Code);
	if (!Cc)
		return ScriptError;

	return d->Execute(Cc, 0, Log, Func, &Args, Ret);
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
