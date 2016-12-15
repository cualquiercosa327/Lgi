/*
 * A mis-guided attempt to make a string class look and feel like a python string.
 *
 * Author: Matthew Allen
 * Email: fret@memecode.com
 * Created: 16 Sept 2014
 */
#ifndef _GSTRING_CLASS_H_
#define _GSTRING_CLASS_H_

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
	// This fixes compile errors in VS2008/Gtk
	#undef _SIGN_DEFINED
	#undef abs
#endif
#include <math.h>
#if defined(_MSC_VER) && _MSC_VER < 1800/*_MSC_VER_VS2013*/
	#include <xmath.h>
	#define PRId64 "I64i"
#else
	#include <stdint.h>
	#include <inttypes.h>
#endif
#include "GUnicode.h"
#include "GArray.h"

LgiExtern int LgiPrintf(class GString &Str, const char *Format, va_list &Arg);

/// A pythonic string class.
class GString
{
protected:
	/// This structure holds the string's data itself and is shared
	/// between one or more GString instances.
	struct RefStr
	{
		/// A reference count
		int32 Refs;
		/// The bytes in 'Str' not including the NULL terminator
		uint32 Len;
		/// The first byte of the string. Further bytes are allocated
		/// off the end of the structure using malloc. This must always
		/// be the last element in the struct.
		char Str[1];
	}	*Str;

	inline void _strip(GString &ret, const char *set, bool left, bool right)
	{
		if (!Str)
			return;
		
		char *s = Str->Str;
		char *e = s + Str->Len;

		if (!set) set = " \t\r\n";

		if (left)
		{		
			while (s < e && strchr(set, *s))
				s++;
		}
		
		if (right)
		{
			while (e > s && strchr(set, e[-1]))
				e--;
		}
		
		if (e > s)
			ret.Set(s, e - s);
	}

public:
	#ifdef LGI_UNIT_TESTS
	static int32 RefStrCount;
	#endif

	/// A copyable array of strings
	class Array : public GArray<GString>
	{
	public:
		Array() {}
		Array(const Array &a)
		{
			*this = (Array&)a;
		}
		
		Array &operator =(const Array &a)
		{
			SetFixedLength(false);
			*((GArray<GString>*)this) = a;
			SetFixedLength(true);
			return *this;
		}
	};

	/// Empty constructor
	GString()
	{
		Str = NULL;
	}

	// This odd looking constructor allows the object to be used as the value type
	// in a GHashTable, where the initialiser is '0', an integer.
	GString(int i)
	{
		Str = NULL;
	}
	
	/// String constructor
	GString(const char *str, int bytes)
	{
		Str = NULL;
		Set(str, bytes);
	}

	/// const char* constructor
	GString(const char *str)
	{
		Str = NULL;
		Set(str);
	}

	/// const char16* constructor
	GString(const wchar_t *str, int chars = -1)
	{
		Str = NULL;
		char *Utf = WideToUtf8(str, chars < 0 ? -1 : chars);
		if (Utf)
		{
			Set(Utf);
			delete [] Utf;
		}
	}

	/// GString constructor
	GString(const GString &s)
	{
		Str = s.Str;
		if (Str)
			Str->Refs++;
	}
	
	~GString()
	{
		Empty();
	}
	
	/// Removes a reference to the string and deletes if needed
	void Empty()
	{
		if (!Str) return;
		Str->Refs--;
		assert(Str->Refs >= 0);
		if (Str->Refs == 0)
		{
			free(Str);
			#ifdef LGI_UNIT_TESTS
			RefStrCount--;
			#endif
		}
		Str = NULL;
	}
	
	/// Returns the pointer to the string data
	char *Get() const
	{
		return Str ? Str->Str : NULL;
	}
	
	/// Sets the string to a new value
	bool Set
	(
		/// Can be a pointer to string data or NULL to create an empty buffer (requires valid length)
		const char *str,
		/// Byte length of input string or -1 to copy till the NULL terminator.
		ptrdiff_t bytes = -1
	)
	{
		Empty();

		if (bytes < 0)
		{
			if (str)
				bytes = strlen(str);
			else
				return false;
		}

		Str = (RefStr*)malloc(sizeof(RefStr) + bytes);
		if (!Str)
			return false;
		
		Str->Refs = 1;
		Str->Len = (uint32)bytes;
		#ifdef LGI_UNIT_TESTS
		RefStrCount++;
		#endif
		
		if (str)
			memcpy(Str->Str, str, bytes);
		
		Str->Str[bytes] = 0;
		return true;
	}

	/// Equality operator (case sensitive)
	bool operator ==(const GString &s)
	{
		const char *a = Get();
		const char *b = s.Get();
		if (!a && !b)
			return true;
		if (!a || !b)
			return false;
		return !strcmp(a, b);
	}

	bool operator !=(const GString &s)
	{
		return !(*this == s);
	}
	
	// Equality function (default: case insensitive, as the operator== is case sensitive)
	bool Equals(const GString &s, bool CaseInsensitive = true)
	{
		const char *a = Get();
		const char *b = s.Get();
		if (!a && !b)
			return true;
		if (!a || !b)
			return false;
		return !(CaseInsensitive ? _stricmp(a, b) : strcmp(a, b));
	}

	/// Assignment operator to copy one string to another
	GString &operator =(const GString &s)
	{
		if (this != &s)
		{
			Empty();
			Str = s.Str;
			if (Str)
				Str->Refs++;
		}
		return *this;
	}

	/// Equality with a C string (case sensitive)
	bool operator ==(const char *b)
	{
		const char *a = Get();
		if (!a && !b)
			return true;
		if (!a || !b)
			return false;
		return !strcmp(a, b);
	}

	bool operator !=(const char *b)
	{
		return !(*this == b);
	}
	
	/// Assignment operators
	GString &operator =(const char *s)
	{
		if (Str == NULL ||
			s < Str->Str ||
			s > Str->Str + Str->Len)
		{
			Empty();
			Set(s);
		}
		else if (s != Str->Str)
		{
			// Special case for setting it to part of itself
			// If you try and set a string to the start, it's a NOP
			int Off = s - Str->Str;
			memmove(Str->Str, s, Str->Len - Off + 1);
			Str->Len -= Off;
		}
		
		return *this;
	}
	
	GString &operator =(const wchar_t *s)
	{
		Empty();
		
		if (s)
		{
			// FIXME, this needs to work without allocating 2 blocks of 
			// heap memory
			char *u = WideToUtf8(s);
			if (u)
			{
				*this = u;
				delete [] u;
			}			
		}
		
		return *this;
	}

	GString &operator =(int val)
	{
		char n[32];
		sprintf_s(n, sizeof(n), "%i", val);
		Set(n);
		return *this;
	}

	GString &operator =(int64 val)
	{
		char n[32];
		sprintf_s(n, sizeof(n), "%"PRId64, val);
		Set(n);
		return *this;
	}
	
	/// Cast to C string operator
	operator char *()
	{
		return Str ? Str->Str : NULL;
	}

	/// Concatenation operator
	GString operator +(const GString &s)
	{
		GString Ret;
		int Len = Length() + s.Length();
		if (Ret.Set(NULL, Len))
		{
			char *p = Ret.Get();
			if (p)
			{
				if (Str)
				{
					memcpy(p, Str->Str, Str->Len);
					p += Str->Len;
				}
				if (s.Str)
				{
					memcpy(p, s.Str->Str, s.Str->Len);
					p += s.Str->Len;
				}
				*p++ = 0;
			}
		}
		
		return Ret;
	}

	/// Concatenation / assignment operator
	GString &operator +=(const GString &s)
	{
		int Len = Length() + s.Length();
		int Alloc = sizeof(RefStr) + Len;
		RefStr *rs = (RefStr*)malloc(Alloc);
		if (rs)
		{
			rs->Refs = 1;
			rs->Len = Len;
			#ifdef LGI_UNIT_TESTS
			RefStrCount++;
			#endif
			
			char *p = rs->Str;
			if (Str)
			{
				memcpy(p, Str->Str, Str->Len);
				p += Str->Len;
			}
			if (s.Str)
			{
				memcpy(p, s.Str->Str, s.Str->Len);
				p += s.Str->Len;
			}
			*p++ = 0;
			assert(p - (char*)rs <= Alloc);
			
			Empty();
			Str = rs;
		}
		
		return *this;
	}
	
	/// Gets the length in bytes
	uint32 Length() const
	{
		return Str ? Str->Len : 0;
	}

	uint32 Length(uint32 NewLen)
	{
		if (Str)
		{
			if (NewLen < Str->Len)
			{
				Str->Len = NewLen;
				Str->Str[NewLen] = 0;
			}
			else
			{
				RefStr *n = (RefStr*)malloc(sizeof(RefStr) + NewLen);
				if (n)
				{
					n->Len = NewLen;
					n->Refs = 1;
					memcpy(n->Str, Str->Str, Str->Len);
					n->Str[Str->Len] = 0; // NULL terminate...
					
					Empty(); // Deref the old string...
					
					Str = n;
				}
				else return 0;
			}
		}
		else
		{
			Str = (RefStr*)malloc(sizeof(RefStr) + NewLen);
			if (Str)
			{
				Str->Len = NewLen;
				Str->Refs = 1;
				Str->Str[0] = 0; // NULL terminate...
			}
			else return 0;
		}
		
		return Str->Len;
	}

	/// Splits the string into parts using a separator
	Array Split(const char *Sep, int Count = -1, bool CaseSen = false)
	{
		Array a;

		if (Str && Sep)
		{
			const char *s = Str->Str, *Prev = s;
			const char *end = s + Str->Len;
			size_t SepLen = strlen(Sep);
			
			if (s[Str->Len] == 0)
			{
				while ((s = CaseSen ? strstr(s, Sep) : Stristr(s, Sep)))
				{
					if (s > Prev)
						a.New().Set(Prev, s - Prev);
					s += SepLen;
					Prev = s;
					if (Count > 0 && a.Length() >= (uint32)Count)
						break;
				}
			
				if (Prev < end)
					a.New().Set(Prev, end - Prev);

				a.SetFixedLength();
			}
			else assert(!"String not NULL terminated.");
		}

		return a;
	}

	/// Splits the string into parts using a separator
	Array RSplit(const char *Sep, int Count = -1, bool CaseSen = false)
	{
		Array a;

		if (Str && Sep)
		{
			const char *s = Get();
			size_t SepLen = strlen(Sep);
			
			GArray<const char*> seps;
			
			while ((s = CaseSen ? strstr(s, Sep) : Stristr(s, Sep)))
			{
				seps.Add(s);
				s += SepLen;
			}
			
			int i;
			int Last = seps.Length() - 1;
			GString p;
			for (i=Last; i>=0; i--)
			{
				const char *part = seps[i] + SepLen;
				
				if (i == Last)
					p.Set(part);
				else
					p.Set(part, seps[i+1]-part);
				a.AddAt(0, p);
				
				if (Count > 0 && a.Length() >= (uint32)Count)
					break;
			}
			
			const char *End = seps[i > 0 ? i : 0];
			p.Set(Get(), End - Get());
			a.AddAt(0, p);
		}

		a.SetFixedLength();
		return a;
	}

	/// Splits the string into parts using delimiter chars
	Array SplitDelimit(const char *Delimiters = NULL, int Count = -1, bool GroupDelimiters = true) const
	{
		Array a;

		if (Str)
		{
			const char *delim = Delimiters ? Delimiters : " \t\r\n";
			const char *s = Get();
			while (*s)
			{
				// Skip over non-delimiters
				const char *e = s;
				while (*e && !strchr(delim, *e))
					e++;

				if (e > s || !GroupDelimiters)
					a.New().Set(s, e - s);

				s = e;
				if (*s) s++;
				if (GroupDelimiters)
				{
					// Skip any delimiters
					while (*s && strchr(delim, *s))
						s++;
				}

				// Create the string			
				if (Count > 0 && a.Length() >= (uint32)Count)
					break;
			}
			
			if
			(
				*s ||
				(
					!GroupDelimiters &&
					s > Get() &&
					strchr(delim, s[-1])
				)
			)
				a.New().Set(s);
		}

		a.SetFixedLength();
		return a;
	}
	
	/// Joins an array of strings using a separator
	GString Join(Array &a)
	{
		GString ret;
		
		if (a.Length() == 0)
			return ret;

		char *Sep = Get();
		size_t SepLen = Sep ? strlen(Sep) : 0;
		size_t Bytes = SepLen * (a.Length() - 1);
		GArray<unsigned> ALen;
		for (unsigned i=0; i<a.Length(); i++)
		{
			ALen[i] = a[i].Length();
			Bytes += ALen[i];
		}
		
		// Create an empty buffer of the right size
		if (ret.Set(NULL, Bytes))
		{
			char *ptr = ret.Get();
			for (unsigned i=0; i<a.Length(); i++)
			{
				if (i && Sep)
				{
					memcpy(ptr, Sep, SepLen);
					ptr += SepLen;
				}
				memcpy(ptr, a[i].Get(), ALen[i]);
				ptr += ALen[i];
			}
			assert(ptr - ret.Get() == Bytes);
			*ptr++ = 0; // NULL terminate
		}

		return ret;
	}

	// Replaces a sub-string with another
	GString Replace(const char *Old, const char *New = NULL, int Count = -1, bool CaseSen = false)
	{
		GString s;
		
		if (Old)
		{
			// Calculate the new of the new string...
			size_t OldLen = strlen(Old);
			size_t NewLen = New ? strlen(New) : 0;
			char *Match = Str->Str;
			GArray<char*> Matches;
			while (Match = (CaseSen ? strstr(Match, Old) : Stristr(Match, Old)))
			{
				Matches.Add(Match);
				if (Count >= 0 && (int)Matches.Length() >= Count)
					break;
				Match += OldLen;
			}

			size_t NewSize = Str->Len + (Matches.Length() * (NewLen - OldLen));
			s.Length(NewSize);
			char *Out = s.Get();
			char *In = Str->Str;

			// For each match...
			for (unsigned i=0; i<Matches.Length(); i++)
			{
				char *m = Matches[i];
				if (In < m)
				{
					// Copy any part before the match
					int Bytes = m - In;
					memcpy(Out, In, Bytes);
					Out += Bytes;
				}
				In = m + OldLen;

				if (New)
				{
					// Copy any new string
					memcpy(Out, New, NewLen);
					Out += NewLen;
				}
			}

			// Copy any trailer characters
			char *End = Str->Str + Str->Len;
			if (In < End)
			{
				int Bytes = End - In;
				memcpy(Out, In, Bytes);
				Out += Bytes;
			}

			assert(Out - s.Get() == NewSize); // Check we got the size right...
			*Out = 0; // Null terminate
		}
		else
		{
			s = *this;
		}
		
		return s;
	}

	/// Convert string to double
	double Float()
	{
		return Str ? atof(Str->Str) : NAN;
	}

	/// Convert to integer
	int64 Int(int Base = 10)
	{
		if (Str)
			return Atoi(Str->Str, Base);
		return -1;
	}
	
	/// Reverses all the characters in the string
	GString Reverse()
	{
		GString s;
		
		if (Length() > 0)
		{
			s = Str->Str;
			for (char *a = s, *b = s.Get() + s.Length() - 1; a < b; a++, b--)
			{
				char t = *a;
				*a = *b;
				*b = t;
			}
		}
		
		return s;
	}

	/// Find a sub-string	
	ptrdiff_t Find(const char *needle, int start = 0, int end = -1)
	{
		if (!needle) return -1;
		char *c = Get();
		if (!c) return -1;

		char *pos = c + start;
		while (c < pos)
		{
			if (!*c)
				return -1;
			c++;
		}
		
		char *found = (end > 0) ? Strnstr(c, needle, end - start) : strstr(c, needle);
		return (found) ? found - Get() : -1;
	}

	/// Reverse find a string (starting from the end)
	ptrdiff_t RFind(const char *needle, int start = 0, int end = -1)
	{
		if (!needle) return -1;
		char *c = Get();
		if (!c) return -1;

		char *pos = c + start;
		while (c < pos)
		{
			if (!*c)
				return -1;
			c++;
		}
		
		char *found, *prev = NULL;
		size_t str_len = strlen(needle);
		while
		((
			found =
			(
				(end > 0) ?
				Strnstr(c, needle, end - start) :
				strstr(c, needle)
			)
		))
		{
			prev = found;
			c = found + str_len;
		}
		return (prev) ? prev - Get() : -1;
	}

	/// Returns a copy of the string with all the characters converted to lower case
	GString Lower()
	{
		GString s;
		if (Str && s.Set(Str->Str, Str->Len))
			Strlwr(s.Get());
		return s;
	}
	
	/// Returns a copy of the string with all the characters converted to upper case
	GString Upper()
	{
		GString s;
		if (Str && s.Set(Str->Str, Str->Len))
			Strupr(s.Get());
		return s;
	}

	/// Gets the character at 'index'
	int operator() (int index)
	{
		if (!Str)
			return 0;

		char *c = Str->Str;
		if (index < 0)
		{
			int idx = Str->Len + index;
			if (idx >= 0)
			{
				return c[idx];
			}
		}
		else if (index < (int)Str->Len)
		{
			return c[index];
		}
		
		return 0;
	}
	
	/// Gets the string between at 'start' and 'end' (not including the end'th character)
	GString operator() (int start, int end)
	{
		GString s;
		if (Str)
		{
			char *c = Str->Str;
			int start_idx = start < 0 ? Str->Len + start + 1 : start;
			int end_idx = end < 0 ? Str->Len + end + 1 : end;
			if (end_idx > Str->Len)
				end_idx = Str->Len;
			
			if (start_idx >= 0 && end_idx > start_idx)
				s.Set(c + start_idx, end_idx - start_idx);
		}
		return s;
	}

	/// Strip off any leading and trailing whitespace	
	GString Strip(const char *set = NULL)
	{
		GString ret;
		_strip(ret, set, true, true);
		return ret;
	}

	/// Strip off any leading whitespace	
	GString LStrip(const char *set = NULL)
	{
		GString ret;
		_strip(ret, set, true, false);
		return ret;
	}	

	/// Strip off any trailing whitespace	
	GString RStrip(const char *set = NULL)
	{
		GString ret;
		_strip(ret, set, false, true);
		return ret;
	}

	/// Prints a formatted string to this object
	int Printf(const char *Fmt, ...)
	{
		Empty();
		va_list Arg;
		va_start(Arg, Fmt);
		int Bytes = Printf(Arg, Fmt);
		va_end(Arg);
		return Bytes;
	}

	/// Prints a varargs string
	int Printf(va_list &Arg, const char *Fmt)
	{
		Empty();
		return LgiPrintf(*this, Fmt, Arg);
	}
	
	#if defined(MAC) // && __COREFOUNDATION_CFBASE__

	GString(const CFStringRef r)
	{
		Str = NULL;
		*this = r;
	}
	
	GString &operator =(CFStringRef r)
	{
		CFIndex length = CFStringGetLength(r);
		CFRange range = CFRangeMake(0, length);
		CFIndex usedBufLen = 0;
		CFIndex slen = CFStringGetBytes(r,
										range,
										kCFStringEncodingUTF8,
										'?',
										false,
										NULL,
										0,
										&usedBufLen);
		if (Set(NULL, usedBufLen))
		{
			slen = CFStringGetBytes(	r,
										range,
										kCFStringEncodingUTF8,
										'?',
										false,
										(UInt8*)Str->Str,
										Str->Len,
										&usedBufLen);
			Str->Str[usedBufLen] = 0; // NULL terminate
		}
		
		return *this;
	}
	
	CFStringRef CreateStringRef()
	{
		char *s = Get();
		if (!s)
			return NULL;
		return CFStringCreateWithCString(kCFAllocatorDefault, s, kCFStringEncodingUTF8);
	}
	
	#endif
};

#endif