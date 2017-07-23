#ifndef __TOKEN_H
#define __TOKEN_H

#include "GArray.h"
#include "GString.h"

/// \returns true if the string represents a int or float number
template<typename T>
bool LgiIsNumber(T *p)
{
	if (!p)
		return false;

	if (*p == '0' && ToLower(p[1]) == 'x')
	{
		p += 2;
		while (*p)
		{
			if
			(
				!
				(
					(*p >= '0' && *p <= '9')
					||
					(*p >= 'a' && *p <= 'f')
					||
					(*p >= 'A' && *p <= 'F')
				)
			)
				return false;
			p++;
		}
		
		return true;
	}
	
	// Integer or float...?
	if (*p == '-')
		p++;

	while (*p)
	{
		if
		(
			!
			(
				(*p >= '0' && *p <= '9')
				||
				(*p == '.')
				||
				(ToLower(*p) == 'e')
			)
		)
			return false;
		p++;
	}

	return true;
}

/// Skips over a set of delimiters, returning the next non-delimiter
LgiFunc char *LgiSkipDelim(char *p, const char *Delimiter = " \r\n\t", bool NotDelim = false);

/// A simple token parser
class GToken : public GArray<char*>
{
	char *Raw;

public:
	GToken()
	{
		Raw = 0;
		fixed = true;
	}
	
	GToken(const char *Str, const char *Delimiters = " \r\n\t,", bool GroupDelim = true, ssize_t Length = -1)
	{
		Raw = 0;
		fixed = true;
		Parse(Str, Delimiters, GroupDelim, Length);
	}

	~GToken()
	{
		Empty();
	}

	/// Parses a string with set delimiters
	void Parse(const char *Str, const char *Delimiters = " \r\n\t,", bool GroupDelim = true, ssize_t Length = -1)
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
	
	/// Empties the object
	void Empty()
	{
		DeleteArray(Raw);
		Length(0);
	}

	/// Appends some tokens on the end of this list.
	void AppendTokens(GArray<char*> *T)
	{
		if (T)
		{
			// int64 Total = Length() + T->Length();
			size_t Bytes = 0;
			unsigned i;
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
					size_t Len = strlen(Ptr) + 1;
					memcpy(p, Ptr, Len);
					(*this)[i] = p;
					p += Len;
				}

				DeleteArray(Raw);
			
				for (i=0; i<T->Length(); i++)
				{
					char *Ptr = (*T)[i];
					size_t Len = strlen(Ptr) + 1;
					memcpy(p, Ptr, Len);
					Add(p);
					p += Len;
				}

				fixed = true;
				Raw = Buf;
			}
		}
	}

	char *&operator [](size_t i)
	{
		if (i < Length())
			return GArray<char*>::operator[](i);

		static char *Null = 0;
		return Null;
	}
};

#endif
