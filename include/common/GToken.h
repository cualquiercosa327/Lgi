#ifndef __TOKEN_H
#define __TOKEN_H

#include "GArray.h"

/// \returns true if the string represents a int or float number
LgiFunc bool LgiIsNumber(char *p);

/// Skips over a set of delimiters, returning the next non-delimiter
LgiFunc char *LgiSkipDelim(char *p, char *Delimiter = " \r\n\t");

/// A simple token parser
class LgiClass GToken : public GArray<char*>
{
	char *Raw;

public:
	GToken();
	GToken(char *Str, char *Delimiters = " \r\n\t,", bool GroupDelim = true, int Length = -1);
	~GToken();

	/// Parses a string with set delimiters
	void Parse(char *Str, char *Delimiters = " \r\n\t,", bool GroupDelim = true, int Length = -1);
	
	/// Empties the object
	void Empty();

	/// Appends some tokens on the end of this list.
	void AppendTokens(GArray<char*> *T);

	char *&operator [](uint32 i)
	{
		if (i < Length())
			return GArray<char*>::operator[](i);

		static char *Null = 0;
		return Null;
	}
};

#endif
