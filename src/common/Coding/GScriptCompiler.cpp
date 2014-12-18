/// \file
#include "Lgi.h"
#include "GScripting.h"
#include "GScriptingPriv.h"
#include "GLexCpp.h"
#include "GString.h"

#define GetTok(c) ((c) < Tokens.Length() ? Tokens[c] : NULL)

int GFunctionInfo::_Infos = 0;

enum GExpTokens
{
	NoToken,
	StartRdBracket,
	EndRdBracket,
	Comma,
	SemiColon,
	StartSqBracket,
	EndSqBracket,
	ConstTrue,
	ConstFalse,
	ConstNull,
};

struct LinkFixup
{
	int Tok;
	int Offset;
	int Args;
	GFunctionInfo *Func;
};

struct Node
{
	typedef GArray<Node> NodeExp;
	struct VariablePart
	{
		GVariant Name;
		NodeExp Array;
		bool Call;
		GArray<NodeExp*> Args;
		
		VariablePart()
		{
			Call = false;
		}
		
		~VariablePart()
		{
			Args.DeleteObjects();
		}
	};

	// Heirarchy
	NodeExp Child;

	// One of the following are valid:
	GOperator Op;
	// -or-
	bool Constant;
	int Tok;
	GExpTokens ConstTok;
	// -or-
	GFunc *ContextFunc;
	GArray<NodeExp> Args;
	// -or-
	GFunctionInfo *ScriptFunc;
	// -or-
	GArray<VariablePart> Variable;
	
	// Used during building
	GVarRef Reg;
	GVarRef ArrayIdx;

	void Init()
	{
		Op = OpNull;
		ContextFunc = 0;
		ScriptFunc = 0;
		Constant = false;
		Tok = -1;
		ConstTok = NoToken;
		Reg.Empty();
		ArrayIdx.Empty();
	}

	void SetOp(GOperator o, int t)
	{
		Init();
		Op = o;
		Tok = t;
	}

	void SetConst(int t, GExpTokens e)
	{
		Init();
		Constant = true;
		Tok = t;
		ConstTok = e;
	}

	void SetContextFunction(GFunc *m, int tok)
	{
		Init();
		ContextFunc = m;
		Tok = tok;
	}

	void SetScriptFunction(GFunctionInfo *m, int tok)
	{
		Init();
		ScriptFunc = m;
		Tok = tok;
	}

	void SetVar(char16 *Var, int t)
	{
		Init();
		Variable[0].Name = Var;
		Tok = t;
	}

	bool IsVar() { return Variable.Length() > 0; }
	bool IsContextFunc() { return ContextFunc != 0; }
	bool IsScriptFunc() { return ScriptFunc != 0; }
	bool IsConst() { return Constant; }
};

GCompiledCode::GCompiledCode() : Globals(SCOPE_GLOBAL), Debug(0, true, -1, -1)
{
	SysContext = NULL;
	UserContext = NULL;
}

GCompiledCode::GCompiledCode(GCompiledCode &copy) : Globals(SCOPE_GLOBAL), Debug(0, true, -1, -1)
{
	*this = copy;
}

GCompiledCode::~GCompiledCode()
{
}

GCompiledCode &GCompiledCode::operator =(GCompiledCode &c)
{
	Globals = c.Globals;
	ByteCode = c.ByteCode;
	Types = c.Types;
	Debug = c.Debug;
	Methods = c.Methods;
	FileName.Reset(NewStr(c.FileName));

	return *this;
}

GFunctionInfo *GCompiledCode::GetMethod(const char *Name, bool Create)
{
	for (unsigned i=0; i<Methods.Length(); i++)
	{
		if (!strcmp(Methods[i]->GetName(), Name))
			return Methods[i];
	}

	if (Create)
	{
		GAutoRefPtr<GFunctionInfo> n(new GFunctionInfo(Name));
		if (n)
		{
			Methods.Add(n);
			return n;
		}
	}

	return 0;
}

int GCompiledCode::ObjectToSourceAddress(int ObjAddr)
{
	return Debug.Find(ObjAddr);
}

const char *GCompiledCode::AddrToSourceRef(int ObjAddr)
{
	static char Status[256];
	int Addr = ObjAddr;
	int LineNum = ObjectToSourceAddress(Addr);
	
	// Search for the start of the instruction...
	while (Addr > 0 && LineNum < 0)
		LineNum = ObjectToSourceAddress(--Addr);

	char *Dir = FileName ? strrchr(FileName, DIR_CHAR) : NULL;
	int PathLen = Dir ? Dir - FileName : 0;

	if (LineNum >= 0)
		sprintf_s(	Status, sizeof(Status),
					"%s:%i",
					FileName ? (PathLen > 24 ? Dir + 1 : FileName.Get()) : "(untitled)",
					LineNum);
	else
		sprintf_s(Status, sizeof(Status), "0x%x", ObjAddr);
	
	return Status;
}

GVariant *GCompiledCode::Set(const char *Name, GVariant &v)
{
	int i = Globals.Var(Name, true);
	if (i >= 0)
	{
		Globals[i] = v;
		return &Globals[i];
	}

	return 0;
}

template <class T>
void UnEscape(T *t)
{
	T *i = t, *o = t;
	while (*i)
	{
		if (*i == '\\')
		{
			i++;
			switch (*i)
			{
				case '\\':
					*o++ = '\\'; i++;
					break;
				case 'n':
					*o++ = '\n'; i++;
					break;
				case 'r':
					*o++ = '\r'; i++;
					break;
				case 't':
					*o++ = '\t'; i++;
					break;
				case 'b':
					*o++ = '\b'; i++;
					break;
			}
		}
		else *o++ = *i++;
	}

	*o++ = 0;
}

class TokenRanges
{
	struct Range
	{
		int Start;
		GArray<int> Lines;
		GVariant File;
	};

	GArray<Range> r;

public:
	void Empty()
	{
		r.Length(0);
	}

	int Length()
	{
		return r.Length();
	}

	/// Gets the file/line at a given token
	char *operator [](int Tok)
	{
		for (unsigned i=0; i<r.Length(); i++)
		{
			Range &n = r[i];
			if (Tok >= n.Start && Tok < n.Start + (int)n.Lines.Length())
			{
				static char fl[256];
				sprintf_s(fl, sizeof(fl), "%s:%i", n.File.Str(), n.Lines[Tok - n.Start]);
				return fl;
			}
		}
		return 0;
	}

	/// Add a file/line reference for the next token
	void Add(const char *FileName, int Line)
	{
		const char *Empty = "";
		const char *d = FileName ? strrchr((char*)FileName, DIR_CHAR) : 0;
		if (d) FileName = d + 1;

		if (!r.Length())
		{
			r[0].File = FileName;
			r[0].Start = 0;
			r[0].Lines.Add(Line);
		}
		else
		{
			int Last = r.Length()-1;
			char *LastFile = r[Last].File.Str();
			if (_stricmp(LastFile?LastFile:Empty, FileName?FileName:Empty) != 0)
			{
				// File changed...
				Range &n = r.New();
				n.File = FileName;
				n.Start = r[Last].Start + r[Last].Lines.Length();
				n.Lines.Add(Line);
			}
			else
			{
				// Same file...
				r[Last].Lines.Add(Line);
			}
		}
	}
};

/// Scripting language compiler implementation
class GCompilerPriv :
	public GCompileTools,
	public GScriptUtils
{
	GHashTbl<const char*, GVariantType> Types;

public:
	GScriptContext *Ctx;
	GCompiledCode *Code;
	GStream *Log;
	GArray<char16*> Tokens;	
	TokenRanges Lines;	
	char16 *Script;
	GHashTbl<char*, GFunc*> Methods;
	int Regs;
	GArray<GVariables*> Scopes;
	GArray<LinkFixup> Fixups;
	GHashTbl<char16*, char16*> Defines;
	GHashTbl<char16*, GExpTokens> ExpTok;

	#ifdef _DEBUG
	GArray<GVariant> RegAllocators;
	#endif

	GCompilerPriv() : ExpTok(0, false)
	{
		Ctx = 0;
		Code = 0;
		Log = 0;
		Script = 0;
		Regs = 0;
		
		ExpTok.Add(sStartRdBracket, StartRdBracket);
		ExpTok.Add(sEndRdBracket, EndRdBracket);
		ExpTok.Add(sComma, Comma);
		ExpTok.Add(sSemiColon, SemiColon);
		ExpTok.Add(sStartSqBracket, StartSqBracket);
		ExpTok.Add(sEndSqBracket, EndSqBracket);
		ExpTok.Add(sTrue, ConstTrue);
		ExpTok.Add(sFalse, ConstFalse);
		ExpTok.Add(sNull, ConstNull);

		Types.Add("int32", GV_INT32);
		Types.Add("int", GV_INT32);
		Types.Add("int64", GV_INT64);
		Types.Add("bool", GV_BOOL);
		Types.Add("boolean", GV_BOOL);
		Types.Add("double", GV_DOUBLE);
		Types.Add("float", GV_DOUBLE);
		Types.Add("char", GV_STRING);
		Types.Add("GDom", GV_DOM);
		Types.Add("void", GV_VOID_PTR);
		Types.Add("GDateTime", GV_DATETIME);
		Types.Add("GHashTable", GV_HASHTABLE);
		Types.Add("GOperator", GV_OPERATOR);
		Types.Add("GView", GV_GVIEW);
		Types.Add("GMouse", GV_GMOUSE);
		Types.Add("GKey", GV_GKEY);
		// Types.Add("binary", GV_BINARY);
		// Types.Add("List", GV_LIST);
		// Types.Add("GDom&", GV_DOMREF);
	}

	~GCompilerPriv()
	{
		Empty();
	}

	void Empty()
	{
		Ctx = 0;
		DeleteObj(Code);
		Log = 0;
		Tokens.DeleteArrays();
		Lines.Empty();
		DeleteArray(Script);
		Defines.DeleteArrays();
	}

	bool OnError(int Tok, const char *Msg, ...)
	{
		char Buf[512];
		va_list Arg;
		va_start(Arg, Msg);
		#ifndef WIN32
		#define _vsnprintf vsnprintf
		#endif
		vsprintf_s(Buf, sizeof(Buf), Msg, Arg);
		Log->Print("CompileError:%s - %s\n", Lines[Tok], Buf);
		va_end(Arg);

		return false;
	}

	void DebugInfo(int Tok)
	{
		char *Line = Lines[Tok];
		if (Line)
		{
			char *c = strrchr(Line, ':');
			if (c)
			{
				Code->Debug.Add(Code->ByteCode.Length(), ::atoi(c+1));
			}
		}
	}

	/// Assemble a zero argument instruction
	bool Asm0(int Tok, uint8 Op)
	{
		DebugInfo(Tok);

		int Len = Code->ByteCode.Length();
		if (Code->ByteCode.Length(Len + 1))
		{
			GPtr p;
			p.u8 = &Code->ByteCode[Len];
			*p.u8++ = Op;
		}
		else return false;

		return true;
	}

	/// Assemble one arg instruction
	bool Asm1(int Tok, uint8 Op, GVarRef a)
	{
		DebugInfo(Tok);

		int Len = Code->ByteCode.Length();
		if (Code->ByteCode.Length(Len + 5))
		{
			GPtr p;
			p.u8 = &Code->ByteCode[Len];
			*p.u8++ = Op;
			*p.r++ = a;
		}
		else return false;

		return true;
	}

	/// Assemble two arg instruction
	bool Asm2(int Tok, uint8 Op, GVarRef a, GVarRef b)
	{
		DebugInfo(Tok);

		int Len = Code->ByteCode.Length();
		if (Code->ByteCode.Length(Len + 9))
		{
			GPtr p;
			p.u8 = &Code->ByteCode[Len];
			*p.u8++ = Op;
			*p.r++ = a;
			*p.r++ = b;
		}
		else return false;

		return true;
	}

	/// Assemble three arg instruction
	bool Asm3(int Tok, uint8 Op, GVarRef a, GVarRef b, GVarRef c)
	{
		DebugInfo(Tok);

		int Len = Code->ByteCode.Length();
		if (Code->ByteCode.Length(Len + 1 + (sizeof(GVarRef) * 3) ))
		{
			GPtr p;
			p.u8 = &Code->ByteCode[Len];
			*p.u8++ = Op;
			*p.r++ = a;
			*p.r++ = b;
			*p.r++ = c;
		}
		else return false;

		return true;
	}

	/// Assemble four arg instruction
	bool Asm4(int Tok, uint8 Op, GVarRef a, GVarRef b, GVarRef c, GVarRef d)
	{
		DebugInfo(Tok);

		int Len = Code->ByteCode.Length();
		if (Code->ByteCode.Length(Len + 1 + (sizeof(GVarRef) * 4) ))
		{
			GPtr p;
			p.u8 = &Code->ByteCode[Len];
			*p.u8++ = Op;

			if (!a.Valid()) AllocNull(a);
			*p.r++ = a;
			if (!b.Valid()) AllocNull(b);
			*p.r++ = b;
			if (!c.Valid()) AllocNull(c);
			*p.r++ = c;
			if (!d.Valid()) AllocNull(d);
			*p.r++ = d;
		}
		else return false;

		return true;
	}

	/// Assemble 'n' length arg instruction
	bool AsmN(int Tok, uint8 Op, GArray<GVarRef> &Args)
	{
		DebugInfo(Tok);

		int Len = Code->ByteCode.Length();
		if (Code->ByteCode.Length(Len + 1 + (sizeof(GVarRef) * Args.Length()) ))
		{
			GPtr p;
			p.u8 = &Code->ByteCode[Len];
			*p.u8++ = Op;

			for (unsigned i=0; i<Args.Length(); i++)
			{
				if (!Args[i].Valid())
					AllocNull(Args[i]);
				*p.r++ = Args[i];
			}
		}
		else return false;

		return true;
	}

	/// Convert the source from one big string into an array of tokens
	bool Lex(char *Source, const char *FileName)
	{
		char16 *w = LgiNewUtf8To16(Source);
		if (w)
		{
			int Line = 1;
			char16 *s = w, *t, *prev = w;
			while ((t = LexCpp(s, LexStrdup, NULL, &Line)))
			{
				if (*t == '#')
				{
					int Len;
					if (!StrnicmpW(t + 1, sInclude, Len = StrlenW(sInclude)))
					{
						char16 *Raw = LexCpp(s, LexStrdup);
						char16 *File = TrimStrW(Raw, (char16*)L"\"\'");
						DeleteArray(Raw);
						if (File)
						{
							GVariant v;
							char *IncCode = 0;
							v.OwnStr(File);

							if ((IncCode = Ctx->GetIncludeFile(v.Str())))
							{
								Lex(IncCode, v.Str());
							}
							else
							{
								DeleteArray(t);
								return OnError(Lines.Length()-1, "Couldn't include '%s'", v.Str());
							}

							DeleteArray(IncCode);
						}
						else OnError(Tokens.Length(), "No file for #include.");
					}
					else if (!StrnicmpW(t + 1, sDefine, Len = StrlenW(sDefine)))
					{
						GAutoWString Name(LexCpp(s, LexStrdup));
						if (Name && IsAlpha(*Name))
						{
							Lines.Add(FileName, Line);
							
							char16 *Start = s;
							while (*Start && strchr(WhiteSpace, *Start))
								Start++;
							char16 *Eol = StrchrW(Start, '\n');
							if (!Eol)
								Eol = Start + StrlenW(Start);
							while (Eol > Start && strchr(WhiteSpace, Eol[-1]))
								Eol--;
							
							Defines.Add(Name, NewStrW(Start, Eol - Start));
							
							s = Eol;
						}
					}

					DeleteArray(t);
					continue;
				}

				char16 *DefineValue;
				if (IsAlpha(*t) && (DefineValue = Defines.Find(t)))
				{
					char16 *Def = DefineValue, *f;
					while ((f = LexCpp(Def, LexStrdup)))
					{
						Tokens.Add(f);
						Lines.Add(FileName, Line);
					}
					DeleteArray(t);
				}
				else
				{
					Tokens.Add(t);
					Lines.Add(FileName, Line);
				}
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

	/// Create a null var ref
	void AllocNull(GVarRef &r)
	{
		r.Scope = SCOPE_GLOBAL;

		if (Code->Globals.NullIndex < 0)
			Code->Globals.NullIndex = Code->Globals.Length();
		
		r.Index = Code->Globals.NullIndex;
		Code->Globals[r.Index].Type = GV_NULL;
	}

	/// Allocate a constant double
	void AllocConst(GVarRef &r, double d)
	{
		r.Scope = SCOPE_GLOBAL;
		r.Index = Code->Globals.Length();
		Code->Globals[r.Index] = d;
	}

	/// Allocate a constant bool
	void AllocConst(GVarRef &r, bool b)
	{
		r.Scope = SCOPE_GLOBAL;
		r.Index = Code->Globals.Length();
		Code->Globals[r.Index] = b;
	}

	/// Allocate a constant int
	void AllocConst(GVarRef &r, int i)
	{
		r.Scope = SCOPE_GLOBAL;

		if (Code->Globals.Length())
		{
			// Check for existing int
			GVariant *p = &Code->Globals[0];
			GVariant *e = p + Code->Globals.Length();
			while (p < e)
			{
				if (p->Type == GV_INT32 &&
					p->Value.Int == i)
				{
					r.Index = p - &Code->Globals[0];
					return;
				}
				p++;
			}
		}

		// Allocate new global
		r.Index = Code->Globals.Length();
		Code->Globals[r.Index] = i;
	}

	/// Allocate a constant string
	void AllocConst(GVarRef &r, char *s, int len = -1)
	{
		LgiAssert(s != 0);
		if (len < 0)
			len = strlen(s);
		
		r.Scope = SCOPE_GLOBAL;
		r.Index = Code->Globals.Length();

		for (unsigned i=0; i<Code->Globals.Length(); i++)
		{
			if (Code->Globals[i].Type == GV_STRING)
			{
				char *c = Code->Globals[i].Str();
				if (*s == *c && strcmp(s, c) == 0)
				{
					r.Index = i;
					return;
				}
			}
		}

		GVariant &v = Code->Globals[r.Index];
		v.Type = GV_STRING;
		if ((v.Value.String = NewStr(s, len)))
		{
			UnEscape<char>(v.Value.String);
		}
	}

	/// Allocate a constant wide string
	void AllocConst(GVarRef &r, char16 *s, int len)
	{
		LgiAssert(s != 0);

		char *utf = LgiNewUtf16To8(s, len * sizeof(char16));
		if (!utf)
			utf = NewStr("");

		r.Scope = SCOPE_GLOBAL;
		r.Index = Code->Globals.Length();

		for (unsigned i=0; i<Code->Globals.Length(); i++)
		{
			if (Code->Globals[i].Type == GV_STRING)
			{
				char *c = Code->Globals[i].Str();
				if (*utf == *c && strcmp(utf, c) == 0)
				{
					r.Index = i;
					DeleteArray(utf);
					return;
				}
			}
		}

		GVariant &v = Code->Globals[r.Index];
		v.Type = GV_STRING;
		if ((v.Value.String = utf))
		{
			UnEscape<char>(v.Value.String);
		}
	}

	/// Find a variable by name, creating it if needed
	GVarRef FindVariable(GVariant &Name, bool Create)
	{
		GVarRef r = {0, -1};

		// Look for existing variable...
		int i;
		for (i=Scopes.Length()-1; i>=0; i--)
		{
			r.Index = Scopes[i]->Var(Name.Str(), false);
			if (r.Index >= 0)
			{
				r.Scope = Scopes[i]->Scope;
				return r;
			}
		}

		// Create new variable on most recent scope
		i = Scopes.Length() - 1;
		r.Index = Scopes[i]->Var(Name.Str(), Create);
		if (r.Index >= 0)
		{
			r.Scope = Scopes[i]->Scope;
		}

		return r;
	}

	/// Build asm to assign a var ref
	bool AssignVarRef(Node &n, GVarRef &Value)
	{
		/*
		Examples and what their assembly should look like:

			a = Value

				Assign a <- Value

			a[b] = Value

				R0 = AsmExpression(b);
				ArraySet a[R0] <- Value

			a[b].c = Value

				R0 = AsmExpression(b);
				R0 = ArrayGet a[R0]
				DomSet(R0, "c", Null, Value)

			a[b].c[d]
				
				R0 = AsmExpression(b);
				R0 = ArrayGet a[R0];
				R1 = AsmExpression(d);
				R0 = DomGet(R0, "c", R1);
				
				ArraySet R0[R1] = Value

			a.b[c].d = Value

				R1 = AsmExpression(c);
				R0 = DomGet(a, "b", R1);
				DomSet(R1, "d", Null, Value)


			// resolve initial array
			if (parts > 1 && part[0].array)
			{
				// 
				cur = ArrayGet(part[0].var, part[0].array)
			}
			else
			{
				cur = part[0]
			}

			// dom loop over
			loop (p = 0 to parts - 2)
			{
				if (part[p+1].array)
					arr = exp(part[p+1].array)
				else
					arr = null
				
				cur = DomGet(cur, part[p+1].var, arr)
			}

			// final part
			if (part[parts-1].arr)
			{
				arr = exp(part[parts-1].arr)
				ArraySet(cur, arr)
			}
			else
			{
				DomSet(cur, part[parts-1].var, null, value)
			}		
		*/
		if (!n.IsVar())
			return false;

		// Gets the first part of the variable.
		GVarRef Cur = FindVariable(n.Variable[0].Name, true);
		if (Cur.Index < 0)
			return false;

		if (n.Variable.Length() > 1)
		{
			// Do any initial array dereference
			if (n.Variable[0].Array.Length())
			{
				// Assemble the array index's expression into 'Idx'
				GVarRef Idx;
				Idx.Empty();
				if (!AsmExpression(&Idx, n.Variable[0].Array))
					return OnError(n.Tok, "Error creating bytecode for array index.");

				GVarRef Dst = Cur;
				if (!Dst.IsReg())
				{
					AllocReg(Dst, _FL);
				}
				
				// Assemble array load instruction
				Asm3(n.Tok, IArrayGet, Dst, Cur, Idx);
				Cur = Dst;

				// Cleanup
				DeallocReg(Idx);
			}

			// Do all the DOM "get" instructions
			unsigned p;
			for (p=0; p<n.Variable.Length() - 2; p++)
			{
				GVarRef Idx;
				Idx.Empty();

				// Does the next part have an array deref?
				if (n.Variable[p+1].Array.Length())
				{
					// Evaluate the array indexing expression
					if (!AsmExpression(&Idx, n.Variable[p+1].Array))
					{
						return OnError(n.Tok, "Error creating bytecode for array index.");
					}
				}

				// Alloc constant string for the DOM get
				GVarRef DomName;
				AllocConst(DomName, n.Variable[p+1].Name.Str());

				// Move temp into register?
				if (Cur.Scope != SCOPE_REGISTER)
				{
					GVarRef Dest;
					AllocReg(Dest, _FL);
					Asm4(n.Tok, IDomGet, Dest, Cur, DomName, Idx);
					Cur = Dest;
				}
				else
				{
					// Assemble the DOM get instruction
					Asm4(n.Tok, IDomGet, Cur, Cur, DomName, Idx);
				}

				// Cleanup
				DeallocReg(Idx);
			}

			// Do final assignment
			GVarRef Idx;
			Idx.Empty();

			int Last = n.Variable.Length() - 1;
			if (n.Variable[Last].Array.Length())
			{
				// Evaluate the array indexing expression
				if (!AsmExpression(&Idx, n.Variable[Last].Array))
				{
					return OnError(n.Tok, "Error creating bytecode for array index.");
				}
			}

			// Alloc constant string for the DOM get
			GVarRef DomName;
			AllocConst(DomName, n.Variable[p+1].Name.Str());

			// Final instruction to set DOM value
			Asm4(n.Tok, IDomSet, Cur, DomName, Idx, Value);

			// Cleanup
			DeallocReg(Idx);
			DeallocReg(Cur);
		}
		else
		{
			// Look up the array index if any
			if (n.Variable[0].Array.Length())
			{
				// Assemble the array index's expression into 'Idx'
				GVarRef Idx;
				Idx.Empty();
				if (!AsmExpression(&Idx, n.Variable[0].Array))
					return OnError(n.Tok, "Error creating bytecode for array index.");

				// Assemble array store instruction
				Asm3(n.Tok, IArraySet, Cur, Idx, Value);

				// Cleanup
				DeallocReg(Idx);
			}
			else
			{
				// Non array based assignment
				Asm2(n.Tok, IAssign, Cur, Value);
			}
		}

		n.Reg = Value;
		return true;
	}

	/// Convert a token stream to a var ref
	bool TokenToVarRef(Node &n)
	{
		if (!n.Reg.Valid())
		{
			if (n.IsVar())
			{
				// Variable
				GVarRef v = FindVariable(n.Variable[0].Name, true);
				if (v.Index < 0)
					return false;

				n.Reg = v;

				// Does it have an array deref?
				if (n.Variable[0].Array.Length())
				{
					// Evaluate the array indexing expression
					if (!AsmExpression(&n.ArrayIdx, n.Variable[0].Array))
					{
						return OnError(n.Tok, "Error creating bytecode for array index.");
					}

					// Do we need to create code to load the value from the array?
					GVarRef Val;
					if (AllocReg(Val, _FL))
					{
						Asm3(n.Tok, IArrayGet, Val, n.Reg, n.ArrayIdx);
						n.Reg = Val;
					}
					else return OnError(n.Tok, "Error allocating register.");
				}

				// Load DOM parts...
				for (unsigned p=1; p<n.Variable.Length(); p++)
				{
					GVarRef Name, Arr;
					GArray<GVarRef> Args;
					Node::VariablePart &Part = n.Variable[p];
					
					char *nm = Part.Name.Str();
					AllocConst(Name, nm, strlen(nm));
					
					if (Part.Array.Length())
					{
						if (!AsmExpression(&Arr, Part.Array))
						{
							return OnError(n.Tok, "Can't assemble array expression.");
						}
					}
					else if (Part.Call)
					{
						for (unsigned i=0; i<Part.Args.Length(); i++)
						{
							GVarRef &a = Args.New();
							if (!AsmExpression(&a, *Part.Args[i]))
							{
								return OnError(n.Tok, "Can't assemble argument expression.");
							}
						}
					}
					else
					{
						AllocNull(Arr);
					}

					GVarRef Dst;
					if (n.Reg.IsReg())
					{
						Dst = n.Reg;
					}
					else
					{
						AllocReg(Dst, _FL);
					}
					
					if (Part.Call)
					{
						DebugInfo(n.Tok);

						GArray<GVarRef> Call;
						Call[0] = Dst;
						Call[1] = n.Reg;
						Call[2] = Name;
						
						// This must always be a global, the decompiler requires access
						// to the constant to know how many arguments to print. And it
						// can only see the globals.
						AllocConst(Call[3], (int)Args.Length());
						
						Call.Add(Args);
						AsmN(n.Tok, IDomCall, Call);
					}
					else
					{
						Asm4(n.Tok, IDomGet, Dst, n.Reg, Name, Arr);
					}
					
					n.Reg = Dst;
				}
			}
			else if (n.IsConst())
			{
				// Constant
				switch (n.ConstTok)
				{
					case ConstTrue:
					{
						AllocConst(n.Reg, true);
						break;
					}
					case ConstFalse:
					{
						AllocConst(n.Reg, false);
						break;
					}
					case ConstNull:
					{
						AllocNull(n.Reg);
						break;
					}
					default:
					{
						char16 *t = Tokens[n.Tok];
						if (*t == '\"' || *t == '\'')
						{
							// string
							int Len = StrlenW(t);
							AllocConst(n.Reg, t + 1, Len - 2);
						}
						else if (StrchrW(t, '.'))
						{
							// double
							AllocConst(n.Reg, atof(t));
						}
						else if (t[0] == '0' && tolower(t[1]) == 'x')
						{
							// hex integer
							AllocConst(n.Reg, htoi(t + 2));
						}
						else
						{
							// decimal integer
							AllocConst(n.Reg, atoi(t));
						}
						break;
					}
				}
			}
			else if (n.IsContextFunc())
			{
				// Method call, create byte codes to put func value into n.Reg
				if (AllocReg(n.Reg, _FL))
				{
					DebugInfo(n.Tok);

					GArray<GVarRef> a;
					for (unsigned i=0; i<n.Args.Length(); i++)
					{
						if (!AsmExpression(&a[i], n.Args[i]))
						{
							return OnError(n.Tok, "Error creating bytecode for context function argument.");
						}
					}

					int Len = Code->ByteCode.Length();
					int Size = 1 + sizeof(GFunc*) + sizeof(GVarRef) + 2 + (a.Length() * sizeof(GVarRef));
					Code->ByteCode.Length(Len + Size);
					GPtr p;
					uint8 *Start = &Code->ByteCode[Len];
					p.u8 = Start;
					*p.u8++ = ICallMethod;
					*p.fn++ = n.ContextFunc;
					*p.r++ = n.Reg;
					*p.u16++ = n.Args.Length();
					for (unsigned i=0; i<n.Args.Length(); i++)
					{
						*p.r++ = a[i];
					}
					LgiAssert(p.u8 == Start + Size);

					// Deallocate argument registers
					for (unsigned i=0; i<a.Length(); i++)
					{
						DeallocReg(a[i]);
					}
				}
				else return OnError(n.Tok, "Can't allocate register for method return value.");
			}
			else if (n.IsScriptFunc())
			{
				// Call to a script function, create byte code to call function
				if (AllocReg(n.Reg, _FL))
				{
					DebugInfo(n.Tok);

					GArray<GVarRef> a;
					for (unsigned i=0; i<n.Args.Length(); i++)
					{
						if (!AsmExpression(&a[i], n.Args[i]))
						{
							return OnError(n.Tok, "Error creating bytecode for script function argument.");
						}
					}

					int Len = Code->ByteCode.Length();
					int Size =	1 + // instruction
								sizeof(uint32) + // address of function
								sizeof(uint16) + // size of frame
								sizeof(GVarRef) + // return value
								2 + // number of args
								(a.Length() * sizeof(GVarRef)); // args

					Code->ByteCode.Length(Len + Size);
					GPtr p;
					uint8 *Start = &Code->ByteCode[Len];
					p.u8 = Start;
					*p.u8++ = ICallScript;
					
					if (n.ScriptFunc->StartAddr)
					{
						// Compile func address straight into code...
						*p.u32++ = n.ScriptFunc->StartAddr;
						*p.u16++ = n.ScriptFunc->FrameSize;
					}
					else
					{
						// Add link time fixup
						LinkFixup &Fix = Fixups.New();
						Fix.Tok = n.Tok;
						Fix.Args = n.Args.Length();
						Fix.Offset = p.u8 - &Code->ByteCode[0];
						Fix.Func = n.ScriptFunc;
						*p.u32++ = 0;
						*p.u16++ = 0;
					}

					*p.r++ = n.Reg;
					*p.u16++ = n.Args.Length();
					for (unsigned i=0; i<n.Args.Length(); i++)
					{
						*p.r++ = a[i];
					}
					LgiAssert(p.u8 == Start + Size);

					// Deallocate argument registers
					for (unsigned i=0; i<a.Length(); i++)
					{
						DeallocReg(a[i]);
					}
				}
				else return OnError(n.Tok, "Can't allocate register for method return value.");
			}
			else return false;
		}

		return true;
	}

	/// Parse expression into a node tree
	bool Expression(uint32 &Cur, GArray<Node> &n, int Depth = 0)
	{
		if (Cur < Tokens.Length())
		{
			char16 *t;
			bool PrevIsOp = true;
			while ((t = Tokens[Cur]))
			{
				GExpTokens Tok = ExpTok.Find(t);
				if (Tok == StartRdBracket)
				{
					Cur++;

					if (!Expression(Cur, n[n.Length()].Child, Depth + 1))
						return false;
					PrevIsOp = false;
				}
				else if (Tok == EndRdBracket)
				{
					if (Depth > 0)
						Cur++;
					break;
				}
				else if (Tok == Comma || Tok == SemiColon)
				{
					break;
				}
				else if (Depth == 0 && Tok == EndSqBracket)
				{
					break;
				}
				else if (Tok == ConstTrue || Tok == ConstFalse || Tok == ConstNull)
				{
					n.New().SetConst(Cur++, Tok);
				}
				else
				{
					GOperator o = IsOp(t, PrevIsOp);
					if (o != OpNull)
					{
						// Operator
						PrevIsOp = 1;
						n.New().SetOp(o, Cur);
					}
					else
					{
						PrevIsOp = 0;

						GVariant m;
						m = t;
						GFunc *f = Methods.Find(m.Str());
						GFunctionInfo *sf = 0;
						char16 *Next;
						if (f)
						{
							Node &Call = n.New();
							Call.SetContextFunction(f, Cur++);

							// Now parse arguments
							
							// Get the start bracket
							if ((t = GetTok(Cur)))
							{
								if (StricmpW(t, sStartRdBracket) == 0)
									Cur++;
								else return OnError(Cur, "Function missing '('");
							}
							else return OnError(Cur, "No token.");
							
							// Parse the args as expressions
							while ((t = GetTok(Cur)))
							{
								if (StricmpW(t, sComma) == 0)
								{
									// Do nothing...
									Cur++;
								}
								else if (StricmpW(t, sEndRdBracket) == 0)
								{
									break;
								}
								else if (!Expression(Cur, Call.Args.New()))
								{
									return OnError(Cur, "Can't parse function argument.");
								}
							}
						}
						else if
						(
							(
								sf = Code->GetMethod
								(
									m.Str(),
									(Next = GetTok(Cur+1)) != 0
									&&
									StricmpW(Next, sStartRdBracket) == 0
								)
							)
						)
						{
							Node &Call = n.New();

							Call.SetScriptFunction(sf, Cur++);

							// Now parse arguments
							
							// Get the start bracket
							if ((t = GetTok(Cur)))
							{
								if (StricmpW(t, sStartRdBracket) == 0)
									Cur++;
								else return OnError(Cur, "Function missing '('");
							}
							else return OnError(Cur, "No token.");
							
							// Parse the args as expressions
							while ((t = GetTok(Cur)))
							{
								if (StricmpW(t, sComma) == 0)
								{
									// Do nothing...
									Cur++;
								}
								else if (StricmpW(t, sEndRdBracket) == 0)
								{
									break;
								}
								else if (!Expression(Cur, Call.Args[Call.Args.Length()]))
								{
									return OnError(Cur, "Can't parse function argument.");
								}
							}
						}
						else if (IsAlpha(*t))
						{
							// Variable...
							Node &Var = n.New();
							Var.SetVar(t, Cur);

							while ((t = GetTok(Cur+1)))
							{
								// Get the last variable part...
								Node::VariablePart &vp = Var.Variable[Var.Variable.Length()-1];

								// Check for array index...
								if (StricmpW(t, sStartSqBracket) == 0)
								{
									// Got array index
									Cur += 2;
									if (!Expression(Cur, vp.Array))
										return OnError(Cur, "Couldn't parse array index expression.");
									if (!(t = GetTok(Cur)) || StricmpW(t, sEndSqBracket) != 0)
									{
										return OnError(Cur, "Expecting ']', didn't get it.");
									}

									t = GetTok(Cur+1);
								}
								else if (StricmpW(t, sStartRdBracket) == 0)
								{
									Cur += 2;
									t = GetTok(Cur);

									vp.Call = true;
									if (t && StricmpW(t, sEndRdBracket))
									{
										while (true)
										{
											GAutoPtr<Node::NodeExp> e(new Node::NodeExp);
											if (!e)
												return OnError(Cur, "Mem alloc error.");

											if (!Expression(Cur, *e))
												return OnError(Cur, "Couldn't parse func call argument expression.");
											
											vp.Args.Add(e.Release());
											
											t = GetTok(Cur);
											if (!t)
												return OnError(Cur, "Unexpected end of file.");

											if (!StricmpW(t, sComma))
												Cur++;
											else if (!StricmpW(t, sEndRdBracket))
												break;
											else
												return OnError(Cur, "Expecting ',', didn't get it.");
										}
									}
									
									t = GetTok(Cur+1);
								}
								
								// Check for DOM operator...
								if (StricmpW(t, sPeriod) == 0)
								{
									// Got Dom operator
									Cur += 2;
									t = GetTok(Cur);
									if (!t)
										return OnError(Cur, "Unexpected eof.");

									Var.Variable.New().Name = t;
								}
								else break;
							}
						}
						else
						{
							n.New().SetConst(Cur, NoToken);
						}
					}

					Cur++;
				}
			}
		}
		else
		{
			Log->Print("%s:%i - Unexpected end of file.\n", _FL);
			return false;
		}

		return true;
	}

	/// Allocate a register (must be mirrored with DeallocReg)
	bool AllocReg(GVarRef &r, const char *file, int line)
	{
		for (int i=0; i<MAX_REGISTER; i++)
		{
			int m = 1 << i;
			if ((Regs & m) == 0)
			{
				Regs |= m;
				r.Index = i;
				r.Scope = SCOPE_REGISTER;
				
				#ifdef _DEBUG
				char s[256];
				sprintf_s(s, sizeof(s), "%s:%i", file, line);
				RegAllocators[i] = s;
				#endif
				return true;
			}
		}

		#ifdef _DEBUG
		if (Log)
		{
			for (unsigned n=0; n<RegAllocators.Length(); n++)
			{
				char *a = RegAllocators[n].Str();
				if (a)
				{
					Log->Print("CompileError:Register[%i] allocated by %s\n", n, a);
				}
			}
		}
		#endif

		return false;
	}

	/// Deallocate a register
	bool DeallocReg(GVarRef &r)
	{
		if (r.Scope == SCOPE_REGISTER && r.Index >= 0)
		{
			int Bit = 1 << r.Index;
			LgiAssert((Bit & Regs) != 0);
			Regs &= ~Bit;

			#ifdef _DEBUG
			RegAllocators[r.Index].Empty();
			#endif
		}

		return true;
	}

	/// Count allocated registers
	int RegAllocCount()
	{
		int c = 0;

		for (int i=0; i<MAX_REGISTER; i++)
		{
			int m = 1 << i;
			if (Regs & m)
			{
				c++;
			}
		}

		return c;
	}

	char *DumpExp(GArray<Node> &n)
	{
		GStringPipe e;
		for (unsigned i=0; i<n.Length(); i++)
		{
			if (n[i].Op)
			{
				e.Print(" op(%i)", n[i].Op);
			}
			else if (n[i].Variable.Length())
			{
				e.Print(" %s", n[i].Variable[0].Name.Str());
			}
			else if (n[i].Constant)
			{
				char16 *t = Tokens[n[i].Tok];
				e.Print(" %S", t);
			}
			else if (n[i].ContextFunc)
			{
				e.Print(" %s(...)", n[i].ContextFunc->Method.Get());
			}
			else if (n[i].ScriptFunc)
			{
				e.Print(" %s(...)", n[i].ScriptFunc->Name.Str());
			}
			else
			{
				e.Print(" #err#");
			}
		}

		return e.NewStr();
	}
	
	/// Creates byte code to evaluate an expression
	bool AsmExpression
	(
		/// Where the result got stored
		GVarRef *Result,
		/// The nodes to create code for
		GArray<Node> &n,
		/// The depth of recursion
		int Depth = 0
	)
	{
		// Resolve any sub-expressions and store their values
		for (unsigned i = 0; i < n.Length(); i++)
		{
			if (!n[i].IsVar() &&
				n[i].Child.Length())
			{
				AllocReg(n[i].Reg, _FL);
				AsmExpression(&n[i].Reg, n[i].Child, Depth + 1);
			}
		}

		while (n.Length() > 1)
		{
			// Find which operator to handle first
			#ifdef _DEBUG
			int StartLength = n.Length();
			#endif
			int OpIdx = -1;
			int Prec = -1;
			int Ops = 0;
			for (unsigned i=0; i<n.Length(); i++)
			{
				if (n[i].Op)
				{
					Ops++;
					int p = GetPrecedence(n[i].Op);
					if (OpIdx < 0 || p < Prec)
					{
						Prec = p;
						OpIdx = i;
					}
				}
			}

			if (OpIdx < 0)
			{
				GVariant e;
				e.Type = GV_STRING;
				e.Value.String = DumpExp(n);
				return OnError(n[0].Tok, "No operator found in expression '%s'.", e.Str());
			}
			
			// Evaluate
			GOperator Op = n[OpIdx].Op;
			OperatorType Type = OpType(Op);
			if (Type == OpPrefix ||
				Type == OpPostfix)
			{
				Node &a = n[OpIdx + (Type == OpPrefix ? 1 : -1)];

				if (TokenToVarRef(a))
				{
					GVarRef Reg;
					if (a.Reg.Scope != SCOPE_REGISTER)
					{
						if (AllocReg(Reg, _FL))
						{
							Asm2(a.Tok, IAssign, Reg, a.Reg);
							a.Reg = Reg;
						}
						else
						{
							LgiAssert(!"Can't alloc register");
							return OnError(a.Tok, "No operator found in expression.");
						}
					}

					Asm1(a.Tok, Op, a.Reg);

					if (Op == OpPostInc || Op == OpPostDec)
					{
						// Write value back to origin
						GVariant &VarName = a.Variable[0].Name;

						GVarRef n = FindVariable(VarName, false);
						if (n.Valid())
						{
							if (a.Child.Length())
							{
								Asm3(a.Tok, IArraySet, n, a.ArrayIdx, a.Reg);
							}
							else if (n != a.Reg)
							{
								Asm2(a.Tok, IAssign, n, a.Reg);
							}
						}
						else
							return OnError(a.Tok, "Symbol '%s' not found.", VarName.Str());
					}

					n.DeleteAt(OpIdx, true);
				}
				else
				{
					LgiAssert(!"Can't turn tokens to refs.");
					return OnError(a.Tok, "Can't turn tokens to refs.");
				}
			}
			else if (Type == OpInfix)
			{
				if (OpIdx - 1 < 0)
				{
					return OnError(n[OpIdx].Tok, "Index %i is not a valid infix location", OpIdx);
				}

				Node &a = n[OpIdx-1];
				Node &b = n[OpIdx+1];

				if (TokenToVarRef(b))
				{
					if ((int)Op == (int)IAssign)
					{
						AssignVarRef(a, b.Reg);
					}
					else if (TokenToVarRef(a))
					{
						GVarRef Reg;
						if (a.Reg.Scope != SCOPE_REGISTER)
						{
							if (AllocReg(Reg, _FL))
							{
								Asm2(a.Tok, IAssign, Reg, a.Reg);
								a.Reg = Reg;
							}
							else return OnError(a.Tok, "Can't alloc register, Regs=0x%x", Regs);
						}

						Asm2(a.Tok, Op, a.Reg, b.Reg);

						if ((int)Op == (int)IPlusEquals ||
							(int)Op == (int)IMinusEquals ||
							(int)Op == (int)IMulEquals ||
							(int)Op == (int)IDivEquals)
						{
							AssignVarRef(a, a.Reg);
						}
					}
					else return OnError(a.Tok, "Can't convert left token to var ref.");

					if (a.Reg != b.Reg)
						DeallocReg(b.Reg);
					n.DeleteAt(OpIdx+1, true);
					n.DeleteAt(OpIdx, true);
				}
				else return OnError(b.Tok, "Can't convert right token to var ref.");
			}
			else
			{
				LgiAssert(!"Not a valid type");
				return OnError(n[0].Tok, "Not a valid type.");
			}

			#ifdef _DEBUG
			if (StartLength == n.Length())
			{
				// No nodes removed... infinite loop!
				LgiAssert(!"No nodes removed.");
				return false;
			}
			#endif
		}

		if (n.Length() == 1)
		{
			if (!n[0].Reg.Valid())
			{
				if (!TokenToVarRef(n[0]))
				{
					return false;
				}
			}

			if (Result)
			{
				*Result = n[0].Reg;
			}
			else
			{
				DeallocReg(n[0].Reg);
			}

			return true;
		}

		return false;
	}

	/// Parses and assembles an expression
	bool DoExpression(uint32 &Cur, GVarRef *Result)
	{
		GArray<Node> n;
		if (Expression(Cur, n))
		{
			bool Status = AsmExpression(Result, n);
			return Status;
		}

		return false;
	}

	/// Parses statements
	bool DoStatements(uint32 &Cur, bool *LastWasReturn, bool MoreThanOne = true)
	{
		while (Cur < Tokens.Length())
		{
			char16 *t = GetTok(Cur);
			if (!t)
				break;

			if (StricmpW(t, sSemiColon) == 0)
			{
				Cur++;
			}
			else if (!StricmpW(t, sReturn))
			{
				Cur++;
				if (!DoReturn(Cur))
					return false;
				if (LastWasReturn)
					*LastWasReturn = true;
			}
			else if (!StricmpW(t, sEndCurlyBracket) ||
					 !StricmpW(t, sFunction))
			{
				break;
			}
			else if (!StricmpW(t, sIf))
			{
				if (!DoIf(Cur))
					return false;
				if (LastWasReturn)
					*LastWasReturn = false;
			}
			else if (!StricmpW(t, sFor))
			{
				if (!DoFor(Cur))
					return false;
				if (LastWasReturn)
					*LastWasReturn = false;
			}
			else if (!StricmpW(t, sWhile))
			{
				if (!DoWhile(Cur))
					return false;
				if (LastWasReturn)
					*LastWasReturn = false;
			}
			else if (!StricmpW(t, sExtern))
			{
				if (!DoExtern(Cur))
					return false;
				if (LastWasReturn)
					*LastWasReturn = false;
			}
			else
			{
				if (!DoExpression(Cur, 0))
					return false;
				if (LastWasReturn)
					*LastWasReturn = false;
			}

			if (!MoreThanOne)
				break;
		}

		return true;
	}

	/// Parses if/else if/else construct
	bool DoIf(uint32 &Cur)
	{
		Cur++;
		char16 *t = GetTok(Cur);
		if (t && !StricmpW(t, sStartRdBracket))
		{
			Cur++;

			// Compile and asm code to evaluate the expression
			GVarRef Result;
			int ExpressionTok = Cur;
			if (DoExpression(Cur, &Result))
			{
				t = GetTok(Cur);
				if (!t || StricmpW(t, sEndRdBracket))
					return OnError(Cur, "if missing ')'.");
				Cur++;
				t = GetTok(Cur);
				if (!t)
					return OnError(Cur, "if missing body statement.");

				// Output the jump instruction
				Asm1(ExpressionTok, IJumpZero, Result);
				DeallocReg(Result);
				int JzOffset = Code->ByteCode.Length();
				Code->ByteCode.Length(JzOffset + sizeof(int32));

				if (!StricmpW(t, sStartCurlyBracket))
				{
					// Statement block
					Cur++;
					while ((t = GetTok(Cur)))
					{
						if (!StricmpW(t, sSemiColon))
						{
							Cur++;
						}
						else if (!StricmpW(t, sEndCurlyBracket))
						{
							Cur++;
							break;
						}
						else if (!DoStatements(Cur, NULL))
						{
							return false;
						}
					}

				}
				else
				{
					// Single statement
					if (!DoStatements(Cur, NULL, false))
						return false;
				}

				// Check for else...
				if ((t = GetTok(Cur)) && StricmpW(t, sElse) == 0)
				{
					// Add a jump for the "true" part of the expression to
					// jump over the "else" part.
					Asm0(Cur, IJump);
					int JOffset = Code->ByteCode.Length();
					if (Code->ByteCode.Length(JOffset + 4))
					{
						// Initialize the ptr to zero
						int32 *Ptr = (int32*)&Code->ByteCode[JOffset];
						*Ptr = 0;

						// Resolve jz to here...
						if (JzOffset)
						{
							// Adjust the "if" jump point
							int32 *Ptr = (int32*)&Code->ByteCode[JzOffset];
							*Ptr = Code->ByteCode.Length() - JzOffset - sizeof(int32);
							JzOffset = 0;
						}

						// Compile the else block
						Cur++;
						if ((t = GetTok(Cur)) && StricmpW(t, sStartCurlyBracket) == 0)
						{
							// 'Else' Statement block
							Cur++;
							while ((t = GetTok(Cur)))
							{
								if (!StricmpW(t, sSemiColon))
								{
									Cur++;
								}
								else if (!StricmpW(t, sEndCurlyBracket))
								{
									Cur++;
									break;
								}
								else if (!DoStatements(Cur, NULL))
								{
									return false;
								}
							}
						}
						else
						{
							// Single statement
							if (!DoStatements(Cur, NULL, false))
								return false;
						}

						// Resolve the "JOffset" jump that takes execution of
						// the 'true' part over the 'else' part
						Ptr = (int32*)&Code->ByteCode[JOffset];
						*Ptr = Code->ByteCode.Length() - JOffset - sizeof(int32);
						if (*Ptr == 0)
						{
							// Empty statement... so delete the Jump instruction
							Code->ByteCode.Length(JOffset - 1);
						}
					}
					else OnError(Cur, "Mem alloc");
				}
				if (JzOffset)
				{
					// Adjust the jump point
					int32 *Ptr = (int32*)&Code->ByteCode[JzOffset];
					int CurLen = Code->ByteCode.Length();
					*Ptr = CurLen - JzOffset - sizeof(int32);
					LgiAssert(*Ptr != 0);
				}
				return true;
			}
		}
		else return OnError(Cur, "if missing '('");

		return false;
	}

	GArray<uint8> &GetByteCode()
	{
		return Code->ByteCode;
	}

	class GJumpZero
	{
		GCompilerPriv *Comp;
		int JzOffset;

	public:
		GJumpZero(GCompilerPriv *d, uint32 &Cur, GVarRef &r)
		{
			// Create jump instruction to jump over the body if the expression evaluates to false
			Comp = d;
			Comp->Asm1(Cur, IJumpZero, r);
			Comp->DeallocReg(r);
			JzOffset = Comp->GetByteCode().Length();
			Comp->GetByteCode().Length(JzOffset + sizeof(int32));
		}

		~GJumpZero()
		{
			// Resolve jump
			int32 *Ptr = (int32*) &Comp->GetByteCode()[JzOffset];
			*Ptr = Comp->GetByteCode().Length() - (JzOffset + sizeof(int32));
		}
	};

	/// Parses while construct
	bool DoWhile(uint32 &Cur)
	{
		Cur++;
		char16 *t = GetTok(Cur);
		if (!t || StricmpW(t, sStartRdBracket))
			return OnError(Cur, "Expecting '(' after 'while'");
		Cur++;

		// Store start of condition code
		int ConditionStart = Code->ByteCode.Length();

		// Compile condition evalulation
		GVarRef r;
		if (!DoExpression(Cur, &r))
			return false;

		// Create jump instruction to jump over the body if the expression evaluates to false
		{
			GJumpZero Jump(this, Cur, r);

			if (!(t = GetTok(Cur)) || StricmpW(t, sEndRdBracket))
				return OnError(Cur, "Expecting ')'");
			Cur++;

			// Compile the body of the loop
			if (!(t = GetTok(Cur)))
				return OnError(Cur, "Unexpected eof.");
			Cur++;

			if (StricmpW(t, sStartCurlyBracket) == 0)
			{
				// Block
				while ((t = GetTok(Cur)))
				{
					if (!StricmpW(t, sSemiColon))
					{
						Cur++;
					}
					else if (!StricmpW(t, sEndCurlyBracket))
					{
						Cur++;
						break;
					}
					else if (!DoStatements(Cur, NULL))
					{
						return false;
					}
				}
			}
			else
			{
				// Single statement
				DoStatements(Cur, NULL, false);
			}

			// Jump to condition evaluation at 'ConditionStart'
			Asm0(Cur, IJump);
			int JOffset = Code->ByteCode.Length();
			Code->ByteCode.Length(JOffset + sizeof(int32));
			int32 *Ptr = (int32*) &Code->ByteCode[JOffset];
			*Ptr = ConditionStart - Code->ByteCode.Length();
		}
		
		return true;
	}

	/// Parses for construct
	bool DoFor(uint32 &Cur)
	{
		/*
			For loop asm structure:

				+---------------------------+
				| Pre-condition expression  |
				+---------------------------+
				| Eval loop contdition exp. |<--+
				|                           |   |
				| JUMP ZERO (jumpz)         |---+-+
				+---------------------------+   | |
				| Body of loop...           |   | |
				|                           |   | |
				|                           |   | |
				|                           |   | |
				|                           |   | |
				|                           |   | |
				+---------------------------+   | |
				| Post-cond. expression     |   | |
				|                           |   | |
				| JUMP                      |---+ |
				+---------------------------+     |
				| Following code...         |<----+
				.                           .
		
		*/

		Cur++;
		char16 *t = GetTok(Cur);
		if (!t || StricmpW(t, sStartRdBracket))
			return OnError(Cur, "Expecting '(' after 'for'");
		Cur++;
		
		// Compile initial statement
		GVarRef r;
		t = GetTok(Cur);
		if (!t)
			return false;
		if (StricmpW(t, sSemiColon) && !DoExpression(Cur, 0))
			return false;
		t = GetTok(Cur);

		// Look for ';'
		if (!t || StricmpW(t, sSemiColon))
			return OnError(Cur, "Expecting ';'");
		Cur++;
		
		// Store start of condition code
		int ConditionStart = Code->ByteCode.Length();
		
		// Compile condition evalulation
		if (!DoExpression(Cur, &r))
			return false;

		{
			GJumpZero Jmp(this, Cur, r);
			t = GetTok(Cur);

			// Look for ';'
			if (!t || StricmpW(t, sSemiColon))
				return OnError(Cur, "Expecting ';'");
			Cur++;

			// Compile the post expression code
			int PostCodeStart = Code->ByteCode.Length();
			t = GetTok(Cur);
			if (StricmpW(t, sEndRdBracket) && !DoExpression(Cur, 0))
				return false;

			// Store post expression code in temp variable
			GArray<uint8> PostCode;
			int PostCodeLen = Code->ByteCode.Length() - PostCodeStart;
			if (PostCodeLen)
			{
				PostCode.Length(PostCodeLen);
				memcpy(&PostCode[0], &Code->ByteCode[PostCodeStart], PostCodeLen);
				
				// Delete the post expression off the byte code, we are putting it after the block code
				Code->ByteCode.Length(PostCodeStart);
			}

			// Look for ')'
			t = GetTok(Cur);
			if (!t || StricmpW(t, sEndRdBracket))
				return OnError(Cur, "Expecting ')'");
			Cur++;
			
			// Compile body of loop
			if ((t = GetTok(Cur)) && StricmpW(t, sStartCurlyBracket) == 0)
			{
				Cur++;
				while ((t = GetTok(Cur)))
				{
					if (!StricmpW(t, sSemiColon))
					{
						Cur++;
					}
					else if (!StricmpW(t, sEndCurlyBracket))
					{
						Cur++;
						break;
					}
					else if (!DoStatements(Cur, NULL))
					{
						return false;
					}
				}
			}
			
			// Add post expression code
			if (PostCodeLen)
			{
				int Len = Code->ByteCode.Length();
				Code->ByteCode.Length(Len + PostCodeLen);
				memcpy(&Code->ByteCode[Len], &PostCode[0], PostCodeLen);
			}
			
			// Jump to condition evaluation at 'ConditionStart'
			Asm0(Cur, IJump);
			int JOffset = Code->ByteCode.Length();
			Code->ByteCode.Length(JOffset + sizeof(int32));
			int32 *Ptr = (int32*) &Code->ByteCode[JOffset];
			*Ptr = ConditionStart - Code->ByteCode.Length();
		}
		
		return true;
	}

	/// Compiles return construct
	bool DoReturn(uint32 &Cur)
	{
		GVarRef ReturnValue;
		char16 *t;
		int StartTok = Cur;

		GArray<Node> Exp;
		if (!Expression(Cur, Exp))
		{
			return OnError(Cur, "Failed to compile return expression.");
		}
		else if (!AsmExpression(&ReturnValue, Exp))
		{
			return OnError(Cur, "Failed to assemble return expression.");
		}

		if (!(t = GetTok(Cur)) ||
			StricmpW(t, sSemiColon))
		{
			return OnError(Cur, "Expecting ';' after return expression.");
		}

		Asm1(StartTok, IRet, ReturnValue);
		Cur++;
		return true;
	}

	// Compile a method definition
	bool DoFunction(uint32 &Cur)
	{
		bool Status = false;
		bool LastInstIsReturn = false;

		GVariant FunctionName;
		char16 *Name = GetTok(Cur);
		if (Name)
		{
			Cur++;
			char16 *t = GetTok(Cur);
			if (!t || StricmpW(t, sStartRdBracket))
				return OnError(Cur, "Expecting '(' in function.");

			FunctionName = Name;
			GFunctionInfo *f = Code->GetMethod(FunctionName.Str(), true);
			if (!f)
				return OnError(Cur, "Can't define method '%s'.", FunctionName.Str());

			f->Name = t;
			f->StartAddr = Code->ByteCode.Length();

			// Parse parameters
			Cur++;
			while ((t = GetTok(Cur)))
			{
				if (IsAlpha(*t))
				{
					f->Params.New() = t;
					Cur++;
					if (!(t = GetTok(Cur)))
						goto UnexpectedFuncEof;
				}

				if (!StricmpW(t, sComma))
					;
				else if (!StricmpW(t, sEndRdBracket))
				{
					Cur++;
					break;
				}
				else
					goto UnexpectedFuncEof;
				
				Cur++;
			}

			// Parse start of body
			if (!(t = GetTok(Cur)))
				goto UnexpectedFuncEof;
			if (StricmpW(t, sStartCurlyBracket))
				return OnError(Cur, "Expecting '{'.");

			// Setup new scope
			GVariables LocalScope(SCOPE_LOCAL);
			Scopes.Add(&LocalScope);
			for (unsigned i=0; i<f->Params.Length(); i++)
			{
				LocalScope.Var(f->Params[i].Str(), true);
			}

			// Parse contents of function body
			Cur++;
			while ((t = GetTok(Cur)))
			{
				// End of the function?
				if (!StricmpW(t, sEndCurlyBracket))
				{
					f->Name = Name;
					f->FrameSize = LocalScope.Length();
					Status = true;
					Cur++;

					// LgiTrace("Added method %s @ %i, stack=%i, args=%i\n", f->Name.Str(), f->StartAddr, f->FrameSize, f->Params.Length());
					break;
				}
				
				// Parse statment in the body
				if (!DoStatements(Cur, &LastInstIsReturn))
					return OnError(Cur, "Can't compile function body.");
			}
			
			// Remove local scope from scopes
			Scopes.Length(Scopes.Length()-1);
		}

		if (!LastInstIsReturn)
		{
			GVarRef RetVal;
			AllocNull(RetVal);
			Asm1(Cur, IRet, RetVal);
		}
		
		return Status;

	UnexpectedFuncEof:
		return OnError(Cur, "Unexpected EOF in function.");
	}
	
	GExternFunc::ExternType ParseExternType(GArray<const char16*> &Strs)
	{
		GExternFunc::ExternType Type;
		
		Type.Out = false;
		Type.Ptr = 0;
		Type.ArrayLen = 1;
		Type.Base = GV_NULL;
		Type.Unsigned = false;
		
		bool InArray = false;
		for (unsigned i=0; i<Strs.Length(); i++)
		{
			const char16 *t = Strs[i];
			if (t[0] == '*' && t[1] == 0)
				Type.Ptr++;
			else if (!StricmpW(t, sOutParam))
				Type.Out = true;
			else if (!StricmpW(t, sStartSqBracket))
				InArray = true;
			else if (!StricmpW(t, sEndSqBracket))
				InArray = false;
			else if (IsDigit(*t))
			{
				if (InArray)
					Type.ArrayLen = AtoiW(t);
			}
			else
			{
				GAutoString u(LgiNewUtf16To8(t));
				GVariantType tok_type = Types.Find(u);
				if (tok_type != GV_NULL)
				{
					Type.Base = tok_type;
				}
			}
		}
		
		if (Type.Ptr && Type.Base == GV_NULL)
			Type.Base = GV_VOID_PTR;
		
		Strs.Length(0);
		return Type;
	}

	bool DoExtern(uint32 &Cur)
	{
		GArray<const char16*> Tok;
		const char16 *t;
		while ((t = GetTok(Cur)))
		{
			if (!StricmpW(t, sStartRdBracket))
			{
				Cur++;
				break;
			}
			else Tok.Add(t);
			Cur++;
		}
		
		if (Tok.Length() < 3)
		{
			return OnError(Cur, "Not enough tokens in extern decl.");
		}
		
		// First token is library name
		GExternFunc *e = new GExternFunc;
		if (!e)
			return OnError(Cur, "Alloc error.");
		
		Code->Externs.Add(e);
		e->Type = ExternFunc;
		char16 sQuotes[] = {'\'','\"',0};
		GAutoWString LibName(TrimStrW(Tok[0], sQuotes));
		e->Lib.Reset(LgiNewUtf16To8(LibName));
		Tok.DeleteAt(0, true);

		e->Method.Reset(LgiNewUtf16To8(Tok.Last()));
		Tok.DeleteAt(Tok.Length()-1, true);

		if (!ValidStr(e->Method))
		{
			Code->Externs.Length(Code->Externs.Length()-1);
			return OnError(Cur, "Failed to get extern method name.");
		}
		
		// Parse return type
		e->ReturnType = ParseExternType(Tok);

		// Parse argument types
		Tok.Length(0);
		while ((t = GetTok(Cur)))
		{
			if (!StricmpW(t, sEndRdBracket))
			{
				e->ArgType.New() = ParseExternType(Tok);
				Cur++;
				break;
			}
			else if (!StricmpW(t, sComma))
			{
				e->ArgType.New() = ParseExternType(Tok);
			}
			else Tok.Add(t);
			Cur++;
		}
		
		if ((t = GetTok(Cur)))
		{
			if (StricmpW(t, sSemiColon))
				return OnError(Cur, "Expecting ';' in extern decl.");
			Cur++;
		}
		
		Methods.Add(e->Method, e);
		return true;
	}

	/// Compiles struct construct
	bool DoStruct(uint32 &Cur)
	{
		bool Status = false;

		// Parse struct name and setup a type
		char16 *t;
		GTypeDef *Def = Code->Types.Find(t = GetTok(Cur));
		if (!Def)
			Code->Types.Add(t, Def = new GTypeDef(t));
		Cur++;
		t = GetTok(Cur);
		if (!t || StricmpW(t, sStartCurlyBracket))
			return OnError(Cur, "Expecting '{'");
		Cur++;

		// Parse members
		while ((t = GetTok(Cur)))
		{
			// End of type def?
			if (!StricmpW(t, sEndCurlyBracket))
			{
				Cur++;
				t = GetTok(Cur);
				if (!t || StricmpW(t, sSemiColon))
					return OnError(Cur, "Expecting ';' after '}'");

				Status = true;
				break;
			}

			// Parse member field
			GVariant TypeName = t;

			GTypeDef *NestedType = 0;
			GVariantType Type = Types.Find(TypeName.Str());
			if (!Type)
			{
				// Check other custom types
				NestedType = Code->GetType(t);
				if (!NestedType)
					return OnError(Cur, "Unknown type '%S' in struct definition.", t);

				// Ok, nested type.
				Type = GV_CUSTOM;
			}

			Cur++;
			if (!(t = GetTok(Cur)))
				goto EofError;

			bool Pointer = false;
			if (t[0] == '*' && t[1] == 0)
			{
				Pointer = true;
				Cur++;
				if (!(t = GetTok(Cur)))
					goto EofError;
			}

			GVariant Name = t;
			Cur++;
			if (!(t = GetTok(Cur)))
				goto EofError;

			int Array = 0;
			if (!StricmpW(t, sStartSqBracket))
			{
				// Array
				Cur++;
				if (!(t = GetTok(Cur)))
					goto EofError;

				Array = atoi(t);

				Cur++;
				if (!(t = GetTok(Cur)))
					goto EofError;
				if (StricmpW(t, sEndSqBracket))
					return OnError(Cur, "Expecting ']' in array definition.");
				Cur++;
			}

			GTypeDef::GMember *Mem = Def->Members.Find(Name.Str());
			if (Mem)
				return OnError(Cur, "Member '%s' can't be defined twice.", Name.Str());
			if (!(Mem = new GTypeDef::GMember))
				return OnError(Cur, "Alloc.");

			Mem->Offset = Def->Size;
			Mem->Array = Array;
			Mem->Type = Type;
			Mem->Nest = NestedType;
			if ((Mem->Pointer = Pointer))
			{
				Mem->Size = sizeof(void*);
			}
			else
			{
				switch (Mem->Type)
				{
					case GV_INT32:
						Mem->Size = sizeof(int32);
						break;
					case GV_INT64:
						Mem->Size = sizeof(int64);
						break;
					case GV_BOOL:
						Mem->Size = sizeof(bool);
						break;
					case GV_DOUBLE:
						Mem->Size = sizeof(double);
						break;
					case GV_STRING:
						Mem->Size = sizeof(char);
						break;
					case GV_DATETIME:
						Mem->Size = sizeof(GDateTime);
						break;
					case GV_HASHTABLE:
						Mem->Size = sizeof(GHashTable);
						break;
					case GV_OPERATOR:
						Mem->Size = sizeof(GOperator);
						break;
					case GV_GMOUSE:
						Mem->Size = sizeof(GMouse);
						break;
					case GV_GKEY:
						Mem->Size = sizeof(GKey);
						break;
					case GV_CUSTOM:
						Mem->Size = Mem->Nest->Sizeof();
						break;
					default:
						return OnError(Cur, "Can't have non-pointer of type '%s'", TypeName.Str());
				}
			}
			if (Mem->Array)
			{
				Mem->Size *= Mem->Array;
			}

			Def->Size += Mem->Size;
			Def->Members.Add(Name.Str(), Mem);

			t = GetTok(Cur);
			if (StricmpW(t, sSemiColon))
				return OnError(Cur, "Expecting ';'");
			Cur++;
		}

		return Status;

	EofError:
		return OnError(Cur, "Unexpected EOF.");
	}

	/// Compiler entry point
	bool Compile()
	{
		uint32 Cur = 0;
		int JumpLoc = 0;

		// Setup the global scope
		Scopes.Length(0);
		Scopes.Add(&Code->Globals);

		// Compile the code...
		while (Cur < Tokens.Length())
		{
			char16 *t = GetTok(Cur);
			if (!t)
				break;

			if (*t == '#' ||
				StricmpW(t, sSemiColon) == 0)
			{
				Cur++;
			}
			else if (!StricmpW(t, sFunction))
			{
				if (!JumpLoc)
				{
					int Len = Code->ByteCode.Length();
					if (Code->ByteCode.Length(Len + 5))
					{
						GPtr p;
						p.u8 = &Code->ByteCode[Len];
						*p.u8++ = IJump;
						*p.i32++ = 0;
						JumpLoc = Len + 1;
					}
					else OnError(Cur, "Mem alloc failed.");
				}

				if (!DoFunction(++Cur))
					return false;
			}
			else if (!StricmpW(t, sStruct))
			{
				if (!DoStruct(++Cur))
					return false;
			}
			else if (!StricmpW(t, sEndCurlyBracket))
			{
				return OnError(Cur, "Not expecting '}'.");
			}
			else if (!StricmpW(t, sExtern))
			{
				if (!DoExtern(++Cur))
					return false;
			}
			else
			{
				if (JumpLoc)
				{
					GPtr p;
					p.u8 = &Code->ByteCode[JumpLoc];
					*p.u32 = Code->ByteCode.Length() - (JumpLoc + 4);
					JumpLoc = 0;
				}

				if (!DoStatements(Cur, NULL))
				{
					return OnError(Cur, "Statement compilation failed.");
				}
			}
		}

		if (JumpLoc)
		{
			GPtr p;
			p.u8 = &Code->ByteCode[JumpLoc];
			*p.u32 = Code->ByteCode.Length() - (JumpLoc + 4);
			JumpLoc = 0;
		}

		// Do link time fixups...
		for (unsigned i=0; i<Fixups.Length(); i++)
		{
			LinkFixup &f = Fixups[i];
			if (f.Func->StartAddr)
			{
				if (f.Args == f.Func->Params.Length())
				{
					GPtr p;
					p.u8 = &Code->ByteCode[f.Offset];
					LgiAssert(*p.u32 == 0);
					*p.u32++ = f.Func->StartAddr;
					*p.u16++ = f.Func->FrameSize;
				}
				else return OnError(f.Tok, "Function call '%s' has wrong arg count (caller=%i, method=%i).",
											f.Func->Name.Str(),
											f.Args,
											f.Func->Params.Length());
			}
			else
			{
				return OnError(f.Tok, "Function '%s' not defined.", f.Func->Name.Str());
			}
		}
		Fixups.Length(0);

		return true;
	}
};

GCompiler::GCompiler()
{
	d = new GCompilerPriv;
}

GCompiler::~GCompiler()
{
	DeleteObj(d);
}

bool GCompiler::Compile
(
	GAutoPtr<GScriptObj> &Code,
	GScriptContext *SysContext,
	GScriptContext *UserContext,
	const char *FileName,
	const char *Script
)
{
	if (!Script)
		return NULL;

	GStringPipe p;
	
	if (SysContext && SysContext->GetLog())
		d->Log = SysContext->GetLog();
	else if (UserContext && UserContext->GetLog())
		d->Log = UserContext->GetLog();
	else
		d->Log = &p;

	d->Methods.Empty();
	if (SysContext)
	{
		GHostFunc *f = SysContext->GetCommands();
		for (int i=0; f[i].Method; i++)
		{
			f[i].Context = SysContext;
			d->Methods.Add(f[i].Method, &f[i]);
		}
	}

	d->Ctx = UserContext;
	if (d->Ctx)
	{
		GHostFunc *f = d->Ctx->GetCommands();
		for (int i=0; f[i].Method; i++)
		{
			f[i].Context = d->Ctx;
			
			if (!d->Methods.Find(f[i].Method))
				d->Methods.Add(f[i].Method, f+i);
			else
				LgiAssert(!"Conflicting name of method in application's context.");
		}
	}

	if (!Code)
		Code.Reset(new GCompiledCode);
	
	bool Status = false;
	d->Code = dynamic_cast<GCompiledCode*>(Code.Get());
	if (d->Code)
	{
		d->Code->UserContext = UserContext;
		d->Code->SysContext = SysContext;
		d->Code->SetFileName(FileName);
		
		bool LexResult = d->Lex((char*)Script, FileName);
		if (LexResult)
		{
			Status = d->Compile();
		}
	}

	d->Code = NULL;
	return Status;
}

//////////////////////////////////////////////////////////////////////
class GScriptEnginePrivate2
{
public:
	GViewI *Parent;
	SystemFunctions SysContext;
	GScriptContext *UserContext;

	GScriptEnginePrivate2()
	{
		UserContext = NULL;
		Parent = NULL;
	}
};

GScriptEngine2::GScriptEngine2(GViewI *parent, GScriptContext *UserContext)
{
	d = new GScriptEnginePrivate2;
	d->Parent = parent;
	d->UserContext = UserContext;
}

GScriptEngine2::~GScriptEngine2()
{
	DeleteObj(d);
}

GScriptObj *GScriptEngine2::CreateObj()
{
	return new GCompiledCode;
}

bool GScriptEngine2::Compile(GAutoPtr<GScriptObj> &Obj, GScriptContext *UserContext, char *Script, const char *FileName)
{
	if (!Script)
	{
		LgiAssert(!"Param error");
		return NULL;
	}

	GCompiler Comp;
	return Comp.Compile(Obj,
						&d->SysContext,
						UserContext ? UserContext : d->UserContext,
						FileName,
						Script);
}

GExecutionStatus GScriptEngine2::Run(GScriptObj *Obj)
{
	GExecutionStatus Status = ScriptError;

	GCompiledCode *Code = dynamic_cast<GCompiledCode*>(Obj);
	if (Code)
	{
		GVirtualMachine Vm;
		Status = Vm.Execute(Code);
	}

	return Status;
}

GExecutionStatus GScriptEngine2::RunTemporary(GScriptObj *Obj, char *Script)
{
	GExecutionStatus Status = ScriptError;
	GCompiledCode *Code = dynamic_cast<GCompiledCode*>(Obj);
	if (Script && Code)
	{
		GAutoPtr<GScriptObj> Temp(new GCompiledCode(*Code));
		uint32 TempLen = Temp->Length();
		
		GCompiler Comp;
		if (Comp.Compile(Temp, &d->SysContext, d->UserContext, Temp->GetFileName(), Script))
		{
			GVirtualMachine Vm;
			Status = Vm.Execute(dynamic_cast<GCompiledCode*>(Temp.Get()), TempLen);
		}
	}

	return Status;
}

bool GScriptEngine2::EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression)
{
	if (!Result || !VariableSource || !Expression)
	{
		LgiAssert(!"Param error");
		return false;
	}

	GStringPipe p;
	p.Print("return %s;", Expression);
	GAutoString a(p.NewStr());
	
	GCompiler Comp;
	GAutoPtr<GScriptObj> Obj;
	if (!Comp.Compile(Obj, NULL, NULL, NULL, a))
		return false;
	
	GVirtualMachine Vm;
	GCompiledCode *Code = dynamic_cast<GCompiledCode*>(Obj.Get());
	GExecutionStatus s = Vm.Execute(Code, 0, NULL, true, Result);
	
	return s != ScriptError;
}

GStream *GScriptEngine2::GetConsole()
{
	if (d->SysContext.GetLog())
		return d->SysContext.GetLog();

	if (d->UserContext && d->UserContext->GetLog())
		return d->UserContext->GetLog();

	return NULL;
}

bool GScriptEngine2::SetConsole(GStream *t)
{
	d->SysContext.SetLog(t);

	if (d->UserContext)
		d->UserContext->SetLog(t);

	return true;
}

bool GScriptEngine2::CallMethod(GScriptObj *Obj, const char *Method, GVariant *Ret, ArgumentArray &Args)
{
	GCompiledCode *Code = dynamic_cast<GCompiledCode*>(Obj);
	if (!Code || !Method)
		return false;

	GFunctionInfo *i = Code->GetMethod(Method);
	if (!i)
		return false;

	GVirtualMachine Vm;
	GExecutionStatus Status = Vm.ExecuteFunction(Code, i, Args, Ret, NULL);
	return Status != ScriptError;
}

GScriptContext *GScriptEngine2::GetSystemContext()
{
	return &d->SysContext;
}
