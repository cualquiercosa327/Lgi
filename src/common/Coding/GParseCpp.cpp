#include "Lgi.h"
#include "GParseCpp.h"
#include "GThreadEvent.h"
#include "GLexCpp.h"

enum StackType
{
	StackNone,
	StackIfDefined,
	StackIfNotDefined,
	StackFunction,
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
};

struct GCppParserWorker : public GThread, public GMutex
{
	GCppParserPriv *d;
	GArray<WorkUnit*> Work;
	bool Loop;
	GThreadEvent Event;
	WorkUnit *w;
	GCppStringPool GeneralPool;
	
	GArray<GSourceFile*> Srcs;
	GHashTbl<const char*,GSourceFile*> SrcHash;
	GArray<GSourceScope*> Scopes; // from global to local
	
public:
	GCppParserWorker(GCppParserPriv *priv);
	~GCppParserWorker();
	
	void Add(WorkUnit *w);
	GSourceFile *ParseCpp(const char *Path);	
	GSymbol *Resolve(char16 *Symbol);
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
	
	
	Scopes.Add(new GSourceScope);
	
	#ifdef WIN32
	char16 buf[256];
	GSymbol *s = Scopes[0]->Define(L"_MSC_VER", SymDefine);
	int ch = swprintf_s(buf, CountOf(buf), L"%i", _MSC_VER);
	s->Tokens.Add(GeneralPool.Alloc(buf, ch));
	s->File = "none";
	#endif
	
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
	bool CompileActive = true;
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
				if (!StrcmpW(t, L"#ifndef"))
				{
					t = NextToken();
					CheckToken(t);

					GSymbol *def = Resolve(t);
					CompileActive = def == NULL || def->Type != SymDefine;
					Stack.Add(StackIfNotDefined);					
				}
				else if (!StrcmpW(t, L"#ifdef"))
				{
					t = NextToken();
					CheckToken(t);

					GSymbol *def = Resolve(t);
					CompileActive = def != NULL && def->Type == SymDefine;
					Stack.Add(StackIfDefined);					
				}
				else if (!StrcmpW(t, L"#define"))
				{
					t = NextToken();
					CheckToken(t);

					GSymbol *s = Scopes[0]->Define(t, SymDefine);
					s->File = sf->Path;
					s->Line = CurLine;
					
					int LastLine = s->Line;
					while (t = NextToken())
					{
						if (CurLine != LastLine)
							goto ProcessToken;
						
						if (!StrcmpW(t, L"\\"))
							LastLine++;
						else
							s->Tokens.Add(t);
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

