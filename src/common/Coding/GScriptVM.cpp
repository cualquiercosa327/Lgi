#include <math.h>
#include "Lgi.h"
#include "GScripting.h"
#include "GScriptingPriv.h"

#define LOG_ASM					0
#define TIME_INSTRUCTIONS		0
#define POST_EXECUTE_STATE		0

// #define BREAK_POINT				0x00000060
#define Resolve() \
	&Scope[c.r->Scope][c.r->Index]; c.r++

#define ExitScriptExecution		c.u8 = e
#define SetScriptError			c.u8 = e; LgiAssert(0); Status = ScriptError
#define CurrentScriptAddress	(c.u8 - Base)



/// During execution the code needs to look up various object members via
/// string. These enumeration values allow the executer to look up a string
/// member reference via a hash table in O(1) time and then do another O(1)
/// jump via a switch to the code that handles the member.

class GVirtualMachinePriv
{
	struct StackFrame
	{
		uint32 CurrentFrameSize;
		int PrevFrameStart;
		int ReturnIp;
		GVariant *ReturnValue;
	};

public:
	GStream *Log;
	GScriptContext *Context;

	GVirtualMachinePriv(GScriptContext *context)
	{
		Log = 0;
		Context = context;
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

		GStream &f = *log;
		for (unsigned k=0; k<Code->Globals.Length(); k++)
		{
			f.Print("G%i = ", k);
			DumpVariant(&f, Code->Globals[k]);
			f.Print("\n");
		}
		f.Print("\n");

		GHashTbl<int, char*> Fn;
		for (unsigned m=0; m<Code->Methods.Length(); m++)
		{
			GFunctionInfo *Info = Code->Methods[m];
			if (Info->StartAddr)
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
				f.Print("%s:\n", Meth);
			}

			int LineNum = Code->Debug.Find(c.u8 - Base);
			if (LineNum >= 0 && LineNum != OldLineNum)
			{
				f.Print("  %i:\n", OldLineNum = LineNum);
			}

			switch (*c.u8++)
			{
				default:
				{
					f.Print("\t%p Unknown instruction %i\n",
							c.u8 - Base - 1,
							c.u8[-1]);
					ExitScriptExecution;
					break;
				}
				case INop:
				{
					f.Print("\t%p Nop\n", c.u8 - Base - 1);
					break;
				}
				case IRet:
				{
					f.Print("\t%p Ret %s\n",
						c.u8 - Base - 1,
						c.r[0].GetStr());
					c.r++;
					break;
				}
				case IAssign:
				{
					f.Print("\t%p Assign %s <- %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IJump:
				{
					int32 Offset = *c.i32++;

					f.Print("\t%p Jump by %s0x%x to %p\n",
							c.u8 - Base - 5,
							Offset >= 0 ? "" : "-",
							Offset >= 0 ? Offset : -Offset,
							(c.u8 - Base) + Offset);
					break;
				}
				case IJumpZero:
				{
					f.Print("\t%p JumpZ(%s) by 0x%x to %p\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.i32[1],
							((uchar*)(c.i32 + 2) - Base) + c.i32[1]);
					c.r++;
					c.i32++;
					break;
				}
				// case IUnaryPlus:
				case IUnaryMinus:
				{
					f.Print("\t%p UnaryMinus %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr());
					c.r++;
					break;
				}
				case IPlus:
				case IPlusEquals:
				{
					f.Print("\t%p Plus %s += %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IMinus:
				case IMinusEquals:
				{
					f.Print("\t%p Minus %s -= %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IMul:
				case IMulEquals:
				{
					f.Print("\t%p Mul %s *= %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IDiv:
				case IDivEquals:
				{
					f.Print("\t%p Div %s /= %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IMod:
				{
					f.Print("\t%p Mod %s %= %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IPostInc:
				case IPreInc:
				{
					f.Print("\t%p PostInc %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr());
					c.r += 1;
					break;
				}
				case IPostDec:
				case IPreDec:
				{
					f.Print("\t%p PostDec %sn",
							c.u8 - Base - 1,
							c.r[0].GetStr());
					c.r += 1;
					break;
				}
				case ICallMethod:
				{
					GFunc *Meth = *c.fn++;
					if (!Meth)
					{
						Log->Print("%p ICallMethod error: No method struct.\n", CurrentScriptAddress - 1);
						SetScriptError;
						break;
					}

					f.Print("\t%p Call: %s = %s\n",
							c.u8 - Base - 5,
							c.r[0].GetStr(),
							Meth->Method);

					c.r++;
					uint16 Args = *c.u16++;
					c.r += Args;
					break;
				}
				case ICallScript:
				{
					uchar *Instr = c.u8;
					uint32 FuncAddr = *c.u32++;
					if (!FuncAddr)
					{
						Log->Print("%p ICallScript error: Script function call invalid addr (0).\n", CurrentScriptAddress - 4);
						SetScriptError;
						break;
					}

					uint16 Frame = *c.u16++;

					f.Print("\t%p %s = ScriptCall(@0x%x)(",
							Instr - Base - 1,
							c.r[0].GetStr(),
							FuncAddr);

					c.r++;
					uint16 Args = *c.u16++;
					for (int i=0; i<Args; i++)
					{
						if (i) f.Print(",");
						f.Print("%s", c.r[0].GetStr());
						c.r++;
					}
					f.Print(") [frame=%i]\n", Frame);
					break;
				}
				case IArrayGet:
				{
					f.Print("\t%p ArrayGet %s = %s[%s]\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr());
					c.r += 3;
					break;
				}
				case IArraySet:
				{
					f.Print("\t%p ArraySet %s[%s] = %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr());
					c.r += 3;
					break;
				}
				case IEquals:
				{
					f.Print("\t%p %s == %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case INotEquals:
				{
					f.Print("\t%p %s != %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case ILessThan:
				{
					f.Print("\t%p %s < %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case ILessThanEqual:
				{
					f.Print("\t%p %s <= %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IGreaterThan:
				{
					f.Print("\t%p %s > %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IGreaterThanEqual:
				{
					f.Print("\t%p %s >= %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IAnd:
				{
					f.Print("\t%p %s && %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case IOr:
				{
					f.Print("\t%p %s || %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					c.r += 2;
					break;
				}
				case INot:
				{
					f.Print("\t%p %s = !%s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[0].GetStr());
					c.r += 1;
					break;
				}
				case IDomGet:
				{
					f.Print("\t%p %s = %s->DomGet(%s, %s)\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr(),
							c.r[3].GetStr());
					c.r += 4;
					break;
				}
				case IDomSet:
				{
					f.Print("\t%p %s->DomSet(%s, %s) = %s\n",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr(),
							c.r[3].GetStr());
					c.r += 4;
					break;
				}
				case IDomCall:
				{
					f.Print("\t%p %s = %s->DomCall(%s, [",
							c.u8 - Base - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr());

					c.r += 3;
					GVariant *Count = NULL;
					switch (c.r->Scope)
					{
						case SCOPE_GLOBAL:
							Count = &Code->Globals[c.r->Index];
							c.r++;
							break;
						default:
							LgiAssert(0);
							return ScriptError;
					}
					
					int Args = Count->CastInt32();
					for (int i=0; i<Args; i++)
					{
						f.Print("%s%s", i ? ", " : "", c.r->GetStr());
						c.r++;
					}
					f.Print("])\n");
					break;
				}
				/*
				case ICallSystem:
				{
					break;
				}
				*/
			}
		}

		return Status;
	}

	GExecutionStatus Execute(GCompiledCode *Code, GStream *log, GFunctionInfo *Func, ArgumentArray *Args, GVariant *Ret)
	{
		GExecutionStatus Status = ScriptSuccess;
		
		Log = log;
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
		        strcat(Obj, ".asm");
		}
		else
		{
		    GAutoString DataPath = Context->GetDataFolder();
		    if (!DataPath)
		    {
			    char p[256];
			    if (LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p)))
				    DataPath.Reset(NewStr(p));
		    }
    		
		    LgiMakePath(Obj, sizeof(Obj), DataPath, "Script.asm");
		}
		
		{
			GFile f;
			if (f.Open(Obj, O_WRITE))
			{
				f.SetSize(0);
				GExecutionStatus Decomp = Decompile(Context, Code, &f);
				f.Close();
				if (Decomp != ScriptSuccess)
				{
					LgiAssert(!"Decompilation failed.");
					return ScriptError;
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
				default:
				{
					#if LOG_ASM
					f.Print("%p Unknown instruction %i\n",
							CurrentScriptAddress - 1,
							c.u8[-1]);
					#endif
					LgiAssert(!"Unknown instruction");
					SetScriptError;
					break;
				}
				case INop:
				{
					#if LOG_ASM
					f.Print("%p Nop\n", c.u8[-1]);
					#endif
					c.u8++;
					break;
				}
				case IAssign:
				{
					#if LOG_ASM
					f.Print("%p Assign %s <- %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					LgiAssert(Dst != Src);
					*Dst = *Src;
					break;
				}
				case IJump:
				{
					#if LOG_ASM
					f.Print("%p Jump by 0x%x\n",
							CurrentScriptAddress - 1,
							c.i32[0]);
					#endif

					int32 Jmp = *c.i32++;
					LgiAssert(Jmp != 0);
					c.u8 += Jmp;
					break;
				}
				case IJumpZero:
				{
					#if LOG_ASM
					f.Print("%p JumpZ(%s) by 0x%x\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.i32[1]);
					#endif

					GVariant *Exp = Resolve();
					int32 Jmp = *c.i32++;
					LgiAssert(Jmp != 0);
					
					if (!Exp->CastInt32())
					{
						c.u8 += Jmp;
					}
					break;
				}
				// case IUnaryPlus:
				case IUnaryMinus:
				{
					#if LOG_ASM
					f.Print("%p UnaryMinus %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr());
					#endif

					GVariant *Var = Resolve();
					if (Var->Type == GV_DOUBLE)
						*Var = -Var->CastDouble();
					else
						*Var = -Var->CastInt32();
					break;
				}
				case IPlus:
				case IPlusEquals:
				{
					#if LOG_ASM
					f.Print("%p Plus %s += %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
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
					break;
				}
				case IMinus:
				case IMinusEquals:
				{
					#if LOG_ASM
					f.Print("%p Minus %s -= %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					if (Dst->Type == GV_DOUBLE ||
						Src->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() - Src->CastDouble();
					else
						*Dst = Dst->CastInt32() - Src->CastInt32();
					break;
				}
				case IMul:
				case IMulEquals:
				{
					#if LOG_ASM
					f.Print("%p Mul %s *= %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					if (Dst->Type == GV_DOUBLE ||
						Src->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() * Src->CastDouble();
					else
						*Dst = Dst->CastInt32() * Src->CastInt32();
					break;
				}
				case IDiv:
				case IDivEquals:
				{
					#if LOG_ASM
					f.Print("%p Div %s /= %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					if (Dst->Type == GV_DOUBLE ||
						Src->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() / Src->CastDouble();
					else
						*Dst = Dst->CastInt32() / Src->CastInt32();
					break;
				}
				case IMod:
				{
					#if LOG_ASM
					f.Print("%p Mod %s %= %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					if (Dst->Type == GV_DOUBLE ||
						Src->Type == GV_DOUBLE)
					{
						*Dst = fmod(Dst->CastDouble(), Src->CastDouble());
					}
					else
					{
						*Dst = Dst->CastInt32() % Src->CastInt32();
					}
					break;
				}
				case IPostInc:
				case IPreInc:
				{
					#if LOG_ASM
					f.Print("%p PostInc %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr());
					#endif

					GVariant *v = Resolve();
					if (v->Type == GV_DOUBLE)
						*v = v->CastDouble() + 1;
					else
						*v = v->CastInt32() + 1;
					break;
				}
				case IPostDec:
				case IPreDec:
				{
					#if LOG_ASM
					f.Print("%p PostDec %sn",
							CurrentScriptAddress - 1,
							c.r[0].GetStr());
					#endif

					GVariant *v = Resolve();
					if (v->Type == GV_DOUBLE)
						*v = v->CastDouble() - 1;
					else
						*v = v->CastInt32() - 1;
					break;
				}
				case ICallMethod:
				{
					GHostFunc *Meth = dynamic_cast<GHostFunc*>(*c.fn++);
					if (!Meth)
					{
						Log->Print("%p ICallMethod error: No method struct.\n", CurrentScriptAddress - 1);
						SetScriptError;
						break;
					}

					#if LOG_ASM
					f.Print("%p Call: %s = %s(",
							CurrentScriptAddress - 5,
							c.r[0].GetStr(),
							Meth->Method);
					#endif

					GVariant *Ret = Resolve();
					uint16 Args = *c.u16++;
					GArray<GVariant*> Arg;
					for (int i=0; i<Args; i++)
					{
						#if LOG_ASM
						f.Print("%s%s", i?",":"", c.r[0].GetStr());
						#endif

						Arg[i] = Resolve();
						LgiAssert(Arg[i] != NULL);
					}

					#if LOG_ASM
					f.Print(")\n");
					#endif

					if (!(Meth->Context->*(Meth->Func))(Ret, Arg))
					{
						if (Log)
							Log->Print("%p ICallMethod error: Method '%s' failed.\n", CurrentScriptAddress, Meth->Method);
						SetScriptError;
					}
					break;
				}
				case ICallScript:
				{
					uint32 FuncAddr = *c.u32++;
					if (!FuncAddr)
					{
						Log->Print("%p ICallScript error: Script function call invalid addr '0'.\n", CurrentScriptAddress);
						SetScriptError;
						break;
					}

					uint16 Frame = *c.u16++;

					#if LOG_ASM
					f.Print("%p ScriptCall: %s = %p(frame=%i)(",
							CurrentScriptAddress - 5,
							c.r[0].GetStr(),
							FuncAddr,
							Frame);
					#endif

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
					for (int i=0; i<Args; i++)
					{
						#if LOG_ASM
						f.Print("%s%s", i?",":"", c.r[0].GetStr());
						#endif

						Locals[LocalsBase+i] = *Resolve();
					}

					// Now adjust the local stack to point to the locals for the function
					Scope[1] = Locals.Length() ? &Locals[LocalsBase] : NULL;

					#if LOG_ASM
					f.Print(")\n");
					#endif

					// Set IP to start of function
					Sf.ReturnIp = CurrentScriptAddress;
					c.u8 = Base + FuncAddr;
					break;
				}
				case IRet:
				{
					#if LOG_ASM
					f.Print("%p Ret %s\n",
						CurrentScriptAddress - 1,
						c.r[0].GetStr());
					#endif

					GVariant *ReturnValue = Resolve();

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
					break;
				}
				case IArrayGet:
				{
					#if LOG_ASM
					f.Print("%p ArrayGet %s = %s[%s]\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Var = Resolve();
					GVariant *Idx = Resolve();
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
								Log->Print("%p IArrayGet warning: Can't array deref variant type %i\n", CurrentScriptAddress, Var->Type);
							Status = ScriptWarning;
							break;
						}
					}
					break;
				}
				case IArraySet:
				{
					#if LOG_ASM
					f.Print("%p ArraySet %s[%s] = %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr());
					#endif

					GVariant *Var = Resolve();
					GVariant *Idx = Resolve();
					GVariant *Val = Resolve();
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
								Log->Print("%p IArraySet warning: Can't refer variant type '%s'\n", GVariant::TypeToString(Var->Type));
							Status = ScriptWarning;
							break;
						}
					}
					break;
				}
				case IEquals:
				{
					#if LOG_ASM
					f.Print("%p %s == %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() == Src->CastDouble();
					else if (Src->Type == GV_DATETIME && Dst->Type == GV_DATETIME)
						*Dst = *Dst->Value.Date == *Src->Value.Date;
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
					break;
				}
				case INotEquals:
				{
					#if LOG_ASM
					f.Print("%p %s != %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					if (Src->Type == GV_NULL || Dst->Type == GV_NULL)
					{
						if ((Src->Type == GV_NULL) ^ (Dst->Type == GV_NULL))
							*Dst = (Src->Type == GV_NULL ? Dst : Src)->CastVoidPtr() != 0;
						else
							*Dst = false;
					}
					else if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() != Src->CastDouble();
					else if (Src->Type == GV_DATETIME || Dst->Type == GV_DATETIME)
						*Dst = *Dst->Value.Date != *Src->Value.Date;
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
					break;
				}
				case ILessThan:
				{
					#if LOG_ASM
					f.Print("%p %s < %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					
					if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() < Src->CastDouble();
					else if (Src->Type == GV_DATETIME || Dst->Type == GV_DATETIME)
						*Dst = *Dst->Value.Date < *Src->Value.Date;
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
					break;
				}
				case ILessThanEqual:
				{
					#if LOG_ASM
					f.Print("%p %s < %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					
					if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() <= Src->CastDouble();
					else if (Src->Type == GV_DATETIME || Dst->Type == GV_DATETIME)
						*Dst = *Dst->Value.Date <= *Src->Value.Date;
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
					break;
				}
				case IGreaterThan:
				{
					#if LOG_ASM
					f.Print("%p %s < %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					
					if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() > Src->CastDouble();
					else if (Src->Type == GV_DATETIME || Dst->Type == GV_DATETIME)
						*Dst = *Dst->Value.Date > *Src->Value.Date;
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
					break;
				}
				case IGreaterThanEqual:
				{
					#if LOG_ASM
					f.Print("%p %s < %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					
					if (Src->Type == GV_DOUBLE || Dst->Type == GV_DOUBLE)
						*Dst = Dst->CastDouble() >= Src->CastDouble();
					else if (Src->Type == GV_DATETIME || Dst->Type == GV_DATETIME)
						*Dst = *Dst->Value.Date >= *Src->Value.Date;
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
					break;
				}
				case IAnd:
				{
					#if LOG_ASM
					f.Print("%p %s && %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					*Dst = Dst->CastInt32() && Src->CastInt32();
					break;
				}
				case IOr:
				{
					#if LOG_ASM
					f.Print("%p %s || %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Src = Resolve();
					*Dst = Dst->CastInt32() || Src->CastInt32();
					break;
				}
				case INot:
				{
					#if LOG_ASM
					f.Print("%p %s = !%s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[0].GetStr());
					#endif

					GVariant *Dst = Resolve();
					*Dst = !Dst->CastBool();
					break;
				}
				case IDomGet:
				{
					#if LOG_ASM
					f.Print("%p %s = %s->DomGet(%s, %s)\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr(),
							c.r[3].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Dom = Resolve();
					GVariant *Name = Resolve();
					GVariant *Arr = Resolve();

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
						{
							if (Arr->Type == GV_STRING)
								Dom->Value.Dom->GetVariant(sName, *Dst, Arr->Str());
							else if (Arr->Type != GV_NULL)
							{
								GVariant ArrayTemp = *Arr;
								Dom->Value.Dom->GetVariant(sName, *Dst, ArrayTemp.CastString());
							}
							else
								Dom->Value.Dom->GetVariant(sName, *Dst, 0);
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
										Log->Print("%p IDomGet warning: Unexpected string member %s (%s:%i).\n",
													CurrentScriptAddress,
													sName,
													_FL);
									Status = ScriptWarning;
									break;
								}
							}
							break;
						}
						case GV_DATETIME:
						{
							GDomProperty p = GStringToProp(sName);
							switch (p)
							{
								case DateYear:
									(*Dst) = Dom->Value.Date->Year();
									break;
								case DateMonth:
									(*Dst) = Dom->Value.Date->Month();
									break;
								case DateDay:
									(*Dst) = Dom->Value.Date->Day();
									break;
								case DateHour:
									(*Dst) = Dom->Value.Date->Hours();
									break;
								case DateMin:
									(*Dst) = Dom->Value.Date->Minutes();
									break;
								case DateSec:
									(*Dst) = Dom->Value.Date->Seconds();
									break;
								case DateDate:
								{
									char s[32];
									Dom->Value.Date->GetDate(s, sizeof(s));
									(*Dst) = s;
									break;
								}
								case DateTime:
								{
									char s[32];
									Dom->Value.Date->GetTime(s, sizeof(s));
									(*Dst) = s;
									break;
								}
								case DateDateTime:
								{
									char s[32];
									Dom->Value.Date->Get(s, sizeof(s));
									(*Dst) = s;
									break;
								}
								case DateInt64:
								{
									uint64 i = 0;
									Dom->Value.Date->Get(i);
									*Dst = (int64)i;
									break;
								}
								default:
								{
									Dst->Empty();
									if (Log)
										Log->Print("%p IDomGet warning: Unexpected datetime member %s (%s:%i).\n",
													CurrentScriptAddress,
													sName,
													_FL);
									Status = ScriptWarning;
									break;
								}
							}
							break;
						}
						case GV_GSURFACE:
						{
							if (!Dom->Value.Surface.Ptr)
							{
								Dst->Empty();
								if (Log)
									Log->Print("%p IDomGet warning: No surface pointer (%s:%i).\n",
												CurrentScriptAddress,
												_FL);
								Status = ScriptWarning;
								break;
							}

							GDomProperty p = GStringToProp(sName);
							switch (p)
							{
								case SurfaceX:
								{
									(*Dst) = Dom->Value.Surface.Ptr->X();
									break;
								}
								case SurfaceY:
								{
									(*Dst) = Dom->Value.Surface.Ptr->Y();
									break;
								}
								case SurfaceBits:
								{
									(*Dst) = Dom->Value.Surface.Ptr->GetBits();
									break;
								}
								case SurfaceColourSpace:
								{
									(*Dst) = Dom->Value.Surface.Ptr->GetColourSpace();
									break;
								}
								default:
								{
									Dst->Empty();
									if (Log)
										Log->Print("%p IDomGet warning: Unexpected surface member %s (%s:%i).\n",
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
							if (Log)
								Log->Print("%p IDomGet warning: Unexpected type %s (%s:%i).\n",
											CurrentScriptAddress,
											GVariant::TypeToString(Dom->Type),
											_FL);
							Status = ScriptWarning;
							break;
						}
					}
					break;
				}
				case IDomSet:
				{
					#if LOG_ASM
					f.Print("%p %s->DomSet(%s, %s) = %s\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr(),
							c.r[3].GetStr());
					#endif

					GVariant *Dom = Resolve();
					GVariant *Name = Resolve();
					GVariant *Arr = Resolve();
					GVariant *Value = Resolve();
					
					char *sName = Name->Str();
					if (!sName)
					{
						if (Log)
							Log->Print("%p IDomSet error: No name string (%s:%i).\n",
										CurrentScriptAddress,
										_FL);
						SetScriptError;
						break;
					}

					switch (Dom->Type)
					{
						case GV_DOM:
						{					
							Dom->Value.Dom->SetVariant(sName, *Value, Arr->Str());
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
						case GV_DATETIME:
						{
							GDomProperty p = GStringToProp(sName);
							switch (p)
							{
								case DateYear:
									Dom->Value.Date->Year(Value->CastInt32());
									break;
								case DateMonth:
									Dom->Value.Date->Month(Value->CastInt32());
									break;
								case DateDay:
									Dom->Value.Date->Day(Value->CastInt32());
									break;
								case DateHour:
									Dom->Value.Date->Hours(Value->CastInt32());
									break;
								case DateMin:
									Dom->Value.Date->Minutes(Value->CastInt32());
									break;
								case DateSec:
									Dom->Value.Date->Seconds(Value->CastInt32());
									break;
								case DateDate:
									Dom->Value.Date->SetDate(Value->Str());
									break;
								case DateTime:
									Dom->Value.Date->SetTime(Value->Str());
									break;
								case DateDateTime:
									Dom->Value.Date->Set(Value->Str());
									break;
								case DateInt64:
									Dom->Value.Date->Set((uint64)Value->CastInt64());
									break;
								default:
									if (Log)
										Log->Print("%p IDomSet warning: Unexpected datetime member %s (%s:%i).\n",
													CurrentScriptAddress,
													sName,
													_FL);
									Status = ScriptWarning;
									break;
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
										Log->Print("%p IDomSet warning: Unexpected string member %s (%s:%i).\n",
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
							if (Log)
								Log->Print("%p IDomSet warning: Unexpected type %i (%s:%i).\n",
											CurrentScriptAddress,
											Dom->Type,
											_FL);
							Status = ScriptWarning;
							break;
						}
					}
					break;
				}
				case IDomCall:
				{
					#if LOG_ASM
					f.Print("%p %s = %s->DomCall(%s, %s, ....)\n",
							CurrentScriptAddress - 1,
							c.r[0].GetStr(),
							c.r[1].GetStr(),
							c.r[2].GetStr(),
							c.r[3].GetStr());
					#endif

					GVariant *Dst = Resolve();
					GVariant *Dom = Resolve();
					GVariant *Name = Resolve();
					GVariant *Args = Resolve();
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
						{
							bool Ret = Dom->Value.Dom->CallMethod(sName, Dst, Arg);
							if (!Ret)
							{
								Dst->Empty();
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
										Log->Print("%p IDomCall warning: Unexpected list member %s (%s:%i).\n",
													CurrentScriptAddress,
													sName,
													_FL);
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
										Log->Print("%p IDomCall warning: Unexpected hashtable member %s (%s:%i).\n",
													CurrentScriptAddress,
													sName,
													_FL);
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
										*Dst = pos - s;
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
									
									strlwr(Dst->Str());
									break;
								}
								case StrUpper:
								{
									if (Dst != Dom)
										*Dst = Dom->CastString();

									strupr(Dst->Str());
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
						case GV_GFILE:
						{
							GFile *f = Dom->Value.File.Ptr;
							if (f)
							{
								bool b = f->CallMethod(sName, Dst, Arg);
								if (!b)
								{
									Dst->Empty();
									if (Log)
										Log->Print("%p IDomCall warning: Unexpected file member %s (%s:%i).\n",
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
									Log->Print("%p IDomCall warning: Unexpected type '%s' (%s:%i).\n",
												CurrentScriptAddress,
												Type,
												_FL);
								}
								Status = ScriptWarning;
							}
							break;
						}
					}
					break;
				}
				/*
				case ICallSystem:
				{
					break;
				}
				*/
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

GVirtualMachine::GVirtualMachine(GScriptContext *Context)
{
	d = new GVirtualMachinePriv(Context);
}

GVirtualMachine::~GVirtualMachine()
{
	DeleteObj(d);
}

GExecutionStatus GVirtualMachine::Execute(GCompiledCode *Code, GStream *Log)
{
	if (!d->Context || !Code)
		return ScriptError;

	return d->Execute(Code, Log, 0, 0, 0);
}

GExecutionStatus GVirtualMachine::ExecuteFunction(GCompiledCode *Code, GFunctionInfo *Func, ArgumentArray &Args, GVariant *Ret, GStream *Log)
{
	if (!d->Context || !Code)
		return ScriptError;

	return d->Execute(Code, Log, Func, &Args, Ret);
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
