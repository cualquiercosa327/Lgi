/// \file
/// \author Matthew Allen

#ifndef __OS_CLASS_H
#define __OS_CLASS_H

class LgiClass OsApplication
{
protected:
	HCURSOR hNormalCursor;

public:
	OsApplication()
	{
		hNormalCursor = 0;
	}
};

// Win32 Memory Handler
class LgiClass GMem {

	int64 Size;
	HGLOBAL hMem;
	void *pMem;

public:
	GMem(int64 size)
	{
		Size = size;
		hMem = GlobalAlloc(GMEM_FIXED, (DWORD)Size);
		pMem = 0;
	}

	GMem(HGLOBAL hmem)
	{
		hMem = hmem;
		Size = GlobalSize(hMem);
		pMem = 0;
	}

	~GMem()
	{
		if (pMem)
		{
			GlobalUnlock(hMem);
			pMem = 0;
		}
		if (hMem)
		{
			GlobalFree(hMem);
			hMem = 0;
		}
	}

	void *Lock()
	{
		return pMem = GlobalLock(hMem);
	}

	void UnLock()
	{
		if (pMem)
		{
			GlobalUnlock(hMem);
			pMem = 0;
		}
	}

	int64 GetSize()
	{
		return Size;
	}

	void Detach()
	{
		if (pMem)
		{
			GlobalUnlock(hMem);
			pMem = 0;
		}
		hMem = 0;
		Size = 0;
	}

	HGLOBAL Handle() { return hMem; }
};

// Win32 window class
class LgiClass GWin32Class : public GBase
{
	friend class GControl;
	friend class GApp;

	WNDPROC ParentProc;

#ifdef __GNUC__
public:
#endif
	static LRESULT CALLBACK Redir(OsView hWnd, UINT m, WPARAM a, LPARAM b);
	static LRESULT CALLBACK SubClassRedir(OsView hWnd, UINT m, WPARAM a, LPARAM b);

public:
	WNDCLASSEXW Class;

	GWin32Class(const char *Name);
	~GWin32Class();

	static bool IsSystem(const char *Cls);
	bool Register();
	bool SubClass(char *Parent);
	LRESULT CALLBACK CallParent(OsView hWnd, UINT m, WPARAM a, LPARAM b);

	static GWin32Class *Create(const char *ClassName);
};

#include "GRegKey.h"

#endif
