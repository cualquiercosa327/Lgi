#ifndef _SIMPLE_CPP_PARSE_
#define _SIMPLE_CPP_PARSE_

#include "GLexCpp.h"

#define isword(s)			(s && (isdigit(s) || isalpha(s) || (s) == '_') )
#define iswhite(s)			(s && strchr(WhiteSpace, s) != 0)
#define skipws(s)			while (iswhite(*s)) s++;
#define defnskipws(s)		while (iswhite(*s)) { if (*s == '\n') Line++; s++; }
#define defnskipsym(s)		while (isalpha(*s) || isdigit(*s) || strchr("_:.~", *s)) { s++; }

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

class DefnInfo
{
public:
	DefnType Type;
	GString Name;
	GString File;
	int Line;
	
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
			
			char *t;
			while ((t = strchr(Name, '\t')))
			{
				*t = ' ';
			}
		}		
	}
};

extern bool BuildDefnList
(
	char *FileName,
	char16 *Cpp,
	GArray<DefnInfo> &Funcs,
	/// Use DefnType bits
	int LimitTo,
	bool Debug = false
);

#endif