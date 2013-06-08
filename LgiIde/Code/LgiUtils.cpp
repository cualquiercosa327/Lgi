#include <stdio.h>

#include "Lgi.h"
#include "LgiIde.h"

static char *White = " \r\t\n";
#define iswhite(s)		(s AND strchr(White, s) != 0)
#define isword(s)		(s AND (isdigit(s) || isalpha(s) || (s) == '_') )
#define skipws(s)		while (iswhite(*s)) s++;

char *FindHeader(char *Short, GArray<char*> &Paths)
{
	char *Status = 0;
	
	// Search through the paths
	for (int i=0; i<Paths.Length(); i++)
	{
		char *Path = Paths[i];
		
		char f[MAX_PATH];
		LgiMakePath(f, sizeof(f), Path, Short);
		if (FileExists(f))
		{
			Status = NewStr(f);
		}
	}
	
	return Status;
}

bool BuildHeaderList(char *Cpp, GArray<char*> &Headers, GArray<char*> &IncPaths, bool Recurse)
{
	char Include[] = {'i', 'n', 'c', 'l', 'u', 'd', 'e', 0 };
	
	for (char *c = Cpp; c && *c; )
	{
		skipws(c);
		if (*c == '#')
		{
			// preprocessor instruction
			if (strncmp(++c, Include, 7) == 0)
			{
				char *s = c + 7;
				skipws(s);
				if (*s == '\"' || *s == '<')
				{
					char d = (*s == '\"') ? '\"' : '>';					
					char *e = strchr(++s, d);
					char *Short = NewStr(s, e - s);

					if (Short)
					{
						char *File = FindHeader(Short, IncPaths);
						if (File)
						{
							bool Has = false;
							for (int i=0; i<Headers.Length(); i++)
							{
								char *s = Headers[i];
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
								Headers.Add(File);
								
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
		
		while (*c && *c != '\n')
			c++;
		if (*c == '\n')
			c++;
	}
	
	return Headers.Length() > 0;
}

