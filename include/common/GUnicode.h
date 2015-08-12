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
unsigned Strlen(T *str)
{
	if (!str)
		return 0;

	register T *s = str;
	while (*s)
		s++;

	return s - str;
}

// Compares two strings, case sensitive
template<typename T>
int Strcmp(T *str_a, T *str_b)
{
	if (!str_a || !str_b)
		return -1;

	register T *a = str_a;
	register T *b = str_b;

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
int Strncmp(T *str_a, T *str_b, unsigned len)
{
	if (!str_a || !str_b)
		return -1;

	register T *a = str_a;
	register T *b = str_b;
	register T *end = a + len;

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
int Stricmp(T *str_a, T *str_b)
{
	if (!str_a || !str_b)
		return -1;

	register T *a = str_a;
	register T *b = str_b;
	register T ach, bch;

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
int Strnicmp(T *str_a, T *str_b, unsigned len)
{
	if (!str_a || !str_b)
		return -1;

	register T *a = str_a;
	register T *b = str_b;
	register T *end = a + len;
	register T ach, bch;

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
T *Strcpy(T *dst, unsigned dst_len, T *src)
{
	if (!dst || !src || dst_len == 0)
		return NULL;
	
	register T *d = dst;
	register T *end = d + dst_len - 1; // leave 1 char for NULL terminator
	register T *s = src;
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
	
	for (register T *s = str; *s; s++)
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
	for (register T *s = str; *s; s++)
	{
		if (*s == ch)
			last = s;
	}
	
	return last;
}

/// Finds the first instance of a character in the string
template<typename T>
T *Strcat(T *dst, int dst_len, T *postfix)
{
	if (!dst || !postfix || dst_len < 1)
		return NULL;
	
	// Find the end of the string to append to
	while (*dst)
	{
		dst++;
		dst_len--;
	}
	
	// Copy the postfix...
	register T *s = postfix;
	for (; *s && dst_len > 1; s++)
	{
		*dst++ = *s++;
	}
	
	if (dst_len > 0)
		*s = 0;
	
	return NULL;
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
