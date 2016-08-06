/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef GSTRING_H
#define GSTRING_H

// #include "LgiInc.h"
#include "LgiDefs.h"
#include "LgiOsDefs.h"

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
template<typename Char>
Char *StrLwr(Char *c)
{
	for (Char *s = c; s && *s; s++)
	{
		if (*s >= 'A' && *s <= 'Z')
			*s = *s - 'A' + 'a';
	}
	return c;
}

template<typename Char>
Char *StrUpr(Char *c)
{
	for (Char *s = c; s && *s; s++)
	{
		if (*s >= 'a' && *s <= 'z')
			*s = *s - 'a' + 'A';
	}
	return c;
}

template<typename T>
int StringLen(T *s)
{
	if (!s) return 0;
	int len = 0;
	while (s[len])
		len++;
	return len;
}

// 8 bit strings

/// Returns a pointer to the char 'c' if found in the first 'Len' bytes of the string 's'
LgiFunc char *strnchr
(
	/// The string to search
	const char *s,
	/// The character to find
	char c,
	/// The maximum number of bytes to search
	NativeInt Len
);

#if defined(MAC)
LgiFunc char *strncpy_s(char *dest, size_t dest_size, const char *src, size_t src_size);
#else
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
	const char *b,
	/// The maximum number of bytes in 'a' to seach through
	NativeInt n
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
	const char *b,
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
	const char *a,
	/// The string to find
	const char *b
);

#if LGI_DRMEMORY

	/// String compare function
	#define strcmp strcompare
	LgiFunc int strcompare(const char *a, const char *b, bool case_sensitive = true);

	/// Find character in string function
	#define strchr strchar
	LgiFunc char *strchar(const char *a, int ch);

	/// Append to a string
	#define strcat strconcat
	LgiFunc char *strconcat(char *dst, const char *src);

#endif

#if !defined(_MSC_VER)

#if !defined(WIN32)
LgiFunc int strnicmp(const char *a, const char *b, int i);
#endif

/// \brief Safe string copy
///
/// This function should be used anytime the size of the destination
/// buffer is known when using strcpy. It will truncate the resultant
/// string to fit in the output buffer, properly NULL terminating it.
LgiFunc char *strcpy_s
(
	/// The destination string buffer
	char *dst,
	/// The size in bytes of 'dst'
	size_t len,
	/// The string to append to 'dst'
	const char *src
);

/// \brief Safe string copy
LgiFunc char *strncpy_s
(
	/// The destination string buffer
	char *dst,
	/// The size in bytes of 'dst'
	size_t dst_len,
	/// The string to append to 'dst'
	const char *src,
	/// Max size of src
	size_t src_len
);

/// \brief Safe string append
///
/// This function should be used anytime the size of the destination
/// buffer is known when using strcat. It will truncate the resultant
/// string to fit in the output buffer, properly NULL terminating it.
LgiFunc char *strcat_s
(
	/// The destination string buffer
	char *dst,
	/// The size in bytes of 'dst'
	size_t len,
	/// The string to append to 'dst'
	const char *src
);
#endif

/// \brief Converts a hex string into a integer.
///
/// Stops scanning when it hits a NULL or a non-hex character. Accepts
/// input characters in the ranges 0-9, a-f and A-F.
template<typename Ret, typename Char>
Ret HexToInt
(
	/// The string of hex characters
	const Char *a
)
{
	Ret Status = 0;

	if (a[0] == '0' && (a[1] == 'x' || a[1] == 'X'))
		a += 2;

	for (; a && *a; a++)
	{
		if (*a >= '0' && *a <= '9')
		{
			Status <<= 4;
			Status |= *a - '0';
		}
		else if (*a >= 'a' && *a <= 'f')
		{
			Status <<= 4;
			Status |= *a - 'a' + 10;
		}
		else if (*a >= 'A' && *a <= 'F')
		{
			Status <<= 4;
			Status |= *a - 'A' + 10;
		}
		else break;
	}

	return Status;
}

template<typename Char>
uint32 htoi(const Char *s)
{
	return HexToInt<uint32,Char>(s);
}

template<typename Char>
uint64 htoi64(const Char *s)
{
	return HexToInt<uint64,Char>(s);
}

/// Trims delimiter characters off a string.
///
/// \returns A dynamically allocated copy of the input without any delimiter characters
/// on the start or end.
LgiFunc char *TrimStr(const char *s, const char *Delim = " \r\n\t");
/// Returns true if the string points to something with one or more non-whitespace characters.
LgiFunc bool ValidStr(const char *s);
/// Makes a heap allocated copy of a string.
LgiFunc char *NewStr
(
	/// The input string
	const char *s,
	/// The maximum number of bytes in the input string to use or -1 for the whole string.
	NativeInt Len = -1
);
/// Does a wildcard match.
LgiFunc bool MatchStr
(
	/// The wildcard template
	const char *Template,
	/// The string to test against.
	const char *Data
);

/// Find a character in a wide string
LgiFunc char16 *StrchrW(const char16 *s, char16 c);
/// Find the last instance of a character in a wide string
LgiFunc char16 *StrrchrW(char16 *s, char16 c);
/// Find a character in the first 'n' characters of a wide string
LgiFunc char16 *StrnchrW(char16 *s, char16 c, int Len);

/// Find a sub-string in a wide string (case sensitive)
LgiFunc char16 *StrstrW(char16 *a, const char16 *b);
/// Find a sub-string in a wide string (case insensitive)
LgiFunc char16 *StristrW(char16 *a, const char16 *b);
/// Find a sub-string in the first 'n' characters of a wide string (case sensitive)
LgiFunc char16 *StrnstrW(char16 *a, const char16 *b, int n);
/// Find a sub-string in the first 'n' characters of a wide string (case insensitive)
LgiFunc char16 *StrnistrW(char16 *a, const char16 *b, int n);

/// Compare wide strings (case sensitive)
LgiFunc int StrcmpW(const char16 *a, const char16 *b);
/// Compare wide strings (case insensitive)
LgiFunc int StricmpW(const char16 *a, const char16 *b);
/// Compare 'n' characters of 2 wide strings (case sensitive)
LgiFunc int StrncmpW(const char16 *a, const char16 *b, int n);
/// Compare 'n' characters of 2 wide strings (case insensitive)
LgiFunc int StrnicmpW(const char16 *a, const char16 *b, int n);

/// String copy one wide string to another
LgiFunc char16 *StrcpyW(char16 *a, const char16 *b);
/// String copy a maximum of 'n' characters of one wide string to another
LgiFunc char16 *StrncpyW(char16 *a, const char16 *b, int n);

/// Count the number of char16's in a wide string
LgiFunc int StrlenW(const char16 *a);
/// Append a wide string to another
LgiFunc void StrcatW(char16 *a, const char16 *b);
/// Convert a wide string to an integer
LgiFunc int AtoiW(const char16 *a);
/// Convert a wide hex string to an integer
LgiFunc int HtoiW(const char16 *a);
/// Convert a wide hex string to an 64bit integer
LgiFunc int64 HtoiW64(const char16 *a);
/// Makes a heap allocated copy of a wide string.
LgiFunc char16 *NewStrW
(
	/// The input string
	const char16 *s,
	/// The maximum number of characters in the input string to use or -1 for the whole string.
	int CharLen = -1
);
/// Trim delimiters from a wide string. Returns a heap allocated string.
LgiFunc char16 *TrimStrW(const char16 *s, const char16 *Delim = 0);
/// Returns true if 's' points to a wide string with at least 1 non-whitespace character.
LgiFunc bool ValidStrW(const char16 *s);
/// Does a widecard match between wide strings.
LgiFunc bool MatchStrW(const char16 *Template, const char16 *Data);

#endif
