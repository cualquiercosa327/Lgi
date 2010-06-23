#include "Lgi.h"
#include "RtfHtml.h"

#define SkipWs(s) while (*s AND strchr(Ws, *s)) s++;
#define SkipNotWs(s) while (*s AND !strchr(Ws, *s)) s++;

static char Ws[] = " \r\t\n";

bool HasTag(List<char> &Tags, char *Tag)
{
	int Len = strlen(Tag);
	for (char *t=Tags.First(); t; t=Tags.Next())
	{
		if (strnicmp(Tag, t, Len) == 0) return true;
	}
	return false;
}

void ParseRtf(GStringPipe &p, char *&s)
{
	List<char> Tags;

	while (s AND *s)
	{
		SkipWs(s);

		if (*s == '{')
		{
			s++;
			ParseRtf(p, s);
		}
		else if (*s == '}')
		{
			s++;
			return;
		}
		else if (*s == '\\')
		{
			s++;
			char *Start = s;
			while (*s AND (isalpha(*s) OR strchr("*\'", *s))) s++;
			while (*s AND isdigit(*s)) s++;

			int Len = (int)s-(int)Start;
			if (Len > 1)
			{
				char *Tag = NewStr(Start, (int)s-(int)Start);
				if (Tag)
				{
					if (strcmp(Tag, "par") == 0)
					{
						p.Push("\n");
					}
					Tags.Insert(Tag);
				}
			}
		}
		else // content
		{
			SkipWs(s);

			char *Start = s;
			while (*s AND !strchr(Ws, *s) AND !strchr("\\{}\n", *s)) s++;
			SkipWs(s);

			if (HasTag(Tags, "htmlrtf") OR
				HasTag(Tags, "htmltag"))
			{
				p.Push(Start, (int)s-(int)Start);
			}
		}
	}

	Tags.DeleteArrays();
}

char *MsRtfToHtml(char *s)
{
	GStringPipe p;
	ParseRtf(p, s);
	return p.NewStr();
}

char *MsHtmlToRtf(char *s)
{
	return 0;
}
