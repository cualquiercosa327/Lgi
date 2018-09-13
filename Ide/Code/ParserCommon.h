#ifndef _PARSER_COMMON_H_
#define _PARSER_COMMON_H_

#include "GLexCpp.h"

#define isword(s)				(s && (isdigit(s) || isalpha(s) || (s) == '_') )
#define iswhite(s)				(s && strchr(WhiteSpace, s) != 0)
#define skipws(s)				while (iswhite(*s)) s++;
#define defnskipws(s)			while (iswhite(*s)) { if (*s == '\n') Line++; s++; }
#define defnskipsym(s)			while (IsAlpha(*s) || IsDigit(*s) || strchr("_:.~", *s)) { s++; }
#define IsValidVariableChar(ch)	(IsAlpha(ch) || IsDigit(ch) || strchr("_", ch) != NULL)

enum DefnType
{
	DefnNone,
	DefnDefine = 0x1,
	DefnFunc = 0x2,
	DefnClass = 0x4,
	DefnEnum = 0x8,
	DefnEnumValue = 0x10,
	DefnTypedef = 0x20,
	DefnVariable = 0x40,
};

extern bool ParseFunction(GRange &Return, GRange &Name, GRange &Args, const char *Defn);

class DefnInfo
{
public:
	DefnType Type;
	GString Name;
	GString File;
	int Line;
	GRange FnName;
	
	DefnInfo()
	{
		Type = DefnNone;
		Line = 0;
	}

	DefnInfo(const DefnInfo &d)
	{
		Type = d.Type;
		Name = d.Name;
		File = d.File;
		Line = d.Line;
	}
	
	void Set(DefnType type, char *file, GString s, int line)
	{
		LgiAssert(s(0) != ')');
		
		Type = type;
		File = file;
		
		Line = line;
		Name = s.Strip();
		if (Name && Type == DefnFunc)
		{
			GRange Return, Args;
			if (ParseFunction(Return, FnName, Args, Name))
			{
				if (strlen(Name) > 42)
				{
					char *b = strchr(Name, '(');
					if (b)
					{
						if (strlen(b) > 5)
						{
							strcpy(b, "(...)");
						}
						else
						{
							*b = 0;
						}
					}
				}
			}
			
			char *t;
			while ((t = strchr(Name, '\t')))
			{
				*t = ' ';
			}
		}		
	}

	int Find(const char *Str)
	{
		int Slen = strlen(Str);
		GString Src;

		if (Type == DefnFunc)
			Src = Name(FnName.Start, FnName.End());
		else
			Src = Name;

		char *Match = stristr(Src, Str);
		if (!Match)
			return 0;
		int Idx = Match - Src.Get();
		if (Idx < 0)
			return 0;

		int Score = 1;
		
		// Is it an exact match?
		bool Exact =
			(	// Start:
				Idx == 0
				||
				!IsValidVariableChar(Src(Idx-1))
			)
			&&
			(
				Idx+Slen >= Name.Length()
				||
				!IsValidVariableChar(Src(Idx+Slen))
			);
		if (Exact)
			Score++;

		return Score;
	}
};

extern const char *TypeToStr(DefnType t);

extern bool BuildCppDefnList
(
	char *FileName,
	char16 *Cpp,
	GArray<DefnInfo> &Funcs,
	/// Use DefnType bits
	int LimitTo,
	bool Debug = false
);

extern bool BuildPyDefnList(char *FileName, char16 *Source, GArray<DefnInfo> &Defns, int LimitTo, bool Debug = false);

inline bool BuildDefnList(char *FileName, char16 *Source, GArray<DefnInfo> &Funcs, int LimitTo, bool Debug = false)
{
	auto Ext = LgiGetExtension(FileName);
	auto Fn = BuildCppDefnList;
	if (Ext && !stricmp(Ext, "py"))
		Fn = BuildPyDefnList;
	return Fn(FileName, Source, Funcs, LimitTo, Debug);
}

#endif