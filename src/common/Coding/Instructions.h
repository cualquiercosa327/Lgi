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
		if (!(Hf->Context->*(Hf->Func))(Ret, Arg))
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
		if (!Meth->Call(NULL, Ret, Arg))
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
		if (Sf.ReturnValue)
		{
			*Sf.ReturnValue = *ReturnValue;
			CheckParam(Sf.ReturnValue->Type == ReturnValue->Type);
		}

		Frames.Length(Frames.Length()-1);
		
		Locals.SetFixedLength(false);
		if (Locals.Length() >= Sf.CurrentFrameSize)
		{
			int Base = Locals.Length() - Sf.CurrentFrameSize;
			if (ArgsOutput)
			{
				/*
				LgiTrace("Frames=%i Local=%i Base=%i\n",
					Frames.Length(),
					Locals.Length(),
					Base);
				*/
				
				if (Frames.Length() == 0)
				{
					for (unsigned i=0; i<ArgsOutput->Length(); i++)
					{
						*(*ArgsOutput)[i] = Locals[Base+i];
						/*
						GString s = Locals[Base+i].ToString();
						LgiTrace("\t%s\n", s.Get());
						*/
					}
				}
			}
			Locals.Length(Base);
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
				GDomProperty p = GStringToProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Lst->Length();
				break;
			}
			case GV_HASHTABLE:
			{
				CheckParam(Dom->Value.Hash);
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = GStringToProp(sName);
				if (p == ObjLength)
					(*Dst) = (int)Dom->Value.Hash->Length();
				break;
			}
			case GV_BINARY:
			{
				char *sName = Name->Str();
				CheckParam(sName);
				GDomProperty p = GStringToProp(sName);
				if (p == ObjLength)
					(*Dst) = Dom->Value.Binary.Length;
				break;
			}
			case GV_STRING:
			{
				char *sName = Name->Str();
				CheckParam(sName);
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
		Log->Print("%p %s = %s->DomCall(%s, ",
					CurrentScriptAddress - 1,
					c.r[0].GetStr(),
					c.r[1].GetStr(),
					c.r[2].GetStr());
	#endif

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
		Sf.ReturnValue = Dst;

		// Increase the local stack size
		int LocalsBase = Locals.Length();
		Locals.SetFixedLength(false);
		Locals.Length(LocalsBase + m->FrameSize + 1);
		Locals.SetFixedLength();
		
		#if DEBUG_CUSTOM_METHOD_CALL
		LgiTrace("CustomType.Call(%s) Args=%i, Frame=%i, Addr=%i, LocalsBase=%i ",
				sName, ArgCount, m->FrameSize, m->Address, LocalsBase);
		#endif
		
		int i = LocalsBase;
		Locals[i++] = Dom; // this pointer...
		#if DEBUG_CUSTOM_METHOD_CALL
		GString s = Dom->ToString();
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
	for (unsigned i=0; i<ArgCount; i++)
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
		case GV_BINARY:
		{
			GDomProperty p = GStringToProp(sName);
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
					CheckParam(0);
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

#undef Resolve
#undef GResolveRef