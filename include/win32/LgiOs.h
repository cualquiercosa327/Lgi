//
//      FILE:           LgiOs.h (Win32)
//      AUTHOR:         Matthew Allen
//      DATE:           24/9/99
//      DESCRIPTION:    Lgi Win32 OS defines
//
//      Copyright (C) 1999, Matthew Allen
//              fret@memecode.com
//

#include "windows.h"
// WIN32 defined here
#include "commctrl.h"

// Stupid mouse wheel defines don't work. hmmm
#define WM_MOUSEWHEEL				0x020A
#define WHEEL_DELTA					120
#define SPI_GETWHEELSCROLLLINES		104

// directories
#define DIR_CHAR					'\\'
#define DIR_STR						"\\"

#define IsSlash(c)					(((c)=='/')||((c)=='\\'))
#define IsQuote(c)					(((c)=='\"')||((c)=='\''))

// Win32 Memory Handler
class LgiClass GMem
{
	int64 Size;
	HGLOBAL hMem;
	void *pMem;

public:
	GMem(int64 size)
	{
		Size = size;
		hMem = GlobalAlloc(GMEM_FIXED, Size);
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
		GlobalFree(hMem);
		hMem = 0;
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

	HGLOBAL Handle() { return hMem; }
	int64 GetSize() { return Size; }
};

