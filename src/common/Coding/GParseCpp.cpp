#include "Lgi.h"
#include "GParseCpp.h"
#include "GThreadEvent.h"
#include "GLexCpp.h"
#include "GVariant.h"
#include "GScriptingPriv.h"

enum StackType
{
	StackNone,
	StackCompileActive,
	StackCompileInactive,
	StackScope,
};

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

struct GSourceFile
{
	uint32 Id;
	GAutoString Path;
	GArray<GSymbol> Syms;
	GCppStringPool Pool;
	
	GSourceFile() : Pool(16 << 10)
	{
		Id = 0;
	}
};

struct GSourceScope : public GHashTbl<char16*, GSymbol*>
{
	GSymbol *Define(char16 *name, GSymbolType Type)
	{
		GSymbol *s = Find(name);
		if (!s) Add(name, s = new GSymbol);
		s->Type = Type;
		s->File = NULL;
		s->Line = 0;		
		return s;
	}
	
	~GSourceScope()
	{
		DeleteObjects();
	}
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
	
	void InitScopes();
	
public:
	GCppParserWorker(GCppParserPriv *priv);
	~GCppParserWorker();
	
	void Add(WorkUnit *w);
	GSourceFile *ParseCpp(const char *Path);	
	GSymbol *Resolve(char16 *Symbol);
	int Evaluate(GArray<char16*> &Exp);
	void DoWork();
	int Main();
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

void GCppParserWorker::InitScopes()
{
	Scopes.DeleteObjects();

	Scopes.Add(new GSourceScope);
	
	#ifdef WIN32
	char16 buf[256];

	GSymbol *s = Scopes[0]->Define(L"_MSC_VER", SymDefine);
	int ch = swprintf_s(buf, CountOf(buf), L"%i", _MSC_VER);
	s->Tokens.Add(GeneralPool.Alloc(buf, ch));
	s->File = "none";

	s = Scopes[0]->Define(L"WIN32", SymDefine);
	s->File = "none";

	#endif
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

char16 *LexPoolAlloc(void *Context, const char16 *s, int len)
{
	return ((GCppStringPool*)Context)->Alloc(s, len);
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

#define NextToken() LexCpp(cur, LexPoolAlloc, &sf->Pool, &CurLine)
#define CheckToken(t) if (!t) { Error = true; break; }

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
				LgiAssert(!"Couldn't find matching ')'");
				Error = true;
				break;
			}

			// Create array of sub-expression tokens and then evaulate that			
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
						LgiAssert(!"No matching ')'");
						break;
					}
					
					GSymbol *Sym = Resolve(t);
					if (Sym)
					{
						LgiAssert(!"Implement macro calling.");
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
			LgiAssert(!"No Operator?");
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
					LgiAssert(!"Missing op arg.");
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
						LgiAssert(!"Impl this op");
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
					LgiAssert(!"Missing op args.");
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
						LgiAssert(!"Impl this op");
						break;
					}
				}

				Values.DeleteAt(OpIdx, true);
				Values.DeleteAt(OpIdx, true);
				break;
			}
			default:
			{
				LgiAssert(!"Impl this op type");
				break;
			}
		}
		
		
		LgiAssert(StartLength > Values.Length());
		if (Values.Length() >= StartLength)
		{
			LgiAssert(!"Nothing happened");
			break;
		}
	}

	return Values.Length() == 1 ? Values[0].CastInt32() : 0;
}

GSourceFile *GCppParserWorker::ParseCpp(const char *Path)
{
	GAutoString f(ReadTextFile(Path));
	if (!f)
		return NULL;

	GSourceFile *sf = new GSourceFile;
	if (!sf)
		return NULL;

	sf->Path.Reset(NewStr(Path));
	Srcs.Add(sf);
	SrcHash.Add(Path, sf);
	
	// Lex the source code
	GAutoWString FileW(LgiNewUtf8To16(f));
	char16 *t, *cur = FileW;
	int CurLine = 1;
	bool Error = false;
	bool Active = true;
	GArray<StackType> Stack;
	
	while (!Error)
	{
		char16 *prev = cur;
		t = NextToken();
		if (!t)
			break;
		
		ProcessToken:
		switch (*t)
		{
			case '#':
			{
				if (!StrcmpW(t, L"#if"))
				{
					char16 *Start = cur;					
					int StartLine = CurLine;

					Stack.Add(Active ? StackCompileActive : StackCompileInactive);

					GArray<char16*> Exp;
					while (t = NextToken())
					{
						if (CurLine != StartLine)
							break;
						Exp.Add(t);
					}
					if (Active)
						Active = Evaluate(Exp) != 0;
					
					goto ProcessToken;
				}
				else if (!StrcmpW(t, L"#elif"))
				{
					char16 *Start = cur;					
					int StartLine = CurLine;
					
					GArray<char16*> Exp;
					while (t = NextToken())
					{
						if (CurLine != StartLine)
							break;
						Exp.Add(t);
					}
					Active = Evaluate(Exp) != 0;
					goto ProcessToken;
				}
				else if (!StrcmpW(t, L"#ifndef"))
				{
					t = NextToken();
					CheckToken(t);

					GSymbol *def = Resolve(t);
					Stack.Add(Active ? StackCompileActive : StackCompileInactive);
					Active = Active &&
							(
								def == NULL ||
								def->Type != SymDefine
							);
				}
				else if (!StrcmpW(t, L"#ifdef"))
				{
					t = NextToken();
					CheckToken(t);

					GSymbol *def = Resolve(t);
					Stack.Add(Active ? StackCompileActive : StackCompileInactive);
					Active = Active &&
							def != NULL &&
							def->Type == SymDefine;
				}
				else if (!StrcmpW(t, L"#else"))
				{
					Active = !Active;
				}
				else if (!StrcmpW(t, L"#endif"))
				{
					LgiAssert(Stack.Length() > 0);
					if (Stack.Last() == StackCompileActive)
						Active = true;
					else if (Stack.Last() == StackCompileInactive)
						Active = false;
					else
						LgiAssert(!"Unexpected #endif");
						
					Stack.Length(Stack.Length() - 1) ;
				}
				else if (!StrcmpW(t, L"#pragma") ||
						 !StrcmpW(t, L"#error") ||
						 !StrcmpW(t, L"#warning"))
				{
					char16 *eol = StrchrW(cur, '\n');
					CheckToken(eol);
					cur = eol;
				}
				else if (!StrcmpW(t, L"#define"))
				{
					t = NextToken();
					CheckToken(t);

					GSymbol *s = NULL;
					if (Active)
					{
						s = Scopes[0]->Define(t, SymDefine);
						s->File = sf->Path;
						s->Line = CurLine;
						s->Tokens.Length(0);
					}
					
					int LastLine = CurLine;
					while (t = NextToken())
					{
						if (CurLine != LastLine)
							goto ProcessToken;
						
						if (!StrcmpW(t, L"\\"))
							LastLine++;
						else if (s)
							s->Tokens.Add(t);
					}
				}
				else if (!StrcmpW(t, L"#undef"))
				{
					t = NextToken();
					CheckToken(t);

					GSymbol *s = Scopes[0]->Define(t, SymDefine);
					if (s)
					{
						Scopes[0]->Delete(t);
						delete s;
					}					
				}
				else if (!StrcmpW(t, L"#include"))
				{
					// Include a file
					t = NextToken();
					CheckToken(t);
					
					char16 *FileName = NULL;
					if (!StricmpW(t, L"<"))
					{
						// System include
						char16 *End = StrchrW(cur, '>');
						CheckToken(End);
						FileName = sf->Pool.Alloc(cur, End - cur);
						CheckToken(FileName);
						cur = End + 1;
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
							LgiMakePath(p, sizeof(p), Path, "..");
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
							GSourceFile *IncSf = SrcHash.Find(p);
							if (!IncSf)
							{
								IncSf = ParseCpp(p);
							}
							if (IncSf)
							{
								// Add all symbols to the global scope
							}
						}
					}
				}
				else
				{
					LgiAssert(!"Impl me");
				}
				break;
			}
			default:
			{
				if (IsAlpha(*t))
				{
					// Variable
				}
				else
				{
					int asd=0;
				}
				break;
			}
		}
	}
	
	return sf;
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

