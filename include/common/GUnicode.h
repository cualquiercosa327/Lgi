//
//  GUnicode.h
//
//  Created by Matthew Allen on 1/08/15.
//

#ifndef _GUnicode_h
#define _GUnicode_h

#include "LgiInc.h"

#ifdef COCOA
#define REG
#else
#define REG register
#endif
typedef unsigned char uint8;
typedef signed char int8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;
#ifdef _MSC_VER
	typedef signed __int64 int64;
	typedef unsigned __int64 uint64;
	#ifdef  _WIN64
	typedef signed __int64		ssize_t;
	#else
	typedef signed int			ssize_t;
	#endif
#else
	typedef signed long long int64;
	typedef unsigned long long uint64;
#endif

// Defines for decoding UTF8
#define IsUtf8_1Byte(c)		( ((uint8)(c) & 0x80) == 0x00 )
#define IsUtf8_2Byte(c)		( ((uint8)(c) & 0xe0) == 0xc0 )
#define IsUtf8_3Byte(c)		( ((uint8)(c) & 0xf0) == 0xe0 )
#define IsUtf8_4Byte(c)		( ((uint8)(c) & 0xf8) == 0xf0 )

#define IsUtf8_Lead(c)		( ((uint8)(c) & 0xc0) == 0xc0 )
#define IsUtf8_Trail(c)		( ((uint8)(c) & 0xc0) == 0x80 )

// Stand-alone functions

/// Convert a single utf-8 char to utf-32 or returns -1 on error.
inline int32 LgiUtf8To32(uint8 *&i, ssize_t &Len)
{
	int32 Out = 0;

	#define InvalidUtf()		{ Len--; i++; return -1; }

	if (Len > 0)
	{
		if (!*i)
		{
			Len = 0;
			return 0;
		}

		if (IsUtf8_1Byte(*i))
		{
			// 1 byte UTF-8
			Len--;
			return *i++;
		}
		else if (IsUtf8_2Byte(*i))
		{
			// 2 byte UTF-8
			if (Len > 1)
			{
				Out = ((int)(*i++ & 0x1f)) << 6;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= *i++ & 0x3f;
					Len--;
				}
				else InvalidUtf()
			}
		}
		else if (IsUtf8_3Byte(*i))
		{
			// 3 byte UTF-8
			if (Len > 2)
			{
				Out = ((int)(*i++ & 0x0f)) << 12;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= ((int)(*i++ & 0x3f)) << 6;
					Len--;

					if (IsUtf8_Trail(*i))
					{
						Out |= *i++ & 0x3f;
						Len--;
					}
					else InvalidUtf()
				}
				else InvalidUtf()
			}
		}
		else if (IsUtf8_4Byte(*i))
		{
			// 4 byte UTF-8
			if (Len > 3)
			{
				Out = ((int)(*i++ & 0x07)) << 18;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= ((int)(*i++ & 0x3f)) << 12;
					Len--;

					if (IsUtf8_Trail(*i))
					{
						Out |= ((int)(*i++ & 0x3f)) << 6;
						Len--;

						if (IsUtf8_Trail(*i))
						{
							Out |= *i++ & 0x3f;
							Len--;
						}
						else InvalidUtf()
					}
					else InvalidUtf()
				}
				else InvalidUtf()
			}
		}
		else InvalidUtf()
	}

	return Out;
}

/// Convert a single utf-32 char to utf-8
inline bool LgiUtf32To8(uint32 c, uint8 *&i, ssize_t &Len)
{
	if ((c & ~0x7f) == 0)
	{
		if (Len > 0)
		{
			*i++ = c;
			Len--;
			return true;
		}
	}
	else if ((c & ~0x7ff) == 0)
	{
		if (Len > 1)
		{
			*i++ = 0xc0 | (c >> 6);
			*i++ = 0x80 | (c & 0x3f);
			Len -= 2;
			return true;
		}
	}
	else if ((c & 0xffff0000) == 0)
	{
		if (Len > 2)
		{
			*i++ = 0xe0 | (c >> 12);
			*i++ = 0x80 | ((c & 0x0fc0) >> 6);
			*i++ = 0x80 | (c & 0x3f);
			Len -= 3;
			return true;
		}
	}
	else
	{
		if (Len > 3)
		{
			*i++ = 0xf0 | (c >> 18);
			*i++ = 0x80 | ((c&0x3f000) >> 12);
			*i++ = 0x80 | ((c&0xfc0) >> 6);
			*i++ = 0x80 | (c&0x3f);
			Len -= 4;
			return true;
		}
	}

	return false;
}

// Defined for decoding UTF16
#define IsUtf16_Lead(c)		( ((uint16)(c) & 0xfc00) == 0xD800 )
#define IsUtf16_Trail(c)	( ((uint16)(c) & 0xfc00) == 0xDc00 )

/// Convert a single utf-16 char to utf-32
inline uint32 LgiUtf16To32(const uint16 *&i, ssize_t &Len)
{
	if (Len > 1)
	{
		if (!*i)
		{
			Len = 0;
			return 0;
		}

		int n = *i & 0xfc00;
		if (n == 0xd800 || n == 0xdc00)
		{
			// 2 word UTF
			if (Len > 3)
			{
				Len -= sizeof(uint16)<<1;
				int w = (*i & 0x3c0) >> 6;
				int zy = *i++ & 0x3f;
				return ((w + 1) << 16) | (zy << 10) | (*i++ & 0x3ff);
			}
		}

		// 1 word UTF
		Len -= sizeof(uint16);
		return *i++;
	}

	return 0;
}

/// Convert a single utf-32 char to utf-16
inline bool LgiUtf32To16(uint32 c, uint16 *&i, ssize_t &Len)
{
	if (c >= 0x10000)
	{
		// 2 word UTF
		if (Len < 4)
			return false;

		int w = c - 0x10000;
		*i++ = 0xd800 + (w >> 10);
		*i++ = 0xdc00 + (c & 0x3ff);
		Len -= sizeof(*i) << 1;
	}
	else
	{
		if (Len < 2)
			return false;
		if (c > 0xD7FF && c < 0xE000)
			return false;

		// 1 word UTF
		*i++ = c;
		Len -= sizeof(*i);
		return true;
	}
	
	return false;
}

/// Seeks the pointer 'Ptr' to the next utf-8 character
inline bool LgiNextUtf8(char *&p)
{
	char *old = p;
	
	if (IsUtf8_Lead(*p))
	{
		p++;

		while (IsUtf8_Trail(*p))
			p++;
	}
	else p++;
	
	return p > old;
}

/// Seeks the pointer 'Ptr' to the previous utf-8 character
inline void LgiPrevUtf8(char *&p)
{
	p--;
	while (IsUtf8_Trail(*p))
		p--;
}

/// Pointer to utf-8 string
class LgiClass GUtf8Ptr
{
protected:
	uint8 *Ptr;

public:
	GUtf8Ptr(void *p = 0);

	/// Assign a new pointer to the string
	GUtf8Ptr &operator =(char *s) { Ptr = (uint8*)s; return *this; }
	/// Assign a new pointer to the string
	GUtf8Ptr &operator =(uint8 *s) { Ptr = s; return *this; }
	/// \returns the current character in the string or -1 on error.
	operator int32();

	/// Seeks forward
	GUtf8Ptr &operator ++();
	GUtf8Ptr &operator ++(const int i);
	GUtf8Ptr &operator +=(const int n);

	/// Seeks 1 character backward
	GUtf8Ptr &operator --();
	GUtf8Ptr &operator --(const int i);
	GUtf8Ptr &operator -=(const int n);

	// Comparison
	bool operator <(const GUtf8Ptr &p) { return Ptr < p.Ptr; }
	bool operator <=(const GUtf8Ptr &p) { return Ptr <= p.Ptr; }
	bool operator >(const GUtf8Ptr &p) { return Ptr > p.Ptr; }
	bool operator >=(const GUtf8Ptr &p) { return Ptr >= p.Ptr; }
	bool operator ==(const GUtf8Ptr &p) { return Ptr == p.Ptr; }
	bool operator !=(const GUtf8Ptr &p) { return Ptr != p.Ptr; }
	ptrdiff_t operator -(const GUtf8Ptr &p) { return Ptr - p.Ptr; }

	/// Gets the bytes between the cur pointer and the end of the buffer or string.
	int GetBytes();
	/// Gets the characters between the cur pointer and the end of the buffer or string.
	int GetChars();
	/// Encodes a utf-8 char at the current location and moves the pointer along
	void Add(wchar_t c);

	/// Returns the current pointer.
	uint8 *GetPtr() { return Ptr; }
};

/// Unicode string class. Allows traversing a utf-8 strings.
class LgiClass GUtf8Str : public GUtf8Ptr
{
	// Complete buffer
	uint8 *Start;
	uint8 *End;
	GUtf8Ptr Cur;
	bool Own;

	void Empty();

public:
	/// Constructor
	GUtf8Str
	(
		/// The string pointer to start with
		char *utf,
		/// The number of bytes containing characters, or -1 if NULL terminated.
		int bytes = -1,
		/// Copy the string first
		bool Copy = false
	);
	/// Constructor
	GUtf8Str
	(
		/// The string pointer to start with. A utf-8 copy of the string will be created.
		wchar_t *wide,
		/// The number of wide chars, or -1 if NULL terminated.
		int chars = -1
	);
	~GUtf8Str();

	/// Assign a new pointer to the string
	GUtf8Str &operator =(char *s);

	/// Allocates a block of memory containing the wide representation of the string.
	wchar_t *ToWide();
	/// \returns true if the class seems to be valid.
	bool Valid();
	/// \returns true if at the start
	bool IsStart();
	/// \returns true if at the end
	bool IsEnd();
};


// Converts character to lower case
template<typename T>
T Tolower(T ch)
{
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 'a';
	return ch;
}

template<typename T>
T *Strlwr(T *Str)
{
	if (!Str)
		return NULL;
	for (T *s = Str; *s; s++)
	{
		if (*s >= 'A' && *s <= 'Z')
			*s = *s - 'A' + 'a';
	}

	return Str;
}

// Converts character to upper case
template<typename T>
T Toupper(T ch)
{
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 'A';
	return ch;
}

template<typename T>
T *Strupr(T *Str)
{
	if (!Str)
		return NULL;
	for (T *s = Str; *s; s++)
	{
		if (*s >= 'a' && *s <= 'z')
			*s = *s - 'a' + 'A';
	}

	return Str;
}

// Finds the length of the string in characters
template<typename T>
ssize_t Strlen(const T *str)
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
int Strncmp(const T *str_a, const T *str_b, ssize_t len)
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
int Strnicmp(const T *str_a, const T *str_b, ssize_t len)
{
	if (!str_a || !str_b || len == 0)
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
T *Strcpy(T *dst, ssize_t dst_len, const T *src)
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

/// Finds the first instance of a character in the string
template<typename T>
T *Strnchr(T *str, int ch, size_t len)
{
	if (!str || len == 0)
		return NULL;
	
	REG T *e = str + len;
	for (REG T *s = str; s < e; s++)
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

/// Searches the string 'Data' for the 'Value' in a case insensitive manner
template<typename T>
T *Stristr(const T *Data, const T *Value)
{
	if (!Data || !Value)
		return NULL;

	const T v = Tolower(*Value);
	while (*Data)
	{
		if (Tolower(*Data) == v)
		{
			int i;
			for (i=1; Data[i] && Tolower(Data[i]) == Tolower(Value[i]); i++)
				;

			if (Value[i] == 0)
				return (T*)Data;
		}
		
		Data++;
	}

	return NULL;
}

/// Searches the string 'Data' for the 'Value' in a case insensitive manner
template<typename T>
T *Strnstr(const T *Data, const T *Value, ptrdiff_t DataLen)
{
	if (!Data || !Value)
		return NULL;

	const T v = *Value;
	ptrdiff_t ValLen = Strlen(Value);
	if (ValLen > DataLen)
		return NULL;
	
	while (*Data && DataLen >= ValLen)
	{
		if (*Data == v)
		{
			int i;
			for (i=1; Data[i] && Data[i] == Value[i]; i++)
				;

			if (Value[i] == 0)
				return (T*)Data;
		}

		Data++;
		DataLen--;
	}

	return NULL;
}

/// Searches the string 'Data' for the 'Value' in a case insensitive manner
template<typename T>
T *Strnistr(const T *Data, const T *Value, ptrdiff_t DataLen)
{
	if (!Data || !Value)
		return NULL;

	const T v = Tolower(*Value);
	ptrdiff_t ValLen = Strlen(Value);
	if (ValLen > DataLen)
		return NULL;
	
	while (*Data && DataLen >= ValLen)
	{
		if (Tolower(*Data) == v)
		{
			int i;
			for (i=1; Data[i] && Tolower(Data[i]) == Tolower(Value[i]); i++)
				;

			if (Value[i] == 0)
				return (T*)Data;
		}

		Data++;
		DataLen--;
	}

	return NULL;
}

/// Converts a string to int64 (base 10)
template<typename T>
int64 Atoi(const T *s, int Base = 10, int64 DefaultValue = -1)
{
	if (!s)
		return DefaultValue;

	bool Minus = false;
	if (*s == '-')
	{
		Minus = true;
		s++;
	}
	else if (*s == '+')
		s++;

	int64 v = 0;
	const T *Start = s;
	if (Base <= 10)
	{
		while (*s >= '0' && *s <= '9')
		{
			int d = *s - '0';
			v *= Base;
			v += d;
			s++;
		}
	}
	else
	{
		if (*s == '0' && Tolower(s[1]) == 'x')
			s += 2;

		int ValidChars = Base > 10 ? Base - 10 : 0;
		while (*s)
		{
			int d;
			if (*s >= '0' && *s <= '9')
				d = *s - '0';
			else if (*s >= 'a' && *s <= 'a' + ValidChars)
				d = *s - 'a' + 10;
			else if (*s >= 'A' && *s <= 'A' + ValidChars)
				d = *s - 'A' + 10;
			else
				break;

			v *= Base;
			v += d;
			s++;
		}
	}

	if (s == Start)
		return DefaultValue;

	return Minus ? -v : v;
}

/// Converts a utf-8 string into a wide character string
/// \ingroup Text
LgiFunc wchar_t *Utf8ToWide
(
	/// Input string
	const char *In,
	/// [Optional] Size of 'In' in 'chars' or -1 for NULL terminated
	ssize_t InLen = -1
);

/// Converts a wide character string into a utf-8 string
/// \ingroup Text
LgiFunc char *WideToUtf8
(
	/// Input string
	const wchar_t *In,
	/// [Optional] Number of wchar_t's in the input or -1 for NULL terminated
	ptrdiff_t InLen = -1
);

#endif
