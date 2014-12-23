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
char16 sTrue[]		= { 't','r','u','e',0 };
char16 sFalse[]		= { 'f','a','l','s','e',0 };
char16 sNull[]		= { 'n','u','l','l',0 };
char16 sOutParam[]  = { '_','o','u','t','_',0};

char16 sHash[]				= { '#', 0 };
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
GExecutionStatus GHostFunc::Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args)
{
	return (Ctx->*(Func))(Ret, Args) ? ScriptSuccess : ScriptError;
}

const char *InstToString(GInstruction i)
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
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
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
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
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
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
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
		int Bytes = LgiBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;
		i = ::htoi(b);
	}
	return i;
}


//////////////////////////////////////////////////////////////////////////////////////
SystemFunctions::SystemFunctions()
{
	Engine = NULL;
	Log = NULL;

	#ifdef WINNATIVE
	Brk = NULL;
	#endif
}

SystemFunctions::~SystemFunctions()
{
}

GStream *SystemFunctions::GetLog()
{
	return Log;
}

bool SystemFunctions::SetLog(GStream *log)
{
	LgiAssert(Log == NULL);
	Log = log;
	return true;
}

void SystemFunctions::SetEngine(GScriptEngine *Eng)
{
	Engine = Eng;
}

bool SystemFunctions::LoadString(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() != 1)
		return false;

	*Ret = LgiLoadString(Args[0]->CastInt32());
	return true;	
}

bool SystemFunctions::Sprintf(GVariant *Ret, ArgumentArray &Args)
{
    #ifdef __GNUC__
    return false;
    #else
	if (Args.Length() < 1)
		return false;
	char *Fmt = Args[0]->Str();
	if (!Fmt)
		return false;

	GArray<UNativeInt> Params;
	va_list a;

	unsigned i = 1;
	for (char *f = Fmt; *f; f++)
	{
		if (f[0] == '%' && f[1] != '%')
		{
			char *t = f + 1;
			while (*t && !IsAlpha(*t))
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
					Params.Add((UNativeInt)Args[i++]->Str());
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
	vsprintf_s(Buf, sizeof(Buf), Fmt, a);
	*Ret = Buf;

	return true;
	#endif
}

bool SystemFunctions::ReadTextFile(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() == 1 &&
		FileExists(Args[0]->CastString()))
	{
		if ((Ret->Value.String = ::ReadTextFile(Args[0]->CastString())))
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
					default: break;
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
		default: break;
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
	for (unsigned i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		char *sp = strrchr(c, ' ');
		if (sp)
		{
			*sp++ = 0;
			s.Type(sp, c);
		}
		else
		{
			char *dot = strrchr(c, '.');
			if (dot)
			{
				char Type[256];
				sprintf_s(Type, sizeof(Type), "%s files", dot + 1);
				s.Type(Type, c);
			}
		}
	}
	s.Type("All Files", LGI_ALL_FILES);

	s.InitialDir(Args.Length() > 2 ? Args[2]->CastString() : 0);
	s.MultiSelect(Args.Length() > 3 ? Args[3]->CastInt32() != 0 : true);

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

bool SystemFunctions::Print(GVariant *Ret, ArgumentArray &Args)
{
	GStream *Out = Log ? Log : (Engine ? Engine->GetConsole() : NULL);

	for (unsigned n=0; Out && n<Args.Length(); n++)
	{
		if (!Args[n])
			continue;

		GVariant v = *Args[n];
		char *f = v.CastString();
		if (!f)
		{
			Out->Write("NULL", 4);
			continue;
		}

		#if 1
		int Len = strlen(f);
		Out->Write(f, Len);
		#else
		char *i = f, *o = f;
		for (; *i; i++)
		{
			if (*i == '\\')
			{
				i++;
				switch (*i)
				{
					case 'n':
						*o++ = '\n';
						break;
					case 'r':
						*o++ = '\r';
						break;
					case 't':
						*o++ = '\t';
						break;
					case '\\':
						*o++ = '\\';
						break;
					case '0':
						*o++ = 0;
						break;
				}
			}
			else
			{
				*o++ = *i;
			}
		}
		*o = 0;
		Out->Write(f, o - f);
		#endif
	}

	return true;
}

bool SystemFunctions::FormatSize(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() != 1)
		return false;

	char s[64];
	LgiFormatSize(s, sizeof(s), Args[0]->CastInt64());
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
	if (Args.Length() != 1 || !Args[0] || !Ret)
		return false;

	Ret->Empty();
	char *sType = Args[0]->CastString();
	if (!sType)
		return false;

	if (IsDigit(*sType))
	{
		// Binary block
		int Bytes = ::atoi(sType);
		if (!Bytes)
			return false;
		
		return Ret->SetBinary(Bytes, new char[Bytes], true);
	}

	GDomProperty Type = GStringToProp(sType);
	switch (Type)
	{	
		case TypeList:
		{
			Ret->SetList();
			break;
		}
		case TypeHashTable:
		{
			Ret->SetHashTable();
			break;
		}
		case TypeSurface:
		{
			Ret->Empty();
			Ret->Type = GV_GSURFACE;
			if ((Ret->Value.Surface.Ptr = new GMemDC))
			{
				Ret->Value.Surface.Ptr->AddRef();
				Ret->Value.Surface.Own = true;
			}
			break;
		}
		case TypeFile:
		{
			Ret->Empty();
			Ret->Type = GV_GFILE;
			if ((Ret->Value.File.Ptr = new GFile))
			{
				Ret->Value.File.Ptr->AddRef();
				Ret->Value.File.Own = true;
			}
			break;
		}
		case TypeDateTime:
		{
			Ret->Empty();
			Ret->Type = GV_DATETIME;
			Ret->Value.Date = new GDateTime;
			break;
		}
		default:
		{
			Ret->Empty();

			GCompiledCode *c = Engine ? Engine->GetCurrentCode() : NULL;
			if (!c)
				return false;

			GAutoWString o(LgiNewUtf8To16(Args[0]->CastString()));
			GTypeDef *t = c->GetType(o);
			if (t)
			{
				Ret->Type = GV_CUSTOM;
				Ret->Value.Custom.Dom = t;
				Ret->Value.Custom.Data = new char[t->Sizeof()];
			}
		}
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
	}
	else
	{
		v->Empty();
	}

	*Ret = true;
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

	bool GetVariant(const char *Var, GVariant &Value, char *Arr = 0)
	{
		GDomProperty p = GStringToProp(Var);
		switch (p)
		{
			case ObjName:
				Value = Name;
				break;
			case ObjLength:
				Value = Size;
				break;
			case FileFolder:
				Value = Folder;
				break;
			case FileModified:
				Value = &Modified;
				break;
			default:
				return false;
		}
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

bool SystemFunctions::CurrentScript(GVariant *Ret, ArgumentArray &Args)
{
	if (Engine &&
		Engine->GetCurrentCode())
	{
		*Ret = Engine->GetCurrentCode()->GetFileName();
		return true;
	}
	return false;
}

bool SystemFunctions::PathExists(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() == 0)
		return false;
		
	GDirectory d;
	if (d.First(Args[0]->CastString(), NULL))
	{
		if (d.IsDir())
			*Ret = 2;
		else
			*Ret = 1;
	}
	else
	{
		*Ret = 0;
	}
	
	return true;		
}

bool SystemFunctions::PathJoin(GVariant *Ret, ArgumentArray &Args)
{
	char p[MAX_PATH] = "";
	for (unsigned i=0; i<Args.Length(); i++)
	{
		char *s = Args[i]->CastString();
		if (i)
			LgiMakePath(p, sizeof(p), p, s);
		else
			strcpy_s(p, sizeof(p), s);
	}
	
	if (*p)
		*Ret = p;
	else
		Ret->Empty();
	return true;
}

bool SystemFunctions::PathSep(GVariant *Ret, ArgumentArray &Args)
{
	*Ret = DIR_STR;
	return true;
}

bool SystemFunctions::ListFiles(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 1)
		return false;

	Ret->SetList();
	GDirectory d;
	char *Pattern = Args.Length() > 1 ? Args[1]->CastString() : 0;
	
	char *Folder = Args[0]->CastString();
	for (int b=d.First(Folder); b; b=d.Next())
	{
		if (!Pattern || MatchStr(Pattern, d.GetName()))
		{
			Ret->Value.Lst->Insert(new GVariant(new GFileListEntry(&d)));
		}
	}

	return true;
}

bool SystemFunctions::CreateSurface(GVariant *Ret, ArgumentArray &Args)
{
	Ret->Empty();

	if (Args.Length() < 2)
		return false;

	int x = Args[0]->CastInt32();
	int y = Args[1]->CastInt32();
	GColourSpace Cs = CsNone;

	if (Args.Length() > 2)
	{
		GVariant *Type = Args[2];
		const char *c;
		if (Type->IsInt())
		{
			// Bit depth... convert to default Colour Space.
			Cs = GBitsToColourSpace(Type->CastInt32());
		}
		else if ((c = Type->Str()))
		{
			// Parse string colour space def
			Cs = GStringToColourSpace(Type->Str());
		}
	}

	if (!Cs) // Catch all error cases and make it the default screen depth.
		Cs = GdcD->GetColourSpace();

	if ((Ret->Value.Surface.Ptr = new GMemDC(x, y, Cs)))
	{
		Ret->Type = GV_GSURFACE;
		Ret->Value.Surface.Own = true;
		Ret->Value.Surface.Ptr->AddRef();
	}

	return true;
}

bool SystemFunctions::GetInputDlg(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 4)
		return false;

	GViewI *Parent = CastGView(*Args[0]);
	char *InitVal = Args[1]->Str();
	char *Msg = Args[2]->Str();
	char *Title = Args[3]->Str();
	bool Pass = Args.Length() > 4 ? Args[4]->CastInt32() != 0 : false;
	GInput Dlg(Parent, InitVal, Msg, Title, Pass);
	if (Dlg.DoModal())
	{
		*Ret = Dlg.Str;
	}

	return true;
}

bool SystemFunctions::GetViewById(GVariant *Ret, ArgumentArray &Args)
{
	Ret->Empty();

	if (Args.Length() < 2)
		return false;

	GViewI *Parent = CastGView(*Args[0]);
	int Id = Args[1]->CastInt32();
	if (!Parent || Id <= 0)
		return false;

	if (Parent->GetViewById(Id, Ret->Value.View))
	{
		Ret->Type = GV_GVIEW;
	}

	return true;
}

bool SystemFunctions::Execute(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 2)
		return false;

	GProcess e;
	GStringPipe p;
	char *Exe = Args[0]->CastString();
	char *Arguments = Args[1]->CastString();
	bool Status = e.Run(Exe, Arguments, 0, true, 0, &p);
	if (Status)
	{
		GAutoString o(p.NewStr());
		*Ret = o;
	}
	else if (Log)
	{
		uint32 ErrCode = e.GetErrorCode();
		GAutoString ErrMsg = LgiErrorCodeToString(ErrCode);
		if (ErrMsg)
			Log->Print("Error: Execute(\"%s\",\"%s\") failed with '%s'\n", Exe, Arguments, ErrMsg.Get());
		else
			Log->Print("Error: Execute(\"%s\",\"%s\") failed with '0x%x'\n", Exe, Arguments, ErrCode);
	}
	
	return Status;
}

bool SystemFunctions::System(GVariant *Ret, ArgumentArray &Args)
{
	if (Args.Length() < 2)
		return false;

	char *Exe = Args[0]->Str();
	char *Arg = Args[1]->Str();
	*Ret = LgiExecute(Exe, Arg);
	return true;
}

bool SystemFunctions::OsName(GVariant *Ret, ArgumentArray &Args)
{
	*Ret = LgiGetOsName();
	return true;
}

bool SystemFunctions::OsVersion(GVariant *Ret, ArgumentArray &Args)
{
	Ret->Empty();

	GArray<int> Ver;
	int Os = LgiGetOs(&Ver);
	Ret->SetList();
	for (int i=0; i<3; i++)
		Ret->Value.Lst->Insert(new GVariant(Ver[i]));

	return true;
}

#define DefFn(Name) \
	GHostFunc(#Name, 0, (ScriptCmd)&SystemFunctions::Name)

GHostFunc SystemLibrary[] =
{
	// String handling
	DefFn(LoadString),
	DefFn(FormatSize),
	DefFn(Sprintf),
	DefFn(Print),
	
	// Containers/objects
	DefFn(New),
	DefFn(Delete),

	// Files
	DefFn(ReadTextFile),
	DefFn(WriteTextFile),
	DefFn(SelectFiles),
	DefFn(ListFiles),
	DefFn(DeleteFile),

	DefFn(CurrentScript),
	DefFn(PathJoin),
	DefFn(PathExists),
	DefFn(PathSep),

	// Time	
	DefFn(ClockTick),
	DefFn(Sleep),
	DefFn(Now),

	// Images
	DefFn(CreateSurface),

	// UI
	DefFn(GetInputDlg),
	DefFn(GetViewById),

	// System
	DefFn(Execute),
	DefFn(System),
	DefFn(OsName),
	DefFn(OsVersion),

	// End of list marker
	GHostFunc(0, 0, 0),
};

GHostFunc *SystemFunctions::GetCommands()
{
	return SystemLibrary;
}

