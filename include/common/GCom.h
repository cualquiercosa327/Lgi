#if !defined(_GCOM_H_) && defined(WIN32)
#define _GCOM_H_

#ifdef __GNUC__
#include <unknwn.h>
#define COM_NO_WINDOWS_H
#endif
#include <wtypes.h>
#include <oaidl.h>
#include "GContainers.h"

template <class T>
class GUnknownImpl : public T
{
	int Count;

	class Interface
	{
	public:
		REFIID iid;
		void *pvObject;

		Interface(REFIID i, void *p) : iid(i)
		{
			pvObject = p;
		}
	};

	List<Interface> Interfaces;

protected:
	void AddInterface(REFIID iid, void *pvObject)
	{
		Interface *i = new Interface(iid, pvObject);
		if (i)
		{
			Interfaces.Insert(i);
		}
	}

public:
	GUnknownImpl()
	{
		Count = 0;
	}

	virtual ~GUnknownImpl()
	{
		Interfaces.DeleteObjects();
	}
	
	// IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
	{
		if (memcmp(&IID_IUnknown, &iid, sizeof(IID)) == 0)
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}

		for (Interface *i = Interfaces.First(); i; i=Interfaces.Next())
		{
			if (memcmp(&i->iid, &iid, sizeof(IID)) == 0)
			{
				*ppvObject = i->pvObject;
				AddRef();
				return S_OK;
			}
		}

		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef()
	{
		return ++Count;
	}
	
	ULONG STDMETHODCALLTYPE Release()
	{
		int i = --Count;
		if (i <= 0)
		{
			delete this;
		}
		return  i;
	}

	// Helpers
	BSTR VariantToBstr(VARIANT *v)
	{
		if (v)
		{
			if (v->vt == (VT_VARIANT | VT_BYREF)) v = v->pvarVal;
			
			if (v->vt == VT_BSTR) return v->bstrVal;
			if (v->vt == (VT_BSTR | VT_BYREF)) return *v->pbstrVal;
		}

		return 0;
	}
};

template <class T>
class GDispatchImpl : public T
{
public:
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT __RPC_FAR *pctinfo)
	{
		return S_OK;
	}
    
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo)
	{
		return S_OK;
	}
    
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid,
											LPOLESTR __RPC_FAR *rgszNames,
											UINT cNames,
											LCID lcid,
											DISPID __RPC_FAR *rgDispId)
	{
		return S_OK;
	}
    
    HRESULT STDMETHODCALLTYPE Invoke(		DISPID dispIdMember,
											REFIID riid,
											LCID lcid,
											WORD wFlags,
											DISPPARAMS __RPC_FAR *pDispParams,
											VARIANT __RPC_FAR *pVarResult,
											EXCEPINFO __RPC_FAR *pExcepInfo,
											UINT __RPC_FAR *puArgErr)
	{
		return S_OK;
	}
};

#endif
