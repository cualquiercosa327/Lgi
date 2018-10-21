#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "GMem.h"
#include "LgiOsDefs.h"
#include "LgiClass.h"
#include "GString.h"
#include "GContainers.h"
#include "LgiCommon.h"

char WhiteSpace[] = " \t\r\n";

#if !defined(_MSC_VER)
char *strncpy_s(char *dest, size_t dest_size, const char *src, size_t src_size)
{
	if (dest && src)
	{
		char *end = dest + dest_size - 1;
		while (dest < end && *src && src_size-- > 0)
		{
			*dest++ = *src++;
		}
		*dest++ = 0;
	}
	
	return dest;
}

char *strcpy_s(char *dest, size_t dest_size, const char *src)
{
	if (!dest || !src)
		return NULL;

	char *Start = dest;

	// Copy the string
	while (*src && dest_size > 1)
	{
		*dest++ = *src++;
		dest_size--;
	}
	
	// NULL terminate
	if (dest_size > 0)
	{
		*dest++ = 0;
		dest_size--;
	}
	
	// Assert if we ran out of buffer.
	if (*src != 0)
		LgiAssert(!"Buffer too small");
	
	return Start;
}

char *strcat_s(char *dest, size_t dest_size, const char *src)
{
	if (!dest || !src)
		return NULL;
	
	char *Start = dest;
	
	// Scan over the existing string
	while (*dest && dest_size > 0)
	{
		dest++;
		dest_size--;
	}

	// Write as much as we can
	while (*src && dest_size > 1)
	{
		*dest++ = *src++;
		dest_size--;
	}
	
	// NULL terminate...
	if (dest_size > 0)
	{
		*dest++ = NULL;
		dest_size--;
	}
	
	// Assert if we ran out of buffer.
	if (*src != 0)
		LgiAssert(!"Buffer too small");
	
	return Start;
}

#endif

// 8 Bit
char *strnchr(const char *s, char c, NativeInt Len)
{
	if (s && Len >= 0)
	{
		for (ssize_t i=0; i<Len; i++)
		{
			if (s[i] == c)
				return (char*)s + i;
		}
	}
	else LgiAssert(!"Bad params");

	return 0;
}

char *strnstr(const char *a, const char *b, size_t n)
{
	if (a && b)
	{
		size_t SLen = strlen(b);
		size_t DLen = 0;
		
		while (DLen < n && a[DLen])
		{
		    DLen++;
		}

		while (SLen <= DLen && n >= SLen)
		{
			int i;
			for (i=0; i<SLen && a[i] == b[i]; i++);
			if (i == SLen) return (char*)a;

			n--;
			DLen--;
			a++;
		}
	}

	return NULL;
}

char *strnistr(char *a, const char *b, size_t n)
{
	if (a && b)
	{
		size_t SLen = strlen(b);
		while (n >= SLen)
		{
			int i;
			for (i=0; a[i] && i<SLen && tolower(a[i]) == tolower(b[i]); i++);
			if (i == SLen) return a;
			if (a[i] == 0) return 0;

			n--;
			a++;
		}
	}

	return NULL;
}

#if LGI_DRMEMORY
/// This exists to get rid of false positives when running Dr Memory.
/// The vs2005 clib causes memory errors by accessing bytes after the NULL.

char *strchar(const char *a, int ch)
{
	if (!a)
		return NULL;
	
	while (*a)
	{
		if (*a == ch)
			return (char*)a;
		a++;
	}
	
	return NULL;	
}

int strcompare(const char *a, const char *b, bool case_sensitive)
{
	if (!a || !b)
		return -1;

	if (case_sensitive)
	{
		while (*a && *b && *a == *b)
		{
			a++;
			b++;
		}
	}
	else
	{
		while (*a && *b && tolower(*a) == tolower(*b))
		{
			a++;
			b++;
		}
	}
	
	return *a - *b;
}

char *strconcat(char *dst, const char *src)
{
	char *d = dst;
	if (d && src)
	{
		while (*d)
			d++;
		while (*src)
			*d++ = *src++;
		*d++ = 0;
	}
	return dst;
}

#endif

int strnicmp(const char *a, const char *b, ssize_t i)
{
	int Cmp = -1;
	if (a && b && i > 0)
	{
		for (Cmp = 0; i-- && Cmp == 0; )
		{
			Cmp += tolower(*a) - tolower(*b);
			if (!*a || !*b) break;
			a++;
			b++;
		}
	}
	
	return Cmp;
}

char *stristr(const char *a, const char *b)
{
	if (a && b)
	{
		while (a && *a)
		{
			if (tolower(*a) == tolower(*b))
			{
				int i;
				for (i=0; a[i] && tolower(a[i]) == tolower(b[i]); i++)
					;

				if (b[i] == 0) return (char*)a;
			}
			a++;
		}
	}

	return NULL;
}

int strcmp(char *a, char *b)
{
	int c = -1;
	if (a && b)
	{
		c = 0;
		while (!c)
		{
			c = *a - *b;
			if (!*a || !*b) break;
			a++;
			b++;
		}
	}
	return c;
}

#ifndef WIN32
int stricmp(const char *a, const char *b)
{
	int c = -1;
	if (a && b)
	{
		c = 0;
		while (!c)
		{
			c = tolower(*a) - tolower(*b);
			if (!*a || !*b) break;
			a++;
			b++;
		}
	}
	return c;
}

char *strupr(char *a)
{
	for (char *s = a; s && *s; s++)
	{
		*s = toupper(*s);
	}

	return a;
}

char *strlwr(char *a)
{
	for (char *s = a; s && *s; s++)
	{
		*s = tolower(*s);
	}

	return a;
}
#endif

char *TrimStr(const char *s, const char *Delim)
{
	if (!s)
		return NULL;
	
	const char *Start = s;
	while (*Start && strchr(Delim, *Start))
	{
		Start++;
	}

	size_t StartLen = strlen(Start);
	if (StartLen == 0)
		return NULL;

	const char *End = Start + strlen(Start) - 1;
	while (*End && End > Start && strchr(Delim, *End))
	{
		End--;
	}

	if (*Start == 0)
		return NULL;

	size_t Len = (End - Start) + 1;
	char *n = new char[Len+1];
	if (!n)
		return NULL;

	memcpy(n, Start, Len);
	n[Len] = 0;
	return n;
}

bool ValidStr(const char *s)
{
	if (s)
	{
		while (*s)
		{
			if (*s != ' ' &&
				*s != '\t' &&
				*s != '\r' &&
				*s != '\n' &&
				((uchar)*s) != 0xa0)
			{
				return true;
			}

			s++;
		}
	}

	return false;
}

char *NewStr(const char *s, ptrdiff_t Len)
{
	if (s)
	{
		if (Len < 0) Len = strlen(s);
		NativeInt Bytes = Len + 1;
		char *Ret = new char[LGI_ALLOC_ALIGN(Bytes)];
		if (Ret)
		{
			if (Len > 0) memcpy(Ret, s, Len);
			Ret[Len] = 0;
			return Ret;
		}
	}
	return NULL;
}

#ifdef BEOS
#define toupper(c) ( ((c)>='a' && (c)<='z') ? (c)-'a'+'A' : (c) )
#define tolower(c) ( ((c)>='A' && (c)<='Z') ? (c)-'A'+'a' : (c) )
#endif

bool MatchStr(const char *Template, const char *Data)
{
	if (!Template)
	{
		return false;
	}

	if (_stricmp(Template, (char*)"*") == 0)
	{
		// matches anything
		return true;
	}

	if (!Data)
	{
		return false;
	}

	while (*Template && *Data)
	{
		if (*Template == '*')
		{
			Template++;

			if (*Template)
			{
				const char *EndA;
				for (EndA = Template; *EndA && *EndA!='?' && *EndA!='*'; EndA++);
				size_t SegLen = EndA - Template;
				char *Seg = NewStr(Template, SegLen);
				if (!Seg) return false;

				// find end of non match
				while (*Data)
				{
					if (_strnicmp(Data, Seg, SegLen) == 0)
					{
						break;
					}
					Data++;
				}

				DeleteArray(Seg);

				if (*Data)
				{
					Template += SegLen;
					Data += SegLen;
				}
				else
				{
					// can't find any matching part in Data
					return false;
				}
			}
			else
			{
				// '*' matches everything
				return true;
			}

		}
		else if (*Template == '?')
		{
			Template++;
			Data++;
		}
		else
		{
			if (tolower(*Template) != tolower(*Data))
			{
				return false;
			}

			Template++;
			Data++;
		}
	}

	return	((*Template == 0) || (_stricmp(Template, (char*)"*") == 0)) &&
			(*Data == 0);
}

//////////////////////////////////////////////////////////////////////////
// UTF-16
#define CompareDefault	(a && b ? *a - *b : -1)

char16 *StrchrW(const char16 *s, char16 c)
{
	if (s)
	{
		while (*s)
		{
			if (*s == c) return (char16*) s;
			s++;
		}
	}

	return 0;
}

char16 *StrnchrW(char16 *s, char16 c, int Len)
{
	if (s)
	{
		while (*s && Len > 0)
		{
			if (*s == c)
				return s;
			s++;
			Len--;
		}
	}

	return 0;
}

char16 *StrrchrW(char16 *s, char16 c)
{
	char16 *Last = 0;

	while (s && *s)
	{
		if (*s == c)
			Last = s;
		s++;
	}

	return Last;
}

char16 *StrstrW(char16 *a, const char16 *b)
{
	if (a && b)
	{
		size_t Len = StrlenW(b);
		for (char16 *s=a; *s; s++)
		{
			if (*s == *b)
			{
				// check match
				if (StrncmpW(s+1, b+1, Len-1) == 0)
					return s;
			}
		}
	}

	return 0;
}

char16 *StristrW(char16 *a, const char16 *b)
{
	if (a && b)
	{
		size_t Len = StrlenW(b);
		for (char16 *s=a; *s; s++)
		{
			if (tolower(*s) == tolower(*b))
			{
				// check match
				if (StrnicmpW(s+1, b+1, Len-1) == 0)
					return s;
			}
		}
	}

	return 0;
}

char16 *StrnstrW(char16 *a, const char16 *b, ssize_t n)
{
	if (a && b)
	{
		ssize_t Len = StrlenW(b);
		for (char16 *s=a; n >= Len && *s; s++, n--)
		{
			if (*s == *b)
			{
				// check match
				if (StrncmpW(s+1, b+1, Len-1) == 0)
					return s;
			}
		}
	}

	return 0;
}

char16 *StrnistrW(char16 *a, const char16 *b, ssize_t n)
{
	if (a && b)
	{
		ssize_t Len = StrlenW(b);
		for (char16 *s=a; n >= Len && *s; s++, n--)
		{
			if (*s == *b)
			{
				// check match
				if (StrnicmpW(s+1, b+1, Len-1) == 0)
					return s;
			}
		}
	}

	return 0;
}

int StrcmpW(const char16 *a, const char16 *b)
{
	if (a && b)
	{
		while (true)
		{
			if (!*a || !*b || *a != *b)
				return *a - *b;

			a++;
			b++;
		}

		return 0;
	}

	return -1;
}

int StricmpW(const char16 *a, const char16 *b)
{
	if (a && b)
	{
		while (true)
		{
			char16 A = tolower(*a);
			char16 B = tolower(*b);

			if (!A || !B || A != B)
				return A - B;

			a++;
			b++;
		}

		return 0;
	}

	return -1;
}

int StrncmpW(const char16 *a, const char16 *b, ssize_t n)
{
	if (a && b)
	{
		while (n > 0)
		{
			if (!*a || !*b || *a != *b)
				return *a - *b;

			a++;
			b++;
			n--;
		}

		return 0;
	}

	return -1;
}

int StrnicmpW(const char16 *a, const char16 *b, ssize_t n)
{
	if (a && b)
	{
		while (n > 0)
		{
			char16 A = tolower(*a);
			char16 B = tolower(*b);

			if (!A || !B || A != B)
				return A - B;

			a++;
			b++;
			n--;
		}

		return 0;
	}

	return -1;
}

char16 *StrcpyW(char16 *a, const char16 *b)
{
	if (a && b)
	{
		do
		{
			*a++ = *b++;
		}
		while (*b);

		*a++ = 0;
	}

	return a;
}

char16 *StrncpyW(char16 *a, const char16 *b, ssize_t n)
{
	if (a && b && n > 0)
	{
		while (*b)
		{
			if (--n <= 0)
			{
				break;
			}

			*a++ = *b++;
		}

		*a++ = 0; // always NULL terminate
	}

	return a;
}

void StrcatW(char16 *a, const char16 *b)
{
	if (a && b)
	{
		// Seek to end of string
		while (*a)
		{
			a++;
		}

		// Append 'b'
		while (*b)
		{
			*a++ = *b++;
		}

		*a++ = 0;
	}
}

ssize_t StrlenW(const char16 *a)
{
	if (!a)
		return 0;

	ssize_t i = 0;
	while (*a++)
	{
		i++;
	}

	return i;
}

int AtoiW(const char16 *a)
{
	int i = 0;
	while (a && *a >= '0' && *a <= '9')
	{
		i *= 10;
		i += *a - '0';
		a++;
	}
	return i;
}

int HtoiW(const char16 *a)
{
	int i = 0;
	if (a)
	{
		while (*a)
		{
			if (*a >= '0' && *a <= '9')
			{
				i <<= 4;
				i |= *a - '0';
			}
			else if (*a >= 'a' && *a <= 'f')
			{
				i <<= 4;
				i |= *a - 'a' + 10;
			}
			else if (*a >= 'A' && *a <= 'F')
			{
				i <<= 4;
				i |= *a - 'A' + 10;
			}
			else break;
			a++;
		}
	}
	return i;
}

char16 *TrimStrW(const char16 *s, const char16 *Delim)
{
	if (!Delim)
	{
		static char16 Def[] = {' ', '\r', '\n', '\t', 0};
		Delim = Def;
	}

	if (s)
	{
		// Leading delim
		while (StrchrW(Delim, *s))
		{
			s++;
		}

		// Trailing delim
		size_t i = StrlenW(s);
		while (i > 0 && StrchrW(Delim, s[i-1]))
		{
			i--;
		}

		return NewStrW(s, i);
	}

	return 0;
}

bool MatchStrW(const char16 *a, const char16 *b)
{
	LgiAssert(0);
	return 0;
}

char16 *NewStrW(const char16 *Str, ssize_t l)
{
	char16 *s = 0;
	if (Str)
	{
		if (l < 0) l = StrlenW(Str);
		s = new char16[l+1];
		if (s)
		{
			memcpy(s, Str, l * sizeof(char16));
			s[l] = 0;
		}
	}
	return s;
}

bool ValidStrW(const char16 *s)
{
	if (s)
	{
		for (const char16 *c=s; *c; c++)
		{
			if (*c != ' ' &&
				*c != '\t' &&
				*c != 0xfeff &&
				*c != 0xfffe)
				return true;
		}
	}

	return false;
}

char *LgiDecodeUri(const char *uri, int len)
{
	GStringPipe p;
	if (uri)
	{
		const char *end = len >= 0 ? uri + len : 0;
		for (const char *s=uri; s && *s; )
		{
			int Len;
			const char *e = s;
			for (Len = 0; *e && *e != '%' && (!end || e < end); e++)
				Len++;
			
			p.Push(s, Len);
			if ((!end || e < end) && *e)
			{
				e++;
				if (e[0] && e[1])
				{
					char h[3] = { e[0], e[1], 0 };
					char c = htoi(h);
					p.Push(&c, 1);
					e += 2;
					s = e;
				}
				else break;
			}
			else
			{
				break;
			}
		}
	}
	return p.NewStr();
}

char *LgiEncodeUri(const char *uri, int len)
{
	GStringPipe p;
	if (uri)
	{
		const char *end = len >= 0 ? uri + len : 0;
		for (const char *s=uri; s && *s; )
		{
			int Len;
			const char *e = s;
			for
			(
				Len = 0;
				*e
				&&
				(!end || e < end)
				&&
				*e > ' '
				&&
				(uchar)*e < 0x7f
				&&
				strchr("$&+,/:;=?@\'\"<>#%{}|\\^~[]`", *e) == 0;
				e++
			)
			{
				Len++;
			}
			
			p.Push(s, Len);
			if ((!end || e < end) && *e)
			{
				char h[4];
				sprintf_s(h, sizeof(h), "%%%2.2X", (uchar)*e);
				p.Push(h, 3);
				s = ++e;
			}
			else
			{
				break;
			}
		}
	}
	return p.NewStr();
}

GString LUrlEncode(const char *s, const char *delim)
{
	if (!s || !*s)
		return GString();

	char buf[256];
	int ch = 0;
	GString out;
	while (*s)
	{
		if (*s == '%' || strchr(delim, *s))
			ch += sprintf_s(buf+ch, sizeof(buf)-ch, "%%%02.2x", (uint8)*s);
		else
			buf[ch++] = *s;
		s++;
		if (ch > sizeof(buf) - 6)
		{
			buf[ch] = 0;
			out += buf;
			ch = 0;
		}
	}
	buf[ch] = 0;
	if (ch > 0)
		out += buf;
	return out;
}

GString LUrlDecode(const char *s)
{
	if (!s || !*s)
		return GString();

	char buf[256];
	int ch = 0;
	GString out;
	while (*s)
	{
		if (*s == '%')
		{
			s++;
			if (!s[0] || !s[1])
				break;
			char h[3] = {s[0], s[1], 0};
			buf[ch++] = htoi(h);
			s++;
		}
		else buf[ch++] = *s;
		s++;
		if (ch > sizeof(buf) - 6)
		{
			buf[ch] = 0;
			out += buf;
			ch = 0;
		}	
	}
	buf[ch] = 0;
	if (ch > 0)
		out += buf;
	return out;
}
