/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef GSTRING_H
#define GSTRING_H

#include "LgiInc.h"
#include "LgiDefs.h"

/////////////////////////////////////////////////////////////////////
// Strings

// Externs
LgiExtern char WhiteSpace[];

// Defines
#ifndef ToUpper
#define ToUpper(c)		((c) >= 'a' && (c) <= 'z' ? (c)-'a'+'A' : (c))
#endif
#ifndef ToLower
#define ToLower(c)		((c) >= 'A' && (c) <= 'Z' ? (c)-'A'+'a' : (c))
#endif

// Functions

// 8 bit strings

/// Returns a pointer to the char 'c' if found in the first 'Len' bytes of the string 's'
LgiFunc char *strnchr
(
	/// The string to search
	char *s,
	/// The character to find
	char c,
	/// The maximum number of bytes to search
	int Len
);

#ifndef MAC
/// \brief Search for a substring in another string.
///
/// The search is case sensitive.
/// 
/// \returns A pointer to the sub-string or NULL if not found
LgiFunc char *strnstr
(
	/// The string to search
	char *a,
	/// The string to find
	char *b,
	/// The maximum number of bytes in 'a' to seach through
	int n
);
#endif

/// \brief Search for a case insensitive sub-string in another string
///
/// The search is not case sensitive.
///
/// \returns A pointer to the sub-string or NULL if not found.
LgiFunc char *strnistr
(
	/// The string to search
	char *a,
	/// The string to find
	char *b,
	/// The maximum number of bytes of 'a' to search.
	int n
);
/// \brief Case insensitive sub-string search.
///
/// The search is not case sensitive.
///
/// \returns A pointer to the sub-string or NULL if not found.

// LgiFunc
LgiFunc char *stristr
(
	/// The string to search
	char *a,
	/// The string to find
	char *b
);


// LgiFunc int stricmp(char *a, char *b);
LgiFunc char *strnistr(char *a, char *b, int n);
#ifndef WIN32
LgiFunc int strnicmp(char *a, char *b, int i);
#endif


/// \brief Safe string copy
///
/// This function should be used anytime the size of the destination
/// buffer is known when using strcpy. It will truncate the resultant
/// string to fit in the output buffer, properly NULL terminating it.
LgiFunc char *strsafecpy
(
	/// The destination string buffer
	char *dst,
	/// The string to append to 'dst'
	char *src,
	/// The size in bytes of 'dst'
	int len
);
/// \brief Safe string append
///
/// This function should be used anytime the size of the destination
/// buffer is known when using strcat. It will truncate the resultant
/// string to fit in the output buffer, properly NULL terminating it.
LgiFunc char *strsafecat
(
	/// The destination string buffer
	char *dst,
	/// The string to append to 'dst'
	char *src,
	/// The size in bytes of 'dst'
	int len
);
/// \brief Converts a hex string into a integer.
///
/// Stops scanning when it hits a NULL or a non-hex character. Accepts
/// input characters in the ranges 0-9, a-f and A-F.
LgiFunc int htoi
(
	/// The string of hex characters
	char *a
);
/// \brief Converts a hex string into a 64bit integer.
///
/// Stops scanning when it hits a NULL or a non-hex character. Accepts
/// input characters in the ranges 0-9, a-f and A-F.
LgiFunc int64 htoi64
(
	/// The string of hex characters
	char *a
);
/// Trims delimiter characters off a string.
///
/// \returns A dynamically allocated copy of the input without any delimiter characters
/// on the start or end.
LgiFunc char *TrimStr(char *s, char *Delim = " \r\n\t");
/// Returns true if the string points to something with one or more non-whitespace characters.
LgiFunc bool ValidStr(char *s);
/// Makes a heap allocated copy of a string.
LgiFunc char *NewStr
(
	/// The input string
	char *s,
	/// The maximum number of bytes in the input string to use or -1 for the whole string.
	int Len = -1
);
/// Does a wildcard match.
LgiFunc bool MatchStr
(
	/// The wildcard template
	char *Template,
	/// The string to test against.
	char *Data
);

/// Find a character in a wide string
LgiFunc char16 *StrchrW(char16 *s, char16 c);
/// Find the last instance of a character in a wide string
LgiFunc char16 *StrrchrW(char16 *s, char16 c);
/// Find a character in the first 'n' characters of a wide string
LgiFunc char16 *StrnchrW(char16 *s, char16 c, int Len);

/// Find a sub-string in a wide string (case sensitive)
LgiFunc char16 *StrstrW(char16 *a, char16 *b);
/// Find a sub-string in a wide string (case insensitive)
LgiFunc char16 *StristrW(char16 *a, char16 *b);
/// Find a sub-string in the first 'n' characters of a wide string (case sensitive)
LgiFunc char16 *StrnstrW(char16 *a, char16 *b, int n);
/// Find a sub-string in the first 'n' characters of a wide string (case insensitive)
LgiFunc char16 *StrnistrW(char16 *a, char16 *b, int n);

/// Compare wide strings (case sensitive)
LgiFunc int StrcmpW(char16 *a, char16 *b);
/// Compare wide strings (case insensitive)
LgiFunc int StricmpW(char16 *a, char16 *b);
/// Compare 'n' characters of 2 wide strings (case sensitive)
LgiFunc int StrncmpW(char16 *a, char16 *b, int n);
/// Compare 'n' characters of 2 wide strings (case insensitive)
LgiFunc int StrnicmpW(char16 *a, char16 *b, int n);

/// String copy one wide string to another
LgiFunc char16 *StrcpyW(char16 *a, char16 *b);
/// String copy a maximum of 'n' characters of one wide string to another
LgiFunc char16 *StrncpyW(char16 *a, char16 *b, int n);

/// Count the number of char16's in a wide string
LgiFunc int StrlenW(char16 *a);
/// Append a wide string to another
LgiFunc void StrcatW(char16 *a, char16 *b);
/// Convert a wide hex string to an integer
LgiFunc int HtoiW(char16 *a);
/// Convert a wide hex string to an 64bit integer
LgiFunc int64 HtoiW64(char16 *a);
/// Makes a heap allocated copy of a wide string.
LgiFunc char16 *NewStrW
(
	/// The input string
	char16 *s,
	/// The maximum number of bytes in the input string to use or -1 for the whole string.
	int Len = -1
);
/// Trim delimiters from a wide string. Returns a heap allocated string.
LgiFunc char16 *TrimStrW(char16 *s, char16 *Delim = 0);
/// Returns true if 's' points to a wide string with at least 1 non-whitespace character.
LgiFunc bool ValidStrW(char16 *s);
/// Does a widecard match between wide strings.
LgiFunc bool MatchStrW(char16 *Template, char16 *Data);

#endif
