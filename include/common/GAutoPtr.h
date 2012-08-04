#ifndef _GAUTOPTR_H_
#define _GAUTOPTR_H_

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
		
		// This is needed to be able to use GAutoPtr inside GArray's.
		Ptr = 0;
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
		LgiAssert(Ptr != 0); 
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

typedef GAutoPtr<char, true> GAutoString;
typedef GAutoPtr<char16, true> GAutoWString;

#endif // __cplusplus

#endif // _GAUTOPTR_H_