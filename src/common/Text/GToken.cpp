// Basic tokeniser

#include <string.h>
#include "GMem.h"
#include "GContainers.h"
#include "GToken.h"
#include "GString.h"

/////////////////////////////////////////////////////////////////////////
bool LgiIsNumber(char *p)
{
	if (p)
	{
		for (uint i=0; i<strlen(p); i++)
		{
			if ((p[i]<'0' || p[i]>'9') && p[i]!='-')
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

char *LgiSkipDelim(char *p, const char *Delimiter)
{
	while (p && *p && strchr(Delimiter, *p)) p++;
	return p;
}

////////////////////////////////////////////////////////////////////////
GToken::GToken()
{
	Raw = 0;
	fixed = true;
}

GToken::GToken(const char *Str, const char *Delimiters, bool GroupDelim, int Length)
{
	Raw = 0;
	fixed = true;
	Parse(Str, Delimiters, GroupDelim, Length);
}

GToken::~GToken()
{
	Empty();
}

void GToken::Parse(const char *Str, const char *Delimiters, bool GroupDelim, int Length)
{
	Empty();
	if (Str)
	{
		// Prepare lookup table
		bool Lut[256];
		ZeroObj(Lut);
		if (Delimiters)
		{
			for (const char *d=Delimiters; *d; d++)
			{
				Lut[(const uchar)*d] = true;
			}
		}

		// Do tokenisation
		Raw = NewStr(Str, Length);
		if (Raw)
		{
			fixed = false;

			if (Delimiters)
			{
				if (GroupDelim)
				{
					for (char *s=Raw; *s;)
					{
						// Skip Delimiters
						while (*s && Lut[(uchar)*s]) *s++ = 0;

						// Emit pointer
						if (*s)
						{
							Add(s);
						}

						// Skip Non-delimiters
						while (*s && !Lut[(unsigned char)*s]) s++;
					}
				}
				else
				{
					for (char *s=Raw; *s;)
					{
						// Emit pointer
						if (*s)
						{
							Add(Lut[(uchar)*s] ? (char*)0 : s);
						}

						// Skip Non-delimiters
						while (*s && !Lut[(unsigned char)*s]) s++;

						// Skip Delimiter
						if (*s && Lut[(uchar)*s]) *s++ = 0;
					}
				}
			}
			else
			{
				Add(Raw);
			}

			fixed = true;
		}
	}
}

void GToken::Empty()
{
	DeleteArray(Raw);
	Length(0);
}

void GToken::AppendTokens(GArray<char*> *T)
{
	if (T)
	{
		int Total = Length() + T->Length();
		int Bytes = 0, i;
		for (i=0; i<Length(); i++)
		{
			Bytes += strlen((*this)[i]) + 1;
		}
		for (i=0; i<T->Length(); i++)
		{
			Bytes += strlen((*T)[i]) + 1;
		}

		char *Buf = new char[Bytes];
		if (Buf)
		{
			char *p = Buf;

			fixed = false;

			for (i=0; i<Length(); i++)
			{
				char *Ptr = (*this)[i];
				int Len = strlen(Ptr) + 1;
				memcpy(p, Ptr, Len);
				(*this)[i] = p;
				p += Len;
			}

			DeleteArray(Raw);
			
			for (i=0; i<T->Length(); i++)
			{
				char *Ptr = (*T)[i];
				int Len = strlen(Ptr) + 1;
				memcpy(p, Ptr, Len);
				Add(p);
				p += Len;
			}

			fixed = true;
			Raw = Buf;
		}
	}
}



