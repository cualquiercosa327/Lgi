//
//  GUnicode.h
//  LgiCarbon
//
//  Created by Matthew Allen on 1/08/15.
//
//

#ifndef LgiCarbon_GUnicode_h
#define LgiCarbon_GUnicode_h

#include "LgiInc.h"

#ifdef COCOA
#define REG
#else
#define REG register
#endif

// Converts character to lower case
template<typename T>
T Tolower(T ch)
{
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 'a';
	return ch;
}

// Converts character to upper case
template<typename T>
T Toupper(T ch)
{
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 'A';
	return ch;
}

// Finds the length of the string in characters
template<typename T>
unsigned Strlen(const T *str)
{
	if (!str)
		return 0;

	REG const T *s = str;
	while (*s)
		s++;

	return s - str;
}

// Compares two strings, case sensitive
template<typename T>
int Strcmp(const T *str_a, const T *str_b)
{
	if (!str_a || !str_b)
		return -1;

	REG const T *a = str_a;
	REG const T *b = str_b;

	while (true)
	{
		if (!*a || !*b || *a != *b)
			return *a - *b;

		a++;
		b++;
	}

	return 0;
}

// Compares the first 'len' chars of two strings, case sensitive
template<typename T>
int Strncmp(const T *str_a, const T *str_b, unsigned len)
{
	if (!str_a || !str_b)
		return -1;

	REG const T *a = str_a;
	REG const T *b = str_b;
	REG const T *end = a + len;

	while (a < end)
	{
		if (!*a || !*b || *a != *b)
			return *a - *b;

		a++;
		b++;
	}

	return 0;
}

// Compares two strings, case insensitive
template<typename T>
int Stricmp(const T *str_a, const T *str_b)
{
	if (!str_a || !str_b)
		return -1;

	REG const T *a = str_a;
	REG const T *b = str_b;
	REG T ach, bch;

	while (true)
	{
		ach = Tolower(*a);
		bch = Tolower(*b);
			
		if (!ach || !bch || ach != bch)
			return ach - bch;
			
		a++;
		b++;
	}

	return 0;
}

// Compares the first 'len' chars of two strings, case insensitive
template<typename T>
int Strnicmp(const T *str_a, const T *str_b, unsigned len)
{
	if (!str_a || !str_b)
		return -1;

	REG const T *a = str_a;
	REG const T *b = str_b;
	REG const T *end = a + len;
	REG T ach, bch;

	while (a < end)
	{
		ach = Tolower(*a);
		bch = Tolower(*b);
			
		if (!ach || !bch || ach != bch)
			return ach - bch;
			
		a++;
		b++;
	}

	return 0;
}

/// Copies a string
template<typename T>
T *Strcpy(T *dst, unsigned dst_len, const T *src)
{
	if (!dst || !src || dst_len == 0)
		return NULL;
	
	REG T *d = dst;
	REG T *end = d + dst_len - 1; // leave 1 char for NULL terminator
	REG const T *s = src;
	while (d < end && *s)
	{
		*d++ = *s++;
	}
	
	*d = 0; // NULL terminate
	return dst;
}

/// Finds the first instance of a character in the string
template<typename T>
T *Strchr(T *str, int ch)
{
	if (!str)
		return NULL;
	
	for (REG T *s = str; *s; s++)
	{
		if (*s == ch)
			return s;
	}
	
	return NULL;
}

/// Finds the last instance of a character in the string
template<typename T>
T *Strrchr(T *str, int ch)
{
	if (!str)
		return NULL;
	
	T *last = NULL;
	for (REG T *s = str; *s; s++)
	{
		if (*s == ch)
			last = s;
	}
	
	return last;
}

/// Appends a string to another
template<typename T>
T *Strcat(T *dst, int dst_len, const T *postfix)
{
	if (!dst || !postfix || dst_len < 1)
		return NULL;
	
	// Find the end of the string to append to
	while (*dst)
	{
		dst++;
		dst_len--;
	}
	
	// Reuse string copy at this point
	Strcpy(dst, dst_len, postfix);
	
	// Return the start of the complete string
	return dst;
}

/// Converts a utf-8 string into a wide character string
/// \ingroup Text
LgiFunc char16 *LgiNewUtf8To16(const char *In, int InLen = -1);

/// Converts a wide character string into a utf-8 string
/// \ingroup Text
LgiFunc char *LgiNewUtf16To8
(
	/// Input string
	const char16 *In,
	/// Number of bytes in the input or -1 for NULL terminated
	int InLen = -1
);


#endif
