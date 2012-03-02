#include <stdio.h>

#include "Lgi.h"
#include "LgiIde.h"

static char *White = " \r\t\n";
#define iswhite(s)		(s AND strchr(White, s) != 0)
#define isword(s)		(s AND (isdigit(s) OR isalpha(s) OR (s) == '_') )
#define skipws(s)		while (iswhite(*s)) s++;

char *FindHeader(char *Short, List<char> &Paths)
{
	char *Status = 0;
	
	// Search through the paths
	for (char *Path=Paths.First(); Path; Path=Paths.Next())
	{
		char f[300];
		LgiMakePath(f, sizeof(f), Path, Short);
		if (FileExists(f))
		{
			Status = NewStr(f);
		}
	}
	
	return Status;
}

bool BuildHeaderList(char *Cpp, List<char> &Headers, List<char> &IncPaths, bool Recurse)
{
	char Include[] = {'i', 'n', 'c', 'l', 'u', 'd', 'e', 0 };
	
	for (char *c = Cpp; c AND *c; )
	{
		skipws(c);
		if (*c == '#')
		{
			// preprocessor instruction
			if (strncmp(++c, Include, 7) == 0)
			{
				char *s = c + 7;
				skipws(s);
				if (*s == '\"' OR *s == '<')
				{
					char d = (*s == '\"') ? '\"' : '>';					
					char *e = strchr(++s, d);
					char *Short = NewStr(s, e-s);

					if (Short)
					{
						char *File = FindHeader(Short, IncPaths);
						if (File)
						{
							bool Has = false;
							for (char *s=Headers.First(); s; s=Headers.Next())
							{
								if (stricmp(s, File) == 0)
								{
									Has = true;
									break;
								}
							}
							if (Has)
							{
								DeleteArray(File);
							}
							else
							{
								Headers.Insert(File);
								
								if (Recurse)
								{
									// Recursively add includes...
									char *c8 = ReadTextFile(File);
									if (c8)
									{
										BuildHeaderList(c8, Headers, IncPaths, Recurse);
										DeleteArray(c8);
									}
								}
							}
						}
						else
						{
							// printf("%s:%i - didn't find '%s'\n", __FILE__, __LINE__, Short);
						}
						DeleteArray(Short);
					}
				}
			}
		}
		
		while (*c AND *c != '\n') c++;
		if (*c == '\n') c++;
	}
	
	return Headers.Length() > 0;
}

