#ifndef _GAUTOPTR_H_
#define _GAUTOPTR_H_

#include <string.h>
#include "GMem.h"
#include "GString.h"
#include "GArray.h"

#ifdef __cplusplus
template<class X>
class GAutoPtrRef
{
	typedef GAutoPtrRef<X> Self;
	Self& operator = (Self const&) {}

public:    
	X *Ptr;

	explicit
	GAutoPtrRef(X *p)
	{
		Ptr = p;
	}
};

template<class X, bool Arr = false>
class GAutoPtr
{
	X *Ptr;

public:
 	explicit GAutoPtr(X* p=0)
	{
		Ptr = p;
	}

	GAutoPtr(GAutoPtr &ap)
	{
		Ptr = ap.Release();
	}

	GAutoPtr(GAutoPtrRef<X> apr)
	{
		Ptr = apr.Ptr;
	}

	// template<class Y> GAutoPtr(GAutoPtr<Y>&)

	GAutoPtr& operator=(GAutoPtr ap)
	{
		Reset(ap.Release());
		return *this;
	}
	
	GAutoPtr& operator=(GAutoPtrRef<X> ap)
	{
		if (ap.Ptr != Ptr)
		{
			Reset(ap.Ptr);
		}
		return *this;
	}

	// template<class Y> GAutoPtr& operator=(GAutoPtr<Y>&) throw();

	~GAutoPtr()
	{
		if (Arr)
			delete [] Ptr;
		else
			delete Ptr;
	}

	/*
	X& operator*() const
	{
		LgiAssert(Ptr); 
		return Ptr;
	}
	*/

	operator X*() const
	{
		return Ptr;
	}

	X* operator->() const
	{
		LgiAssert(Ptr); 
		return Ptr;
	}

	X* Get() const
	{
		return Ptr;
	}

	X* Release()
	{
		X *p = Ptr;
		Ptr = 0;
		return p;
	}

	bool Reset(X* p=0)
	{
		if (Ptr == p)
			return Ptr != 0;

		if (Arr)
			delete [] Ptr;
		else
			delete Ptr;
		Ptr = p;
		return Ptr != 0;
	}

	template<class Y>
	operator GAutoPtrRef<Y>()
	{
		return GAutoPtrRef<Y>(Release());
	}

	template<class Y>
	operator GAutoPtr<Y>()
	{
		return GAutoPtr<Y>(Release());
	}
};

#if 1

typedef GAutoPtr<char, true> GAutoString;
typedef GAutoPtr<char16, true> GAutoWString;

#else

template<class X>
class GAutoStringRef
{
	typedef GAutoStringRef<X> Self;
	Self& operator = (Self const&) {}

public:    
	X *Ptr;

	explicit
	GAutoStringRef(X *p)
	{
		Ptr = p;
	}
};

class LgiClass GAutoString : public GAutoPtr<char, true>
{
public:
	typedef GArray<GAutoString> A;
	
	GAutoString(char *str = 0) : GAutoPtr<char, true>(str) {}
	GAutoString(GAutoString &s) : GAutoPtr<char, true>(s) {}
	GAutoString(GAutoStringRef<char> r) : GAutoPtr<char, true>(r.Ptr) {}
	GAutoString& operator=(GAutoStringRef<char> ap)
	{
		if (ap.Ptr != Get())
			Reset(ap.Ptr);
		return *this;
	}
	template<class Y>
	operator GAutoStringRef<Y>()
	{
		return GAutoStringRef<Y>(Release());
	}
	
	int length()
	{
		return Get() ? strlen(Get()) : 0;
	}

	int find(char *sub, int start = 0, int end = -1)
	{
		if (!sub)
			return -1;
		char *s = Get(), *r;
		int i;
		for (i=0; i<start && s[i]; i++);
		s += i;
		if (end >= 0)
			r = strnstr(s, sub, end - start);
		else
			r = strstr(s, sub);
		return r ? r - Get() : -1;
	}

	GAutoString strip(char *ch = 0)
	{
		if (!ch)
			ch = " \t\r\n";
		char *s = Get();
		while (*s && strchr(ch, *s))
			s++;
		char *e = s + strlen(s);
		while (e > s && strchr(ch, e[-1]))
			e--;

		return GAutoString(NewStr(s, e-s));
	}

	GAutoString lower()
	{
		GAutoString c(NewStr(Get()));
		strlwr(c);
		return c;
	}

	GAutoString upper()
	{
		GAutoString c(NewStr(Get()));
		strupr(c);
		return c;
	}

	/*
	A split(char *sep = 0, int maxsplit = -1)
	{
		A a;

		if (!sep)
			sep = " \t\r\n";
		char *s = Get();
		while (*s && (maxsplit < 0 || a.Length() < maxsplit))
		{
			while (*s && strchr(sep, *s))
				s++;
			char *e = s;
			while (*e && !strchr(sep, *e))
				e++;
			if (e > s)
				a.New().Reset(NewStr(s, e-s));
			s = e;
		}

		return a;
	}
	*/
};

class LgiClass GAutoWString : public GAutoPtr<char16, true>
{
public:
	typedef GArray<GAutoWString> A;

	GAutoWString(char16 *str = 0) : GAutoPtr<char16, true>(str) {}
	GAutoWString(GAutoWString &s) : GAutoPtr<char16, true>(s) {}

	int length()
	{
		return Get() ? StrlenW(Get()) : 0;
	}

	int find(char16 *sub, int start = 0, int end = -1)
	{
		if (!sub)
			return -1;
		char16 *s = Get(), *r;
		int i;
		for (i=0; i<start && s[i]; i++);
		s += i;
		if (end >= 0)
			r = StrnstrW(s, sub, end - start);
		else
			r = StrstrW(s, sub);
		return r ? r - Get() : -1;
	}

	GAutoWString strip(char16 *ch = 0)
	{
		if (!ch)
			ch = L" \t\r\n";
		char16 *s = Get();
		while (*s && StrchrW(ch, *s))
			s++;
		char16 *e = s + StrlenW(s);
		while (e > s && StrchrW(ch, e[-1]))
			e--;

		return GAutoWString(NewStrW(s, e-s));
	}

	GAutoWString lower()
	{
		GAutoWString c(NewStrW(Get()));
		_wcslwr(c);
		return c;
	}

	GAutoWString upper()
	{
		GAutoWString c(NewStrW(Get()));
		_wcsupr(c);
		return c;
	}

	/*
	A split(char16 *sep = 0, int maxsplit = -1)
	{
		A a;

		if (!sep)
			sep = L" \t\r\n";
		char16 *s = Get();
		while (*s && (maxsplit < 0 || a.Length() < maxsplit))
		{
			while (*s && StrchrW(sep, *s))
				s++;
			char16 *e = s;
			while (*e && !StrchrW(sep, *e))
				e++;
			if (e > s)
				a.New().Reset(NewStrW(s, e-s));
			s = e;
		}
		
		return a;
	}
	*/
};

#endif

#endif // __cplusplus

#endif // _GAUTOPTR_H_