class ParseRule;
class ParseContext;

typedef GArray<int> IndexArr;
typedef GArray<char16*> TokenArr;
typedef GArray<ParseRule*> RuleArr;

enum RuleTypes
{
	NoRule,
	NormalRule,
	VariableName,
	Declaration,
	DebugRule,
};

class ParseRule
{
public:
	static GHashTable Rules;
	static GHashTable Keywords;

	char *Name;
	char *Const;
	char *LinkToRule;
	ParseRule *Rule;
	bool List;
	RuleTypes Type;
	
	char *Me()
	{
		if (Const) return Const;
		if (Name) return Name;
		if (LinkToRule) return LinkToRule; 
		return 0;
	}
	
	GArray<RuleArr*> Cond;
	
	ParseRule(GToken &t, int &i);
	ParseRule(char *s);

	virtual ~ParseRule();
	virtual bool Eval(ParseContext &c, int d);
};

class Identifier : public ParseRule
{
public:
	Identifier();
	bool Eval(ParseContext &c, int d);
};

class IntegerConst : public ParseRule
{
public:
	IntegerConst();
	bool Eval(ParseContext &c, int d);
};

class Type : public ParseRule
{
public:
	Type() : ParseRule(0) {}
	bool Eval(ParseContext &c, int d);
};

class ClassName : public Type
{
public:
	ClassName()
	{
		Rules.Add(Name = NewStr("class-name"), this);
	}
};

class EnumName : public Type
{
public:
	EnumName()
	{
		Rules.Add(Name = NewStr("enum-name"), this);
	}
};

class TypedefName : public Type
{
public:
	TypedefName()
	{
		Rules.Add(Name = NewStr("typedef-name"), this);
	}
};

class ParseContext
{

public:
	TokenArr Tokens;
	IndexArr Idx;
	int Cur;
	int Depth;
	char16 *Source;
	GHashTable Types;
	
	ParseContext()
	{
		Cur = 0;
		Depth = 0;
		Source = 0;
		
		char *f = LgiFindFile("cpp.txt");
		if (f)
		{
			ParseRule::Keywords.Add("auto");
			ParseRule::Keywords.Add("register");
			ParseRule::Keywords.Add("static");
			ParseRule::Keywords.Add("extern");
			ParseRule::Keywords.Add("inline");
			ParseRule::Keywords.Add("virtual");
			ParseRule::Keywords.Add("const");
			ParseRule::Keywords.Add("volatile");
			ParseRule::Keywords.Add("friend");
			ParseRule::Keywords.Add("typedef");
			ParseRule::Keywords.Add("char");
			ParseRule::Keywords.Add("short");
			ParseRule::Keywords.Add("int");
			ParseRule::Keywords.Add("long");
			ParseRule::Keywords.Add("signed");
			ParseRule::Keywords.Add("unsigned");
			ParseRule::Keywords.Add("float");
			ParseRule::Keywords.Add("double");
			ParseRule::Keywords.Add("void");
			ParseRule::Keywords.Add("class");
			ParseRule::Keywords.Add("struct");
			ParseRule::Keywords.Add("union");
			ParseRule::Keywords.Add("enum");
			ParseRule::Keywords.Add("asm");
			ParseRule::Keywords.Add("const");
			ParseRule::Keywords.Add("operator");
			ParseRule::Keywords.Add("private");
			ParseRule::Keywords.Add("protected");
			ParseRule::Keywords.Add("public");
			ParseRule::Keywords.Add("new");
			ParseRule::Keywords.Add("delete");
			
			// Initialize some custom rules
			new Identifier;
			new ClassName;
			new EnumName;
			new TypedefName;
			new IntegerConst;
			
			// Load the grammer
			char *Grammar = ReadTextFile(f);
			DeleteArray(f);
			if (Grammar)
			{
				GToken t(Grammar, "\n", false);
				for (int i=0; i<t.Length(); )
				{
					if (NOT t[i] OR t[i][0] == '#')
					{
						// Comment
						i++;
					}
					else if (ValidStr(t[i]))
					{
						// Rule
						new ParseRule(t, i);
					}
					else break;
				}
			}
			
			// Post setup
			ParseRule *r = (ParseRule*)ParseRule::Rules.Find("dname");
			if (r)
			{
				r->Type = VariableName;
			}
			
			r = (ParseRule*)ParseRule::Rules.Find("declarator");
			if (r)
			{
				r->Type = DebugRule;
			}
		}
	}
	
	~ParseContext()
	{
	}
	
	char16 *t()
	{
		return Tokens[Cur];
	}
	
	bool Next()
	{
		Cur++;
		return Tokens[Cur] != 0;
	}
};

GHashTable ParseRule::Rules;
GHashTable ParseRule::Keywords;

ParseRule::ParseRule(char *s)
{
	Name = 0;
	Const = 0;
	Rule = 0;
	LinkToRule = 0;
	List = false;
	Type = NormalRule;
	
	if (s)
	{
		if (strnicmp(s, "listof(", 7) == 0)
		{
			List = true;
			s += 7;
			char *e = strchr(s, ')');
			if (e)
			{
				LinkToRule = NewStr(s, (int)e-(int)s);
			}
		}
		else if (isalpha(*s) AND NOT ParseRule::Keywords.Find(s))
		{
			LinkToRule = NewStr(s);
		}
		else
		{
			Const = NewStr(s);
		}
	}
}

ParseRule::ParseRule(GToken &t, int &i)
{
	List = false;
	Const = 0;
	Rule = 0;
	LinkToRule = 0;
	Type = NormalRule;
	
	Name = NewStr(t[i++]);
	if (Name)
	{
		// printf("Adding '%s'\n", Name);
		Rules.Add(Name, this);
		
		int c = 0;
		char *s;
		while (i < t.Length())
		{
			s = t[i++];
			if (ValidStr(s))
			{
				if (iswhite(*s))
				{
					if (NOT Cond[c])
					{
						Cond[c] = new RuleArr;
					}
					
					GToken p(s, " ");
					for (int n=0; n<p.Length(); n++)
					{
						(*Cond[c])[Cond[c]->Length()] = new ParseRule(p[n]);
					}

					c++;
				}
				else
				{
					printf("Invalid rule line %i: '%s'\n", i-1, s);
					LgiAssert(0);
					break;
				}
			}
			else
			{
				break;
			}
		}
	}
}

ParseRule::~ParseRule()
{
	for (int i=0; i<Cond.Length(); i++)
	{
		Cond[i]->DeleteObjects();
	}
	Cond.DeleteObjects();
	DeleteArray(Name);
	DeleteArray(Const);
	DeleteArray(LinkToRule);
}

bool ParseRule::Eval(ParseContext &c, int d)
{
	static int Count=0;
	if (Count++ == 600) exit(0);

	for (int j=0; j<d; j++) printf(" ");
	printf("Eval '%s' on '%S'\n", Me(), c.t());

	if (Const)
	{
		// Compare the constant
		char *p = Const;
		char16 *t = c.t();
		if (t)
		{
			while (*t AND *p)
			{
				if (*t != *p)
				{
					return false;
				}
				
				t++;
				p++;
			}
			if (NOT *t AND NOT *p)
			{
				printf("Match const '%s'\n", Const);
				c.Next();
				return true;
			}
		}
		
		return false;
	}
	else if (LinkToRule AND NOT Rule)
	{
		// Resolve the Name if we can
		Rule = (ParseRule*) Rules.Find(LinkToRule);
		if (NOT Rule)
		{
			printf("The rule '%s' is not in the grammar.\n", LinkToRule);
			LgiAssert(0);
		}
		if (this == Rule)
		{
			printf("The rule '%s' is self referencing.\n", LinkToRule);
			LgiAssert(0);
		}
		
		// printf("Mapping '%s' in '%s'\n", Rule->Me(), Me());
	}
	
	if (Rule)
	{
		// Test the target rule
		if (List)
		{
			int i = c.Cur;
			bool s = Rule->Eval(c, d + 1);
			if (s)
			{
				while (s)
				{
					i = c.Cur;
					if (NOT Rule->Eval(c, d + 1))
					{
						c.Cur = i;
						break;
					}					
				}
			}
			else
			{
				c.Cur = i;
			}

			return s;
		}
		else
		{
			bool s = Rule->Eval(c, d + 1);
			return s;
		}
	}
	else
	{
		// Test the conditions...
		int i = c.Cur;
		bool Status = false;
		for (int o=0; o<Cond.Length() AND NOT Status; o++)
		{
			if (Type == DebugRule)
			{
				for (int j=0; j<d; j++) printf(" ");
				printf("%i of %i\n", o, Cond.Length());
			}
			
			bool s = true;
			RuleArr *Arr = Cond[o];
			if (Arr)
			{
				for (int a=0; a<Arr->Length(); a++)
				{
					s = s AND (*Arr)[a]->Eval(c, d + 1);
				}
				
				if (s)
				{
					Status = true;
					break;
				}
				else
				{
					c.Cur = i;
				}
			}
			else break;
		}

		if (Type == DebugRule)
		{
			for (int j=0; j<d; j++) printf(" ");
			printf("Status=%i\n", Status);
		}

		if (Status)
		{
			if (Type == VariableName)
			{
				printf("Var='%S'\n", c.Tokens[i]);
			}
			
			for (int j=0; j<d; j++) printf(" ");
			printf("Match rule '%s'\n", Name);
						
			return true;
		}
		else
		{
			c.Cur = i;
		}
	}

	return false;
}

bool Type::Eval(ParseContext &c, int d)
{
	bool Status = false;
	char *w = LgiNewUtf16To8(c.t());
	if (w)
	{
		if (c.Types.Find(w))
		{
			c.Next();
			Status = true;
		}
		DeleteArray(w);
	}
	return Status;
}

Identifier::Identifier() : ParseRule(0)
{
	Rules.Add(Name = NewStr("identifier"), this);
}

bool Identifier::Eval(ParseContext &c, int d)
{
	char16 *s = c.t();
	if (s)
	{
		char *w = LgiNewUtf16To8(s);
		if (w)
		{
			if (ParseRule::Keywords.Find(w))
			{
				DeleteArray(w);
				return false;
			}
			DeleteArray(w);
		}
		
		if (*s == '_' OR isalpha(*s))
		{
			printf("Identifier found '%S'\n", s);
			c.Next();
			return true;
		}
	}
	
	return false;	
}

IntegerConst::IntegerConst() : ParseRule(0)
{
	Rules.Add(Name = NewStr("integer-constant"), this);
}

bool IntegerConst::Eval(ParseContext &c, int d)
{
	char16 *Str = c.t();
	if (ValidStrW(Str))
	{
		for (char16 *s=Str; *s; s++)
		{
			if (NOT isdigit(*s))
				return false;
		}
		
		// printf("Int const found '%S'\n", Str);
		c.Next();
		return true;
	}
	
	return false;	
}

/*
		// Tokenize the file.
		ParseContext c;
		c.Source = Source;
		
		for (char16 *s=Source; s AND *s; )
		{
			int Index = ((int)s-(int)Source)/sizeof(*s);
			char16 *t = LexCpp(s);
			if (t)
			{
				c.Tokens[c.Tokens.Length()] = t;
				c.Idx[c.Idx.Length()] = Index;
			}
			else break;
		}
		
		ParseRule *Root = (ParseRule*)ParseRule::Rules.Find("declaration");
		if (Root)
		{
			char16 *t;
			while (t = c.t())
			{
				if (*t == '#')
				{
					c.Next();
				}
				else
				{
					if (Root->Eval(c, 0))
					{
					}
					else
					{
						printf("Parsing error.\n");
						break;
					}
				}
			}
		}

*/