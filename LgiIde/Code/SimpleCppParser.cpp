#include "Lgi.h"
#include "SimpleCppParser.h"

#if 1
#define DEBUG_FILE		"C:\\Users\\matthew\\Audinate\\Callendar\\apec3\\source\\nad_apcp.c"
#define DEBUG_LINE		42
#elif 0
#define DEBUG_FILE		"apcp-stdin.c"
#define DEBUG_LINE		396
#else
#define DEBUG_FILE		"korthals.c"
#define DEBUG_LINE		130
#endif

const char *TypeToStr(DefnType t)
{
	switch (t)
	{
		default:
		case DefnNone: return "DefnNone";
		case DefnDefine: return "DefnDefine";
		case DefnFunc: return "DefnFunc";
		case DefnClass: return "DefnClass";
		case DefnEnum: return "DefnEnum";
		case DefnEnumValue: return "DefnEnumValue";
		case DefnTypedef: return "DefnTypedef";
		case DefnVariable: return "DefnVariable";
	}
}


bool BuildDefnList(char *FileName, char16 *Cpp, GArray<DefnInfo> &Defns, int LimitTo, bool Debug)
{
	if (!Cpp)
		return false;

	static char16 StrClass[]		= {'c', 'l', 'a', 's', 's', 0};
	static char16 StrStruct[]		= {'s', 't', 'r', 'u', 'c', 't', 0};
	static char16 StrEnum[]		    = {'e', 'n', 'u', 'm', 0};
	static char16 StrOpenBracket[]	= {'{', 0};
	static char16 StrColon[]		= {':', 0};
	static char16 StrSemiColon[]	= {';', 0};
	static char16 StrDefine[]		= {'d', 'e', 'f', 'i', 'n', 'e', 0};
	static char16 StrExtern[]		= {'e', 'x', 't', 'e', 'r', 'n', 0};
	static char16 StrTypedef[]		= {'t', 'y', 'p', 'e', 'd', 'e', 'f', 0};
	static char16 StrC[]			= {'\"', 'C', '\"', 0};
	
	char16 *s = Cpp;
	char16 *LastDecl = s;
	int Depth = 0;
	int Line = 0, PrevLine = 0;
	int CaptureLevel = 0;
	int InClass = false;	// true if we're in a class definition			
	char16 *CurClassDecl = 0;
	bool IsEnum = 0, IsClass = false, IsStruct = false;
	bool FnEmit = false;	// don't emit functions between a f(n) and the next '{'
							// they are only parent class initializers

	#ifdef DEBUG_FILE
	Debug |= FileName && stristr(FileName, DEBUG_FILE) != NULL;
	#endif

	while (s && *s)
	{
		// skip ws
		while (*s && strchr(" \t\r", *s)) s++;
		
		#ifdef DEBUG_LINE
		if (Debug)
		{
			if (Line >= DEBUG_LINE - 1)
				int asd=0;
			else if (PrevLine != Line)
			{
				PrevLine = Line;
				// LgiTrace("%s:%i '%.10S'\n", FileName, Line + 1, s);
			}
		}
		#endif

		// tackle decl
		switch (*s)
		{
			case 0:
				break;
			case '\n':
			{
				Line ++;
				s++;
				break;
			}
			case '#':
			{
				char16 *Hash = s;
				
				s++;
				if
				(
					(
						LimitTo == DefnNone
						||
						(LimitTo & DefnDefine) != 0
					)
					&&
					StrncmpW(StrDefine, s, 6) == 0
				)
				{
					s += 6;
					defnskipws(s);
					LexCpp(s, LexNoReturn);

					char16 r = *s;
					*s = 0;					
					Defns.New().Set(DefnDefine, FileName, Hash, Line + 1);					
					*s = r;
					
				}
				
				while (*s)
				{
					if (*s == '\n')
					{
						// could be end of # command
						char Last = (s[-1] == '\r') ? s[-2] : s[-1];
						if (Last != '\\') break;
						Line++;
					}

					s++;
					LastDecl = s;
				}
				break;
			}
			case '\"':
			case '\'':
			{
				char16 Delim = *s;
				s++;
				while (*s)
				{
					if (*s == Delim) { s++; break; }
					if (*s == '\\') s++;
					if (*s == '\n') Line++;
					s++;
				}
				break;
			}
			case '{':
			{
				s++;
				Depth++;
				FnEmit = false;
				#if 0 // def DEBUG_FILE
				if (Debug)
					LgiTrace("%s:%i - FnEmit=%i Depth=%i @ line %i\n", _FL, FnEmit, Depth, Line);
				#endif				
				break;
			}
			case ';':
			{
				s++;
				LastDecl = s;
				
				if (Depth == 0 && InClass)
				{
					// Check for typedef struct name
					char16 *Start = s - 1;
					while (Start > Cpp && Start[-1] != '}')
						Start--;
					GString TypeDef = GString(Start, s - Start - 1).Strip();
					if (TypeDef.Length() > 0)
					{
						if (LimitTo == DefnNone || (LimitTo & DefnClass) != 0)
						{
							Defns.New().Set(DefnClass, FileName, TypeDef, Line + 1);
						}
					}

					// End the class def
					InClass = false;
					CaptureLevel = 0;
					#ifdef DEBUG_FILE
					if (Debug)
						LgiTrace("%s:%i - CaptureLevel=%i Depth=%i @ line %i\n", _FL, CaptureLevel, Depth, Line);
					#endif
					DeleteArray(CurClassDecl);
				}
				break;
			}
			case '}':
			{
				s++;
				if (Depth > 0) Depth--;
				LastDecl = s;
				IsEnum = false;
				break;
			}
			case '/':
			{
				s++;
				if (*s == '/')
				{
					// one line comment
					while (*s && *s != '\n') s++;
					LastDecl = s;
				}
				else if (*s == '*')
				{
					// multi line comment
					s++;
					while (*s)
					{
						if (s[0] == '*' && s[1] == '/')
						{
							s += 2;
							break;
						}

						if (*s == '\n') Line++;
						s++;
					}
					LastDecl = s;
				}
				break;
			}
			case '(':
			{
				s++;
				if (Depth == CaptureLevel && !FnEmit && LastDecl)
				{
					// function?
					
					// find start:
					char16 *Start = LastDecl;
					skipws(Start);
					
					// find end
					char16 *End = StrchrW(Start, ')');
					if (End)
					{
						End++;

						char16 *Buf = NewStrW(Start, End-Start);
						if (Buf)
						{
							// remove new-lines
							char16 *Out = Buf;
							bool HasEquals = false;
							for (char16 *In = Buf; *In; In++)
							{
								if (*In == '\r' || *In == '\n' || *In == '\t' || *In == ' ')
								{
									*Out++ = ' ';
									skipws(In);
									In--;
								}
								else if (In[0] == '/' && In[1] == '/')
								{
									In = StrchrW(In, '\n');
									if (!In) break;
								}
								else
								{
									if (*In == '=')
										HasEquals = true;
									*Out++ = *In;
								}
							}
							*Out++ = 0;

							if (CurClassDecl)
							{
								char16 Str[1024];
								ZeroObj(Str);
								
								StrncpyW(Str, Buf, CountOf(Str));
								char16 *b = StrchrW(Str, '(');
								if (b)
								{
									// Skip whitespace between name and '('
									while
									(
										b > Str
										&&
										strchr(" \t\r\n", b[-1])																									
									)
									{
										b--;
									}

									// Skip over to the start of the name..
									while
									(
										b > Str
										&&
										b[-1] != '*'
										&&
										b[-1] != '&'
										&&
										!strchr(" \t\r\n", b[-1])																									
									)
									{
										b--;
									}
									
									int ClsLen = StrlenW(CurClassDecl);
									memmove(b + ClsLen + 2, b, sizeof(*b) * (StrlenW(b)+1));
									memcpy(b, CurClassDecl, sizeof(*b) * ClsLen);
									b += ClsLen;
									*b++ = ':';
									*b++ = ':';
									DeleteArray(Buf);
									Buf = NewStrW(Str);
								}
							}

							// cache f(n) def
							DefnType Type = HasEquals ? DefnVariable : DefnFunc;
							if
							(
								(
									LimitTo == DefnNone ||
									(LimitTo & Type) != 0
								)
								&&
								*Buf != ')'
							)
								Defns.New().Set(Type, FileName, Buf, Line + 1);
							DeleteArray(Buf);
							
							while (*End && *End != ';' && *End != ':')
								End++;
							
							FnEmit = *End != ';';
							#if 0 //DEBUG_FILE
							if (Debug)
								LgiTrace("%s:%i - FnEmit=%i Depth=%i @ line %i\n", _FL, FnEmit, Depth, Line);
							#endif
						}
					}
				}
				break;
			}
			default:
			{
				if (IsAlpha(*s) || IsDigit(*s) || *s == '_')
				{
					char16 *Start = s;
					
					s++;
					defnskipsym(s);
					
					int TokLen = s - Start;
					if (TokLen == 6 && StrncmpW(StrExtern, Start, 6) == 0)
					{
						// extern "C" block
						char16 *t = LexCpp(s, LexStrdup); // "C"
						if (t && StrcmpW(t, StrC) == 0)
						{
							defnskipws(s);
							if (*s == '{')
							{
								Depth--;
							}
						}
						DeleteArray(t);
					}
					else if (TokLen == 7 && StrncmpW(StrTypedef, Start, 7) == 0)
					{
						// Typedef
						skipws(s);
						
						IsStruct = !Strnicmp(StrStruct, s, StrlenW(StrStruct));
						IsClass = !Strnicmp(StrClass, s, StrlenW(StrClass));
						if (IsStruct || IsClass)
						{
							Start = s;
							goto DefineStructClass;
						}

						IsEnum = !Strnicmp(StrEnum, s, StrlenW(StrEnum));
						if (IsEnum)
						{
							Start = s;
							s += 4;
							goto DefineEnum;
						}
						
						GStringPipe p;
						char16 *i;
						for (i = Start; i && *i;)
						{
							switch (*i)
							{
								case ' ':
								case '\t':
								{
									p.Push(Start, i - Start);
									
									defnskipws(i);

									char16 sp[] = {' ', 0};
									p.Push(sp);

									Start = i;
									break;
								}
								case '\n':
									Line++;
									// fall thru
								case '\r':
								{
									p.Push(Start, i - Start);
									i++;
									Start = i;
									break;
								}
								case '{':
								{
									p.Push(Start, i - Start);
									
									int Depth = 1;
									i++;
									while (*i && Depth > 0)
									{
										switch (*i)
										{
											case '{':
												Depth++;
												break;
											case '}':
												Depth--;
												break;
											case '\n':
												Line++;
												break;
										}
										i++;
									}
									Start = i;
									break;
								}
								case ';':
								{
									p.Push(Start, i - Start);
									s = i;
									i = 0;
									break;
								}
								default:
								{
									i++;
									break;
								}
							}
						}
						
						char16 *Typedef = p.NewStrW();
						if (Typedef)
						{
							if (LimitTo == DefnNone || (LimitTo & DefnTypedef) != 0)
							{
								Defns.New().Set(DefnTypedef, FileName, Typedef, Line + 1);
							}
							DeleteArray(Typedef);
						}
					}
					else if
					(
						(
							TokLen == 5
							&&
							(IsClass = !StrncmpW(StrClass, Start, 5))
						)
						||
						(
							TokLen == 6
							&&
							(IsStruct = !StrncmpW(StrStruct, Start, 6))
						)
					)
					{
						DefineStructClass:
						
						// Class / Struct
						if (Depth == 0)
						{
							// Check if this is really a class/struct definition or just a reference
							char16 *next = s;
							while (*next && !strchr(";){", *next))
								next++;
							
							if (*next == '{')
							{
								InClass = true;
								CaptureLevel = 1;
								#ifdef DEBUG_FILE
								if (Debug)
									LgiTrace("%s:%i - CaptureLevel=%i Depth=%i @ line %i\n", _FL, CaptureLevel, Depth, Line);
								#endif
								
								char16 *n = Start + (IsClass ? StrlenW(StrClass) : StrlenW(StrStruct)), *t;
								List<char16> Tok;
								
								while (n && *n)
								{
									char16 *Last = n;
									if ((t = LexCpp(n, LexStrdup)))
									{
										if (StrcmpW(t, StrSemiColon) == 0)
										{
											break;
										}
										else if (StrcmpW(t, StrOpenBracket) == 0 ||
												 StrcmpW(t, StrColon) == 0)
										{
											DeleteArray(CurClassDecl);
											CurClassDecl = Tok.Last();
											Tok.Delete(CurClassDecl);
											
											if (LimitTo == DefnNone || (LimitTo & DefnClass) != 0)
											{
												char16 r = *Last;
												*Last = 0;
												Defns.New().Set(DefnClass, FileName, Start, Line + 1);
												*Last = r;
												s = Last;
											}
											break;
										}
										else
										{
											Tok.Insert(t);
										}
									}
									else
									{
										LgiTrace("%s:%i - LexCpp failed at %s:%i.\n", _FL, FileName, Line);
										break;
									}
								}
								Tok.DeleteArrays();
							}
						}
					}
					else if
					(
						TokLen == 4
						&&
						StrncmpW(StrEnum, Start, 4) == 0
					)
					{
						DefineEnum:

						IsEnum = true;
						defnskipws(s);
						
						GAutoWString t(LexCpp(s, LexStrdup));
						if (t && isalpha(*t))
						{
							Defns.New().Set(DefnEnum, FileName, t.Get(), Line + 1);
						}
					}
					else if (IsEnum)
					{
						char16 r = *s;
						*s = 0;
						Defns.New().Set(DefnEnumValue, FileName, Start, Line + 1);
						*s = r;
						defnskipws(s);
						if (*s == '=')
						{
							s++;
							while (true)
							{
								defnskipws(s);
								defnskipsym(s);
								defnskipws(s);
								if (*s == 0 || *s == ',' || *s == '}')
									break;
								LgiAssert(*s != '\n');
								s++;
							}
						}
					}
					
					if (s[-1] == ':')
					{
						LastDecl = s;
					}
				}
				else
				{
					s++;
				}

				break;
			}
		}
	}
	
	DeleteArray(CurClassDecl);

	if (Debug)
	{
		for (unsigned i=0; i<Defns.Length(); i++)
		{
			DefnInfo *def = &Defns[i];
			LgiTrace("    %s: %s:%i %s\n", TypeToStr(def->Type), def->File.Get(), def->Line, def->Name.Get());
		}
	}
	
	return Defns.Length() > 0;
}

