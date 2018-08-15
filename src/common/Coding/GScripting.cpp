/// \file
/// \author Matthew Allen
///
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "Lgi.h"
#include "GToken.h"
#include "GScripting.h"
#include "GLexCpp.h"
#include "GScriptingPriv.h"
#include "GUtf8.h"

extern GHostFunc SystemLibrary[];

#define ThisToken()		(Cur < Tokens.Length() ? Tokens[Cur] : 0)
#define NextToken()		(Cur < Tokens.Length() - 1 ? Tokens[Cur + 1] : 0)
// #define IsDigit(c)		( ((c)>='0') && ((c)<='9')  )

enum GStatementType
{
	GS_UNKNOWN,
	GS_IF,
	GS_FOR,
	GS_FUNCTION_DEFN,
	GS_FUNCTION_CALL,
	GS_LVALUE,
};

/// Executable statement
struct GCode
{
	struct GConditionBlock
	{
		/// Index of the expression to evaluate
		int Condition;
		/// The code to execute if the expression is true
		GArray<GCode> Block;
	};
	typedef GArray<GConditionBlock> GCondBlockArr;

	/// The type of statement
	GStatementType	Type;
	/// The start token for the statement
	int				Start;
	/// Child code element
	GArray<GCode>	Code;

	union
	{
		struct
		{
			/// Index of the statement executed at the start of the loop
			GArray<GCode> *PreStatement;
			/// The index of the condition expression
			int Condition;
			/// Index of the statement to execute after the loop body is executed
			GArray<GCode> *PostStatement;

		}	For;

		struct
		{
			GCondBlockArr *Blocks;

		}	If;

		struct
		{
			/// The command to call
			GFunc *Method;
			/// Indexes to the start of each argument expression
			GArray<int> *Args;

		}	MethodCall;

		struct
		{
			/// The operator after the L-value
			GOperator Op;
			/// The index of the expression
			int Expression;

		}	LValue;
	};

	~GCode()
	{
		switch (Type)
		{
			default:
				break;
			case GS_IF:
			{
				DeleteObj(If.Blocks);
				break;
			}
			case GS_FOR:
			{
				DeleteObj(For.PreStatement);
				DeleteObj(For.PostStatement);
				break;
			}
			case GS_FUNCTION_CALL:
			{
				DeleteObj(MethodCall.Args);
				break;
			}
			case GS_LVALUE:
			{
				break;
			}
		}
	}
};

class GExternType;
class GExternType
{
public:
	GVariantType Simple;
	GArray<GExternType*> Struct;

	int Sizeof()
	{
		switch (Simple)
		{
			default: break;
			case GV_INT32:
				return sizeof(int32);
			case GV_INT64:
				return sizeof(int64);
			case GV_STRING:
				return sizeof(char*);
			case GV_DOUBLE:
				return sizeof(double);
			case GV_NULL:
			{
				int s = 0;
				for (int i=0; i<Struct.Length(); i++)
				{
					s += Struct[i]->Sizeof();
				}
				return s;
			}
		}

		LgiAssert(0);
		return 0;
	}

	GExternType()
	{
		Simple = GV_NULL;
	}
};

struct GExternFunc : public GFunc
{
	GScriptEnginePrivate *Priv;
	char *Library;
	GExternType Return;
	GArray<GExternType*> Args;

	GExternFunc(GScriptEnginePrivate *p) : GFunc(0, ExternFunc)
	{
		Priv = p;
		Library = 0;
	}

	~GExternFunc()
	{
		DeleteArray(Library);
		Args.DeleteObjects();
	}

	GExecutionStatus Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args);
};

struct GScriptFunc : public GFunc
{
	GArray<GCode> *BodyArr;
	int	BodyIdx;
	GArray<char*> Args;
	GScriptEnginePrivate *Priv;

	GScriptFunc(GScriptEnginePrivate *p, char *meth = 0) : GFunc(meth, ScriptFunc)
	{
		Priv = p;
		BodyArr = 0;
		BodyIdx = -1;
	}

	~GScriptFunc()
	{
		Args.DeleteArrays();
	}

	GExecutionStatus Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args);
};

struct GDomPart
{
	char16 *Name;
	int Array;

	GDomPart()
	{
		Name = 0;
		Array = 0;
	}
};

struct GDomAddr
{
	GArray<GDomPart> Parts;

	char16 *operator[](int i)
	{
		if (i >= 0 && i < Parts.Length())
		{
			return Parts[i].Name;
		}

		LgiAssert(0);
		return 0;
	}

	int Length()
	{
		return Parts.Length();
	}

	void Add(char16 *part, int array = 0)
	{
		GDomPart *p = &Parts[Parts.Length()];
		p->Name = part;
		p->Array = array;
	}
	
	void Print()
	{
		for (int i=0; i<Parts.Length(); i++)
		{
			if (i)
				printf(".");
				
			GAutoString PartsName(LgiNewUtf16To8(Parts[i].Name));
			if (Parts[i].Array)
				printf("%s[%i]", PartsName.Get(), Parts[i].Array);
			else
				printf("%s", PartsName.Get());
		}
		
		if (Parts.Length() == 0)
		{
			printf("<no parts>");
		}
	}
};

struct GDomRef
{
	GVariant *Var;
	
	/// Any member name, i.e. if Var is a DOM reference
	char *Member;
	
	/// start of expression to evaluate for array index
	int Array;
	
	/// True if we own the 'Var' object
	bool FreeVar;

	GDomRef()
	{
		Var = 0;
		Member = 0;
		Array = 0;
		FreeVar = false;
	}

	GDomRef(GDomRef &dr)
	{
		if ((FreeVar = dr.FreeVar))
		{
			if (dr.Var && (Var = new GVariant) != 0)
			{
				*Var = *dr.Var;
			}
		}
		else Var = dr.Var;

		Array = dr.Array;
		Member = NewStr(dr.Member);
	}

	~GDomRef()
	{
		DeleteArray(Member);
		if (FreeVar) DeleteObj(Var);
	}

	bool Get(GScriptEnginePrivate *Priv, GVariant &v);
	bool Set(GScriptEnginePrivate *Priv, GVariant &v);
};

bool InsertUtfCharacter(GVariant *Var, GVariant &v, int Idx)
{
	bool Status = false;
	uint8 *Start = (uint8*) Var->CastString();
	uint8 *Str = Start;
	if (Str)
	{
		int OldLen = strlen((char*) Str);
		int Len = OldLen;
		uint32 ch = *Str, i = 0;
		do
		{
			if (i++ == Idx)
				break;
		}
		while ((ch = LgiUtf8To32(Str, Len)));

		if (ch)
		{
			int NewC = 0, OldC = Len;
			uint8 *Temp = Str;
			int TempLen = Len;
			LgiUtf8To32(Temp, TempLen);
			OldC -= TempLen;

			char NewBytes[8];
			uint8 *s = (uint8*) NewBytes;
			if (v.Type == GV_STRING)
			{
				uint8 *f = (uint8*) v.Str(); // from
				if (IsUtf8_1Byte(*f))
				{
					*s++ = *f;
				}
				else if (IsUtf8_Lead(*f))
				{
					*s++ = *f++;
					while (IsUtf8_Trail(*f))
						*s++ = *f++;
				}
				NewC = s - (uint8*)NewBytes;
			}
			else
			{
				int l = sizeof(NewBytes);
				LgiUtf32To8(v.CastInt32(), s, l);
				NewC = sizeof(NewBytes) - l;
			}

			if (NewC > OldC)
			{
				// Re-alloc memory to make space for larger character
				char *Ins = new char[OldLen + (NewC - OldC) + 1];
				if (Ins)
				{
					char *i = Ins;
					memcpy(i, Start, Str - Start);
					i += Str - Start;
					memcpy(i, NewBytes, NewC);
					i += NewC;
					memcpy(i, Str + OldC, Len - OldC + 1);

					Var->Empty();
					Var->Type = GV_STRING;
					Var->Value.String = Ins;
					Status = true;
				}
			}
			else
			{
				// Just insert char in existing memory
				for (int n=0; n<NewC; n++)
					Str[n] = NewBytes[n];
				if (NewC < OldC)
					memmove(Str + NewC, Str + OldC, Len - OldC + 1);
				Status = true;
			}
		}
	}
	return Status;
}

class StackFrame : public GHashTable
{
public:
	~StackFrame()
	{
		for (void *v = First(); v; v = Next())
		{
			GVariant *i = (GVariant*)v;
			DeleteObj(i);
		}
		Empty();
	}
};

class GScriptEnginePrivate : public GScriptUtils, public GCompileTools
{
public:
	GScriptEngine1 *Engine;
	GStringPipe Term;
	GViewI *Parent;
	char16 *Script;
	GArray<StackFrame*> Stack;
	GScriptContext *Context;
	GHashTbl<const char*, GFunc*> Methods;
	bool IsCompiling;

	GArray<GCode> Compiled;
	GArray<char16*> Tokens;
	GArray<int> Lines;

	GScriptEnginePrivate(GScriptEngine1 *e)
	{
		Engine = e;
		Script = 0;
		IsCompiling = false;
		Stack.Add(new StackFrame);
	}

	~GScriptEnginePrivate()
	{
		Empty(true);
	}

	char *ErrorTitle()
	{
		return (char*) (IsCompiling ? "CompileError" : "ExecuteError");
	}

	GExecutionStatus Call(GFunc *Fn, GVariant *Ret, GArray<int> *Idx)
	{
		if (!Fn)
			return ScriptError;

		ArgumentArray Args, Mem;
		ProcessArguments(Args, Mem, Idx);
		GExecutionStatus Status = Fn->Call(Context, Ret, Args);
		Mem.DeleteObjects();

		return Status;
	}

	void Empty(bool Deleting = false)
	{
		DeleteArray(Script);
		Tokens.DeleteArrays();
		Lines.Length(0);
		Compiled.Length(0);

		Stack.DeleteObjects();
		if (!Deleting)
			Stack.Add(new StackFrame);

		GArray<GFunc*> Del;
		for (void *v = Methods.First(); v; v = Methods.Next())
		{
			GFunc *i = (GFunc*)v;
			if (i->Type != HostFunc)
			{
				Del.Add(i);
			}
		}
		for (int i=0; i<Del.Length(); i++)
		{
			Methods.Delete(Del[i]->Method);
			DeleteObj(Del[i]);
		}
	}

	void DumpCode(GArray<GCode> &c, int Depth = 0)
	{
		char Tabs[265];
		memset(Tabs, '\t', Depth);
		Tabs[Depth] = 0;

		for (int i=0; i<c.Length(); i++)
		{
			GCode &Code = c[i];
			switch (Code.Type)
			{
				case GS_IF:
				{
					GCode::GConditionBlock *b = Code.If.Blocks ? &(*Code.If.Blocks)[0] : 0;
					LgiTrace("%sif (%i)\n%s{\n", Tabs, b ? b->Condition : 0, Tabs);
					break;
				}
				case GS_FOR:
				{
					LgiTrace("%sfor (%p; %i; %p)\n%s{\n",
						Tabs,
						Code.For.PreStatement,
						Code.For.Condition,
						Code.For.PostStatement,
						Tabs);
					break;
				}
				case GS_FUNCTION_CALL:
				{
					LgiTrace("%sfunc:%s(",
						Tabs, Code.MethodCall.Method->Method);
					if (Code.MethodCall.Args)
					{
						int Len = Code.MethodCall.Args->Length();
						for (int n=0; n<Len; n++)
						{
							int Idx = (*Code.MethodCall.Args)[n];
							LgiTrace("%i%s", Idx, n<Len-1?", ":"");
						}
					}
					LgiTrace(")\n");
					break;
				}
				case GS_LVALUE:
				{
					LgiTrace("%slvalue\n", Tabs);
					break;
				}
				default:
				{
					LgiAssert(0);
					break;
				}
			}

			if (Code.Code.Length())
			{
				DumpCode(Code.Code, Depth + 1);
			}
		}
	}

	/// Adds tokens to the 'Tokens' array
	bool LexScript(char *script)
	{
		char16 *w = LgiNewUtf8To16(script);
		if (w)
		{
			int Line = 1;
			char16 *s = w, *t, *prev = w;
			while ((t = LexCpp(s, LexStrdup)))
			{
				while (prev < s)
				{
					if (*prev++ == '\n')
						Line++;
				}

				Tokens.Add(t);
				Lines.Add(Line);
			}

			if (!Script)
			{
				Script = w;
			}
			else
			{
				DeleteArray(w);
			}

			return true;
		}

		return false;
	}

	bool CompileTo(GArray<GCode> &Compiled, char *script)
	{
		bool Status = false;

		// Lex script into tokens
		int Cur = Tokens.Length();
		if (LexScript(script))
		{
			// Do compile stage
			IsCompiling = true;
			while (Cur < Tokens.Length())
			{
				// int Start = Cur;
				if (!Compile_Statement(Compiled, Cur))
				{
					if (Compiled.Length() > 0)
						Compiled.Length(Compiled.Length()-1);
					Status = false;
					break;
				}
				else Status = true;
			}
			IsCompiling = false;
		}

		return Status;
	}

	void ProcessArguments(ArgumentArray &Args, ArgumentArray &Mem, GArray<int> *Idx);
	int FindEndOfBlock(int Cur);
	GVariant *Var(char16 *name, bool create = true);
	bool ToVar(GVariant &v, char16 *s);
	bool Require(int &Cur, const char *tok);
	bool Type(int &Cur, class GExternType &Type);
	bool CallExtern(int &Cur, uint32 &ReturnValue, struct GExternFunc *Func);

	// Dom addressing
	bool CreateDomAddr(int &Cur, GDomAddr &Addr);
	bool ResolveDomAddr(struct GDomRef &Ref, GDomAddr &Addr, bool Create);

	// Compiling of code	
	bool	Compile_MethodCall(int &Cur);
	bool	Compile_Expression(int &Cur, int Depth = 0);
	bool    Compile_Statement(GArray<GCode> &To, int &Cur);

	// Execution of code
	GVariant *Execute_Expression(int &Cur, GDom *Src = 0, int Depth = 0);
	GExecutionStatus Execute_Statement(GArray<GCode> &To);
};

bool GDomRef::Get(GScriptEnginePrivate *Priv, GVariant &v)
{
	if (Var)
	{
		if (Member)
		{
			if (Var->Type == GV_DOM)
			{
				if (Var->Value.Dom)
				{
					GVariant *ArrVal = 0;
					if (Array)
					{
						int Cur = Array;
						ArrVal = Priv->Execute_Expression(Cur);
					}

					bool Ret = Var->Value.Dom->GetVariant(Member, v, ArrVal ? ArrVal->CastString() : 0);
					DeleteObj(ArrVal);
					if (!Ret)
					{
						Priv->Term.Print("%s - Couldn't resolve member '%s'\n", Priv->ErrorTitle(), Member);
					}
					return Ret;
				}
			}
			else if (Var->Type == GV_STRING)
			{
				if (stricmp(Member, "length") == 0)
				{
					v = (int) strlen(Var->Str());
					return true;
				}
				else
				{
					Priv->Term.Print("%s - Couldn't resolve member '%s'\n", Priv->ErrorTitle(), Member);
				}
			}
			else if (Var->Type == GV_DATETIME)
			{
				if (Var->Value.Date)
				{
					if (stricmp(Member, "day") == 0)
					{
						v = Var->Value.Date->Day();
					}
					else if (stricmp(Member, "month") == 0)
					{
						v = Var->Value.Date->Month();
					}
					else if (stricmp(Member, "year") == 0)
					{
						v = Var->Value.Date->Year();
					}
					else if (stricmp(Member, "hour") == 0)
					{
						v = Var->Value.Date->Hours();
					}
					else if (stricmp(Member, "minute") == 0)
					{
						v = Var->Value.Date->Minutes();
					}
					else if (stricmp(Member, "second") == 0)
					{
						v = Var->Value.Date->Seconds();
					}
					else return false;

					return true;
				}
				else
				{
					Priv->Term.Print("%s - Couldn't resolve member '%s'\n", Priv->ErrorTitle(), Member);
				}
			}
			else if (Var->Type == GV_LIST)
			{
				if (stricmp(Member, "length") == 0)
				{
					v = Var->Value.Lst->Length();
					return true;
				}
				else
				{
					Priv->Term.Print("%s - Couldn't resolve member '%s'\n", Priv->ErrorTitle(), Member);
				}
			}
			else if (Var->Type == GV_HASHTABLE)
			{
				if (stricmp(Member, "length") == 0)
				{
					v = Var->Value.Hash->Length();
					return true;
				}
				else
				{
					Priv->Term.Print("%s - Couldn't resolve member '%s'\n", Priv->ErrorTitle(), Member);
				}
			}
		}
		else
		{
			bool Status = false;

			if (Array)
			{
				int Cur = Array;
				GVariant *a = Priv->Execute_Expression(Cur);
				if (a)
				{
					if (Var->Type == GV_STRING)
					{
						int Idx = a->CastInt32();
						uint8 *Str = (uint8*) Var->CastString();
						if (Str)
						{
							int Len = strlen((char*) Str);
							int i = 0;
							uint32 ch;
							while ((ch = LgiUtf8To32(Str, Len)))
							{
								if (i++ == Idx)
								{
									v = (int)ch;
									Status = true;
									break;
								}
							}
						}
					}
					else if (Var->Type == GV_HASHTABLE)
					{
						char *Key = a->CastString();
						GVariant *h = (GVariant*) Var->Value.Hash->Find(Key);
						if (h)
						{
							v = *h;
						}
						else
						{
							v.Empty();
						}
						Status = true;
					}
					else if (Var->Type == GV_LIST)
					{
						int Idx = a->CastInt32();
						GVariant *h = (*Var->Value.Lst)[Idx];
						if (h)
						{
							v = *h;
							Status = true;
						}
					}
					else
					{
						Priv->Term.Print("%s:%i - Can't index into type '%i'.\n", __FILE__, __LINE__, Var->Type);
					}

					DeleteObj(a);
				}
				else Priv->Term.Print("%s:%i - Can't evaluate array expression.\n", __FILE__, __LINE__);
			}
			else
			{
				v = *Var;
				Status = true;
			}

			return Status;
		}
	}

	return false;
}

bool GDomRef::Set(GScriptEnginePrivate *Priv, GVariant &v)
{
	if (Var)
	{
		if (Member)
		{
			if (Var->Type == GV_DOM)
			{
				if (Var->Value.Dom)
				{
					return Var->Value.Dom->SetValue(Member, v);
				}
			}
			else if (Var->Type == GV_STRING ||
					 Var->Type == GV_NULL)
			{
				if (stricmp(Member, "length") == 0)
				{
					Var->Empty();
					int Len = v.CastInt32();
					Var->Type = GV_STRING;
					if ((Var->Value.String = new char[Len+1]))
					{
						memset(Var->Value.String, ' ', Len);
						Var->Value.String[Len] = 0;
						return true;
					}
				}
			}
			else if (Var->Type == GV_DATETIME)
			{
				if (Var->Value.Date)
				{
					if (stricmp(Member, "day") == 0)
					{
						Var->Value.Date->Day(v.CastInt32());
					}
					else if (stricmp(Member, "month") == 0)
					{
						Var->Value.Date->Month(v.CastInt32());
					}
					else if (stricmp(Member, "year") == 0)
					{
						Var->Value.Date->Year(v.CastInt32());
					}
					else if (stricmp(Member, "hour") == 0)
					{
						Var->Value.Date->Hours(v.CastInt32());
					}
					else if (stricmp(Member, "minute") == 0)
					{
						Var->Value.Date->Minutes(v.CastInt32());
					}
					else if (stricmp(Member, "second") == 0)
					{
						Var->Value.Date->Seconds(v.CastInt32());
					}
					else return false;

					return true;
				}
			}
		}
		else
		{
			if (Array)
			{
				bool Status = false;

				int Cur = Array;
				GVariant *a = Priv->Execute_Expression(Cur);
				if (a)
				{
					if (Var->Type == GV_STRING)
					{
						Status = InsertUtfCharacter(Var, v, a->CastInt32());
					}
					else if (Var->Type == GV_HASHTABLE)
					{
						char *Key = a->CastString();
						GVariant *n = (GVariant*) Var->Value.Hash->Find(Key);
						if (!n)
						{
							Var->Value.Hash->Add(Key, n = new GVariant);
						}
						if (n)
						{
							*n = v;
							Status = true;
						}
					}
					else if (Var->Type == GV_LIST)
					{
						int Idx = a->CastInt32();
						if (Idx < 0 || Idx >= Var->Value.Lst->Length())
						{
							// Add a new element
							GVariant *h = new GVariant(v);
							if (h)
							{
								Var->Value.Lst->Insert(h);
								Status = true;
							}
						}
						else
						{
							// Change an existing element
							GVariant *h = (*Var->Value.Lst)[Idx];
							if (h) *h = v;
							Status = true;
						}
					}
					else
					{
						Priv->Term.Print("%s:%i - Can't index into type '%i'.\n", __FILE__, __LINE__, Var->Type);
						LgiAssert(0);
					}

					DeleteObj(a);
				}
				else Priv->Term.Print("%s:%i - Can't evaluate array expression.\n", __FILE__, __LINE__);
			}
			else
			{
				*Var = v;
				return true;
			}
		}
	}

	return false;
}


#define CompToInt(v) \
	(v[0] == '0' && v[1] == 'x') ? htoi(v + 2) : atoi(v);

GScriptEngine1::GScriptEngine1(GViewI *parent, GScriptContext *context)
{
	d = new GScriptEnginePrivate(this);
	d->Parent = parent;
	d->Context = context;
	d->Script = 0;

	int i = 0;
	for (GHostFunc *cmd = SystemLibrary + i; cmd->Method; cmd++)
	{
		d->Methods.Add(cmd->Method, cmd);
	}

	if (d->Context)
	{
		d->Context->SetEngine(this);
		GHostFunc *CmdTable = d->Context->GetCommands();
		if (CmdTable)
		{
			for (GHostFunc *cmd = CmdTable; cmd->Method; cmd++)
			{
				d->Methods.Add(cmd->Method, (GFunc*)cmd);
			}
		}
	}
}

GScriptEngine1::~GScriptEngine1()
{
	if (d->Context)
	{
		d->Context->SetEngine(0);
	}

	d->Tokens.DeleteArrays();
	DeleteObj(d);
}

GVariant *GScriptEngine1::Var(char16 *name, bool create)
{
	return d->Var(name, create);
}

GStringPipe *GScriptEngine1::GetTerm()
{
	return &d->Term;
}

bool GScriptEngine1::CallMethod(const char *Method, GVariant *Ret, ArgumentArray &Args)
{
	bool Status = false;

	if (Method)
	{
		GFunc *Func = (GFunc*) d->Methods.Find(Method);
		if (Func)
		{
			Status = Func->Call(d->Context, Ret, Args);
		}		
	}

	return Status;
}

bool GScriptEnginePrivate::CreateDomAddr(int &Cur, GDomAddr &Addr)
{
	char16 *t = ThisToken();
	if (t && isalpha(*t))
	{
		Addr.Add(t);
		Cur++;

		// Build DOM address
		while ((t = ThisToken()))
		{
			if (StricmpW(t, sPeriod) == 0)
			{
				Cur++;
				t = ThisToken();
				if (isalpha(*t))
				{
					Cur++;
					Addr.Add(t);
				}
				else
				{
					Term.Print("%s:%i - Expecting name after '.'.\n", ErrorTitle(), Lines[Cur]);
					break;
				}
			}
			else if (StricmpW(t, sStartSqBracket) == 0)
			{
				Cur++;

				int Start = Cur;
				Addr.Parts[Addr.Length()-1].Array = Start;

				if (Compile_Expression(Cur))
				{
					t = ThisToken();
					if (t && StricmpW(t, sEndSqBracket) == 0)
					{
						Cur++;
					}
					else
					{
						Term.Print("%s:%i - Expecting ']'.\n", ErrorTitle(), Lines[Cur]);
						break;
					}
				}
				else
				{
					Term.Print("%s:%i - Expression error.\n", ErrorTitle(), Lines[Start]);
					break;
				}
			}
			else break;
		}

		return true;
	}

	return false;
}

bool GScriptEnginePrivate::ResolveDomAddr(GDomRef &Ref, GDomAddr &Addr, bool Create)
{
	if (Addr.Length() == 1)
	{
		if ((Ref.Var = Var(Addr[0], Create)))
		{
			Ref.Array = Addr.Parts[0].Array;
		}
		return Ref.Var != 0;
	}
	else if (Addr.Length() > 1)
	{
		Ref.Var = Var(Addr[0], Create);
		if (Ref.Var)
		{
			for (int i=1; i<Addr.Length() - 1; i++)
			{
				if (Ref.Var->Type == GV_DOM &&
					Ref.Var->Value.Dom)
				{
					GVariant *v = new GVariant;
					if (v)
					{
						char *c = LgiNewUtf16To8(Addr[i]);
						if (c)
						{
							char *ArrayVal = 0;
							int ArrayPos = Addr.Parts[i].Array;
							if (ArrayPos)
							{
								GVariant *a = Execute_Expression(ArrayPos);
								if (a)
								{
									ArrayVal = NewStr(a->Str());
									DeleteObj(a);
								}
							}

							bool Has = Ref.Var->Value.Dom->GetVariant(c, *v, ArrayVal);
							DeleteArray(c);
							if (Has)
							{
								if (Ref.FreeVar)
									DeleteObj(Ref.Var);
								Ref.Var = v;
								Ref.FreeVar = true;
								v = 0;
							}
							else
							{
								DeleteObj(v);
								return false;
							}
						}

						DeleteObj(v);
					}
					else return false;
				}
				else return false;
			}

			GDomPart &Part = Addr.Parts[Addr.Length()-1];
			Ref.Member = LgiNewUtf16To8(Part.Name);
			Ref.Array = Part.Array;

			return true;
		}
		else
		{
			Term.Print("%s - Failed to resolve '%S'\n", ErrorTitle(), Addr[0]);
		}
	}

	return false;
}

void GScriptEngine1::Empty()
{
	d->Empty();
}

bool GScriptEngine1::Compile(char *Script, const char *FileName, bool Add)
{
	bool Status = false;

	if (!Add)
		d->Empty();

	if (Script)
	{
		Status = d->CompileTo(d->Compiled, Script);

		#if 0
		d->DumpCode(d->Compiled);
		#endif
	}

	return Status;
}

GExecutionStatus GScriptEngine1::Run()
{
	if (d->Compiled.Length() > 0)
	{
		// Run it
		return d->Execute_Statement(d->Compiled);
	}

	return ScriptError;
}

class ScriptTokenState
{
	GScriptEnginePrivate *d;
	int TokLen;
	int LinesLen;

public:
	ScriptTokenState(GScriptEnginePrivate *priv)
	{
		d = priv;
		TokLen = d->Tokens.Length();
		LinesLen = d->Lines.Length();
	}

	~ScriptTokenState()
	{
		for (int i=TokLen; i<d->Tokens.Length(); i++)
		{
			DeleteArray(d->Tokens[i]);
		}
		d->Tokens.Length(TokLen);
		d->Lines.Length(LinesLen);
	}
};

GExecutionStatus GScriptEngine1::RunTemporary(char *Script)
{
	GExecutionStatus Status = ScriptError;

	if (Script)
	{
		ScriptTokenState State(d);

		GArray<GCode> Compiled;
		if (d->CompileTo(Compiled, Script))
		{
			if (Compiled.Length() > 0)
			{
				Status = d->Execute_Statement(Compiled);
			}
		}
	}

	return Status;
}

bool GScriptEngine1::EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression)
{
	bool Status = false;

	if (Result && Expression)
	{
		ScriptTokenState State(d);

		int Cur = d->Tokens.Length();
		if (d->LexScript(Expression))
		{
			GVariant *v = d->Execute_Expression(Cur, VariableSource);
			if (v)
			{
				*Result = *v;
				DeleteObj(v);
				Status = true;
			}
		}
	}

	return Status;
}

void GScriptEngine1::DumpVariables()
{
	d->Term.Print("Variables:\n");
	char *VarName;
	
	for (int i=0; i<d->Stack.Length(); i++)
	{
		StackFrame *sf = d->Stack[i];
		for (GVariant *v = (GVariant*)sf->First(&VarName); v; v = (GVariant*)sf->Next(&VarName))
		{
			char t[128];
			ZeroObj(t);
			memset(t, ' ', (i + 1) * 4);
			switch (v->Type)
			{
				case GV_NULL:
					d->Term.Print("%s%s = null\n", t, VarName);
					break;
				case GV_BOOL:
					d->Term.Print("%s%s = (bool) %s\n", t, VarName, (char*) (v->Value.Bool ? "true" : "false"));
					break;
				case GV_INT32:
					d->Term.Print("%s%s = (int) %i\n", t, VarName, v->CastInt32());
					break;
				case GV_DOUBLE:
					d->Term.Print("%s%s = (double) %f\n", t, VarName, v->CastDouble());
					break;
				case GV_STRING:
					d->Term.Print("%s%s = (string) \"%s\"\n", t, VarName, v->Str());
					break;
				case GV_DOM:
					d->Term.Print("%s%s = (GDom*) 0x%p\n", t, VarName, v->Value.Dom);
					break;
				case GV_VOID_PTR:
					d->Term.Print("%s%s = (void*) 0x%p\n", t, VarName, v->Value.Ptr);
					break;
				default:
					d->Term.Print("%s%s = (type %i) ?????\n", t, VarName, v->Type);
					break;
			}
		}
	}
}

GVariant *GScriptEnginePrivate::Var(char16 *name, bool create)
{
	if (name)
	{
		char b[256];
		int Len = StrlenW(name) * sizeof(*name);
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (const void*&)name, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;

		GVariant *v = 0;
		for (int i=Stack.Length()-1; i>=0; i--)
		{
			StackFrame *sf = Stack[i];
			if ((v = (GVariant*) sf->Find(b)))
			{
				break;
			}
		}

		if (!v && create)
		{
			if ((v = new GVariant))
			{
				Stack[Stack.Length()-1]->Add(b, v);
			}
		}

		return v;
	}

	return 0;
}

bool GScriptEnginePrivate::ToVar(GVariant &v, char16 *s)
{
	if (s)
	{
		char16 *f = *s == '\"' ? s + 1 : s;
		bool IsNumber = false;

		if (f[0] == '0' && tolower(f[1]) == 'x')
		{
			v = htoi(f + 2);
			return true;
		}
		else if (IsDigit(*f) || *f == '-' || *f == '.')
		{
			IsNumber = true;

			for (char16 *c = f; *s; s++)
			{
				if (*c == '\"' ||
					*c == ' ' ||
					*c == '\t' ||
					*c == '\r' ||
					*c == '\n')
					break;
				else if (!IsDigit(*c) && !strchr("-e.", *c))
				{
					IsNumber = false;
					break;
				}
			}
		}

		if (IsNumber)
		{
			if (StrchrW(f, '.'))
			{
				v = atof(f);
				return true;
			}
			else
			{
				v = atoi(f);
				return true;
			}
		}
		else
		{
			v.Empty();
			v.Type = GV_STRING;
			if (*s == '\"')
			{
				int Len = StrlenW(s);
				v.Value.String = LgiNewUtf16To8(s + 1, (Len - 2) * sizeof(*s));
			}
			else
			{
				v.Value.String = LgiNewUtf16To8(s);
			}
			return true;
		}
	}

	return false;
}

bool GScriptEnginePrivate::Require(int &Cur, const char *tok)
{
	if (tok && Cur >= 0 && Cur < Tokens.Length())
	{
		char16 *t = Tokens[Cur];
		if (t)
		{
			char b[64];
			int Len = StrlenW(t) * sizeof(*t);
			if (Len < 32)
			{
				int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (const void*&)t, LGI_WideCharset, Len);
				b[Bytes/sizeof(*b)] = 0;
				if (stricmp(b, tok) == 0)
				{
					Cur++;
					return true;
				}
			}
		}
	}

	return false;
}

bool GScriptEnginePrivate::Compile_MethodCall(int &Cur)
{
	char16 *t = ThisToken();
	if (t)
	{
		if (!Require(Cur, "("))
		{
			Term.Print("%s:%i - Expecting '(' after method name\n", ErrorTitle(), Lines[Cur]);
			return false;
		}

		while ((t = ThisToken()))
		{
			if (*t == ')')
			{
				break;
			}
			else if (*t == ',')
			{
				Cur++;
			}
			else
			{
				if (!Compile_Expression(Cur))
				{
					break;
				}
			}
		}

		if (!Require(Cur, ")"))
		{
			Term.Print("%s:%i - Expecting '(' after method name\n", ErrorTitle(), Lines[Cur]);
			return false;
		}
		else return true;
	}

	return false;
}

bool GScriptEnginePrivate::Compile_Expression(int &Cur, int Depth)
{
	if (Cur >= 0 && Cur < Tokens.Length())
	{
		int Args = 0;
		int Ops = 0;
		int StartToken = Cur;
		int PrevIsOp = -1;

		char16 *t;
		while ((t = Tokens[Cur]))
		{
			if (StricmpW(t, sStartRdBracket) == 0)
			{
				Cur++;
				if (Compile_Expression(Cur, Depth + 1))
				{
					Args++;
					PrevIsOp = 0;
				}
				else
				{
					break;
				}
			}
			else if (StricmpW(t, sEndRdBracket) == 0)
			{
				if (Depth > 0)
					Cur++;
				break;
			}
			else if (StricmpW(t, sComma) == 0 ||
					 StricmpW(t, sSemiColon) == 0)
			{
				break;
			}
			else if (Depth == 0 && StricmpW(t, sEndSqBracket) == 0)
			{
				break;
			}
			else
			{
				GOperator Top = IsOp(t, PrevIsOp);
				if (Top != OpNull)
				{
					if (PrevIsOp == 1)
					{
						GBase u;
						u.NameW(t);
						Term.Print("%s:%i - Not expecting '%s', 2nd operator in a row.\n", ErrorTitle(), Lines[StartToken], u.Name());
						break;
					}
					else
					{
						Ops++;
						PrevIsOp = 1;
					}
				}
				else
				{
					if (PrevIsOp == 0)
					{
						GBase u;
						u.NameW(t);
						Term.Print("%s:%i - Not expecting '%s' after non-operator token.\n", ErrorTitle(), Lines[StartToken], u.Name());
						break;
					}

					char m[64], *mp = m;
					for (char16 *c = t; *c && mp < m + sizeof(m) - 1; c++)
					{
						*mp++ = *c;
					}
					*mp++ = 0;

					if (Methods.Find(m))
					{
						Cur++;

						if (!Compile_MethodCall(Cur))
						{
							return false;
						}

						Args++;
						Cur--;
					}
					else
					{
						GDomAddr Addr;
						if (CreateDomAddr(Cur, Addr))
						{
							Cur--;
							Args++;
						}
						else
						{
							Args++;
						}
					}
					PrevIsOp = 0;
				}

				Cur++;
			}
		}

		if (Args > 1 && (Ops == Args - 1 || Ops == Args))
		{
			return true;
		}
		else if (Args == 1)
		{
			return true;
		}
	}
	else
	{
		Term.Print("%s:%i - Unexpected end of file.\n", __FILE__, __LINE__);
	}

	return false;
}

GVariant *GScriptEnginePrivate::Execute_Expression(int &Cur, GDom *Src, int Depth)
{
	// Internally to this function we use the GVariant::User member to
	// store non-zero if we don't own the GVariant object. And thus
	// shouldn't free the memory on exit.

	if (Cur >= 0 && Cur < Tokens.Length())
	{
		GArray<GVariant*> Args;
		int StartToken = Cur;
		int PrevIsOp = -1;

		char16 *t;
		while ((t = Tokens[Cur]))
		{
			if (StricmpW(t, sStartRdBracket) == 0)
			{
				Cur++;
				GVariant *r = Execute_Expression(Cur, Src, Depth + 1);
				if (r)
				{
					LgiAssert(r->Type != GV_NULL);
					Args.Add(r);
					PrevIsOp = 0;
				}
				else
				{
					Term.Print("%s:%i - Expression returned NULL (Line %i)\n",
						__FILE__, __LINE__, Lines[StartToken]);
					break;
				}
			}
			else if (StricmpW(t, sEndRdBracket) == 0)
			{
				if (Depth > 0)
					Cur++;
				break;
			}
			else if (StricmpW(t, sComma) == 0 ||
					 StricmpW(t, sSemiColon) == 0)
			{
				break;
			}
			else if (Depth == 0 && StricmpW(t, sEndSqBracket) == 0)
			{
				break;
			}
			else
			{
				GOperator Top = IsOp(t, PrevIsOp);
				if (Top != OpNull)
				{
					if (PrevIsOp == 1)
					{
						GBase u;
						u.NameW(t);
						Term.Print("%s:%i - '%s' was the 2nd operator in a row.\n", ErrorTitle(), Lines[StartToken], u.Name());
						break;
					}
					else
					{
						Args.Add(new GVariant(Top));
						PrevIsOp = 1;
					}
				}
				else
				{
					if (PrevIsOp == 0)
					{
						if (*t == '-')
						{
							// Emit a minus operator and treat the value as a number
							t++;
							Args.Add(new GVariant(OpMinus));
						}
						else
						{						
							GBase u;
							u.NameW(t);
							Term.Print("%s:%i - Not expecting '%s' after non-operator.\n", ErrorTitle(), Lines[StartToken], u.Name());
							break;
						}
					}

					GFunc *Method = 0;
					char16 *Next = NextToken();
					if (Next && Next[0] == '(' && Next[1] == 0)
					{
						char Name[256];
						const void *In = t;
						int InLen = StrlenW(t) * sizeof(*t);
						int Converted = LgiBufConvertCp(Name, "utf-8", sizeof(Name)-1, In, LGI_WideCharset, InLen);
						Name[Converted] = 0;
						Method = (GFunc*) Methods.Find(Name);
						if (Method)
						{
							GArray<int> Idx;
							ArgumentArray Arg, Mem;
							GVariant *Ret = new GVariant;

							// Collect arguments
							Cur += 2;
							while ((t = ThisToken()))
							{
								if (*t == ')')
								{
									Cur++;
									break;
								}
								else if (*t == ',')
								{
									Cur++;
								}
								else
								{
									Idx.Add(Cur);
									Compile_Expression(Cur);
								}
							}

							// Call method
							ProcessArguments(Arg, Mem, &Idx);
							if (Method->Call(Context, Ret, Arg))
							{
								LgiAssert(Ret->Type != GV_NULL);
								Args.Add(Ret);
								PrevIsOp = 0;
							}
							else DeleteObj(Ret);

							Mem.DeleteObjects();
							Cur--;
						}
						else
						{
							Term.Print("%s:%i - Method '%s' not defined. (Line %i)\n",
								__FILE__, __LINE__, Name, Lines[StartToken]);
						}
					}

					if (!Method)
					{
						GVariant *v = 0;
						GDomRef *Ref = 0;

						int Start = Cur;
						GDomAddr Addr;
						if (CreateDomAddr(Cur, Addr))
						{
							if (Src)
							{
								// Using variable source

								// Set up the variable name as a NULL terminated wide string
								GArray<char16> a;
								for (int i=Start; i<Cur; i++)
								{
									char16 *k = Tokens[i];
									for (int n=0; k[n]; n++)
										a.Add(k[n]);
								}
								a.Add(0);
								char *utf = LgiNewUtf16To8(&a[0]);

								// Ask the variable source to get the value
								if ((v = new GVariant))
									Src->GetValue(utf, *v);

								// Clean up...
								DeleteArray(utf);
								Cur--;
							}
							else
							{
								Cur--;

								// Normal DOM reference, resolve it here
								Ref = new GDomRef;
								if (!ResolveDomAddr(*Ref, Addr, false))
								{
									// Variable name...
									LgiAssert(Ref->Array == 0);
									DeleteObj(Ref);
									if ((v = new GVariant))
										ToVar(*v, t);
								}
							}
						}
						else
						{
							if ((v = new GVariant))
								ToVar(*v, t);
						}

						if (v)
						{
							if (v->Type == GV_NULL)
							{
								return NULL;
							}

							Args.Add(v);
						}
						else if (Ref && Ref->Var)
						{
							if (Ref->Var->Type == GV_DOMREF)
							{
								GDom *dom = Ref->Var->CastDom();
								if (dom)
									Args.Add(new GVariant(dom, Ref->Member));
								else
									LgiAssert(!"No DOM ptr.");
							}
							else
							{
								if (Ref->Array)
								{
									switch (Ref->Var->Type)
									{
										case GV_LIST:
										{
											char16 *a = Tokens[Ref->Array];
											int idx = atoi(a);
											GVariant *k = Ref->Var->Value.Lst->ItemAt(idx);
											if (k)
											{
												k->User = true;
												Args.Add(k);
											}
											else
											{
												Term.Print("%s:%i - List idx %i doesn't exist.\n", ErrorTitle(), Lines[StartToken], idx);
												Args.Add(new GVariant);
											}
											break;
										}
										case GV_HASHTABLE:
										{
											char16 *a = Tokens[Ref->Array];
											char *u = 0;
											GVariant *k = 0;

											if (a &&
												(u = LgiNewUtf16To8(a)) != 0 &&
												(k = (GVariant*) Ref->Var->Value.Hash->Find(u)) != 0)
											{
												k->User = true;
												Args.Add(k);
											}
											else
											{
												Term.Print("%s:%i - Hash element '%s' doesn't exist.\n", ErrorTitle(), Lines[StartToken], u);
												Args.Add(new GVariant);
											}

											DeleteArray(u);
											break;
										}
										case GV_DOM:
										{
											if (Ref->Var->Value.Dom)
											{
												int Exp = Ref->Array;
												GVariant *Idx = Execute_Expression(Exp, 0, 0);
												if (Idx)
												{
													GVariant *Result = new GVariant;
													if (Ref->Var->Value.Dom->GetVariant(Ref->Member, *Result, Idx->CastString()))
													{
														Args.Add(Result);
													}
													else DeleteObj(Result);													
													DeleteObj(Idx);
												}
											}
											break;
										}
										default:
										{
											Term.Print("%s:%i - Array lookup not support for variant type %i.\n", ErrorTitle(), Lines[StartToken], Ref->Var->Type);
											Args.Add(new GVariant);
											break;
										}
									}
								}
								else
								{
									Ref->Var->User = !Ref->FreeVar;

									if (Ref->Member)
									{
										GVariant *t = new GVariant;
										if (t)
										{
											Ref->Get(this, *t);
											Args.Add(t);
										}
									}
									else
									{
										Args.Add(Ref->Var);
										Ref->Var = 0;
									}
								}
							}
						}

						PrevIsOp = 0;
					}
				}

				Cur++;
			}
		}

		if (Args.Length() > 1)
		{
			// Loop through and combine values and operators according to precedence
			while (Args.Length() > 1)
			{
				// Find which operator to handle first
				int LowestOp = -1;
				int LowestPre = -1;
				for (int i=0; i<Args.Length(); i++)
				{
					LgiAssert(Args[i]->Type);

					if (Args[i]->Type == GV_OPERATOR)
					{
						int p = GetPrecedence(Args[i]->Value.Op);
						if (LowestOp < 0 || p < LowestPre)
						{
							LowestPre = p;
							LowestOp = i;
						}
					}
				}

				LgiAssert(LowestOp >= 0 && LowestOp < Args.Length());

				OperatorType Type = OpType(Args[LowestOp]->Value.Op);
				if (Type == OpPrefix)
				{
					if (LowestOp >= Args.Length() - 1)
					{
						Term.Print("%s:%i - Prefix operator followed by nothing.\n", ErrorTitle(), Lines[StartToken]);
						break;
					}
					else
					{
						GVariant *Op = Args[LowestOp];
						GVariant *Var = Args[LowestOp + 1];
						GVariant r;

						switch (Op->Value.Op)
						{
							case OpUnaryPlus:
							{
								// do nothing
								break;
							}
							case OpUnaryMinus:
							{
								// Negate the argument
								if (Var->Type == GV_DOUBLE)
								{
									r = -Var->CastDouble();
								}
								else if (r.Type == GV_INT64)
								{
									r = -Var->CastInt64();
								}
								else
								{
									r = -Var->CastInt32();
								}
								break;
							}
							case OpPreInc:
							{
								// Add one
								if (Var->Type == GV_DOUBLE)
								{
									r = Var->CastDouble() + 1;
								}
								else if (Var->Type == GV_INT64)
								{
									r = Var->CastInt64() + 1;
								}
								else
								{
									r = Var->CastInt32() + 1;
								}
								break;
							}
							case OpPreDec:
							{
								// Minus one
								if (Var->Type == GV_DOUBLE)
								{
									r = Var->CastDouble() - 1;
								}
								else if (Var->Type == GV_INT64)
								{
									r = Var->CastInt64() - 1;
								}
								else
								{
									r = Var->CastInt32() - 1;
								}
								break;
							}
							case OpNot:
							{
								// Logical not
								if (Var->Type == GV_DOUBLE)
								{
									r = !Var->CastDouble();
								}
								else if (Var->Type == GV_INT64)
								{
									r = !Var->CastInt64();
								}
								else
								{
									r = !Var->CastInt32();
								}
								break;
							}
							default:
							{
								LgiAssert(!"Not a valid prefix operator");
								break;
							}
						}

						if (Var->Type == GV_DOMREF)
						{
							Var->Value.Dom->SetValue(Var->Value.DomRef.Name, r);
						}
						else if (Var->User)
						{
							Args[LowestOp+1] = new GVariant(r);
						}
						else
						{
							*Var = r;
						}

						// Remove the prefix op for the array
						Args.DeleteAt(LowestOp, true);
					}
				}
				else if (Type == OpPostfix)
				{
					if (LowestOp == 0)
					{
						Term.Print("%s:%i - Postfix operator preceded by nothing.\n", ErrorTitle(), Lines[StartToken]);
						break;
					}
					else
					{
						GVariant *Op = Args[LowestOp];
						GVariant *Var = Args[LowestOp - 1];
						GVariant r;

						switch (Op->Value.Op)
						{
							case OpPostInc:
							{
								// Add one
								if (Var->Type == GV_DOUBLE)
								{
									r = Var->CastDouble() + 1;
								}
								else if (Var->Type == GV_INT64)
								{
									r = Var->CastInt64() + 1;
								}
								else
								{
									r = Var->CastInt32() + 1;
								}
								break;
							}
							case OpPostDec:
							{
								// Minus one
								if (Var->Type == GV_DOUBLE)
								{
									r = Var->CastDouble() - 1;
								}
								else if (Var->Type == GV_INT64)
								{
									r = Var->CastInt64() - 1;
								}
								else
								{
									r = Var->CastInt32() - 1;
								}
								break;
							}
							default:
							{
								LgiAssert(!"Not a valid postfix operator");
								Term.Print("%s:%i - Not a valid postfix operator.\n", ErrorTitle(), Lines[StartToken]);
								break;
							}
						}

						if (Var->Type == GV_DOMREF)
						{
							Var->Value.Dom->SetValue(Var->Value.DomRef.Name, r);
						}
						else if (Var->User)
						{
							Args[LowestOp-1] = new GVariant(r);
						}
						else
						{
							*Var = r;
						}

						// Remove the postfix op for the array
						Args.DeleteAt(LowestOp, true);
					}
				}
				else if (Type == OpInfix)
				{
					if (LowestOp >= (Args.Length() - 1) ||
						LowestOp <= 0)
					{
						Term.Print("%s:%i - Infix operator missing argument.\n", ErrorTitle(), Lines[StartToken]);
						Args.Length(0);
						break;
					}

					GVariant *a = Args[LowestOp - 1];
					GVariant *op = Args[LowestOp];
					GVariant *b = Args[LowestOp + 1];
					GVariant r;

					LgiAssert(op->Value.Op);

					switch (op->Value.Op)
					{
						case OpPlus:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() + b->CastDouble();
							}
							else if (a->Type == GV_STRING || b->Type == GV_STRING)
							{
								GStringPipe p;
								
								#define ToStr(v) \
									if (v->Type == GV_DOM) p.Print("%p", v->Value.Dom); \
									else if (v->Type == GV_VOID_PTR) p.Print("%p", v->Value.Ptr); \
									else p.Print("%s", v->CastString());
								
								ToStr(a);
								ToStr(b);

								r.Type = GV_STRING;
								r.Value.String = p.NewStr();
							}
							else
							{
								r = a->CastInt32() + b->CastInt32();
							}
							break;
						}
						case OpMinus:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() - b->CastDouble();
							}
							else
							{
								r = a->CastInt32() - b->CastInt32();
							}
							break;
						}
						case OpMul:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() * b->CastDouble();
							}
							else
							{
								r = a->CastInt32() * b->CastInt32();
							}
							break;
						}
						case OpMod:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = fmod(a->CastDouble(), b->CastDouble());
							}
							else
							{
								r = a->CastInt32() % b->CastInt32();
							}
							break;
						}
						case OpDiv:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								double Denom = b->CastDouble();
								if (Denom != 0.0)
								{
									r = a->CastDouble() / Denom;
								}
							}
							else
							{
								int Denom = b->CastInt32();
								if (Denom)
								{
									r = a->CastInt32() / Denom;
								}
							}
							break;
						}
						case OpLessThan:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() < b->CastDouble();
							}
							else
							{
								r = a->CastInt32() < b->CastInt32();
							}
							break;
						}
						case OpGreaterThan:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() > b->CastDouble();
							}
							else
							{
								r = a->CastInt32() > b->CastInt32();
							}
							break;
						}
						case OpLessThanEqual:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() <= b->CastDouble();
							}
							else
							{
								r = a->CastInt32() <= b->CastInt32();
							}
							break;
						}
						case OpGreaterThanEqual:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() >= b->CastDouble();
							}
							else
							{
								r = a->CastInt32() >= b->CastInt32();
							}
							break;
						}
						case OpEquals:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() == b->CastDouble();
							}
							else
							{
								r = a->CastInt32() == b->CastInt32();
							}
							break;
						}
						case OpNotEquals:
						{
							if (a->Type == GV_DOUBLE || b->Type == GV_DOUBLE)
							{
								r = a->CastDouble() != b->CastDouble();
							}
							else
							{
								r = a->CastInt32() != b->CastInt32();
							}
							break;
						}
						case OpAnd:
						{
							r = a->CastInt32() && b->CastInt32();
							break;
						}
						case OpOr:
						{
							r = a->CastInt32() || b->CastInt32();
							break;
						}
						default:
						{
							Term.Print("%s:%i - Unhandled operator '%i'\n", ErrorTitle(), Lines[StartToken], op->Value.Op);
							break;
						}
					}

					if (a->Type == GV_DOMREF)
					{
						a->Value.Dom->SetValue(a->Value.DomRef.Name, r);
					}
					else if (a->User)
					{
						Args[LowestOp-1] = new GVariant(r);
					}
					else
					{
						*a = r;
					}

					Args.DeleteAt(LowestOp+1, true);
					Args.DeleteAt(LowestOp, true);
				}
				else LgiAssert(!"Unknown operator type");
			}
		}

		if (Args.Length() == 1)
		{
			GVariant *v = Args[0];
			return v->User ? new GVariant(*v) : v;
		}
	}
	else
	{
		Term.Print("%s:%i - Unexpected end of file.\n", __FILE__, __LINE__);
	}

	return 0;
}

int GScriptEnginePrivate::FindEndOfBlock(int Cur)
{
	int Depth = 1;
	while (Cur < Tokens.Length())
	{
		char16 *t = Tokens[Cur];
		if (StricmpW(t, sEndSqBracket) == 0)
		{
			Depth++;
		}
		else if (StricmpW(t, sEndSqBracket) == 0)
		{
			Depth--;
			if (Depth == 0)
			{
				return Cur;
				break;
			}
		}
		Cur++;
	}

	return -1;
}

bool GScriptEnginePrivate::Type(int &Cur, GExternType &Type)
{
	char16 *t = Tokens[Cur];
	if (t)
	{
		if (StricmpW(t, sInt) == 0 ||
			StricmpW(t, sUInt) == 0 ||
			StricmpW(t, sUInt) == 0 ||
			StricmpW(t, sUInt32) == 0 ||
			StrcmpW(t, sHWND) == 0 ||
			StrcmpW(t, sDWORD) == 0)
		{
			Type.Simple = GV_INT32;
		}
		else if (StricmpW(t, sInt64) == 0)
		{
			Type.Simple = GV_INT64;
		}
		else if (StricmpW(t, sLPCTSTR) == 0 ||
				 StricmpW(t, sLPTSTR) == 0)
		{
			Type.Simple = GV_STRING;
		}
		else if (StricmpW(t, sChar) == 0)
		{
			t = Tokens[Cur + 1];
			if (t && t[0] == '*' && !t[1])
			{
				Cur++;
				Type.Simple = GV_STRING;
			}
		}
		else
		{
			return false;
		}

		Cur++;
		return true;
	}
	
	return false;
}

GExecutionStatus GExternFunc::Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &In)
{
	if (!Priv || !Library || !Method)
		return ScriptError;

	GExecutionStatus Status = ScriptError;
	GArray<uint32> ArgVal;
	GArray<char*> Mem;

	for (int i=0; i<In.Length(); i++)
	{
		/*
		int Cur = (*Idx)[i];
		char16 *t1 = Priv->Tokens[Cur];
		char16 *t2 = Priv->Tokens[Cur + 1];
		GVariant *v = t1 && isalpha(*t1) && t2 && (*t2 == ',' || *t2 == ')') ? Priv->Var(t1, false) : 0;
		*/

		GVariant *v = In[i];
		if (v)
		{
			switch (Args[i]->Simple)
			{
				case GV_INT32:
				{
					ArgVal[i] = v->CastInt32();
					break;
				}
				case GV_INT64:
				{
					ArgVal[i] = v->CastInt32();
					break;
				}
				case GV_STRING:
				{
					if (v->Str())
					{
						ArgVal[i] = (NativeInt) v->Str();
					}
					else
					{
						ArgVal[i] = (NativeInt) v->CastString();
					}
					break;
				}
				default:
				{
					LgiAssert(0);
					break;
				}
			}
		}
	}

	if (ArgVal.Length() == Args.Length())
	{
		GLibrary Lib(Library);
		if (Lib.IsLoaded())
		{
			void *c = Lib.GetAddress(Method);
			if (c)
			{
				#if defined(_MSC_VER)
				uint32 a = ArgVal.Length();
				NativeInt b = (NativeInt) &ArgVal[Args.Length()-1];
				#endif
				uint32 r = 0;

				#if defined(_MSC_VER) && !defined(_WIN64)
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
				#else
				// Not implemented
				LgiAssert(0);
				#endif

				if (Ret) (*Ret) = (int) r;
				Status = ScriptSuccess;
			}
			else if (Priv)
			{
				Priv->Term.Print("%s - Extern library '%s' missing function '%s'.\n",
					Priv->ErrorTitle(),
					Library,
					Method);
			}
		}
		else if (Priv)
		{
			Priv->Term.Print("%s - Extern library '%s' missing.\n", Priv->ErrorTitle(), Library);
		}
	}
	else if (Priv)
	{
		Priv->Term.Print("%s - Extern '%s' expecting %i arguments, not %i given.\n",
			Priv->ErrorTitle(),
			Method,
			Args.Length(),
			ArgVal.Length());
	}

	Mem.DeleteArrays();

	return Status;
}

GExecutionStatus GScriptFunc::Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Values)
{
	if (!BodyArr || BodyIdx < 0 || !Priv)
		return ScriptError;

	GExecutionStatus Status = ScriptError;

	// Process the arguments into values...
	if (Args.Length() == Values.Length())
	{
		// Set up new stack frame...
		GVariant *n;
		StackFrame *sf = new StackFrame;
		if (!sf)
			return ScriptError;

		// Add to the stack
		Priv->Stack.Add(sf);

		// Set copies of all the arguments as local variables
		for (int i=0; i<Args.Length(); i++)
		{
			if ((n = new GVariant))
			{
				*n = *(Values[i]);
				sf->Add(Args[i], (void*)n);
			}
		}

		// Call the method
		LgiAssert(BodyIdx < BodyArr->Length());
		GCode *Body = &(*BodyArr)[BodyIdx];
		LgiAssert(Body->Type == GS_FUNCTION_DEFN);
		Status = Priv->Execute_Statement(Body->Code);

		// Remove the local stack frame
		Priv->Stack.Delete(sf);
		DeleteObj(sf);
	}
	else
	{
		Priv->Term.Print("%s - Script function '%s' was expecting %i arguments, not the %i given.\n",
			Priv->ErrorTitle(),
			Method,
			Args.Length(),
			Values.Length());
	}

	return Status;
}

void GScriptEnginePrivate::ProcessArguments(ArgumentArray &Args, ArgumentArray &Mem, GArray<int> *Idx)
{
	if (Idx)
	{
		for (int i=0; i<Idx->Length(); i++)
		{
			int Cur = (*Idx)[i];
			char16 *Tok = ThisToken();
			char16 *Next = NextToken();
			GVariant *v = 0;
			if (Tok && Next && isalpha(*Tok) && (*Next == ')' || *Next == ','))
			{
				v = Var(Tok, false);
			}
			if (v)
			{
				Args[i] = v;
			}
			else
			{
				GVariant *v = Execute_Expression(Cur);
				if (!v)
				{
					v = new GVariant;
				}
				if (v)
				{
					Args[i] = v;
					Mem.Add(v);
				}
			}
		}
	}
}

GExecutionStatus GScriptEnginePrivate::Execute_Statement(GArray<GCode> &To)
{
	GExecutionStatus Status = ScriptError;

	for (int k=0; k<To.Length(); k++)
	{
		GCode &Code = To[k];
		switch (Code.Type)
		{
			case GS_IF:
			{
				if (!Code.If.Blocks)
				{
					LgiAssert(!"No blocks in if statement.");
					break;
				}

				GCode::GConditionBlock *Block = 0;
				for (int Blk = 0; Blk < Code.If.Blocks->Length(); Blk++)
				{
					GCode::GConditionBlock &b = (*Code.If.Blocks)[Blk];
					
					int Cur = b.Condition;
					if (!Cur)
					{
						Block = &b;
						break;
					}

					GVariant *e = Execute_Expression(Cur);
					if (e)
					{
						bool IsTrue;
						switch (e->Type)
						{
							case GV_VOID_PTR:
								IsTrue = e->Value.Ptr != 0;
								break;
							case GV_DOM:
								IsTrue = e->Value.Dom != 0;
								break;
							case GV_STRING:
								IsTrue = e->Value.String != 0;
								break;
							default:
								IsTrue = e->CastInt32();
								break;
						}

						DeleteObj(e);
						if (IsTrue)
						{
							Block = &b;
							break;
						}
					}
					else
					{
						Term.Print("%s:%i - Couldn't evaluate if statement's condition.\n", ErrorTitle(), Lines[Code.Start]);
					}
				}

				if (Block)
				{
					Status = Execute_Statement(Block->Block);
				}
				else Status = ScriptSuccess;
				break;
			}
			case GS_FUNCTION_DEFN:
			{
				// Do nothing here...
				break;
			}
			case GS_FUNCTION_CALL:
			{
				if (!Code.MethodCall.Method)
					break;

				GVariant Ret;
				Status = Call(Code.MethodCall.Method, &Ret, Code.MethodCall.Args);
				if (!Status)
				{
					Term.Print("%s:%i - Command '%s' failed.\n", ErrorTitle(), Lines[Code.Start], Code.MethodCall.Method->Method);
				}
				break;
			}
			case GS_FOR:
			{
				if (Code.For.PreStatement)
				{
					if (!Execute_Statement(*Code.For.PreStatement))
					{
						Term.Print("%s:%i - Failed to execute for loop's PreStatement.\n", ErrorTitle(), Lines[Code.Start]);
						break;
					}
				}

				while (true)
				{
					// Evaluate the condition
					int Cur = Code.For.Condition;
					GVariant *v = Execute_Expression(Cur);
					if (!v)
					{
						break;
					}

					bool Cond = v->CastInt32() != 0;
					DeleteObj(v);
					if (!Cond)
					{
						Status = ScriptSuccess;
						break;
					}

					// Execute the body
					Execute_Statement(Code.Code);

					// Execute the post statement
					if (Code.For.PostStatement)
					{
						if (!Execute_Statement(*Code.For.PostStatement))
						{
							Term.Print("%s:%i - Failed to execute for loop's PostStatement.\n", ErrorTitle(), Lines[Code.Start]);
							break;
						}
					}
				}
				break;
			}
			case GS_LVALUE:
			{
				GDomAddr Dom;
				int Cur = Code.Start;
				if (CreateDomAddr(Cur, Dom))
				{
					switch (Code.LValue.Op)
					{
						case OpAssign:
						{
							Cur = Code.LValue.Expression;
							GVariant *v = Execute_Expression(Cur);
							if (v)
							{
								GDomRef Ref;

								/*
								printf("Assigning to ");
								Dom.Print();
								printf("\n");
								*/

								if (ResolveDomAddr(Ref, Dom, true))
								{
									if (Ref.Member)
									{
										if (Ref.Var->Type == GV_DOM &&
											Ref.Var->Value.Dom)
										{
											Ref.Var->Value.Dom->SetValue(Ref.Member, *v);
										}
										else if (Ref.Var->Type == GV_STRING ||
												Ref.Var->Type == GV_NULL)
										{
											if (stricmp(Ref.Member, "length") == 0)
											{
												Ref.Var->Empty();

												int Len = v->CastInt32();
												char *Mem = new char[Len+1];
												if (Mem)
												{
													memset(Mem, ' ', Len);
													Mem[Len] = 0;
													Ref.Var->Value.String = Mem;
													Ref.Var->Type = GV_STRING;
												}
											}
										}
									}
									else
									{
										if (Ref.Array)
										{
											Ref.Set(this, *v);
										}
										else
										{
											// Pure assignment..
											*Ref.Var = *v;
										}
									}

									Status = ScriptSuccess;
								}
								DeleteObj(v);
							}
							else
							{
								Term.Print("%s:%i - Failed to handle expression after assignment.\n", ErrorTitle(), Lines[Code.Start]);
							}
							break;
						}
						case OpPostInc:
						{
							GVariant *v = Var(Dom[0]);
							if (v)
							{
								*v = v->CastInt32() + 1;
								Status = ScriptSuccess;
							}
							break;
						}
						case OpPostDec:
						{
							GVariant *v = Var(Dom[0]);
							if (v)
							{
								*v = v->CastInt32() - 1;
								Status = ScriptSuccess;
							}
							break;
						}
						case OpPlusEquals:
						{
							Cur = Code.LValue.Expression;
							GVariant *v = Execute_Expression(Cur);
							if (v)
							{
								GDomRef Ref;
								if (ResolveDomAddr(Ref, Dom, true))
								{
									if (Ref.Member)
									{
										if (Ref.Var->Type == GV_DOM &&
											Ref.Var->Value.Dom)
										{
											GVariant a;
											if (Ref.Var->Value.Dom->GetValue(Ref.Member, a))
											{
												GVariant b = a.CastInt32() + v->CastInt32();
												Ref.Var->Value.Dom->SetValue(Ref.Member, b);
											}
										}
										else if (Ref.Var->Type == GV_STRING ||
												Ref.Var->Type == GV_NULL)
										{
											LgiAssert(0);

											/*
											if (stricmp(Ref.Member, "length") == 0)
											{
												Ref.Var->Empty();

												int Len = v->CastInt32();
												char *Mem = new char[Len+1];
												if (Mem)
												{
													memset(Mem, ' ', Len);
													Mem[Len] = 0;
													Ref.Var->Value.String = Mem;
													Ref.Var->Type = GV_STRING;
												}
											}
											*/
										}
									}
									else
									{
										*Ref.Var = Ref.Var->CastInt32() + v->CastInt32();
									}

									Status = ScriptSuccess;
								}
								DeleteObj(v);
							}
							else
							{
								Term.Print("%s:%i - Failed to handle expression after assignment.\n", ErrorTitle(), Lines[Code.Start]);
							}
							break;
						}
						default:
						{
							LgiAssert(0);
							break;
						}
					}
				}
				break;
			}
			default:
			{
				LgiAssert(0);
				return ScriptError;
				break;
			}
		}
	}

	return ScriptSuccess;
}

bool GScriptEnginePrivate::Compile_Statement(GArray<GCode> &To, int &Cur)
{
	bool Status = false;

	int StartToken = Cur;
	char16 *t = Cur < Tokens.Length() ? Tokens[Cur++] : 0;
	if (t)
	{
		if (StricmpW(t, sIf) == 0)
		{
			GCode &Code = To[To.Length()];
			Code.Type = GS_IF;
			Code.Start = Cur - 1;
			Code.If.Blocks = new GCode::GCondBlockArr;

			ParseNextIfStatement:
			if (!Require(Cur, "("))
			{
				Term.Print("%s:%i - Missing '(' after 'if'.\n", ErrorTitle(), Lines[StartToken]);
			}
			else
			{
				GCode::GConditionBlock &b = Code.If.Blocks->New();
				b.Condition = Cur;
				if (!Compile_Expression(Cur))
				{
					Term.Print("%s:%i - Couldn't compile if statement's condition expression.\n", ErrorTitle(), Lines[StartToken]);
				}
				else
				{
					if (!Require(Cur, ")"))
					{
						Term.Print("%s:%i - Missing ')' after if statement's expression.\n", ErrorTitle(), Lines[Cur]);
					}
					else
					{
						if (!Require(Cur, "{"))
						{
							Term.Print("%s:%i - Missing start block '{' in if statement.\n", ErrorTitle(), Lines[Cur]);
						}
						else
						{
							while ((t = ThisToken()))
							{
								if (StricmpW(t, sEndCurlyBracket) == 0)
								{
									Status = true;
									Cur++;
									break;
								}
								else
								{
									if (!Compile_Statement(b.Block, Cur))
									{
										break;
									}
								}
							}

							if (Status &&
								(t = ThisToken()) != 0 &&
								StricmpW(t, sElse) == 0)
							{
								Cur++;

								// There are only 2 things that can follow an 'else' keyword,
								// either the start of a block '{' or another 'if' keyword.
								if (!(t = ThisToken()))
								{
									Term.Print("%s:%i - Missing token after 'else'.\n", ErrorTitle(), Lines[Cur-1]);
								}
								else if (!StricmpW(t, sIf))
								{
									// It's an 'else if' statement
									Cur++;
									goto ParseNextIfStatement;
								}
								else if (!StricmpW(t, sStartCurlyBracket))
								{
									// It's an 'else' block
									if (!Require(Cur, "{"))
									{
										Term.Print("%s:%i - Missing start block '{' after else statement.\n", ErrorTitle(), Lines[Cur]);
									}
									else
									{
										GCode::GConditionBlock &b = Code.If.Blocks->New();
										b.Condition = 0;

										while ((t = ThisToken()))
										{
											if (StricmpW(t, sEndCurlyBracket) == 0)
											{
												Status = true;
												Cur++;
												break;
											}
											else
											{
												if (!Compile_Statement(b.Block, Cur))
												{
													break;
												}
											}
										}
									}
								}
								else
								{
									Term.Print("%s:%i - Error after 'else'.\n", ErrorTitle(), Lines[Cur-1]);
								}
							}
						}
					}
				}
			}
		}
		else if (StricmpW(t, sFunction) == 0)
		{
			GCode &Code = To[To.Length()];
			Code.Type = GS_FUNCTION_DEFN;
			Code.Start = Cur - 1;
			if ((t = ThisToken()))
			{
				GScriptFunc *Func = 0;
				if (!isalpha(*t))
				{
					Term.Print("%s:%i - Function name must start with a letter.\n", ErrorTitle(), Lines[Cur]);
					goto EndFuncDefn;
				}

				Func = new GScriptFunc(this);
				if (Func)
				{
					Func->Method = LgiNewUtf16To8(t);
					Func->BodyArr = &To;
					Func->BodyIdx = To.Length() - 1;
					Methods.Add(Func->Method, (GFunc*)Func);

					if (!Require(++Cur, "("))
					{
						Term.Print("%s:%i - Missing '(' in funcion defn argument list.\n", ErrorTitle(), Lines[Cur]);
						goto EndFuncDefn;
					}

					// Read any arguments
					while ((t = ThisToken()))
					{
						if (isalpha(*t))
						{
							Func->Args.Add(LgiNewUtf16To8(t));
						}
						else if (StricmpW(t, sComma) == 0)
						{
							// Skip
						}
						else if (StricmpW(t, sEndRdBracket) == 0)
						{
							break;
						}
						else
						{
							char *t8 = LgiNewUtf16To8(t);
							Term.Print("%s:%i - Unexpected token '%s' in argument defn.\n", ErrorTitle(), Lines[Cur], t8);
							DeleteArray(t8);
						}
						Cur++;
					}

					if (!Require(Cur, ")"))
					{
						Term.Print("%s:%i - Missing ')' in funcion defn argument list.\n", ErrorTitle(), Lines[Cur]);
						goto EndFuncDefn;
					}

					if (!Require(Cur, "{"))
					{
						Term.Print("%s:%i - Expecting '{' after function name.\n", ErrorTitle(), Lines[Cur]);
						goto EndFuncDefn;
					}

					Status = true;
					do
					{
						t = ThisToken();
						if (*t != '}')
						{
							if (!Compile_Statement(Code.Code, Cur))
							{
								Status = false;
								break;
							}
						}
					}
					while (StricmpW(t, sEndCurlyBracket) != 0);

					if (Require(Cur, "}"))
					{
						Func = 0;
					}
					else
					{
						Status = false;
						Term.Print("%s:%i - Expecting '}' after function body.\n", ErrorTitle(), Lines[Cur]);
					}
				}

				EndFuncDefn:
				if (Func)
				{
					Methods.Delete(Func->Method);
					DeleteObj(Func);
				}
			}
			else
			{
				Term.Print("%s:%i - Expecting function name.\n", ErrorTitle(), Lines[Cur-1]);
			}						
		}
		else if (StricmpW(t, sExtern) == 0)
		{
			GExternFunc *Func = new GExternFunc(this);
			if (Func)
			{
				// Parse the return type
				if (Type(Cur, Func->Return))
				{
					char16 *t = Tokens[Cur];
					if (t && isalpha(*t))
					{
						// Store the method name
						Func->Method = LgiNewUtf16To8(t);
						Cur++;

						if (!Require(Cur, "("))
						{
							Term.Print("%s:%i - Expecting '('.\n", ErrorTitle(), Lines[Cur]);
						}
						else
						{
							// Parse arguments to the func
							while (true)
							{
								GExternType *k = new GExternType;
								if (k)
								{
									if (Type(Cur, *k))
									{
										Func->Args.Add(k);
									}
									else
									{
										Term.Print("%s:%i - Expecting type.\n", ErrorTitle(), Lines[Cur]);
										break;
									}

									t = Tokens[Cur];
									if (t && *t == ',')
									{
										Cur++;
									}
									else break;										
								}
								else break;
							}

							if (!Require(Cur, ")"))
							{
								Term.Print("%s:%i - Expecting ')'.\n", ErrorTitle(), Lines[Cur]);
							}
							else if (!Require(Cur, "in"))
							{
								Term.Print("%s:%i - Expecting 'in'.\n", ErrorTitle(), Lines[Cur]);
							}
							else
							{
								t = Tokens[Cur];
								if (t)
								{
									Cur++;

									GVariant v;
									ToVar(v, t);
									Func->Library = NewStr(v.Str());

									if (!Require(Cur, ";"))
									{
										Term.Print("%s:%i - Expecting ';'.\n", ErrorTitle(), Lines[Cur]);
									}
									else
									{
										Methods.Add(Func->Method, (GFunc*)Func);
										Status = true;
										Func = 0;
									}
								}
							}
						}
					}
					else
					{
						Term.Print("%s:%i - Expecting method name.\n", ErrorTitle(), Lines[Cur]);
					}
				}

				DeleteObj(Func);
			}
		}
		else if (StricmpW(t, sFor) == 0)
		{
			GCode &Code = To[To.Length()];
			Code.Type = GS_FOR;
			Code.Start = Cur - 1;

			// For loop
			if (!Require(Cur, "("))
			{
				Term.Print("%s:%i - Missing '(' after 'for'.\n", ErrorTitle(), Lines[StartToken]);
			}
			else
			{
				// Execute the initial statement
				if ((Code.For.PreStatement = new GArray<GCode>) != NULL &&
					Compile_Statement(*Code.For.PreStatement, Cur))
				{
					// Store the start position of the condition tokens
					Code.For.Condition = Cur;

					// Compile the condition
					Compile_Expression(Cur);

					if (Require(Cur, ";"))
					{
						// Skip over the post statement
						if ((Code.For.PostStatement = new GArray<GCode>) != NULL &&
							Compile_Statement(*Code.For.PostStatement, Cur))
						{
							// See the start of the block...
							if (Require(Cur, "{"))
							{
								// Execute content
								while ((t = Tokens[Cur]))
								{
									if (StricmpW(t, sEndCurlyBracket) == 0)
									{
										Status = true;
										Cur++;
										break;
									}
									else if (!Compile_Statement(Code.Code, Cur))
									{
										break;
									}
								}
							}
							else
							{
								Term.Print("%s:%i - Missing '{'.\n", ErrorTitle(), Lines[Cur]);
							}
						}
						else
						{
							Term.Print("%s:%i - Failed to compile post statement.\n", ErrorTitle(), Lines[StartToken]);
						}
					}
					else
					{
						Term.Print("%s:%i - Missing ';' after expression.\n", ErrorTitle(), Lines[StartToken]);
					}
				}
			}
		}
		else
		{
			// See if this token is a method
			char *t8 = LgiNewUtf16To8(t);
			GFunc *Func = (GFunc*) Methods.Find(t8);
			if (Func)
			{
				GCode &Code = To[To.Length()];
				Code.Type = GS_FUNCTION_CALL;
				Code.Start = Cur - 1;
				Code.MethodCall.Args = 0;
				Code.MethodCall.Method = Func;

				if (!Require(Cur, "("))
				{
					Term.Print("%s:%i - Missing beginning '(' function call.\n", ErrorTitle(), Lines[Cur]);
				}
				else
				{
					// Parse through any arguments.
					while (true)
					{
						int Start = Cur;
						if (Compile_Expression(Cur))
						{
							if (!Code.MethodCall.Args)
							{
								Code.MethodCall.Args = new GArray<int>;
							}
							if (Code.MethodCall.Args)
							{
								Code.MethodCall.Args->Add(Start);
							}
						}
						t = Tokens[Cur];
						if (t)
						{
							if (*t == ',')
							{
								Cur++;
							}
							else if (*t == ')')
							{
								Cur++;
								break;
							}
							else
							{
								GBase u;
								u.NameW(t);
								Term.Print("%s:%i - Not expecting token '%s' in argument list.\n",
									ErrorTitle(),
									Lines[Start], u.Name());
								break;
							}
						}
						else break;
					}

					if (Require(Cur, ";"))
					{
						Status = true;
					}
					else
					{
						Term.Print("%s:%i - Missing ';' after function call.\n", ErrorTitle(), Lines[StartToken]);
					}
				}
			}
			else
			{
				// Not an external method, is it an L-value?
				GDomAddr Dom;
				Cur--;
				if (CreateDomAddr(Cur, Dom))
				{
					// Look at operator
					char16 *n = ThisToken();
					if (n)
					{
						GOperator Op = IsOp(n, 0);
						switch (Op)
						{
							case OpPlusEquals:
							case OpAssign:
							case OpMinusEquals:
							case OpMulEquals:
							case OpDivEquals:
							{
								GCode &Code = To[To.Length()];
								Code.Type = GS_LVALUE;
								Code.Start = StartToken;
								Code.LValue.Op = Op;
								Code.LValue.Expression = ++Cur;

								if (Compile_Expression(Cur))
								{
									t = ThisToken();
									if (t)
									{
										if (StricmpW(t, sSemiColon) == 0)
										{
											Cur++;
											Status = true;
										}
										else if (StricmpW(t, sEndRdBracket) == 0)
										{
											Cur++;
											Status = true;
										}
									}
								}
								else
								{
									Term.Print("%s:%i - Failed to compile expression after assignment.\n", ErrorTitle(), Lines[StartToken]);
								}
								break;
							}
							case OpPostInc:
							case OpPostDec:
							{
								GCode &Code = To[To.Length()];
								Code.Type = GS_LVALUE;
								Code.Start = StartToken;
								Code.LValue.Op = Op;
								Code.LValue.Expression = ++Cur;
								
								t = ThisToken();
								if (t)
								{
									if (StricmpW(t, sSemiColon) == 0)
									{
										Cur++;
										Status = true;
									}
									else if (StricmpW(t, sEndRdBracket) == 0)
									{
										Cur++;
										Status = true;
									}
								}
								break;
							}
							default:
							{
								Term.Print("%s:%i - '%S' isn't an operator.\n", ErrorTitle(), Lines[StartToken], n);
								break;
							}
						}
					}
				}
			}

			DeleteArray(t8);
		}
	}
	else
	{
		Term.Print("%s:%i - Unexpected end of file in statement.\n", __FILE__, __LINE__);
	}

	return Status;
}


