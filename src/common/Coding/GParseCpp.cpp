#include "Lgi.h"
#include "GParseCpp.h"
#include "GThreadEvent.h"
#include "GLexCpp.h"
#include "GVariant.h"
#include "GScriptingPriv.h"

#define CheckToken(t)	if (!t) { Error = true; break; }
#define Debug			__asm int 3

struct PreprocessState
{
	bool ParentIsActive;
	bool IfBranchClaimed;
};

bool CmpToken(char16 *a, const char *b)
{
	if (!a || !b)
		return false;
		
	while (*a && *b)
	{
		if (ToLower(*a) != ToLower(*b))
			return false;
		a++;
		b++;
	}
	
	return !*a && !*b;
}

bool HasToken(GArray<char16*> &a, const char *b)
{
	if (!a.Length() || !b)
		return false;
	
	for (int i=0; i<a.Length(); i++)
	{
		if (CmpToken(a[i], b))
			return true;
	}
	
	return false;
}

struct WorkUnit
{
	GArray<GAutoString> IncludePaths;
	GArray<GCppParser::ValuePair*> PreDefines;
	GArray<GAutoString> Source;

	GAutoString SearchTerm;
	
	WorkUnit
	(
		GArray<const char*> &IncPaths,
		GArray<GCppParser::ValuePair*> &PreDefs,
		GArray<char*> &Src
	)
	{
		int i;
		for (i=0; i<IncPaths.Length(); i++)
		{
			IncludePaths.New().Reset(NewStr(IncPaths[i]));
		}
		for (i=0; i<PreDefs.Length(); i++)
		{
			PreDefs.Add(new GCppParser::ValuePair(*PreDefs[i]));
		}
		for (i=0; i<Src.Length(); i++)
		{
			Source.New().Reset(NewStr(Src[i]));
		}
	}
	
	WorkUnit(const char *Search)
	{
		SearchTerm.Reset(NewStr(Search));
	}
	
	~WorkUnit()
	{
		PreDefines.DeleteObjects();
	}
};

struct GSymbol
{
	GSymbolType Type;
	char *File;
	uint32 Line;
	GArray<char16*> Tokens;
	
	GSymbol *Parent;
	GArray<GSymbol*> Children;
	
	const char *DefineFile;
	int DefineLine;
	
	bool FriendDecl;
	bool ForwardDecl;
	
	GSymbol(const char *def_file, int def_line)
	{
		Parent = 0;
		Type = SymNone;
		File = NULL;
		Line = 0;
		FriendDecl = false;
		ForwardDecl = false;
		DefineFile = def_file;
		DefineLine = def_line;
	}

	~GSymbol()
	{
		Children.DeleteObjects();
	}
	
	void Add(GSymbol *s)
	{
		if (!Children.HasItem(s))
		{
			Children.Add(s);
			s->Parent = this;
		}
	}
};

struct GCppStringPool
{
	int Size;
	int Used;
	GArray<char16*> Mem;
	
	GCppStringPool(int sz)
	{
		Used = Size = sz;
	}	
	
	~GCppStringPool()
	{
		Mem.DeleteArrays();
	}
	
	char16 *Alloc(const char16 *s, int len)
	{
		if (len < 0)
			len = StrlenW(s);
		
		int remaining = Size - Used;
		char16 *n;
		if (len + 1 > remaining)
		{
			n = new char16[Size];
			Mem.Add(n);
			Used = 0;
		}
		else
		{
			LgiAssert(Mem.Length() > 0);
			n = Mem.Last() + Used;
		}
		
		memcpy(n, s, sizeof(*s) * len);
		n[len] = 0;
		Used += len + 1;
		
		return n;
	}
};

char16 *LexPoolAlloc(void *Context, const char16 *s, int len)
{
	return ((GCppStringPool*)Context)->Alloc(s, len);
}

struct GSourceFile
{
	uint32 Id;
	GAutoString Path;
	GCppStringPool Pool;

	GAutoWString Raw;
	char16 *Cur;
	int Line;
	GArray<char16*> Tokens;
	GHashTbl<char16*,int> LineMap;
	GArray<PreprocessState> Stack;
	bool Active;
	
	GSourceFile() : Pool(16 << 10)
	{
		Id = 0;
		Line = 1;
		Active = true;
		Cur = NULL;
	}
	
	bool Read(const char *path)
	{
		if (!Path.Reset(NewStr(path)))
			return false;
		
		GAutoString f(ReadTextFile(Path));
		if (!f)
			return false;

		Raw.Reset(LgiNewUtf8To16(f));
		Cur = Raw;
		LineMap.SetSize(StrlenW(Raw) / 2);
		
		return true;
	}
	
	char16 *NextToken()
	{
		int PrevLine = Line;
		char16 *t = LexCpp(Cur, LexPoolAlloc, &Pool, &Line);
		if (t)
			LineMap.Add(t, Line);
		return t;
	}
};

struct GSourceScope : public GHashTbl<char16*, GSymbol*>
{
	~GSourceScope()
	{
		DeleteObjects();
	}

	bool Add(char16 *k, GSymbol *v)
	{
		if (CmpToken(k, "GKey"))
		{
			int asd=0;
		}
		
		return GHashTbl<char16*, GSymbol*>::Add(k, v);
	}

	GSymbol *Define(char16 *name, GSymbolType Type, const char *def_file, int def_line)
	{
		GSymbol *s = Find(name);
		if (!s) Add(name, s = new GSymbol(def_file, def_line));
		s->Type = Type;
		s->File = NULL;
		s->Line = 0;		
		return s;
	}
};

enum KeywordType
{
	KwNone,
	KwSpecifier,
	KwType,
	KwQualifier,
	KwModifier,
	KwUserType,
	KwVisibility,
	KwPreprocessor
};

enum MsgType
{
	MsgError,
	MsgInfo,
};

struct GCppParserWorker : public GThread, public GMutex, public GCompileTools
{
	GCppParserPriv *d;
	GArray<WorkUnit*> Work;
	bool Loop;
	GThreadEvent Event;
	WorkUnit *w;
	GCppStringPool GeneralPool;
	
	GArray<GSourceFile*> Srcs;
	GHashTbl<const char*, GSourceFile*> SrcHash;
	GArray<GSourceScope*> Scopes; // from global to local
	
	GHashTbl<char16*, KeywordType> Keywords;
	void AddHash(const char *name, KeywordType type);
	
	void InitScopes();
	GSourceScope *CurrentScope() { return Scopes[Scopes.Length()-1]; }
	GAutoWString GetSymbolName(GArray<char16*> &a);

	GSymbol *ParseDecl(GSourceFile *sf, char16 *t);
	GSymbol *ParseUserType(GSourceFile *sf, char16 *t);
	GSymbol *ParseTypedef(GSourceFile *sf, char16 *t);
	bool ParsePreprocessor(GSourceFile *sf, char16 *t);
	
public:
	GCppParserWorker(GCppParserPriv *priv);
	~GCppParserWorker();
	
	void Add(WorkUnit *w);
	GSourceFile *ParseCpp(const char *Path);	
	GSymbol *Resolve(char16 *Symbol);
	int Evaluate(GArray<char16*> &Exp);
	void DoWork();
	int Main();
	void Msg(MsgType Type, char *Fmt, ...);
};

struct GCppParserPriv
{
	GArray<char*> IncPaths;
	GAutoPtr<GCppParserWorker> Worker;
};

GCppParserWorker::GCppParserWorker(GCppParserPriv *priv) :
	GThread("GCppParserWorker"),
	GMutex("GCppParserWorker"),
	Event("GCppParserEvent"),
	SrcHash(0, false),
	GeneralPool(4 << 10)
{
	d = priv;
	Loop = true;
	w = NULL;

	AddHash("auto", KwSpecifier);
	AddHash("extern", KwSpecifier);
	AddHash("static", KwSpecifier);
	AddHash("register", KwSpecifier);
	AddHash("mutable", KwSpecifier);

	AddHash("bool", KwType);
	AddHash("char", KwType);
	AddHash("wchar_t", KwType);
	AddHash("short", KwType);
	AddHash("int", KwType);
	AddHash("long", KwType);
	AddHash("float", KwType);
	AddHash("double", KwType);
	AddHash("void", KwType);
	#ifdef WIN32
	AddHash("__int64", KwType);
	AddHash("_W64", KwModifier);
	#endif
	AddHash("unsigned", KwModifier);
	AddHash("signed", KwModifier);
	AddHash("__declspec", KwModifier);
	AddHash("template", KwModifier);
	AddHash("__stdcall", KwModifier);
	AddHash("friend", KwModifier);
	
	AddHash("class", KwUserType);
	AddHash("struct", KwUserType);
	AddHash("union", KwUserType);
	AddHash("enum", KwUserType);
	
	AddHash("public:", KwVisibility);
	AddHash("private:", KwVisibility);
	AddHash("protected:", KwVisibility);

	AddHash("#if", KwPreprocessor);
	AddHash("#ifdef", KwPreprocessor);
	AddHash("#ifndef", KwPreprocessor);
	AddHash("#elif", KwPreprocessor);
	AddHash("#else", KwPreprocessor);
	AddHash("#endif", KwPreprocessor);
	AddHash("#include", KwPreprocessor);
	AddHash("#define", KwPreprocessor);
	AddHash("#undef", KwPreprocessor);
	AddHash("#error", KwPreprocessor);
	AddHash("#warning", KwPreprocessor);
	AddHash("#pragma", KwPreprocessor);

	Run();
}

GCppParserWorker::~GCppParserWorker()
{
	Loop = false;
	Event.Signal();
	while (!IsExited())
	{
		LgiSleep(1);
	}
	
	Srcs.DeleteObjects();
}

void GCppParserWorker::Msg(MsgType Type, char *Fmt, ...)
{
	va_list Arg;
	va_start(Arg, Fmt);
	#ifdef WIN32
	char Buf[256];
	vsnprintf_s(Buf, sizeof(Buf), _TRUNCATE, Fmt, Arg);
	OutputDebugString(Buf);
	#else
	vprintf(Fmt, Arg);
	#endif
	va_end(Arg);
	if (Type == MsgError)
		Debug;
}

void GCppParserWorker::AddHash(const char *name, KeywordType type)
{
	char16 s[256], *o = s;
	while (*name)
		*o++ = *name++;
	*o++ = 0;
	Keywords.Add(s, type);
}

void GCppParserWorker::InitScopes()
{
	Scopes.DeleteObjects();

	Scopes.Add(new GSourceScope);
	
	#ifdef WIN32
	char16 buf[256];

	GSymbol *s = Scopes[0]->Define(L"_MSC_VER", SymDefine, _FL);
	int ch = swprintf_s(buf, CountOf(buf), L"%i", _MSC_VER);
	s->Tokens.Add(GeneralPool.Alloc(buf, ch));
	s->File = "none";

	s = Scopes[0]->Define(L"WIN32", SymDefine, _FL);
	s->File = "none";

	#endif
}

struct RoundBracket
{
	int Depth;
	int Sections;
	bool HasPtr;
	
	RoundBracket()
	{
		Depth = 0;
		Sections = 0;
		HasPtr = false;
	}
	
	void Parse(GArray<char16*> &a)
	{
		for (int i=0; i<a.Length(); i++)
		{
			char16 *t = a[i];
			if (CmpToken(t, "__declspec"))
			{
				while (t = a[++i])
				{
					if (CmpToken(t, ")"))
						break;
				}
			}
			else if (CmpToken(t, "("))
			{
				if (Depth == 0)
					Sections++;
				Depth++;
			}
			else if (CmpToken(t, ")"))
			{
				Depth--;
				LgiAssert(Depth >= 0);
			}
			else if (CmpToken(t, "*") && Sections == 1)
			{
				HasPtr = true;
			}
		}
		
		LgiAssert(Depth == 0);
	}
};

GAutoWString GCppParserWorker::GetSymbolName(GArray<char16*> &in)
{
	GArray<char16*> a = in;
	GAutoWString ret;
	GStringPipe p(64);
	RoundBracket Rb;
	int LastName = -1;
	bool FuncPtr = false;
	char16 *t, *prev = NULL;

	Rb.Parse(a);
	if (Rb.Sections == 2 && Rb.HasPtr)
		FuncPtr = true;
		
	Rb.Sections = 0;
	for (int i=0; i<a.Length(); i++)
	{
		t = a[i];
		GSymbol *sym;
		
		KeywordType kt = Keywords.Find(t);
		if (kt)
		{
			if (kt == KwModifier && CmpToken(t, "__declspec"))
			{
				t = a[++i];
				if (!CmpToken(t, "("))
				{
					Msg(MsgError, "%s:%i - unexpected token.\n", _FL);
					break;
				}
				while (t = a[++i])
				{
					if (CmpToken(t, ")"))
					{
						break;
					}
				}
			}
		}
		else if (sym = Resolve(t))
		{
			if (sym->Type == SymDefine)
			{
				a.DeleteAt(i, true);
				for (int n=0; n<sym->Tokens.Length(); n++)
				{
					char16 *s = sym->Tokens[n];
					a.AddAt(i+n, s);
				}
				i--;
			}
			else if (sym->Type == SymTypedef)
			{
				a.DeleteAt(i, true);
				if (i < a.Length())
				{
					char16 *Next = a[i];
					a.DeleteAt(i, true);
					
					for (int n=0; n<sym->Tokens.Length(); n++)
					{
						char16 *s = sym->Tokens[n];
						if (StricmpW(s, t))
							a.AddAt(i+n, s);
						else
							a.AddAt(i+n, Next);
					}
					
					i--;
					
					RoundBracket rb;
					rb.Parse(a);
					if (rb.Sections == 2 && rb.HasPtr)
						FuncPtr = true;
				}
				else
				{
					// This can happen when you define a typedef to twice
					GAutoWString Empty;
					return Empty;
				}
			}
			else if (sym->Type == SymFunction)
			{
				Debug
			}
			else
			{
				goto IsAlpha;
			}
		}
		else if (CmpToken(t, "("))
		{
			if (Rb.Depth == 0)
				Rb.Sections++;
			Rb.Depth++;
		}
		else if (CmpToken(t, ")"))
		{
			Rb.Depth--;
			LgiAssert(Rb.Depth >= 0);
		}
		else if (IsAlpha(*t) || *t == '_')
		{
			IsAlpha:
			if (FuncPtr)
			{
				if (Rb.Depth == 1 &&
					Rb.Sections == 1)
				{
					LastName = i;
				}
			}
			else if (Rb.Depth == 0)
			{
				LastName = i;
			}
		}
		
		prev = t;
	}

	LgiAssert(LastName >= 0);
	t = a[LastName];
	if (CmpToken(t, "n"))
		Debug
	p.Write(t, StrlenW(t) * sizeof(char16) );
	
	ret.Reset(p.NewStrW());
	LgiAssert(ret);
	
	return ret;
}

void GCppParserWorker::Add(WorkUnit *w)
{
	if (Lock(_FL))	
	{
		Work.Add(w);
		Unlock();
		
		Event.Signal();
	}
}

GSymbol *GCppParserWorker::Resolve(char16 *Symbol)
{
	for (int i = Scopes.Length()-1; i >= 0; i--)
	{
		GSourceScope *scope = Scopes[i];
		GSymbol *sym = scope->Find(Symbol);
		if (sym)
			return sym;
	}
	
	return NULL;
}

GVariant *GetArg(GArray<GVariant> &Values, int i)
{
	if (i < 0 || i >= Values.Length())
		return NULL;
	if (Values[i].Type == GV_OPERATOR)
		return NULL;
	return &Values[i];	
}

int FindMatchingBracket(GArray<char16*> &Exp, int Start)
{
	int Depth = 1;
	for (int i = Start + 1; i<Exp.Length(); i++)
	{
		char16 *t = Exp[i];
		if (!StrcmpW(t, L"("))
		{
			Depth++;
		}
		else if (!StrcmpW(t, L")"))
		{
			if (--Depth == 0)
				return i;
		}
	}
	
	return -1;	
}

int GCppParserWorker::Evaluate(GArray<char16*> &Exp)
{
	bool Error = false;
	GArray<GVariant> Values;
	
	for (int i=0; i<Exp.Length(); i++)
	{
		char16 *t = Exp[i];
		if (!StricmpW(t, L"defined"))
		{
			t = Exp[++i];
			CheckToken(t);
			char16 *Define = t;
			if (!StricmpW(t, L"("))
			{
				Define = Exp[++i];
				CheckToken(Define);
				t = Exp[++i];
				CheckToken(t);
				if (StricmpW(t, L")"))
				{
					Error = true;
					break;
				}
			}
			GSymbol *DefineSym = Resolve(Define);
			Values.New() = DefineSym != NULL;
		}
		else if (*t == '(')
		{
			int End = FindMatchingBracket(Exp, i);
			if (End < 0)
			{
				Msg(MsgError, "Couldn't find matching ')'\n");
				Error = true;
				break;
			}

			// Create array of sub-expression tokens and then evaluate that			
			GArray<char16*> Sub;
			for (int n=i+1; n<End; n++)
			{
				Sub.Add(Exp[n]);
			}
			i = End;			
			int Result = Evaluate(Sub);
			Values.New() = Result;
		}
		else if (IsDigit(*t))
		{
			Values.New() = AtoiW(t);
		}
		else
		{
			GOperator Op = IsOp(t, false);
			if (Op != OpNull)
			{
				GVariant &o = Values.New();
				o.Type = GV_OPERATOR;
				o.Value.Op = Op;
			}
			else
			{
				char16 *Next = i < Exp.Length() - 1 ? Exp[i + 1] : NULL;
				if (Next && !StrcmpW(Next, L"("))
				{
					// Macro call
					int End = FindMatchingBracket(Exp, i + 1);
					if (End < 0)
					{
						Error = true;
						Msg(MsgError, "No matching ')'\n");
						break;
					}
					
					GSymbol *Sym = Resolve(t);
					if (Sym)
					{
						Msg(MsgError, "Implement macro calling.\n");
					}

					i = End;
					Values.New() = 0; // No macro defined... just zero it
				}
				else
				{
					// Straight value
					GSymbol *Sym = Resolve(t);
					if (Sym)
					{
						int Val = Evaluate(Sym->Tokens);
						Values.New() = Val;
					}
					else
					{
						Values.New() = 0;
					}
				}
			}
		}
	}

	if (Error)
		return false;
	
	// Evaluate the contents of Values
	while (Values.Length() > 1)
	{
		int StartLength = Values.Length();
		int OpIdx = -1;
		int Prec = -1;
		for (int i=0; i<Values.Length(); i++)
		{
			GVariant &v = Values[i];
			if (v.Type == GV_OPERATOR)
			{
				int p = GetPrecedence(v.Value.Op);
				if (OpIdx < 0 || p < Prec)
				{
					OpIdx = i;
					Prec = p;
				}
			}
		}
		
		if (OpIdx < 0)
		{
			Msg(MsgError, "No Operator?\n");
			break;
		}
		
		GVariant &OpVar = Values[OpIdx];
		OperatorType Type = OpType(OpVar.Value.Op);
		switch (Type)
		{
			case OpPrefix:
			{
				GVariant *a = GetArg(Values, OpIdx + 1);
				if (!a)
				{
					Msg(MsgError, "Missing op arg.\n");
					break;
				}

				switch (OpVar.Value.Op)
				{
					case OpNot:
					{
						OpVar = !a->CastBool();
						break;
					}
					default:
					{
						Msg(MsgError, "Impl this op.\n");
						break;
					}
				}

				Values.DeleteAt(OpIdx + 1, true);
				break;
			}
			case OpInfix:
			{
				GVariant *a = GetArg(Values, OpIdx - 1);
				GVariant *b = GetArg(Values, OpIdx + 1);
				if (!a || !b)
				{
					Msg(MsgError, "Missing op args.\n");
					break;
				}

				switch (OpVar.Value.Op)
				{
					#define CombineUsingOp(op, type, cpp_op) \
						case op: \
						{ \
							*a = a->Cast##type() cpp_op b->Cast##type(); \
							break; \
						}
					CombineUsingOp(OpOr, Bool, ||);
					CombineUsingOp(OpAnd, Bool, &&);
					CombineUsingOp(OpGreaterThan, Int32, >);
					CombineUsingOp(OpGreaterThanEqual, Int32, >=);
					CombineUsingOp(OpLessThan, Int32, <);
					CombineUsingOp(OpLessThanEqual, Int32, <=);
					CombineUsingOp(OpEquals, Int32, ==);
					CombineUsingOp(OpNotEquals, Int32, !=);
					CombineUsingOp(OpPlus, Int32, +);
					CombineUsingOp(OpMinus, Int32, -);
					CombineUsingOp(OpMul, Int32, *);
					CombineUsingOp(OpDiv, Int32, /);
					CombineUsingOp(OpMod, Int32, %);
					default:
					{
						Msg(MsgError, "Impl this op.\n");
						break;
					}
				}

				Values.DeleteAt(OpIdx, true);
				Values.DeleteAt(OpIdx, true);
				break;
			}
			default:
			{
				Msg(MsgError, "Impl this op type.\n");
				break;
			}
		}
		
		
		LgiAssert(StartLength > Values.Length());
		if (Values.Length() >= StartLength)
		{
			Msg(MsgError, "Nothing happened.\n");
			break;
		}
	}

	return Values.Length() == 1 ? Values[0].CastInt32() : 0;
}

GSourceFile *GCppParserWorker::ParseCpp(const char *Path)
{
	GSourceFile *sf = new GSourceFile;
	if (!sf)
		return NULL;

	if (!sf->Read(Path))
	{
		delete sf;
		return NULL;
	}

	Srcs.Add(sf);
	SrcHash.Add(Path, sf);
	
	bool Error = false;
	char16 *t;

	const char16 *CppDef = L"__cplusplus";
	char *Ext = LgiGetExtension((char*)Path);
	if (Ext && stricmp(Ext, "cpp"))
	{
		Scopes[0]->Define((char16*)CppDef, SymDefine, _FL);
	}
	
	while (!Error && (t = sf->NextToken()))
	{
		KeywordType kt = Keywords.Find(t);
		switch (kt)
		{
			case KwPreprocessor:
			{
				if (!ParsePreprocessor(sf, t))
					Error = true;
				break;
			}
			case KwUserType:
			{
				char16 *Start = sf->Cur;
				GSymbol *sym = ParseUserType(sf, t);
				if (sym)
				{
					if (sf->Active)
					{
						GAutoWString SymName = GetSymbolName(sym->Tokens);
						if (SymName)
						{
							CurrentScope()->Add(SymName, sym);
						}
						else
						{
							Msg(MsgError, "%s:%i - Failed to get symbol name.\n", _FL);
							DeleteObj(sym);
						}
					}
					else
					{
						DeleteObj(sym);
					}
				}
				break;
			}
			default:
			{
				char16 *Start = sf->Cur;
				if (CmpToken(t, "typedef"))
				{
					GSymbol *sym = ParseTypedef(sf, t);
				}
				else
				{
					GSymbol *sym = ParseDecl(sf, t);
					if (sym)
					{
						if (sf->Active)
						{
							GAutoWString Name = GetSymbolName(sym->Tokens);
							if (Name)
							{
								Scopes[0]->Add(Name, sym);
							}
							else
							{
								Msg(MsgError, "%s:%i - Failed to get name of decl.\n", _FL);
								DeleteObj(sym);
							}
						}
						else
						{
							DeleteObj(sym);
						}
					}
				}
				break;
			}
		}
	}
	
	return sf;
}

GSymbol *GCppParserWorker::ParseDecl(GSourceFile *sf, char16 *t)
{
	GSymbol *sym = new GSymbol(_FL);
	sym->Type = SymVariable;
	sym->File = sf->Path;
	sym->Line = sf->Line;
	if (CmpToken(t, "friend"))
		sym->FriendDecl = true;
	else
		sym->Tokens.Add(t);

	GArray<char16*> Temp;
	if (CmpToken(t, "template"))
	{
		Temp.Add(t);
		while (t = sf->NextToken())
		{
			Temp.Add(t);
			if (CmpToken(t, ">"))
				break;
		}
	}

	while (t = sf->NextToken())
	{
		if (CmpToken(t, "_lgi_assert"))
		{
			GSymbol *a = Resolve(t);
			int asd=0;
		}
	
		KeywordType kt = Keywords.Find(t);
		if (kt == KwUserType)
		{
			DeleteObj(sym);
			sym = ParseUserType(sf, t);
			break;
		}
		
		if (CmpToken(t, ";"))
			break;
		if (CmpToken(t, "{"))
		{
			int Depth = 1;
			while (t = sf->NextToken())
			{
				if (CmpToken(t, "{"))
				{
					Depth++;					
				}
				else if (CmpToken(t, "}"))
				{
					if (--Depth == 0)
						break;
				}
			}
			break;
		}
		if (CmpToken(t, "("))
		{
			sym->Type = SymFunction;
		}
		
		sym->Tokens.Add(t);
	}

	return sym;
}

GSymbol *GCppParserWorker::ParseTypedef(GSourceFile *sf, char16 *t)
{
	GSymbol *sym = NULL;
	int StartLine = sf->Line;
	GArray<char16*> a;
	
	while (t = sf->NextToken())
	{
		KeywordType kt = Keywords.Find(t);
		if (kt == KwUserType)
		{
			return ParseUserType(sf, t);
		}
		
		if (CmpToken(t, ";"))
			break;
		a.Add(t);
	}
	
	if (sf->Active)
	{
		GAutoWString Name = GetSymbolName(a);
		if (Name)
		{
			if (CmpToken(Name, "HWND"))
			{
				int asd=0;
			}
			
			sym = CurrentScope()->Define(Name, SymTypedef, _FL);
			if (sym)
			{
				sym->File = sf->Path;
				sym->Line = StartLine;
				sym->Tokens = a;
			}
		}
	}
	
	return sym;
}

GSymbol *GCppParserWorker::ParseUserType(GSourceFile *sf, char16 *t)
{
	if (!sf || !t)
		return NULL;

	GSymbol *sym = new GSymbol(_FL);
	int InScope = 0;
	bool InParentList = false;
	
	if (CmpToken(t, "union"))
		sym->Type = SymUnion;
	else if (CmpToken(t, "class"))
		sym->Type = SymClass;
	else if (CmpToken(t, "struct"))
		sym->Type = SymStruct;
	else if (CmpToken(t, "enum"))
		sym->Type = SymEnum;
	else
	{
		delete sym;
		return NULL;
	}	
	
	sym->Tokens.Add(t);
	sym->File = sf->Path;
	sym->Line = sf->Line;
	
	while (t = sf->NextToken())
	{
		if (CmpToken(t, "{"))
		{
			InScope++;
		}
		else if (CmpToken(t, "}"))
		{
			ProcessEndBracket:
			if (--InScope == 0)
			{
				while (t = sf->NextToken())
				{
					if (CmpToken(t, ";"))
						break;
					sym->Tokens.Add(t);
				}

				break;
			}
		}
		else if (CmpToken(t, ":"))
		{
			if (InScope == 0)
			{
				InParentList = true;
			}
		}
		else if (CmpToken(t, ";"))
		{
			LgiAssert(!InScope);
			break;
		}
		else
		{
			KeywordType kt = Keywords.Find(t);
			if (kt == KwVisibility)
			{
			}
			else if (kt == KwPreprocessor)
			{
				ParsePreprocessor(sf, t);
			}
			else if (kt == KwUserType)
			{
				GSymbol *cls = ParseUserType(sf, t);
				if (cls)
				{
					if (sf->Active)
					{
						sym->Add(cls);
					}
					else
					{
						DeleteObj(cls);
					}
				}
			}
			else if (InScope > 0)
			{
				if (sym->Type == SymEnum)
				{
					GSymbol *def = new GSymbol(_FL);
					if (def)
					{
						bool GotEquals = false;
						
						def->File = sf->Path;
						def->Line = sf->Line;
						
						while (t = sf->NextToken())
						{
							if (CmpToken(t, ","))
								break;
							
							if (CmpToken(t, "}"))
								goto ProcessEndBracket;

							if (CmpToken(t, "="))
								GotEquals = true;
							else if (GotEquals)							
								def->Tokens.Add(t);
						}
						
						sym->Add(def);
					}
					else
					{
						Msg(MsgError, "%s:%i - Failed to create defn.\n", _FL);
					}
				}
				else
				{
					GSymbol *def = ParseDecl(sf, t);
					if (def)
					{
						if (sf->Active && !def->FriendDecl)
						{
							sym->Add(def);
						}
						else
						{
							DeleteObj(def);
						}
					}
					else
					{
						Msg(MsgError, "%s:%i - Failed to parse defn.\n", _FL);
					}
				}
			}
			else if (InParentList)
			{
			}
			else
			{
				sym->Tokens.Add(t);
			}
		}
	}
	
	return sym;
}

char16 *FindEol(char16 *s)
{
	char16 *LastNonWhite = NULL;
	while (*s)
	{
		if (!strchr(WhiteSpace, *s))
			LastNonWhite = s;
		else if (*s == '\n')
		{
			if (!LastNonWhite || *LastNonWhite != '\\')	
				break;
		}
		s++;
	}
	return s;
}

bool HasNonWhiteSpace(char16 *Start, char16 *End)
{
	while (*Start && Start < End)
	{
		if (Start[0] == '/' &&
			Start[1] == '/')
		{
			// Single line comments don't contribute to non-whitespace
			for (Start += 2; Start<End; Start++)
			{
				if (*Start == '\n')
					break;
			}
			if (Start >= End)
				return false;
			else
				continue;
		}
		else if (Start[0] == '/' &&
				 Start[1] == '*')
		{
			// Start of multi-line comment that doesn't contribute to non-whitespace
			for (Start += 2; Start<End; Start++)
			{
				if (Start[0] == '*' &&
					Start[1] == '/')
				{
					Start += 2;
					break;
				}
			}
			if (Start >= End)
				return false;
			else
				continue;
		}
		
		if (!strchr(" \t\r", *Start))
		{
			return true;
		}
		
		*Start++;
	}
	return false;
}

bool GCppParserWorker::ParsePreprocessor(GSourceFile *sf, char16 *t)
{
	if (!t)
		return false;
		
	if (CmpToken(t, "#if"))
	{
		char16 *Start = sf->Cur;
		char16 *End = FindEol(Start);

		PreprocessState &ps = sf->Stack.New();
		ps.ParentIsActive = sf->Active;

		GArray<char16*> Exp;
		while (HasNonWhiteSpace(sf->Cur, End))
		{
			t = sf->NextToken();
			if (!t)
				break;
			Exp.Add(t);
		}

		ps.IfBranchClaimed = Evaluate(Exp) != 0;
		if (sf->Active)
			sf->Active = ps.IfBranchClaimed;
	}
	else if (CmpToken(t, "#elif"))
	{
		char16 *Start = sf->Cur;					
		char16 *End = FindEol(Start);
		
		GArray<char16*> Exp;
		while (HasNonWhiteSpace(sf->Cur, End))
		{
			t = sf->NextToken();
			if (!t)
				break;
			Exp.Add(t);
		}
		
		PreprocessState &ps = sf->Stack.Last();
		if (!ps.IfBranchClaimed)
		{
			ps.IfBranchClaimed = Evaluate(Exp) != 0;
			if (sf->Active)
				sf->Active = ps.IfBranchClaimed;
		}
	}
	else if (CmpToken(t, "#ifndef"))
	{
		t = sf->NextToken();
		if (!t)
			return false;

		GSymbol *def = Resolve(t);

		PreprocessState &ps = sf->Stack.New();
		ps.ParentIsActive = sf->Active;
		ps.IfBranchClaimed = def == NULL || def->Type != SymDefine;

		if (sf->Active)
			sf->Active = ps.IfBranchClaimed;
	}
	else if (CmpToken(t, "#ifdef"))
	{
		t = sf->NextToken();
		if (!t)
			return false;

		GSymbol *def = Resolve(t);
		PreprocessState &ps = sf->Stack.New();
		ps.ParentIsActive = sf->Active;
		ps.IfBranchClaimed = def != NULL && def->Type == SymDefine;
		if (sf->Active)
			sf->Active = ps.IfBranchClaimed;
	}
	else if (CmpToken(t, "#else"))
	{
		PreprocessState &ps = sf->Stack.Last();
		if (!ps.IfBranchClaimed)
		{
			ps.IfBranchClaimed = true;
			if (ps.ParentIsActive)
				sf->Active = true;
		}
	}
	else if (CmpToken(t, "#endif"))
	{
		LgiAssert(sf->Stack.Length() > 0);
		PreprocessState &ps = sf->Stack.Last();
		sf->Active = ps.ParentIsActive;
		sf->Stack.Length(sf->Stack.Length() - 1) ;
	}
	else if (CmpToken(t, "#pragma") ||
			 CmpToken(t, "#error") ||
			 CmpToken(t, "#warning"))
	{
		char16 *eol = StrchrW(sf->Cur, '\n');
		if (!eol)
			return false;

		sf->Cur = eol;
	}
	else if (CmpToken(t, "#define"))
	{
		t = sf->NextToken();
		if (!t)
			return false;

		GSymbol *sym = NULL;
		if (sf->Active)
		{
			sym = Scopes[0]->Define(t, SymDefine, _FL);
			sym->File = sf->Path;
			sym->Line = sf->Line;
			sym->Tokens.Length(0);
		}
		
		char16 *End = FindEol(sf->Cur);
		while (HasNonWhiteSpace(sf->Cur, End))
		{
			// Look for and skip line-continuation mark
			char16 *s = sf->Cur;
			while (strchr(WhiteSpace, *s))
				s++;
			if (*s == '\\')
			{
				*s++;
				while (strchr(" \r\t", *s))
					s++;
				if (*s == '\n')
				{
					sf->Cur = s + 1;
					sf->Line++;
				}
			}
			
			t = sf->NextToken();
			if (!t)
				break;
				
			if (sym)
				sym->Tokens.Add(t);
		}
	}
	else if (CmpToken(t, "#undef"))
	{
		t = sf->NextToken();
		if (!t)
			return false;

		GSymbol *s = Scopes[0]->Define(t, SymDefine, _FL);
		if (s)
		{
			Scopes[0]->Delete(t);
			delete s;
		}					
	}
	else if (CmpToken(t, "#include"))
	{
		// Include a file
		t = sf->NextToken();
		if (!t)
			return false;
		
		char16 *FileName = NULL;
		if (!StricmpW(t, L"<"))
		{
			// System include
			char16 *End = StrchrW(sf->Cur, '>');
			if (!End)
				return false;
			FileName = sf->Pool.Alloc(sf->Cur, End - sf->Cur);
			if (!FileName)
				return false;
			sf->Cur = End + 1;
		}
		else
		{
			// Basic include
			FileName = t + 1;
			char16 *e = StrrchrW(t, '\"');
			if (e) *e = 0;
		}
		
		GAutoString FileName8(LgiNewUtf16To8(FileName));
		if (FileName8)
		{
			// Resolve filename
			char p[MAX_PATH];
			bool Exists = false;
			if (LgiIsRelativePath(FileName8))
			{
				LgiMakePath(p, sizeof(p), sf->Path, "..");
				LgiMakePath(p, sizeof(p), p, FileName8);
				Exists = FileExists(p);
			}

			for (int i=0; !Exists && i<w->IncludePaths.Length(); i++)
			{
				LgiMakePath(p, sizeof(p), w->IncludePaths[i], FileName8);
				Exists = FileExists(p);							
			}
			
			if (Exists)
			{
				ParseCpp(p);
			}
		}
	}
	else
	{
		LgiAssert(!"Impl me");
		return false;
	}
	
	return true;
}

void GCppParserWorker::DoWork()
{
	if (w->SearchTerm)
	{
		// Do search...
	}
	else if (w->Source.Length())
	{
		// Parse source code...
		for (int i=0; i<w->Source.Length(); i++)
		{
			char *ext = LgiGetExtension(w->Source[i]);
			if (!ext)
				continue;
			
			if (!stricmp(ext, "c") ||
				!stricmp(ext, "cpp") ||
				!stricmp(ext, "h"))
			{
				InitScopes();
				
				LgiTrace("Compiling '%s'\n", w->Source[i].Get());
				ParseCpp(w->Source[i]);
				
				#if 0
				char16 *k;
				for (GSymbol *s = Scopes[0]->First(&k); s; s = Scopes[0]->Next(&k))
				{
					LgiTrace("    %S = %p\n", k, s);
				}
				#endif
			}
			else
			{
				// No parser impl yet
			}
		}
	}
	else LgiAssert(!"Unknown work unit type.");
}

int GCppParserWorker::Main()
{
	while (Loop)
	{
		GThreadEvent::WaitStatus s = Event.Wait();
		switch (s)
		{
			case GThreadEvent::WaitSignaled:
			{
				if (!Loop)
					break;
				
				if (Lock(_FL))
				{
					w = NULL;
					if (Work.Length() > 0)
					{
						w = Work[0];
						Work.DeleteAt(0, true);
					}
					Unlock();
					if (w)
					{
						DoWork();
						w = NULL;
					}
				}
				break;
			}
			default:
			{
				Loop = false;
				break;
			}
		}
	}
	
	return 0;
}

GCppParser::GCppParser()
{
	d = new GCppParserPriv;
}

GCppParser::~GCppParser()
{
	delete d;
}

void GCppParser::ParseCode
(
	GArray<const char*> &IncludePaths,
	GArray<ValuePair*> &PreDefines,
	GArray<char*> &Source
)
{
	WorkUnit *w = new WorkUnit(IncludePaths, PreDefines, Source);
	if (w)
	{
		if (!d->Worker)
			d->Worker.Reset(new GCppParserWorker(d));
		d->Worker->Add(w);
	}
}

void GCppParser::Search(const char *Str, SearchResultsCb Callback, void *CallbackData)
{
	WorkUnit *w = new WorkUnit(Str);
	if (w)
	{
		if (!d->Worker)
			d->Worker.Reset(new GCppParserWorker(d));
		d->Worker->Add(w);
	}
}

