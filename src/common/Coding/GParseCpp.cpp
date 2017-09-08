#include <stdio.h>
#include <wchar.h>

#include "Lgi.h"
#include "GParseCpp.h"
#include "LThreadEvent.h"
#include "GLexCpp.h"
#include "GVariant.h"
#include "GScriptingPriv.h"
#include "GXmlTree.h"
#include "GToken.h"

#define CheckToken(t)	if (!t) { Error = true; break; }
#if defined(WINDOWS) && defined(_MSC_VER) && !defined(WIN64)
#define Debug			__asm int 3
#else
#define Debug			assert(0)
#endif

static const char16 sRoundBracketStart[] = { '(', 0 };
static const char16 sRoundBracketEnd[] = { ')', 0 };
static const char16 sDefined[] = {'d','e','f','i','n','e','d',0};
static const char16 sCpp[] = {'_','_','c','p','l','u','s','p','l','u','s',0};

enum LoopState
{
	LsLooping,
	LsFinished,
	LsError
};

struct PreprocessState
{
	bool ParentIsActive;
	bool IfBranchClaimed;
};

enum PreprocessSymbol
{
	SourceBlock,
	HashIf,
	HashElif,
	HashIfndef,
	HashIfdef,
	HashElse,
	HashEndif,
	HashPragma,
	HashError,
	HashWarning,
	HashDefine,
	HashUndef,
	HashInclude,
};

const char *PreprocessSymbolNames[] =
{
	"SourceBlock",
	"HashIf",
	"HashElif",
	"HashIfndef",
	"HashIfdef",
	"HashElse",
	"HashEndif",
	"HashPragma",
	"HashError",
	"HashWarning",
	"HashDefine",
	"HashUndef",
	"HashInclude",
};

/// A file is parsed into a stream of these preprocessed blocks. The content
/// of the block may be present in unlexed form, or parsed form. This saves
/// on having to re-lex files over and over.
class PreprocessBlock
{
	friend struct GSourceFile;
	
	GArray<int> Lines;

public:
	/// The type of block this is.
	PreprocessSymbol Type;
	
	/// The start line in the original source code of this block.
	int BlockLine;
	
	// Unparsed string data
	char16 *Start;
	int Len;
	
	// -or-
	
	// Lexed data
	int CurToken;
	int Line;
	GArray<char16*> Tokens;
	
	PreprocessBlock()
	{
		Type = SourceBlock;
		Start = NULL;
		Len = 0;
		CurToken = 0;
		BlockLine = 0; // Invalid line..
	}
	
	char16 *NextToken()
	{
		if (Start != NULL)
			LgiAssert(0); // Call GSourceFile::Lex first
			
		if (CurToken >= Tokens.Length())
			return NULL;
			
		Line = Lines[CurToken];
		return Tokens[CurToken++];
	}
	
	int GetLine()
	{
		if (CurToken >= Tokens.Length())
			Lines.Last();
		
		return Lines[CurToken];		
	}
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
	
	char16 *Alloc(const char16 *s, ssize_t len)
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

char16 *LexPoolAlloc(void *Context, const char16 *s, ssize_t len)
{
	return ((GCppStringPool*)Context)->Alloc(s, len);
}

struct GSourceFile
{
	uint32 Id;
	GAutoString Path;
	GCppStringPool Pool;

	GAutoWString Raw;
	bool Active;
	GArray<PreprocessState> Stack;

	int CurBlock;
	GArray<PreprocessBlock> Blocks;
	PreprocessBlock *Block;
	
	GSourceFile() : Pool(64 << 10)
	{
		Id = 0;
		CurBlock = 0;
		Block = NULL;
		Active = true;
	}

	PreprocessBlock *Current()
	{
		if (!Block && CurBlock == 0)
			Block = &Blocks[CurBlock];
		return Block;
	}

	PreprocessBlock *Next()
	{
		CurBlock++;
		if (CurBlock >= Blocks.Length())
		{
			Block = NULL;
			return NULL;
		}
		
		Block = &Blocks[CurBlock];
		return Block;
	}
	
	bool Read(const char *path)
	{
		if (!Path.Reset(NewStr(path)))
			return false;
		
		GAutoString f(ReadTextFile(Path));
		if (!f)
			return false;

		Raw.Reset(Utf8ToWide(f));

		return true;
	}
	
	bool Lex(PreprocessBlock &blk)
	{
		if (blk.Start)
		{
			char16 *Cur = blk.Start;
			char16 *End = blk.Start + blk.Len;
			int Line = blk.BlockLine;
			char16 *t;

			// Temporarily NULL terminate this block
			char16 OldEndChar = *End;
			*End = 0;

			if (blk.Type == HashInclude)
			{
				// Special include parsing...
				while (*Cur && (t = LexCpp(Cur, LexPoolAlloc, &Pool, &Line)))
				{
					if (CmpToken(t, "<"))
					{
						char16 delim = '>';
						char16 *End = StrchrW(t = Cur, delim);
						*End = 0;
					}
					else if (*t == '\"' || *t == '\'')
					{
						// Trim the string markers
						char16 delim = *t++;
						char16 *End = StrchrW(t, delim);
						*End = 0;
					}
					
					blk.Tokens.Add(t);
					blk.Lines.Add(Line);
					break;
				}
			}
			else if (blk.Type == HashDefine)
			{
				// Special #define parsing to remove the line continuation characters...
				while (*Cur && (t = LexCpp(Cur, LexPoolAlloc, &Pool, &Line)))
				{
					if (CmpToken(t, "\\"))
					{
						// Is this a line continuation char?
						char16 *End = Cur;
						bool AllWhiteSpace = true;
						while (*End && *End != '\r' && *End != '\n')
						{
							if (*End != ' ' && *End != '\t')
							{
								AllWhiteSpace = false;
								break;
							}
							End++;
						}
						if (AllWhiteSpace)
						{
							continue;
						}
					}
					
					blk.Tokens.Add(t);
					blk.Lines.Add(Line);
				}
			}
			else
			{
				// General code parsing
				while (*Cur && (t = LexCpp(Cur, LexPoolAlloc, &Pool, &Line)))
				{
					blk.Tokens.Add(t);
					blk.Lines.Add(Line);
				}			
			}

			// Restore the string to normal
			*End = OldEndChar;
			
			// Clear the unparsed data ptr
			blk.Start = NULL;
		}
		
		return NULL;
	}
	
	void DumpBlocks()
	{
		#if 1
		GXmlTree t;
		GFile f;
		char *Leaf = strrchr(Path, DIR_CHAR);
		char Out[MAX_PATH];
		LgiMakePath(Out, sizeof(Out), "c:\\temp", Leaf + 1);
		char *Ext = LgiGetExtension(Out);
		strcpy(Ext, "xml");
		if (f.Open(Out, O_WRITE))
		{
			f.SetSize(0);
			GXmlTag r("Dump");
			r.SetAttr("Path", Path);
			for (int i=0; i<Blocks.Length(); i++)
			{
				PreprocessBlock &b = Blocks[i];
				GXmlTag *c = new GXmlTag(PreprocessSymbolNames[b.Type]);
				r.InsertTag(c);
				c->SetAttr("Line", b.BlockLine);
				
				GAutoString a(WideToUtf8(b.Start, b.Len));
				c->SetContent(a);
			}
			
			t.Write(&r, &f);
			f.Close();
		}
		else LgiAssert(0);
		#endif		
	}

	/*	
	bool At(const char *file, int line)
	{
		char *d = Path ? strrchr(Path, DIR_CHAR) : NULL;
		if (!d)
			return false;
		return stricmp(d+1, file) == 0 && Line == line;
	}
	*/
};

struct GSourceScope : public GHashTbl<char16*, GSymbol*>
{
	GSourceScope() : GHashTbl<char16*, GSymbol*>(5000, true)
	{
		SetStringPool(true);
	}

	~GSourceScope()
	{
		DeleteObjects();
	}

	bool Add(char16 *k, GSymbol *v)
	{
		if (Find(k))
		{
			return false;
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
	MsgWarning,
	MsgInfo,
};

template<typename T>
void AddHash(GHashTbl<char16*, T> &tbl, const char *name, T type)
{
	char16 s[256], *o = s;
	while (*name)
		*o++ = *name++;
	*o++ = 0;
	tbl.Add(s, type);
}

struct GCppParserWorker : public GCompileTools
{
	GCppParserPriv *d;
	GAutoPtr<WorkUnit> w;
	GCppStringPool GeneralPool;
	
	GArray<GSourceFile*> Srcs;
	GArray<GSourceScope*> Scopes; // from global to local
	
	GHashTbl<char16*, KeywordType> Keywords;
	GHashTbl<char16*, PreprocessSymbol> PreprocessSyms;
	
	GHashTbl<char*, char*> IncludePathFiles;
	char *FindInclude(char *file);
	bool Preprocess(GSourceFile *sf);
	
	void InitScopes();
	GSourceScope *CurrentScope() { return Scopes[Scopes.Length()-1]; }
	GAutoWString GetSymbolName(GArray<char16*> &a, bool IsEnum = false);

	GSymbol *ParseDecl(GSourceFile *sf, char16 *t);
	GSymbol *ParseUserType(GSourceFile *sf, char16 *t);
	GSymbol *ParseTypedef(GSourceFile *sf);
	bool ParsePreprocessor(GSourceFile *sf);
	
public:
	GCppParserWorker(GCppParserPriv *priv);
	~GCppParserWorker();
	
	GSourceFile *ParseCpp(const char *Path);	
	GSymbol *Resolve(char16 *Symbol);
	int Evaluate(GArray<char16*> &Exp);
	void DoWork(WorkUnit *wk);
	int Main();
	void Msg(MsgType Type, const char *Fmt, ...);
};

struct GCppParserPriv
{
	GArray<char*> IncPaths;
	GAutoPtr<GCppParserWorker> Worker;
};

GCppParserWorker::GCppParserWorker(GCppParserPriv *priv) :
	GeneralPool(4 << 10),
	IncludePathFiles(0, false),
	PreprocessSyms(0, false)
{
	d = priv;
	
	AddHash(Keywords, "auto", KwSpecifier);
	AddHash(Keywords, "extern", KwSpecifier);
	AddHash(Keywords, "static", KwSpecifier);
	AddHash(Keywords, "register", KwSpecifier);
	AddHash(Keywords, "mutable", KwSpecifier);

	AddHash(Keywords, "bool", KwType);
	AddHash(Keywords, "char", KwType);
	AddHash(Keywords, "wchar_t", KwType);
	AddHash(Keywords, "short", KwType);
	AddHash(Keywords, "int", KwType);
	AddHash(Keywords, "long", KwType);
	AddHash(Keywords, "float", KwType);
	AddHash(Keywords, "double", KwType);
	AddHash(Keywords, "void", KwType);
	#ifdef WIN32
	AddHash(Keywords, "__int64", KwType);
	AddHash(Keywords, "_W64", KwModifier);
	#endif
	AddHash(Keywords, "unsigned", KwModifier);
	AddHash(Keywords, "signed", KwModifier);
	AddHash(Keywords, "__declspec", KwModifier);
	AddHash(Keywords, "template", KwModifier);
	AddHash(Keywords, "__stdcall", KwModifier);
	AddHash(Keywords, "friend", KwModifier);
	AddHash(Keywords, "operator", KwModifier);
	
	AddHash(Keywords, "class", KwUserType);
	AddHash(Keywords, "struct", KwUserType);
	AddHash(Keywords, "union", KwUserType);
	AddHash(Keywords, "enum", KwUserType);
	
	AddHash(Keywords, "public:", KwVisibility);
	AddHash(Keywords, "private:", KwVisibility);
	AddHash(Keywords, "protected:", KwVisibility);

	AddHash(Keywords, "#if", KwPreprocessor);
	AddHash(Keywords, "#ifdef", KwPreprocessor);
	AddHash(Keywords, "#ifndef", KwPreprocessor);
	AddHash(Keywords, "#elif", KwPreprocessor);
	AddHash(Keywords, "#else", KwPreprocessor);
	AddHash(Keywords, "#endif", KwPreprocessor);
	AddHash(Keywords, "#include", KwPreprocessor);
	AddHash(Keywords, "#define", KwPreprocessor);
	AddHash(Keywords, "#undef", KwPreprocessor);
	AddHash(Keywords, "#error", KwPreprocessor);
	AddHash(Keywords, "#warning", KwPreprocessor);
	AddHash(Keywords, "#pragma", KwPreprocessor);

	AddHash(PreprocessSyms, "#if", HashIf);
	AddHash(PreprocessSyms, "#elif", HashElif);
	AddHash(PreprocessSyms, "#ifndef", HashIfndef);
	AddHash(PreprocessSyms, "#ifdef", HashIfdef);
	AddHash(PreprocessSyms, "#else", HashElse);
	AddHash(PreprocessSyms, "#endif", HashEndif);
	AddHash(PreprocessSyms, "#pragma", HashPragma);
	AddHash(PreprocessSyms, "#error", HashError);
	AddHash(PreprocessSyms, "#warning", HashWarning);
	AddHash(PreprocessSyms, "#define", HashDefine);
	AddHash(PreprocessSyms, "#undef", HashUndef);
	AddHash(PreprocessSyms, "#include", HashInclude);
}

GCppParserWorker::~GCppParserWorker()
{
	Srcs.DeleteObjects();
	Scopes.DeleteObjects();
	IncludePathFiles.DeleteArrays();
}

void GCppParserWorker::Msg(MsgType Type, const char *Fmt, ...)
{
	va_list Arg;
	va_start(Arg, Fmt);
	#ifdef WIN32
	char Buf[256];
	#ifdef _MSC_VER
	vsnprintf_s(Buf, sizeof(Buf), _TRUNCATE, Fmt, Arg);
	#else
	vsnprintf(Buf, sizeof(Buf), Fmt, Arg);
	#endif
	OutputDebugStringA(Buf);
	#else
	vprintf(Fmt, Arg);
	#endif
	va_end(Arg);
	if (Type == MsgError)
		Debug;
}

char *GCppParserWorker::FindInclude(char *File)
{
	if (IncludePathFiles.Length() == 0)
	{
		for (int i=0; i<w->IncludePaths.Length(); i++)
		{
			GDirectory dir;
			char p[MAX_PATH];
			for (bool b=dir.First(w->IncludePaths[i]); b; b=dir.Next())
			{
				char *Leaf = dir.GetName();
				if (!dir.IsDir())
				{
					char *ext = LgiGetExtension(Leaf);
					if (ext && ext[0] == 'h' && !ext[1])
					{
						if (dir.Path(p, sizeof(p)))
						{
							if (!IncludePathFiles.Find(Leaf))
								IncludePathFiles.Add(Leaf, NewStr(p));
						}
					}
				}
			}
		}
	}
	
	char *p = IncludePathFiles.Find(File);
	return p;
}

void GCppParserWorker::InitScopes()
{
	Scopes.DeleteObjects();

	Scopes.Add(new GSourceScope);
	
	#ifdef WIN32
	char16 buf[256];
	GSymbol *s;

	#ifdef _MSC_VER
	s = Scopes[0]->Define(L"_MSC_VER", SymDefineValue, _FL);
	int ch = swprintf_s(buf, CountOf(buf), L"%i", _MSC_VER);
	s->Tokens.Add(GeneralPool.Alloc(buf, ch));
	s->File = "none";
	#endif

	s = Scopes[0]->Define(L"WIN32", SymDefineValue, _FL);
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
				while ((t = a[++i]))
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

GAutoWString GCppParserWorker::GetSymbolName(GArray<char16*> &in, bool IsEnum)
{
	GArray<char16*> a = in;
	GAutoWString ret;
	GStringPipe p(64);
	RoundBracket Rb;
	int LastName = -1;
	bool FuncPtr = false;
	int Operator = -1;
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
			if (kt == KwModifier)
			{
				if (CmpToken(t, "__declspec"))
				{
					t = a[++i];
					if (!CmpToken(t, "("))
					{
						Msg(MsgError, "%s:%i - unexpected token.\n", _FL);
						break;
					}
					while ((t = a[++i]))
					{
						if (CmpToken(t, ")"))
						{
							break;
						}
					}
				}				
				else if (CmpToken(t, "operator"))
				{
					Operator = i;
				}
			}
		}
		else if ((sym = Resolve(t)))
		{
			if (sym->Type == SymDefineValue ||
				sym->Type == SymDefineFunction)
			{
				if (sym->Type == SymDefineFunction)
				{
					// Resolve the arguments...
					t = a[++i];
					if (!CmpToken(t, "("))
					{
						Msg(MsgError, "Missing '(' in call to '%S' define.\n", t);
						break;
					}
					
					int Depth = 1;
					while ((t = a[++i]))
					{
						if (CmpToken(t, "("))
						{
							Depth++;
						}
						else if (CmpToken(t, ")"))
						{
							if (--Depth <= 0)
								break;

						}
					}

					Msg(MsgWarning, "NotImpl: Parsing of #define arguments.\n");
				}
				
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

	if (Operator >= 0)
	{
		for (int i=Operator; i<a.Length(); i++)
		{
			char16 *t = a[i];
			if (i > Operator)
				p.Write(L"_", sizeof(char16));
			p.Write(t, StrlenW(t) * sizeof(char16) );
		}
	}
	else
	{
		if (IsEnum && LastName < 0)
			return GAutoWString();
		
		LgiAssert(LastName >= 0);
		t = a[LastName];
		p.Write(t, StrlenW(t) * sizeof(char16) );
	}
	
	ret.Reset(p.NewStrW());
	LgiAssert(ret);
	
	return ret;
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
		if (!StrcmpW(t, sRoundBracketStart))
		{
			Depth++;
		}
		else if (!StrcmpW(t, sRoundBracketEnd))
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
		if (!StricmpW(t, sDefined))
		{
			t = Exp[++i];
			CheckToken(t);
			char16 *Define = t;
			if (!StricmpW(t, sRoundBracketStart))
			{
				Define = Exp[++i];
				CheckToken(Define);
				t = Exp[++i];
				CheckToken(t);
				if (StricmpW(t, sRoundBracketEnd))
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
				if (Next && !StrcmpW(Next, sRoundBracketStart))
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

#define IsWhite(c)		(  (c) == ' ' || (c) == '\t' )
#define SkipNewLine(Cur) \
	if (Cur[0] == '\r' && Cur[1] == '\n') \
		Cur += 2; \
	else if (Cur[0] == '\r') \
		Cur++; \
	else if (Cur[0] == '\n') \
		Cur++; \
	Line++;

bool GCppParserWorker::Preprocess(GSourceFile *sf)
{
	char16 *Cur = sf->Raw;
	int Line = 1;
	char16 Cmd[256];
	PreprocessBlock *Blk = NULL;
	
	while (*Cur)
	{
		// Find Start of line
		while (IsWhite(*Cur))
			Cur++;

		if (*Cur == '#')
		{
			// Emit a source code block
			if (Blk)
			{
				Blk->Len = Cur - Blk->Start;
				if (Blk->Len <= 8)
				{
					bool IsWhiteSpace = true;
					for (int i=0; i<Blk->Len; i++)
					{
						if (Blk->Start[i] != ' ' &&
							Blk->Start[i] != '\t' &&
							Blk->Start[i] != '\r' &&
							Blk->Start[i] != '\n')
						{
							IsWhiteSpace = false;
						}
					}
					if (IsWhiteSpace)
					{
						Blk->Start = NULL;
						Blk->Len = 0;
					}
					else
					{
						Blk = NULL;
					}
				}
				else
				{
					Blk = NULL;
				}
			}
			
			// Read over the preprocessor command and save the name
			char16 *o = Cmd;
			char16 *e = Cmd + 255;
			*o++ = *Cur++;
			while (IsWhite(*Cur))
				Cur++;
			while (	*Cur &&
					!IsWhite(*Cur) &&
					*Cur != '\r' &&
					*Cur != '\n' &&
					o < e)
			{
				*o++ = *Cur++;
			}
			*o++ = 0;
			while (IsWhite(*Cur))
				Cur++;
			
			// Create a block for the statement
			if (Blk == NULL)
				Blk = &sf->Blocks.New();
			Blk->BlockLine = Line;
			Blk->Type = PreprocessSyms.Find(Cmd);
			LgiAssert(Blk->Type != SourceBlock);
			Blk->Start = Cur;

			// Scan till the end of line...		
			char16 *LastNonWhite = NULL;			
			do 
			{
				while (*Cur && *Cur != '\r' && *Cur != '\n')
				{
					if (!IsWhite(*Cur))
						LastNonWhite = Cur;
					Cur++;
				}
				if (LastNonWhite && *LastNonWhite == '\\' && Blk->Type == HashDefine)
				{
					// Multi-line #define...
					SkipNewLine(Cur);
				}
				else
				{
					SkipNewLine(Cur);
					break;
				}
			}
			while (*Cur);
			
			Blk->Len = Cur - Blk->Start;
			while (Blk->Len > 0 && strchr(" \r\t\n", Blk->Start[Blk->Len - 1]))
				Blk->Len--;
			Blk = NULL;
		}
		else
		{
			if (!Blk)
			{
				Blk = &sf->Blocks.New();
				Blk->BlockLine = Line;
				Blk->Start = Cur;
			}
			
			// Scan till the end of line...		
			while (*Cur && *Cur != '\r' && *Cur != '\n')
			{
				if (Cur[0] == '/' && Cur[1] == '*')
				{
					// Process multi-line comment...
					for (Cur += 2; *Cur; )
					{
						if (*Cur == '\r' || *Cur == '\n')
						{
							SkipNewLine(Cur);
						}
						else if (Cur[0] == '*' && Cur[1] == '/')
						{
							Cur += 2;
							break;
						}
						else Cur++;
					}
				}
				else
				{				
					Cur++;
				}
			}
				
			// Skip over new line...
			SkipNewLine(Cur);
		}
	}	

	if (Blk)
	{
		Blk->Len = Cur - Blk->Start;
	}

	// sf->DumpBlocks();	
	return true;
}

GSourceFile *GCppParserWorker::ParseCpp(const char *Path)
{
	GSourceFile *sf = NULL;

	for (int i=0; i<Srcs.Length(); i++)
	{
		GSourceFile *s = Srcs[i];
		if (s->Path && !strcmp(s->Path, Path))
		{
			sf = s;
			break;
		}
	}
	
	if (!sf)
	{
		sf = new GSourceFile;
		if (sf)
		{
			if (!sf->Read(Path))
			{
				delete sf;
				return NULL;
			}
			
			Srcs.Add(sf);
			
			Preprocess(sf);
		}
		else return NULL;
	}
	
	bool IsExternC = false;
	char16 *t;

	PreprocessBlock *blk;
	while ((blk = sf->Current()))
	{
		if (blk->Type != SourceBlock)
		{
			if (!ParsePreprocessor(sf))
				break;
		}
		else if (sf->Active)
		{
			bool Error = false;
			
			sf->Lex(*blk);
						
			while (!Error)
			{
				t = blk->NextToken();
				if (!t)
					break;
				
				KeywordType kt = Keywords.Find(t);
				switch (kt)
				{
					case KwPreprocessor:
					{
						LgiAssert(!"We shouldn't see these here at all.");
						Error = true;
						break;
					}
					case KwUserType:
					{
						GSymbol *sym = ParseUserType(sf, t);
						if (sym)
						{
							if (sf->Active)
							{
								GAutoWString SymName = GetSymbolName(sym->Tokens, sym->Type == SymEnum);
								if (SymName)
								{
									if (CurrentScope()->Find(SymName))
									{
										DeleteObj(sym);
									}
									else
									{
										CurrentScope()->Add(SymName, sym);
									}
								}
								else if (sym->Type == SymEnum)
								{
									// Unnamed enum
									static int Idx = 1;
									#ifdef _MSC_VER
									char16 Buf[256];
									swprintf_s(Buf, CountOf(Buf), L"UnnamedEnum%i", Idx++);
									#else
									char Tmp[256];
									sprintf_s(Tmp, CountOf(Tmp), "UnnamedEnum%i", Idx++);
									GAutoWString Buf(Utf8ToWide(Tmp));
									#endif

									if (CurrentScope()->Find(SymName))
									{
										DeleteObj(sym);
									}
									else
									{
										CurrentScope()->Add(Buf, sym);
									}
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
						
						if (sf->Current())
							blk = sf->Current();
						break;
					}
					default:
					{
						if (IsExternC && CmpToken(t, "}"))
						{
							if (sf->Active)
								IsExternC = false;
						}
						else if (CmpToken(t, "typedef"))
						{
							GSymbol *sym = ParseTypedef(sf);
							if (sym)
							{
								GAutoWString Name = GetSymbolName(sym->Tokens);
								if (Name)
								{
									GSymbol *existing = CurrentScope()->Find(Name);
									if (existing)
									{
										Msg(MsgError, "Symbol '%S' already exists.\n", Name.Get());
									}
									else
									{
										CurrentScope()->Add(Name, sym);
										sym->File = sf->Path;
										sym->Line = blk->GetLine();
										sym = NULL;
									}
								}
								
								DeleteObj(sym);
							}
						}
						else if (CmpToken(t, ";"))
						{
						}
						else
						{
							GSymbol *sym = ParseDecl(sf, t);
							if (sym)
							{
								if (sf->Active)
								{
									if (sym->Type == SymExternC)
									{
										IsExternC = true;
										delete sym;
									}
									else
									{
										GAutoWString Name = GetSymbolName(sym->Tokens);
										if (Name)
										{
											GSymbol *e = Scopes[0]->Find(Name);
											if (e)
											{
												DeleteObj(sym);
											}
											else
											{
												Scopes[0]->Add(Name, sym);
											}
										}
										else
										{
											Msg(MsgError, "%s:%i - Failed to get name of decl.\n", _FL);
											DeleteObj(sym);
										}
									}
								}
								else
								{
									DeleteObj(sym);
								}
							}
							
							if (sf->Current())
								blk = sf->Current();
						}
						break;
					}
				}
			}			
			
			sf->Next();
		}
		else
		{
			sf->Next();
		}
	}

	return sf;
}

GSymbol *GCppParserWorker::ParseDecl(GSourceFile *sf, char16 *t)
{
	PreprocessBlock *blk = sf->Current();
	int ExternC = CmpToken(t, "extern") ? 1 : 0;
	GSymbol *sym = new GSymbol(_FL);

	sym->Type = SymVariable;
	sym->File = sf->Path;
	sym->Line = blk->GetLine();
	
	if (CmpToken(t, "virtual"))
		sym->Type = SymFunction;
		
	if (CmpToken(t, "friend"))
		sym->FriendDecl = true;
	else
		sym->Tokens.Add(t);

	GArray<char16*> Temp;
	if (CmpToken(t, "template"))
	{
		Temp.Add(t);
		while ((t = blk->NextToken()))
		{
			Temp.Add(t);
			if (CmpToken(t, ">"))
				break;
		}
		if (!t)
			blk = sf->Next();
	}

	LoopState Loop = LsLooping;
	int ScopeDepth = 0;
	while (Loop == LsLooping && blk)
	{
		if (blk->Type != SourceBlock)
		{
			// if (stristr(sf->Path, "GDragAndDrop.cpp") && blk->BlockLine == 
			
			if (!ParsePreprocessor(sf))
			{
				Loop = LsError;
				break;
			}
			
			blk = sf->Current();
		}
		else if (!sf->Active)
		{
			blk = sf->Next();
		}
		else
		{
			sf->Lex(*blk);
			
			while (Loop == LsLooping)
			{
				t = blk->NextToken();
				if (!t)
				{
					blk = sf->Next();
					break;
				}
				
				if (CmpToken(t, "{"))
				{
					if (ExternC == 2 && CmpToken(t, "{"))
					{
						sym->Type = SymExternC;
						Loop = LsFinished;
						break;
					}
					
					ScopeDepth++;
				}
				else if (CmpToken(t, "}"))
				{
					if (ScopeDepth <= 0)
					{
						Loop = LsError;
						break;
					}

					if (--ScopeDepth <= 0)
					{
						Loop = LsFinished;
						break;
					}
				}
				else if (ScopeDepth == 0)
				{
					KeywordType kt = Keywords.Find(t);
					if (kt == KwUserType && sym->Type != SymFunction)
					{
						GSymbol *ut = ParseUserType(sf, t);
						if (ut)
							ut->FriendDecl = sym->FriendDecl;
						DeleteObj(sym);
						sym = ut;
						Loop = LsFinished;
						
						if (sf->Current())
							blk = sf->Current();
						break;
					}
					
					if (ExternC > 0)
					{
						if (ExternC == 1 && CmpToken(t, "\"C\""))
						{
							ExternC++;
						}
						else if (ExternC == 2 && CmpToken(t, "{"))
						{
							sym->Type = SymExternC;
							Loop = LsFinished;
							break;
						}
						else
						{
							ExternC = 0;
						}
					}

					if (CmpToken(t, ";"))
					{
						Loop = LsFinished;
						break;
					}

					if (CmpToken(t, "("))
					{
						sym->Type = SymFunction;
					}
					
					sym->Tokens.Add(t);
				}
			}
		}
	}

	return sym;
}

GSymbol *GCppParserWorker::ParseTypedef(GSourceFile *sf)
{
	GSymbol *sym = new GSymbol(_FL);
	sym->Type = SymTypedef;
	PreprocessBlock *blk = sf->Current();
	
	char16 *t = blk->NextToken();
	if (t == NULL)
	{
		delete sym;
		return NULL;
	}

	KeywordType kt = Keywords.Find(t);
	if (kt == KwUserType)
	{
		DeleteObj(sym);
		return ParseUserType(sf, t);
	}

	sym->Tokens.Add(t);
	
	while ((t = blk->NextToken()))
	{
		if (CmpToken(t, ";"))
			break;

		sym->Tokens.Add(t);
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
	bool Finished = false;
	
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
	
	PreprocessBlock *blk = sf->Current();
	
	sym->Tokens.Add(t);
	sym->File = sf->Path;
	sym->Line = blk->GetLine();
	
	while (blk && !Finished)
	{
		if (blk->Type != SourceBlock)
		{
			if (!ParsePreprocessor(sf))
				Finished = true;
			
			blk = sf->Current();
		}
		else if (!sf->Active)
		{
			blk = sf->Next();
		}
		else
		{
			sf->Lex(*blk);
			
			while ((t = blk->NextToken()))
			{
				if (CmpToken(t, "{"))
				{
					InScope++;
				}
				else if (CmpToken(t, "}"))
				{
					ProcessEndBracket:
					if (InScope > 0)
					{
						InScope--;
					}
					else
					{
						LgiAssert(!"End scope mis-match.");
						Finished = true;
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
					if (!InScope)
					{
						Finished = true;
						break;
					}
				}
				else
				{
					KeywordType kt = Keywords.Find(t);
					if (kt == KwVisibility)
					{
					}
					else if (kt == KwPreprocessor)
					{
						LgiAssert(0);
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

						if (sf->Current())
							blk = sf->Current();
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
								def->Line = blk->GetLine();
								
								while ((t = blk->NextToken()))
								{
									if (CmpToken(t, ","))
										break;
									
									if (CmpToken(t, "}"))
									{
										sym->Add(def);
										goto ProcessEndBracket;
									}

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

							if (sf->Current())
								blk = sf->Current();
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
			
			if (!Finished)
				blk = sf->Next();
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
		
		Start++;
	}
	return false;
}

bool GCppParserWorker::ParsePreprocessor(GSourceFile *sf)
{
	char16 *t;
	
	PreprocessBlock *blk = sf->Current();
	if (blk->Type == SourceBlock)
	{
		LgiAssert(0);
		return false;
	}
		
	bool Status = true;
	sf->Lex(*blk);
	
	/*
	LgiTrace("%s:%i - %s, stack=%i\n",
		sf->Path.Get(),
		blk->BlockLine,
		PreprocessSymbolNames[blk->Type],
		sf->Stack.Length());
	*/
	
	switch (blk->Type)
	{
		case HashIf:
		{
			PreprocessState &ps = sf->Stack.New();
			ps.ParentIsActive = sf->Active;

			ps.IfBranchClaimed = Evaluate(blk->Tokens) != 0;
			if (sf->Active)
				sf->Active = ps.IfBranchClaimed;			
			break;
		}
		case HashElif:
		{
			PreprocessState &ps = sf->Stack.Last();
			if (!ps.IfBranchClaimed)
			{
				ps.IfBranchClaimed = Evaluate(blk->Tokens) != 0;
				if (sf->Active)
					sf->Active = ps.IfBranchClaimed;
			}
			else
			{
				sf->Active = false;
			}
			break;
		}
		case HashIfndef:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}

			GSymbol *def = Resolve(t);

			PreprocessState &ps = sf->Stack.New();
			ps.ParentIsActive = sf->Active;
			ps.IfBranchClaimed = def == NULL ||
								(
									def->Type != SymDefineValue &&
									def->Type != SymDefineFunction
								);

			if (sf->Active)
				sf->Active = ps.IfBranchClaimed;
			break;
		}
		case HashIfdef:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}
				
			GSymbol *def = Resolve(t);
			PreprocessState &ps = sf->Stack.New();
			ps.ParentIsActive = sf->Active;
			ps.IfBranchClaimed = def != NULL &&
								(
									def->Type == SymDefineValue ||
									def->Type == SymDefineFunction
								);
			if (sf->Active)
				sf->Active = ps.IfBranchClaimed;
			break;
		}
		case HashElse:
		{
			PreprocessState &ps = sf->Stack.Last();
			if (!ps.IfBranchClaimed)
			{
				ps.IfBranchClaimed = true;
				if (ps.ParentIsActive)
					sf->Active = true;
			}
			else
			{
				sf->Active = false;
			}
			break;
		}
		case HashEndif:
		{
			LgiAssert(sf->Stack.Length() > 0);
			PreprocessState &ps = sf->Stack.Last();
			sf->Active = ps.ParentIsActive;
			sf->Stack.Length(sf->Stack.Length() - 1);
			break;
		}
		case HashPragma:
		case HashError:
		case HashWarning:
		{
			break;
		}
		case HashDefine:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}

			GSymbol *sym = NULL;
			if (sf->Active)
			{
				sym = Scopes[0]->Define(t, SymDefineValue, _FL);
				sym->File = sf->Path;
				sym->Line = blk->BlockLine;
				if (blk->Tokens.Length() > 1)
				{
					sym->Tokens = blk->Tokens;
					sym->Tokens.DeleteAt(0, true);
				}
				else
				{
					sym->Tokens.Length(0);
				}
				
				if (sym->Tokens.Length() > 0 &&
					CmpToken(sym->Tokens[0], "("))
					sym->Type = SymDefineFunction;
			}
			break;
		}
		case HashUndef:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}

			GSymbol *s = Scopes[0]->Find(t);
			if (s)
			{
				Scopes[0]->Delete(t);
				delete s;
			}
			break;
		}
		case HashInclude:
		{
			// Include a file
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}
			
			GAutoString FileName8(WideToUtf8(t));
			if (!FileName8)
			{
				Status = false;
				break;
			}

			// Resolve filename
			char p[MAX_PATH];
			bool Exists = false;
			char *IncPath = NULL;
			if (LgiIsRelativePath(FileName8))
			{
				LgiMakePath(p, sizeof(p), sf->Path, "..");
				LgiMakePath(p, sizeof(p), p, FileName8);
				if ((Exists = FileExists(p)))
					IncPath = p;
			}
			
			if (!Exists)
			{
				if ((IncPath = FindInclude(FileName8)))
					Exists = true;
			}
			
			if (Exists)
			{
				ParseCpp(IncPath);
			}
			break;
		}
		default:
		{
			LgiAssert(!"Impl me");
			Status = false;
			break;
		}
	}
	
	sf->Next();	
	return Status;
}

void GCppParserWorker::DoWork(WorkUnit *wk)
{
	w.Reset(wk);
	
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
				!stricmp(ext, "cpp"))
			{
				InitScopes();
				
				uint64 Start = LgiCurrentTime();

				if (!stricmp(ext, "cpp"))
				{
					Scopes[0]->Define((char16*)sCpp, SymDefineValue, _FL);
				}	

				ParseCpp(w->Source[i]);
				uint64 Time = LgiCurrentTime() - Start;
				
				#if 0
				char16 *k;
				for (GSymbol *s = Scopes[0]->First(&k); s; s = Scopes[0]->Next(&k))
				{
					LgiTrace("    %S = %p\n", k, s);
				}
				#endif

				LgiTrace("%i of %i: %s, %i syms, %i ms\n", i, w->Source.Length(), w->Source[i].Get(), Scopes[0]->Length(), (int)Time);
			}
			else
			{
				// No parser impl yet
			}
			
			Srcs.DeleteObjects();
		}
	}
	else LgiAssert(!"Unknown work unit type.");
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
		d->Worker->DoWork(w);
	}
}

void GCppParser::Search(const char *Str, SearchResultsCb Callback, void *CallbackData)
{
	WorkUnit *w = new WorkUnit(Str);
	if (w)
	{
		if (!d->Worker)
			d->Worker.Reset(new GCppParserWorker(d));
		d->Worker->DoWork(w);
	}
}

