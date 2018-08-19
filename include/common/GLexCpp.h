#ifndef _GLEXCPP_H_

#include "LgiInc.h"
#include "LgiOsDefs.h"
#include "GString.h"

/// The callback type for string duplication while lexing
typedef char16 *(*LexCppStrdup)(void *Context, const char16 *Str, ssize_t Chars);

/// Default callback if you want a heap string
extern char16 *LexStrdup(void *context, const char16 *start, ssize_t len);
/// Default callback if you don't need the string
extern char16 *LexNoReturn(void *context, const char16 *start, ssize_t len);

/// Converts the next lexical component into a string token
extern char16 *LexCpp
(
	/// The current position in the parsing
	char16 *&Cur,
	/// The callback responsible for duplicating the string.
	/// Use 'LexStrdup' to allocate a heap string and return it.
	/// Use 'LexNoReturn' to toss the string away.
	/// Or roll your own string handling.
	LexCppStrdup Alloc,
	/// The callback's context variable
	void *Context = NULL,
	/// [Optional] Line count in the source file
	int *LineCount = NULL
);

#endif
