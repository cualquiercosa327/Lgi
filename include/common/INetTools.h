/// \file
/// \author Matthew Allen
/// \brief General socket / mail-news related classes and functions

#ifndef __INET_TOOLS_H
#define __INET_TOOLS_H

#include "GContainers.h"
#include "LgiNetInc.h"

/// Header class, holds a single header
class LgiNetClass GInetHeader
{
public:
	char *Name;
	char *Str;

	GInetHeader(char *n = 0, char *s = 0)
	{
		Name = n;
		Str = s;
	}

	~GInetHeader()
	{
		DeleteArray(Name);
		DeleteArray(Str);
	}
};

/// Turns a set of headers into a list of name/values
LgiNetFunc void InetTokeniseHeaders(List<GInetHeader> &Out, const char *In);

/// Get a field's value as a dynamic string
LgiNetFunc char *InetGetField(const char *s);

/// Get a specific value from a list of headers (as a dynamic string)
LgiNetFunc char *InetGetHeaderField(const char *Headers, const char *Field, int Len = -1);

/// Gets a sub field of a header value
LgiNetFunc char *InetGetSubField(const char *s, char *Field);

/// Removes a specific name/value from a dynamic string
LgiNetFunc char *InetRemoveField(const char *Headers, const char *Field);

/// Returns the string up to '\r\n\r\n' as a dynamic string
LgiNetFunc char *InetGetAllHeaders(const char *s);

/// Returns the boundry part out of the 'Content-Type' header value (as a dynamic string)
LgiNetFunc char *InetExtractBoundary(const char *Field);

#endif
