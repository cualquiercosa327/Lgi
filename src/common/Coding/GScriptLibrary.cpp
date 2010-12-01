#include <stdio.h>
#include <math.h>

#include "Lgi.h"
#include "GScripting.h"
#include "GScriptingPriv.h"
#include "GUtf8.h"
#include "GToken.h"
#include "GProcess.h"

//////////////////////////////////////////////////////////////////////////////////////
char16 sChar[]		= { 'c','h','a','r', 0 };
char16 sInt[]		= { 'i','n','t', 0 };
char16 sUInt[]		= { 'u','i','n','t', 0 };
char16 sInt32[]		= { 'i','n','t','3','2', 0 };
char16 sUInt32[]	= { 'u','i','n','t','3','2', 0 };
char16 sInt64[]		= { 'i','n','t','6','4', 0 };
char16 sHWND[]		= { 'H','W','N','D', 0 };
char16 sDWORD[]		= { 'D','W','O','R','D', 0 };
char16 sLPTSTR[]	= { 's','L','P','T','S','T','R', 0 };
char16 sLPCTSTR[]	= { 's','L','P','C','T','S','T','R', 0 };
char16 sElse[]		= { 'e','l','s','e', 0 };
char16 sIf[]		= { 'i','f',0 };
char16 sFunction[]	= { 'f','u','n','c','t','i','o','n',0 };
char16 sExtern[]	= { 'e','x','t','e','r','n',0 };
char16 sFor[]		= { 'f','o','r',0 };
char16 sWhile[]		= { 'w','h','i','l','e',0 };
char16 sReturn[]	= { 'r','e','t','u','r','n',0 };
char16 sInclude[]	= { 'i','n','c','l','u','d','e',0 };
char16 sDefine[]	= { 'd','e','f','i','n','e',0 };
char16 sStruct[]	= { 's','t','r','u','c','t',0 };

char16 sHash[]			= { '#', 0 };
char16 sPeriod[]			= { '.', 0 };
char16 sComma[]				= { ',', 0 };
char16 sSemiColon[]			= { ';', 0 };
char16 sStartRdBracket[]	= { '(', 0 };
char16 sEndRdBracket[]		= { ')', 0 };
char16 sStartSqBracket[]	= { '[', 0 };
char16 sEndSqBracket[]		= { ']', 0 };
char16 sStartCurlyBracket[]	= { '{', 0 };
char16 sEndCurlyBracket[]	= { '}', 0 };

//////////////////////////////////////////////////////////////////////////////////////
bool GHostFunc::Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args)
{
	return (Ctx->*(Func))(Ret, Args);
}

char *InstToString(GInstruction i)
{
	#undef _i
	#define _i(name, opcode, desc) \
		case name: return desc;

	switch (i)
	{
		AllInstructions
	}

	return "#err";
}

//////////////////////////////////////////////////////////////////////////////////////
int GScriptUtils::atoi(char16 *s)
{
	int i = 0;
	if (s)
	{
		char b[64];
		int Len = StrlenW(s) * sizeof(*s);
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;
		i = ::atoi(b);
	}
	return i;
}

int64 GScriptUtils::atoi64(char16 *s)
{
	int64 i = 0;
	if (s)
	{
		#ifdef _MSC_VER

		i = _wtoi64(s);

		#else

		char b[64];
		int Len = StrlenW(s) * sizeof(*s);
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;

		i = strtoll(b, 0, 10);
		
		#endif
	}
	return i;
}

double GScriptUtils::atof(char16 *s)
{
	double i = 0;
	if (s)
	{
		char b[64];
		int Len = StrlenW(s) * sizeof(*s);
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;
		i = ::atof(b);
	}
	return i;
}

int GScriptUtils::htoi(char16 *s)
{
	int i = 0;
	if (s)
	{
		char b[64];
		int Len = StrlenW(s) * sizeof(*s);
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;
		i = ::htoi(b);
	}
	return i;
}


//////////////////////////////////////////////////////////////////////////////////////
bool SystemFunctions::LoadString(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() != 1)
		return false;

	*Ret = LgiLoadString(Args[0]->CastInt32());
	return true;	
}

bool SystemFunctions::Strchr(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 2)
		return false;

	*Ret = -1;

	char *str = Args[0]->CastString();
	char *ch = Args[1]->CastString();
	// int str_len = Args.Length() > 2 ? Args[2]->CastInt32() : -1;
	bool rev = Args.Length() >= 3 ? Args[2]->CastBool() : false;
	if (str AND ch)
	{
		GUtf8Ptr Str(str);
		GUtf8Ptr Ch(ch);
		uint32 c = Ch;

		int n = 0;
		for (uint32 s = Str; s; s = Str++, n++)
		{
			if (c == s)
			{
				*Ret = n;
				if (!rev)
					break;
			}
		}
	}

	return true;
}

bool SystemFunctions::Strstr(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() >= 2)
	{
		char *str1 = (char*) Args[0]->CastString();
		char *str2 = (char*) Args[1]->CastString();
		int case_insensitive =	Args.Length() > 2 ? Args[2]->CastInt32() : true;
		int str_len =			Args.Length() > 3 ? Args[3]->CastInt32() : -1;

		if (str1 && str2)
		{
			char *r = 0;

			if (str_len >= 0)
			{
				if (case_insensitive)
					r = strnistr(str1, str2, str_len);
				else
					r = strnstr(str1, str2, str_len);

			}
			else
			{
				if (case_insensitive)
					r = stristr(str1, str2);
				else
					r = strstr(str1, str2);
			}

			if (r)
				*Ret = r - str1;
			else
				*Ret = -1;
		}
		else *Ret = -1;

		return true;
	}
	return false;
}

bool SystemFunctions::Sprintf(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 1)
		return false;
	char *Fmt = Args[0]->Str();
	if (!Fmt)
		return false;

	GArray<uint32> Params;
	va_list a;

	int i = 1;
	for (char *f = Fmt; *f; f++)
	{
		if (f[0] == '%' && f[1] != '%')
		{
			char *t = f + 1;
			while (*t && !isalpha(*t))
				t++;

			if (i >= Args.Length())
			{
				LgiAssert(!"Not enough args.");
				break;
			}

			switch (*t)
			{
				case 's':
				{
					Params.Add((uint32)Args[i++]->Str());
					break;
				}
				case 'c':
				{
					char *Str = Args[i++]->Str();
					Params.Add(Str ? *Str : '?');
					break;
				}
				case 'f':
				case 'g':
				{
					union tmp
					{
						double Dbl;
						struct
						{
							uint32 High;
							uint32 Low;
						};
					} Tmp;
					Tmp.Dbl = Args[i++]->CastDouble();

					Params.Add(Tmp.High);
					Params.Add(Tmp.Low);
					break;
				}
				default:
				{
					Params.Add(Args[i++]->CastInt32());
					break;
				}
			}
			
			f = *t ? t + 1 : t;
		}
	}

	a = (va_list) &Params[0];

	#ifndef WIN32
	#define _vsnprintf vsnprintf
	#endif

	char Buf[1024];
	_vsnprintf(Buf, sizeof(Buf), Fmt, a);
	*Ret = Buf;

	return true;
}

bool SystemFunctions::Strcmp(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() >= 2)
	{
		char *a = Args[0]->CastString();
		char *b = Args[1]->CastString();
		int case_insensitive =	Args.Length() > 2 ? Args[2]->CastInt32() : true;
		int str_len =			Args.Length() > 3 ? Args[3]->CastInt32() : -1;

		if (a AND b)
		{
			if (str_len > 0)
			{
				if (case_insensitive)
					*Ret = strnicmp(a, b, str_len);
				else
					*Ret = strncmp(a, b, str_len);
			}
			else
			{
				if (case_insensitive)
					*Ret = stricmp(a, b);
				else
					*Ret = strcmp(a, b);
			}

			return true;
		}
	}

	return false;
}

bool SystemFunctions::Substr(GVariant *Ret, ArgumentArray &Args)
{
	// Substr(Str, Start, Len)
	if (Args.Length() >= 2)
	{
		uint8 *str = (uint8*) Args[0]->CastString();
		int start = Args[1]->CastInt32();
		int len = Args.Length() > 2 ? Args[2]->CastInt32() : -1;
		if (str)
		{
			uint32 ch;
			int slen = strlen((char*) str);
			while (start > 0 AND (ch = LgiUtf8To32(str, slen)) != 0)
			{
				start--;
			}

			if (len < 0)
			{
				*Ret = (char*)str;
			}
			else
			{
				uint8 *end = str;
				while (len > 0 AND (ch = LgiUtf8To32(end, slen)) != 0)
				{
					len--;
				}
				Ret->Empty();
				Ret->Type = GV_STRING;
				Ret->Value.String = NewStr((char*)str, end - str);
			}
			return true;
		}
	}

	return false;
}

bool SystemFunctions::Tokenize(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 2)
		return false;

	char *Start = Args[0]->CastString();
	char *Delim = Args[1]->CastString();
	if (!Start || !Delim)
		return false;

	GUtf8Ptr s(Start);
	GUtf8Ptr Delimiter(Delim);
	uint32 d = Delimiter;

	Ret->SetList();

	uint32 i;
	while (i = s)
	{
		if (i == d)
		{
			char *Cur = (char*)s.GetCurrent();
			GVariant *v = new GVariant;
			if (!v) return false;
			if (Start && Cur > Start)
			{
				v->OwnStr(NewStr(Start, Cur - Start));
			}
			Ret->Value.Lst->Insert(v);
			Start = 0;
		}
		else if (!Start)
			Start = (char*)s.GetCurrent();
		s++;
	}

	char *Cur = (char*)s.GetCurrent();
	if (Cur > Start)
	{
		GVariant *v = new GVariant;
		if (!v) return false;
		v->OwnStr(NewStr(Start, Cur - Start));
		Ret->Value.Lst->Insert(v);
	}

	return true;
}

bool SystemFunctions::NewHashTable(GVariant *Ret, ArgumentArray &Args)
{
	bool CaseSen = Args.Length() == 1 ? Args[0]->CastInt32() != 0 : true;
	Ret->SetHashTable(new GHashTable(0, CaseSen));
	return true;
}

bool SystemFunctions::NewList(GVariant *Ret, ArgumentArray &Args)
{
	Ret->SetList(new List<GVariant>);
	return true;
}

bool SystemFunctions::DeleteElement(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() == 2)
	{
		switch (Args[0]->Type)
		{
			case GV_HASHTABLE:
			{
				char *Key = Args[1]->CastString();
				if (Key)
				{
					return Args[0]->Value.Hash->Delete(Key);
				}
				break;
			}
			case GV_LIST:
			{
				int32 Idx = Args[1]->CastInt32();
				if (Idx >= 0 AND Idx < Args[0]->Value.Lst->Length())
				{
					return Args[0]->Value.Lst->Delete(Idx);
				}
				break;
			}
		}
	}

	return false;
}

bool SystemFunctions::ReadTextFile(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() == 1 AND
		FileExists(Args[0]->CastString()))
	{
		if (Ret->Value.String = ::ReadTextFile(Args[0]->CastString()))
		{
			Ret->Type = GV_STRING;
			return true;
		}
	}

	return false;
}

bool SystemFunctions::WriteTextFile(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() == 2)
	{
		GFile f;
		if (f.Open(Args[0]->CastString(), O_WRITE))
		{
			f.SetSize(0);
			
			GVariant *v = Args[1];
			if (v)
			{
				switch (v->Type)
				{
					case GV_STRING:
					{
						int Len = strlen(v->Value.String);
						*Ret = f.Write(v->Value.String, Len) == Len;
						return true;
						break;
					}
					case GV_BINARY:
					{
						*Ret = f.Write(v->Value.Binary.Data, v->Value.Binary.Length) == v->Value.Binary.Length;
						return true;
						break;
					}
				}
			}
		}
	}

	return false;
}

GView *SystemFunctions::CastGView(GVariant &v)
{
	switch (v.Type)
	{
		case GV_DOM:
			return dynamic_cast<GView*>(v.Value.Dom);
		case GV_GVIEW:
			return v.Value.View;
	}
	return 0;
}

bool SystemFunctions::SelectFiles(GVariant *Ret, ArgumentArray &Args)
{
	GFileSelect s;
	
	if (Args.Length() > 0)
		s.Parent(CastGView(*Args[0]));
	
	GToken t(Args.Length() > 1 ? Args[1]->CastString() : 0, ",;:");
	for (int i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		char *sp = strrchr(c, ' ');
		if (*sp)
		{
			*sp++ = 0;
			s.Type(c, sp);
		}
	}
	s.Type("All Files", LGI_ALL_FILES);

	s.InitialDir(Args.Length() > 2 ? Args[2]->CastString() : 0);
	s.MultiSelect(Args.Length() > 3 ? Args[3]->CastInt32() : true);

	if (s.Open())
	{
		Ret->SetList();
		for (int i=0; i<s.Length(); i++)
		{
			Ret->Value.Lst->Insert(new GVariant(s[i]));
		}
	}

	return true;
}

bool SystemFunctions::Sleep(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() != 1)
		return false;

	LgiSleep(Args[0]->CastInt32());
	return true;
}

bool SystemFunctions::FormatSize(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() != 1)
		return false;

	char s[64];
	LgiFormatSize(s, Args[0]->CastInt64());
	*Ret = s;
	return true;
}

bool SystemFunctions::ClockTick(GVariant *Ret, ArgumentArray &Args)
{
	*Ret = (int64)LgiCurrentTime();
	return true;
}

bool SystemFunctions::Now(GVariant *Ret, ArgumentArray &Args)
{
	Ret->Empty();
	Ret->Type = GV_DATETIME;
	Ret->Value.Date = new GDateTime;
	Ret->Value.Date->SetNow();
	return true;
}

bool SystemFunctions::New(GVariant *Ret, ArgumentArray &Args)
{
	if (!Engine || Args.Length() != 1)
		return false;

	GCompiledCode *c = Engine->GetCurrentCode();
	if (!c)
		return false;

	GBase o;
	o.Name(Args[0]->CastString());
	GTypeDef *t = c->GetType(o.NameW());
	if (t)
	{
		Ret->Empty();
		Ret->Type = GV_CUSTOM;
		Ret->Value.Custom.Dom = t;
		Ret->Value.Custom.Data = new char[t->Sizeof()];
	}

	return true;
}

bool SystemFunctions::Delete(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() != 1)
		return false;

	GVariant *v = Args[0];
	if (v->Type == GV_CUSTOM)
	{
		DeleteArray(v->Value.Custom.Data);
		v->Empty();
		*Ret = true;
	}
	else
	{
		LgiAssert(!"Not a custom type.");
		*Ret = false;
	}

	return true;
}

class GFileListEntry : public GDom
{
	bool Folder;
	GVariant Name;
	int64 Size;
	GDateTime Modified;

public:
	GFileListEntry(GDirectory *d)
	{
		Folder = d->IsDir();
		Name = d->GetName();
		Size = d->GetSize();
		Modified.Set(d->GetLastWriteTime());
	}

	bool GetVariant(char *Var, GVariant &Value, char *Arr = 0)
	{
		if (!stricmp(Var, "Name"))
			Value = Name;
		if (!stricmp(Var, "Folder"))
			Value = Folder;
		if (!stricmp(Var, "Modified"))
			Value = &Modified;
		if (!stricmp(Var, "Size"))
			Value = Size;
		else return false;
		return true;
	}
};

bool SystemFunctions::DeleteFile(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() != 1)
		return false;

	char *f = Args[0]->CastString();
	if (f)
		*Ret = FileDev->Delete(Args[0]->CastString());
	else
		*Ret = false;
	return true;
}

bool SystemFunctions::ListFiles(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 1)
		return false;

	Ret->SetList();
	GDirectory *d = FileDev->GetDir();
	if (d)
	{
		char *Pattern = Args.Length() > 1 ? Args[1]->CastString() : 0;
		for (bool b=d->First(Args[0]->CastString()); b; b=d->Next())
		{
			if (!Pattern || MatchStr(Pattern, d->GetName()))
			{
				Ret->Value.Lst->Insert(new GVariant(new GFileListEntry(d)));
			}
		}
		DeleteObj(d);
	}
	else Ret->Empty();

	return true;
}

bool SystemFunctions::Execute(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 2)
		return false;

	GProcess e;
	GStringPipe p;
	bool Status = e.Run(Args[0]->CastString(), Args[1]->CastString(), 0, true, 0, &p);
	if (Status)
	{
		GAutoString o(p.NewStr());
		*Ret = o;
	}	
	return Status;
}

bool SystemFunctions::GetInputDlg(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 4)
		return false;

	GViewI *Parent = CastGView(*Args[0]);
	char *InitVal = Args[1]->Str();
	char *Msg = Args[2]->Str();
	char *Title = Args[3]->Str();
	bool Pass = Args.Length() > 4 ? Args[4]->CastBool() : false;
	GInput Dlg(Parent, InitVal, Msg, Title, Pass);
	if (Dlg.DoModal())
	{
		*Ret = Dlg.Str;
	}

	return true;
}

#define DefFn(Name) \
	GHostFunc(#Name, 0, (ScriptCmd)&SystemFunctions::Name)

GHostFunc SystemLibrary[] =
{
	// String handling
	DefFn(Strchr),
	DefFn(Strstr),
	DefFn(Strcmp),
	DefFn(Substr),
	DefFn(LoadString),
	DefFn(FormatSize),
	DefFn(Sprintf),
	DefFn(Tokenize),
	
	// Containers/objects
	DefFn(NewHashTable),
	DefFn(NewList),
	DefFn(DeleteElement),
	DefFn(New),
	DefFn(Delete),

	// Files
	DefFn(ReadTextFile),
	DefFn(WriteTextFile),
	DefFn(SelectFiles),
	DefFn(ListFiles),
	DefFn(DeleteFile),

	// Time	
	DefFn(ClockTick),
	DefFn(Sleep),
	DefFn(Now),

	// System
	DefFn(Execute),
	DefFn(GetInputDlg),

	// End of list marker
	GHostFunc(0, 0, 0),
};

GHostFunc *SystemFunctions::GetCommands()
{
	return SystemLibrary;
}

