/*
 * A mis-guided attempt to make a string class look and feel like a python string.
 *
 * Author: Matthew Allen
 * Email: fret@memecode.com
 * Created: 16 Sept 2014
 */
#ifndef _GSTRING_H_
#define _GSTRING_H_

#include <xmath.h>
#include "GUtf8.h"

class GString : public GAutoString
{
	inline void _strip(GString &ret, const char *set, bool left, bool right)
	{
		char *s = Get();
		if (s)
		{
			if (!set) set = " \t\r\n";
			
			while (left && *s && strchr(set, *s))
				s++;
			
			char *e = s + strlen(s);
			while (right && e > s && strchr(set, e[-1]))
				e--;
			
			ret.Reset(NewStr(s, e - s));
		}
	}

public:
	GString()
	{
	}
	
	GString(const char *str, int len = -1)
	{
		Reset(NewStr(str, len));
	}
	
	class Array : public GArray<GString>
	{
	public:
		Array() {}
		Array(const Array &a)
		{
			*this = (Array&)a;
		}
		
		Array &operator =(Array &a)
		{
			SetFixedLength(false);
			Length(a.Length());
			for (uint32 i=0; i<a.Length(); i++)
			{
				GString &s = a[i];
				char *ns = NewStr(s);
				((*this)[i]).Reset(ns);
			}
			SetFixedLength(true);
			return *this;
		}
	};
	
	Array Split(const char *sep, int count = -1, bool case_sen = false)
	{
		Array a;
		const char *s = Get(), *prev = Get();
		int sep_len = strlen(sep);
		while (s = case_sen ? strstr(s, sep) : stristr(s, sep))
		{
			a.New().Reset(NewStr(prev, s - prev));
			s += sep_len;
			prev = s;
			if (count > 0 && a.Length() >= (uint32)count)
				break;
		}
		if (*prev)
			a.New().Reset(NewStr(prev));

		a.SetFixedLength();
		return a;
	}
	
	GString Join(Array &a)
	{
		GString ret;
		char *sep = Get();
		if (sep)
		{
			int sep_len = strlen(sep);
			int bytes = sep_len * (a.Length() - 1);
			GArray<unsigned> a_len;
			for (unsigned i=0; i<a.Length(); i++)
			{
				a_len[i] = strlen(a[i].Get());
				bytes += a_len[i];
			}
			
			if (ret.Reset(new char[bytes+1]))
			{
				char *ptr = ret;
				for (unsigned i=0; i<a.Length(); i++)
				{
					if (i)
					{
						memcpy(ptr, sep, sep_len);
						ptr += sep_len;
					}
					memcpy(ptr, a[i], a_len[i]);
					ptr += a_len[i];
				}
				LgiAssert(ptr - ret.Get() == bytes);
				*ptr++ = 0; // NULL terminate
			}
		}
		return ret;
	}
	
	double Float()
	{
		return Get() ? atof(Get()) : NAN;
	}
	
	int Int()
	{
		return Get() ? atoi(Get()) : -1;
	}
	
	int Find(const char *str, int start = 0, int end = -1)
	{
		if (!str) return -1;
		char *c = Get();
		if (!c) return -1;

		char *pos = c + start;
		while (c < pos)
		{
			if (!*c)
				return -1;
			c++;
		}
		
		char *found = (end > 0) ? strnstr(c, str, end - start) : strstr(c, str);
		return (found) ? found - Get() : -1;
	}

	int RFind(const char *str, int start = 0, int end = -1)
	{
		if (!str) return -1;
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
		int str_len = strlen(str);
		while
		(
			found =
			(
				(end > 0) ?
				strnstr(c, str, end - start) :
				strstr(c, str)
			)
		)
		{
			prev = found;
			c = found + str_len;
		}
		return (prev) ? prev - Get() : -1;
	}

	GString Lower()
	{
		GString s(*this);
		StrLwr(s.Get());
		return s;
	}
	
	GString Upper()
	{
		GString s(*this);
		StrUpr(s.Get());
		return s;
	}

	GString operator() (int index)
	{
		GString s;
		char *c = Get();
		if (c)
		{
			int len = strlen(c);
			if (index < 0)
			{
				int idx = len + index;
				if (idx >= 0)
				{
					s.Reset(NewStr(c + idx, 1));
				}
			}
			else if (index < len)
			{
				s.Reset(NewStr(c + index, 1));
			}
		}
		return s;
	}
	
	GString operator() (int start, int end)
	{
		GString s;
		char *c = Get();
		if (c)
		{
			int len = strlen(c);
			int start_idx = start < 0 ? len + start + 1 : start;
			int end_idx = end < 0 ? len + end + 1 : end;
			if (start_idx >= 0 && end_idx > start_idx)
			{
				s.Reset(NewStr(c + start_idx, end_idx - start_idx));
			}
		}		
		return s;
	}
	
	GString Strip(const char *set = NULL)
	{
		GString ret;
		_strip(ret, set, true, true);
		return ret;
	}	

	GString LStrip(const char *set = NULL)
	{
		GString ret;
		_strip(ret, set, true, false);
		return ret;
	}	

	GString RStrip(const char *set = NULL)
	{
		GString ret;
		_strip(ret, set, false, true);
		return ret;
	}	
};

#endif