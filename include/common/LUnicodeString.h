#ifndef _LUNICODE_STR_H_
#define _LUNICODE_STR_H_

#include "GUnicode.h"

/// Yet another attempt to abstract away the unicode nightmare.
template<typename T>
class LUnicodeString
{
	T *start, *cur, *next;
	ssize_t words, chars, alloc/*words*/;
	bool own;

	bool Alloc(ssize_t words)
	{
		if (start && own)
		{
			ssize_t c = cur - start, n = next - start;
			start = (T*) realloc(start, sizeof(T) * words);
			if (!start)
				return false;
			cur = start + c;
			next = start + n;
		}
		else
		{
			start = (T*) calloc(sizeof(T), words);
			cur = start;
			next = NULL;
		}
		alloc = words;
		own = true;
		return true;
	}

	uint32 Read(const uint8 *&s)
	{
		if (IsUtf8_1Byte(*s))
			return *s++;

		#define Trailing(sh) \
			if (!IsUtf8_Trail(*s)) return 0; \
			Out |= (*s++ & 0x3f) << sh;

		if (IsUtf8_2Byte(*s))
		{
			uint32 Out = ((int)(*s++ & 0x1f)) << 6;
			Trailing(0);
			return Out;
		}

		if (IsUtf8_3Byte(*s))
		{
			uint32 Out = ((int)(*s++ & 0x1f)) << 12;
			Trailing(6);
			Trailing(0);
			return Out;
		}

		if (IsUtf8_4Byte(*s))
		{
			uint32 Out = ((int)(*s++ & 0x1f)) << 18;
			Trailing(12);
			Trailing(6);
			Trailing(0);
			return Out;
		}

		#undef Trailing

		return 0;
	}

	uint32 Read(uint8 *&s) { return Read((const uint8*&)s); }
	uint32 Read(char *&s) { return Read((const uint8*&)s); }

	uint32 Read(const uint16 *&s)
	{
		int n = *s & 0xfc00;
		if (n == 0xd800 || n == 0xdc00)
		{
			int w = (*s & 0x3c0) >> 6;
			int zy = *s++ & 0x3f;
			return ((w + 1) << 16) | (zy << 10) | (*s++ & 0x3ff);
		}
		return *s++;
	}

	uint32 Read(uint16 *&s) { return Read((const uint16*&)s); }

	void Write(uint16 *&s, uint32 ch)
	{
		if (ch < 0x10000)
		{
			if (!(ch > 0xD7FF && ch < 0xE000))
			{
				// 1 word UTF
				*s++ = ch;
				return;
			}
			
			LgiAssert(!"Invalid char.");
			return;
		}

		// 2 word UTF
		int w = ch - 0x10000;
		*s++ = 0xd800 + (w >> 10);
		*s++ = 0xdc00 + (ch & 0x3ff);
	}

	#ifdef WINDOWS
	uint32 Read(char16 *&s) { return Read((const uint16*&)s); }
	void Write(char16 *&s, uint32 ch) { Write((uint16*&)s, ch); }
	#else
	uint32 Read(char16 *&s) { return Read((const uint32*&)s); }
	#endif

	uint32 Read(const uint32 *&s)
	{
		return *s++;
	}

	void Write(uint32 *&s, uint32 ch)
	{
		*s++ = ch;
	}

	void Scan()
	{
		if (chars >= 0)
			return;

		chars = 0;
		if (words >= 0)
		{
			// Length terminated
			T *End = start + words;
			T *s = start;
			while (s < End)
			{
				if (Read(s))
					chars++;
			}
		}
		else
		{
			// NULL terminated
			T *s = start;
			while (Read(s))
				chars++;
			words = s - start;
		}
	}

	void Construct(T *init, ssize_t sz)
	{
		own = false;

		start = init;
		cur = init;
		next = NULL;

		words = sz;
		chars = -1;
		alloc = 0;
	}

public:
	LUnicodeString(T *init = NULL, ssize_t words = -1) { Construct(init, words); }
	~LUnicodeString() { Empty(); }
	ssize_t Bytes() { Scan(); return words * sizeof(T); }
	ssize_t Words() { Scan(); return words; }
	ssize_t Chars() { Scan(); return chars; }
	T *Get() { return Start; }
	T *End() { Scan(); return start + words; }

	void Empty()
	{
		if (own)
		{
			free(start);
			start = NULL;
			own = false;
		}
		next = cur = NULL;
		words = alloc = 0;
		chars = -1;
	}
	
	class Iter
	{
		LUnicodeString<T> *u;
		T *p;
	
	public:
		Iter(LUnicodeString<T> *str, T *cur) : u(str), p(cur) {}
		
		operator uint32()
		{
			return u->Read(p);
		}

		Iter &operator =(uint32 ch)
		{
			LgiAssert(u->start != NULL);
			u->Write(p, ch);
			return *this;
		}

		Iter &operator *()
		{
			return *this;
		}
	};

	Iter operator *()
	{
		return Iter(this, cur);
	}

	Iter operator ++(int)
	{
		if (!start)
			Alloc(256);
		Iter old(this, cur);
		Read(cur);
		return old;
	}
};

LgiFunc bool LUnicodeString_UnitTests();

#endif