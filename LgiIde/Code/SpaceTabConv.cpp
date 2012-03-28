#include "GMem.h"
#include "GContainers.h"

char *SpacesToTabs(char *Text, int TabSize)
{
	GStringPipe p(4 << 10);
	
	if (Text)
	{
		int	x = 0;
		for	(char *s = Text; s AND *s;  )
		{
			if (*s == ' ')
			{
				char *Sp = s;
				if (s[1] == ' ')
				{
					while (*s == ' ')
					{
						s++;
						x++;
						if (x % TabSize == 0)
						{
							p.Push("\t", 1);
							Sp = s;
						}
					}
				}
				else s++;
				
				if (Sp < s)
				{
					int Len = s-Sp;
					p.Push(Sp, Len);
					x += Len;
				}
			}
			else if (*s == '\n')
			{
				x = 0;
				p.Push(s++, 1);
			}
			else if (*s == '\t')
			{
				x += TabSize - (x % TabSize);
				p.Push(s++, 1);
			}
			else
			{
				char *e = s;
				while (*e AND *e != ' ' AND *e != '\n' AND *e != '\t') e++;
				int Len = e-s;
				p.Push(s, Len);
				x += Len;
				s += Len;
			}
		}
	}
	
	return p.NewStr();
}

char *TabsToSpaces(char *Text, int TabSize)
{
	GStringPipe p(4 << 10);
	
	if (Text)
	{
		int x = 0;
		for (char *s = Text; s AND *s; )
		{
			if (*s == '\t')
			{
				while (*s == '\t')
				{
					int Spaces = TabSize - (x % TabSize);
					for (int i=0; i<Spaces; i++)
					{
						p.Push(" ", 1);
					}
					x += Spaces;
					s++;
				}
			}
			else if (*s == '\n')
			{
				x = 0;
				p.Push(s++, 1);
			}
			else
			{
				char *e = s;
				while (*e AND *e != '\n' AND *e != '\t') e++;
				int Len = e-s;
				p.Push(s, Len);
				x += Len;
				s += Len;
			}
		}
	}	
	
	return p.NewStr();
}

