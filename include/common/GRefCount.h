#ifndef _REF_COUNT_H_
#define _REF_COUNT_H_

class GRefCount
{
	int _Count;

protected:
    #ifdef _DEBUG
    int _GetCount() { return _Count; }
    #endif

public:
	GRefCount()
	{
		_Count = 0;
	}

	virtual ~GRefCount()
	{
		LgiAssert(_Count == 0);
	}

	virtual void AddRef()
	{
		_Count++;
	}

	virtual bool DecRef()
	{
		LgiAssert(_Count > 0);
		if (--_Count == 0)
		{
			delete this;
			return true;
		}
		return false;
	}
};

template <typename T>
class GAutoRefPtr
{
	T *Ptr;

public:
	GAutoRefPtr(T *ptr = 0)
	{
		Ptr = ptr;
		if (Ptr)
			Ptr->AddRef();
	}

	~GAutoRefPtr()
	{
		Empty();
	}

	operator T*()
	{
		return Ptr;
	}
	
	T *operator->() const
	{
		LgiAssert(Ptr != NULL);
		return Ptr;
	}

	void Empty()
	{
		if (Ptr)
		{
			Ptr->DecRef();
			Ptr = NULL;
		}
	}

	GAutoRefPtr<T> &operator =(T *p)
	{
		Empty();
		Ptr = p;
		if (Ptr)
			Ptr->AddRef();
		return *this;
	}

	GAutoRefPtr &operator =(const GAutoRefPtr<T> &refptr)
	{
		Empty();
		Ptr = refptr.Ptr;
		if (Ptr)
			Ptr->AddRef();
		return *this;
	}

};

#endif